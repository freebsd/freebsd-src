/*
 * Copyright (c) 2013-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Directive descriptions harvested from defaults-file comments (-d).
 */

#include "sysconf_priv.h"

/*
 * Directive descriptions harvested from the defaults file by
 * load_descriptions() below.
 */
struct descent {
	char		*name;		/* directive name */
	char		*desc;		/* description (may be empty) */
};

static struct descent	*descs;		/* set by load_descriptions() */
static size_t		ndescs;
static size_t		descsize;

/*
 * Harvest every directive description from the defaults file, where a
 * description is the inline comment trailing the directive's default
 * assignment, continued across any subsequent lines of whitespace
 * followed by a comment character. A commented-out default
 * (`#comconsole_speed="115200" # Set the ...') yields its directive and
 * description all the same -- deliberately unlike sysrc(8), which goes
 * blind when the default itself is disabled. Comment prose is never
 * mistaken for a directive: a name must directly abut the optional
 * leading `#' and run unbroken to its `='.
 */
void
load_descriptions(void)
{
	int current = -1;	/* entry whose description is growing */
	FILE *fp;
	ssize_t linelen;
	size_t len;
	size_t linesize = 0;
	size_t n;
	char *defaults;
	char *line = NULL;
	char *p;
	char *v;
	struct descent *tmp;

	if ((defaults = defaults_path()) == NULL)
		errx(EXIT_FAILURE, "target has no defaults file");
	if ((fp = fopen(defaults, "r")) == NULL)
		err(EXIT_FAILURE, "%s", defaults);

	while ((linelen = getline(&line, &linesize, fp)) != -1) {
		int quoted = 0;

		if (linelen > 0 && line[linelen - 1] == '\n')
			line[--linelen] = '\0';

		/* Whitespace then `#' continues the open description */
		p = line;
		if (*p == ' ' || *p == '\t') {
			p += strspn(p, " \t");
			if (*p == '#' && current >= 0) {
				char *grown;

				p++;
				p += strspn(p, " \t");
				if (*p == '\0')
					continue;
				len = strlen(descs[current].desc) +
				    strlen(p) + 2;
				if ((grown = malloc(len)) == NULL)
					err(EXIT_FAILURE, NULL);
				snprintf(grown, len, "%s%s%s",
				    descs[current].desc,
				    *descs[current].desc != '\0' ? " " : "",
				    p);
				free(descs[current].desc);
				descs[current].desc = grown;
				continue;
			}
			current = -1;
			continue;
		}
		current = -1;

		/* A commented-out default still describes its directive */
		if (*p == '#')
			p++;

		/* A directive name runs unbroken to its `=' */
		len = strcspn(p, " \t\"#=");
		if (len == 0 || p[len] != '=')
			continue;

		/* First description wins on a repeated directive */
		for (n = 0; n < ndescs; n++)
			if (strncmp(descs[n].name, p, len) == 0 &&
			    descs[n].name[len] == '\0')
				break;
		if (n < ndescs)
			continue;

		/* Skip the value (quotes may hide a `#') */
		for (v = p + len + 1; *v != '\0'; v++) {
			if (*v == '"')
				quoted = !quoted;
			else if (*v == '#' && !quoted)
				break;
		}
		if (*v == '#') {
			v++;
			v += strspn(v, " \t");
		}

		if (ndescs >= descsize) {
			descsize = (descsize == 0) ? 64 : descsize << 1;
			tmp = realloc(descs, descsize * sizeof(*descs));
			if (tmp == NULL)
				err(EXIT_FAILURE, NULL);
			descs = tmp;
		}
		descs[ndescs].name = strndup(p, len);
		descs[ndescs].desc = strdup(v);
		if (descs[ndescs].name == NULL ||
		    descs[ndescs].desc == NULL)
			err(EXIT_FAILURE, NULL);
		current = (int)ndescs;
		ndescs++;
	}
	free(line);
	fclose(fp);
	free(defaults);
}

/*
 * Return the harvested description of `name' or NULL if the defaults file
 * does not mention it.
 */
static const char *
desc_find(const char *name)
{
	size_t n;

	for (n = 0; n < ndescs; n++)
		if (strcmp(descs[n].name, name) == 0)
			return (descs[n].desc);
	return (NULL);
}

/*
 * Print the description of `directive' according to the display flags: for
 * the sysctl target, from the running kernel; otherwise from the table
 * harvested off the defaults file (a directive the defaults never mention
 * prints an empty description).
 */
void
print_desc(const char *directive)
{
	const char *desc = NULL;
#if defined(__FreeBSD__)
	char buf[BUFSIZ];

	if (format == BSDCONF_FORMAT_SYSCTL) {
		(void)sysctl_descr(directive, buf, sizeof(buf));
		desc = buf;
	}
#endif
	if (desc == NULL && (desc = desc_find(directive)) == NULL)
		desc = "";
	if (name_only)
		puts(directive);
	else if (value_only)
		puts(desc);
	else
		printf("%s: %s\n", directive, desc);
}

/*
 * Process `-d' with explicit names for a defaults-backed target. Returns
 * EXIT_SUCCESS or EXIT_FAILURE.
 */
int
describe_defaults(void)
{
	int rv = EXIT_SUCCESS;
	unsigned int n;

	load_descriptions();
	for (n = 0; n < nreqs; n++) {
		if (desc_find(reqs[n].name) == NULL) {
			if (ignore_unknown)
				continue;
			if (!quiet)
				warnx("unknown directive '%s'",
				    reqs[n].name);
			rv = EXIT_FAILURE;
			continue;
		}
		print_desc(reqs[n].name);
	}

	return (rv);
}
