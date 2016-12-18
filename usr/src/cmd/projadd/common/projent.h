#ifndef	_PROJENT_PROJENT_H
#define	_PROJENT_PROJENT_H

#include <sys/types.h>
#include <regex.h>
#include <project.h>
#include <sys/varargs.h>
#include <sys/list.h>

#include "lst.h"

#define	F_PAR_VLD	0x0001	/* Run validation after parsing */
#define	F_PAR_SPC	0x0002	/* Allow spaces between names */
#define	F_PAR_UNT	0x0004	/* Allow units in attribs values */
#define	F_PAR_RES	0x0008	/* Allow projid < 100 */
#define	F_PAR_DUP	0x0010	/* Allow duplicate projids */

#define	F_MOD_ADD	0x0100
#define	F_MOD_REM	0x0200
#define	F_MOD_SUB	0x0400
#define	F_MOD_REP	0x0800

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
} projent_t;


extern void projent_free(projent_t *);
extern projent_t *projent_parse(char *, int, list_t *);
extern projent_t *projent_parse_components(char *, char *, char *, char *,
    char *, char *, int, list_t *);
extern int projent_validate(projent_t *, int, list_t *);
extern lst_t *projent_get_lst(char *, int, list_t *);
extern void projent_free_lst(lst_t *);
extern int  projent_parse_name(char *, list_t *);
extern int  projent_validate_unique_name(lst_t *, char *, list_t *);
extern int  projent_parse_projid(char *, projid_t *, list_t *);
extern int  projent_validate_projid(projid_t, int, list_t *);
extern int  projent_validate_unique_id(lst_t *, projid_t, list_t *);
extern int  projent_parse_comment(char *, list_t *);
extern void projent_merge_usrgrp(char *, char **, char *, int, list_t *);
extern char *projent_parse_users(char *, int, list_t *);
extern char *projent_parse_groups(char *, int, list_t *);
extern void projent_merge_attributes(lst_t **, lst_t *, int, list_t *);
extern lst_t *projent_parse_attributes(char *, int, list_t *);
extern void projent_sort_attributes(lst_t *);
extern void projent_free_attributes(lst_t *);
extern char *projent_attrib_tostring(void *);
extern char *projent_attrib_lst_tostring(lst_t *);
extern void projent_put_lst(char *, lst_t *, list_t *);

#ifdef  __cplusplus
}
#endif

#endif /* _PROJENT_PROJENT_H */
