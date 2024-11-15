/*--------------------------------------------------------------------*/
/* ft.c                                                               */
/* Author: anish                                                     */
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
  The File Tree (FT) maintains a hierarchy of directories and files.
  It uses three static variables to represent its state.
*/

/* Indicates whether the FT has been initialized */
static boolean isInitialized;

/* The root node of the FT */
static Node_T root;

/* The total number of nodes in the FT */
static size_t nodeCount;

/*
  Traverses the FT from the root towards `oPPath` as far as possible.
  Stops traversal if a file node is encountered since files cannot have children.
  On success, sets `*poNFurthest` to the furthest node reached and `*pbIsFile` to TRUE
  if traversal stopped due to a file node. Returns SUCCESS.
  On failure, sets `*poNFurthest` to NULL and returns an appropriate error code.
*/
static int FT_traversePath(Path_T oPPath, Node_T *poNFurthest, boolean *pbIsFile) {
    int iStatus;
    Path_T oPPrefix = NULL;
    Node_T currentNode = NULL;
    Node_T childNode = NULL;
    size_t pathDepth, i;
    size_t childIndex = 0;
    boolean isFile = FALSE;

    assert(oPPath != NULL);
    assert(poNFurthest != NULL);
    assert(pbIsFile != NULL);

    if (root == NULL) {
        *poNFurthest = NULL;
        *pbIsFile = FALSE;
        return SUCCESS;
    }

    iStatus = Path_prefix(oPPath, 1, &oPPrefix);
    if (iStatus != SUCCESS) {
        *poNFurthest = NULL;
        return iStatus;
    }

    /* Corrected condition */
    if (Path_comparePath(NodeFT_getPath(root), oPPrefix) != 0) {
        Path_free(oPPrefix);
        *poNFurthest = NULL;
        return CONFLICTING_PATH;
    }
    Path_free(oPPrefix);

    currentNode = root;
    pathDepth = Path_getDepth(oPPath);
    for (i = 2; i <= pathDepth; i++) {
        iStatus = Path_prefix(oPPath, i, &oPPrefix);
        if (iStatus != SUCCESS) {
            *poNFurthest = NULL;
            return iStatus;
        }

        if (NodeFT_isFile(currentNode)) {
            /* Cannot traverse further if current node is a file */
            *poNFurthest = currentNode;
            *pbIsFile = TRUE;
            Path_free(oPPrefix);
            return NOT_A_DIRECTORY;
        }

        /* Check for child directory */
        if (NodeFT_hasChild(currentNode, oPPrefix, &childIndex, FALSE)) {
            iStatus = NodeFT_getChild(currentNode, childIndex, &childNode, FALSE);
            if (iStatus != SUCCESS) {
                Path_free(oPPrefix);
                *poNFurthest = NULL;
                return iStatus;
            }
            currentNode = childNode;
            isFile = FALSE;
        }
        /* Check for child file */
        else if (NodeFT_hasChild(currentNode, oPPrefix, &childIndex, TRUE)) {
            iStatus = NodeFT_getChild(currentNode, childIndex, &childNode, TRUE);
            if (iStatus != SUCCESS) {
                Path_free(oPPrefix);
                *poNFurthest = NULL;
                return iStatus;
            }
            currentNode = childNode;
            isFile = TRUE;
        }
        else {
            /* No child found, traversal ends */
            Path_free(oPPrefix);
            break;
        }

        Path_free(oPPrefix);
    }

    *poNFurthest = currentNode;
    *pbIsFile = isFile;
    return SUCCESS;
}

/*
  Finds the node corresponding to `pcPath` in the FT.
  On success, sets `*poNResult` to the node and returns SUCCESS.
  On failure, sets `*poNResult` to NULL and returns an appropriate error code.
*/
static int FT_findNode(const char *pcPath, Node_T *poNResult) {
    int iStatus;
    Path_T oPPath = NULL;
    Node_T foundNode = NULL;
    boolean isFile = FALSE;

    assert(pcPath != NULL);
    assert(poNResult != NULL);

    if (!isInitialized) {
        *poNResult = NULL;
        return INITIALIZATION_ERROR;
    }

    iStatus = Path_new(pcPath, &oPPath);
    if (iStatus != SUCCESS) {
        *poNResult = NULL;
        return iStatus;
    }

    iStatus = FT_traversePath(oPPath, &foundNode, &isFile);
    if (iStatus != SUCCESS) {
        Path_free(oPPath);
        *poNResult = NULL;
        return iStatus;
    }

    if (foundNode == NULL) {
        Path_free(oPPath);
        *poNResult = NULL;
        return NO_SUCH_PATH;
    }

    if (Path_comparePath(NodeFT_getPath(foundNode), oPPath) != 0) {
        Path_free(oPPath);
        *poNResult = NULL;
        return NO_SUCH_PATH;
    }

    Path_free(oPPath);
    *poNResult = foundNode;
    return SUCCESS;
}

int FT_init(void) {
    assert(CheckerFT_isValid(isInitialized, root, nodeCount));

    if (isInitialized)
        return INITIALIZATION_ERROR;

    isInitialized = TRUE;
    root = NULL;
    nodeCount = 0;

    assert(CheckerFT_isValid(isInitialized, root, nodeCount));
    return SUCCESS;
}

int FT_destroy(void) {
    assert(CheckerFT_isValid(isInitialized, root, nodeCount));

    if (!isInitialized)
        return INITIALIZATION_ERROR;

    if (root != NULL) {
        nodeCount -= NodeFT_free(root);
        root = NULL;
    }

    isInitialized = FALSE;

    assert(CheckerFT_isValid(isInitialized, root, nodeCount));
    return SUCCESS;
}

int FT_insertDir(const char *pcPath) {
    int iStatus;
    Path_T oPPath = NULL;
    Node_T currentNode = NULL;
    Node_T firstNewNode = NULL;
    size_t pathDepth, currDepth;
    size_t newNodesCount = 0;
    boolean isFile = FALSE;
    size_t i; /* Loop variable */

    assert(pcPath != NULL);
    assert(CheckerFT_isValid(isInitialized, root, nodeCount));

    if (!isInitialized)
        return INITIALIZATION_ERROR;

    iStatus = Path_new(pcPath, &oPPath);
    if (iStatus != SUCCESS)
        return iStatus;

    /* Traverse the tree towards oPPath */
    iStatus = FT_traversePath(oPPath, &currentNode, &isFile);
    if (iStatus != SUCCESS) {
        Path_free(oPPath);
        return iStatus;
    }

    if (currentNode == NULL && root != NULL) {
        Path_free(oPPath);
        return CONFLICTING_PATH;
    }

    if (isFile) {
        Path_free(oPPath);
        return NOT_A_DIRECTORY;
    }

    pathDepth = Path_getDepth(oPPath);

    if (currentNode == NULL)
        currDepth = 0;
    else
        currDepth = Path_getDepth(NodeFT_getPath(currentNode));

    if (currDepth == pathDepth) {
        if (Path_comparePath(NodeFT_getPath(currentNode), oPPath) == 0) {
            Path_free(oPPath);
            return ALREADY_IN_TREE;
        }
    }

    /* Build the path from currentNode towards oPPath */
    for (i = currDepth + 1; i <= pathDepth; i++) {
        Path_T prefixPath = NULL;
        Node_T newChild = NULL;

        iStatus = Path_prefix(oPPath, i, &prefixPath);
        if (iStatus != SUCCESS) {
            Path_free(oPPath);
            if (firstNewNode != NULL)
                NodeFT_free(firstNewNode);
            assert(CheckerFT_isValid(isInitialized, root, nodeCount));
            return iStatus;
        }

        iStatus = NodeFT_new(prefixPath, currentNode, FALSE, &newChild);
        Path_free(prefixPath);
        if (iStatus != SUCCESS) {
            Path_free(oPPath);
            if (firstNewNode != NULL)
                NodeFT_free(firstNewNode);
            assert(CheckerFT_isValid(isInitialized, root, nodeCount));
            return iStatus;
        }

        if (firstNewNode == NULL)
            firstNewNode = newChild;

        currentNode = newChild;
        newNodesCount++;
    }

    Path_free(oPPath);

    if (root == NULL)
        root = firstNewNode;
    nodeCount += newNodesCount;

    assert(CheckerFT_isValid(isInitialized, root, nodeCount));
    return SUCCESS;
}

boolean FT_containsDir(const char *pcPath) {
    int iStatus;
    Node_T foundNode = NULL;

    assert(pcPath != NULL);

    iStatus = FT_findNode(pcPath, &foundNode);
    if (iStatus != SUCCESS)
        return FALSE;

    return !NodeFT_isFile(foundNode);
}

int FT_rmDir(const char *pcPath) {
    int iStatus;
    Node_T targetNode = NULL;

    assert(pcPath != NULL);
    assert(CheckerFT_isValid(isInitialized, root, nodeCount));

    iStatus = FT_findNode(pcPath, &targetNode);
    if (iStatus != SUCCESS)
        return iStatus;

    if (NodeFT_isFile(targetNode))
        return NOT_A_DIRECTORY;

    nodeCount -= NodeFT_free(targetNode);
    if (nodeCount == 0)
        root = NULL;

    assert(CheckerFT_isValid(isInitialized, root, nodeCount));
    return SUCCESS;
}

int FT_insertFile(const char *pcPath, void *pvContents, size_t ulLength) {
    int iStatus;
    Path_T oPPath = NULL;
    Node_T currentNode = NULL;
    Node_T firstNewNode = NULL;
    size_t pathDepth, currDepth;
    size_t newNodesCount = 0;
    boolean isFile = FALSE;
    size_t i; /* Loop variable */

    assert(pcPath != NULL);
    assert(CheckerFT_isValid(isInitialized, root, nodeCount));

    if (!isInitialized)
        return INITIALIZATION_ERROR;

    iStatus = Path_new(pcPath, &oPPath);
    if (iStatus != SUCCESS)
        return iStatus;

    if (Path_getDepth(oPPath) == 1) {
        Path_free(oPPath);
        return CONFLICTING_PATH;
    }

    iStatus = FT_traversePath(oPPath, &currentNode, &isFile);
    if (iStatus != SUCCESS) {
        Path_free(oPPath);
        return iStatus;
    }

    if (currentNode == NULL) {
        Path_free(oPPath);
        return CONFLICTING_PATH;
    }

    if (isFile) {
        Path_free(oPPath);
        return NOT_A_DIRECTORY;
    }

    pathDepth = Path_getDepth(oPPath);
    currDepth = Path_getDepth(NodeFT_getPath(currentNode));

    if (currDepth == pathDepth) {
        if (Path_comparePath(NodeFT_getPath(currentNode), oPPath) == 0) {
            Path_free(oPPath);
            return ALREADY_IN_TREE;
        }
    }

    /* Build the path from currentNode towards oPPath */
    for (i = currDepth + 1; i <= pathDepth; i++) {
        Path_T prefixPath = NULL;
        Node_T newChild = NULL;
        boolean createFileNode = (i == pathDepth);

        iStatus = Path_prefix(oPPath, i, &prefixPath);
        if (iStatus != SUCCESS) {
            Path_free(oPPath);
            if (firstNewNode != NULL)
                NodeFT_free(firstNewNode);
            assert(CheckerFT_isValid(isInitialized, root, nodeCount));
            return iStatus;
        }

        iStatus = NodeFT_new(prefixPath, currentNode, createFileNode, &newChild);
        Path_free(prefixPath);
        if (iStatus != SUCCESS) {
            Path_free(oPPath);
            if (firstNewNode != NULL)
                NodeFT_free(firstNewNode);
            assert(CheckerFT_isValid(isInitialized, root, nodeCount));
            return iStatus;
        }

        if (createFileNode) {
            iStatus = NodeFT_setContents(newChild, pvContents, ulLength);
            if (iStatus != SUCCESS) {
                Path_free(oPPath);
                if (firstNewNode != NULL)
                    NodeFT_free(firstNewNode);
                assert(CheckerFT_isValid(isInitialized, root, nodeCount));
                return iStatus;
            }
        }

        if (firstNewNode == NULL)
            firstNewNode = newChild;

        currentNode = newChild;
        newNodesCount++;
    }

    Path_free(oPPath);

    if (root == NULL)
        root = firstNewNode;
    nodeCount += newNodesCount;

    assert(CheckerFT_isValid(isInitialized, root, nodeCount));
    return SUCCESS;
}

boolean FT_containsFile(const char *pcPath) {
    int iStatus;
    Node_T foundNode = NULL;

    assert(pcPath != NULL);

    iStatus = FT_findNode(pcPath, &foundNode);
    if (iStatus != SUCCESS)
        return FALSE;

    return NodeFT_isFile(foundNode);
}

int FT_rmFile(const char *pcPath) {
    int iStatus;
    Node_T targetNode = NULL;

    assert(pcPath != NULL);
    assert(CheckerFT_isValid(isInitialized, root, nodeCount));

    iStatus = FT_findNode(pcPath, &targetNode);
    if (iStatus != SUCCESS)
        return iStatus;

    if (!NodeFT_isFile(targetNode))
        return NOT_A_FILE;

    nodeCount -= NodeFT_free(targetNode);
    if (nodeCount == 0)
        root = NULL;

    assert(CheckerFT_isValid(isInitialized, root, nodeCount));
    return SUCCESS;
}

void *FT_getFileContents(const char *pcPath) {
    int iStatus;
    Node_T foundNode = NULL;
    void *pvContents = NULL;

    assert(pcPath != NULL);

    iStatus = FT_findNode(pcPath, &foundNode);
    if (iStatus != SUCCESS)
        return NULL;

    if (!NodeFT_isFile(foundNode))
        return NULL;

    iStatus = NodeFT_getContents(foundNode, &pvContents);
    if (iStatus != SUCCESS)
        return NULL;

    return pvContents;
}

void *FT_replaceFileContents(const char *pcPath, void *pvNewContents, size_t ulNewLength) {
    int iStatus;
    Node_T foundNode = NULL;
    void *pvOldContents = NULL;
    size_t ulOldLength = 0;
    void *pvOldContentsCopy = NULL;

    assert(pcPath != NULL);
    assert(pvNewContents != NULL);
    assert(CheckerFT_isValid(isInitialized, root, nodeCount));

    iStatus = FT_findNode(pcPath, &foundNode);
    if (iStatus != SUCCESS)
        return NULL;

    if (!NodeFT_isFile(foundNode))
        return NULL;

    /* Get old contents */
    iStatus = NodeFT_getContents(foundNode, &pvOldContents);
    if (iStatus != SUCCESS)
        return NULL;

    iStatus = NodeFT_getContentLength(foundNode, &ulOldLength);
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
    iStatus = NodeFT_setContents(foundNode, pvNewContents, ulNewLength);
    if (iStatus != SUCCESS) {
        free(pvOldContentsCopy);
        return NULL;
    }

    return pvOldContentsCopy;
}

int FT_stat(const char *pcPath, boolean *pbIsFile, size_t *pulSize) {
    int iStatus;
    Node_T foundNode = NULL;

    assert(pcPath != NULL);
    assert(pbIsFile != NULL);
    assert(pulSize != NULL);

    if (!isInitialized)
        return INITIALIZATION_ERROR;

    iStatus = FT_findNode(pcPath, &foundNode);
    if (iStatus != SUCCESS)
        return iStatus;

    *pbIsFile = NodeFT_isFile(foundNode);
    if (*pbIsFile) {
        iStatus = NodeFT_getContentLength(foundNode, pulSize);
        if (iStatus != SUCCESS)
            return iStatus;
    }

    return SUCCESS;
}

/*
  Helper function for pre-order traversal of the FT.
  Inserts each node into `dynArray` starting at index `index`.
  Returns the next available index in `dynArray` after insertion.
*/
static size_t FT_preOrderTraversal(Node_T node, DynArray_T dynArray, size_t index) {
    size_t i;
    size_t numFileChildren, numDirChildren;

    assert(dynArray != NULL);

    if (node != NULL) {
        DynArray_set(dynArray, index, node);
        index++;

        if (!NodeFT_isFile(node)) {
            /* Traverse file children */
            numFileChildren = NodeFT_getNumChildren(node, TRUE);
            for (i = 0; i < numFileChildren; i++) {
                Node_T childNode = NULL;
                int iStatus = NodeFT_getChild(node, i, &childNode, TRUE);
                assert(iStatus == SUCCESS);
                index = FT_preOrderTraversal(childNode, dynArray, index);
            }

            /* Traverse directory children */
            numDirChildren = NodeFT_getNumChildren(node, FALSE);
            for (i = 0; i < numDirChildren; i++) {
                Node_T childNode = NULL;
                int iStatus = NodeFT_getChild(node, i, &childNode, FALSE);
                assert(iStatus == SUCCESS);
                index = FT_preOrderTraversal(childNode, dynArray, index);
            }
        }
    }
    return index;
}

char *FT_toString(void) {
    DynArray_T nodeArray;
    size_t totalStrLen = 1; /* Start with 1 for null terminator */
    char *resultStr = NULL;
    size_t i;

    if (!isInitialized)
        return NULL;

    nodeArray = DynArray_new(nodeCount);
    if (nodeArray == NULL)
        return NULL;

    FT_preOrderTraversal(root, nodeArray, 0);

    /* Calculate total string length */
    for (i = 0; i < DynArray_getLength(nodeArray); i++) {
        Node_T node = DynArray_get(nodeArray, i);
        char *nodeStr = NodeFT_toString(node);
        if (nodeStr != NULL) {
            totalStrLen += strlen(nodeStr) + 1; /* +1 for newline */
            free(nodeStr);
        }
    }

    resultStr = malloc(totalStrLen);
    if (resultStr == NULL) {
        DynArray_free(nodeArray);
        return NULL;
    }
    *resultStr = '\0';

    /* Concatenate node strings */
    for (i = 0; i < DynArray_getLength(nodeArray); i++) {
        Node_T node = DynArray_get(nodeArray, i);
        char *nodeStr = NodeFT_toString(node);
        if (nodeStr != NULL) {
            strcat(resultStr, nodeStr);
            strcat(resultStr, "\n");
            free(nodeStr);
        }
    }

    DynArray_free(nodeArray);

    return resultStr;
}