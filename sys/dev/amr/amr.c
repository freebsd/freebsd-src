/*-
 * Copyright (c) 1999,2000 Michael Smith
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

/*
 * Driver for the AMI MegaRaid family of controllers.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>

#include <dev/amr/amr_compat.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/devicestat.h>
#include <sys/disk.h>
#include <sys/stat.h>

#include <machine/bus_memio.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/clock.h>
#include <sys/rman.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <dev/amr/amrio.h>
#include <dev/amr/amrreg.h>
#include <dev/amr/amrvar.h>
#define AMR_DEFINE_TABLES
#include <dev/amr/amr_tables.h>

#define AMR_CDEV_MAJOR	132

static d_open_t         amr_open;
static d_close_t        amr_close;
static d_ioctl_t        amr_ioctl;

static struct cdevsw amr_cdevsw = {
		/* open */	amr_open,
		/* close */	amr_close,
		/* read */	noread,
		/* write */	nowrite,
		/* ioctl */	amr_ioctl,
		/* poll */	nopoll,
		/* mmap */	nommap,
		/* strategy */	nostrategy,
		/* name */ 	"amr",
		/* maj */	AMR_CDEV_MAJOR,
		/* dump */	nodump,
		/* psize */ 	nopsize,
		/* flags */	0,
		/* bmaj */	254	/* XXX magic no-bdev */
};

/*
 * Initialisation, bus interface.
 */
static void	amr_startup(void *arg);

/*
 * Command wrappers
 */
static int	amr_query_controller(struct amr_softc *sc);
static void	*amr_enquiry(struct amr_softc *sc, size_t bufsize, 
			     u_int8_t cmd, u_int8_t cmdsub, u_int8_t cmdqual);
static void	amr_completeio(struct amr_command *ac);

/*
 * Command buffer allocation.
 */
static void	amr_alloccmd_cluster(struct amr_softc *sc);
static void	amr_freecmd_cluster(struct amr_command_cluster *acc);

/*
 * Command processing.
 */
static int	amr_bio_command(struct amr_softc *sc, struct amr_command **acp);
static int	amr_wait_command(struct amr_command *ac);
static int	amr_poll_command(struct amr_command *ac);
static int	amr_getslot(struct amr_command *ac);
static void	amr_mapcmd(struct amr_command *ac);
static void	amr_unmapcmd(struct amr_command *ac);
static int	amr_start(struct amr_command *ac);
static void	amr_complete(void *context, int pending);

/*
 * Status monitoring
 */
static void	amr_periodic(void *data);

/*
 * Interface-specific shims
 */
static int	amr_quartz_submit_command(struct amr_softc *sc);
static int	amr_quartz_get_work(struct amr_softc *sc, struct amr_mailbox *mbsave);

static int	amr_std_submit_command(struct amr_softc *sc);
static int	amr_std_get_work(struct amr_softc *sc, struct amr_mailbox *mbsave);
static void	amr_std_attach_mailbox(struct amr_softc *sc);

#ifdef AMR_BOARD_INIT
static int	amr_quartz_init(struct amr_softc *sc);
static int	amr_std_init(struct amr_softc *sc);
#endif

/*
 * Debugging
 */
static void	amr_describe_controller(struct amr_softc *sc);
#ifdef AMR_DEBUG
static void	amr_printcommand(struct amr_command *ac);
#endif

/********************************************************************************
 ********************************************************************************
                                                                      Inline Glue
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 ********************************************************************************
                                                                Public Interfaces
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Initialise the controller and softc.
 */
int
amr_attach(struct amr_softc *sc)
{

    debug_called(1);

    /*
     * Initialise per-controller queues.
     */
    TAILQ_INIT(&sc->amr_completed);
    TAILQ_INIT(&sc->amr_freecmds);
    TAILQ_INIT(&sc->amr_cmd_clusters);
    TAILQ_INIT(&sc->amr_ready);
    bioq_init(&sc->amr_bioq);

#if __FreeBSD_version >= 500005
    /*
     * Initialise command-completion task.
     */
    TASK_INIT(&sc->amr_task_complete, 0, amr_complete, sc);
#endif

    debug(2, "queue init done");

    /*
     * Configure for this controller type.
     */
    if (AMR_IS_QUARTZ(sc)) {
	sc->amr_submit_command = amr_quartz_submit_command;
	sc->amr_get_work       = amr_quartz_get_work;
    } else {
	sc->amr_submit_command = amr_std_submit_command;
	sc->amr_get_work       = amr_std_get_work;
	amr_std_attach_mailbox(sc);;
    }

#ifdef AMR_BOARD_INIT
    if ((AMR_IS_QUARTZ(sc) ? amr_quartz_init(sc) : amr_std_init(sc))))
	return(ENXIO);
#endif

    /*
     * Quiz controller for features and limits.
     */
    if (amr_query_controller(sc))
	return(ENXIO);

    debug(2, "controller query complete");

#ifdef AMR_SCSI_PASSTHROUGH
    /*
     * Attach our 'real' SCSI channels to CAM.
     */
    if (amr_cam_attach(sc))
	return(ENXIO);
    debug(2, "CAM attach done");
#endif

    /*
     * Create the control device.
     */
    sc->amr_dev_t = make_dev(&amr_cdevsw, device_get_unit(sc->amr_dev), UID_ROOT, GID_OPERATOR,
			     S_IRUSR | S_IWUSR, "amr%d", device_get_unit(sc->amr_dev));
    sc->amr_dev_t->si_drv1 = sc;

    /*
     * Schedule ourselves to bring the controller up once interrupts are
     * available.
     */
    bzero(&sc->amr_ich, sizeof(struct intr_config_hook));
    sc->amr_ich.ich_func = amr_startup;
    sc->amr_ich.ich_arg = sc;
    if (config_intrhook_establish(&sc->amr_ich) != 0) {
	device_printf(sc->amr_dev, "can't establish configuration hook\n");
	return(ENOMEM);
    }

    /*
     * Print a little information about the controller.
     */
    amr_describe_controller(sc);

    debug(2, "attach complete");
    return(0);
}

/********************************************************************************
 * Locate disk resources and attach children to them.
 */
static void
amr_startup(void *arg)
{
    struct amr_softc	*sc = (struct amr_softc *)arg;
    struct amr_logdrive	*dr;
    int			i, error;
    
    debug_called(1);

    /* pull ourselves off the intrhook chain */
    config_intrhook_disestablish(&sc->amr_ich);

    /* get up-to-date drive information */
    if (amr_query_controller(sc)) {
	device_printf(sc->amr_dev, "can't scan controller for drives\n");
	return;
    }

    /* iterate over available drives */
    for (i = 0, dr = &sc->amr_drive[0]; (i < AMR_MAXLD) && (dr->al_size != 0xffffffff); i++, dr++) {
	/* are we already attached to this drive? */
	if (dr->al_disk == 0) {
	    /* generate geometry information */
	    if (dr->al_size > 0x200000) {	/* extended translation? */
		dr->al_heads = 255;
		dr->al_sectors = 63;
	    } else {
		dr->al_heads = 64;
		dr->al_sectors = 32;
	    }
	    dr->al_cylinders = dr->al_size / (dr->al_heads * dr->al_sectors);
	    
	    dr->al_disk = device_add_child(sc->amr_dev, NULL, -1);
	    if (dr->al_disk == 0)
		device_printf(sc->amr_dev, "device_add_child failed\n");
	    device_set_ivars(dr->al_disk, dr);
	}
    }
    
    if ((error = bus_generic_attach(sc->amr_dev)) != 0)
	device_printf(sc->amr_dev, "bus_generic_attach returned %d\n", error);
    
    /* mark controller back up */
    sc->amr_state &= ~AMR_STATE_SHUTDOWN;

    /* interrupts will be enabled before we do anything more */
    sc->amr_state |= AMR_STATE_INTEN;

    /*
     * Start the timeout routine.
     */
/*    sc->amr_timeout = timeout(amr_periodic, sc, hz);*/

    return;
}

/*******************************************************************************
 * Free resources associated with a controller instance
 */
void
amr_free(struct amr_softc *sc)
{
    struct amr_command_cluster	*acc;

#ifdef AMR_SCSI_PASSTHROUGH
    /* detach from CAM */
    amr_cam_detach(sc);
#endif

    /* cancel status timeout */
    untimeout(amr_periodic, sc, sc->amr_timeout);
    
    /* throw away any command buffers */
    while ((acc = TAILQ_FIRST(&sc->amr_cmd_clusters)) != NULL) {
	TAILQ_REMOVE(&sc->amr_cmd_clusters, acc, acc_link);
	amr_freecmd_cluster(acc);
    }
}

/*******************************************************************************
 * Receive a bio structure from a child device and queue it on a particular
 * disk resource, then poke the disk resource to start as much work as it can.
 */
int
amr_submit_bio(struct amr_softc *sc, struct bio *bio)
{
    debug_called(2);

    amr_enqueue_bio(sc, bio);
    amr_startio(sc);
    return(0);
}

/********************************************************************************
 * Accept an open operation on the control device.
 */
int
amr_open(dev_t dev, int flags, int fmt, struct proc *p)
{
    int			unit = minor(dev);
    struct amr_softc	*sc = devclass_get_softc(amr_devclass, unit);

    debug_called(1);

    sc->amr_state |= AMR_STATE_OPEN;
    return(0);
}

/********************************************************************************
 * Accept the last close on the control device.
 */
int
amr_close(dev_t dev, int flags, int fmt, struct proc *p)
{
    int			unit = minor(dev);
    struct amr_softc	*sc = devclass_get_softc(amr_devclass, unit);

    debug_called(1);

    sc->amr_state &= ~AMR_STATE_OPEN;
    return (0);
}

/********************************************************************************
 * Handle controller-specific control operations.
 */
int
amr_ioctl(dev_t dev, u_long cmd, caddr_t addr, int32_t flag, struct proc *p)
{
    struct amr_softc		*sc = (struct amr_softc *)dev->si_drv1;
    int				*arg = (int *)addr;
    struct amr_user_ioctl	*au = (struct amr_user_ioctl *)addr;
    struct amr_command		*ac;
    struct amr_mailbox_ioctl	*mbi;
    struct amr_passthrough	*ap;
    void			*dp;
    int				error;

    debug_called(1);

    error = 0;
    dp = NULL;
    ap = NULL;
    ac = NULL;
    switch(cmd) {

    case AMR_IO_VERSION:
	debug(1, "AMR_IO_VERSION");
	*arg = AMR_IO_VERSION_NUMBER;
	break;

    case AMR_IO_COMMAND:
	debug(1, "AMR_IO_COMMAND");
	/* handle inbound data buffer */
	if (au->au_length != 0) {
	    if ((dp = malloc(au->au_length, M_DEVBUF, M_WAITOK)) == NULL) {
		error = ENOMEM;
		break;
	    }
	    if ((error = copyin(au->au_buffer, dp, au->au_length)) != 0)
		break;
	}

	if ((ac = amr_alloccmd(sc)) == NULL) {
	    error = ENOMEM;
	    break;
	}

	/* handle SCSI passthrough command */
	if (au->au_cmd[0] == AMR_CMD_PASS) {
	    if ((ap = malloc(sizeof(*ap), M_DEVBUF, M_WAITOK)) == NULL) {
		error = ENOMEM;
		break;
	    }
	    bzero(ap, sizeof(*ap));

	    /* copy cdb */
	    ap->ap_cdb_length = au->au_cmd[2];
	    bcopy(&au->au_cmd[3], &ap->ap_cdb[0], ap->ap_cdb_length);

	    /* build passthrough */
	    ap->ap_timeout		= au->au_cmd[ap->ap_cdb_length + 3] & 0x07;
	    ap->ap_ars			= (au->au_cmd[ap->ap_cdb_length + 3] & 0x08) ? 1 : 0;
	    ap->ap_islogical		= (au->au_cmd[ap->ap_cdb_length + 3] & 0x80) ? 1 : 0;
	    ap->ap_logical_drive_no	= au->au_cmd[ap->ap_cdb_length + 4];
	    ap->ap_channel		= au->au_cmd[ap->ap_cdb_length + 5];
	    ap->ap_scsi_id 		= au->au_cmd[ap->ap_cdb_length + 6];
	    ap->ap_request_sense_length	= 14;
	    /* XXX what about the request-sense area? does the caller want it? */

	    /* build command */
	    ac->ac_data = ap;
	    ac->ac_length = sizeof(*ap);
	    ac->ac_flags |= AMR_CMD_DATAOUT;
	    ac->ac_ccb_data = dp;
	    ac->ac_ccb_length = au->au_length;
	    if (au->au_direction & AMR_IO_READ)
		ac->ac_flags |= AMR_CMD_CCB_DATAIN;
	    if (au->au_direction & AMR_IO_WRITE)
		ac->ac_flags |= AMR_CMD_CCB_DATAOUT;

	    ac->ac_mailbox.mb_command = AMR_CMD_PASS;

	} else {
	    /* direct command to controller */
	    mbi = (struct amr_mailbox_ioctl *)&ac->ac_mailbox;

	    /* copy pertinent mailbox items */
	    mbi->mb_command = au->au_cmd[0];
	    mbi->mb_channel = au->au_cmd[1];
	    mbi->mb_param = au->au_cmd[2];
	    mbi->mb_pad[0] = au->au_cmd[3];
	    mbi->mb_drive = au->au_cmd[4];

	    /* build the command */
	    ac->ac_data = dp;
	    ac->ac_length = au->au_length;
	    if (au->au_direction & AMR_IO_READ)
		ac->ac_flags |= AMR_CMD_DATAIN;
	    if (au->au_direction & AMR_IO_WRITE)
		ac->ac_flags |= AMR_CMD_DATAOUT;
	}

	/* run the command */
	if ((error = amr_wait_command(ac)) != 0)
	    break;

	/* copy out data and set status */
	if (au->au_length != 0)
	    error = copyout(dp, au->au_buffer, au->au_length);
	au->au_status = ac->ac_status;
	break;
	
    default:
	debug(1, "unknown ioctl 0x%lx", cmd);
	error = ENOIOCTL;
	break;
    }

    if (dp != NULL)
	free(dp, M_DEVBUF);
    if (ap != NULL)
	free(ap, M_DEVBUF);
    if (ac != NULL)
	amr_releasecmd(ac);
    return(error);
}

/********************************************************************************
 ********************************************************************************
                                                                Status Monitoring
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Perform a periodic check of the controller status
 */
static void
amr_periodic(void *data)
{
    struct amr_softc	*sc = (struct amr_softc *)data;

    debug_called(2);

    /* XXX perform periodic status checks here */

    /* compensate for missed interrupts */
    amr_done(sc);

    /* reschedule */
    sc->amr_timeout = timeout(amr_periodic, sc, hz);
}

/********************************************************************************
 ********************************************************************************
                                                                 Command Wrappers
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Interrogate the controller for the operational parameters we require.
 */
static int
amr_query_controller(struct amr_softc *sc)
{
    struct amr_enquiry3	*aex;
    struct amr_prodinfo	*ap;
    struct amr_enquiry	*ae;
    int			ldrv;

    /* 
     * If we haven't found the real limit yet, let us have a couple of commands in
     * order to be able to probe.
     */
    if (sc->amr_maxio == 0)
	sc->amr_maxio = 2;

    /* 
     * Try to issue an ENQUIRY3 command 
     */
    if ((aex = amr_enquiry(sc, 2048, AMR_CMD_CONFIG, AMR_CONFIG_ENQ3, 
			   AMR_CONFIG_ENQ3_SOLICITED_FULL)) != NULL) {

	/*
	 * Fetch current state of logical drives.
	 */
	for (ldrv = 0; ldrv < aex->ae_numldrives; ldrv++) {
	    sc->amr_drive[ldrv].al_size       = aex->ae_drivesize[ldrv];
	    sc->amr_drive[ldrv].al_state      = aex->ae_drivestate[ldrv];
	    sc->amr_drive[ldrv].al_properties = aex->ae_driveprop[ldrv];
	    debug(2, "  drive %d: %d state %x properties %x\n", ldrv, sc->amr_drive[ldrv].al_size,
		  sc->amr_drive[ldrv].al_state, sc->amr_drive[ldrv].al_properties);
	}
	free(aex, M_DEVBUF);

	/*
	 * Get product info for channel count.
	 */
	if ((ap = amr_enquiry(sc, 2048, AMR_CMD_CONFIG, AMR_CONFIG_PRODUCT_INFO, 0)) == NULL) {
	    device_printf(sc->amr_dev, "can't obtain product data from controller\n");
	    return(1);
	}
	sc->amr_maxdrives = 40;
	sc->amr_maxchan = ap->ap_nschan;
	sc->amr_maxio = ap->ap_maxio;
	sc->amr_type |= AMR_TYPE_40LD;
	free(ap, M_DEVBUF);

    } else {

	/* failed, try the 8LD ENQUIRY commands */
	if ((ae = (struct amr_enquiry *)amr_enquiry(sc, 2048, AMR_CMD_EXT_ENQUIRY2, 0, 0)) == NULL) {
	    if ((ae = (struct amr_enquiry *)amr_enquiry(sc, 2048, AMR_CMD_ENQUIRY, 0, 0)) == NULL) {
		device_printf(sc->amr_dev, "can't obtain configuration data from controller\n");
		return(1);
	    }
	    ae->ae_signature = 0;
	}

	/*
	 * Fetch current state of logical drives.
	 */
	for (ldrv = 0; ldrv < ae->ae_ldrv.al_numdrives; ldrv++) {
	    sc->amr_drive[ldrv].al_size       = ae->ae_ldrv.al_size[ldrv];
	    sc->amr_drive[ldrv].al_state      = ae->ae_ldrv.al_state[ldrv];
	    sc->amr_drive[ldrv].al_properties = ae->ae_ldrv.al_properties[ldrv];
	    debug(2, "  drive %d: %d state %x properties %x\n", ldrv, sc->amr_drive[ldrv].al_size,
		  sc->amr_drive[ldrv].al_state, sc->amr_drive[ldrv].al_properties);
	}

	sc->amr_maxdrives = 8;
	sc->amr_maxchan = ae->ae_adapter.aa_channels;
	sc->amr_maxio = ae->ae_adapter.aa_maxio;
	free(ae, M_DEVBUF);
    }

    /*
     * Mark remaining drives as unused.
     */
    for (; ldrv < AMR_MAXLD; ldrv++)
	sc->amr_drive[ldrv].al_size = 0xffffffff;

    /* 
     * Cap the maximum number of outstanding I/Os.  AMI's Linux driver doesn't trust
     * the controller's reported value, and lockups have been seen when we do.
     */
    sc->amr_maxio = imin(sc->amr_maxio, AMR_LIMITCMD);

    return(0);
}

/********************************************************************************
 * Run a generic enquiry-style command.
 */
static void *
amr_enquiry(struct amr_softc *sc, size_t bufsize, u_int8_t cmd, u_int8_t cmdsub, u_int8_t cmdqual)
{
    struct amr_command	*ac;
    void		*result;
    u_int8_t		*mbox;
    int			error;

    debug_called(1);

    error = 1;
    result = NULL;
    
    /* get ourselves a command buffer */
    if ((ac = amr_alloccmd(sc)) == NULL)
	goto out;
    /* allocate the response structure */
    if ((result = malloc(bufsize, M_DEVBUF, M_NOWAIT)) == NULL)
	goto out;
    /* set command flags */
    ac->ac_flags |= AMR_CMD_PRIORITY | AMR_CMD_DATAOUT;
    
    /* point the command at our data */
    ac->ac_data = result;
    ac->ac_length = bufsize;
    
    /* build the command proper */
    mbox = (u_int8_t *)&ac->ac_mailbox;		/* XXX want a real structure for this? */
    mbox[0] = cmd;
    mbox[2] = cmdsub;
    mbox[3] = cmdqual;

    /* can't assume that interrupts are going to work here, so play it safe */
    if (amr_poll_command(ac))
	goto out;
    error = ac->ac_status;
    
 out:
    if (ac != NULL)
	amr_releasecmd(ac);
    if ((error != 0) && (result != NULL)) {
	free(result, M_DEVBUF);
	result = NULL;
    }
    return(result);
}

/********************************************************************************
 * Flush the controller's internal cache, return status.
 */
int
amr_flush(struct amr_softc *sc)
{
    struct amr_command	*ac;
    int			error;

    /* get ourselves a command buffer */
    error = 1;
    if ((ac = amr_alloccmd(sc)) == NULL)
	goto out;
    /* set command flags */
    ac->ac_flags |= AMR_CMD_PRIORITY | AMR_CMD_DATAOUT;
    
    /* build the command proper */
    ac->ac_mailbox.mb_command = AMR_CMD_FLUSH;

    /* we have to poll, as the system may be going down or otherwise damaged */
    if (amr_poll_command(ac))
	goto out;
    error = ac->ac_status;
    
 out:
    if (ac != NULL)
	amr_releasecmd(ac);
    return(error);
}

/********************************************************************************
 * Try to find I/O work for the controller from one or more of the work queues.
 *
 * We make the assumption that if the controller is not ready to take a command
 * at some given time, it will generate an interrupt at some later time when
 * it is.
 */
void
amr_startio(struct amr_softc *sc)
{
    struct amr_command	*ac;

    /* spin until something prevents us from doing any work */
    for (;;) {

	/* try to get a ready command */
	ac = amr_dequeue_ready(sc);

	/* if that failed, build a command from a bio */
	if (ac == NULL)
	    (void)amr_bio_command(sc, &ac);

#ifdef AMR_SCSI_PASSTHROUGH
	/* if that failed, build a command from a ccb */
	if (ac == NULL)
	    (void)amr_cam_command(sc, &ac);
#endif
	
	/* if we don't have anything to do, give up */
	if (ac == NULL)
	    break;

	/* try to give the command to the controller; if this fails save it for later and give up */
	if (amr_start(ac)) {
	    debug(2, "controller busy, command deferred");
	    amr_requeue_ready(ac);	/* XXX schedule retry very soon? */
	    break;
	}
    }
}

/********************************************************************************
 * Handle completion of an I/O command.
 */
static void
amr_completeio(struct amr_command *ac)
{
    struct amr_softc	*sc = ac->ac_sc;
    
    if (ac->ac_status != AMR_STATUS_SUCCESS) {	/* could be more verbose here? */
	ac->ac_bio->bio_error = EIO;
	ac->ac_bio->bio_flags |= BIO_ERROR;

	device_printf(sc->amr_dev, "I/O error - 0x%x\n", ac->ac_status);
/*	amr_printcommand(ac);*/
    }
    amrd_intr(ac->ac_bio);
    amr_releasecmd(ac);
}

/********************************************************************************
 ********************************************************************************
                                                               Command Processing
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Convert a bio off the top of the bio queue into a command.
 */
static int
amr_bio_command(struct amr_softc *sc, struct amr_command **acp)
{
    struct amr_command	*ac;
    struct amrd_softc	*amrd;
    struct bio		*bio;
    int			error;
    int			blkcount;
    int			driveno;
    int			cmd;

    ac = NULL;
    error = 0;

    /* get a bio to work on */
    if ((bio = amr_dequeue_bio(sc)) == NULL)
	goto out;

    /* get a command */
    if ((ac = amr_alloccmd(sc)) == NULL) {
	error = ENOMEM;
	goto out;
    }	
	
    /* connect the bio to the command */
    ac->ac_complete = amr_completeio;
    ac->ac_bio = bio;
    ac->ac_data = bio->bio_data;
    ac->ac_length = bio->bio_bcount;
    if (BIO_IS_READ(bio)) {
	ac->ac_flags |= AMR_CMD_DATAIN;
	cmd = AMR_CMD_LREAD;
    } else {
	ac->ac_flags |= AMR_CMD_DATAOUT;
	cmd = AMR_CMD_LWRITE;
    }
    amrd = (struct amrd_softc *)bio->bio_dev->si_drv1;
    driveno = amrd->amrd_drive - sc->amr_drive;
    blkcount = (bio->bio_bcount + AMR_BLKSIZE - 1) / AMR_BLKSIZE;

    ac->ac_mailbox.mb_command = cmd;
    ac->ac_mailbox.mb_blkcount = blkcount;
    ac->ac_mailbox.mb_lba = bio->bio_pblkno;
    ac->ac_mailbox.mb_drive = driveno;
    /* we fill in the s/g related data when the command is mapped */

    if ((bio->bio_pblkno + blkcount) > sc->amr_drive[driveno].al_size)
	device_printf(sc->amr_dev, "I/O beyond end of unit (%u,%d > %u)\n", 
		      bio->bio_pblkno, blkcount, sc->amr_drive[driveno].al_size);

out:
    if (error != 0) {
	if (ac != NULL)
	    amr_releasecmd(ac);
	if (bio != NULL)			/* this breaks ordering... */
	    amr_enqueue_bio(sc, bio);
    }
    *acp = ac;
    return(error);
}

/********************************************************************************
 * Take a command, submit it to the controller and sleep until it completes
 * or fails.  Interrupts must be enabled, returns nonzero on error.
 */
static int
amr_wait_command(struct amr_command *ac)
{
    struct amr_softc	*sc = ac->ac_sc;
    int			error, count;
    
    debug_called(1);

    ac->ac_complete = NULL;
    ac->ac_flags |= AMR_CMD_SLEEP;
    if ((error = amr_start(ac)) != 0)
	return(error);
    
    count = 0;
    /* XXX better timeout? */
    while ((ac->ac_flags & AMR_CMD_BUSY) && (count < 30)) {
	tsleep(ac, PRIBIO | PCATCH, "amrwcmd", hz);
    }
    
    if (ac->ac_status != 0) {
	device_printf(sc->amr_dev, "I/O error - 0x%x\n", ac->ac_status);
	return(EIO);
    }
    return(0);
}

/********************************************************************************
 * Take a command, submit it to the controller and busy-wait for it to return.
 * Returns nonzero on error.  Can be safely called with interrupts enabled.
 */
static int
amr_poll_command(struct amr_command *ac)
{
    struct amr_softc	*sc = ac->ac_sc;
    int			error, count;

    debug_called(2);

    ac->ac_complete = NULL;
    if ((error = amr_start(ac)) != 0)
	return(error);

    count = 0;
    do {
	/* 
	 * Poll for completion, although the interrupt handler may beat us to it. 
	 * Note that the timeout here is somewhat arbitrary.
	 */
	amr_done(sc);
	DELAY(1000);
    } while ((ac->ac_flags & AMR_CMD_BUSY) && (count++ < 1000));
    if (!(ac->ac_flags & AMR_CMD_BUSY)) {
	error = 0;
    } else {
	/* XXX the slot is now marked permanently busy */
	error = EIO;
	device_printf(sc->amr_dev, "polled command timeout\n");
    }
    return(error);
}

/********************************************************************************
 * Get a free command slot for a command if it doesn't already have one.
 *
 * May be safely called multiple times for a given command.
 */
static int
amr_getslot(struct amr_command *ac)
{
    struct amr_softc	*sc = ac->ac_sc;
    int			s, slot, limit, error;

    debug_called(3);

    /* if the command already has a slot, don't try to give it another one */
    if (ac->ac_slot != 0)
	return(0);

    /* enforce slot usage limit */
    limit = (ac->ac_flags & AMR_CMD_PRIORITY) ? sc->amr_maxio : sc->amr_maxio - 4;
    if (sc->amr_busyslots > limit)
	return(EBUSY);
    
    /*
     * Allocate a slot.  XXX linear scan is slow
     */
    error = EBUSY;
    s = splbio();
    for (slot = 0; slot < sc->amr_maxio; slot++) {
	if (sc->amr_busycmd[slot] == NULL) {
	    sc->amr_busycmd[slot] = ac;
	    sc->amr_busyslots++;
	    ac->ac_slot = slot;
	    error = 0;
	    break;
	}
    }
    splx(s);

    return(error);
}

/********************************************************************************
 * Map/unmap (ac)'s data in the controller's addressable space as required.
 *
 * These functions may be safely called multiple times on a given command.
 */
static void
amr_setup_dmamap(void *arg, bus_dma_segment_t *segs, int nsegments, int error)
{
    struct amr_command	*ac = (struct amr_command *)arg;
    struct amr_softc	*sc = ac->ac_sc;
    struct amr_sgentry	*sg;
    int			i;

    debug_called(3);

    /* get base address of s/g table */
    sg = sc->amr_sgtable + (ac->ac_slot * AMR_NSEG);

    /* save data physical address */
    ac->ac_dataphys = segs[0].ds_addr;

    /* decide whether we need to populate the s/g table */
    if (nsegments < 2) {
	ac->ac_mailbox.mb_nsgelem = 0;
	ac->ac_mailbox.mb_physaddr = ac->ac_dataphys;
    } else {
	ac->ac_mailbox.mb_nsgelem = nsegments;
	ac->ac_mailbox.mb_physaddr = sc->amr_sgbusaddr + (ac->ac_slot * AMR_NSEG * sizeof(struct amr_sgentry));
	for (i = 0; i < nsegments; i++, sg++) {
	    sg->sg_addr = segs[i].ds_addr;
	    sg->sg_count = segs[i].ds_len;
	}
    }
}

static void
amr_setup_ccbmap(void *arg, bus_dma_segment_t *segs, int nsegments, int error)
{
    struct amr_command		*ac = (struct amr_command *)arg;
    struct amr_softc		*sc = ac->ac_sc;
    struct amr_sgentry		*sg;
    struct amr_passthrough	*ap = (struct amr_passthrough *)ac->ac_data;
    int				i;

    /* get base address of s/g table */
    sg = sc->amr_sgtable + (ac->ac_slot * AMR_NSEG);

    /* save s/g table information in passthrough */
    ap->ap_no_sg_elements = nsegments;
    ap->ap_data_transfer_address = sc->amr_sgbusaddr + (ac->ac_slot * AMR_NSEG * sizeof(struct amr_sgentry));

    /* save pointer to passthrough in command   XXX is this already done above? */
    ac->ac_mailbox.mb_physaddr = ac->ac_dataphys;

    debug(2, "slot %d  %d segments at 0x%x, passthrough at 0x%x", ac->ac_slot,
	   ap->ap_no_sg_elements, ap->ap_data_transfer_address, ac->ac_dataphys);
    
    /* populate s/g table (overwrites previous call which mapped the passthrough) */
    for (i = 0; i < nsegments; i++, sg++) {
	sg->sg_addr = segs[i].ds_addr;
	sg->sg_count = segs[i].ds_len;
	debug(2, " %d: 0x%x/%d", i, sg->sg_addr, sg->sg_count);
    }
}

static void
amr_mapcmd(struct amr_command *ac)
{
    struct amr_softc	*sc = ac->ac_sc;

    debug_called(2);

    /* if the command involves data at all, and hasn't been mapped */
    if (!(ac->ac_flags & AMR_CMD_MAPPED)) {

	if (ac->ac_data != NULL) {
	    /* map the data buffers into bus space and build the s/g list */
	    bus_dmamap_load(sc->amr_buffer_dmat, ac->ac_dmamap, ac->ac_data, ac->ac_length, 
			    amr_setup_dmamap, ac, 0);
	    if (ac->ac_flags & AMR_CMD_DATAIN)
		bus_dmamap_sync(sc->amr_buffer_dmat, ac->ac_dmamap, BUS_DMASYNC_PREREAD);
	    if (ac->ac_flags & AMR_CMD_DATAOUT)
		bus_dmamap_sync(sc->amr_buffer_dmat, ac->ac_dmamap, BUS_DMASYNC_PREWRITE);
	}

	if (ac->ac_ccb_data != NULL) {
	    bus_dmamap_load(sc->amr_buffer_dmat, ac->ac_ccb_dmamap, ac->ac_ccb_data, ac->ac_ccb_length, 
			    amr_setup_ccbmap, ac, 0);
	    if (ac->ac_flags & AMR_CMD_CCB_DATAIN)
		bus_dmamap_sync(sc->amr_buffer_dmat, ac->ac_ccb_dmamap, BUS_DMASYNC_PREREAD);
	    if (ac->ac_flags & AMR_CMD_CCB_DATAOUT)
		bus_dmamap_sync(sc->amr_buffer_dmat, ac->ac_ccb_dmamap, BUS_DMASYNC_PREWRITE);
	}
	ac->ac_flags |= AMR_CMD_MAPPED;
    }
}

static void
amr_unmapcmd(struct amr_command *ac)
{
    struct amr_softc	*sc = ac->ac_sc;

    debug_called(2);

    /* if the command involved data at all and was mapped */
    if (ac->ac_flags & AMR_CMD_MAPPED) {

	if (ac->ac_data != NULL) {
	    if (ac->ac_flags & AMR_CMD_DATAIN)
		bus_dmamap_sync(sc->amr_buffer_dmat, ac->ac_dmamap, BUS_DMASYNC_POSTREAD);
	    if (ac->ac_flags & AMR_CMD_DATAOUT)
		bus_dmamap_sync(sc->amr_buffer_dmat, ac->ac_dmamap, BUS_DMASYNC_POSTWRITE);
	    bus_dmamap_unload(sc->amr_buffer_dmat, ac->ac_dmamap);
	}

	if (ac->ac_ccb_data != NULL) {
	    if (ac->ac_flags & AMR_CMD_CCB_DATAIN)
		bus_dmamap_sync(sc->amr_buffer_dmat, ac->ac_ccb_dmamap, BUS_DMASYNC_POSTREAD);
	    if (ac->ac_flags & AMR_CMD_CCB_DATAOUT)
		bus_dmamap_sync(sc->amr_buffer_dmat, ac->ac_ccb_dmamap, BUS_DMASYNC_POSTWRITE);
	    bus_dmamap_unload(sc->amr_buffer_dmat, ac->ac_ccb_dmamap);
	}
	ac->ac_flags &= ~AMR_CMD_MAPPED;
    }
}

/********************************************************************************
 * Take a command and give it to the controller, returns 0 if successful, or
 * EBUSY if the command should be retried later.
 */
static int
amr_start(struct amr_command *ac)
{
    struct amr_softc	*sc = ac->ac_sc;
    int			done, s, i;
    
    debug_called(2);

    /* mark command as busy so that polling consumer can tell */
    ac->ac_flags |= AMR_CMD_BUSY;

    /* get a command slot (freed in amr_done) */
    if (amr_getslot(ac))
	return(EBUSY);

    /* now we have a slot, we can map the command (unmapped in amr_complete) */
    amr_mapcmd(ac);

    /* mark the new mailbox we are going to copy in as busy */
    ac->ac_mailbox.mb_busy = 1;

    /* clear the poll/ack fields in the mailbox */
    sc->amr_mailbox->mb_poll = 0;
    sc->amr_mailbox->mb_ack = 0;

    /* 
     * Save the slot number so that we can locate this command when complete.
     * Note that ident = 0 seems to be special, so we don't use it.
     */
    ac->ac_mailbox.mb_ident = ac->ac_slot + 1;

    /* 
     * Spin waiting for the mailbox, give up after ~1 second.  We expect the
     * controller to be able to handle our I/O.
     *
     * XXX perhaps we should wait for less time, and count on the deferred command
     * handling to deal with retries?
     */
    debug(2, "wait for mailbox");
    for (i = 10000, done = 0; (i > 0) && !done; i--) {
	s = splbio();
	
	/* is the mailbox free? */
	if (sc->amr_mailbox->mb_busy == 0) {
	    debug(2, "got mailbox");
	    sc->amr_mailbox64->mb64_segment = 0;
	    bcopy(&ac->ac_mailbox, (void *)(uintptr_t)(volatile void *)sc->amr_mailbox, AMR_MBOX_CMDSIZE);
	    done = 1;

	    /* not free, spin waiting */
	} else {
	    debug(3, "busy flag %x\n", sc->amr_mailbox->mb_busy);
	    /* this is somewhat ugly */
	    DELAY(100);
	}
	splx(s);	/* drop spl to allow completion interrupts */
    }

    /*
     * Now give the command to the controller
     */
    if (done) {
	if (sc->amr_submit_command(sc)) {
	    /* the controller wasn't ready to take the command, forget that we tried to post it */
	    sc->amr_mailbox->mb_busy = 0;
	    return(EBUSY);
	}
	debug(2, "posted command");
	return(0);
    }
    
    /*
     * The controller wouldn't take the command.  Return the command as busy
     * so that it is retried later.
     */
    return(EBUSY);
}

/********************************************************************************
 * Extract one or more completed commands from the controller (sc)
 *
 * Returns nonzero if any commands on the work queue were marked as completed.
 */
int
amr_done(struct amr_softc *sc)
{
    struct amr_command	*ac;
    struct amr_mailbox	mbox;
    int			i, idx, result;
    
    debug_called(2);

    /* See if there's anything for us to do */
    result = 0;

    /* loop collecting completed commands */
    for (;;) {
	/* poll for a completed command's identifier and status */
	if (sc->amr_get_work(sc, &mbox)) {
	    result = 1;
	    
	    /* iterate over completed commands in this result */
	    for (i = 0; i < mbox.mb_nstatus; i++) {
		/* get pointer to busy command */
		idx = mbox.mb_completed[i] - 1;
		ac = sc->amr_busycmd[idx];

		/* really a busy command? */
		if (ac != NULL) {

		    /* pull the command from the busy index */
		    sc->amr_busycmd[idx] = NULL;
		    sc->amr_busyslots--;
		
		    /* save status for later use */
		    ac->ac_status = mbox.mb_status;
		    amr_enqueue_completed(ac);
		    debug(3, "completed command with status %x", mbox.mb_status);
		} else {
		    device_printf(sc->amr_dev, "bad slot %d completed\n", idx);
		}
	    }
	} else {
	    break;	/* no work */
	}
    }
    
    /* if we've completed any commands, try posting some more */
    if (result)
	amr_startio(sc);
    
    /* handle completion and timeouts */
#if __FreeBSD_version >= 500005
    if (sc->amr_state & AMR_STATE_INTEN) 
	taskqueue_enqueue(taskqueue_swi, &sc->amr_task_complete);
    else
#endif
	amr_complete(sc, 0);
    
    return(result);
}

/********************************************************************************
 * Do completion processing on done commands on (sc)
 */
static void
amr_complete(void *context, int pending)
{
    struct amr_softc	*sc = (struct amr_softc *)context;
    struct amr_command	*ac;

    debug_called(2);

    /* pull completed commands off the queue */
    for (;;) {
	ac = amr_dequeue_completed(sc);
	if (ac == NULL)
	    break;

	/* unmap the command's data buffer */
	amr_unmapcmd(ac);

	/* unbusy the command */
	ac->ac_flags &= ~AMR_CMD_BUSY;
	    
	/* 
	 * Is there a completion handler? 
	 */
	if (ac->ac_complete != NULL) {
	    ac->ac_complete(ac);
	    
	    /* 
	     * Is someone sleeping on this one?
	     */
	} else if (ac->ac_flags & AMR_CMD_SLEEP) {
	    wakeup(ac);
	}
    }
}

/********************************************************************************
 ********************************************************************************
                                                        Command Buffer Management
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Get a new command buffer.
 *
 * This may return NULL in low-memory cases.
 *
 * If possible, we recycle a command buffer that's been used before.
 */
struct amr_command *
amr_alloccmd(struct amr_softc *sc)
{
    struct amr_command	*ac;

    debug_called(3);

    ac = amr_dequeue_free(sc);
    if (ac == NULL) {
	amr_alloccmd_cluster(sc);
	ac = amr_dequeue_free(sc);
    }
    if (ac == NULL)
	return(NULL);

    /* clear out significant fields */
    ac->ac_slot = 0;
    ac->ac_status = 0;
    bzero(&ac->ac_mailbox, sizeof(struct amr_mailbox));
    ac->ac_flags = 0;
    ac->ac_bio = NULL;
    ac->ac_data = NULL;
    ac->ac_ccb_data = NULL;
    ac->ac_complete = NULL;
    return(ac);
}

/********************************************************************************
 * Release a command buffer for recycling.
 */
void
amr_releasecmd(struct amr_command *ac)
{
    debug_called(3);

    amr_enqueue_free(ac);
}

/********************************************************************************
 * Allocate a new command cluster and initialise it.
 */
void
amr_alloccmd_cluster(struct amr_softc *sc)
{
    struct amr_command_cluster	*acc;
    struct amr_command		*ac;
    int				s, i;

    acc = malloc(AMR_CMD_CLUSTERSIZE, M_DEVBUF, M_NOWAIT);
    if (acc != NULL) {
	s = splbio();
	TAILQ_INSERT_TAIL(&sc->amr_cmd_clusters, acc, acc_link);
	splx(s);
	for (i = 0; i < AMR_CMD_CLUSTERCOUNT; i++) {
	    ac = &acc->acc_command[i];
	    bzero(ac, sizeof(*ac));
	    ac->ac_sc = sc;
	    if (!bus_dmamap_create(sc->amr_buffer_dmat, 0, &ac->ac_dmamap) &&
		!bus_dmamap_create(sc->amr_buffer_dmat, 0, &ac->ac_ccb_dmamap))
		amr_releasecmd(ac);
	}
    }
}

/********************************************************************************
 * Free a command cluster
 */
void
amr_freecmd_cluster(struct amr_command_cluster *acc)
{
    struct amr_softc	*sc = acc->acc_command[0].ac_sc;
    int			i;

    for (i = 0; i < AMR_CMD_CLUSTERCOUNT; i++)
	bus_dmamap_destroy(sc->amr_buffer_dmat, acc->acc_command[i].ac_dmamap);
    free(acc, M_DEVBUF);
}

/********************************************************************************
 ********************************************************************************
                                                         Interface-specific Shims
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Tell the controller that the mailbox contains a valid command
 */
static int
amr_quartz_submit_command(struct amr_softc *sc)
{
    debug_called(3);

    if (AMR_QGET_IDB(sc) & AMR_QIDB_SUBMIT)
	return(EBUSY);
    AMR_QPUT_IDB(sc, sc->amr_mailboxphys | AMR_QIDB_SUBMIT);
    return(0);
}

static int
amr_std_submit_command(struct amr_softc *sc)
{
    debug_called(3);

    if (AMR_SGET_MBSTAT(sc) & AMR_SMBOX_BUSYFLAG)
	return(EBUSY);
    AMR_SPOST_COMMAND(sc);
    return(0);
}

/********************************************************************************
 * Claim any work that the controller has completed; acknowledge completion,
 * save details of the completion in (mbsave)
 */
static int
amr_quartz_get_work(struct amr_softc *sc, struct amr_mailbox *mbsave)
{
    int		s, worked;
    u_int32_t	outd;

    debug_called(3);

    worked = 0;
    s = splbio();

    /* work waiting for us? */
    if ((outd = AMR_QGET_ODB(sc)) == AMR_QODB_READY) {

	/* save mailbox, which contains a list of completed commands */
	bcopy((void *)(uintptr_t)(volatile void *)sc->amr_mailbox, mbsave, sizeof(*mbsave));

	/* acknowledge interrupt */
	AMR_QPUT_ODB(sc, AMR_QODB_READY);

	/* acknowledge that we have the commands */
	AMR_QPUT_IDB(sc, sc->amr_mailboxphys | AMR_QIDB_ACK);

#if AMR_QUARTZ_GOFASTER
	/*
	 * This waits for the controller to notice that we've taken the
	 * command from it.  It's very inefficient, and we shouldn't do it,
	 * but if we remove this code, we stop completing commands under
	 * load.
	 *
	 * Peter J says we shouldn't do this.  The documentation says we
	 * should.  Who is right?
	 */
	while(AMR_QGET_IDB(sc) & AMR_QIDB_ACK)
	    ;				/* XXX aiee! what if it dies? */
#endif

	worked = 1;			/* got some work */
    }

    splx(s);
    return(worked);
}

static int
amr_std_get_work(struct amr_softc *sc, struct amr_mailbox *mbsave)
{
    int		s, worked;
    u_int8_t	istat;

    debug_called(3);

    worked = 0;
    s = splbio();

    /* check for valid interrupt status */
    istat = AMR_SGET_ISTAT(sc);
    if ((istat & AMR_SINTR_VALID) != 0) {
	AMR_SPUT_ISTAT(sc, istat);	/* ack interrupt status */

	/* save mailbox, which contains a list of completed commands */
	bcopy((void *)(uintptr_t)(volatile void *)sc->amr_mailbox, mbsave, sizeof(*mbsave));

	AMR_SACK_INTERRUPT(sc);		/* acknowledge we have the mailbox */
	worked = 1;
    }

    splx(s);
    return(worked);
}

/********************************************************************************
 * Notify the controller of the mailbox location.
 */
static void
amr_std_attach_mailbox(struct amr_softc *sc)
{

    /* program the mailbox physical address */
    AMR_SBYTE_SET(sc, AMR_SMBOX_0, sc->amr_mailboxphys         & 0xff);
    AMR_SBYTE_SET(sc, AMR_SMBOX_1, (sc->amr_mailboxphys >>  8) & 0xff);
    AMR_SBYTE_SET(sc, AMR_SMBOX_2, (sc->amr_mailboxphys >> 16) & 0xff);
    AMR_SBYTE_SET(sc, AMR_SMBOX_3, (sc->amr_mailboxphys >> 24) & 0xff);
    AMR_SBYTE_SET(sc, AMR_SMBOX_ENABLE, AMR_SMBOX_ADDR);

    /* clear any outstanding interrupt and enable interrupts proper */
    AMR_SACK_INTERRUPT(sc);
    AMR_SENABLE_INTR(sc);
}

#ifdef AMR_BOARD_INIT
/********************************************************************************
 * Initialise the controller
 */
static int
amr_quartz_init(struct amr_softc *sc)
{
    int		status, ostatus;

    device_printf(sc->amr_dev, "initial init status %x\n", AMR_QGET_INITSTATUS(sc));

    AMR_QRESET(sc);

    ostatus = 0xff;
    while ((status = AMR_QGET_INITSTATUS(sc)) != AMR_QINIT_DONE) {
	if (status != ostatus) {
	    device_printf(sc->amr_dev, "(%x) %s\n", status, amr_describe_code(amr_table_qinit, status));
	    ostatus = status;
	}
	switch (status) {
	case AMR_QINIT_NOMEM:
	    return(ENOMEM);

	case AMR_QINIT_SCAN:
	    /* XXX we could print channel/target here */
	    break;
	}
    }
    return(0);
}

static int
amr_std_init(struct amr_softc *sc)
{
    int		status, ostatus;

    device_printf(sc->amr_dev, "initial init status %x\n", AMR_SGET_INITSTATUS(sc));

    AMR_SRESET(sc);
 
    ostatus = 0xff;
    while ((status = AMR_SGET_INITSTATUS(sc)) != AMR_SINIT_DONE) {
	if (status != ostatus) {
	    device_printf(sc->amr_dev, "(%x) %s\n", status, amr_describe_code(amr_table_sinit, status));
	    ostatus = status;
	}
	switch (status) {
	case AMR_SINIT_NOMEM:
	    return(ENOMEM);

	case AMR_SINIT_INPROG:
	    /* XXX we could print channel/target here? */
	    break;
	}
    }
    return(0);
}
#endif

/********************************************************************************
 ********************************************************************************
                                                                        Debugging
 ********************************************************************************
 ********************************************************************************/

/********************************************************************************
 * Identify the controller and print some information about it.
 */
static void
amr_describe_controller(struct amr_softc *sc)
{
    struct amr_prodinfo	*ap;
    struct amr_enquiry	*ae;
    char		*prod;

    /*
     * Try to get 40LD product info, which tells us what the card is labelled as.
     */
    if ((ap = amr_enquiry(sc, 2048, AMR_CMD_CONFIG, AMR_CONFIG_PRODUCT_INFO, 0)) != NULL) {
	device_printf(sc->amr_dev, "<%.80s> Firmware %.16s, BIOS %.16s, %dMB RAM\n",
		      ap->ap_product, ap->ap_firmware, ap->ap_bios,
		      ap->ap_memsize);

	free(ap, M_DEVBUF);
	return;
    }

    /*
     * Try 8LD extended ENQUIRY to get controller signature, and use lookup table.
     */
    if ((ae = (struct amr_enquiry *)amr_enquiry(sc, 2048, AMR_CMD_EXT_ENQUIRY2, 0, 0)) != NULL) {
	prod = amr_describe_code(amr_table_adaptertype, ae->ae_signature);

    } else if ((ae = (struct amr_enquiry *)amr_enquiry(sc, 2048, AMR_CMD_ENQUIRY, 0, 0)) != NULL) {

	/*
	 * Try to work it out based on the PCI signatures.
	 */
	switch (pci_get_device(sc->amr_dev)) {
	case 0x9010:
	    prod = "Series 428";
	    break;
	case 0x9060:
	    prod = "Series 434";
	    break;
	default:
	    prod = "unknown controller";
	    break;
	}
    } else {
	prod = "unsupported controller";
    }
    device_printf(sc->amr_dev, "<%s> Firmware %.4s, BIOS %.4s, %dMB RAM\n", 
		  prod, ae->ae_adapter.aa_firmware, ae->ae_adapter.aa_bios,
		  ae->ae_adapter.aa_memorysize);
    free(ae, M_DEVBUF);
}

#ifdef AMR_DEBUG
/********************************************************************************
 * Print the command (ac) in human-readable format
 */
static void
amr_printcommand(struct amr_command *ac)
{
    struct amr_softc	*sc = ac->ac_sc;
    struct amr_sgentry	*sg;
    int			i;
    
    device_printf(sc->amr_dev, "cmd %x  ident %d  drive %d\n",
		  ac->ac_mailbox.mb_command, ac->ac_mailbox.mb_ident, ac->ac_mailbox.mb_drive);
    device_printf(sc->amr_dev, "blkcount %d  lba %d\n", 
		  ac->ac_mailbox.mb_blkcount, ac->ac_mailbox.mb_lba);
    device_printf(sc->amr_dev, "virtaddr %p  length %lu\n", ac->ac_data, (unsigned long)ac->ac_length);
    device_printf(sc->amr_dev, "sg physaddr %08x  nsg %d\n",
		  ac->ac_mailbox.mb_physaddr, ac->ac_mailbox.mb_nsgelem);
    device_printf(sc->amr_dev, "ccb %p  bio %p\n", ac->ac_ccb_data, ac->ac_bio);

    /* get base address of s/g table */
    sg = sc->amr_sgtable + (ac->ac_slot * AMR_NSEG);
    for (i = 0; i < ac->ac_mailbox.mb_nsgelem; i++, sg++)
	device_printf(sc->amr_dev, "  %x/%d\n", sg->sg_addr, sg->sg_count);
}
#endif
