/*
 * linux/fs/nfsd/stats.c
 *
 * procfs-based user access to knfsd statistics
 *
 * /proc/net/rpc/nfsd
 *
 * Format:
 *	rc <hits> <misses> <nocache>
 *			Statistsics for the reply cache
 *	fh <stale> <total-lookups> <anonlookups> <dir-not-in-dcache> <nondir-not-in-dcache>
 *			statistics for filehandle lookup
 *	io <bytes-read> <bytes-writtten>
 *			statistics for IO throughput
 *	th <threads> <fullcnt> <10%-20%> <20%-30%> ... <90%-100%> <100%> 
 *			time (seconds) when nfsd thread usage above thresholds
 *			and number of times that all threads were in use
 *	ra cache-size  <10%  <20%  <30% ... <100% not-found
 *			number of times that read-ahead entry was found that deep in
 *			the cache.
 *	plus generic RPC stats (see net/sunrpc/stats.c)
 *
 * Copyright (C) 1995, 1996, 1997 Olaf Kirch <okir@monad.swb.de>
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/stat.h>
#define __NO_VERSION__
#include <linux/module.h>

#include <linux/sunrpc/svc.h>
#include <linux/sunrpc/stats.h>
#include <linux/nfsd/nfsd.h>
#include <linux/nfsd/stats.h>

struct nfsd_stats	nfsdstats;
struct svc_stat		nfsd_svcstats = { &nfsd_program, };

static int
nfsd_proc_read(char *buffer, char **start, off_t offset, int count,
				int *eof, void *data)
{
	int	len;
	int	i;

	len = sprintf(buffer, "rc %u %u %u\nfh %u %u %u %u %u\nio %u %u\n",
		      nfsdstats.rchits,
		      nfsdstats.rcmisses,
		      nfsdstats.rcnocache,
		      nfsdstats.fh_stale,
		      nfsdstats.fh_lookup,
		      nfsdstats.fh_anon,
		      nfsdstats.fh_nocache_dir,
		      nfsdstats.fh_nocache_nondir,
		      nfsdstats.io_read,
		      nfsdstats.io_write);
	/* thread usage: */
	len += sprintf(buffer+len, "th %u %u", nfsdstats.th_cnt, nfsdstats.th_fullcnt);
	for (i=0; i<10; i++) {
		unsigned int jifs = nfsdstats.th_usage[i];
		unsigned int sec = jifs / HZ, msec = (jifs % HZ)*1000/HZ;
		len += sprintf(buffer+len, " %u.%03u", sec, msec);
	}

	/* newline and ra-cache */
	len += sprintf(buffer+len, "\nra %u", nfsdstats.ra_size);
	for (i=0; i<11; i++)
		len += sprintf(buffer+len, " %u", nfsdstats.ra_depth[i]);
	len += sprintf(buffer+len, "\n");
	

	/* Assume we haven't hit EOF yet. Will be set by svc_proc_read. */
	*eof = 0;

	/*
	 * Append generic nfsd RPC statistics if there's room for it.
	 */
	if (len <= offset) {
		len = svc_proc_read(buffer, start, offset - len, count,
				    eof, data);
		return len;
	}

	if (len < count) {
		len += svc_proc_read(buffer + len, start, 0, count - len,
				     eof, data);
	}

	if (offset >= len) {
		*start = buffer;
		return 0;
	}

	*start = buffer + offset;
	if ((len -= offset) > count)
		return count;
	return len;
}

void
nfsd_stat_init(void)
{
	struct proc_dir_entry	*ent;

	if ((ent = svc_proc_register(&nfsd_svcstats)) != 0) {
		ent->read_proc = nfsd_proc_read;
		ent->owner = THIS_MODULE;
	}
}

void
nfsd_stat_shutdown(void)
{
	svc_proc_unregister("nfsd");
}
