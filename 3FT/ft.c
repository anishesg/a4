/*--------------------------------------------------------------------*/
/* ft.c                                                               */
/* Author: anish                                            */
/*--------------------------------------------------------------------*/

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "ft.h"
#include "a4def.h"
#include "path.h"
#include "dynarray.h"
#include "nodeFT.h"

/* State variables for the File Tree */
static boolean isInitialized = FALSE;
static Node_T root = NULL;
static size_t nodeCount = 0;

/* Helper function to traverse the File Tree as far as possible towards a given path */
static int FT_traversePath(Path_T targetPath, Node_T *resultNode) {
    int status;
    Path_T prefixPath = NULL;
    Node_T currentNode = NULL;
    size_t depth;
    size_t level;

    assert(targetPath != NULL);
    assert(resultNode != NULL);

    if (root == NULL) {
        *resultNode = NULL;
        return SUCCESS;
    }

    /* Check if root's path is a prefix of targetPath */
    status = Path_prefix(targetPath, 1, &prefixPath);
    if (status != SUCCESS) {
        *resultNode = NULL;
        return status;
    }

    if (Path_comparePath(Node_getPath(root), prefixPath) != 0) {
        Path_free(prefixPath);
        *resultNode = NULL;
        return CONFLICTING_PATH;
    }
    Path_free(prefixPath);

    currentNode = root;
    depth = Path_getDepth(targetPath);

    for (level = 2; level <= depth; level++) {
        size_t childIndex;
        boolean found = FALSE;
        size_t numChildren;
        Node_T childNode = NULL;

        status = Path_prefix(targetPath, level, &prefixPath);
        if (status != SUCCESS) {
            *resultNode = NULL;
            return status;
        }

        status = Node_getNumChildren(currentNode, &numChildren);
        if (status != SUCCESS) {
            Path_free(prefixPath);
            *resultNode = NULL;
            return status;
        }

        for (childIndex = 0; childIndex < numChildren; childIndex++) {
            status = Node_getChild(currentNode, childIndex, &childNode);
            if (status != SUCCESS) {
                Path_free(prefixPath);
                *resultNode = NULL;
                return status;
            }

            if (Path_comparePath(Node_getPath(childNode), prefixPath) == 0) {
                found = TRUE;
                break;
            }
        }
        Path_free(prefixPath);

        if (found) {
            if (Node_getType(childNode) == IS_FILE && level != depth) {
                *resultNode = NULL;
                return NOT_A_DIRECTORY;
            }
            currentNode = childNode;
        } else {
            break;
        }
    }

    *resultNode = currentNode;
    return SUCCESS;
}

/* Helper function to find a node with a given path in the File Tree */
static int FT_findNode(const char *path, Node_T *foundNode) {
    int status;
    Path_T searchPath = NULL;
    Node_T node = NULL;

    assert(path != NULL);
    assert(foundNode != NULL);

    if (!isInitialized) {
        *foundNode = NULL;
        return INITIALIZATION_ERROR;
    }

    status = Path_new(path, &searchPath);
    if (status != SUCCESS) {
        *foundNode = NULL;
        return status;
    }

    status = FT_traversePath(searchPath, &node);
    if (status != SUCCESS) {
        Path_free(searchPath);
        *foundNode = NULL;
        return status;
    }

    if (node == NULL) {
        Path_free(searchPath);
        *foundNode = NULL;
        return NO_SUCH_PATH;
    }

    if (Path_comparePath(Node_getPath(node), searchPath) != 0) {
        Path_free(searchPath);
        *foundNode = NULL;
        return NO_SUCH_PATH;
    }

    Path_free(searchPath);
    *foundNode = node;
    return SUCCESS;
}

/* Helper function for pre-order traversal used in FT_toString */
static void FT_preOrder(Node_T node, DynArray_T array) {
    size_t numChildren;
    size_t i;
    Node_T childNode = NULL;

    if (node == NULL) {
        return;
    }

    DynArray_add(array, node);

    if (Node_getNumChildren(node, &numChildren) == SUCCESS) {
        /* First, add file children */
        for (i = 0; i < numChildren; i++) {
            if (Node_getChild(node, i, &childNode) == SUCCESS && Node_getType(childNode) == IS_FILE) {
                FT_preOrder(childNode, array);
            }
        }
        /* Then, add directory children */
        for (i = 0; i < numChildren; i++) {
            if (Node_getChild(node, i, &childNode) == SUCCESS && Node_getType(childNode) == IS_DIRECTORY) {
                FT_preOrder(childNode, array);
            }
        }
    }
}

/* Function to insert a directory into the File Tree */
int FT_insertDir(const char *path) {
    int status;
    Path_T newPath = NULL;
    Node_T parentNode = NULL;
    Node_T newNode = NULL;
    size_t depth;
    size_t level;
    size_t newNodesCount = 0;

    assert(path != NULL);

    if (!isInitialized) {
        return INITIALIZATION_ERROR;
    }

    status = Path_new(path, &newPath);
    if (status != SUCCESS) {
        return status;
    }

    status = FT_traversePath(newPath, &parentNode);
    if (status != SUCCESS) {
        Path_free(newPath);
        return status;
    }

    if (parentNode == NULL && root != NULL) {
        Path_free(newPath);
        return CONFLICTING_PATH;
    }

    depth = Path_getDepth(newPath);
    level = parentNode ? Path_getDepth(Node_getPath(parentNode)) + 1 : 1;

    if (parentNode && Path_comparePath(Node_getPath(parentNode), newPath) == 0) {
        Path_free(newPath);
        return ALREADY_IN_TREE;
    }

    while (level <= depth) {
        Path_T prefixPath = NULL;

        status = Path_prefix(newPath, level, &prefixPath);
        if (status != SUCCESS) {
            Path_free(newPath);
            return status;
        }

        status = Node_create(prefixPath, IS_DIRECTORY, parentNode, &newNode);
        if (status != SUCCESS) {
            Path_free(prefixPath);
            Path_free(newPath);
            return status;
        }

        parentNode = newNode;
        if (root == NULL) {
            root = newNode;
        }
        newNodesCount++;
        level++;
        Path_free(prefixPath);
    }

    nodeCount += newNodesCount;
    Path_free(newPath);
    return SUCCESS;
}

/* Function to check if a directory exists in the File Tree */
boolean FT_containsDir(const char *path) {
    int status;
    Node_T node = NULL;

    assert(path != NULL);

    if (root == NULL) {
        return FALSE;
    }

    status = FT_findNode(path, &node);
    if (status != SUCCESS || Node_getType(node) != IS_DIRECTORY) {
        return FALSE;
    }

    return TRUE;
}

/* Function to remove a directory from the File Tree */
int FT_rmDir(const char *path) {
    int status;
    Node_T node = NULL;

    assert(path != NULL);

    status = FT_findNode(path, &node);
    if (status != SUCCESS) {
        return status;
    }

    if (Node_getType(node) != IS_DIRECTORY) {
        return NOT_A_DIRECTORY;
    }

    nodeCount -= Node_destroy(node);
    if (nodeCount == 0) {
        root = NULL;
    }

    return SUCCESS;
}

/* Function to insert a file into the File Tree */
int FT_insertFile(const char *path, void *contents, size_t length) {
    int status;
    Path_T newPath = NULL;
    Node_T parentNode = NULL;
    Node_T newNode = NULL;
    size_t depth;
    size_t level;
    size_t newNodesCount = 0;

    assert(path != NULL);
    assert(contents != NULL);

    if (!isInitialized) {
        return INITIALIZATION_ERROR;
    }

    status = Path_new(path, &newPath);
    if (status != SUCCESS) {
        return status;
    }

    if (Path_getDepth(newPath) == 1) {
        Path_free(newPath);
        return CONFLICTING_PATH;
    }

    status = FT_traversePath(newPath, &parentNode);
    if (status != SUCCESS) {
        Path_free(newPath);
        return status;
    }

    if (parentNode == NULL && root != NULL) {
        Path_free(newPath);
        return CONFLICTING_PATH;
    }

    depth = Path_getDepth(newPath);
    level = parentNode ? Path_getDepth(Node_getPath(parentNode)) + 1 : 1;

    if (parentNode && Path_comparePath(Node_getPath(parentNode), newPath) == 0) {
        Path_free(newPath);
        return ALREADY_IN_TREE;
    }

    while (level <= depth) {
        Path_T prefixPath = NULL;

        status = Path_prefix(newPath, level, &prefixPath);
        if (status != SUCCESS) {
            Path_free(newPath);
            return status;
        }

        if (level == depth) {
            status = Node_create(prefixPath, IS_FILE, parentNode, &newNode);
            if (status != SUCCESS) {
                Path_free(prefixPath);
                Path_free(newPath);
                return status;
            }
            status = Node_setContents(newNode, contents, length);
            if (status != SUCCESS) {
                Path_free(prefixPath);
                Path_free(newPath);
                return status;
            }
        } else {
            status = Node_create(prefixPath, IS_DIRECTORY, parentNode, &newNode);
            if (status != SUCCESS) {
                Path_free(prefixPath);
                Path_free(newPath);
                return status;
            }
        }

        parentNode = newNode;
        if (root == NULL) {
            root = newNode;
        }
        newNodesCount++;
        level++;
        Path_free(prefixPath);
    }

    nodeCount += newNodesCount;
    Path_free(newPath);
    return SUCCESS;
}

/* Function to check if a file exists in the File Tree */
boolean FT_containsFile(const char *path) {
    int status;
    Node_T node = NULL;

    assert(path != NULL);

    if (root == NULL) {
        return FALSE;
    }

    status = FT_findNode(path, &node);
    if (status != SUCCESS || Node_getType(node) != IS_FILE) {
        return FALSE;
    }

    return TRUE;
}

/* Function to remove a file from the File Tree */
int FT_rmFile(const char *path) {
    int status;
    Node_T node = NULL;

    assert(path != NULL);

    status = FT_findNode(path, &node);
    if (status != SUCCESS) {
        return status;
    }

    if (Node_getType(node) != IS_FILE) {
        return NOT_A_FILE;
    }

    nodeCount -= Node_destroy(node);
    if (nodeCount == 0) {
        root = NULL;
    }

    return SUCCESS;
}

/* Function to get the contents of a file */
void *FT_getFileContents(const char *path) {
    int status;
    Node_T node = NULL;

    assert(path != NULL);

    status = FT_findNode(path, &node);
    if (status != SUCCESS || Node_getType(node) != IS_FILE) {
        return NULL;
    }

    return Node_getContents(node);
}

/* Function to replace the contents of a file */
void *FT_replaceFileContents(const char *path, void *newContents, size_t newLength) {
    int status;
    Node_T node = NULL;
    void *oldContents = NULL;

    assert(path != NULL);
    assert(newContents != NULL);

    status = FT_findNode(path, &node);
    if (status != SUCCESS || Node_getType(node) != IS_FILE) {
        return NULL;
    }

    oldContents = Node_getContents(node);
    status = Node_setContents(node, newContents, newLength);
    if (status != SUCCESS) {
        return NULL;
    }

    return oldContents;
}

/* Function to get information about a node in the File Tree */
int FT_stat(const char *path, boolean *isFile, size_t *size) {
    int status;
    Node_T node = NULL;

    assert(path != NULL);
    assert(isFile != NULL);
    assert(size != NULL);

    status = FT_findNode(path, &node);
    if (status != SUCCESS) {
        return status;
    }

    if (Node_getType(node) == IS_FILE) {
        *isFile = TRUE;
        *size = Node_getSize(node);
    } else {
        *isFile = FALSE;
    }

    return SUCCESS;
}

/* Function to initialize the File Tree */
int FT_init(void) {
    if (isInitialized) {
        return INITIALIZATION_ERROR;
    }

    isInitialized = TRUE;
    root = NULL;
    nodeCount = 0;

    return SUCCESS;
}

/* Function to destroy the File Tree */
int FT_destroy(void) {
    if (!isInitialized) {
        return INITIALIZATION_ERROR;
    }

    if (root != NULL) {
        nodeCount -= Node_destroy(root);
        root = NULL;
    }

    isInitialized = FALSE;
    return SUCCESS;
}

/* Function to generate a string representation of the File Tree */
char *FT_toString(void) {
    DynArray_T nodesArray;
    size_t totalLength = 1; /* For null terminator */
    char *resultString = NULL;
    size_t i;

    if (!isInitialized) {
        return NULL;
    }

    nodesArray = DynArray_new(nodeCount);
    if (nodesArray == NULL) {
        return NULL;
    }

    FT_preOrder(root, nodesArray);

    for (i = 0; i < DynArray_getLength(nodesArray); i++) {
        Node_T node = DynArray_get(nodesArray, i);
        totalLength += Path_getStrLength(Node_getPath(node)) + 1; /* +1 for newline */
    }

    resultString = malloc(totalLength);
    if (resultString == NULL) {
        DynArray_free(nodesArray);
        return NULL;
    }
    *resultString = '\0';

    for (i = 0; i < DynArray_getLength(nodesArray); i++) {
        Node_T node = DynArray_get(nodesArray, i);
        strcat(resultString, Path_getPathname(Node_getPath(node)));
        strcat(resultString, "\n");
    }

    DynArray_free(nodesArray);
    return resultString;
}