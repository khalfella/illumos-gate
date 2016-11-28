#ifndef _PROJENT_PROJENT_H
#define _PROJENT_PROJENT_H

#include <sys/list.h>
#include <sys/types.h>
#include <regex.h>
#include <project.h>
#include <sys/varargs.h>

#include "lst.h"

#define F_PAR_VLD       0x00000001      /* Run validation after parsing */
#define F_PAR_SPC       0x00000002      /* Allow spaces between names */
#define F_PAR_UNT       0x00000004      /* Allow units in attribs values */
#define F_PAR_RES       0x00000008      /* Allow projid < 100 */
#define F_PAR_DUP       0x00000010      /* Allow duplicate projids */

#define F_MOD_ADD       0x00000100
#define F_MOD_REM       0x00000200
#define F_MOD_SUB       0x00000400
#define F_MOD_REP       0x00000800

#ifdef  __cplusplus
extern "C" {
#endif


typedef struct projent {
	char *projname;
	projid_t projid;
	char *comment;
	char *userlist;
	char *grouplist;
	lst_t *attrs;
	list_node_t next;
} projent_t;


extern projent_t *projent_parse_components(char *, char * , char *, char *,
    char *, char *, int, list_t *);
extern int projent_validate(projent_t *, int, list_t *);
extern list_t *projent_get_list(char *, int, list_t *);
extern void projent_free_list(list_t *);
extern int  projent_parse_name(char *, list_t *);
extern int  projent_validate_unique_name(list_t *, char *, list_t *);
extern int  projent_parse_projid(char *, projid_t *, list_t *);
extern int  projent_validate_projid(projid_t, int, list_t *);
extern int  projent_validate_unique_id(list_t *, projid_t ,list_t *);
extern int  projent_parse_comment(char *, list_t *);
extern void projent_merge_usrgrp(char *, char **, char *, int, list_t *);
extern char *projent_parse_usrgrp(char *, char *, int, list_t *);
extern void projent_merge_attributes(lst_t **, lst_t *, int, list_t *);
extern lst_t *projent_parse_attributes(char *, int, list_t *);
extern void projent_sort_attributes(lst_t *);
extern void projent_free_attributes(lst_t *);
extern char *projent_attrib_tostring(void *);
extern char *projent_attrib_lst_tostring(lst_t *);
extern void projent_put_list(char *, list_t *, list_t *);

#ifdef  __cplusplus
}
#endif

#endif /* _PROJENT_PROJENT_H */
