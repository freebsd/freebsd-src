/*
 * Copyright (c) 2002-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _BSDCONF_H_
#define _BSDCONF_H_

#ifdef __FreeBSD__
#include <sys/cdefs.h>
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Supplant functionality missing on non-FreeBSD systems (musl libc, for
 * example, provides no <sys/cdefs.h>). On glibc, <stdint.h> pulls in the
 * definitions via <features.h>.
 */
#ifndef __BEGIN_DECLS
#ifdef __cplusplus
#define __BEGIN_DECLS	extern "C" {
#define __END_DECLS	}
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif
#endif

/*
 * Library version info
 */
#define BSDCONF_VERSION		"1.1.1 2026-09-16"
#define BSDCONF_VERSION_MAJOR	1
#define BSDCONF_VERSION_MINOR	1
#define BSDCONF_VERSION_PATCH	1

/*
 * Union for storing various types of data in a single common container.
 *
 * NB: When writing a value with bsdconf_put(), the caller supplies the value
 * as a NUL-terminated string via the `str' member regardless of `type'; the
 * type only governs how the value is rendered (see bsdconf_put() below).
 * Signed and unsigned 32- and 64-bit members match sysctl(3) CTLTYPE widths.
 * Until CTLTYPE grows a floating-point kind, the union needs no float member;
 * a value that happens to contain a decimal point is still read and written
 * as a string.
 */
union bsdconf_value {
	void		*data;		/* Opaque pointer (DATA1..DATA3) */
	char		*str;		/* Pointer to NUL-terminated string */
	char		**strarray;	/* Pointer to an array of strings */
	int32_t		num;		/* Signed 32-bit integer value */
	uint32_t	u_num;		/* Unsigned 32-bit integer value */
	int64_t		num64;		/* Signed 64-bit integer value */
	uint64_t	u_num64;	/* Unsigned 64-bit integer value */
	bool		boolean;	/* Boolean value */
};

/*
 * Option types (based on above value union)
 */
enum bsdconf_type {
	BSDCONF_TYPE_NONE	= 0x0000, /* directives with no value */
	BSDCONF_TYPE_BOOL	= 0x0001, /* boolean */
	BSDCONF_TYPE_INT	= 0x0002, /* signed 32-bit integer */
	BSDCONF_TYPE_UINT	= 0x0004, /* unsigned 32-bit integer */
	BSDCONF_TYPE_STR	= 0x0008, /* string pointer */
	BSDCONF_TYPE_STRARRAY	= 0x0010, /* string array pointer */
	BSDCONF_TYPE_DATA1	= 0x0020, /* void data type-1 (open) */
	BSDCONF_TYPE_DATA2	= 0x0040, /* void data type-2 (open) */
	BSDCONF_TYPE_DATA3	= 0x0080, /* void data type-3 (open) */
	BSDCONF_TYPE_INT64	= 0x0100, /* signed 64-bit integer */
	BSDCONF_TYPE_UINT64	= 0x0200, /* unsigned 64-bit integer */
	BSDCONF_TYPE_RESERVED	= 0x0400, /* reserved */
};

/*
 * Assignment operators. The default for a given file format is
 * BSDCONF_OP_ASSIGN; the remainder require BSDCONF_OPERATOR_EQUALS (make(1)
 * style configuration files such as make.conf(5)).
 */
enum bsdconf_op {
	BSDCONF_OP_DEFAULT = 0,		/* format's natural operator */
	BSDCONF_OP_ASSIGN,		/* `=' assign */
	BSDCONF_OP_APPEND,		/* `+=' append (make) */
	BSDCONF_OP_COND,		/* `?=' assign if undefined (make) */
	BSDCONF_OP_EXPAND,		/* `:=' assign with expansion (make) */
	BSDCONF_OP_SHELL,		/* `!=' assign shell output (make) */
};

/*
 * Known configuration file formats (see bsdconf_format(3)). Formats numbered
 * BSDCONF_FORMAT_USER and above are assigned by bsdconf_format_register().
 */
enum bsdconf_format {
	BSDCONF_FORMAT_GENERIC = 0,	/* quote values only when required */
	BSDCONF_FORMAT_LOADER,		/* loader.conf(5); always quoted */
	BSDCONF_FORMAT_SYSCTL,		/* sysctl.conf(5); quote when needed */
	BSDCONF_FORMAT_MAKE,		/* make.conf(5); `+=' et al. */
	BSDCONF_FORMAT_SRC,		/* src build triad; make(1) syntax */
	BSDCONF_FORMAT_USER = 32,	/* first registered format */
};

/*
 * A single entry in a format's ordered list of configuration sources. Most
 * formats are backed by more than one file (read in a deterministic order
 * with directives in later files overriding earlier ones) and some pull
 * additional files from drop-in directories.
 */
enum bsdconf_source_type {
	BSDCONF_SOURCE_FILE = 0,	/* a single file */
	BSDCONF_SOURCE_DIR,		/* each `*.conf' in a directory */
	BSDCONF_SOURCE_MODDIR,		/* `<module>.conf' in a directory */
};
struct bsdconf_source {
	enum bsdconf_source_type type;	/* how to interpret path */
	const char	*path;		/* file or directory path */
};

/*
 * The format descriptor: a read-only table of properties characterizing a
 * configuration file format (no relation to file descriptors). The built-in
 * formats (above) are described by format descriptors of this same shape; a
 * new format is "bolted on" by registering a format descriptor of its own --
 * typically derived from the built-in it most resembles (see
 * bsdconf_format_derive() below) with only the differing members adjusted.
 *
 * A format whose backing files are themselves configuration data -- the
 * boot loader reads only /boot/defaults/loader.conf and discovers every
 * other file from the loader_conf_files, loader_conf_dirs, and
 * local_loader_conf_files directives it finds along the way -- describes
 * that machinery with the `defaults' member quintet below, and
 * bsdconf_format_files() performs the same discovery the consumer does.
 * The static `sources' list remains as the fallback for systems whose
 * defaults file is missing.
 */
struct bsdconf_format_def {
	const char	*keyword;	/* target keyword (or NULL) */
	const char	*path;		/* default write path (or NULL) */
	const struct bsdconf_source
			*sources;	/* ordered sources; NULL-path
					   terminated (or NULL if `path'
					   is the only source) */
	const char	*defaults;	/* defaults file (or NULL) */
	const char	*defaults_env;	/* environment variable overriding
					   the defaults file (or NULL) */
	const char	*files_directive; /* directive listing conf files */
	const char	*dirs_directive;  /* directive listing drop-in dirs */
	const char	*local_directive; /* directive listing local files */
	uint16_t	processing;	/* processing_options bitmask */
	uint16_t	put;		/* put_options bitmask */
};

/*
 * Options to bsdconf_parse() and bsdconf_put() for processing_options bitmask
 */
enum bsdconf_processing {
	BSDCONF_BREAK_ON_EQUALS		= 0x0001, /* stop at `=' */
	BSDCONF_BREAK_ON_SEMICOLON	= 0x0002, /* `;' starts a new line */
	BSDCONF_CASE_SENSITIVE		= 0x0004, /* applies to directives */
	BSDCONF_REQUIRE_EQUALS		= 0x0008, /* assignment directives */
	BSDCONF_STRICT_EQUALS		= 0x0010, /* `=' part of directive */
	BSDCONF_OPERATOR_EQUALS		= 0x0020, /* `+=' `?=' `:=' `!=' */
};

/*
 * Options to bsdconf_put() for put_options bitmask
 */
enum bsdconf_put_flags {
	BSDCONF_PUT_NO_DUPLICATES	= 0x0001, /* error if found twice */
	BSDCONF_PUT_ALLOW_EMPTY		= 0x0002, /* allow empty SET_VALUE */
	BSDCONF_PUT_BACKUP		= 0x0004, /* back up file (`.bak') */
	BSDCONF_PUT_UNQUOTED		= 0x0008, /* value.str as file text */
	BSDCONF_PUT_QUOTE_ALWAYS	= 0x0010, /* always quote on output */
};

/*
 * Per-directive actions for bsdconf_put()
 */
enum bsdconf_action {
	BSDCONF_ACTION_SET_VALUE	= 0x0000, /* set/replace (default) */
	BSDCONF_ACTION_CHECK		= 0x0001, /* compare against current */
	BSDCONF_ACTION_REMOVE		= 0x0002, /* remove from config */
};

/*
 * Per-directive result codes set by bsdconf_put()
 */
enum bsdconf_result {
	BSDCONF_DIRECTIVE_FOUND		= 0x0001, /* vs not found (see added) */
	BSDCONF_VALUE_CHANGED		= 0x0002, /* vs no change required */
	BSDCONF_DIRECTIVE_ADDED		= 0x0004, /* vs already existed */
	BSDCONF_DIRECTIVE_REMOVED	= 0x0008, /* vs not found */
};

/*
 * Anatomy of a config file option; used for both reading and writing.
 *
 * When parsing with bsdconf_parse(), `directive' is an fnmatch(3) pattern and
 * `parse' is invoked for each matching statement (with `op' set to the
 * statement's assignment operator beforehand).
 *
 * When writing with bsdconf_put(), `directive' is an exact token (a pattern
 * is not a writable target), `action' selects the operation, and `result'
 * and `line' report what was done. A non-zero `match_line' restricts the
 * put to the statement on that physical line (0 matches any, as before);
 * make(1) `+=' SET_VALUE still appends a new line when `match_line' is 0,
 * but rewrites the matched statement when `match_line' selects it.
 */
struct bsdconf_option {
	enum bsdconf_type	type;		/* Option value type */
	const char		*directive;	/* config file keyword */
	union bsdconf_value	value;		/* NB: set by parse action;
						 *     value to write for put */
	enum bsdconf_op		op;		/* assignment operator */
	uint8_t			action;		/* bsdconf_put() action */
	uint16_t		result;		/* NB: set by bsdconf_put() */
	uint32_t		line;		/* NB: set by bsdconf_put() */
	uint32_t		match_line;	/* put: 0=any, else only line */

	/*
	 * Function pointer; action to be taken when the directive is found
	 * by bsdconf_parse(). Non-zero return aborts the parse (and is
	 * propagated to the bsdconf_parse() caller).
	 */
	int (*parse)(struct bsdconf_option *option, uint32_t line,
	    char *directive, char *value);
};

/*
 * Function prototypes
 *
 * All functions returning int return zero on success (except
 * bsdconf_spool(), which returns a new file descriptor); otherwise -1 (or
 * the non-zero result of a parse call-back) and errno should be consulted.
 */
__BEGIN_DECLS
int			 bsdconf_parse(struct bsdconf_option _options[],
			    const char *_path,
			    int (*_unknown)(struct bsdconf_option *_option,
			    uint32_t _line, char *_directive, char *_value),
			    uint16_t _processing_options);
int			 bsdconf_fparse(struct bsdconf_option _options[],
			    int _fd,
			    int (*_unknown)(struct bsdconf_option *_option,
			    uint32_t _line, char *_directive, char *_value),
			    uint16_t _processing_options);
int			 bsdconf_spool(int _fd);
struct bsdconf_option	*bsdconf_get_option(struct bsdconf_option _options[],
			    const char *_directive);
char			*bsdconf_unquote(char *_value);
int			 bsdconf_set_option(struct bsdconf_option _options[],
			    const char *_directive,
			    union bsdconf_value *_value);
int			 bsdconf_put(struct bsdconf_option _options[],
			    const char *_path, uint16_t _processing_options,
			    uint16_t _put_options);

/*
 * Format abstraction layer (see bsdconf_format(3))
 */
int			 bsdconf_format_derive(enum bsdconf_format _base,
			    struct bsdconf_format_def *_def);
int			 bsdconf_format_files(enum bsdconf_format _format,
			    const char *_rootdir, const char *_module,
			    const char *_defaults, char ***_filesp,
			    size_t *_nfilesp, size_t *_write_idxp);
void			 bsdconf_format_files_free(char **_files,
			    size_t _nfiles);
int			 bsdconf_format_find(const char *_keyword,
			    enum bsdconf_format *_format);
enum bsdconf_format	 bsdconf_format_guess(const char *_path);
const struct bsdconf_format_def
			*bsdconf_format_lookup(enum bsdconf_format _format);
const char		*bsdconf_format_path(enum bsdconf_format _format);
uint16_t		 bsdconf_format_processing(
			    enum bsdconf_format _format);
uint16_t		 bsdconf_format_put(enum bsdconf_format _format);
int			 bsdconf_format_register(
			    const struct bsdconf_format_def *_def,
			    enum bsdconf_format *_format);
__END_DECLS

#endif /* !_BSDCONF_H_ */
