/*
 * Copyright (c) 1990, 1991, 1992, 1993
 * The Regents of the University of California. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#if !defined(lint) && !defined(__GNUC__)
static char rcsid[] =
    "@(#)$FreeBSD: src/usr.sbin/tcpdump/tcpslice/search.c,v 1.4 1999/08/28 05:11:32 peter Exp $ (LBL)";
#endif

/*
 * search.c - supports fast searching through tcpdump files for timestamps
 */

#include "tcpslice.h"


/* Maximum number of seconds that we can conceive of a dump file spanning. */
#define MAX_REASONABLE_FILE_SPAN (3600*24*366)	/* one year */

/* Maximum packet length we ever expect to see. */
#define MAX_REASONABLE_PACKET_LENGTH 65535

/* Size of a packet header in bytes; easier than typing the sizeof() all
 * the time ...
 */
#define PACKET_HDR_LEN (sizeof( struct pcap_pkthdr ))

extern int snaplen;

/* The maximum size of a packet, including its header. */
#define MAX_PACKET_SIZE (PACKET_HDR_LEN + snaplen)

/* Number of contiguous bytes from a dumpfile in which there's guaranteed
 * to be enough information to find a "definite" header if one exists
 * therein.  This takes 3 full packets - the first to be just misaligned
 * (one byte short of a full packet), missing its timestamp; the second
 * to have the legitimate timestamp; and the third to provide confirmation
 * that the second is legit, making it a "definite" header.  We could
 * scrimp a bit here since not the entire third packet is required, but
 * it doesn't seem worth it
 */
#define MAX_BYTES_FOR_DEFINITE_HEADER (3 * MAX_PACKET_SIZE)

/* Maximum number of seconds that might reasonably separate two headers. */
#define MAX_REASONABLE_HDR_SEPARATION (3600 * 24 * 7)	/* one week */

/* When searching a file for a packet, if we think we're within this many
 * bytes of the packet we just search linearly.  Since linear searches are
 * probably much faster than random ones (random ones require searching for
 * the beginning of the packet, which may be unaligned in memory), we make
 * this value pretty hefty.
 */
#define STRAIGHT_SCAN_THRESHOLD (100 * MAX_PACKET_SIZE)


/* Given a header and an acceptable first and last time stamp, returns non-zero
 * if the header looks reasonable and zero otherwise.
 */
static int
reasonable_header( struct pcap_pkthdr *hdr, long first_time, long last_time )
	{
	if ( last_time == 0 )
		last_time = first_time + MAX_REASONABLE_FILE_SPAN;

	return hdr->ts.tv_sec >= first_time &&
	       hdr->ts.tv_sec <= last_time &&
	       hdr->len > 0 &&
	       hdr->len <= MAX_REASONABLE_PACKET_LENGTH &&
	       hdr->caplen > 0 &&
	       hdr->caplen <= MAX_REASONABLE_PACKET_LENGTH;
	}


#define	SWAPLONG(y) \
((((y)&0xff)<<24) | (((y)&0xff00)<<8) | (((y)&0xff0000)>>8) | (((y)>>24)&0xff))
#define	SWAPSHORT(y) \
	( (((y)&0xff)<<8) | (((y)&0xff00)>>8) )

/* Given a buffer, extracts a (properly aligned) packet header from it. */

static void
extract_header( pcap_t *p, u_char *buf, struct pcap_pkthdr *hdr )
	{
	bcopy((char *) buf, (char *) hdr, sizeof(struct pcap_pkthdr));

	if ( pcap_is_swapped( p ) )
		{
		hdr->ts.tv_sec = SWAPLONG(hdr->ts.tv_sec);
		hdr->ts.tv_usec = SWAPLONG(hdr->ts.tv_usec);
		hdr->len = SWAPLONG(hdr->len);
		hdr->caplen = SWAPLONG(hdr->caplen);
		}

	/*
	 * From bpf/libpcap/savefile.c:
	 *
	 * We interchanged the caplen and len fields at version 2.3,
	 * in order to match the bpf header layout.  But unfortunately
	 * some files were written with version 2.3 in their headers
	 * but without the interchanged fields.
	 */
	if ( pcap_minor_version( p ) < 3 ||
	     (pcap_minor_version( p ) == 3 && hdr->caplen > hdr->len) )
		{
		int t = hdr->caplen;
		hdr->caplen = hdr->len;
		hdr->len = t;
		}
	}


/* Search a buffer to locate the first header within it.  Return values
 * are HEADER_NONE, HEADER_CLASH, HEADER_PERHAPS, and HEADER_DEFINITELY.
 * The first indicates that no evidence of a header was found; the second
 * that two or more possible headers were found, neither more convincing
 * than the other(s); the third that exactly one "possible" header was
 * found; and the fourth that exactly one "definite" header was found.
 *
 * Headers are detected by looking for positions in the buffer which have
 * reasonable timestamps and lengths.  If there is enough room in the buffer
 * for another header to follow a candidate header, a check is made for
 * that following header.  If it is present then the header is *definite*
 * (unless another "perhaps" or "definite" header is found); if not, then
 * the header is discarded.  If there is not enough room in the buffer for
 * another header then the candidate is *perhaps* (unless another header
 * is subsequently found).  A "tie" between a "definite" header and a
 * "perhaps" header is resolved in favor of the definite header.  Any
 * other tie leads to HEADER_CLASH.
 *
 * The buffer position of the header is returned in hdrpos_addr and
 * for convenience the corresponding header in return_hdr.
 *
 * first_time is the earliest possible acceptable timestamp in the
 * header.  last_time, if non-zero, is the last such timestamp.  If
 * zero, then up to MAX_REASONABLE_FILE_SPAN seconds after first_time
 * is acceptable.
 */

#define HEADER_NONE 0
#define HEADER_CLASH 1
#define HEADER_PERHAPS 2
#define HEADER_DEFINITELY 3

static int
find_header( pcap_t *p, u_char *buf, int buf_len,
		long first_time, long last_time,
		u_char **hdrpos_addr, struct pcap_pkthdr *return_hdr )
	{
	u_char *bufptr, *bufend, *last_pos_to_try;
	struct pcap_pkthdr hdr, hdr2;
	int status = HEADER_NONE;
	int saw_PERHAPS_clash = 0;

	/* Initially, try each buffer position to see whether it looks like
	 * a valid packet header.  We may later restrict the positions we look
	 * at to avoid seeing a sequence of legitimate headers as conflicting
	 * with one another.
	 */
	bufend = buf + buf_len;
	last_pos_to_try = bufend - PACKET_HDR_LEN;

	for ( bufptr = buf; bufptr < last_pos_to_try; ++bufptr )
	    {
	    extract_header( p, bufptr, &hdr );

	    if ( reasonable_header( &hdr, first_time, last_time ) )
		{
		u_char *next_header = bufptr + PACKET_HDR_LEN + hdr.caplen;

		if ( next_header + PACKET_HDR_LEN < bufend )
		    { /* check for another good header */
		    extract_header( p, next_header, &hdr2 );

		    if ( reasonable_header( &hdr2, hdr.ts.tv_sec,
			    hdr.ts.tv_sec + MAX_REASONABLE_HDR_SEPARATION ) )
			{ /* a confirmed header */
			switch ( status )
			    {
			    case HEADER_NONE:
			    case HEADER_PERHAPS:
				status = HEADER_DEFINITELY;
				*hdrpos_addr = bufptr;
				*return_hdr = hdr;

				/* Make sure we don't demote this "definite"
				 * to a "clash" if we stumble across its
				 * successor.
				 */
				last_pos_to_try = next_header - PACKET_HDR_LEN;
				break;

			    case HEADER_DEFINITELY:
				return HEADER_CLASH;

			    default:
				error( "bad status in find_header()" );
			    }
			}

		    /* ... else the header is bogus - we've verified that it's
		     * not followed by a reasonable header.
		     */
		    }

		else
		    { /* can't check for another good header */
		    switch ( status )
			{
			case HEADER_NONE:
			    status = HEADER_PERHAPS;
			    *hdrpos_addr = bufptr;
			    *return_hdr = hdr;
			    break;

			case HEADER_PERHAPS:
			    /* We don't immediately turn this into a
			     * clash because perhaps we'll later see a
			     * "definite" which will save us ...
			     */
			    saw_PERHAPS_clash = 1;
			    break;

			case HEADER_DEFINITELY:
			    /* Keep the definite in preference to this one. */
			    break;

			default:
			    error( "bad status in find_header()" );
			}
		    }
		}
	    }

	if ( status == HEADER_PERHAPS && saw_PERHAPS_clash )
		status = HEADER_CLASH;

	return status;
	}


/* Positions the sf_readfile stream such that the next sf_read() will
 * read the final full packet in the file.  Returns non-zero if
 * successful, zero if unsuccessful.  If successful, returns the
 * timestamp of the last packet in last_timestamp.
 *
 * Note that this routine is a special case of sf_find_packet().  In
 * order to use sf_find_packet(), one first must use this routine in
 * order to give sf_find_packet() an upper bound on the timestamps
 * present in the dump file.
 */
int
sf_find_end( pcap_t *p, struct timeval *first_timestamp,
		struct timeval *last_timestamp )
	{
	long first_time = first_timestamp->tv_sec;
	u_int num_bytes;
	u_char *buf, *bufpos, *bufend;
	u_char *hdrpos;
	struct pcap_pkthdr hdr, successor_hdr;
	int status;

	/* Allow enough room for at least two full (untruncated) packets,
	 * perhaps followed by a truncated packet, so we have a shot at
	 * finding a "definite" header and following its chain to the
	 * end of the file.
	 */
	num_bytes = MAX_BYTES_FOR_DEFINITE_HEADER;
	if ( fseek( pcap_file( p ), (long) -num_bytes, 2 ) < 0 )
		return 0;

	buf = (u_char *)malloc((u_int) num_bytes);
	if ( ! buf )
		return 0;

	status = 0;
	bufpos = buf;
	bufend = buf + num_bytes;

	if ( fread( (char *) bufpos, num_bytes, 1, pcap_file( p ) ) != 1 )
		goto done;

	if ( find_header( p, bufpos, num_bytes,
			  first_time, 0L, &hdrpos, &hdr ) != HEADER_DEFINITELY )
		goto done;

	/* Okay, we have a definite header in our hands.  Follow its
	 * chain till we find the last valid packet in the file ...
	 */
	for ( ; ; )
		{
		/* move to the next header position */
		bufpos = hdrpos + PACKET_HDR_LEN + hdr.caplen;

		/* bufpos now points to a candidate packet, which if valid
		 * should replace the current packet pointed to by hdrpos as
		 * the last valid packet ...
		 */
		if ( bufpos >= bufend - PACKET_HDR_LEN )
			/* not enough room for another header */
			break;

		extract_header( p, bufpos, &successor_hdr );

		first_time = hdr.ts.tv_sec;
		if ( ! reasonable_header( &successor_hdr, first_time, 0L ) )
			/* this bodes ill - it means bufpos is perhaps a
			 * bogus packet header after all ...
			 */
			break;

		/* Note that the following test is for whether the next
		 * packet starts at a position > bufend, *not* for a
		 * position >= bufend.  If this is the last packet in the
		 * file and there isn't a subsequent partial packet, then
		 * we expect the first buffer position beyond this packet
		 * to be just beyond the end of the buffer, i.e., at bufend
		 * itself.
		 */
		if ( bufpos + PACKET_HDR_LEN + successor_hdr.caplen > bufend )
			/* the packet is truncated */
			break;

		/* Accept this packet as fully legit. */
		hdrpos = bufpos;
		hdr = successor_hdr;
		}

	/* Success!  Last valid packet is at hdrpos. */
	*last_timestamp = hdr.ts;
	status = 1;

	/* Seek so that the next read will start at last valid packet. */
	if ( fseek( pcap_file( p ), (long) -(bufend - hdrpos), 2 ) < 0 )
		error( "final fseek() failed in sf_find_end()" );

    done:
	free( (char *) buf );

	return status;
	}


/* Takes two timeval's and returns the difference, tv2 - tv1, as a double. */

static double
timeval_diff( struct timeval *tv1, struct timeval *tv2 )
	{
	double result = (tv2->tv_sec - tv1->tv_sec);
	result += (tv2->tv_usec - tv1->tv_usec) / 1000000.0;

	return result;
	}


/* Returns true if timestamp t1 is chronologically less than timestamp t2. */

int
sf_timestamp_less_than( struct timeval *t1, struct timeval *t2 )
	{
	return t1->tv_sec < t2->tv_sec ||
	       (t1->tv_sec == t2->tv_sec &&
	        t1->tv_usec < t2->tv_usec);
	}


/* Given two timestamps on either side of desired_time and their positions,
 * returns the interpolated position of the desired_time packet.  Returns a
 * negative value if the desired_time is outside the given range.
 */

static long
interpolated_position( struct timeval *min_time, long min_pos,
			struct timeval *max_time, long max_pos,
			struct timeval *desired_time )
	{
	double full_span = timeval_diff( max_time, min_time );
	double desired_span = timeval_diff( desired_time, min_time );
	long full_span_pos = max_pos - min_pos;
	double fractional_offset = desired_span / full_span;

	if ( fractional_offset < 0.0 || fractional_offset > 1.0 )
		return -1;

	return min_pos + (long) (fractional_offset * (double) full_span_pos);
	}


/* Reads packets linearly until one with a time >= the given desired time
 * is found; positions the dump file so that the next read will start
 * at the given packet.  Returns non-zero on success, 0 if an EOF was
 * first encountered.
 */

static int
read_up_to( pcap_t *p, struct timeval *desired_time )
	{
	struct pcap_pkthdr hdr;
	const u_char *buf;
	long pos;
	int status;

	for ( ; ; )
		{
		struct timeval *timestamp;

		pos = ftell( pcap_file( p ) );
		buf = pcap_next( p, &hdr );

		if ( buf == 0 )
			{
			if ( feof( pcap_file( p ) ) )
				{
				status = 0;
				clearerr( pcap_file( p ) );
				break;
				}

			error( "bad status in read_up_to()" );
			}

		timestamp = &hdr.ts;

		if ( ! sf_timestamp_less_than( timestamp, desired_time ) )
			{
			status = 1;
			break;
			}
		}

	if ( fseek( pcap_file( p ), pos, 0 ) < 0 )
		error( "fseek() failed in read_up_to()" );

	return (status);
	}


/* Positions the sf_readfile stream so that the next sf_read() will
 * return the first packet with a time greater than or equal to
 * desired_time.  desired_time must be greater than min_time and less
 * than max_time, which should correspond to actual packets in the
 * file.  min_pos is the file position (byte offset) corresponding to
 * the min_time packet and max_pos is the same for the max_time packet.
 *
 * Returns non-zero on success, 0 if the given position is beyond max_pos.
 *
 * NOTE: when calling this routine, the sf_readfile stream *must* be
 * already aligned so that the next call to sf_next_packet() will yield
 * a valid packet.
 */

int
sf_find_packet( pcap_t *p,
		struct timeval *min_time, long min_pos,
		struct timeval *max_time, long max_pos,
		struct timeval *desired_time )
	{
	int status = 1;
	struct timeval min_time_copy, max_time_copy;
	u_int num_bytes = MAX_BYTES_FOR_DEFINITE_HEADER;
	int num_bytes_read;
	long desired_pos, present_pos;
	u_char *buf, *hdrpos;
	struct pcap_pkthdr hdr;

	buf = (u_char *) malloc( num_bytes );
	if ( ! buf )
		error( "malloc() failured in sf_find_packet()" );

	min_time_copy = *min_time;
	min_time = &min_time_copy;

	max_time_copy = *max_time;
	max_time = &max_time_copy;

	for ( ; ; )	/* loop until positioned correctly */
		{
		desired_pos =
			interpolated_position( min_time, min_pos,
					       max_time, max_pos,
					       desired_time );

		if ( desired_pos < 0 )
			{
			status = 0;
			break;
			}

		present_pos = ftell( pcap_file( p ) );

		if ( present_pos <= desired_pos &&
		     desired_pos - present_pos < STRAIGHT_SCAN_THRESHOLD )
			{ /* we're close enough to just blindly read ahead */
			status = read_up_to( p, desired_time );
			break;
			}

		/* Undershoot the target a little bit - it's much easier to
		 * then scan straight forward than to try to read backwards ...
		 */
		desired_pos -= STRAIGHT_SCAN_THRESHOLD / 2;
		if ( desired_pos < min_pos )
			desired_pos = min_pos;

		if ( fseek( pcap_file( p ), desired_pos, 0 ) < 0 )
			error( "fseek() failed in sf_find_packet()" );

		num_bytes_read =
			fread( (char *) buf, 1, num_bytes, pcap_file( p ) );

		if ( num_bytes_read == 0 )
			/* This shouldn't ever happen because we try to
			 * undershoot, unless the dump file has only a
			 * couple packets in it ...
			 */
			error( "fread() failed in sf_find_packet()" );

		if ( find_header( p, buf, num_bytes, min_time->tv_sec,
				  max_time->tv_sec, &hdrpos, &hdr ) !=
		     HEADER_DEFINITELY )
			error( "can't find header at position %ld in dump file",
				desired_pos );

		/* Correct desired_pos to reflect beginning of packet. */
		desired_pos += (hdrpos - buf);

		/* Seek to the beginning of the header. */
		if ( fseek( pcap_file( p ), desired_pos, 0 ) < 0 )
			error( "fseek() failed in sf_find_packet()" );

		if ( sf_timestamp_less_than( &hdr.ts, desired_time ) )
			{ /* too early in the file */
			*min_time = hdr.ts;
			min_pos = desired_pos;
			}

		else if ( sf_timestamp_less_than( desired_time, &hdr.ts ) )
			{ /* too late in the file */
			*max_time = hdr.ts;
			max_pos = desired_pos;
			}

		else
			/* got it! */
			break;
		}

	free( (char *) buf );

	return status;
	}
