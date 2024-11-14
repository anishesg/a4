
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "node.h"
#include "dynarray.h"

/* a node in a ft */
struct node {
   /* node's path */
   Path_T   oPPath;
   /* node parent */
   Node_T   oNParent;
   /* boolean of if node is dir, else is file */
   boolean  isDir;
   /* length of file contents if is file */
   size_t   ulLength;
   /* either children of dir, or contents of file */
   void    *contents;
};

/*
  Compares the string representation of oNfirst with a string
  pcSecond representing a node's path. pcSecond is a dir
  Returns <0, 0, or >0 if oNFirst is "less than", "equal to", or
  "greater than" pcSecond, respectively.
*/
static int Node_compareDirString(const Node_T oNFirst,
                                 const char *pcSecond) {
   assert(oNFirst != NULL);
   assert(pcSecond != NULL);

   if (oNFirst->isDir)
      return Path_compareString(oNFirst->oPPath, pcSecond);
   return -1; /* -1 means file before dir */
}

/*
  Compares the string representation of oNfirst with a string
  pcSecond representing a node's path. pcSecond is a file
  Returns <0, 0, or >0 if oNFirst is "less than", "equal to", or
  "greater than" pcSecond, respectively.
*/
static int Node_compareFileString(const Node_T oNFirst,
                                  const char *pcSecond) {
   assert(oNFirst != NULL);
   assert(pcSecond != NULL);

   if (!oNFirst->isDir)
      return Path_compareString(oNFirst->oPPath, pcSecond);
   return 1; /* 1 means file before dir */
}

/*
   Compares oNFirst and oNSecond's string representation as well as
   node type.
   Returns <0, 0, or >0 if oNFirst is "less than", "equal to", or
  "greater than" pcSecond, respectively.
*/
static int Node_compare(Node_T oNFirst, Node_T oNSecond) {
   assert(oNFirst != NULL);
   assert(oNSecond != NULL);

   if (oNFirst->isDir && !oNSecond->isDir) return 1;
   if (!oNFirst->isDir && oNSecond->isDir) return -1;

   return Path_comparePath(oNFirst->oPPath, oNSecond->oPPath);
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

   if(DynArray_addAt(oNParent->contents, ulIndex, oNChild)) {
      return SUCCESS;
   }
   else
      return MEMORY_ERROR;
}

int Node_new(Path_T oPPath, Node_T oNParent, boolean isDir,
      void *contents, size_t ulLength, Node_T *poNResult) {
   struct node *psNew;
   Path_T oPParentPath = NULL;
   Path_T oPNewPath = NULL;
   size_t ulParentDepth;
   size_t ulIndex;
   size_t ulIndexCopy;
   int iStatus;

   assert(oPPath != NULL);

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
      if(Node_hasChild(oNParent, oPPath, &ulIndex, isDir)) {
         Path_free(psNew->oPPath);
         free(psNew);
         *poNResult = NULL;
         return ALREADY_IN_TREE;
      }
      ulIndexCopy = ulIndex;
      if(Node_hasChild(oNParent, oPPath, &ulIndexCopy, !isDir)) {
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

   /* initialize the new node */
   /* if is dir */
   psNew->isDir = isDir;
   if (isDir) {
      psNew->contents = DynArray_new(0);
      if(psNew->contents == NULL) {
         Path_free(psNew->oPPath);
         free(psNew);
         *poNResult = NULL;
         return MEMORY_ERROR;
      }
   }

   /* if is file */
   else {
      if (contents != NULL) {
         psNew->contents = (void *)malloc(ulLength);
         if (psNew->contents == NULL) {
            Path_free(psNew->oPPath);
            free(psNew);
            *poNResult = NULL;
            return MEMORY_ERROR;
         }
         strcpy(psNew->contents, contents);
      } else {
         psNew->contents = contents;
      }
      psNew->ulLength = ulLength;
   }

   /* Link into parent's children list */
   if(oNParent != NULL) {
      iStatus = Node_addChild(oNParent, psNew, ulIndex);
      if(iStatus != SUCCESS) {
         Path_free(psNew->oPPath);
         free(psNew);
         *poNResult = NULL;
         return iStatus;
      }
   }

   *poNResult = psNew;

   return SUCCESS;
}

size_t Node_free(Node_T oNNode) {
   size_t ulIndex;
   size_t ulCount = 0;

   assert(oNNode != NULL);

   /* remove from parent's list */
   if(oNNode->oNParent != NULL) {
      if(DynArray_bsearch(
            oNNode->oNParent->contents,
            oNNode, &ulIndex,
            (int (*)(const void *, const void *)) Node_compare)
        )
         (void) DynArray_removeAt(oNNode->oNParent->contents,
                                  ulIndex);
   }

   /* recursively remove children if is dir */
   if (oNNode->isDir) {
      while(DynArray_getLength(oNNode->contents) != 0) {
         ulCount += Node_free(DynArray_get(oNNode->contents, 0));
      }
      DynArray_free(oNNode->contents);
   }
   /* if file */
   else {
      free(oNNode->contents);
   }

   /* remove path */
   Path_free(oNNode->oPPath);

   /* finally, free the struct node */
   free(oNNode);
   ulCount++;
   return ulCount;
}

Path_T Node_getPath(Node_T oNNode) {
   assert(oNNode != NULL);

   return oNNode->oPPath;
}

boolean Node_hasChild(Node_T oNParent, Path_T oPPath,
                         size_t *pulChildID, boolean isDir) {
   assert(oNParent != NULL);
   assert(oPPath != NULL);
   assert(pulChildID != NULL);

   /* *pulChildID is the index into oNParent->oDChildren */
   if (isDir) {
      return DynArray_bsearch(oNParent->contents,
         (char*) Path_getPathname(oPPath), pulChildID,
         (int (*)(const void*,const void*)) Node_compareDirString);
   }
   return DynArray_bsearch(oNParent->contents,
      (char*) Path_getPathname(oPPath), pulChildID,
      (int (*)(const void*,const void*)) Node_compareFileString);
}

size_t Node_getNumChildren(Node_T oNParent) {
   assert(oNParent != NULL);

   return DynArray_getLength(oNParent->contents);
}

int Node_getChild(Node_T oNParent, size_t ulChildID,
                  Node_T *poNResult) {
   assert(oNParent != NULL);
   assert(poNResult != NULL);

   /* ulChildID is the index into oNParent->oDChildren */
   if(ulChildID >= Node_getNumChildren(oNParent)) {
      *poNResult = NULL;
      return NO_SUCH_PATH;
   }
   else {
      *poNResult = DynArray_get(oNParent->contents, ulChildID);
      return SUCCESS;
   }
}

Node_T Node_getParent(Node_T oNNode) {
   assert(oNNode != NULL);

   return oNNode->oNParent;
}

char *Node_toString(Node_T oNNode) {
   char *copyPath;

   assert(oNNode != NULL);

   copyPath = malloc(Path_getStrLength(Node_getPath(oNNode))+1);
   if(copyPath == NULL)
      return NULL;
   else
      return strcpy(copyPath, Path_getPathname(Node_getPath(oNNode)));
}

boolean Node_isDirectory(Node_T oNNode)  {
    return oNNode->isDir;
}

void *Node_getContents(Node_T oNNode) {
   return oNNode->contents;
}

void *Node_replaceContents(Node_T oNNode, void *pvContents,
      size_t ulNewLength) {
   void *oldContents = (void *)malloc(oNNode->ulLength);
   strcpy(oldContents, oNNode->contents);

   if (oNNode->contents == NULL) {
      oNNode->contents = (void *)malloc(ulNewLength);
      if (oNNode->contents == NULL) {
         return NULL;
      }
      strcpy(oNNode->contents, pvContents);
   } else {
      free(oNNode->contents);
      oNNode->contents = (void *)malloc(ulNewLength);
      if (oNNode->contents == NULL) {
         return NULL;
      }
      strcpy(oNNode->contents, pvContents);
   }
   oNNode->ulLength = ulNewLength;
   return oldContents;
}

size_t Node_getContentLength(Node_T oNNode) {
   return oNNode->ulLength;
}