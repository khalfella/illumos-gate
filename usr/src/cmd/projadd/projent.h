#include <sys/list.h>
#include <sys/types.h>
#include <regex.h>

#include <project.h>

#include <sys/varargs.h>


#include "lst.h"

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

typedef struct rctl_info_s {
	unsigned long long value;
	int flags;
} rctl_info_t;


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


/*
 * Utility functions
 */
extern void *util_safe_malloc(size_t);
extern char **util_tokenize(char *, list_t *);
extern void util_free_tokens(char **);
extern char *util_substr(regex_t *, regmatch_t *, char *, int);


/*
 * Attribute handling functions
 */
extern char *attrib_lst_tostring(lst_t *);
extern attrib_t *attrib_parse(regex_t *, regex_t *, char *, list_t *);
extern void attrib_free_lst(lst_t *);
extern char *attrib_val_tostring(attrib_val_t *);

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

extern lst_t *projent_parse_attributes(char *, list_t *);
