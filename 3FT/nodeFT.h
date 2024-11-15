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
  Constructs a new node with the specified path `path`, parent `parent`,
  and type determined by `isFile` (TRUE for files, FALSE for directories).

  Parameters:
    - path: the path associated with the new node
    - parent: the parent node under which the new node will be added
    - isFile: boolean indicating if the new node is a file (TRUE) or directory (FALSE)
    - resultNode: pointer to where the newly created node will be stored

  Returns:
    - SUCCESS on successful creation
    - MEMORY_ERROR if memory allocation fails
    - CONFLICTING_PATH if `parent`'s path is not a prefix of `path`
    - NO_SUCH_PATH if `path` is invalid, or `parent` is NULL but `path` is not root,
                   or `parent`'s path is not the immediate parent of `path`
    - ALREADY_IN_TREE if a child with `path` already exists under `parent`

  On success, sets `*resultNode` to the new node.
  On failure, sets `*resultNode` to NULL.
*/
int NodeFT_new(Path_T path, Node_T parent, boolean isFile, Node_T *resultNode);

/*
  Recursively frees the subtree rooted at `node`, including `node` itself.

  Parameters:
    - node: the root node of the subtree to free

  Returns:
    - The total number of nodes freed.
*/
size_t NodeFT_free(Node_T node);

/* 
  Returns the path object representing `node`'s absolute path.

  Parameters:
    - node: the node whose path is to be retrieved

  Returns:
    - The Path_T object representing the node's path
*/
Path_T NodeFT_getPath(Node_T node);

/*
  Checks if `parent` has a child node with path `childPath` and type specified by `isFile`.

  Parameters:
    - parent: the parent node to search within
    - childPath: the path of the child node to search for
    - childIndexPtr: pointer to where the index will be stored
    - isFile: boolean indicating the type of child (TRUE for file, FALSE for directory)

  Returns:
    - TRUE if such a child exists
    - FALSE otherwise

  If the child exists, stores its index in `*childIndexPtr`.
  Otherwise, stores the index where such a child would be inserted.
*/
boolean NodeFT_hasChild(Node_T parent, Path_T childPath, size_t *childIndexPtr, boolean isFile);

/* 
  Returns the number of children of `parent` of type specified by `isFile`.

  Parameters:
    - parent: the parent node whose children are to be counted
    - isFile: boolean indicating the type of children to count (TRUE for files, FALSE for directories)

  Returns:
    - The number of children of the specified type
*/
size_t NodeFT_getNumChildren(Node_T parent, boolean isFile);

/*
  Retrieves the child node of `parent` at index `childID` of type specified by `isFile`.

  Parameters:
    - parent: the parent node from which to retrieve the child
    - childID: the index of the child within the child array
    - resultNode: pointer to where the retrieved child node will be stored
    - isFile: boolean indicating the type of child to retrieve (TRUE for file, FALSE for directory)

  Returns:
    - SUCCESS on successful retrieval
    - NO_SUCH_PATH if `childID` is out of bounds

  On success, sets `*resultNode` to the retrieved child node.
  On failure, sets `*resultNode` to NULL.
*/
int NodeFT_getChild(Node_T parent, size_t childID, Node_T *resultNode, boolean isFile);

/*
  If `node` is a file node, stores its contents in `*contentsPtr` and returns SUCCESS.
  On failure, sets `*contentsPtr` to NULL and returns:
    - NO_SUCH_PATH if `node` is NULL

  Parameters:
    - node: the file node whose contents are to be retrieved
    - contentsPtr: pointer to where the contents will be stored

  Returns:
    - SUCCESS on successful retrieval
    - NO_SUCH_PATH if `node` is NULL
*/
int NodeFT_getContents(Node_T node, void **contentsPtr);

/*
  If `node` is a file node, stores the length of its contents in `*lengthPtr` and returns SUCCESS.
  On failure, sets `*lengthPtr` to 0 and returns:
    - NO_SUCH_PATH if `node` is NULL

  Parameters:
    - node: the file node whose content length is to be retrieved
    - lengthPtr: pointer to where the content length will be stored

  Returns:
    - SUCCESS on successful retrieval
    - NO_SUCH_PATH if `node` is NULL
*/
int NodeFT_getContentLength(Node_T node, size_t *lengthPtr);

/*
  Sets the contents of file node `node` to `newContents` of length `newLength`.

  Parameters:
    - node: the file node whose contents are to be set
    - newContents: pointer to the new contents
    - newLength: the length of the new contents

  Returns:
    - SUCCESS on successful update
    - MEMORY_ERROR if memory allocation fails
*/
int NodeFT_setContents(Node_T node, void *newContents, size_t newLength);

/*
  Returns TRUE if `node` is a file, FALSE if it is a directory.

  Parameters:
    - node: the node to check

  Returns:
    - TRUE if `node` is a file
    - FALSE if `node` is a directory or NULL
*/
boolean NodeFT_isFile(Node_T node);

/*
  Returns the parent of `node`. If `node` is the root node, returns NULL.

  Parameters:
    - node: the node whose parent is to be retrieved

  Returns:
    - The parent node if it exists
    - NULL if `node` is the root node
*/
Node_T NodeFT_getParent(Node_T node);

/*
  Generates a string representation of `node`.

  Parameters:
    - node: the node to represent as a string

  Returns:
    - A dynamically allocated string representing the node
      (e.g., "File: /path/to/file" or "Dir:  /path/to/dir")
    - NULL if memory allocation fails

  The caller is responsible for freeing the allocated memory.
*/
char *NodeFT_toString(Node_T node);

#endif