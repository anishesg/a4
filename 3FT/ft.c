/*--------------------------------------------------------------------*/
/* ft.c                                                               */
/* Author: anish                                              */
/*--------------------------------------------------------------------*/

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "ft.h"
#include "path.h"
#include "dynarray.h"
#include "a4def.h"
#include "nodeFT.h" 

/* state variables for the file tree */
static boolean isInitialized = FALSE;
static Node_T root = NULL;
static size_t nodeCount = 0;

/* helper function to traverse the tree towards a given path */
static int FT_traversePath(Path_T path, Node_T *node) {
    int status;
    Path_T prefix = NULL;
    Node_T currentNode = NULL;
    size_t depth, level;

    assert(path != NULL);
    assert(node != NULL);

    if (root == NULL) {
        *node = NULL;
        return SUCCESS;
    }

    /* make sure the root's path is a prefix of the given path */
    status = Path_prefix(path, 1, &prefix);
    if (status != SUCCESS) {
        *node = NULL;
        return status;
    }

    if (Path_comparePath(Node_getPath(root), prefix) != 0) {
        Path_free(prefix);
        *node = NULL;
        return CONFLICTING_PATH;
    }
    Path_free(prefix);

    currentNode = root;
    depth = Path_getDepth(path);

    for (level = 2; level <= depth; level++) {
        size_t childIndex;
        boolean hasChild;
        Node_T childNode = NULL;

        status = Path_prefix(path, level, &prefix);
        if (status != SUCCESS) {
            *node = NULL;
            return status;
        }

        hasChild = Node_hasChild(currentNode, prefix, &childIndex);
        Path_free(prefix);

        if (hasChild) {
            status = Node_getChild(currentNode, childIndex, &childNode);
            if (status != SUCCESS) {
                *node = NULL;
                return status;
            }
            if (Node_getType(childNode) == IS_FILE && level != depth) {
                *node = NULL;
                return NOT_A_DIRECTORY;
            }
            currentNode = childNode;
        } else {
            break;
        }
    }

    *node = currentNode;
    return SUCCESS;
}

/* helper function to find a node with the given path */
static int FT_findNode(const char *pathStr, Node_T *node) {
    int status;
    Path_T path = NULL;
    Node_T foundNode = NULL;

    assert(pathStr != NULL);
    assert(node != NULL);

    if (!isInitialized) {
        *node = NULL;
        return INITIALIZATION_ERROR;
    }

    status = Path_new(pathStr, &path);
    if (status != SUCCESS) {
        *node = NULL;
        return status;
    }

    status = FT_traversePath(path, &foundNode);
    if (status != SUCCESS) {
        Path_free(path);
        *node = NULL;
        return status;
    }

    if (foundNode == NULL) {
        Path_free(path);
        *node = NULL;
        return NO_SUCH_PATH;
    }

    if (Path_comparePath(Node_getPath(foundNode), path) != 0) {
        Path_free(path);
        *node = NULL;
        return NO_SUCH_PATH;
    }

    Path_free(path);
    *node = foundNode;
    return SUCCESS;
}

/* function to insert a directory into the file tree */
int FT_insertDir(const char *pcPath) {
    int status;
    Path_T path = NULL;
    Node_T currentNode = NULL;
    size_t depth, level;
    size_t newNodes = 0;
    Node_T firstNewNode = NULL;

    assert(pcPath != NULL);

    if (!isInitialized) {
        return INITIALIZATION_ERROR;
    }

    status = Path_new(pcPath, &path);
    if (status != SUCCESS) {
        return status;
    }

    status = FT_traversePath(path, &currentNode);
    if (status != SUCCESS) {
        Path_free(path);
        return status;
    }

    if (currentNode == NULL && root != NULL) {
        Path_free(path);
        return CONFLICTING_PATH;
    }

    depth = Path_getDepth(path);
    if (currentNode == NULL) {
        level = 1;
    } else {
        level = Path_getDepth(Node_getPath(currentNode)) + 1;
        if (Path_comparePath(Node_getPath(currentNode), path) == 0) {
            Path_free(path);
            return ALREADY_IN_TREE;
        }
    }

    while (level <= depth) {
        Path_T prefix = NULL;
        Node_T newNode = NULL;

        status = Path_prefix(path, level, &prefix);
        if (status != SUCCESS) {
            Path_free(path);
            if (firstNewNode != NULL) {
                Node_free(firstNewNode);
            }
            return status;
        }

        status = Node_new(prefix, IS_DIRECTORY, currentNode, &newNode);
        Path_free(prefix);
        if (status != SUCCESS) {
            Path_free(path);
            if (firstNewNode != NULL) {
                Node_free(firstNewNode);
            }
            return status;
        }

        if (firstNewNode == NULL) {
            firstNewNode = newNode;
        }

        currentNode = newNode;
        newNodes++;
        level++;
    }

    if (root == NULL) {
        root = firstNewNode;
    }

    nodeCount += newNodes;
    Path_free(path);
    return SUCCESS;
}

/* function to check if a directory exists */
boolean FT_containsDir(const char *pcPath) {
    Node_T node = NULL;
    int status;

    assert(pcPath != NULL);

    if (root == NULL) {
        return FALSE;
    }

    status = FT_findNode(pcPath, &node);
    if (status != SUCCESS || Node_getType(node) != IS_DIRECTORY) {
        return FALSE;
    }

    return TRUE;
}

/* function to remove a directory from the file tree */
int FT_rmDir(const char *pcPath) {
    Node_T node = NULL;
    int status;

    assert(pcPath != NULL);

    status = FT_findNode(pcPath, &node);
    if (status != SUCCESS) {
        return status;
    }

    if (Node_getType(node) != IS_DIRECTORY) {
        return NOT_A_DIRECTORY;
    }

    nodeCount -= Node_free(node);  /* Use Node_free */
    if (nodeCount == 0) {
        root = NULL;
    }

    return SUCCESS;
}

/* function to insert a file into the file tree */
int FT_insertFile(const char *pcPath, void *pvContents, size_t ulLength) {
    int status;
    Path_T path = NULL;
    Node_T currentNode = NULL;
    size_t depth, level;
    size_t newNodes = 0;
    Node_T firstNewNode = NULL;

    assert(pcPath != NULL);

    if (!isInitialized) {
        return INITIALIZATION_ERROR;
    }

    status = Path_new(pcPath, &path);
    if (status != SUCCESS) {
        return status;
    }

    if (Path_getDepth(path) == 1) {
        Path_free(path);
        return CONFLICTING_PATH;
    }

    status = FT_traversePath(path, &currentNode);
    if (status != SUCCESS) {
        Path_free(path);
        return status;
    }

    if (currentNode == NULL && root != NULL) {
        Path_free(path);
        return CONFLICTING_PATH;
    }

    depth = Path_getDepth(path);
    if (currentNode == NULL) {
        Path_free(path);
        return CONFLICTING_PATH;
    } else {
        level = Path_getDepth(Node_getPath(currentNode)) + 1;
        if (Path_comparePath(Node_getPath(currentNode), path) == 0) {
            Path_free(path);
            return ALREADY_IN_TREE;
        }
    }

    while (level <= depth) {
        Path_T prefix = NULL;
        Node_T newNode = NULL;

        status = Path_prefix(path, level, &prefix);
        if (status != SUCCESS) {
            Path_free(path);
            if (firstNewNode != NULL) {
                Node_free(firstNewNode);
            }
            return status;
        }

        if (level == depth) {
            status = Node_new(prefix, IS_FILE, currentNode, &newNode);
        } else {
            status = Node_new(prefix, IS_DIRECTORY, currentNode, &newNode);
        }
        Path_free(prefix);
        if (status != SUCCESS) {
            Path_free(path);
            if (firstNewNode != NULL) {
                Node_free(firstNewNode);
            }
            return status;
        }

        if (level == depth) {
            status = Node_insertFileContents(newNode, pvContents, ulLength);
            if (status != SUCCESS) {
                Path_free(path);
                if (firstNewNode != NULL) {
                    Node_free(firstNewNode);
                }
                return status;
            }
        }

        if (firstNewNode == NULL) {
            firstNewNode = newNode;
        }

        currentNode = newNode;
        newNodes++;
        level++;
    }

    nodeCount += newNodes;
    Path_free(path);
    return SUCCESS;
}

/* function to check if a file exists */
boolean FT_containsFile(const char *pcPath) {
    Node_T node = NULL;
    int status;

    assert(pcPath != NULL);

    if (root == NULL) {
        return FALSE;
    }

    status = FT_findNode(pcPath, &node);
    if (status != SUCCESS || Node_getType(node) != IS_FILE) {
        return FALSE;
    }

    return TRUE;
}

/* function to remove a file from the file tree */
int FT_rmFile(const char *pcPath) {
    Node_T node = NULL;
    int status;

    assert(pcPath != NULL);

    status = FT_findNode(pcPath, &node);
    if (status != SUCCESS) {
        return status;
    }

    if (Node_getType(node) != IS_FILE) {
        return NOT_A_FILE;
    }

    nodeCount -= Node_free(node);  /* Use Node_free */
    if (nodeCount == 0) {
        root = NULL;
    }

    return SUCCESS;
}

/* function to get the contents of a file */
void *FT_getFileContents(const char *pcPath) {
    Node_T node = NULL;
    int status;

    assert(pcPath != NULL);

    status = FT_findNode(pcPath, &node);
    if (status != SUCCESS || Node_getType(node) != IS_FILE) {
        return NULL;
    }

    return Node_getContents(node);
}

/* function to replace the contents of a file */
void *FT_replaceFileContents(const char *pcPath, void *pvNewContents, size_t ulNewLength) {
    Node_T node = NULL;
    void *oldContents = NULL;
    int status;

    assert(pcPath != NULL);

    status = FT_findNode(pcPath, &node);
    if (status != SUCCESS || Node_getType(node) != IS_FILE) {
        return NULL;
    }

    oldContents = Node_getContents(node);
    status = Node_insertFileContents(node, pvNewContents, ulNewLength);
    if (status != SUCCESS) {
        return NULL;
    }

    return oldContents;
}

/* function to get information about a path in the file tree */
int FT_stat(const char *pcPath, boolean *pbIsFile, size_t *pulSize) {
    Node_T node = NULL;
    int status;

    assert(pcPath != NULL);
    assert(pbIsFile != NULL);
    assert(pulSize != NULL);

    status = FT_findNode(pcPath, &node);
    if (status != SUCCESS) {
        return status;
    }

    if (Node_getType(node) == IS_FILE) {
        *pbIsFile = TRUE;
        *pulSize = Node_getSize(node);
    } else {
        *pbIsFile = FALSE;
    }

    return SUCCESS;
}

/* function to initialize the file tree */
int FT_init(void) {
    if (isInitialized) {
        return INITIALIZATION_ERROR;
    }

    isInitialized = TRUE;
    root = NULL;
    nodeCount = 0;

    return SUCCESS;
}

/* function to destroy the file tree */
int FT_destroy(void) {
    if (!isInitialized) {
        return INITIALIZATION_ERROR;
    }

    if (root != NULL) {
        nodeCount -= Node_free(root);  /* Use Node_free */
        root = NULL;
    }

    isInitialized = FALSE;
    return SUCCESS;
}

/* helper function for pre-order traversal */
static void FT_preOrderTraversal(Node_T node, DynArray_T array, size_t *index) {
    size_t numChildren;
    size_t i;

    if (node == NULL) {
        return;
    }

    DynArray_set(array, *index, node);
    (*index)++;

    if (Node_getType(node) == IS_DIRECTORY) {
        if (Node_getNumChildren(node, &numChildren) == SUCCESS) {
            /* First, add file children */
            for (i = 0; i < numChildren; i++) {
                Node_T childNode = NULL;
                if (Node_getChild(node, i, &childNode) == SUCCESS && Node_getType(childNode) == IS_FILE) {
                    FT_preOrderTraversal(childNode, array, index);
                }
            }
            /* Then, add directory children */
            for (i = 0; i < numChildren; i++) {
                Node_T childNode = NULL;
                if (Node_getChild(node, i, &childNode) == SUCCESS && Node_getType(childNode) == IS_DIRECTORY) {
                    FT_preOrderTraversal(childNode, array, index);
                }
            }
        }
    }
}

/* function to generate a string representation of the file tree */
char *FT_toString(void) {
    DynArray_T nodes;
    size_t totalLength = 1;
    char *result = NULL;
    size_t index = 0;
    size_t i;

    if (!isInitialized) {
        return NULL;
    }

    nodes = DynArray_new(nodeCount);
    if (nodes == NULL) {
        return NULL;
    }

    FT_preOrderTraversal(root, nodes, &index);

    for (i = 0; i < nodeCount; i++) {
        Node_T node = DynArray_get(nodes, i);
        totalLength += Path_getStrLength(Node_getPath(node)) + 1;
    }

    result = malloc(totalLength);
    if (result == NULL) {
        DynArray_free(nodes);
        return NULL;
    }
    *result = '\0';

    for (i = 0; i < nodeCount; i++) {
        Node_T node = DynArray_get(nodes, i);
        strcat(result, Path_getPathname(Node_getPath(node)));
        strcat(result, "\n");
    }

    DynArray_free(nodes);
    return result;
}