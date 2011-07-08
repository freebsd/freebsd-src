/*-
 * Copyright (c) 2009 Isilon Inc http://www.isilon.com/
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
 */
/**
 * @file
 *
 * fail(9) Facility.
 *
 * @ingroup failpoint_private
 */
/**
 * @defgroup failpoint fail(9) Facility
 *
 * Failpoints allow for injecting fake errors into running code on the fly,
 * without modifying code or recompiling with flags.  Failpoints are always
 * present, and are very efficient when disabled.  Failpoints are described
 * in man fail(9).
 */
/**
 * @defgroup failpoint_private Private fail(9) Implementation functions
 *
 * Private implementations for the actual failpoint code.
 *
 * @ingroup failpoint
 */
/**
 * @addtogroup failpoint_private
 * @{
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/ctype.h>
#include <sys/errno.h>
#include <sys/fail.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/sbuf.h>

#include <machine/stdarg.h>

#ifdef ILOG_DEFINE_FOR_FILE
ILOG_DEFINE_FOR_FILE(L_ISI_FAIL_POINT, L_ILOG, fail_point);
#endif

MALLOC_DEFINE(M_FAIL_POINT, "Fail Points", "fail points system");
#define fp_free(ptr) free(ptr, M_FAIL_POINT)
#define fp_malloc(size, flags) malloc((size), M_FAIL_POINT, (flags))

static struct mtx g_fp_mtx;
MTX_SYSINIT(g_fp_mtx, &g_fp_mtx, "fail point mtx", MTX_DEF);
#define FP_LOCK()	mtx_lock(&g_fp_mtx)
#define FP_UNLOCK()	mtx_unlock(&g_fp_mtx)

/**
 * Failpoint types.
 * Don't change these without changing fail_type_strings in fail.c.
 * @ingroup failpoint_private
 */
enum fail_point_t {
	FAIL_POINT_OFF,		/**< don't fail */
	FAIL_POINT_PANIC,	/**< panic */
	FAIL_POINT_RETURN,	/**< return an errorcode */
	FAIL_POINT_BREAK,	/**< break into the debugger */
	FAIL_POINT_PRINT,	/**< print a message */
	FAIL_POINT_SLEEP,	/**< sleep for some msecs */
	FAIL_POINT_NUMTYPES
};

static struct {
	const char *name;
	int	nmlen;
} fail_type_strings[] = {
#define	FP_TYPE_NM_LEN(s)	{ s, sizeof(s) - 1 }
	[FAIL_POINT_OFF] =	FP_TYPE_NM_LEN("off"),
	[FAIL_POINT_PANIC] =	FP_TYPE_NM_LEN("panic"),
	[FAIL_POINT_RETURN] =	FP_TYPE_NM_LEN("return"),
	[FAIL_POINT_BREAK] =	FP_TYPE_NM_LEN("break"),
	[FAIL_POINT_PRINT] =	FP_TYPE_NM_LEN("print"),
	[FAIL_POINT_SLEEP] =	FP_TYPE_NM_LEN("sleep"),
};

/**
 * Internal structure tracking a single term of a complete failpoint.
 * @ingroup failpoint_private
 */
struct fail_point_entry {
	enum fail_point_t fe_type;	/**< type of entry */
	int		fe_arg;		/**< argument to type (e.g. return value) */
	int		fe_prob;	/**< likelihood of firing in millionths */
	int		fe_count;	/**< number of times to fire, 0 means always */

	TAILQ_ENTRY(fail_point_entry) fe_entries; /**< next entry in fail point */
};

static inline void
fail_point_sleep(struct fail_point *fp, struct fail_point_entry *ent,
    int msecs, enum fail_point_return_code *pret)
{
	/* convert from millisecs to ticks, rounding up */
	int timo = ((msecs * hz) + 999) / 1000;

	if (timo > 0) {
		if (fp->fp_sleep_fn == NULL) {
			msleep(fp, &g_fp_mtx, PWAIT, "failpt", timo);
		} else {
			timeout(fp->fp_sleep_fn, fp->fp_sleep_arg, timo);
			*pret = FAIL_POINT_RC_QUEUED;
		}
	}
}


/**
 * Defines stating the equivalent of probablilty one (100%)
 */
enum {
	PROB_MAX = 1000000,	/* probability between zero and this number */
	PROB_DIGITS = 6,        /* number of zero's in above number */
};

static char *parse_fail_point(struct fail_point_entries *, char *);
static char *parse_term(struct fail_point_entries *, char *);
static char *parse_number(int *out_units, int *out_decimal, char *);
static char *parse_type(struct fail_point_entry *, char *);
static void free_entry(struct fail_point_entries *, struct fail_point_entry *);
static void clear_entries(struct fail_point_entries *);

/**
 * Initialize a fail_point.  The name is formed in a printf-like fashion
 * from "fmt" and subsequent arguments.  This function is generally used
 * for custom failpoints located at odd places in the sysctl tree, and is
 * not explicitly needed for standard in-line-declared failpoints.
 *
 * @ingroup failpoint
 */
void
fail_point_init(struct fail_point *fp, const char *fmt, ...)
{
	va_list ap;
	char *name;
	int n;

	TAILQ_INIT(&fp->fp_entries);
	fp->fp_flags = 0;

	/* Figure out the size of the name. */
	va_start(ap, fmt);
	n = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);

	/* Allocate the name and fill it in. */
	name = fp_malloc(n + 1, M_WAITOK);
	if (name != NULL) {
		va_start(ap, fmt);
		vsnprintf(name, n + 1, fmt, ap);
		va_end(ap);
	}
	fp->fp_name = name;
	fp->fp_location = "";
	fp->fp_flags |= FAIL_POINT_DYNAMIC_NAME;
	fp->fp_sleep_fn = NULL;
	fp->fp_sleep_arg = NULL;
}

/**
 * Free the resources held by a fail_point.
 *
 * @ingroup failpoint
 */
void
fail_point_destroy(struct fail_point *fp)
{

	if ((fp->fp_flags & FAIL_POINT_DYNAMIC_NAME) != 0) {
		fp_free(__DECONST(void *, fp->fp_name));
		fp->fp_name = NULL;
	}
	fp->fp_flags = 0;
	clear_entries(&fp->fp_entries);
}

/**
 * This does the real work of evaluating a fail point. If the fail point tells
 * us to return a value, this function returns 1 and fills in 'return_value'
 * (return_value is allowed to be null). If the fail point tells us to panic,
 * we never return. Otherwise we just return 0 after doing some work, which
 * means "keep going".
 */
enum fail_point_return_code
fail_point_eval_nontrivial(struct fail_point *fp, int *return_value)
{
	enum fail_point_return_code ret = FAIL_POINT_RC_CONTINUE;
	struct fail_point_entry *ent, *next;
	int msecs;

	FP_LOCK();

	TAILQ_FOREACH_SAFE(ent, &fp->fp_entries, fe_entries, next) {
		int cont = 0; /* don't continue by default */

		if (ent->fe_prob < PROB_MAX &&
		    ent->fe_prob < random() % PROB_MAX)
			continue;

		switch (ent->fe_type) {
		case FAIL_POINT_PANIC:
			panic("fail point %s panicking", fp->fp_name);
			/* NOTREACHED */

		case FAIL_POINT_RETURN:
			if (return_value != NULL)
				*return_value = ent->fe_arg;
			ret = FAIL_POINT_RC_RETURN;
			break;

		case FAIL_POINT_BREAK:
			printf("fail point %s breaking to debugger\n",
			    fp->fp_name);
			breakpoint();
			break;

		case FAIL_POINT_PRINT:
			printf("fail point %s executing\n", fp->fp_name);
			cont = ent->fe_arg;
			break;

		case FAIL_POINT_SLEEP:
			/*
			 * Free the entry now if necessary, since
			 * we're about to drop the mutex and sleep.
			 */
			msecs = ent->fe_arg;
			if (ent->fe_count > 0 && --ent->fe_count == 0) {
				free_entry(&fp->fp_entries, ent);
				ent = NULL;
			}

			if (msecs)
				fail_point_sleep(fp, ent, msecs, &ret);
			break;

		default:
			break;
		}

		if (ent != NULL && ent->fe_count > 0 && --ent->fe_count == 0)
			free_entry(&fp->fp_entries, ent);
		if (cont == 0)
			break;
	}

	/* Get rid of "off"s at the end. */
	while ((ent = TAILQ_LAST(&fp->fp_entries, fail_point_entries)) &&
	       ent->fe_type == FAIL_POINT_OFF)
		free_entry(&fp->fp_entries, ent);

	FP_UNLOCK();

	return (ret);
}

/**
 * Translate internal fail_point structure into human-readable text.
 */
static void
fail_point_get(struct fail_point *fp, struct sbuf *sb)
{
	struct fail_point_entry *ent;

	FP_LOCK();

	TAILQ_FOREACH(ent, &fp->fp_entries, fe_entries) {
		if (ent->fe_prob < PROB_MAX) {
			int decimal = ent->fe_prob % (PROB_MAX / 100);
			int units = ent->fe_prob / (PROB_MAX / 100);
			sbuf_printf(sb, "%d", units);
			if (decimal) {
				int digits = PROB_DIGITS - 2;
				while (!(decimal % 10)) {
					digits--;
					decimal /= 10;
				}
				sbuf_printf(sb, ".%0*d", digits, decimal);
			}
			sbuf_printf(sb, "%%");
		}
		if (ent->fe_count > 0)
			sbuf_printf(sb, "%d*", ent->fe_count);
		sbuf_printf(sb, "%s", fail_type_strings[ent->fe_type].name);
		if (ent->fe_arg)
			sbuf_printf(sb, "(%d)", ent->fe_arg);
		if (TAILQ_NEXT(ent, fe_entries))
			sbuf_printf(sb, "->");
	}
	if (TAILQ_EMPTY(&fp->fp_entries))
		sbuf_printf(sb, "off");

	FP_UNLOCK();
}

/**
 * Set an internal fail_point structure from a human-readable failpoint string
 * in a lock-safe manner.
 */
static int
fail_point_set(struct fail_point *fp, char *buf)
{
	int error = 0;
	struct fail_point_entry *ent, *ent_next;
	struct fail_point_entries new_entries;

	/* Parse new entries. */
	TAILQ_INIT(&new_entries);
	if (!parse_fail_point(&new_entries, buf)) {
	        clear_entries(&new_entries);
		error = EINVAL;
		goto end;
	}

	FP_LOCK();

	/* Move new entries in. */
	TAILQ_SWAP(&fp->fp_entries, &new_entries, fail_point_entry, fe_entries);
	clear_entries(&new_entries);

	/* Get rid of useless zero probability entries. */
	TAILQ_FOREACH_SAFE(ent, &fp->fp_entries, fe_entries, ent_next) {
		if (ent->fe_prob == 0)
			free_entry(&fp->fp_entries, ent);
	}

	/* Get rid of "off"s at the end. */
	while ((ent = TAILQ_LAST(&fp->fp_entries, fail_point_entries)) &&
		ent->fe_type == FAIL_POINT_OFF)
		free_entry(&fp->fp_entries, ent);

	FP_UNLOCK();

 end:
#ifdef IWARNING
	if (error)
		IWARNING("Failed to set %s %s to %s",
		    fp->fp_name, fp->fp_location, buf);
	else
		INOTICE("Set %s %s to %s",
		    fp->fp_name, fp->fp_location, buf);
#endif /* IWARNING */

	return (error);
}

#define MAX_FAIL_POINT_BUF	1023

/**
 * Handle kernel failpoint set/get.
 */
int
fail_point_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct fail_point *fp = arg1;
	char *buf = NULL;
	struct sbuf sb;
	int error;

	/* Retrieving */
	sbuf_new(&sb, NULL, 128, SBUF_AUTOEXTEND);
	fail_point_get(fp, &sb);
	sbuf_trim(&sb);
	sbuf_finish(&sb);
	error = SYSCTL_OUT(req, sbuf_data(&sb), sbuf_len(&sb));
	sbuf_delete(&sb);

	/* Setting */
	if (!error && req->newptr) {
		if (req->newlen > MAX_FAIL_POINT_BUF) {
			error = EINVAL;
			goto out;
		}

		buf = fp_malloc(req->newlen + 1, M_WAITOK);

		error = SYSCTL_IN(req, buf, req->newlen);
		if (error)
			goto out;
		buf[req->newlen] = '\0';

		error = fail_point_set(fp, buf);
        }

out:
	fp_free(buf);
	return (error);
}

/**
 * Internal helper function to translate a human-readable failpoint string
 * into a internally-parsable fail_point structure.
 */
static char *
parse_fail_point(struct fail_point_entries *ents, char *p)
{
	/*  <fail_point> ::
	 *      <term> ( "->" <term> )*
	 */
	p = parse_term(ents, p);
	if (p == NULL)
		return (NULL);
	while (*p != '\0') {
		if (p[0] != '-' || p[1] != '>')
			return (NULL);
		p = parse_term(ents, p + 2);
		if (p == NULL)
			return (NULL);
	}
	return (p);
}

/**
 * Internal helper function to parse an individual term from a failpoint.
 */
static char *
parse_term(struct fail_point_entries *ents, char *p)
{
	struct fail_point_entry *ent;

	ent = fp_malloc(sizeof *ent, M_WAITOK | M_ZERO);
	ent->fe_prob = PROB_MAX;
	TAILQ_INSERT_TAIL(ents, ent, fe_entries);

	/*
	 * <term> ::
	 *     ( (<float> "%") | (<integer> "*" ) )*
	 *     <type>
	 *     [ "(" <integer> ")" ]
	 */

	/* ( (<float> "%") | (<integer> "*" ) )* */
	while (isdigit(*p) || *p == '.') {
		int units, decimal;

		p = parse_number(&units, &decimal, p);
		if (p == NULL)
			return (NULL);

		if (*p == '%') {
			if (units > 100) /* prevent overflow early */
				units = 100;
			ent->fe_prob = units * (PROB_MAX / 100) + decimal;
			if (ent->fe_prob > PROB_MAX)
				ent->fe_prob = PROB_MAX;
		} else if (*p == '*') {
			if (!units || decimal)
				return (NULL);
			ent->fe_count = units;
		} else
			return (NULL);
		p++;
	}

	/* <type> */
	p = parse_type(ent, p);
	if (p == NULL)
		return (NULL);
	if (*p == '\0')
		return (p);

	/* [ "(" <integer> ")" ] */
	if (*p != '(')
		return p;
	p++;
	if (!isdigit(*p) && *p != '-')
		return (NULL);
	ent->fe_arg = strtol(p, &p, 0);
	if (*p++ != ')')
		return (NULL);

	return (p);
}

/**
 * Internal helper function to parse a numeric for a failpoint term.
 */
static char *
parse_number(int *out_units, int *out_decimal, char *p)
{
	char *old_p;

	/*
	 *  <number> ::
	 *      <integer> [ "." <integer> ] |
	 *      "." <integer>
	 */

	/* whole part */
	old_p = p;
	*out_units = strtol(p, &p, 10);
	if (p == old_p && *p != '.')
		return (NULL);

	/* fractional part */
	*out_decimal = 0;
	if (*p == '.') {
		int digits = 0;
		p++;
		while (isdigit(*p)) {
			int digit = *p - '0';
			if (digits < PROB_DIGITS - 2)
				*out_decimal = *out_decimal * 10 + digit;
			else if (digits == PROB_DIGITS - 2 && digit >= 5)
				(*out_decimal)++;
			digits++;
			p++;
		}
		if (!digits) /* need at least one digit after '.' */
			return (NULL);
		while (digits++ < PROB_DIGITS - 2) /* add implicit zeros */
			*out_decimal *= 10;
	}

	return (p); /* success */
}

/**
 * Internal helper function to parse an individual type for a failpoint term.
 */
static char *
parse_type(struct fail_point_entry *ent, char *beg)
{
	enum fail_point_t type;
	int len;

	for (type = FAIL_POINT_OFF; type < FAIL_POINT_NUMTYPES; type++) {
		len = fail_type_strings[type].nmlen;
		if (strncmp(fail_type_strings[type].name, beg, len) == 0) {
			ent->fe_type = type;
			return (beg + len);
		}
	}
	return (NULL);
}

/**
 * Internal helper function to free an individual failpoint term.
 */
static void
free_entry(struct fail_point_entries *ents, struct fail_point_entry *ent)
{
	TAILQ_REMOVE(ents, ent, fe_entries);
	fp_free(ent);
}

/**
 * Internal helper function to clear out all failpoint terms for a single
 * failpoint.
 */
static void
clear_entries(struct fail_point_entries *ents)
{
	struct fail_point_entry *ent, *ent_next;

	TAILQ_FOREACH_SAFE(ent, ents, fe_entries, ent_next)
		fp_free(ent);
	TAILQ_INIT(ents);
}

/* The fail point sysctl tree. */
SYSCTL_NODE(_debug, OID_AUTO, fail_point, CTLFLAG_RW, 0, "fail points");
