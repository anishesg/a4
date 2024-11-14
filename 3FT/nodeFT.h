/*--------------------------------------------------------------------*/
/* nodeFT.h                                                             */
/* Author: anish                                               */
/*--------------------------------------------------------------------*/

#ifndef NODE_INCLUDED
#define NODE_INCLUDED

#include <stddef.h>
#include "a4def.h"
#include "path.h"
#include "dynarray.h"

/*
  A Node_T is a node in the File Tree.
*/

typedef struct node *Node_T;

/* Enumeration for node types */
typedef enum { IS_FILE, IS_DIRECTORY } nodeType;

/*
  Creates a new node in the File Tree with path `path`, type `type`,
  and parent `parent`. Returns `SUCCESS` if the new node is created
  successfully and sets `*resultNode` to the new node.
  Otherwise, sets `*resultNode` to `NULL` and returns:
  - `MEMORY_ERROR` if memory could not be allocated
  - `CONFLICTING_PATH` if `parent`'s path is not an ancestor of `path`
  - `NO_SUCH_PATH` if `path` is of depth 0
    or `parent`'s path is not `path`'s direct parent
    or `parent` is `NULL` but `path` is not of depth 1
  - `ALREADY_IN_TREE` if `parent` already has a child with this path
*/
int Node_create(Path_T path, nodeType type, Node_T parent, Node_T *resultNode);

/*
  Destroys and frees all memory allocated for the subtree rooted at
  `node`, i.e., deletes this node and all its descendants. Returns the
  number of nodes deleted.
*/
size_t Node_destroy(Node_T node);

/* Returns the path object representing `node`'s absolute path. */
Path_T Node_getPath(Node_T node);

/* Returns the type of `node`, either `IS_FILE` or `IS_DIRECTORY`. */
nodeType Node_getType(Node_T node);

/*
  Returns the contents of `node` if it is a file.
  Returns `NULL` if `node` is a directory.
*/
void *Node_getContents(Node_T node);

/*
  Sets the contents of `node` to `contents` with size `size`.
  Returns `SUCCESS` if successful.
  Returns `NOT_A_FILE` if `node` is not a file.
*/
int Node_setContents(Node_T node, void *contents, size_t size);

/*
  Returns the size of the contents of `node`.
  Returns 0 if `node` is a directory.
*/
size_t Node_getSize(Node_T node);

/*
  Returns `SUCCESS` and sets `*numChildren` to the number of children
  of `node` if `node` is a directory.
  Returns `NOT_A_DIRECTORY` if `node` is not a directory.
*/
int Node_getNumChildren(Node_T node, size_t *numChildren);

/*
  Returns `SUCCESS` and sets `*childNode` to the child node at `index`
  of `node` if `node` is a directory and `index` is valid.
  Returns `NOT_A_DIRECTORY` if `node` is not a directory.
  Returns `NO_SUCH_PATH` if `index` is out of bounds.
*/
int Node_getChild(Node_T node, size_t index, Node_T *childNode);

/*
  Returns the parent of `node`. Returns `NULL` if `node` is the root.
*/
Node_T Node_getParent(Node_T node);

/*
  Checks if `node` has a child with path `path`.
  If found, sets `*childIndex` to the index of the child.
  Returns `TRUE` if found, `FALSE` otherwise.
*/
boolean Node_hasChild(Node_T node, Path_T path, size_t *childIndex);

#endif /* NODE_INCLUDED */