/*
 * Copyright (c) 2013-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Display helpers: name/value pairs, `-V' trails, write echoes.
 */

#include <ctype.h>

#include "sysconf_priv.h"

/*
 * True when `name' is a presence-only WITH_/WITHOUT_ build knob
 * (share/mk/bsd.mkopt.mk tests defined(), and src.conf(5) ignores the
 * value). WITHOUT_MODULES is excluded: its value is a module reject list.
 */
int
make_knob_name_p(const char *name)
{

	if (strcmp(name, "WITHOUT_MODULES") == 0)
		return (0);
	return (strncmp(name, "WITH_", 5) == 0 ||
	    strncmp(name, "WITHOUT_", 8) == 0);
}

/*
 * Precursor to share/mk/bsd.mkopt.mk (seen under /usr/src after
 * bsd.opts.mk / src.opts.mk, not bare make or ports): WITH_FOO=no
 * does not disable FOO (presence still counts). Warn only for WITH_*
 * with the exact value "no" (not WITHOUT_*, not "NO"). On reads,
 * `path'/`line' locate the authoritative assignment when known.
 */
void
warn_with_equals_no(const char *name, const char *value, const char *path,
    uint32_t line)
{
	const char *opt;

	if (format != BSDCONF_FORMAT_MAKE && format != BSDCONF_FORMAT_SRC)
		return;
	if (name == NULL || value == NULL || strcmp(value, "no") != 0)
		return;
	/* WITH_ but not WITHOUT_ */
	if (strncmp(name, "WITH_", 5) != 0 ||
	    strncmp(name, "WITHOUT_", 8) == 0)
		return;
	opt = name + 5;
	if (path != NULL && line != 0)
		warnx("%s:%u: Use WITHOUT_%s=1 instead of WITH_%s=no",
		    path, line, opt, opt);
	else
		warnx("Use WITHOUT_%s=1 instead of WITH_%s=no", opt, opt);
}

/*
 * True when `name' is a presence-only WITH_/WITHOUT_ knob on make/src
 * (bsd.mkopt.mk tests defined(); src.conf(5) ignores the value).
 */
int
make_knob_p(const char *name)
{

	if (format != BSDCONF_FORMAT_MAKE && format != BSDCONF_FORMAT_SRC)
		return (0);
	return (make_knob_name_p(name));
}

/*
 * Print a directive/value pair according to the display flags. `path' is
 * the configuration file holding the authoritative definition (or NULL).
 */
void
print_pair(const char *path, const char *directive, const char *value)
{
	int knob;

	if (show_file)
		value = path != NULL ? path : "(none)";
	if (verbose && path != NULL && !show_file)
		printf("%s: ", path);
	if (name_only) {
		puts(directive);
		return;
	}
	/* Presence knobs: any assigned value means present (defined()). */
	knob = !show_file && make_knob_p(directive);
	if (value_only) {
		puts(value != NULL ? value : "");
		return;
	}
	/*
	 * Presence display is for the human-readable form only. With -e,
	 * emit the real assignment (`NAME=') so the spelling is valid make
	 * and matches what -s writes.
	 */
	if (knob && !show_equals) {
		printf("%s (present)\n", directive);
		return;
	}
	if (show_equals && !show_file) {
		if ((bsdconf_format_put(format) &
		    BSDCONF_PUT_QUOTE_ALWAYS) != 0)
			printf("%s=\"%s\"\n", directive,
			    value != NULL ? value : "");
		else
			printf("%s=%s\n", directive,
			    value != NULL ? value : "");
		return;
	}
	printf("%s: %s\n", directive, value != NULL ? value : "");
}

/*
 * Spelling of a make(1) (or plain) assignment operator for `-V' trails.
 */
const char *
op_str(enum bsdconf_op op)
{

	switch (op) {
	case BSDCONF_OP_APPEND:
		return ("+=");
	case BSDCONF_OP_COND:
		return ("?=");
	case BSDCONF_OP_EXPAND:
		return (":=");
	case BSDCONF_OP_SHELL:
		return ("!=");
	case BSDCONF_OP_ASSIGN:
	case BSDCONF_OP_DEFAULT:
	default:
		return ("=");
	}
}

/*
 * Append one assignment step to a `-V' trail. No-op when `-V' is unset.
 * Returns zero on success; -1 (errno set) on allocation failure.
 */
int
trail_push(struct trail_step **trailp, size_t *ntrailp,
    size_t fileidx, uint32_t line, enum bsdconf_op op,
    const char *piece, const char *effective)
{
	struct trail_step *nt;
	char *ecopy;
	char *pcopy;

	if (!verbose_trail)
		return (0);
	if (piece == NULL)
		piece = "";
	if (effective == NULL)
		effective = "";
	if ((pcopy = strdup(piece)) == NULL)
		return (-1);
	if ((ecopy = strdup(effective)) == NULL) {
		free(pcopy);
		return (-1);
	}
	nt = realloc(*trailp, (*ntrailp + 1) * sizeof(*nt));
	if (nt == NULL) {
		free(pcopy);
		free(ecopy);
		return (-1);
	}
	*trailp = nt;
	nt[*ntrailp].fileidx = fileidx;
	nt[*ntrailp].line = line;
	nt[*ntrailp].op = op;
	nt[*ntrailp].piece = pcopy;
	nt[*ntrailp].effective = ecopy;
	(*ntrailp)++;
	return (0);
}

void
trail_free(struct trail_step *trail, size_t ntrail)
{
	size_t i;

	for (i = 0; i < ntrail; i++) {
		free(trail[i].piece);
		free(trail[i].effective);
	}
	free(trail);
}

/*
 * Print every recorded assignment step for `name'. Each line is
 * `file:line: nameoppiece' and, when the running effective value differs
 * from the fragment, ` -> effective'.
 */
void
print_trail(const char *name, const struct trail_step *trail, size_t ntrail)
{
	const char *path;
	size_t i;

	for (i = 0; i < ntrail; i++) {
		path = conf_files[trail[i].fileidx];
		if (strcmp(trail[i].piece, trail[i].effective) != 0)
			printf("%s:%u: %s%s%s -> %s\n", path, trail[i].line,
			    name, op_str(trail[i].op), trail[i].piece,
			    trail[i].effective);
		else
			printf("%s:%u: %s%s%s\n", path, trail[i].line, name,
			    op_str(trail[i].op), trail[i].piece);
	}
}

/*
 * Echo a write result. With -V: `file:line: name=old -> name=new',
 * `name=value (unchanged)', or `name=value (added)', matching read-trail
 * shape. With -v alone: prefix the pathname. Otherwise: `name: old -> new'
 * / `name: value (unchanged)' (additions still use old -> new with an
 * empty old value). With `-e', the non-trail form uses `=' (and quotes
 * for formats that always quote), matching print_pair() on reads --
 * except make/src WITH_/WITHOUT_ presence knobs, which use
 * `(not present) -> (present)' when newly defined,
 * `(present) (unchanged)' when `-s' finds them already defined or
 * an explicit write leaves the value unchanged, and a normal `old ->
 * new' echo when the placeholder value changes while still present
 * (also for `-e'). Removals are only echoed when the caller passes
 * `-v'/`-V' (not bare `-x'), using the same `name (removed)' / trail
 * form as other directives.
 */
void
print_write_echo(const char *path, uint32_t line, const char *name,
    enum bsdconf_op op, const char *oldv, const char *newv, int changed,
    int added, int removed)
{
	const char *ops;
	const char *shown;
	int knob;
	int quote;
	int was_present;

	/* NULL oldv: absent; "" : present with empty assignment */
	was_present = (oldv != NULL);
	if (oldv == NULL)
		oldv = "";
	if (newv == NULL)
		newv = "";
	ops = op_str(op);
	shown = oldv[0] != '\0' ? oldv : newv;
	knob = make_knob_p(name) && op != BSDCONF_OP_APPEND;

	if (verbose_trail && path != NULL) {
		if (removed) {
			printf("%s:%u: %s (removed)\n", path, line, name);
			return;
		}
		if (knob) {
			if (!was_present || added) {
				printf("%s:%u: %s: (not present) -> "
				    "(present)\n", path, line, name);
				return;
			}
			if (strcmp(oldv, newv) == 0) {
				printf("%s:%u: %s: (present) (unchanged)\n",
				    path, line, name);
				return;
			}
			/* value change while present: fall through */
		}
		if (added)
			printf("%s:%u: %s%s%s (added)\n", path, line, name,
			    ops, newv);
		else if (changed)
			printf("%s:%u: %s%s%s -> %s%s%s\n", path, line, name,
			    ops, oldv, name, ops, newv);
		else
			printf("%s:%u: %s%s%s (unchanged)\n", path, line, name,
			    ops, shown);
		return;
	}
	if (verbose && path != NULL)
		printf("%s: ", path);
	if (removed) {
		printf("%s (removed)\n", name);
		return;
	}
	if (knob) {
		if (!was_present || added) {
			printf("%s: (not present) -> (present)\n", name);
			return;
		}
		if (strcmp(oldv, newv) == 0) {
			printf("%s: (present) (unchanged)\n", name);
			return;
		}
		/* value change while present: fall through */
	}
	quote = show_equals && !show_file &&
	    (bsdconf_format_put(format) & BSDCONF_PUT_QUOTE_ALWAYS) != 0;
	if (show_equals && !show_file) {
		if (changed || added) {
			/* sysrc(8)-style: `name=old # -> new' with `-e' */
			if (quote)
				printf("%s=\"%s\" # -> \"%s\"\n", name, oldv,
				    newv);
			else
				printf("%s=%s # -> %s\n", name, oldv, newv);
		} else if (quote)
			printf("%s=\"%s\" (unchanged)\n", name, shown);
		else
			printf("%s=%s (unchanged)\n", name, shown);
		return;
	}
	if (changed || added)
		printf("%s: %s -> %s\n", name, oldv, newv);
	else
		printf("%s: %s (unchanged)\n", name, shown);
}
