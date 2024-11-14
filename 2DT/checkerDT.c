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

/* static variable to maintain the actual node count during traversal */
static size_t ulVerifiedNodeCount = 0;

/* see checkerDT.h for specification */
boolean CheckerDT_Node_isValid(Node_T oNNode) {
    Node_T oParentNode;
    Path_T oCurrentPath;
    Path_T oParentPath;

    /* confirm the node is non-null to proceed */
    if (oNNode == NULL) {
        fprintf(stderr, "validation error: node is a null pointer. Node validation failed.\n");
        return FALSE;
    }

    /* retrieve the node's parent to validate path relationship */
    oParentNode = Node_getParent(oNNode);
    if (oParentNode != NULL) {
        oCurrentPath = Node_getPath(oNNode);
        oParentPath = Node_getPath(oParentNode);

        /* check that the parent's path correctly prefixes the node's path */
        if (Path_getSharedPrefixDepth(oCurrentPath, oParentPath) !=
            Path_getDepth(oCurrentPath) - 1) {
            fprintf(stderr, "validation error: expected parent path (%s) to be the prefix of node path (%s),"
                            " but it does not match. Parent-child path relationship invalid.\n",
                    Path_getPathname(oParentPath), Path_getPathname(oCurrentPath));
            return FALSE;
        }
    }

    /* node has passed all necessary validation criteria */
    return TRUE;
}

/*
   performs a recursive pre-order traversal starting from oNNode.
   returns FALSE immediately if any inconsistency or invariant violation is found.
   otherwise, increments ulVerifiedNodeCount for each valid node and returns TRUE.
*/
static boolean CheckerDT_treeCheck(Node_T oNNode) {
    size_t i, j;
    Node_T oChildNode = NULL;
    Node_T oPrevChildNode = NULL;
    Node_T oComparisonNode = NULL;
    int status;

    if (oNNode != NULL) {

        /* confirm the current node meets validation requirements */
        if (!CheckerDT_Node_isValid(oNNode)) {
            return FALSE;
        }

        /* increment our count of verified nodes */
        ulVerifiedNodeCount++;

        /* iterate through each child node of the current node */
        for (i = 0; i < Node_getNumChildren(oNNode); i++) {
            /* retrieve the child node at the specified index */
            status = Node_getChild(oNNode, i, &oChildNode);
            if (status != SUCCESS) {
                fprintf(stderr, "validation error: Node_getChild failed at child index %lu."
                                " Unable to retrieve child node.\n", (unsigned long)i);
                return FALSE;
            }

            /* confirm the child's parent node is correctly set to the current node */
            if (Node_getParent(oChildNode) != oNNode) {
                fprintf(stderr, "validation error: child node at index %lu has an incorrect parent reference."
                                " Expected parent does not match.\n", (unsigned long)i);
                return FALSE;
            }

            /* check for duplicates among the child nodes of this parent */
            for (j = 0; j < i; j++) {
                status = Node_getChild(oNNode, j, &oComparisonNode);
                if (status != SUCCESS) {
                    fprintf(stderr, "validation error: failed to retrieve comparison child node at index %lu."
                                    " Node retrieval unsuccessful.\n", (unsigned long)j);
                    return FALSE;
                }

                if (Node_compare(oChildNode, oComparisonNode) == 0) {
                    fprintf(stderr, "validation error: duplicate child nodes detected at indices %lu and %lu."
                                    " Each child node must be unique within the same parent.\n",
                            (unsigned long)i, (unsigned long)j);
                    return FALSE;
                }
            }

            /* ensure the children of the node are stored in lexicographical order */
            if (i > 0) {
                status = Node_getChild(oNNode, i - 1, &oPrevChildNode);
                if (status != SUCCESS) {
                    fprintf(stderr, "validation error: unable to retrieve previous child node at index %lu."
                                    " Node retrieval unsuccessful.\n", (unsigned long)(i - 1));
                    return FALSE;
                }

                if (strcmp(Path_getPathname(Node_getPath(oPrevChildNode)),
                           Path_getPathname(Node_getPath(oChildNode))) > 0) {
                    fprintf(stderr, "validation error: children are not in lexicographic order between indices %lu and %lu."
                                    " Ensure children nodes are sorted lexicographically.\n",
                            (unsigned long)(i - 1), (unsigned long)i);
                    return FALSE;
                }
            }

            /* recursively validate the subtree rooted at this child node */
            if (!CheckerDT_treeCheck(oChildNode)) {
                return FALSE;
            }
        }
    }

    /* all checks passed for this branch of the tree */
    return TRUE;
}

/* see checkerDT.h for specification */
boolean CheckerDT_isValid(boolean bIsInitialized, Node_T oNRoot,
                          size_t ulCount) {

    /* reset node count tracker before initiating traversal */
    ulVerifiedNodeCount = 0;

    /* top-level invariant check: tree should have zero nodes if uninitialized */
    if (!bIsInitialized) {
        if (ulCount != 0) {
            fprintf(stderr, "validation error: tree is not initialized, yet node count is %lu."
                            " Expected node count to be zero.\n", (unsigned long)ulCount);
            return FALSE;
        }
    }

    /* perform recursive traversal validation and node count accumulation */
    if (!CheckerDT_treeCheck(oNRoot)) {
        return FALSE;
    }

    /* ensure the counted nodes align with the provided node count */
    if (ulVerifiedNodeCount != ulCount) {
        fprintf(stderr, "validation error: mismatch between actual node count (%lu) and expected node count (%lu)."
                        " Node count inconsistency detected.\n",
                (unsigned long)ulVerifiedNodeCount, (unsigned long)ulCount);
        return FALSE;
    }

    /* all validations completed successfully */
    return TRUE;
}