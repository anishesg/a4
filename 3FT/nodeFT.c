/*--------------------------------------------------------------------*/
/* nodeFT.c                                                           */
/* Author: [Your Name]                                                */
/*--------------------------------------------------------------------*/

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "nodeFT.h"
#include "path.h"
#include "dynarray.h"
#include "checkerDT.h"

/*
 * A node in the File Tree (FT) represents either a file or a directory.
 * Each node maintains the following:
 * - path: the absolute path of the node.
 * - parent: a pointer to the parent node.
 * - children: a dynamic array of child nodes (for directories).
 * - type: the type of the node (FILE_NODE or DIRECTORY_NODE).
 * - contents: a pointer to the file contents (for files).
 * - contentLength: the size of the file contents in bytes (for files).
 */

struct node {
    Path_T path;              /* The absolute path of this node */
    Node_T parent;            /* Pointer to the parent node */
    DynArray_T children;      /* Dynamic array of child nodes */
    NodeType type;            /* Type of the node: FILE_NODE or DIRECTORY_NODE */
    void *contents;           /* Contents of the file (if FILE_NODE) */
    size_t contentLength;     /* Size of the contents (if FILE_NODE) */
};

/*
 * Helper function to compare two nodes based on their paths.
 * Returns <0 if firstNode < secondNode, 0 if they are equal, >0 otherwise.
 */
static int Node_compare(Node_T firstNode, Node_T secondNode) {
    assert(firstNode != NULL);
    assert(secondNode != NULL);

    return Path_comparePath(firstNode->path, secondNode->path);
}

/*
 * Helper function to compare a node's path with a given path string.
 * Returns <0 if node's path < pathString, 0 if equal, >0 otherwise.
 */
static int Node_compareString(const Node_T node, const char *pathString) {
    assert(node != NULL);
    assert(pathString != NULL);

    return Path_compareString(node->path, pathString);
}

/*
 * Adds a child node to the parent node's children array at the specified index.
 * Returns SUCCESS if the child is added successfully.
 * Returns MEMORY_ERROR if memory allocation fails.
 */
static int Node_addChild(Node_T parentNode, Node_T childNode, size_t index) {
    assert(parentNode != NULL);
    assert(childNode != NULL);

    if (DynArray_addAt(parentNode->children, index, childNode)) {
        return SUCCESS;
    } else {
        return MEMORY_ERROR;
    }
}

/*
 * Creates a new node with the specified path, parent, and type.
 * For files, contents and contentLength specify the file's content and size.
 * Returns SUCCESS if the node is created successfully.
 * Returns CONFLICTING_PATH if there is a path conflict.
 * Returns NO_SUCH_PATH if the parent path does not exist.
 * Returns ALREADY_IN_TREE if a node already exists at the given path.
 * Returns MEMORY_ERROR if memory allocation fails.
 */
int Node_new(Path_T nodePath, Node_T parentNode, Node_T *resultNode, NodeType nodeType) {
    Node_T newNode = NULL;
    Path_T parentPath = NULL;
    Path_T newPath = NULL;
    size_t parentDepth;
    size_t sharedDepth;
    size_t index;
    int status;

    assert(nodePath != NULL);
    assert(resultNode != NULL);
    /* parentNode can be NULL for the root node */

    /* Allocate memory for the new node */
    newNode = malloc(sizeof(struct node));
    if (newNode == NULL) {
        *resultNode = NULL;
        return MEMORY_ERROR;
    }

    /* Duplicate the path for the new node */
    status = Path_dup(nodePath, &newPath);
    if (status != SUCCESS) {
        free(newNode);
        *resultNode = NULL;
        return status;
    }
    newNode->path = newPath;

    /* Initialize contents and contentLength for files */
    if (nodeType == FILE_NODE) {
        newNode->contents = NULL;
        newNode->contentLength = 0;
    } else {
        newNode->contents = NULL;
        newNode->contentLength = 0;
    }

    newNode->type = nodeType;
    newNode->parent = parentNode;

    /* Initialize the children array for directories */
    if (nodeType == DIRECTORY_NODE) {
        newNode->children = DynArray_new(0);
        if (newNode->children == NULL) {
            Path_free(newNode->path);
            free(newNode);
            *resultNode = NULL;
            return MEMORY_ERROR;
        }
    } else {
        newNode->children = NULL;
    }

    /* Validate and set the new node's parent */
    if (parentNode != NULL) {
        parentPath = parentNode->path;
        parentDepth = Path_getDepth(parentPath);
        sharedDepth = Path_getSharedPrefixDepth(newNode->path, parentPath);

        /* The parent must be an ancestor of the new node */
        if (sharedDepth < parentDepth) {
            if (newNode->children != NULL) {
                DynArray_free(newNode->children);
            }
            Path_free(newNode->path);
            free(newNode);
            *resultNode = NULL;
            return CONFLICTING_PATH;
        }

        /* The new node must be directly under the parent */
        if (Path_getDepth(newNode->path) != parentDepth + 1) {
            if (newNode->children != NULL) {
                DynArray_free(newNode->children);
            }
            Path_free(newNode->path);
            free(newNode);
            *resultNode = NULL;
            return NO_SUCH_PATH;
        }

        /* The parent must not already have a child with this path */
        if (Node_hasChild(parentNode, nodePath, &index)) {
            if (newNode->children != NULL) {
                DynArray_free(newNode->children);
            }
            Path_free(newNode->path);
            free(newNode);
            *resultNode = NULL;
            return ALREADY_IN_TREE;
        }

        /* Add the new node to the parent's children */
        status = Node_addChild(parentNode, newNode, index);
        if (status != SUCCESS) {
            if (newNode->children != NULL) {
                DynArray_free(newNode->children);
            }
            Path_free(newNode->path);
            free(newNode);
            *resultNode = NULL;
            return status;
        }
    } else {
        /* For root node, the depth must be 1 */
        if (Path_getDepth(newNode->path) != 1) {
            if (newNode->children != NULL) {
                DynArray_free(newNode->children);
            }
            Path_free(newNode->path);
            free(newNode);
            *resultNode = NULL;
            return NO_SUCH_PATH;
        }
    }

    *resultNode = newNode;
    return SUCCESS;
}

/*
 * Frees the node and all of its descendants recursively.
 * Returns the number of nodes freed.
 */
size_t Node_free(Node_T node) {
    size_t count = 0;
    size_t i;

    assert(node != NULL);

    /* Remove from parent's children array */
    if (node->parent != NULL && node->parent->children != NULL) {
        size_t index;
        if (DynArray_bsearch(node->parent->children, node, &index, (int (*)(const void *, const void *)) Node_compare)) {
            (void) DynArray_removeAt(node->parent->children, index);
        }
    }

    /* Recursively free children (if directory) */
    if (node->type == DIRECTORY_NODE && node->children != NULL) {
        while (DynArray_getLength(node->children) > 0) {
            Node_T childNode = DynArray_get(node->children, 0);
            count += Node_free(childNode);
        }
        DynArray_free(node->children);
    }

    /* Free the path */
    Path_free(node->path);

    /* Free the node itself */
    free(node);
    count++;

    return count;
}

/*
 * Returns the path of the node.
 */
Path_T Node_getPath(Node_T node) {
    assert(node != NULL);

    return node->path;
}

/*
 * Checks if the node has a child with the specified path.
 * If found, sets *childIndex to the index of the child in the children array.
 * Returns TRUE if the child is found, FALSE otherwise.
 */
boolean Node_hasChild(Node_T node, Path_T childPath, size_t *childIndex) {
    assert(node != NULL);
    assert(childPath != NULL);
    assert(childIndex != NULL);

    if (node->children == NULL) {
        return FALSE;
    }

    return DynArray_bsearch(node->children, (char *) Path_getPathname(childPath), childIndex, (int (*)(const void *, const void *)) Node_compareString);
}

/*
 * Returns the number of children the node has.
 */
size_t Node_getNumChildren(Node_T node) {
    assert(node != NULL);

    if (node->children == NULL) {
        return 0;
    }

    return DynArray_getLength(node->children);
}

/*
 * Retrieves the child node at the specified index.
 * Returns SUCCESS if the child is retrieved successfully.
 * Returns NO_SUCH_PATH if the index is out of bounds.
 */
int Node_getChild(Node_T node, size_t childIndex, Node_T *childNode) {
    assert(node != NULL);
    assert(childNode != NULL);

    if (node->children == NULL || childIndex >= Node_getNumChildren(node)) {
        *childNode = NULL;
        return NO_SUCH_PATH;
    }

    *childNode = DynArray_get(node->children, childIndex);
    return SUCCESS;
}

/*
 * Returns the parent of the node.
 */
Node_T Node_getParent(Node_T node) {
    assert(node != NULL);

    return node->parent;
}

/*
 * Returns the type of the node: FILE_NODE or DIRECTORY_NODE.
 */
NodeType Node_getType(Node_T node) {
    assert(node != NULL);

    return node->type;
}

/*
 * Sets the contents and contentLength of a file node.
 * For directories, this function has no effect.
 */
void Node_setFileContents(Node_T node, void *contents, size_t contentLength) {
    assert(node != NULL);

    if (node->type == FILE_NODE) {
        node->contents = contents;
        node->contentLength = contentLength;
    }
}

/*
 * Retrieves the contents of a file node.
 * Returns NULL if the node is not a file or has no contents.
 */
void *Node_getFileContents(Node_T node) {
    assert(node != NULL);

    if (node->type == FILE_NODE) {
        return node->contents;
    } else {
        return NULL;
    }
}

/*
 * Returns the length of the file contents.
 * Returns 0 if the node is not a file.
 */
size_t Node_getFileLength(Node_T node) {
    assert(node != NULL);

    if (node->type == FILE_NODE) {
        return node->contentLength;
    } else {
        return 0;
    }
}

/*
 * Replaces the contents of a file node with new contents.
 * Returns the old contents.
 * For directories, this function returns NULL.
 */
void *Node_replaceFileContents(Node_T node, void *newContents, size_t newLength) {
    void *oldContents = NULL;

    assert(node != NULL);

    if (node->type == FILE_NODE) {
        oldContents = node->contents;
        node->contents = newContents;
        node->contentLength = newLength;
    }

    return oldContents;
}

/*
 * Creates a string representation of the node's path.
 * Returns a dynamically allocated string that should be freed by the caller.
 */
char *Node_toString(Node_T node) {
    char *pathCopy;

    assert(node != NULL);

    pathCopy = malloc(Path_getStrLength(node->path) + 1);
    if (pathCopy == NULL) {
        return NULL;
    }

    return strcpy(pathCopy, Path_getPathname(node->path));
}