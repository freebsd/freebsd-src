/*-
 * Copyright (c) 2004, 2008, 2009 Silicon Graphics International Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 * $Id: //depot/users/kenm/FreeBSD-test2/usr.bin/ctlstat/ctlstat.c#4 $
 */
/*
 * CAM Target Layer statistics program
 *
 * Authors: Ken Merry <ken@FreeBSD.org>, Will Andrews <will@FreeBSD.org>
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/sysctl.h>
#include <sys/resource.h>
#include <sys/queue.h>
#include <sys/callout.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <ctype.h>
#include <bitstring.h>
#include <cam/scsi/scsi_all.h>
#include <cam/ctl/ctl.h>
#include <cam/ctl/ctl_io.h>
#include <cam/ctl/ctl_scsi_all.h>
#include <cam/ctl/ctl_util.h>
#include <cam/ctl/ctl_backend.h>
#include <cam/ctl/ctl_ioctl.h>

/*
 * The default amount of space we allocate for LUN storage space.  We
 * dynamically allocate more if needed.
 */
#define	CTL_STAT_NUM_LUNS	30

/*
 * The default number of LUN selection bits we allocate.  This is large
 * because we don't currently increase it if the user specifies a LUN
 * number of 1024 or larger.
 */
#define	CTL_STAT_LUN_BITS	1024L

static const char *ctlstat_opts = "Cc:Ddhjl:n:tw:";
static const char *ctlstat_usage = "Usage:  ctlstat [-CDdjht] [-l lunnum]"
				   "[-c count] [-n numdevs] [-w wait]\n";

struct ctl_cpu_stats {
	uint64_t user;
	uint64_t nice;
	uint64_t system;
	uint64_t intr;
	uint64_t idle;
};

typedef enum {
	CTLSTAT_MODE_STANDARD,
	CTLSTAT_MODE_DUMP,
	CTLSTAT_MODE_JSON,
} ctlstat_mode_types;

#define	CTLSTAT_FLAG_CPU		(1 << 0)
#define	CTLSTAT_FLAG_HEADER		(1 << 1)
#define	CTLSTAT_FLAG_FIRST_RUN		(1 << 2)
#define	CTLSTAT_FLAG_TOTALS		(1 << 3)
#define	CTLSTAT_FLAG_DMA_TIME		(1 << 4)
#define	CTLSTAT_FLAG_LUN_TIME_VALID	(1 << 5)
#define	F_CPU(ctx) ((ctx)->flags & CTLSTAT_FLAG_CPU)
#define	F_HDR(ctx) ((ctx)->flags & CTLSTAT_FLAG_HEADER)
#define	F_FIRST(ctx) ((ctx)->flags & CTLSTAT_FLAG_FIRST_RUN)
#define	F_TOTALS(ctx) ((ctx)->flags & CTLSTAT_FLAG_TOTALS)
#define	F_DMA(ctx) ((ctx)->flags & CTLSTAT_FLAG_DMA_TIME)
#define	F_LUNVAL(ctx) ((ctx)->flags & CTLSTAT_FLAG_LUN_TIME_VALID)

struct ctlstat_context {
	ctlstat_mode_types mode;
	int flags;
	struct ctl_lun_io_stats *cur_lun_stats, *prev_lun_stats,
		*tmp_lun_stats;
	struct ctl_lun_io_stats cur_total_stats[3], prev_total_stats[3];
	struct timespec cur_time, prev_time;
	struct ctl_cpu_stats cur_cpu, prev_cpu;
	uint64_t cur_total_jiffies, prev_total_jiffies;
	uint64_t cur_idle, prev_idle;
	bitstr_t bit_decl(lun_mask, CTL_STAT_LUN_BITS);
	int num_luns;
	int numdevs;
	int header_interval;
};

#ifndef min
#define	min(x,y)	(((x) < (y)) ? (x) : (y))
#endif

static void usage(int error);
static int getstats(int fd, int *num_luns, struct ctl_lun_io_stats **xlun_stats,
		    struct timespec *cur_time, int *lun_time_valid);
static int getcpu(struct ctl_cpu_stats *cpu_stats);
static void compute_stats(struct ctl_lun_io_stats *cur_stats,
			  struct ctl_lun_io_stats *prev_stats,
			  long double etime, long double *mbsec,
			  long double *kb_per_transfer,
			  long double *transfers_per_second,
			  long double *ms_per_transfer,
			  long double *ms_per_dma,
			  long double *dmas_per_second);

static void
usage(int error)
{
	fputs(ctlstat_usage, error ? stderr : stdout);
}

static int
getstats(int fd, int *num_luns, struct ctl_lun_io_stats **xlun_stats,
	 struct timespec *cur_time, int *flags)
{
	struct ctl_lun_io_stats *lun_stats;
	struct ctl_stats stats;
	int more_space_count;

	more_space_count = 0;

	if (*num_luns == 0)
		*num_luns = CTL_STAT_NUM_LUNS;

	lun_stats = *xlun_stats;
retry:

	if (lun_stats == NULL) {
		lun_stats = (struct ctl_lun_io_stats *)malloc(
			sizeof(*lun_stats) * *num_luns);
	}

	memset(&stats, 0, sizeof(stats));
	stats.alloc_len = *num_luns * sizeof(*lun_stats);
	memset(lun_stats, 0, stats.alloc_len);
	stats.lun_stats = lun_stats;

	if (ioctl(fd, CTL_GETSTATS, &stats) == -1)
		err(1, "error returned from CTL_GETSTATS ioctl");

	switch (stats.status) {
	case CTL_SS_OK:
		break;
	case CTL_SS_ERROR:
		err(1, "CTL_SS_ERROR returned from CTL_GETSTATS ioctl");
		break;
	case CTL_SS_NEED_MORE_SPACE:
		if (more_space_count > 0) {
			errx(1, "CTL_GETSTATS returned NEED_MORE_SPACE again");
		}
		*num_luns = stats.num_luns;
		free(lun_stats);
		lun_stats = NULL;
		more_space_count++;
		goto retry;
		break; /* NOTREACHED */
	default:
		errx(1, "unknown status %d returned from CTL_GETSTATS ioctl",
		     stats.status);
		break;
	}

	*xlun_stats = lun_stats;
	*num_luns = stats.num_luns;
	cur_time->tv_sec = stats.timestamp.tv_sec;
	cur_time->tv_nsec = stats.timestamp.tv_nsec;
	if (stats.flags & CTL_STATS_FLAG_TIME_VALID)
		*flags |= CTLSTAT_FLAG_LUN_TIME_VALID;
	else
		*flags &= ~CTLSTAT_FLAG_LUN_TIME_VALID;

	return (0);
}

static int
getcpu(struct ctl_cpu_stats *cpu_stats)
{
	long cp_time[CPUSTATES];
	size_t cplen;

	cplen = sizeof(cp_time);

	if (sysctlbyname("kern.cp_time", &cp_time, &cplen, NULL, 0) == -1) {
		warn("sysctlbyname(kern.cp_time...) failed");
		return (1);
	}

	cpu_stats->user = cp_time[CP_USER];
	cpu_stats->nice = cp_time[CP_NICE];
	cpu_stats->system = cp_time[CP_SYS];
	cpu_stats->intr = cp_time[CP_INTR];
	cpu_stats->idle = cp_time[CP_IDLE];

	return (0);
}

static void
compute_stats(struct ctl_lun_io_stats *cur_stats,
	      struct ctl_lun_io_stats *prev_stats, long double etime,
	      long double *mbsec, long double *kb_per_transfer,
	      long double *transfers_per_second, long double *ms_per_transfer,
	      long double *ms_per_dma, long double *dmas_per_second)
{
	uint64_t total_bytes = 0, total_operations = 0, total_dmas = 0;
	uint32_t port;
	struct bintime total_time_bt, total_dma_bt;
	struct timespec total_time_ts, total_dma_ts;
	int i;

	bzero(&total_time_bt, sizeof(total_time_bt));
	bzero(&total_dma_bt, sizeof(total_dma_bt));
	bzero(&total_time_ts, sizeof(total_time_ts));
	bzero(&total_dma_ts, sizeof(total_dma_ts));
	for (port = 0; port < CTL_MAX_PORTS; port++) {
		for (i = 0; i < CTL_STATS_NUM_TYPES; i++) {
			total_bytes += cur_stats->ports[port].bytes[i];
			total_operations +=
			    cur_stats->ports[port].operations[i];
			total_dmas += cur_stats->ports[port].num_dmas[i];
			bintime_add(&total_time_bt,
			    &cur_stats->ports[port].time[i]);
			bintime_add(&total_dma_bt,
			    &cur_stats->ports[port].dma_time[i]);
			if (prev_stats != NULL) {
				total_bytes -=
				    prev_stats->ports[port].bytes[i];
				total_operations -=
				    prev_stats->ports[port].operations[i];
				total_dmas -=
				    prev_stats->ports[port].num_dmas[i];
				bintime_sub(&total_time_bt,
				    &prev_stats->ports[port].time[i]);
				bintime_sub(&total_dma_bt,
				    &prev_stats->ports[port].dma_time[i]);
			}
		}
	}

	*mbsec = total_bytes;
	*mbsec /= 1024 * 1024;
	if (etime > 0.0)
		*mbsec /= etime;
	else
		*mbsec = 0;
	*kb_per_transfer = total_bytes;
	*kb_per_transfer /= 1024;
	if (total_operations > 0)
		*kb_per_transfer /= total_operations;
	else
		*kb_per_transfer = 0;
	*transfers_per_second = total_operations;
	*dmas_per_second = total_dmas;
	if (etime > 0.0) {
		*transfers_per_second /= etime;
		*dmas_per_second /= etime;
	} else {
		*transfers_per_second = 0;
		*dmas_per_second = 0;
	}

	bintime2timespec(&total_time_bt, &total_time_ts);
	bintime2timespec(&total_dma_bt, &total_dma_ts);
	if (total_operations > 0) {
		/*
		 * Convert the timespec to milliseconds.
		 */
		*ms_per_transfer = total_time_ts.tv_sec * 1000;
		*ms_per_transfer += total_time_ts.tv_nsec / 1000000;
		*ms_per_transfer /= total_operations;
	} else
		*ms_per_transfer = 0;

	if (total_dmas > 0) {
		/*
		 * Convert the timespec to milliseconds.
		 */
		*ms_per_dma = total_dma_ts.tv_sec * 1000;
		*ms_per_dma += total_dma_ts.tv_nsec / 1000000;
		*ms_per_dma /= total_dmas;
	} else
		*ms_per_dma = 0;
}

/* The dump_stats() and json_stats() functions perform essentially the same
 * purpose, but dump the statistics in different formats.  JSON is more
 * conducive to programming, however.
 */

#define	PRINT_BINTIME(prefix, bt) \
	printf("%s %jd s %ju frac\n", prefix, (intmax_t)(bt).sec, \
	       (uintmax_t)(bt).frac)
static const char *iotypes[] = {"NO IO", "READ", "WRITE"};

static void
ctlstat_dump(struct ctlstat_context *ctx) {
	int iotype, lun, port;
	struct ctl_lun_io_stats *stats = ctx->cur_lun_stats;

	for (lun = 0; lun < ctx->num_luns;lun++) {
		printf("lun %d\n", lun);
		for (port = 0; port < CTL_MAX_PORTS; port++) {
			printf(" port %d\n",
			    stats[lun].ports[port].targ_port);
			for (iotype = 0; iotype < CTL_STATS_NUM_TYPES;
			    iotype++) {
				printf("  io type %d (%s)\n", iotype,
				    iotypes[iotype]);
				printf("   bytes %ju\n", (uintmax_t)
				    stats[lun].ports[port].bytes[iotype]);
				printf("   operations %ju\n", (uintmax_t)
				    stats[lun].ports[port].operations[iotype]);
				PRINT_BINTIME("   io time",
				    stats[lun].ports[port].time[iotype]);
				printf("   num dmas %ju\n", (uintmax_t)
				    stats[lun].ports[port].num_dmas[iotype]);
				PRINT_BINTIME("   dma time",
				    stats[lun].ports[port].dma_time[iotype]);
			}
		}
	}
}

#define	JSON_BINTIME(prefix, bt) \
	printf("\"%s\":{\"sec\":%jd,\"frac\":%ju},", \
	    prefix, (intmax_t)(bt).sec, (uintmax_t)(bt).frac)

static void
ctlstat_json(struct ctlstat_context *ctx) {
	int iotype, lun, port;
	struct ctl_lun_io_stats *stats = ctx->cur_lun_stats;

	printf("{\"luns\":[");
	for (lun = 0; lun < ctx->num_luns; lun++) {
		printf("{\"ports\":[");
		for (port = 0; port < CTL_MAX_PORTS;port++) {
			printf("{\"num\":%d,\"io\":[",
			    stats[lun].ports[port].targ_port);
			for (iotype = 0; iotype < CTL_STATS_NUM_TYPES;
			    iotype++) {
				printf("{\"type\":\"%s\",", iotypes[iotype]);
				printf("\"bytes\":%ju,", (uintmax_t)stats[
				       lun].ports[port].bytes[iotype]);
				printf("\"operations\":%ju,", (uintmax_t)stats[
				       lun].ports[port].operations[iotype]);
				JSON_BINTIME("io time",
				    stats[lun].ports[port].time[iotype]);
				JSON_BINTIME("dma time",
				    stats[lun].ports[port].dma_time[iotype]);
				printf("\"num dmas\":%ju}", (uintmax_t)
				    stats[lun].ports[port].num_dmas[iotype]);
				if (iotype < (CTL_STATS_NUM_TYPES - 1))
					printf(","); /* continue io array */
			}
			printf("]}"); /* close port */
			if (port < (CTL_MAX_PORTS - 1))
				printf(","); /* continue port array */
		}
		printf("]}"); /* close lun */
		if (lun < (ctx->num_luns - 1))
			printf(","); /* continue lun array */
	}
	printf("]}"); /* close luns and toplevel */
}

static void
ctlstat_standard(struct ctlstat_context *ctx) {
	long double etime;
	uint64_t delta_jiffies, delta_idle;
	uint32_t port;
	long double cpu_percentage;
	int i;
	int j;

	cpu_percentage = 0;

	if (F_CPU(ctx) && (getcpu(&ctx->cur_cpu) != 0))
		errx(1, "error returned from getcpu()");

	etime = ctx->cur_time.tv_sec - ctx->prev_time.tv_sec +                  
	    (ctx->prev_time.tv_nsec - ctx->cur_time.tv_nsec) * 1e-9; 

	if (F_CPU(ctx)) {
		ctx->prev_total_jiffies = ctx->cur_total_jiffies;
		ctx->cur_total_jiffies = ctx->cur_cpu.user +
		    ctx->cur_cpu.nice + ctx->cur_cpu.system +
		    ctx->cur_cpu.intr + ctx->cur_cpu.idle;
		delta_jiffies = ctx->cur_total_jiffies;
		if (F_FIRST(ctx) == 0)
			delta_jiffies -= ctx->prev_total_jiffies;
		ctx->prev_idle = ctx->cur_idle;
		ctx->cur_idle = ctx->cur_cpu.idle;
		delta_idle = ctx->cur_idle - ctx->prev_idle;

		cpu_percentage = delta_jiffies - delta_idle;
		cpu_percentage /= delta_jiffies;
		cpu_percentage *= 100;
	}

	if (F_HDR(ctx)) {
		ctx->header_interval--;
		if (ctx->header_interval <= 0) {
			int hdr_devs;

			hdr_devs = 0;

			if (F_TOTALS(ctx)) {
				fprintf(stdout, "%s     System Read     %s"
					"System Write     %sSystem Total%s\n",
					(F_LUNVAL(ctx) != 0) ? "     " : "",
					(F_LUNVAL(ctx) != 0) ? "     " : "",
					(F_LUNVAL(ctx) != 0) ? "     " : "",
					(F_CPU(ctx))   ? "    CPU" : "");
				hdr_devs = 3;
			} else {
				if (F_CPU(ctx))
					fprintf(stdout, "  CPU  ");
				for (i = 0; i < min(CTL_STAT_LUN_BITS,
				     ctx->num_luns); i++) {
					int lun;

					/*
					 * Obviously this won't work with
					 * LUN numbers greater than a signed
					 * integer.
					 */
					lun = (int)ctx->cur_lun_stats[i
						].lun_number;

					if (bit_test(ctx->lun_mask, lun) == 0)
						continue;
					fprintf(stdout, "%15.6s%d %s",
					    "lun", lun,
					    (F_LUNVAL(ctx) != 0) ? "     " : "");
					hdr_devs++;
				}
				fprintf(stdout, "\n");
			}
			for (i = 0; i < hdr_devs; i++)
				fprintf(stdout, "%s  %sKB/t %s  MB/s ",
					((F_CPU(ctx) != 0) && (i == 0) &&
					(F_TOTALS(ctx) == 0)) ?  "       " : "",
					(F_LUNVAL(ctx) != 0) ? " ms  " : "",
					(F_DMA(ctx) == 0) ? "tps" : "dps");
			fprintf(stdout, "\n");
			ctx->header_interval = 20;
		}
	}

	if (F_TOTALS(ctx) != 0) {
		long double mbsec[3];
		long double kb_per_transfer[3];
		long double transfers_per_sec[3];
		long double ms_per_transfer[3];
		long double ms_per_dma[3];
		long double dmas_per_sec[3];

		for (i = 0; i < 3; i++) 
			ctx->prev_total_stats[i] = ctx->cur_total_stats[i];

		memset(&ctx->cur_total_stats, 0, sizeof(ctx->cur_total_stats));

		/* Use macros to make the next loop more readable. */
#define	ADD_STATS_BYTES(st, p, i, j) \
	ctx->cur_total_stats[st].ports[p].bytes[j] += \
	    ctx->cur_lun_stats[i].ports[p].bytes[j]
#define	ADD_STATS_OPERATIONS(st, p, i, j) \
	ctx->cur_total_stats[st].ports[p].operations[j] += \
	    ctx->cur_lun_stats[i].ports[p].operations[j]
#define	ADD_STATS_NUM_DMAS(st, p, i, j) \
	ctx->cur_total_stats[st].ports[p].num_dmas[j] += \
	    ctx->cur_lun_stats[i].ports[p].num_dmas[j]
#define	ADD_STATS_TIME(st, p, i, j) \
	bintime_add(&ctx->cur_total_stats[st].ports[p].time[j], \
	    &ctx->cur_lun_stats[i].ports[p].time[j])
#define	ADD_STATS_DMA_TIME(st, p, i, j) \
	bintime_add(&ctx->cur_total_stats[st].ports[p].dma_time[j], \
	    &ctx->cur_lun_stats[i].ports[p].dma_time[j])

		for (i = 0; i < ctx->num_luns; i++) {
			for (port = 0; port < CTL_MAX_PORTS; port++) {
				for (j = 0; j < CTL_STATS_NUM_TYPES; j++) {
					ADD_STATS_BYTES(2, port, i, j);
					ADD_STATS_OPERATIONS(2, port, i, j);
					ADD_STATS_NUM_DMAS(2, port, i, j);
					ADD_STATS_TIME(2, port, i, j);
					ADD_STATS_DMA_TIME(2, port, i, j);
				}
				ADD_STATS_BYTES(0, port, i, CTL_STATS_READ);
				ADD_STATS_OPERATIONS(0, port, i,
				    CTL_STATS_READ);
				ADD_STATS_NUM_DMAS(0, port, i, CTL_STATS_READ);
				ADD_STATS_TIME(0, port, i, CTL_STATS_READ);
				ADD_STATS_DMA_TIME(0, port, i, CTL_STATS_READ);

				ADD_STATS_BYTES(1, port, i, CTL_STATS_WRITE);
				ADD_STATS_OPERATIONS(1, port, i,
				    CTL_STATS_WRITE);
				ADD_STATS_NUM_DMAS(1, port, i, CTL_STATS_WRITE);
				ADD_STATS_TIME(1, port, i, CTL_STATS_WRITE);
				ADD_STATS_DMA_TIME(1, port, i, CTL_STATS_WRITE);
			}
		}

		for (i = 0; i < 3; i++) {
			compute_stats(&ctx->cur_total_stats[i],
				F_FIRST(ctx) ? NULL : &ctx->prev_total_stats[i],
				etime, &mbsec[i], &kb_per_transfer[i],
				&transfers_per_sec[i],
				&ms_per_transfer[i], &ms_per_dma[i],
				&dmas_per_sec[i]);
			if (F_DMA(ctx) != 0)
				fprintf(stdout, " %2.2Lf",
					ms_per_dma[i]);
			else if (F_LUNVAL(ctx) != 0)
				fprintf(stdout, " %2.2Lf",
					ms_per_transfer[i]);
			fprintf(stdout, " %5.2Lf %3.0Lf %5.2Lf ",
				kb_per_transfer[i],
				(F_DMA(ctx) == 0) ? transfers_per_sec[i] :
				dmas_per_sec[i], mbsec[i]);
		}
		if (F_CPU(ctx))
			fprintf(stdout, " %5.1Lf%%", cpu_percentage);
	} else {
		if (F_CPU(ctx))
			fprintf(stdout, "%5.1Lf%% ", cpu_percentage);

		for (i = 0; i < min(CTL_STAT_LUN_BITS, ctx->num_luns); i++) {
			long double mbsec, kb_per_transfer;
			long double transfers_per_sec;
			long double ms_per_transfer;
			long double ms_per_dma;
			long double dmas_per_sec;

			if (bit_test(ctx->lun_mask,
			    (int)ctx->cur_lun_stats[i].lun_number) == 0)
				continue;
			compute_stats(&ctx->cur_lun_stats[i], F_FIRST(ctx) ?
				NULL : &ctx->prev_lun_stats[i], etime,
				&mbsec, &kb_per_transfer,
				&transfers_per_sec, &ms_per_transfer,
				&ms_per_dma, &dmas_per_sec);
			if (F_DMA(ctx))
				fprintf(stdout, " %2.2Lf",
					ms_per_dma);
			else if (F_LUNVAL(ctx) != 0)
				fprintf(stdout, " %2.2Lf",
					ms_per_transfer);
			fprintf(stdout, " %5.2Lf %3.0Lf %5.2Lf ",
				kb_per_transfer, (F_DMA(ctx) == 0) ?
				transfers_per_sec : dmas_per_sec, mbsec);
		}
	}
}

int
main(int argc, char **argv)
{
	int c;
	int count, waittime;
	int set_lun;
	int fd, retval;
	struct ctlstat_context ctx;

	/* default values */
	retval = 0;
	waittime = 1;
	count = -1;
	memset(&ctx, 0, sizeof(ctx));
	ctx.numdevs = 3;
	ctx.mode = CTLSTAT_MODE_STANDARD;
	ctx.flags |= CTLSTAT_FLAG_CPU;
	ctx.flags |= CTLSTAT_FLAG_FIRST_RUN;
	ctx.flags |= CTLSTAT_FLAG_HEADER;

	while ((c = getopt(argc, argv, ctlstat_opts)) != -1) {
		switch (c) {
		case 'C':
			ctx.flags &= ~CTLSTAT_FLAG_CPU;
			break;
		case 'c':
			count = atoi(optarg);
			break;
		case 'd':
			ctx.flags |= CTLSTAT_FLAG_DMA_TIME;
			break;
		case 'D':
			ctx.mode = CTLSTAT_MODE_DUMP;
			waittime = 30;
			break;
		case 'h':
			ctx.flags &= ~CTLSTAT_FLAG_HEADER;
			break;
		case 'j':
			ctx.mode = CTLSTAT_MODE_JSON;
			waittime = 30;
			break;
		case 'l': {
			int cur_lun;

			cur_lun = atoi(optarg);
			if (cur_lun > CTL_STAT_LUN_BITS)
				errx(1, "Invalid LUN number %d", cur_lun);

			bit_ffs(ctx.lun_mask, CTL_STAT_LUN_BITS, &set_lun);
			if (set_lun == -1)
				ctx.numdevs = 1;
			else
				ctx.numdevs++;
			bit_set(ctx.lun_mask, cur_lun);
			break;
		}
		case 'n':
			ctx.numdevs = atoi(optarg);
			break;
		case 't':
			ctx.flags |= CTLSTAT_FLAG_TOTALS;
			ctx.numdevs = 3;
			break;
		case 'w':
			waittime = atoi(optarg);
			break;
		default:
			retval = 1;
			usage(retval);
			exit(retval);
			break;
		}
	}

	bit_ffs(ctx.lun_mask, CTL_STAT_LUN_BITS, &set_lun);

	if ((F_TOTALS(&ctx))
	 && (set_lun != -1)) {
		errx(1, "Total Mode (-t) is incompatible with individual "
		     "LUN mode (-l)");
	} else if (set_lun == -1) {
		/*
		 * Note that this just selects the first N LUNs to display,
		 * but at this point we have no knoweledge of which LUN
		 * numbers actually exist.  So we may select LUNs that
		 * aren't there.
		 */
		bit_nset(ctx.lun_mask, 0, min(ctx.numdevs - 1,
			 CTL_STAT_LUN_BITS - 1));
	}

	if ((fd = open(CTL_DEFAULT_DEV, O_RDWR)) == -1)
		err(1, "cannot open %s", CTL_DEFAULT_DEV);

	for (;count != 0;) {
		ctx.tmp_lun_stats = ctx.prev_lun_stats;
		ctx.prev_lun_stats = ctx.cur_lun_stats;
		ctx.cur_lun_stats = ctx.tmp_lun_stats;
		ctx.prev_time = ctx.cur_time;
		ctx.prev_cpu = ctx.cur_cpu;
		if (getstats(fd, &ctx.num_luns, &ctx.cur_lun_stats,
			     &ctx.cur_time, &ctx.flags) != 0)
			errx(1, "error returned from getstats()");

		switch(ctx.mode) {
		case CTLSTAT_MODE_STANDARD:
			ctlstat_standard(&ctx);
			break;
		case CTLSTAT_MODE_DUMP:
			ctlstat_dump(&ctx);
			break;
		case CTLSTAT_MODE_JSON:
			ctlstat_json(&ctx);
			break;
		default:
			break;
		}

		fprintf(stdout, "\n");
		ctx.flags &= ~CTLSTAT_FLAG_FIRST_RUN;
		if (count != 1)
			sleep(waittime);
		if (count > 0)
			count--;
	}

	exit (retval);
}

/*
 * vim: ts=8
 */
