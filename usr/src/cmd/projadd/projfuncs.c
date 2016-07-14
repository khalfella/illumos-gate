#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <locale.h>
#include <stddef.h>



#include "projadd.h"

void *
safe_malloc(size_t sz)
{
 	char *ptr;
	if ((ptr = malloc(sz)) == NULL) {
		(void) fprintf(stderr, gettext(
		    "projadd: error allocating %d bytes of memory"),
		    sz);
		exit(1);
        }
        return (ptr);
}

void projent_add_errmsg(list_t *errmsgs, char *errmsg)
{
	if (errmsgs == NULL || errmsg == NULL)
		return;

	errmsg_t *msg = safe_malloc(sizeof(errmsg_t));
	list_link_init(&msg->next);
	msg->msg = strdup(errmsg);
	list_insert_tail(errmsgs, msg);
} 

void projent_print_errmsgs(list_t *errmsgs)
{
	errmsg_t *msg;
	for (msg = list_head(errmsgs); msg != NULL;
	    msg = list_next(errmsgs, msg)) {
		fprintf(stderr, "%s\n", msg->msg);
	}
}

void projent_free_errmsgs(list_t *errmsgs)
{
	errmsg_t *msg;
	while ((msg = list_head(errmsgs)) != NULL) {
		list_remove(errmsgs, msg);
		free(msg->msg);
		free(msg);
	}
}

void
projent_print_ent(projent_t *ent)
{
	if (ent->projname) printf("projname = %s\n", ent->projname);
	printf("projid = %d\n", ent->projid);
	if (ent->comment) printf("comment = %s\n", ent->comment);
	if (ent->userlist) printf("userlist = %s\n", ent->userlist);
	if (ent->grouplist) printf("grouplist = %s\n", ent->grouplist);
	if (ent->attr) printf("attr = %s\n", ent->attr);
}

projent_t
*projent_parse(char *str) {
	char *s1;

	if (str == NULL)
		return (NULL);

	projent_t *ent = safe_malloc(sizeof(projent_t));
	list_link_init(&ent->next);

	if ((ent->projname = strsep(&str, ":")) != NULL &&
	    (s1 = strsep(&str, ":")) != NULL &&
	    ((ent->projid = atoi(s1)) != 0 || errno != EINVAL) &&
	    (ent->comment = strsep(&str, ":")) != NULL &&
	    (ent->userlist = strsep(&str, ":")) != NULL &&
	    (ent->grouplist = strsep(&str, ":")) != NULL &&
	    (ent->attr = strsep(&str, ":")) != NULL &&
	    strsep(&str, ":") == NULL) {
		return (ent);
	}

	free(ent);
	return (NULL);
}


void
projent_free_list(list_t *plist) {
	projent_t *ent;
	while ((ent = list_head(plist)) != NULL) {
		list_remove(plist, ent);
		free(ent->projname);
		free(ent);
	}
}



list_t
*projent_get_list(char *projfile, list_t *errors)
{
	FILE *fp;
	list_t *ret;
	int read, line = 0;
	char *buf = NULL, *ptr, *nlp;
	char *errorstr;
	size_t cap = 0;
	projent_t *ent;

	ret = safe_malloc(sizeof(list_t));
	list_create(ret, sizeof(projent_t), offsetof(projent_t, next));

	if ((fp = fopen(projfile, "r")) == NULL) {
		if (errno == ENOENT) {
			/*
			 * There is no project file,
			 * return an empty list
			 */
			return (ret);
		} else {
			/* Report the error unable to open the file */
			asprintf(&errorstr, gettext("Cannot open %s: %s"),
			    projfile, strerror(errno));
			projent_add_errmsg(errors, errorstr);
			free(errorstr);

			/* destory and free the to-be-returned list */
			list_destroy(ret);
			free(ret);
			return (NULL);
		}
	}

	while((getline(&buf, &cap, fp)) != -1 && ++line) {

		ptr = strdup(buf);	
		if ((nlp = strchr(ptr, '\n')) != NULL)
			*nlp = '\0';

		if ((ent = projent_parse(ptr)) != NULL) {
			list_insert_tail(ret, ent);
		} else {
			/* Report the error */
			asprintf(&errorstr, gettext(
			    "Error parsing: %s line: %d\n\"%s\""),
			    projfile, line, ptr);
			projent_add_errmsg(errors, errorstr);
			free(errorstr);

			/* free the allocated resources */
			projent_free_list(ret);
			list_destroy(ret);
			free(ret);
			free(ptr);
			ret = NULL;
			break;
		}
	}

	free(buf);
	fclose(fp);
	return (ret);
}
