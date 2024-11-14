/*--------------------------------------------------------------------*/
/* nodeFT.c                                                          */
/* Author: anish                                                      */
/*--------------------------------------------------------------------*/

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "dynarray.h"
#include "nodeFT.h"
#include "a4def.h"

/* Definition of a node in the File Tree (FT) structure */
struct node {
    Path_T path;
    Node_T parent;
    DynArray_T children;
    nodeType type;
    void *contents;
    size_t contentSize;
};

/* ------------------------------------------------------------------ */

/*
  Adds a new child node oNChild to the parent node oNParent at the
  specified index. Ensures that the parent is a directory.
*/
static int Node_linkChild(Node_T oNParent, Node_T oNChild,
                          size_t index) {
    assert(oNParent != NULL);
    assert(oNChild != NULL);

    if(oNParent -> type != IS_DIRECTORY)
        return NOT_A_DIRECTORY;

    if(DynArray_addAt(oNParent->children, index, oNChild))
        return SUCCESS;
    else
        return MEMORY_ERROR;
}

/* ------------------------------------------------------------------ */

static int Node_comparePathToString(const Node_T oNFirst,
                                    const char *pcSecond) {
   assert(oNFirst != NULL);
   assert(pcSecond != NULL);

   return Path_compareString(oNFirst->path, pcSecond);
}

/* ------------------------------------------------------------------ */

int Node_new(Path_T oPPath, nodeType type, Node_T oNParent,
             Node_T *poNResult) {
    struct node *psNew;
    Path_T parentPath = NULL;
    Path_T newPath = NULL;
    int status;
    size_t sharedDepth;
    
    assert(oPPath != NULL);
    assert(poNResult != NULL);

    /* Allocate new node */
    psNew = calloc(1, sizeof(struct node));
    if(psNew == NULL) {
        *poNResult = NULL;
        return MEMORY_ERROR;
    }
    psNew->contents = NULL;
    psNew->type = type;

    /* Duplicate path */
    status = Path_dup(oPPath, &newPath);
    if(status != SUCCESS) {
        free(psNew);
        *poNResult = NULL;
        return status;
    }
    psNew->path = newPath;

    /* Parent path validation */
    if(oNParent != NULL) {
        size_t parentDepth = Path_getDepth(oNParent->path);

        parentPath = oNParent->path;
        sharedDepth = Path_getSharedPrefixDepth(psNew->path, parentPath);

        if (oNParent->type == IS_FILE){
            Path_free(psNew->path);
            free(psNew);
            *poNResult = NULL;
            return NOT_A_DIRECTORY;
        }

        if(sharedDepth < parentDepth) {
            Path_free(psNew->path);
            free(psNew);
            *poNResult = NULL;
            return CONFLICTING_PATH;
        }

        if(Path_getDepth(psNew->path) != parentDepth + 1) {
            Path_free(psNew->path);
            free(psNew);
            *poNResult = NULL;
            return NO_SUCH_PATH;
        }

        size_t childIndex = 0;
        if(Node_hasChild(oNParent, oPPath, &childIndex)) {
            Path_free(psNew->path);
            free(psNew);
            *poNResult = NULL;
            return ALREADY_IN_TREE;
        }
    }
    else {
        /* Root node must be directory and depth 1 */
        if(Path_getDepth(psNew->path) != 1) {
            Path_free(psNew->path);
            free(psNew);
            *poNResult = NULL;
            return NO_SUCH_PATH;
        }
    }
    psNew->parent = oNParent;

    /* Initialize children */
    psNew->children = DynArray_new(0);
    if(psNew->children == NULL) {
        Path_free(psNew->path);
        free(psNew);
        *poNResult = NULL;
        return MEMORY_ERROR;
    }

    if(oNParent != NULL) {
        status = Node_linkChild(oNParent, psNew, sharedDepth);
        if (status != SUCCESS) {
            Path_free(psNew->path);
            free(psNew);
            *poNResult = NULL;
            return status;
        }
    }

    *poNResult = psNew;

    return SUCCESS;
}

/* ------------------------------------------------------------------ */

size_t Node_free(Node_T oNNode) {
    size_t index = 0;
    size_t removedCount = 0;

    assert(oNNode != NULL);

    if(oNNode->parent != NULL) {
        if(DynArray_bsearch(
                oNNode->parent->children,
                oNNode, &index,
                (int (*)(const void *, const void *)) Node_compare)
            )
            (void) DynArray_removeAt(oNNode->parent->children, index);
    }

    /* Recursively remove child nodes */
    while(DynArray_getLength(oNNode->children) != 0) {
        removedCount += Node_free(DynArray_get(oNNode->children, 0));
    }
    DynArray_free(oNNode->children);

    /* Free path and struct node */
    Path_free(oNNode->path);
    free(oNNode);
    removedCount++;
    return removedCount;
}

/* ------------------------------------------------------------------ */

Path_T Node_getPath(Node_T oNNode) {
   assert(oNNode != NULL);

   return oNNode->path;
}

/* ------------------------------------------------------------------ */

boolean Node_hasChild(Node_T oNParent, Path_T oPPath,
                         size_t *pulChildID) {
    assert(oNParent != NULL);
    assert(oPPath != NULL);
    assert(pulChildID != NULL);

    if (oNParent->type == IS_FILE){
        return FALSE;
    }

    return DynArray_bsearch(oNParent->children,
            (char*) Path_getPathname(oPPath), pulChildID,
            (int (*)(const void*,const void*)) Node_comparePathToString);
}

/* ------------------------------------------------------------------ */

int Node_getNumChildren(Node_T oNParent, size_t *pulNum) {
    assert(oNParent != NULL);
    assert(pulNum != NULL);

    if (oNParent->type == IS_FILE){
        return NOT_A_DIRECTORY;
    }

    *pulNum = DynArray_getLength(oNParent->children);
    return SUCCESS;
}

/* ------------------------------------------------------------------ */

int Node_getChild(Node_T oNParent, size_t ulChildID,
                  Node_T *poNResult) {
    int status;
    size_t numChildren = 0;
    
    assert(oNParent != NULL);
    assert(poNResult != NULL);

    status = Node_getNumChildren(oNParent, &numChildren);
    if (status != SUCCESS) {
        return status;
    }

    if(ulChildID >= numChildren) {
        *poNResult = NULL;
        return NO_SUCH_PATH;
    }
    else {
        *poNResult = DynArray_get(oNParent->children, ulChildID);
        return SUCCESS;
    }
}

/* ------------------------------------------------------------------ */

Node_T Node_getParent(Node_T oNNode) {
   assert(oNNode != NULL);

   return oNNode->parent;
}

/* ------------------------------------------------------------------ */

char *Node_toString(Node_T oNNode) {
   char *pathString;

   assert(oNNode != NULL);

   pathString = malloc(Path_getStrLength(Node_getPath(oNNode))+1);
   if(pathString == NULL)
      return NULL;
   else
      return strcpy(pathString, Path_getPathname(Node_getPath(oNNode)));
}

/* ------------------------------------------------------------------ */

int Node_insertFileContents(Node_T oNNode, void *pvContents, size_t 
ulLength){
    assert(oNNode !=NULL);

    if (oNNode -> type == IS_DIRECTORY)
        return BAD_PATH;
    
    oNNode->contents = pvContents;
    oNNode->contentSize = ulLength;

    return SUCCESS;
}

/* ------------------------------------------------------------------ */

void *Node_getContents(Node_T oNNode){
    assert(oNNode != NULL);
    return oNNode->contents;
}

/* ------------------------------------------------------------------ */

nodeType Node_getType(Node_T oNNode){
    assert(oNNode != NULL);
    return oNNode->type;
}

/* ------------------------------------------------------------------ */

size_t Node_getSize(Node_T oNNode) {
    assert(oNNode != NULL);
    return oNNode->contentSize;
}

/*--------------------------------------------------------------------*/