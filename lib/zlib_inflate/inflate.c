/* inflate.c -- zlib interface to inflate modules
 * Copyright (C) 1995-1998 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h 
 */

#include <linux/module.h>
#include <linux/zutil.h>
#include "infblock.h"
#include "infutil.h"

int ZEXPORT zlib_inflate_workspacesize(void)
{
  return sizeof(struct inflate_workspace);
}


int ZEXPORT zlib_inflateReset(z)
z_streamp z;
{
  if (z == Z_NULL || z->state == Z_NULL || z->workspace == Z_NULL)
    return Z_STREAM_ERROR;
  z->total_in = z->total_out = 0;
  z->msg = Z_NULL;
  z->state->mode = z->state->nowrap ? BLOCKS : METHOD;
  zlib_inflate_blocks_reset(z->state->blocks, z, Z_NULL);
  return Z_OK;
}


int ZEXPORT zlib_inflateEnd(z)
z_streamp z;
{
  if (z == Z_NULL || z->state == Z_NULL || z->workspace == Z_NULL)
    return Z_STREAM_ERROR;
  if (z->state->blocks != Z_NULL)
    zlib_inflate_blocks_free(z->state->blocks, z);
  z->state = Z_NULL;
  return Z_OK;
}


int ZEXPORT zlib_inflateInit2_(z, w, version, stream_size)
z_streamp z;
int w;
const char *version;
int stream_size;
{
  if (version == Z_NULL || version[0] != ZLIB_VERSION[0] ||
      stream_size != sizeof(z_stream) || z->workspace == Z_NULL)
      return Z_VERSION_ERROR;

  /* initialize state */
  if (z == Z_NULL)
    return Z_STREAM_ERROR;
  z->msg = Z_NULL;
  z->state = &WS(z)->internal_state;
  z->state->blocks = Z_NULL;

  /* handle undocumented nowrap option (no zlib header or check) */
  z->state->nowrap = 0;
  if (w < 0)
  {
    w = - w;
    z->state->nowrap = 1;
  }

  /* set window size */
  if (w < 8 || w > 15)
  {
    zlib_inflateEnd(z);
    return Z_STREAM_ERROR;
  }
  z->state->wbits = (uInt)w;

  /* create inflate_blocks state */
  if ((z->state->blocks =
      zlib_inflate_blocks_new(z, z->state->nowrap ? Z_NULL : zlib_adler32, (uInt)1 << w))
      == Z_NULL)
  {
    zlib_inflateEnd(z);
    return Z_MEM_ERROR;
  }

  /* reset state */
  zlib_inflateReset(z);
  return Z_OK;
}


/*
 * At the end of a Deflate-compressed PPP packet, we expect to have seen
 * a `stored' block type value but not the (zero) length bytes.
 */
static int zlib_inflate_packet_flush(inflate_blocks_statef *s)
{
    if (s->mode != LENS)
	return Z_DATA_ERROR;
    s->mode = TYPE;
    return Z_OK;
}


int ZEXPORT zlib_inflateInit_(z, version, stream_size)
z_streamp z;
const char *version;
int stream_size;
{
  return zlib_inflateInit2_(z, DEF_WBITS, version, stream_size);
}

#undef NEEDBYTE
#undef NEXTBYTE
#define NEEDBYTE {if(z->avail_in==0)goto empty;r=trv;}
#define NEXTBYTE (z->avail_in--,z->total_in++,*z->next_in++)

int ZEXPORT zlib_inflate(z, f)
z_streamp z;
int f;
{
  int r, trv;
  uInt b;

  if (z == Z_NULL || z->state == Z_NULL || z->next_in == Z_NULL)
    return Z_STREAM_ERROR;
  trv = f == Z_FINISH ? Z_BUF_ERROR : Z_OK;
  r = Z_BUF_ERROR;
  while (1) switch (z->state->mode)
  {
    case METHOD:
      NEEDBYTE
      if (((z->state->sub.method = NEXTBYTE) & 0xf) != Z_DEFLATED)
      {
        z->state->mode = I_BAD;
        z->msg = (char*)"unknown compression method";
        z->state->sub.marker = 5;       /* can't try inflateSync */
        break;
      }
      if ((z->state->sub.method >> 4) + 8 > z->state->wbits)
      {
        z->state->mode = I_BAD;
        z->msg = (char*)"invalid window size";
        z->state->sub.marker = 5;       /* can't try inflateSync */
        break;
      }
      z->state->mode = FLAG;
    case FLAG:
      NEEDBYTE
      b = NEXTBYTE;
      if (((z->state->sub.method << 8) + b) % 31)
      {
        z->state->mode = I_BAD;
        z->msg = (char*)"incorrect header check";
        z->state->sub.marker = 5;       /* can't try inflateSync */
        break;
      }
      if (!(b & PRESET_DICT))
      {
        z->state->mode = BLOCKS;
        break;
      }
      z->state->mode = DICT4;
    case DICT4:
      NEEDBYTE
      z->state->sub.check.need = (uLong)NEXTBYTE << 24;
      z->state->mode = DICT3;
    case DICT3:
      NEEDBYTE
      z->state->sub.check.need += (uLong)NEXTBYTE << 16;
      z->state->mode = DICT2;
    case DICT2:
      NEEDBYTE
      z->state->sub.check.need += (uLong)NEXTBYTE << 8;
      z->state->mode = DICT1;
    case DICT1:
      NEEDBYTE
      z->state->sub.check.need += (uLong)NEXTBYTE;
      z->adler = z->state->sub.check.need;
      z->state->mode = DICT0;
      return Z_NEED_DICT;
    case DICT0:
      z->state->mode = I_BAD;
      z->msg = (char*)"need dictionary";
      z->state->sub.marker = 0;       /* can try inflateSync */
      return Z_STREAM_ERROR;
    case BLOCKS:
      r = zlib_inflate_blocks(z->state->blocks, z, r);
      if (f == Z_PACKET_FLUSH && z->avail_in == 0 && z->avail_out != 0)
	  r = zlib_inflate_packet_flush(z->state->blocks);
      if (r == Z_DATA_ERROR)
      {
        z->state->mode = I_BAD;
        z->state->sub.marker = 0;       /* can try inflateSync */
        break;
      }
      if (r == Z_OK)
        r = trv;
      if (r != Z_STREAM_END)
        return r;
      r = trv;
      zlib_inflate_blocks_reset(z->state->blocks, z, &z->state->sub.check.was);
      if (z->state->nowrap)
      {
        z->state->mode = I_DONE;
        break;
      }
      z->state->mode = CHECK4;
    case CHECK4:
      NEEDBYTE
      z->state->sub.check.need = (uLong)NEXTBYTE << 24;
      z->state->mode = CHECK3;
    case CHECK3:
      NEEDBYTE
      z->state->sub.check.need += (uLong)NEXTBYTE << 16;
      z->state->mode = CHECK2;
    case CHECK2:
      NEEDBYTE
      z->state->sub.check.need += (uLong)NEXTBYTE << 8;
      z->state->mode = CHECK1;
    case CHECK1:
      NEEDBYTE
      z->state->sub.check.need += (uLong)NEXTBYTE;

      if (z->state->sub.check.was != z->state->sub.check.need)
      {
        z->state->mode = I_BAD;
        z->msg = (char*)"incorrect data check";
        z->state->sub.marker = 5;       /* can't try inflateSync */
        break;
      }
      z->state->mode = I_DONE;
    case I_DONE:
      return Z_STREAM_END;
    case I_BAD:
      return Z_DATA_ERROR;
    default:
      return Z_STREAM_ERROR;
  }
 empty:
  if (f != Z_PACKET_FLUSH)
    return r;
  z->state->mode = I_BAD;
  z->msg = (char *)"need more for packet flush";
  z->state->sub.marker = 0;       /* can try inflateSync */
  return Z_DATA_ERROR;
}


int ZEXPORT zlib_inflateSync(z)
z_streamp z;
{
  uInt n;       /* number of bytes to look at */
  Bytef *p;     /* pointer to bytes */
  uInt m;       /* number of marker bytes found in a row */
  uLong r, w;   /* temporaries to save total_in and total_out */

  /* set up */
  if (z == Z_NULL || z->state == Z_NULL)
    return Z_STREAM_ERROR;
  if (z->state->mode != I_BAD)
  {
    z->state->mode = I_BAD;
    z->state->sub.marker = 0;
  }
  if ((n = z->avail_in) == 0)
    return Z_BUF_ERROR;
  p = z->next_in;
  m = z->state->sub.marker;

  /* search */
  while (n && m < 4)
  {
    static const Byte mark[4] = {0, 0, 0xff, 0xff};
    if (*p == mark[m])
      m++;
    else if (*p)
      m = 0;
    else
      m = 4 - m;
    p++, n--;
  }

  /* restore */
  z->total_in += p - z->next_in;
  z->next_in = p;
  z->avail_in = n;
  z->state->sub.marker = m;

  /* return no joy or set up to restart on a new block */
  if (m != 4)
    return Z_DATA_ERROR;
  r = z->total_in;  w = z->total_out;
  zlib_inflateReset(z);
  z->total_in = r;  z->total_out = w;
  z->state->mode = BLOCKS;
  return Z_OK;
}


/* Returns true if inflate is currently at the end of a block generated
 * by Z_SYNC_FLUSH or Z_FULL_FLUSH. This function is used by one PPP
 * implementation to provide an additional safety check. PPP uses Z_SYNC_FLUSH
 * but removes the length bytes of the resulting empty stored block. When
 * decompressing, PPP checks that at the end of input packet, inflate is
 * waiting for these length bytes.
 */
int ZEXPORT zlib_inflateSyncPoint(z)
z_streamp z;
{
  if (z == Z_NULL || z->state == Z_NULL || z->state->blocks == Z_NULL)
    return Z_STREAM_ERROR;
  return zlib_inflate_blocks_sync_point(z->state->blocks);
}

/*
 * This subroutine adds the data at next_in/avail_in to the output history
 * without performing any output.  The output buffer must be "caught up";
 * i.e. no pending output (hence s->read equals s->write), and the state must
 * be BLOCKS (i.e. we should be willing to see the start of a series of
 * BLOCKS).  On exit, the output will also be caught up, and the checksum
 * will have been updated if need be.
 */
static int zlib_inflate_addhistory(inflate_blocks_statef *s,
				      z_stream              *z)
{
    uLong b;              /* bit buffer */  /* NOT USED HERE */
    uInt k;               /* bits in bit buffer */ /* NOT USED HERE */
    uInt t;               /* temporary storage */
    Bytef *p;             /* input data pointer */
    uInt n;               /* bytes available there */
    Bytef *q;             /* output window write pointer */
    uInt m;               /* bytes to end of window or read pointer */

    if (s->read != s->write)
	return Z_STREAM_ERROR;
    if (s->mode != TYPE)
	return Z_DATA_ERROR;

    /* we're ready to rock */
    LOAD
    /* while there is input ready, copy to output buffer, moving
     * pointers as needed.
     */
    while (n) {
	t = n;  /* how many to do */
	/* is there room until end of buffer? */
	if (t > m) t = m;
	/* update check information */
	if (s->checkfn != Z_NULL)
	    s->check = (*s->checkfn)(s->check, q, t);
	memcpy(q, p, t);
	q += t;
	p += t;
	n -= t;
	z->total_out += t;
	s->read = q;    /* drag read pointer forward */
/*      WWRAP  */ 	/* expand WWRAP macro by hand to handle s->read */
	if (q == s->end) {
	    s->read = q = s->window;
	    m = WAVAIL;
	}
    }
    UPDATE
    return Z_OK;
}


/*
 * This subroutine adds the data at next_in/avail_in to the output history
 * without performing any output.  The output buffer must be "caught up";
 * i.e. no pending output (hence s->read equals s->write), and the state must
 * be BLOCKS (i.e. we should be willing to see the start of a series of
 * BLOCKS).  On exit, the output will also be caught up, and the checksum
 * will have been updated if need be.
 */

int ZEXPORT zlib_inflateIncomp(z)
z_stream *z;
{
    if (z->state->mode != BLOCKS)
	return Z_DATA_ERROR;
    return zlib_inflate_addhistory(z->state->blocks, z);
}
