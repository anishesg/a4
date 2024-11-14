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
boolean CheckerDT_Node_isValid(Node_T currentNode) {
    Node_T parentNode;
    Path_T nodePath;
    Path_T parentPath;
    size_t sharedDepth;
    size_t expectedSharedDepth;

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
        sharedDepth = Path_getSharedPrefixDepth(nodePath, parentPath);
        expectedSharedDepth = Path_getDepth(parentPath);

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
   Performs a pre-order traversal of the tree rooted at rootNode.
   Returns FALSE if a broken invariant is found and
   returns TRUE otherwise.

   Parameters:
       rootNode - The node from which to start the traversal.
       validNodeCount - Pointer to a counter tracking the number of valid nodes.

   Returns:
       TRUE if the subtree is valid, FALSE otherwise.
*/
static boolean CheckerDT_treeValidate(Node_T rootNode, size_t *validNodeCount) {
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

    /* Increment the node count */
    (*validNodeCount)++;

    /* Retrieve the number of children the current node has */
    size_t numChildren = Node_getNumChildren(rootNode);

    /* Iterate through each child node */
    for (childIndex = 0; childIndex < numChildren; childIndex++) {
        /* Attempt to retrieve the child node */
        if (Node_getChild(rootNode, childIndex, &childNode) != SUCCESS) {
            fprintf(stderr, "Validation Error: Unable to retrieve child %lu of node %s.\n",
                    (unsigned long)childIndex, Path_getPathname(Node_getPath(rootNode)));
            return FALSE;
        }

        /* Recursively validate the child subtree */
        if (!CheckerDT_treeValidate(childNode, validNodeCount)) {
            /* Error message is already printed in the recursive call */
            return FALSE;
        }
    }

    return TRUE;
}

/* Function to count nodes in the tree */
static size_t CheckerDT_countNodes(Node_T rootNode) {
    size_t childIndex;
    Node_T childNode;
    size_t count = 0;

    /* Base Case: If the current node is NULL, count is 0 */
    if (rootNode == NULL) {
        return 0;
    }

    /* Count the current node */
    count++;

    /* Retrieve the number of children */
    size_t numChildren = Node_getNumChildren(rootNode);

    /* Iterate through each child and count recursively */
    for (childIndex = 0; childIndex < numChildren; childIndex++) {
        if (Node_getChild(rootNode, childIndex, &childNode) != SUCCESS) {
            fprintf(stderr, "Validation Error: Unable to retrieve child %lu of node %s for counting.\n",
                    (unsigned long)childIndex, Path_getPathname(Node_getPath(rootNode)));
            return 0; /* Returning 0 to indicate failure */
        }

        /* Recursively count the child nodes */
        size_t childCount = CheckerDT_countNodes(childNode);
        if (childCount == 0 && childNode != NULL) { /* childNode being NULL is acceptable */
            /* Error message is already printed in the recursive call */
            return 0;
        }
        count += childCount;
    }

    return count;
}

/* see checkerDT.h for specification */
boolean CheckerDT_isValid(boolean isInitialized, Node_T rootNode, size_t expectedCount) {
    size_t actualCount = 0;

    /* Validation: If the Directory Tree is not initialized, ensure it has no nodes */
    if (!isInitialized) {
        if (expectedCount != 0) {
            fprintf(stderr, "Validation Error: Directory Tree is not initialized, but node count is %lu (expected 0).\n", 
                    (unsigned long)expectedCount);
            return FALSE;
        }
        if (rootNode != NULL) {
            fprintf(stderr, "Validation Error: Directory Tree is not initialized, but the root node is not NULL.\n");
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

    /* Perform pre-order traversal to validate the tree and count nodes */
    if (!CheckerDT_treeValidate(rootNode, &actualCount)) {
        /* Error messages are already printed in CheckerDT_treeValidate */
        return FALSE;
    }

    /* Perform node counting */
    actualCount = CheckerDT_countNodes(rootNode);
    if (actualCount == 0 && rootNode != NULL) {
        /* Error message is already printed in CheckerDT_countNodes */
        fprintf(stderr, "Validation Error: Failed to count nodes correctly.\n");
        return FALSE;
    }

    /* Verify that the actual node count matches the expected count */
    if (actualCount != expectedCount) {
        fprintf(stderr, "Validation Error: Directory Tree node count mismatch. Expected %lu, found %lu.\n", 
                (unsigned long)expectedCount, (unsigned long)actualCount);
        return FALSE;
    }

    /* All checks passed */
    return TRUE;
}