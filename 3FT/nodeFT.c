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
    /* the path associated with this node */
    Path_T path;
    /* pointer to the parent node */
    Node_T parent;
    /* TRUE if this node is a file, FALSE if a directory */
    boolean isFile;
    /* dynamic array of directory children */
    DynArray_T dirChildren;
    /* dynamic array of file children */
    DynArray_T fileChildren;
    /* contents of the file (only valid if isFile is TRUE) */
    void *contents;
    /* length of the contents (only valid if isFile is TRUE) */
    size_t contentLength;
};

/*---------------------------------------------------------------*/
/* static helper functions                                       */
/*---------------------------------------------------------------*/

/*
  compares the paths of two nodes for sorting and searching.
  returns negative, zero, or positive depending on comparison.
*/
static int NodeFT_compareNodes(const Node_T node1, const Node_T node2) {
    assert(node1 != NULL);
    assert(node2 != NULL);

    return Path_comparePath(node1->path, node2->path);
}

/*
  compares the path of a node and a string for searching.
  returns negative, zero, or positive depending on comparison.
*/
static int NodeFT_comparePathString(const void *nodePtr, const void *pathStrPtr) {
    const Node_T node = (const Node_T)nodePtr;
    const char *pathStr = (const char *)pathStrPtr;

    assert(node != NULL);
    assert(pathStr != NULL);

    return Path_compareString(node->path, pathStr);
}

/*
  initializes a new node with given path, parent, and type (file or dir).
  allocates memory, duplicates path, and sets initial values.
  on success, sets *resultNode to the new node and returns SUCCESS.
  on failure, sets *resultNode to NULL and returns appropriate error code.
*/
static int NodeFT_initializeNode(Path_T path, Node_T parent, boolean isFile, Node_T *resultNode) {
    Node_T newNode = NULL;
    Path_T pathCopy = NULL;
    int status;

    assert(path != NULL);
    assert(resultNode != NULL);

    /* allocate memory for the new node */
    newNode = (Node_T)malloc(sizeof(struct node));
    if (newNode == NULL) {
        *resultNode = NULL;
        return MEMORY_ERROR;
    }

    /* duplicate the path */
    status = Path_dup(path, &pathCopy);
    if (status != SUCCESS) {
        free(newNode);
        *resultNode = NULL;
        return status;
    }

    /* initialize the new node's fields */
    newNode->path = pathCopy;
    newNode->parent = parent;
    newNode->isFile = isFile;
    newNode->contents = NULL;
    newNode->contentLength = 0;

    if (isFile) {
        /* files don't have children */
        newNode->dirChildren = NULL;
        newNode->fileChildren = NULL;
    } else {
        /* directories have separate arrays for dir and file children */
        newNode->dirChildren = DynArray_new(0);
        newNode->fileChildren = DynArray_new(0);
        if (newNode->dirChildren == NULL || newNode->fileChildren == NULL) {
            if (newNode->dirChildren)
                DynArray_free(newNode->dirChildren);
            if (newNode->fileChildren)
                DynArray_free(newNode->fileChildren);
            Path_free(newNode->path);
            free(newNode);
            *resultNode = NULL;
            return MEMORY_ERROR;
        }
    }

    *resultNode = newNode;
    return SUCCESS;
}

/*
  validates the parent-child relationship.
  ensures newNode is directly under parent and paths are consistent.
  returns SUCCESS if valid, appropriate error code otherwise.
*/
static int NodeFT_validateParentChild(Node_T parent, Node_T newNode) {
    size_t parentDepth, sharedDepth;

    assert(newNode != NULL);

    if (parent == NULL) {
        /* if no parent, must be the root node */
        if (Path_getDepth(newNode->path) != 1)
            return NO_SUCH_PATH;
    } else {
        parentDepth = Path_getDepth(parent->path);
        sharedDepth = Path_getSharedPrefixDepth(newNode->path, parent->path);

        /* parent's path must be a prefix of newNode's path */
        if (sharedDepth < parentDepth)
            return CONFLICTING_PATH;

        /* newNode must be directly under the parent */
        if (Path_getDepth(newNode->path) != parentDepth + 1)
            return NO_SUCH_PATH;
    }

    return SUCCESS;
}

/*
  inserts childNode into parentNode's appropriate child array.
  returns SUCCESS on success, or appropriate error code on failure.
*/
static int NodeFT_insertChild(Node_T parentNode, Node_T childNode, boolean isFile) {
    size_t childIndex = 0;
    int insertStatus;
    DynArray_T childArray;

    assert(parentNode != NULL);
    assert(childNode != NULL);
    assert(!parentNode->isFile); /* parent must be a directory */

    /* choose the correct child array based on the node type */
    if (isFile)
        childArray = parentNode->fileChildren;
    else
        childArray = parentNode->dirChildren;

    /* check for duplicate child */
    if (NodeFT_hasChild(parentNode, childNode->path, &childIndex, isFile))
        return ALREADY_IN_TREE;

    /* insert the child into the array at the correct position */
    insertStatus = DynArray_addAt(childArray, childIndex, childNode);
    if (insertStatus == TRUE)
        return SUCCESS;
    else
        return MEMORY_ERROR;
}

/*
  removes node from its parent's child array.
*/
static void NodeFT_removeFromParent(Node_T node) {
    DynArray_T childArray;
    size_t childIndex = 0;
    int found;

    assert(node != NULL);

    if (node->parent != NULL) {
        /* choose the correct child array */
        if (node->isFile)
            childArray = node->parent->fileChildren;
        else
            childArray = node->parent->dirChildren;

        /* find and remove the node from the array */
        found = DynArray_bsearch(childArray, node, &childIndex,
            (int (*)(const void *, const void *))NodeFT_compareNodes);
        if (found)
            DynArray_removeAt(childArray, childIndex);
    }
}

/*---------------------------------------------------------------*/
/* public interface functions                                    */
/*---------------------------------------------------------------*/

/*
  constructs a new node with specified path, parent, and type.
  on success, stores the new node in *resultNode and returns SUCCESS.
  on failure, sets *resultNode to NULL and returns appropriate error code.
*/
int NodeFT_new(Path_T path, Node_T parent, boolean isFile, Node_T *resultNode) {
    Node_T newNode = NULL;
    int status;

    assert(path != NULL);
    assert(resultNode != NULL);

    /* initialize the new node */
    status = NodeFT_initializeNode(path, parent, isFile, &newNode);
    if (status != SUCCESS)
        return status;

    /* validate the parent-child relationship */
    status = NodeFT_validateParentChild(parent, newNode);
    if (status != SUCCESS) {
        /* cleanup in case of failure */
        if (!isFile) {
            DynArray_free(newNode->dirChildren);
            DynArray_free(newNode->fileChildren);
        }
        Path_free(newNode->path);
        free(newNode);
        *resultNode = NULL;
        return status;
    }

    /* add the new node as a child of the parent */
    if (parent != NULL) {
        status = NodeFT_insertChild(parent, newNode, isFile);
        if (status != SUCCESS) {
            /* cleanup in case of failure */
            if (!isFile) {
                DynArray_free(newNode->dirChildren);
                DynArray_free(newNode->fileChildren);
            }
            Path_free(newNode->path);
            free(newNode);
            *resultNode = NULL;
            return status;
        }
    }

    *resultNode = newNode;
    return SUCCESS;
}

/*
  recursively frees the subtree rooted at node, including node itself.
  returns the total number of nodes freed.
*/
size_t NodeFT_free(Node_T node) {
    size_t freedNodes = 0;

    assert(node != NULL);

    /* remove the node from its parent's child array */
    NodeFT_removeFromParent(node);

    /* recursively free children if directory */
    if (!node->isFile) {
        /* free directory children */
        while (DynArray_getLength(node->dirChildren) > 0) {
            Node_T childNode = DynArray_get(node->dirChildren, 0);
            freedNodes += NodeFT_free(childNode);
        }
        DynArray_free(node->dirChildren);

        /* free file children */
        while (DynArray_getLength(node->fileChildren) > 0) {
            Node_T childNode = DynArray_get(node->fileChildren, 0);
            freedNodes += NodeFT_free(childNode);
        }
        DynArray_free(node->fileChildren);
    } else {
        /* free file contents if any */
        if (node->contents != NULL)
            free(node->contents);
    }

    /* free the node's path and the node itself */
    Path_free(node->path);
    free(node);
    freedNodes++;

    return freedNodes;
}

/*
  returns the path object representing node's absolute path.
*/
Path_T NodeFT_getPath(Node_T node) {
    assert(node != NULL);

    return node->path;
}

/*
  checks if parent has a child node with path childPath and type isFile.
  if such a child exists, stores its index in *childIndexPtr and returns TRUE.
  otherwise, stores the index where such a child would be inserted and returns FALSE.
*/
boolean NodeFT_hasChild(Node_T parent, Path_T childPath, size_t *childIndexPtr, boolean isFile) {
    DynArray_T childArray;
    const char *childPathStr;
    int found;

    assert(parent != NULL);
    assert(childPath != NULL);
    assert(childIndexPtr != NULL);

    /* choose the correct child array */
    if (isFile)
        childArray = parent->fileChildren;
    else
        childArray = parent->dirChildren;

    childPathStr = Path_getPathname(childPath);

    /* search for the child */
    found = DynArray_bsearch(childArray, (void *)childPathStr, childIndexPtr, NodeFT_comparePathString);

    return found;
}

/*
  returns the number of children of parent of type isFile.
*/
size_t NodeFT_getNumChildren(Node_T parent, boolean isFile) {
    DynArray_T childArray;

    assert(parent != NULL);
    assert(!parent->isFile);

    /* choose the correct child array */
    if (isFile)
        childArray = parent->fileChildren;
    else
        childArray = parent->dirChildren;

    return DynArray_getLength(childArray);
}

/*
  retrieves the child node of parent at index childID of type isFile.
  on success, stores the child in *resultNode and returns SUCCESS.
  on failure, sets *resultNode to NULL and returns NO_SUCH_PATH.
*/
int NodeFT_getChild(Node_T parent, size_t childID, Node_T *resultNode, boolean isFile) {
    DynArray_T childArray;

    assert(parent != NULL);
    assert(resultNode != NULL);
    assert(!parent->isFile);

    /* choose the correct child array */
    if (isFile)
        childArray = parent->fileChildren;
    else
        childArray = parent->dirChildren;

    /* check if the index is within bounds */
    if (childID >= DynArray_getLength(childArray)) {
        *resultNode = NULL;
        return NO_SUCH_PATH;
    }

    /* retrieve the child node */
    *resultNode = DynArray_get(childArray, childID);
    return SUCCESS;
}

/*
  if node is a file node, stores its contents in *contentsPtr and returns SUCCESS.
  on failure, sets *contentsPtr to NULL and returns NO_SUCH_PATH.
*/
int NodeFT_getContents(Node_T node, void **contentsPtr) {
    assert(contentsPtr != NULL);

    if (node == NULL)
        return NO_SUCH_PATH;

    *contentsPtr = node->contents;
    return SUCCESS;
}

/*
  if node is a file node, stores the length of its contents in *lengthPtr and returns SUCCESS.
  on failure, sets *lengthPtr to 0 and returns NO_SUCH_PATH.
*/
int NodeFT_getContentLength(Node_T node, size_t *lengthPtr) {
    assert(lengthPtr != NULL);

    if (node == NULL)
        return NO_SUCH_PATH;

    *lengthPtr = node->contentLength;
    return SUCCESS;
}

/*
  sets the contents of file node to newContents of length newLength.
  returns SUCCESS on success, or appropriate error code on failure.
*/
int NodeFT_setContents(Node_T node, void *newContents, size_t newLength) {
    assert(node != NULL);

    /* free existing contents if any */
    if (node->contents != NULL)
        free(node->contents);

    if (newContents != NULL && newLength > 0) {
        /* allocate memory and copy new contents */
        node->contents = malloc(newLength);
        if (node->contents == NULL) {
            node->contentLength = 0;
            return MEMORY_ERROR;
        }
        memcpy(node->contents, newContents, newLength);
        node->contentLength = newLength;
    } else {
        /* set contents to NULL if no contents provided */
        node->contents = NULL;
        node->contentLength = 0;
    }

    return SUCCESS;
}

/*
  returns TRUE if node is a file, FALSE if it is a directory.
*/
boolean NodeFT_isFile(Node_T node) {
    if (node == NULL)
        return FALSE;

    return node->isFile;
}

/*
  returns the parent of node. if node is the root node, returns NULL.
*/
Node_T NodeFT_getParent(Node_T node) {
    assert(node != NULL);

    return node->parent;
}

/*
  generates a string representation of node.
  the caller is responsible for freeing the allocated memory.
  returns NULL if memory allocation fails.
*/
char *NodeFT_toString(Node_T node) {
    const char *pathStr = NULL;
    const char *typePrefix = NULL;
    char *resultStr = NULL;
    size_t totalLength;

    assert(node != NULL);

    /* get the string representation of the node's path */
    pathStr = Path_getPathname(node->path);
    if (pathStr == NULL)
        return NULL;

    /* determine the node type */
    if (node->isFile)
        typePrefix = "File: ";
    else
        typePrefix = "Dir:  ";

    /* calculate total length and allocate memory */
    totalLength = strlen(typePrefix) + strlen(pathStr) + 1;
    resultStr = (char *)malloc(totalLength);
    if (resultStr == NULL) {
        /* no need to free pathStr as it's managed elsewhere */
        return NULL;
    }

    /* construct the final string */
    strcpy(resultStr, typePrefix);
    strcat(resultStr, pathStr);

    /* no need to free pathStr as it's managed elsewhere */

    return resultStr;
}