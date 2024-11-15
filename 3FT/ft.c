/*--------------------------------------------------------------------*/
/* ft.c                                                               */
/* Author: anish                                                      */
/*--------------------------------------------------------------------*/

#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "dynarray.h"
#include "path.h"
#include "nodeFT.h"
#include "ft.h"

/*
  the file tree (ft) maintains a hierarchical structure of directories and files.
  it uses three static variables to represent its state.
*/

/* flag indicating whether the file tree has been initialized */
static boolean bIsInitialized;

/* root node of the file tree */
static Node_T oNRoot;

/* total number of nodes in the file tree */
static size_t ulCount;

/* helper function prototypes */
static int FT_traversePath(Path_T oPPath, Node_T *poNFurthestNode, boolean *pbIsFile);
static int FT_findNode(const char *pcPath, Node_T *poNResult);
static int FT_handleInsertError(int iStatus, Path_T oPPath, Node_T oNNewNodes);
static size_t FT_preOrderTraversal(Node_T oNNode, DynArray_T oDNodes, size_t ulIndex);

/*---------------------------------------------------------------*/
/* static helper functions                                       */
/*---------------------------------------------------------------*/

/*
  traverses the file tree starting from the root towards the given path 'oPPath'.
  it stops if it encounters a file node, since files can't have children.

  parameters:
    - oPPath: the target path we want to traverse to
    - poNFurthestNode: where we'll store the deepest node we reach
    - pbIsFile: set to TRUE if we stopped at a file node

  returns:
    - SUCCESS if traversal is successful
    - sets *poNFurthestNode to the deepest node reached
    - sets *pbIsFile to TRUE if traversal stopped at a file node
    - on failure, sets *poNFurthestNode to NULL and returns an error code
*/
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
        /* tree is empty, so we can't traverse */
        *poNFurthestNode = NULL;
        *pbIsFile = FALSE;
        return SUCCESS;
    }

    /* get the first prefix of the path */
    iStatus = Path_prefix(oPPath, 1, &oPPrefixPath);
    if (iStatus != SUCCESS) {
        *poNFurthestNode = NULL;
        return iStatus;
    }

    /* compare the root's path with the first prefix */
    if (Path_comparePath(NodeFT_getPath(oNRoot), oPPrefixPath) != 0) {
        Path_free(oPPrefixPath);
        *poNFurthestNode = NULL;
        return CONFLICTING_PATH;
    }
    Path_free(oPPrefixPath);

    oNCurrNode = oNRoot;
    ulDepth = Path_getDepth(oPPath);
    ulIndex = 2;

    /* traverse the path prefixes */
    while (ulIndex <= ulDepth) {
        iStatus = Path_prefix(oPPath, ulIndex, &oPPrefixPath);
        if (iStatus != SUCCESS) {
            *poNFurthestNode = NULL;
            return iStatus;
        }

        if (NodeFT_isFile(oNCurrNode)) {
            /* can't go further if current node is a file */
            *poNFurthestNode = oNCurrNode;
            *pbIsFile = TRUE;
            Path_free(oPPrefixPath);
            return NOT_A_DIRECTORY;
        }

        /* check for a file child */
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
        /* check for a directory child */
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
            /* no child found, stop traversal */
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

/*
  finds the node in the file tree with the absolute path 'pcPath'.

  parameters:
    - pcPath: the string of the path we're looking for
    - poNResult: where we'll store the found node

  returns:
    - SUCCESS if the node is found
    - sets *poNResult to the found node
    - on failure, sets *poNResult to NULL and returns an error code
*/
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

    /* create a Path_T object from the string */
    iStatus = Path_new(pcPath, &oPPath);
    if (iStatus != SUCCESS) {
        *poNResult = NULL;
        return iStatus;
    }

    /* traverse the tree to find the node */
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

    /* ensure the found node's path matches exactly */
    if (Path_comparePath(NodeFT_getPath(oNFoundNode), oPPath) != 0) {
        Path_free(oPPath);
        *poNResult = NULL;
        return NO_SUCH_PATH;
    }

    Path_free(oPPath);
    *poNResult = oNFoundNode;
    return SUCCESS;
}

/*
  handles errors during insertion operations.
  frees allocated resources and returns the provided status code.

  parameters:
    - iStatus: the error code to return
    - oPPath: the path that was being inserted
    - oNNewNodes: the newly created nodes that need to be freed

  returns:
    - the provided error status
*/
static int FT_handleInsertError(int iStatus, Path_T oPPath, Node_T oNNewNodes) {
    /* free the path and any new nodes created */
    Path_free(oPPath);
    if (oNNewNodes != NULL)
        (void)NodeFT_free(oNNewNodes);
    return iStatus;
}

/*
  helper function for pre-order traversal of the file tree.
  inserts each node into 'oDNodes' starting at index 'ulIndex'.

  parameters:
    - oNNode: the current node being traversed
    - oDNodes: the dynamic array to store nodes
    - ulIndex: the current index in the array

  returns:
    - the next available index in 'oDNodes' after insertion
*/
static size_t FT_preOrderTraversal(Node_T oNNode, DynArray_T oDNodes, size_t ulIndex) {
    size_t ulNumFileChildren, ulNumDirChildren;
    size_t ulChildIndex;
    Node_T oNChild = NULL;
    int iStatus;

    assert(oDNodes != NULL);

    if (oNNode != NULL) {
        /* add the current node to the array */
        (void)DynArray_set(oDNodes, ulIndex, oNNode);
        ulIndex++;

        if (!NodeFT_isFile(oNNode)) {
            /* traverse file children first */
            ulNumFileChildren = NodeFT_getNumChildren(oNNode, TRUE);
            for (ulChildIndex = 0; ulChildIndex < ulNumFileChildren; ulChildIndex++) {
                iStatus = NodeFT_getChild(oNNode, ulChildIndex, &oNChild, TRUE);
                assert(iStatus == SUCCESS);
                ulIndex = FT_preOrderTraversal(oNChild, oDNodes, ulIndex);
            }

            /* then traverse directory children */
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
/* public interface functions                                    */
/*---------------------------------------------------------------*/

/*
  sets the ft data structure to an initialized state.
  the data structure is initially empty.

  returns:
    - SUCCESS if initialization is successful
    - INITIALIZATION_ERROR if the tree is already initialized
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
  removes all contents of the data structure and
  returns it to an uninitialized state.

  returns:
    - SUCCESS if destruction is successful
    - INITIALIZATION_ERROR if the tree is not initialized
*/
int FT_destroy(void) {
    if (!bIsInitialized)
        return INITIALIZATION_ERROR;

    if (oNRoot != NULL) {
        /* free all nodes in the tree */
        ulCount -= NodeFT_free(oNRoot);
        oNRoot = NULL;
    }

    bIsInitialized = FALSE;

    return SUCCESS;
}

/*---------------------------------------------------------------*/
/* insertion functions                                           */
/*---------------------------------------------------------------*/

/*
  inserts a new directory into the ft with absolute path 'pcPath'.

  parameters:
    - pcPath: the absolute path of the directory to insert

  returns:
    - SUCCESS if the directory is inserted successfully
    - appropriate error code otherwise
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

    /* create a Path_T object from the string */
    iStatus = Path_new(pcPath, &oPPath);
    if (iStatus != SUCCESS)
        return iStatus;

    /* traverse the tree to find how far we can go */
    iStatus = FT_traversePath(oPPath, &oNCurrNode, &bIsFile);
    if (iStatus != SUCCESS) {
        Path_free(oPPath);
        return iStatus;
    }

    if (oNCurrNode == NULL && oNRoot != NULL) {
        /* path conflicts with existing root */
        Path_free(oPPath);
        return CONFLICTING_PATH;
    }

    if (bIsFile) {
        /* can't insert under a file */
        Path_free(oPPath);
        return NOT_A_DIRECTORY;
    }

    ulDepth = Path_getDepth(oPPath);
    ulCurrDepth = oNCurrNode ? Path_getDepth(NodeFT_getPath(oNCurrNode)) : 0;
    ulIndex = ulCurrDepth + 1;

    if (ulCurrDepth == ulDepth) {
        /* the directory already exists */
        if (Path_comparePath(NodeFT_getPath(oNCurrNode), oPPath) == 0) {
            Path_free(oPPath);
            return ALREADY_IN_TREE;
        }
    }

    /* build the path from oNCurrNode towards oPPath */
    while (ulIndex <= ulDepth) {
        Path_T oPPrefixPath = NULL;
        Node_T oNNewNode = NULL;

        /* get the next prefix */
        iStatus = Path_prefix(oPPath, ulIndex, &oPPrefixPath);
        if (iStatus != SUCCESS) {
            return FT_handleInsertError(iStatus, oPPath, oNNewNodes);
        }

        /* create a new directory node */
        iStatus = NodeFT_new(oPPrefixPath, oNCurrNode, FALSE, &oNNewNode);
        Path_free(oPPrefixPath);
        if (iStatus != SUCCESS) {
            return FT_handleInsertError(iStatus, oPPath, oNNewNodes);
        }

        if (oNNewNodes == NULL)
            oNNewNodes = oNNewNode; /* keep track of the first new node */

        oNCurrNode = oNNewNode;
        ulNewNodesCount++;
        ulIndex++;
    }

    Path_free(oPPath);

    if (oNRoot == NULL)
        oNRoot = oNNewNodes; /* set the new root if needed */
    ulCount += ulNewNodesCount;

    return SUCCESS;
}

/*
  inserts a new file into the ft with absolute path 'pcPath',
  with file contents 'pvContents' of size 'ulLength' bytes.

  parameters:
    - pcPath: the absolute path of the file to insert
    - pvContents: pointer to the file contents
    - ulLength: length of the file contents in bytes

  returns:
    - SUCCESS if the file is inserted successfully
    - appropriate error code otherwise
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

    /* create a Path_T object from the string */
    iStatus = Path_new(pcPath, &oPPath);
    if (iStatus != SUCCESS)
        return iStatus;

    if (Path_getDepth(oPPath) == 1) {
        /* cannot insert a file at the root */
        Path_free(oPPath);
        return CONFLICTING_PATH;
    }

    /* traverse the tree to find how far we can go */
    iStatus = FT_traversePath(oPPath, &oNCurrNode, &bIsFile);
    if (iStatus != SUCCESS) {
        Path_free(oPPath);
        return iStatus;
    }

    if (oNCurrNode == NULL) {
        /* path conflicts with existing root */
        Path_free(oPPath);
        return CONFLICTING_PATH;
    }

    if (bIsFile) {
        /* can't insert under a file */
        Path_free(oPPath);
        return NOT_A_DIRECTORY;
    }

    ulDepth = Path_getDepth(oPPath);
    ulCurrDepth = oNCurrNode ? Path_getDepth(NodeFT_getPath(oNCurrNode)) : 0;
    ulIndex = ulCurrDepth + 1;

    if (ulCurrDepth == ulDepth) {
        /* the file already exists */
        if (Path_comparePath(NodeFT_getPath(oNCurrNode), oPPath) == 0) {
            Path_free(oPPath);
            return ALREADY_IN_TREE;
        }
    }

    /* build the path from oNCurrNode towards oPPath */
    while (ulIndex <= ulDepth) {
        Path_T oPPrefixPath = NULL;
        Node_T oNNewNode = NULL;
        boolean bIsFileNode = (ulIndex == ulDepth);

        /* get the next prefix */
        iStatus = Path_prefix(oPPath, ulIndex, &oPPrefixPath);
        if (iStatus != SUCCESS) {
            return FT_handleInsertError(iStatus, oPPath, oNNewNodes);
        }

        /* create a new node (file or directory) */
        iStatus = NodeFT_new(oPPrefixPath, oNCurrNode, bIsFileNode, &oNNewNode);
        Path_free(oPPrefixPath);
        if (iStatus != SUCCESS) {
            return FT_handleInsertError(iStatus, oPPath, oNNewNodes);
        }

        if (bIsFileNode) {
            /* set the contents of the new file */
            iStatus = NodeFT_setContents(oNNewNode, pvContents, ulLength);
            if (iStatus != SUCCESS) {
                return FT_handleInsertError(iStatus, oPPath, oNNewNodes);
            }
        }

        if (oNNewNodes == NULL)
            oNNewNodes = oNNewNode; /* keep track of the first new node */

        oNCurrNode = oNNewNode;
        ulNewNodesCount++;
        ulIndex++;
    }

    Path_free(oPPath);

    if (oNRoot == NULL)
        oNRoot = oNNewNodes; /* set the new root if needed */
    ulCount += ulNewNodesCount;

    return SUCCESS;
}

/*---------------------------------------------------------------*/
/* removal functions                                             */
/*---------------------------------------------------------------*/

/*
  removes the ft hierarchy (subtree) at the directory with absolute path 'pcPath'.

  parameters:
    - pcPath: the absolute path of the directory to remove

  returns:
    - SUCCESS if the directory is found and removed
    - appropriate error code otherwise
*/
int FT_rmDir(const char *pcPath) {
    int iStatus;
    Node_T oNTargetNode = NULL;

    assert(pcPath != NULL);

    /* find the node to remove */
    iStatus = FT_findNode(pcPath, &oNTargetNode);
    if (iStatus != SUCCESS)
        return iStatus;

    if (NodeFT_isFile(oNTargetNode))
        return NOT_A_DIRECTORY;

    /* remove the node and its subtree */
    ulCount -= NodeFT_free(oNTargetNode);
    if (ulCount == 0)
        oNRoot = NULL;

    return SUCCESS;
}

/*
  removes the ft file with absolute path 'pcPath'.

  parameters:
    - pcPath: the absolute path of the file to remove

  returns:
    - SUCCESS if the file is found and removed
    - appropriate error code otherwise
*/
int FT_rmFile(const char *pcPath) {
    int iStatus;
    Node_T oNTargetNode = NULL;

    assert(pcPath != NULL);

    /* find the node to remove */
    iStatus = FT_findNode(pcPath, &oNTargetNode);
    if (iStatus != SUCCESS)
        return iStatus;

    if (!NodeFT_isFile(oNTargetNode))
        return NOT_A_FILE;

    /* remove the node */
    ulCount -= NodeFT_free(oNTargetNode);
    if (ulCount == 0)
        oNRoot = NULL;

    return SUCCESS;
}

/*---------------------------------------------------------------*/
/* query functions                                               */
/*---------------------------------------------------------------*/

/*
  returns TRUE if the ft contains a directory with absolute path 'pcPath'.

  parameters:
    - pcPath: the absolute path to check

  returns:
    - TRUE if the directory exists
    - FALSE if not or if there is an error
*/
boolean FT_containsDir(const char *pcPath) {
    int iStatus;
    Node_T oNFoundNode = NULL;

    assert(pcPath != NULL);

    /* find the node */
    iStatus = FT_findNode(pcPath, &oNFoundNode);
    if (iStatus != SUCCESS)
        return FALSE;

    /* check if it's a directory */
    return !NodeFT_isFile(oNFoundNode);
}

/*
  returns TRUE if the ft contains a file with absolute path 'pcPath'.

  parameters:
    - pcPath: the absolute path to check

  returns:
    - TRUE if the file exists
    - FALSE if not or if there is an error
*/
boolean FT_containsFile(const char *pcPath) {
    int iStatus;
    Node_T oNFoundNode = NULL;

    assert(pcPath != NULL);

    /* find the node */
    iStatus = FT_findNode(pcPath, &oNFoundNode);
    if (iStatus != SUCCESS)
        return FALSE;

    /* check if it's a file */
    return NodeFT_isFile(oNFoundNode);
}

/*
  returns the contents of the file with absolute path 'pcPath'.

  parameters:
    - pcPath: the absolute path of the file

  returns:
    - pointer to the file contents if successful
    - NULL if unable to complete the request
*/
void *FT_getFileContents(const char *pcPath) {
    int iStatus;
    Node_T oNFoundNode = NULL;
    void *pvContents = NULL;

    assert(pcPath != NULL);

    /* find the node */
    iStatus = FT_findNode(pcPath, &oNFoundNode);
    if (iStatus != SUCCESS)
        return NULL;

    if (!NodeFT_isFile(oNFoundNode))
        return NULL;

    /* get the contents */
    iStatus = NodeFT_getContents(oNFoundNode, &pvContents);
    if (iStatus != SUCCESS)
        return NULL;

    return pvContents;
}

/*
  replaces current contents of the file with absolute path 'pcPath' with
  the parameter 'pvNewContents' of size 'ulNewLength' bytes.

  parameters:
    - pcPath: the absolute path of the file
    - pvNewContents: pointer to the new contents
    - ulNewLength: length of the new contents in bytes

  returns:
    - pointer to the old contents if successful
    - NULL if unable to complete the request
*/
void *FT_replaceFileContents(const char *pcPath, void *pvNewContents, size_t ulNewLength) {
    int iStatus;
    Node_T oNFoundNode = NULL;
    void *pvOldContents = NULL;
    size_t ulOldLength = 0;
    void *pvOldContentsCopy = NULL;

    assert(pcPath != NULL);

    /* find the node */
    iStatus = FT_findNode(pcPath, &oNFoundNode);
    if (iStatus != SUCCESS)
        return NULL;

    if (!NodeFT_isFile(oNFoundNode))
        return NULL;

    /* get old contents */
    iStatus = NodeFT_getContents(oNFoundNode, &pvOldContents);
    if (iStatus != SUCCESS)
        return NULL;

    iStatus = NodeFT_getContentLength(oNFoundNode, &ulOldLength);
    if (iStatus != SUCCESS)
        return NULL;

    /* copy old contents */
    if (pvOldContents != NULL && ulOldLength > 0) {
        pvOldContentsCopy = malloc(ulOldLength);
        if (pvOldContentsCopy == NULL)
            return NULL;

        memcpy(pvOldContentsCopy, pvOldContents, ulOldLength);
    }

    /* set new contents */
    iStatus = NodeFT_setContents(oNFoundNode, pvNewContents, ulNewLength);
    if (iStatus != SUCCESS) {
        free(pvOldContentsCopy);
        return NULL;
    }

    return pvOldContentsCopy;
}

/*
  gets information about the node at absolute path 'pcPath'.

  parameters:
    - pcPath: the absolute path of the node
    - pbIsFile: pointer to store TRUE if node is a file, FALSE if directory
    - pulSize: pointer to store the size of the file contents (if node is a file)

  returns:
    - SUCCESS if information is retrieved successfully
    - appropriate error code otherwise
*/
int FT_stat(const char *pcPath, boolean *pbIsFile, size_t *pulSize) {
    int iStatus;
    Node_T oNFoundNode = NULL;

    assert(pcPath != NULL);
    assert(pbIsFile != NULL);
    assert(pulSize != NULL);

    if (!bIsInitialized)
        return INITIALIZATION_ERROR;

    /* find the node */
    iStatus = FT_findNode(pcPath, &oNFoundNode);
    if (iStatus != SUCCESS)
        return iStatus;

    *pbIsFile = NodeFT_isFile(oNFoundNode);
    if (*pbIsFile) {
        /* get the size of the file contents */
        iStatus = NodeFT_getContentLength(oNFoundNode, pulSize);
        if (iStatus != SUCCESS)
            return iStatus;
    }

    return SUCCESS;
}

/*---------------------------------------------------------------*/
/* utility functions                                             */
/*---------------------------------------------------------------*/

/*
  returns a string representation of the data structure,
  or NULL if the structure is not initialized or there is an allocation error.

  returns:
    - pointer to the string representation
    - caller is responsible for freeing the allocated memory
    - NULL if unable to create the string
*/
char *FT_toString(void) {
    DynArray_T oDNodes;
    size_t ulTotalStrLen = 1; /* start with 1 for null terminator */
    char *pcResultStr = NULL;
    size_t ulIndex;

    if (!bIsInitialized)
        return NULL;

    /* create a dynamic array to hold nodes */
    oDNodes = DynArray_new(ulCount);
    if (oDNodes == NULL)
        return NULL;

    /* perform a pre-order traversal to fill the array */
    FT_preOrderTraversal(oNRoot, oDNodes, 0);

    /* calculate total string length */
    for (ulIndex = 0; ulIndex < DynArray_getLength(oDNodes); ulIndex++) {
        Node_T oNNode = DynArray_get(oDNodes, ulIndex);
        char *pcNodeStr = NodeFT_toString(oNNode);
        if (pcNodeStr != NULL) {
            ulTotalStrLen += strlen(pcNodeStr) + 1; /* +1 for newline */
            free(pcNodeStr);
        }
    }

    /* allocate memory for the result string */
    pcResultStr = malloc(ulTotalStrLen);
    if (pcResultStr == NULL) {
        DynArray_free(oDNodes);
        return NULL;
    }
    *pcResultStr = '\0';

    /* concatenate node strings */
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