/*--------------------------------------------------------------------*/
/* checkerDT.c                                                        */
/* Author: anish                               */
/*--------------------------------------------------------------------*/

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "checkerDT.h"
#include "dynarray.h"
#include "path.h"


/* checks if the given node is valid.
 * returns FALSE if the node is null or if the parent node's path 
 * does not correctly prefix the node's path.
 */
boolean CheckerDT_Node_isValid(Node_T oNode) {
   Node_T oParentNode;
   Path_T oNodePath;
   Path_T oParentPath;

   /* check for null node */
   if(oNode == NULL) {
      fprintf(stderr, "error: node is a null pointer\n");
      return FALSE;
   }

   /* verify parent path is correct prefix of node path */
   oParentNode = Node_getParent(oNode);
   if(oParentNode != NULL) {
      oNodePath = Node_getPath(oNode);
      oParentPath = Node_getPath(oParentNode);

      if(Path_getSharedPrefixDepth(oNodePath, oParentPath) !=
         Path_getDepth(oNodePath) - 1) {
         fprintf(stderr, "error: parent-child path mismatch: (%s) (%s)\n",
                 Path_getPathname(oParentPath), Path_getPathname(oNodePath));
         return FALSE;
      }
   }

   return TRUE;
}

/* verifies that ulCount matches the actual node count.
 * returns FALSE if the two values are inconsistent.
 */
static boolean CheckerDT_countMatches(size_t ulCount, size_t* actualCount) {

   assert(actualCount != NULL);

   if(ulCount != *actualCount) {
      fprintf(stderr, "error: ulCount does not match number of nodes.\n");
      fprintf(stderr, "ulCount: %lu, Actual Count: %lu\n",
              (unsigned long) ulCount, (unsigned long) *actualCount);
      *actualCount = 0;
      return FALSE;
   }
   *actualCount = 0;
   return TRUE;
}

/* checks that all child nodes are unique within a parent.
 * returns FALSE if duplicate child nodes are detected.
 */
static boolean CheckerDT_noDuplicateChildren(Node_T oNode) {
   size_t index, compareIndex;
   size_t childCount;
   Node_T oChild1 = NULL;
   Node_T oChild2 = NULL;
   Path_T path1, path2;

   childCount = Node_getNumChildren(oNode);

   for(index = 0; index < childCount; index++) {
      Node_getChild(oNode, index, &oChild1);

      for(compareIndex = index + 1; compareIndex < childCount; compareIndex++) {
         Node_getChild(oNode, compareIndex, &oChild2);

         path1 = Node_getPath(oChild1);
         path2 = Node_getPath(oChild2);

         if(Path_comparePath(path1, path2) == 0) {
            fprintf(stderr, "error: duplicate nodes detected.\n");
            fprintf(stderr, "Node 1: %s\n", Path_getPathname(path1));
            fprintf(stderr, "Node 2: %s\n", Path_getPathname(path2));
            return FALSE;
         }
      }
   }

   return TRUE;
}

/*
 * performs a pre-order traversal to check the tree's structure.
 * returns FALSE if any structural invariant is violated.
 * takes ulCount and nodeCount as inputs to ensure count consistency.
 */
static boolean CheckerDT_treeStructureValid(Node_T oNode, size_t ulCount, size_t* nodeCount) {
   size_t index, childCount;
   Path_T path1, path2;
   int comparison;

   assert(nodeCount != NULL);

   if(oNode != NULL) {
      (*nodeCount)++;

      /* check for unique child nodes */
      if(!CheckerDT_noDuplicateChildren(oNode)) {
         return FALSE;
      }

      /* validate the current node */
      if(!CheckerDT_Node_isValid(oNode)) {
         return FALSE;
      }

      childCount = Node_getNumChildren(oNode);

      /* recursively check each child node */
      for(index = 0; index < childCount; index++) {
         Node_T oChild = NULL;
         Node_T oNextChild = NULL;
         int status = Node_getChild(oNode, index, &oChild);

         if(status != SUCCESS) {
            fprintf(stderr, "error: child retrieval mismatch at index %lu\n", (unsigned long) index);
            return FALSE;
         }

         /* ensure children are in lexicographical order */
         if(index < childCount - 1) {
            Node_getChild(oNode, index + 1, &oNextChild);
            path1 = Node_getPath(oChild);
            path2 = Node_getPath(oNextChild);
            comparison = Path_comparePath(path1, path2);

            if(comparison >= 0) {
               fprintf(stderr, "error: child nodes not in lexicographical order.\n");
               fprintf(stderr, "Node 1: %s\n", Path_getPathname(path1));
               fprintf(stderr, "Node 2: %s\n", Path_getPathname(path2));
               return FALSE;
            }
         }

         /* recursive check on subtree rooted at child */
         if(!CheckerDT_treeStructureValid(oChild, ulCount, nodeCount)) {
            return FALSE;
         }
      }

      /* validate total count after reaching root */
      if(Node_getParent(oNode) == NULL) {
         if(!CheckerDT_countMatches(ulCount, nodeCount)) {
            return FALSE;
         }
      }
   }

   return TRUE;
}

/* see checkerDT.h for specification */
boolean CheckerDT_isValid(boolean bIsInitialized, Node_T oRoot,
                          size_t ulCount) {

   size_t nodeCount = 0;

   /* check top-level invariant: if uninitialized, count should be 0 */
   if(!bIsInitialized && ulCount != 0) {
      fprintf(stderr, "error: uninitialized tree with non-zero count.\n");
      return FALSE;
   }

   /* recursively validate the entire tree starting from the root */
   return CheckerDT_treeStructureValid(oRoot, ulCount, &nodeCount);
}