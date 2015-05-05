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
 * libxo provides a means of generating text, XML, JSON, and HTML output
 * using a single set of function calls, maximizing the value of output
 * while minimizing the cost/impact on the code.
 */

#ifndef INCLUDE_XO_H
#define INCLUDE_XO_H

#include <sys/types.h>

#ifdef __dead2
#define NORETURN __dead2
#else
#define NORETURN
#endif /* __dead2 */

/*
 * Normally we'd use the HAVE_PRINTFLIKE define triggered by the
 * --enable-printflike option to configure, but we don't install
 * our internal "xoconfig.h", and I'd rather not.  Taking the
 * coward's path, we'll turn it on inside a #if that allows
 * others to turn it off where needed.  Not ideal, but functional.
 */
#if !defined(NO_PRINTFLIKE) && !defined(__linux__)
#define PRINTFLIKE(_x, _y) __printflike(_x, _y)
#else
#define PRINTFLIKE(_x, _y)
#endif /* NO_PRINTFLIKE */

/** Formatting types */
typedef unsigned xo_style_t;
#define XO_STYLE_TEXT	0	/** Generate text output */
#define XO_STYLE_XML	1	/** Generate XML output */
#define XO_STYLE_JSON	2	/** Generate JSON output */
#define XO_STYLE_HTML	3	/** Generate HTML output */

/** Flags for libxo */
typedef unsigned long long xo_xof_flags_t;
#define XOF_BIT(_n) ((xo_xof_flags_t) 1 << (_n))
#define XOF_CLOSE_FP	XOF_BIT(0) /** Close file pointer on xo_close() */
#define XOF_PRETTY	XOF_BIT(1) /** Make 'pretty printed' output */
#define XOF_DIV_OPEN	XOF_BIT(2) /** Internal use only: a <div> is open */
#define XOF_LINE_OPEN	XOF_BIT(3) /** Internal use only: <div class="line"> */

#define XOF_WARN	XOF_BIT(4) /** Generate warnings for broken calls */
#define XOF_XPATH	XOF_BIT(5) /** Emit XPath attributes in HTML  */
#define XOF_INFO	XOF_BIT(6) /** Emit additional info fields (HTML) */
#define XOF_WARN_XML	XOF_BIT(7) /** Emit warnings in XML (on stdout) */

#define XOF_NO_ENV	XOF_BIT(8) /** Don't look at LIBXO_OPTIONS env var */
#define XOF_NO_VA_ARG	XOF_BIT(9) /** Don't advance va_list w/ va_arg() */
#define XOF_DTRT	XOF_BIT(10) /** Enable "do the right thing" mode */
#define XOF_KEYS	XOF_BIT(11) /** Flag 'key' fields for xml and json */

#define XOF_IGNORE_CLOSE XOF_BIT(12) /** Ignore errors on close tags */
#define XOF_NOT_FIRST	XOF_BIT(13) /* Not the first item (JSON)  */
#define XOF_NO_LOCALE	XOF_BIT(14) /** Don't bother with locale */
#define XOF_TOP_EMITTED	XOF_BIT(15) /* The top JSON braces have been emitted  */

#define XOF_NO_TOP	XOF_BIT(16) /** Don't emit the top braces in JSON */
#define XOF_ANCHOR	XOF_BIT(17) /** An anchor is in place  */
#define XOF_UNITS	XOF_BIT(18) /** Encode units in XML */
#define XOF_UNITS_PENDING XOF_BIT(19) /** We have a units-insertion pending */

#define XOF_UNDERSCORES	XOF_BIT(20) /** Replace dashes with underscores (JSON)*/
#define XOF_COLUMNS	XOF_BIT(21) /** xo_emit should return a column count */
#define XOF_FLUSH	XOF_BIT(22) /** Flush after each xo_emit call */
#define XOF_FLUSH_LINE	XOF_BIT(23) /** Flush after each newline */

#define XOF_NO_CLOSE	XOF_BIT(24) /** xo_finish won't close open elements */
#define XOF_COLOR_ALLOWED XOF_BIT(25) /** Allow color/effects to be enabled */
#define XOF_COLOR	XOF_BIT(26) /** Enable color and effects */

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
typedef int (*xo_flush_func_t)(void *);
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
	       xo_close_func_t close_func, xo_flush_func_t flush_func);

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
xo_open_marker_h (xo_handle_t *xop, const char *name);

int
xo_open_marker (const char *name);

int
xo_close_marker_h (xo_handle_t *xop, const char *name);

int
xo_close_marker (const char *name);

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

int
xo_flush_h (xo_handle_t *xop);

int
xo_flush (void);

int
xo_finish_h (xo_handle_t *xop);

int
xo_finish (void);

void
xo_set_leading_xpath (xo_handle_t *xop, const char *path);

void
xo_warn_hc (xo_handle_t *xop, int code, const char *fmt, ...) PRINTFLIKE(3, 4);

void
xo_warn_c (int code, const char *fmt, ...) PRINTFLIKE(2, 3);

void
xo_warn (const char *fmt, ...) PRINTFLIKE(1, 2);

void
xo_warnx (const char *fmt, ...) PRINTFLIKE(1, 2);

void
xo_err (int eval, const char *fmt, ...) NORETURN PRINTFLIKE(2, 3);

void
xo_errx (int eval, const char *fmt, ...) NORETURN PRINTFLIKE(2, 3);

void
xo_errc (int eval, int code, const char *fmt, ...) NORETURN PRINTFLIKE(3, 4);

void
xo_message_hcv (xo_handle_t *xop, int code, const char *fmt, va_list vap);

void
xo_message_hc (xo_handle_t *xop, int code, const char *fmt, ...) PRINTFLIKE(3, 4);

void
xo_message_c (int code, const char *fmt, ...) PRINTFLIKE(2, 3);

void
xo_message (const char *fmt, ...) PRINTFLIKE(1, 2);

void
xo_no_setlocale (void);

/**
 * @brief Lift libxo-specific arguments from a set of arguments
 *
 * libxo-enable programs typically use command line options to enable
 * all the nifty-cool libxo features.  xo_parse_args() makes this simple
 * by pre-processing the command line arguments given to main(), handling
 * and removing the libxo-specific ones, meaning anything starting with
 * "--libxo".  A full description of these arguments is in the base
 * documentation.
 * @param[in] argc Number of arguments (ala #main())
 * @param[in] argc Array of argument strings (ala #main())
 * @return New number of arguments, or -1 for failure.
 */
int
xo_parse_args (int argc, char **argv);

/**
 * This is the "magic" number returned by libxo-supporting commands
 * when passed the equally magic "--libxo-check" option.  If you
 * return this, we can (unsafely) assume that since you know the magic
 * handshake, you'll happily handle future --libxo options and not do
 * something violent like reboot the box or create another hole in the
 * ozone layer.
 */
#define XO_HAS_LIBXO	121

/**
 * externs for libxo's version number strings
 */
extern const char xo_version[];	      /** Base version triple string */
extern const char xo_version_extra[]; /** Extra version magic content */

/**
 * @brief Dump the internal stack of a libxo handle.
 *
 * This diagnostic function is something I will ask you to call from
 * your program when you write to tell me libxo has gone bat-stink
 * crazy and has discarded your list or container or content.  Output
 * content will be what we lovingly call "developer entertainment".
 * @param[in] xop A valid libxo handle, or NULL for the default handle
 */
void
xo_dump_stack (xo_handle_t *xop);

/**
 * @brief Recode the name of the program, suitable for error output.
 *
 * libxo will record the given name for use while generating error
 * messages.  The contents are not copied, so the value must continue
 * to point to a valid memory location.  This allows the caller to change
 * the value, but requires the caller to manage the memory.  Typically
 * this is called with argv[0] from main().
 * @param[in] name The name of the current application program
 */
void
xo_set_program (const char *name);

/**
 * @brief Add a version string to the output, where possible.
 *
 * Adds a version number to the output, suitable for tracking
 * changes in the content.  This is only important for the "encoding"
 * format styles (XML and JSON) and allows a user of the data to
 * discern which version of the data model is in use.
 * @param[in] version The version number, encoded as a string
 */
void
xo_set_version (const char *version);

/**
 * #xo_set_version with a handle.
 * @param[in] xop A valid libxo handle, or NULL for the default handle
 * @param[in] version The version number, encoded as a string
 */
void
xo_set_version_h (xo_handle_t *xop, const char *version);

#endif /* INCLUDE_XO_H */
