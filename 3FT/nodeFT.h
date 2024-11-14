/*--------------------------------------------------------------------*/
/* nodeFT.h                                                           */
/* Author: Mirabelle Weinbach and John Wallace                        */
/*--------------------------------------------------------------------*/

#ifndef NODEFT_INCLUDED
#define NODEFT_INCLUDED

#include <stddef.h>
#include "a4def.h"
#include "path.h"

/* Enum to specify the type of node, either directory or file */
typedef enum {
    IS_DIRECTORY,
    IS_FILE
} nodeType;

/* A Node_T is a node in a File Tree, which can be either a file or a directory */
typedef struct node *Node_T;

/*
  Creates a new node in the File Tree with a specified type (file or directory),
  at the given path oPPath, and a parent node oNParent. On success, sets
  *poNResult to the new node and returns SUCCESS. Otherwise, sets *poNResult
  to NULL and returns:
  * MEMORY_ERROR if memory allocation fails,
  * CONFLICTING_PATH if oNParent's path is not an ancestor of oPPath,
  * NO_SUCH_PATH if oPPath is of invalid depth or hierarchy,
  * ALREADY_IN_TREE if oNParent already has a child with oPPath.
*/
int Node_new(Path_T oPPath, nodeType type, Node_T oNParent,
             Node_T *poNResult);

/*
  Frees all memory for the subtree rooted at oNNode (including oNNode itself).
  Returns the count of nodes freed.
*/
size_t Node_free(Node_T oNNode);

/* Retrieves the path object representing oNNode's full path. */
Path_T Node_getPath(Node_T oNNode);

/*
  Checks if oNParent has a child with path oPPath. If found, sets
  *pulChildID to the identifier of this child (as used in Node_getChild)
  and returns TRUE. If not, sets *pulChildID to where it would be inserted
  and returns FALSE.
*/
boolean Node_hasChild(Node_T oNParent, Path_T oPPath,
                      size_t *pulChildID);

/*
  Sets *pulNum to the count of children of oNParent if it is a directory.
  If oNParent is not a directory, returns NOT_A_DIRECTORY.
*/
int Node_getNumChildren(Node_T oNParent, size_t *pulNum);

/*
  Retrieves the child node of oNParent at the specified ulChildID.
  If successful, sets *poNResult to this child and returns SUCCESS.
  If ulChildID is invalid, sets *poNResult to NULL and returns NO_SUCH_PATH.
*/
int Node_getChild(Node_T oNParent, size_t ulChildID,
                  Node_T *poNResult);

/* Returns the parent node of oNNode, or NULL if oNNode is the root node. */
Node_T Node_getParent(Node_T oNNode);

/*
  Compares two nodes, oNFirst and oNSecond, based on their paths
  lexicographically. Returns:
  * <0 if oNFirst is less than oNSecond,
  * 0 if they are equal,
  * >0 if oNFirst is greater than oNSecond.
*/
int Node_compare(Node_T oNFirst, Node_T oNSecond);

/*
  Provides a dynamically-allocated string representation of oNNode's path.
  Caller is responsible for freeing the returned string.
  Returns NULL if allocation fails.
*/
char *Node_toString(Node_T oNNode);

/* Returns the type of oNNode (IS_FILE or IS_DIRECTORY). */
nodeType Node_getType(Node_T oNNode);

/*
  Sets the data contents for oNNode if it is a file node. If oNNode is a file,
  assigns pvContents (of length ulLength) to it and returns SUCCESS.
  If oNNode is a directory, returns BAD_PATH.
*/
int Node_insertFileContents(Node_T oNNode, void *pvContents, size_t ulLength);

/* Returns a pointer to the contents of a file node, or NULL if not a file. */
void *Node_getContents(Node_T oNNode);

/* Returns the size of the contents of a file node in bytes, or 0