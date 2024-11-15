/*--------------------------------------------------------------------*/
/* ft.c                                                               */
/* Author: anish                                                      */
/*--------------------------------------------------------------------*/

#include "ft.h"  /* Include ft.h first to ensure declarations match definitions */

#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "dynarray.h"
#include "path.h"
#include "nodeFT.h"

/*
  The File Tree (FT) maintains a hierarchical structure of directories and files.
  It uses three static variables to represent its state.
*/

/* Flag indicating whether the File Tree has been initialized */
static boolean bIsInitialized;

/* Root node of the File Tree */
static Node_T oNRoot;

/* Total number of nodes in the File Tree */
static size_t ulCount;

/*---------------------------------------------------------------*/
/* Static Helper Function Prototypes                             */
/*---------------------------------------------------------------*/

/*
  Navigates the File Tree starting from the root node towards the specified path `oPPath`.
  The traversal stops if a file node is encountered, as files cannot have children.

  Parameters:
    - oPPath: the target `Path_T` object representing the path to traverse to
    - poNFurthestNode: pointer to where we'll store the deepest `Node_T` reached
    - pbIsFile: pointer to a boolean that will be set to TRUE if traversal stopped at a file node

  Returns:
    - SUCCESS if traversal is successful
    - Appropriate error code otherwise

  On success, sets `*poNFurthestNode` to the deepest node reached and `*pbIsFile` accordingly.
  On failure, sets `*poNFurthestNode` to NULL.
*/
static int FT_traversePath(Path_T oPPath, Node_T *poNFurthestNode, boolean *pbIsFile);

/*
  Traverses the File Tree to find a node with absolute path `pcPath`.

  Parameters:
    - pcPath: the string representing the absolute path we're looking for
    - poNResult: pointer to where we'll store the found `Node_T`

  Returns:
    - SUCCESS if the node is found
    - Appropriate error code otherwise

  On success, sets `*poNResult` to the found node.
  On failure, sets `*poNResult` to NULL.
*/
static int FT_findNode(const char *pcPath, Node_T *poNResult);

/*
  Handles errors during insertion operations.
  Frees allocated resources and returns the provided status code.

  Parameters:
    - iStatus: the error code to return
    - oPPath: the `Path_T` object that was being inserted
    - oNNewNodes: the newly created nodes that need to be freed

  Returns:
    - The provided error status `iStatus`
*/
static int FT_handleInsertError(int iStatus, Path_T oPPath, Node_T oNNewNodes);

/*
  Helper function for pre-order traversal of the File Tree.
  Inserts each node into `oDNodes` starting at index `ulIndex`.

  Parameters:
    - oNNode: the current `Node_T` being traversed
    - oDNodes: the dynamic array `DynArray_T` to store nodes
    - ulIndex: the current index in the array

  Returns:
    - The next available index in `oDNodes` after insertion
*/
static size_t FT_preOrderTraversal(Node_T oNNode, DynArray_T oDNodes, size_t ulIndex);

/*---------------------------------------------------------------*/
/* Static Helper Function Definitions                            */
/*---------------------------------------------------------------*/

static int FT_traversePath(Path_T oPPath, Node_T *poNFurthestNode, boolean *pbIsFile) {
    int iStatus;
    Path_T oPPrefixPath = NULL;
    Node_T oNCurrNode = NULL;
    Node_T oNChild = NULL;
    size_t ulDepth, ulIndex;
    size_t ulChildID = 0;
    boolean bFoundFile = FALSE;

    assert(oPPath != NULL);
    assert(poNFurthestNode != NULL);
    assert(pbIsFile != NULL);

    if (oNRoot == NULL) {
        *poNFurthestNode = NULL;
        *pbIsFile = FALSE;
        return SUCCESS;
    }

    iStatus = Path_prefix(oPPath, 1, &oPPrefixPath);
    if (iStatus != SUCCESS) {
        *poNFurthestNode = NULL;
        return iStatus;
    }

    if (Path_comparePath(NodeFT_getPath(oNRoot), oPPrefixPath) != 0) {
        Path_free(oPPrefixPath);
        *poNFurthestNode = NULL;
        return CONFLICTING_PATH;
    }
    Path_free(oPPrefixPath);

    oNCurrNode = oNRoot;
    ulDepth = Path_getDepth(oPPath);
    ulIndex = 2;

    /* Loop through each component of the path */
    while (ulIndex <= ulDepth) {
        iStatus = Path_prefix(oPPath, ulIndex, &oPPrefixPath);
        if (iStatus != SUCCESS) {
            *poNFurthestNode = NULL;
            return iStatus;
        }

        if (NodeFT_isFile(oNCurrNode)) {
            /* Cannot traverse further if current node is a file */
            *poNFurthestNode = oNCurrNode;
            *pbIsFile = TRUE;
            Path_free(oPPrefixPath);
            return NOT_A_DIRECTORY;
        }

        /* Check for file child first */
        if (NodeFT_hasChild(oNCurrNode, oPPrefixPath, &ulChildID, TRUE)) {
            iStatus = NodeFT_getChild(oNCurrNode, ulChildID, &oNChild, TRUE);
            if (iStatus != SUCCESS) {
                Path_free(oPPrefixPath);
                *poNFurthestNode = NULL;
                return iStatus;
            }
            oNCurrNode = oNChild;
            bFoundFile = TRUE;
        }
        /* Check for directory child */
        else if (NodeFT_hasChild(oNCurrNode, oPPrefixPath, &ulChildID, FALSE)) {
            iStatus = NodeFT_getChild(oNCurrNode, ulChildID, &oNChild, FALSE);
            if (iStatus != SUCCESS) {
                Path_free(oPPrefixPath);
                *poNFurthestNode = NULL;
                return iStatus;
            }
            oNCurrNode = oNChild;
            bFoundFile = FALSE;
        }
        else {
            /* No child found, traversal ends */
            Path_free(oPPrefixPath);
            break;
        }

        Path_free(oPPrefixPath);
        ulIndex++;
    }

    *poNFurthestNode = oNCurrNode;
    *pbIsFile = bFoundFile;
    return SUCCESS;
}

static int FT_findNode(const char *pcPath, Node_T *poNResult) {
    int iStatus;
    Path_T oPPath = NULL;
    Node_T oNFoundNode = NULL;
    boolean bIsFile = FALSE;

    assert(pcPath != NULL);
    assert(poNResult != NULL);

    if (!bIsInitialized) {
        *poNResult = NULL;
        return INITIALIZATION_ERROR;
    }

    /* Create a Path_T object from the string */
    iStatus = Path_new(pcPath, &oPPath);
    if (iStatus != SUCCESS) {
        *poNResult = NULL;
        return iStatus;
    }

    /* Traverse the tree to find the node */
    iStatus = FT_traversePath(oPPath, &oNFoundNode, &bIsFile);
    if (iStatus != SUCCESS) {
        Path_free(oPPath);
        *poNResult = NULL;
        return iStatus;
    }

    if (oNFoundNode == NULL) {
        Path_free(oPPath);
        *poNResult = NULL;
        return NO_SUCH_PATH;
    }

    /* Ensure the found node's path matches exactly */
    if (Path_comparePath(NodeFT_getPath(oNFoundNode), oPPath) != 0) {
        Path_free(oPPath);
        *poNResult = NULL;
        return NO_SUCH_PATH;
    }

    Path_free(oPPath);
    *poNResult = oNFoundNode;
    return SUCCESS;
}

static int FT_handleInsertError(int iStatus, Path_T oPPath, Node_T oNNewNodes) {
    /* Free the path and any new nodes created */
    Path_free(oPPath);
    if (oNNewNodes != NULL)
        (void)NodeFT_free(oNNewNodes);
    return iStatus;
}

static size_t FT_preOrderTraversal(Node_T oNNode, DynArray_T oDNodes, size_t ulIndex) {
    size_t ulNumFileChildren, ulNumDirChildren;
    size_t ulChildIndex;
    Node_T oNChild = NULL;
    int iStatus;

    assert(oDNodes != NULL);

    if (oNNode != NULL) {
        /* Add the current node to the array */
        (void)DynArray_set(oDNodes, ulIndex, oNNode);
        ulIndex++;

        if (!NodeFT_isFile(oNNode)) {
            /* Traverse file children first */
            ulNumFileChildren = NodeFT_getNumChildren(oNNode, TRUE);
            for (ulChildIndex = 0; ulChildIndex < ulNumFileChildren; ulChildIndex++) {
                iStatus = NodeFT_getChild(oNNode, ulChildIndex, &oNChild, TRUE);
                assert(iStatus == SUCCESS);
                ulIndex = FT_preOrderTraversal(oNChild, oDNodes, ulIndex);
            }

            /* Then traverse directory children */
            ulNumDirChildren = NodeFT_getNumChildren(oNNode, FALSE);
            for (ulChildIndex = 0; ulChildIndex < ulNumDirChildren; ulChildIndex++) {
                iStatus = NodeFT_getChild(oNNode, ulChildIndex, &oNChild, FALSE);
                assert(iStatus == SUCCESS);
                ulIndex = FT_preOrderTraversal(oNChild, oDNodes, ulIndex);
            }
        }
    }
    return ulIndex;
}

/*---------------------------------------------------------------*/
/* Public Interface Functions                                    */
/*---------------------------------------------------------------*/

/*
  Sets the FT data structure to an initialized state.
  The data structure is initially empty.
  Returns INITIALIZATION_ERROR if already initialized,
  and SUCCESS otherwise.
*/
int FT_init(void) {
    if (bIsInitialized)
        return INITIALIZATION_ERROR;

    bIsInitialized = TRUE;
    oNRoot = NULL;
    ulCount = 0;

    return SUCCESS;
}

/*
  Removes all contents of the data structure and
  returns it to an uninitialized state.
  Returns INITIALIZATION_ERROR if not already initialized,
  and SUCCESS otherwise.
*/
int FT_destroy(void) {
    if (!bIsInitialized)
        return INITIALIZATION_ERROR;

    if (oNRoot != NULL) {
        ulCount -= NodeFT_free(oNRoot);
        oNRoot = NULL;
    }

    bIsInitialized = FALSE;

    return SUCCESS;
}

/*---------------------------------------------------------------*/
/* Insertion Functions                                           */
/*---------------------------------------------------------------*/

/*
  Inserts a new directory into the FT with absolute path pcPath.
  Returns SUCCESS if the new directory is inserted successfully.
  Otherwise, returns:
  * INITIALIZATION_ERROR if the FT is not in an initialized state
  * BAD_PATH if pcPath does not represent a well-formatted path
  * CONFLICTING_PATH if the root exists but is not a prefix of pcPath
  * NOT_A_DIRECTORY if a proper prefix of pcPath exists as a file
  * ALREADY_IN_TREE if pcPath is already in the FT (as dir or file)
  * MEMORY_ERROR if memory could not be allocated to complete request
*/
int FT_insertDir(const char *pcPath) {
    int iStatus;
    Path_T oPPath = NULL;
    Node_T oNCurrNode = NULL;
    Node_T oNNewNodes = NULL;
    size_t ulDepth, ulCurrDepth;
    size_t ulNewNodesCount = 0;
    boolean bIsFile = FALSE;
    size_t ulIndex;

    assert(pcPath != NULL);

    if (!bIsInitialized)
        return INITIALIZATION_ERROR;

    iStatus = Path_new(pcPath, &oPPath);
    if (iStatus != SUCCESS)
        return iStatus;

    iStatus = FT_traversePath(oPPath, &oNCurrNode, &bIsFile);
    if (iStatus != SUCCESS) {
        Path_free(oPPath);
        return iStatus;
    }

    if (oNCurrNode == NULL && oNRoot != NULL) {
        Path_free(oPPath);
        return CONFLICTING_PATH;
    }

    if (bIsFile) {
        Path_free(oPPath);
        return NOT_A_DIRECTORY;
    }

    ulDepth = Path_getDepth(oPPath);
    ulCurrDepth = oNCurrNode ? Path_getDepth(NodeFT_getPath(oNCurrNode)) : 0;
    ulIndex = ulCurrDepth + 1;

    if (ulCurrDepth == ulDepth) {
        if (Path_comparePath(NodeFT_getPath(oNCurrNode), oPPath) == 0) {
            Path_free(oPPath);
            return ALREADY_IN_TREE;
        }
    }

    /* Build the path from oNCurrNode towards oPPath */
    while (ulIndex <= ulDepth) {
        Path_T oPPrefixPath = NULL;
        Node_T oNNewNode = NULL;

        iStatus = Path_prefix(oPPath, ulIndex, &oPPrefixPath);
        if (iStatus != SUCCESS) {
            return FT_handleInsertError(iStatus, oPPath, oNNewNodes);
        }

        iStatus = NodeFT_new(oPPrefixPath, oNCurrNode, FALSE, &oNNewNode);
        Path_free(oPPrefixPath);
        if (iStatus != SUCCESS) {
            return FT_handleInsertError(iStatus, oPPath, oNNewNodes);
        }

        if (oNNewNodes == NULL)
            oNNewNodes = oNNewNode;

        oNCurrNode = oNNewNode;
        ulNewNodesCount++;
        ulIndex++;
    }

    Path_free(oPPath);

    if (oNRoot == NULL)
        oNRoot = oNNewNodes;
    ulCount += ulNewNodesCount;

    return SUCCESS;
}

/*
  Inserts a new file into the FT with absolute path pcPath, with
  file contents pvContents of size ulLength bytes.
  Returns SUCCESS if the new file is inserted successfully.
  Otherwise, returns:
  * INITIALIZATION_ERROR if the FT is not in an initialized state
  * BAD_PATH if pcPath does not represent a well-formatted path
  * CONFLICTING_PATH if the root exists but is not a prefix of pcPath,
                     or if the new file would be the FT root
  * NOT_A_DIRECTORY if a proper prefix of pcPath exists as a file
  * ALREADY_IN_TREE if pcPath is already in the FT (as dir or file)
  * MEMORY_ERROR if memory could not be allocated to complete request
*/
int FT_insertFile(const char *pcPath, void *pvContents, size_t ulLength) {
    int iStatus;
    Path_T oPPath = NULL;
    Node_T oNCurrNode = NULL;
    Node_T oNNewNodes = NULL;
    size_t ulDepth, ulCurrDepth;
    size_t ulNewNodesCount = 0;
    boolean bIsFile = FALSE;
    size_t ulIndex;

    assert(pcPath != NULL);

    if (!bIsInitialized)
        return INITIALIZATION_ERROR;

    iStatus = Path_new(pcPath, &oPPath);
    if (iStatus != SUCCESS)
        return iStatus;

    if (Path_getDepth(oPPath) == 1) {
        Path_free(oPPath);
        return CONFLICTING_PATH;
    }

    iStatus = FT_traversePath(oPPath, &oNCurrNode, &bIsFile);
    if (iStatus != SUCCESS) {
        Path_free(oPPath);
        return iStatus;
    }

    if (oNCurrNode == NULL) {
        Path_free(oPPath);
        return CONFLICTING_PATH;
    }

    if (bIsFile) {
        Path_free(oPPath);
        return NOT_A_DIRECTORY;
    }

    ulDepth = Path_getDepth(oPPath);
    ulCurrDepth = oNCurrNode ? Path_getDepth(NodeFT_getPath(oNCurrNode)) : 0;
    ulIndex = ulCurrDepth + 1;

    if (ulCurrDepth == ulDepth) {
        if (Path_comparePath(NodeFT_getPath(oNCurrNode), oPPath) == 0) {
            Path_free(oPPath);
            return ALREADY_IN_TREE;
        }
    }

    /* Build the path from oNCurrNode towards oPPath */
    while (ulIndex <= ulDepth) {
        Path_T oPPrefixPath = NULL;
        Node_T oNNewNode = NULL;
        boolean bIsFileNode = (ulIndex == ulDepth) ? TRUE : FALSE;  /* Ensure boolean assignment */

        iStatus = Path_prefix(oPPath, ulIndex, &oPPrefixPath);
        if (iStatus != SUCCESS) {
            return FT_handleInsertError(iStatus, oPPath, oNNewNodes);
        }

        iStatus = NodeFT_new(oPPrefixPath, oNCurrNode, bIsFileNode, &oNNewNode);
        Path_free(oPPrefixPath);
        if (iStatus != SUCCESS) {
            return FT_handleInsertError(iStatus, oPPath, oNNewNodes);
        }

        if (bIsFileNode) {
            iStatus = NodeFT_setContents(oNNewNode, pvContents, ulLength);
            if (iStatus != SUCCESS) {
                return FT_handleInsertError(iStatus, oPPath, oNNewNodes);
            }
        }

        if (oNNewNodes == NULL)
            oNNewNodes = oNNewNode;

        oNCurrNode = oNNewNode;
        ulNewNodesCount++;
        ulIndex++;
    }

    Path_free(oPPath);

    if (oNRoot == NULL)
        oNRoot = oNNewNodes;
    ulCount += ulNewNodesCount;

    return SUCCESS;
}

/*---------------------------------------------------------------*/
/* Removal Functions                                             */
/*---------------------------------------------------------------*/

/*
  Removes the FT hierarchy (subtree) at the directory with absolute
  path pcPath. Returns SUCCESS if found and removed.
  Otherwise, returns:
  * INITIALIZATION_ERROR if the FT is not in an initialized state
  * BAD_PATH if pcPath does not represent a well-formatted path
  * CONFLICTING_PATH if the root exists but is not a prefix of pcPath
  * NO_SUCH_PATH if absolute path pcPath does not exist in the FT
  * NOT_A_DIRECTORY if pcPath is in the FT as a file not a directory
  * MEMORY_ERROR if memory could not be allocated to complete request
*/
int FT_rmDir(const char *pcPath) {
    int iStatus;
    Node_T oNTargetNode = NULL;

    assert(pcPath != NULL);

    iStatus = FT_findNode(pcPath, &oNTargetNode);
    if (iStatus != SUCCESS)
        return iStatus;

    if (NodeFT_isFile(oNTargetNode))
        return NOT_A_DIRECTORY;

    ulCount -= NodeFT_free(oNTargetNode);
    if (ulCount == 0)
        oNRoot = NULL;

    return SUCCESS;
}

/*
  Removes the FT file with absolute path pcPath.
  Returns SUCCESS if found and removed.
  Otherwise, returns:
  * INITIALIZATION_ERROR if the FT is not in an initialized state
  * BAD_PATH if pcPath does not represent a well-formatted path
  * CONFLICTING_PATH if the root exists but is not a prefix of pcPath
  * NO_SUCH_PATH if absolute path pcPath does not exist in the FT
  * NOT_A_FILE if pcPath is in the FT as a directory not a file
  * MEMORY_ERROR if memory could not be allocated to complete request
*/
int FT_rmFile(const char *pcPath) {
    int iStatus;
    Node_T oNTargetNode = NULL;

    assert(pcPath != NULL);

    iStatus = FT_findNode(pcPath, &oNTargetNode);
    if (iStatus != SUCCESS)
        return iStatus;

    if (!NodeFT_isFile(oNTargetNode))
        return NOT_A_FILE;

    ulCount -= NodeFT_free(oNTargetNode);
    if (ulCount == 0)
        oNRoot = NULL;

    return SUCCESS;
}

/*---------------------------------------------------------------*/
/* Query Functions                                               */
/*---------------------------------------------------------------*/

/*
  Returns TRUE if the FT contains a directory with absolute path
  pcPath and FALSE if not or if there is an error while checking.
*/
boolean FT_containsDir(const char *pcPath) {
    int iStatus;
    Node_T oNFoundNode = NULL;

    assert(pcPath != NULL);

    iStatus = FT_findNode(pcPath, &oNFoundNode);
    if (iStatus != SUCCESS)
        return FALSE;

    return !NodeFT_isFile(oNFoundNode);
}

/*
  Returns TRUE if the FT contains a file with absolute path
  pcPath and FALSE if not or if there is an error while checking.
*/
boolean FT_containsFile(const char *pcPath) {
    int iStatus;
    Node_T oNFoundNode = NULL;

    assert(pcPath != NULL);

    iStatus = FT_findNode(pcPath, &oNFoundNode);
    if (iStatus != SUCCESS)
        return FALSE;

    return NodeFT_isFile(oNFoundNode);
}

/*
  Returns the contents of the file with absolute path pcPath.
  Returns NULL if unable to complete the request for any reason.

  Note: checking for a non-NULL return is not an appropriate
  contains check, because the contents of a file may be NULL.
*/
void *FT_getFileContents(const char *pcPath) {
    int iStatus;
    Node_T oNFoundNode = NULL;
    void *pvContents = NULL;

    assert(pcPath != NULL);

    iStatus = FT_findNode(pcPath, &oNFoundNode);
    if (iStatus != SUCCESS)
        return NULL;

    if (!NodeFT_isFile(oNFoundNode))
        return NULL;

    iStatus = NodeFT_getContents(oNFoundNode, &pvContents);
    if (iStatus != SUCCESS)
        return NULL;

    return pvContents;
}

/*
  Replaces current contents of the file with absolute path pcPath with
  the parameter pvNewContents of size ulNewLength bytes.
  Returns the old contents if successful. (Note: contents may be NULL.)
  Returns NULL if unable to complete the request for any reason.
*/
void *FT_replaceFileContents(const char *pcPath, void *pvNewContents, size_t ulNewLength) {
    int iStatus;
    Node_T oNFoundNode = NULL;
    void *pvOldContents = NULL;
    size_t ulOldLength = 0;
    void *pvOldContentsCopy = NULL;

    assert(pcPath != NULL);

    iStatus = FT_findNode(pcPath, &oNFoundNode);
    if (iStatus != SUCCESS)
        return NULL;

    if (!NodeFT_isFile(oNFoundNode))
        return NULL;

    /* Get old contents */
    iStatus = NodeFT_getContents(oNFoundNode, &pvOldContents);
    if (iStatus != SUCCESS)
        return NULL;

    iStatus = NodeFT_getContentLength(oNFoundNode, &ulOldLength);
    if (iStatus != SUCCESS)
        return NULL;

    /* Copy old contents */
    if (pvOldContents != NULL && ulOldLength > 0) {
        pvOldContentsCopy = malloc(ulOldLength);
        if (pvOldContentsCopy == NULL)
            return NULL;

        memcpy(pvOldContentsCopy, pvOldContents, ulOldLength);
    }

    /* Set new contents */
    iStatus = NodeFT_setContents(oNFoundNode, pvNewContents, ulNewLength);
    if (iStatus != SUCCESS) {
        free(pvOldContentsCopy);
        return NULL;
    }

    return pvOldContentsCopy;
}

/*
  Returns SUCCESS if pcPath exists in the hierarchy,
  Otherwise, returns:
  * INITIALIZATION_ERROR if the FT is not in an initialized state
  * BAD_PATH if pcPath does not represent a well-formatted path
  * CONFLICTING_PATH if the root's path is not a prefix of pcPath
  * NO_SUCH_PATH if absolute path pcPath does not exist in the FT
  * MEMORY_ERROR if memory could not be allocated to complete request

  When returning SUCCESS,
  if path is a directory: sets *pbIsFile to FALSE, *pulSize unchanged
  if path is a file: sets *pbIsFile to TRUE, and
                     sets *pulSize to the length of file's contents

  When returning another status, *pbIsFile and *pulSize are unchanged.
*/
int FT_stat(const char *pcPath, boolean *pbIsFile, size_t *pulSize) {
    int iStatus;
    Node_T oNFoundNode = NULL;

    assert(pcPath != NULL);
    assert(pbIsFile != NULL);
    assert(pulSize != NULL);

    if (!bIsInitialized)
        return INITIALIZATION_ERROR;

    iStatus = FT_findNode(pcPath, &oNFoundNode);
    if (iStatus != SUCCESS)
        return iStatus;

    *pbIsFile = NodeFT_isFile(oNFoundNode);
    if (*pbIsFile) {
        iStatus = NodeFT_getContentLength(oNFoundNode, pulSize);
        if (iStatus != SUCCESS)
            return iStatus;
    }

    return SUCCESS;
}

/*---------------------------------------------------------------*/
/* Utility Functions                                             */
/*---------------------------------------------------------------*/

/*
  Returns a string representation of the
  data structure, or NULL if the structure is
  not initialized or there is an allocation error.

  The representation is depth-first with files
  before directories at any given level, and nodes
  of the same type ordered lexicographically.

  Allocates memory for the returned string,
  which is then owned by client!
*/
char *FT_toString(void) {
    DynArray_T oDNodes;
    size_t ulTotalStrLen = 1; /* Start with 1 for null terminator */
    char *pcResultStr = NULL;
    size_t ulIndex;

    if (!bIsInitialized)
        return NULL;

    oDNodes = DynArray_new(ulCount);
    if (oDNodes == NULL)
        return NULL;

    (void)FT_preOrderTraversal(oNRoot, oDNodes, 0);

    /* Calculate total string length */
    for (ulIndex = 0; ulIndex < DynArray_getLength(oDNodes); ulIndex++) {
        Node_T oNNode = DynArray_get(oDNodes, ulIndex);
        char *pcNodeStr = NodeFT_toString(oNNode);
        if (pcNodeStr != NULL) {
            ulTotalStrLen += strlen(pcNodeStr) + 1; /* +1 for newline */
            free(pcNodeStr);
        }
    }

    pcResultStr = malloc(ulTotalStrLen);
    if (pcResultStr == NULL) {
        DynArray_free(oDNodes);
        return NULL;
    }
    *pcResultStr = '\0';

    /* Concatenate node strings */
    for (ulIndex = 0; ulIndex < DynArray_getLength(oDNodes); ulIndex++) {
        Node_T oNNode = DynArray_get(oDNodes, ulIndex);
        char *pcNodeStr = NodeFT_toString(oNNode);
        if (pcNodeStr != NULL) {
            strcat(pcResultStr, pcNodeStr);
            strcat(pcResultStr, "\n");
            free(pcNodeStr);
        }
    }

    DynArray_free(oDNodes);

    return pcResultStr;
}