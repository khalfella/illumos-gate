#ifndef _PROJENT_PROJENT_H
#define _PROJENT_PROJENT_H

#include <sys/list.h>
#include <sys/types.h>
#include <regex.h>
#include <project.h>
#include <sys/varargs.h>

#include "lst.h"


#ifdef  __cplusplus
extern "C" {
#endif


typedef struct projent {
	char *projname;
	projid_t projid;
	char *comment;
	char *userlist;
	char *grouplist;
	char *attr;
	list_node_t next;
} projent_t;


extern int projent_validate(projent_t *, lst_t *, list_t *);
extern list_t *projent_get_list(char *, list_t *);
extern void projent_free_list(list_t *);
extern int  projent_parse_name(char *, list_t *);
extern int  projent_validate_unique_name(list_t *, char *, list_t *);
extern int  projent_parse_projid(char *, projid_t *, list_t *);
extern int  projent_validate_projid(projid_t, list_t *);
extern int  projent_validate_unique_id(list_t *, projid_t ,list_t *);
extern int  projent_parse_comment(char *, list_t *);
extern char *projent_parse_usrgrp(char *, char *, list_t *);
extern lst_t *projent_parse_attributes(char *, list_t *);
extern void projent_sort_attributes(lst_t *);
extern void projent_free_attributes(lst_t *);
extern char *projent_attrib_lst_tostring(lst_t *);
extern void projent_put_list(char *, list_t *, list_t *);

#ifdef  __cplusplus
}
#endif

#endif /* _PROJENT_PROJENT_H */
