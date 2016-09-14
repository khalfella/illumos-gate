#ifndef _PROJENT_LST_H
#define _PROJENT_LST_H


/*
 * #include <stdlib.h>
 */

#ifdef  __cplusplus
extern "C" {
#endif



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
void *lst_replace_at(lst_t *, int, void *);
uint32_t lst_numelements(lst_t *);



#ifdef  __cplusplus
}
#endif
#endif /* _PROJENT_LST_H */
