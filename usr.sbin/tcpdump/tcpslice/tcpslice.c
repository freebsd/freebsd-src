/*
 * Copyright (c) 1987-1990 The Regents of the University of California.
 * All rights reserved.
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

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1987-1990\n\
	The Regents of the University of California. All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static const char rcsid[] =
	"$Id$";
#endif /* not lint */

/*
 * tcpslice - extract pieces of and/or glue together tcpdump files
 */

#include <err.h>
#include "tcpslice.h"

int tflag = 0;	/* global that util routines are sensitive to */
int fddipad;	/* XXX: libpcap needs this global */

/* Style in which to print timestamps; RAW is "secs.usecs"; READABLE is
 * ala the Unix "date" tool; and PARSEABLE is tcpslice's custom format,
 * designed to be easy to parse.  The default is RAW.
 */
enum stamp_styles { TIMESTAMP_RAW, TIMESTAMP_READABLE, TIMESTAMP_PARSEABLE };
enum stamp_styles timestamp_style = TIMESTAMP_RAW;

#ifndef __FreeBSD__
extern int getopt( int argc, char **argv, char *optstring );
#endif

int is_timestamp( char *str );
long local_time_zone(long timestamp);
struct timeval parse_time(char *time_string, struct timeval base_time);
void fill_tm(char *time_string, int is_delta, struct tm *t, time_t *usecs_addr);
void get_file_range( char filename[], pcap_t **p,
			struct timeval *first_time, struct timeval *last_time );
struct timeval first_packet_time(char filename[], pcap_t **p_addr);
void extract_slice(char filename[], char write_file_name[],
			struct timeval *start_time, struct timeval *stop_time);
char *timestamp_to_string(struct timeval *timestamp);
void dump_times(pcap_t **p, char filename[]);
static void usage(void);


pcap_dumper_t *dumper = 0;

int
main(int argc, char **argv)
{
	int op;
	int dump_flag = 0;
	int report_times = 0;
	char *start_time_string = 0;
	char *stop_time_string = 0;
	char *write_file_name = "-";	/* default is stdout */
	struct timeval first_time, start_time, stop_time;
	pcap_t *pcap;

	opterr = 0;
	while ((op = getopt(argc, argv, "dRrtw:")) != -1)
		switch (op) {

		case 'd':
			dump_flag = 1;
			break;

		case 'R':
			++report_times;
			timestamp_style = TIMESTAMP_RAW;
			break;

		case 'r':
			++report_times;
			timestamp_style = TIMESTAMP_READABLE;
			break;

		case 't':
			++report_times;
			timestamp_style = TIMESTAMP_PARSEABLE;
			break;

		case 'w':
 			write_file_name = optarg;
 			break;

		default:
			usage();
			/* NOTREACHED */
		}

	if ( report_times > 1 )
		error( "only one of -R, -r, or -t can be specified" );


	if (optind < argc)
		/* See if the next argument looks like a possible
		 * start time, and if so assume it is one.
		 */
		if (isdigit(argv[optind][0]) || argv[optind][0] == '+')
			start_time_string = argv[optind++];

	if (optind < argc)
		if (isdigit(argv[optind][0]) || argv[optind][0] == '+')
			stop_time_string = argv[optind++];


	if (optind >= argc)
		error("at least one input file must be given");


	first_time = first_packet_time(argv[optind], &pcap);
	pcap_close(pcap);


	if (start_time_string)
		start_time = parse_time(start_time_string, first_time);
	else
		start_time = first_time;

	if (stop_time_string)
		stop_time = parse_time(stop_time_string, start_time);

	else
		{
		stop_time = start_time;
		stop_time.tv_sec += 86400*3660;	/* + 10 years; "forever" */
		}


	if (report_times) {
		for (; optind < argc; ++optind)
			dump_times(&pcap, argv[optind]);
	}

	if (dump_flag) {
		printf( "start\t%s\nstop\t%s\n",
			timestamp_to_string( &start_time ),
			timestamp_to_string( &stop_time ) );
	}

	if (! report_times && ! dump_flag) {
		if ( ! strcmp( write_file_name, "-" ) &&
		     isatty( fileno(stdout) ) )
			error("stdout is a terminal; redirect or use -w");

		for (; optind < argc; ++optind)
			extract_slice(argv[optind], write_file_name,
					&start_time, &stop_time);
	}

	return 0;
}


/* Returns non-zero if a string matches the format for a timestamp,
 * 0 otherwise.
 */
int is_timestamp( char *str )
	{
	while ( isdigit(*str) || *str == '.' )
		++str;

	return *str == '\0';
	}


/* Return the correction in seconds for the local time zone with respect
 * to Greenwich time.
 */
long local_time_zone(long timestamp)
{
	struct timeval now;
	struct timezone tz;
	long localzone;

	if (gettimeofday(&now, &tz) < 0)
		err(1, "gettimeofday");
	localzone = tz.tz_minuteswest * -60;

	if (localtime((time_t *) &timestamp)->tm_isdst)
		localzone += 3600;

	return localzone;
}

/* Given a string specifying a time (or a time offset) and a "base time"
 * from which to compute offsets and fill in defaults, returns a timeval
 * containing the specified time.
 */

struct timeval
parse_time(char *time_string, struct timeval base_time)
{
	struct tm *bt = localtime((time_t *) &base_time.tv_sec);
	struct tm t;
	struct timeval result;
	time_t usecs = 0;
	int is_delta = (time_string[0] == '+');

	if ( is_delta )
		++time_string;	/* skip over '+' sign */

	if ( is_timestamp( time_string ) )
		{ /* interpret as a raw timestamp or timestamp offset */
		char *time_ptr;

		result.tv_sec = atoi( time_string );
		time_ptr = strchr( time_string, '.' );

		if ( time_ptr )
			{ /* microseconds are specified, too */
			int num_digits = strlen( time_ptr + 1 );
			result.tv_usec = atoi( time_ptr + 1 );

			/* turn 123.456 into 123 seconds plus 456000 usec */
			while ( num_digits++ < 6 )
				result.tv_usec *= 10;
			}

		else
			result.tv_usec = 0;

		if ( is_delta )
			{
			result.tv_sec += base_time.tv_sec;
			result.tv_usec += base_time.tv_usec;

			if ( result.tv_usec > 1000000 )
				{
				result.tv_usec -= 1000000;
				++result.tv_sec;
				}
			}

		return result;
		}

	if (is_delta) {
		t = *bt;
		usecs = base_time.tv_usec;
	} else {
		/* Zero struct (easy way around lack of tm_gmtoff/tm_zone
		 * under older systems) */
		bzero((char *)&t, sizeof(t));

		/* Set values to "not set" flag so we can later identify
		 * and default them.
		 */
		t.tm_sec = t.tm_min = t.tm_hour = t.tm_mday = t.tm_mon =
			t.tm_year = -1;
	}

	fill_tm(time_string, is_delta, &t, &usecs);

	/* Now until we reach a field that was specified, fill in the
	 * missing fields from the base time.
	 */
#define CHECK_FIELD(field_name) 		\
	if (t.field_name < 0) 			\
		t.field_name = bt->field_name;	\
	else					\
		break

	do {	/* bogus do-while loop so "break" in CHECK_FIELD will work */
		CHECK_FIELD(tm_year);
		CHECK_FIELD(tm_mon);
		CHECK_FIELD(tm_mday);
		CHECK_FIELD(tm_hour);
		CHECK_FIELD(tm_min);
		CHECK_FIELD(tm_sec);
	} while ( 0 );

	/* Set remaining unspecified fields to 0. */
#define ZERO_FIELD_IF_NOT_SET(field_name,zero_val)	\
	if (t.field_name < 0)				\
		t.field_name = zero_val

	if (! is_delta) {
		ZERO_FIELD_IF_NOT_SET(tm_year,90);  /* should never happen */
		ZERO_FIELD_IF_NOT_SET(tm_mon,0);
		ZERO_FIELD_IF_NOT_SET(tm_mday,1);
		ZERO_FIELD_IF_NOT_SET(tm_hour,0);
		ZERO_FIELD_IF_NOT_SET(tm_min,0);
		ZERO_FIELD_IF_NOT_SET(tm_sec,0);
	}

	result.tv_sec = gwtm2secs(&t);
	result.tv_sec -= local_time_zone(result.tv_sec);
	result.tv_usec = usecs;

	return result;
}


/* Fill in (or add to, if is_delta is true) the time values in the
 * tm struct "t" as specified by the time specified in the string
 * "time_string".  "usecs_addr" is updated with the specified number
 * of microseconds, if any.
 */
void
fill_tm(char *time_string, int is_delta, struct tm *t, time_t *usecs_addr)
{
	char *t_start, *t_stop, format_ch;
	int val;

#define SET_VAL(lhs,rhs)	\
	if (is_delta)		\
		lhs += rhs;	\
	else			\
		lhs = rhs

	/* Loop through the time string parsing one specification at
	 * a time.  Each specification has the form <number><letter>
	 * where <number> indicates the amount of time and <letter>
	 * the units.
	 */
	for (t_stop = t_start = time_string; *t_start; t_start = ++t_stop) {
		if (! isdigit(*t_start))
			error("bad date format %s, problem starting at %s",
			      time_string, t_start);

		while (isdigit(*t_stop))
			++t_stop;
		if (! t_stop)
			error("bad date format %s, problem starting at %s",
			      time_string, t_start);

		val = atoi(t_start);

		format_ch = *t_stop;
		if ( isupper( format_ch ) )
			format_ch = tolower( format_ch );

		switch (format_ch) {
			case 'y':
				if ( val > 1900 )
					val -= 1900;
				SET_VAL(t->tm_year, val);
				break;

			case 'm':
				if (strchr(t_stop+1, 'D') ||
				    strchr(t_stop+1, 'd'))
					/* it's months */
					SET_VAL(t->tm_mon, val - 1);
				else	/* it's minutes */
					SET_VAL(t->tm_min, val);
				break;

			case 'd':
				SET_VAL(t->tm_mday, val);
				break;

			case 'h':
				SET_VAL(t->tm_hour, val);
				break;

			case 's':
				SET_VAL(t->tm_sec, val);
				break;

			case 'u':
				SET_VAL(*usecs_addr, val);
				break;

			default:
				error(
				"bad date format %s, problem starting at %s",
				      time_string, t_start);
		}
	}
}


/* Return in first_time and last_time the timestamps of the first and
 * last packets in the given file.
 */
void
get_file_range( char filename[], pcap_t **p,
		struct timeval *first_time, struct timeval *last_time )
{
	*first_time = first_packet_time( filename, p );

	if ( ! sf_find_end( *p, first_time, last_time ) )
		error( "couldn't find final packet in file %s", filename );
}

int snaplen;

/* Returns the timestamp of the first packet in the given tcpdump save
 * file, which as a side-effect is initialized for further save-file
 * reading.
 */

struct timeval
first_packet_time(char filename[], pcap_t **p_addr)
{
	struct pcap_pkthdr hdr;
	pcap_t *p;
	char errbuf[PCAP_ERRBUF_SIZE];

	p = *p_addr = pcap_open_offline(filename, errbuf);
	if (! p)
		error( "bad tcpdump file %s: %s", filename, errbuf );

	snaplen = pcap_snapshot( p );

	if (pcap_next(p, &hdr) == 0)
		error( "bad status reading first packet in %s", filename );

	return hdr.ts;
}


/* Extract from the given file all packets with timestamps between
 * the two time values given (inclusive).  These packets are written
 * to the save file given by write_file_name.
 *
 * Upon return, start_time is adjusted to reflect a time just after
 * that of the last packet written to the output.
 */

void
extract_slice(char filename[], char write_file_name[],
		struct timeval *start_time, struct timeval *stop_time)
{
	long start_pos, stop_pos;
	struct timeval file_start_time, file_stop_time;
	struct pcap_pkthdr hdr;
	pcap_t *p;
	char errbuf[PCAP_ERRBUF_SIZE];

	p = pcap_open_offline(filename, errbuf);
	if (! p)
		error( "bad tcpdump file %s: %s", filename, errbuf );

	snaplen = pcap_snapshot( p );
	start_pos = ftell( pcap_file( p ) );

	if ( ! dumper )
		{
		dumper = pcap_dump_open(p, write_file_name);
		if ( ! dumper )
			error( "error creating output file %s: ",
				write_file_name, pcap_geterr( p ) );
		}

	if (pcap_next(p, &hdr) == 0)
		error( "error reading packet in %s: ",
			filename, pcap_geterr( p ) );

	file_start_time = hdr.ts;


	if ( ! sf_find_end( p, &file_start_time, &file_stop_time ) )
		error( "problems finding end packet of file %s",
			filename );

	stop_pos = ftell( pcap_file( p ) );


	/* sf_find_packet() requires that the time it's passed as its last
	 * argument be in the range [min_time, max_time], so we enforce
	 * that constraint here.
	 */
	if ( sf_timestamp_less_than( start_time, &file_start_time ) )
		*start_time = file_start_time;

	if ( sf_timestamp_less_than( &file_stop_time, start_time ) )
		return;	/* there aren't any packets of interest in the file */


	sf_find_packet( p, &file_start_time, start_pos,
			&file_stop_time, stop_pos,
			start_time );

	for ( ; ; )
		{
		struct timeval *timestamp;
		const u_char *pkt = pcap_next( p, &hdr );

		if ( pkt == 0 )
			{
#ifdef notdef
			int status;
			if ( status != SFERR_EOF )
				error( "bad status %d reading packet in %s",
					status, filename );
#endif
			break;
			}

		timestamp = &hdr.ts;

		if ( ! sf_timestamp_less_than( timestamp, start_time ) )
			{ /* packet is recent enough */
			if ( sf_timestamp_less_than( stop_time, timestamp ) )
				/* We've gone beyond the end of the region
				 * of interest ... We're done with this file.
				 */
				break;

			pcap_dump((u_char *) dumper, &hdr, pkt);

			*start_time = *timestamp;

			/* We know that each packet is guaranteed to have
			 * a unique timestamp, so we push forward the
			 * allowed minimum time to weed out duplicate
			 * packets.
			 */
			++start_time->tv_usec;
			}
		}

	pcap_close( p );
}


/* Translates a timestamp to the time format specified by the user.
 * Returns a pointer to the translation residing in a static buffer.
 * There are two such buffers, which are alternated on subseqeuent
 * calls, so two calls may be made to this routine without worrying
 * about the results of the first call being overwritten by the
 * results of the second.
 */

char *
timestamp_to_string(struct timeval *timestamp)
{
	struct tm *t;
#define NUM_BUFFERS 2
	static char buffers[NUM_BUFFERS][128];
	static int buffer_to_use = 0;
	char *buf;

	buf = buffers[buffer_to_use];
	buffer_to_use = (buffer_to_use + 1) % NUM_BUFFERS;

	switch ( timestamp_style )
	    {
	    case TIMESTAMP_RAW:
		sprintf(buf, "%ld.%ld", timestamp->tv_sec, timestamp->tv_usec);
		break;

	    case TIMESTAMP_READABLE:
		t = localtime((time_t *) &timestamp->tv_sec);
		strcpy( buf, asctime( t ) );
		buf[24] = '\0';	/* nuke final newline */
		break;

	    case TIMESTAMP_PARSEABLE:
		t = localtime((time_t *) &timestamp->tv_sec);
		sprintf( buf, "%02dy%02dm%02dd%02dh%02dm%02ds%06ldu",
			t->tm_year, t->tm_mon + 1, t->tm_mday, t->tm_hour,
			t->tm_min, t->tm_sec, timestamp->tv_usec );
		break;

	    }

	return buf;
}


/* Given a tcpdump save filename, reports on the times of the first
 * and last packets in the file.
 */

void
dump_times(pcap_t **p, char filename[])
{
	struct timeval first_time, last_time;

	get_file_range( filename, p, &first_time, &last_time );

	printf( "%s\t%s\t%s\n",
		filename,
		timestamp_to_string( &first_time ),
		timestamp_to_string( &last_time ) );
}

static void
usage(void)
{
	(void)fprintf(stderr, "tcpslice for tcpdump version %d.%d\n",
		      VERSION_MAJOR, VERSION_MINOR);
	(void)fprintf(stderr,
"usage: tcpslice [-dRrt] [-w file] [start-time [end-time]] file ... \n");

	exit(1);
}

