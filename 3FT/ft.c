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
#include "checkerFT.h"
#include "ft.h"

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

/* Helper function prototypes */
static int FT_traversePath(Path_T oPPath, Node_T *poNFurthestNode, boolean *pbIsFile);
static int FT_findNode(const char *pcPath, Node_T *poNResult);
static int FT_handleInsertError(int iStatus, Path_T oPPath, Node_T oNNewNodes);
static size_t FT_preOrderTraversal(Node_T oNNode, DynArray_T oDNodes, size_t ulIndex);

/*---------------------------------------------------------------*/
/* Static Helper Functions                                       */
/*---------------------------------------------------------------*/

/*
  Navigates the File Tree starting from the root node towards the specified path `oPPath`.
  The traversal stops if a file node is encountered, as files cannot have children.
  On success, sets `*poNFurthestNode` to the deepest node reached and `*pbIsFile` to TRUE
  if traversal stopped at a file node. Returns SUCCESS.
  On failure, sets `*poNFurthestNode` to NULL and returns an appropriate error code.
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

/*
  Traverses the File Tree to find a node with absolute path `pcPath`.
  On success, sets `*poNResult` to the found node and returns SUCCESS.
  On failure, sets `*poNResult` to NULL and returns an appropriate error code.
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

    iStatus = Path_new(pcPath, &oPPath);
    if (iStatus != SUCCESS) {
        *poNResult = NULL;
        return iStatus;
    }

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
  Helper function to handle errors during insertion operations.
  Frees allocated resources and returns the provided status code.
*/
static int FT_handleInsertError(int iStatus, Path_T oPPath, Node_T oNNewNodes) {
    Path_free(oPPath);
    if (oNNewNodes != NULL)
        (void)NodeFT_free(oNNewNodes);
    assert(CheckerFT_isValid(bIsInitialized, oNRoot, ulCount));
    return iStatus;
}

/*---------------------------------------------------------------*/
/* Public Interface Functions                                    */
/*---------------------------------------------------------------*/

int FT_init(void) {
    assert(CheckerFT_isValid(bIsInitialized, oNRoot, ulCount));

    if (bIsInitialized)
        return INITIALIZATION_ERROR;

    bIsInitialized = TRUE;
    oNRoot = NULL;
    ulCount = 0;

    assert(CheckerFT_isValid(bIsInitialized, oNRoot, ulCount));
    return SUCCESS;
}

int FT_destroy(void) {
    assert(CheckerFT_isValid(bIsInitialized, oNRoot, ulCount));

    if (!bIsInitialized)
        return INITIALIZATION_ERROR;

    if (oNRoot != NULL) {
        ulCount -= NodeFT_free(oNRoot);
        oNRoot = NULL;
    }

    bIsInitialized = FALSE;

    assert(CheckerFT_isValid(bIsInitialized, oNRoot, ulCount));
    return SUCCESS;
}

/*---------------------------------------------------------------*/
/* Insertion Functions                                           */
/*---------------------------------------------------------------*/

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
    assert(CheckerFT_isValid(bIsInitialized, oNRoot, ulCount));

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

    assert(CheckerFT_isValid(bIsInitialized, oNRoot, ulCount));
    return SUCCESS;
}

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
    assert(CheckerFT_isValid(bIsInitialized, oNRoot, ulCount));

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
        boolean bIsFileNode = (ulIndex == ulDepth);

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

    assert(CheckerFT_isValid(bIsInitialized, oNRoot, ulCount));
    return SUCCESS;
}

/*---------------------------------------------------------------*/
/* Removal Functions                                             */
/*---------------------------------------------------------------*/

int FT_rmDir(const char *pcPath) {
    int iStatus;
    Node_T oNTargetNode = NULL;

    assert(pcPath != NULL);
    assert(CheckerFT_isValid(bIsInitialized, oNRoot, ulCount));

    iStatus = FT_findNode(pcPath, &oNTargetNode);
    if (iStatus != SUCCESS)
        return iStatus;

    if (NodeFT_isFile(oNTargetNode))
        return NOT_A_DIRECTORY;

    ulCount -= NodeFT_free(oNTargetNode);
    if (ulCount == 0)
        oNRoot = NULL;

    assert(CheckerFT_isValid(bIsInitialized, oNRoot, ulCount));
    return SUCCESS;
}

int FT_rmFile(const char *pcPath) {
    int iStatus;
    Node_T oNTargetNode = NULL;

    assert(pcPath != NULL);
    assert(CheckerFT_isValid(bIsInitialized, oNRoot, ulCount));

    iStatus = FT_findNode(pcPath, &oNTargetNode);
    if (iStatus != SUCCESS)
        return iStatus;

    if (!NodeFT_isFile(oNTargetNode))
        return NOT_A_FILE;

    ulCount -= NodeFT_free(oNTargetNode);
    if (ulCount == 0)
        oNRoot = NULL;

    assert(CheckerFT_isValid(bIsInitialized, oNRoot, ulCount));
    return SUCCESS;
}

/*---------------------------------------------------------------*/
/* Query Functions                                               */
/*---------------------------------------------------------------*/

boolean FT_containsDir(const char *pcPath) {
    int iStatus;
    Node_T oNFoundNode = NULL;

    assert(pcPath != NULL);

    iStatus = FT_findNode(pcPath, &oNFoundNode);
    if (iStatus != SUCCESS)
        return FALSE;

    return !NodeFT_isFile(oNFoundNode);
}

boolean FT_containsFile(const char *pcPath) {
    int iStatus;
    Node_T oNFoundNode = NULL;

    assert(pcPath != NULL);

    iStatus = FT_findNode(pcPath, &oNFoundNode);
    if (iStatus != SUCCESS)
        return FALSE;

    return NodeFT_isFile(oNFoundNode);
}

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

void *FT_replaceFileContents(const char *pcPath, void *pvNewContents, size_t ulNewLength) {
    int iStatus;
    Node_T oNFoundNode = NULL;
    void *pvOldContents = NULL;
    size_t ulOldLength = 0;
    void *pvOldContentsCopy = NULL;

    assert(pcPath != NULL);
    assert(pvNewContents != NULL);
    assert(CheckerFT_isValid(bIsInitialized, oNRoot, ulCount));

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
  Helper function for pre-order traversal of the File Tree.
  Inserts each node into `oDNodes` starting at index `ulIndex`.
  Returns the next available index in `oDNodes` after insertion.
*/
static size_t FT_preOrderTraversal(Node_T oNNode, DynArray_T oDNodes, size_t ulIndex) {
    size_t ulChildCount;
    size_t ulNumFileChildren, ulNumDirChildren;
    size_t ulChildIndex;
    Node_T oNChild = NULL;
    int iStatus;

    assert(oDNodes != NULL);

    if (oNNode != NULL) {
        (void)DynArray_set(oDNodes, ulIndex, oNNode);
        ulIndex++;

        if (!NodeFT_isFile(oNNode)) {
            /* Traverse file children */
            ulNumFileChildren = NodeFT_getNumChildren(oNNode, TRUE);
            for (ulChildIndex = 0; ulChildIndex < ulNumFileChildren; ulChildIndex++) {
                iStatus = NodeFT_getChild(oNNode, ulChildIndex, &oNChild, TRUE);
                assert(iStatus == SUCCESS);
                ulIndex = FT_preOrderTraversal(oNChild, oDNodes, ulIndex);
            }

            /* Traverse directory children */
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

    FT_preOrderTraversal(oNRoot, oDNodes, 0);

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