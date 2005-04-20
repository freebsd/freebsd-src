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

#include <sys/types.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/stat.h>

#include <net/bpf.h>

#include <ctype.h>
#ifdef SOLARIS
#include <fcntl.h>
#endif
#ifdef __STDC__
#include <stdlib.h>
#endif
#include <stdio.h>
#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <string.h>
#include <unistd.h>

#include "pcap.h"
#include "version.h"


time_t	gwtm2secs( struct tm *tm );

int	sf_find_end( struct pcap *p, struct timeval *first_timestamp,
			struct timeval *last_timestamp );
int	sf_timestamp_less_than( struct timeval *t1, struct timeval *t2 );
int	sf_find_packet( struct pcap *p,
		struct timeval *min_time, long min_pos,
		struct timeval *max_time, long max_pos,
		struct timeval *desired_time );

void	error(const char *fmt, ...);
