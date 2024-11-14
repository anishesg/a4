/*--------------------------------------------------------------------*/
/* nodeFT.c                                                             */
/* Author: anish                                              */
/*--------------------------------------------------------------------*/

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "nodeFT.h"
#include "a4def.h"
#include "dynarray.h"
#include "path.h"

/* Structure representing a node in the File Tree */
struct node {
    Path_T path;
    Node_T parent;
    DynArray_T children;
    nodeType type;
    void *contents;
    size_t size;
};

/* Helper function to compare nodes based on their paths */
static int Node_compare(const void *first, const void *second) {
    Node_T node1 = *(Node_T *)first;
    Node_T node2 = *(Node_T *)second;

    return Path_comparePath(node1->path, node2->path);
}

/* Helper function to find the insertion index for a child node */
static size_t Node_findChildIndex(Node_T parent, Path_T childPath) {
    size_t numChildren = DynArray_getLength(parent->children);
    size_t index;

    for (index = 0; index < numChildren; index++) {
        Node_T child = DynArray_get(parent->children, index);
        if (Path_comparePath(child->path, childPath) >= 0) {
            break;
        }
    }

    return index;
}

/* Function to create a new node */
int Node_create(Path_T path, nodeType type, Node_T parent, Node_T *resultNode) {
    Node_T newNode = NULL;
    int status;

    assert(path != NULL);
    assert(resultNode != NULL);

    newNode = malloc(sizeof(struct node));
    if (newNode == NULL) {
        *resultNode = NULL;
        return MEMORY_ERROR;
    }

    status = Path_dup(path, &newNode->path);
    if (status != SUCCESS) {
        free(newNode);
        *resultNode = NULL;
        return status;
    }

    newNode->type = type;
    newNode->parent = parent;
    newNode->contents = NULL;
    newNode->size = 0;
    newNode->children = DynArray_new(0);
    if (newNode->children == NULL) {
        Path_free(newNode->path);
        free(newNode);
        *resultNode = NULL;
        return MEMORY_ERROR;
    }

    if (parent != NULL) {
        size_t index = Node_findChildIndex(parent, path);
        if (DynArray_addAt(parent->children, index, newNode) == FALSE) {
            Path_free(newNode->path);
            DynArray_free(newNode->children);
            free(newNode);
            *resultNode = NULL;
            return MEMORY_ERROR;
        }
    }

    *resultNode = newNode;
    return SUCCESS;
}

/* Function to destroy a node and its descendants */
size_t Node_destroy(Node_T node) {
    size_t count = 0;
    size_t numChildren;
    size_t i;

    assert(node != NULL);

    if (node->parent != NULL) {
        size_t index;
        for (index = 0; index < DynArray_getLength(node->parent->children); index++) {
            if (DynArray_get(node->parent->children, index) == node) {
                DynArray_removeAt(node->parent->children, index);
                break;
            }
        }
    }

    numChildren = DynArray_getLength(node->children);
    for (i = 0; i < numChildren; i++) {
        Node_T child = DynArray_get(node->children, i);
        count += Node_destroy(child);
    }

    DynArray_free(node->children);
    Path_free(node->path);
    free(node);
    count++;

    return count;
}

/* Function to get the path of a node */
Path_T Node_getPath(Node_T node) {
    assert(node != NULL);
    return node->path;
}

/* Function to get the type of a node */
nodeType Node_getType(Node_T node) {
    assert(node != NULL);
    return node->type;
}

/* Function to get the contents of a node (file) */
void *Node_getContents(Node_T node) {
    assert(node != NULL);
    if (node->type == IS_FILE) {
        return node->contents;
    }
    return NULL;
}

/* Function to set the contents of a node (file) */
int Node_setContents(Node_T node, void *contents, size_t size) {
    assert(node != NULL);

    if (node->type != IS_FILE) {
        return NOT_A_FILE;
    }

    node->contents = contents;
    node->size = size;

    return SUCCESS;
}

/* Function to get the size of a node's contents */
size_t Node_getSize(Node_T node) {
    assert(node != NULL);

    if (node->type == IS_FILE) {
        return node->size;
    }
    return 0;
}

/* Function to get the number of children of a node */
int Node_getNumChildren(Node_T node, size_t *numChildren) {
    assert(node != NULL);
    assert(numChildren != NULL);

    if (node->type != IS_DIRECTORY) {
        return NOT_A_DIRECTORY;
    }

    *numChildren = DynArray_getLength(node->children);
    return SUCCESS;
}

/* Function to get a child node by index */
int Node_getChild(Node_T node, size_t index, Node_T *childNode) {
    assert(node != NULL);
    assert(childNode != NULL);

    if (node->type != IS_DIRECTORY) {
        return NOT_A_DIRECTORY;
    }

    if (index >= DynArray_getLength(node->children)) {
        *childNode = NULL;
        return NO_SUCH_PATH;
    }

    *childNode = DynArray_get(node->children, index);
    return SUCCESS;
}

/* Function to get the parent of a node */
Node_T Node_getParent(Node_T node) {
    assert(node != NULL);
    return node->parent;
}

/* Function to check if a node has a child with a given path */
boolean Node_hasChild(Node_T node, Path_T path, size_t *childIndex) {
    size_t numChildren;
    size_t i;

    assert(node != NULL);
    assert(path != NULL);
    assert(childIndex != NULL);

    numChildren = DynArray_getLength(node->children);

    for (i = 0; i < numChildren; i++) {
        Node_T child = DynArray_get(node->children, i);
        int cmp = Path_comparePath(Node_getPath(child), path);
        if (cmp == 0) {
            *childIndex = i;
            return TRUE;
        } else if (cmp > 0) {
            break;
        }
    }

    *childIndex = i;
    return FALSE;
}