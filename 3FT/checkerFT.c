/*--------------------------------------------------------------------*/
/* checkerFT.c                                                        */
/* Author: [Your Name]                                                */
/*--------------------------------------------------------------------*/

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "checkerFT.h"
#include "dynarray.h"
#include "path.h"
#include "nodeFT.h"
#include "ft.h"

/*
    Forward declaration of the helper function to perform recursive checks.
    It traverses the tree rooted at `node` and validates each node.
*/
static boolean CheckerFT_validateTree(Node_T node, DynArray_T seenNodes);

/* See checkerFT.h for specification */
boolean CheckerFT_Node_isValid(Node_T node) {
    Node_T parentNode;
    Path_T nodePath;
    Path_T parentPath;

    /* Check: node should not be NULL */
    if (node == NULL) {
        fprintf(stderr, "Error: Node is NULL\n");
        return FALSE;
    }

    /* Check: If node has a parent, validate the parent-child relationship */
    parentNode = NodeFT_getParent(node);
    if (parentNode != NULL) {
        nodePath = NodeFT_getPath(node);
        parentPath = NodeFT_getPath(parentNode);

        /* Parent's path should be a proper prefix of node's path */
        if (Path_getSharedPrefixDepth(nodePath, parentPath) != Path_getDepth(parentPath)) {
            fprintf(stderr, "Error: Parent path is not a prefix of node path: (%s) (%s)\n",
                    Path_getPathname(parentPath), Path_getPathname(nodePath));
            return FALSE;
        }
    }

    return TRUE;
}

/*
    Helper function to check if the children of a node are in the correct order.
    Ensures that both file and directory children are sorted lexicographically.
    Returns TRUE if the order is correct, FALSE otherwise.
*/
static boolean CheckerFT_childrenAreOrdered(Node_T parentNode) {
    size_t numDirChildren, numFileChildren;
    size_t i;

    assert(parentNode != NULL);

    /* Validate directory children */
    numDirChildren = NodeFT_getNumChildren(parentNode, FALSE);
    for (i = 0; i + 1 < numDirChildren; i++) {
        Node_T child1 = NULL;
        Node_T child2 = NULL;

        if (NodeFT_getChild(parentNode, i, &child1, FALSE) != SUCCESS ||
            NodeFT_getChild(parentNode, i + 1, &child2, FALSE) != SUCCESS) {
            fprintf(stderr, "Error: Unable to retrieve directory children\n");
            return FALSE;
        }

        if (Path_comparePath(NodeFT_getPath(child1), NodeFT_getPath(child2)) > 0) {
            fprintf(stderr, "Error: Directory children are not in lexicographical order\n");
            return FALSE;
        }
    }

    /* Validate file children */
    numFileChildren = NodeFT_getNumChildren(parentNode, TRUE);
    for (i = 0; i + 1 < numFileChildren; i++) {
        Node_T child1 = NULL;
        Node_T child2 = NULL;

        if (NodeFT_getChild(parentNode, i, &child1, TRUE) != SUCCESS ||
            NodeFT_getChild(parentNode, i + 1, &child2, TRUE) != SUCCESS) {
            fprintf(stderr, "Error: Unable to retrieve file children\n");
            return FALSE;
        }

        if (Path_comparePath(NodeFT_getPath(child1), NodeFT_getPath(child2)) > 0) {
            fprintf(stderr, "Error: File children are not in lexicographical order\n");
            return FALSE;
        }
    }

    return TRUE;
}

/*
    Helper function to check for duplicate paths in the tree.
    Returns TRUE if no duplicates are found, FALSE otherwise.
*/
static boolean CheckerFT_noDuplicatePaths(Node_T node, DynArray_T seenNodes) {
    size_t i;
    assert(node != NULL);
    assert(seenNodes != NULL);

    for (i = 0; i < DynArray_getLength(seenNodes); i++) {
        Node_T existingNode = DynArray_get(seenNodes, i);
        if (Path_comparePath(NodeFT_getPath(node), NodeFT_getPath(existingNode)) == 0) {
            fprintf(stderr, "Error: Duplicate path detected: %s\n",
                    Path_getPathname(NodeFT_getPath(node)));
            return FALSE;
        }
    }
    return TRUE;
}

/*
    Helper function to recursively validate all children of a node.
    Returns TRUE if all children are valid, FALSE otherwise.
*/
static boolean CheckerFT_validateChildren(Node_T node, DynArray_T seenNodes) {
    size_t i;
    size_t numChildren;
    Node_T childNode = NULL;
    int status;

    assert(node != NULL);
    assert(seenNodes != NULL);

    /* Validate directory children */
    numChildren = NodeFT_getNumChildren(node, FALSE);
    for (i = 0; i < numChildren; i++) {
        status = NodeFT_getChild(node, i, &childNode, FALSE);
        if (status != SUCCESS || childNode == NULL) {
            fprintf(stderr, "Error: Inconsistent number of directory children\n");
            return FALSE;
        }
        if (!CheckerFT_validateTree(childNode, seenNodes))
            return FALSE;
    }

    /* Validate file children */
    numChildren = NodeFT_getNumChildren(node, TRUE);
    for (i = 0; i < numChildren; i++) {
        status = NodeFT_getChild(node, i, &childNode, TRUE);
        if (status != SUCCESS || childNode == NULL) {
            fprintf(stderr, "Error: Inconsistent number of file children\n");
            return FALSE;
        }
        if (!CheckerFT_validateTree(childNode, seenNodes))
            return FALSE;
    }

    return TRUE;
}

/*
    Recursive function to validate the tree starting from a given node.
    It checks for:
    - Node validity
    - No duplicate paths
    - Correct ordering of children
    Returns TRUE if the subtree is valid, FALSE otherwise.
*/
static boolean CheckerFT_validateTree(Node_T node, DynArray_T seenNodes) {
    assert(seenNodes != NULL);

    if (node == NULL)
        return TRUE;

    /* Validate the current node */
    if (!CheckerFT_Node_isValid(node))
        return FALSE;

    /* Check for duplicate paths */
    if (!CheckerFT_noDuplicatePaths(node, seenNodes))
        return FALSE;

    /* Add the current node to the list of seen nodes */
    if (!DynArray_add(seenNodes, node)) {
        fprintf(stderr, "Error: Failed to add node to seenNodes array\n");
        return FALSE;
    }

    /* If the node is a directory, check its children */
    if (!NodeFT_isFile(node)) {
        /* Validate the ordering of children */
        if (!CheckerFT_childrenAreOrdered(node))
            return FALSE;

        /* Recursively validate all children */
        if (!CheckerFT_validateChildren(node, seenNodes))
            return FALSE;
    }

    return TRUE;
}

/* See checkerFT.h for specification */
boolean CheckerFT_isValid(boolean isInitialized, Node_T root, size_t count) {
    DynArray_T seenNodes;
    boolean result;
    size_t actualCount;

    /* Check: If not initialized, root should be NULL and count should be zero */
    if (!isInitialized) {
        if (root != NULL) {
            fprintf(stderr, "Error: Uninitialized tree has non-NULL root\n");
            return FALSE;
        }
        if (count != 0) {
            fprintf(stderr, "Error: Uninitialized tree has non-zero count\n");
            return FALSE;
        }
        return TRUE;
    }

    /* Initialize dynamic array to keep track of seen nodes */
    seenNodes = DynArray_new(0);
    if (seenNodes == NULL) {
        fprintf(stderr, "Error: Unable to allocate memory for seenNodes\n");
        return FALSE;
    }

    /* Validate the tree starting from the root */
    result = CheckerFT_validateTree(root, seenNodes);

    /* Verify that the number of nodes matches the count */
    actualCount = DynArray_getLength(seenNodes);
    if (actualCount != count) {
        fprintf(stderr, "Error: Node count mismatch. Expected: %zu, Actual: %zu\n",
                count, actualCount);
        result = FALSE;
    }

    DynArray_free(seenNodes);
    return result;
}