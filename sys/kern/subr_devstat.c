/*
 * Copyright (c) 1997, 1998, 1999 Kenneth D. Merry.
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/sysctl.h>

#include <sys/devicestat.h>

static int devstat_num_devs;
static long devstat_generation;
static int devstat_version = DEVSTAT_VERSION;
static int devstat_current_devnumber;

static struct devstatlist device_statq;

/*
 * Take a malloced and zeroed devstat structure given to us, fill it in 
 * and add it to the queue of devices.  
 */
void
devstat_add_entry(struct devstat *ds, const char *dev_name, 
		  int unit_number, u_int32_t block_size,
		  devstat_support_flags flags,
		  devstat_type_flags device_type,
		  devstat_priority priority)
{
	struct devstatlist *devstat_head;
	struct devstat *ds_tmp;

	if (ds == NULL)
		return;

	if (devstat_num_devs == 0)
		STAILQ_INIT(&device_statq);

	devstat_generation++;
	devstat_num_devs++;

	devstat_head = &device_statq;

	/*
	 * Priority sort.  Each driver passes in its priority when it adds
	 * its devstat entry.  Drivers are sorted first by priority, and
	 * then by probe order.
	 * 
	 * For the first device, we just insert it, since the priority
	 * doesn't really matter yet.  Subsequent devices are inserted into
	 * the list using the order outlined above.
	 */
	if (devstat_num_devs == 1)
		STAILQ_INSERT_TAIL(devstat_head, ds, dev_links);
	else {
		STAILQ_FOREACH(ds_tmp, devstat_head, dev_links) {
			struct devstat *ds_next;

			ds_next = STAILQ_NEXT(ds_tmp, dev_links);

			/*
			 * If we find a break between higher and lower
			 * priority items, and if this item fits in the
			 * break, insert it.  This also applies if the
			 * "lower priority item" is the end of the list.
			 */
			if ((priority <= ds_tmp->priority)
			 && ((ds_next == NULL)
			   || (priority > ds_next->priority))) {
				STAILQ_INSERT_AFTER(devstat_head, ds_tmp, ds,
						    dev_links);
				break;
			} else if (priority > ds_tmp->priority) {
				/*
				 * If this is the case, we should be able
				 * to insert ourselves at the head of the
				 * list.  If we can't, something is wrong.
				 */
				if (ds_tmp == STAILQ_FIRST(devstat_head)) {
					STAILQ_INSERT_HEAD(devstat_head,
							   ds, dev_links);
					break;
				} else {
					STAILQ_INSERT_TAIL(devstat_head,
							   ds, dev_links);
					printf("devstat_add_entry: HELP! "
					       "sorting problem detected "
					       "for %s%d\n", dev_name,
					       unit_number);
					break;
				}
			}
		}
	}

	ds->device_number = devstat_current_devnumber++;
	ds->unit_number = unit_number;
	strlcpy(ds->device_name, dev_name, DEVSTAT_NAME_LEN);
	ds->block_size = block_size;
	ds->flags = flags;
	ds->device_type = device_type;
	ds->priority = priority;
	getmicrotime(&ds->dev_creation_time);
}

/*
 * Remove a devstat structure from the list of devices.
 */
void
devstat_remove_entry(struct devstat *ds)
{
	struct devstatlist *devstat_head;

	if (ds == NULL)
		return;

	devstat_generation++;
	devstat_num_devs--;

	devstat_head = &device_statq;

	/* Remove this entry from the devstat queue */
	STAILQ_REMOVE(devstat_head, ds, devstat, dev_links);
}

/*
 * Record a transaction start.
 */
void
devstat_start_transaction(struct devstat *ds)
{
	/* sanity check */
	if (ds == NULL)
		return;

	/*
	 * We only want to set the start time when we are going from idle
	 * to busy.  The start time is really the start of the latest busy
	 * period.
	 */
	if (ds->busy_count == 0)
		getmicrouptime(&ds->start_time);
	ds->busy_count++;
}

/*
 * Record the ending of a transaction, and incrment the various counters.
 */
void
devstat_end_transaction(struct devstat *ds, u_int32_t bytes, 
			devstat_tag_type tag_type, devstat_trans_flags flags)
{
	struct timeval busy_time;

	/* sanity check */
	if (ds == NULL)
		return;

	getmicrouptime(&ds->last_comp_time);
	ds->busy_count--;

	/*
	 * There might be some transactions (DEVSTAT_NO_DATA) that don't
	 * transfer any data.
	 */
	if (flags == DEVSTAT_READ) {
		ds->bytes_read += bytes;
		ds->num_reads++;
	} else if (flags == DEVSTAT_WRITE) {
		ds->bytes_written += bytes;
		ds->num_writes++;
	} else if (flags == DEVSTAT_FREE) {
		ds->bytes_freed += bytes;
		ds->num_frees++;
	} else
		ds->num_other++;

	/*
	 * Keep a count of the various tag types sent.
	 */
	if ((ds->flags & DEVSTAT_NO_ORDERED_TAGS) == 0 &&
	    tag_type != DEVSTAT_TAG_NONE)
		ds->tag_types[tag_type]++;

	/*
	 * We only update the busy time when we go idle.  Otherwise, this
	 * calculation would require many more clock cycles.
	 */
	if (ds->busy_count == 0) {
		/* Calculate how long we were busy */
		busy_time = ds->last_comp_time;
		timevalsub(&busy_time, &ds->start_time);

		/* Add our busy time to the total busy time. */
		timevaladd(&ds->busy_time, &busy_time);
	} else if (ds->busy_count < 0)
		printf("devstat_end_transaction: HELP!! busy_count "
		       "for %s%d is < 0 (%d)!\n", ds->device_name,
		       ds->unit_number, ds->busy_count);
}

void
devstat_end_transaction_bio(struct devstat *ds, struct bio *bp)
{
	devstat_trans_flags flg;

	if (bp->bio_cmd == BIO_DELETE)
		flg = DEVSTAT_FREE;
	else if (bp->bio_cmd == BIO_READ)
		flg = DEVSTAT_READ;
	else
		flg = DEVSTAT_WRITE;

	devstat_end_transaction(ds, bp->bio_bcount - bp->bio_resid,
				DEVSTAT_TAG_SIMPLE, flg);
}

/*
 * This is the sysctl handler for the devstat package.  The data pushed out
 * on the kern.devstat.all sysctl variable consists of the current devstat
 * generation number, and then an array of devstat structures, one for each
 * device in the system.
 *
 * I'm really not too fond of this method of doing things, but there really
 * aren't that many alternatives.  We must have some method of making sure
 * that the generation number the user gets corresponds with the data the
 * user gets.  If the user makes a separate sysctl call to get the
 * generation, and then a sysctl call to get the device statistics, the
 * device list could have changed in that brief period of time.  By
 * supplying the generation number along with the statistics output, we can
 * guarantee that the generation number and the statistics match up.
 */
static int
sysctl_devstat(SYSCTL_HANDLER_ARGS)
{
	int error, i;
	struct devstat *nds;
	struct devstatlist *devstat_head;

	if (devstat_num_devs == 0)
		return(EINVAL);

	error = 0;
	devstat_head = &device_statq;

	/*
	 * First push out the generation number.
	 */
	error = SYSCTL_OUT(req, &devstat_generation, sizeof(long));

	/*
	 * Now push out all the devices.
	 */
	for (i = 0, nds = STAILQ_FIRST(devstat_head); 
	    (nds != NULL) && (i < devstat_num_devs) && (error == 0); 
	     nds = STAILQ_NEXT(nds, dev_links), i++)
		error = SYSCTL_OUT(req, nds, sizeof(struct devstat));

	return(error);
}

/*
 * Sysctl entries for devstat.  The first one is a node that all the rest
 * hang off of. 
 */
SYSCTL_NODE(_kern, OID_AUTO, devstat, CTLFLAG_RD, 0, "Device Statistics");

SYSCTL_PROC(_kern_devstat, OID_AUTO, all, CTLFLAG_RD|CTLTYPE_OPAQUE,
    0, 0, sysctl_devstat, "S,devstat", "All devices in the devstat list");
/*
 * Export the number of devices in the system so that userland utilities
 * can determine how much memory to allocate to hold all the devices.
 */
SYSCTL_INT(_kern_devstat, OID_AUTO, numdevs, CTLFLAG_RD, 
    &devstat_num_devs, 0, "Number of devices in the devstat list");
SYSCTL_LONG(_kern_devstat, OID_AUTO, generation, CTLFLAG_RD,
    &devstat_generation, 0, "Devstat list generation");
SYSCTL_INT(_kern_devstat, OID_AUTO, version, CTLFLAG_RD, 
    &devstat_version, 0, "Devstat list version number");
