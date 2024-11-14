/*--------------------------------------------------------------------*/
/* checkerDT.c                                                        */
/* Author:                                                            */
/*--------------------------------------------------------------------*/

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "checkerDT.h"
#include "dynarray.h"
#include "path.h"

/*
 * Function: validateNode
 * ----------------------
 * Ensures that a single node within the Directory Tree adheres to all
 * structural and relational invariants.
 *
 * Parameters:
 *   node - The node to be validated.
 *
 * Returns:
 *   TRUE if the node is valid, FALSE otherwise.
 */
boolean validateNode(Node_T node) {
    Node_T parentNode;
    Path_T currentPath;
    Path_T parentPath;

    /*
     * Check if the node reference is valid.
     */
    if (node == NULL) {
        fprintf(stderr, "Error: Encountered a NULL node reference.\n");
        return FALSE;
    }

    /*
     * Retrieve and validate the node's path.
     */
    currentPath = Node_getPath(node);
    if (currentPath == NULL || strlen(Path_getPathname(currentPath)) == 0) {
        fprintf(stderr, "Error: Node has an invalid or empty path.\n");
        return FALSE;
    }

    /*
     * Obtain the parent node and verify path hierarchy.
     */
    parentNode = Node_getParent(node);
    if (parentNode != NULL) {
        parentPath = Node_getPath(parentNode);
        if (parentPath == NULL) {
            fprintf(stderr, "Error: Parent node possesses an invalid path.\n");
            return FALSE;
        }

        size_t sharedPrefix = Path_getSharedPrefixDepth(currentPath, parentPath);
        size_t nodeDepth = Path_getDepth(currentPath);
        size_t expectedShared = Path_getDepth(parentPath);

        /*
         * The shared prefix depth should exactly match the parent's depth.
         */
        if (sharedPrefix != expectedShared) {
            fprintf(stderr, "Error: Parent's path is not a direct prefix of the node's path.\n");
            fprintf(stderr, "Parent Path: %s\nNode Path: %s\n", 
                    Path_getPathname(parentPath), Path_getPathname(currentPath));
            return FALSE;
        }

        /*
         * Prevent cycles by ensuring the node's path is distinct from its parent's path.
         */
        if (Path_comparePath(currentPath, parentPath) == 0) {
            fprintf(stderr, "Error: Node's path matches its parent's path, indicating a cycle.\n");
            return FALSE;
        }
    }

    /* Additional node-level validations can be incorporated here. */

    return TRUE;
}

/*
 * Function: traverseAndValidate
 * -----------------------------
 * Recursively traverses the Directory Tree in a pre-order manner, validating
 * each node and ensuring the tree's structural integrity.
 *
 * Parameters:
 *   currentNode - The node currently being traversed.
 *   count       - Pointer to a counter tracking the number of validated nodes.
 *
 * Returns:
 *   TRUE if the subtree rooted at currentNode is valid, FALSE otherwise.
 */
static boolean traverseAndValidate(Node_T currentNode, size_t *count) {
    size_t childIndex;
    Node_T childNode;

    /*
     * Base case: If the current node is NULL, it's considered valid.
     */
    if (currentNode == NULL) {
        return TRUE;
    }

    /*
     * Validate the current node.
     */
    if (!validateNode(currentNode)) {
        /* Error details are handled within validateNode */
        return FALSE;
    }

    /*
     * Increment the count of validated nodes.
     */
    (*count)++;

    /*
     * Determine the number of children the current node has.
     */
    size_t totalChildren = Node_getNumChildren(currentNode);

    /*
     * Iterate through each child node.
     */
    for (childIndex = 0; childIndex < totalChildren; childIndex++) {
        /*
         * Attempt to retrieve the child node.
         */
        if (Node_getChild(currentNode, childIndex, &childNode) != SUCCESS) {
            fprintf(stderr, "Error: Unable to access child %zu of node %s.\n",
                    childIndex, Path_getPathname(Node_getPath(currentNode)));
            return FALSE;
        }

        /*
         * Check for duplicate paths among siblings.
         */
        for (size_t sibling = 0; sibling < childIndex; sibling++) {
            Node_T siblingNode = NULL;
            if (Node_getChild(currentNode, sibling, &siblingNode) != SUCCESS) {
                fprintf(stderr, "Error: Unable to access sibling %zu of node %s.\n",
                        sibling, Path_getPathname(Node_getPath(currentNode)));
                return FALSE;
            }

            /*
             * Compare the paths of the current child with its siblings to detect duplicates.
             */
            if (Path_comparePath(Node_getPath(childNode), Node_getPath(siblingNode)) == 0) {
                fprintf(stderr, "Error: Duplicate path detected: %s.\n", 
                        Path_getPathname(Node_getPath(childNode)));
                return FALSE;
            }

            /*
             * Ensure that children are ordered lexicographically.
             */
            if (Path_comparePath(Node_getPath(childNode), Node_getPath(siblingNode)) < 0) {
                fprintf(stderr, "Error: Children are not in lexicographic order. '%s' should follow '%s'.\n",
                        Path_getPathname(Node_getPath(siblingNode)),
                        Path_getPathname(Node_getPath(childNode)));
                return FALSE;
            }
        }

        /*
         * Recursively validate the child subtree.
         */
        if (!traverseAndValidate(childNode, count)) {
            /* Errors are reported in recursive calls */
            return FALSE;
        }
    }

    return TRUE;
}

/*
 * Function: CheckerDT_isValid
 * ---------------------------
 * Initiates the validation process for the entire Directory Tree, ensuring
 * that all structural and relational invariants are maintained.
 *
 * Parameters:
 *   isInitialized - Boolean flag indicating whether the Directory Tree has been initialized.
 *   rootNode      - The root node of the Directory Tree.
 *   expectedCount - The expected total number of nodes in the Directory Tree.
 *
 * Returns:
 *   TRUE if the Directory Tree is valid, FALSE otherwise.
 */
boolean CheckerDT_isValid(boolean isInitialized, Node_T rootNode, size_t expectedCount) {
    size_t validatedCount = 0;

    /*
     * Validate the initialization state of the Directory Tree.
     */
    if (!isInitialized) {
        if (expectedCount != 0) {
            fprintf(stderr, "Error: Directory Tree is not initialized, but node count is %zu (expected 0).\n", expectedCount);
            return FALSE;
        }
        if (rootNode != NULL) {
            fprintf(stderr, "Error: Directory Tree is not initialized, but root node exists.\n");
            return FALSE;
        }
        /* If not initialized and counts are correct, the tree is valid */
        return TRUE;
    }

    /*
     * Ensure that an initialized Directory Tree has a valid root node.
     */
    if (rootNode == NULL) {
        fprintf(stderr, "Error: Directory Tree is initialized, but root node is NULL.\n");
        return FALSE;
    }

    /*
     * Traverse the tree to validate each node and count the total number of nodes.
     */
    if (!traverseAndValidate(rootNode, &validatedCount)) {
        /* Specific error messages are handled within traverseAndValidate */
        return FALSE;
    }

    /*
     * Verify that the actual number of validated nodes matches the expected count.
     */
    if (validatedCount != expectedCount) {
        fprintf(stderr, "Error: Node count mismatch. Expected %zu, but found %zu.\n", expectedCount, validatedCount);
        return FALSE;
    }

    /* All validations passed; the Directory Tree is considered valid */
    return TRUE;
}