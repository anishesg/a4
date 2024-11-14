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
 * we use this static variable to keep track of the actual node count
 * during traversal. it helps us ensure that the reported node count
 * matches the actual number of nodes in the tree.
 */
static size_t ulVerifiedNodeCount = 0;

/*
 * the function CheckerDT_validateChildParentReference checks if a child node's
 * parent pointer correctly references the expected parent node. this ensures that
 * each child node points back to the correct parent.
 *
 * parameters:
 *   - oParentNode: the expected parent node.
 *   - oChildNode: the child node being checked.
 *   - childIndex: the index of the child within its parent's list.
 *
 * returns:
 *   - TRUE if the parent reference is correct, or FALSE otherwise.
 */
static boolean CheckerDT_validateChildParentReference(Node_T oParentNode, Node_T oChildNode, size_t childIndex) {
    Node_T oActualParent = Node_getParent(oChildNode);

    if (oActualParent != oParentNode) {
        fprintf(stderr, "validation error: child node at index %lu has an incorrect parent reference.\n"
                        "expected parent: %p, found parent: %p.\n",
                (unsigned long)childIndex, (void*)oParentNode, (void*)oActualParent);
        return FALSE;
    }

    return TRUE;
}

/*
 * the function CheckerDT_checkForDuplicateChildren checks for duplicate child nodes by
 * comparing the current child with all previously processed child nodes under the same parent.
 * this ensures that there aren't two nodes with the same name under the same parent node.
 *
 * parameters:
 *   - oParentNode: the parent node containing the child nodes.
 *   - oChildNode: the child node being checked.
 *   - currentIndex: the index of the current child within the parent's list.
 *
 * returns:
 *   - TRUE if there are no duplicates found, or FALSE if a duplicate is detected.
 */
static boolean CheckerDT_checkForDuplicateChildren(Node_T oParentNode, Node_T oChildNode, size_t currentIndex) {
    size_t compareIdx;
    Node_T oComparisonNode = NULL;
    int status;

    for (compareIdx = 0; compareIdx < currentIndex; compareIdx++) {
        status = Node_getChild(oParentNode, compareIdx, &oComparisonNode);
        if (status != SUCCESS) {
            fprintf(stderr, "validation error: failed to retrieve comparison child node at index %lu.\n"
                            "Node_getChild returned status: %d.\n",
                    (unsigned long)compareIdx, status);
            return FALSE;
        }

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
 * the function CheckerDT_verifyLexicographicalOrder ensures that child nodes are stored
 * in lexicographical order within their parent's list. it checks if each child node is
 * in the correct position relative to its neighboring nodes.
 *
 * parameters:
 *   - oParentNode: the parent node containing the child nodes.
 *   - oChildNode: the child node being checked.
 *   - currentIndex: the index of the current child within the parent's list.
 *
 * returns:
 *   - TRUE if the child nodes are in the correct lexicographical order,
 *     or FALSE if an ordering violation is detected.
 */
static boolean CheckerDT_verifyLexicographicalOrder(Node_T oParentNode, Node_T oChildNode, size_t currentIndex) {
    if (currentIndex > 0) {
        Node_T oPrevChildNode = NULL;
        int status;

        status = Node_getChild(oParentNode, currentIndex - 1, &oPrevChildNode);
        if (status != SUCCESS) {
            fprintf(stderr, "validation error: unable to retrieve previous child node at index %lu.\n"
                            "Node_getChild returned status: %d.\n",
                    (unsigned long)(currentIndex - 1), status);
            return FALSE;
        }

        if (strcmp(Path_getPathname(Node_getPath(oPrevChildNode)),
                   Path_getPathname(Node_getPath(oChildNode))) > 0) {
            fprintf(stderr, "validation error: children are not in lexicographic order between indices %lu and %lu.\n"
                            "make sure child nodes are sorted lexicographically.\n",
                    (unsigned long)(currentIndex - 1), (unsigned long)currentIndex);
            return FALSE;
        }
    }

    return TRUE;
}

/*
 * the function CheckerDT_validateChildNode checks a single child node to ensure it
 * meets all the required rules. it performs parent reference validation, duplicate detection,
 * and lexicographical order verification.
 *
 * parameters:
 *   - oParentNode: the parent node of the child.
 *   - childIndex: the index of the child node within the parent's list.
 *
 * returns:
 *   - TRUE if the child node passes all checks, or FALSE if any validation fails.
 */
static boolean CheckerDT_validateChildNode(Node_T oParentNode, size_t childIndex) {
    Node_T oChildNode = NULL;
    int status;

    status = Node_getChild(oParentNode, childIndex, &oChildNode);
    if (status != SUCCESS) {
        fprintf(stderr, "validation error: Node_getChild failed at child index %lu.\n"
                        "Node_getChild returned status: %d.\n",
                (unsigned long)childIndex, status);
        return FALSE;
    }

    if (!CheckerDT_validateChildParentReference(oParentNode, oChildNode, childIndex)) {
        return FALSE;
    }

    if (!CheckerDT_checkForDuplicateChildren(oParentNode, oChildNode, childIndex)) {
        return FALSE;
    }

    if (!CheckerDT_verifyLexicographicalOrder(oParentNode, oChildNode, childIndex)) {
        return FALSE;
    }

    if (!CheckerDT_treeCheck(oChildNode)) {
        return FALSE;
    }

    return TRUE;
}

/*
 * the function CheckerDT_Node_isValid checks if a given node 'oNNode' satisfies all the
 * required rules. it ensures that 'oNNode' is not null and validates the parent-child
 * path relationship.
 *
 * parameters:
 *   - oNNode: the node being validated.
 *
 * returns:
 *   - TRUE if 'oNNode' passes all validation checks, or FALSE otherwise.
 */
boolean CheckerDT_Node_isValid(Node_T oNNode) {
    Node_T oParentNode;
    Path_T oCurrentPath;
    Path_T oParentPath;

    if (oNNode == NULL) {
        fprintf(stderr, "validation error: node is null, so it fails validation.\n");
        return FALSE;
    }

    oParentNode = Node_getParent(oNNode);
    if (oParentNode != NULL) {
        oCurrentPath = Node_getPath(oNNode);
        oParentPath = Node_getPath(oParentNode);

        if (Path_getSharedPrefixDepth(oCurrentPath, oParentPath) !=
            Path_getDepth(oCurrentPath) - 1) {
            fprintf(stderr, "validation error: expected parent path (%s) to prefix node path (%s),"
                            " but this isn't the case.\n",
                    Path_getPathname(oParentPath), Path_getPathname(oCurrentPath));
            return FALSE;
        }
    }

    return TRUE;
}

/*
 * the function CheckerDT_treeCheck recursively traverses the tree starting at 'oNNode'.
 * it checks each node and its children for validity, and counts the number of nodes visited.
 *
 * parameters:
 *   - oNNode: the starting node for the tree traversal.
 *
 * returns:
 *   - TRUE if all nodes in the subtree rooted at 'oNNode' pass validation,
 *     or FALSE if any validation fails.
 */
static boolean CheckerDT_treeCheck(Node_T oNNode) {
    size_t childIndex;

    if (oNNode != NULL) {
        if (!CheckerDT_Node_isValid(oNNode)) {
            return FALSE;
        }

        ulVerifiedNodeCount++;

        for (childIndex = 0; childIndex < Node_getNumChildren(oNNode); childIndex++) {
            if (!CheckerDT_validateChildNode(oNNode, childIndex)) {
                return FALSE;
            }
        }
    }

    return TRUE;
}

/*
 * the function CheckerDT_isValid checks the overall validity of the File Tree (DT).
 * it ensures that top-level invariants are met, performs a full recursive validation
 * of the tree structure, and checks that the reported node count matches the actual count.
 *
 * parameters:
 *   - bIsInitialized: boolean indicating if the tree is initialized.
 *   - oNRoot: the root node of the tree.
 *   - ulCount: the reported node count.
 *
 * returns:
 *   - TRUE if the tree passes all checks, or FALSE if any validation fails.
 */
boolean CheckerDT_isValid(boolean bIsInitialized, Node_T oNRoot, size_t ulCount) {

    ulVerifiedNodeCount = 0;

    if (!bIsInitialized) {
        if (ulCount != 0) {
            fprintf(stderr, "validation error: tree is not initialized, but node count is %lu."
                            " expected node count to be zero when uninitialized.\n", (unsigned long)ulCount);
            return FALSE;
        }
    }

    if (!CheckerDT_treeCheck(oNRoot)) {
        return FALSE;
    }

    if (ulVerifiedNodeCount != ulCount) {
        fprintf(stderr, "validation error: actual node count (%lu) doesn't match expected count (%lu)."
                        " inconsistency detected in node count.\n",
                (unsigned long)ulVerifiedNodeCount, (unsigned long)ulCount);
        return FALSE;
    }

    return TRUE;
}