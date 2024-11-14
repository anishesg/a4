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

/* static variable to keep track of the actual node count during traversal */
static size_t ulCheck = 0;

/* see checkerDT.h for specification */
boolean CheckerDT_Node_isValid(Node_T oNNode) {
    Node_T oParentNode;
    Path_T oCurrentPath;
    Path_T oParentPath;

    /* ensure the node is not null */
    if (oNNode == NULL) {
        fprintf(stderr, "error: node is a null pointer\n");
        return FALSE;
    }

    /* obtain the parent of the current node */
    oParentNode = Node_getParent(oNNode);
    if (oParentNode != NULL) {
        oCurrentPath = Node_getPath(oNNode);
        oParentPath = Node_getPath(oParentNode);

        /* verify that the parent's path is the immediate prefix of the node's path */
        if (Path_getSharedPrefixDepth(oCurrentPath, oParentPath) !=
            Path_getDepth(oCurrentPath) - 1) {
            fprintf(stderr, "error: parent path (%s) does not correctly prefix node path (%s)\n",
                    Path_getPathname(oParentPath), Path_getPathname(oCurrentPath));
            return FALSE;
        }
    }

    /* node passed all validation checks */
    return TRUE;
}

/*
   performs a pre-order traversal starting from oNNode.
   returns FALSE if any inconsistency is detected, TRUE otherwise.
   increments ulCheck for each valid node encountered.
*/
static boolean CheckerDT_treeCheck(Node_T oNNode) {
    size_t index;
    size_t compareIdx;
    Node_T oChild = NULL;
    Node_T oCompareChild = NULL;
    Node_T oPrevChild = NULL;
    int status;

    if (oNNode != NULL) {

        /* validate the current node */
        if (!CheckerDT_Node_isValid(oNNode)) {
            return FALSE;
        }

        /* increment the node count */
        ulCheck++;

        /* iterate through each child of the current node */
        for (index = 0; index < Node_getNumChildren(oNNode); index++) {
            /* retrieve the child node at the current index */
            status = Node_getChild(oNNode, index, &oChild);
            if (status != SUCCESS) {
                fprintf(stderr, "error: getChild failed for child index %lu\n", index);
                return FALSE;
            }

            /* ensure the child's parent is correctly set */
            if (Node_getParent(oChild) != oNNode) {
                fprintf(stderr, "error: child's parent reference mismatch at index %lu\n", index);
                return FALSE;
            }

            /* check for duplicate children */
            for (compareIdx = 0; compareIdx < index; compareIdx++) {
                /* retrieve the child node at compareIdx */
                status = Node_getChild(oNNode, compareIdx, &oCompareChild);
                if (status != SUCCESS) {
                    fprintf(stderr, "error: getChild failed for compare index %lu\n", compareIdx);
                    return FALSE;
                }

                if (Node_compare(oChild, oCompareChild) == 0) {
                    fprintf(stderr, "error: duplicate child node found at index %lu\n", index);
                    return FALSE;
                }
            }

            /* verify lexicographical order of children */
            if (index > 0) {
                /* retrieve the previous child node */
                status = Node_getChild(oNNode, index - 1, &oPrevChild);
                if (status != SUCCESS) {
                    fprintf(stderr, "error: getChild failed for previous index %lu\n", index - 1);
                    return FALSE;
                }

                if (strcmp(Path_getPathname(Node_getPath(oPrevChild)),
                           Path_getPathname(Node_getPath(oChild))) > 0) {
                    fprintf(stderr, "error: children are not in lexicographic order at index %lu\n", index);
                    return FALSE;
                }
            }

            /* recursively validate the subtree rooted at the child node */
            if (!CheckerDT_treeCheck(oChild)) {
                return FALSE;
            }
        }
    }

    /* all checks passed for this branch */
    return TRUE;
}

/* see checkerDT.h for specification */
boolean CheckerDT_isValid(boolean bIsInitialized, Node_T oNRoot,
                          size_t ulCount) {

    /* reset the node count before starting traversal */
    ulCheck = 0;

    /* verify top-level invariants */
    if (!bIsInitialized) {
        if (ulCount != 0) {
            fprintf(stderr, "error: tree not initialized but node count is %lu\n", ulCount);
            return FALSE;
        }
    }

    /* perform recursive validation and count nodes */
    if (!CheckerDT_treeCheck(oNRoot)) {
        return FALSE;
    }

    /* verify that the counted nodes match the reported count */
    if (ulCheck != ulCount) {
        fprintf(stderr, "error: mismatch in node count (counted: %lu, reported: %lu)\n", ulCheck, ulCount);
        return FALSE;
    }

    /* all validations passed */
    return TRUE;
}