/*
 * Copyright (c) 2013-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * make(1)/src triad word-removal across += assignment fragments.
 */

#include <ctype.h>

#include "sysconf_priv.h"

int
make_strike_p(const struct request *req)
{

	return ((bsdconf_format_processing(format) &
	    BSDCONF_OPERATOR_EQUALS) != 0 &&
	    req->listop == '-' && req->newvalue != NULL);
}

int
make_assign_push(struct request *req, enum bsdconf_op op,
    const char *piece, uint32_t line)
{
	struct make_assign *na;
	char *pcopy;

	if (piece == NULL)
		piece = "";
	if ((pcopy = strdup(piece)) == NULL)
		return (-1);
	na = realloc(req->assigns, (req->nassigns + 1) * sizeof(*na));
	if (na == NULL) {
		free(pcopy);
		return (-1);
	}
	req->assigns = na;
	na[req->nassigns].fileidx = conf_scanidx;
	na[req->nassigns].line = line;
	na[req->nassigns].op = op;
	na[req->nassigns].piece = pcopy;
	req->nassigns++;
	return (0);
}

/*
 * Apply make/src `name-=word' strikes: for each whitespace-separated word,
 * find the last assignment whose piece contains it and remove the word from
 * that piece (delete the statement if the piece becomes empty). With
 * `check_only', report whether any change would be required without writing.
 * Echoes effective old/new (or `(unchanged)') unless -q. Neutralizes each
 * handled request so do_writes() skips it. Returns EXIT_SUCCESS or
 * EXIT_FAILURE.
 */
int
do_make_strikes(int check_only)
{
	char **work;
	char *new_eff;
	char *token;
	char *wordscopy;
	char *wp;
	const char *old_eff;
	int differs;
	int rv = EXIT_SUCCESS;
	size_t i;
	size_t j;
	size_t wlen;
	unsigned int n;

	for (n = 0; n < nreqs; n++) {
		if (!make_strike_p(&reqs[n]))
			continue;
		if (!reqs[n].found || reqs[n].nassigns == 0) {
			if (!ignore_unknown) {
				if (!quiet)
					warnx("unknown directive '%s'",
					    reqs[n].name);
				rv = EXIT_FAILURE;
			}
			reqs[n].newvalue = NULL;
			reqs[n].name = "";
			continue;
		}

		if ((work = calloc(reqs[n].nassigns, sizeof(*work))) == NULL)
			err(EXIT_FAILURE, NULL);
		for (i = 0; i < reqs[n].nassigns; i++) {
			work[i] = strdup(reqs[n].assigns[i].piece);
			if (work[i] == NULL)
				err(EXIT_FAILURE, NULL);
		}

		differs = 0;
		if ((wordscopy = strdup(reqs[n].newvalue)) == NULL)
			err(EXIT_FAILURE, NULL);
		wp = wordscopy;
		while ((token = strsep(&wp, " \t")) != NULL) {
			if (*token == '\0')
				continue;
			wlen = strlen(token);
			for (i = reqs[n].nassigns; i > 0; i--) {
				j = i - 1;
				if (!list_has(work[j], token, wlen))
					continue;
				{
					char *merged;

					merged = list_merge(work[j], token,
					    '-');
					free(work[j]);
					work[j] = merged;
					differs = 1;
				}
				break;
			}
		}
		free(wordscopy);

		if (check_only) {
			if (differs) {
				if (!quiet)
					warnx("%s: value differs",
					    reqs[n].name);
				rv = EXIT_FAILURE;
			}
			goto neutralize;
		}

		old_eff = reqs[n].value != NULL ? reqs[n].value : "";
		if (!differs) {
			if (!quiet)
				print_write_echo(
				    reqs[n].srcidx >= 0 ?
				    conf_files[reqs[n].srcidx] : NULL,
				    0, reqs[n].name, BSDCONF_OP_ASSIGN,
				    old_eff, old_eff, 0, 0, 0);
			goto neutralize;
		}

		/* Effective value after applying the struck pieces */
		new_eff = NULL;
		for (i = 0; i < reqs[n].nassigns; i++) {
			char *merged;

			if (work[i][0] == '\0')
				continue;
			merged = make_apply(reqs[n].assigns[i].op, new_eff,
			    work[i]);
			if (merged == NULL)
				err(EXIT_FAILURE, NULL);
			free(new_eff);
			new_eff = merged;
		}
		if (new_eff == NULL) {
			if ((new_eff = strdup("")) == NULL)
				err(EXIT_FAILURE, NULL);
		}

		/* Put each changed statement (line-targeted) */
		for (i = 0; i < nconf_files; i++) {
			struct bsdconf_option *options;
			unsigned int nopts = 0;
			size_t k;

			for (k = 0; k < reqs[n].nassigns; k++)
				if (reqs[n].assigns[k].fileidx == i &&
				    strcmp(work[k],
				    reqs[n].assigns[k].piece) != 0)
					nopts++;
			if (nopts == 0)
				continue;
			options = calloc(nopts + 1, sizeof(*options));
			if (options == NULL)
				err(EXIT_FAILURE, NULL);
			nopts = 0;
			for (k = 0; k < reqs[n].nassigns; k++) {
				struct bsdconf_option *opt;

				if (reqs[n].assigns[k].fileidx != i ||
				    strcmp(work[k],
				    reqs[n].assigns[k].piece) == 0)
					continue;
				opt = &options[nopts++];
				memset(opt, 0, sizeof(*opt));
				opt->type = BSDCONF_TYPE_STR;
				opt->directive = reqs[n].name;
				opt->match_line = reqs[n].assigns[k].line;
				opt->op = reqs[n].assigns[k].op;
				if (work[k][0] == '\0') {
					opt->action = BSDCONF_ACTION_REMOVE;
					opt->value.str = NULL;
				} else {
					opt->action = BSDCONF_ACTION_SET_VALUE;
					opt->value.str = work[k];
				}
			}
			{
				int fd;

				if ((fd = open(conf_files[i],
				    O_WRONLY | O_CREAT | O_EXCL, 0666)) >= 0)
					close(fd);
				else if (errno != EEXIST)
					err(EXIT_FAILURE, "%s", conf_files[i]);
			}
			if (bsdconf_put(options, conf_files[i],
			    bsdconf_format_processing(format),
			    bsdconf_format_put(format)) != 0)
				err(EXIT_FAILURE, "%s", conf_files[i]);
			if (!quiet && verbose_trail) {
				for (k = 0; k < nopts; k++) {
					struct bsdconf_option *opt;
					const char *oval;
					const char *nval;

					opt = &options[k];
					/* Recover old piece from assigns */
					oval = "";
					nval = opt->value.str != NULL ?
					    opt->value.str : "";
					for (j = 0; j < reqs[n].nassigns;
					    j++) {
						if (reqs[n].assigns[j].fileidx
						    != i ||
						    reqs[n].assigns[j].line !=
						    opt->match_line)
							continue;
						oval =
						    reqs[n].assigns[j].piece;
						break;
					}
					if (opt->action ==
					    BSDCONF_ACTION_REMOVE)
						printf("%s:%u: %s%s%s "
						    "(removed)\n",
						    conf_files[i],
						    opt->match_line,
						    reqs[n].name,
						    op_str(opt->op), oval);
					else
						print_write_echo(conf_files[i],
						    opt->match_line,
						    reqs[n].name, opt->op,
						    oval, nval, 1, 0, 0);
				}
			}
			free(options);
		}

		if (!quiet && !verbose_trail)
			print_write_echo(
			    reqs[n].srcidx >= 0 ?
			    conf_files[reqs[n].srcidx] : NULL,
			    0, reqs[n].name, BSDCONF_OP_ASSIGN, old_eff,
			    new_eff, 1, 0, 0);
		free(reqs[n].value);
		reqs[n].value = new_eff;

neutralize:
		for (i = 0; i < reqs[n].nassigns; i++)
			free(work[i]);
		free(work);
		reqs[n].newvalue = NULL;
		reqs[n].listop = '\0';
		/* Keep name for any trailing read of the same argv slot */
	}

	return (rv);
}
