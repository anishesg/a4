/* Implementation of a node type used in ft.c to compose an ft*/
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "dynarray.h"
#include "nodeFT.h"
#include "checkerFT.h"
/* A node in a FT */
struct node {
   /* the object corresponding to the node's absolute path */
   Path_T oPPath;
   /* this node's parent */
   Node_T oNParent;
   /* the object containing links to this node's children */
   DynArray_T oDChildren;
   /* boolean to differentiate between file (True) vs 
   directory (False) */
   boolean ftType;
   /* pointer to file contents: (null) if directory */
   void* fileContents;
   /* size of contents*/
   size_t sizeContents;
};

/* see nodeFT.h for specification*/
boolean Node_getType(Node_T oNNode) {
   assert(oNNode!=NULL);
   return oNNode->ftType;
}
/* see nodeFT.h for specification*/
void *Node_getFileContents(Node_T oNNode) {
   assert(oNNode!=NULL);
   return oNNode->fileContents;
}
/* see nodeFT.h for specification*/
size_t Node_getSizeContents(Node_T oNNode) {
   assert(oNNode!=NULL);
   return oNNode->sizeContents;
}
/* see nodeFT.h for specification*/
int Node_setFileContents(Node_T oNNode, void *pvNewContents) {
   assert(oNNode!=NULL);
   oNNode->fileContents = pvNewContents;
   return SUCCESS;
}
/* see nodeFT.h for specification*/
int Node_setSizeContents(Node_T oNNode, size_t ulNewLength) {
   assert(oNNode!=NULL);
   oNNode->sizeContents = ulNewLength;
   return SUCCESS;
}
/*
  Links new child oNChild into oNParent's children array at index
  ulIndex. Returns SUCCESS if the new child was added successfully,
  or  MEMORY_ERROR if allocation fails adding oNChild to the array.
*/
static int Node_addChild(Node_T oNParent, Node_T oNChild,
                         size_t ulIndex) {
   assert(oNParent != NULL);
   assert(oNChild != NULL);
   if(DynArray_addAt(oNParent->oDChildren, ulIndex, oNChild))
      return SUCCESS;
   else
      return MEMORY_ERROR;
}
/*
  Compares the string representation of oNfirst with a string
  pcSecond representing a node's path.
  Returns <0, 0, or >0 if oNFirst is "less than", "equal to", or
  "greater than" pcSecond, respectively.
*/
static int Node_compareString(const Node_T oNFirst,
                                 const char *pcSecond) {
   assert(oNFirst != NULL);
   assert(pcSecond != NULL);
   return Path_compareString(oNFirst->oPPath, pcSecond);
}

/*
  Compares oNFirst and oNSecond lexicographically based on their paths.
  Returns <0, 0, or >0 if onFirst is "less than", "equal to", or
  "greater than" oNSecond, respectively.
*/
static int Node_compare(Node_T oNFirst, Node_T oNSecond) {
   assert(oNFirst != NULL);
   assert(oNSecond != NULL);
   return Path_comparePath(oNFirst->oPPath, oNSecond->oPPath);
}

/* see nodeFT.h for specification*/
int Node_newFile(Path_T oPPath, Node_T oNParent, Node_T *poNResult, 
                           void *pvNewContents, size_t ulNewLength) {
   struct node *psNew;
   Path_T oPParentPath = NULL;
   Path_T oPNewPath = NULL;
   size_t ulParentDepth;
   size_t ulIndex;
   int iStatus;
   assert(oPPath != NULL);
   assert(oNParent == NULL || CheckerFT_Node_isValid(oNParent));
   /* allocate space for a new node */
   psNew = malloc(sizeof(struct node));
   if(psNew == NULL) {
      *poNResult = NULL;
      return MEMORY_ERROR;
   }
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
      oPParentPath = oNParent->oPPath;
      ulParentDepth = Path_getDepth(oPParentPath);
      ulSharedDepth = Path_getSharedPrefixDepth(psNew->oPPath,
                                                oPParentPath);
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
      /* new node must be root */
      /* can only create one "level" at a time */
      if(Path_getDepth(psNew->oPPath) != 1) {
         Path_free(psNew->oPPath);
         free(psNew);
         *poNResult = NULL;
         return NO_SUCH_PATH;
      }
   }
   psNew->oNParent = oNParent;
   /* Link into parent's children list */
   if(oNParent != NULL && !Node_getType(oNParent)) {
      iStatus = Node_addChild(oNParent, psNew, ulIndex);
      if(iStatus != SUCCESS) {
         Path_free(psNew->oPPath);
         free(psNew);
         *poNResult = NULL;
         return iStatus;
      }
   }
   /*check is parent is file and return NOT_A_DIRECTORY*/
   else if (oNParent != NULL && Node_getType(oNParent)){
      Path_free(psNew->oPPath);
      free(psNew);
      *poNResult = NULL;
      return NOT_A_DIRECTORY;
   }

   /* points to file contents pvNewContents with size of 
   ulNewLength bytes*/ 
   psNew->fileContents = pvNewContents;
   psNew->sizeContents = ulNewLength;
   /*update ftType to true*/
   psNew->ftType = TRUE;
   *poNResult = psNew;
   assert(oNParent == NULL || CheckerFT_Node_isValid(oNParent));
   assert(CheckerFT_Node_isValid(*poNResult));
   return SUCCESS;
}
/* see nodeFT.h for specification*/
int Node_newDir(Path_T oPPath, Node_T oNParent, Node_T *poNResult) {
   struct node *psNew;
   Path_T oPParentPath = NULL;
   Path_T oPNewPath = NULL;
   size_t ulParentDepth;
   size_t ulIndex;
   int iStatus;
   assert(oPPath != NULL);
   assert(oNParent == NULL || CheckerFT_Node_isValid(oNParent));
   /* allocate space for a new node */
   psNew = malloc(sizeof(struct node));
   if(psNew == NULL) {
      *poNResult = NULL;
      return MEMORY_ERROR;
   }
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
      oPParentPath = oNParent->oPPath;
      ulParentDepth = Path_getDepth(oPParentPath);
      ulSharedDepth = Path_getSharedPrefixDepth(psNew->oPPath,
                                                oPParentPath);
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
      /* new node must be root */
      /* can only create one "level" at a time */
      if(Path_getDepth(psNew->oPPath) != 1) {
         Path_free(psNew->oPPath);
         free(psNew);
         *poNResult = NULL;
         return NO_SUCH_PATH;
      }
   }
   psNew->oNParent = oNParent;
   /* initialize the new node's dynarray */
   psNew->oDChildren = DynArray_new(0);
   if(psNew->oDChildren == NULL) {
      Path_free(psNew->oPPath);
      free(psNew);
      *poNResult = NULL;
      return MEMORY_ERROR;
   }
   /* Link into parent's children list */
   if(oNParent != NULL && !Node_getType(oNParent)) {
      iStatus = Node_addChild(oNParent, psNew, ulIndex);
      if(iStatus != SUCCESS) {
         Path_free(psNew->oPPath);
         free(psNew);
         *poNResult = NULL;
         return iStatus;
      }
   }
   /*check is parent is file and return NOT_A_DIRECTORY*/
   else if (oNParent != NULL && Node_getType(oNParent)){
      Path_free(psNew->oPPath);
      free(psNew);
      *poNResult = NULL;
      return NOT_A_DIRECTORY;
   }
   /* sets "file" contents to NULL and sizeContents to 0*/
   psNew->fileContents = NULL;
   psNew->sizeContents = 0;
   /*update ftType to false*/
   psNew->ftType = FALSE;
   *poNResult = psNew;
   assert(oNParent == NULL || CheckerFT_Node_isValid(oNParent));
   assert(CheckerFT_Node_isValid(*poNResult));
   return SUCCESS;
}

/* see nodeFT.h for specification*/
size_t Node_free(Node_T oNNode) {
   size_t ulIndex;
   size_t ulCount = 0;
   assert(oNNode != NULL);
   assert(CheckerFT_Node_isValid(oNNode));
   /* remove from parent's list */
   if(oNNode->oNParent != NULL) {
      if(DynArray_bsearch(
            oNNode->oNParent->oDChildren,
            oNNode, &ulIndex,
            (int (*)(const void *, const void *)) Node_compare)
        )
         (void) DynArray_removeAt(oNNode->oNParent->oDChildren,
                                  ulIndex);
   }
   /* recursively remove children if directory */
   if(!Node_getType(oNNode)) {
      while(DynArray_getLength(oNNode->oDChildren) != 0) {
         ulCount += Node_free(DynArray_get(oNNode->oDChildren, 0));
      }
      DynArray_free(oNNode->oDChildren);
   }

   /* remove path */
   Path_free(oNNode->oPPath);
   /* finally, free the struct node */
   free(oNNode);
   ulCount++;
   return ulCount;
}

/* see nodeFT.h for specification*/
Path_T Node_getPath(Node_T oNNode) {
   assert(oNNode != NULL);
   return oNNode->oPPath;
}

/* see nodeFT.h for specification*/
boolean Node_hasChild(Node_T oNParent, Path_T oPPath,
                         size_t *pulChildID) {
   assert(oNParent != NULL);
   assert(oPPath != NULL);
   assert(pulChildID != NULL);
   if (oNParent->ftType) return FALSE;
   /* *pulChildID is the index into oNParent->oDChildren */
   return DynArray_bsearch(oNParent->oDChildren,
            (char*) Path_getPathname(oPPath), pulChildID,
            (int (*)(const void*,const void*)) Node_compareString);
}

/* see nodeFT.h for specification*/
size_t Node_getNumChildren(Node_T oNParent) {
   assert(oNParent != NULL);
   if (oNParent->ftType) return 0;
   return DynArray_getLength(oNParent->oDChildren);
}

/* see nodeFT.h for specification*/
int Node_getChild(Node_T oNParent, size_t ulChildID,
                   Node_T *poNResult) {
   assert(oNParent != NULL);
   assert(poNResult != NULL);
   if (oNParent->ftType) return NOT_A_DIRECTORY;
   /* ulChildID is the index into oNParent->oDChildren */
   if(ulChildID >= Node_getNumChildren(oNParent)) {
      *poNResult = NULL;
      return NO_SUCH_PATH;
   }
   else {
      *poNResult = DynArray_get(oNParent->oDChildren, ulChildID);
      return SUCCESS;
   }
}

/* see nodeFT.h for specification*/
Node_T Node_getParent(Node_T oNNode) {
   assert(oNNode != NULL);
   return oNNode->oNParent;
}

/* see nodeFT.h for specification*/
char *Node_toString(Node_T oNNode) {
   char *copyPath;
   assert(oNNode != NULL);
   copyPath = malloc(Path_getStrLength(Node_getPath(oNNode))+1);
   if(copyPath == NULL)
      return NULL;
   else
      return strcpy(copyPath, Path_getPathname(Node_getPath(oNNode)));
}