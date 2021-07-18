#include "less.h"
#include "xbuf.h"

/*
 * Initialize an expandable text buffer.
 */
	public void
xbuf_init(xbuf)
	struct xbuffer *xbuf;
{
	xbuf->data = NULL;
	xbuf->size = xbuf->end = 0;
}

	public void
xbuf_deinit(xbuf)
	struct xbuffer *xbuf;
{
	if (xbuf->data != NULL)
		free(xbuf->data);
	xbuf_init(xbuf);
}

	public void
xbuf_reset(xbuf)
	struct xbuffer *xbuf;
{
	xbuf->end = 0;
}

/*
 * Add a char to an expandable text buffer.
 */
	public void
xbuf_add(xbuf, ch)
	struct xbuffer *xbuf;
	char ch;
{
	if (xbuf->end >= xbuf->size)
	{
		char *data;
		xbuf->size = (xbuf->size == 0) ? 16 : xbuf->size * 2;
		data = (char *) ecalloc(xbuf->size, sizeof(char));
		if (xbuf->data != NULL)
		{
			memcpy(data, xbuf->data, xbuf->end);
			free(xbuf->data);
		}
		xbuf->data = data;
	}
	xbuf->data[xbuf->end++] = ch;
}
