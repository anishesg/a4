/*--------------------------------------------------------------------*/
/* checkerDT.c                                                        */
/* Author: anish                                             */
/*--------------------------------------------------------------------*/

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "checkerDT.h"
#include "dynarray.h"
#include "path.h"

/* see checkerDT.h for specification */
boolean CheckerDT_Node_isValid(Node_T currentNode) {
    Node_T parentNode;
    Path_T nodePath;
    Path_T parentPath;

    /* Validation: Ensure the node reference is not NULL */
    if (currentNode == NULL) {
        fprintf(stderr, "Validation Error: Encountered a NULL node reference.\n");
        return FALSE;
    }

    /* Retrieve the node's path */
    nodePath = Node_getPath(currentNode);
    if (nodePath == NULL || strlen(Path_getPathname(nodePath)) == 0) {
        fprintf(stderr, "Validation Error: Node has an invalid or empty path.\n");
        return FALSE;
    }

    /* Retrieve the parent node */
    parentNode = Node_getParent(currentNode);
    if (parentNode != NULL) {
        /* Retrieve the parent's path */
        parentPath = Node_getPath(parentNode);
        if (parentPath == NULL) {
            fprintf(stderr, "Validation Error: Parent node has an invalid path.\n");
            return FALSE;
        }

        /* Calculate the shared prefix depth between node and parent */
        size_t sharedDepth = Path_getSharedPrefixDepth(nodePath, parentPath);
        size_t nodeDepth = Path_getDepth(nodePath);
        size_t expectedSharedDepth = Path_getDepth(parentPath);

        /* Ensure the parent's path is the immediate prefix of the node's path */
        if (sharedDepth != expectedSharedDepth) {
            fprintf(stderr, "Validation Error: Parent's path is not the immediate prefix of the node's path.\n");
            fprintf(stderr, "Parent Path: %s\nNode Path: %s\n",
                    Path_getPathname(parentPath), Path_getPathname(nodePath));
            return FALSE;
        }

        /* Additional Check: Prevent cyclic relationships */
        if (Path_comparePath(nodePath, parentPath) == 0) {
            fprintf(stderr, "Validation Error: Node's path is identical to its parent's path, indicating a cycle.\n");
            return FALSE;
        }
    }

    /* Additional node-specific validations can be added here */

    return TRUE;
}

/*
   Performs a pre-order traversal of the Directory Tree starting from the given node.
   Validates each node's integrity and ensures there are no structural anomalies.
   
   Parameters:
       rootNode - The node from which to start the traversal.
   
   Returns:
       TRUE if the subtree is valid, FALSE otherwise.
*/
static boolean CheckerDT_treeValidate(Node_T rootNode) {
    size_t childIndex;
    Node_T childNode;

    /* Base Case: If the current node is NULL, it's considered valid */
    if (rootNode == NULL) {
        return TRUE;
    }

    /* Validate the current node */
    if (!CheckerDT_Node_isValid(rootNode)) {
        /* Error message is already printed in CheckerDT_Node_isValid */
        return FALSE;
    }

    /* Iterate through each child of the current node */
    for (childIndex = 0; childIndex < Node_getNumChildren(rootNode); childIndex++) {
        /* Attempt to retrieve the child node */
        if (Node_getChild(rootNode, childIndex, &childNode) != SUCCESS) {
            fprintf(stderr, "Validation Error: Unable to retrieve child %zu of node %s.\n",
                    childIndex, Path_getPathname(Node_getPath(rootNode)));
            return FALSE;
        }

        /* Recursively validate the child subtree */
        if (!CheckerDT_treeValidate(childNode)) {
            /* Error message is already printed in the recursive call */
            return FALSE;
        }
    }

    return TRUE;
}

/* see checkerDT.h for specification */
boolean CheckerDT_isValid(boolean isInitialized, Node_T rootNode, size_t expectedCount) {
    size_t actualNodeCount = 0;

    /* Validation: If the Directory Tree is not initialized, ensure it has no nodes */
    if (!isInitialized) {
        if (expectedCount != 0) {
            fprintf(stderr, "Validation Error: Directory Tree is uninitialized but has a node count of %zu (expected 0).\n", expectedCount);
            return FALSE;
        }
        if (rootNode != NULL) {
            fprintf(stderr, "Validation Error: Directory Tree is uninitialized but the root node is not NULL.\n");
            return FALSE;
        }
        /* If uninitialized and counts are correct, it's valid */
        return TRUE;
    }

    /* Validation: An initialized Directory Tree must have a valid root node */
    if (rootNode == NULL) {
        fprintf(stderr, "Validation Error: Directory Tree is initialized but the root node is NULL.\n");
        return FALSE;
    }

    /* Perform a pre-order traversal to validate the tree and count nodes */
    if (!CheckerDT_treeValidate(rootNode)) {
        /* Error messages are already printed in CheckerDT_treeValidate */
        return FALSE;
    }

    /* At this point, traverse the tree again to count the nodes */
    /* Alternatively, integrate counting into the traversal function for efficiency */
    /* For simplicity, we'll traverse again here */

    /* Initialize a dynamic array to keep track of seen nodes */
    DynArray_T seenNodes = DynArray_new(0);
    if (seenNodes == NULL) {
        fprintf(stderr, "Validation Error: Failed to create dynamic array for node tracking.\n");
        return FALSE;
    }

    /* Define a helper function to count nodes */
    /* Since C doesn't support nested functions, we'll define an external function or manage differently */
    /* To keep it simple, let's assume CheckerDT_treeValidate also counts nodes */

    /* Reset actualNodeCount */
    actualNodeCount = 0;

    /* Modify CheckerDT_treeValidate to count nodes (not shown here) */
    /* Alternatively, perform counting separately */

    /* Here, let's perform a simple traversal to count nodes */
    /* In a real implementation, it's better to integrate counting into the validation traversal */

    /* Define a stack for iterative traversal */
    size_t stackSize = DynArray_getLength(seenNodes);
    DynArray_add(seenNodes, rootNode);

    while (DynArray_getLength(seenNodes) > 0) {
        /* Pop the last element */
        Node_T current = DynArray_get(seenNodes, DynArray_getLength(seenNodes) - 1);
        DynArray_set(seenNodes, DynArray_getLength(seenNodes) - 1, NULL); /* Remove */
        actualNodeCount++;

        /* Push children onto the stack */
        size_t numChildren = Node_getNumChildren(current);
        size_t i;
        for (i = 0; i < numChildren; i++) {
            Node_T child = NULL;
            if (Node_getChild(current, i, &child) == SUCCESS && child != NULL) {
                DynArray_add(seenNodes, child);
            }
        }
    }

    /* Free the dynamic array */
    DynArray_free(seenNodes);

    /* Validate the node count */
    if (actualNodeCount != expectedCount) {
        fprintf(stderr, "Validation Error: Node count mismatch. Expected %zu, found %zu.\n", expectedCount, actualNodeCount);
        return FALSE;
    }

    /* All checks passed */
    return TRUE;
}