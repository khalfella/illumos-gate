#ifndef _PROJENT_ATTRIB_H
#define _PROJENT_ATTRIB_H


#include <sys/types.h>
#include <regex.h>
#include <project.h>
#include <sys/varargs.h>

#include "projent.h"
#include "lst.h"

#ifdef  __cplusplus
extern "C" {
#endif

extern char *attrib_lst_tostring(lst_t *);
extern lst_t *attrib_parse_attributes(char *, int, lst_t *);
extern void attrib_free_lst(lst_t *);
extern char *attrib_tostring(void *);
extern void attrib_sort_lst(lst_t *);
extern int attrib_validate_lst(lst_t *, lst_t *);
extern void attrib_merge_attrib_lst(lst_t **, lst_t *, int, lst_t *);

#ifdef  __cplusplus
}
#endif
#endif /* _PROJENT_ATTRIB_H */
