/*
 * Copyright (c) 1997, 1998 Kenneth D. Merry.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/lib/libdevstat/devstat.h,v 1.3 1999/08/28 00:04:27 peter Exp $
 */

#ifndef _DEVSTAT_H
#define _DEVSTAT_H
#include <sys/cdefs.h>
#include <sys/devicestat.h>

#define DEVSTAT_ERRBUF_SIZE  2048 /* size of the devstat library error string */

extern char devstat_errbuf[];

typedef enum {
	DEVSTAT_MATCH_NONE	= 0x00,
	DEVSTAT_MATCH_TYPE	= 0x01,
	DEVSTAT_MATCH_IF	= 0x02,
	DEVSTAT_MATCH_PASS	= 0x04
} devstat_match_flags;

struct devstat_match {
	devstat_match_flags	match_fields;
	devstat_type_flags	device_type;
	int			num_match_categories;
};

struct devstat_match_table {
	char 			*match_str;
	devstat_type_flags	type;
	devstat_match_flags	match_field;
};

struct device_selection {
	u_int32_t	device_number;
	char		device_name[DEVSTAT_NAME_LEN];
	int		unit_number;
	int		selected;
	u_int64_t	bytes;
	int		position;
};

struct devinfo {
	struct devstat	*devices;
	u_int8_t	*mem_ptr;
	long		generation;
	int		numdevs;
};

struct statinfo {
	long		cp_time[CPUSTATES];
	long		tk_nin;
	long		tk_nout;
	struct devinfo	*dinfo;
	struct timeval	busy_time;
};

typedef enum {
	DS_SELECT_ADD,
	DS_SELECT_ONLY,
	DS_SELECT_REMOVE,
	DS_SELECT_ADDONLY
} devstat_select_mode;

__BEGIN_DECLS
int getnumdevs(void);
long getgeneration(void);
int getversion(void);
int checkversion(void);
int getdevs(struct statinfo *stats);
int selectdevs(struct device_selection **dev_select, int *num_selected,
	       int *num_selections, long *select_generation, 
	       long current_generation, struct devstat *devices, int numdevs,
	       struct devstat_match *matches, int num_matches,
	       char **dev_selections, int num_dev_selections,
	       devstat_select_mode select_mode, int maxshowdevs,
	       int perf_select);
int buildmatch(char *match_str, struct devstat_match **matches,
	       int *num_matches);
int compute_stats(struct devstat *current, struct devstat *previous,
		  long double etime, u_int64_t *total_bytes,
		  u_int64_t *total_transfers, u_int64_t *total_blocks,
		  long double *kb_per_transfer,
		  long double *transfers_per_second, long double *mb_per_second,
		  long double *blocks_per_second,
		  long double *ms_per_transaction);
long double compute_etime(struct timeval cur_time, struct timeval prev_time);
__END_DECLS

#endif /* _DEVSTAT_H  */
