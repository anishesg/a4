/*--------------------------------------------------------------------*/
/* ft.c                                                               */
/* Author: anish                                              */
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
 * A File Tree (FT) represents a hierarchy of files and directories.
 * It maintains the following state variables:
 * 1. isInitialized: a flag indicating whether the FT is initialized.
 * 2. rootNode: a pointer to the root node of the FT.
 * 3. nodeCount: the total number of nodes in the FT.
 */

/* Flag indicating whether the FT has been initialized */
static boolean isInitialized = FALSE;
/* Pointer to the root node of the FT */
static Node_T rootNode = NULL;
/* Total number of nodes in the FT */
static size_t nodeCount = 0;

/* --------------------------------------------------------------------
   The following functions help in traversing the File Tree and finding
   nodes based on their paths.
*/

/*
 * Traverses the File Tree (FT) starting from the rootNode towards the given targetPath.
 * If traversal is successful, sets *furthestNode to the deepest node reached (which may
 * be a prefix of targetPath or NULL if the tree is empty).
 * Returns SUCCESS on successful traversal.
 * Returns CONFLICTING_PATH if the root's path is not a prefix of targetPath.
 * Returns MEMORY_ERROR if memory allocation fails during the operation.
 */
static int FT_traversePath(Path_T targetPath, Node_T *furthestNode) {
    int status;
    Path_T currentPrefix = NULL;
    Node_T currentNode;
    Node_T childNode = NULL;
    size_t targetDepth;
    size_t depthIndex;
    size_t childIndex;

    assert(targetPath != NULL);
    assert(furthestNode != NULL);

    /* If the tree is empty, cannot traverse further */
    if (rootNode == NULL) {
        *furthestNode = NULL;
        return SUCCESS;
    }

    /* Get the first prefix (depth 1) of the target path */
    status = Path_prefix(targetPath, 1, &currentPrefix);
    if (status != SUCCESS) {
        *furthestNode = NULL;
        return status;
    }

    /* If the root's path does not match the first prefix, return CONFLICTING_PATH */
    if (Path_comparePath(Node_getPath(rootNode), currentPrefix) != 0) {
        Path_free(currentPrefix);
        *furthestNode = NULL;
        return CONFLICTING_PATH;
    }

    /* Free the prefix as it is no longer needed */
    Path_free(currentPrefix);
    currentPrefix = NULL;

    /* Start traversal from the root node */
    currentNode = rootNode;
    targetDepth = Path_getDepth(targetPath);

    for (depthIndex = 2; depthIndex <= targetDepth; depthIndex++) {
        /* Get the prefix of targetPath at the current depth */
        status = Path_prefix(targetPath, depthIndex, &currentPrefix);
        if (status != SUCCESS) {
            *furthestNode = NULL;
            return status;
        }

        /* Check if the current node has a child with the current prefix */
        if (Node_hasChild(currentNode, currentPrefix, &childIndex)) {
            /* Move to the child node */
            Path_free(currentPrefix);
            currentPrefix = NULL;

            status = Node_getChild(currentNode, childIndex, &childNode);
            if (status != SUCCESS) {
                *furthestNode = NULL;
                return status;
            }

            currentNode = childNode;
        } else {
            /* No child with the current prefix; traversal ends here */
            break;
        }
    }

    /* Free the current prefix as it is no longer needed */
    Path_free(currentPrefix);
    *furthestNode = currentNode;
    return SUCCESS;
}

/*
 * Finds the node in the File Tree (FT) with the absolute path specified by pathString.
 * If the node is found, sets *resultNode to point to it and returns SUCCESS.
 * Returns INITIALIZATION_ERROR if the FT is not initialized.
 * Returns BAD_PATH if pathString is not a valid path.
 * Returns CONFLICTING_PATH if the root's path is not a prefix of pathString.
 * Returns NO_SUCH_PATH if the node does not exist in the FT.
 * Returns MEMORY_ERROR if memory allocation fails during the operation.
 */
static int FT_findNode(const char *pathString, Node_T *resultNode) {
    Path_T targetPath = NULL;
    Node_T foundNode = NULL;
    int status;

    assert(pathString != NULL);
    assert(resultNode != NULL);

    if (!isInitialized) {
        *resultNode = NULL;
        return INITIALIZATION_ERROR;
    }

    /* Create a Path_T object from the path string */
    status = Path_new(pathString, &targetPath);
    if (status != SUCCESS) {
        *resultNode = NULL;
        return status;
    }

    /* Traverse the FT to find the furthest node towards the target path */
    status = FT_traversePath(targetPath, &foundNode);
    if (status != SUCCESS) {
        Path_free(targetPath);
        *resultNode = NULL;
        return status;
    }

    if (foundNode == NULL) {
        /* The path does not exist in the FT */
        Path_free(targetPath);
        *resultNode = NULL;
        return NO_SUCH_PATH;
    }

    /* Check if the found node's path matches the target path */
    if (Path_comparePath(Node_getPath(foundNode), targetPath) != 0) {
        Path_free(targetPath);
        *resultNode = NULL;
        return NO_SUCH_PATH;
    }

    Path_free(targetPath);
    *resultNode = foundNode;
    return SUCCESS;
}

/*
 * Removes the node at the specified pathString from the File Tree (FT).
 * The nodeType indicates whether it's a directory or a file.
 * Returns SUCCESS if the node is successfully removed.
 * Returns INITIALIZATION_ERROR if the FT is not initialized.
 * Returns BAD_PATH if pathString is not a valid path.
 * Returns CONFLICTING_PATH if the path conflicts with existing paths in the FT.
 * Returns NO_SUCH_PATH if the node does not exist in the FT.
 * Returns NOT_A_DIRECTORY if attempting to remove a directory but the node is a file.
 * Returns NOT_A_FILE if attempting to remove a file but the node is a directory.
 * Returns MEMORY_ERROR if memory allocation fails during the operation.
 */
static int FT_rm(const char *pathString, NodeType nodeType) {
    int status;
    Node_T targetNode = NULL;

    assert(pathString != NULL);
    assert(CheckerFT_isValid(isInitialized, rootNode, nodeCount));

    status = FT_findNode(pathString, &targetNode);
    if (status != SUCCESS) {
        return status;
    }

    /* Check if the node type matches */
    if (Node_getType(targetNode) != nodeType) {
        if (nodeType == FILE_NODE) {
            return NOT_A_FILE;
        } else if (nodeType == DIRECTORY_NODE) {
            return NOT_A_DIRECTORY;
        }
    }

    /* Remove the node and update the node count */
    nodeCount -= Node_free(targetNode);
    if (nodeCount == 0) {
        rootNode = NULL;
    }

    assert(CheckerFT_isValid(isInitialized, rootNode, nodeCount));
    return SUCCESS;
}

/*
 * Inserts a new node into the File Tree (FT) at the specified pathString.
 * The nodeType indicates whether it's a directory or a file.
 * For files, contents and contentLength specify the file's content and its size.
 * Returns SUCCESS if the node is successfully inserted.
 * Returns INITIALIZATION_ERROR if the FT is not initialized.
 * Returns BAD_PATH if pathString is not a valid path.
 * Returns CONFLICTING_PATH if the path conflicts with existing paths in the FT.
 * Returns NOT_A_DIRECTORY if a prefix of pathString exists as a file.
 * Returns ALREADY_IN_TREE if a node already exists at pathString.
 * Returns MEMORY_ERROR if memory allocation fails during the operation.
 */
static int FT_insert(const char *pathString, NodeType nodeType, void *contents, size_t contentLength) {
    int status;
    Path_T targetPath = NULL;
    Node_T firstNewNode = NULL;
    Node_T currentNode = NULL;
    size_t targetDepth, currentDepth;
    size_t newNodesCount = 0;

    assert(pathString != NULL);
    assert(CheckerFT_isValid(isInitialized, rootNode, nodeCount));

    /* Validate and create a Path_T object from the path string */
    if (!isInitialized) {
        return INITIALIZATION_ERROR;
    }

    status = Path_new(pathString, &targetPath);
    if (status != SUCCESS) {
        return status;
    }

    /* Find the closest ancestor of targetPath that exists in the FT */
    status = FT_traversePath(targetPath, &currentNode);
    if (status != SUCCESS) {
        Path_free(targetPath);
        return status;
    }

    /* If no ancestor node is found and the FT is not empty, the path is conflicting */
    if (currentNode == NULL && rootNode != NULL) {
        Path_free(targetPath);
        return CONFLICTING_PATH;
    }

    targetDepth = Path_getDepth(targetPath);

    if (currentNode == NULL) {
        /* Inserting a new root node */
        if (nodeType == FILE_NODE && targetDepth == 1) {
            /* Cannot insert a file as the root node */
            Path_free(targetPath);
            return CONFLICTING_PATH;
        }
        currentDepth = 1;
    } else {
        if (Node_getType(currentNode) == FILE_NODE) {
            /* Cannot insert under a file node */
            Path_free(targetPath);
            return NOT_A_DIRECTORY;
        }

        currentDepth = Path_getDepth(Node_getPath(currentNode)) + 1;

        /* Check if the node already exists */
        if (currentDepth == targetDepth + 1 && !Path_comparePath(targetPath, Node_getPath(currentNode))) {
            Path_free(targetPath);
            return ALREADY_IN_TREE;
        }
    }

    /* Build the rest of the path, one level at a time */
    while (currentDepth <= targetDepth) {
        Path_T currentPrefix = NULL;
        Node_T newNode = NULL;

        /* Get the prefix of targetPath at the current depth */
        status = Path_prefix(targetPath, currentDepth, &currentPrefix);
        if (status != SUCCESS) {
            Path_free(targetPath);
            if (firstNewNode != NULL) {
                (void) Node_free(firstNewNode);
            }
            assert(CheckerFT_isValid(isInitialized, rootNode, nodeCount));
            return status;
        }

        /* Insert a new node at this level */
        if (currentDepth < targetDepth) {
            /* Intermediate nodes are directories */
            status = Node_new(currentPrefix, currentNode, &newNode, DIRECTORY_NODE);
        } else {
            /* The final node is of the specified nodeType */
            status = Node_new(currentPrefix, currentNode, &newNode, nodeType);
            if (status == SUCCESS && nodeType == FILE_NODE) {
                /* Set the file contents */
                Node_setFileContents(newNode, contents, contentLength);
            }
        }

        if (status != SUCCESS) {
            Path_free(targetPath);
            Path_free(currentPrefix);
            if (firstNewNode != NULL) {
                (void) Node_free(firstNewNode);
            }
            assert(CheckerFT_isValid(isInitialized, rootNode, nodeCount));
            return status;
        }

        /* Prepare for the next level */
        Path_free(currentPrefix);
        currentNode = newNode;
        newNodesCount++;
        if (firstNewNode == NULL) {
            firstNewNode = currentNode;
        }
        currentDepth++;
    }

    Path_free(targetPath);

    /* Update the FT's state variables */
    if (rootNode == NULL) {
        rootNode = firstNewNode;
    }
    nodeCount += newNodesCount;

    assert(CheckerFT_isValid(isInitialized, rootNode, nodeCount));
    return SUCCESS;
}

/* -------------------------------------------------------------------- */

int FT_insertDir(const char *pathString) {
    assert(pathString != NULL);

    return FT_insert(pathString, DIRECTORY_NODE, NULL, 0);
}

boolean FT_containsDir(const char *pathString) {
    Node_T targetNode = NULL;
    int status;

    assert(pathString != NULL);

    status = FT_findNode(pathString, &targetNode);
    if (status != SUCCESS) {
        return FALSE;
    }

    return (boolean)(Node_getType(targetNode) == DIRECTORY_NODE);
}

int FT_rmDir(const char *pathString) {
    assert(pathString != NULL);

    return FT_rm(pathString, DIRECTORY_NODE);
}

int FT_insertFile(const char *pathString, void *contents, size_t contentLength) {
    assert(pathString != NULL);

    return FT_insert(pathString, FILE_NODE, contents, contentLength);
}

boolean FT_containsFile(const char *pathString) {
    Node_T targetNode = NULL;
    int status;

    assert(pathString != NULL);

    status = FT_findNode(pathString, &targetNode);
    if (status != SUCCESS) {
        return FALSE;
    }

    return (boolean)(Node_getType(targetNode) == FILE_NODE);
}

int FT_rmFile(const char *pathString) {
    assert(pathString != NULL);

    return FT_rm(pathString, FILE_NODE);
}

void *FT_getFileContents(const char *pathString) {
    int status;
    Node_T targetNode = NULL;

    assert(pathString != NULL);

    status = FT_findNode(pathString, &targetNode);
    if (status != SUCCESS) {
        return NULL;
    }

    if (Node_getType(targetNode) != FILE_NODE) {
        /* The node is not a file */
        return NULL;
    }

    return Node_getFileContents(targetNode);
}

void *FT_replaceFileContents(const char *pathString, void *newContents, size_t newLength) {
    int status;
    Node_T targetNode = NULL;
    void *oldContents = NULL;

    assert(pathString != NULL);

    status = FT_findNode(pathString, &targetNode);
    if (status != SUCCESS) {
        return NULL;
    }

    if (Node_getType(targetNode) != FILE_NODE) {
        /* The node is not a file */
        return NULL;
    }

    /* Replace the file contents */
    oldContents = Node_getFileContents(targetNode);
    Node_setFileContents(targetNode, newContents, newLength);

    return oldContents;
}

int FT_stat(const char *pathString, boolean *isFile, size_t *size) {
    int status;
    Node_T targetNode = NULL;

    assert(pathString != NULL);
    assert(isFile != NULL);
    assert(size != NULL);

    status = FT_findNode(pathString, &targetNode);
    if (status != SUCCESS) {
        return status;
    }

    if (Node_getType(targetNode) == FILE_NODE) {
        *isFile = TRUE;
        *size = Node_getFileLength(targetNode);
    } else {
        *isFile = FALSE;
        *size = 0;
    }

    return SUCCESS;
}

int FT_init(void) {
    assert(CheckerFT_isValid(isInitialized, rootNode, nodeCount));

    if (isInitialized) {
        return INITIALIZATION_ERROR;
    }

    isInitialized = TRUE;
    rootNode = NULL;
    nodeCount = 0;

    assert(CheckerFT_isValid(isInitialized, rootNode, nodeCount));
    return SUCCESS;
}

int FT_destroy(void) {
    assert(CheckerFT_isValid(isInitialized, rootNode, nodeCount));

    if (!isInitialized) {
        return INITIALIZATION_ERROR;
    }

    if (rootNode != NULL) {
        nodeCount -= Node_free(rootNode);
        rootNode = NULL;
    }

    isInitialized = FALSE;

    assert(CheckerFT_isValid(isInitialized, rootNode, nodeCount));
    return SUCCESS;
}

/* --------------------------------------------------------------------
   The following auxiliary functions are used for generating the
   string representation of the FT.
*/

/*
 * Performs a pre-order traversal of the tree rooted at node,
 * inserting each node into the dynamic array dynArray starting at index.
 * Returns the next unused index in dynArray after the insertion(s).
 */
static size_t FT_preOrderTraversal(Node_T node, DynArray_T dynArray, size_t index) {
    size_t childCount;
    size_t i;
    int status;
    Node_T childNode = NULL;

    assert(dynArray != NULL);

    if (node != NULL) {
        (void) DynArray_set(dynArray, index, node);
        index++;

        childCount = Node_getNumChildren(node);
        for (i = 0; i < childCount; i++) {
            status = Node_getChild(node, i, &childNode);
            assert(status == SUCCESS);
            index = FT_preOrderTraversal(childNode, dynArray, index);
        }
    }
    return index;
}

/*
 * Calculates the total length of the string representation of the FT.
 * Adds the length of the node's path plus one (for the newline character) to *accumulator.
 */
static void FT_strlenAccumulate(Node_T node, size_t *accumulator) {
    assert(accumulator != NULL);

    if (node != NULL) {
        *accumulator += (Path_getStrLength(Node_getPath(node)) + 1);
    }
}

/*
 * Concatenates the node's path to the string accumulator, followed by a newline character.
 */
static void FT_strcatAccumulate(Node_T node, char *accumulator) {
    assert(accumulator != NULL);

    if (node != NULL) {
        strcat(accumulator, Path_getPathname(Node_getPath(node)));
        strcat(accumulator, "\n");
    }
}

char *FT_toString(void) {
    DynArray_T nodesArray;
    size_t totalStrLength = 1; /* Start with 1 for the null terminator */
    char *resultString = NULL;

    if (!isInitialized) {
        return NULL;
    }

    nodesArray = DynArray_new(nodeCount);
    if (nodesArray == NULL) {
        return NULL;
    }

    /* Perform pre-order traversal to fill the nodesArray */
    (void) FT_preOrderTraversal(rootNode, nodesArray, 0);

    /* Calculate the total length of the string representation */
    DynArray_map(nodesArray, (void (*)(void *, void *)) FT_strlenAccumulate, (void *) &totalStrLength);

    /* Allocate memory for the result string */
    resultString = malloc(totalStrLength);
    if (resultString == NULL) {
        DynArray_free(nodesArray);
        return NULL;
    }
    *resultString = '\0';

    /* Concatenate the paths into the result string */
    DynArray_map(nodesArray, (void (*)(void *, void *)) FT_strcatAccumulate, (void *) resultString);

    DynArray_free(nodesArray);

    return resultString;
}