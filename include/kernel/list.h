#ifndef _LIST_H_
#define _LIST_H_

#include <kernel/types.h>
#include <lib/stddef.h>

struct list_node {
    struct list_node* prev;
    struct list_node* next;
};

struct list {
    struct list_node header;
    int magic;
};

typedef struct list_node Node;
typedef struct list List;

/**
 * list_entry - get the struct for this entry
 * @node:	the Node* of this entry.
 * @struct:	the type of the struct this is embedded in.
 * @member:	the name of the Node within the struct.
 */
#define list_entry(node, struct, member) \
	retrieve_struct(node, struct, member)

/*
 * Perform some operation for a node in the list. Return 0 on success.
 * aux can be used as accumulator or anything to pass back information to the caller.
 */
typedef err_t node_op(Node *node, void *aux);

/*
 * Define comparator function between two nodes.
 * Return value > 0 when a > b, value == 0 when a = b, value < 0 when a < b
 */
typedef int comparator(const Node *a, const Node *b, void *aux);

/*
 * Initialize a circular linked list with aux as additional information for the list.
 * List provided by the caller.
 */
void list_init(List* list);

/* Return 1 if list is empty, 0 otherwise */
int list_empty(List* list);

/* Append node at the end of a list. */
void list_append(List* list, Node* node);

/* Append node behind the largest smaller node according to comparator func. */
void list_append_ordered(List *list, Node *node, comparator *compare, void *aux);

/*
 * Remove the given node from its list. Returns the next node. NOTE: this
 * doesn't prevent one from removing the head node.
 */
Node* list_remove(Node* node);

/*
 * Function to help with list traversal. Returns the first node of the list.
 * for (Node *n = list_begin(list); n != list_end(list); n = list_next(n)) { ... }
 * If list removal occurs while traversing the list, make sure to set n = list_remove_node(n)
 */
Node* list_begin(List* list);

/* Function to help with list traversal. */
Node* list_end(List* list);

/* Return the next node of the given node */
Node* list_next(Node* n);

/* Return the prev node of the the given node */
Node* list_prev(Node* n);

/*
 * Do an operation on each node of the list. If any operation fails, return failed op return value.
 * Otherwise return 0. aux is passed down to op, can be null if not using it.
 */
err_t list_foreach_do(List* list, node_op op, void* aux);


#endif /* _LIST_H_ */
