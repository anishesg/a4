/*--------------------------------------------------------------------*/
/* checkerDT.c                                                        */
/* Author: anish                                                      */
/*--------------------------------------------------------------------*/
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "checkerDT.h"
#include "dynarray.h"
#include "path.h"

/* see checkerDT.h for specification */
boolean CheckerDT_Node_isValid(Node_T oNNode) {
   /* checks if node is valid by ensuring it’s not NULL and 
      checking the consistency of the node's path with its parent's path */

   if (!oNNode) {
      fprintf(stderr, "Node is NULL\n");
      return FALSE;
   }

   Node_T oParentNode = Node_getParent(oNNode);
   if (oParentNode) {
      Path_T oNodePath = Node_getPath(oNNode);
      Path_T oParentPath = Node_getPath(oParentNode);

      /* parent path should be the exact prefix of node path */
      if (Path_getSharedPrefixDepth(oNodePath, oParentPath) != Path_getDepth(oNodePath) - 1) {
         fprintf(stderr, "Parent-child path mismatch: parent path (%s) does not prefix node path (%s)\n",
                 Path_getPathname(oParentPath), Path_getPathname(oNodePath));
         return FALSE;
      }
   }
   return TRUE;
}

/* verifies that the count of nodes is consistent with ulCount */
static boolean CheckerDT_verifyCount(size_t ulCount, size_t actualCount) {
   if (ulCount != actualCount) {
      fprintf(stderr, "Node count mismatch: expected %lu, found %lu\n", (unsigned long)ulCount, (unsigned long)actualCount);
      return FALSE;
   }
   return TRUE;
}

/* checks for duplicates among children and verifies lexicographic order */
static boolean CheckerDT_checkChildren(Node_T oNNode) {
   size_t numChildren = Node_getNumChildren(oNNode);

   for (size_t i = 0; i < numChildren; i++) {
      Node_T oNChild;
      if (Node_getChild(oNNode, i, &oNChild) != SUCCESS) {
         fprintf(stderr, "Child retrieval failed for index %lu\n", (unsigned long)i);
         return FALSE;
      }

      /* check for duplicates and order */
      for (size_t j = i + 1; j < numChildren; j++) {
         Node_T oNNextChild;
         if (Node_getChild(oNNode, j, &oNNextChild) != SUCCESS) {
            fprintf(stderr, "Child retrieval failed for index %lu\n", (unsigned long)j);
            return FALSE;
         }

         /* checking for duplicates by comparing paths */
         if (Path_comparePath(Node_getPath(oNChild), Node_getPath(oNNextChild)) == 0) {
            fprintf(stderr, "Duplicate child nodes found at %s\n", Path_getPathname(Node_getPath(oNChild)));
            return FALSE;
         }

         /* lexicographic order check */
         if (Path_comparePath(Node_getPath(oNChild), Node_getPath(oNNextChild)) > 0) {
            fprintf(stderr, "Children out of order: %s should precede %s\n",
                    Path_getPathname(Node_getPath(oNChild)),
                    Path_getPathname(Node_getPath(oNNextChild)));
            return FALSE;
         }
      }
   }
   return TRUE;
}

/* recursively traverses and validates the tree, checking each node */
static boolean CheckerDT_traverseAndValidate(Node_T oNNode, size_t *count) {
   /* returns FALSE if any issue is encountered with node structure */

   if (!oNNode) return TRUE; /* NULL node is trivially valid */

   (*count)++; /* increment count for each valid node encountered */

   if (!CheckerDT_Node_isValid(oNNode)) return FALSE;

   /* validate children and order */
   if (!CheckerDT_checkChildren(oNNode)) return FALSE;

   size_t numChildren = Node_getNumChildren(oNNode);

   /* recursively validate each child node */
   for (size_t i = 0; i < numChildren; i++) {
      Node_T oNChild;
      if (Node_getChild(oNNode, i, &oNChild) != SUCCESS) {
         fprintf(stderr, "Failed to retrieve child node at index %lu\n", (unsigned long)i);
         return FALSE;
      }
      if (!CheckerDT_traverseAndValidate(oNChild, count)) return FALSE;
   }

   return TRUE;
}

/* see checkerDT.h for specification */
boolean CheckerDT_isValid(boolean bIsInitialized, Node_T oNRoot, size_t ulCount) {
   /* if DT is not initialized, ensure ulCount is zero */
   if (!bIsInitialized) {
      if (ulCount != 0) {
         fprintf(stderr, "Tree not initialized, but node count is %lu\n", (unsigned long)ulCount);
         return FALSE;
      }
      return TRUE; /* uninitialized DT with count 0 is valid */
   }

   /* validate the structure of initialized DT */
   size_t actualCount = 0;

   if (!CheckerDT_traverseAndValidate(oNRoot, &actualCount)) return FALSE;

   /* confirm count consistency */
   if (!CheckerDT_verifyCount(ulCount, actualCount)) return FALSE;

   return TRUE;
}