#include <kernel/radix_tree.h>
#include <kernel/kmalloc.h>
#include <kernel/console.h>
#include <lib/errcode.h>
#include <lib/string.h>
#include <lib/stddef.h>

static struct kmem_cache *node_allocator = NULL; // Tree node allocator

/*
 * Create a new radix node.
 */
static struct radix_tree_node *radix_tree_node_create(void);

/*
 * Return the max index of a tree. This is the max possible index, not max
 * allocated index.
 */
static inline int radix_tree_max_index(const struct radix_tree_root *root);

/*
 * Given an index and a level (0 as the leaf level), return the index into the
 * node array.
 */
static inline int radix_tree_level_index(int index, int level);

/*
 * Return the index of leaf in its parent node.
 */
#define radix_tree_leaf_index(index) radix_tree_level_index(index, 0)

/*
 * Search and return the parent node of a leaf.
 * alloc: True if allocating non-exist nodes along the search path, False otherwise
 * Return NULL if failed to find parent (alloc = 0) or failed to allocate nodes
 * (alloc = 1)
 */
static struct radix_tree_node *radix_tree_find_parent(struct radix_tree_root *root, int index, int alloc);

/*
 * Add a level to a tree. Return ERR_RADIX_TREE_ALLOC if failed to allocate
 * nodes.
 */
static err_t radix_tree_add_level(struct radix_tree_root *root);

/*
 * Add a child node at an index.
 * is_node: True if child is an internal radix tree node.
 * Return ERR_RADIX_TREE_NODE_EXIST if slot already taken.
 */
static err_t radix_tree_add_child(struct radix_tree_node *node, int index, void *child, int is_node);

static struct radix_tree_node*
radix_tree_node_create(void)
{
    struct radix_tree_node *node;
    if (node_allocator == NULL) {
        if ((node_allocator = kmem_cache_create(sizeof(struct radix_tree_node))) == NULL) {
            return NULL;
        }
    }
    if ((node = kmem_cache_alloc(node_allocator)) != NULL) {
        node->count = 0;
        node->parent = NULL;
        memset(node->slots, 0, RADIX_TREE_WIDTH * sizeof(void*));
    }
    return node;
}

static inline int
radix_tree_max_index(const struct radix_tree_root *root)
{
    if (root->height == 0) {
        return -1;
    }
    return (1 << (RADIX_TREE_WIDTH_POWER * root->height)) - 1;
}

static inline int
radix_tree_level_index(int index, int level)
{
    return (index >> (level * RADIX_TREE_WIDTH_POWER)) & \
        ((1 << RADIX_TREE_WIDTH_POWER) - 1);
}

static struct radix_tree_node*
radix_tree_find_parent(struct radix_tree_root *root, int index, int alloc)
{
    int level, level_index;
    struct radix_tree_node *node, *child;

    for (level = root->height - 1, node = root->root_node; level > 0 && node != NULL; level--, node = child) {
        level_index = radix_tree_level_index(index, level);
        child = (struct radix_tree_node*)node->slots[level_index];
        if (child == NULL && alloc) {
            if ((child = radix_tree_node_create()) == NULL) {
                return NULL;
            }
            radix_tree_add_child(node, level_index, child, True);
        }
    }
    return node;
}

static err_t
radix_tree_add_level(struct radix_tree_root *root)
{
    struct radix_tree_node *node;

    if ((node = radix_tree_node_create()) == NULL) {
        return ERR_RADIX_TREE_ALLOC;
    }
    // Existing root node is always the 0th child node in the new root
    if (root->root_node != NULL) {
        radix_tree_add_child(node, 0, root->root_node, True);
    }
    // Update root node
    root->root_node = node;
    // Update tree height
    root->height += 1;
    return ERR_OK;
}

static err_t
radix_tree_add_child(struct radix_tree_node *node, int index, void *child, int is_node)
{
    kassert(node);
    kassert(child);
    if (node->slots[index] != NULL) {
        return ERR_RADIX_TREE_NODE_EXIST;
    }
    node->slots[index] = child;
    node->count++;
    kassert(node->count <= RADIX_TREE_WIDTH);
    if (is_node) {
        // Create reverse link
        ((struct radix_tree_node*)child)->parent = node;
    }
    return ERR_OK;
}

void
radix_tree_construct(struct radix_tree_root *root)
{
    kassert(root);
    root->height = 0;
    root->root_node = NULL;
}

void
radix_tree_destroy(struct radix_tree_root *root)
{
    kassert(root);
    // XXX: Deallocate all nodes in the tree
    root->root_node = NULL;
    root->height = 0;
}

int
radix_tree_empty(struct radix_tree_root *root)
{
    return root->height == 0;
}

void*
radix_tree_lookup(struct radix_tree_root *root, int index)
{
    struct radix_tree_node *node;
    kassert(root);
    if (index > radix_tree_max_index(root)) {
        return NULL;
    }
    if ((node = radix_tree_find_parent(root, index, False)) == NULL) {
        return NULL;
    }
    return node->slots[radix_tree_leaf_index(index)];
}

err_t
radix_tree_insert(struct radix_tree_root *root, int index, void *leaf)
{
    struct radix_tree_node *node;

    kassert(root);
    kassert(leaf);
    // First make sure the tree has enough levels for the leaf node
    while (index > radix_tree_max_index(root)) {
        if (radix_tree_add_level(root) != ERR_OK) {
            return ERR_RADIX_TREE_ALLOC;
        }
    }
    // Find the parent node and insert the leaf node
    if ((node = radix_tree_find_parent(root, index, True)) == NULL) {
        return ERR_RADIX_TREE_ALLOC;
    }
    if (radix_tree_add_child(node, radix_tree_leaf_index(index), leaf, False) != ERR_OK) {
        return ERR_RADIX_TREE_NODE_EXIST;
    }
    return ERR_OK;
}

void*
radix_tree_remove(struct radix_tree_root *root, int index)
{
    int level, level_index;
    struct radix_tree_node *node, *parent;
    void *leaf;

    kassert(root);
    // Find the leaf node
    if ((node = radix_tree_find_parent(root, index, False)) == NULL) {
        return NULL;
    }
    if ((leaf = node->slots[radix_tree_leaf_index(index)]) == NULL) {
        return NULL;
    }
    // Remove empty nodes in the tree
    for (level = 0; level < root->height; level++, node = parent) {
        level_index = radix_tree_level_index(index, level);
        kassert(node);
        kassert(node->slots[level_index]);
        kassert(node->count > 0);
        node->count -= 1;
        node->slots[level_index] = NULL;
        if (node->count == 0) {
            parent = node->parent;
            kmem_cache_free(node_allocator, node);
        } else {
            return leaf;
        }
    }
    // If we have not returned yet, the root node has been freed. Update root
    // properly
    kassert(node == NULL);
    root->root_node = NULL;
    root->height = 0;
    return leaf;
}
