#include <stdlib.h>


#include "lst.h"


lst_node_t
*lst_node_allocate(lst_t *plst)
{
	lst_node_t *node;
	if ((node = malloc(sizeof(lst_node_t))) == NULL)
		return (NULL);
	node->lst = plst;
	node->data = NULL;
	node->next = NULL;
	return (node);
}

int lst_create(lst_t *plst)
{
	plst->numelements = 0;
	plst->head = NULL;
	return (0);
}

int
lst_is_empty(lst_t *plst)
{
	return (plst->numelements == 0);
}

int lst_insert_head(lst_t *plst, void *ndata)
{
	lst_node_t *nnode;

	if ((nnode = lst_node_allocate(plst)) == NULL)
		return (-1);
	nnode->data = ndata;
	nnode->next = plst->head;
	plst->head = nnode;
	plst->numelements++;
	return (0);
}

int lst_insert_tail(lst_t *plst, void *ndata)
{
	lst_node_t *n;
	lst_node_t *nnode;

	if (lst_is_empty(plst)) {
		return (lst_insert_head(plst, ndata));
	}
	if ((nnode = lst_node_allocate(plst)) == NULL)
		return (-1);

	nnode->data = ndata;
	n = plst->head;
	while(n->next != NULL) {
		n = n->next;
	}
	n->next = nnode;
	plst->numelements++;
	return (0);
}

int lst_remove(lst_t *plst, void *rdata)
{
	lst_node_t *rnode, *prev;

	if (lst_is_empty(plst))
		return (-1);

	for (prev = rnode = plst->head; rnode != NULL; rnode = rnode->next) {
		if (rnode->data == rdata) {

			/* Is this the head of the list?*/
			if (prev == rnode) {
				plst->head = plst->head->next;
				plst->numelements--;
				free(rnode);
				return (0);
			}

			/*It is an element in the middle of the list */
			prev->next = rnode->next;
			free(rnode);
			plst->numelements--;
			return (0);
		}
		prev = rnode;
	}

	/* element was not found in this lst */
	return (-1);
}

void
*lst_at(lst_t *plst, int idx)
{
	int cnt = 0;
	lst_node_t *node;

	if ((idx < 0) || (idx >= plst->numelements))
		return (NULL);

	for (node = plst->head;
	    (node != NULL) && (cnt < plst->numelements);
	    node = node->next, cnt++) {
		if (cnt == idx)
			return (node->data);
	}

	/* Should not reach here */
	return (NULL);
}
