/*
Copyright (c) 2003-2006 Hewlett-Packard Development Company, L.P.
Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/

#include "uwx_env.h"
#include "uwx_bstream.h"


/* uwx_init_bstream: initialize a byte stream for reading */

void uwx_init_bstream(
    struct uwx_bstream *bstream,
    struct uwx_env *env,
    uint64_t source,
    unsigned int len,
    int request)
{
    bstream->buf = 0;
    if (env->remote) {
	bstream->source = source;
	bstream->bufp = (unsigned char *) &bstream->buf;
	bstream->nbuf = 0;
	bstream->copyin = env->copyin;
	bstream->cb_token = env->cb_token;
	bstream->request = request;
    }
    else {
	bstream->source = 0;
	bstream->bufp = (unsigned char *) (intptr_t) source;
	bstream->nbuf = len;
	bstream->copyin = 0;
	bstream->cb_token = 0;
	bstream->request = 0;
    }
    bstream->ntotal = len;
    bstream->peekc = -1;
}


/* uwx_get_byte: read the next byte from the byte stream */

int uwx_get_byte(struct uwx_bstream *bstream)
{
    int len;
    int n;
    int b;

    if (bstream->peekc >= 0) {
	b = bstream->peekc;
	bstream->peekc = -1;
	return b;
    }
    if (bstream->ntotal <= 0)
	return -1;
    if (bstream->nbuf <= 0) {
	if (bstream->source & 0x7 || bstream->ntotal < sizeof(uint64_t))
	    len = sizeof(uint32_t);
	else
	    len = sizeof(uint64_t);
	n = (*bstream->copyin)(bstream->request, (char *)&bstream->buf,
		bstream->source, len, bstream->cb_token);
	if (n != len)
	    return -1;
	bstream->bufp = (unsigned char *) &bstream->buf;
	bstream->nbuf = n;
	bstream->source += n;
    }

    b = *bstream->bufp++;
    bstream->nbuf--;
    bstream->ntotal--;
    return b;
}


/* uwx_unget_byte: push a byte back onto the byte stream */

int uwx_unget_byte(struct uwx_bstream *bstream, int b)
{
    bstream->peekc = b;
    return 0;
}


/* uwx_get_uleb128: read a ULEB128 value from the byte stream */

int uwx_get_uleb128(struct uwx_bstream *bstream, uint64_t *valp)
{
    uint64_t val;
    int i;
    int b;

    b = uwx_get_byte(bstream);
    val = (uint64_t)(b & 0x7f) << 56;
    for (i = 0; i < 8; i++) {
	val = val >> 7;
	if (b & 0x80) {
	    b = uwx_get_byte(bstream);
	    val |= (uint64_t)(b & 0x7f) << 56;
	}
    }
    if (b & 0x80) {
	b = uwx_get_byte(bstream);
	val |= (uint64_t)(b & 0x7f) << 63;
    }
    if (b & 0x80)
	return -1;
    *valp = val;
    return 0;
}

#if 0
int uwx_get_uleb128_alt(struct uwx_bstream *bstream, uint64_t *valp)
{
    uint64_t val;
    int b;

    b = uwx_get_byte(bstream);
    val = b & 0x7f;
    if (b & 0x80) {
	b = uwx_get_byte(bstream);
	val |= (uint64_t)(b & 0x7f) << 7;
	if (b & 0x80) {
	    b = uwx_get_byte(bstream);
	    val |= (uint64_t)(b & 0x7f) << 14;
	    if (b & 0x80) {
		b = uwx_get_byte(bstream);
		val |= (uint64_t)(b & 0x7f) << 21;
		if (b & 0x80) {
		    b = uwx_get_byte(bstream);
		    val |= (uint64_t)(b & 0x7f) << 28;
		    if (b & 0x80) {
			b = uwx_get_byte(bstream);
			val |= (uint64_t)(b & 0x7f) << 35;
			if (b & 0x80) {
			    b = uwx_get_byte(bstream);
			    val |= (uint64_t)(b & 0x7f) << 42;
			    if (b & 0x80) {
				b = uwx_get_byte(bstream);
				val |= (uint64_t)(b & 0x7f) << 49;
				if (b & 0x80) {
				    b = uwx_get_byte(bstream);
				    val |= (uint64_t)(b & 0x7f) << 56;
				    if (b & 0x80) {
					b = uwx_get_byte(bstream);
					val |= (uint64_t)(b & 0x7f) << 63;
					if (b & 0x80)
					    return -1;
				    }
				}
			    }
			}
		    }
		}
	    }
	}
    }
    *valp = val;
    return 0;
}
#endif
