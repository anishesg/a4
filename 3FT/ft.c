/*--------------------------------------------------------------------*/
/* ft.c                                                               */
/* Author: anish                                             */
/*--------------------------------------------------------------------*/

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ft.h"
#include "dynarray.h"
#include "path.h"
#include "a4def.h"

/* Structure representing a node in the File Tree */
typedef struct FileTreeNode {
    /* The path object corresponding to this node's absolute path */
    Path_T oNodePath;
    /* Parent node, NULL if this is the root */
    struct FileTreeNode *psParentNode;
    /* Dynamic array of child nodes */
    DynArray_T oChildren;
    /* TRUE if this node is a file, FALSE if it's a directory */
    boolean bIsFile;
    /* Contents of the file, NULL if not a file */
    void *pvFileContents;
    /* Size of the file contents */
    size_t ulFileLength;
} FileTreeNode_T;

/* Static variables for the File Tree's state */
static boolean bIsInitialized = FALSE;
static FileTreeNode_T *psRootNode = NULL;
static size_t ulTotalNodes = 0;

/* Helper function to free a node and its descendants */
static size_t FT_freeNode(FileTreeNode_T *psNode) {
    size_t ulFreedNodes = 0;
    size_t ulChildIndex;
    size_t ulNumChildren;

    assert(psNode != NULL);

    ulNumChildren = DynArray_getLength(psNode->oChildren);
    for (ulChildIndex = 0; ulChildIndex < ulNumChildren; ulChildIndex++) {
        FileTreeNode_T *psChildNode = DynArray_get(psNode->oChildren, ulChildIndex);
        ulFreedNodes += FT_freeNode(psChildNode);
    }

    DynArray_free(psNode->oChildren);
    Path_free(psNode->oNodePath);

    if (psNode->bIsFile && psNode->pvFileContents != NULL) {
        free(psNode->pvFileContents);
    }

    free(psNode);
    ulFreedNodes++;

    return ulFreedNodes;
}

/* Helper function to compare nodes for sorting */
static int FT_compareNodes(const void *pvNode1, const void *pvNode2) {
    FileTreeNode_T *psNode1 = *(FileTreeNode_T **)pvNode1;
    FileTreeNode_T *psNode2 = *(FileTreeNode_T **)pvNode2;
    int iTypeComparison;

    /* Files come before directories */
    iTypeComparison = (int)psNode2->bIsFile - (int)psNode1->bIsFile;
    if (iTypeComparison != 0) {
        return iTypeComparison;
    }

    /* Lexicographical order */
    return Path_comparePath(psNode1->oNodePath, psNode2->oNodePath);
}

/* Helper function to traverse the tree towards a path */
static int FT_traversePath(Path_T oTargetPath, FileTreeNode_T **ppsFurthestNode) {
    int iResult;
    Path_T oCurrentPrefix = NULL;
    FileTreeNode_T *psCurrentNode = NULL;
    size_t ulDepth, ulLevel;

    assert(oTargetPath != NULL);
    assert(ppsFurthestNode != NULL);

    if (psRootNode == NULL) {
        *ppsFurthestNode = NULL;
        return SUCCESS;
    }

    iResult = Path_prefix(oTargetPath, 1, &oCurrentPrefix);
    if (iResult != SUCCESS) {
        *ppsFurthestNode = NULL;
        return iResult;
    }

    if (Path_comparePath(psRootNode->oNodePath, oCurrentPrefix) != 0) {
        Path_free(oCurrentPrefix);
        *ppsFurthestNode = NULL;
        return CONFLICTING_PATH;
    }
    Path_free(oCurrentPrefix);

    psCurrentNode = psRootNode;
    ulDepth = Path_getDepth(oTargetPath);

    for (ulLevel = 2; ulLevel <= ulDepth; ulLevel++) {
        size_t ulChildIndex;
        size_t ulNumChildren = DynArray_getLength(psCurrentNode->oChildren);
        FileTreeNode_T *psChildNode = NULL;
        boolean bFound = FALSE;

        iResult = Path_prefix(oTargetPath, ulLevel, &oCurrentPrefix);
        if (iResult != SUCCESS) {
            *ppsFurthestNode = NULL;
            return iResult;
        }

        for (ulChildIndex = 0; ulChildIndex < ulNumChildren; ulChildIndex++) {
            psChildNode = DynArray_get(psCurrentNode->oChildren, ulChildIndex);
            if (Path_comparePath(psChildNode->oNodePath, oCurrentPrefix) == 0) {
                bFound = TRUE;
                break;
            }
        }

        Path_free(oCurrentPrefix);

        if (bFound) {
            if (psChildNode->bIsFile && ulLevel != ulDepth) {
                /* Intermediate path is a file, invalid */
                *ppsFurthestNode = NULL;
                return NOT_A_DIRECTORY;
            }
            psCurrentNode = psChildNode;
        } else {
            break;
        }
    }

    *ppsFurthestNode = psCurrentNode;
    return SUCCESS;
}

/* Helper function to find a node given its path */
static int FT_findNode(const char *pcPath, FileTreeNode_T **ppsResultNode) {
    int iResult;
    Path_T oSearchPath = NULL;
    FileTreeNode_T *psFoundNode = NULL;

    assert(pcPath != NULL);
    assert(ppsResultNode != NULL);

    if (!bIsInitialized) {
        *ppsResultNode = NULL;
        return INITIALIZATION_ERROR;
    }

    iResult = Path_new(pcPath, &oSearchPath);
    if (iResult != SUCCESS) {
        *ppsResultNode = NULL;
        return iResult;
    }

    iResult = FT_traversePath(oSearchPath, &psFoundNode);
    if (iResult != SUCCESS) {
        Path_free(oSearchPath);
        *ppsResultNode = NULL;
        return iResult;
    }

    if (psFoundNode == NULL) {
        Path_free(oSearchPath);
        *ppsResultNode = NULL;
        return NO_SUCH_PATH;
    }

    if (Path_comparePath(psFoundNode->oNodePath, oSearchPath) != 0) {
        Path_free(oSearchPath);
        *ppsResultNode = NULL;
        return NO_SUCH_PATH;
    }

    Path_free(oSearchPath);
    *ppsResultNode = psFoundNode;
    return SUCCESS;
}

/* Initializes the File Tree */
int FT_init(void) {
    if (bIsInitialized) {
        return INITIALIZATION_ERROR;
    }

    bIsInitialized = TRUE;
    psRootNode = NULL;
    ulTotalNodes = 0;

    return SUCCESS;
}

/* Destroys the File Tree */
int FT_destroy(void) {
    if (!bIsInitialized) {
        return INITIALIZATION_ERROR;
    }

    if (psRootNode != NULL) {
        ulTotalNodes -= FT_freeNode(psRootNode);
        psRootNode = NULL;
    }

    bIsInitialized = FALSE;
    return SUCCESS;
}

/* Inserts a new directory into the File Tree */
int FT_insertDir(const char *pcPath) {
    int iResult;
    Path_T oInsertPath = NULL;
    FileTreeNode_T *psCurrentNode = NULL;
    size_t ulDepth, ulLevel;

    assert(pcPath != NULL);

    if (!bIsInitialized) {
        return INITIALIZATION_ERROR;
    }

    iResult = Path_new(pcPath, &oInsertPath);
    if (iResult != SUCCESS) {
        return iResult;
    }

    iResult = FT_traversePath(oInsertPath, &psCurrentNode);
    if (iResult != SUCCESS) {
        Path_free(oInsertPath);
        return iResult;
    }

    if (psCurrentNode == NULL && psRootNode != NULL) {
        Path_free(oInsertPath);
        return CONFLICTING_PATH;
    }

    ulDepth = Path_getDepth(oInsertPath);
    ulLevel = psCurrentNode ? Path_getDepth(psCurrentNode->oNodePath) + 1 : 1;

    /* Check if directory already exists */
    if (psCurrentNode && Path_comparePath(psCurrentNode->oNodePath, oInsertPath) == 0) {
        Path_free(oInsertPath);
        return ALREADY_IN_TREE;
    }

    while (ulLevel <= ulDepth) {
        FileTreeNode_T *psNewNode = malloc(sizeof(FileTreeNode_T));
        Path_T oPrefixPath = NULL;

        if (psNewNode == NULL) {
            Path_free(oInsertPath);
            return MEMORY_ERROR;
        }

        iResult = Path_prefix(oInsertPath, ulLevel, &oPrefixPath);
        if (iResult != SUCCESS) {
            free(psNewNode);
            Path_free(oInsertPath);
            return iResult;
        }

        psNewNode->oNodePath = oPrefixPath;
        psNewNode->psParentNode = psCurrentNode;
        psNewNode->oChildren = DynArray_new(0);
        if (psNewNode->oChildren == NULL) {
            Path_free(oPrefixPath);
            free(psNewNode);
            Path_free(oInsertPath);
            return MEMORY_ERROR;
        }

        psNewNode->bIsFile = FALSE;
        psNewNode->pvFileContents = NULL;
        psNewNode->ulFileLength = 0;

        if (psCurrentNode) {
            DynArray_add(psCurrentNode->oChildren, psNewNode);
            DynArray_sort(psCurrentNode->oChildren, FT_compareNodes);
        } else {
            psRootNode = psNewNode;
        }

        psCurrentNode = psNewNode;
        ulTotalNodes++;
        ulLevel++;
    }

    Path_free(oInsertPath);
    return SUCCESS;
}

/* Checks if the FT contains a directory with the given path */
boolean FT_containsDir(const char *pcPath) {
    int iResult;
    FileTreeNode_T *psNode = NULL;

    assert(pcPath != NULL);

    iResult = FT_findNode(pcPath, &psNode);
    if (iResult != SUCCESS || psNode == NULL) {
        return FALSE;
    }

    return !psNode->bIsFile;
}

/* Removes a directory from the File Tree */
int FT_rmDir(const char *pcPath) {
    int iResult;
    FileTreeNode_T *psNode = NULL;

    assert(pcPath != NULL);

    iResult = FT_findNode(pcPath, &psNode);
    if (iResult != SUCCESS) {
        return iResult;
    }

    if (psNode->bIsFile) {
        return NOT_A_DIRECTORY;
    }

    /* Remove node from parent's children */
    if (psNode->psParentNode) {
        DynArray_remove(psNode->psParentNode->oChildren, psNode);
    } else {
        psRootNode = NULL;
    }

    ulTotalNodes -= FT_freeNode(psNode);
    return SUCCESS;
}

/* Inserts a new file into the File Tree */
int FT_insertFile(const char *pcPath, void *pvContents, size_t ulLength) {
    int iResult;
    Path_T oInsertPath = NULL;
    FileTreeNode_T *psCurrentNode = NULL;
    size_t ulDepth, ulLevel;

    assert(pcPath != NULL);

    if (!bIsInitialized) {
        return INITIALIZATION_ERROR;
    }

    iResult = Path_new(pcPath, &oInsertPath);
    if (iResult != SUCCESS) {
        return iResult;
    }

    /* Cannot insert a file as the root of the tree */
    if (Path_getDepth(oInsertPath) == 1) {
        Path_free(oInsertPath);
        return CONFLICTING_PATH;
    }

    iResult = FT_traversePath(oInsertPath, &psCurrentNode);
    if (iResult != SUCCESS) {
        Path_free(oInsertPath);
        return iResult;
    }

    if (psCurrentNode == NULL && psRootNode != NULL) {
        Path_free(oInsertPath);
        return CONFLICTING_PATH;
    }

    ulDepth = Path_getDepth(oInsertPath);
    ulLevel = psCurrentNode ? Path_getDepth(psCurrentNode->oNodePath) + 1 : 1;

    /* Check if file already exists */
    if (psCurrentNode && Path_comparePath(psCurrentNode->oNodePath, oInsertPath) == 0) {
        Path_free(oInsertPath);
        return ALREADY_IN_TREE;
    }

    while (ulLevel <= ulDepth) {
        FileTreeNode_T *psNewNode = malloc(sizeof(FileTreeNode_T));
        Path_T oPrefixPath = NULL;

        if (psNewNode == NULL) {
            Path_free(oInsertPath);
            return MEMORY_ERROR;
        }

        iResult = Path_prefix(oInsertPath, ulLevel, &oPrefixPath);
        if (iResult != SUCCESS) {
            free(psNewNode);
            Path_free(oInsertPath);
            return iResult;
        }

        psNewNode->oNodePath = oPrefixPath;
        psNewNode->psParentNode = psCurrentNode;
        psNewNode->oChildren = DynArray_new(0);
        if (psNewNode->oChildren == NULL) {
            Path_free(oPrefixPath);
            free(psNewNode);
            Path_free(oInsertPath);
            return MEMORY_ERROR;
        }

        if (ulLevel == ulDepth) {
            /* Final node is the file */
            psNewNode->bIsFile = TRUE;
            psNewNode->pvFileContents = pvContents;
            psNewNode->ulFileLength = ulLength;
        } else {
            /* Intermediate directories */
            psNewNode->bIsFile = FALSE;
            psNewNode->pvFileContents = NULL;
            psNewNode->ulFileLength = 0;
        }

        if (psCurrentNode) {
            DynArray_add(psCurrentNode->oChildren, psNewNode);
            DynArray_sort(psCurrentNode->oChildren, FT_compareNodes);
        } else {
            psRootNode = psNewNode;
        }

        psCurrentNode = psNewNode;
        ulTotalNodes++;
        ulLevel++;
    }

    Path_free(oInsertPath);
    return SUCCESS;
}

/* Checks if the FT contains a file with the given path */
boolean FT_containsFile(const char *pcPath) {
    int iResult;
    FileTreeNode_T *psNode = NULL;

    assert(pcPath != NULL);

    iResult = FT_findNode(pcPath, &psNode);
    if (iResult != SUCCESS || psNode == NULL) {
        return FALSE;
    }

    return psNode->bIsFile;
}

/* Removes a file from the File Tree */
int FT_rmFile(const char *pcPath) {
    int iResult;
    FileTreeNode_T *psNode = NULL;

    assert(pcPath != NULL);

    iResult = FT_findNode(pcPath, &psNode);
    if (iResult != SUCCESS) {
        return iResult;
    }

    if (!psNode->bIsFile) {
        return NOT_A_FILE;
    }

    /* Remove node from parent's children */
    if (psNode->psParentNode) {
        DynArray_remove(psNode->psParentNode->oChildren, psNode);
    } else {
        psRootNode = NULL;
    }

    ulTotalNodes -= FT_freeNode(psNode);
    return SUCCESS;
}

/* Retrieves the contents of a file */
void *FT_getFileContents(const char *pcPath) {
    int iResult;
    FileTreeNode_T *psNode = NULL;

    assert(pcPath != NULL);

    iResult = FT_findNode(pcPath, &psNode);
    if (iResult != SUCCESS || !psNode->bIsFile) {
        return NULL;
    }

    return psNode->pvFileContents;
}

/* Replaces the contents of a file */
void *FT_replaceFileContents(const char *pcPath, void *pvNewContents, size_t ulNewLength) {
    int iResult;
    FileTreeNode_T *psNode = NULL;
    void *pvOldContents = NULL;

    assert(pcPath != NULL);

    iResult = FT_findNode(pcPath, &psNode);
    if (iResult != SUCCESS || !psNode->bIsFile) {
        return NULL;
    }

    pvOldContents = psNode->pvFileContents;
    psNode->pvFileContents = pvNewContents;
    psNode->ulFileLength = ulNewLength;

    return pvOldContents;
}

/* Retrieves information about a node */
int FT_stat(const char *pcPath, boolean *pbIsFile, size_t *pulSize) {
    int iResult;
    FileTreeNode_T *psNode = NULL;

    assert(pcPath != NULL);
    assert(pbIsFile != NULL);
    assert(pulSize != NULL);

    iResult = FT_findNode(pcPath, &psNode);
    if (iResult != SUCCESS) {
        return iResult;
    }

    if (psNode->bIsFile) {
        *pbIsFile = TRUE;
        *pulSize = psNode->ulFileLength;
    } else {
        *pbIsFile = FALSE;
        /* Do not change *pulSize */
    }

    return SUCCESS;
}

/* Helper function for pre-order traversal */
static void FT_preOrderTraversal(FileTreeNode_T *psNode, DynArray_T oNodes) {
    size_t ulChildIndex;
    size_t ulNumChildren;

    if (psNode == NULL) {
        return;
    }

    DynArray_add(oNodes, psNode);
    ulNumChildren = DynArray_getLength(psNode->oChildren);

    /* Files before directories */
    for (ulChildIndex = 0; ulChildIndex < ulNumChildren; ulChildIndex++) {
        FileTreeNode_T *psChildNode = DynArray_get(psNode->oChildren, ulChildIndex);
        if (psChildNode->bIsFile) {
            FT_preOrderTraversal(psChildNode, oNodes);
        }
    }

    for (ulChildIndex = 0; ulChildIndex < ulNumChildren; ulChildIndex++) {
        FileTreeNode_T *psChildNode = DynArray_get(psNode->oChildren, ulChildIndex);
        if (!psChildNode->bIsFile) {
            FT_preOrderTraversal(psChildNode, oNodes);
        }
    }
}

/* Generates a string representation of the File Tree */
char *FT_toString(void) {
    DynArray_T oNodes;
    size_t ulTotalLength = 1; /* For the null terminator */
    char *pcResult = NULL;
    size_t ulIndex;

    if (!bIsInitialized) {
        return NULL;
    }

    oNodes = DynArray_new(0);
    if (oNodes == NULL) {
        return NULL;
    }

    FT_preOrderTraversal(psRootNode, oNodes);

    /* Calculate total length */
    for (ulIndex = 0; ulIndex < DynArray_getLength(oNodes); ulIndex++) {
        FileTreeNode_T *psNode = DynArray_get(oNodes, ulIndex);
        ulTotalLength += Path_getStrLength(psNode->oNodePath) + 1; /* For newline */
    }

    pcResult = malloc(ulTotalLength);
    if (pcResult == NULL) {
        DynArray_free(oNodes);
        return NULL;
    }
    *pcResult = '\0';

    /* Concatenate paths */
    for (ulIndex = 0; ulIndex < DynArray_getLength(oNodes); ulIndex++) {
        FileTreeNode_T *psNode = DynArray_get(oNodes, ulIndex);
        strcat(pcResult, Path_getPathname(psNode->oNodePath));
        strcat(pcResult, "\n");
    }

    DynArray_free(oNodes);
    return pcResult;
}