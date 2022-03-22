# B-Plus-Tree

In this project a two field disk file is implemented, based on a BlockFile library, using B+ Tree. In order to manage files Access Method functions are used. These functions support integer, float and string data.

The functions are:
* AM_Init(): Initialization of data used.
* AM_Close(): Free any data used.
* AM_CreateIndex(): Using B+ tree, create a file and it's identifying block
* AM_DestroyIndex(): Remove a file.
* AM_OpenIndex(): Using B+ tree, open an existing file from those we have created.
* AM_CloseIndex(): Using B+ tree, close an open file.
* AM_InsertEntry(): Using B+ tree insert an entry into a file.
* AM_OpenIndexScan(): Given a comparison operator, open a scan file, and find entries which are related using the operator.
* AM_CloseIndexScan():  Close a file's scan, using B+ tree
* AM_FindNextEntry(): For the previous scan, locate the next entry that satisfies the comparison operator.
* AM_PrintError(): Print error messages from the AM functions.

