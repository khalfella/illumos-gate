#include <sys/list.h>
#include <sys/types.h>

#include <project.h>

#include <sys/varargs.h>



typedef struct projent {
	char *projname;
	id_t projid;
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
