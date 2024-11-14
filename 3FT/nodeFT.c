/*--------------------------------------------------------------------*/
/* nodeFT.c                                                           */
/* Author: anish                                                      */
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
    Path_T oPPath;
    Node_T oNParent;
    DynArray_T oDChildren;
    nodeType type;
    void *pvContents;
    size_t ulSize;
};

/* Helper function to compare nodes based on their paths */
static int Node_compare(const void *first, const void *second) {
    Node_T node1 = *(Node_T *)first;
    Node_T node2 = *(Node_T *)second;

    return Path_comparePath(node1->oPPath, node2->oPPath);
}

/* Helper function to compare node paths with a string */
static int Node_compareString(const Node_T oNFirst, const char *pcSecond) {
    assert(oNFirst != NULL);
    assert(pcSecond != NULL);

    return Path_compareString(oNFirst->oPPath, pcSecond);
}

/* Function to create a new node */
int Node_new(Path_T oPPath, nodeType type, Node_T oNParent, Node_T *poNResult) {
    struct node *psNew;
    Path_T oPParentPath = NULL;
    Path_T oPNewPath = NULL;
    int iStatus;
    size_t ulIndex = 0;

    assert(oPPath != NULL);
    assert(poNResult != NULL);

    /* allocate space for a new node */
    psNew = calloc(1, sizeof(struct node));
    if(psNew == NULL) {
        *poNResult = NULL;
        return MEMORY_ERROR;
    }
    psNew->pvContents = NULL;
    psNew->type = type; /* set the node's type */

    /* set the new node's path */
    iStatus = Path_dup(oPPath, &oPNewPath);
    if(iStatus != SUCCESS) {
        free(psNew);
        *poNResult = NULL;
        return iStatus;
    }
    psNew->oPPath = oPNewPath;

    /* validate and set the new node's parent */
    if(oNParent != NULL) {
        size_t ulSharedDepth;
        size_t ulParentDepth;

        oPParentPath = oNParent->oPPath;
        ulParentDepth = Path_getDepth(oPParentPath);
        ulSharedDepth = Path_getSharedPrefixDepth(psNew->oPPath, oPParentPath);

        /* parent must be a directory */
        if (oNParent->type == IS_FILE){
            Path_free(psNew->oPPath);
            free(psNew);
            *poNResult = NULL;
            return NOT_A_DIRECTORY;
        }

        /* parent must be an ancestor of child */
        if(ulSharedDepth < ulParentDepth) {
            Path_free(psNew->oPPath);
            free(psNew);
            *poNResult = NULL;
            return CONFLICTING_PATH;
        }

        /* parent must be exactly one level up from child */
        if(Path_getDepth(psNew->oPPath) != ulParentDepth + 1) {
            Path_free(psNew->oPPath);
            free(psNew);
            *poNResult = NULL;
            return NO_SUCH_PATH;
        }

        /* parent must not already have child with this path */
        if(Node_hasChild(oNParent, oPPath, &ulIndex)) {
            Path_free(psNew->oPPath);
            free(psNew);
            *poNResult = NULL;
            return ALREADY_IN_TREE;
        }
    }
    else {
        /* new node must be root and therefore must be directory*/
        /* can only create one "level" at a time */

        if(Path_getDepth(psNew->oPPath) != 1) {
            Path_free(psNew->oPPath);
            free(psNew);
            *poNResult = NULL;
            return NO_SUCH_PATH;
        }
    }
    psNew->oNParent = oNParent;

    /* initialize the new node */
    psNew->oDChildren = DynArray_new(0);
    if(psNew->oDChildren == NULL) {
        Path_free(psNew->oPPath);
        free(psNew);
        *poNResult = NULL;
        return MEMORY_ERROR;
    }

    /* Link into parent's children list */
    if(oNParent != NULL) {
        /* insert into parent's children array in sorted order */
        if(DynArray_addAt(oNParent->oDChildren, ulIndex, psNew) == FALSE) {
            Path_free(psNew->oPPath);
            DynArray_free(psNew->oDChildren);
            free(psNew);
            *poNResult = NULL;
            return MEMORY_ERROR;
        }
    }

    *poNResult = psNew;

    return SUCCESS;
}

/* Function to destroy a node and its descendants */
size_t Node_free(Node_T oNNode) {
    size_t ulIndex = 0;
    size_t ulCount = 0;

    assert(oNNode != NULL);

    /* remove from parent's list */
    if(oNNode->oNParent != NULL) {
        if(DynArray_bsearch(
                oNNode->oNParent->oDChildren,
                oNNode, &ulIndex,
                (int (*)(const void *, const void *)) Node_compare)
            )
            (void) DynArray_removeAt(oNNode->oNParent->oDChildren, ulIndex);
    }

    /* recursively remove children */
    while(DynArray_getLength(oNNode->oDChildren) != 0) {
        ulCount += Node_free(DynArray_get(oNNode->oDChildren, 0));
    }
    DynArray_free(oNNode->oDChildren);

    /* remove path */
    Path_free(oNNode->oPPath);

    /* finally, free the struct node */
    free(oNNode);
    ulCount++;
    return ulCount;
}

/* Function to get the path of a node */
Path_T Node_getPath(Node_T oNNode) {
    assert(oNNode != NULL);
    return oNNode->oPPath;
}

/* Function to get the type of a node */
nodeType Node_getType(Node_T oNNode) {
    assert(oNNode != NULL);
    return oNNode->type;
}

/* Function to set the contents of a node (file) */
int Node_insertFileContents(Node_T oNNode, void *pvContents, size_t ulLength) {
    assert(oNNode != NULL);

    if (oNNode->type != IS_FILE) {
        return BAD_PATH;
    }

    oNNode->pvContents = pvContents;
    oNNode->ulSize = ulLength;

    return SUCCESS;
}

/* Function to get the contents of a node (file) */
void *Node_getContents(Node_T oNNode) {
    assert(oNNode != NULL);
    if (oNNode->type == IS_FILE) {
        return oNNode->pvContents;
    }
    return NULL;
}

/* Function to get the size of a node's contents */
size_t Node_getSize(Node_T oNNode) {
    assert(oNNode != NULL);
    if (oNNode->type == IS_FILE) {
        return oNNode->ulSize;
    }
    return 0;
}

/* Function to get the number of children of a node */
int Node_getNumChildren(Node_T oNParent, size_t *pulNum) {
    assert(oNParent != NULL);
    assert(pulNum != NULL);

    /* verify oNParent is a directory */
    if (oNParent->type != IS_DIRECTORY){
        return NOT_A_DIRECTORY;
    }

    *pulNum = DynArray_getLength(oNParent->oDChildren);
    return SUCCESS;
}

/* Function to get a child node by index */
int Node_getChild(Node_T oNParent, size_t ulChildID, Node_T *poNResult) {
    size_t ulNumChildren = 0;
    int iStatus;

    assert(oNParent != NULL);
    assert(poNResult != NULL);

    iStatus = Node_getNumChildren(oNParent, &ulNumChildren);
    if (iStatus != SUCCESS) {
        return iStatus;
    }

    if (ulChildID >= ulNumChildren) {
        *poNResult = NULL;
        return NO_SUCH_PATH;
    }

    *poNResult = DynArray_get(oNParent->oDChildren, ulChildID);
    return SUCCESS;
}

/* Function to check if a node has a child with a given path */
boolean Node_hasChild(Node_T oNParent, Path_T oPPath, size_t *pulChildID) {
    assert(oNParent != NULL);
    assert(oPPath != NULL);
    assert(pulChildID != NULL);

    /* invariant */
    if (oNParent->type != IS_DIRECTORY){
        return FALSE;
    }

    /* *pulChildID is the index into oNParent->oDChildren */
    return DynArray_bsearch(oNParent->oDChildren,
            (char*) Path_getPathname(oPPath), pulChildID,
            (int (*)(const void*,const void*)) Node_compareString);
}

/* Function to get the parent of a node */
Node_T Node_getParent(Node_T oNNode) {
    assert(oNNode != NULL);
    return oNNode->oNParent;
}