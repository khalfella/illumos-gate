#ifndef	_PROJENT_ATTRIB_H
#define	_PROJENT_ATTRIB_H


#include <sys/types.h>
#include <regex.h>
#include <project.h>
#include <sys/varargs.h>

#include "projent.h"

#ifdef  __cplusplus
extern "C" {
#endif

extern char *attrib_list_tostring(list_t *);
extern list_t *attrib_parse_attributes(char *, int, list_t *);
extern void attrib_free_list(list_t *);
extern char *attrib_tostring(void *);
extern void attrib_sort_list(list_t *);
extern int attrib_validate_list(list_t *, list_t *);
extern void attrib_merge_attrib_list(list_t **, list_t *, int, list_t *);

#ifdef  __cplusplus
}
#endif
#endif /* _PROJENT_ATTRIB_H */
