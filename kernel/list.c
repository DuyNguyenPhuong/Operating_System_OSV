#include <kernel/list.h>
#include <kernel/console.h>
#include <lib/errcode.h>

void
list_init(List* list)
{
    kassert(list);
    list->header.prev = &list->header;
    list->header.next = &list->header;
}

int
list_empty(List* list)
{
    kassert(list);
    return (list->header.prev == &list->header)
                && (list->header.next == &list->header);
}

void
list_append(List* list, Node* node)
{
    kassert(list);
    if (list_empty(list)) {
        list->header.next = node; // empty list
    } else {
        list->header.prev->next = node; // non empty list
    }
    node->prev = list->header.prev;
    node->next = &list->header;
    list->header.prev = node; // inserting node to the last element
}

void
list_append_ordered(List *list, Node *node, comparator *compare, void *aux)
{
    Node *n;
    kassert(list && node && compare);

    for (n = list_begin(list); n != list_end(list); n = list_next(n)) {
        // if n > node, insert node in front of n
        if (compare(n, node, aux) > 0) {
            node->prev = n->prev;
            node->next = n;
            n->prev->next = node;
            n->prev = node;
            return;
        }
    }
    // end of the list, just append to the end
    list_append(list, node);
}

Node*
list_remove(Node* node)
{
    kassert(node);
    node->prev->next = node->next;
    node->next->prev = node->prev;
    return node->next;
}

Node*
list_begin(List* list)
{
    kassert(list);
    return list->header.next;
}

Node*
list_end(List* list)
{
    kassert(list);
    return &list->header;
}

Node*
list_next(Node* n)
{
    kassert(n);
    return n->next;
}

Node*
list_prev(Node* n)
{
    kassert(n);
    return n->prev;
}

err_t
list_foreach_do(List* list, node_op op, void* aux)
{
    kassert(list && op);
    Node* node = list_begin(list);
    err_t err;
    while(node != list_end(list)) {
        if ((err = op(node, aux)) != ERR_OK) {
            return err;
        }
        node = list_next(node);
    }
    return ERR_OK;
}
