#include <sys/list.h>
#include <sys/types.h>

#include <project.h>

#include <sys/varargs.h>

typedef struct projent {
	char *projname;
	projid_t projid;
	char *comment;
	char *userlist;
	char *grouplist;
	char *attr;
	list_node_t next;
} projent_t;


typedef struct errmsg {
	char *msg;
	list_node_t next;
} errmsg_t;


extern void *safe_malloc(size_t);
extern list_t *projent_get_list(char *, list_t *);
extern void projent_print_ent(projent_t *);
extern void projent_add_errmsg(list_t *, char *, ...);
extern void projent_print_errmsgs(list_t *);
extern int  projent_parse_name(char *, list_t *);
extern int  projent_validate_unique_name(list_t *, char *, list_t *);
extern int  projent_parse_projid(char *, projid_t *, list_t *);
extern int  projent_validate_projid(projid_t, list_t *);
extern int  projent_validate_unique_id(list_t *, projid_t ,list_t *);
extern int  projent_parse_comment(char *, list_t *);
extern char *projent_parse_usrgrp(char *, char *, list_t *);

extern int  projent_parse_attributes(char *, list_t *);
