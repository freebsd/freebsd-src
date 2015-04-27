/*
 * Copyright (c) 2014-2015, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 * Phil Shafer, July 2014
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <stddef.h>
#include <wchar.h>
#include <locale.h>
#include <sys/types.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>
#include <wctype.h>
#include <getopt.h>

#include "xoconfig.h"
#include "xo.h"
#include "xoversion.h"

#ifdef HAVE_STDIO_EXT_H
#include <stdio_ext.h>
#endif /* HAVE_STDIO_EXT_H */

const char xo_version[] = LIBXO_VERSION;
const char xo_version_extra[] = LIBXO_VERSION_EXTRA;

#ifndef UNUSED
#define UNUSED __attribute__ ((__unused__))
#endif /* UNUSED */

#define XO_INDENT_BY 2	/* Amount to indent when pretty printing */
#define XO_BUFSIZ	(8*1024) /* Initial buffer size */
#define XO_DEPTH	512	 /* Default stack depth */
#define XO_MAX_ANCHOR_WIDTH (8*1024) /* Anything wider is just sillyb */

#define XO_FAILURE_NAME	"failure"

/*
 * xo_buffer_t: a memory buffer that can be grown as needed.  We
 * use them for building format strings and output data.
 */
typedef struct xo_buffer_s {
    char *xb_bufp;		/* Buffer memory */
    char *xb_curp;		/* Current insertion point */
    int xb_size;		/* Size of buffer */
} xo_buffer_t;

/* Flags for the stack frame */
typedef unsigned xo_xsf_flags_t; /* XSF_* flags */
#define XSF_NOT_FIRST	(1<<0)	/* Not the first element */
#define XSF_LIST	(1<<1)	/* Frame is a list */
#define XSF_INSTANCE	(1<<2)	/* Frame is an instance */
#define XSF_DTRT	(1<<3)	/* Save the name for DTRT mode */

#define XSF_CONTENT	(1<<4)	/* Some content has been emitted */
#define XSF_EMIT	(1<<5)	/* Some field has been emitted */
#define XSF_EMIT_KEY	(1<<6)	/* A key has been emitted */
#define XSF_EMIT_LEAF_LIST (1<<7) /* A leaf-list field has been emitted */

/* These are the flags we propagate between markers and their parents */
#define XSF_MARKER_FLAGS \
 (XSF_NOT_FIRST | XSF_CONTENT | XSF_EMIT | XSF_EMIT_KEY | XSF_EMIT_LEAF_LIST )

/*
 * A word about states:  We're moving to a finite state machine (FMS)
 * approach to help remove fragility from the caller's code.  Instead
 * of requiring a specific order of calls, we'll allow the caller more
 * flexibility and make the library responsible for recovering from
 * missed steps.  The goal is that the library should not be capable of
 * emitting invalid xml or json, but the developer shouldn't need
 * to know or understand all the details about these encodings.
 *
 * You can think of states as either states or event, since they
 * function rather like both.  None of the XO_CLOSE_* events will
 * persist as states, since their stack frame will be popped.
 * Same is true of XSS_EMIT, which is an event that asks us to
 * prep for emitting output fields.
 */

/* Stack frame states */
typedef unsigned xo_state_t;
#define XSS_INIT		0      	/* Initial stack state */
#define XSS_OPEN_CONTAINER	1
#define XSS_CLOSE_CONTAINER	2
#define XSS_OPEN_LIST		3
#define XSS_CLOSE_LIST		4
#define XSS_OPEN_INSTANCE	5
#define XSS_CLOSE_INSTANCE	6
#define XSS_OPEN_LEAF_LIST	7
#define XSS_CLOSE_LEAF_LIST	8
#define XSS_DISCARDING		9	/* Discarding data until recovered */
#define XSS_MARKER		10	/* xo_open_marker's marker */
#define XSS_EMIT		11	/* xo_emit has a leaf field */
#define XSS_EMIT_LEAF_LIST	12	/* xo_emit has a leaf-list ({l:}) */
#define XSS_FINISH		13	/* xo_finish was called */

#define XSS_MAX			13

#define XSS_TRANSITION(_old, _new) ((_old) << 8 | (_new))

/*
 * xo_stack_t: As we open and close containers and levels, we
 * create a stack of frames to track them.  This is needed for
 * XOF_WARN and XOF_XPATH.
 */
typedef struct xo_stack_s {
    xo_xsf_flags_t xs_flags;	/* Flags for this frame */
    xo_state_t xs_state;	/* State for this stack frame */
    char *xs_name;		/* Name (for XPath value) */
    char *xs_keys;		/* XPath predicate for any key fields */
} xo_stack_t;

/* "colors" refers to fancy ansi codes */
#define XO_COL_DEFAULT		0
#define XO_COL_BLACK		1
#define XO_COL_RED		2
#define XO_COL_GREEN		3
#define XO_COL_YELLOW		4
#define XO_COL_BLUE		5
#define XO_COL_MAGENTA		6
#define XO_COL_CYAN		7
#define XO_COL_WHITE		8

#define XO_NUM_COLORS		9

/* "effects" refers to fancy ansi codes */
/*
 * Yes, there's no blink.  We're civilized.  We like users.  Blink
 * isn't something one does to someone you like.  Friends don't let
 * friends use blink.  On friends.  You know what I mean.  Blink is
 * like, well, it's like bursting into show tunes at a funeral.  It's
 * just not done.  Not something anyone wants.  And on those rare
 * instances where it might actually be appropriate, it's still wrong.
 * It's likely done my the wrong person for the wrong reason.  Just
 * like blink.  And if I implemented blink, I'd be like a funeral
 * director who adds "Would you like us to burst into show tunes?" on
 * the list of questions asking while making funeral arrangements.
 * It's formalizing wrongness in the wrong way.  And we're just too
 * civilized to do that.   Hhhmph!
 */
#define XO_EFF_RESET		(1<<0)
#define XO_EFF_NORMAL		(1<<1)
#define XO_EFF_BOLD		(1<<2)
#define XO_EFF_UNDERLINE	(1<<3)
#define XO_EFF_INVERSE		(1<<4)

#define XO_EFF_CLEAR_BITS 	XO_EFF_RESET

typedef uint8_t xo_effect_t;
typedef uint8_t xo_color_t;
typedef struct xo_colors_s {
    xo_effect_t xoc_effects;	/* Current effect set */
    xo_color_t xoc_col_fg;	/* Foreground color */
    xo_color_t xoc_col_bg;	/* Background color */
} xo_colors_t;

/*
 * xo_handle_t: this is the principle data structure for libxo.
 * It's used as a store for state, options, and content.
 */
struct xo_handle_s {
    xo_xof_flags_t xo_flags;	/* Flags */
    unsigned short xo_style;	/* XO_STYLE_* value */
    unsigned short xo_indent;	/* Indent level (if pretty) */
    unsigned short xo_indent_by; /* Indent amount (tab stop) */
    xo_write_func_t xo_write;	/* Write callback */
    xo_close_func_t xo_close;	/* Close callback */
    xo_flush_func_t xo_flush;	/* Flush callback */
    xo_formatter_t xo_formatter; /* Custom formating function */
    xo_checkpointer_t xo_checkpointer; /* Custom formating support function */
    void *xo_opaque;		/* Opaque data for write function */
    xo_buffer_t xo_data;	/* Output data */
    xo_buffer_t xo_fmt;	   	/* Work area for building format strings */
    xo_buffer_t xo_attrs;	/* Work area for building XML attributes */
    xo_buffer_t xo_predicate;	/* Work area for building XPath predicates */
    xo_stack_t *xo_stack;	/* Stack pointer */
    int xo_depth;		/* Depth of stack */
    int xo_stack_size;		/* Size of the stack */
    xo_info_t *xo_info;		/* Info fields for all elements */
    int xo_info_count;		/* Number of info entries */
    va_list xo_vap;		/* Variable arguments (stdargs) */
    char *xo_leading_xpath;	/* A leading XPath expression */
    mbstate_t xo_mbstate;	/* Multi-byte character conversion state */
    unsigned xo_anchor_offset;	/* Start of anchored text */
    unsigned xo_anchor_columns;	/* Number of columns since the start anchor */
    int xo_anchor_min_width;	/* Desired width of anchored text */
    unsigned xo_units_offset;	/* Start of units insertion point */
    unsigned xo_columns;	/* Columns emitted during this xo_emit call */
    uint8_t xo_color_map_fg[XO_NUM_COLORS]; /* Foreground color mappings */
    uint8_t xo_color_map_bg[XO_NUM_COLORS]; /* Background color mappings */
    xo_colors_t xo_colors;	/* Current color and effect values */
    xo_buffer_t xo_color_buf;	/* HTML: buffer of colors and effects */
    char *xo_version;		/* Version string */
};

/* Flags for formatting functions */
typedef unsigned long xo_xff_flags_t;
#define XFF_COLON	(1<<0)	/* Append a ":" */
#define XFF_COMMA	(1<<1)	/* Append a "," iff there's more output */
#define XFF_WS		(1<<2)	/* Append a blank */
#define XFF_ENCODE_ONLY	(1<<3)	/* Only emit for encoding formats (xml, json) */

#define XFF_QUOTE	(1<<4)	/* Force quotes */
#define XFF_NOQUOTE	(1<<5)	/* Force no quotes */
#define XFF_DISPLAY_ONLY (1<<6)	/* Only emit for display formats (text and html) */
#define XFF_KEY		(1<<7)	/* Field is a key (for XPath) */

#define XFF_XML		(1<<8)	/* Force XML encoding style (for XPath) */
#define XFF_ATTR	(1<<9)	/* Escape value using attribute rules (XML) */
#define XFF_BLANK_LINE	(1<<10)	/* Emit a blank line */
#define XFF_NO_OUTPUT	(1<<11)	/* Do not make any output */

#define XFF_TRIM_WS	(1<<12)	/* Trim whitespace off encoded values */
#define XFF_LEAF_LIST	(1<<13)	/* A leaf-list (list of values) */
#define XFF_UNESCAPE	(1<<14)	/* Need to printf-style unescape the value */

/*
 * Normal printf has width and precision, which for strings operate as
 * min and max number of columns.  But this depends on the idea that
 * one byte means one column, which UTF-8 and multi-byte characters
 * pitches on its ear.  It may take 40 bytes of data to populate 14
 * columns, but we can't go off looking at 40 bytes of data without the
 * caller's permission for fear/knowledge that we'll generate core files.
 * 
 * So we make three values, distinguishing between "max column" and
 * "number of bytes that we will inspect inspect safely" We call the
 * later "size", and make the format "%[[<min>].[[<size>].<max>]]s".
 *
 * Under the "first do no harm" theory, we default "max" to "size".
 * This is a reasonable assumption for folks that don't grok the
 * MBS/WCS/UTF-8 world, and while it will be annoying, it will never
 * be evil.
 *
 * For example, xo_emit("{:tag/%-14.14s}", buf) will make 14
 * columns of output, but will never look at more than 14 bytes of the
 * input buffer.  This is mostly compatible with printf and caller's
 * expectations.
 *
 * In contrast xo_emit("{:tag/%-14..14s}", buf) will look at however
 * many bytes (or until a NUL is seen) are needed to fill 14 columns
 * of output.  xo_emit("{:tag/%-14.*.14s}", xx, buf) will look at up
 * to xx bytes (or until a NUL is seen) in order to fill 14 columns
 * of output.
 *
 * It's fairly amazing how a good idea (handle all languages of the
 * world) blows such a big hole in the bottom of the fairly weak boat
 * that is C string handling.  The simplicity and completenesss are
 * sunk in ways we haven't even begun to understand.
 */

#define XF_WIDTH_MIN	0	/* Minimal width */
#define XF_WIDTH_SIZE	1	/* Maximum number of bytes to examine */
#define XF_WIDTH_MAX	2	/* Maximum width */
#define XF_WIDTH_NUM	3	/* Numeric fields in printf (min.size.max) */

/* Input and output string encodings */
#define XF_ENC_WIDE	1	/* Wide characters (wchar_t) */
#define XF_ENC_UTF8	2	/* UTF-8 */
#define XF_ENC_LOCALE	3	/* Current locale */

/*
 * A place to parse printf-style format flags for each field
 */
typedef struct xo_format_s {
    unsigned char xf_fc;	/* Format character */
    unsigned char xf_enc;	/* Encoding of the string (XF_ENC_*) */
    unsigned char xf_skip;	/* Skip this field */
    unsigned char xf_lflag;	/* 'l' (long) */
    unsigned char xf_hflag;;	/* 'h' (half) */
    unsigned char xf_jflag;	/* 'j' (intmax_t) */
    unsigned char xf_tflag;	/* 't' (ptrdiff_t) */
    unsigned char xf_zflag;	/* 'z' (size_t) */
    unsigned char xf_qflag;	/* 'q' (quad_t) */
    unsigned char xf_seen_minus; /* Seen a minus */
    int xf_leading_zero;	/* Seen a leading zero (zero fill)  */
    unsigned xf_dots;		/* Seen one or more '.'s */
    int xf_width[XF_WIDTH_NUM]; /* Width/precision/size numeric fields */
    unsigned xf_stars;		/* Seen one or more '*'s */
    unsigned char xf_star[XF_WIDTH_NUM]; /* Seen one or more '*'s */
} xo_format_t;

/*
 * We keep a default handle to allow callers to avoid having to
 * allocate one.  Passing NULL to any of our functions will use
 * this default handle.
 */
static xo_handle_t xo_default_handle;
static int xo_default_inited;
static int xo_locale_inited;
static const char *xo_program;

/*
 * To allow libxo to be used in diverse environment, we allow the
 * caller to give callbacks for memory allocation.
 */
static xo_realloc_func_t xo_realloc = realloc;
static xo_free_func_t xo_free = free;

/* Forward declarations */
static void
xo_failure (xo_handle_t *xop, const char *fmt, ...);

static int
xo_transition (xo_handle_t *xop, xo_xsf_flags_t flags, const char *name,
	       xo_state_t new_state);

static void
xo_buf_append_div (xo_handle_t *xop, const char *class, xo_xff_flags_t flags,
		   const char *name, int nlen,
		   const char *value, int vlen,
		   const char *encoding, int elen);

static void
xo_anchor_clear (xo_handle_t *xop);

/*
 * xo_style is used to retrieve the current style.  When we're built
 * for "text only" mode, we use this function to drive the removal
 * of most of the code in libxo.  We return a constant and the compiler
 * happily removes the non-text code that is not longer executed.  This
 * trims our code nicely without needing to trampel perfectly readable
 * code with ifdefs.
 */
static inline unsigned short
xo_style (xo_handle_t *xop UNUSED)
{
#ifdef LIBXO_TEXT_ONLY
    return XO_STYLE_TEXT;
#else /* LIBXO_TEXT_ONLY */
    return xop->xo_style;
#endif /* LIBXO_TEXT_ONLY */
}

/*
 * Callback to write data to a FILE pointer
 */
static int
xo_write_to_file (void *opaque, const char *data)
{
    FILE *fp = (FILE *) opaque;

    return fprintf(fp, "%s", data);
}

/*
 * Callback to close a file
 */
static void
xo_close_file (void *opaque)
{
    FILE *fp = (FILE *) opaque;

    fclose(fp);
}

/*
 * Callback to flush a FILE pointer
 */
static int
xo_flush_file (void *opaque)
{
    FILE *fp = (FILE *) opaque;

    return fflush(fp);
}

/*
 * Initialize the contents of an xo_buffer_t.
 */
static void
xo_buf_init (xo_buffer_t *xbp)
{
    xbp->xb_size = XO_BUFSIZ;
    xbp->xb_bufp = xo_realloc(NULL, xbp->xb_size);
    xbp->xb_curp = xbp->xb_bufp;
}

/*
 * Reset the buffer to empty
 */
static void
xo_buf_reset (xo_buffer_t *xbp)
{
    xbp->xb_curp = xbp->xb_bufp;
}

/*
 * Reset the buffer to empty
 */
static int
xo_buf_is_empty (xo_buffer_t *xbp)
{
    return (xbp->xb_curp == xbp->xb_bufp);
}

/*
 * Initialize the contents of an xo_buffer_t.
 */
static void
xo_buf_cleanup (xo_buffer_t *xbp)
{
    if (xbp->xb_bufp)
	xo_free(xbp->xb_bufp);
    bzero(xbp, sizeof(*xbp));
}

static int
xo_depth_check (xo_handle_t *xop, int depth)
{
    xo_stack_t *xsp;

    if (depth >= xop->xo_stack_size) {
	depth += 16;
	xsp = xo_realloc(xop->xo_stack, sizeof(xop->xo_stack[0]) * depth);
	if (xsp == NULL) {
	    xo_failure(xop, "xo_depth_check: out of memory (%d)", depth);
	    return 0;
	}

	int count = depth - xop->xo_stack_size;

	bzero(xsp + xop->xo_stack_size, count * sizeof(*xsp));
	xop->xo_stack_size = depth;
	xop->xo_stack = xsp;
    }

    return 0;
}

void
xo_no_setlocale (void)
{
    xo_locale_inited = 1;	/* Skip initialization */
}

/*
 * We need to decide if stdout is line buffered (_IOLBF).  Lacking a
 * standard way to decide this (e.g. getlinebuf()), we have configure
 * look to find __flbf, which glibc supported.  If not, we'll rely on
 * isatty, with the assumption that terminals are the only thing
 * that's line buffered.  We _could_ test for "steam._flags & _IOLBF",
 * which is all __flbf does, but that's even tackier.  Like a
 * bedazzled Elvis outfit on an ugly lap dog sort of tacky.  Not
 * something we're willing to do.
 */
static int
xo_is_line_buffered (FILE *stream)
{
#if HAVE___FLBF
    if (__flbf(stream))
	return 1;
#else /* HAVE___FLBF */
    if (isatty(fileno(stream)))
	return 1;
#endif /* HAVE___FLBF */
    return 0;
}

/*
 * Initialize an xo_handle_t, using both static defaults and
 * the global settings from the LIBXO_OPTIONS environment
 * variable.
 */
static void
xo_init_handle (xo_handle_t *xop)
{
    xop->xo_opaque = stdout;
    xop->xo_write = xo_write_to_file;
    xop->xo_flush = xo_flush_file;

    if (xo_is_line_buffered(stdout))
	xop->xo_flags |= XOF_FLUSH_LINE;

    /*
     * We only want to do color output on terminals, but we only want
     * to do this if the user has asked for color.
     */
    if ((xop->xo_flags & XOF_COLOR_ALLOWED) && isatty(1))
	xop->xo_flags |= XOF_COLOR;

    /*
     * We need to initialize the locale, which isn't really pretty.
     * Libraries should depend on their caller to set up the
     * environment.  But we really can't count on the caller to do
     * this, because well, they won't.  Trust me.
     */
    if (!xo_locale_inited) {
	xo_locale_inited = 1;	/* Only do this once */

	const char *cp = getenv("LC_CTYPE");
	if (cp == NULL)
	    cp = getenv("LANG");
	if (cp == NULL)
	    cp = getenv("LC_ALL");
	if (cp == NULL)
	    cp = "UTF-8";	/* Optimistic? */
	(void) setlocale(LC_CTYPE, cp);
    }

    /*
     * Initialize only the xo_buffers we know we'll need; the others
     * can be allocated as needed.
     */
    xo_buf_init(&xop->xo_data);
    xo_buf_init(&xop->xo_fmt);

    xop->xo_indent_by = XO_INDENT_BY;
    xo_depth_check(xop, XO_DEPTH);

#if !defined(NO_LIBXO_OPTIONS)
    if (!(xop->xo_flags & XOF_NO_ENV)) {
	char *env = getenv("LIBXO_OPTIONS");
	if (env)
	    xo_set_options(xop, env);
    }
#endif /* NO_GETENV */
}

/*
 * Initialize the default handle.
 */
static void
xo_default_init (void)
{
    xo_handle_t *xop = &xo_default_handle;

    xo_init_handle(xop);

    xo_default_inited = 1;
}

/*
 * Does the buffer have room for the given number of bytes of data?
 * If not, realloc the buffer to make room.  If that fails, we
 * return 0 to tell the caller they are in trouble.
 */
static int
xo_buf_has_room (xo_buffer_t *xbp, int len)
{
    if (xbp->xb_curp + len >= xbp->xb_bufp + xbp->xb_size) {
	int sz = xbp->xb_size + XO_BUFSIZ;
	char *bp = xo_realloc(xbp->xb_bufp, sz);
	if (bp == NULL) {
	    /*
	     * XXX If we wanted to put a stick XOF_ENOMEM on xop,
	     * this would be the place to do it.  But we'd need
	     * to churn the code to pass xop in here....
	     */
	    return 0;
	}

	xbp->xb_curp = bp + (xbp->xb_curp - xbp->xb_bufp);
	xbp->xb_bufp = bp;
	xbp->xb_size = sz;
    }

    return 1;
}

/*
 * Cheap convenience function to return either the argument, or
 * the internal handle, after it has been initialized.  The usage
 * is:
 *    xop = xo_default(xop);
 */
static xo_handle_t *
xo_default (xo_handle_t *xop)
{
    if (xop == NULL) {
	if (xo_default_inited == 0)
	    xo_default_init();
	xop = &xo_default_handle;
    }

    return xop;
}

/*
 * Return the number of spaces we should be indenting.  If
 * we are pretty-printing, this is indent * indent_by.
 */
static int
xo_indent (xo_handle_t *xop)
{
    int rc = 0;

    xop = xo_default(xop);

    if (xop->xo_flags & XOF_PRETTY) {
	rc = xop->xo_indent * xop->xo_indent_by;
	if (xop->xo_flags & XOF_TOP_EMITTED)
	    rc += xop->xo_indent_by;
    }

    return (rc > 0) ? rc : 0;
}

static void
xo_buf_indent (xo_handle_t *xop, int indent)
{
    xo_buffer_t *xbp = &xop->xo_data;

    if (indent <= 0)
	indent = xo_indent(xop);

    if (!xo_buf_has_room(xbp, indent))
	return;

    memset(xbp->xb_curp, ' ', indent);
    xbp->xb_curp += indent;
}

static char xo_xml_amp[] = "&amp;";
static char xo_xml_lt[] = "&lt;";
static char xo_xml_gt[] = "&gt;";
static char xo_xml_quot[] = "&quot;";

static int
xo_escape_xml (xo_buffer_t *xbp, int len, int attr)
{
    int slen;
    unsigned delta = 0;
    char *cp, *ep, *ip;
    const char *sp;

    for (cp = xbp->xb_curp, ep = cp + len; cp < ep; cp++) {
	/* We're subtracting 2: 1 for the NUL, 1 for the char we replace */
	if (*cp == '<')
	    delta += sizeof(xo_xml_lt) - 2;
	else if (*cp == '>')
	    delta += sizeof(xo_xml_gt) - 2;
	else if (*cp == '&')
	    delta += sizeof(xo_xml_amp) - 2;
	else if (attr && *cp == '"')
	    delta += sizeof(xo_xml_quot) - 2;
    }

    if (delta == 0)		/* Nothing to escape; bail */
	return len;

    if (!xo_buf_has_room(xbp, delta)) /* No room; bail, but don't append */
	return 0;

    ep = xbp->xb_curp;
    cp = ep + len;
    ip = cp + delta;
    do {
	cp -= 1;
	ip -= 1;

	if (*cp == '<')
	    sp = xo_xml_lt;
	else if (*cp == '>')
	    sp = xo_xml_gt;
	else if (*cp == '&')
	    sp = xo_xml_amp;
	else if (attr && *cp == '"')
	    sp = xo_xml_quot;
	else {
	    *ip = *cp;
	    continue;
	}

	slen = strlen(sp);
	ip -= slen - 1;
	memcpy(ip, sp, slen);
	
    } while (cp > ep && cp != ip);

    return len + delta;
}

static int
xo_escape_json (xo_buffer_t *xbp, int len)
{
    unsigned delta = 0;
    char *cp, *ep, *ip;

    for (cp = xbp->xb_curp, ep = cp + len; cp < ep; cp++) {
	if (*cp == '\\' || *cp == '"')
	    delta += 1;
	else if (*cp == '\n' || *cp == '\r')
	    delta += 1;
    }

    if (delta == 0)		/* Nothing to escape; bail */
	return len;

    if (!xo_buf_has_room(xbp, delta)) /* No room; bail, but don't append */
	return 0;

    ep = xbp->xb_curp;
    cp = ep + len;
    ip = cp + delta;
    do {
	cp -= 1;
	ip -= 1;

	if (*cp == '\\' || *cp == '"') {
	    *ip-- = *cp;
	    *ip = '\\';
	} else if (*cp == '\n') {
	    *ip-- = 'n';
	    *ip = '\\';
	} else if (*cp == '\r') {
	    *ip-- = 'r';
	    *ip = '\\';
	} else {
	    *ip = *cp;
	}
	
    } while (cp > ep && cp != ip);

    return len + delta;
}

/*
 * Append the given string to the given buffer
 */
static void
xo_buf_append (xo_buffer_t *xbp, const char *str, int len)
{
    if (!xo_buf_has_room(xbp, len))
	return;

    memcpy(xbp->xb_curp, str, len);
    xbp->xb_curp += len;
}

/*
 * Append the given NUL-terminated string to the given buffer
 */
static void
xo_buf_append_str (xo_buffer_t *xbp, const char *str)
{
    int len = strlen(str);

    if (!xo_buf_has_room(xbp, len))
	return;

    memcpy(xbp->xb_curp, str, len);
    xbp->xb_curp += len;
}

static void
xo_buf_escape (xo_handle_t *xop, xo_buffer_t *xbp,
	       const char *str, int len, xo_xff_flags_t flags)
{
    if (!xo_buf_has_room(xbp, len))
	return;

    memcpy(xbp->xb_curp, str, len);

    switch (xo_style(xop)) {
    case XO_STYLE_XML:
    case XO_STYLE_HTML:
	len = xo_escape_xml(xbp, len, (flags & XFF_ATTR));
	break;

    case XO_STYLE_JSON:
	len = xo_escape_json(xbp, len);
	break;
    }

    xbp->xb_curp += len;
}

/*
 * Write the current contents of the data buffer using the handle's
 * xo_write function.
 */
static int
xo_write (xo_handle_t *xop)
{
    int rc = 0;
    xo_buffer_t *xbp = &xop->xo_data;

    if (xbp->xb_curp != xbp->xb_bufp) {
	xo_buf_append(xbp, "", 1); /* Append ending NUL */
	xo_anchor_clear(xop);
	rc = xop->xo_write(xop->xo_opaque, xbp->xb_bufp);
	xbp->xb_curp = xbp->xb_bufp;
    }

    /* Turn off the flags that don't survive across writes */
    xop->xo_flags &= ~(XOF_UNITS_PENDING);

    return rc;
}

/*
 * Format arguments into our buffer.  If a custom formatter has been set,
 * we use that to do the work; otherwise we vsnprintf().
 */
static int
xo_vsnprintf (xo_handle_t *xop, xo_buffer_t *xbp, const char *fmt, va_list vap)
{
    va_list va_local;
    int rc;
    int left = xbp->xb_size - (xbp->xb_curp - xbp->xb_bufp);

    va_copy(va_local, vap);

    if (xop->xo_formatter)
	rc = xop->xo_formatter(xop, xbp->xb_curp, left, fmt, va_local);
    else
	rc = vsnprintf(xbp->xb_curp, left, fmt, va_local);

    if (rc >= left) {
	if (!xo_buf_has_room(xbp, rc)) {
	    va_end(va_local);
	    return -1;
	}

	/*
	 * After we call vsnprintf(), the stage of vap is not defined.
	 * We need to copy it before we pass.  Then we have to do our
	 * own logic below to move it along.  This is because the
	 * implementation can have va_list be a pointer (bsd) or a
	 * structure (macosx) or anything in between.
	 */

	va_end(va_local);	/* Reset vap to the start */
	va_copy(va_local, vap);

	left = xbp->xb_size - (xbp->xb_curp - xbp->xb_bufp);
	if (xop->xo_formatter)
	    rc = xop->xo_formatter(xop, xbp->xb_curp, left, fmt, va_local);
	else
	    rc = vsnprintf(xbp->xb_curp, left, fmt, va_local);
    }
    va_end(va_local);

    return rc;
}

/*
 * Print some data thru the handle.
 */
static int
xo_printf_v (xo_handle_t *xop, const char *fmt, va_list vap)
{
    xo_buffer_t *xbp = &xop->xo_data;
    int left = xbp->xb_size - (xbp->xb_curp - xbp->xb_bufp);
    int rc;
    va_list va_local;

    va_copy(va_local, vap);

    rc = vsnprintf(xbp->xb_curp, left, fmt, va_local);

    if (rc > xbp->xb_size) {
	if (!xo_buf_has_room(xbp, rc)) {
	    va_end(va_local);
	    return -1;
	}

	va_end(va_local);	/* Reset vap to the start */
	va_copy(va_local, vap);

	left = xbp->xb_size - (xbp->xb_curp - xbp->xb_bufp);
	rc = vsnprintf(xbp->xb_curp, left, fmt, va_local);
    }

    va_end(va_local);

    if (rc > 0)
	xbp->xb_curp += rc;

    return rc;
}

static int
xo_printf (xo_handle_t *xop, const char *fmt, ...)
{
    int rc;
    va_list vap;

    va_start(vap, fmt);

    rc = xo_printf_v(xop, fmt, vap);

    va_end(vap);
    return rc;
}

/*
 * These next few function are make The Essential UTF-8 Ginsu Knife.
 * Identify an input and output character, and convert it.
 */
static int xo_utf8_bits[7] = { 0, 0x7f, 0x1f, 0x0f, 0x07, 0x03, 0x01 };

static int
xo_is_utf8 (char ch)
{
    return (ch & 0x80);
}

static int
xo_utf8_to_wc_len (const char *buf)
{
    unsigned b = (unsigned char) *buf;
    int len;

    if ((b & 0x80) == 0x0)
	len = 1;
    else if ((b & 0xe0) == 0xc0)
	len = 2;
    else if ((b & 0xf0) == 0xe0)
	len = 3;
    else if ((b & 0xf8) == 0xf0)
	len = 4;
    else if ((b & 0xfc) == 0xf8)
	len = 5;
    else if ((b & 0xfe) == 0xfc)
	len = 6;
    else
	len = -1;

    return len;
}

static int
xo_buf_utf8_len (xo_handle_t *xop, const char *buf, int bufsiz)
{

    unsigned b = (unsigned char) *buf;
    int len, i;

    len = xo_utf8_to_wc_len(buf);
    if (len == -1) {
        xo_failure(xop, "invalid UTF-8 data: %02hhx", b);
	return -1;
    }

    if (len > bufsiz) {
        xo_failure(xop, "invalid UTF-8 data (short): %02hhx (%d/%d)",
		   b, len, bufsiz);
	return -1;
    }

    for (i = 2; i < len; i++) {
	b = (unsigned char ) buf[i];
	if ((b & 0xc0) != 0x80) {
	    xo_failure(xop, "invalid UTF-8 data (byte %d): %x", i, b);
	    return -1;
	}
    }

    return len;
}

/*
 * Build a wide character from the input buffer; the number of
 * bits we pull off the first character is dependent on the length,
 * but we put 6 bits off all other bytes.
 */
static wchar_t
xo_utf8_char (const char *buf, int len)
{
    int i;
    wchar_t wc;
    const unsigned char *cp = (const unsigned char *) buf;

    wc = *cp & xo_utf8_bits[len];
    for (i = 1; i < len; i++) {
	wc <<= 6;
	wc |= cp[i] & 0x3f;
	if ((cp[i] & 0xc0) != 0x80)
	    return (wchar_t) -1;
    }

    return wc;
}

/*
 * Determine the number of bytes needed to encode a wide character.
 */
static int
xo_utf8_emit_len (wchar_t wc)
{
    int len;

    if ((wc & ((1<<7) - 1)) == wc) /* Simple case */
	len = 1;
    else if ((wc & ((1<<11) - 1)) == wc)
	len = 2;
    else if ((wc & ((1<<16) - 1)) == wc)
	len = 3;
    else if ((wc & ((1<<21) - 1)) == wc)
	len = 4;
    else if ((wc & ((1<<26) - 1)) == wc)
	len = 5;
    else
	len = 6;

    return len;
}

static void
xo_utf8_emit_char (char *buf, int len, wchar_t wc)
{
    int i;

    if (len == 1) { /* Simple case */
	buf[0] = wc & 0x7f;
	return;
    }

    for (i = len - 1; i >= 0; i--) {
	buf[i] = 0x80 | (wc & 0x3f);
	wc >>= 6;
    }

    buf[0] &= xo_utf8_bits[len];
    buf[0] |= ~xo_utf8_bits[len] << 1;
}

static int
xo_buf_append_locale_from_utf8 (xo_handle_t *xop, xo_buffer_t *xbp,
				const char *ibuf, int ilen)
{
    wchar_t wc;
    int len;

    /*
     * Build our wide character from the input buffer; the number of
     * bits we pull off the first character is dependent on the length,
     * but we put 6 bits off all other bytes.
     */
    wc = xo_utf8_char(ibuf, ilen);
    if (wc == (wchar_t) -1) {
	xo_failure(xop, "invalid utf-8 byte sequence");
	return 0;
    }

    if (xop->xo_flags & XOF_NO_LOCALE) {
	if (!xo_buf_has_room(xbp, ilen))
	    return 0;

	memcpy(xbp->xb_curp, ibuf, ilen);
	xbp->xb_curp += ilen;

    } else {
	if (!xo_buf_has_room(xbp, MB_LEN_MAX + 1))
	    return 0;

	bzero(&xop->xo_mbstate, sizeof(xop->xo_mbstate));
	len = wcrtomb(xbp->xb_curp, wc, &xop->xo_mbstate);

	if (len <= 0) {
	    xo_failure(xop, "could not convert wide char: %lx",
		       (unsigned long) wc);
	    return 0;
	}
	xbp->xb_curp += len;
    }

    return wcwidth(wc);
}

static void
xo_buf_append_locale (xo_handle_t *xop, xo_buffer_t *xbp,
		      const char *cp, int len)
{
    const char *sp = cp, *ep = cp + len;
    unsigned save_off = xbp->xb_bufp - xbp->xb_curp;
    int slen;
    int cols = 0;

    for ( ; cp < ep; cp++) {
	if (!xo_is_utf8(*cp)) {
	    cols += 1;
	    continue;
	}

	/*
	 * We're looking at a non-ascii UTF-8 character.
	 * First we copy the previous data.
	 * Then we need find the length and validate it.
	 * Then we turn it into a wide string.
	 * Then we turn it into a localized string.
	 * Then we repeat.  Isn't i18n fun?
	 */
	if (sp != cp)
	    xo_buf_append(xbp, sp, cp - sp); /* Append previous data */

	slen = xo_buf_utf8_len(xop, cp, ep - cp);
	if (slen <= 0) {
	    /* Bad data; back it all out */
	    xbp->xb_curp = xbp->xb_bufp + save_off;
	    return;
	}

	cols += xo_buf_append_locale_from_utf8(xop, xbp, cp, slen);

	/* Next time thru, we'll start at the next character */
	cp += slen - 1;
	sp = cp + 1;
    }

    /* Update column values */
    if (xop->xo_flags & XOF_COLUMNS)
	xop->xo_columns += cols;
    if (xop->xo_flags & XOF_ANCHOR)
	xop->xo_anchor_columns += cols;

    /* Before we fall into the basic logic below, we need reset len */
    len = ep - sp;
    if (len != 0) /* Append trailing data */
	xo_buf_append(xbp, sp, len);
}

/*
 * Append the given string to the given buffer
 */
static void
xo_data_append (xo_handle_t *xop, const char *str, int len)
{
    xo_buf_append(&xop->xo_data, str, len);
}

/*
 * Append the given string to the given buffer
 */
static void
xo_data_escape (xo_handle_t *xop, const char *str, int len)
{
    xo_buf_escape(xop, &xop->xo_data, str, len, 0);
}

/*
 * Generate a warning.  Normally, this is a text message written to
 * standard error.  If the XOF_WARN_XML flag is set, then we generate
 * XMLified content on standard output.
 */
static void
xo_warn_hcv (xo_handle_t *xop, int code, int check_warn,
	     const char *fmt, va_list vap)
{
    xop = xo_default(xop);
    if (check_warn && !(xop->xo_flags & XOF_WARN))
	return;

    if (fmt == NULL)
	return;

    int len = strlen(fmt);
    int plen = xo_program ? strlen(xo_program) : 0;
    char *newfmt = alloca(len + 1 + plen + 2); /* NUL, and ": " */

    if (plen) {
	memcpy(newfmt, xo_program, plen);
	newfmt[plen++] = ':';
	newfmt[plen++] = ' ';
    }
    memcpy(newfmt + plen, fmt, len);
    newfmt[len + plen] = '\0';

    if (xop->xo_flags & XOF_WARN_XML) {
	static char err_open[] = "<error>";
	static char err_close[] = "</error>";
	static char msg_open[] = "<message>";
	static char msg_close[] = "</message>";

	xo_buffer_t *xbp = &xop->xo_data;

	xo_buf_append(xbp, err_open, sizeof(err_open) - 1);
	xo_buf_append(xbp, msg_open, sizeof(msg_open) - 1);

	va_list va_local;
	va_copy(va_local, vap);

	int left = xbp->xb_size - (xbp->xb_curp - xbp->xb_bufp);
	int rc = vsnprintf(xbp->xb_curp, left, newfmt, vap);
	if (rc > xbp->xb_size) {
	    if (!xo_buf_has_room(xbp, rc)) {
		va_end(va_local);
		return;
	    }

	    va_end(vap);	/* Reset vap to the start */
	    va_copy(vap, va_local);

	    left = xbp->xb_size - (xbp->xb_curp - xbp->xb_bufp);
	    rc = vsnprintf(xbp->xb_curp, left, fmt, vap);
	}
	va_end(va_local);

	rc = xo_escape_xml(xbp, rc, 1);
	xbp->xb_curp += rc;

	xo_buf_append(xbp, msg_close, sizeof(msg_close) - 1);
	xo_buf_append(xbp, err_close, sizeof(err_close) - 1);

	if (code >= 0) {
	    const char *msg = strerror(code);
	    if (msg) {
		xo_buf_append(xbp, ": ", 2);
		xo_buf_append(xbp, msg, strlen(msg));
	    }
	}

	xo_buf_append(xbp, "\n", 2); /* Append newline and NUL to string */
	(void) xo_write(xop);

    } else {
	vfprintf(stderr, newfmt, vap);
	if (code >= 0) {
	    const char *msg = strerror(code);
	    if (msg)
		fprintf(stderr, ": %s", msg);
	}
	fprintf(stderr, "\n");
    }
}

void
xo_warn_hc (xo_handle_t *xop, int code, const char *fmt, ...)
{
    va_list vap;

    va_start(vap, fmt);
    xo_warn_hcv(xop, code, 0, fmt, vap);
    va_end(vap);
}

void
xo_warn_c (int code, const char *fmt, ...)
{
    va_list vap;

    va_start(vap, fmt);
    xo_warn_hcv(NULL, code, 0, fmt, vap);
    va_end(vap);
}

void
xo_warn (const char *fmt, ...)
{
    int code = errno;
    va_list vap;

    va_start(vap, fmt);
    xo_warn_hcv(NULL, code, 0, fmt, vap);
    va_end(vap);
}

void
xo_warnx (const char *fmt, ...)
{
    va_list vap;

    va_start(vap, fmt);
    xo_warn_hcv(NULL, -1, 0, fmt, vap);
    va_end(vap);
}

void
xo_err (int eval, const char *fmt, ...)
{
    int code = errno;
    va_list vap;

    va_start(vap, fmt);
    xo_warn_hcv(NULL, code, 0, fmt, vap);
    va_end(vap);
    xo_finish();
    exit(eval);
}

void
xo_errx (int eval, const char *fmt, ...)
{
    va_list vap;

    va_start(vap, fmt);
    xo_warn_hcv(NULL, -1, 0, fmt, vap);
    va_end(vap);
    xo_finish();
    exit(eval);
}

void
xo_errc (int eval, int code, const char *fmt, ...)
{
    va_list vap;

    va_start(vap, fmt);
    xo_warn_hcv(NULL, code, 0, fmt, vap);
    va_end(vap);
    xo_finish();
    exit(eval);
}

/*
 * Generate a warning.  Normally, this is a text message written to
 * standard error.  If the XOF_WARN_XML flag is set, then we generate
 * XMLified content on standard output.
 */
void
xo_message_hcv (xo_handle_t *xop, int code, const char *fmt, va_list vap)
{
    static char msg_open[] = "<message>";
    static char msg_close[] = "</message>";
    xo_buffer_t *xbp;
    int rc;
    va_list va_local;

    xop = xo_default(xop);

    if (fmt == NULL || *fmt == '\0')
	return;

    int need_nl = (fmt[strlen(fmt) - 1] != '\n');

    switch (xo_style(xop)) {
    case XO_STYLE_XML:
	xbp = &xop->xo_data;
	if (xop->xo_flags & XOF_PRETTY)
	    xo_buf_indent(xop, xop->xo_indent_by);
	xo_buf_append(xbp, msg_open, sizeof(msg_open) - 1);

	va_copy(va_local, vap);

	int left = xbp->xb_size - (xbp->xb_curp - xbp->xb_bufp);
	rc = vsnprintf(xbp->xb_curp, left, fmt, vap);
	if (rc > xbp->xb_size) {
	    if (!xo_buf_has_room(xbp, rc)) {
		va_end(va_local);
		return;
	    }

	    va_end(vap);	/* Reset vap to the start */
	    va_copy(vap, va_local);

	    left = xbp->xb_size - (xbp->xb_curp - xbp->xb_bufp);
	    rc = vsnprintf(xbp->xb_curp, left, fmt, vap);
	}
	va_end(va_local);

	rc = xo_escape_xml(xbp, rc, 1);
	xbp->xb_curp += rc;

	if (need_nl && code > 0) {
	    const char *msg = strerror(code);
	    if (msg) {
		xo_buf_append(xbp, ": ", 2);
		xo_buf_append(xbp, msg, strlen(msg));
	    }
	}

	xo_buf_append(xbp, msg_close, sizeof(msg_close) - 1);
	if (need_nl)
	    xo_buf_append(xbp, "\n", 2); /* Append newline and NUL to string */
	(void) xo_write(xop);
	break;

    case XO_STYLE_HTML:
	{
	    char buf[BUFSIZ], *bp = buf, *cp;
	    int bufsiz = sizeof(buf);
	    int rc2;

	    va_copy(va_local, vap);

	    rc = vsnprintf(bp, bufsiz, fmt, va_local);
	    if (rc > bufsiz) {
		bufsiz = rc + BUFSIZ;
		bp = alloca(bufsiz);
		va_end(va_local);
		va_copy(va_local, vap);
		rc = vsnprintf(bp, bufsiz, fmt, va_local);
	    }
	    va_end(va_local);
	    cp = bp + rc;

	    if (need_nl) {
		rc2 = snprintf(cp, bufsiz - rc, "%s%s\n",
			       (code > 0) ? ": " : "",
			       (code > 0) ? strerror(code) : "");
		if (rc2 > 0)
		    rc += rc2;
	    }

	    xo_buf_append_div(xop, "message", 0, NULL, 0, bp, rc, NULL, 0);
	}
	break;

    case XO_STYLE_JSON:
	/* No meanings of representing messages in JSON */
	break;

    case XO_STYLE_TEXT:
	rc = xo_printf_v(xop, fmt, vap);
	/*
	 * XXX need to handle UTF-8 widths
	 */
	if (rc > 0) {
	    if (xop->xo_flags & XOF_COLUMNS)
		xop->xo_columns += rc;
	    if (xop->xo_flags & XOF_ANCHOR)
		xop->xo_anchor_columns += rc;
	}

	if (need_nl && code > 0) {
	    const char *msg = strerror(code);
	    if (msg) {
		xo_printf(xop, ": %s", msg);
	    }
	}
	if (need_nl)
	    xo_printf(xop, "\n");

	break;
    }

    (void) xo_flush_h(xop);
}

void
xo_message_hc (xo_handle_t *xop, int code, const char *fmt, ...)
{
    va_list vap;

    va_start(vap, fmt);
    xo_message_hcv(xop, code, fmt, vap);
    va_end(vap);
}

void
xo_message_c (int code, const char *fmt, ...)
{
    va_list vap;

    va_start(vap, fmt);
    xo_message_hcv(NULL, code, fmt, vap);
    va_end(vap);
}

void
xo_message (const char *fmt, ...)
{
    int code = errno;
    va_list vap;

    va_start(vap, fmt);
    xo_message_hcv(NULL, code, fmt, vap);
    va_end(vap);
}

static void
xo_failure (xo_handle_t *xop, const char *fmt, ...)
{
    if (!(xop->xo_flags & XOF_WARN))
	return;

    va_list vap;

    va_start(vap, fmt);
    xo_warn_hcv(xop, -1, 1, fmt, vap);
    va_end(vap);
}

/**
 * Create a handle for use by later libxo functions.
 *
 * Note: normal use of libxo does not require a distinct handle, since
 * the default handle (used when NULL is passed) generates text on stdout.
 *
 * @style Style of output desired (XO_STYLE_* value)
 * @flags Set of XOF_* flags in use with this handle
 */
xo_handle_t *
xo_create (xo_style_t style, xo_xof_flags_t flags)
{
    xo_handle_t *xop = xo_realloc(NULL, sizeof(*xop));

    if (xop) {
	bzero(xop, sizeof(*xop));

	xop->xo_style  = style;
	xop->xo_flags = flags;
	xo_init_handle(xop);
    }

    return xop;
}

/**
 * Create a handle that will write to the given file.  Use
 * the XOF_CLOSE_FP flag to have the file closed on xo_destroy().
 * @fp FILE pointer to use
 * @style Style of output desired (XO_STYLE_* value)
 * @flags Set of XOF_* flags to use with this handle
 */
xo_handle_t *
xo_create_to_file (FILE *fp, xo_style_t style, xo_xof_flags_t flags)
{
    xo_handle_t *xop = xo_create(style, flags);

    if (xop) {
	xop->xo_opaque = fp;
	xop->xo_write = xo_write_to_file;
	xop->xo_close = xo_close_file;
	xop->xo_flush = xo_flush_file;
    }

    return xop;
}

/**
 * Release any resources held by the handle.
 * @xop XO handle to alter (or NULL for default handle)
 */
void
xo_destroy (xo_handle_t *xop_arg)
{
    xo_handle_t *xop = xo_default(xop_arg);

    if (xop->xo_close && (xop->xo_flags & XOF_CLOSE_FP))
	xop->xo_close(xop->xo_opaque);

    xo_free(xop->xo_stack);
    xo_buf_cleanup(&xop->xo_data);
    xo_buf_cleanup(&xop->xo_fmt);
    xo_buf_cleanup(&xop->xo_predicate);
    xo_buf_cleanup(&xop->xo_attrs);
    xo_buf_cleanup(&xop->xo_color_buf);

    if (xop->xo_version)
	xo_free(xop->xo_version);

    if (xop_arg == NULL) {
	bzero(&xo_default_handle, sizeof(xo_default_handle));
	xo_default_inited = 0;
    } else
	xo_free(xop);
}

/**
 * Record a new output style to use for the given handle (or default if
 * handle is NULL).  This output style will be used for any future output.
 *
 * @xop XO handle to alter (or NULL for default handle)
 * @style new output style (XO_STYLE_*)
 */
void
xo_set_style (xo_handle_t *xop, xo_style_t style)
{
    xop = xo_default(xop);
    xop->xo_style = style;
}

xo_style_t
xo_get_style (xo_handle_t *xop)
{
    xop = xo_default(xop);
    return xo_style(xop);
}

static int
xo_name_to_style (const char *name)
{
    if (strcmp(name, "xml") == 0)
	return XO_STYLE_XML;
    else if (strcmp(name, "json") == 0)
	return XO_STYLE_JSON;
    else if (strcmp(name, "text") == 0)
	return XO_STYLE_TEXT;
    else if (strcmp(name, "html") == 0)
	return XO_STYLE_HTML;

    return -1;
}

/*
 * Convert string name to XOF_* flag value.
 * Not all are useful.  Or safe.  Or sane.
 */
static unsigned
xo_name_to_flag (const char *name)
{
    if (strcmp(name, "pretty") == 0)
	return XOF_PRETTY;
    if (strcmp(name, "warn") == 0)
	return XOF_WARN;
    if (strcmp(name, "xpath") == 0)
	return XOF_XPATH;
    if (strcmp(name, "info") == 0)
	return XOF_INFO;
    if (strcmp(name, "warn-xml") == 0)
	return XOF_WARN_XML;
    if (strcmp(name, "color") == 0)
	return XOF_COLOR_ALLOWED;
    if (strcmp(name, "columns") == 0)
	return XOF_COLUMNS;
    if (strcmp(name, "dtrt") == 0)
	return XOF_DTRT;
    if (strcmp(name, "flush") == 0)
	return XOF_FLUSH;
    if (strcmp(name, "keys") == 0)
	return XOF_KEYS;
    if (strcmp(name, "ignore-close") == 0)
	return XOF_IGNORE_CLOSE;
    if (strcmp(name, "not-first") == 0)
	return XOF_NOT_FIRST;
    if (strcmp(name, "no-locale") == 0)
	return XOF_NO_LOCALE;
    if (strcmp(name, "no-top") == 0)
	return XOF_NO_TOP;
    if (strcmp(name, "units") == 0)
	return XOF_UNITS;
    if (strcmp(name, "underscores") == 0)
	return XOF_UNDERSCORES;

    return 0;
}

int
xo_set_style_name (xo_handle_t *xop, const char *name)
{
    if (name == NULL)
	return -1;

    int style = xo_name_to_style(name);
    if (style < 0)
	return -1;

    xo_set_style(xop, style);
    return 0;
}

/*
 * Set the options for a handle using a string of options
 * passed in.  The input is a comma-separated set of names
 * and optional values: "xml,pretty,indent=4"
 */
int
xo_set_options (xo_handle_t *xop, const char *input)
{
    char *cp, *ep, *vp, *np, *bp;
    int style = -1, new_style, len, rc = 0;
    xo_xof_flags_t new_flag;

    if (input == NULL)
	return 0;

    xop = xo_default(xop);

#ifdef LIBXO_COLOR_ON_BY_DEFAULT
    /* If the installer used --enable-color-on-by-default, then we allow it */
    xop->xo_flags |= XOF_COLOR_ALLOWED;
#endif /* LIBXO_COLOR_ON_BY_DEFAULT */

    /*
     * We support a simpler, old-school style of giving option
     * also, using a single character for each option.  It's
     * ideal for lazy people, such as myself.
     */
    if (*input == ':') {
	int sz;

	for (input++ ; *input; input++) {
	    switch (*input) {
	    case 'c':
		xop->xo_flags |= XOF_COLOR_ALLOWED;
		break;

	    case 'f':
		xop->xo_flags |= XOF_FLUSH;
		break;

	    case 'F':
		xop->xo_flags |= XOF_FLUSH_LINE;
		break;

	    case 'H':
		xop->xo_style = XO_STYLE_HTML;
		break;

	    case 'I':
		xop->xo_flags |= XOF_INFO;
		break;

	    case 'i':
		sz = strspn(input + 1, "0123456789");
		if (sz > 0) {
		    xop->xo_indent_by = atoi(input + 1);
		    input += sz - 1;	/* Skip value */
		}
		break;

	    case 'k':
		xop->xo_flags |= XOF_KEYS;
		break;

	    case 'J':
		xop->xo_style = XO_STYLE_JSON;
		break;

	    case 'P':
		xop->xo_flags |= XOF_PRETTY;
		break;

	    case 'T':
		xop->xo_style = XO_STYLE_TEXT;
		break;

	    case 'U':
		xop->xo_flags |= XOF_UNITS;
		break;

	    case 'u':
		xop->xo_flags |= XOF_UNDERSCORES;
		break;

	    case 'W':
		xop->xo_flags |= XOF_WARN;
		break;

	    case 'X':
		xop->xo_style = XO_STYLE_XML;
		break;

	    case 'x':
		xop->xo_flags |= XOF_XPATH;
		break;
	    }
	}
	return 0;
    }

    len = strlen(input) + 1;
    bp = alloca(len);
    memcpy(bp, input, len);

    for (cp = bp, ep = cp + len - 1; cp && cp < ep; cp = np) {
	np = strchr(cp, ',');
	if (np)
	    *np++ = '\0';

	vp = strchr(cp, '=');
	if (vp)
	    *vp++ = '\0';

	if (strcmp("colors", cp) == 0) {
	    /* XXX Look for colors=red-blue+green-yellow */
	    continue;
	}

	new_style = xo_name_to_style(cp);
	if (new_style >= 0) {
	    if (style >= 0)
		xo_warnx("ignoring multiple styles: '%s'", cp);
	    else
		style = new_style;
	} else {
	    new_flag = xo_name_to_flag(cp);
	    if (new_flag != 0)
		xop->xo_flags |= new_flag;
	    else {
		if (strcmp(cp, "no-color") == 0) {
		    xop->xo_flags &= ~XOF_COLOR_ALLOWED;
		} else if (strcmp(cp, "indent") == 0) {
		    xop->xo_indent_by = atoi(vp);
		} else {
		    xo_warnx("unknown option: '%s'", cp);
		    rc = -1;
		}
	    }
	}
    }

    if (style > 0)
	xop->xo_style= style;

    return rc;
}

/**
 * Set one or more flags for a given handle (or default if handle is NULL).
 * These flags will affect future output.
 *
 * @xop XO handle to alter (or NULL for default handle)
 * @flags Flags to be set (XOF_*)
 */
void
xo_set_flags (xo_handle_t *xop, xo_xof_flags_t flags)
{
    xop = xo_default(xop);

    xop->xo_flags |= flags;
}

xo_xof_flags_t
xo_get_flags (xo_handle_t *xop)
{
    xop = xo_default(xop);

    return xop->xo_flags;
}

/**
 * Record a leading prefix for the XPath we generate.  This allows the
 * generated data to be placed within an XML hierarchy but still have
 * accurate XPath expressions.
 *
 * @xop XO handle to alter (or NULL for default handle)
 * @path The XPath expression
 */
void
xo_set_leading_xpath (xo_handle_t *xop, const char *path)
{
    xop = xo_default(xop);

    if (xop->xo_leading_xpath) {
	xo_free(xop->xo_leading_xpath);
	xop->xo_leading_xpath = NULL;
    }

    if (path == NULL)
	return;

    int len = strlen(path);
    xop->xo_leading_xpath = xo_realloc(NULL, len + 1);
    if (xop->xo_leading_xpath) {
	memcpy(xop->xo_leading_xpath, path, len + 1);
    }
}

/**
 * Record the info data for a set of tags
 *
 * @xop XO handle to alter (or NULL for default handle)
 * @info Info data (xo_info_t) to be recorded (or NULL) (MUST BE SORTED)
 * @count Number of entries in info (or -1 to count them ourselves)
 */
void
xo_set_info (xo_handle_t *xop, xo_info_t *infop, int count)
{
    xop = xo_default(xop);

    if (count < 0 && infop) {
	xo_info_t *xip;

	for (xip = infop, count = 0; xip->xi_name; xip++, count++)
	    continue;
    }

    xop->xo_info = infop;
    xop->xo_info_count = count;
}

/**
 * Set the formatter callback for a handle.  The callback should
 * return a newly formatting contents of a formatting instruction,
 * meaning the bits inside the braces.
 */
void
xo_set_formatter (xo_handle_t *xop, xo_formatter_t func,
		  xo_checkpointer_t cfunc)
{
    xop = xo_default(xop);

    xop->xo_formatter = func;
    xop->xo_checkpointer = cfunc;
}

/**
 * Clear one or more flags for a given handle (or default if handle is NULL).
 * These flags will affect future output.
 *
 * @xop XO handle to alter (or NULL for default handle)
 * @flags Flags to be cleared (XOF_*)
 */
void
xo_clear_flags (xo_handle_t *xop, xo_xof_flags_t flags)
{
    xop = xo_default(xop);

    xop->xo_flags &= ~flags;
}

static const char *
xo_state_name (xo_state_t state)
{
    static const char *names[] = {
	"init",
	"open_container",
	"close_container",
	"open_list",
	"close_list",
	"open_instance",
	"close_instance",
	"open_leaf_list",
	"close_leaf_list",
	"discarding",
	"marker",
	"emit",
	"emit_leaf_list",
	"finish",
	NULL
    };

    if (state < (sizeof(names) / sizeof(names[0])))
	return names[state];

    return "unknown";
}

static void
xo_line_ensure_open (xo_handle_t *xop, xo_xff_flags_t flags UNUSED)
{
    static char div_open[] = "<div class=\"line\">";
    static char div_open_blank[] = "<div class=\"blank-line\">";

    if (xop->xo_flags & XOF_DIV_OPEN)
	return;

    if (xo_style(xop) != XO_STYLE_HTML)
	return;

    xop->xo_flags |= XOF_DIV_OPEN;
    if (flags & XFF_BLANK_LINE)
	xo_data_append(xop, div_open_blank, sizeof(div_open_blank) - 1);
    else
	xo_data_append(xop, div_open, sizeof(div_open) - 1);

    if (xop->xo_flags & XOF_PRETTY)
	xo_data_append(xop, "\n", 1);
}

static void
xo_line_close (xo_handle_t *xop)
{
    static char div_close[] = "</div>";

    switch (xo_style(xop)) {
    case XO_STYLE_HTML:
	if (!(xop->xo_flags & XOF_DIV_OPEN))
	    xo_line_ensure_open(xop, 0);

	xop->xo_flags &= ~XOF_DIV_OPEN;
	xo_data_append(xop, div_close, sizeof(div_close) - 1);

	if (xop->xo_flags & XOF_PRETTY)
	    xo_data_append(xop, "\n", 1);
	break;

    case XO_STYLE_TEXT:
	xo_data_append(xop, "\n", 1);
	break;
    }
}

static int
xo_info_compare (const void *key, const void *data)
{
    const char *name = key;
    const xo_info_t *xip = data;

    return strcmp(name, xip->xi_name);
}


static xo_info_t *
xo_info_find (xo_handle_t *xop, const char *name, int nlen)
{
    xo_info_t *xip;
    char *cp = alloca(nlen + 1); /* Need local copy for NUL termination */

    memcpy(cp, name, nlen);
    cp[nlen] = '\0';

    xip = bsearch(cp, xop->xo_info, xop->xo_info_count,
		  sizeof(xop->xo_info[0]), xo_info_compare);
    return xip;
}

#define CONVERT(_have, _need) (((_have) << 8) | (_need))

/*
 * Check to see that the conversion is safe and sane.
 */
static int
xo_check_conversion (xo_handle_t *xop, int have_enc, int need_enc)
{
    switch (CONVERT(have_enc, need_enc)) {
    case CONVERT(XF_ENC_UTF8, XF_ENC_UTF8):
    case CONVERT(XF_ENC_UTF8, XF_ENC_LOCALE):
    case CONVERT(XF_ENC_WIDE, XF_ENC_UTF8):
    case CONVERT(XF_ENC_WIDE, XF_ENC_LOCALE):
    case CONVERT(XF_ENC_LOCALE, XF_ENC_LOCALE):
    case CONVERT(XF_ENC_LOCALE, XF_ENC_UTF8):
	return 0;

    default:
	xo_failure(xop, "invalid conversion (%c:%c)", have_enc, need_enc);
	return 1;
    }
}

static int
xo_format_string_direct (xo_handle_t *xop, xo_buffer_t *xbp,
			 xo_xff_flags_t flags,
			 const wchar_t *wcp, const char *cp, int len, int max,
			 int need_enc, int have_enc)
{
    int cols = 0;
    wchar_t wc = 0;
    int ilen, olen, width;
    int attr = (flags & XFF_ATTR);
    const char *sp;

    if (len > 0 && !xo_buf_has_room(xbp, len))
	return 0;

    for (;;) {
	if (len == 0)
	    break;

	if (cp) {
	    if (*cp == '\0')
		break;
	    if ((flags & XFF_UNESCAPE) && (*cp == '\\' || *cp == '%')) {
		cp += 1;
		len -= 1;
	    }
	}

	if (wcp && *wcp == L'\0')
	    break;

	ilen = 0;

	switch (have_enc) {
	case XF_ENC_WIDE:		/* Wide character */
	    wc = *wcp++;
	    ilen = 1;
	    break;

	case XF_ENC_UTF8:		/* UTF-8 */
	    ilen = xo_utf8_to_wc_len(cp);
	    if (ilen < 0) {
		xo_failure(xop, "invalid UTF-8 character: %02hhx", *cp);
		return -1;
	    }

	    if (len > 0 && len < ilen) {
		len = 0;	/* Break out of the loop */
		continue;
	    }

	    wc = xo_utf8_char(cp, ilen);
	    if (wc == (wchar_t) -1) {
		xo_failure(xop, "invalid UTF-8 character: %02hhx/%d",
			   *cp, ilen);
		return -1;
	    }
	    cp += ilen;
	    break;

	case XF_ENC_LOCALE:		/* Native locale */
	    ilen = (len > 0) ? len : MB_LEN_MAX;
	    ilen = mbrtowc(&wc, cp, ilen, &xop->xo_mbstate);
	    if (ilen < 0) {		/* Invalid data; skip */
		xo_failure(xop, "invalid mbs char: %02hhx", *cp);
		continue;
	    }
	    if (ilen == 0) {		/* Hit a wide NUL character */
		len = 0;
		continue;
	    }

	    cp += ilen;
	    break;
	}

	/* Reduce len, but not below zero */
	if (len > 0) {
	    len -= ilen;
	    if (len < 0)
		len = 0;
	}

	/*
	 * Find the width-in-columns of this character, which must be done
	 * in wide characters, since we lack a mbswidth() function.  If
	 * it doesn't fit
	 */
	width = wcwidth(wc);
	if (width < 0)
	    width = iswcntrl(wc) ? 0 : 1;

	if (xo_style(xop) == XO_STYLE_TEXT || xo_style(xop) == XO_STYLE_HTML) {
	    if (max > 0 && cols + width > max)
		break;
	}

	switch (need_enc) {
	case XF_ENC_UTF8:

	    /* Output in UTF-8 needs to be escaped, based on the style */
	    switch (xo_style(xop)) {
	    case XO_STYLE_XML:
	    case XO_STYLE_HTML:
		if (wc == '<')
		    sp = xo_xml_lt;
		else if (wc == '>')
		    sp = xo_xml_gt;
		else if (wc == '&')
		    sp = xo_xml_amp;
		else if (attr && wc == '"')
		    sp = xo_xml_quot;
		else
		    break;

		int slen = strlen(sp);
		if (!xo_buf_has_room(xbp, slen - 1))
		    return -1;

		memcpy(xbp->xb_curp, sp, slen);
		xbp->xb_curp += slen;
		goto done_with_encoding; /* Need multi-level 'break' */

	    case XO_STYLE_JSON:
		if (wc != '\\' && wc != '"' && wc != '\n' && wc != '\r')
		    break;

		if (!xo_buf_has_room(xbp, 2))
		    return -1;

		*xbp->xb_curp++ = '\\';
		if (wc == '\n')
		    wc = 'n';
		else if (wc == '\r')
		    wc = 'r';
		else wc = wc & 0x7f;

		*xbp->xb_curp++ = wc;
		goto done_with_encoding;
	    }

	    olen = xo_utf8_emit_len(wc);
	    if (olen < 0) {
		xo_failure(xop, "ignoring bad length");
		continue;
	    }

	    if (!xo_buf_has_room(xbp, olen))
		return -1;

	    xo_utf8_emit_char(xbp->xb_curp, olen, wc);
	    xbp->xb_curp += olen;
	    break;

	case XF_ENC_LOCALE:
	    if (!xo_buf_has_room(xbp, MB_LEN_MAX + 1))
		return -1;

	    olen = wcrtomb(xbp->xb_curp, wc, &xop->xo_mbstate);
	    if (olen <= 0) {
		xo_failure(xop, "could not convert wide char: %lx",
			   (unsigned long) wc);
		olen = 1;
		width = 1;
		*xbp->xb_curp++ = '?';
	    } else
		xbp->xb_curp += olen;
	    break;
	}

    done_with_encoding:
	cols += width;
    }

    return cols;
}

static int
xo_format_string (xo_handle_t *xop, xo_buffer_t *xbp, xo_xff_flags_t flags,
		  xo_format_t *xfp)
{
    static char null[] = "(null)";

    char *cp = NULL;
    wchar_t *wcp = NULL;
    int len, cols = 0, rc = 0;
    int off = xbp->xb_curp - xbp->xb_bufp, off2;
    int need_enc = (xo_style(xop) == XO_STYLE_TEXT)
	? XF_ENC_LOCALE : XF_ENC_UTF8;

    if (xo_check_conversion(xop, xfp->xf_enc, need_enc))
	return 0;

    len = xfp->xf_width[XF_WIDTH_SIZE];

    if (xfp->xf_enc == XF_ENC_WIDE) {
	wcp = va_arg(xop->xo_vap, wchar_t *);
	if (xfp->xf_skip)
	    return 0;

	/*
	 * Dont' deref NULL; use the traditional "(null)" instead
	 * of the more accurate "who's been a naughty boy, then?".
	 */
	if (wcp == NULL) {
	    cp = null;
	    len = sizeof(null) - 1;
	}

    } else {
	cp = va_arg(xop->xo_vap, char *); /* UTF-8 or native */
	if (xfp->xf_skip)
	    return 0;

	/* Echo "Dont' deref NULL" logic */
	if (cp == NULL) {
	    cp = null;
	    len = sizeof(null) - 1;
	}

	/*
	 * Optimize the most common case, which is "%s".  We just
	 * need to copy the complete string to the output buffer.
	 */
	if (xfp->xf_enc == need_enc
		&& xfp->xf_width[XF_WIDTH_MIN] < 0
		&& xfp->xf_width[XF_WIDTH_SIZE] < 0
		&& xfp->xf_width[XF_WIDTH_MAX] < 0
		&& !(xop->xo_flags & (XOF_ANCHOR | XOF_COLUMNS))) {
	    len = strlen(cp);
	    xo_buf_escape(xop, xbp, cp, len, flags);

	    /*
	     * Our caller expects xb_curp left untouched, so we have
	     * to reset it and return the number of bytes written to
	     * the buffer.
	     */
	    off2 = xbp->xb_curp - xbp->xb_bufp;
	    rc = off2 - off;
	    xbp->xb_curp = xbp->xb_bufp + off;

	    return rc;
	}
    }

    cols = xo_format_string_direct(xop, xbp, flags, wcp, cp, len,
				   xfp->xf_width[XF_WIDTH_MAX],
				   need_enc, xfp->xf_enc);
    if (cols < 0)
	goto bail;

    /*
     * xo_buf_append* will move xb_curp, so we save/restore it.
     */
    off2 = xbp->xb_curp - xbp->xb_bufp;
    rc = off2 - off;
    xbp->xb_curp = xbp->xb_bufp + off;

    if (cols < xfp->xf_width[XF_WIDTH_MIN]) {
	/*
	 * Find the number of columns needed to display the string.
	 * If we have the original wide string, we just call wcswidth,
	 * but if we did the work ourselves, then we need to do it.
	 */
	int delta = xfp->xf_width[XF_WIDTH_MIN] - cols;
	if (!xo_buf_has_room(xbp, delta))
	    goto bail;

	/*
	 * If seen_minus, then pad on the right; otherwise move it so
	 * we can pad on the left.
	 */
	if (xfp->xf_seen_minus) {
	    cp = xbp->xb_curp + rc;
	} else {
	    cp = xbp->xb_curp;
	    memmove(xbp->xb_curp + delta, xbp->xb_curp, rc);
	}

	/* Set the padding */
	memset(cp, (xfp->xf_leading_zero > 0) ? '0' : ' ', delta);
	rc += delta;
	cols += delta;
    }

    if (xop->xo_flags & XOF_COLUMNS)
	xop->xo_columns += cols;
    if (xop->xo_flags & XOF_ANCHOR)
	xop->xo_anchor_columns += cols;

    return rc;

 bail:
    xbp->xb_curp = xbp->xb_bufp + off;
    return 0;
}

static void
xo_data_append_content (xo_handle_t *xop, const char *str, int len)
{
    int cols;
    int need_enc = (xo_style(xop) == XO_STYLE_TEXT)
	? XF_ENC_LOCALE : XF_ENC_UTF8;

    cols = xo_format_string_direct(xop, &xop->xo_data, XFF_UNESCAPE,
				   NULL, str, len, -1,
				   need_enc, XF_ENC_UTF8);

    if (xop->xo_flags & XOF_COLUMNS)
	xop->xo_columns += cols;
    if (xop->xo_flags & XOF_ANCHOR)
	xop->xo_anchor_columns += cols;
}

static void
xo_bump_width (xo_format_t *xfp, int digit)
{
    int *ip = &xfp->xf_width[xfp->xf_dots];

    *ip = ((*ip > 0) ? *ip : 0) * 10 + digit;
}

static int
xo_trim_ws (xo_buffer_t *xbp, int len)
{
    char *cp, *sp, *ep;
    int delta;

    /* First trim leading space */
    for (cp = sp = xbp->xb_curp, ep = cp + len; cp < ep; cp++) {
	if (*cp != ' ')
	    break;
    }

    delta = cp - sp;
    if (delta) {
	len -= delta;
	memmove(sp, cp, len);
    }

    /* Then trim off the end */
    for (cp = xbp->xb_curp, sp = ep = cp + len; cp < ep; ep--) {
	if (ep[-1] != ' ')
	    break;
    }

    delta = sp - ep;
    if (delta) {
	len -= delta;
	cp[len] = '\0';
    }

    return len;
}

static int
xo_format_data (xo_handle_t *xop, xo_buffer_t *xbp,
		const char *fmt, int flen, xo_xff_flags_t flags)
{
    xo_format_t xf;
    const char *cp, *ep, *sp, *xp = NULL;
    int rc, cols;
    int style = (flags & XFF_XML) ? XO_STYLE_XML : xo_style(xop);
    unsigned make_output = !(flags & XFF_NO_OUTPUT);
    int need_enc = (xo_style(xop) == XO_STYLE_TEXT)
	? XF_ENC_LOCALE : XF_ENC_UTF8;
    
    if (xbp == NULL)
	xbp = &xop->xo_data;

    for (cp = fmt, ep = fmt + flen; cp < ep; cp++) {
	if (*cp != '%') {
	add_one:
	    if (xp == NULL)
		xp = cp;

	    if (*cp == '\\' && cp[1] != '\0')
		cp += 1;
	    continue;

	} if (cp + 1 < ep && cp[1] == '%') {
	    cp += 1;
	    goto add_one;
	}

	if (xp) {
	    if (make_output) {
		cols = xo_format_string_direct(xop, xbp, flags | XFF_UNESCAPE,
					       NULL, xp, cp - xp, -1,
					       need_enc, XF_ENC_UTF8);
		if (xop->xo_flags & XOF_COLUMNS)
		    xop->xo_columns += cols;
		if (xop->xo_flags & XOF_ANCHOR)
		    xop->xo_anchor_columns += cols;
	    }

	    xp = NULL;
	}

	bzero(&xf, sizeof(xf));
	xf.xf_leading_zero = -1;
	xf.xf_width[0] = xf.xf_width[1] = xf.xf_width[2] = -1;

	/*
	 * "%@" starts an XO-specific set of flags:
	 *   @X@ - XML-only field; ignored if style isn't XML
	 */
	if (cp[1] == '@') {
	    for (cp += 2; cp < ep; cp++) {
		if (*cp == '@') {
		    break;
		}
		if (*cp == '*') {
		    /*
		     * '*' means there's a "%*.*s" value in vap that
		     * we want to ignore
		     */
		    if (!(xop->xo_flags & XOF_NO_VA_ARG))
			va_arg(xop->xo_vap, int);
		}
	    }
	}

	/* Hidden fields are only visible to JSON and XML */
	if (xop->xo_flags & XFF_ENCODE_ONLY) {
	    if (style != XO_STYLE_XML
		    && xo_style(xop) != XO_STYLE_JSON)
		xf.xf_skip = 1;
	} else if (xop->xo_flags & XFF_DISPLAY_ONLY) {
	    if (style != XO_STYLE_TEXT
		    && xo_style(xop) != XO_STYLE_HTML)
		xf.xf_skip = 1;
	}

	if (!make_output)
	    xf.xf_skip = 1;

	/*
	 * Looking at one piece of a format; find the end and
	 * call snprintf.  Then advance xo_vap on our own.
	 *
	 * Note that 'n', 'v', and '$' are not supported.
	 */
	sp = cp;		/* Save start pointer */
	for (cp += 1; cp < ep; cp++) {
	    if (*cp == 'l')
		xf.xf_lflag += 1;
	    else if (*cp == 'h')
		xf.xf_hflag += 1;
	    else if (*cp == 'j')
		xf.xf_jflag += 1;
	    else if (*cp == 't')
		xf.xf_tflag += 1;
	    else if (*cp == 'z')
		xf.xf_zflag += 1;
	    else if (*cp == 'q')
		xf.xf_qflag += 1;
	    else if (*cp == '.') {
		if (++xf.xf_dots >= XF_WIDTH_NUM) {
		    xo_failure(xop, "Too many dots in format: '%s'", fmt);
		    return -1;
		}
	    } else if (*cp == '-')
		xf.xf_seen_minus = 1;
	    else if (isdigit((int) *cp)) {
		if (xf.xf_leading_zero < 0)
		    xf.xf_leading_zero = (*cp == '0');
		xo_bump_width(&xf, *cp - '0');
	    } else if (*cp == '*') {
		xf.xf_stars += 1;
		xf.xf_star[xf.xf_dots] = 1;
	    } else if (strchr("diouxXDOUeEfFgGaAcCsSp", *cp) != NULL)
		break;
	    else if (*cp == 'n' || *cp == 'v') {
		xo_failure(xop, "unsupported format: '%s'", fmt);
		return -1;
	    }
	}

	if (cp == ep)
	    xo_failure(xop, "field format missing format character: %s",
			  fmt);

	xf.xf_fc = *cp;

	if (!(xop->xo_flags & XOF_NO_VA_ARG)) {
	    if (*cp == 's' || *cp == 'S') {
		/* Handle "%*.*.*s" */
		int s;
		for (s = 0; s < XF_WIDTH_NUM; s++) {
		    if (xf.xf_star[s]) {
			xf.xf_width[s] = va_arg(xop->xo_vap, int);
			
			/* Normalize a negative width value */
			if (xf.xf_width[s] < 0) {
			    if (s == 0) {
				xf.xf_width[0] = -xf.xf_width[0];
				xf.xf_seen_minus = 1;
			    } else
				xf.xf_width[s] = -1; /* Ignore negative values */
			}
		    }
		}
	    }
	}

	/* If no max is given, it defaults to size */
	if (xf.xf_width[XF_WIDTH_MAX] < 0 && xf.xf_width[XF_WIDTH_SIZE] >= 0)
	    xf.xf_width[XF_WIDTH_MAX] = xf.xf_width[XF_WIDTH_SIZE];

	if (xf.xf_fc == 'D' || xf.xf_fc == 'O' || xf.xf_fc == 'U')
	    xf.xf_lflag = 1;

	if (!xf.xf_skip) {
	    xo_buffer_t *fbp = &xop->xo_fmt;
	    int len = cp - sp + 1;
	    if (!xo_buf_has_room(fbp, len + 1))
		return -1;

	    char *newfmt = fbp->xb_curp;
	    memcpy(newfmt, sp, len);
	    newfmt[0] = '%';	/* If we skipped over a "%@...@s" format */
	    newfmt[len] = '\0';

	    /*
	     * Bad news: our strings are UTF-8, but the stock printf
	     * functions won't handle field widths for wide characters
	     * correctly.  So we have to handle this ourselves.
	     */
	    if (xop->xo_formatter == NULL
		    && (xf.xf_fc == 's' || xf.xf_fc == 'S')) {
		xf.xf_enc = (xf.xf_lflag || (xf.xf_fc == 'S'))
		    ? XF_ENC_WIDE : xf.xf_hflag ? XF_ENC_LOCALE : XF_ENC_UTF8;
		rc = xo_format_string(xop, xbp, flags, &xf);

		if ((flags & XFF_TRIM_WS)
			&& (xo_style(xop) == XO_STYLE_XML
				|| xo_style(xop) == XO_STYLE_JSON))
		    rc = xo_trim_ws(xbp, rc);

	    } else {
		int columns = rc = xo_vsnprintf(xop, xbp, newfmt, xop->xo_vap);

		/*
		 * For XML and HTML, we need "&<>" processing; for JSON,
		 * it's quotes.  Text gets nothing.
		 */
		switch (style) {
		case XO_STYLE_XML:
		    if (flags & XFF_TRIM_WS)
			columns = rc = xo_trim_ws(xbp, rc);
		    /* fall thru */
		case XO_STYLE_HTML:
		    rc = xo_escape_xml(xbp, rc, (flags & XFF_ATTR));
		    break;

		case XO_STYLE_JSON:
		    if (flags & XFF_TRIM_WS)
			columns = rc = xo_trim_ws(xbp, rc);
		    rc = xo_escape_json(xbp, rc);
		    break;
		}

		/*
		 * We can assume all the data we've added is ASCII, so
		 * the columns and bytes are the same.  xo_format_string
		 * handles all the fancy string conversions and updates
		 * xo_anchor_columns accordingly.
		 */
		if (xop->xo_flags & XOF_COLUMNS)
		    xop->xo_columns += columns;
		if (xop->xo_flags & XOF_ANCHOR)
		    xop->xo_anchor_columns += columns;
	    }

	    xbp->xb_curp += rc;
	}

	/*
	 * Now for the tricky part: we need to move the argument pointer
	 * along by the amount needed.
	 */
	if (!(xop->xo_flags & XOF_NO_VA_ARG)) {

	    if (xf.xf_fc == 's' ||xf.xf_fc == 'S') {
		/*
		 * The 'S' and 's' formats are normally handled in
		 * xo_format_string, but if we skipped it, then we
		 * need to pop it.
		 */
		if (xf.xf_skip)
		    va_arg(xop->xo_vap, char *);

	    } else {
		int s;
		for (s = 0; s < XF_WIDTH_NUM; s++) {
		    if (xf.xf_star[s])
			va_arg(xop->xo_vap, int);
		}

		if (strchr("diouxXDOU", xf.xf_fc) != NULL) {
		    if (xf.xf_hflag > 1) {
			va_arg(xop->xo_vap, int);

		    } else if (xf.xf_hflag > 0) {
			va_arg(xop->xo_vap, int);

		    } else if (xf.xf_lflag > 1) {
			va_arg(xop->xo_vap, unsigned long long);

		    } else if (xf.xf_lflag > 0) {
			va_arg(xop->xo_vap, unsigned long);

		    } else if (xf.xf_jflag > 0) {
			va_arg(xop->xo_vap, intmax_t);

		    } else if (xf.xf_tflag > 0) {
			va_arg(xop->xo_vap, ptrdiff_t);

		    } else if (xf.xf_zflag > 0) {
			va_arg(xop->xo_vap, size_t);

		    } else if (xf.xf_qflag > 0) {
			va_arg(xop->xo_vap, quad_t);

		    } else {
			va_arg(xop->xo_vap, int);
		    }
		} else if (strchr("eEfFgGaA", xf.xf_fc) != NULL)
		    if (xf.xf_lflag)
			va_arg(xop->xo_vap, long double);
		    else
			va_arg(xop->xo_vap, double);

		else if (xf.xf_fc == 'C' || (xf.xf_fc == 'c' && xf.xf_lflag))
		    va_arg(xop->xo_vap, wint_t);

		else if (xf.xf_fc == 'c')
		    va_arg(xop->xo_vap, int);

		else if (xf.xf_fc == 'p')
		    va_arg(xop->xo_vap, void *);
	    }
	}
    }

    if (xp) {
	if (make_output) {
	    cols = xo_format_string_direct(xop, xbp, flags | XFF_UNESCAPE,
					   NULL, xp, cp - xp, -1,
					   need_enc, XF_ENC_UTF8);
	    if (xop->xo_flags & XOF_COLUMNS)
		xop->xo_columns += cols;
	    if (xop->xo_flags & XOF_ANCHOR)
		xop->xo_anchor_columns += cols;
	}

	xp = NULL;
    }

    return 0;
}

static char *
xo_fix_encoding (xo_handle_t *xop UNUSED, char *encoding)
{
    char *cp = encoding;

    if (cp[0] != '%' || !isdigit((int) cp[1]))
	return encoding;

    for (cp += 2; *cp; cp++) {
	if (!isdigit((int) *cp))
	    break;
    }

    cp -= 1;
    *cp = '%';

    return cp;
}

static void
xo_color_append_html (xo_handle_t *xop)
{
    /*
     * If the color buffer has content, we add it now.  It's already
     * prebuilt and ready, since we want to add it to every <div>.
     */
    if (!xo_buf_is_empty(&xop->xo_color_buf)) {
	xo_buffer_t *xbp = &xop->xo_color_buf;

	xo_data_append(xop, xbp->xb_bufp, xbp->xb_curp - xbp->xb_bufp);
    }
}

static void
xo_buf_append_div (xo_handle_t *xop, const char *class, xo_xff_flags_t flags,
		   const char *name, int nlen,
		   const char *value, int vlen,
		   const char *encoding, int elen)
{
    static char div_start[] = "<div class=\"";
    static char div_tag[] = "\" data-tag=\"";
    static char div_xpath[] = "\" data-xpath=\"";
    static char div_key[] = "\" data-key=\"key";
    static char div_end[] = "\">";
    static char div_close[] = "</div>";

    /*
     * To build our XPath predicate, we need to save the va_list before
     * we format our data, and then restore it before we format the
     * xpath expression.
     * Display-only keys implies that we've got an encode-only key
     * elsewhere, so we don't use them from making predicates.
     */
    int need_predidate = 
	(name && (flags & XFF_KEY) && !(flags & XFF_DISPLAY_ONLY)
	 && (xop->xo_flags & XOF_XPATH));

    if (need_predidate) {
	va_list va_local;

	va_copy(va_local, xop->xo_vap);
	if (xop->xo_checkpointer)
	    xop->xo_checkpointer(xop, xop->xo_vap, 0);

	/*
	 * Build an XPath predicate expression to match this key.
	 * We use the format buffer.
	 */
	xo_buffer_t *pbp = &xop->xo_predicate;
	pbp->xb_curp = pbp->xb_bufp; /* Restart buffer */

	xo_buf_append(pbp, "[", 1);
	xo_buf_escape(xop, pbp, name, nlen, 0);
	if (xop->xo_flags & XOF_PRETTY)
	    xo_buf_append(pbp, " = '", 4);
	else
	    xo_buf_append(pbp, "='", 2);

	/* The encoding format defaults to the normal format */
	if (encoding == NULL) {
	    char *enc  = alloca(vlen + 1);
	    memcpy(enc, value, vlen);
	    enc[vlen] = '\0';
	    encoding = xo_fix_encoding(xop, enc);
	    elen = strlen(encoding);
	}

	xo_format_data(xop, pbp, encoding, elen, XFF_XML | XFF_ATTR);

	xo_buf_append(pbp, "']", 2);

	/* Now we record this predicate expression in the stack */
	xo_stack_t *xsp = &xop->xo_stack[xop->xo_depth];
	int olen = xsp->xs_keys ? strlen(xsp->xs_keys) : 0;
	int dlen = pbp->xb_curp - pbp->xb_bufp;

	char *cp = xo_realloc(xsp->xs_keys, olen + dlen + 1);
	if (cp) {
	    memcpy(cp + olen, pbp->xb_bufp, dlen);
	    cp[olen + dlen] = '\0';
	    xsp->xs_keys = cp;
	}

	/* Now we reset the xo_vap as if we were never here */
	va_end(xop->xo_vap);
	va_copy(xop->xo_vap, va_local);
	va_end(va_local);
	if (xop->xo_checkpointer)
	    xop->xo_checkpointer(xop, xop->xo_vap, 1);
    }

    if (flags & XFF_ENCODE_ONLY) {
	/*
	 * Even if this is encode-only, we need to go thru the
	 * work of formatting it to make sure the args are cleared
	 * from xo_vap.
	 */
	xo_format_data(xop, &xop->xo_data, encoding, elen,
		       flags | XFF_NO_OUTPUT);
	return;
    }

    xo_line_ensure_open(xop, 0);

    if (xop->xo_flags & XOF_PRETTY)
	xo_buf_indent(xop, xop->xo_indent_by);

    xo_data_append(xop, div_start, sizeof(div_start) - 1);
    xo_data_append(xop, class, strlen(class));

    /*
     * If the color buffer has content, we add it now.  It's already
     * prebuilt and ready, since we want to add it to every <div>.
     */
    if (!xo_buf_is_empty(&xop->xo_color_buf)) {
	xo_buffer_t *xbp = &xop->xo_color_buf;

	xo_data_append(xop, xbp->xb_bufp, xbp->xb_curp - xbp->xb_bufp);
    }

    if (name) {
	xo_data_append(xop, div_tag, sizeof(div_tag) - 1);
	xo_data_escape(xop, name, nlen);

	/*
	 * Save the offset at which we'd place units.  See xo_format_units.
	 */
	if (xop->xo_flags & XOF_UNITS) {
	    xop->xo_flags |= XOF_UNITS_PENDING;
	    /*
	     * Note: We need the '+1' here because we know we've not
	     * added the closing quote.  We add one, knowing the quote
	     * will be added shortly.
	     */
	    xop->xo_units_offset =
		xop->xo_data.xb_curp -xop->xo_data.xb_bufp + 1;
	}
    }

    if (name) {
	if (xop->xo_flags & XOF_XPATH) {
	    int i;
	    xo_stack_t *xsp;

	    xo_data_append(xop, div_xpath, sizeof(div_xpath) - 1);
	    if (xop->xo_leading_xpath)
		xo_data_append(xop, xop->xo_leading_xpath,
			       strlen(xop->xo_leading_xpath));

	    for (i = 0; i <= xop->xo_depth; i++) {
		xsp = &xop->xo_stack[i];
		if (xsp->xs_name == NULL)
		    continue;

		/*
		 * XSS_OPEN_LIST and XSS_OPEN_LEAF_LIST stack frames
		 * are directly under XSS_OPEN_INSTANCE frames so we
		 * don't need to put these in our XPath expressions.
		 */
		if (xsp->xs_state == XSS_OPEN_LIST
			|| xsp->xs_state == XSS_OPEN_LEAF_LIST)
		    continue;

		xo_data_append(xop, "/", 1);
		xo_data_escape(xop, xsp->xs_name, strlen(xsp->xs_name));
		if (xsp->xs_keys) {
		    /* Don't show keys for the key field */
		    if (i != xop->xo_depth || !(flags & XFF_KEY))
			xo_data_append(xop, xsp->xs_keys, strlen(xsp->xs_keys));
		}
	    }

	    xo_data_append(xop, "/", 1);
	    xo_data_escape(xop, name, nlen);
	}

	if ((xop->xo_flags & XOF_INFO) && xop->xo_info) {
	    static char in_type[] = "\" data-type=\"";
	    static char in_help[] = "\" data-help=\"";

	    xo_info_t *xip = xo_info_find(xop, name, nlen);
	    if (xip) {
		if (xip->xi_type) {
		    xo_data_append(xop, in_type, sizeof(in_type) - 1);
		    xo_data_escape(xop, xip->xi_type, strlen(xip->xi_type));
		}
		if (xip->xi_help) {
		    xo_data_append(xop, in_help, sizeof(in_help) - 1);
		    xo_data_escape(xop, xip->xi_help, strlen(xip->xi_help));
		}
	    }
	}

	if ((flags & XFF_KEY) && (xop->xo_flags & XOF_KEYS))
	    xo_data_append(xop, div_key, sizeof(div_key) - 1);
    }

    xo_data_append(xop, div_end, sizeof(div_end) - 1);

    xo_format_data(xop, NULL, value, vlen, 0);

    xo_data_append(xop, div_close, sizeof(div_close) - 1);

    if (xop->xo_flags & XOF_PRETTY)
	xo_data_append(xop, "\n", 1);
}

static void
xo_format_text (xo_handle_t *xop, const char *str, int len)
{
    switch (xo_style(xop)) {
    case XO_STYLE_TEXT:
	xo_buf_append_locale(xop, &xop->xo_data, str, len);
	break;

    case XO_STYLE_HTML:
	xo_buf_append_div(xop, "text", 0, NULL, 0, str, len, NULL, 0);
	break;
    }
}

static void
xo_format_title (xo_handle_t *xop, const char *str, int len,
		 const char *fmt, int flen)
{
    static char div_open[] = "<div class=\"title";
    static char div_middle[] = "\">";
    static char div_close[] = "</div>";

    if (flen == 0) {
	fmt = "%s";
	flen = 2;
    }

    switch (xo_style(xop)) {
    case XO_STYLE_XML:
    case XO_STYLE_JSON:
	/*
	 * Even though we don't care about text, we need to do
	 * enough parsing work to skip over the right bits of xo_vap.
	 */
	if (len == 0)
	    xo_format_data(xop, NULL, fmt, flen, XFF_NO_OUTPUT);
	return;
    }

    xo_buffer_t *xbp = &xop->xo_data;
    int start = xbp->xb_curp - xbp->xb_bufp;
    int left = xbp->xb_size - start;
    int rc;
    int need_enc = XF_ENC_LOCALE;

    if (xo_style(xop) == XO_STYLE_HTML) {
	need_enc = XF_ENC_UTF8;
	xo_line_ensure_open(xop, 0);
	if (xop->xo_flags & XOF_PRETTY)
	    xo_buf_indent(xop, xop->xo_indent_by);
	xo_buf_append(&xop->xo_data, div_open, sizeof(div_open) - 1);
	xo_color_append_html(xop);
	xo_buf_append(&xop->xo_data, div_middle, sizeof(div_middle) - 1);
    }

    start = xbp->xb_curp - xbp->xb_bufp; /* Reset start */
    if (len) {
	char *newfmt = alloca(flen + 1);
	memcpy(newfmt, fmt, flen);
	newfmt[flen] = '\0';

	/* If len is non-zero, the format string apply to the name */
	char *newstr = alloca(len + 1);
	memcpy(newstr, str, len);
	newstr[len] = '\0';

	if (newstr[len - 1] == 's') {
	    int cols;
	    char *bp;

	    rc = snprintf(NULL, 0, newfmt, newstr);
	    if (rc > 0) {
		/*
		 * We have to do this the hard way, since we might need
		 * the columns.
		 */
		bp = alloca(rc + 1);
		rc = snprintf(bp, rc + 1, newfmt, newstr);
		cols = xo_format_string_direct(xop, xbp, 0, NULL, bp, rc, -1,
					       need_enc, XF_ENC_UTF8);
		if (cols > 0) {
		    if (xop->xo_flags & XOF_COLUMNS)
			xop->xo_columns += cols;
		    if (xop->xo_flags & XOF_ANCHOR)
			xop->xo_anchor_columns += cols;
		}
	    }
	    goto move_along;

	} else {
	    rc = snprintf(xbp->xb_curp, left, newfmt, newstr);
	    if (rc > left) {
		if (!xo_buf_has_room(xbp, rc))
		    return;
		left = xbp->xb_size - (xbp->xb_curp - xbp->xb_bufp);
		rc = snprintf(xbp->xb_curp, left, newfmt, newstr);
	    }

	    if (rc > 0) {
		if (xop->xo_flags & XOF_COLUMNS)
		    xop->xo_columns += rc;
		if (xop->xo_flags & XOF_ANCHOR)
		    xop->xo_anchor_columns += rc;
	    }
	}

    } else {
	xo_format_data(xop, NULL, fmt, flen, 0);

	/* xo_format_data moved curp, so we need to reset it */
	rc = xbp->xb_curp - (xbp->xb_bufp + start);
	xbp->xb_curp = xbp->xb_bufp + start;
    }

    /* If we're styling HTML, then we need to escape it */
    if (xo_style(xop) == XO_STYLE_HTML) {
	rc = xo_escape_xml(xbp, rc, 0);
    }

    if (rc > 0)
	xbp->xb_curp += rc;

 move_along:
    if (xo_style(xop) == XO_STYLE_HTML) {
	xo_data_append(xop, div_close, sizeof(div_close) - 1);
	if (xop->xo_flags & XOF_PRETTY)
	    xo_data_append(xop, "\n", 1);
    }
}

static void
xo_format_prep (xo_handle_t *xop, xo_xff_flags_t flags)
{
    if (xop->xo_stack[xop->xo_depth].xs_flags & XSF_NOT_FIRST) {
	xo_data_append(xop, ",", 1);
	if (!(flags & XFF_LEAF_LIST) && (xop->xo_flags & XOF_PRETTY))
	    xo_data_append(xop, "\n", 1);
    } else
	xop->xo_stack[xop->xo_depth].xs_flags |= XSF_NOT_FIRST;
}

#if 0
/* Useful debugging function */
void
xo_arg (xo_handle_t *xop);
void
xo_arg (xo_handle_t *xop)
{
    xop = xo_default(xop);
    fprintf(stderr, "0x%x", va_arg(xop->xo_vap, unsigned));
}
#endif /* 0 */

static void
xo_format_value (xo_handle_t *xop, const char *name, int nlen,
		 const char *format, int flen,
		 const char *encoding, int elen, xo_xff_flags_t flags)
{
    int pretty = (xop->xo_flags & XOF_PRETTY);
    int quote;
    xo_buffer_t *xbp;

    /*
     * Before we emit a value, we need to know that the frame is ready.
     */
    xo_stack_t *xsp = &xop->xo_stack[xop->xo_depth];

    if (flags & XFF_LEAF_LIST) {
	/*
	 * Check if we've already started to emit normal leafs
	 * or if we're not in a leaf list.
	 */
	if ((xsp->xs_flags & (XSF_EMIT | XSF_EMIT_KEY))
	    || !(xsp->xs_flags & XSF_EMIT_LEAF_LIST)) {
	    char nbuf[nlen + 1];
	    memcpy(nbuf, name, nlen);
	    nbuf[nlen] = '\0';

	    int rc = xo_transition(xop, 0, nbuf, XSS_EMIT_LEAF_LIST);
	    if (rc < 0)
		flags |= XFF_DISPLAY_ONLY | XFF_ENCODE_ONLY;
	    else
		xop->xo_stack[xop->xo_depth].xs_flags |= XSF_EMIT_LEAF_LIST;
	}

	xsp = &xop->xo_stack[xop->xo_depth];
	if (xsp->xs_name) {
	    name = xsp->xs_name;
	    nlen = strlen(name);
	}

    } else if (flags & XFF_KEY) {
	/* Emitting a 'k' (key) field */
	if ((xsp->xs_flags & XSF_EMIT) && !(flags & XFF_DISPLAY_ONLY)) {
	    xo_failure(xop, "key field emitted after normal value field: '%.*s'",
		       nlen, name);

	} else if (!(xsp->xs_flags & XSF_EMIT_KEY)) {
	    char nbuf[nlen + 1];
	    memcpy(nbuf, name, nlen);
	    nbuf[nlen] = '\0';

	    int rc = xo_transition(xop, 0, nbuf, XSS_EMIT);
	    if (rc < 0)
		flags |= XFF_DISPLAY_ONLY | XFF_ENCODE_ONLY;
	    else
		xop->xo_stack[xop->xo_depth].xs_flags |= XSF_EMIT_KEY;

	    xsp = &xop->xo_stack[xop->xo_depth];
	    xsp->xs_flags |= XSF_EMIT_KEY;
	}

    } else {
	/* Emitting a normal value field */
	if ((xsp->xs_flags & XSF_EMIT_LEAF_LIST)
	    || !(xsp->xs_flags & XSF_EMIT)) {
	    char nbuf[nlen + 1];
	    memcpy(nbuf, name, nlen);
	    nbuf[nlen] = '\0';

	    int rc = xo_transition(xop, 0, nbuf, XSS_EMIT);
	    if (rc < 0)
		flags |= XFF_DISPLAY_ONLY | XFF_ENCODE_ONLY;
	    else
		xop->xo_stack[xop->xo_depth].xs_flags |= XSF_EMIT;

	    xsp = &xop->xo_stack[xop->xo_depth];
	    xsp->xs_flags |= XSF_EMIT;
	}
    }

    switch (xo_style(xop)) {
    case XO_STYLE_TEXT:
	if (flags & XFF_ENCODE_ONLY)
	    flags |= XFF_NO_OUTPUT;
	xo_format_data(xop, NULL, format, flen, flags);
	break;

    case XO_STYLE_HTML:
	if (flags & XFF_ENCODE_ONLY)
	    flags |= XFF_NO_OUTPUT;
	xo_buf_append_div(xop, "data", flags, name, nlen,
			  format, flen, encoding, elen);
	break;

    case XO_STYLE_XML:
	/*
	 * Even though we're not making output, we still need to
	 * let the formatting code handle the va_arg popping.
	 */
	if (flags & XFF_DISPLAY_ONLY) {
	    flags |= XFF_NO_OUTPUT;
	    xo_format_data(xop, NULL, format, flen, flags);
	    break;
	}

	if (encoding) {
   	    format = encoding;
	    flen = elen;
	} else {
	    char *enc  = alloca(flen + 1);
	    memcpy(enc, format, flen);
	    enc[flen] = '\0';
	    format = xo_fix_encoding(xop, enc);
	    flen = strlen(format);
	}

	if (nlen == 0) {
	    static char missing[] = "missing-field-name";
	    xo_failure(xop, "missing field name: %s", format);
	    name = missing;
	    nlen = sizeof(missing) - 1;
	}

	if (pretty)
	    xo_buf_indent(xop, -1);
	xo_data_append(xop, "<", 1);
	xo_data_escape(xop, name, nlen);

	if (xop->xo_attrs.xb_curp != xop->xo_attrs.xb_bufp) {
	    xo_data_append(xop, xop->xo_attrs.xb_bufp,
			   xop->xo_attrs.xb_curp - xop->xo_attrs.xb_bufp);
	    xop->xo_attrs.xb_curp = xop->xo_attrs.xb_bufp;
	}

	/*
	 * We indicate 'key' fields using the 'key' attribute.  While
	 * this is really committing the crime of mixing meta-data with
	 * data, it's often useful.  Especially when format meta-data is
	 * difficult to come by.
	 */
	if ((flags & XFF_KEY) && (xop->xo_flags & XOF_KEYS)) {
	    static char attr[] = " key=\"key\"";
	    xo_data_append(xop, attr, sizeof(attr) - 1);
	}

	/*
	 * Save the offset at which we'd place units.  See xo_format_units.
	 */
	if (xop->xo_flags & XOF_UNITS) {
	    xop->xo_flags |= XOF_UNITS_PENDING;
	    xop->xo_units_offset = xop->xo_data.xb_curp -xop->xo_data.xb_bufp;
	}

	xo_data_append(xop, ">", 1);
	xo_format_data(xop, NULL, format, flen, flags);
	xo_data_append(xop, "</", 2);
	xo_data_escape(xop, name, nlen);
	xo_data_append(xop, ">", 1);
	if (pretty)
	    xo_data_append(xop, "\n", 1);
	break;

    case XO_STYLE_JSON:
	if (flags & XFF_DISPLAY_ONLY) {
	    flags |= XFF_NO_OUTPUT;
	    xo_format_data(xop, NULL, format, flen, flags);
	    break;
	}

	if (encoding) {
	    format = encoding;
	    flen = elen;
	} else {
	    char *enc  = alloca(flen + 1);
	    memcpy(enc, format, flen);
	    enc[flen] = '\0';
	    format = xo_fix_encoding(xop, enc);
	    flen = strlen(format);
	}

	int first = !(xop->xo_stack[xop->xo_depth].xs_flags & XSF_NOT_FIRST);

	xo_format_prep(xop, flags);

	if (flags & XFF_QUOTE)
	    quote = 1;
	else if (flags & XFF_NOQUOTE)
	    quote = 0;
	else if (flen == 0) {
	    quote = 0;
	    format = "true";	/* JSON encodes empty tags as a boolean true */
	    flen = 4;
	} else if (strchr("diouxXDOUeEfFgGaAcCp", format[flen - 1]) == NULL)
	    quote = 1;
	else
	    quote = 0;

	if (nlen == 0) {
	    static char missing[] = "missing-field-name";
	    xo_failure(xop, "missing field name: %s", format);
	    name = missing;
	    nlen = sizeof(missing) - 1;
	}

	if (flags & XFF_LEAF_LIST) {
	    if (!first && pretty)
		xo_data_append(xop, "\n", 1);
	    if (pretty)
		xo_buf_indent(xop, -1);
	} else {
	    if (pretty)
		xo_buf_indent(xop, -1);
	    xo_data_append(xop, "\"", 1);

	    xbp = &xop->xo_data;
	    int off = xbp->xb_curp - xbp->xb_bufp;

	    xo_data_escape(xop, name, nlen);

	    if (xop->xo_flags & XOF_UNDERSCORES) {
		int now = xbp->xb_curp - xbp->xb_bufp;
		for ( ; off < now; off++)
		    if (xbp->xb_bufp[off] == '-')
			xbp->xb_bufp[off] = '_';
	    }
	    xo_data_append(xop, "\":", 2);
	    if (pretty)
	        xo_data_append(xop, " ", 1);
	}

	if (quote)
	    xo_data_append(xop, "\"", 1);

	xo_format_data(xop, NULL, format, flen, flags);

	if (quote)
	    xo_data_append(xop, "\"", 1);
	break;
    }
}

static void
xo_format_content (xo_handle_t *xop, const char *class_name,
		   const char *xml_tag, int display_only,
		   const char *str, int len, const char *fmt, int flen)
{
    switch (xo_style(xop)) {
    case XO_STYLE_TEXT:
	if (len) {
	    xo_data_append_content(xop, str, len);
	} else
	    xo_format_data(xop, NULL, fmt, flen, 0);
	break;

    case XO_STYLE_HTML:
	if (len == 0) {
	    str = fmt;
	    len = flen;
	}

	xo_buf_append_div(xop, class_name, 0, NULL, 0, str, len, NULL, 0);
	break;

    case XO_STYLE_XML:
	if (xml_tag) {
	    if (len == 0) {
		str = fmt;
		len = flen;
	    }

	    xo_open_container_h(xop, xml_tag);
	    xo_format_value(xop, "message", 7, str, len, NULL, 0, 0);
	    xo_close_container_h(xop, xml_tag);

	} else {
	    /*
	     * Even though we don't care about labels, we need to do
	     * enough parsing work to skip over the right bits of xo_vap.
	     */
	    if (len == 0)
		xo_format_data(xop, NULL, fmt, flen, XFF_NO_OUTPUT);
	}
	break;

    case XO_STYLE_JSON:
	/*
	 * Even though we don't care about labels, we need to do
	 * enough parsing work to skip over the right bits of xo_vap.
	 */
	if (display_only) {
	    if (len == 0)
		xo_format_data(xop, NULL, fmt, flen, XFF_NO_OUTPUT);
	    break;
	}
	/* XXX need schem for representing errors in JSON */
	break;
    }
}

static const char *xo_color_names[] = {
    "default",	/* XO_COL_DEFAULT */
    "black",	/* XO_COL_BLACK */
    "red",	/* XO_CLOR_RED */
    "green",	/* XO_COL_GREEN */
    "yellow",	/* XO_COL_YELLOW */
    "blue",	/* XO_COL_BLUE */
    "magenta",	/* XO_COL_MAGENTA */
    "cyan",	/* XO_COL_CYAN */
    "white",	/* XO_COL_WHITE */
    NULL
};

static int
xo_color_find (const char *str)
{
    int i;

    for (i = 0; xo_color_names[i]; i++) {
	if (strcmp(xo_color_names[i], str) == 0)
	    return i;
    }

    return -1;
}

static const char *xo_effect_names[] = {
    "reset",			/* XO_EFF_RESET */
    "normal",			/* XO_EFF_NORMAL */
    "bold",			/* XO_EFF_BOLD */
    "underline",		/* XO_EFF_UNDERLINE */
    "inverse",			/* XO_EFF_INVERSE */
    NULL
};

static const char *xo_effect_on_codes[] = {
    "0",			/* XO_EFF_RESET */
    "0",			/* XO_EFF_NORMAL */
    "1",			/* XO_EFF_BOLD */
    "4",			/* XO_EFF_UNDERLINE */
    "7",			/* XO_EFF_INVERSE */
    NULL
};

#if 0
/*
 * See comment below re: joy of terminal standards.  These can
 * be use by just adding:
 *	if (newp->xoc_effects & bit)
 *	    code = xo_effect_on_codes[i];
 * +	else
 * +	    code = xo_effect_off_codes[i];
 * in xo_color_handle_text.
 */
static const char *xo_effect_off_codes[] = {
    "0",			/* XO_EFF_RESET */
    "0",			/* XO_EFF_NORMAL */
    "21",			/* XO_EFF_BOLD */
    "24",			/* XO_EFF_UNDERLINE */
    "27",			/* XO_EFF_INVERSE */
    NULL
};
#endif /* 0 */

static int
xo_effect_find (const char *str)
{
    int i;

    for (i = 0; xo_effect_names[i]; i++) {
	if (strcmp(xo_effect_names[i], str) == 0)
	    return i;
    }

    return -1;
}

static void
xo_colors_parse (xo_handle_t *xop, xo_colors_t *xocp, char *str)
{
#ifdef LIBXO_TEXT_ONLY
    return;
#endif /* LIBXO_TEXT_ONLY */

    char *cp, *ep, *np, *xp;
    int len = strlen(str);
    int rc;

    /*
     * Possible tokens: colors, bg-colors, effects, no-effects, "reset".
     */
    for (cp = str, ep = cp + len - 1; cp && cp < ep; cp = np) {
	/* Trim leading whitespace */
	while (isspace((int) *cp))
	    cp += 1;

	np = strchr(cp, ',');
	if (np)
	    *np++ = '\0';

	/* Trim trailing whitespace */
	xp = cp + strlen(cp) - 1;
	while (isspace(*xp) && xp > cp)
	    *xp-- = '\0';

	if (cp[0] == 'f' && cp[1] == 'g' && cp[2] == '-') {
	    rc = xo_color_find(cp + 3);
	    if (rc < 0)
		goto unknown;

	    xocp->xoc_col_fg = rc;

	} else if (cp[0] == 'b' && cp[1] == 'g' && cp[2] == '-') {
	    rc = xo_color_find(cp + 3);
	    if (rc < 0)
		goto unknown;
	    xocp->xoc_col_bg = rc;

	} else if (cp[0] == 'n' && cp[1] == 'o' && cp[2] == '-') {
	    rc = xo_effect_find(cp + 3);
	    if (rc < 0)
		goto unknown;
	    xocp->xoc_effects &= ~(1 << rc);

	} else {
	    rc = xo_effect_find(cp);
	    if (rc < 0)
		goto unknown;
	    xocp->xoc_effects |= 1 << rc;

	    switch (1 << rc) {
	    case XO_EFF_RESET:
		xocp->xoc_col_fg = xocp->xoc_col_bg = 0;
		/* Note: not "|=" since we want to wipe out the old value */
		xocp->xoc_effects = XO_EFF_RESET;
		break;

	    case XO_EFF_NORMAL:
		xocp->xoc_effects &= ~(XO_EFF_BOLD | XO_EFF_UNDERLINE
				      | XO_EFF_INVERSE | XO_EFF_NORMAL);
		break;
	    }
	}
	continue;

    unknown:
	if (xop->xo_flags & XOF_WARN)
	    xo_failure(xop, "unknown color/effect string detected: '%s'", cp);
    }
}

static inline int
xo_colors_enabled (xo_handle_t *xop UNUSED)
{
#ifdef LIBXO_TEXT_ONLY
    return 0;
#else /* LIBXO_TEXT_ONLY */
    return ((xop->xo_flags & XOF_COLOR) ? 1 : 0);
#endif /* LIBXO_TEXT_ONLY */
}

static void
xo_colors_handle_text (xo_handle_t *xop UNUSED, xo_colors_t *newp)
{
    char buf[BUFSIZ];
    char *cp = buf, *ep = buf + sizeof(buf);
    unsigned i, bit;
    xo_colors_t *oldp = &xop->xo_colors;
    const char *code = NULL;

    /*
     * Start the buffer with an escape.  We don't want to add the '['
     * now, since we let xo_effect_text_add unconditionally add the ';'.
     * We'll replace the first ';' with a '[' when we're done.
     */
    *cp++ = 0x1b;		/* Escape */

    /*
     * Terminals were designed back in the age before "certainty" was
     * invented, when standards were more what you'd call "guidelines"
     * than actual rules.  Anyway we can't depend on them to operate
     * correctly.  So when display attributes are changed, we punt,
     * reseting them all and turning back on the ones we want to keep.
     * Longer, but should be completely reliable.  Savvy?
     */
    if (oldp->xoc_effects != (newp->xoc_effects & oldp->xoc_effects)) {
	newp->xoc_effects |= XO_EFF_RESET;
	oldp->xoc_effects = 0;
    }

    for (i = 0, bit = 1; xo_effect_names[i]; i++, bit <<= 1) {
	if ((newp->xoc_effects & bit) == (oldp->xoc_effects & bit))
	    continue;

	if (newp->xoc_effects & bit)
	    code = xo_effect_on_codes[i];

	cp += snprintf(cp, ep - cp, ";%s", code);
	if (cp >= ep)
	    return;		/* Should not occur */

	if (bit == XO_EFF_RESET) {
	    /* Mark up the old value so we can detect current values as new */
	    oldp->xoc_effects = 0;
	    oldp->xoc_col_fg = oldp->xoc_col_bg = XO_COL_DEFAULT;
	}
    }

    if (newp->xoc_col_fg != oldp->xoc_col_fg) {
	cp += snprintf(cp, ep - cp, ";3%u",
		       (newp->xoc_col_fg != XO_COL_DEFAULT)
		       ? newp->xoc_col_fg - 1 : 9);
    }

    if (newp->xoc_col_bg != oldp->xoc_col_bg) {
	cp += snprintf(cp, ep - cp, ";4%u",
		       (newp->xoc_col_bg != XO_COL_DEFAULT)
		       ? newp->xoc_col_bg - 1 : 9);
    }

    if (cp - buf != 1 && cp < ep - 3) {
	buf[1] = '[';		/* Overwrite leading ';' */
	*cp++ = 'm';
	*cp = '\0';
	xo_buf_append(&xop->xo_data, buf, cp - buf);
    }
}

static void
xo_colors_handle_html (xo_handle_t *xop, xo_colors_t *newp)
{
    xo_colors_t *oldp = &xop->xo_colors;

    /*
     * HTML colors are mostly trivial: fill in xo_color_buf with
     * a set of class tags representing the colors and effects.
     */

    /* If nothing changed, then do nothing */
    if (oldp->xoc_effects == newp->xoc_effects
	&& oldp->xoc_col_fg == newp->xoc_col_fg
	&& oldp->xoc_col_bg == newp->xoc_col_bg)
	return;

    unsigned i, bit;
    xo_buffer_t *xbp = &xop->xo_color_buf;

    xo_buf_reset(xbp);		/* We rebuild content after each change */

    for (i = 0, bit = 1; xo_effect_names[i]; i++, bit <<= 1) {
	if (!(newp->xoc_effects & bit))
	    continue;

	xo_buf_append_str(xbp, " effect-");
	xo_buf_append_str(xbp, xo_effect_names[i]);
    }

    const char *fg = NULL;
    const char *bg = NULL;

    if (newp->xoc_col_fg != XO_COL_DEFAULT)
	fg = xo_color_names[newp->xoc_col_fg];
    if (newp->xoc_col_bg != XO_COL_DEFAULT)
	bg = xo_color_names[newp->xoc_col_bg];

    if (newp->xoc_effects & XO_EFF_INVERSE) {
	const char *tmp = fg;
	fg = bg;
	bg = tmp;
	if (fg == NULL)
	    fg = "inverse";
	if (bg == NULL)
	    bg = "inverse";

    }

    if (fg) {
	xo_buf_append_str(xbp, " color-fg-");
	xo_buf_append_str(xbp, fg);
    }

    if (bg) {
	xo_buf_append_str(xbp, " color-bg-");
	xo_buf_append_str(xbp, bg);
    }
}

static void
xo_format_colors (xo_handle_t *xop, const char *str, int len,
		  const char *fmt, int flen)
{
    xo_buffer_t xb;

    /* If the string is static and we've in an encoding style, bail */
    if (len != 0
	&& (xo_style(xop) == XO_STYLE_XML || xo_style(xop) == XO_STYLE_JSON))
	return;

    xo_buf_init(&xb);

    if (len)
	xo_buf_append(&xb, str, len);
    else if (flen)
	xo_format_data(xop, &xb, fmt, flen, 0);
    else
	xo_buf_append(&xb, "reset", 6); /* Default if empty */

    if (xo_colors_enabled(xop)) {
	switch (xo_style(xop)) {
	case XO_STYLE_TEXT:
	case XO_STYLE_HTML:
	    xo_buf_append(&xb, "", 1);

	    xo_colors_t xoc = xop->xo_colors;
	    xo_colors_parse(xop, &xoc, xb.xb_bufp);

	    if (xo_style(xop) == XO_STYLE_TEXT) {
		/*
		 * Text mode means emitting the colors as ANSI character
		 * codes.  This will allow people who like colors to have
		 * colors.  The issue is, of course conflicting with the
		 * user's perfectly reasonable color scheme.  Which leads
		 * to the hell of LSCOLORS, where even app need to have
		 * customization hooks for adjusting colors.  Instead we
		 * provide a simpler-but-still-annoying answer where one
		 * can map colors to other colors.
		 */
		xo_colors_handle_text(xop, &xoc);
		xoc.xoc_effects &= ~XO_EFF_RESET; /* After handling it */

	    } else {
		/*
		 * HTML output is wrapped in divs, so the color information
		 * must appear in every div until cleared.  Most pathetic.
		 * Most unavoidable.
		 */
		xoc.xoc_effects &= ~XO_EFF_RESET; /* Before handling effects */
		xo_colors_handle_html(xop, &xoc);
	    }

	    xop->xo_colors = xoc;
	    break;

	case XO_STYLE_XML:
	case XO_STYLE_JSON:
	    /*
	     * Nothing to do; we did all that work just to clear the stack of
	     * formatting arguments.
	     */
	    break;
	}
    }

    xo_buf_cleanup(&xb);
}

static void
xo_format_units (xo_handle_t *xop, const char *str, int len,
		 const char *fmt, int flen)
{
    static char units_start_xml[] = " units=\"";
    static char units_start_html[] = " data-units=\"";

    if (!(xop->xo_flags & XOF_UNITS_PENDING)) {
	xo_format_content(xop, "units", NULL, 1, str, len, fmt, flen);
	return;
    }

    xo_buffer_t *xbp = &xop->xo_data;
    int start = xop->xo_units_offset;
    int stop = xbp->xb_curp - xbp->xb_bufp;

    if (xo_style(xop) == XO_STYLE_XML)
	xo_buf_append(xbp, units_start_xml, sizeof(units_start_xml) - 1);
    else if (xo_style(xop) == XO_STYLE_HTML)
	xo_buf_append(xbp, units_start_html, sizeof(units_start_html) - 1);
    else
	return;

    if (len)
	xo_data_append(xop, str, len);
    else
	xo_format_data(xop, NULL, fmt, flen, 0);

    xo_buf_append(xbp, "\"", 1);

    int now = xbp->xb_curp - xbp->xb_bufp;
    int delta = now - stop;
    if (delta < 0) {		/* Strange; no output to move */
	xbp->xb_curp = xbp->xb_bufp + stop; /* Reset buffer to prior state */
	return;
    }

    /*
     * Now we're in it alright.  We've need to insert the unit value
     * we just created into the right spot.  We make a local copy,
     * move it and then insert our copy.  We know there's room in the
     * buffer, since we're just moving this around.
     */
    char *buf = alloca(delta);

    memcpy(buf, xbp->xb_bufp + stop, delta);
    memmove(xbp->xb_bufp + start + delta, xbp->xb_bufp + start, stop - start);
    memmove(xbp->xb_bufp + start, buf, delta);
}

static int
xo_find_width (xo_handle_t *xop, const char *str, int len,
		 const char *fmt, int flen)
{
    long width = 0;
    char *bp;
    char *cp;

    if (len) {
	bp = alloca(len + 1);	/* Make local NUL-terminated copy of str */
	memcpy(bp, str, len);
	bp[len] = '\0';

	width = strtol(bp, &cp, 0);
	if (width == LONG_MIN || width == LONG_MAX
	    || bp == cp || *cp != '\0' ) {
	    width = 0;
	    xo_failure(xop, "invalid width for anchor: '%s'", bp);
	}
    } else if (flen) {
	if (flen != 2 || strncmp("%d", fmt, flen) != 0)
	    xo_failure(xop, "invalid width format: '%*.*s'", flen, flen, fmt);
	if (!(xop->xo_flags & XOF_NO_VA_ARG))
	    width = va_arg(xop->xo_vap, int);
    }

    return width;
}

static void
xo_anchor_clear (xo_handle_t *xop)
{
    xop->xo_flags &= ~XOF_ANCHOR;
    xop->xo_anchor_offset = 0;
    xop->xo_anchor_columns = 0;
    xop->xo_anchor_min_width = 0;
}

/*
 * An anchor is a marker used to delay field width implications.
 * Imagine the format string "{[:10}{min:%d}/{cur:%d}/{max:%d}{:]}".
 * We are looking for output like "     1/4/5"
 *
 * To make this work, we record the anchor and then return to
 * format it when the end anchor tag is seen.
 */
static void
xo_anchor_start (xo_handle_t *xop, const char *str, int len,
		 const char *fmt, int flen)
{
    if (xo_style(xop) != XO_STYLE_TEXT && xo_style(xop) != XO_STYLE_HTML)
	return;

    if (xop->xo_flags & XOF_ANCHOR)
	xo_failure(xop, "the anchor already recording is discarded");

    xop->xo_flags |= XOF_ANCHOR;
    xo_buffer_t *xbp = &xop->xo_data;
    xop->xo_anchor_offset = xbp->xb_curp - xbp->xb_bufp;
    xop->xo_anchor_columns = 0;

    /*
     * Now we find the width, if possible.  If it's not there,
     * we'll get it on the end anchor.
     */
    xop->xo_anchor_min_width = xo_find_width(xop, str, len, fmt, flen);
}

static void
xo_anchor_stop (xo_handle_t *xop, const char *str, int len,
		 const char *fmt, int flen)
{
    if (xo_style(xop) != XO_STYLE_TEXT && xo_style(xop) != XO_STYLE_HTML)
	return;

    if (!(xop->xo_flags & XOF_ANCHOR)) {
	xo_failure(xop, "no start anchor");
	return;
    }

    xop->xo_flags &= ~XOF_UNITS_PENDING;

    int width = xo_find_width(xop, str, len, fmt, flen);
    if (width == 0)
	width = xop->xo_anchor_min_width;

    if (width == 0)		/* No width given; nothing to do */
	goto done;

    xo_buffer_t *xbp = &xop->xo_data;
    int start = xop->xo_anchor_offset;
    int stop = xbp->xb_curp - xbp->xb_bufp;
    int abswidth = (width > 0) ? width : -width;
    int blen = abswidth - xop->xo_anchor_columns;

    if (blen <= 0)		/* Already over width */
	goto done;

    if (abswidth > XO_MAX_ANCHOR_WIDTH) {
	xo_failure(xop, "width over %u are not supported",
		   XO_MAX_ANCHOR_WIDTH);
	goto done;
    }

    /* Make a suitable padding field and emit it */
    char *buf = alloca(blen);
    memset(buf, ' ', blen);
    xo_format_content(xop, "padding", NULL, 1, buf, blen, NULL, 0);

    if (width < 0)		/* Already left justified */
	goto done;

    int now = xbp->xb_curp - xbp->xb_bufp;
    int delta = now - stop;
    if (delta < 0)		/* Strange; no output to move */
	goto done;

    /*
     * Now we're in it alright.  We've need to insert the padding data
     * we just created (which might be an HTML <div> or text) before
     * the formatted data.  We make a local copy, move it and then
     * insert our copy.  We know there's room in the buffer, since
     * we're just moving this around.
     */
    if (delta > blen)
	buf = alloca(delta);	/* Expand buffer if needed */

    memcpy(buf, xbp->xb_bufp + stop, delta);
    memmove(xbp->xb_bufp + start + delta, xbp->xb_bufp + start, stop - start);
    memmove(xbp->xb_bufp + start, buf, delta);

 done:
    xo_anchor_clear(xop);
}

static int
xo_do_emit (xo_handle_t *xop, const char *fmt)
{
    int rc = 0;
    const char *cp, *sp, *ep, *basep;
    char *newp = NULL;
    int flush = (xop->xo_flags & XOF_FLUSH) ? 1 : 0;
    int flush_line = (xop->xo_flags & XOF_FLUSH_LINE) ? 1 : 0;

    xop->xo_columns = 0;	/* Always reset it */

    for (cp = fmt; *cp; ) {
	if (*cp == '\n') {
	    xo_line_close(xop);
	    if (flush_line && xo_flush_h(xop) < 0)
		return -1;
	    cp += 1;
	    continue;

	} else if (*cp == '{') {
	    if (cp[1] == '{') {	/* Start of {{escaped braces}} */

		cp += 2;	/* Skip over _both_ characters */
		for (sp = cp; *sp; sp++) {
		    if (*sp == '}' && sp[1] == '}')
			break;
		}
		if (*sp == '\0') {
		    xo_failure(xop, "missing closing '}}': %s", fmt);
		    return -1;
		}

		xo_format_text(xop, cp, sp - cp);

		/* Move along the string, but don't run off the end */
		if (*sp == '}' && sp[1] == '}')
		    sp += 2;
		cp = *sp ? sp + 1 : sp;
		continue;
	    }
	    /* Else fall thru to the code below */

	} else {
	    /* Normal text */
	    for (sp = cp; *sp; sp++) {
		if (*sp == '{' || *sp == '\n')
		    break;
	    }
	    xo_format_text(xop, cp, sp - cp);

	    cp = sp;
	    continue;
	}

	basep = cp + 1;

	/*
	 * We are looking at the start of a field definition.  The format is:
	 *  '{' modifiers ':' content [ '/' print-fmt [ '/' encode-fmt ]] '}'
	 * Modifiers are optional and include the following field types:
	 *   'D': decoration; something non-text and non-data (colons, commmas)
	 *   'E': error message
	 *   'L': label; text preceding data
	 *   'N': note; text following data
	 *   'P': padding; whitespace
	 *   'T': Title, where 'content' is a column title
	 *   'U': Units, where 'content' is the unit label
	 *   'V': value, where 'content' is the name of the field (the default)
	 *   'W': warning message
	 *   '[': start a section of anchored text
	 *   ']': end a section of anchored text
         * The following flags are also supported:
	 *   'c': flag: emit a colon after the label
	 *   'd': field is only emitted for display formats (text and html)
	 *   'e': field is only emitted for encoding formats (xml and json)
	 *   'k': this field is a key, suitable for XPath predicates
	 *   'l': a leaf-list, a simple list of values
	 *   'n': no quotes around this field
	 *   'q': add quotes around this field
	 *   't': trim whitespace around the value
	 *   'w': emit a blank after the label
	 * The print-fmt and encode-fmt strings is the printf-style formating
	 * for this data.  JSON and XML will use the encoding-fmt, if present.
	 * If the encode-fmt is not provided, it defaults to the print-fmt.
	 * If the print-fmt is not provided, it defaults to 's'.
	 */
	unsigned ftype = 0, flags = 0;
	const char *content = NULL, *format = NULL, *encoding = NULL;
	int clen = 0, flen = 0, elen = 0;

	for (sp = basep; sp; sp++) {
	    if (*sp == ':' || *sp == '/' || *sp == '}')
		break;

	    if (*sp == '\\') {
		if (sp[1] == '\0') {
		    xo_failure(xop, "backslash at the end of string");
		    return -1;
		}
		sp += 1;
		continue;
	    }

	    switch (*sp) {
	    case 'C':
	    case 'D':
	    case 'E':
	    case 'L':
	    case 'N':
	    case 'P':
	    case 'T':
	    case 'U':
	    case 'V':
	    case 'W':
	    case '[':
	    case ']':
		if (ftype != 0) {
		    xo_failure(xop, "field descriptor uses multiple types: %s",
				  fmt);
		    return -1;
		}
		ftype = *sp;
		break;

	    case 'c':
		flags |= XFF_COLON;
		break;

	    case 'd':
		flags |= XFF_DISPLAY_ONLY;
		break;

	    case 'e':
		flags |= XFF_ENCODE_ONLY;
		break;

	    case 'k':
		flags |= XFF_KEY;
		break;

	    case 'l':
		flags |= XFF_LEAF_LIST;
		break;

	    case 'n':
		flags |= XFF_NOQUOTE;
		break;

	    case 'q':
		flags |= XFF_QUOTE;
		break;

	    case 't':
		flags |= XFF_TRIM_WS;
		break;

	    case 'w':
		flags |= XFF_WS;
		break;

	    default:
		xo_failure(xop, "field descriptor uses unknown modifier: %s",
			      fmt);
		/*
		 * No good answer here; a bad format will likely
		 * mean a core file.  We just return and hope
		 * the caller notices there's no output, and while
		 * that seems, well, bad.  There's nothing better.
		 */
		return -1;
	    }
	}

	if (*sp == ':') {
	    for (ep = ++sp; *sp; sp++) {
		if (*sp == '}' || *sp == '/')
		    break;
		if (*sp == '\\') {
		    if (sp[1] == '\0') {
			xo_failure(xop, "backslash at the end of string");
			return -1;
		    }
		    sp += 1;
		    continue;
		}
	    }
	    if (ep != sp) {
		clen = sp - ep;
		content = ep;
	    }
	} else {
	    xo_failure(xop, "missing content (':'): %s", fmt);
	    return -1;
	}

	if (*sp == '/') {
	    for (ep = ++sp; *sp; sp++) {
		if (*sp == '}' || *sp == '/')
		    break;
		if (*sp == '\\') {
		    if (sp[1] == '\0') {
			xo_failure(xop, "backslash at the end of string");
			return -1;
		    }
		    sp += 1;
		    continue;
		}
	    }
	    flen = sp - ep;
	    format = ep;
	}

	if (*sp == '/') {
	    for (ep = ++sp; *sp; sp++) {
		if (*sp == '}')
		    break;
	    }
	    elen = sp - ep;
	    encoding = ep;
	}

	if (*sp == '}') {
	    sp += 1;
	} else {
	    xo_failure(xop, "missing closing '}': %s", fmt);
	    return -1;
	}

	if (ftype == 0 || ftype == 'V') {
	    if (format == NULL) {
		/* Default format for value fields is '%s' */
		format = "%s";
		flen = 2;
	    }

	    xo_format_value(xop, content, clen, format, flen,
			    encoding, elen, flags);

	} else if (ftype == '[')
		xo_anchor_start(xop, content, clen, format, flen);
	else if (ftype == ']')
		xo_anchor_stop(xop, content, clen, format, flen);
	else if (ftype == 'C')
		xo_format_colors(xop, content, clen, format, flen);

	else  if (clen || format) { /* Need either content or format */
	    if (format == NULL) {
		/* Default format for value fields is '%s' */
		format = "%s";
		flen = 2;
	    }

	    if (ftype == 'D')
		xo_format_content(xop, "decoration", NULL, 1,
				  content, clen, format, flen);
	    else if (ftype == 'E')
		xo_format_content(xop, "error", "error", 0,
				  content, clen, format, flen);
	    else if (ftype == 'L')
		xo_format_content(xop, "label", NULL, 1,
				  content, clen, format, flen);
	    else if (ftype == 'N')
		xo_format_content(xop, "note", NULL, 1,
				  content, clen, format, flen);
	    else if (ftype == 'P')
		xo_format_content(xop, "padding", NULL, 1,
				  content, clen, format, flen);
	    else if (ftype == 'T')
		xo_format_title(xop, content, clen, format, flen);
	    else if (ftype == 'U') {
		if (flags & XFF_WS)
		    xo_format_content(xop, "padding", NULL, 1, " ", 1, NULL, 0);
		xo_format_units(xop, content, clen, format, flen);
	    } else if (ftype == 'W')
		xo_format_content(xop, "warning", "warning", 0,
				  content, clen, format, flen);
	}

	if (flags & XFF_COLON)
	    xo_format_content(xop, "decoration", NULL, 1, ":", 1, NULL, 0);
	if (ftype != 'U' && (flags & XFF_WS))
	    xo_format_content(xop, "padding", NULL, 1, " ", 1, NULL, 0);

	cp += sp - basep + 1;
	if (newp) {
	    xo_free(newp);
	    newp = NULL;
	}
    }

    /* If we don't have an anchor, write the text out */
    if (flush && !(xop->xo_flags & XOF_ANCHOR)) {
	if (xo_write(xop) < 0) 
	    rc = -1;		/* Report failure */
	else if (xop->xo_flush && xop->xo_flush(xop->xo_opaque) < 0)
	    rc = -1;
    }

    return (rc < 0) ? rc : (int) xop->xo_columns;
}

int
xo_emit_hv (xo_handle_t *xop, const char *fmt, va_list vap)
{
    int rc;

    xop = xo_default(xop);
    va_copy(xop->xo_vap, vap);
    rc = xo_do_emit(xop, fmt);
    va_end(xop->xo_vap);
    bzero(&xop->xo_vap, sizeof(xop->xo_vap));

    return rc;
}

int
xo_emit_h (xo_handle_t *xop, const char *fmt, ...)
{
    int rc;

    xop = xo_default(xop);
    va_start(xop->xo_vap, fmt);
    rc = xo_do_emit(xop, fmt);
    va_end(xop->xo_vap);
    bzero(&xop->xo_vap, sizeof(xop->xo_vap));

    return rc;
}

int
xo_emit (const char *fmt, ...)
{
    xo_handle_t *xop = xo_default(NULL);
    int rc;

    va_start(xop->xo_vap, fmt);
    rc = xo_do_emit(xop, fmt);
    va_end(xop->xo_vap);
    bzero(&xop->xo_vap, sizeof(xop->xo_vap));

    return rc;
}

int
xo_attr_hv (xo_handle_t *xop, const char *name, const char *fmt, va_list vap)
{
    const int extra = 5; 	/* space, equals, quote, quote, and nul */
    xop = xo_default(xop);

    if (xo_style(xop) != XO_STYLE_XML)
	return 0;

    int nlen = strlen(name);
    xo_buffer_t *xbp = &xop->xo_attrs;

    if (!xo_buf_has_room(xbp, nlen + extra))
	return -1;

    *xbp->xb_curp++ = ' ';
    memcpy(xbp->xb_curp, name, nlen);
    xbp->xb_curp += nlen;
    *xbp->xb_curp++ = '=';
    *xbp->xb_curp++ = '"';

    int rc = xo_vsnprintf(xop, xbp, fmt, vap);

    if (rc > 0) {
	rc = xo_escape_xml(xbp, rc, 1);
	xbp->xb_curp += rc;
    }

    if (!xo_buf_has_room(xbp, 2))
	return -1;

    *xbp->xb_curp++ = '"';
    *xbp->xb_curp = '\0';

    return rc + nlen + extra;
}

int
xo_attr_h (xo_handle_t *xop, const char *name, const char *fmt, ...)
{
    int rc;
    va_list vap;

    va_start(vap, fmt);
    rc = xo_attr_hv(xop, name, fmt, vap);
    va_end(vap);

    return rc;
}

int
xo_attr (const char *name, const char *fmt, ...)
{
    int rc;
    va_list vap;

    va_start(vap, fmt);
    rc = xo_attr_hv(NULL, name, fmt, vap);
    va_end(vap);

    return rc;
}

static void
xo_stack_set_flags (xo_handle_t *xop)
{
    if (xop->xo_flags & XOF_NOT_FIRST) {
	xo_stack_t *xsp = &xop->xo_stack[xop->xo_depth];

	xsp->xs_flags |= XSF_NOT_FIRST;
	xop->xo_flags &= ~XOF_NOT_FIRST;
    }
}

static void
xo_depth_change (xo_handle_t *xop, const char *name,
		 int delta, int indent, xo_state_t state, xo_xsf_flags_t flags)
{
    if (xo_style(xop) == XO_STYLE_HTML || xo_style(xop) == XO_STYLE_TEXT)
	indent = 0;

    if (xop->xo_flags & XOF_DTRT)
	flags |= XSF_DTRT;

    if (delta >= 0) {			/* Push operation */
	if (xo_depth_check(xop, xop->xo_depth + delta))
	    return;

	xo_stack_t *xsp = &xop->xo_stack[xop->xo_depth + delta];
	xsp->xs_flags = flags;
	xsp->xs_state = state;
	xo_stack_set_flags(xop);

	if (name == NULL)
	    name = XO_FAILURE_NAME;

	int len = strlen(name) + 1;
	char *cp = xo_realloc(NULL, len);
	if (cp) {
	    memcpy(cp, name, len);
	    xsp->xs_name = cp;
	}

    } else {			/* Pop operation */
	if (xop->xo_depth == 0) {
	    if (!(xop->xo_flags & XOF_IGNORE_CLOSE))
		xo_failure(xop, "close with empty stack: '%s'", name);
	    return;
	}

	xo_stack_t *xsp = &xop->xo_stack[xop->xo_depth];
	if (xop->xo_flags & XOF_WARN) {
	    const char *top = xsp->xs_name;
	    if (top && strcmp(name, top) != 0) {
		xo_failure(xop, "incorrect close: '%s' .vs. '%s'",
			      name, top);
		return;
	    } 
	    if ((xsp->xs_flags & XSF_LIST) != (flags & XSF_LIST)) {
		xo_failure(xop, "list close on list confict: '%s'",
			      name);
		return;
	    }
	    if ((xsp->xs_flags & XSF_INSTANCE) != (flags & XSF_INSTANCE)) {
		xo_failure(xop, "list close on instance confict: '%s'",
			      name);
		return;
	    }
	}

	if (xsp->xs_name) {
	    xo_free(xsp->xs_name);
	    xsp->xs_name = NULL;
	}
	if (xsp->xs_keys) {
	    xo_free(xsp->xs_keys);
	    xsp->xs_keys = NULL;
	}
    }

    xop->xo_depth += delta;	/* Record new depth */
    xop->xo_indent += indent;
}

void
xo_set_depth (xo_handle_t *xop, int depth)
{
    xop = xo_default(xop);

    if (xo_depth_check(xop, depth))
	return;

    xop->xo_depth += depth;
    xop->xo_indent += depth;
}

static xo_xsf_flags_t
xo_stack_flags (unsigned xflags)
{
    if (xflags & XOF_DTRT)
	return XSF_DTRT;
    return 0;
}

static void
xo_emit_top (xo_handle_t *xop, const char *ppn)
{
    xo_printf(xop, "%*s{%s", xo_indent(xop), "", ppn);
    xop->xo_flags |= XOF_TOP_EMITTED;

    if (xop->xo_version) {
	xo_printf(xop, "%*s\"__version\": \"%s\", %s",
		  xo_indent(xop), "", xop->xo_version, ppn);
	xo_free(xop->xo_version);
	xop->xo_version = NULL;
    }
}

static int
xo_do_open_container (xo_handle_t *xop, xo_xof_flags_t flags, const char *name)
{
    int rc = 0;
    const char *ppn = (xop->xo_flags & XOF_PRETTY) ? "\n" : "";
    const char *pre_nl = "";

    if (name == NULL) {
	xo_failure(xop, "NULL passed for container name");
	name = XO_FAILURE_NAME;
    }

    flags |= xop->xo_flags;	/* Pick up handle flags */

    switch (xo_style(xop)) {
    case XO_STYLE_XML:
	rc = xo_printf(xop, "%*s<%s", xo_indent(xop), "", name);

	if (xop->xo_attrs.xb_curp != xop->xo_attrs.xb_bufp) {
	    rc += xop->xo_attrs.xb_curp - xop->xo_attrs.xb_bufp;
	    xo_data_append(xop, xop->xo_attrs.xb_bufp,
			   xop->xo_attrs.xb_curp - xop->xo_attrs.xb_bufp);
	    xop->xo_attrs.xb_curp = xop->xo_attrs.xb_bufp;
	}

	rc += xo_printf(xop, ">%s", ppn);
	break;

    case XO_STYLE_JSON:
	xo_stack_set_flags(xop);

	if (!(xop->xo_flags & XOF_NO_TOP)
		&& !(xop->xo_flags & XOF_TOP_EMITTED))
	    xo_emit_top(xop, ppn);

	if (xop->xo_stack[xop->xo_depth].xs_flags & XSF_NOT_FIRST)
	    pre_nl = (xop->xo_flags & XOF_PRETTY) ? ",\n" : ", ";
	xop->xo_stack[xop->xo_depth].xs_flags |= XSF_NOT_FIRST;

	rc = xo_printf(xop, "%s%*s\"%s\": {%s",
		       pre_nl, xo_indent(xop), "", name, ppn);
	break;
    }

    xo_depth_change(xop, name, 1, 1, XSS_OPEN_CONTAINER,
		    xo_stack_flags(flags));

    return rc;
}

static int
xo_open_container_hf (xo_handle_t *xop, xo_xof_flags_t flags, const char *name)
{
    return xo_transition(xop, flags, name, XSS_OPEN_CONTAINER);
}

int
xo_open_container_h (xo_handle_t *xop, const char *name)
{
    return xo_open_container_hf(xop, 0, name);
}

int
xo_open_container (const char *name)
{
    return xo_open_container_hf(NULL, 0, name);
}

int
xo_open_container_hd (xo_handle_t *xop, const char *name)
{
    return xo_open_container_hf(xop, XOF_DTRT, name);
}

int
xo_open_container_d (const char *name)
{
    return xo_open_container_hf(NULL, XOF_DTRT, name);
}

static int
xo_do_close_container (xo_handle_t *xop, const char *name)
{
    xop = xo_default(xop);

    int rc = 0;
    const char *ppn = (xop->xo_flags & XOF_PRETTY) ? "\n" : "";
    const char *pre_nl = "";

    if (name == NULL) {
	xo_stack_t *xsp = &xop->xo_stack[xop->xo_depth];

	name = xsp->xs_name;
	if (name) {
	    int len = strlen(name) + 1;
	    /* We need to make a local copy; xo_depth_change will free it */
	    char *cp = alloca(len);
	    memcpy(cp, name, len);
	    name = cp;
	} else if (!(xsp->xs_flags & XSF_DTRT)) {
	    xo_failure(xop, "missing name without 'dtrt' mode");
	    name = XO_FAILURE_NAME;
	}
    }

    switch (xo_style(xop)) {
    case XO_STYLE_XML:
	xo_depth_change(xop, name, -1, -1, XSS_CLOSE_CONTAINER, 0);
	rc = xo_printf(xop, "%*s</%s>%s", xo_indent(xop), "", name, ppn);
	break;

    case XO_STYLE_JSON:
	pre_nl = (xop->xo_flags & XOF_PRETTY) ? "\n" : "";
	ppn = (xop->xo_depth <= 1) ? "\n" : "";

	xo_depth_change(xop, name, -1, -1, XSS_CLOSE_CONTAINER, 0);
	rc = xo_printf(xop, "%s%*s}%s", pre_nl, xo_indent(xop), "", ppn);
	xop->xo_stack[xop->xo_depth].xs_flags |= XSF_NOT_FIRST;
	break;

    case XO_STYLE_HTML:
    case XO_STYLE_TEXT:
	xo_depth_change(xop, name, -1, 0, XSS_CLOSE_CONTAINER, 0);
	break;
    }

    return rc;
}

int
xo_close_container_h (xo_handle_t *xop, const char *name)
{
    return xo_transition(xop, 0, name, XSS_CLOSE_CONTAINER);
}

int
xo_close_container (const char *name)
{
    return xo_close_container_h(NULL, name);
}

int
xo_close_container_hd (xo_handle_t *xop)
{
    return xo_close_container_h(xop, NULL);
}

int
xo_close_container_d (void)
{
    return xo_close_container_h(NULL, NULL);
}

static int
xo_do_open_list (xo_handle_t *xop, xo_xsf_flags_t flags, const char *name)
{
    int rc = 0;
    int indent = 0;

    xop = xo_default(xop);

    if (xo_style(xop) == XO_STYLE_JSON) {
	const char *ppn = (xop->xo_flags & XOF_PRETTY) ? "\n" : "";
	const char *pre_nl = "";

	indent = 1;
	if (!(xop->xo_flags & XOF_NO_TOP)
		&& !(xop->xo_flags & XOF_TOP_EMITTED))
	    xo_emit_top(xop, ppn);

	if (name == NULL) {
	    xo_failure(xop, "NULL passed for list name");
	    name = XO_FAILURE_NAME;
	}

	xo_stack_set_flags(xop);

	if (xop->xo_stack[xop->xo_depth].xs_flags & XSF_NOT_FIRST)
	    pre_nl = (xop->xo_flags & XOF_PRETTY) ? ",\n" : ", ";
	xop->xo_stack[xop->xo_depth].xs_flags |= XSF_NOT_FIRST;

	rc = xo_printf(xop, "%s%*s\"%s\": [%s",
		       pre_nl, xo_indent(xop), "", name, ppn);
    }

    xo_depth_change(xop, name, 1, indent, XSS_OPEN_LIST,
		    XSF_LIST | xo_stack_flags(flags));

    return rc;
}

static int
xo_open_list_hf (xo_handle_t *xop, xo_xsf_flags_t flags, const char *name)
{
    return xo_transition(xop, flags, name, XSS_OPEN_LIST);
}

int
xo_open_list_h (xo_handle_t *xop, const char *name UNUSED)
{
    return xo_open_list_hf(xop, 0, name);
}

int
xo_open_list (const char *name)
{
    return xo_open_list_hf(NULL, 0, name);
}

int
xo_open_list_hd (xo_handle_t *xop, const char *name UNUSED)
{
    return xo_open_list_hf(xop, XOF_DTRT, name);
}

int
xo_open_list_d (const char *name)
{
    return xo_open_list_hf(NULL, XOF_DTRT, name);
}

static int
xo_do_close_list (xo_handle_t *xop, const char *name)
{
    int rc = 0;
    const char *pre_nl = "";

    if (name == NULL) {
	xo_stack_t *xsp = &xop->xo_stack[xop->xo_depth];

	name = xsp->xs_name;
	if (name) {
	    int len = strlen(name) + 1;
	    /* We need to make a local copy; xo_depth_change will free it */
	    char *cp = alloca(len);
	    memcpy(cp, name, len);
	    name = cp;
	} else if (!(xsp->xs_flags & XSF_DTRT)) {
	    xo_failure(xop, "missing name without 'dtrt' mode");
	    name = XO_FAILURE_NAME;
	}
    }

    if (xo_style(xop) == XO_STYLE_JSON) {
	if (xop->xo_stack[xop->xo_depth].xs_flags & XSF_NOT_FIRST)
	    pre_nl = (xop->xo_flags & XOF_PRETTY) ? "\n" : "";
	xop->xo_stack[xop->xo_depth].xs_flags |= XSF_NOT_FIRST;

	xo_depth_change(xop, name, -1, -1, XSS_CLOSE_LIST, XSF_LIST);
	rc = xo_printf(xop, "%s%*s]", pre_nl, xo_indent(xop), "");
	xop->xo_stack[xop->xo_depth].xs_flags |= XSF_NOT_FIRST;

    } else {
	xo_depth_change(xop, name, -1, 0, XSS_CLOSE_LIST, XSF_LIST);
	xop->xo_stack[xop->xo_depth].xs_flags |= XSF_NOT_FIRST;
    }

    return rc;
}

int
xo_close_list_h (xo_handle_t *xop, const char *name)
{
    return xo_transition(xop, 0, name, XSS_CLOSE_LIST);
}

int
xo_close_list (const char *name)
{
    return xo_close_list_h(NULL, name);
}

int
xo_close_list_hd (xo_handle_t *xop)
{
    return xo_close_list_h(xop, NULL);
}

int
xo_close_list_d (void)
{
    return xo_close_list_h(NULL, NULL);
}

static int
xo_do_open_leaf_list (xo_handle_t *xop, xo_xsf_flags_t flags, const char *name)
{
    int rc = 0;
    int indent = 0;

    xop = xo_default(xop);

    if (xo_style(xop) == XO_STYLE_JSON) {
	const char *ppn = (xop->xo_flags & XOF_PRETTY) ? "\n" : "";
	const char *pre_nl = "";

	indent = 1;

	if (!(xop->xo_flags & XOF_NO_TOP)) {
	    if (!(xop->xo_flags & XOF_TOP_EMITTED)) {
		xo_printf(xop, "%*s{%s", xo_indent(xop), "", ppn);
		xop->xo_flags |= XOF_TOP_EMITTED;
	    }
	}

	if (name == NULL) {
	    xo_failure(xop, "NULL passed for list name");
	    name = XO_FAILURE_NAME;
	}

	xo_stack_set_flags(xop);

	if (xop->xo_stack[xop->xo_depth].xs_flags & XSF_NOT_FIRST)
	    pre_nl = (xop->xo_flags & XOF_PRETTY) ? ",\n" : ", ";
	xop->xo_stack[xop->xo_depth].xs_flags |= XSF_NOT_FIRST;

	rc = xo_printf(xop, "%s%*s\"%s\": [%s",
		       pre_nl, xo_indent(xop), "", name, ppn);
    }

    xo_depth_change(xop, name, 1, indent, XSS_OPEN_LEAF_LIST,
		    XSF_LIST | xo_stack_flags(flags));

    return rc;
}

static int
xo_do_close_leaf_list (xo_handle_t *xop, const char *name)
{
    int rc = 0;
    const char *pre_nl = "";

    if (name == NULL) {
	xo_stack_t *xsp = &xop->xo_stack[xop->xo_depth];

	name = xsp->xs_name;
	if (name) {
	    int len = strlen(name) + 1;
	    /* We need to make a local copy; xo_depth_change will free it */
	    char *cp = alloca(len);
	    memcpy(cp, name, len);
	    name = cp;
	} else if (!(xsp->xs_flags & XSF_DTRT)) {
	    xo_failure(xop, "missing name without 'dtrt' mode");
	    name = XO_FAILURE_NAME;
	}
    }

    if (xo_style(xop) == XO_STYLE_JSON) {
	if (xop->xo_stack[xop->xo_depth].xs_flags & XSF_NOT_FIRST)
	    pre_nl = (xop->xo_flags & XOF_PRETTY) ? "\n" : "";
	xop->xo_stack[xop->xo_depth].xs_flags |= XSF_NOT_FIRST;

	xo_depth_change(xop, name, -1, -1, XSS_CLOSE_LEAF_LIST, XSF_LIST);
	rc = xo_printf(xop, "%s%*s]", pre_nl, xo_indent(xop), "");
	xop->xo_stack[xop->xo_depth].xs_flags |= XSF_NOT_FIRST;

    } else {
	xo_depth_change(xop, name, -1, 0, XSS_CLOSE_LEAF_LIST, XSF_LIST);
	xop->xo_stack[xop->xo_depth].xs_flags |= XSF_NOT_FIRST;
    }

    return rc;
}

static int
xo_do_open_instance (xo_handle_t *xop, xo_xsf_flags_t flags, const char *name)
{
    xop = xo_default(xop);

    int rc = 0;
    const char *ppn = (xop->xo_flags & XOF_PRETTY) ? "\n" : "";
    const char *pre_nl = "";

    flags |= xop->xo_flags;

    if (name == NULL) {
	xo_failure(xop, "NULL passed for instance name");
	name = XO_FAILURE_NAME;
    }

    switch (xo_style(xop)) {
    case XO_STYLE_XML:
	rc = xo_printf(xop, "%*s<%s", xo_indent(xop), "", name);

	if (xop->xo_attrs.xb_curp != xop->xo_attrs.xb_bufp) {
	    rc += xop->xo_attrs.xb_curp - xop->xo_attrs.xb_bufp;
	    xo_data_append(xop, xop->xo_attrs.xb_bufp,
			   xop->xo_attrs.xb_curp - xop->xo_attrs.xb_bufp);
	    xop->xo_attrs.xb_curp = xop->xo_attrs.xb_bufp;
	}

	rc += xo_printf(xop, ">%s", ppn);
	break;

    case XO_STYLE_JSON:
	xo_stack_set_flags(xop);

	if (xop->xo_stack[xop->xo_depth].xs_flags & XSF_NOT_FIRST)
	    pre_nl = (xop->xo_flags & XOF_PRETTY) ? ",\n" : ", ";
	xop->xo_stack[xop->xo_depth].xs_flags |= XSF_NOT_FIRST;

	rc = xo_printf(xop, "%s%*s{%s",
		       pre_nl, xo_indent(xop), "", ppn);
	break;
    }

    xo_depth_change(xop, name, 1, 1, XSS_OPEN_INSTANCE, xo_stack_flags(flags));

    return rc;
}

static int
xo_open_instance_hf (xo_handle_t *xop, xo_xsf_flags_t flags, const char *name)
{
    return xo_transition(xop, flags, name, XSS_OPEN_INSTANCE);
}

int
xo_open_instance_h (xo_handle_t *xop, const char *name)
{
    return xo_open_instance_hf(xop, 0, name);
}

int
xo_open_instance (const char *name)
{
    return xo_open_instance_hf(NULL, 0, name);
}

int
xo_open_instance_hd (xo_handle_t *xop, const char *name)
{
    return xo_open_instance_hf(xop, XOF_DTRT, name);
}

int
xo_open_instance_d (const char *name)
{
    return xo_open_instance_hf(NULL, XOF_DTRT, name);
}

static int
xo_do_close_instance (xo_handle_t *xop, const char *name)
{
    xop = xo_default(xop);

    int rc = 0;
    const char *ppn = (xop->xo_flags & XOF_PRETTY) ? "\n" : "";
    const char *pre_nl = "";

    if (name == NULL) {
	xo_stack_t *xsp = &xop->xo_stack[xop->xo_depth];

	name = xsp->xs_name;
	if (name) {
	    int len = strlen(name) + 1;
	    /* We need to make a local copy; xo_depth_change will free it */
	    char *cp = alloca(len);
	    memcpy(cp, name, len);
	    name = cp;
	} else if (!(xsp->xs_flags & XSF_DTRT)) {
	    xo_failure(xop, "missing name without 'dtrt' mode");
	    name = XO_FAILURE_NAME;
	}
    }

    switch (xo_style(xop)) {
    case XO_STYLE_XML:
	xo_depth_change(xop, name, -1, -1, XSS_CLOSE_INSTANCE, 0);
	rc = xo_printf(xop, "%*s</%s>%s", xo_indent(xop), "", name, ppn);
	break;

    case XO_STYLE_JSON:
	pre_nl = (xop->xo_flags & XOF_PRETTY) ? "\n" : "";

	xo_depth_change(xop, name, -1, -1, XSS_CLOSE_INSTANCE, 0);
	rc = xo_printf(xop, "%s%*s}", pre_nl, xo_indent(xop), "");
	xop->xo_stack[xop->xo_depth].xs_flags |= XSF_NOT_FIRST;
	break;

    case XO_STYLE_HTML:
    case XO_STYLE_TEXT:
	xo_depth_change(xop, name, -1, 0, XSS_CLOSE_INSTANCE, 0);
	break;
    }

    return rc;
}

int
xo_close_instance_h (xo_handle_t *xop, const char *name)
{
    return xo_transition(xop, 0, name, XSS_CLOSE_INSTANCE);
}

int
xo_close_instance (const char *name)
{
    return xo_close_instance_h(NULL, name);
}

int
xo_close_instance_hd (xo_handle_t *xop)
{
    return xo_close_instance_h(xop, NULL);
}

int
xo_close_instance_d (void)
{
    return xo_close_instance_h(NULL, NULL);
}

static int
xo_do_close_all (xo_handle_t *xop, xo_stack_t *limit)
{
    xo_stack_t *xsp;
    int rc = 0;
    xo_xsf_flags_t flags;

    for (xsp = &xop->xo_stack[xop->xo_depth]; xsp >= limit; xsp--) {
	switch (xsp->xs_state) {
	case XSS_INIT:
	    /* Nothing */
	    rc = 0;
	    break;

	case XSS_OPEN_CONTAINER:
	    rc = xo_do_close_container(xop, NULL);
	    break;

	case XSS_OPEN_LIST:
	    rc = xo_do_close_list(xop, NULL);
	    break;

	case XSS_OPEN_INSTANCE:
	    rc = xo_do_close_instance(xop, NULL);
	    break;

	case XSS_OPEN_LEAF_LIST:
	    rc = xo_do_close_leaf_list(xop, NULL);
	    break;

	case XSS_MARKER:
	    flags = xsp->xs_flags & XSF_MARKER_FLAGS;
	    xo_depth_change(xop, xsp->xs_name, -1, 0, XSS_MARKER, 0);
	    xop->xo_stack[xop->xo_depth].xs_flags |= flags;
	    rc = 0;
	    break;
	}

	if (rc < 0)
	    xo_failure(xop, "close %d failed: %d", xsp->xs_state, rc);
    }

    return 0;
}

/*
 * This function is responsible for clearing out whatever is needed
 * to get to the desired state, if possible.
 */
static int
xo_do_close (xo_handle_t *xop, const char *name, xo_state_t new_state)
{
    xo_stack_t *xsp, *limit = NULL;
    int rc;
    xo_state_t need_state = new_state;

    if (new_state == XSS_CLOSE_CONTAINER)
	need_state = XSS_OPEN_CONTAINER;
    else if (new_state == XSS_CLOSE_LIST)
	need_state = XSS_OPEN_LIST;
    else if (new_state == XSS_CLOSE_INSTANCE)
	need_state = XSS_OPEN_INSTANCE;
    else if (new_state == XSS_CLOSE_LEAF_LIST)
	need_state = XSS_OPEN_LEAF_LIST;
    else if (new_state == XSS_MARKER)
	need_state = XSS_MARKER;
    else
	return 0; /* Unknown or useless new states are ignored */

    for (xsp = &xop->xo_stack[xop->xo_depth]; xsp > xop->xo_stack; xsp--) {
	/*
	 * Marker's normally stop us from going any further, unless
	 * we are popping a marker (new_state == XSS_MARKER).
	 */
	if (xsp->xs_state == XSS_MARKER && need_state != XSS_MARKER) {
	    if (name) {
		xo_failure(xop, "close (xo_%s) fails at marker '%s'; "
			   "not found '%s'",
			   xo_state_name(new_state),
			   xsp->xs_name, name);
		return 0;

	    } else {
		limit = xsp;
		xo_failure(xop, "close stops at marker '%s'", xsp->xs_name);
	    }
	    break;
	}
	
	if (xsp->xs_state != need_state)
	    continue;

	if (name && xsp->xs_name && strcmp(name, xsp->xs_name) != 0)
	    continue;

	limit = xsp;
	break;
    }

    if (limit == NULL) {
	xo_failure(xop, "xo_%s can't find match for '%s'",
		   xo_state_name(new_state), name);
	return 0;
    }

    rc = xo_do_close_all(xop, limit);

    return rc;
}

/*
 * We are in a given state and need to transition to the new state.
 */
static int
xo_transition (xo_handle_t *xop, xo_xsf_flags_t flags, const char *name,
	       xo_state_t new_state)
{
    xo_stack_t *xsp;
    int rc;
    int old_state, on_marker;

    xop = xo_default(xop);

    rc = 0;
    xsp = &xop->xo_stack[xop->xo_depth];
    old_state = xsp->xs_state;
    on_marker = (old_state == XSS_MARKER);

    /* If there's a marker on top of the stack, we need to find a real state */
    while (old_state == XSS_MARKER) {
	if (xsp == xop->xo_stack)
	    break;
	xsp -= 1;
	old_state = xsp->xs_state;
    }

    /*
     * At this point, the list of possible states are:
     *   XSS_INIT, XSS_OPEN_CONTAINER, XSS_OPEN_LIST,
     *   XSS_OPEN_INSTANCE, XSS_OPEN_LEAF_LIST, XSS_DISCARDING
     */
    switch (XSS_TRANSITION(old_state, new_state)) {

    open_container:
    case XSS_TRANSITION(XSS_INIT, XSS_OPEN_CONTAINER):
    case XSS_TRANSITION(XSS_OPEN_INSTANCE, XSS_OPEN_CONTAINER):
    case XSS_TRANSITION(XSS_OPEN_CONTAINER, XSS_OPEN_CONTAINER):
       rc = xo_do_open_container(xop, flags, name);
       break;

    case XSS_TRANSITION(XSS_OPEN_LIST, XSS_OPEN_CONTAINER):
	if (on_marker)
	    goto marker_prevents_close;
	rc = xo_do_close_list(xop, NULL);
	if (rc >= 0)
	    goto open_container;
	break;

    case XSS_TRANSITION(XSS_OPEN_LEAF_LIST, XSS_OPEN_CONTAINER):
	if (on_marker)
	    goto marker_prevents_close;
	rc = xo_do_close_leaf_list(xop, NULL);
	if (rc >= 0)
	    goto open_container;
	break;

    /*close_container:*/
    case XSS_TRANSITION(XSS_OPEN_CONTAINER, XSS_CLOSE_CONTAINER):
	if (on_marker)
	    goto marker_prevents_close;
	rc = xo_do_close(xop, name, new_state);
	break;

    case XSS_TRANSITION(XSS_INIT, XSS_CLOSE_CONTAINER):
	/* This is an exception for "xo --close" */
	rc = xo_do_close_container(xop, name);
	break;

    case XSS_TRANSITION(XSS_OPEN_LIST, XSS_CLOSE_CONTAINER):
    case XSS_TRANSITION(XSS_OPEN_INSTANCE, XSS_CLOSE_CONTAINER):
	if (on_marker)
	    goto marker_prevents_close;
	rc = xo_do_close(xop, name, new_state);
	break;

    case XSS_TRANSITION(XSS_OPEN_LEAF_LIST, XSS_CLOSE_CONTAINER):
	if (on_marker)
	    goto marker_prevents_close;
	rc = xo_do_close_leaf_list(xop, NULL);
	if (rc >= 0)
	    rc = xo_do_close(xop, name, new_state);
	break;

    open_list:
    case XSS_TRANSITION(XSS_INIT, XSS_OPEN_LIST):
    case XSS_TRANSITION(XSS_OPEN_CONTAINER, XSS_OPEN_LIST):
    case XSS_TRANSITION(XSS_OPEN_INSTANCE, XSS_OPEN_LIST):
	rc = xo_do_open_list(xop, flags, name);
	break;

    case XSS_TRANSITION(XSS_OPEN_LIST, XSS_OPEN_LIST):
	if (on_marker)
	    goto marker_prevents_close;
	rc = xo_do_close_list(xop, NULL);
	if (rc >= 0)
	    goto open_list;
	break;

    case XSS_TRANSITION(XSS_OPEN_LEAF_LIST, XSS_OPEN_LIST):
	if (on_marker)
	    goto marker_prevents_close;
	rc = xo_do_close_leaf_list(xop, NULL);
	if (rc >= 0)
	    goto open_list;
	break;

    /*close_list:*/
    case XSS_TRANSITION(XSS_OPEN_LIST, XSS_CLOSE_LIST):
	if (on_marker)
	    goto marker_prevents_close;
	rc = xo_do_close(xop, name, new_state);
	break;

    case XSS_TRANSITION(XSS_INIT, XSS_CLOSE_LIST):
    case XSS_TRANSITION(XSS_OPEN_CONTAINER, XSS_CLOSE_LIST):
    case XSS_TRANSITION(XSS_OPEN_INSTANCE, XSS_CLOSE_LIST):
    case XSS_TRANSITION(XSS_OPEN_LEAF_LIST, XSS_CLOSE_LIST):
	rc = xo_do_close(xop, name, new_state);
	break;

    open_instance:
    case XSS_TRANSITION(XSS_OPEN_LIST, XSS_OPEN_INSTANCE):
	rc = xo_do_open_instance(xop, flags, name);
	break;

    case XSS_TRANSITION(XSS_INIT, XSS_OPEN_INSTANCE):
    case XSS_TRANSITION(XSS_OPEN_CONTAINER, XSS_OPEN_INSTANCE):
	rc = xo_do_open_list(xop, flags, name);
	if (rc >= 0)
	    goto open_instance;
	break;

    case XSS_TRANSITION(XSS_OPEN_INSTANCE, XSS_OPEN_INSTANCE):
	if (on_marker) {
	    rc = xo_do_open_list(xop, flags, name);
	} else {
	    rc = xo_do_close_instance(xop, NULL);
	}
	if (rc >= 0)
	    goto open_instance;
	break;

    case XSS_TRANSITION(XSS_OPEN_LEAF_LIST, XSS_OPEN_INSTANCE):
	if (on_marker)
	    goto marker_prevents_close;
	rc = xo_do_close_leaf_list(xop, NULL);
	if (rc >= 0)
	    goto open_instance;
	break;

    /*close_instance:*/
    case XSS_TRANSITION(XSS_OPEN_INSTANCE, XSS_CLOSE_INSTANCE):
	if (on_marker)
	    goto marker_prevents_close;
	rc = xo_do_close_instance(xop, name);
	break;

    case XSS_TRANSITION(XSS_INIT, XSS_CLOSE_INSTANCE):
	/* This one makes no sense; ignore it */
	xo_failure(xop, "xo_close_instance ignored when called from "
		   "initial state ('%s')", name ?: "(unknown)");
	break;

    case XSS_TRANSITION(XSS_OPEN_CONTAINER, XSS_CLOSE_INSTANCE):
    case XSS_TRANSITION(XSS_OPEN_LIST, XSS_CLOSE_INSTANCE):
	if (on_marker)
	    goto marker_prevents_close;
	rc = xo_do_close(xop, name, new_state);
	break;

    case XSS_TRANSITION(XSS_OPEN_LEAF_LIST, XSS_CLOSE_INSTANCE):
	if (on_marker)
	    goto marker_prevents_close;
	rc = xo_do_close_leaf_list(xop, NULL);
	if (rc >= 0)
	    rc = xo_do_close(xop, name, new_state);
	break;

    open_leaf_list:
    case XSS_TRANSITION(XSS_OPEN_CONTAINER, XSS_OPEN_LEAF_LIST):
    case XSS_TRANSITION(XSS_OPEN_INSTANCE, XSS_OPEN_LEAF_LIST):
    case XSS_TRANSITION(XSS_INIT, XSS_OPEN_LEAF_LIST):
	rc = xo_do_open_leaf_list(xop, flags, name);
	break;

    case XSS_TRANSITION(XSS_OPEN_LIST, XSS_OPEN_LEAF_LIST):
    case XSS_TRANSITION(XSS_OPEN_LEAF_LIST, XSS_OPEN_LEAF_LIST):
	if (on_marker)
	    goto marker_prevents_close;
	rc = xo_do_close_list(xop, NULL);
	if (rc >= 0)
	    goto open_leaf_list;
	break;

    /*close_leaf_list:*/
    case XSS_TRANSITION(XSS_OPEN_LEAF_LIST, XSS_CLOSE_LEAF_LIST):
	if (on_marker)
	    goto marker_prevents_close;
	rc = xo_do_close_leaf_list(xop, name);
	break;

    case XSS_TRANSITION(XSS_INIT, XSS_CLOSE_LEAF_LIST):
	/* Makes no sense; ignore */
	xo_failure(xop, "xo_close_leaf_list ignored when called from "
		   "initial state ('%s')", name ?: "(unknown)");
	break;

    case XSS_TRANSITION(XSS_OPEN_CONTAINER, XSS_CLOSE_LEAF_LIST):
    case XSS_TRANSITION(XSS_OPEN_LIST, XSS_CLOSE_LEAF_LIST):
    case XSS_TRANSITION(XSS_OPEN_INSTANCE, XSS_CLOSE_LEAF_LIST):
	if (on_marker)
	    goto marker_prevents_close;
	rc = xo_do_close(xop, name, new_state);
	break;

    /*emit:*/
    case XSS_TRANSITION(XSS_OPEN_CONTAINER, XSS_EMIT):
    case XSS_TRANSITION(XSS_OPEN_INSTANCE, XSS_EMIT):
	break;

    case XSS_TRANSITION(XSS_OPEN_LIST, XSS_EMIT):
	if (on_marker)
	    goto marker_prevents_close;
	rc = xo_do_close(xop, NULL, XSS_CLOSE_LIST);
	break;

    case XSS_TRANSITION(XSS_INIT, XSS_EMIT):
	break;

    case XSS_TRANSITION(XSS_OPEN_LEAF_LIST, XSS_EMIT):
	if (on_marker)
	    goto marker_prevents_close;
	rc = xo_do_close_leaf_list(xop, NULL);
	break;

    /*emit_leaf_list:*/
    case XSS_TRANSITION(XSS_INIT, XSS_EMIT_LEAF_LIST):
    case XSS_TRANSITION(XSS_OPEN_CONTAINER, XSS_EMIT_LEAF_LIST):
    case XSS_TRANSITION(XSS_OPEN_INSTANCE, XSS_EMIT_LEAF_LIST):
	rc = xo_do_open_leaf_list(xop, flags, name);
	break;

    case XSS_TRANSITION(XSS_OPEN_LEAF_LIST, XSS_EMIT_LEAF_LIST):
	break;

    case XSS_TRANSITION(XSS_OPEN_LIST, XSS_EMIT_LEAF_LIST):
	/*
	 * We need to be backward compatible with the pre-xo_open_leaf_list
	 * API, where both lists and leaf-lists were opened as lists.  So
	 * if we find an open list that hasn't had anything written to it,
	 * we'll accept it.
	 */
	break;

    default:
	xo_failure(xop, "unknown transition: (%u -> %u)",
		   xsp->xs_state, new_state);
    }

    return rc;

 marker_prevents_close:
    xo_failure(xop, "marker '%s' prevents transition from %s to %s",
	       xop->xo_stack[xop->xo_depth].xs_name,
	       xo_state_name(old_state), xo_state_name(new_state));
    return -1;
}

int
xo_open_marker_h (xo_handle_t *xop, const char *name)
{
    xop = xo_default(xop);

    xo_depth_change(xop, name, 1, 0, XSS_MARKER,
		    xop->xo_stack[xop->xo_depth].xs_flags & XSF_MARKER_FLAGS);

    return 0;
}

int
xo_open_marker (const char *name)
{
    return xo_open_marker_h(NULL, name);
}

int
xo_close_marker_h (xo_handle_t *xop, const char *name)
{
    xop = xo_default(xop);

    return xo_do_close(xop, name, XSS_MARKER);
}

int
xo_close_marker (const char *name)
{
    return xo_close_marker_h(NULL, name);
}

void
xo_set_writer (xo_handle_t *xop, void *opaque, xo_write_func_t write_func,
	       xo_close_func_t close_func, xo_flush_func_t flush_func)
{
    xop = xo_default(xop);

    xop->xo_opaque = opaque;
    xop->xo_write = write_func;
    xop->xo_close = close_func;
    xop->xo_flush = flush_func;
}

void
xo_set_allocator (xo_realloc_func_t realloc_func, xo_free_func_t free_func)
{
    xo_realloc = realloc_func;
    xo_free = free_func;
}

int
xo_flush_h (xo_handle_t *xop)
{
    static char div_close[] = "</div>";
    int rc;

    xop = xo_default(xop);

    switch (xo_style(xop)) {
    case XO_STYLE_HTML:
	if (xop->xo_flags & XOF_DIV_OPEN) {
	    xop->xo_flags &= ~XOF_DIV_OPEN;
	    xo_data_append(xop, div_close, sizeof(div_close) - 1);

	    if (xop->xo_flags & XOF_PRETTY)
		xo_data_append(xop, "\n", 1);
	}
	break;
    }

    rc = xo_write(xop);
    if (rc >= 0 && xop->xo_flush)
	if (xop->xo_flush(xop->xo_opaque) < 0)
	    return -1;

    return rc;
}

int
xo_flush (void)
{
    return xo_flush_h(NULL);
}

int
xo_finish_h (xo_handle_t *xop)
{
    const char *cp = "";
    xop = xo_default(xop);

    if (!(xop->xo_flags & XOF_NO_CLOSE))
	xo_do_close_all(xop, xop->xo_stack);

    switch (xo_style(xop)) {
    case XO_STYLE_JSON:
	if (!(xop->xo_flags & XOF_NO_TOP)) {
	    if (xop->xo_flags & XOF_TOP_EMITTED)
		xop->xo_flags &= ~XOF_TOP_EMITTED; /* Turn off before output */
	    else
		cp = "{ ";
	    xo_printf(xop, "%*s%s}\n",xo_indent(xop), "", cp);
	}
	break;
    }

    return xo_flush_h(xop);
}

int
xo_finish (void)
{
    return xo_finish_h(NULL);
}

/*
 * Generate an error message, such as would be displayed on stderr
 */
void
xo_error_hv (xo_handle_t *xop, const char *fmt, va_list vap)
{
    xop = xo_default(xop);

    /*
     * If the format string doesn't end with a newline, we pop
     * one on ourselves.
     */
    int len = strlen(fmt);
    if (len > 0 && fmt[len - 1] != '\n') {
	char *newfmt = alloca(len + 2);
	memcpy(newfmt, fmt, len);
	newfmt[len] = '\n';
	newfmt[len] = '\0';
	fmt = newfmt;
    }

    switch (xo_style(xop)) {
    case XO_STYLE_TEXT:
	vfprintf(stderr, fmt, vap);
	break;

    case XO_STYLE_HTML:
	va_copy(xop->xo_vap, vap);
	
	xo_buf_append_div(xop, "error", 0, NULL, 0, fmt, strlen(fmt), NULL, 0);

	if (xop->xo_flags & XOF_DIV_OPEN)
	    xo_line_close(xop);

	xo_write(xop);

	va_end(xop->xo_vap);
	bzero(&xop->xo_vap, sizeof(xop->xo_vap));
	break;

    case XO_STYLE_XML:
    case XO_STYLE_JSON:
	va_copy(xop->xo_vap, vap);

	xo_open_container_h(xop, "error");
	xo_format_value(xop, "message", 7, fmt, strlen(fmt), NULL, 0, 0);
	xo_close_container_h(xop, "error");

	va_end(xop->xo_vap);
	bzero(&xop->xo_vap, sizeof(xop->xo_vap));
	break;
    }
}

void
xo_error_h (xo_handle_t *xop, const char *fmt, ...)
{
    va_list vap;

    va_start(vap, fmt);
    xo_error_hv(xop, fmt, vap);
    va_end(vap);
}

/*
 * Generate an error message, such as would be displayed on stderr
 */
void
xo_error (const char *fmt, ...)
{
    va_list vap;

    va_start(vap, fmt);
    xo_error_hv(NULL, fmt, vap);
    va_end(vap);
}

int
xo_parse_args (int argc, char **argv)
{
    static char libxo_opt[] = "--libxo";
    char *cp;
    int i, save;

    /* Save our program name for xo_err and friends */
    xo_program = argv[0];
    cp = strrchr(xo_program, '/');
    if (cp)
	xo_program = cp + 1;

    for (save = i = 1; i < argc; i++) {
	if (argv[i] == NULL
	    || strncmp(argv[i], libxo_opt, sizeof(libxo_opt) - 1) != 0) {
	    if (save != i)
		argv[save] = argv[i];
	    save += 1;
	    continue;
	}

	cp = argv[i] + sizeof(libxo_opt) - 1;
	if (*cp == 0) {
	    cp = argv[++i];
	    if (cp == 0) {
		xo_warnx("missing libxo option");
		return -1;
	    }
		
	    if (xo_set_options(NULL, cp) < 0)
		return -1;
	} else if (*cp == ':') {
	    if (xo_set_options(NULL, cp) < 0)
		return -1;

	} else if (*cp == '=') {
	    if (xo_set_options(NULL, ++cp) < 0)
		return -1;

	} else if (*cp == '-') {
	    cp += 1;
	    if (strcmp(cp, "check") == 0) {
		exit(XO_HAS_LIBXO);

	    } else {
		xo_warnx("unknown libxo option: '%s'", argv[i]);
		return -1;
	    }
	} else {
		xo_warnx("unknown libxo option: '%s'", argv[i]);
	    return -1;
	}
    }

    argv[save] = NULL;
    return save;
}

void
xo_dump_stack (xo_handle_t *xop)
{
    int i;
    xo_stack_t *xsp;

    xop = xo_default(xop);

    fprintf(stderr, "Stack dump:\n");

    xsp = xop->xo_stack;
    for (i = 1, xsp++; i <= xop->xo_depth; i++, xsp++) {
	fprintf(stderr, "   [%d] %s '%s' [%x]\n",
		i, xo_state_name(xsp->xs_state),
		xsp->xs_name ?: "--", xsp->xs_flags);
    }
}

void
xo_set_program (const char *name)
{
    xo_program = name;
}

void
xo_set_version_h (xo_handle_t *xop, const char *version UNUSED)
{
    xop = xo_default(xop);

    if (version == NULL || strchr(version, '"') != NULL)
	return;

    switch (xo_style(xop)) {
    case XO_STYLE_XML:
	/* For XML, we record this as an attribute for the first tag */
	xo_attr_h(xop, "__version", "%s", version);
	break;

    case XO_STYLE_JSON:
	{
	    /*
	     * For XML, we record the version string in our handle, and emit
	     * it in xo_emit_top.
	     */
	    int len = strlen(version) + 1;
	    xop->xo_version = xo_realloc(NULL, len);
	    if (xop->xo_version)
		memcpy(xop->xo_version, version, len);
	}
	break;
    }
}

void
xo_set_version (const char *version)
{
    xo_set_version_h(NULL, version);
}

#ifdef UNIT_TEST
int
main (int argc, char **argv)
{
    static char base_grocery[] = "GRO";
    static char base_hardware[] = "HRD";
    struct item {
	const char *i_title;
	int i_sold;
	int i_instock;
	int i_onorder;
	const char *i_sku_base;
	int i_sku_num;
    };
    struct item list[] = {
	{ "gum&this&that", 1412, 54, 10, base_grocery, 415 },
	{ "<rope>", 85, 4, 2, base_hardware, 212 },
	{ "ladder", 0, 2, 1, base_hardware, 517 },
	{ "\"bolt\"", 4123, 144, 42, base_hardware, 632 },
	{ "water\\blue", 17, 14, 2, base_grocery, 2331 },
	{ NULL, 0, 0, 0, NULL, 0 }
    };
    struct item list2[] = {
	{ "fish", 1321, 45, 1, base_grocery, 533 },
	{ NULL, 0, 0, 0, NULL, 0 }
    };
    struct item *ip;
    xo_info_t info[] = {
	{ "in-stock", "number", "Number of items in stock" },
	{ "name", "string", "Name of the item" },
	{ "on-order", "number", "Number of items on order" },
	{ "sku", "string", "Stock Keeping Unit" },
	{ "sold", "number", "Number of items sold" },
	{ NULL, NULL, NULL },
    };
    int info_count = (sizeof(info) / sizeof(info[0])) - 1;
    
    argc = xo_parse_args(argc, argv);
    if (argc < 0)
	exit(1);

    xo_set_info(NULL, info, info_count);

    xo_open_container_h(NULL, "top");

    xo_open_container("data");
    xo_open_list("item");

    xo_emit("{T:Item/%-15s}{T:Total Sold/%12s}{T:In Stock/%12s}"
	    "{T:On Order/%12s}{T:SKU/%5s}\n");

    for (ip = list; ip->i_title; ip++) {
	xo_open_instance("item");

	xo_emit("{k:name/%-15s/%s}{n:sold/%12u/%u}{:in-stock/%12u/%u}"
		"{:on-order/%12u/%u} {q:sku/%5s-000-%u/%s-000-%u}\n",
		ip->i_title, ip->i_sold, ip->i_instock, ip->i_onorder,
		ip->i_sku_base, ip->i_sku_num);

	xo_close_instance("item");
    }

    xo_close_list("item");
    xo_close_container("data");

    xo_emit("\n\n");

    xo_open_container("data");
    xo_open_list("item");

    for (ip = list; ip->i_title; ip++) {
	xo_open_instance("item");

	xo_attr("fancy", "%s%d", "item", ip - list);
	xo_emit("{L:Item} '{k:name/%s}':\n", ip->i_title);
	xo_emit("{P:   }{L:Total sold}: {n:sold/%u%s}{e:percent/%u}\n",
		ip->i_sold, ip->i_sold ? ".0" : "", 44);
	xo_emit("{P:   }{Lcw:In stock}{:in-stock/%u}\n", ip->i_instock);
	xo_emit("{P:   }{Lcw:On order}{:on-order/%u}\n", ip->i_onorder);
	xo_emit("{P:   }{L:SKU}: {q:sku/%s-000-%u}\n",
		ip->i_sku_base, ip->i_sku_num);

	xo_close_instance("item");
    }

    xo_close_list("item");
    xo_close_container("data");

    xo_open_container("data");
    xo_open_list("item");

    for (ip = list2; ip->i_title; ip++) {
	xo_open_instance("item");

	xo_emit("{L:Item} '{k:name/%s}':\n", ip->i_title);
	xo_emit("{P:   }{L:Total sold}: {n:sold/%u%s}\n",
		ip->i_sold, ip->i_sold ? ".0" : "");
	xo_emit("{P:   }{Lcw:In stock}{:in-stock/%u}\n", ip->i_instock);
	xo_emit("{P:   }{Lcw:On order}{:on-order/%u}\n", ip->i_onorder);
	xo_emit("{P:   }{L:SKU}: {q:sku/%s-000-%u}\n",
		ip->i_sku_base, ip->i_sku_num);

	xo_open_list("month");

	const char *months[] = { "Jan", "Feb", "Mar", NULL };
	int discounts[] = { 10, 20, 25, 0 };
	int i;
	for (i = 0; months[i]; i++) {
	    xo_open_instance("month");
	    xo_emit("{P:       }"
		    "{Lwc:Month}{k:month}, {Lwc:Special}{:discount/%d}\n",
		    months[i], discounts[i]);
	    xo_close_instance("month");
	}
	
	xo_close_list("month");

	xo_close_instance("item");
    }

    xo_close_list("item");
    xo_close_container("data");

    xo_close_container_h(NULL, "top");

    xo_finish();

    return 0;
}
#endif /* UNIT_TEST */
