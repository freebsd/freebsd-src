#ifndef XBUF_H_
#define XBUF_H_

struct xbuffer
{
	char *data;
	int end;
	int size;
};

void xbuf_init(struct xbuffer *xbuf);
void xbuf_reset(struct xbuffer *xbuf);
void xbuf_add(struct xbuffer *xbuf, char ch);

#endif
