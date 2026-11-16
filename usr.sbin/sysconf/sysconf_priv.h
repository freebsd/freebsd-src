/*
 * Copyright (c) 2013-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _SYSCONF_PRIV_H_
#define _SYSCONF_PRIV_H_

#ifdef __FreeBSD__
#include <sys/param.h>
#include <sys/capsicum.h>
#include <sys/jail.h>
#include <sys/linker.h>
#include <sys/sysctl.h>

#include <capsicum_helpers.h>
#include <jail.h>
#endif

#include <bsdconf.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef __unused
#define __unused	/* not all compilers support attributes */
#endif

#define SYSCONF_VERSION	"2.3 2026-10-26"

/* getopt(3) optstring; shared by parse_options() and find_target() */
#define OPTSTRING	"AacdDEeFf:hij:k:lLnNqR:svVx"

/* One assignment step recorded for `-V' trails */
struct trail_step {
	size_t		fileidx;	/* index into conf_files */
	uint32_t	line;		/* 1-based line in that file */
	enum bsdconf_op	op;		/* operator of this statement */
	char		*piece;		/* unquoted fragment */
	char		*effective;	/* value after applying the op */
};

/*
 * One make(1) assignment statement for `name-=word' strikes: the last
 * statement whose piece contains the word is rewritten (or removed if
 * the piece becomes empty).
 */
struct make_assign {
	size_t		fileidx;
	uint32_t	line;
	enum bsdconf_op	op;
	char		*piece;
};

#define UCL_INDEX_COUNT	LONG_MIN	/* `name.#' read: the element count */
#define UCL_INDEX_APPEND	(LONG_MIN + 1)	/* `name.#=value': one past the end */

/* Requests to process (one per name argument) */
struct request {
	const char	*name;		/* directive name */
	const char	*newvalue;	/* value to write (NULL for reads) */
	const char	*rawvalue;	/* ucl: value as typed, before
					   newvalue took the library's
					   rendering (NULL otherwise) */
	const char	*ucl_base;	/* ucl: path of the array behind a
					   `name.i...' request (NULL if none) */
	char		*ucl_label;	/* ucl: `name.i...' as typed, then as
					   resolved; shown on reads */
	long		*ucl_idx;	/* ucl: the indices as typed, outermost
					   first; negative counts from the end,
					   the last may be UCL_INDEX_COUNT or
					   UCL_INDEX_APPEND */
	long		*ucl_res;	/* ucl: the same, resolved against the
					   arrays (negative: out of range) */
	size_t		 ucl_nidx;
	enum bsdconf_op	op;		/* assignment operator (writes) */
	char		listop;		/* `+' append / `-' strike words */
	char		*value;		/* value read from file(s) */
	int		found;		/* directive seen while parsing */
	int		append_present; /* identical make `+=value' line */
	int		remove;		/* nonzero for `-x' removals */
	int		srcidx;		/* conf file that last set the value
					   (authoritative); -1 if unset */
	uint32_t	line;		/* 1-based line of that definition */
	unsigned char	*infile;	/* per-file presence map */
	struct trail_step *trail;	/* `-V' assignment steps */
	size_t		ntrail;
	struct make_assign *assigns;	/* make `-=': per-statement pieces */
	size_t		nassigns;
};

/* Union of all directives, for `-a' against multi-file targets */
struct dumpent {
	char		*name;		/* directive name */
	char		*value;		/* last value seen */
	int		srcidx;		/* conf file that last set the value */
	uint32_t	line;		/* 1-based line of that definition */
	struct trail_step *trail;	/* `-V' assignment steps */
	size_t		ntrail;
};

/* Request / dump state (defined in sysconf.c) */
extern struct request	*reqs;
extern unsigned int	nreqs;
extern struct dumpent	*dumps;
extern size_t		ndumps;
extern size_t		dumpsize;

/* Resolved target state */
extern char		**conf_files;
extern size_t		nconf_files;
extern size_t		conf_scanidx;
extern size_t		write_idx;
extern int		defaults_idx;
extern unsigned char	*conf_ro;
extern bool		ucl_target;
extern bool		pkg_target;
extern size_t		pkg_conf_idx;
extern size_t		pkg_repos_idx;
extern enum bsdconf_format format;
extern enum bsdconf_format ucl_format;
extern enum bsdconf_format pkg_format;

/* Display / mode flags */
extern const char	*pgm;
extern bool		check;
extern bool		defaults_only;
extern bool		dump_all;
extern bool		existing_only;
extern bool		ignore_unknown;
extern bool		list_all;
extern bool		list_files;
extern bool		name_only;
extern bool		quiet;
extern bool		remove_mode;
extern bool		set_knobs;
extern bool		show_desc;
extern bool		show_equals;
extern bool		show_file;
extern bool		value_only;
extern bool		verbose;
extern bool		verbose_trail;
extern bool		with_defaults;

/* Option arguments */
extern const char	*file;
extern bool		file_stdin;
extern const char	*jailname;
extern const char	*module;
extern const char	*rootdir;

/* sysconf_print.c */
int		 make_knob_name_p(const char *_name);
int		 make_knob_p(const char *_name);
void		 warn_with_equals_no(const char *_name, const char *_value,
		    const char *_path, uint32_t _line);
void		 print_pair(const char *_path, const char *_directive,
		    const char *_value);
const char	*op_str(enum bsdconf_op _op);
int		 trail_push(struct trail_step **_trailp, size_t *_ntrailp,
		    size_t _fileidx, uint32_t _line, enum bsdconf_op _op,
		    const char *_piece, const char *_effective);
void		 trail_free(struct trail_step *_trail, size_t _ntrail);
void		 print_trail(const char *_name,
		    const struct trail_step *_trail, size_t _ntrail);
void		 print_write_echo(const char *_path, uint32_t _line,
		    const char *_name, enum bsdconf_op _op, const char *_oldv,
		    const char *_newv, int _changed, int _added, int _removed);

/* sysconf_resolve.c */
char		*defaults_path(void);
void		 resolve_target(const char *_target);
int		 do_list(void);

/* sysconf_scan.c */
char		*make_apply(enum bsdconf_op _op, const char *_old,
		    const char *_piece);
int		 scan_record(uint32_t _line, const char *_directive,
		    const char *_value);
int		 scan_pass(int _sandbox);

/* sysconf_edit_assign.c */
void		 split_request(char *_arg, struct request *_req, int _remove);
int		 list_has(const char *_list, const char *_word, size_t _wlen);
char		*list_merge(const char *_current, const char *_words, int _op);
int		 merge_list_requests(void);

/* sysconf_edit_make.c */
int		 make_strike_p(const struct request *_req);
int		 make_assign_push(struct request *_req, enum bsdconf_op _op,
		    const char *_piece, uint32_t _line);
int		 do_make_strikes(int _check_only);

/* sysconf_edit_write.c */
int		 do_checks(void);
int		 do_writes(void);

/* sysconf_query_value.c */
int		 do_reads(void);

/* sysconf_query_desc.c */
void		 load_descriptions(void);
void		 print_desc(const char *_directive);
int		 describe_defaults(void);

/* sysconf_query_sysctl.c */
#if defined(__FreeBSD__)
int		 sysctl_descr(const char *_name, char *_buf, size_t _bufsize);
int		 describe_sysctl(void);
int		 sysctl_writable(const char *_name, const char *_value,
		    int _quiet_unknown);
#endif

/* sysconf-targets.c */
int		 find_operand(int _argc, char **_argv, int _from, int _past_dd);
const char	*resolve_keyword(const char *_token,
		    const char *const *_keywords, const char *_what);
const char	*target_resolve(int *_argcp, char **_argv);

/* sysconf.c */
void		 usage(void);
void		 help(void);

/* sysconf_ucl.c (ucl_object_t is only known with <ucl.h>) */
struct ucl_object_s;
int		 ucl_register(void);
int		 ucl_scan_fd(int _fd);
void		 ucl_canon_requests(void);
void		 ucl_index_requests(void);
bool		 ucl_list_merge(struct request *_req);
char		*ucl_render(const struct ucl_object_s *_obj);
int		 ucl_do_writes(void);

/* sysconf_ucl_text.c */
char		*ucl_text_edit(const char *_text, size_t _len,
		    const char *_name, const struct ucl_object_s *_val,
		    const char *_raw, const char *_style_src,
		    size_t _style_len, bool *_changed);
char		*ucl_text_ready(const char *_text, size_t _len,
		    size_t *_outlen);

/* sysconf_pkg.c */
int		 pkg_register(void);
void		 pkg_prepare(void);

/* sysconf_poudriere.c */
int		 poudriere_longopt_p(const char *_arg);
int		 poudriere_dispatch(int _argc, char **_argv, int _tgt);
int		 poudriere_native(void);
void		 poudriere_setup(void);
void		 poudriere_usage(void);
void		 poudriere_help(void);

#endif /* !_SYSCONF_PRIV_H_ */
