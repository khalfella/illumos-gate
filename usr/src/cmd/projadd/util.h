#include <sys/list.h>
#include <sys/types.h>
#include <regex.h>
#include <sys/varargs.h>

#define	UTIL_STR_APPEND1(S, S1)		util_str_append((S), 1, (S1))
#define	UTIL_STR_APPEND2(S, S1, S2)	util_str_append((S), 2, (S1), (S2))

#define	BYTES_SCALE	1
#define	SCNDS_SCALE	2


typedef struct errmsg {
	char *msg;
	list_node_t next;
} errmsg_t;

extern void *util_safe_malloc(size_t);
extern void *util_safe_zmalloc(size_t);
extern char **util_tokenize(char *, list_t *);
extern void util_free_tokens(char **);
extern char *util_substr(regex_t *, regmatch_t *, char *, int);
extern char *util_str_append(char *, int, ...);
extern int util_val2num(char *, int, list_t *, char **, char **,char **);
extern void util_add_errmsg(list_t *errmsgs, char *format, ...);
extern void util_print_errmsgs(list_t *);
extern int util_pool_exist(char *);
