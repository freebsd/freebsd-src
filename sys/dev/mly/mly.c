/*-
 * Copyright (c) 2000, 2001 Michael Smith
 * Copyright (c) 2000 BSDi
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
 *	$FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/ctype.h>
#include <sys/ioccom.h>
#include <sys/stat.h>

#include <machine/bus_memio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <cam/scsi/scsi_all.h>

#include <dev/mly/mlyreg.h>
#include <dev/mly/mlyio.h>
#include <dev/mly/mlyvar.h>
#define MLY_DEFINE_TABLES
#include <dev/mly/mly_tables.h>

static int	mly_get_controllerinfo(struct mly_softc *sc);
static void	mly_scan_devices(struct mly_softc *sc);
static void	mly_rescan_btl(struct mly_softc *sc, int bus, int target);
static void	mly_complete_rescan(struct mly_command *mc);
static int	mly_get_eventstatus(struct mly_softc *sc);
static int	mly_enable_mmbox(struct mly_softc *sc);
static int	mly_flush(struct mly_softc *sc);
static int	mly_ioctl(struct mly_softc *sc, struct mly_command_ioctl *ioctl, void **data, 
			  size_t datasize, u_int8_t *status, void *sense_buffer, size_t *sense_length);
static void	mly_fetch_event(struct mly_softc *sc);
static void	mly_complete_event(struct mly_command *mc);
static void	mly_process_event(struct mly_softc *sc, struct mly_event *me);
static void	mly_periodic(void *data);

static int	mly_immediate_command(struct mly_command *mc);
static int	mly_start(struct mly_command *mc);
static void	mly_complete(void *context, int pending);

static void	mly_alloc_commands_map(void *arg, bus_dma_segment_t *segs, int nseg, int error);
static int	mly_alloc_commands(struct mly_softc *sc);
static void	mly_map_command(struct mly_command *mc);
static void	mly_unmap_command(struct mly_command *mc);

static int	mly_fwhandshake(struct mly_softc *sc);

static void	mly_describe_controller(struct mly_softc *sc);
#ifdef MLY_DEBUG
static void	mly_printstate(struct mly_softc *sc);
static void	mly_print_command(struct mly_command *mc);
static void	mly_print_packet(struct mly_command *mc);
static void	mly_panic(struct mly_softc *sc, char *reason);
#endif
void		mly_print_controller(int controller);

static d_open_t		mly_user_open;
static d_close_t	mly_user_close;
static d_ioctl_t	mly_user_ioctl;
static int	mly_user_command(struct mly_softc *sc, struct mly_user_command *uc);
static int	mly_user_health(struct mly_softc *sc, struct mly_user_health *uh);

#define MLY_CDEV_MAJOR  158

static struct cdevsw mly_cdevsw = {
    mly_user_open,
    mly_user_close,
    noread,
    nowrite,
    mly_user_ioctl,
    nopoll,
    nommap,
    nostrategy,
    "mly",
    MLY_CDEV_MAJOR,
    nodump,
    nopsize,
    0
};

/********************************************************************************
 ********************************************************************************
                                                                 Device Interface
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Initialise the controller and softc
 */
int
mly_attach(struct mly_softc *sc)
{
    int		error;

    debug_called(1);

    /*
     * Initialise per-controller queues.
     */
    mly_initq_free(sc);
    mly_initq_ready(sc);
    mly_initq_busy(sc);
    mly_initq_complete(sc);

#if __FreeBSD_version >= 500005
    /*
     * Initialise command-completion task.
     */
    TASK_INIT(&sc->mly_task_complete, 0, mly_complete, sc);
#endif

    /* disable interrupts before we start talking to the controller */
    MLY_MASK_INTERRUPTS(sc);

    /* 
     * Wait for the controller to come ready, handshake with the firmware if required.
     * This is typically only necessary on platforms where the controller BIOS does not
     * run.
     */
    if ((error = mly_fwhandshake(sc)))
	return(error);

    /*
     * Allocate command buffers
     */
    if ((error = mly_alloc_commands(sc)))
	return(error);

    /* 
     * Obtain controller feature information
     */
    if ((error = mly_get_controllerinfo(sc)))
	return(error);

    /*
     * Get the current event counter for health purposes, populate the initial
     * health status buffer.
     */
    if ((error = mly_get_eventstatus(sc)))
	return(error);

    /*
     * Enable memory-mailbox mode
     */
    if ((error = mly_enable_mmbox(sc)))
	return(error);

    /*
     * Attach to CAM.
     */
    if ((error = mly_cam_attach(sc)))
	return(error);

    /* 
     * Print a little information about the controller 
     */
    mly_describe_controller(sc);

    /*
     * Mark all attached devices for rescan
     */
    mly_scan_devices(sc);

    /*
     * Instigate the first status poll immediately.  Rescan completions won't
     * happen until interrupts are enabled, which should still be before
     * the SCSI subsystem gets to us. (XXX assuming CAM and interrupt-driven
     * discovery here...)
     */
    mly_periodic((void *)sc);

    /*
     * Create the control device.
     */
    sc->mly_dev_t = make_dev(&mly_cdevsw, device_get_unit(sc->mly_dev), UID_ROOT, GID_OPERATOR,
			     S_IRUSR | S_IWUSR, "mly%d", device_get_unit(sc->mly_dev));
    sc->mly_dev_t->si_drv1 = sc;

    /* enable interrupts now */
    MLY_UNMASK_INTERRUPTS(sc);

    return(0);
}

/********************************************************************************
 * Bring the controller to a state where it can be safely left alone.
 */
void
mly_detach(struct mly_softc *sc)
{

    debug_called(1);

    /* kill the periodic event */
    untimeout(mly_periodic, sc, sc->mly_periodic);

    sc->mly_state |= MLY_STATE_SUSPEND;

    /* flush controller */
    mly_printf(sc, "flushing cache...");
    printf("%s\n", mly_flush(sc) ? "failed" : "done");

    MLY_MASK_INTERRUPTS(sc);
}

/********************************************************************************
 ********************************************************************************
                                                                 Command Wrappers
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Fill in the mly_controllerinfo and mly_controllerparam fields in the softc.
 */
static int
mly_get_controllerinfo(struct mly_softc *sc)
{
    struct mly_command_ioctl	mci;
    u_int8_t			status;
    int				error;

    debug_called(1);

    if (sc->mly_controllerinfo != NULL)
	free(sc->mly_controllerinfo, M_DEVBUF);

    /* build the getcontrollerinfo ioctl and send it */
    bzero(&mci, sizeof(mci));
    sc->mly_controllerinfo = NULL;
    mci.sub_ioctl = MDACIOCTL_GETCONTROLLERINFO;
    if ((error = mly_ioctl(sc, &mci, (void **)&sc->mly_controllerinfo, sizeof(*sc->mly_controllerinfo),
			   &status, NULL, NULL)))
	return(error);
    if (status != 0)
	return(EIO);

    if (sc->mly_controllerparam != NULL)
	free(sc->mly_controllerparam, M_DEVBUF);

    /* build the getcontrollerparameter ioctl and send it */
    bzero(&mci, sizeof(mci));
    sc->mly_controllerparam = NULL;
    mci.sub_ioctl = MDACIOCTL_GETCONTROLLERPARAMETER;
    if ((error = mly_ioctl(sc, &mci, (void **)&sc->mly_controllerparam, sizeof(*sc->mly_controllerparam),
			   &status, NULL, NULL)))
	return(error);
    if (status != 0)
	return(EIO);

    return(0);
}

/********************************************************************************
 * Schedule all possible devices for a rescan.
 *
 */
static void
mly_scan_devices(struct mly_softc *sc)
{
    int		bus, target, nchn;

    debug_called(1);

    /*
     * Clear any previous BTL information.
     */
    bzero(&sc->mly_btl, sizeof(sc->mly_btl));

    /*
     * Mark all devices as requiring a rescan, and let the early periodic scan collect them.
     */
    nchn = sc->mly_controllerinfo->physical_channels_present +
	sc->mly_controllerinfo->virtual_channels_present;
    for (bus = 0; bus < nchn; bus++)
	for (target = 0; target < MLY_MAX_TARGETS; target++)
	    sc->mly_btl[bus][target].mb_flags = MLY_BTL_RESCAN;

}

/********************************************************************************
 * Rescan a device, possibly as a consequence of getting an event which suggests
 * that it may have changed.
 */
static void
mly_rescan_btl(struct mly_softc *sc, int bus, int target)
{
    struct mly_command		*mc;
    struct mly_command_ioctl	*mci;

    debug_called(2);

    /* get a command */
    mc = NULL;
    if (mly_alloc_command(sc, &mc))
	return;				/* we'll be retried soon */

    /* set up the data buffer */
    if ((mc->mc_data = malloc(sizeof(union mly_devinfo), M_DEVBUF, M_NOWAIT | M_ZERO)) == NULL) {
	mly_release_command(mc);
	return;				/* we'll get retried the next time a command completes */
    }
    mc->mc_flags |= MLY_CMD_DATAIN;
    mc->mc_complete = mly_complete_rescan;

    sc->mly_btl[bus][target].mb_flags &= ~MLY_BTL_RESCAN;

    /* 
     * Build the ioctl.
     *
     * At this point we are committed to sending this request, as it
     * will be the only one constructed for this particular update.
     */
    mci = (struct mly_command_ioctl *)&mc->mc_packet->ioctl;
    mci->opcode = MDACMD_IOCTL;
    mci->addr.phys.controller = 0;
    mci->timeout.value = 30;
    mci->timeout.scale = MLY_TIMEOUT_SECONDS;
    if (bus >= sc->mly_controllerinfo->physical_channels_present) {
	mc->mc_length = mci->data_size = sizeof(struct mly_ioctl_getlogdevinfovalid);
	mci->sub_ioctl = MDACIOCTL_GETLOGDEVINFOVALID;
	mci->addr.log.logdev = ((bus - sc->mly_controllerinfo->physical_channels_present) * MLY_MAX_TARGETS) 
	    + target;
	debug(2, "logical device %d", mci->addr.log.logdev);
    } else {
	mc->mc_length = mci->data_size = sizeof(struct mly_ioctl_getphysdevinfovalid);
	mci->sub_ioctl = MDACIOCTL_GETPHYSDEVINFOVALID;
	mci->addr.phys.lun = 0;
	mci->addr.phys.target = target;
	mci->addr.phys.channel = bus;
	debug(2, "physical device %d:%d", mci->addr.phys.channel, mci->addr.phys.target);
    }
    
    /*
     * Use the ready queue to get this command dispatched.
     */
    mly_enqueue_ready(mc);
    mly_startio(sc);
}

/********************************************************************************
 * Handle the completion of a rescan operation
 */
static void
mly_complete_rescan(struct mly_command *mc)
{
    struct mly_softc				*sc = mc->mc_sc;
    struct mly_ioctl_getlogdevinfovalid		*ldi;
    struct mly_ioctl_getphysdevinfovalid	*pdi;
    int						bus, target;

    debug_called(2);

    /* iff the command completed OK, we should use the result to update our data */
    if (mc->mc_status == 0) {
	if (mc->mc_length == sizeof(*ldi)) {
	    ldi = (struct mly_ioctl_getlogdevinfovalid *)mc->mc_data;
	    bus = MLY_LOGDEV_BUS(sc, ldi->logical_device_number);
	    target = MLY_LOGDEV_TARGET(ldi->logical_device_number);
	    sc->mly_btl[bus][target].mb_flags = MLY_BTL_LOGICAL;	/* clears all other flags */
	    sc->mly_btl[bus][target].mb_type = ldi->raid_level;
	    sc->mly_btl[bus][target].mb_state = ldi->state;
	    debug(2, "BTL rescan for %d returns %s, %s", ldi->logical_device_number, 
		  mly_describe_code(mly_table_device_type, ldi->raid_level),
		  mly_describe_code(mly_table_device_state, ldi->state));
	} else if (mc->mc_length == sizeof(*pdi)) {
	    pdi = (struct mly_ioctl_getphysdevinfovalid *)mc->mc_data;
	    bus = pdi->channel;
	    target = pdi->target;
	    sc->mly_btl[bus][target].mb_flags = MLY_BTL_PHYSICAL;	/* clears all other flags */
	    sc->mly_btl[bus][target].mb_type = MLY_DEVICE_TYPE_PHYSICAL;
	    sc->mly_btl[bus][target].mb_state = pdi->state;
	    sc->mly_btl[bus][target].mb_speed = pdi->speed;
	    sc->mly_btl[bus][target].mb_width = pdi->width;
	    if (pdi->state != MLY_DEVICE_STATE_UNCONFIGURED)
		sc->mly_btl[bus][target].mb_flags |= MLY_BTL_PROTECTED;
	    debug(2, "BTL rescan for %d:%d returns %s", bus, target, 
		  mly_describe_code(mly_table_device_state, pdi->state));
	} else {
	    mly_printf(sc, "BTL rescan result corrupted\n");
	}
    } else {
	/*
	 * A request sent for a device beyond the last device present will fail.
	 * We don't care about this, so we do nothing about it.
	 */
    }
    free(mc->mc_data, M_DEVBUF);
    mly_release_command(mc);
}

/********************************************************************************
 * Get the current health status and set the 'next event' counter to suit.
 */
static int
mly_get_eventstatus(struct mly_softc *sc)
{
    struct mly_command_ioctl	mci;
    struct mly_health_status	*mh;
    u_int8_t			status;
    int				error;

    /* build the gethealthstatus ioctl and send it */
    bzero(&mci, sizeof(mci));
    mh = NULL;
    mci.sub_ioctl = MDACIOCTL_GETHEALTHSTATUS;

    if ((error = mly_ioctl(sc, &mci, (void **)&mh, sizeof(*mh), &status, NULL, NULL)))
	return(error);
    if (status != 0)
	return(EIO);

    /* get the event counter */
    sc->mly_event_change = mh->change_counter;
    sc->mly_event_waiting = mh->next_event;
    sc->mly_event_counter = mh->next_event;

    /* save the health status into the memory mailbox */
    bcopy(mh, &sc->mly_mmbox->mmm_health.status, sizeof(*mh));

    debug(1, "initial change counter %d, event counter %d", mh->change_counter, mh->next_event);
    
    free(mh, M_DEVBUF);
    return(0);
}

/********************************************************************************
 * Enable the memory mailbox mode.
 */
static int
mly_enable_mmbox(struct mly_softc *sc)
{
    struct mly_command_ioctl	mci;
    u_int8_t			*sp, status;
    int				error;

    debug_called(1);

    /* build the ioctl and send it */
    bzero(&mci, sizeof(mci));
    mci.sub_ioctl = MDACIOCTL_SETMEMORYMAILBOX;
    /* set buffer addresses */
    mci.param.setmemorymailbox.command_mailbox_physaddr = 
	sc->mly_mmbox_busaddr + offsetof(struct mly_mmbox, mmm_command);
    mci.param.setmemorymailbox.status_mailbox_physaddr = 
	sc->mly_mmbox_busaddr + offsetof(struct mly_mmbox, mmm_status);
    mci.param.setmemorymailbox.health_buffer_physaddr = 
	sc->mly_mmbox_busaddr + offsetof(struct mly_mmbox, mmm_health);

    /* set buffer sizes - abuse of data_size field is revolting */
    sp = (u_int8_t *)&mci.data_size;
    sp[0] = ((sizeof(union mly_command_packet) * MLY_MMBOX_COMMANDS) / 1024);
    sp[1] = (sizeof(union mly_status_packet) * MLY_MMBOX_STATUS) / 1024;
    mci.param.setmemorymailbox.health_buffer_size = sizeof(union mly_health_region) / 1024;

    debug(1, "memory mailbox at %p (0x%llx/%d 0x%llx/%d 0x%llx/%d", sc->mly_mmbox,
	  mci.param.setmemorymailbox.command_mailbox_physaddr, sp[0],
	  mci.param.setmemorymailbox.status_mailbox_physaddr, sp[1],
	  mci.param.setmemorymailbox.health_buffer_physaddr, 
	  mci.param.setmemorymailbox.health_buffer_size);

    if ((error = mly_ioctl(sc, &mci, NULL, 0, &status, NULL, NULL)))
	return(error);
    if (status != 0)
	return(EIO);
    sc->mly_state |= MLY_STATE_MMBOX_ACTIVE;
    debug(1, "memory mailbox active");
    return(0);
}

/********************************************************************************
 * Flush all pending I/O from the controller.
 */
static int
mly_flush(struct mly_softc *sc)
{
    struct mly_command_ioctl	mci;
    u_int8_t			status;
    int				error;

    debug_called(1);

    /* build the ioctl */
    bzero(&mci, sizeof(mci));
    mci.sub_ioctl = MDACIOCTL_FLUSHDEVICEDATA;
    mci.param.deviceoperation.operation_device = MLY_OPDEVICE_PHYSICAL_CONTROLLER;

    /* pass it off to the controller */
    if ((error = mly_ioctl(sc, &mci, NULL, 0, &status, NULL, NULL)))
	return(error);

    return((status == 0) ? 0 : EIO);
}

/********************************************************************************
 * Perform an ioctl command.
 *
 * If (data) is not NULL, the command requires data transfer.  If (*data) is NULL
 * the command requires data transfer from the controller, and we will allocate
 * a buffer for it.  If (*data) is not NULL, the command requires data transfer
 * to the controller.
 *
 * XXX passing in the whole ioctl structure is ugly.  Better ideas?
 *
 * XXX we don't even try to handle the case where datasize > 4k.  We should.
 */
static int
mly_ioctl(struct mly_softc *sc, struct mly_command_ioctl *ioctl, void **data, size_t datasize, 
	  u_int8_t *status, void *sense_buffer, size_t *sense_length)
{
    struct mly_command		*mc;
    struct mly_command_ioctl	*mci;
    int				error;

    debug_called(1);

    mc = NULL;
    if (mly_alloc_command(sc, &mc)) {
	error = ENOMEM;
	goto out;
    }

    /* copy the ioctl structure, but save some important fields and then fixup */
    mci = &mc->mc_packet->ioctl;
    ioctl->sense_buffer_address = mci->sense_buffer_address;
    ioctl->maximum_sense_size = mci->maximum_sense_size;
    *mci = *ioctl;
    mci->opcode = MDACMD_IOCTL;
    mci->timeout.value = 30;
    mci->timeout.scale = MLY_TIMEOUT_SECONDS;
    
    /* handle the data buffer */
    if (data != NULL) {
	if (*data == NULL) {
	    /* allocate data buffer */
	    if ((mc->mc_data = malloc(datasize, M_DEVBUF, M_NOWAIT)) == NULL) {
		error = ENOMEM;
		goto out;
	    }
	    mc->mc_flags |= MLY_CMD_DATAIN;
	} else {
	    mc->mc_data = *data;
	    mc->mc_flags |= MLY_CMD_DATAOUT;
	}
	mc->mc_length = datasize;
	mc->mc_packet->generic.data_size = datasize;
    }
    
    /* run the command */
    if ((error = mly_immediate_command(mc)))
	goto out;
    
    /* clean up and return any data */
    *status = mc->mc_status;
    if ((mc->mc_sense > 0) && (sense_buffer != NULL)) {
	bcopy(mc->mc_packet, sense_buffer, mc->mc_sense);
	*sense_length = mc->mc_sense;
	goto out;
    }

    /* should we return a data pointer? */
    if ((data != NULL) && (*data == NULL))
	*data = mc->mc_data;

    /* command completed OK */
    error = 0;

out:
    if (mc != NULL) {
	/* do we need to free a data buffer we allocated? */
	if (error && (mc->mc_data != NULL) && (*data == NULL))
	    free(mc->mc_data, M_DEVBUF);
	mly_release_command(mc);
    }
    return(error);
}

/********************************************************************************
 * Fetch one event from the controller.
 */
static void
mly_fetch_event(struct mly_softc *sc)
{
    struct mly_command		*mc;
    struct mly_command_ioctl	*mci;
    int				s;
    u_int32_t			event;

    debug_called(2);

    /* get a command */
    mc = NULL;
    if (mly_alloc_command(sc, &mc))
	return;				/* we'll get retried the next time a command completes */

    /* set up the data buffer */
    if ((mc->mc_data = malloc(sizeof(struct mly_event), M_DEVBUF, M_NOWAIT | M_ZERO)) == NULL) {
	mly_release_command(mc);
	return;				/* we'll get retried the next time a command completes */
    }
    mc->mc_length = sizeof(struct mly_event);
    mc->mc_flags |= MLY_CMD_DATAIN;
    mc->mc_complete = mly_complete_event;

    /*
     * Get an event number to fetch.  It's possible that we've raced with another
     * context for the last event, in which case there will be no more events.
     */
    s = splcam();
    if (sc->mly_event_counter == sc->mly_event_waiting) {
	mly_release_command(mc);
	splx(s);
	return;
    }
    event = sc->mly_event_counter++;
    splx(s);

    /* 
     * Build the ioctl.
     *
     * At this point we are committed to sending this request, as it
     * will be the only one constructed for this particular event number.
     */
    mci = (struct mly_command_ioctl *)&mc->mc_packet->ioctl;
    mci->opcode = MDACMD_IOCTL;
    mci->data_size = sizeof(struct mly_event);
    mci->addr.phys.lun = (event >> 16) & 0xff;
    mci->addr.phys.target = (event >> 24) & 0xff;
    mci->addr.phys.channel = 0;
    mci->addr.phys.controller = 0;
    mci->timeout.value = 30;
    mci->timeout.scale = MLY_TIMEOUT_SECONDS;
    mci->sub_ioctl = MDACIOCTL_GETEVENT;
    mci->param.getevent.sequence_number_low = event & 0xffff;

    debug(2, "fetch event %u", event);

    /*
     * Use the ready queue to get this command dispatched.
     */
    mly_enqueue_ready(mc);
    mly_startio(sc);
}

/********************************************************************************
 * Handle the completion of an event poll.
 *
 * Note that we don't actually have to instigate another poll; the completion of
 * this command will trigger that if there are any more events to poll for.
 */
static void
mly_complete_event(struct mly_command *mc)
{
    struct mly_softc	*sc = mc->mc_sc;
    struct mly_event	*me = (struct mly_event *)mc->mc_data;

    debug_called(2);

    /* 
     * If the event was successfully fetched, process it.
     */
    if (mc->mc_status == SCSI_STATUS_OK) {
	mly_process_event(sc, me);
	free(me, M_DEVBUF);
    }
    mly_release_command(mc);
}

/********************************************************************************
 * Process a controller event.
 */
static void
mly_process_event(struct mly_softc *sc, struct mly_event *me)
{
    struct scsi_sense_data	*ssd = (struct scsi_sense_data *)&me->sense[0];
    char			*fp, *tp;
    int				bus, target, event, class, action;

    /* 
     * Errors can be reported using vendor-unique sense data.  In this case, the
     * event code will be 0x1c (Request sense data present), the sense key will
     * be 0x09 (vendor specific), the MSB of the ASC will be set, and the 
     * actual event code will be a 16-bit value comprised of the ASCQ (low byte)
     * and low seven bits of the ASC (low seven bits of the high byte).
     */
    if ((me->code == 0x1c) && 
	((ssd->flags & SSD_KEY) == SSD_KEY_Vendor_Specific) &&
	(ssd->add_sense_code & 0x80)) {
	event = ((int)(ssd->add_sense_code & ~0x80) << 8) + ssd->add_sense_code_qual;
    } else {
	event = me->code;
    }

    /* look up event, get codes */
    fp = mly_describe_code(mly_table_event, event);

    debug(2, "Event %d  code 0x%x", me->sequence_number, me->code);

    /* quiet event? */
    class = fp[0];
    if (isupper(class) && bootverbose)
	class = tolower(class);

    /* get action code, text string */
    action = fp[1];
    tp = &fp[2];

    /*
     * Print some information about the event.
     *
     * This code uses a table derived from the corresponding portion of the Linux
     * driver, and thus the parser is very similar.
     */
    switch(class) {
    case 'p':		/* error on physical device */
	mly_printf(sc, "physical device %d:%d %s\n", me->channel, me->target, tp);
	if (action == 'r')
	    sc->mly_btl[me->channel][me->target].mb_flags |= MLY_BTL_RESCAN;
	break;
    case 'l':		/* error on logical unit */
    case 'm':		/* message about logical unit */
	bus = MLY_LOGDEV_BUS(sc, me->lun);
	target = MLY_LOGDEV_TARGET(me->lun);
	mly_name_device(sc, bus, target);
	mly_printf(sc, "logical device %d (%s) %s\n", me->lun, sc->mly_btl[bus][target].mb_name, tp);
	if (action == 'r')
	    sc->mly_btl[bus][target].mb_flags |= MLY_BTL_RESCAN;
	break;
      break;
    case 's':		/* report of sense data */
	if (((ssd->flags & SSD_KEY) == SSD_KEY_NO_SENSE) ||
	    (((ssd->flags & SSD_KEY) == SSD_KEY_NOT_READY) && 
	     (ssd->add_sense_code == 0x04) && 
	     ((ssd->add_sense_code_qual == 0x01) || (ssd->add_sense_code_qual == 0x02))))
	    break;	/* ignore NO_SENSE or NOT_READY in one case */

	mly_printf(sc, "physical device %d:%d %s\n", me->channel, me->target, tp);
	mly_printf(sc, "  sense key %d  asc %02x  ascq %02x\n", 
		      ssd->flags & SSD_KEY, ssd->add_sense_code, ssd->add_sense_code_qual);
	mly_printf(sc, "  info %4D  csi %4D\n", ssd->info, "", ssd->cmd_spec_info, "");
	if (action == 'r')
	    sc->mly_btl[me->channel][me->target].mb_flags |= MLY_BTL_RESCAN;
	break;
    case 'e':
	mly_printf(sc, tp, me->target, me->lun);
	break;
    case 'c':
	mly_printf(sc, "controller %s\n", tp);
	break;
    case '?':
	mly_printf(sc, "%s - %d\n", tp, me->code);
	break;
    default:	/* probably a 'noisy' event being ignored */
	break;
    }
}

/********************************************************************************
 * Perform periodic activities.
 */
static void
mly_periodic(void *data)
{
    struct mly_softc	*sc = (struct mly_softc *)data;
    int			nchn, bus, target;

    debug_called(2);

    /*
     * Scan devices.
     */
    nchn = sc->mly_controllerinfo->physical_channels_present +
	sc->mly_controllerinfo->virtual_channels_present;
    for (bus = 0; bus < nchn; bus++) {
	for (target = 0; target < MLY_MAX_TARGETS; target++) {

	    /* ignore the controller in this scan */
	    if (target == sc->mly_controllerparam->initiator_id)
		continue;

	    /* perform device rescan? */
	    if (sc->mly_btl[bus][target].mb_flags & MLY_BTL_RESCAN)
		mly_rescan_btl(sc, bus, target);
	}
    }

    sc->mly_periodic = timeout(mly_periodic, sc, hz);
}

/********************************************************************************
 ********************************************************************************
                                                               Command Processing
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Run a command and wait for it to complete.
 *
 */
static int
mly_immediate_command(struct mly_command *mc)
{
    struct mly_softc	*sc = mc->mc_sc;
    int			error, s;

    debug_called(2);

    /* spinning at splcam is ugly, but we're only used during controller init */
    s = splcam();
    if ((error = mly_start(mc)))
	return(error);

    if (sc->mly_state & MLY_STATE_INTERRUPTS_ON) {
	/* sleep on the command */
	while(!(mc->mc_flags & MLY_CMD_COMPLETE)) {
	    tsleep(mc, PRIBIO, "mlywait", 0);
	}
    } else {
	/* spin and collect status while we do */
	while(!(mc->mc_flags & MLY_CMD_COMPLETE)) {
	    mly_done(mc->mc_sc);
	}
    }
    splx(s);
    return(0);
}

/********************************************************************************
 * Start as much queued I/O as possible on the controller
 */
void
mly_startio(struct mly_softc *sc)
{
    struct mly_command	*mc;

    debug_called(2);

    for (;;) {

	/* try for a ready command */
	mc = mly_dequeue_ready(sc);

	/* try to build a command from a queued ccb */
	if (!mc)
	    mly_cam_command(sc, &mc);

	/* no command == nothing to do */
	if (!mc)
	    break;

	/* try to post the command */
	if (mly_start(mc)) {
	    /* controller busy, or no resources - defer for later */
	    mly_requeue_ready(mc);
	    break;
	}
    }
}

/********************************************************************************
 * Deliver a command to the controller; allocate controller resources at the
 * last moment.
 */
static int
mly_start(struct mly_command *mc)
{
    struct mly_softc		*sc = mc->mc_sc;
    union mly_command_packet	*pkt;
    int				s;

    debug_called(2);

    /* 
     * Set the command up for delivery to the controller. 
     */
    mly_map_command(mc);
    mc->mc_packet->generic.command_id = mc->mc_slot;

    s = splcam();

    /*
     * Do we have to use the hardware mailbox?
     */
    if (!(sc->mly_state & MLY_STATE_MMBOX_ACTIVE)) {
	/*
	 * Check to see if the controller is ready for us.
	 */
	if (MLY_IDBR_TRUE(sc, MLY_HM_CMDSENT)) {
	    splx(s);
	    return(EBUSY);
	}
	mc->mc_flags |= MLY_CMD_BUSY;
	
	/*
	 * It's ready, send the command.
	 */
	MLY_SET_MBOX(sc, sc->mly_command_mailbox, &mc->mc_packetphys);
	MLY_SET_REG(sc, sc->mly_idbr, MLY_HM_CMDSENT);

    } else {	/* use memory-mailbox mode */

	pkt = &sc->mly_mmbox->mmm_command[sc->mly_mmbox_command_index];

	/* check to see if the next index is free yet */
	if (pkt->mmbox.flag != 0) {
	    splx(s);
	    return(EBUSY);
	}
	mc->mc_flags |= MLY_CMD_BUSY;
	
	/* copy in new command */
	bcopy(mc->mc_packet->mmbox.data, pkt->mmbox.data, sizeof(pkt->mmbox.data));
	/* barrier to ensure completion of previous write before we write the flag */
	bus_space_barrier(NULL, NULL, 0, 0, BUS_SPACE_BARRIER_WRITE);	/* tag/handle? */
	/* copy flag last */
	pkt->mmbox.flag = mc->mc_packet->mmbox.flag;
	/* barrier to ensure completion of previous write before we notify the controller */
	bus_space_barrier(NULL, NULL, 0, 0, BUS_SPACE_BARRIER_WRITE);	/* tag/handle */

	/* signal controller, update index */
	MLY_SET_REG(sc, sc->mly_idbr, MLY_AM_CMDSENT);
	sc->mly_mmbox_command_index = (sc->mly_mmbox_command_index + 1) % MLY_MMBOX_COMMANDS;
    }

    mly_enqueue_busy(mc);
    splx(s);
    return(0);
}

/********************************************************************************
 * Pick up command status from the controller, schedule a completion event
 */
void
mly_done(struct mly_softc *sc) 
{
    struct mly_command		*mc;
    union mly_status_packet	*sp;
    u_int16_t			slot;
    int				s, worked;

    s = splcam();
    worked = 0;

    /* pick up hardware-mailbox commands */
    if (MLY_ODBR_TRUE(sc, MLY_HM_STSREADY)) {
	slot = MLY_GET_REG2(sc, sc->mly_status_mailbox);
	if (slot < MLY_SLOT_MAX) {
	    mc = &sc->mly_command[slot - MLY_SLOT_START];
	    mc->mc_status = MLY_GET_REG(sc, sc->mly_status_mailbox + 2);
	    mc->mc_sense = MLY_GET_REG(sc, sc->mly_status_mailbox + 3);
	    mc->mc_resid = MLY_GET_REG4(sc, sc->mly_status_mailbox + 4);
	    mly_remove_busy(mc);
	    mc->mc_flags &= ~MLY_CMD_BUSY;
	    mly_enqueue_complete(mc);
	    worked = 1;
	} else {
	    /* slot 0xffff may mean "extremely bogus command" */
	    mly_printf(sc, "got HM completion for illegal slot %u\n", slot);
	}
	/* unconditionally acknowledge status */
	MLY_SET_REG(sc, sc->mly_odbr, MLY_HM_STSREADY);
	MLY_SET_REG(sc, sc->mly_idbr, MLY_HM_STSACK);
    }

    /* pick up memory-mailbox commands */
    if (MLY_ODBR_TRUE(sc, MLY_AM_STSREADY)) {
	for (;;) {
	    sp = &sc->mly_mmbox->mmm_status[sc->mly_mmbox_status_index];

	    /* check for more status */
	    if (sp->mmbox.flag == 0)
		break;

	    /* get slot number */
	    slot = sp->status.command_id;
	    if (slot < MLY_SLOT_MAX) {
		mc = &sc->mly_command[slot - MLY_SLOT_START];
		mc->mc_status = sp->status.status;
		mc->mc_sense = sp->status.sense_length;
		mc->mc_resid = sp->status.residue;
		mly_remove_busy(mc);
		mc->mc_flags &= ~MLY_CMD_BUSY;
		mly_enqueue_complete(mc);
		worked = 1;
	    } else {
		/* slot 0xffff may mean "extremely bogus command" */
		mly_printf(sc, "got AM completion for illegal slot %u at %d\n", 
			   slot, sc->mly_mmbox_status_index);
	    }

	    /* clear and move to next index */
	    sp->mmbox.flag = 0;
	    sc->mly_mmbox_status_index = (sc->mly_mmbox_status_index + 1) % MLY_MMBOX_STATUS;
	}
	/* acknowledge that we have collected status value(s) */
	MLY_SET_REG(sc, sc->mly_odbr, MLY_AM_STSREADY);
    }

    splx(s);
    if (worked) {
#if __FreeBSD_version >= 500005
	if (sc->mly_state & MLY_STATE_INTERRUPTS_ON)
	    taskqueue_enqueue(taskqueue_swi, &sc->mly_task_complete);
	else
#endif
	    mly_complete(sc, 0);
    }
}

/********************************************************************************
 * Process completed commands
 */
static void
mly_complete(void *context, int pending)
{
    struct mly_softc	*sc = (struct mly_softc *)context;
    struct mly_command	*mc;
    void	        (* mc_complete)(struct mly_command *mc);


    debug_called(2);

    /* 
     * Spin pulling commands off the completed queue and processing them.
     */
    while ((mc = mly_dequeue_complete(sc)) != NULL) {

	/*
	 * Free controller resources, mark command complete.
	 *
	 * Note that as soon as we mark the command complete, it may be freed
	 * out from under us, so we need to save the mc_complete field in
	 * order to later avoid dereferencing mc.  (We would not expect to
	 * have a polling/sleeping consumer with mc_complete != NULL).
	 */
	mly_unmap_command(mc);
	mc_complete = mc->mc_complete;
	mc->mc_flags |= MLY_CMD_COMPLETE;

	/* 
	 * Call completion handler or wake up sleeping consumer.
	 */
	if (mc_complete != NULL) {
	    mc_complete(mc);
	} else {
	    wakeup(mc);
	}
    }

    /*
     * We may have freed up controller resources which would allow us
     * to push more commands onto the controller, so we check here.
     */
    mly_startio(sc);

    /*
     * The controller may have updated the health status information,
     * so check for it here.
     *
     * Note that we only check for health status after a completed command.  It
     * might be wise to ping the controller occasionally if it's been idle for
     * a while just to check up on it.  While a filesystem is mounted, or I/O is
     * active this isn't really an issue.
     */
    if (sc->mly_mmbox->mmm_health.status.change_counter != sc->mly_event_change) {
	sc->mly_event_change = sc->mly_mmbox->mmm_health.status.change_counter;
	debug(1, "event change %d, event status update, %d -> %d", sc->mly_event_change,
	      sc->mly_event_waiting, sc->mly_mmbox->mmm_health.status.next_event);
	sc->mly_event_waiting = sc->mly_mmbox->mmm_health.status.next_event;

	/* wake up anyone that might be interested in this */
	wakeup(&sc->mly_event_change);
    }
    if (sc->mly_event_counter != sc->mly_event_waiting)
	mly_fetch_event(sc);
}

/********************************************************************************
 ********************************************************************************
                                                        Command Buffer Management
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Allocate a command.
 */
int
mly_alloc_command(struct mly_softc *sc, struct mly_command **mcp)
{
    struct mly_command	*mc;

    debug_called(3);

    if ((mc = mly_dequeue_free(sc)) == NULL)
	return(ENOMEM);

    *mcp = mc;
    return(0);
}

/********************************************************************************
 * Release a command back to the freelist.
 */
void
mly_release_command(struct mly_command *mc)
{
    debug_called(3);

    /*
     * Fill in parts of the command that may cause confusion if
     * a consumer doesn't when we are later allocated.
     */
    mc->mc_data = NULL;
    mc->mc_flags = 0;
    mc->mc_complete = NULL;
    mc->mc_private = NULL;

    /*
     * By default, we set up to overwrite the command packet with
     * sense information.
     */
    mc->mc_packet->generic.sense_buffer_address = mc->mc_packetphys;
    mc->mc_packet->generic.maximum_sense_size = sizeof(union mly_command_packet);

    mly_enqueue_free(mc);
}

/********************************************************************************
 * Map helper for command allocation.
 */
static void
mly_alloc_commands_map(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
    struct mly_softc	*sc = (struct mly_softc *)arg

    debug_called(2);

    sc->mly_packetphys = segs[0].ds_addr;
}

/********************************************************************************
 * Allocate and initialise command and packet structures.
 */
static int
mly_alloc_commands(struct mly_softc *sc)
{
    struct mly_command		*mc;
    int				i;
 
    /*
     * Allocate enough space for all the command packets in one chunk and
     * map them permanently into controller-visible space.
     */
    if (bus_dmamem_alloc(sc->mly_packet_dmat, (void **)&sc->mly_packet, 
			 BUS_DMA_NOWAIT, &sc->mly_packetmap)) {
	return(ENOMEM);
    }
    bus_dmamap_load(sc->mly_packet_dmat, sc->mly_packetmap, sc->mly_packet, 
		    MLY_MAXCOMMANDS * sizeof(union mly_command_packet), 
		    mly_alloc_commands_map, sc, 0);

    for (i = 0; i < MLY_MAXCOMMANDS; i++) {
	mc = &sc->mly_command[i];
	bzero(mc, sizeof(*mc));
	mc->mc_sc = sc;
	mc->mc_slot = MLY_SLOT_START + i;
	mc->mc_packet = sc->mly_packet + i;
	mc->mc_packetphys = sc->mly_packetphys + (i * sizeof(union mly_command_packet));
	if (!bus_dmamap_create(sc->mly_buffer_dmat, 0, &mc->mc_datamap))
	    mly_release_command(mc);
    }
    return(0);
}

/********************************************************************************
 * Command-mapping helper function - populate this command's s/g table
 * with the s/g entries for its data.
 */
static void
mly_map_command_sg(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
    struct mly_command		*mc = (struct mly_command *)arg;
    struct mly_softc		*sc = mc->mc_sc;
    struct mly_command_generic	*gen = &(mc->mc_packet->generic);
    struct mly_sg_entry		*sg;
    int				i, tabofs;

    debug_called(3);

    /* can we use the transfer structure directly? */
    if (nseg <= 2) {
	sg = &gen->transfer.direct.sg[0];
	gen->command_control.extended_sg_table = 0;
    } else {
	tabofs = ((mc->mc_slot - MLY_SLOT_START) * MLY_MAXSGENTRIES);
	sg = sc->mly_sg_table + tabofs;
	gen->transfer.indirect.entries[0] = nseg;
	gen->transfer.indirect.table_physaddr[0] = sc->mly_sg_busaddr + (tabofs * sizeof(struct mly_sg_entry));
	gen->command_control.extended_sg_table = 1;
    }

    /* copy the s/g table */
    for (i = 0; i < nseg; i++) {
	sg[i].physaddr = segs[i].ds_addr;
	sg[i].length = segs[i].ds_len;
    }

}

#if 0
/********************************************************************************
 * Command-mapping helper function - save the cdb's physical address.
 *
 * We don't support 'large' SCSI commands at this time, so this is unused.
 */
static void
mly_map_command_cdb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
    struct mly_command			*mc = (struct mly_command *)arg;

    debug_called(3);

    /* XXX can we safely assume that a CDB will never cross a page boundary? */
    if ((segs[0].ds_addr % PAGE_SIZE) > 
	((segs[0].ds_addr + mc->mc_packet->scsi_large.cdb_length) % PAGE_SIZE))
	panic("cdb crosses page boundary");

    /* fix up fields in the command packet */
    mc->mc_packet->scsi_large.cdb_physaddr = segs[0].ds_addr;
}
#endif

/********************************************************************************
 * Map a command into controller-visible space
 */
static void
mly_map_command(struct mly_command *mc)
{
    struct mly_softc	*sc = mc->mc_sc;

    debug_called(2);

    /* don't map more than once */
    if (mc->mc_flags & MLY_CMD_MAPPED)
	return;

    /* does the command have a data buffer? */
    if (mc->mc_data != NULL)
	bus_dmamap_load(sc->mly_buffer_dmat, mc->mc_datamap, mc->mc_data, mc->mc_length, 
			mly_map_command_sg, mc, 0);
	
    if (mc->mc_flags & MLY_CMD_DATAIN)
	bus_dmamap_sync(sc->mly_buffer_dmat, mc->mc_datamap, BUS_DMASYNC_PREREAD);
    if (mc->mc_flags & MLY_CMD_DATAOUT)
	bus_dmamap_sync(sc->mly_buffer_dmat, mc->mc_datamap, BUS_DMASYNC_PREWRITE);

    mc->mc_flags |= MLY_CMD_MAPPED;
}

/********************************************************************************
 * Unmap a command from controller-visible space
 */
static void
mly_unmap_command(struct mly_command *mc)
{
    struct mly_softc	*sc = mc->mc_sc;

    debug_called(2);

    if (!(mc->mc_flags & MLY_CMD_MAPPED))
	return;

    if (mc->mc_flags & MLY_CMD_DATAIN)
	bus_dmamap_sync(sc->mly_buffer_dmat, mc->mc_datamap, BUS_DMASYNC_POSTREAD);
    if (mc->mc_flags & MLY_CMD_DATAOUT)
	bus_dmamap_sync(sc->mly_buffer_dmat, mc->mc_datamap, BUS_DMASYNC_POSTWRITE);

    /* does the command have a data buffer? */
    if (mc->mc_data != NULL)
	bus_dmamap_unload(sc->mly_buffer_dmat, mc->mc_datamap);

    mc->mc_flags &= ~MLY_CMD_MAPPED;
}

/********************************************************************************
 ********************************************************************************
                                                                 Hardware Control
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Handshake with the firmware while the card is being initialised.
 */
static int
mly_fwhandshake(struct mly_softc *sc) 
{
    u_int8_t	error, param0, param1;
    int		spinup = 0;

    debug_called(1);

    /* set HM_STSACK and let the firmware initialise */
    MLY_SET_REG(sc, sc->mly_idbr, MLY_HM_STSACK);
    DELAY(1000);	/* too short? */

    /* if HM_STSACK is still true, the controller is initialising */
    if (!MLY_IDBR_TRUE(sc, MLY_HM_STSACK))
	return(0);
    mly_printf(sc, "controller initialisation started\n");

    /* spin waiting for initialisation to finish, or for a message to be delivered */
    while (MLY_IDBR_TRUE(sc, MLY_HM_STSACK)) {
	/* check for a message */
	if (MLY_ERROR_VALID(sc)) {
	    error = MLY_GET_REG(sc, sc->mly_error_status) & ~MLY_MSG_EMPTY;
	    param0 = MLY_GET_REG(sc, sc->mly_command_mailbox);
	    param1 = MLY_GET_REG(sc, sc->mly_command_mailbox + 1);

	    switch(error) {
	    case MLY_MSG_SPINUP:
		if (!spinup) {
		    mly_printf(sc, "drive spinup in progress\n");
		    spinup = 1;			/* only print this once (should print drive being spun?) */
		}
		break;
	    case MLY_MSG_RACE_RECOVERY_FAIL:
		mly_printf(sc, "mirror race recovery failed, one or more drives offline\n");
		break;
	    case MLY_MSG_RACE_IN_PROGRESS:
		mly_printf(sc, "mirror race recovery in progress\n");
		break;
	    case MLY_MSG_RACE_ON_CRITICAL:
		mly_printf(sc, "mirror race recovery on a critical drive\n");
		break;
	    case MLY_MSG_PARITY_ERROR:
		mly_printf(sc, "FATAL MEMORY PARITY ERROR\n");
		return(ENXIO);
	    default:
		mly_printf(sc, "unknown initialisation code 0x%x\n", error);
	    }
	}
    }
    return(0);
}

/********************************************************************************
 ********************************************************************************
                                                        Debugging and Diagnostics
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Print some information about the controller.
 */
static void
mly_describe_controller(struct mly_softc *sc)
{
    struct mly_ioctl_getcontrollerinfo	*mi = sc->mly_controllerinfo;

    mly_printf(sc, "%16s, %d channel%s, firmware %d.%02d-%d-%02d (%02d%02d%02d%02d), %dMB RAM\n", 
	       mi->controller_name, mi->physical_channels_present, (mi->physical_channels_present) > 1 ? "s" : "",
	       mi->fw_major, mi->fw_minor, mi->fw_turn, mi->fw_build,	/* XXX turn encoding? */
	       mi->fw_century, mi->fw_year, mi->fw_month, mi->fw_day,
	       mi->memory_size);

    if (bootverbose) {
	mly_printf(sc, "%s %s (%x), %dMHz %d-bit %.16s\n", 
		   mly_describe_code(mly_table_oemname, mi->oem_information), 
		   mly_describe_code(mly_table_controllertype, mi->controller_type), mi->controller_type,
		   mi->interface_speed, mi->interface_width, mi->interface_name);
	mly_printf(sc, "%dMB %dMHz %d-bit %s%s%s, cache %dMB\n",
		   mi->memory_size, mi->memory_speed, mi->memory_width, 
		   mly_describe_code(mly_table_memorytype, mi->memory_type),
		   mi->memory_parity ? "+parity": "",mi->memory_ecc ? "+ECC": "",
		   mi->cache_size);
	mly_printf(sc, "CPU: %s @ %dMHZ\n", 
		   mly_describe_code(mly_table_cputype, mi->cpu[0].type), mi->cpu[0].speed);
	if (mi->l2cache_size != 0)
	    mly_printf(sc, "%dKB L2 cache\n", mi->l2cache_size);
	if (mi->exmemory_size != 0)
	    mly_printf(sc, "%dMB %dMHz %d-bit private %s%s%s\n",
		       mi->exmemory_size, mi->exmemory_speed, mi->exmemory_width,
		       mly_describe_code(mly_table_memorytype, mi->exmemory_type),
		       mi->exmemory_parity ? "+parity": "",mi->exmemory_ecc ? "+ECC": "");
	mly_printf(sc, "battery backup %s\n", mi->bbu_present ? "present" : "not installed");
	mly_printf(sc, "maximum data transfer %d blocks, maximum sg entries/command %d\n",
		   mi->maximum_block_count, mi->maximum_sg_entries);
	mly_printf(sc, "logical devices present/critical/offline %d/%d/%d\n",
		   mi->logical_devices_present, mi->logical_devices_critical, mi->logical_devices_offline);
	mly_printf(sc, "physical devices present %d\n",
		   mi->physical_devices_present);
	mly_printf(sc, "physical disks present/offline %d/%d\n",
		   mi->physical_disks_present, mi->physical_disks_offline);
	mly_printf(sc, "%d physical channel%s, %d virtual channel%s of %d possible\n",
		   mi->physical_channels_present, mi->physical_channels_present == 1 ? "" : "s",
		   mi->virtual_channels_present, mi->virtual_channels_present == 1 ? "" : "s",
		   mi->virtual_channels_possible);
	mly_printf(sc, "%d parallel commands supported\n", mi->maximum_parallel_commands);
	mly_printf(sc, "%dMB flash ROM, %d of %d maximum cycles\n",
		   mi->flash_size, mi->flash_age, mi->flash_maximum_age);
    }
}

#ifdef MLY_DEBUG
/********************************************************************************
 * Print some controller state
 */
static void
mly_printstate(struct mly_softc *sc)
{
    mly_printf(sc, "IDBR %02x  ODBR %02x  ERROR %02x  (%x %x %x)\n",
		  MLY_GET_REG(sc, sc->mly_idbr),
		  MLY_GET_REG(sc, sc->mly_odbr),
		  MLY_GET_REG(sc, sc->mly_error_status),
		  sc->mly_idbr,
		  sc->mly_odbr,
		  sc->mly_error_status);
    mly_printf(sc, "IMASK %02x  ISTATUS %02x\n",
		  MLY_GET_REG(sc, sc->mly_interrupt_mask),
		  MLY_GET_REG(sc, sc->mly_interrupt_status));
    mly_printf(sc, "COMMAND %02x %02x %02x %02x %02x %02x %02x %02x\n",
		  MLY_GET_REG(sc, sc->mly_command_mailbox),
		  MLY_GET_REG(sc, sc->mly_command_mailbox + 1),
		  MLY_GET_REG(sc, sc->mly_command_mailbox + 2),
		  MLY_GET_REG(sc, sc->mly_command_mailbox + 3),
		  MLY_GET_REG(sc, sc->mly_command_mailbox + 4),
		  MLY_GET_REG(sc, sc->mly_command_mailbox + 5),
		  MLY_GET_REG(sc, sc->mly_command_mailbox + 6),
		  MLY_GET_REG(sc, sc->mly_command_mailbox + 7));
    mly_printf(sc, "STATUS  %02x %02x %02x %02x %02x %02x %02x %02x\n",
		  MLY_GET_REG(sc, sc->mly_status_mailbox),
		  MLY_GET_REG(sc, sc->mly_status_mailbox + 1),
		  MLY_GET_REG(sc, sc->mly_status_mailbox + 2),
		  MLY_GET_REG(sc, sc->mly_status_mailbox + 3),
		  MLY_GET_REG(sc, sc->mly_status_mailbox + 4),
		  MLY_GET_REG(sc, sc->mly_status_mailbox + 5),
		  MLY_GET_REG(sc, sc->mly_status_mailbox + 6),
		  MLY_GET_REG(sc, sc->mly_status_mailbox + 7));
    mly_printf(sc, "        %04x        %08x\n",
		  MLY_GET_REG2(sc, sc->mly_status_mailbox),
		  MLY_GET_REG4(sc, sc->mly_status_mailbox + 4));
}

struct mly_softc	*mly_softc0 = NULL;
void
mly_printstate0(void)
{
    if (mly_softc0 != NULL)
	mly_printstate(mly_softc0);
}

/********************************************************************************
 * Print a command
 */
static void
mly_print_command(struct mly_command *mc)
{
    struct mly_softc	*sc = mc->mc_sc;
    
    mly_printf(sc, "COMMAND @ %p\n", mc);
    mly_printf(sc, "  slot      %d\n", mc->mc_slot);
    mly_printf(sc, "  status    0x%x\n", mc->mc_status);
    mly_printf(sc, "  sense len %d\n", mc->mc_sense);
    mly_printf(sc, "  resid     %d\n", mc->mc_resid);
    mly_printf(sc, "  packet    %p/0x%llx\n", mc->mc_packet, mc->mc_packetphys);
    if (mc->mc_packet != NULL)
	mly_print_packet(mc);
    mly_printf(sc, "  data      %p/%d\n", mc->mc_data, mc->mc_length);
    mly_printf(sc, "  flags     %b\n", mc->mc_flags, "\20\1busy\2complete\3slotted\4mapped\5datain\6dataout\n");
    mly_printf(sc, "  complete  %p\n", mc->mc_complete);
    mly_printf(sc, "  private   %p\n", mc->mc_private);
}

/********************************************************************************
 * Print a command packet
 */
static void
mly_print_packet(struct mly_command *mc)
{
    struct mly_softc			*sc = mc->mc_sc;
    struct mly_command_generic		*ge = (struct mly_command_generic *)mc->mc_packet;
    struct mly_command_scsi_small	*ss = (struct mly_command_scsi_small *)mc->mc_packet;
    struct mly_command_scsi_large	*sl = (struct mly_command_scsi_large *)mc->mc_packet;
    struct mly_command_ioctl		*io = (struct mly_command_ioctl *)mc->mc_packet;
    int					transfer;

    mly_printf(sc, "   command_id           %d\n", ge->command_id);
    mly_printf(sc, "   opcode               %d\n", ge->opcode);
    mly_printf(sc, "   command_control      fua %d  dpo %d  est %d  dd %s  nas %d ddis %d\n",
		  ge->command_control.force_unit_access,
		  ge->command_control.disable_page_out,
		  ge->command_control.extended_sg_table,
		  (ge->command_control.data_direction == MLY_CCB_WRITE) ? "WRITE" : "READ",
		  ge->command_control.no_auto_sense,
		  ge->command_control.disable_disconnect);
    mly_printf(sc, "   data_size            %d\n", ge->data_size);
    mly_printf(sc, "   sense_buffer_address 0x%llx\n", ge->sense_buffer_address);
    mly_printf(sc, "   lun                  %d\n", ge->addr.phys.lun);
    mly_printf(sc, "   target               %d\n", ge->addr.phys.target);
    mly_printf(sc, "   channel              %d\n", ge->addr.phys.channel);
    mly_printf(sc, "   logical device       %d\n", ge->addr.log.logdev);
    mly_printf(sc, "   controller           %d\n", ge->addr.phys.controller);
    mly_printf(sc, "   timeout              %d %s\n", 
		  ge->timeout.value,
		  (ge->timeout.scale == MLY_TIMEOUT_SECONDS) ? "seconds" : 
		  ((ge->timeout.scale == MLY_TIMEOUT_MINUTES) ? "minutes" : "hours"));
    mly_printf(sc, "   maximum_sense_size   %d\n", ge->maximum_sense_size);
    switch(ge->opcode) {
    case MDACMD_SCSIPT:
    case MDACMD_SCSI:
	mly_printf(sc, "   cdb length           %d\n", ss->cdb_length);
	mly_printf(sc, "   cdb                  %*D\n", ss->cdb_length, ss->cdb, " ");
	transfer = 1;
	break;
    case MDACMD_SCSILC:
    case MDACMD_SCSILCPT:
	mly_printf(sc, "   cdb length           %d\n", sl->cdb_length);
	mly_printf(sc, "   cdb                  0x%llx\n", sl->cdb_physaddr);
	transfer = 1;
	break;
    case MDACMD_IOCTL:
	mly_printf(sc, "   sub_ioctl            0x%x\n", io->sub_ioctl);
	switch(io->sub_ioctl) {
	case MDACIOCTL_SETMEMORYMAILBOX:
	    mly_printf(sc, "   health_buffer_size   %d\n", 
			  io->param.setmemorymailbox.health_buffer_size);
	    mly_printf(sc, "   health_buffer_phys   0x%llx\n",
			  io->param.setmemorymailbox.health_buffer_physaddr);
	    mly_printf(sc, "   command_mailbox      0x%llx\n",
			  io->param.setmemorymailbox.command_mailbox_physaddr);
	    mly_printf(sc, "   status_mailbox       0x%llx\n",
			  io->param.setmemorymailbox.status_mailbox_physaddr);
	    transfer = 0;
	    break;

	case MDACIOCTL_SETREALTIMECLOCK:
	case MDACIOCTL_GETHEALTHSTATUS:
	case MDACIOCTL_GETCONTROLLERINFO:
	case MDACIOCTL_GETLOGDEVINFOVALID:
	case MDACIOCTL_GETPHYSDEVINFOVALID:
	case MDACIOCTL_GETPHYSDEVSTATISTICS:
	case MDACIOCTL_GETLOGDEVSTATISTICS:
	case MDACIOCTL_GETCONTROLLERSTATISTICS:
	case MDACIOCTL_GETBDT_FOR_SYSDRIVE:	    
	case MDACIOCTL_CREATENEWCONF:
	case MDACIOCTL_ADDNEWCONF:
	case MDACIOCTL_GETDEVCONFINFO:
	case MDACIOCTL_GETFREESPACELIST:
	case MDACIOCTL_MORE:
	case MDACIOCTL_SETPHYSDEVPARAMETER:
	case MDACIOCTL_GETPHYSDEVPARAMETER:
	case MDACIOCTL_GETLOGDEVPARAMETER:
	case MDACIOCTL_SETLOGDEVPARAMETER:
	    mly_printf(sc, "   param                %10D\n", io->param.data.param, " ");
	    transfer = 1;
	    break;

	case MDACIOCTL_GETEVENT:
	    mly_printf(sc, "   event                %d\n", 
		       io->param.getevent.sequence_number_low + ((u_int32_t)io->addr.log.logdev << 16));
	    transfer = 1;
	    break;

	case MDACIOCTL_SETRAIDDEVSTATE:
	    mly_printf(sc, "   state                %d\n", io->param.setraiddevstate.state);
	    transfer = 0;
	    break;

	case MDACIOCTL_XLATEPHYSDEVTORAIDDEV:
	    mly_printf(sc, "   raid_device          %d\n", io->param.xlatephysdevtoraiddev.raid_device);
	    mly_printf(sc, "   controller           %d\n", io->param.xlatephysdevtoraiddev.controller);
	    mly_printf(sc, "   channel              %d\n", io->param.xlatephysdevtoraiddev.channel);
	    mly_printf(sc, "   target               %d\n", io->param.xlatephysdevtoraiddev.target);
	    mly_printf(sc, "   lun                  %d\n", io->param.xlatephysdevtoraiddev.lun);
	    transfer = 0;
	    break;

	case MDACIOCTL_GETGROUPCONFINFO:
	    mly_printf(sc, "   group                %d\n", io->param.getgroupconfinfo.group);
	    transfer = 1;
	    break;

	case MDACIOCTL_GET_SUBSYSTEM_DATA:
	case MDACIOCTL_SET_SUBSYSTEM_DATA:
	case MDACIOCTL_STARTDISOCVERY:
	case MDACIOCTL_INITPHYSDEVSTART:
	case MDACIOCTL_INITPHYSDEVSTOP:
	case MDACIOCTL_INITRAIDDEVSTART:
	case MDACIOCTL_INITRAIDDEVSTOP:
	case MDACIOCTL_REBUILDRAIDDEVSTART:
	case MDACIOCTL_REBUILDRAIDDEVSTOP:
	case MDACIOCTL_MAKECONSISTENTDATASTART:
	case MDACIOCTL_MAKECONSISTENTDATASTOP:
	case MDACIOCTL_CONSISTENCYCHECKSTART:
	case MDACIOCTL_CONSISTENCYCHECKSTOP:
	case MDACIOCTL_RESETDEVICE:
	case MDACIOCTL_FLUSHDEVICEDATA:
	case MDACIOCTL_PAUSEDEVICE:
	case MDACIOCTL_UNPAUSEDEVICE:
	case MDACIOCTL_LOCATEDEVICE:
	case MDACIOCTL_SETMASTERSLAVEMODE:
	case MDACIOCTL_DELETERAIDDEV:
	case MDACIOCTL_REPLACEINTERNALDEV:
	case MDACIOCTL_CLEARCONF:
	case MDACIOCTL_GETCONTROLLERPARAMETER:
	case MDACIOCTL_SETCONTRLLERPARAMETER:
	case MDACIOCTL_CLEARCONFSUSPMODE:
	case MDACIOCTL_STOREIMAGE:
	case MDACIOCTL_READIMAGE:
	case MDACIOCTL_FLASHIMAGES:
	case MDACIOCTL_RENAMERAIDDEV:
	default:			/* no idea what to print */
	    transfer = 0;
	    break;
	}
	break;

    case MDACMD_IOCTLCHECK:
    case MDACMD_MEMCOPY:
    default:
	transfer = 0;
	break;	/* print nothing */
    }
    if (transfer) {
	if (ge->command_control.extended_sg_table) {
	    mly_printf(sc, "   sg table             0x%llx/%d\n",
			  ge->transfer.indirect.table_physaddr[0], ge->transfer.indirect.entries[0]);
	} else {
	    mly_printf(sc, "   0000                 0x%llx/%lld\n",
			  ge->transfer.direct.sg[0].physaddr, ge->transfer.direct.sg[0].length);
	    mly_printf(sc, "   0001                 0x%llx/%lld\n",
			  ge->transfer.direct.sg[1].physaddr, ge->transfer.direct.sg[1].length);
	}
    }
}

/********************************************************************************
 * Panic in a slightly informative fashion
 */
static void
mly_panic(struct mly_softc *sc, char *reason)
{
    mly_printstate(sc);
    panic(reason);
}
#endif

/********************************************************************************
 * Print queue statistics, callable from DDB.
 */
void
mly_print_controller(int controller)
{
    struct mly_softc	*sc;
    
    if ((sc = devclass_get_softc(devclass_find("mly"), controller)) == NULL) {
	printf("mly: controller %d invalid\n", controller);
    } else {
	device_printf(sc->mly_dev, "queue    curr max\n");
	device_printf(sc->mly_dev, "free     %04d/%04d\n", 
		      sc->mly_qstat[MLYQ_FREE].q_length, sc->mly_qstat[MLYQ_FREE].q_max);
	device_printf(sc->mly_dev, "ready    %04d/%04d\n", 
		      sc->mly_qstat[MLYQ_READY].q_length, sc->mly_qstat[MLYQ_READY].q_max);
	device_printf(sc->mly_dev, "busy     %04d/%04d\n", 
		      sc->mly_qstat[MLYQ_BUSY].q_length, sc->mly_qstat[MLYQ_BUSY].q_max);
	device_printf(sc->mly_dev, "complete %04d/%04d\n", 
		      sc->mly_qstat[MLYQ_COMPLETE].q_length, sc->mly_qstat[MLYQ_COMPLETE].q_max);
    }
}


/********************************************************************************
 ********************************************************************************
                                                         Control device interface
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Accept an open operation on the control device.
 */
static int
mly_user_open(dev_t dev, int flags, int fmt, struct proc *p)
{
    int			unit = minor(dev);
    struct mly_softc	*sc = devclass_get_softc(devclass_find("mly"), unit);

    sc->mly_state |= MLY_STATE_OPEN;
    return(0);
}

/********************************************************************************
 * Accept the last close on the control device.
 */
static int
mly_user_close(dev_t dev, int flags, int fmt, struct proc *p)
{
    int			unit = minor(dev);
    struct mly_softc	*sc = devclass_get_softc(devclass_find("mly"), unit);

    sc->mly_state &= ~MLY_STATE_OPEN;
    return (0);
}

/********************************************************************************
 * Handle controller-specific control operations.
 */
static int
mly_user_ioctl(dev_t dev, u_long cmd, caddr_t addr, int32_t flag, struct proc *p)
{
    struct mly_softc		*sc = (struct mly_softc *)dev->si_drv1;
    struct mly_user_command	*uc = (struct mly_user_command *)addr;
    struct mly_user_health	*uh = (struct mly_user_health *)addr;
    
    switch(cmd) {
    case MLYIO_COMMAND:
	return(mly_user_command(sc, uc));
    case MLYIO_HEALTH:
	return(mly_user_health(sc, uh));
    default:
	return(ENOIOCTL);
    }
}

/********************************************************************************
 * Execute a command passed in from userspace.
 *
 * The control structure contains the actual command for the controller, as well
 * as the user-space data pointer and data size, and an optional sense buffer
 * size/pointer.  On completion, the data size is adjusted to the command
 * residual, and the sense buffer size to the size of the returned sense data.
 * 
 */
static int
mly_user_command(struct mly_softc *sc, struct mly_user_command *uc)
{
    struct mly_command			*mc;
    int					error, s;

    /* allocate a command */
    if (mly_alloc_command(sc, &mc)) {
	error = ENOMEM;
	goto out;		/* XXX Linux version will wait for a command */
    }

    /* handle data size/direction */
    mc->mc_length = (uc->DataTransferLength >= 0) ? uc->DataTransferLength : -uc->DataTransferLength;
    if (mc->mc_length > 0) {
	if ((mc->mc_data = malloc(mc->mc_length, M_DEVBUF, M_NOWAIT)) == NULL) {
	    error = ENOMEM;
	    goto out;
	}
    }
    if (uc->DataTransferLength > 0) {
	mc->mc_flags |= MLY_CMD_DATAIN;
	bzero(mc->mc_data, mc->mc_length);
    }
    if (uc->DataTransferLength < 0) {
	mc->mc_flags |= MLY_CMD_DATAOUT;
	if ((error = copyin(uc->DataTransferBuffer, mc->mc_data, mc->mc_length)) != 0)
	    goto out;
    }

    /* copy the controller command */
    bcopy(&uc->CommandMailbox, mc->mc_packet, sizeof(uc->CommandMailbox));

    /* clear command completion handler so that we get woken up */
    mc->mc_complete = NULL;

    /* execute the command */
    s = splcam();
    mly_requeue_ready(mc);
    mly_startio(sc);
    while (!(mc->mc_flags & MLY_CMD_COMPLETE))
	tsleep(mc, PRIBIO, "mlyioctl", 0);
    splx(s);

    /* return the data to userspace */
    if (uc->DataTransferLength > 0)
	if ((error = copyout(mc->mc_data, uc->DataTransferBuffer, mc->mc_length)) != 0)
	    goto out;
    
    /* return the sense buffer to userspace */
    if ((uc->RequestSenseLength > 0) && (mc->mc_sense > 0)) {
	if ((error = copyout(mc->mc_packet, uc->RequestSenseBuffer, 
			     min(uc->RequestSenseLength, mc->mc_sense))) != 0)
	    goto out;
    }
    
    /* return command results to userspace (caller will copy out) */
    uc->DataTransferLength = mc->mc_resid;
    uc->RequestSenseLength = min(uc->RequestSenseLength, mc->mc_sense);
    uc->CommandStatus = mc->mc_status;
    error = 0;

 out:
    if (mc->mc_data != NULL)
	free(mc->mc_data, M_DEVBUF);
    if (mc != NULL)
	mly_release_command(mc);
    return(error);
}

/********************************************************************************
 * Return health status to userspace.  If the health change index in the user
 * structure does not match that currently exported by the controller, we
 * return the current status immediately.  Otherwise, we block until either
 * interrupted or new status is delivered.
 */
static int
mly_user_health(struct mly_softc *sc, struct mly_user_health *uh)
{
    struct mly_health_status		mh;
    int					error, s;
    
    /* fetch the current health status from userspace */
    if ((error = copyin(uh->HealthStatusBuffer, &mh, sizeof(mh))) != 0)
	return(error);

    /* spin waiting for a status update */
    s = splcam();
    error = EWOULDBLOCK;
    while ((error != 0) && (sc->mly_event_change == mh.change_counter))
	error = tsleep(&sc->mly_event_change, PRIBIO | PCATCH, "mlyhealth", 0);
    splx(s);
    
    /* copy the controller's health status buffer out (there is a race here if it changes again) */
    error = copyout(&sc->mly_mmbox->mmm_health.status, uh->HealthStatusBuffer, 
		    sizeof(uh->HealthStatusBuffer));
    return(error);
}
