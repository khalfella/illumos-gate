#include <sys/list.h>
#include <sys/types.h>

#include <project.h>



typedef struct projent {
	char *projname;
	id_t projid;
	char *comment;
	char *userlist;
	char *grouplist;
	char *attr;
	list_node_t next;
} projent_t;


extern void *safe_malloc(size_t);
extern list_t *projent_get_list(char *projfile);
extern void projent_print_ent(projent_t *);



