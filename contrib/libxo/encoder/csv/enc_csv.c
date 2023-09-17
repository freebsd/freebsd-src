/*
 * Copyright (c) 2015, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 * Phil Shafer, August 2015
 */

/*
 * CSV encoder generates comma-separated value files for specific
 * subsets of data.  This is not (and cannot be) a generalized
 * facility, but for specific subsets of data, CSV data can be
 * reasonably generated.  For example, the df XML content:
 *     <filesystem>
 *      <name>procfs</name>
 *      <total-blocks>4</total-blocks>
 *      <used-blocks>4</used-blocks>
 *      <available-blocks>0</available-blocks>
 *      <used-percent>100</used-percent>
 *      <mounted-on>/proc</mounted-on>
 *    </filesystem>
 *
 * could be represented as:
 *
 *  #+name,total-blocks,used-blocks,available-blocks,used-percent,mounted-on
 *  procfs,4,4,0,100,/proc
 *
 * Data is then constrained to be sibling leaf values.  In addition,
 * singular leafs can also be matched.  The costs include recording
 * the specific leaf names (to ensure consistency) and some
 * buffering.
 *
 * Some escaping is needed for CSV files, following the rules of RFC4180:
 *
 * - Fields containing a line-break, double-quote or commas should be
 *   quoted. (If they are not, the file will likely be impossible to
 *   process correctly).
 * - A (double) quote character in a field must be represented by two
 *   (double) quote characters.
 * - Leading and trialing whitespace require fields be quoted.
 *
 * Cheesy, but simple.  The RFC also requires MS-DOS end-of-line,
 * which we only do with the "dos" option.  Strange that we still live
 * in a DOS-friendly world, but then again, we make spaceships based
 * on the horse butts (http://www.astrodigital.org/space/stshorse.html
 * though the "built by English expatriates” bit is rubbish; better to
 * say the first engines used in America were built by Englishmen.)
 */

#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>

#include "xo.h"
#include "xo_encoder.h"
#include "xo_buf.h"

#ifndef UNUSED
#define UNUSED __attribute__ ((__unused__))
#endif /* UNUSED */

/*
 * The CSV encoder has three moving parts:
 *
 * - The path holds the path we are matching against
 *   - This is given as input via "options" and does not change
 *
 * - The stack holds the current names of the open elements
 *   - The "open" operations push, while the "close" pop
 *   - Turns out, at this point, the stack is unused, but I've
 *     left "drippings" in the code because I see this as useful
 *     for future features (under CSV_STACK_IS_NEEDED).
 *
 * - The leafs record the current set of leaf
 *   - A key from the parent list counts as a leaf (unless CF_NO_KEYS)
 *   - Once the path is matched, all other leafs at that level are leafs
 *   - Leafs are recorded to get the header comment accurately recorded
 *   - Once the first line is emited, the set of leafs _cannot_ change
 *
 * We use offsets into the buffers, since we know they can be
 * realloc'd out from under us, as the size increases.  The 'path'
 * is fixed, we allocate it once, so it doesn't need offsets.
 */
typedef struct path_frame_s {
    char *pf_name;	       /* Path member name; points into c_path_buf */
    uint32_t pf_flags;	       /* Flags for this path element (PFF_*) */
} path_frame_t;

typedef struct stack_frame_s {
    ssize_t sf_off;		/* Element name; offset in c_stack_buf */
    uint32_t sf_flags;		/* Flags for this frame (SFF_*) */
} stack_frame_t;

/* Flags for sf_flags */

typedef struct leaf_s {
    ssize_t f_name;		/* Name of leaf; offset in c_name_buf */
    ssize_t f_value;		/* Value of leaf; offset in c_value_buf */
    uint32_t f_flags;		/* Flags for this value (FF_*)  */
#ifdef CSV_STACK_IS_NEEDED
    ssize_t f_depth;		/* Depth of stack when leaf was recorded */
#endif /* CSV_STACK_IS_NEEDED */
} leaf_t;

/* Flags for f_flags */
#define LF_KEY		(1<<0)	/* Leaf is a key */
#define LF_HAS_VALUE	(1<<1)	/* Value has been set */

typedef struct csv_private_s {
    uint32_t c_flags;		/* Flags for this encoder */

    /* The path for which we select leafs */
    char *c_path_buf;	    	/* Buffer containing path members */
    path_frame_t *c_path;	/* Array of path members */
    ssize_t c_path_max;		/* Depth of c_path[] */
    ssize_t c_path_cur;		/* Current depth in c_path[] */

    /* A stack of open elements (xo_op_list, xo_op_container) */
#if CSV_STACK_IS_NEEDED
    xo_buffer_t c_stack_buf;	/* Buffer used for stack content */
    stack_frame_t *c_stack;	/* Stack of open tags */
    ssize_t c_stack_max;	/* Maximum stack depth */
#endif /* CSV_STACK_IS_NEEDED */
    ssize_t c_stack_depth;	/* Current stack depth */

    /* List of leafs we are emitting (to ensure consistency) */
    xo_buffer_t c_name_buf;	/* String buffer for leaf names */
    xo_buffer_t c_value_buf;	/* String buffer for leaf values */
    leaf_t *c_leaf;		/* List of leafs */
    ssize_t c_leaf_depth;	/* Current depth of c_leaf[] (next free) */
    ssize_t c_leaf_max;		/* Max depth of c_leaf[] */

    xo_buffer_t c_data;		/* Buffer for creating data */
} csv_private_t;

#define C_STACK_MAX	32	/* default c_stack_max */
#define C_LEAF_MAX	32	/* default c_leaf_max */

/* Flags for this structure */
#define CF_HEADER_DONE	(1<<0)	/* Have already written the header */
#define CF_NO_HEADER	(1<<1)	/* Do not generate header */
#define CF_NO_KEYS	(1<<2)	/* Do not generate excess keys */
#define CF_VALUE_ONLY	(1<<3)	/* Only generate the value */

#define CF_DOS_NEWLINE	(1<<4)	/* Generate CR-NL, just like MS-DOS */
#define CF_LEAFS_DONE	(1<<5)	/* Leafs are already been recorded */
#define CF_NO_QUOTES	(1<<6)	/* Do not generate quotes */
#define CF_RECORD_DATA	(1<<7)	/* Record all sibling leafs */

#define CF_DEBUG	(1<<8)	/* Make debug output */
#define CF_HAS_PATH	(1<<9)	/* A "path" option was provided */

/*
 * A simple debugging print function, similar to psu_dbg.  Controlled by
 * the undocumented "debug" option.
 */
static void
csv_dbg (xo_handle_t *xop UNUSED, csv_private_t *csv UNUSED,
	 const char *fmt, ...)
{
    if (csv == NULL || !(csv->c_flags & CF_DEBUG))
	return;

    va_list vap;

    va_start(vap, fmt);
    vfprintf(stderr, fmt, vap);
    va_end(vap);
}

/*
 * Create the private data for this handle, initialize it, and record
 * the pointer in the handle.
 */
static int
csv_create (xo_handle_t *xop)
{
    csv_private_t *csv = xo_realloc(NULL, sizeof(*csv));
    if (csv == NULL)
	return -1;

    bzero(csv, sizeof(*csv));
    xo_buf_init(&csv->c_data);
    xo_buf_init(&csv->c_name_buf);
    xo_buf_init(&csv->c_value_buf);
#ifdef CSV_STACK_IS_NEEDED
    xo_buf_init(&csv->c_stack_buf);
#endif /* CSV_STACK_IS_NEEDED */

    xo_set_private(xop, csv);

    return 0;
}

/*
 * Clean up and release any data in use by this handle
 */
static void
csv_destroy (xo_handle_t *xop UNUSED, csv_private_t *csv)
{
    /* Clean up */
    xo_buf_cleanup(&csv->c_data);
    xo_buf_cleanup(&csv->c_name_buf);
    xo_buf_cleanup(&csv->c_value_buf);
#ifdef CSV_STACK_IS_NEEDED
    xo_buf_cleanup(&csv->c_stack_buf);
#endif /* CSV_STACK_IS_NEEDED */

    if (csv->c_leaf)
	xo_free(csv->c_leaf);
    if (csv->c_path_buf)
	xo_free(csv->c_path_buf);
}

/*
 * Return the element name at the top of the path stack.  This is the
 * item that we are currently trying to match on.
 */
static const char *
csv_path_top (csv_private_t *csv, ssize_t delta)
{
    if (!(csv->c_flags & CF_HAS_PATH) || csv->c_path == NULL)
	return NULL;

    ssize_t cur = csv->c_path_cur + delta;

    if (cur < 0)
	return NULL;

    return csv->c_path[cur].pf_name;
}

/*
 * Underimplemented stack functionality
 */
static inline void
csv_stack_push (csv_private_t *csv UNUSED, const char *name UNUSED)
{
#ifdef CSV_STACK_IS_NEEDED
    csv->c_stack_depth += 1;
#endif /* CSV_STACK_IS_NEEDED */
}

/*
 * Underimplemented stack functionality
 */
static inline void
csv_stack_pop (csv_private_t *csv UNUSED, const char *name UNUSED)
{
#ifdef CSV_STACK_IS_NEEDED
    csv->c_stack_depth -= 1;
#endif /* CSV_STACK_IS_NEEDED */
}

/* Flags for csv_quote_flags */
#define QF_NEEDS_QUOTES	(1<<0)		/* Needs to be quoted */
#define QF_NEEDS_ESCAPE	(1<<1)		/* Needs to be escaped */

/*
 * Determine how much quote processing is needed.  The details of the
 * quoting rules are given at the top of this file.  We return a set
 * of flags, indicating what's needed.
 */
static uint32_t
csv_quote_flags (xo_handle_t *xop UNUSED, csv_private_t *csv UNUSED,
		  const char *value)
{
    static const char quoted[] = "\n\r\",";
    static const char escaped[] = "\"";

    if (csv->c_flags & CF_NO_QUOTES)	/* User doesn't want quotes */
	return 0;

    size_t len = strlen(value);
    uint32_t rc = 0;

    if (strcspn(value, quoted) != len)
	rc |= QF_NEEDS_QUOTES;
    else if (isspace((int) value[0]))	/* Leading whitespace */
	rc |= QF_NEEDS_QUOTES;
    else if (isspace((int) value[len - 1])) /* Trailing whitespace */
	rc |= QF_NEEDS_QUOTES;

    if (strcspn(value, escaped) != len)
	rc |= QF_NEEDS_ESCAPE;

    csv_dbg(xop, csv, "csv: quote flags [%s] -> %x (%zu/%zu)\n",
	    value, rc, len, strcspn(value, quoted));

    return rc;
}

/*
 * Escape the string, following the rules in RFC4180
 */
static void
csv_escape (xo_buffer_t *xbp, const char *value, size_t len)
{
    const char *cp, *ep, *np;

    for (cp = value, ep = value + len; cp && cp < ep; cp = np) {
	np = strchr(cp, '"');
	if (np) {
	    np += 1;
	    xo_buf_append(xbp, cp, np - cp);
	    xo_buf_append(xbp, "\"", 1);
	} else
	    xo_buf_append(xbp, cp, ep - cp);
    }
}

/*
 * Append a newline to the buffer, following the settings of the "dos"
 * flag.
 */
static void
csv_append_newline (xo_buffer_t *xbp, csv_private_t *csv)
{
    if (csv->c_flags & CF_DOS_NEWLINE)
	xo_buf_append(xbp, "\r\n", 2);
    else 
	xo_buf_append(xbp, "\n", 1);
}

/*
 * Create a 'record' of 'fields' from our recorded leaf values.  If
 * this is the first line and "no-header" isn't given, make a record
 * containing the leaf names.
 */
static void
csv_emit_record (xo_handle_t *xop, csv_private_t *csv)
{
    csv_dbg(xop, csv, "csv: emit: ...\n");

    ssize_t fnum;
    uint32_t quote_flags;
    leaf_t *lp;

    /* If we have no data, then don't bother */
    if (csv->c_leaf_depth == 0)
	return;

    if (!(csv->c_flags & (CF_HEADER_DONE | CF_NO_HEADER))) {
	csv->c_flags |= CF_HEADER_DONE;

	for (fnum = 0; fnum < csv->c_leaf_depth; fnum++) {
	    lp = &csv->c_leaf[fnum];
	    const char *name = xo_buf_data(&csv->c_name_buf, lp->f_name);

	    if (fnum != 0)
		xo_buf_append(&csv->c_data, ",", 1);

	    xo_buf_append(&csv->c_data, name, strlen(name));
	}

	csv_append_newline(&csv->c_data, csv);
    }

    for (fnum = 0; fnum < csv->c_leaf_depth; fnum++) {
	lp = &csv->c_leaf[fnum];
	const char *value;

	if (lp->f_flags & LF_HAS_VALUE) {
	    value = xo_buf_data(&csv->c_value_buf, lp->f_value);
	} else {
	    value = "";
	}

	quote_flags = csv_quote_flags(xop, csv, value);

	if (fnum != 0)
	    xo_buf_append(&csv->c_data, ",", 1);

	if (quote_flags & QF_NEEDS_QUOTES)
	    xo_buf_append(&csv->c_data, "\"", 1);

	if (quote_flags & QF_NEEDS_ESCAPE)
	    csv_escape(&csv->c_data, value, strlen(value));
	else
	    xo_buf_append(&csv->c_data, value, strlen(value));

	if (quote_flags & QF_NEEDS_QUOTES)
	    xo_buf_append(&csv->c_data, "\"", 1);
    }

    csv_append_newline(&csv->c_data, csv);

    /* We flush if either flush flag is set */
    if (xo_get_flags(xop) & (XOF_FLUSH | XOF_FLUSH_LINE))
	xo_flush_h(xop);

    /* Clean out values from leafs */
    for (fnum = 0; fnum < csv->c_leaf_depth; fnum++) {
	lp = &csv->c_leaf[fnum];

	lp->f_flags &= ~LF_HAS_VALUE;
	lp->f_value = 0;
    }

    xo_buf_reset(&csv->c_value_buf);

    /*
     * Once we emit the first line, our set of leafs is locked and
     * cannot be changed.
     */
    csv->c_flags |= CF_LEAFS_DONE;
}

/*
 * Open a "level" of hierarchy, either a container or an instance.  Look
 * for a match in the path=x/y/z hierarchy, and ignore if not a match.
 * If we're at the end of the path, start recording leaf values.
 */
static int
csv_open_level (xo_handle_t *xop UNUSED, csv_private_t *csv,
		const char *name, int instance)
{
    /* An new "open" event means we stop recording */
    if (csv->c_flags & CF_RECORD_DATA) {
	csv->c_flags &= ~CF_RECORD_DATA;
	csv_emit_record(xop, csv);
	return 0;
    }

    const char *path_top = csv_path_top(csv, 0);

    /* If the top of the stack does not match the name, then ignore */
    if (path_top == NULL) {
	if (instance && !(csv->c_flags & CF_HAS_PATH)) {
	    csv_dbg(xop, csv, "csv: recording (no-path) ...\n");
	    csv->c_flags |= CF_RECORD_DATA;
	}

    } else if (xo_streq(path_top, name)) {
	csv->c_path_cur += 1;		/* Advance to next path member */

	csv_dbg(xop, csv, "csv: match: [%s] (%zd/%zd)\n", name,
	       csv->c_path_cur, csv->c_path_max);

	/* If we're all the way thru the path members, start recording */
	if (csv->c_path_cur == csv->c_path_max) {
	    csv_dbg(xop, csv, "csv: recording ...\n");
	    csv->c_flags |= CF_RECORD_DATA;
	}
    }

    /* Push the name on the stack */
    csv_stack_push(csv, name);

    return 0;
}

/*
 * Close a "level", either a container or an instance.
 */
static int
csv_close_level (xo_handle_t *xop UNUSED, csv_private_t *csv, const char *name)
{
    /* If we're recording, a close triggers an emit */
    if (csv->c_flags & CF_RECORD_DATA) {
	csv->c_flags &= ~CF_RECORD_DATA;
	csv_emit_record(xop, csv);
    }

    const char *path_top = csv_path_top(csv, -1);
    csv_dbg(xop, csv, "csv: close: [%s] [%s] (%zd)\n", name,
	   path_top ?: "", csv->c_path_cur);

    /* If the top of the stack does not match the name, then ignore */
    if (path_top != NULL && xo_streq(path_top, name)) {
	csv->c_path_cur -= 1;
	return 0;
    }

    /* Pop the name off the stack */
    csv_stack_pop(csv, name);

    return 0;
}

/*
 * Return the index of a given leaf in the c_leaf[] array, where we
 * record leaf values.  If the leaf is new and we haven't stopped recording
 * leafs, then make a new slot for it and record the name.
 */
static int
csv_leaf_num (xo_handle_t *xop UNUSED, csv_private_t *csv,
	       const char *name, xo_xff_flags_t flags)
{
    ssize_t fnum;
    leaf_t *lp;
    xo_buffer_t *xbp = &csv->c_name_buf;

    for (fnum = 0; fnum < csv->c_leaf_depth; fnum++) {
	lp = &csv->c_leaf[fnum];

	const char *fname = xo_buf_data(xbp, lp->f_name);
	if (xo_streq(fname, name))
	    return fnum;
    }

    /* If we're done with adding new leafs, then bail */
    if (csv->c_flags & CF_LEAFS_DONE)
	return -1;

    /* This leaf does not exist yet, so we need to create it */
    /* Start by checking if there's enough room */
    if (csv->c_leaf_depth + 1 >= csv->c_leaf_max) {
	/* Out of room; realloc it */
	ssize_t new_max = csv->c_leaf_max * 2;
	if (new_max == 0)
	    new_max = C_LEAF_MAX;

	lp = xo_realloc(csv->c_leaf, new_max * sizeof(*lp));
	if (lp == NULL)
	    return -1;			/* No luck; bail */

	/* Zero out the new portion */
	bzero(&lp[csv->c_leaf_max], csv->c_leaf_max * sizeof(*lp));

	/* Update csv data */
	csv->c_leaf = lp;
	csv->c_leaf_max = new_max;
    }

    lp = &csv->c_leaf[csv->c_leaf_depth++];
#ifdef CSV_STACK_IS_NEEDED
    lp->f_depth = csv->c_stack_depth;
#endif /* CSV_STACK_IS_NEEDED */

    lp->f_name = xo_buf_offset(xbp);

    char *cp = xo_buf_cur(xbp);
    xo_buf_append(xbp, name, strlen(name) + 1);

    if (flags & XFF_KEY)
	lp->f_flags |= LF_KEY;

    csv_dbg(xop, csv, "csv: leaf: name: %zd [%s] [%s] %x\n",
	    fnum, name, cp, lp->f_flags);

    return fnum;
}

/*
 * Record a new value for a leaf
 */
static void
csv_leaf_set (xo_handle_t *xop UNUSED, csv_private_t *csv, leaf_t *lp,
	       const char *value)
{
    xo_buffer_t *xbp = &csv->c_value_buf;

    lp->f_value = xo_buf_offset(xbp);
    lp->f_flags |= LF_HAS_VALUE;

    char *cp = xo_buf_cur(xbp);
    xo_buf_append(xbp, value, strlen(value) + 1);

    csv_dbg(xop, csv, "csv: leaf: value: [%s] [%s] %x\n",
	    value, cp, lp->f_flags);
}

/*
 * Record the requested set of leaf names.  The input should be a set
 * of leaf names, separated by periods.
 */
static int
csv_record_leafs (xo_handle_t *xop, csv_private_t *csv, const char *leafs_raw)
{
    char *cp, *ep, *np;
    ssize_t len = strlen(leafs_raw);
    char *leafs_buf = alloca(len + 1);

    memcpy(leafs_buf, leafs_raw, len + 1); /* Make local copy */

    for (cp = leafs_buf, ep = leafs_buf + len; cp && cp < ep; cp = np) {
	np = strchr(cp, '.');
	if (np)
	    *np++ = '\0';

	if (*cp == '\0')		/* Skip empty names */
	    continue;

	csv_dbg(xop, csv, "adding leaf: [%s]\n", cp);
	csv_leaf_num(xop, csv, cp, 0);
    }

    /*
     * Since we've been told explicitly what leafs matter, ignore the rest
     */
    csv->c_flags |= CF_LEAFS_DONE;

    return 0;
}

/*
 * Record the requested path elements.  The input should be a set of
 * container or instances names, separated by slashes.
 */
static int
csv_record_path (xo_handle_t *xop, csv_private_t *csv, const char *path_raw)
{
    int count;
    char *cp, *ep, *np;
    ssize_t len = strlen(path_raw);
    char *path_buf = xo_realloc(NULL, len + 1);

    memcpy(path_buf, path_raw, len + 1);

    for (cp = path_buf, ep = path_buf + len, count = 2;
	 cp && cp < ep; cp = np) {
	np = strchr(cp, '/');
	if (np) {
	    np += 1;
	    count += 1;
	}
    }

    path_frame_t *path = xo_realloc(NULL, sizeof(path[0]) * count);
    if (path == NULL) {
	xo_failure(xop, "allocation failure for path '%s'", path_buf);
	return -1;
    }

    bzero(path, sizeof(path[0]) * count);

    for (count = 0, cp = path_buf; cp && cp < ep; cp = np) {
	path[count++].pf_name = cp;

	np = strchr(cp, '/');
	if (np)
	    *np++ = '\0';
	csv_dbg(xop, csv, "path: [%s]\n", cp);
    }

    path[count].pf_name = NULL;

    if (csv->c_path)		     /* In case two paths are given */
	xo_free(csv->c_path);
    if (csv->c_path_buf)	     /* In case two paths are given */
	xo_free(csv->c_path_buf);

    csv->c_path_buf = path_buf;
    csv->c_path = path;
    csv->c_path_max = count;
    csv->c_path_cur = 0;

    return 0;
}

/*
 * Extract the option values.  The format is:
 *    -libxo encoder=csv:kw=val:kw=val:kw=val,pretty
 *    -libxo encoder=csv+kw=val+kw=val+kw=val,pretty
 */
static int
csv_options (xo_handle_t *xop, csv_private_t *csv,
	     const char *raw_opts, char opts_char)
{
    ssize_t len = strlen(raw_opts);
    char *options = alloca(len + 1);
    memcpy(options, raw_opts, len);
    options[len] = '\0';

    char *cp, *ep, *np, *vp;
    for (cp = options, ep = options + len + 1; cp && cp < ep; cp = np) {
	np = strchr(cp, opts_char);
	if (np)
	    *np++ = '\0';

	vp = strchr(cp, '=');
	if (vp)
	    *vp++ = '\0';

	if (xo_streq(cp, "path")) {
	    /* Record the path */
	    if (vp != NULL && csv_record_path(xop, csv, vp))
  		return -1;

	    csv->c_flags |= CF_HAS_PATH; /* Yup, we have an explicit path now */

	} else if (xo_streq(cp, "leafs")
		   || xo_streq(cp, "leaf")
		   || xo_streq(cp, "leaves")) {
	    /* Record the leafs */
	    if (vp != NULL && csv_record_leafs(xop, csv, vp))
  		return -1;

	} else if (xo_streq(cp, "no-keys")) {
	    csv->c_flags |= CF_NO_KEYS;
	} else if (xo_streq(cp, "no-header")) {
	    csv->c_flags |= CF_NO_HEADER;
	} else if (xo_streq(cp, "value-only")) {
	    csv->c_flags |= CF_VALUE_ONLY;
	} else if (xo_streq(cp, "dos")) {
	    csv->c_flags |= CF_DOS_NEWLINE;
	} else if (xo_streq(cp, "no-quotes")) {
	    csv->c_flags |= CF_NO_QUOTES;
	} else if (xo_streq(cp, "debug")) {
	    csv->c_flags |= CF_DEBUG;
	} else {
	    xo_warn_hc(xop, -1,
		       "unknown encoder option value: '%s'", cp);
	    return -1;
	}
    }

    return 0;
}

/*
 * Handler for incoming data values.  We just record each leaf name and
 * value.  The values are emittd when the instance is closed.
 */
static int
csv_data (xo_handle_t *xop UNUSED, csv_private_t *csv UNUSED,
	  const char *name, const char *value,
	  xo_xof_flags_t flags)
{
    csv_dbg(xop, csv, "data: [%s]=[%s] %llx\n", name, value, (unsigned long long) flags);

    if (!(csv->c_flags & CF_RECORD_DATA))
	return 0;

    /* Find the leaf number */
    int fnum = csv_leaf_num(xop, csv, name, flags);
    if (fnum < 0)
	return 0;			/* Don't bother recording */

    leaf_t *lp = &csv->c_leaf[fnum];
    csv_leaf_set(xop, csv, lp, value);

    return 0;
}

/*
 * The callback from libxo, passing us operations/events as they
 * happen.
 */
static int
csv_handler (XO_ENCODER_HANDLER_ARGS)
{
    int rc = 0;
    csv_private_t *csv = private;
    xo_buffer_t *xbp = csv ? &csv->c_data : NULL;

    csv_dbg(xop, csv, "op %s: [%s] [%s]\n",  xo_encoder_op_name(op),
	   name ?: "", value ?: "");
    fflush(stdout);

    /* If we don't have private data, we're sunk */
    if (csv == NULL && op != XO_OP_CREATE)
	return -1;

    switch (op) {
    case XO_OP_CREATE:		/* Called when the handle is init'd */
	rc = csv_create(xop);
	break;

    case XO_OP_OPTIONS:
	rc = csv_options(xop, csv, value, ':');
	break;

    case XO_OP_OPTIONS_PLUS:
	rc = csv_options(xop, csv, value, '+');
	break;

    case XO_OP_OPEN_LIST:
    case XO_OP_CLOSE_LIST:
	break;				/* Ignore these ops */

    case XO_OP_OPEN_CONTAINER:
    case XO_OP_OPEN_LEAF_LIST:
	rc = csv_open_level(xop, csv, name, 0);
	break;

    case XO_OP_OPEN_INSTANCE:
	rc = csv_open_level(xop, csv, name, 1);
	break;

    case XO_OP_CLOSE_CONTAINER:
    case XO_OP_CLOSE_LEAF_LIST:
    case XO_OP_CLOSE_INSTANCE:
	rc = csv_close_level(xop, csv, name);
	break;

    case XO_OP_STRING:		   /* Quoted UTF-8 string */
    case XO_OP_CONTENT:		   /* Other content */
	rc = csv_data(xop, csv, name, value, flags);
	break;

    case XO_OP_FINISH:		   /* Clean up function */
	break;

    case XO_OP_FLUSH:		   /* Clean up function */
	rc = write(1, xbp->xb_bufp, xbp->xb_curp - xbp->xb_bufp);
	if (rc > 0)
	    rc = 0;

	xo_buf_reset(xbp);
	break;

    case XO_OP_DESTROY:		   /* Clean up function */
	csv_destroy(xop, csv);
	break;

    case XO_OP_ATTRIBUTE:	   /* Attribute name/value */
	break;

    case XO_OP_VERSION:		/* Version string */
	break;
    }

    return rc;
}

/*
 * Callback when our encoder is loaded.
 */
int
xo_encoder_library_init (XO_ENCODER_INIT_ARGS)
{
    arg->xei_handler = csv_handler;
    arg->xei_version = XO_ENCODER_VERSION;

    return 0;
}
