/*--------------------------------------------------------------------*/
/* nodeFT.h                                                           */
/* Author: anish                                                      */
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
  constructs a new node with the specified path `path`, parent `parent`,
  and type determined by `isFile` (TRUE for files, FALSE for directories).
  on success, stores the new node in `*resultNode` and returns SUCCESS.
  on failure, sets `*resultNode` to NULL and returns:
  * MEMORY_ERROR if memory allocation fails
  * CONFLICTING_PATH if `parent`'s path is not a prefix of `path`
  * NO_SUCH_PATH if `path` is invalid, or `parent` is NULL but `path` is not root,
                 or `parent`'s path is not the immediate parent of `path`
  * ALREADY_IN_TREE if a child with `path` already exists under `parent`
*/
int NodeFT_new(Path_T path, Node_T parent, boolean isFile, Node_T *resultNode);

/*
  recursively frees the subtree rooted at `node`, including `node` itself.
  returns the total number of nodes freed.
*/
size_t NodeFT_free(Node_T node);

/* returns the path object representing `node`'s absolute path. */
Path_T NodeFT_getPath(Node_T node);

/*
  checks if `parent` has a child node with path `childPath` and type specified by `isFile`.
  if such a child exists, stores its index in `*childIndexPtr` and returns TRUE.
  otherwise, stores the index where such a child would be inserted in `*childIndexPtr` and returns FALSE.
*/
boolean NodeFT_hasChild(Node_T parent, Path_T childPath, size_t *childIndexPtr, boolean isFile);

/* returns the number of children of `parent` of type specified by `isFile`. */
size_t NodeFT_getNumChildren(Node_T parent, boolean isFile);

/*
  retrieves the child node of `parent` at index `childID` of type specified by `isFile`.
  on success, stores the child in `*resultNode` and returns SUCCESS.
  on failure, sets `*resultNode` to NULL and returns:
  * NO_SUCH_PATH if `childID` is out of bounds
*/
int NodeFT_getChild(Node_T parent, size_t childID, Node_T *resultNode, boolean isFile);

/*
  if `node` is a file node, stores its contents in `*contentsPtr` and returns SUCCESS.
  on failure, sets `*contentsPtr` to NULL and returns:
  * NO_SUCH_PATH if `node` is NULL
*/
int NodeFT_getContents(Node_T node, void **contentsPtr);

/*
  if `node` is a file node, stores the length of its contents in `*lengthPtr` and returns SUCCESS.
  on failure, sets `*lengthPtr` to 0 and returns:
  * NO_SUCH_PATH if `node` is NULL
*/
int NodeFT_getContentLength(Node_T node, size_t *lengthPtr);

/*
  sets the contents of file node `node` to `newContents` of length `newLength`.
  returns SUCCESS on success. on failure, returns:
  * MEMORY_ERROR if memory allocation fails
  * NO_SUCH_PATH if `node` is NULL
*/
int NodeFT_setContents(Node_T node, void *newContents, size_t newLength);

/*
  returns TRUE if `node` is a file, FALSE if it is a directory.
*/
boolean NodeFT_isFile(Node_T node);

/*
  returns the parent of `node`. if `node` is the root node, returns NULL.
*/
Node_T NodeFT_getParent(Node_T node);

/*
  generates a string representation of `node`.
  the caller is responsible for freeing the allocated memory.
  returns NULL if memory allocation fails.
*/
char *NodeFT_toString(Node_T node);

#endif