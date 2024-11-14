/*--------------------------------------------------------------------*/
/* ft.c                                                               */
/* Author: anish                                               */
/*--------------------------------------------------------------------*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ft.h"
#include "dynarray.h"
#include "path.h"
#include "a4def.h"

/* Structure representing a node in the File Tree */
typedef struct FTNode {
    /* Absolute path of this node */
    Path_T oPath;
    /* Parent node, NULL if this is the root */
    struct FTNode *psParent;
    /* Dynamic array of children nodes */
    DynArray_T oChildren;
    /* TRUE if this node is a file, FALSE if it's a directory */
    boolean bIsFile;
    /* Contents of the file, NULL if not a file */
    void *pvContents;
    /* Size of the file contents */
    size_t ulSize;
} FTNode_T;

/* Static variables for the FT's state */
static boolean bIsInitialized = FALSE;
static FTNode_T *psRoot = NULL;
static size_t ulCount = 0;

/* Helper function to recursively free nodes */
static size_t FTNode_free(FTNode_T *psNode) {
    size_t ulFreed = 0;
    size_t ulIndex;
    size_t ulNumChildren;

    if (psNode == NULL)
        return ulFreed;

    ulNumChildren = DynArray_getLength(psNode->oChildren);
    for (ulIndex = 0; ulIndex < ulNumChildren; ulIndex++) {
        FTNode_T *psChild = DynArray_get(psNode->oChildren, ulIndex);
        ulFreed += FTNode_free(psChild);
    }

    DynArray_free(psNode->oChildren);
    Path_free(psNode->oPath);

    if (psNode->bIsFile && psNode->pvContents != NULL)
        free(psNode->pvContents);

    free(psNode);
    ulFreed++;

    return ulFreed;
}

/* Helper function to compare nodes for sorting */
static int FTNode_compare(const void *pvFirst, const void *pvSecond) {
    FTNode_T *psFirst = *(FTNode_T **)pvFirst;
    FTNode_T *psSecond = *(FTNode_T **)pvSecond;
    int iTypeComparison;

    /* Files come before directories */
    iTypeComparison = (int)psSecond->bIsFile - (int)psFirst->bIsFile;
    if (iTypeComparison != 0)
        return iTypeComparison;

    return Path_comparePath(psFirst->oPath, psSecond->oPath);
}

/* Helper function to traverse the tree as far as possible towards oPath */
static int FT_traversePath(Path_T oPath, FTNode_T **ppsResultNode) {
    int iStatus;
    Path_T oPrefix = NULL;
    FTNode_T *psCurrent = NULL;
    size_t ulDepth, ulIndex;

    assert(oPath != NULL);
    assert(ppsResultNode != NULL);

    if (psRoot == NULL) {
        *ppsResultNode = NULL;
        return SUCCESS;
    }

    iStatus = Path_prefix(oPath, 1, &oPrefix);
    if (iStatus != SUCCESS) {
        *ppsResultNode = NULL;
        return iStatus;
    }

    if (Path_comparePath(psRoot->oPath, oPrefix) != 0) {
        Path_free(oPrefix);
        *ppsResultNode = NULL;
        return CONFLICTING_PATH;
    }
    Path_free(oPrefix);

    psCurrent = psRoot;
    ulDepth = Path_getDepth(oPath);

    for (ulIndex = 2; ulIndex <= ulDepth; ulIndex++) {
        size_t ulChildIndex;
        size_t ulNumChildren = DynArray_getLength(psCurrent->oChildren);
        FTNode_T *psChild = NULL;
        boolean bFound = FALSE;

        iStatus = Path_prefix(oPath, ulIndex, &oPrefix);
        if (iStatus != SUCCESS) {
            *ppsResultNode = NULL;
            return iStatus;
        }

        for (ulChildIndex = 0; ulChildIndex < ulNumChildren; ulChildIndex++) {
            psChild = DynArray_get(psCurrent->oChildren, ulChildIndex);
            if (Path_comparePath(psChild->oPath, oPrefix) == 0) {
                bFound = TRUE;
                break;
            }
        }

        Path_free(oPrefix);

        if (bFound) {
            psCurrent = psChild;
        } else {
            break;
        }
    }

    *ppsResultNode = psCurrent;
    return SUCCESS;
}

/* Helper function to find a node given its path */
static int FT_findNode(const char *pcPath, FTNode_T **ppsNode) {
    int iStatus;
    Path_T oPath = NULL;
    FTNode_T *psNode = NULL;

    assert(pcPath != NULL);
    assert(ppsNode != NULL);

    if (!bIsInitialized) {
        *ppsNode = NULL;
        return INITIALIZATION_ERROR;
    }

    iStatus = Path_new(pcPath, &oPath);
    if (iStatus != SUCCESS) {
        *ppsNode = NULL;
        return iStatus;
    }

    iStatus = FT_traversePath(oPath, &psNode);
    if (iStatus != SUCCESS) {
        Path_free(oPath);
        *ppsNode = NULL;
        return iStatus;
    }

    if (psNode == NULL) {
        Path_free(oPath);
        *ppsNode = NULL;
        return NO_SUCH_PATH;
    }

    if (Path_comparePath(psNode->oPath, oPath) != 0) {
        Path_free(oPath);
        *ppsNode = NULL;
        return NO_SUCH_PATH;
    }

    Path_free(oPath);
    *ppsNode = psNode;
    return SUCCESS;
}

/* Initializes the File Tree */
int FT_init(void) {
    if (bIsInitialized)
        return INITIALIZATION_ERROR;

    bIsInitialized = TRUE;
    psRoot = NULL;
    ulCount = 0;

    return SUCCESS;
}

/* Destroys the File Tree */
int FT_destroy(void) {
    if (!bIsInitialized)
        return INITIALIZATION_ERROR;

    if (psRoot != NULL) {
        ulCount -= FTNode_free(psRoot);
        psRoot = NULL;
    }

    bIsInitialized = FALSE;

    return SUCCESS;
}

/* Inserts a directory into the File Tree */
int FT_insertDir(const char *pcPath) {
    int iStatus;
    Path_T oPath = NULL;
    FTNode_T *psCurrent = NULL;
    size_t ulDepth, ulIndex;

    assert(pcPath != NULL);

    if (!bIsInitialized)
        return INITIALIZATION_ERROR;

    iStatus = Path_new(pcPath, &oPath);
    if (iStatus != SUCCESS)
        return iStatus;

    iStatus = FT_traversePath(oPath, &psCurrent);
    if (iStatus != SUCCESS) {
        Path_free(oPath);
        return iStatus;
    }

    if (psCurrent == NULL && psRoot != NULL) {
        Path_free(oPath);
        return CONFLICTING_PATH;
    }

    ulDepth = Path_getDepth(oPath);
    ulIndex = psCurrent ? Path_getDepth(psCurrent->oPath) + 1 : 1;

    /* Check if directory already exists */
    if (psCurrent && Path_comparePath(psCurrent->oPath, oPath) == 0) {
        Path_free(oPath);
        return ALREADY_IN_TREE;
    }

    while (ulIndex <= ulDepth) {
        FTNode_T *psNewNode = malloc(sizeof(FTNode_T));
        Path_T oPrefix = NULL;

        if (psNewNode == NULL) {
            Path_free(oPath);
            return MEMORY_ERROR;
        }

        iStatus = Path_prefix(oPath, ulIndex, &oPrefix);
        if (iStatus != SUCCESS) {
            free(psNewNode);
            Path_free(oPath);
            return iStatus;
        }

        psNewNode->oPath = oPrefix;
        psNewNode->psParent = psCurrent;
        psNewNode->oChildren = DynArray_new(0);
        if (psNewNode->oChildren == NULL) {
            Path_free(oPrefix);
            free(psNewNode);
            Path_free(oPath);
            return MEMORY_ERROR;
        }
        psNewNode->bIsFile = FALSE;
        psNewNode->pvContents = NULL;
        psNewNode->ulSize = 0;

        if (psCurrent) {
            DynArray_add(psCurrent->oChildren, psNewNode);
            DynArray_sort(psCurrent->oChildren, FTNode_compare);
        } else {
            psRoot = psNewNode;
        }

        psCurrent = psNewNode;
        ulCount++;
        ulIndex++;
    }

    Path_free(oPath);
    return SUCCESS;
}

/* Inserts a file into the File Tree */
int FT_insertFile(const char *pcPath, void *pvContents, size_t ulLength) {
    int iStatus;
    Path_T oPath = NULL;
    FTNode_T *psCurrent = NULL;
    size_t ulDepth, ulIndex;

    assert(pcPath != NULL);
    assert(pvContents != NULL);

    if (!bIsInitialized)
        return INITIALIZATION_ERROR;

    iStatus = Path_new(pcPath, &oPath);
    if (iStatus != SUCCESS)
        return iStatus;

    iStatus = FT_traversePath(oPath, &psCurrent);
    if (iStatus != SUCCESS) {
        Path_free(oPath);
        return iStatus;
    }

    if (psCurrent == NULL && psRoot != NULL) {
        Path_free(oPath);
        return CONFLICTING_PATH;
    }

    if (Path_getDepth(oPath) == 1) {
        Path_free(oPath);
        return CONFLICTING_PATH;
    }

    ulDepth = Path_getDepth(oPath);
    ulIndex = psCurrent ? Path_getDepth(psCurrent->oPath) + 1 : 1;

    /* Check if file already exists */
    if (psCurrent && Path_comparePath(psCurrent->oPath, oPath) == 0) {
        Path_free(oPath);
        return ALREADY_IN_TREE;
    }

    while (ulIndex <= ulDepth) {
        FTNode_T *psNewNode = malloc(sizeof(FTNode_T));
        Path_T oPrefix = NULL;

        if (psNewNode == NULL) {
            Path_free(oPath);
            return MEMORY_ERROR;
        }

        iStatus = Path_prefix(oPath, ulIndex, &oPrefix);
        if (iStatus != SUCCESS) {
            free(psNewNode);
            Path_free(oPath);
            return iStatus;
        }

        psNewNode->oPath = oPrefix;
        psNewNode->psParent = psCurrent;
        psNewNode->oChildren = DynArray_new(0);
        if (psNewNode->oChildren == NULL) {
            Path_free(oPrefix);
            free(psNewNode);
            Path_free(oPath);
            return MEMORY_ERROR;
        }

        if (ulIndex == ulDepth) {
            /* This is the file node */
            psNewNode->bIsFile = TRUE;
            psNewNode->pvContents = malloc(ulLength);
            if (psNewNode->pvContents == NULL) {
                DynArray_free(psNewNode->oChildren);
                Path_free(oPrefix);
                free(psNewNode);
                Path_free(oPath);
                return MEMORY_ERROR;
            }
            memcpy(psNewNode->pvContents, pvContents, ulLength);
            psNewNode->ulSize = ulLength;
        } else {
            /* Intermediate directories */
            psNewNode->bIsFile = FALSE;
            psNewNode->pvContents = NULL;
            psNewNode->ulSize = 0;
        }

        if (psCurrent) {
            DynArray_add(psCurrent->oChildren, psNewNode);
            DynArray_sort(psCurrent->oChildren, FTNode_compare);
        } else {
            psRoot = psNewNode;
        }

        psCurrent = psNewNode;
        ulCount++;
        ulIndex++;
    }

    Path_free(oPath);
    return SUCCESS;
}

/* Checks if a directory exists at the given path */
boolean FT_containsDir(const char *pcPath) {
    int iStatus;
    FTNode_T *psNode = NULL;

    assert(pcPath != NULL);

    iStatus = FT_findNode(pcPath, &psNode);
    if (iStatus != SUCCESS || psNode == NULL)
        return FALSE;

    return !psNode->bIsFile;
}

/* Checks if a file exists at the given path */
boolean FT_containsFile(const char *pcPath) {
    int iStatus;
    FTNode_T *psNode = NULL;

    assert(pcPath != NULL);

    iStatus = FT_findNode(pcPath, &psNode);
    if (iStatus != SUCCESS || psNode == NULL)
        return FALSE;

    return psNode->bIsFile;
}

/* Removes a directory from the File Tree */
int FT_rmDir(const char *pcPath) {
    int iStatus;
    FTNode_T *psNode = NULL;

    assert(pcPath != NULL);

    iStatus = FT_findNode(pcPath, &psNode);
    if (iStatus != SUCCESS)
        return iStatus;

    if (psNode->bIsFile)
        return NOT_A_DIRECTORY;

    /* Remove node from parent's children */
    if (psNode->psParent) {
        DynArray_remove(psNode->psParent->oChildren, psNode);
    } else {
        psRoot = NULL;
    }

    ulCount -= FTNode_free(psNode);
    return SUCCESS;
}

/* Removes a file from the File Tree */
int FT_rmFile(const char *pcPath) {
    int iStatus;
    FTNode_T *psNode = NULL;

    assert(pcPath != NULL);

    iStatus = FT_findNode(pcPath, &psNode);
    if (iStatus != SUCCESS)
        return iStatus;

    if (!psNode->bIsFile)
        return NOT_A_FILE;

    /* Remove node from parent's children */
    if (psNode->psParent) {
        DynArray_remove(psNode->psParent->oChildren, psNode);
    } else {
        psRoot = NULL;
    }

    ulCount -= FTNode_free(psNode);
    return SUCCESS;
}

/* Retrieves the contents of a file */
void *FT_getFileContents(const char *pcPath) {
    int iStatus;
    FTNode_T *psNode = NULL;

    assert(pcPath != NULL);

    iStatus = FT_findNode(pcPath, &psNode);
    if (iStatus != SUCCESS || !psNode->bIsFile)
        return NULL;

    return psNode->pvContents;
}

/* Replaces the contents of a file */
void *FT_replaceFileContents(const char *pcPath, void *pvNewContents,
                             size_t ulNewLength) {
    int iStatus;
    FTNode_T *psNode = NULL;
    void *pvOldContents;

    assert(pcPath != NULL);
    assert(pvNewContents != NULL);

    iStatus = FT_findNode(pcPath, &psNode);
    if (iStatus != SUCCESS || !psNode->bIsFile)
        return NULL;

    pvOldContents = psNode->pvContents;
    psNode->pvContents = malloc(ulNewLength);
    if (psNode->pvContents == NULL) {
        psNode->pvContents = pvOldContents;
        return NULL;
    }
    memcpy(psNode->pvContents, pvNewContents, ulNewLength);
    psNode->ulSize = ulNewLength;

    if (pvOldContents != NULL)
        free(pvOldContents);

    return pvOldContents;
}

/* Retrieves information about a node */
int FT_stat(const char *pcPath, boolean *pbIsFile, size_t *pulSize) {
    int iStatus;
    FTNode_T *psNode = NULL;

    assert(pcPath != NULL);
    assert(pbIsFile != NULL);
    assert(pulSize != NULL);

    iStatus = FT_findNode(pcPath, &psNode);
    if (iStatus != SUCCESS)
        return iStatus;

    *pbIsFile = psNode->bIsFile;
    *pulSize = psNode->bIsFile ? psNode->ulSize : 0;

    return SUCCESS;
}

/* Helper function for pre-order traversal */
static void FT_preOrderTraversal(FTNode_T *psNode, DynArray_T oNodes) {
    size_t ulIndex;
    size_t ulNumChildren;

    if (psNode == NULL)
        return;

    DynArray_add(oNodes, psNode);
    ulNumChildren = DynArray_getLength(psNode->oChildren);

    /* Files before directories */
    for (ulIndex = 0; ulIndex < ulNumChildren; ulIndex++) {
        FTNode_T *psChild = DynArray_get(psNode->oChildren, ulIndex);
        if (psChild->bIsFile)
            FT_preOrderTraversal(psChild, oNodes);
    }

    for (ulIndex = 0; ulIndex < ulNumChildren; ulIndex++) {
        FTNode_T *psChild = DynArray_get(psNode->oChildren, ulIndex);
        if (!psChild->bIsFile)
            FT_preOrderTraversal(psChild, oNodes);
    }
}

/* Generates a string representation of the File Tree */
char *FT_toString(void) {
    DynArray_T oNodes;
    size_t ulTotalLength = 1; /* For the null terminator */
    char *pcResult = NULL;
    size_t ulIndex;

    if (!bIsInitialized)
        return NULL;

    oNodes = DynArray_new(0);
    if (oNodes == NULL)
        return NULL;

    FT_preOrderTraversal(psRoot, oNodes);

    /* Calculate total length */
    for (ulIndex = 0; ulIndex < DynArray_getLength(oNodes); ulIndex++) {
        FTNode_T *psNode = DynArray_get(oNodes, ulIndex);
        ulTotalLength += Path_getStrLength(psNode->oPath) + 1; /* For newline */
    }

    pcResult = malloc(ulTotalLength);
    if (pcResult == NULL) {
        DynArray_free(oNodes);
        return NULL;
    }
    *pcResult = '\0';

    /* Concatenate paths */
    for (ulIndex = 0; ulIndex < DynArray_getLength(oNodes); ulIndex++) {
        FTNode_T *psNode = DynArray_get(oNodes, ulIndex);
        strcat(pcResult, Path_getPathname(psNode->oPath));
        strcat(pcResult, "\n");
    }

    DynArray_free(oNodes);
    return pcResult;
}