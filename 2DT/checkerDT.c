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
 * this function checks if a child node's parent pointer correctly references the expected parent node.
 * it ensures that the child node (oChildNode) points back to its parent node (oParentNode).
 * returns TRUE if the parent reference is correct, FALSE otherwise.
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
 * this function checks for duplicate child nodes under a parent node.
 * it compares the current child node (oChildNode) with all previously processed child nodes.
 * we ensure that there aren't two nodes with the same name under the same parent (oParentNode).
 * returns TRUE if no duplicates are found, FALSE otherwise.
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
 * this function ensures that child nodes are stored in lexicographical order.
 * it compares the current child node (oChildNode) with its previous sibling under the same parent (oParentNode).
 * returns TRUE if the child nodes are in lexicographical order, FALSE otherwise.
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
                            "make sure children nodes are sorted lexicographically.\n",
                    (unsigned long)(currentIndex - 1), (unsigned long)currentIndex);
            return FALSE;
        }
    }

    return TRUE;
}

/* 
 * this function validates a single child node under a parent node.
 * it performs parent reference check, duplicate detection, and lexicographical order verification.
 * it takes the parent node (oParentNode) and the index of the child node (childIndex).
 * returns TRUE if the child node passes all checks, FALSE otherwise.
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
 * this function checks if a given node (oNNode) satisfies all required rules.
 * it ensures the node is not NULL and validates the parent-child path relationship.
 * returns TRUE if the node passes all validation checks, FALSE otherwise.
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
 * this function recursively traverses the tree starting at oNNode.
 * it returns FALSE immediately if it finds a violation.
 * increments ulVerifiedNodeCount for each valid node.
 * returns TRUE if all nodes in the subtree rooted at oNNode pass validation, FALSE otherwise.
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
 * this function checks the overall validity of the File Tree (DT).
 * it ensures top-level invariants are met, performs a full recursive validation,
 * and checks that reported node count (ulCount) matches the actual count.
 * it takes the initialization status (bIsInitialized), the root node (oNRoot), and the reported node count (ulCount).
 * returns TRUE if the tree passes all checks, FALSE otherwise.
 */
boolean CheckerDT_isValid(boolean bIsInitialized, Node_T oNRoot,
                          size_t ulCount) {

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