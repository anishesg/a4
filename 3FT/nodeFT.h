/*--------------------------------------------------------------------*/
/* nodeFT.h                                                           */
/* Author:  anish                                              */
/*--------------------------------------------------------------------*/

#ifndef NODEFT_INCLUDED
#define NODEFT_INCLUDED

#include <stddef.h>
#include "a4def.h"
#include "path.h"

/*
  Node_T represents a node within a File Tree, which can be a file or a directory.
  Directories can have both file and directory children, stored in separate dynamic arrays.
  Files contain their contents and the length of the contents.
*/
typedef struct node *Node_T;

/*
  Constructs a new node with the specified path `oPPath`, parent `oNParent`,
  and type determined by `bIsFile` (TRUE for files, FALSE for directories).
  On success, stores the new node in `*poNResult` and returns SUCCESS.
  On failure, sets `*poNResult` to NULL and returns:
  * MEMORY_ERROR if memory allocation fails
  * CONFLICTING_PATH if `oNParent`'s path is not a prefix of `oPPath`
  * NO_SUCH_PATH if `oPPath` is invalid, or `oNParent` is NULL but `oPPath` is not root,
                 or `oNParent`'s path is not the immediate parent of `oPPath`
  * ALREADY_IN_TREE if a child with `oPPath` already exists under `oNParent`
*/
int NodeFT_new(Path_T oPPath, Node_T oNParent, boolean bIsFile, Node_T *poNResult);

/*
  Recursively frees the subtree rooted at `oNNode`, including `oNNode` itself.
  Returns the total number of nodes freed.
*/
size_t NodeFT_free(Node_T oNNode);

/* Returns the path object representing `oNNode`'s absolute path. */
Path_T NodeFT_getPath(Node_T oNNode);

/*
  Checks if `oNParent` has a child node with path `oPPath` and type specified by `bIsFile`.
  If such a child exists, stores its index in `*pulChildID` and returns TRUE.
  Otherwise, stores the index where such a child would be inserted in `*pulChildID` and returns FALSE.
*/
boolean NodeFT_hasChild(Node_T oNParent, Path_T oPPath, size_t *pulChildID, boolean bIsFile);

/* Returns the number of children of `oNParent` of type specified by `bIsFile`. */
size_t NodeFT_getNumChildren(Node_T oNParent, boolean bIsFile);

/*
  Retrieves the child node of `oNParent` at index `ulChildID` of type specified by `bIsFile`.
  On success, stores the child in `*poNResult` and returns SUCCESS.
  On failure, sets `*poNResult` to NULL and returns:
  * NO_SUCH_PATH if `ulChildID` is out of bounds
*/
int NodeFT_getChild(Node_T oNParent, size_t ulChildID, Node_T *poNResult, boolean bIsFile);

/*
  If `oNNode` is a file node, stores its contents in `*ppvResult` and returns SUCCESS.
  On failure, sets `*ppvResult` to NULL and returns:
  * NO_SUCH_PATH if `oNNode` is NULL
*/
int NodeFT_getContents(Node_T oNNode, void **ppvResult);

/*
  If `oNNode` is a file node, stores the length of its contents in `*pulLength` and returns SUCCESS.
  On failure, sets `*pulLength` to 0 and returns:
  * NO_SUCH_PATH if `oNNode` is NULL
*/
int NodeFT_getContentLength(Node_T oNNode, size_t *pulLength);

/*
  Sets the contents of file node `oNNode` to `pvContents` of length `ulLength`.
  Returns SUCCESS on success. On failure, returns:
  * MEMORY_ERROR if memory allocation fails
  * NO_SUCH_PATH if `oNNode` is NULL
*/
int NodeFT_setContents(Node_T oNNode, void *pvContents, size_t ulLength);

/*
  Returns TRUE if `oNNode` is a file, FALSE if it is a directory.
*/
boolean NodeFT_isFile(Node_T oNNode);

/*
  Returns the parent of `oNNode`. If `oNNode` is the root node, returns NULL.
*/
Node_T NodeFT_getParent(Node_T oNNode);

/*
  Generates a string representation of `oNNode`.
  The caller is responsible for freeing the allocated memory.
  Returns NULL if memory allocation fails.
*/
char *NodeFT_toString(Node_T oNNode);

#endif