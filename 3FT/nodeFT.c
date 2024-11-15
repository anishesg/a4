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
/* Static Helper Function Prototypes                             */
/*---------------------------------------------------------------*/

/*
  Compares the paths of two nodes for sorting and searching.

  Parameters:
    - node1: the first node to compare
    - node2: the second node to compare

  Returns:
    - A negative value if node1 < node2
    - Zero if node1 == node2
    - A positive value if node1 > node2
*/
static int NodeFT_compareNodes(const Node_T node1, const Node_T node2);

/*
  Compares the path of a node and a string for searching.

  Parameters:
    - nodePtr: pointer to the node whose path is to be compared
    - pathStrPtr: pointer to the string path to compare against

  Returns:
    - A negative value if node's path < pathStrPtr
    - Zero if node's path == pathStrPtr
    - A positive value if node's path > pathStrPtr
*/
static int NodeFT_comparePathString(const void *nodePtr, const void *pathStrPtr);

/*
  Initializes a new node with given path, parent, and type (file or dir).

  Parameters:
    - path: the path associated with the new node
    - parent: the parent node under which the new node will be added
    - isFile: boolean indicating if the new node is a file (TRUE) or directory (FALSE)
    - resultNode: pointer to where the newly created node will be stored

  Returns:
    - SUCCESS on successful initialization
    - MEMORY_ERROR if memory allocation fails
    - Other appropriate error codes based on Path_dup and DynArray_new failures

  On success, sets `*resultNode` to the new node.
  On failure, sets `*resultNode` to NULL.
*/
static int NodeFT_initializeNode(Path_T path, Node_T parent, boolean isFile, Node_T *resultNode);

/*
  Validates the parent-child relationship.

  Parameters:
    - parent: the parent node
    - newNode: the new node to validate under the parent

  Returns:
    - SUCCESS if the relationship is valid
    - CONFLICTING_PATH if the parent's path is not a prefix of the new node's path
    - NO_SUCH_PATH if the new node is not directly under the parent

  Ensures that `newNode` is directly under `parent` and that their paths are consistent.
*/
static int NodeFT_validateParentChild(Node_T parent, Node_T newNode);

/*
  Inserts childNode into parentNode's appropriate child array.

  Parameters:
    - parentNode: the parent node where the child will be inserted
    - childNode: the child node to insert
    - isFile: boolean indicating if the child node is a file (TRUE) or directory (FALSE)

  Returns:
    - SUCCESS on successful insertion
    - ALREADY_IN_TREE if a child with the same path already exists
    - MEMORY_ERROR if insertion into the dynamic array fails
*/
static int NodeFT_insertChild(Node_T parentNode, Node_T childNode, boolean isFile);

/*
  Removes node from its parent's child array.

  Parameters:
    - node: the node to remove from its parent

  This function does not return a value. It assumes that `node` is a valid node
  and that its parent correctly references its child arrays.
*/
static void NodeFT_removeFromParent(Node_T node);

/*---------------------------------------------------------------*/
/* Static Helper Function Definitions                            */
/*---------------------------------------------------------------*/

/*
  Compares the paths of two nodes for sorting and searching.

  Parameters:
    - node1: the first node to compare
    - node2: the second node to compare

  Returns:
    - A negative value if node1 < node2
    - Zero if node1 == node2
    - A positive value if node1 > node2
*/
static int NodeFT_compareNodes(const Node_T node1, const Node_T node2) {
    assert(node1 != NULL);
    assert(node2 != NULL);

    return Path_comparePath(node1->path, node2->path);
}

/*
  Compares the path of a node and a string for searching.

  Parameters:
    - nodePtr: pointer to the node whose path is to be compared
    - pathStrPtr: pointer to the string path to compare against

  Returns:
    - A negative value if node's path < pathStrPtr
    - Zero if node's path == pathStrPtr
    - A positive value if node's path > pathStrPtr
*/
static int NodeFT_comparePathString(const void *nodePtr, const void *pathStrPtr) {
    const Node_T node = (const Node_T)nodePtr;
    const char *pathStr = (const char *)pathStrPtr;

    assert(node != NULL);
    assert(pathStr != NULL);

    return Path_compareString(node->path, pathStr);
}

/*
  Initializes a new node with given path, parent, and type (file or dir).

  Parameters:
    - path: the path associated with the new node
    - parent: the parent node under which the new node will be added
    - isFile: boolean indicating if the new node is a file (TRUE) or directory (FALSE)
    - resultNode: pointer to where the newly created node will be stored

  Returns:
    - SUCCESS on successful initialization
    - MEMORY_ERROR if memory allocation fails
    - Other appropriate error codes based on Path_dup and DynArray_new failures

  On success, sets `*resultNode` to the new node.
  On failure, sets `*resultNode` to NULL.
*/
static int NodeFT_initializeNode(Path_T path, Node_T parent, boolean isFile, Node_T *resultNode) {
    Node_T newNode = NULL;
    Path_T pathCopy = NULL;
    int status;

    assert(path != NULL);
    assert(resultNode != NULL);

    /* Allocate memory for the new node */
    newNode = (Node_T)malloc(sizeof(struct node));
    if (newNode == NULL) {
        *resultNode = NULL;
        return MEMORY_ERROR;
    }

    /* Duplicate the path */
    status = Path_dup(path, &pathCopy);
    if (status != SUCCESS) {
        free(newNode);
        *resultNode = NULL;
        return status;
    }

    /* Initialize the new node's fields */
    newNode->path = pathCopy;
    newNode->parent = parent;
    newNode->isFile = isFile;
    newNode->contents = NULL;
    newNode->contentLength = 0;

    if (isFile) {
        /* Files don't have children */
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
            *resultNode = NULL;
            return MEMORY_ERROR;
        }
    }

    *resultNode = newNode;
    return SUCCESS;
}

/*
  Validates the parent-child relationship.

  Parameters:
    - parent: the parent node
    - newNode: the new node to validate under the parent

  Returns:
    - SUCCESS if the relationship is valid
    - CONFLICTING_PATH if the parent's path is not a prefix of the new node's path
    - NO_SUCH_PATH if the new node is not directly under the parent

  Ensures that `newNode` is directly under `parent` and that their paths are consistent.
*/
static int NodeFT_validateParentChild(Node_T parent, Node_T newNode) {
    size_t parentDepth, sharedDepth;

    assert(newNode != NULL);

    if (parent == NULL) {
        /* If no parent, must be the root node */
        if (Path_getDepth(newNode->path) != 1)
            return NO_SUCH_PATH;
    } else {
        parentDepth = Path_getDepth(parent->path);
        sharedDepth = Path_getSharedPrefixDepth(newNode->path, parent->path);

        /* Parent's path must be a prefix of newNode's path */
        if (sharedDepth < parentDepth)
            return CONFLICTING_PATH;

        /* newNode must be directly under the parent */
        if (Path_getDepth(newNode->path) != parentDepth + 1)
            return NO_SUCH_PATH;
    }

    return SUCCESS;
}

/*
  Inserts childNode into parentNode's appropriate child array.

  Parameters:
    - parentNode: the parent node where the child will be inserted
    - childNode: the child node to insert
    - isFile: boolean indicating if the child node is a file (TRUE) or directory (FALSE)

  Returns:
    - SUCCESS on successful insertion
    - ALREADY_IN_TREE if a child with the same path already exists
    - MEMORY_ERROR if insertion into the dynamic array fails
*/
static int NodeFT_insertChild(Node_T parentNode, Node_T childNode, boolean isFile) {
    size_t childIndex = 0;
    int insertStatus;
    DynArray_T childArray;

    assert(parentNode != NULL);
    assert(childNode != NULL);
    assert(!parentNode->isFile); /* Parent must be a directory */

    /* Choose the correct child array based on the node type */
    childArray = isFile ? parentNode->fileChildren : parentNode->dirChildren;

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
  Removes node from its parent's child array.

  Parameters:
    - node: the node to remove from its parent

  This function does not return a value. It assumes that `node` is a valid node
  and that its parent correctly references its child arrays.
*/
static void NodeFT_removeFromParent(Node_T node) {
    DynArray_T childArray;
    size_t childIndex = 0;
    boolean found;

    assert(node != NULL);

    if (node->parent != NULL) {
        /* Choose the correct child array */
        childArray = node->isFile ? node->parent->fileChildren : node->parent->dirChildren;

        /* Find and remove the node from the array */
        found = DynArray_bsearch(childArray, node, &childIndex, NodeFT_compareNodes);
        if (found)
            DynArray_removeAt(childArray, childIndex);
    }
}

/*---------------------------------------------------------------*/
/* Public Interface Function Definitions                         */
/*---------------------------------------------------------------*/

/*
  Constructs a new node with specified path, parent, and type.

  Parameters:
    - path: the path associated with the new node
    - parent: the parent node under which the new node will be added
    - isFile: boolean indicating if the new node is a file (TRUE) or directory (FALSE)
    - resultNode: pointer to where the newly created node will be stored

  Returns:
    - SUCCESS on successful creation
    - MEMORY_ERROR if memory allocation fails
    - CONFLICTING_PATH if `parent`'s path is not a prefix of `path`
    - NO_SUCH_PATH if `path` is invalid, or `parent` is NULL but `path` is not root,
                   or `parent`'s path is not the immediate parent of `path`
    - ALREADY_IN_TREE if a child with `path` already exists under `parent`

  On success, sets `*resultNode` to the new node.
  On failure, sets `*resultNode` to NULL.
*/
int NodeFT_new(Path_T path, Node_T parent, boolean isFile, Node_T *resultNode) {
    Node_T newNode = NULL;
    int status;

    assert(path != NULL);
    assert(resultNode != NULL);

    /* Initialize the new node */
    status = NodeFT_initializeNode(path, parent, isFile, &newNode);
    if (status != SUCCESS)
        return status;

    /* Validate the parent-child relationship */
    status = NodeFT_validateParentChild(parent, newNode);
    if (status != SUCCESS) {
        /* Cleanup in case of failure */
        if (!isFile) {
            DynArray_free(newNode->dirChildren);
            DynArray_free(newNode->fileChildren);
        }
        Path_free(newNode->path);
        free(newNode);
        *resultNode = NULL;
        return status;
    }

    /* Add the new node as a child of the parent */
    if (parent != NULL) {
        status = NodeFT_insertChild(parent, newNode, isFile);
        if (status != SUCCESS) {
            /* Cleanup in case of failure */
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
  Recursively frees the subtree rooted at node, including node itself.

  Parameters:
    - node: the root node of the subtree to free

  Returns:
    - The total number of nodes freed.
*/
size_t NodeFT_free(Node_T node) {
    size_t freedNodes = 0;

    assert(node != NULL);

    /* Remove the node from its parent's child array */
    NodeFT_removeFromParent(node);

    /* Recursively free children if directory */
    if (!node->isFile) {
        /* Free directory children */
        while (DynArray_getLength(node->dirChildren) > 0) {
            Node_T childNode = DynArray_get(node->dirChildren, 0);
            freedNodes += NodeFT_free(childNode);
        }
        DynArray_free(node->dirChildren);

        /* Free file children */
        while (DynArray_getLength(node->fileChildren) > 0) {
            Node_T childNode = DynArray_get(node->fileChildren, 0);
            freedNodes += NodeFT_free(childNode);
        }
        DynArray_free(node->fileChildren);
    } else {
        /* Free file contents if any */
        if (node->contents != NULL)
            free(node->contents);
    }

    /* Free the node's path and the node itself */
    Path_free(node->path);
    free(node);
    freedNodes++;

    return freedNodes;
}

/*
  Returns the path object representing node's absolute path.

  Parameters:
    - node: the node whose path is to be retrieved

  Returns:
    - The Path_T object representing the node's path
*/
Path_T NodeFT_getPath(Node_T node) {
    assert(node != NULL);

    return node->path;
}

/*
  Checks if parent has a child node with path childPath and type specified by isFile.

  Parameters:
    - parent: the parent node to search within
    - childPath: the path of the child node to search for
    - childIndexPtr: pointer to where the index will be stored
    - isFile: boolean indicating the type of child (TRUE for file, FALSE for directory)

  Returns:
    - TRUE if such a child exists
    - FALSE otherwise

  If the child exists, stores its index in `*childIndexPtr`.
  Otherwise, stores the index where such a child would be inserted.
*/
boolean NodeFT_hasChild(Node_T parent, Path_T childPath, size_t *childIndexPtr, boolean isFile) {
    DynArray_T childArray;
    const char *childPathStr;
    boolean found;

    assert(parent != NULL);
    assert(childPath != NULL);
    assert(childIndexPtr != NULL);

    /* Choose the correct child array */
    childArray = isFile ? parent->fileChildren : parent->dirChildren;

    childPathStr = Path_getPathname(childPath);

    /* Search for the child */
    found = DynArray_bsearch(childArray, (void *)childPathStr, childIndexPtr, NodeFT_comparePathString);

    return found;
}

/*
  Returns the number of children of parent of type specified by isFile.

  Parameters:
    - parent: the parent node whose children are to be counted
    - isFile: boolean indicating the type of children to count (TRUE for files, FALSE for directories)

  Returns:
    - The number of children of the specified type
*/
size_t NodeFT_getNumChildren(Node_T parent, boolean isFile) {
    DynArray_T childArray;

    assert(parent != NULL);
    assert(!parent->isFile);

    /* Choose the correct child array */
    childArray = isFile ? parent->fileChildren : parent->dirChildren;

    return DynArray_getLength(childArray);
}

/*
  Retrieves the child node of parent at index childID of type specified by isFile.

  Parameters:
    - parent: the parent node from which to retrieve the child
    - childID: the index of the child within the child array
    - resultNode: pointer to where the retrieved child node will be stored
    - isFile: boolean indicating the type of child to retrieve (TRUE for file, FALSE for directory)

  Returns:
    - SUCCESS on successful retrieval
    - NO_SUCH_PATH if childID is out of bounds

  On success, sets `*resultNode` to the retrieved child node.
  On failure, sets `*resultNode` to NULL.
*/
int NodeFT_getChild(Node_T parent, size_t childID, Node_T *resultNode, boolean isFile) {
    DynArray_T childArray;

    assert(parent != NULL);
    assert(resultNode != NULL);
    assert(!parent->isFile);

    /* Choose the correct child array */
    childArray = isFile ? parent->fileChildren : parent->dirChildren;

    /* Check if the index is within bounds */
    if (childID >= DynArray_getLength(childArray)) {
        *resultNode = NULL;
        return NO_SUCH_PATH;
    }

    /* Retrieve the child node */
    *resultNode = DynArray_get(childArray, childID);
    return SUCCESS;
}

/*
  If node is a file node, stores its contents in *contentsPtr and returns SUCCESS.
  On failure, sets *contentsPtr to NULL and returns NO_SUCH_PATH.

  Parameters:
    - node: the file node whose contents are to be retrieved
    - contentsPtr: pointer to where the contents will be stored

  Returns:
    - SUCCESS on successful retrieval
    - NO_SUCH_PATH if node is NULL
*/
int NodeFT_getContents(Node_T node, void **contentsPtr) {
    assert(contentsPtr != NULL);

    if (node == NULL)
        return NO_SUCH_PATH;

    *contentsPtr = node->contents;
    return SUCCESS;
}

/*
  If node is a file node, stores the length of its contents in *lengthPtr and returns SUCCESS.
  On failure, sets *lengthPtr to 0 and returns NO_SUCH_PATH.

  Parameters:
    - node: the file node whose content length is to be retrieved
    - lengthPtr: pointer to where the content length will be stored

  Returns:
    - SUCCESS on successful retrieval
    - NO_SUCH_PATH if node is NULL
*/
int NodeFT_getContentLength(Node_T node, size_t *lengthPtr) {
    assert(lengthPtr != NULL);

    if (node == NULL)
        return NO_SUCH_PATH;

    *lengthPtr = node->contentLength;
    return SUCCESS;
}

/*
  Sets the contents of file node to newContents of length newLength.

  Parameters:
    - node: the file node whose contents are to be set
    - newContents: pointer to the new contents
    - newLength: the length of the new contents

  Returns:
    - SUCCESS on successful update
    - MEMORY_ERROR if memory allocation fails
*/
int NodeFT_setContents(Node_T node, void *newContents, size_t newLength) {
    assert(node != NULL);

    /* Free existing contents if any */
    if (node->contents != NULL)
        free(node->contents);

    if (newContents != NULL && newLength > 0) {
        /* Allocate memory and copy new contents */
        node->contents = malloc(newLength);
        if (node->contents == NULL) {
            node->contentLength = 0;
            return MEMORY_ERROR;
        }
        memcpy(node->contents, newContents, newLength);
        node->contentLength = newLength;
    } else {
        /* Set contents to NULL if no contents provided */
        node->contents = NULL;
        node->contentLength = 0;
    }

    return SUCCESS;
}

/*
  Returns TRUE if node is a file, FALSE if it is a directory.

  Parameters:
    - node: the node to check

  Returns:
    - TRUE if node is a file
    - FALSE if node is a directory or NULL
*/
boolean NodeFT_isFile(Node_T node) {
    if (node == NULL)
        return FALSE;

    return node->isFile;
}

/*
  Returns the parent of node. If node is the root node, returns NULL.

  Parameters:
    - node: the node whose parent is to be retrieved

  Returns:
    - The parent node if it exists
    - NULL if node is the root node
*/
Node_T NodeFT_getParent(Node_T node) {
    assert(node != NULL);

    return node->parent;
}

/*
  Generates a string representation of node.

  Parameters:
    - node: the node to represent as a string

  Returns:
    - A dynamically allocated string representing the node
      (e.g., "File: /path/to/file" or "Dir:  /path/to/dir")
    - NULL if memory allocation fails

  The caller is responsible for freeing the allocated memory.
*/
char *NodeFT_toString(Node_T node) {
    const char *pathStr = NULL;
    const char *typePrefix = NULL;
    char *resultStr = NULL;
    size_t totalLength;

    assert(node != NULL);

    /* Get the string representation of the node's path */
    pathStr = Path_getPathname(node->path);
    if (pathStr == NULL)
        return NULL;

    /* Determine the node type */
    typePrefix = node->isFile ? "File: " : "Dir:  ";

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