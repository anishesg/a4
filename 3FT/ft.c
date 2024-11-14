
#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "a4def.h"
#include "dynarray.h"
#include "path.h"
#include "nodeFT.h"
#include "ft.h"

/*
  A File Tree is a representation of a hierarchy of directories,
  represented as an AO with 3 state variables:
*/

/* 1. a flag for being in an initialized state (TRUE) or not (FALSE) */
static boolean bIsInitialized;
/* 2. a pointer to the root node in the hierarchy, which must be a 
directory or NULL*/
static Node_T oNRoot;
/* 3. a counter of the number of nodes in the hierarchy */
static size_t ulCount;

/* ------------------------------------------------------------------ */

/* The FT_traversePath and FT_findNode functions modularize the common
functionality of going as far as possible down a FT towards a path
and returning either the node of however far was reached or the
node if the full path was reached, respectively.
*/

/*
  Traverses the FT starting at the root as far as possible towards
  absolute path oPPath. If able to traverse, returns an int SUCCESS
  status and sets *poNFurthest to the furthest node reached (which may
  be only a prefix of oPPath, or even NULL if the root is NULL).
  Otherwise, sets *poNFurthest to NULL and returns with status:
  * CONFLICTING_PATH if the root's path is not a prefix of oPPath
  * MEMORY_ERROR if memory could not be allocated to complete request
  * BAD_PATH if a oPPath is not a well-formatted path
*/
static int FT_traversePath(Path_T oPPath, Node_T *poNFurthest) {
    int iStatus;
    Path_T oPPrefix = NULL;
    Node_T oNCurr;
    Node_T oNChild = NULL;
    size_t ulDepth;
    size_t i;
    size_t ulChildID = 0;

    assert(oPPath != NULL);
    assert(poNFurthest != NULL);

    /* root is NULL -> won't find anything */
    if(oNRoot == NULL) {
        *poNFurthest = NULL;
        return SUCCESS;
    }

    /* make sure root of provided path is the same as FT's root*/
    iStatus = Path_prefix(oPPath, 1, &oPPrefix);
    if(iStatus != SUCCESS) {
        *poNFurthest = NULL;
        return iStatus;
    }

    /* make sure path doesn't conflict with one already in the FT */
    if(Path_comparePath(Node_getPath(oNRoot), oPPrefix)) {
        Path_free(oPPrefix);
        *poNFurthest = NULL;
        return CONFLICTING_PATH;
    }
    Path_free(oPPrefix);
    oPPrefix = NULL;

    oNCurr = oNRoot;
    ulDepth = Path_getDepth(oPPath);

    /* iterate down the path */
    for(i = 2; i <= ulDepth; i++) {
        iStatus = Path_prefix(oPPath, i, &oPPrefix);
        if(iStatus != SUCCESS) {
            *poNFurthest = NULL;
            return iStatus;
        }
        if(Node_hasChild(oNCurr, oPPrefix, &ulChildID)) {
            /* go to that child and continue with next prefix */
            Path_free(oPPrefix);
            oPPrefix = NULL;
            iStatus = Node_getChild(oNCurr, ulChildID, &oNChild);
            if(iStatus != SUCCESS) {
                *poNFurthest = NULL;
                return iStatus;
            }

            oNCurr = oNChild;
        }
        else {
            /* oNCurr doesn't have child with path oPPrefix: this is as
            far as we can go */

            /* check if path argument has a file ancestor */
            if((i != ulDepth) && (Node_getType(oNCurr) == IS_FILE)) {
                Path_free(oPPrefix);
                *poNFurthest = NULL;
                return BAD_PATH;
            }

            break;
        }
    }

    Path_free(oPPrefix);
    *poNFurthest = oNCurr;
    return SUCCESS;
}

/* ------------------------------------------------------------------ */

/*
  Traverses the FT to find a node with absolute path pcPath. Returns a
  int SUCCESS status and sets *poNResult to be the node, if found.
  Otherwise, sets *poNResult to NULL and returns with status:
  * INITIALIZATION_ERROR if the FT is not in an initialized state
  * BAD_PATH if pcPath does not represent a well-formatted path
  * CONFLICTING_PATH if the root's path is not a prefix of pcPath
  * NO_SUCH_PATH if no node with pcPath exists in the hierarchy
  * MEMORY_ERROR if memory could not be allocated to complete request
 */
static int FT_findNode(const char *pcPath, Node_T *poNResult) {
    Path_T oPPath = NULL;
    Node_T oNFound = NULL;
    int iStatus;

    assert(pcPath != NULL);
    assert(poNResult != NULL);

    /* check if initialized*/
    if(!bIsInitialized) {
        *poNResult = NULL;
        return INITIALIZATION_ERROR;
    }

    /* create path */
    iStatus = Path_new(pcPath, &oPPath);
    if(iStatus != SUCCESS) {
        *poNResult = NULL;
        return iStatus;
    }

    /*traverse path */
    iStatus = FT_traversePath(oPPath, &oNFound);
    if(iStatus != SUCCESS)
    {
        Path_free(oPPath);
        *poNResult = NULL;
        return iStatus;
    }

    /* node not in pcPath */
    if(oNFound == NULL) {
        Path_free(oPPath);
        *poNResult = NULL;
        return NO_SUCH_PATH;
    }

    /* node in tree but has diffent path*/ 
    if(Path_comparePath(Node_getPath(oNFound), oPPath) != 0) {
        Path_free(oPPath);
        *poNResult = NULL;
        return NO_SUCH_PATH;
    }

    Path_free(oPPath);
    *poNResult = oNFound;
    return SUCCESS;
}

/* --------------------------------------------------------------------

  The following auxiliary functions are used for generating the
  string representation of the FT.
*/

/*
  Performs a pre-order traversal of the tree rooted at oNNode,
  inserting each payload to DynArray_T oDArray beginning at index
  ulIndex with files before directories in lexicographic order. Returns 
  the next unused index in oDArray after the insertion(s). */
static size_t FT_preOrderTraversal(Node_T oNNode,
                                   DynArray_T oDArray, size_t ulIndex) {
    size_t count;
    DynArray_T oDSorted;

    assert(oDArray != NULL);

    if(oNNode != NULL) {
        size_t ulChildren = 0;
        
        /* set first free index to be given node */
        (void) DynArray_set(oDArray, ulIndex, oNNode);
        ulIndex++;

        /* base case - return the index if node is file (leaf node) */
        if (Node_getNumChildren(oNNode, &ulChildren) == NOT_A_DIRECTORY)
        {
            return ulIndex;
        }
        
        oDSorted = DynArray_new(0);
        
        /* sort children of node into lexicographic order, files first*/
        for(count = 0; count < ulChildren; count++) {
            int iStatus;

            Node_T oNChild = NULL;
            iStatus = Node_getChild(oNNode, count, &oNChild);

            assert(iStatus == SUCCESS);

            if(Node_getType(oNChild) == IS_FILE)
                DynArray_add(oDSorted, oNChild);
        }

        /* add directories on the same level after the files */
        for(count = 0; count < ulChildren; count++) {
            int iStatus;

            Node_T oNChild = NULL;
            iStatus = Node_getChild(oNNode, count, &oNChild);

            assert(iStatus == SUCCESS);

            if(Node_getType(oNChild) == IS_DIRECTORY)
                DynArray_add(oDSorted, oNChild);
        }

        /* recur */
        for(count = 0; count < ulChildren; count++) {
            Node_T oNChild = NULL;
            oNChild = DynArray_get(oDSorted, count);

            ulIndex = FT_preOrderTraversal(oNChild, oDArray, ulIndex);
        }

        DynArray_free(oDSorted);
    }

    return ulIndex;
}

/*
  Alternate version of strlen that uses pulAcc as an in-out parameter
  to accumulate a string length, rather than returning the length of
  oNNode's path, and also always adds one addition byte to the sum.
*/
static void FT_strlenAccumulate(Node_T oNNode, size_t *pulAcc) {
    assert(pulAcc != NULL);

    if(oNNode != NULL)
        *pulAcc += (Path_getStrLength(Node_getPath(oNNode)) + 1);
}

/*
  Alternate version of strcat that inverts the typical argument
  order, appending oNNode's path onto pcAcc, and also always adds one
  newline at the end of the concatenated string.
*/
static void FT_strcatAccumulate(Node_T oNNode, char *pcAcc) {
    assert(pcAcc != NULL);

    if(oNNode != NULL) {
        strcat(pcAcc, Path_getPathname(Node_getPath(oNNode)));
        strcat(pcAcc, "\n");
    }
}

/* ------------------------------------------------------------------ */

int FT_insertDir(const char *pcPath){
    int iStatus;
    Path_T oPPath = NULL;
    Node_T oNFirstNew = NULL;
    Node_T oNCurr = NULL;
    size_t ulDepth, ulIndex;
    size_t ulNewNodes = 0;

    assert(pcPath != NULL);
  
    /* validate pcPath and generate a Path_T for it */
    if(!bIsInitialized)
        return INITIALIZATION_ERROR;

    iStatus = Path_new(pcPath, &oPPath);
    if(iStatus != SUCCESS)
        return iStatus;

    /* find the closest ancestor of oPPath already in the tree */
    iStatus = FT_traversePath(oPPath, &oNCurr);
    if(iStatus != SUCCESS)
    {
        Path_free(oPPath);
        return iStatus;
    }

    /* no ancestor node found, so if root is not NULL, pcPath isn't 
    underneath root. */
    if(oNCurr == NULL && oNRoot != NULL) {
        Path_free(oPPath);
        return CONFLICTING_PATH;
    }
    
    ulDepth = Path_getDepth(oPPath);
    if(oNCurr == NULL) /* new root! */
        ulIndex = 1;
    else {
        ulIndex = Path_getDepth(Node_getPath(oNCurr))+1;

        /* oNCurr is the node we're trying to insert */
        if(ulIndex == ulDepth+1 && !Path_comparePath(oPPath,
                                        Node_getPath(oNCurr))) {
            Path_free(oPPath);
            return ALREADY_IN_TREE;
        }
    }

    /* starting at oNCurr, build rest of the path one level at a time */
    while(ulIndex <= ulDepth) {
        Path_T oPPrefix = NULL;
        Node_T oNNewNode = NULL;

        /* generate a Path_T for this level */
        iStatus = Path_prefix(oPPath, ulIndex, &oPPrefix);
        if(iStatus != SUCCESS) {
            Path_free(oPPath);
            if(oNFirstNew != NULL)
                (void) Node_free(oNFirstNew);
            return iStatus;
        }

        /* insert the new directory type node for this level */
        iStatus = Node_new(oPPrefix, IS_DIRECTORY, oNCurr, &oNNewNode);
        if(iStatus != SUCCESS) {
            Path_free(oPPath);
            Path_free(oPPrefix);
            if(oNFirstNew != NULL)
                (void) Node_free(oNFirstNew);
            return iStatus;
        }

        /* set up for next level */
        Path_free(oPPrefix);
        oNCurr = oNNewNode;
        ulNewNodes++;
        if(oNFirstNew == NULL)
            oNFirstNew = oNCurr;
        ulIndex++;

    }

    Path_free(oPPath);
    /* update FT state variables to reflect insertion */
    if(oNRoot == NULL)
        oNRoot = oNFirstNew;
    ulCount += ulNewNodes;

    return SUCCESS;
}

/* ------------------------------------------------------------------ */

boolean FT_containsDir(const char *pcPath){

    int iStatus;
    Node_T oNFound = NULL;

    assert(pcPath != NULL);

    /* no possible path if no root */
    if (oNRoot == NULL)
        return FALSE;

    iStatus = FT_findNode(pcPath, &oNFound);
    if (iStatus != SUCCESS)
        return FALSE;

    /* file, not directory */
    if (Node_getType(oNFound) != IS_DIRECTORY) {
        return FALSE;
    }
    return (boolean) (iStatus == SUCCESS);
}

/* ------------------------------------------------------------------ */

int FT_rmDir(const char *pcPath){
    int iStatus;
    Node_T oNFound = NULL;

    assert(pcPath != NULL);
   
    iStatus = FT_findNode(pcPath, &oNFound);

    
    if (iStatus != SUCCESS)
        return iStatus;
    
    /* file, not directory */
    if (Node_getType(oNFound) == IS_FILE)
        return NOT_A_DIRECTORY;

    /* free subtree from the directory */
    ulCount -= Node_free(oNFound);
    if(ulCount == 0)
        oNRoot = NULL;

    return SUCCESS;
}

/* ------------------------------------------------------------------ */
int FT_insertFile(const char *pcPath, void *pvContents, size_t ulLength) {
    int iStatus;
    Path_T oPPath = NULL;
    Node_T oNFirstNew = NULL;
    Node_T oNCurr = NULL;
    size_t ulDepth, ulIndex;
    size_t ulNewNodes = 0;

    assert(pcPath != NULL);
  
    /* validate pcPath and generate a Path_T for it */
    if(!bIsInitialized)
        return INITIALIZATION_ERROR;

    iStatus = Path_new(pcPath, &oPPath);
    if(iStatus != SUCCESS)
        return iStatus;

    /* find the closest ancestor of oPPath already in the tree */
    iStatus = FT_traversePath(oPPath, &oNCurr);
    if(iStatus != SUCCESS) {
        Path_free(oPPath);
        return iStatus;
    }

    /* no ancestor node found, so if root is not NULL, pcPath isn't 
    underneath root. */
    if(oNCurr == NULL && oNRoot != NULL) {
        Path_free(oPPath);
        return CONFLICTING_PATH;
    }
    
    ulDepth = Path_getDepth(oPPath);
    if(oNCurr == NULL) { /* attempts to insert file as root */
        ulIndex = 1;
        Path_free(oPPath);
        return CONFLICTING_PATH;
    }
    else { /* insert file as non-root */
        ulIndex = Path_getDepth(Node_getPath(oNCurr))+1;

        /* oNCurr is the node we're trying to insert */
        if(ulIndex == ulDepth+1 && !Path_comparePath(oPPath,
                                        Node_getPath(oNCurr))) {
            Path_free(oPPath);
            return ALREADY_IN_TREE;
        }
    }

    /* starting at oNCurr, build rest of the path one level at a time */
    while(ulIndex <= ulDepth) {
        Path_T oPPrefix = NULL;
        Node_T oNNewNode = NULL;

        /* generate a Path_T for this level */
        iStatus = Path_prefix(oPPath, ulIndex, &oPPrefix);
        if(iStatus != SUCCESS) {
            Path_free(oPPath);
            if(oNFirstNew != NULL)
                (void) Node_free(oNFirstNew);
            return iStatus;
        }

        /* insert the new directory type node for this level */
        if (ulIndex < ulDepth)
            iStatus = Node_new(oPPrefix, IS_DIRECTORY, oNCurr, &oNNewNode);
        else
            iStatus = Node_new(oPPrefix, IS_FILE, oNCurr, &oNNewNode);
        
        if(iStatus != SUCCESS) {
            Path_free(oPPath);
            Path_free(oPPrefix);
            if(oNFirstNew != NULL)
                (void) Node_free(oNFirstNew);
            return iStatus;
        }

        /* Handle file contents: assign empty content if NULL */
        if (Node_getType(oNNewNode) == IS_FILE) {
            if (pvContents == NULL) {
                /* Treat NULL content as an empty file */
                pvContents = malloc(1);  // Allocate a single byte
                if (pvContents == NULL) {
                    Path_free(oPPath);
                    Path_free(oPPrefix);
                    if(oNFirstNew != NULL)
                        (void) Node_free(oNFirstNew);
                    return MEMORY_ERROR;
                }
                *(char *)pvContents = '\0';  // Set to empty
                ulLength = 0;
            }

            /* Insert the file contents */
            iStatus = Node_insertFileContents(oNNewNode, pvContents, ulLength);
            if (iStatus != SUCCESS) {
                Path_free(oPPath);
                Path_free(oPPrefix);
                if(oNFirstNew != NULL)
                    (void) Node_free(oNFirstNew);
                return iStatus;
            }
        }

        /* set up for next level */
        Path_free(oPPrefix);
        oNCurr = oNNewNode;
        ulNewNodes++;
        if(oNFirstNew == NULL)
            oNFirstNew = oNCurr;
        ulIndex++;
    }

    Path_free(oPPath);
    /* update FT state variables to reflect insertion */
    if(oNRoot == NULL)
        oNRoot = oNFirstNew;
    ulCount += ulNewNodes;
    
    return SUCCESS;
}
/* ------------------------------------------------------------------ */

boolean FT_containsFile(const char *pcPath){
    int iStatus;
    Node_T oNFound = NULL;

    assert(pcPath != NULL);

    if (oNRoot == NULL)
        return FALSE;

    /* search for file node in the FT */
    iStatus = FT_findNode(pcPath, &oNFound);

    if (iStatus != SUCCESS)
        return FALSE;
    /* directory, not file */ 
    if (Node_getType(oNFound) != IS_FILE) {
        return FALSE;
    }
    return (boolean) (iStatus == SUCCESS);
}

/* ------------------------------------------------------------------ */

int FT_rmFile(const char *pcPath){
    int iStatus;
    Node_T oNFound = NULL;

    assert(pcPath != NULL);
   
    /* search for file node */
    iStatus = FT_findNode(pcPath, &oNFound);

    if (iStatus != SUCCESS)
        return iStatus;
    
    /* directory, not file */
    if (Node_getType(oNFound) == IS_DIRECTORY)
        return NOT_A_FILE;

    ulCount -= Node_free(oNFound);
    if(ulCount == 0)
        oNRoot = NULL;

    return SUCCESS;
    
}

/* ------------------------------------------------------------------ */

void *FT_getFileContents(const char *pcPath){
    int iStatus;
    Node_T oNFound = NULL;

    assert(pcPath != NULL);

    /* find node in the FT */
    iStatus = FT_findNode(pcPath, &oNFound);
    if (iStatus != SUCCESS) {
        return NULL;
    }

    return Node_getContents(oNFound);
}

/* ------------------------------------------------------------------ */

void *FT_replaceFileContents(const char *pcPath, void *pvNewContents,
                             size_t ulNewLength){
    int iStatus;
    Node_T oNFound = NULL;
    void *pvOldContents;

    assert(pcPath != NULL);
    assert (pvNewContents != NULL);

    /* search for the node in the FT */ 
    iStatus = FT_findNode(pcPath, &oNFound);
    if (iStatus != SUCCESS) {
        return NULL;
    }

    /* store old contents to return */ 
    pvOldContents = Node_getContents(oNFound);
    iStatus = Node_insertFileContents(oNFound, pvNewContents, 
    ulNewLength);

    if (iStatus == SUCCESS)
        return pvOldContents;
    return NULL;
}

/* ------------------------------------------------------------------ */

int FT_stat(const char *pcPath, boolean *pbIsFile, size_t *pulSize){
    Node_T oNFound = NULL;
    int iStatus;
    nodeType type;

    assert(pcPath != NULL);
    assert(pbIsFile != NULL);
    assert (pulSize != NULL);
    
    /* search for node */
    iStatus = FT_findNode(pcPath, &oNFound);
    if (iStatus != SUCCESS) {
        return iStatus;
    }

    /* store node type, set type flags */ 
    type = Node_getType(oNFound);
    if (type == IS_DIRECTORY){
        *pbIsFile = FALSE;
    }
    else{
        *pbIsFile = TRUE;
        *pulSize = Node_getSize(oNFound);
    }

    return SUCCESS;
}

/* ------------------------------------------------------------------ */

int FT_init(void){
    if(bIsInitialized)
        return INITIALIZATION_ERROR;

    bIsInitialized = TRUE;
    oNRoot = NULL;
    ulCount = 0;

    return SUCCESS;
}

/* ------------------------------------------------------------------ */

int FT_destroy(void){
    if(!bIsInitialized)
        return INITIALIZATION_ERROR;

    /* if FT has components, free them */
    if(oNRoot) {
        ulCount -= Node_free(oNRoot);
        oNRoot = NULL;
    }

    bIsInitialized = FALSE;

    return SUCCESS;
}

/*--------------------------------------------------------------------*/

char *FT_toString(void){
    DynArray_T nodes;
    size_t totalStrlen = 1;
    char *result = NULL;
    
    if(!bIsInitialized)
      return NULL;

    /* add nodes to dyn array in specified order */
    nodes = DynArray_new(ulCount);
    (void) FT_preOrderTraversal(oNRoot, nodes, 0);

    DynArray_map(nodes, (void (*)(void *, void*)) FT_strlenAccumulate,
                (void*) &totalStrlen);

    /* allocate mem*/
    result = malloc(totalStrlen);
    if(result == NULL) {
        DynArray_free(nodes);
        return NULL;
    }
    *result = '\0';

    /* apply cat accumulate function to array*/
    DynArray_map(nodes, (void (*)(void *, void*)) FT_strcatAccumulate,
                    (void *) result);

    DynArray_free(nodes);

    return result;
}