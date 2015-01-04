/*
 * Copyright (c) 2014, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 * Phil Shafer, July 2014
 */

/**
 * libxo provides a means of generating text, XML, and JSON output
 * using a single set of function calls, maximizing the value of output
 * while minimizing the cost/impact on the code.
 */

#ifndef INCLUDE_XO_H
#define INCLUDE_XO_H

/** Formatting types */
typedef unsigned xo_style_t;
#define XO_STYLE_TEXT	0	/** Generate text output */
#define XO_STYLE_XML	1	/** Generate XML output */
#define XO_STYLE_JSON	2	/** Generate JSON output */
#define XO_STYLE_HTML	3	/** Generate HTML output */

/** Flags for libxo */
typedef unsigned long xo_xof_flags_t;
#define XOF_CLOSE_FP	(1<<0)	/** Close file pointer on xo_close() */
#define XOF_PRETTY	(1<<1)	/** Make 'pretty printed' output */
#define XOF_DIV_OPEN	(1<<2)	/** Internal use only: a <div> is open */
#define XOF_LINE_OPEN	(1<<3)	/** Internal use only: a <div class="line"> */

#define XOF_WARN	(1<<4)	/** Generate warnings for broken calls */
#define XOF_XPATH	(1<<5)	/** Emit XPath attributes in HTML  */
#define XOF_INFO	(1<<6)	/** Emit additional info fields (HTML) */
#define XOF_WARN_XML	(1<<7)	/** Emit warnings in XML (on stdout) */

#define XOF_NO_ENV	(1<<8)	/** Don't look at the LIBXO_OPTIONS env var */
#define XOF_NO_VA_ARG	(1<<9)	/** Don't advance va_list w/ va_arg() */
#define XOF_DTRT	(1<<10)	/** Enable "do the right thing" mode */
#define XOF_KEYS	(1<<11)	/** Flag 'key' fields for xml and json */

#define XOF_IGNORE_CLOSE (1<<12) /** Ignore errors on close tags */
#define XOF_NOT_FIRST	(1<<13)	 /* Not the first item (JSON)  */
#define XOF_NO_LOCALE	(1<<14)	 /** Don't bother with locale */
#define XOF_TOP_EMITTED	(1<<15)	 /* The top JSON braces have been emitted  */

#define XOF_NO_TOP	(1<<16)	/** Don't emit the top braces in JSON */
#define XOF_ANCHOR	(1<<17)	/** An anchor is in place  */
#define XOF_UNITS	(1<<18)	/** Encode units in XML */
#define XOF_UNITS_PENDING (1<<19) /** We have a units-insertion pending */

#define XOF_UNDERSCORES	(1<<20)	/** Replace dashes with underscores (JSON)  */
#define XOF_COLUMNS	(1<<21)	/** xo_emit should return a column count */
#define XOF_FLUSH	(1<<22)	/** Flush after each xo_emit call */

/*
 * The xo_info_t structure provides a mapping between names and
 * additional data emitted via HTML.
 */
typedef struct xo_info_s {
    const char *xi_name;	/* Name of the element */
    const char *xi_type;	/* Type of field */
    const char *xi_help;	/* Description of field */
} xo_info_t;

struct xo_handle_s;		/* Opaque structure forward */
typedef struct xo_handle_s xo_handle_t; /* Handle for XO output */

typedef int (*xo_write_func_t)(void *, const char *);
typedef void (*xo_close_func_t)(void *);
typedef void *(*xo_realloc_func_t)(void *, size_t);
typedef void (*xo_free_func_t)(void *);

/*
 * The formatter function mirrors "vsnprintf", with an additional argument
 * of the xo handle.  The caller should return the number of bytes _needed_
 * to fit the data, even if this exceeds 'len'.
 */
typedef int (*xo_formatter_t)(xo_handle_t *, char *, int,
				const char *, va_list);
typedef void (*xo_checkpointer_t)(xo_handle_t *, va_list, int);

xo_handle_t *
xo_create (xo_style_t style, xo_xof_flags_t flags);

xo_handle_t *
xo_create_to_file (FILE *fp, xo_style_t style, xo_xof_flags_t flags);

void
xo_destroy (xo_handle_t *xop);

void
xo_set_writer (xo_handle_t *xop, void *opaque, xo_write_func_t write_func,
	       xo_close_func_t close_func);

void
xo_set_allocator (xo_realloc_func_t realloc_func, xo_free_func_t free_func);

void
xo_set_style (xo_handle_t *xop, xo_style_t style);

xo_style_t
xo_get_style (xo_handle_t *xop);

int
xo_set_style_name (xo_handle_t *xop, const char *style);

int
xo_set_options (xo_handle_t *xop, const char *input);

xo_xof_flags_t
xo_get_flags (xo_handle_t *xop);

void
xo_set_flags (xo_handle_t *xop, xo_xof_flags_t flags);

void
xo_clear_flags (xo_handle_t *xop, xo_xof_flags_t flags);

void
xo_set_info (xo_handle_t *xop, xo_info_t *infop, int count);

void
xo_set_formatter (xo_handle_t *xop, xo_formatter_t func, xo_checkpointer_t);

void
xo_set_depth (xo_handle_t *xop, int depth);

int
xo_emit_hv (xo_handle_t *xop, const char *fmt, va_list vap);

int
xo_emit_h (xo_handle_t *xop, const char *fmt, ...);

int
xo_emit (const char *fmt, ...);

int
xo_open_container_h (xo_handle_t *xop, const char *name);

int
xo_open_container (const char *name);

int
xo_open_container_hd (xo_handle_t *xop, const char *name);

int
xo_open_container_d (const char *name);

int
xo_close_container_h (xo_handle_t *xop, const char *name);

int
xo_close_container (const char *name);

int
xo_close_container_hd (xo_handle_t *xop);

int
xo_close_container_d (void);

int
xo_open_list_h (xo_handle_t *xop, const char *name);

int
xo_open_list (const char *name);

int
xo_open_list_hd (xo_handle_t *xop, const char *name);

int
xo_open_list_d (const char *name);

int
xo_close_list_h (xo_handle_t *xop, const char *name);

int
xo_close_list (const char *name);

int
xo_close_list_hd (xo_handle_t *xop);

int
xo_close_list_d (void);

int
xo_open_instance_h (xo_handle_t *xop, const char *name);

int
xo_open_instance (const char *name);

int
xo_open_instance_hd (xo_handle_t *xop, const char *name);

int
xo_open_instance_d (const char *name);

int
xo_close_instance_h (xo_handle_t *xop, const char *name);

int
xo_close_instance (const char *name);

int
xo_close_instance_hd (xo_handle_t *xop);

int
xo_close_instance_d (void);

int
xo_attr_h (xo_handle_t *xop, const char *name, const char *fmt, ...);

int
xo_attr_hv (xo_handle_t *xop, const char *name, const char *fmt, va_list vap);

int
xo_attr (const char *name, const char *fmt, ...);

void
xo_error_hv (xo_handle_t *xop, const char *fmt, va_list vap);

void
xo_error_h (xo_handle_t *xop, const char *fmt, ...);

void
xo_error (const char *fmt, ...);

void
xo_flush_h (xo_handle_t *xop);

void
xo_flush (void);

void
xo_finish_h (xo_handle_t *xop);

void
xo_finish (void);

void
xo_set_leading_xpath (xo_handle_t *xop, const char *path);

void
xo_warn_hc (xo_handle_t *xop, int code, const char *fmt, ...);

void
xo_warn_c (int code, const char *fmt, ...);

void
xo_warn (const char *fmt, ...);

void
xo_warnx (const char *fmt, ...);

void
xo_err (int eval, const char *fmt, ...);

void
xo_errx (int eval, const char *fmt, ...);

void
xo_errc (int eval, int code, const char *fmt, ...);

void
xo_message_hcv (xo_handle_t *xop, int code, const char *fmt, va_list vap);

void
xo_message_hc (xo_handle_t *xop, int code, const char *fmt, ...);

void
xo_message_c (int code, const char *fmt, ...);

void
xo_message (const char *fmt, ...);

void
xo_no_setlocale (void);

int
xo_parse_args (int argc, char **argv);

/*
 * This is the "magic" number returned by libxo-supporting commands
 * when passed the equally magic "--libxo-check" option.  If you
 * return this, we can assume that since you know the magic handshake,
 * you'll happily handle future --libxo options and not do something
 * violent like reboot the box or create another hole in the ozone
 * layer.
 */
#define XO_HAS_LIBXO	121

/*
 * externs for our version number strings
 */
extern const char xo_version[];
extern const char xo_version_extra[];

#endif /* INCLUDE_XO_H */
