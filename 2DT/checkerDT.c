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

/* forward declaration */
static boolean CheckerDT_treeCheck(Node_T oNNode);

/* 
 * static variable to keep track of actual node count during traversal.
 * we use this to ensure that the reported node count aligns with 
 * the actual number of nodes in the tree.
 */
static size_t ulVerifiedNodeCount = 0;

/* 
 * this function checks if the child node `oChildNode` at index `childIndex` 
 * has its parent pointer correctly referencing the expected parent node `oParentNode`.
 * it ensures that the child node points back to its parent node.
 * the function returns TRUE if the parent reference is correct, and FALSE otherwise.
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
 * this function checks for duplicate child nodes under the parent node `oParentNode`.
 * it compares the current child node `oChildNode` at index `currentIndex` with all previously processed child nodes.
 * we ensure that there aren't two nodes with the same name under the same parent.
 * the function returns TRUE if no duplicates are found, and FALSE otherwise.
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
 * this function ensures that child nodes under the parent node `oParentNode` 
 * are stored in lexicographical order.
 * it checks the current child node `oChildNode` at index `currentIndex` against its previous sibling.
 * the function returns TRUE if the child nodes are in lexicographical order, and FALSE otherwise.
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
 * this function validates a single child node under the parent node `oParentNode` at index `childIndex`.
 * it performs parent reference check, duplicate detection, and lexicographical order verification.
 * the function returns TRUE if the child node passes all checks, and FALSE otherwise.
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
 * this function checks if the given node `oNNode` satisfies all required rules.
 * it ensures the node is not NULL and validates the parent-child path relationship.
 * the function returns TRUE if the node passes all validation checks, and FALSE otherwise.
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
 * this function recursively traverses the tree starting at the node `oNNode`.
 * it returns FALSE immediately if it finds a violation.
 * increments `ulVerifiedNodeCount` for each valid node.
 * the function returns TRUE if all nodes in the subtree rooted at `oNNode` pass validation, and FALSE otherwise.
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
 * and checks that reported node count `ulCount` matches the actual count.
 * it takes the initialization status `bIsInitialized`, the root node `oNRoot`, and the reported node count `ulCount`.
 * the function returns TRUE if the tree passes all checks, and FALSE otherwise.
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