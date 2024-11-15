/*--------------------------------------------------------------------*/
/* nodeFT.c                                                           */
/* Author: anish                                                      */
/*--------------------------------------------------------------------*/

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "dynarray.h"
#include "path.h"
#include "nodeFT.h"

/* Definition of the Node_T structure */
struct node {
    /* The path associated with this node */
    Path_T path;
    /* Pointer to the parent node */
    Node_T parent;
    /* Indicator of whether this node represents a file (TRUE) or directory (FALSE) */
    boolean isFile;
    /* Dynamic array of directory children */
    DynArray_T dirChildren;
    /* Dynamic array of file children */
    DynArray_T fileChildren;
    /* Contents of the file (only valid if isFile is TRUE) */
    void *contents;
    /* Length of the contents (only valid if isFile is TRUE) */
    size_t contentLength;
};

/*---------------------------------------------------------------*/
/* Static Helper Functions                                       */
/*---------------------------------------------------------------*/

/*
  Comparison function used for sorting and searching nodes in child arrays.
  Compares the paths of two nodes.
*/
static int NodeFT_compareNodes(const Node_T firstNode, const Node_T secondNode) {
    assert(firstNode != NULL);
    assert(secondNode != NULL);

    return Path_comparePath(firstNode->path, secondNode->path);
}

/*
  Comparison function used for searching in the child arrays.
  Compares the paths of a node and a string.
*/
static int NodeFT_comparePathString(const void *nodePtr, const void *pathStrPtr) {
    const Node_T node = (const Node_T)nodePtr;
    const char *pathStr = (const char *)pathStrPtr;

    assert(node != NULL);
    assert(pathStr != NULL);

    return Path_compareString(node->path, pathStr);
}

/*
  Helper function to initialize a new node.
  Allocates memory, duplicates the path, and sets initial values.
*/
static int NodeFT_initializeNode(Path_T oPPath, Node_T oNParent, boolean bIsFile, Node_T *poNResult) {
    Node_T newNode = NULL;
    Path_T pathCopy = NULL;
    int status;

    assert(oPPath != NULL);
    assert(poNResult != NULL);

    /* Allocate memory for the new node */
    newNode = (Node_T)malloc(sizeof(struct node));
    if (newNode == NULL) {
        *poNResult = NULL;
        return MEMORY_ERROR;
    }

    /* Duplicate the path */
    status = Path_dup(oPPath, &pathCopy);
    if (status != SUCCESS) {
        free(newNode);
        *poNResult = NULL;
        return status;
    }

    /* Initialize the new node's fields */
    newNode->path = pathCopy;
    newNode->parent = oNParent;
    newNode->isFile = bIsFile;
    newNode->contents = NULL;
    newNode->contentLength = 0;

    if (bIsFile) {
        /* Files do not have children */
        newNode->dirChildren = NULL;
        newNode->fileChildren = NULL;
    } else {
        /* Directories have separate arrays for dir and file children */
        newNode->dirChildren = DynArray_new(0);
        newNode->fileChildren = DynArray_new(0);
        if (newNode->dirChildren == NULL || newNode->fileChildren == NULL) {
            if (newNode->dirChildren)
                DynArray_free(newNode->dirChildren);
            if (newNode->fileChildren)
                DynArray_free(newNode->fileChildren);
            Path_free(newNode->path);
            free(newNode);
            *poNResult = NULL;
            return MEMORY_ERROR;
        }
    }

    *poNResult = newNode;
    return SUCCESS;
}

/*
  Helper function to validate the parent-child relationship.
  Ensures the new node is directly under the parent and paths are consistent.
*/
static int NodeFT_validateParentChild(Node_T oNParent, Node_T newNode) {
    size_t parentDepth, sharedDepth;

    assert(newNode != NULL);

    if (oNParent == NULL) {
        /* If there is no parent, this must be the root node */
        if (Path_getDepth(newNode->path) != 1)
            return NO_SUCH_PATH;
    } else {
        parentDepth = Path_getDepth(oNParent->path);
        sharedDepth = Path_getSharedPrefixDepth(newNode->path, oNParent->path);

        /* Parent's path must be a prefix of the new node's path */
        if (sharedDepth < parentDepth)
            return CONFLICTING_PATH;

        /* New node must be directly under the parent */
        if (Path_getDepth(newNode->path) != parentDepth + 1)
            return NO_SUCH_PATH;
    }

    return SUCCESS;
}

/*
  Helper function to insert a child node into the appropriate child array.
  Returns SUCCESS on success, or an appropriate error code on failure.
*/
static int NodeFT_insertChild(Node_T parentNode, Node_T childNode, boolean isFile) {
    size_t childIndex = 0;
    int insertStatus;
    DynArray_T childArray;

    assert(parentNode != NULL);
    assert(childNode != NULL);
    assert(!parentNode->isFile); /* Parent must be a directory */

    /* Choose the correct child array based on the node type */
    if (isFile)
        childArray = parentNode->fileChildren;
    else
        childArray = parentNode->dirChildren;

    /* Check for duplicate child */
    if (NodeFT_hasChild(parentNode, childNode->path, &childIndex, isFile))
        return ALREADY_IN_TREE;

    /* Insert the child into the array at the correct position */
    insertStatus = DynArray_addAt(childArray, childIndex, childNode);
    if (insertStatus == TRUE)
        return SUCCESS;
    else
        return MEMORY_ERROR;
}

/*
  Helper function to remove a node from its parent's child array.
*/
static void NodeFT_removeFromParent(Node_T oNNode) {
    DynArray_T childArray;
    size_t childIndex = 0;
    int found;

    assert(oNNode != NULL);

    if (oNNode->parent != NULL) {
        /* Choose the correct child array */
        if (oNNode->isFile)
            childArray = oNNode->parent->fileChildren;
        else
            childArray = oNNode->parent->dirChildren;

        /* Find and remove the node from the array */
        found = DynArray_bsearch(childArray, oNNode, &childIndex,
            (int (*)(const void *, const void *))NodeFT_compareNodes);
        if (found)
            DynArray_removeAt(childArray, childIndex);
    }
}

/*---------------------------------------------------------------*/
/* Public Interface Functions                                    */
/*---------------------------------------------------------------*/

/* See nodeFT.h for specification */
int NodeFT_new(Path_T oPPath, Node_T oNParent, boolean bIsFile, Node_T *poNResult) {
    Node_T newNode = NULL;
    int status;

    assert(oPPath != NULL);
    assert(poNResult != NULL);

    /* Initialize the new node */
    status = NodeFT_initializeNode(oPPath, oNParent, bIsFile, &newNode);
    if (status != SUCCESS)
        return status;

    /* Validate the parent-child relationship */
    status = NodeFT_validateParentChild(oNParent, newNode);
    if (status != SUCCESS) {
        /* Cleanup in case of failure */
        if (!bIsFile) {
            DynArray_free(newNode->dirChildren);
            DynArray_free(newNode->fileChildren);
        }
        Path_free(newNode->path);
        free(newNode);
        *poNResult = NULL;
        return status;
    }

    /* Add the new node as a child of the parent */
    if (oNParent != NULL) {
        status = NodeFT_insertChild(oNParent, newNode, bIsFile);
        if (status != SUCCESS) {
            /* Cleanup in case of failure */
            if (!bIsFile) {
                DynArray_free(newNode->dirChildren);
                DynArray_free(newNode->fileChildren);
            }
            Path_free(newNode->path);
            free(newNode);
            *poNResult = NULL;
            return status;
        }
    }

    *poNResult = newNode;
    return SUCCESS;
}

/* See nodeFT.h for specification */
size_t NodeFT_free(Node_T oNNode) {
    size_t freedNodes = 0;

    assert(oNNode != NULL);

    /* Remove the node from its parent's child array */
    NodeFT_removeFromParent(oNNode);

    /* Recursively free children if directory */
    if (!oNNode->isFile) {
        /* Free directory children */
        while (DynArray_getLength(oNNode->dirChildren) > 0) {
            Node_T childNode = DynArray_get(oNNode->dirChildren, 0);
            freedNodes += NodeFT_free(childNode);
        }
        DynArray_free(oNNode->dirChildren);

        /* Free file children */
        while (DynArray_getLength(oNNode->fileChildren) > 0) {
            Node_T childNode = DynArray_get(oNNode->fileChildren, 0);
            freedNodes += NodeFT_free(childNode);
        }
        DynArray_free(oNNode->fileChildren);
    } else {
        /* Free file contents if any */
        if (oNNode->contents != NULL)
            free(oNNode->contents);
    }

    /* Free the node's path and the node itself */
    Path_free(oNNode->path);
    free(oNNode);
    freedNodes++;

    return freedNodes;
}

/* See nodeFT.h for specification */
Path_T NodeFT_getPath(Node_T oNNode) {
    assert(oNNode != NULL);

    return oNNode->path;
}

/* See nodeFT.h for specification */
boolean NodeFT_hasChild(Node_T oNParent, Path_T oPPath, size_t *pulChildID, boolean bIsFile) {
    DynArray_T childArray;
    const char *childPathStr;
    int found;

    assert(oNParent != NULL);
    assert(oPPath != NULL);
    assert(pulChildID != NULL);

    /* Choose the correct child array */
    if (bIsFile)
        childArray = oNParent->fileChildren;
    else
        childArray = oNParent->dirChildren;

    childPathStr = Path_getPathname(oPPath);

    /* Search for the child */
    found = DynArray_bsearch(childArray, (void *)childPathStr, pulChildID, NodeFT_comparePathString);

    return found;
}

/* See nodeFT.h for specification */
size_t NodeFT_getNumChildren(Node_T oNParent, boolean bIsFile) {
    DynArray_T childArray;

    assert(oNParent != NULL);
    assert(!oNParent->isFile);

    /* Choose the correct child array */
    if (bIsFile)
        childArray = oNParent->fileChildren;
    else
        childArray = oNParent->dirChildren;

    return DynArray_getLength(childArray);
}

/* See nodeFT.h for specification */
int NodeFT_getChild(Node_T oNParent, size_t ulChildID, Node_T *poNResult, boolean bIsFile) {
    DynArray_T childArray;

    assert(oNParent != NULL);
    assert(poNResult != NULL);
    assert(!oNParent->isFile);

    /* Choose the correct child array */
    if (bIsFile)
        childArray = oNParent->fileChildren;
    else
        childArray = oNParent->dirChildren;

    /* Check if the index is within bounds */
    if (ulChildID >= DynArray_getLength(childArray)) {
        *poNResult = NULL;
        return NO_SUCH_PATH;
    }

    /* Retrieve the child node */
    *poNResult = DynArray_get(childArray, ulChildID);
    return SUCCESS;
}

/* See nodeFT.h for specification */
int NodeFT_getContents(Node_T oNNode, void **ppvResult) {
    assert(ppvResult != NULL);

    if (oNNode == NULL)
        return NO_SUCH_PATH;

    *ppvResult = oNNode->contents;
    return SUCCESS;
}

/* See nodeFT.h for specification */
int NodeFT_getContentLength(Node_T oNNode, size_t *pulLength) {
    assert(pulLength != NULL);

    if (oNNode == NULL)
        return NO_SUCH_PATH;

    *pulLength = oNNode->contentLength;
    return SUCCESS;
}

/* See nodeFT.h for specification */
int NodeFT_setContents(Node_T oNNode, void *pvContents, size_t ulLength) {
    assert(oNNode != NULL);

    /* Free existing contents if any */
    if (oNNode->contents != NULL)
        free(oNNode->contents);

    if (pvContents != NULL && ulLength > 0) {
        /* Allocate memory and copy new contents */
        oNNode->contents = malloc(ulLength);
        if (oNNode->contents == NULL) {
            oNNode->contentLength = 0;
            return MEMORY_ERROR;
        }
        memcpy(oNNode->contents, pvContents, ulLength);
        oNNode->contentLength = ulLength;
    } else {
        /* Set contents to NULL if no contents provided */
        oNNode->contents = NULL;
        oNNode->contentLength = 0;
    }

    return SUCCESS;
}

/* See nodeFT.h for specification */
boolean NodeFT_isFile(Node_T oNNode) {
    if (oNNode == NULL)
        return FALSE;

    return oNNode->isFile;
}

/* See nodeFT.h for specification */
Node_T NodeFT_getParent(Node_T oNNode) {
    assert(oNNode != NULL);

    return oNNode->parent;
}

/* See nodeFT.h for specification */
char *NodeFT_toString(Node_T oNNode) {
    const char *pathStr = NULL;
    const char *typePrefix = NULL;
    char *resultStr = NULL;
    size_t totalLength;

    assert(oNNode != NULL);

    /* Get the string representation of the node's path */
    pathStr = Path_getPathname(oNNode->path);
    if (pathStr == NULL)
        return NULL;

    /* Determine the node type */
    if (oNNode->isFile)
        typePrefix = "File: ";
    else
        typePrefix = "Dir:  ";

    /* Calculate total length and allocate memory */
    totalLength = strlen(typePrefix) + strlen(pathStr) + 1;
    resultStr = (char *)malloc(totalLength);
    if (resultStr == NULL) {
        /* No need to free pathStr as it's managed elsewhere */
        return NULL;
    }

    /* Construct the final string */
    strcpy(resultStr, typePrefix);
    strcat(resultStr, pathStr);

    /* No need to free pathStr as it's managed elsewhere */

    return resultStr;
}