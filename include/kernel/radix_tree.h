#ifndef _RADIX_TREE_H_
#define _RADIX_TREE_H_

#include <kernel/types.h>

/*
 * A radix tree implementation
 */

struct radix_tree_root;
struct radix_tree_node;

/*
 * Errors
 */
#define ERR_RADIX_TREE_ALLOC 1 // allocation error
#define ERR_RADIX_TREE_NODE_EXIST 2 // node already exist

#define RADIX_TREE_WIDTH_POWER 6 // width is always power of 2
#define RADIX_TREE_WIDTH (1 << RADIX_TREE_WIDTH_POWER)

struct radix_tree_root {
    int height;
    struct radix_tree_node *root_node;
};

struct radix_tree_node {
    int count;
    struct radix_tree_node *parent;
    void *slots[RADIX_TREE_WIDTH];
};

/*
 * Constructor for a radix tree.
 */
void radix_tree_construct(struct radix_tree_root *root);

/*
 * Destructor for a radix tree.
 */
void radix_tree_destroy(struct radix_tree_root *root);

/*
 * Return True if the radix tree is empty.
 */
int radix_tree_empty(struct radix_tree_root *root);

/*
 * Search and return a leaf node with an index. Return NULL if node not present.
 */
void *radix_tree_lookup(struct radix_tree_root *root, int index);

/*
 * Insert a new leaf node into a tree. Return the following errors:
 * ERR_RADIX_TREE_ALLOC if failed to allocate node
 * ERR_RADIX_TREE_NODE_EXIST if leaf node already exist
 */
err_t radix_tree_insert(struct radix_tree_root *root, int index, void *leaf);

/*
 * Remove a leaf node from a tree and return the leaf node (if present). Return
 * NULL if leaf node is not found.
 */
void *radix_tree_remove(struct radix_tree_root *root, int index);

#endif /* _RADIX_TREE_H_ */
