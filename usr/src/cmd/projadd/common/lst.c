#include <stdio.h>
#include <stdlib.h>
#include <libintl.h>
#include "lst.h"
#include "util.h"


void
lst_create(lst_t *plst)
{
	plst->csz = 0;
	plst->tsz = 0;
	plst->buf = NULL;
}

int
lst_is_empty(lst_t *plst)
{
	return (plst->csz == 0);
}

void
lst_insert_head(lst_t *plst, void *ndata)
{
	int i;
	if (plst->csz == plst->tsz) {
		plst->tsz = (plst->tsz == 0) ? 1 : plst->tsz * 2;
		plst->buf = util_safe_realloc(plst->buf,
		    plst->tsz * sizeof (void *));
	}

	for (i = plst->csz; i > 0; i--)
		plst->buf[i] = plst->buf[i - 1];

	plst->buf[0] = ndata;
	plst->csz++;
}


void
lst_insert_tail(lst_t *plst, void *ndata)
{
	if (plst->csz == plst->tsz) {
		plst->tsz = (plst->tsz == 0) ? 1 : plst->tsz * 2;
		plst->buf = util_safe_realloc(plst->buf,
		    plst->tsz * sizeof (void *));
	}

	plst->buf[plst->csz++] = ndata;
}

int
lst_remove(lst_t *plst, void *rdata)
{
	int i, idx = -1;
	for (i = 0; i < plst->csz; i++) {
		if (plst->buf[i] == rdata) {
			idx = i;
			break;
		}
	}
	if (idx >= 0) {
		for (i = idx; i < plst->csz - 1; i++)
			plst->buf[i] = plst->buf[i + 1];

		if (--plst->csz == 0) {
			plst->tsz = 0;
			free(plst->buf);
			plst->buf = NULL;
		}
		return (0);
	}
	return (-1);
}

void *
lst_at(lst_t *plst, int idx)
{
	if (idx < 0 || idx >= plst->csz) {
		(void) fprintf(stderr, gettext(
		    "error accessing element outside lst\n"));
		exit(1);
	}
	return (plst->buf[idx]);
}

void *
lst_replace_at(lst_t *plst, int idx, void *ndata)
{
	void *odata;

	if (idx < 0 || idx >= plst->csz) {
		(void) fprintf(stderr, gettext(
		    "error accessing element outside lst\n"));
		exit(1);
	}
	odata = plst->buf[idx];
	plst->buf[idx] = ndata;
	return (odata);
}

int
lst_size(lst_t *plst)
{
	return (plst->csz);
}
