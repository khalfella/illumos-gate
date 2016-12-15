#ifndef	_PROJENT_LST_H
#define	_PROJENT_LST_H


/*
 * #include <stdlib.h>
 */

#ifdef  __cplusplus
extern "C" {
#endif



typedef struct lst_s {
	int csz;
	int tsz;
	void **buf;
} lst_t;


void lst_create(lst_t *);
int lst_is_empty(lst_t *);
void lst_insert_head(lst_t *, void *);
void lst_insert_tail(lst_t *, void *);

int lst_remove(lst_t *, void *);
void *lst_at(lst_t *, int);
void *lst_replace_at(lst_t *, int, void *);
int lst_size(lst_t *);



#ifdef  __cplusplus
}
#endif
#endif /* _PROJENT_LST_H */
