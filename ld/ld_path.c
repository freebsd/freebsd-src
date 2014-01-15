/*-
 * Copyright (c) 2010-2013 Kai Wang
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: ld_path.c 2930 2013-03-17 22:54:26Z kaiwang27 $
 */

#include "ld.h"
#include "ld_file.h"
#include "ld_path.h"

static char *_search_file(struct ld *ld, const char *path, const char *file);

static char *
_search_file(struct ld *ld, const char *path, const char *file)
{
	struct dirent *dp;
	DIR *dirp;
	char *fp;

	assert(path != NULL && file != NULL);

	if ((dirp = opendir(path)) == NULL) {
		ld_warn(ld, "opendir failed: %s", strerror(errno));
		return (NULL);
	}

	fp = NULL;
	while ((dp = readdir(dirp)) != NULL) {
		if (!strcmp(dp->d_name, file)) {
			if ((fp = malloc(PATH_MAX + 1)) == NULL)
				ld_fatal_std(ld, "malloc");
			fp[0] = '\0';
			snprintf(fp, PATH_MAX + 1, "%s/%s", path, dp->d_name);
			break;
		}
	}
	(void) closedir(dirp);

	return (fp);
}

void
ld_path_add(struct ld *ld, char *path, enum ld_path_type lpt)
{
	struct ld_state *ls;
	struct ld_path *lp;

	assert(ld != NULL && path != NULL);
	ls = &ld->ld_state;

	if ((lp = calloc(1, sizeof(*lp))) == NULL)
		ld_fatal_std(ld, "calloc");

	if ((lp->lp_path = strdup(path)) == NULL)
		ld_fatal_std(ld, "strdup");

	switch (lpt) {
	case LPT_L:
		STAILQ_INSERT_TAIL(&ls->ls_lplist, lp, lp_next);
		break;
	case LPT_RPATH:
		STAILQ_INSERT_TAIL(&ls->ls_rplist, lp, lp_next);
		break;
	case LPT_RPATH_LINK:
		STAILQ_INSERT_TAIL(&ls->ls_rllist, lp, lp_next);
		break;
	default:
		ld_fatal(ld, "Internal: invalid path type %d", lpt);
		break;
	}
}

void
ld_path_add_multiple(struct ld *ld, char *str, enum ld_path_type lpt)
{
	char *p;

	while ((p = strsep(&str, ":")) != NULL) {
		if (*p != '\0')
			ld_path_add(ld, p, lpt);
	}
}

void
ld_path_cleanup(struct ld *ld)
{
	struct ld_state *ls;
	struct ld_path *lp, *_lp;

	ls = &ld->ld_state;

	STAILQ_FOREACH_SAFE(lp, &ls->ls_lplist, lp_next, _lp) {
		STAILQ_REMOVE(&ls->ls_lplist, lp, ld_path, lp_next);
		free(lp->lp_path);
		free(lp);
	}

	STAILQ_FOREACH_SAFE(lp, &ls->ls_rplist, lp_next, _lp) {
		STAILQ_REMOVE(&ls->ls_rplist, lp, ld_path, lp_next);
		free(lp->lp_path);
		free(lp);
	}

	STAILQ_FOREACH_SAFE(lp, &ls->ls_rllist, lp_next, _lp) {
		STAILQ_REMOVE(&ls->ls_rllist, lp, ld_path, lp_next);
		free(lp->lp_path);
		free(lp);
	}
}

char *
ld_path_join_rpath(struct ld *ld)
{
	struct ld_state *ls;
	struct ld_path *lp;
	char *s;
	int len;

	ls = &ld->ld_state;

	if (STAILQ_EMPTY(&ls->ls_rplist))
		return (NULL);

	len = 0;
	STAILQ_FOREACH(lp, &ls->ls_rplist, lp_next)
		len += strlen(lp->lp_path) + 1;

	if ((s = malloc(len)) == NULL)
		ld_fatal_std(ld, "malloc");

	STAILQ_FOREACH(lp, &ls->ls_rplist, lp_next) {
		strcat(s, lp->lp_path);
		if (lp != STAILQ_LAST(&ls->ls_rplist, ld_path, lp_next))
			strcat(s, ":");
	}

	return (s);
}

void
ld_path_search_file(struct ld *ld, struct ld_file *lf)
{
	struct ld_state *ls;
	struct ld_path *lp;
	char *fp;
	int found;

	assert(lf != NULL);
	ls = &ld->ld_state;

	found = 0;
	STAILQ_FOREACH(lp, &ls->ls_lplist, lp_next) {
		if ((fp = _search_file(ld, lp->lp_path, lf->lf_name)) !=
		    NULL) {
			free(lf->lf_name);
			lf->lf_name = fp;
			found = 1;
			break;
		}
	}

	if (!found)
		ld_fatal(ld, "cannot find %s", lf->lf_name);
}

void
ld_path_search_library(struct ld *ld, const char *name)
{
	struct ld_state *ls;
	struct ld_path *lp;
	struct dirent *dp;
	DIR *dirp;
	char fp[PATH_MAX + 1], sfp[PATH_MAX + 1];
	size_t len;
	int found;

	assert(ld != NULL && name != NULL);
	ls = &ld->ld_state;

	len = strlen(name);
	found = 0;
	STAILQ_FOREACH(lp, &ls->ls_lplist, lp_next) {
		assert(lp->lp_path != NULL);
		if ((dirp = opendir(lp->lp_path)) == NULL) {
			ld_warn(ld, "opendir failed: %s", strerror(errno));
			continue;
		}

		fp[0] = sfp[0] = '\0';
		while ((dp = readdir(dirp)) != NULL) {
			if (strncmp(dp->d_name, "lib", 3))
				continue;
			if (strncmp(name, &dp->d_name[3], len))
				continue;
			if (ls->ls_static == 0 &&
			    !strcmp(&dp->d_name[len + 3], ".so")) {
				snprintf(fp, sizeof(fp), "%s/%s", lp->lp_path,
				    dp->d_name);
				ld_file_add(ld, fp, LFT_DSO);
				(void) closedir(dirp);
				found = 1;
				goto done;
			} else if (*sfp == '\0' &&
			    !strcmp(&dp->d_name[len + 3], ".a")) {
				snprintf(sfp, sizeof(sfp), "%s/%s", lp->lp_path,
				    dp->d_name);
				if (ls->ls_static == 1) {
					ld_file_add(ld, sfp, LFT_ARCHIVE);
					(void) closedir(dirp);
					found = 1;
					goto done;
				}
			}
		}
		(void) closedir(dirp);
	}
done:
	if (!found) {
		if (ls->ls_static == 0 && *sfp != '\0') {
			ld_file_add(ld, sfp, LFT_ARCHIVE);
		} else
			ld_fatal(ld, "cannot find -l%s", name);
	}
}

void
ld_path_search_dso_needed(struct ld *ld, struct ld_file *lf, const char *name)
{
	struct ld_state *ls;
	struct ld_path *lp;
	struct ld_file *_lf;
	char *fp;

	ls = &ld->ld_state;

	/*
	 * First check if we've seen this shared library or if it's
	 * already listed in the input file list.
	 */
	TAILQ_FOREACH(_lf, &ld->ld_lflist, lf_next) {
		if (!strcmp(_lf->lf_name, name) ||
		    !strcmp(basename(_lf->lf_name), name))
			return;
	}

	/* Search -rpath-link directories. */
	STAILQ_FOREACH(lp, &ls->ls_rllist, lp_next) {
		if ((fp = _search_file(ld, lp->lp_path, name)) != NULL)
			goto done;
	}

	/* Search -rpath directories. */
	STAILQ_FOREACH(lp, &ls->ls_rplist, lp_next) {
		if ((fp = _search_file(ld, lp->lp_path, name)) != NULL)
			goto done;
	}

	/* TODO: search additional directories and environment variables. */

	/* Search /lib and /usr/lib. */
	if ((fp = _search_file(ld, "/lib", name)) != NULL)
		goto done;
	if ((fp = _search_file(ld, "/usr/lib", name)) != NULL)
		goto done;

	/* Not found. */
	ld_warn(ld, "cannot find needed shared library: %s", name);
	return;

done:
	ld_file_add_after(ld, fp, LFT_DSO, lf);
	free(fp);
}
