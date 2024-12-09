#include "less.h"
#include "xbuf.h"

/*
 * Initialize an expandable text buffer.
 */
public void xbuf_init(struct xbuffer *xbuf)
{
	xbuf_init_size(xbuf, 16);
}

public void xbuf_init_size(struct xbuffer *xbuf, size_t init_size)
{
	xbuf->data = NULL;
	xbuf->size = xbuf->end = 0;
	xbuf->init_size = init_size;
}

/*
 * Free buffer space in an xbuf.
 */
public void xbuf_deinit(struct xbuffer *xbuf)
{
	if (xbuf->data != NULL)
		free(xbuf->data);
	xbuf_init(xbuf);
}

/*
 * Set xbuf to empty.
 */
public void xbuf_reset(struct xbuffer *xbuf)
{
	xbuf->end = 0;
}

/*
 * Add a byte to an xbuf.
 */
public void xbuf_add_byte(struct xbuffer *xbuf, unsigned char b)
{
	if (xbuf->end >= xbuf->size)
	{
		unsigned char *data;
		if (ckd_add(&xbuf->size, xbuf->size, xbuf->size ? xbuf->size : xbuf->init_size))
			out_of_memory();
		data = (unsigned char *) ecalloc(xbuf->size, sizeof(unsigned char));
		if (xbuf->data != NULL)
		{
			memcpy(data, xbuf->data, xbuf->end);
			free(xbuf->data);
		}
		xbuf->data = data;
	}
	xbuf->data[xbuf->end++] = b;
}

/*
 * Add a char to an xbuf.
 */
public void xbuf_add_char(struct xbuffer *xbuf, char c)
{
	xbuf_add_byte(xbuf, (unsigned char) c);
}

/*
 * Add arbitrary data to an xbuf.
 */
public void xbuf_add_data(struct xbuffer *xbuf, constant unsigned char *data, size_t len)
{
	size_t i;
	for (i = 0;  i < len;  i++)
		xbuf_add_byte(xbuf, data[i]);
}

/*
 * Remove the last byte from an xbuf.
 */
public int xbuf_pop(struct xbuffer *buf)
{
	if (buf->end == 0)
		return -1;
	return (int) buf->data[--(buf->end)];
}

/*
 * Set an xbuf to the contents of another xbuf.
 */
public void xbuf_set(struct xbuffer *dst, struct xbuffer *src)
{
	xbuf_reset(dst);
	xbuf_add_data(dst, src->data, src->end);
}

/*
 * Return xbuf data as a char*.
 */
public constant char * xbuf_char_data(constant struct xbuffer *xbuf)
{
	return (constant char *)(xbuf->data);
}


/*
 * Helper functions for the ckd_add and ckd_mul macro substitutes.
 * These helper functions do not set *R on overflow, and assume that
 * arguments are nonnegative, that INTMAX_MAX <= UINTMAX_MAX, and that
 * sizeof is a reliable way to distinguish integer representations.
 * Despite these limitations they are good enough for 'less' on all
 * known practical platforms.  For more-complicated substitutes
 * without most of these limitations, see Gnulib's stdckdint module.
 */
#if !HAVE_STDCKDINT_H
/*
 * If the integer *R can represent VAL, store the value and return FALSE.
 * Otherwise, possibly set *R to an indeterminate value and return TRUE.
 * R has size RSIZE, and is signed if and only if RSIGNED is nonzero.
 */
static lbool help_fixup(void *r, uintmax val, int rsize, int rsigned)
{
	if (rsigned)
	{
		if (rsize == sizeof (int))
		{
			int *pr = r;
			if (INT_MAX < val)
				return TRUE;
			*pr = (int) val;
#ifdef LLONG_MAX
		} else if (rsize == sizeof (long long))
		{
			long long *pr = r;
			if (LLONG_MAX < val)
				return TRUE;
			*pr = (long long) val;
#endif
#ifdef INTMAX_MAX
		} else if (rsize == sizeof (intmax_t)) {
			intmax_t *pr = r;
			if (INTMAX_MAX < val)
				return TRUE;
			*pr = (intmax_t) val;
#endif
		} else /* rsize == sizeof (long) */
		{
			long *pr = r;
			if (LONG_MAX < val)
				return TRUE;
			*pr = (long) val;
		}
	} else {
		if (rsize == sizeof (unsigned)) {
			unsigned *pr = r;
			if (UINT_MAX < val)
				return TRUE;
			*pr = (unsigned) val;
		} else if (rsize == sizeof (unsigned long)) {
			unsigned long *pr = r;
			if (ULONG_MAX < val)
				return TRUE;
			*pr = (unsigned long) val;
#ifdef ULLONG_MAX
		} else if (rsize == sizeof (unsigned long long)) {
			unsigned long long *pr = r;
			if (ULLONG_MAX < val)
				return TRUE;
			*pr = (unsigned long long) val;
#endif
		} else /* rsize == sizeof (uintmax) */
		{
			uintmax *pr = r;
			*pr = (uintmax) val;
		}
	}
	return FALSE;
}

/*
 * If *R can represent the mathematical sum of A and B, store the sum
 * and return FALSE.  Otherwise, possibly set *R to an indeterminate
 * value and return TRUE.  R has size RSIZE, and is signed if and only
 * if RSIGNED is nonzero.
 */
public lbool help_ckd_add(void *r, uintmax a, uintmax b, int rsize, int rsigned)
{
	uintmax sum = a + b;
	return sum < a || help_fixup(r, sum, rsize, rsigned);
}

/* Likewise, but for the product of A and B.  */
public lbool help_ckd_mul(void *r, uintmax a, uintmax b, int rsize, int rsigned)
{
	uintmax product = a * b;
	return ((b != 0 && a != product / b)
		|| help_fixup(r, product, rsize, rsigned));
}
#endif
