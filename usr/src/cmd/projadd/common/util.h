#ifndef	_PROJENT_UTIL_H
#define	_PROJENT_UTIL_H

#include <sys/types.h>
#include <regex.h>
#include <sys/varargs.h>
#include <sys/list.h>

#include "lst.h"

#ifdef  __cplusplus
extern "C" {
#endif

/* UTIL_STR_APPEND */
#define	UTIL_STR_APPEND1(S, S1)		util_str_append((S), 1, (S1))
#define	UTIL_STR_APPEND2(S, S1, S2)	util_str_append((S), 2, (S1), (S2))

/* UTIL_FREE_SNULL */
#define	UTIL_FREE_SNULL(ptr) {	\
	free(ptr);		\
	ptr = NULL;		\
}

#define	UNKWN_SCALE	0
#define	BYTES_SCALE	1
#define	SCNDS_SCALE	2
#define	COUNT_SCALE	3


extern char *util_safe_strdup(char *);
extern void *util_safe_realloc(void*, size_t);
extern void *util_safe_malloc(size_t);
extern void *util_safe_zmalloc(size_t);
extern char **util_tokenize(char *, list_t *);
extern void util_free_tokens(char **);
extern char *util_substr(regex_t *, regmatch_t *, char *, int);
extern char *util_str_append(char *, int, ...);
extern int util_val2num(char *, int, list_t *, char **, char **, char **);
extern void util_init_errlst(list_t *);
extern void util_free_errlst(list_t *);
extern void util_add_errmsg(list_t *, char *format, ...);
extern void util_print_errmsgs(list_t *);

#ifdef  __cplusplus
}
#endif
#endif /* _PROJENT_UTIL_H */
