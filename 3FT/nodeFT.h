/*--------------------------------------------------------------------*/
/* nodeFT.h                                                           */
/* Author: Christopher Moretti                                        */
/*--------------------------------------------------------------------*/
#ifndef NODE_INCLUDED
#define NODE_INCLUDED
#include <stddef.h>
#include "a4def.h"
#include "path.h"

/* A Node_T is a node in a Directory Tree */
typedef struct node *Node_T;
/* Returns the boolean type of oNNode: file(TRUE), directory (FALSE)*/
boolean Node_getType(Node_T oNNode);
/* Returns a pointer to the file contents of oNNode*/
void *Node_getFileContents(Node_T oNNode);
/* Returns the size of contents of oNNode */
size_t Node_getSizeContents(Node_T oNNode);
/* Sets the file contents of oNNode to pvNewContents and returns an int
SUCCESS*/
int Node_setFileContents(Node_T oNNode, void *pvNewContents);
/* Sets the size of contents of oNNode to ulNewLength and returns an
int SUCCESS*/
int Node_setSizeContents(Node_T oNNode, size_t ulNewLength);

/*
  Creates a new node for directory in the Directory Tree, with path   
  oPPath and parent oNParent. Returns an int SUCCESS status and sets 
  *poNResult to be the new node if successful. Otherwise, sets 
  *poNResult to NULL and returns status:
  * MEMORY_ERROR if memory could not be allocated to complete request
  * CONFLICTING_PATH if oNParent's path is not an ancestor of oPPath
  * NO_SUCH_PATH if oPPath is of depth 0
                 or oNParent's path is not oPPath's direct parent
                 or oNParent is NULL but oPPath is not of depth 1
  * ALREADY_IN_TREE if oNParent already has a child with this path
*/
int Node_newDir(Path_T oPPath, Node_T oNParent, Node_T *poNResult);
/*
  Creates a new node for file in the Directory Tree, with path oPPath,
  parent oNParent, contents pvNewContents with size ulNewLength. 
  Returns an int SUCCESS status and sets *poNResult to be the new node
  if successful. Otherwise, sets *poNResult to NULL and returns status:
  * MEMORY_ERROR if memory could not be allocated to complete request
  * CONFLICTING_PATH if oNParent's path is not an ancestor of oPPath
  * NO_SUCH_PATH if oPPath is of depth 0
                 or oNParent's path is not oPPath's direct parent
                 or oNParent is NULL but oPPath is not of depth 1
  * ALREADY_IN_TREE if oNParent already has a child with this path
*/
int Node_newFile(Path_T oPPath, Node_T oNParent, Node_T *poNResult, 
                          void *pvNewContents, size_t ulNewLength);
/*
  Destroys and frees all memory allocated for the subtree rooted at
  oNNode, i.e., deletes this node and all its descendents. Returns the
  number of nodes deleted.
*/
size_t Node_free(Node_T oNNode);
/* Returns the path object representing oNNode's absolute path. */
Path_T Node_getPath(Node_T oNNode);
/*
  Returns TRUE if oNParent has a child with path oPPath. Returns
  FALSE if it does not.
  If oNParent has such a child, stores in *pulChildID the child's
  identifier (as used in Node_getChild). If oNParent does not have
  such a child, stores in *pulChildID the identifier that such a
  child _would_ have if inserted.
*/
boolean Node_hasChild(Node_T oNParent, Path_T oPPath,
                         size_t *pulChildID);
/* Returns the number of children that oNParent has. */
size_t Node_getNumChildren(Node_T oNParent);
/*
  Returns an int SUCCESS status and sets *poNResult to be the child
  node of oNParent with identifier ulChildID, if one exists.
  Otherwise, sets *poNResult to NULL and returns status:
  * NO_SUCH_PATH if ulChildID is not a valid child for oNParent
*/
int Node_getChild(Node_T oNParent, size_t ulChildID,
                  Node_T *poNResult);
/*
  Returns a the parent node of oNNode.
  Returns NULL if oNNode is the root and thus has no parent.
*/
Node_T Node_getParent(Node_T oNNode);

/*
  Returns a string representation for oNNode, or NULL if
  there is an allocation error.
  Allocates memory for the returned string, which is then owned by
  the caller!
*/
char *Node_toString(Node_T oNNode);
#endif