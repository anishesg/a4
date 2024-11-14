/*--------------------------------------------------------------------*/
/* nodeFT.h                                                           */
/* Author: [Your Name]                                                */
/*--------------------------------------------------------------------*/

#ifndef NODEFT_INCLUDED
#define NODEFT_INCLUDED

#include <stddef.h>
#include "a4def.h"
#include "path.h"
#include "dynarray.h"

/* Enumeration for node types */
typedef enum { IS_FILE, IS_DIRECTORY } nodeType;

/* A Node_T is a node in the File Tree */
typedef struct node *Node_T;

/*
  Creates a new node in the File Tree with path oPPath, type 'type',
  and parent oNParent. Returns SUCCESS if the new node is created
  successfully and sets *poNResult to the new node.
  Otherwise, sets *poNResult to NULL and returns:
  * MEMORY_ERROR if memory could not be allocated
  * CONFLICTING_PATH if oNParent's path is not an ancestor of oPPath
  * NO_SUCH_PATH if oPPath is of depth 0
                 or oNParent's path is not oPPath's direct parent
                 or oNParent is NULL but oPPath is not of depth 1
  * ALREADY_IN_TREE if oNParent already has a child with this path
*/
int Node_new(Path_T oPPath, nodeType type, Node_T oNParent, Node_T *poNResult);

/*
  Destroys and frees all memory allocated for the subtree rooted at
  oNNode, i.e., deletes this node and all its descendants. Returns the
  number of nodes deleted.
*/
size_t Node_free(Node_T oNNode);

/* Returns the path object representing oNNode's absolute path. */
Path_T Node_getPath(Node_T oNNode);

/* Returns the type of oNNode, either IS_FILE or IS_DIRECTORY */
nodeType Node_getType(Node_T oNNode);

/*
  Sets the contents of a file node.
  Returns SUCCESS if successful, or NOT_A_FILE if the node is not a file.
*/
int Node_insertFileContents(Node_T oNNode, void *pvContents, size_t ulLength);

/* Returns the contents of a file node, or NULL if not a file */
void *Node_getContents(Node_T oNNode);

/* Returns the size of the contents of a file node */
size_t Node_getSize(Node_T oNNode);

/*
  Returns SUCCESS and sets *pulNum to the number of children
  of oNParent if oNParent is a directory.
  Returns NOT_A_DIRECTORY if oNParent is not a directory.
*/
int Node_getNumChildren(Node_T oNParent, size_t *pulNum);

/*
  Returns SUCCESS and sets *poNResult to be the child node
  of oNParent with identifier ulChildID, if one exists.
  Otherwise, sets *poNResult to NULL and returns status:
  * NO_SUCH_PATH if ulChildID is not a valid child for oNParent
*/
int Node_getChild(Node_T oNParent, size_t ulChildID, Node_T *poNResult);

/*
  Returns TRUE if oNParent has a child with path oPPath.
  If found, stores in *pulChildID the child's identifier.
  Returns FALSE if it does not.
*/
boolean Node_hasChild(Node_T oNParent, Path_T oPPath, size_t *pulChildID);

/*
  Returns the parent node of oNNode.
  Returns NULL if oNNode is the root and thus has no parent.
*/
Node_T Node_getParent(Node_T oNNode);

#endif /* NODEFT_INCLUDED */