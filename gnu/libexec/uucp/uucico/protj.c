/* protj.c
   The 'j' protocol.

   Copyright (C) 1992, 1994 Ian Lance Taylor

   This file is part of the Taylor UUCP package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, Building 200, 1 Kendall Square, Cambridge, MA 02139.
   */

#include "uucp.h"

#if USE_RCS_ID
const char protj_rcsid[] = "$Id: protj.c,v 1.2 1994/05/07 18:13:50 ache Exp $";
#endif

#include <ctype.h>
#include <errno.h>

#include "uudefs.h"
#include "uuconf.h"
#include "conn.h"
#include "trans.h"
#include "system.h"
#include "prot.h"

/* The 'j' protocol.

   The 'j' protocol is a wrapper around the 'i' protocol, which avoids
   the use of certain characters, such as XON and XOFF.

   Each 'j' protocol packet begins with a '^' character, followed by a
   two byte encoded size giving the total number of bytes in the
   packet.  The first byte is HIGH, the second byte is LOW, and the
   number of bytes is (HIGH - 32) * 64 + (LOW - 32), where 32 <= HIGH
   < 127 and 32 <= LOW < 96 (i.e., HIGH and LOW are printable ASCII
   characters).  This is followed by a '=' character.  The next two
   bytes are the number of data bytes in the packet, using the same
   encoding.  This is followed by a '@' character, and then that
   number of data bytes.  The remaining bytes in the packet are
   indices of bytes which must be transformed, followed by a trailing
   '~' character.  The indices are encoded in the following overly
   complex format.

   Each byte index is two bytes long.  The first byte in the index is
   INDEX-HIGH and the second is INDEX-LOW.  If 32 <= INDEX-HIGH < 126,
   the byte index refers to the byte at position (INDEX-HIGH - 32) *
   32 + INDEX-LOW % 32 in the actual data, where 32 <= INDEX-LOW <
   127.  If 32 <= INDEX-LOW < 64, then 128 must be added to the
   indexed byte.  If 64 <= INDEX-LOW < 96, then the indexed byte must
   be exclusive or'red with 32.  If 96 <= INDEX-LOW < 127, both
   operations must be performed.  If INDEX-HIGH == 126, then the byte
   index refers to the byte at position (INDEX-LOW - 32) * 32 + 31,
   where 32 <= INDEX-LOW < 126.  128 must be added to the byte, and it
   must be exclusive or'red with 32.  This unfortunately requires a
   special test (when encoding INDEX-LOW must be checked for 127; when
   decoding INDEX-HIGH must be checked for 126).  It does, however,
   permit the byte indices field to consist exclusively of printable
   ASCII characters.

   The maximum value for a byte index is (125 - 32) * 32 + 31 == 3007,
   so the is the maximum number of data bytes permitted.  Since it is
   convenient to have each 'j' protocol packet correspond to each 'i'
   protocol packet, we restrict the 'i' protocol accordingly.

   Note that this encoding method assumes that we can send all
   printable ASCII characters.  */

/* The first byte of each packet.  I just picked these values
   randomly, trying to get characters that were perhaps slightly less
   likely to appear in normal text.  */
#define FIRST '\136'

/* The fourth byte of each packet.  */
#define FOURTH '\075'

/* The seventh byte of each packet.  */
#define SEVENTH '\100'

/* The trailing byte of each packet.  */
#define TRAILER '\176'

/* The length of the header.  */
#define CHDRLEN (7)

/* Get a number of bytes encoded in a two byte length at the start of
   a packet.  */
#define CGETLENGTH(b1, b2) (((b1) - 32) * 64 + ((b2) - 32))

/* Set the high and low bytes of a two byte length at the start of a
   packet.  */
#define ISETLENGTH_FIRST(i) ((i) / 64 + 32)
#define ISETLENGTH_SECOND(i) ((i) % 64 + 32)

/* The maximum packet size we support, as determined by the byte
   indices.  */
#define IMAXPACKSIZE ((125 - 32) * 32 + 31)

/* Amount to offset the bytes in the byte index by.  */
#define INDEX_OFFSET (32)

/* Maximum value of INDEX-LOW, before offsetting.  */
#define INDEX_MAX_LOW (32)

/* Maximum value of INDEX-HIGH, before offsetting. */
#define INDEX_MAX_HIGH (94)

/* The set of characters to avoid.  */
static char *zJavoid;

/* The number of characters to avoid.  */
static size_t cJavoid;

/* A buffer used when sending data.  */
static char *zJbuf;

/* The end of the undecoded data in abPrecbuf.  */
static int iJrecend;

/* Local functions.  */
static boolean fjsend_data P((struct sconnection *qconn, const char *zsend,
			      size_t csend, boolean fdoread));
static boolean fjreceive_data P((struct sconnection *qconn, size_t cneed,
				 size_t *pcrec, int ctimeout,
				 boolean freport));
static boolean fjprocess_data P((size_t *pcneed));

/* Start the protocol.  We first send over the list of characters to
   avoid as an escape sequence, starting with FIRST and ending with
   TRAILER.  There is no error checking done on this string.  */

boolean
fjstart (qdaemon, pzlog)
     struct sdaemon *qdaemon;
     char **pzlog;
{
  size_t clen;
  char *zsend;
  int b;
  size_t cbuf, cgot;
  char *zbuf;
  int i;

  /* Send the characters we want to avoid to the other side.  */
  clen = strlen (zJavoid_parameter);
  zsend = zbufalc (clen + 3);
  zsend[0] = FIRST;
  memcpy (zsend + 1, zJavoid_parameter, clen);
  zsend[clen + 1] = TRAILER;
  zsend[clen + 2] = '\0';
  if (! fsend_data (qdaemon->qconn, zsend, clen + 2, TRUE))
    {
      ubuffree (zsend);
      return FALSE;
    }
  ubuffree (zsend);

  /* Read the characters the other side wants to avoid.  */
  while ((b = breceive_char (qdaemon->qconn, cIsync_timeout, TRUE))
	 != FIRST)
    {
      if (b < 0)
	{
	  if (b == -1)
	    ulog (LOG_ERROR, "Timed out in 'j' protocol startup");
	  return FALSE;
	}
    }

  cbuf = 20;
  zbuf = zbufalc (cbuf);
  cgot = 0;
  while ((b = breceive_char (qdaemon->qconn, cIsync_timeout, TRUE))
	 != TRAILER)
    {
      if (b < 0)
	{
	  ubuffree (zbuf);
	  if (b == -1)
	    ulog (LOG_ERROR, "Timed out in 'j' protocol startup");
	  return FALSE;
	}
      if (cgot + 1 >= cbuf)
	{
	  char *znew;

	  cbuf += 20;
	  znew = zbufalc (cbuf);
	  memcpy (znew, zbuf, cgot);
	  ubuffree (zbuf);
	  zbuf = znew;
	}
      zbuf[cgot] = b;
      ++cgot;
    }
  zbuf[cgot] = '\0';

  /* Merge the local and remote avoid bytes into one list, translated
     into bytes.  */
  cgot = cescape (zbuf);

  clen = strlen (zJavoid_parameter);
  zJavoid = zbufalc (clen + cgot + 1);
  memcpy (zJavoid, zJavoid_parameter, clen + 1);
  cJavoid = cescape (zJavoid);

  for (i = 0; i < cgot; i++)
    {
      if (memchr (zJavoid, zbuf[i], cJavoid) == NULL)
	{
	  zJavoid[cJavoid] = zbuf[i];
	  ++cJavoid;
	}
    }

  ubuffree (zbuf);

  /* We can't avoid ASCII printable characters, since the encoding
     method assumes that they can always be sent.  If it ever turns
     out to be important, a different encoding method could be used,
     perhaps keyed by a different FIRST character.  */
  if (cJavoid == 0)
    {
      ulog (LOG_ERROR, "No characters to avoid in 'j' protocol");
      return FALSE;
    }
  for (i = 0; i < cJavoid; i++)
    {
      if (zJavoid[i] >= 32 && zJavoid[i] <= 126)
	{
	  ulog (LOG_ERROR, "'j' protocol can't avoid character '\\%03o'",
		zJavoid[i]);
	  return FALSE;
	}
    }

  /* If we are avoiding XON and XOFF, use XON/XOFF handshaking.  */
  if (memchr (zJavoid, '\021', cJavoid) != NULL
      && memchr (zJavoid, '\023', cJavoid) != NULL)
    {
      if (! fconn_set (qdaemon->qconn, PARITYSETTING_NONE,
		       STRIPSETTING_EIGHTBITS, XONXOFF_ON))
	return FALSE;
    }

  /* Let the port settle.  */
  usysdep_sleep (2);

  /* Allocate a buffer we use when sending data.  We will probably
     never actually need one this big; if this code is ported to a
     computer with small amounts of memory, this should be changed to
     increase the buffer size as needed.  */
  zJbuf = zbufalc (CHDRLEN + IMAXPACKSIZE * 3 + 1);
  zJbuf[0] = FIRST;
  zJbuf[3] = FOURTH;
  zJbuf[6] = SEVENTH;

  /* iJrecend is the end of the undecoded data, and iPrecend is the
     end of the decoded data.  At this point there is no decoded data,
     and we must initialize the variables accordingly.  */
  iJrecend = iPrecend;
  iPrecend = iPrecstart;

  /* Now do the 'i' protocol startup.  */
  return fijstart (qdaemon, pzlog, IMAXPACKSIZE, fjsend_data,
		   fjreceive_data);
}

/* Shut down the protocol.  */

boolean
fjshutdown (qdaemon)
     struct sdaemon *qdaemon;
{
  boolean fret;

  fret = fishutdown (qdaemon);
  ubuffree (zJavoid);
  ubuffree (zJbuf);
  return fret;
}

/* Encode a packet of data and send it.  This copies the data, which
   is a waste of time, but calling fsend_data three times (for the
   header, the body, and the trailer) would waste even more time.  */

static boolean
fjsend_data (qconn, zsend, csend, fdoread)
     struct sconnection *qconn;
     const char *zsend;
     size_t csend;
     boolean fdoread;
{
  char *zput, *zindex;
  const char *zfrom, *zend;
  char bfirst, bsecond;
  int iprecendhold;
  boolean fret;

  zput = zJbuf + CHDRLEN;
  zindex = zput + csend;
  zfrom = zsend;
  zend = zsend + csend;

  /* Optimize for the common case of avoiding two characters.  */
  bfirst = zJavoid[0];
  if (cJavoid <= 1)
    bsecond = bfirst;
  else
    bsecond = zJavoid[1];
  while (zfrom < zend)
    {
      char b;
      boolean f128, f32;
      int i, ihigh, ilow;

      b = *zfrom++;
      if (b != bfirst && b != bsecond)
	{
	  int ca;
	  char *za;

	  if (cJavoid <= 2)
	    {
	      *zput++ = b;
	      continue;
	    }

	  ca = cJavoid - 2;
	  za = zJavoid + 2;
	  while (ca-- != 0)
	    if (*za++ == b)
	      break;

	  if (ca < 0)
	    {
	      *zput++ = b;
	      continue;
	    }
	}

      if ((b & 0x80) == 0)
	f128 = FALSE;
      else
	{
	  b &=~ 0x80;
	  f128 = TRUE;
	}
      if (b >= 32 && b != 127)
	f32 = FALSE;
      else
	{
	  b ^= 0x20;
	  f32 = TRUE;
	}

      /* We must now put the byte index into the buffer.  The byte
	 index is encoded similarly to the length of the actual data,
	 but the byte index also encodes the operations that must be
	 performed on the byte.  The first byte in the index is the
	 most significant bits.  If we only had to subtract 128 from
	 the byte, we use the second byte directly.  If we had to xor
	 the byte with 32, we add 32 to the second byte index.  If we
	 had to perform both operations, we add 64 to the second byte
	 index.  However, if we had to perform both operations, and
	 the second byte index was 31, then after adding 64 and
	 offsetting by 32 we would come up with 127, which we are not
	 permitted to use.  Therefore, in this special case we set the
	 first byte of the index to 126 and put the original first
	 byte into the second byte position instead.  This is why we
	 could not permit the high byte of the length of the actual
	 data to be 126.  We can get away with the switch because both
	 the value of the second byte index (31) and the operations to
	 perform (both) are known.  */
      i = zput - (zJbuf + CHDRLEN);
      ihigh = i / INDEX_MAX_LOW;
      ilow = i % INDEX_MAX_LOW;

      if (f128 && ! f32)
	;
      else if (f32 && ! f128)
	ilow += INDEX_MAX_LOW;
      else
	{
	  /* Both operations had to be performed.  */
	  if (ilow != INDEX_MAX_LOW - 1)
	    ilow += 2 * INDEX_MAX_LOW;
	  else
	    {
	      ilow = ihigh;
	      ihigh = INDEX_MAX_HIGH;
	    }
	}

      *zindex++ = ihigh + INDEX_OFFSET;
      *zindex++ = ilow + INDEX_OFFSET;
      *zput++ = b;
    }

  *zindex++ = TRAILER;

  /* Set the lengths into the buffer.  zJbuf[0,3,6] were set when
     zJbuf was allocated, and are never changed thereafter.  */
  zJbuf[1] = ISETLENGTH_FIRST (zindex - zJbuf);
  zJbuf[2] = ISETLENGTH_SECOND (zindex - zJbuf);
  zJbuf[4] = ISETLENGTH_FIRST (csend);
  zJbuf[5] = ISETLENGTH_SECOND (csend);

  /* Send the data over the line.  We must preserve iPrecend as
     discussed in fjreceive_data.  */
  iprecendhold = iPrecend;
  iPrecend = iJrecend;
  fret = fsend_data (qconn, zJbuf, (size_t) (zindex - zJbuf), fdoread);
  iJrecend = iPrecend;
  iPrecend = iprecendhold;

  /* Process any bytes that may have been placed in abPrecbuf.  */
  if (fret && iPrecend != iJrecend)
    {
      if (! fjprocess_data ((size_t *) NULL))
	return FALSE;
    }

  return fret;
}

/* Receive and decode data.  This is called by fiwait_for_packet.  We
   need to be able to return decoded data between iPrecstart and
   iPrecend, while not losing any undecoded partial packets we may
   have read.  We use iJrecend as a pointer to the end of the
   undecoded data, and set iPrecend for the decoded data.  iPrecend
   points to the start of the undecoded data.  */

static boolean
fjreceive_data (qconn, cineed, pcrec, ctimeout, freport)
     struct sconnection *qconn;
     size_t cineed;
     size_t *pcrec;
     int ctimeout;
     boolean freport;
{
  int iprecendstart;
  size_t cjneed;
  size_t crec;
  int cnew;

  iprecendstart = iPrecend;

  /* Figure out how many bytes we need to decode the next packet.  */
  if (! fjprocess_data (&cjneed))
    return FALSE;

  /* As we long as we read some data but don't have enough to decode a
     packet, we try to read some more.  We decrease the timeout each
     time so that we will not wait forever if the connection starts
     dribbling data.  */
  do
    {
      int iprecendhold;
      size_t cneed;

      if (cjneed > cineed)
	cneed = cjneed;
      else
	cneed = cineed;

      /* We are setting iPrecend to the end of the decoded data for
	 the 'i' protocol.  When we do the actual read, we have to set
	 it to the end of the undecoded data so that any undecoded
	 data we have received is not overwritten.  */
      iprecendhold = iPrecend;
      iPrecend = iJrecend;
      if (! freceive_data (qconn, cneed, &crec, ctimeout, freport))
	return FALSE;
      iJrecend = iPrecend;
      iPrecend = iprecendhold;

      /* Process any data we have received.  This will set iPrecend to
	 the end of the new decoded data.  */
      if (! fjprocess_data (&cjneed))
	return FALSE;

      cnew = iPrecend - iprecendstart;
      if (cnew < 0)
	cnew += CRECBUFLEN;

      if (cnew > cineed)
	cineed = 0;
      else
	cineed -= cnew;

      --ctimeout;
    }
  while (cnew == 0 && crec > 0 && ctimeout > 0);

  DEBUG_MESSAGE1 (DEBUG_PROTO, "fjreceive_data: Got %d decoded bytes",
		  cnew);

  *pcrec = cnew;
  return TRUE;
}

/* Decode the data in the buffer, optionally returning the number of
   bytes needed to complete the next packet.  */

static boolean
fjprocess_data (pcneed)
     size_t *pcneed;
{
  int istart;

  istart = iPrecend;
  while (istart != iJrecend)
    {
      int i, iget;
      char ab[CHDRLEN];
      int cpacket, cdata, chave;
      int iindex, iendindex;

      /* Find the next occurrence of FIRST.  If we have to skip some
	 garbage bytes to get to it, zero them out (so they don't
	 confuse the 'i' protocol) and advance iPrecend.  This will
	 save us from looking at them again.  */
      if (abPrecbuf[istart] != FIRST)
	{
	  int cintro;
	  char *zintro;
	  size_t cskipped;

	  cintro = iJrecend - istart;
	  if (cintro < 0)
	    cintro = CRECBUFLEN - istart;

	  zintro = memchr (abPrecbuf + istart, FIRST, (size_t) cintro);
	  if (zintro == NULL)
	    {
	      bzero (abPrecbuf + istart, (size_t) cintro);
	      istart = (istart + cintro) % CRECBUFLEN;
	      iPrecend = istart;
	      continue;
	    }

	  cskipped = zintro - (abPrecbuf + istart);
	  bzero (abPrecbuf + istart, cskipped);
	  istart += cskipped;
	  iPrecend = istart;
	}

      for (i = 0, iget = istart;
	   i < CHDRLEN && iget != iJrecend;
	   ++i, iget = (iget + 1) % CRECBUFLEN)
	ab[i] = abPrecbuf[iget];

      if (i < CHDRLEN)
	{
	  if (pcneed != NULL)
	    *pcneed = CHDRLEN - i;
	  return TRUE;
	}

      cpacket = CGETLENGTH (ab[1], ab[2]);
      cdata = CGETLENGTH (ab[4], ab[5]);

      /* Make sure the header has the right magic characters, that the
	 data is not larger than the packet, and that we have an even
	 number of byte index characters.  */
      if (ab[3] != FOURTH
	  || ab[6] != SEVENTH
	  || cdata > cpacket - CHDRLEN - 1
	  || (cpacket - cdata - CHDRLEN - 1) % 2 == 1)
	{
	  istart = (istart + 1) % CRECBUFLEN;
	  continue;
	}

      chave = iJrecend - istart;
      if (chave < 0)
	chave += CRECBUFLEN;

      if (chave < cpacket)
	{
	  if (pcneed != NULL)
	    *pcneed = cpacket - chave;
	  return TRUE;
	}

      /* Figure out where the byte indices start and end.  */
      iindex = (istart + CHDRLEN + cdata) % CRECBUFLEN;
      iendindex = (istart + cpacket - 1) % CRECBUFLEN;

      /* Make sure the magic trailer character is there.  */
      if (abPrecbuf[iendindex] != TRAILER)
	{
	  istart = (istart + 1) % CRECBUFLEN;
	  continue;
	}

      /* We have a packet to decode.  The decoding process is simpler
	 than the encoding process, since all we have to do is examine
	 the byte indices.  We zero out the byte indices as we go, so
	 that they will not confuse the 'i' protocol.  */
      while (iindex != iendindex)
	{
	  int ihigh, ilow;
	  boolean f32, f128;
	  int iset;

	  ihigh = abPrecbuf[iindex] - INDEX_OFFSET;
	  abPrecbuf[iindex] = 0;
	  iindex = (iindex + 1) % CRECBUFLEN;
	  ilow = abPrecbuf[iindex] - INDEX_OFFSET;
	  abPrecbuf[iindex] = 0;
	  iindex = (iindex + 1) % CRECBUFLEN;

	  /* Now we must undo the encoding, by adding 128 and xoring
	     with 32 as appropriate.  Which to do is encoded in the
	     low byte, except that if the high byte is the special
	     value 126, then the low byte is actually the high byte
	     and both operations are performed.  */
	  f128 = TRUE;
	  f32 = TRUE;
	  if (ihigh == INDEX_MAX_HIGH)
	    iset = ilow * INDEX_MAX_LOW + INDEX_MAX_LOW - 1;
	  else
	    {
	      iset = ihigh * INDEX_MAX_LOW + ilow % INDEX_MAX_LOW;
	      if (ilow < INDEX_MAX_LOW)
		f32 = FALSE;
	      else if (ilow < 2 * INDEX_MAX_LOW)
		f128 = FALSE;
	    }

	  /* Now iset is the index from the start of the data to the
	     byte to modify; adjust it to an index in abPrecbuf.  */
	  iset = (istart + CHDRLEN + iset) % CRECBUFLEN;

	  if (f128)
	    abPrecbuf[iset] |= 0x80;
	  if (f32)
	    abPrecbuf[iset] ^= 0x20;
	}

      /* Zero out the header and trailer to avoid confusing the 'i'
	 protocol, and update iPrecend to the end of decoded data.  */
      for (i = 0, iget = istart;
	   i < CHDRLEN && iget != iJrecend;
	   ++i, iget = (iget + 1) % CRECBUFLEN)
	abPrecbuf[iget] = 0;
      abPrecbuf[iendindex] = 0;
      iPrecend = (iendindex + 1) % CRECBUFLEN;
      istart = iPrecend;
    }

  if (pcneed != NULL)
    *pcneed = CHDRLEN + 1;
  return TRUE;
}
