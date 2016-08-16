#include <stdlib.h>


struct lst_s;

typedef struct lst_node_s {
	struct lst_s *lst;
	char *data;
	struct lst_node_s *next;
} lst_node_t;

typedef struct lst_s {
	uint32_t numelements;
	lst_node_t *head;
} lst_t;


int lst_create(lst_t *);
int lst_is_empty(lst_t *);
int lst_insert_head(lst_t *, void *);
int lst_insert_tail(lst_t *, void *);

int lst_remove(lst_t *, void *);
void *lst_at(lst_t *, int);
