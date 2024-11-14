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

/* forward declarations */
static boolean CheckerDT_treeCheck(Node_T oNNode);

/* 
 * static variable to keep track of actual node count during traversal.
 * we use this to ensure that the reported node count aligns with 
 * the actual number of nodes in the tree.
 */
static size_t ulVerifiedNodeCount = 0;

/* 
 * checks if a child node’s parent pointer correctly references the
 * expected parent. this makes sure that each child node points back 
 * to the correct parent.
 */
static boolean validateChildParentReference(Node_T oParentNode, Node_T oChildNode, size_t childIndex) {
    /* retrieve the parent of the child node */
    Node_T oActualParent = Node_getParent(oChildNode);
    
    /* check if the actual parent matches the expected parent */
    if (oActualParent != oParentNode) {
        fprintf(stderr, "validation error: child node at index %lu has an incorrect parent reference.\n"
                        "expected parent: %p, found parent: %p.\n", 
                (unsigned long)childIndex, (void*)oParentNode, (void*)oActualParent);
        return FALSE;
    }
    
    return TRUE;
}

/* 
 * checks for duplicate child nodes by comparing the current child
 * with all previously processed child nodes. we’re ensuring there 
 * aren’t two nodes with the same name under the same parent.
 */
static boolean checkForDuplicateChildren(Node_T oParentNode, Node_T oChildNode, size_t currentIndex) {
    size_t compareIdx;
    Node_T oComparisonNode = NULL;
    int status;

    /* loop through all previous children to look for duplicates */
    for (compareIdx = 0; compareIdx < currentIndex; compareIdx++) {
        /* get the child node at compareIdx */
        status = Node_getChild(oParentNode, compareIdx, &oComparisonNode);
        if (status != SUCCESS) {
            fprintf(stderr, "validation error: failed to retrieve comparison child node at index %lu.\n"
                            "Node_getChild returned status: %d.\n", 
                    (unsigned long)compareIdx, status);
            return FALSE;
        }

        /* compare the current child with the comparison child */
        if (Node_compare(oChildNode, oComparisonNode) == 0) {
            fprintf(stderr, "validation error: duplicate child nodes found at indices %lu and %lu.\n"
                            "each child must be unique under the same parent.\n",
                    (unsigned long)currentIndex, (unsigned long)compareIdx);
            return FALSE;
        }
    }

    return TRUE;
}

/* 
 * verifies that child nodes are stored in lexicographical order.
 * makes sure each child node is in the correct position relative
 * to its neighboring nodes.
 */
static boolean verifyLexicographicalOrder(Node_T oParentNode, Node_T oChildNode, size_t currentIndex) {
    /* only check if there is at least one previous child */
    if (currentIndex > 0) {
        Node_T oPrevChildNode = NULL;
        int status;

        /* retrieve the previous child node */
        status = Node_getChild(oParentNode, currentIndex - 1, &oPrevChildNode);
        if (status != SUCCESS) {
            fprintf(stderr, "validation error: unable to retrieve previous child node at index %lu.\n"
                            "Node_getChild returned status: %d.\n",
                    (unsigned long)(currentIndex - 1), status);
            return FALSE;
        }

        /* make sure the current child is lexicographically after the previous */
        if (strcmp(Path_getPathname(Node_getPath(oPrevChildNode)),
                   Path_getPathname(Node_getPath(oChildNode))) > 0) {
            fprintf(stderr, "validation error: children are not in lexicographic order between indices %lu and %lu.\n"
                            "make sure children nodes are sorted lexicographically.\n",
                    (unsigned long)(currentIndex - 1), (unsigned long)currentIndex);
            return FALSE;
        }
    }

    return TRUE;
}

/* 
 * checks a single child node to make sure it meets all rules.
 * does parent reference check, duplicate detection, and order verification.
 */
static boolean validateChildNode(Node_T oParentNode, size_t childIndex) {
    Node_T oChildNode = NULL;
    int status;

    /* get the child node at the specified index */
    status = Node_getChild(oParentNode, childIndex, &oChildNode);
    if (status != SUCCESS) {
        fprintf(stderr, "validation error: Node_getChild failed at child index %lu.\n"
                        "Node_getChild returned status: %d.\n",
                (unsigned long)childIndex, status);
        return FALSE;
    }

    /* confirm the child’s parent is set correctly */
    if (!validateChildParentReference(oParentNode, oChildNode, childIndex)) {
        return FALSE;
    }

    /* check for duplicate nodes under the same parent */
    if (!checkForDuplicateChildren(oParentNode, oChildNode, childIndex)) {
        return FALSE;
    }

    /* make sure nodes are sorted lexicographically */
    if (!verifyLexicographicalOrder(oParentNode, oChildNode, childIndex)) {
        return FALSE;
    }

    /* now check the subtree rooted at this child */
    if (!CheckerDT_treeCheck(oChildNode)) {
        return FALSE;
    }

    return TRUE;
}

/* 
 * checks if a given node satisfies all required rules.
 * makes sure the node is not null and validates the parent-child 
 * path relationship.
 */
boolean CheckerDT_Node_isValid(Node_T oNNode) {
    Node_T oParentNode;
    Path_T oCurrentPath;
    Path_T oParentPath;

    /* confirm the node is non-null to proceed with checks */
    if (oNNode == NULL) {
        fprintf(stderr, "validation error: node is null, so it fails validation.\n");
        return FALSE;
    }

    /* retrieve the node’s parent and check their path relationship */
    oParentNode = Node_getParent(oNNode);
    if (oParentNode != NULL) {
        oCurrentPath = Node_getPath(oNNode);
        oParentPath = Node_getPath(oParentNode);

        /* confirm the parent path is the immediate prefix of node path */
        if (Path_getSharedPrefixDepth(oCurrentPath, oParentPath) !=
            Path_getDepth(oCurrentPath) - 1) {
            fprintf(stderr, "validation error: expected parent path (%s) to prefix node path (%s),"
                            " but this isn’t the case.\n",
                    Path_getPathname(oParentPath), Path_getPathname(oCurrentPath));
            return FALSE;
        }
    }

    /* if all checks pass, the node is valid */
    return TRUE;
}

/* 
 * traverses the tree recursively starting at oNNode.
 * returns FALSE immediately if it finds a violation.
 * increments ulVerifiedNodeCount for each valid node.
 */
static boolean CheckerDT_treeCheck(Node_T oNNode) {
    size_t childIndex;

    if (oNNode != NULL) {

        /* confirm the current node is valid */
        if (!CheckerDT_Node_isValid(oNNode)) {
            return FALSE;
        }

        /* count this valid node */
        ulVerifiedNodeCount++;

        /* go through each child node */
        for (childIndex = 0; childIndex < Node_getNumChildren(oNNode); childIndex++) {
            /* check the current child node and its subtree */
            if (!validateChildNode(oNNode, childIndex)) {
                return FALSE;
            }
        }
    }

    /* all checks passed for this branch */
    return TRUE;
}

/* 
 * checks the overall validity of the File Tree (DT).
 * makes sure top-level invariants are met, performs a full
 * recursive validation, and checks that reported node count 
 * matches the actual count.
 */
boolean CheckerDT_isValid(boolean bIsInitialized, Node_T oNRoot,
                          size_t ulCount) {

    /* reset node count tracker before traversal */
    ulVerifiedNodeCount = 0;

    /* top-level check: if uninitialized, node count should be zero */
    if (!bIsInitialized) {
        if (ulCount != 0) {
            fprintf(stderr, "validation error: tree is not initialized, but node count is %lu."
                            " expected node count to be zero when uninitialized.\n", (unsigned long)ulCount);
            return FALSE;
        }
    }

    /* start recursive traversal and validation */
    if (!CheckerDT_treeCheck(oNRoot)) {
        return FALSE;
    }

    /* confirm counted nodes match the given node count */
    if (ulVerifiedNodeCount != ulCount) {
        fprintf(stderr, "validation error: actual node count (%lu) doesn’t match expected count (%lu)."
                        " inconsistency detected in node count.\n",
                (unsigned long)ulVerifiedNodeCount, (unsigned long)ulCount);
        return FALSE;
    }

    /* everything passed, so tree is valid */
    return TRUE;
}