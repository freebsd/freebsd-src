/*
 * Written By Julian ELischer
 * Copyright julian Elischer 1993.
 * Permission is granted to use or redistribute this file in any way as long
 * as this notice remains. Julian Elischer does not guarantee that this file
 * is totally correct for any given task and users of this file must
 * accept responsibility for any damage that occurs from the application of this
 * file.
 *
 * Written by Julian Elischer (julian@dialix.oz.au)
 *      $Id: scsi_base.c,v 1.32 1995/12/07 12:47:46 davidg Exp $
 */

#define SPLSD splbio
#define ESUCCESS 0
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/errno.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsiconf.h>

static errval sc_err1(struct scsi_xfer *);
static errval scsi_interpret_sense(struct scsi_xfer *);
static struct scsi_xfer *get_xs( struct scsi_link *sc_link, u_int32 flags);
static void free_xs(struct scsi_xfer *xs, struct scsi_link *sc_link,
		u_int32 flags);
static void show_mem(unsigned char *address, u_int32 num);
static void show_scsi_xs (struct scsi_xfer *);

#ifdef notyet
static int scsi_sense_qualifiers (struct scsi_xfer *, int *, int *);
static errval scsi_change_def( struct scsi_link *sc_link, u_int32 flags);
#endif

static struct scsi_xfer *next_free_xs;

/*
 * Get a scsi transfer structure for the caller. Charge the structure
 * to the device that is referenced by the sc_link structure. If the
 * sc_link structure has no 'credits' then the device already has the
 * maximum number or outstanding operations under way. In this stage,
 * wait on the structure so that when one is freed, we are awoken again
 * If the SCSI_NOSLEEP flag is set, then do not wait, but rather, return
 * a NULL pointer, signifying that no slots were available
 * Note in the link structure, that we are waiting on it.
 */

static struct scsi_xfer *
get_xs(sc_link, flags)
	struct	scsi_link *sc_link;	/* who to charge the xs to */
	u_int32	flags;			/* if this call can sleep */
{
	struct	scsi_xfer *xs;
	u_int32	s;

	SC_DEBUG(sc_link, SDEV_DB3, ("get_xs\n"));
	s = splbio();
	while (!sc_link->opennings) {
		SC_DEBUG(sc_link, SDEV_DB3, ("sleeping\n"));
		if (flags & SCSI_NOSLEEP) {
			splx(s);
			return 0;
		}
		sc_link->flags |= SDEV_WAITING;
		tsleep((caddr_t)sc_link, PRIBIO, "scsiget", 0);
	}
	sc_link->active++;
	sc_link->opennings--;
	if ( (xs = next_free_xs) ) {
		next_free_xs = xs->next;
		splx(s);
	} else {
		splx(s);
		SC_DEBUG(sc_link, SDEV_DB3, ("making\n"));
		xs = malloc(sizeof(*xs), M_TEMP,
		    ((flags & SCSI_NOSLEEP) ? M_NOWAIT : M_WAITOK));
		if (xs == NULL) {
			sc_print_addr(sc_link);
			printf("cannot allocate scsi xs\n");
			return (NULL);
		}
	}
	SC_DEBUG(sc_link, SDEV_DB3, ("returning\n"));
	xs->sc_link = sc_link;
	return (xs);
}

/*
 * Given a scsi_xfer struct, and a device (referenced through sc_link)
 * return the struct to the free pool and credit the device with it
 * If another process is waiting for an xs, do a wakeup, let it proceed
 */
static void
free_xs(xs, sc_link, flags)
	struct scsi_xfer *xs;
	struct scsi_link *sc_link;	/* who to credit for returning it */
	u_int32 flags;
{
	xs->next = next_free_xs;
	next_free_xs = xs;

	SC_DEBUG(sc_link, SDEV_DB3, ("free_xs\n"));
	/* if was 0 and someone waits, wake them up */
	sc_link->active--;
	if ((!sc_link->opennings++) && (sc_link->flags & SDEV_WAITING)) {
		sc_link->flags &= ~SDEV_WAITING;
		wakeup((caddr_t)sc_link); /* remember, it wakes them ALL up */
	} else {
		if (sc_link->device->start) {
			SC_DEBUG(sc_link, SDEV_DB2, ("calling private start()\n"));
			(*(sc_link->device->start)) (sc_link->dev_unit, flags);
		}
	}
}

/* XXX dufault: Replace "sd_size" with "scsi_read_capacity"
 * when bde is done with sd.c
 */
/*
 * Find out from the device what its capacity is.
 */
u_int32
scsi_read_capacity(sc_link, blk_size, flags)
	struct scsi_link *sc_link;
	u_int32 *blk_size;
	u_int32 flags;
{
	struct scsi_read_cap_data rdcap;
	struct scsi_read_capacity scsi_cmd;
	u_int32 size;

	/*
	 * make up a scsi command and ask the scsi driver to do
	 * it for you.
	 */
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = READ_CAPACITY;

	/*
	 * If the command works, interpret the result as a 4 byte
	 * number of blocks
	 */
	if (scsi_scsi_cmd(sc_link,
		(struct scsi_generic *) &scsi_cmd,
		sizeof(scsi_cmd),
		(u_char *) & rdcap,
		sizeof(rdcap),
		4,
		5000000,	/* WORMs tend to take a HUGE amount of time */
		NULL,
		flags | SCSI_DATA_IN) != 0) {

		sc_print_addr(sc_link);
		printf("could not get size\n");
		return (0);
	} else {
		size = scsi_4btou(&rdcap.addr_3) + 1;
		if (blk_size)
			*blk_size = scsi_4btou(&rdcap.length_3);
	}
	return (size);
}

errval
scsi_reset_target(sc_link)
	struct scsi_link *sc_link;
{
	return (scsi_scsi_cmd(sc_link,
		0,
		0,
		0,
		0,
		1,
		2000,
		NULL,
		SCSI_RESET));
}

errval
scsi_target_mode(sc_link, on_off)
	struct scsi_link *sc_link;
	int on_off;
{
	struct scsi_generic scsi_cmd;
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = SCSI_OP_TARGET;
	scsi_cmd.bytes[0] = (on_off) ? 1 : 0;

	return (scsi_scsi_cmd(sc_link,
		&scsi_cmd,
		sizeof(scsi_cmd),
		0,
		0,
		1,
		2000,
		NULL,
		SCSI_ESCAPE));
}

/*
 * Get scsi driver to send a "are you ready?" command
 */
errval
scsi_test_unit_ready(sc_link, flags)
	struct scsi_link *sc_link;
	u_int32 flags;
{
	struct scsi_test_unit_ready scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = TEST_UNIT_READY;

	return (scsi_scsi_cmd(sc_link,
		(struct scsi_generic *) &scsi_cmd,
		sizeof(scsi_cmd),
		0,
		0,
		2,
		100000,
		NULL,
		flags));
}

#ifdef notyet
/*
 * Do a scsi operation, asking a device to run as SCSI-II if it can.
 */
static errval
scsi_change_def(sc_link, flags)
	struct scsi_link *sc_link;
	u_int32 flags;
{
	struct scsi_changedef scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = CHANGE_DEFINITION;
	scsi_cmd.how = SC_SCSI_2;

	return (scsi_scsi_cmd(sc_link,
		(struct scsi_generic *) &scsi_cmd,
		sizeof(scsi_cmd),
		0,
		0,
		2,
		100000,
		NULL,
		flags));
}
#endif

/*
 * Do a scsi operation asking a device what it is
 * Use the scsi_cmd routine in the switch table.
 */
errval
scsi_inquire(sc_link, inqbuf, flags)
	struct scsi_link *sc_link;
	struct scsi_inquiry_data *inqbuf;
	u_int32 flags;
{
	struct scsi_inquiry scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = INQUIRY;
	scsi_cmd.length = sizeof(struct scsi_inquiry_data);

	return (scsi_scsi_cmd(sc_link,
		(struct scsi_generic *) &scsi_cmd,
		sizeof(scsi_cmd),
		(u_char *) inqbuf,
		sizeof(struct scsi_inquiry_data),
		2,
		100000,
		NULL,
		SCSI_DATA_IN | flags));
}

/*
 * Prevent or allow the user to remove the media
 */
errval
scsi_prevent(sc_link, type, flags)
	struct scsi_link *sc_link;
	u_int32 type, flags;
{
	struct scsi_prevent scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = PREVENT_ALLOW;
	scsi_cmd.how = type;
	return (scsi_scsi_cmd(sc_link,
		(struct scsi_generic *) &scsi_cmd,
		sizeof(scsi_cmd),
		0,
		0,
		2,
		5000,
		NULL,
		flags));
}

/*
 * Get scsi driver to send a "start up" command
 */
errval
scsi_start_unit(sc_link, flags)
	struct scsi_link *sc_link;
	u_int32 flags;
{
	struct scsi_start_stop scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = START_STOP;
	scsi_cmd.how = SSS_START;

	return (scsi_scsi_cmd(sc_link,
		(struct scsi_generic *) &scsi_cmd,
		sizeof(scsi_cmd),
		0,
		0,
		2,
		10000,
		NULL,
		flags));
}

/*
 * Get scsi driver to send a "stop" command
 */
errval
scsi_stop_unit(sc_link, eject, flags)
	struct scsi_link *sc_link;
	u_int32 eject;
	u_int32 flags;
{
	struct scsi_start_stop scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = START_STOP;
	if (eject) {
		scsi_cmd.how = SSS_LOEJ;
	}

	return (scsi_scsi_cmd(sc_link,
		(struct scsi_generic *) &scsi_cmd,
		sizeof(scsi_cmd),
		0,
		0,
		2,
		10000,
		NULL,
		flags));
}

/*
 * This routine is called by the scsi interrupt when the transfer is complete.
 */
void
scsi_done(xs)
	struct scsi_xfer *xs;
{
	struct scsi_link *sc_link = xs->sc_link;
	struct buf *bp = xs->bp;
	errval  retval;

	SC_DEBUG(sc_link, SDEV_DB2, ("scsi_done\n"));
#ifdef	SCSIDEBUG
	if (sc_link->flags & SDEV_DB1)
	{
		show_scsi_cmd(xs);
	}
#endif /*SCSIDEBUG */
	/*
 	 * If it's a user level request, bypass all usual completion processing,
 	 * let the user work it out.. We take reponsibility for freeing the
 	 * xs when the user returns. (and restarting the device's queue).
 	 */
	if (xs->flags & SCSI_USER) {
		SC_DEBUG(sc_link, SDEV_DB3, ("calling user done()\n"));
		scsi_user_done(xs); /* to take a copy of the sense etc. */
		SC_DEBUG(sc_link, SDEV_DB3, ("returned from user done()\n "));

		free_xs(xs, sc_link, SCSI_NOSLEEP); /* restarts queue too */
		SC_DEBUG(sc_link, SDEV_DB3, ("returning to adapter\n"));
		return;
	}
	/*
	 * If the device has it's own done routine, call it first.
	 * If it returns a legit error value, return that, otherwise
	 * it wants us to continue with normal processing.
	 */

	if (sc_link->device->done) {
		SC_DEBUG(sc_link, SDEV_DB2, ("calling private done()\n"));
		retval = (*sc_link->device->done) (xs);
		if (retval == -1) {
			free_xs(xs, sc_link, SCSI_NOSLEEP);	/*XXX */
			return;	/* it did it all, finish up */
		}
		/* XXX: This isn't used anywhere. Do you have plans for it,
		 * Julian? (dufault@hda.com).
		 * This allows a private 'done' handler to
		 * resubmit the command if it wants to retry,
		 * In this case the xs must NOT be freed. (julian)
		 */
		if (retval == -2) {
			return;	/* it did it all, finish up */
		}
		SC_DEBUG(sc_link, SDEV_DB3, ("continuing with generic done()\n"));
	}
	if ((bp = xs->bp) == NULL) {
		/*
		 * if it's a normal upper level request, then ask
		 * the upper level code to handle error checking
		 * rather than doing it here at interrupt time
		 */
		wakeup((caddr_t)xs);
		return;
	}
	/*
	 * Go and handle errors now.
	 * If it returns SCSIRET_DO_RETRY then we should RETRY
	 */
	if ((retval = sc_err1(xs)) == SCSIRET_DO_RETRY) {
		if ((*(sc_link->adapter->scsi_cmd)) (xs)
		    == SUCCESSFULLY_QUEUED) {	/* don't wake the job, ok? */
			return;
		}
		xs->flags |= ITSDONE;
	}

	free_xs(xs, sc_link, SCSI_NOSLEEP); /* does a start if needed */
	biodone(bp);
}

/*
 * ask the scsi driver to perform a command for us.
 * tell it where to read/write the data, and how
 * long the data is supposed to be. If we have  a buf
 * to associate with the transfer, we need that too.
 */
errval
scsi_scsi_cmd(sc_link, scsi_cmd, cmdlen, data_addr, datalen,
    retries, timeout, bp, flags)
	struct scsi_link *sc_link;
	struct scsi_generic *scsi_cmd;
	u_int32 cmdlen;
	u_char *data_addr;
	u_int32 datalen;
	u_int32 retries;
	u_int32 timeout;
	struct buf *bp;
	u_int32 flags;
{
	struct scsi_xfer *xs;
	errval  retval;
	u_int32 s;

	/*
	 * Illegal command lengths will wedge host adapter software.
	 * Reject zero length commands and assert all defined commands
	 * are the correct length.
	 */
	if ((flags & (SCSI_RESET | SCSI_ESCAPE)) == 0)
	{
		if (cmdlen == 0)
			return EFAULT;
		else
		{
			static u_int8 sizes[] = {6, 10, 10, 0, 0, 12, 0, 0 };
			u_int8 size = sizes[((scsi_cmd->opcode) >> 5)];
			if (size && (size != cmdlen))
				return EIO;
		}
	}

	SC_DEBUG(sc_link, SDEV_DB2, ("scsi_cmd\n"));

	xs = get_xs(sc_link, flags);
	if (!xs) return (ENOMEM);
	/*
	 * Fill out the scsi_xfer structure.  We don't know whose context
	 * the cmd is in, so copy it.
	 */
	bcopy(scsi_cmd, &(xs->cmdstore), cmdlen);
	xs->flags = INUSE | flags;
	xs->sc_link = sc_link;
	xs->retries = retries;
	xs->timeout = timeout;
	xs->cmd = &xs->cmdstore;
	xs->cmdlen = cmdlen;
	xs->data = data_addr;
	xs->datalen = datalen;
 	xs->resid = 0;
	xs->bp = bp;
/*XXX*/ /*use constant not magic number */
	if (datalen && ((caddr_t) data_addr < (caddr_t) KERNBASE)) {
		if (bp) {
			printf("Data buffered space not in kernel context\n");
#ifdef	SCSIDEBUG
			show_scsi_cmd(xs);
#endif	/* SCSIDEBUG */
			retval = EFAULT;
			goto bad;
		}
#ifdef BOUNCE_BUFFERS
		xs->data = (caddr_t) vm_bounce_kva_alloc( (datalen + PAGE_SIZE - 1)/PAGE_SIZE);
#else
		xs->data = malloc(datalen, M_TEMP, M_WAITOK);
#endif
		/* I think waiting is ok *//*XXX */
		switch ((int)(flags & (SCSI_DATA_IN | SCSI_DATA_OUT))) {
		case 0:
			printf("No direction flags, assuming both\n");
#ifdef	SCSIDEBUG
			show_scsi_cmd(xs);
#endif	/* SCSIDEBUG */
		case SCSI_DATA_IN | SCSI_DATA_OUT:	/* weird */
		case SCSI_DATA_OUT:
			bcopy(data_addr, xs->data, datalen);
			break;
		case SCSI_DATA_IN:
			bzero(xs->data, datalen);
		}
	}
retry:
	xs->error = XS_NOERROR;
#ifdef	PARANOID
	if (datalen && ((caddr_t) xs->data < (caddr_t) KERNBASE)) {
		printf("It's still wrong!\n");
	}
#endif	/*PARANOID*/
#ifdef	SCSIDEBUG
	if (sc_link->flags & SDEV_DB3) show_scsi_xs(xs);
#endif /* SCSIDEBUG */
	/*
	 * Do the transfer. If we are polling we will return:
	 * COMPLETE,  Was poll, and scsi_done has been called
	 * TRY_AGAIN_LATER, Adapter short resources, try again
	 *
	 * if under full steam (interrupts) it will return:
	 * SUCCESSFULLY_QUEUED, will do a wakeup when complete
	 * TRY_AGAIN_LATER, (as for polling)
	 * After the wakeup, we must still check if it succeeded
	 *
	 * If we have a bp however, all the error proccessing
	 * and the buffer code both expect us to return straight
	 * to them, so as soon as the command is queued, return
	 */

	retval = (*(sc_link->adapter->scsi_cmd)) (xs);

	switch (retval) {
	case SUCCESSFULLY_QUEUED:
		if (bp) {
			return 0;	/* will sleep (or not) elsewhere */
		}
		s = splbio();
		while (!(xs->flags & ITSDONE)) {
			tsleep((caddr_t)xs, PRIBIO + 1, "scsicmd", 0);
		}
		splx(s);
		/* fall through to check success of completed command */
	case COMPLETE:		/* Polling command completed ok */
/*XXX*/	case HAD_ERROR:		/* Polling command completed with error */
		SC_DEBUG(sc_link, SDEV_DB3, ("back in cmd()\n"));
		if ((retval = sc_err1(xs)) == SCSIRET_DO_RETRY)
			goto retry;
		break;

	case TRY_AGAIN_LATER:	/* adapter resource shortage */
		SC_DEBUG(sc_link, SDEV_DB3, ("will try again \n"));
		/* should sleep 1 sec here */
		if (xs->retries--) {
			xs->flags &= ~ITSDONE;
			goto retry;
		}
	default:
		retval = EIO;
	}
	/*
	 * If we had to copy the data out of the user's context,
	 * then do the other half (copy it back or whatever)
	 * and free the memory buffer
	 */
	if (datalen && (xs->data != data_addr)) {
		switch ((int)(flags & (SCSI_DATA_IN | SCSI_DATA_OUT))) {
		case 0:
		case SCSI_DATA_IN | SCSI_DATA_OUT:	/* weird */
		case SCSI_DATA_IN:
			bcopy(xs->data, data_addr, datalen);
			break;
		}
#ifdef BOUNCE_BUFFERS
		vm_bounce_kva_alloc_free((vm_offset_t) xs->data,
					 (datalen + PAGE_SIZE - 1)/PAGE_SIZE);
#else
		free(xs->data, M_TEMP);
#endif
	}
	/*
	 * we have finished with the xfer stuct, free it and
	 * check if anyone else needs to be started up.
	 */
bad:
	free_xs(xs, sc_link, flags);	/* includes the 'start' op */
	if (bp && retval) {
		bp->b_error = retval;
		bp->b_flags |= B_ERROR;
		biodone(bp);
	}
	return (retval);
}

static errval
sc_done(struct scsi_xfer *xs, int code)
{
	/*
	 * If it has a buf, we might be working with
	 * a request from the buffer cache or some other
	 * piece of code that requires us to process
	 * errors at interrupt time. We have probably
	 * been called by scsi_done()
	 */
	struct buf *bp;

	if (code == SCSIRET_DO_RETRY) {
		if (xs->retries--) {
			xs->error = XS_NOERROR;
			xs->flags &= ~ITSDONE;
			return SCSIRET_DO_RETRY;
		}
		code = EIO;	/* Too many retries */
	}

	/*
 	 * an EOF condition results in a VALID resid..
 	 */
 	if(xs->flags & SCSI_EOF) {
 		xs->resid = xs->datalen;
 		xs->flags |= SCSI_RESID_VALID;
  	}

	bp = xs->bp;
	if (code != ESUCCESS) {
		if (bp) {
			bp->b_error = 0;
			bp->b_flags |= B_ERROR;
			bp->b_error = code;
			bp->b_resid = bp->b_bcount;
			SC_DEBUG(xs->sc_link, SDEV_DB3,
				("scsi_interpret_sense (bp) returned %d\n", code));
		} else {
			SC_DEBUG(xs->sc_link, SDEV_DB3,
				("scsi_interpret_sense (no bp) returned %d\n", code));
		}
	}
	else {
		if (bp) {

			bp->b_error = 0;

			/* XXX: We really shouldn't need this SCSI_RESID_VALID flag.
			 * If we initialize it to 0 and only touch it if we have
			 * a value then we can leave out the test.
			 */

			if (xs->flags & SCSI_RESID_VALID) {
				bp->b_resid = xs->resid;
				bp->b_flags |= B_ERROR;
			} else {
				bp->b_resid = 0;
			}
		}
	}

	return code;
}

/*
 * submit a scsi command, given the command.. used for retries
 * and callable from timeout()
 */
#ifdef NOTYET
errval scsi_submit(xs)
	struct scsi_xfer *xs;
{
	struct scsi_link *sc_link = xs->sc_link;
	int retval;

	retval = (*(sc_link->adapter->scsi_cmd)) (xs);

	return retval;
}

/*
 * Retry a scsi command, given the command,  and a delay.
 */
errval scsi_retry(xs,delay)
	struct scsi_xfer *xs;
	int	delay;
{
	if(delay)
	{
		timeout(((void())*)scsi_submit,xs,hz*delay);
		return(0);
	}
	else
	{
		return(scsi_submit(xs));
	}
}
#endif

/*
 * handle checking for errors..
 * called at interrupt time from scsi_done() and
 * at user time from scsi_scsi_cmd(), depending on whether
 * there was a bp  (basically, if there is a bp, there may be no
 * associated process at the time. (it could be an async operation))
 * lower level routines shouldn't know about xs->bp.. we are the lowest.
 */
static errval
sc_err1(xs)
	struct scsi_xfer *xs;
{
	SC_DEBUG(xs->sc_link, SDEV_DB3, ("sc_err1,err = 0x%lx \n", xs->error));

	switch ((int)xs->error) {
	case XS_SENSE:
		return sc_done(xs, scsi_interpret_sense(xs));

	case XS_NOERROR:
		return sc_done(xs, ESUCCESS);

	case XS_BUSY:
 		/* should somehow arange for a 1 sec delay here (how?)[jre]
 		 * tsleep(&localvar, priority, "foo", hz);
 		 * that's how! [unknown]
 		 * no, we could be at interrupt context..  use
 		 * timeout(scsi_resubmit,xs,hz); [jre] (not implimenteed yet)
 		 */
	case XS_TIMEOUT:
		return sc_done(xs, SCSIRET_DO_RETRY);

		/* fall through */
	case XS_DRIVER_STUFFUP:
		return sc_done(xs, EIO);

	default:
		sc_print_addr(xs->sc_link);
		printf("unknown error category from scsi driver\n");
		return sc_done(xs, EIO);
	}
}

#ifdef notyet
static int
scsi_sense_qualifiers(xs, asc, ascq)
	struct scsi_xfer *xs;
	int *asc;
	int *ascq;
{
	struct scsi_sense_data_new *sense;
	struct scsi_sense_extended *ext;

	sense = (struct scsi_sense_data_new *)&(xs->sense);

	ext = &(sense->ext.extended);

	if (ext->extra_len < 5)
		return 0;

	*asc = (ext->extra_len >= 5) ? ext->add_sense_code : 0;
	*ascq = (ext->extra_len >= 6) ? ext->add_sense_code_qual : 0;

	return 1;
}

#endif

/*
 * scsi_sense_print will decode the sense data into human
 * readable form.  Sense handlers can use this to generate
 * a report.  This NOW DOES send the closing "\n".
 */
void scsi_sense_print(xs)
	struct scsi_xfer *xs;
{
	struct scsi_sense_data_new *sense;
	struct scsi_sense_extended *ext;
	u_int32 key;
	u_int32 info;
	int asc, ascq;

	/* This sense key text now matches what is in the SCSI spec
	 * (Yes, even the capitals)
	 * so that it is easier to look through the spec to find the
	 * appropriate place.
	 */
	static char *sense_key_text[] =
	{
	    "NO SENSE", "RECOVERED ERROR",
	    "NOT READY", "MEDIUM ERROR",
	    "HARDWARE FAILURE", "ILLEGAL REQUEST",
	    "UNIT ATTENTION", "DATA PROTECT",
	    "BLANK CHECK", "Vendor Specific",
	    "COPY ABORTED", "ABORTED COMMAND",
	    "EQUAL", "VOLUME OVERFLOW",
	    "MISCOMPARE", "RESERVED"
	};

	sc_print_start(xs->sc_link);

	sense = (struct scsi_sense_data_new *)&(xs->sense);
	ext = &(sense->ext.extended);

	key = ext->flags & SSD_KEY;

	switch (sense->error_code & SSD_ERRCODE) {
	case 0x71:		/* deferred error */
		printf("Deferred Error: ");

		/* DROP THROUGH */

	case 0x70:

		printf("%s", sense_key_text[key]);
		info = ntohl(*((long *) ext->info));

		if (sense->error_code & SSD_ERRCODE_VALID) {

			switch ((int)key) {
			case 0x2:	/* NOT READY */
			case 0x5:	/* ILLEGAL REQUEST */
			case 0x6:	/* UNIT ATTENTION */
			case 0x7:	/* DATA PROTECT */
				break;
			case 0x8:	/* BLANK CHECK */
				printf(" req sz: %ld (decimal)",
				    info);
				break;
			default:
				if (info) {
		    		if (sense->ext.extended.flags & SSD_ILI) {
						printf(" ILI (length mismatch): %ld", info);
					}
					else {
						printf(" info:%lx", info);
					}
				}
			}
		}
		else if (info)
			printf(" info?:%lx", info);

		if (ext->extra_len >= 4) {
			if (bcmp(ext->cmd_spec_info, "\0\0\0\0", 4)) {
				printf(" csi:%x,%x,%x,%x",
				ext->cmd_spec_info[0],
				ext->cmd_spec_info[1],
				ext->cmd_spec_info[2],
				ext->cmd_spec_info[3]);
			}
		}

		asc = (ext->extra_len >= 5) ? ext->add_sense_code : 0;
		ascq = (ext->extra_len >= 6) ? ext->add_sense_code_qual : 0;

		if (asc || ascq)
		{
			char *desc = scsi_sense_desc(asc, ascq);
			printf(" asc:%x,%x", asc, ascq);

			if (strlen(desc) > 40)
				sc_print_addr(xs->sc_link);;

			printf(" %s", desc);
		}

		if (ext->extra_len >= 7 && ext->fru) {
			printf(" field replaceable unit: %x", ext->fru);
		}

		if (ext->extra_len >= 10 &&
		(ext->sense_key_spec_1 & SSD_SCS_VALID)) {
			printf(" sks:%x,%x", ext->sense_key_spec_1,
			(ext->sense_key_spec_2 |
			ext->sense_key_spec_3));
		}
		break;

	/*
	 * Not code 70, just report it
	 */
	default:
		printf("error code %d",
		    sense->error_code & SSD_ERRCODE);
		if (sense->error_code & SSD_ERRCODE_VALID) {
			printf(" at block no. %ld (decimal)",
			    (((unsigned long)sense->ext.unextended.blockhi) << 16) +
			    (((unsigned long)sense->ext.unextended.blockmed) << 8) +
			    ((unsigned long)sense->ext.unextended.blocklow));
		}
	}

	printf("\n");
	sc_print_finish();
}

/*
 * Look at the returned sense and act on the error, determining
 * the unix error number to pass back.  (0 = report no error)
 *
 * THIS IS THE DEFAULT SENSE HANDLER
 */
static errval
scsi_interpret_sense(xs)
	struct scsi_xfer *xs;
{
	struct scsi_sense_data *sense;
	struct scsi_link *sc_link = xs->sc_link;
	u_int32 key;
	u_int32 silent;
	errval  errcode;
	int error_code;

	/*
	 * If the flags say errs are ok, then always return ok.
	 * XXX: What if it is a deferred error?
	 */
	if (xs->flags & SCSI_ERR_OK)
		return (ESUCCESS);

	sense = &(xs->sense);
#ifdef	SCSIDEBUG
	if (sc_link->flags & SDEV_DB1) {

		u_int32 count = 0;
		printf("code%x valid%x ",
		    sense->error_code & SSD_ERRCODE,
		    sense->error_code & SSD_ERRCODE_VALID ? 1 : 0);
		printf("seg%x key%x ili%x eom%x fmark%x\n",
		    sense->ext.extended.segment,
		    sense->ext.extended.flags & SSD_KEY,
		    sense->ext.extended.flags & SSD_ILI ? 1 : 0,
		    sense->ext.extended.flags & SSD_EOM ? 1 : 0,
		    sense->ext.extended.flags & SSD_FILEMARK ? 1 : 0);
		printf("info: %x %x %x %x followed by %d extra bytes\n",
		    sense->ext.extended.info[0],
		    sense->ext.extended.info[1],
		    sense->ext.extended.info[2],
		    sense->ext.extended.info[3],
		    sense->ext.extended.extra_len);
		printf("extra: ");
		while (count < sense->ext.extended.extra_len) {
			printf("%x ", sense->ext.extended.extra_bytes[count++]);
		}
		printf("\n");
	}
  #endif	/*SCSIDEBUG */
	/*
	 * If the device has it's own sense handler, call it first.
	 * If it returns a legit errno value, return that, otherwise
	 * it should return either DO_RETRY or CONTINUE to either
	 * request a retry or continue with default sense handling.
	 */
	if (sc_link->device->err_handler) {
		SC_DEBUG(sc_link, SDEV_DB2,
			("calling private err_handler()\n"));
		errcode = (*sc_link->device->err_handler) (xs);

		SC_DEBUG(sc_link, SDEV_DB2,
			("private err_handler() returned %d\n",errcode));
		if (errcode >= 0) {
		SC_DEBUG(sc_link, SDEV_DB2,
			("SCSI_EOF = %d\n",(xs->flags & SCSI_EOF)?1:0));
		SC_DEBUG(sc_link, SDEV_DB2,
			("SCSI_RESID_VALID = %d\n",
				(xs->flags & SCSI_RESID_VALID)?1:0));

			if(xs->flags & SCSI_EOF) {
				xs->resid = xs->datalen;
 				xs->flags |= SCSI_RESID_VALID;
			}
			return errcode;			/* valid errno value */
		}

		switch(errcode) {
		case SCSIRET_DO_RETRY:	/* Requested a retry */
			return errcode;

		case SCSIRET_CONTINUE:	/* Continue with default sense processing */
			break;

		default:
			sc_print_addr(xs->sc_link);
			printf("unknown return code %d from sense handler.\n",
			errcode);

			return errcode;
		}
	}

	/* otherwise use the default */
	silent = (xs->flags & SCSI_SILENT);
	key = sense->ext.extended.flags & SSD_KEY;
	error_code = sense->error_code & SSD_ERRCODE;

	if (!silent) {
		scsi_sense_print(xs);
	}

	switch (error_code) {
	case 0x71:		/* deferred error */
		/* Print even if silent (not silent was already done)
		 */
		if (silent) {
			scsi_sense_print(xs);
		}

		/* XXX:
		 * This error doesn't relate to the command associated
		 * with this request sense.  A deferred error is an error
		 * for a command that has already returned GOOD status (see 7.2.14.2).
		 *
		 * By my reading of that section, it looks like the current command
		 * has been cancelled, we should now clean things up (hopefully
		 * recovering any lost data) and then
		 * retry the current command.  There are two easy choices, both
		 * wrong:
		 * 1. Drop through (like we had been doing), thus treating this as
		 * if the error were for the current command and return and stop
		 * the current command.
		 * 2. Issue a retry (like I made it do) thus hopefully recovering
		 * the current transfer, and ignoring the fact that we've dropped
		 * a command.
		 *
		 * These should probably be handled in a device specific
		 * sense handler or punted back up to a user mode daemon
		 */
		return SCSIRET_DO_RETRY;

		/*
		 * If it's code 70, use the extended stuff and interpret the key
		 */
	case 0x70:

		switch ((int)key) {
		case 0x0:	/* NO SENSE */
		case 0x1:	/* RECOVERED ERROR */
		case 0xc:	/* EQUAL */
 			if(xs->flags & SCSI_EOF) {
 				xs->resid = xs->datalen;
 				xs->flags |= SCSI_RESID_VALID;
 			}
			return (ESUCCESS);
		case 0x2:	/* NOT READY */
			sc_link->flags &= ~SDEV_MEDIA_LOADED;
			return (EBUSY);
		case 0x5:	/* ILLEGAL REQUEST */
			return (EINVAL);
		case 0x6:	/* UNIT ATTENTION */
			sc_link->flags &= ~SDEV_MEDIA_LOADED;
			if (sc_link->flags & SDEV_OPEN) {
				return (EIO);
			} else {
				return 0;
			}
		case 0x7:	/* DATA PROTECT */
			return (EACCES);
		case 0xd:	/* VOLUME OVERFLOW */
			return (ENOSPC);
		case 0x8:	/* BLANK CHECK */
			xs->flags |= SCSI_EOF; /* force EOF on tape read */
			return (ESUCCESS);
		default:
			return (EIO);
		}
	/*
	 * Not code 70, return EIO
	 */
	default:
		return (EIO);
	}
}

/*
 * Utility routines often used in SCSI stuff
 */

/*
 * convert a physical address to 3 bytes,
 * MSB at the lowest address,
 * LSB at the highest.
 */
void
scsi_uto3b(val, bytes)
	u_int32	val;
	u_char	*bytes;
{
	*bytes++ = (val & 0xff0000) >> 16;
	*bytes++ = (val & 0xff00) >> 8;
	*bytes = val & 0xff;
}

u_int32
scsi_3btou(bytes)
	u_char *bytes;
{
	u_int32 rc;
	rc = (*bytes++ << 16);
	rc += (*bytes++ << 8);
	rc += *bytes;
	return rc;
}

int32
scsi_3btoi(bytes)
	u_char *bytes;
{
	u_int32 rc = scsi_3btou(bytes);

	if (rc & 0x00800000)
		rc |= 0xff000000;

	return (int32) rc;
}

void
scsi_uto2b(val, bytes)
	u_int32	val;
	u_char	*bytes;
{
	*bytes++ = (val & 0xff00) >> 8;
	*bytes =    val & 0xff;
}

u_int32
scsi_2btou(bytes)
	u_char *bytes;
{
	u_int32 rc;
	rc  = (*bytes++ << 8);
	rc +=  *bytes;
	return rc;
}

void
scsi_uto4b(val, bytes)
	u_int32	val;
	u_char	*bytes;
{
	*bytes++ = (val & 0xff000000) >> 24;
	*bytes++ = (val & 0xff0000) >> 16;
	*bytes++ = (val & 0xff00) >> 8;
	*bytes =    val & 0xff;
}

u_int32
scsi_4btou(bytes)
	u_char *bytes;
{
	u_int32 rc;
	rc  = (*bytes++ << 24);
	rc += (*bytes++ << 16);
	rc += (*bytes++ << 8);
	rc +=  *bytes;
	return rc;
}

static sc_printing;

void
sc_print_start(sc_link)
	struct scsi_link *sc_link;
{
	sc_print_addr(sc_link);
	sc_printing++;
}
void
sc_print_finish()
{
	sc_printing--;
}

static void
id_put(int id, char *after)
{
	switch(id)
	{
		case SCCONF_UNSPEC:
		break;

		case SCCONF_ANY:
		printf("?");
		break;

		default:
		printf("%d", id);
		break;
	}

	printf("%s", after);
}

/*
 * sc_print_addr: Print out the scsi_link structure's address info.
 * This should handle any circumstance, even the transitory ones
 * during system configuration.
 */

void
sc_print_addr(sc_link)
	struct	scsi_link *sc_link;
{
	if (sc_printing)
		printf("\n");

	if (sc_link->device == 0) {
		printf("nodevice");
	}
	else if (strcmp(sc_link->device->name, "probe") != 0) {
		printf("%s", sc_link->device->name);
		id_put(sc_link->dev_unit, "");
	}

	if (sc_link->adapter == 0) {
		printf("(noadapter:");
	}
	else {
		printf("(%s", sc_link->adapter->name);
		id_put(sc_link->adapter_unit, ":");
	}

	id_put(sc_link->target, ":");
	id_put(sc_link->lun, "): ");
}

#ifdef	SCSIDEBUG
/*
 * Given a scsi_xfer, dump the request, in all it's glory
 */
static void
show_scsi_xs(xs)
	struct scsi_xfer *xs;
{
	printf("xs(%p): ", xs);
	printf("flg(0x%lx)", xs->flags);
	printf("sc_link(%p)", xs->sc_link);
	printf("retr(0x%x)", xs->retries);
	printf("timo(0x%lx)", xs->timeout);
	printf("cmd(%p)", xs->cmd);
	printf("len(0x%lx)", xs->cmdlen);
	printf("data(%p)", xs->data);
	printf("len(0x%lx)", xs->datalen);
	printf("res(0x%lx)", xs->resid);
	printf("err(0x%lx)", xs->error);
	printf("bp(%p)", xs->bp);
	show_scsi_cmd(xs);
}

void
show_scsi_cmd(struct scsi_xfer *xs)
{
	u_char *b = (u_char *) xs->cmd;
	int     i = 0;

	sc_print_addr(xs->sc_link);
	printf("command: ");

	if (!(xs->flags & SCSI_RESET)) {
		while (i < xs->cmdlen) {
			if (i)
				printf(",");
			printf("%x", b[i++]);
		}
		printf("-[%ld bytes]\n", xs->datalen);
		if (xs->datalen)
			show_mem(xs->data, min(64, xs->datalen));
	} else {
		printf("-RESET-\n");
	}
}

static void
show_mem(address, num)
	unsigned char *address;
	u_int32 num;
{
	u_int32 y;
	printf("------------------------------");
	for (y = 0; y < num; y += 1) {
		if (!(y % 16))
			printf("\n%03ld: ", y);
		printf("%02x ", *address++);
	}
	printf("\n------------------------------\n");
}
#endif /*SCSIDEBUG */
