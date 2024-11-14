/*--------------------------------------------------------------------*/
/* checkerFT.h                                                        */
/* Author: anish                                                      */
/*--------------------------------------------------------------------*/

#ifndef CHECKERFT_INCLUDED
#define CHECKERFT_INCLUDED

#include "nodeFT.h"

/*
    Validates a single node in the File Tree.
    Returns TRUE if the node `node` is in a valid state, FALSE otherwise.
    If invalid, an error message is printed to stderr.
*/
boolean CheckerFT_Node_isValid(Node_T node);

/*
    Validates the entire File Tree hierarchy.
    Returns TRUE if the File Tree is valid, FALSE otherwise.
    If invalid, an error message is printed to stderr.
    The validation checks are based on:
    - `isInitialized`: whether the File Tree has been initialized
    - `root`: the root node of the hierarchy
    - `count`: the total number of nodes in the hierarchy
*/
boolean CheckerFT_isValid(boolean isInitialized,
                          Node_T root,
                          size_t count);

#endif