/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		IP/TCP/UDP checksumming routines
 *
 * Authors:	Jorge Cwik, <jorge@laser.satlink.net>
 *		Arnt Gulbrandsen, <agulbra@nvg.unit.no>
 *		Tom May, <ftom@netcom.com>
 *		Lots of code moved from tcp.c and ip.c; see those files
 *		for more names.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */

#include <net/checksum.h>

#undef PROFILE_CHECKSUM

#ifdef PROFILE_CHECKSUM
/* these are just for profiling the checksum code with an oscillioscope.. uh */
#if 0
#define BITOFF *((unsigned char *)0xb0000030) = 0xff
#define BITON *((unsigned char *)0xb0000030) = 0x0
#endif
#include <asm/io.h>
#define CBITON LED_ACTIVE_SET(1)
#define CBITOFF LED_ACTIVE_SET(0)
#define BITOFF
#define BITON
#else
#define BITOFF
#define BITON
#define CBITOFF
#define CBITON
#endif

/*
 * computes a partial checksum, e.g. for TCP/UDP fragments
 */

#include <asm/delay.h>

unsigned int csum_partial(const unsigned char * buff, int len, unsigned int sum)
{
  /*
   * Experiments with ethernet and slip connections show that buff
   * is aligned on either a 2-byte or 4-byte boundary.
   */
  const unsigned char *endMarker = buff + len;
  const unsigned char *marker = endMarker - (len % 16);
#if 0
  if((int)buff & 0x3)
    printk("unaligned buff %p\n", buff);
  __delay(900); /* extra delay of 90 us to test performance hit */
#endif
  BITON;
  while (buff < marker) {
    sum += *((unsigned short *)buff)++;
    sum += *((unsigned short *)buff)++;
    sum += *((unsigned short *)buff)++;
    sum += *((unsigned short *)buff)++;
    sum += *((unsigned short *)buff)++;
    sum += *((unsigned short *)buff)++;
    sum += *((unsigned short *)buff)++;
    sum += *((unsigned short *)buff)++;
  }
  marker = endMarker - (len % 2);
  while(buff < marker) {
    sum += *((unsigned short *)buff)++;
  }
  if(endMarker - buff > 0) {
    sum += *buff;                 /* add extra byte seperately */
  }
  BITOFF;
  return(sum);
}

#if 0

/*
 * copy while checksumming, otherwise like csum_partial
 */

unsigned int csum_partial_copy(const unsigned char *src, unsigned char *dst, 
				  int len, unsigned int sum)
{
  const unsigned char *endMarker;
  const unsigned char *marker;
  printk("csum_partial_copy len %d.\n", len);
#if 0
  if((int)src & 0x3)
    printk("unaligned src %p\n", src);
  if((int)dst & 0x3)
    printk("unaligned dst %p\n", dst);
  __delay(1800); /* extra delay of 90 us to test performance hit */
#endif
  endMarker = src + len;
  marker = endMarker - (len % 16);
  CBITON;
  while(src < marker) {
    sum += (*((unsigned short *)dst)++ = *((unsigned short *)src)++);
    sum += (*((unsigned short *)dst)++ = *((unsigned short *)src)++);
    sum += (*((unsigned short *)dst)++ = *((unsigned short *)src)++);
    sum += (*((unsigned short *)dst)++ = *((unsigned short *)src)++);
    sum += (*((unsigned short *)dst)++ = *((unsigned short *)src)++);
    sum += (*((unsigned short *)dst)++ = *((unsigned short *)src)++);
    sum += (*((unsigned short *)dst)++ = *((unsigned short *)src)++);
    sum += (*((unsigned short *)dst)++ = *((unsigned short *)src)++);
  }
  marker = endMarker - (len % 2);
  while(src < marker) {
    sum += (*((unsigned short *)dst)++ = *((unsigned short *)src)++);
  }
  if(endMarker - src > 0) {
    sum += (*dst = *src);                 /* add extra byte seperately */
  }
  CBITOFF;
  return(sum);
}

#endif
