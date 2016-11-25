#ifndef _PROJENT_ATTRIB_H
#define _PROJENT_ATTRIB_H


#include <sys/list.h>
#include <sys/types.h>
#include <regex.h>
#include <project.h>
#include <sys/varargs.h>

#include "projent.h"
#include "lst.h"

#ifdef  __cplusplus
extern "C" {
#endif

#define ATT_VAL_TYPE_NULL	0
#define ATT_VAL_TYPE_VALUE	1
#define ATT_VAL_TYPE_LIST	2

typedef struct attrib_val_s {
	int att_val_type;
	union {
		char *att_val_value;
		lst_t *att_val_values;
	};
} attrib_val_t;

typedef struct attrib_s {
	char *att_name;
	attrib_val_t *att_value;
} attrib_t;

#define	ATT_ALLOC()		attrib_alloc()
#define ATT_VAL_ALLOC(T, V)	attrib_val_alloc((T), (V))
#define ATT_VAL_ALLOC_NULL()	ATT_VAL_ALLOC(ATT_VAL_TYPE_NULL, NULL)
#define ATT_VAL_ALLOC_VALUE(V)	ATT_VAL_ALLOC(ATT_VAL_TYPE_VALUE, (V))
#define ATT_VAL_ALLOC_LIST(L)	ATT_VAL_ALLOC(ATT_VAL_TYPE_LIST, (L))

extern char *attrib_lst_tostring(lst_t *);
extern attrib_t *attrib_parse(regex_t *, regex_t *, char *, int, list_t *);
extern void attrib_free_lst(lst_t *);
extern char *attrib_val_tostring(attrib_val_t *, boolean_t);
extern attrib_t *attrib_alloc();
extern attrib_val_t *attrib_val_alloc(int, void *);
extern void attrib_sort_lst(lst_t *);
extern int attrib_validate_lst(lst_t *, list_t *);
extern void attrib_merge_attrib_lst(lst_t **, lst_t *, int, list_t *);


#ifdef  __cplusplus
}
#endif
#endif /* _PROJENT_ATTRIB_H */
