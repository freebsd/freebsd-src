/*-
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2001 Scott Long
 * Copyright (c) 2000 BSDi
 * Copyright (c) 2001 Adaptec, Inc.
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
 * Driver for the Adaptec 'FSA' family of PCI/SCSI RAID adapters.
 */

#include "opt_aac.h"

/* #include <stddef.h> */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/sysctl.h>

#include <dev/aac/aac_compat.h>

#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/devicestat.h>
#include <sys/disk.h>
#include <sys/file.h>
#include <sys/signalvar.h>
#include <sys/time.h>
#include <sys/eventhandler.h>

#include <machine/bus_memio.h>
#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/aac/aacreg.h>
#include <dev/aac/aac_ioctl.h>
#include <dev/aac/aacvar.h>
#include <dev/aac/aac_tables.h>

devclass_t	aac_devclass;

static void	aac_startup(void *arg);
static void	aac_add_container(struct aac_softc *sc,
				  struct aac_mntinforesponse *mir, int f);

/* Command Processing */
static void	aac_startio(struct aac_softc *sc);
static void	aac_timeout(struct aac_softc *sc);
static int	aac_start(struct aac_command *cm);
static void	aac_complete(void *context, int pending);
static int	aac_bio_command(struct aac_softc *sc, struct aac_command **cmp);
static void	aac_bio_complete(struct aac_command *cm);
static int	aac_wait_command(struct aac_command *cm, int timeout);
static void	aac_host_command(struct aac_softc *sc);
static void	aac_host_response(struct aac_softc *sc);

/* Command Buffer Management */
static int	aac_alloc_command(struct aac_softc *sc,
				  struct aac_command **cmp);
static void	aac_release_command(struct aac_command *cm);
static void	aac_map_command_helper(void *arg, bus_dma_segment_t *segs,
				       int nseg, int error);
static int	aac_alloc_commands(struct aac_softc *sc);
static void	aac_free_commands(struct aac_softc *sc);
static void	aac_map_command(struct aac_command *cm);
static void	aac_unmap_command(struct aac_command *cm);

/* Hardware Interface */
static void	aac_common_map(void *arg, bus_dma_segment_t *segs, int nseg,
			       int error);
static int	aac_init(struct aac_softc *sc);
static int	aac_sync_command(struct aac_softc *sc, u_int32_t command,
				 u_int32_t arg0, u_int32_t arg1, u_int32_t arg2,
				 u_int32_t arg3, u_int32_t *sp);
static int	aac_sync_fib(struct aac_softc *sc, u_int32_t command,
			     u_int32_t xferstate, void *data,
			     u_int16_t datasize, void *result,
			     u_int16_t *resultsize);
static int	aac_enqueue_fib(struct aac_softc *sc, int queue,
				struct aac_command *cm);
static int	aac_dequeue_fib(struct aac_softc *sc, int queue,
				u_int32_t *fib_size, struct aac_fib **fib_addr);
static int	aac_enqueue_response(struct aac_softc *sc, int queue,
				     struct aac_fib *fib);

/* StrongARM interface */
static int	aac_sa_get_fwstatus(struct aac_softc *sc);
static void	aac_sa_qnotify(struct aac_softc *sc, int qbit);
static int	aac_sa_get_istatus(struct aac_softc *sc);
static void	aac_sa_clear_istatus(struct aac_softc *sc, int mask);
static void	aac_sa_set_mailbox(struct aac_softc *sc, u_int32_t command,
				   u_int32_t arg0, u_int32_t arg1,
				   u_int32_t arg2, u_int32_t arg3);
static int	aac_sa_get_mailboxstatus(struct aac_softc *sc);
static void	aac_sa_set_interrupts(struct aac_softc *sc, int enable);

struct aac_interface aac_sa_interface = {
	aac_sa_get_fwstatus,
	aac_sa_qnotify,
	aac_sa_get_istatus,
	aac_sa_clear_istatus,
	aac_sa_set_mailbox,
	aac_sa_get_mailboxstatus,
	aac_sa_set_interrupts
};

/* i960Rx interface */	
static int	aac_rx_get_fwstatus(struct aac_softc *sc);
static void	aac_rx_qnotify(struct aac_softc *sc, int qbit);
static int	aac_rx_get_istatus(struct aac_softc *sc);
static void	aac_rx_clear_istatus(struct aac_softc *sc, int mask);
static void	aac_rx_set_mailbox(struct aac_softc *sc, u_int32_t command,
				   u_int32_t arg0, u_int32_t arg1,
				   u_int32_t arg2, u_int32_t arg3);
static int	aac_rx_get_mailboxstatus(struct aac_softc *sc);
static void	aac_rx_set_interrupts(struct aac_softc *sc, int enable);

struct aac_interface aac_rx_interface = {
	aac_rx_get_fwstatus,
	aac_rx_qnotify,
	aac_rx_get_istatus,
	aac_rx_clear_istatus,
	aac_rx_set_mailbox,
	aac_rx_get_mailboxstatus,
	aac_rx_set_interrupts
};

/* Debugging and Diagnostics */
static void	aac_describe_controller(struct aac_softc *sc);
static char	*aac_describe_code(struct aac_code_lookup *table,
				   u_int32_t code);

/* Management Interface */
static d_open_t		aac_open;
static d_close_t	aac_close;
static d_ioctl_t	aac_ioctl;
static int		aac_ioctl_sendfib(struct aac_softc *sc, caddr_t ufib);
static void		aac_handle_aif(struct aac_softc *sc,
					   struct aac_fib *fib);
static int		aac_rev_check(struct aac_softc *sc, caddr_t udata);
static int		aac_getnext_aif(struct aac_softc *sc, caddr_t arg);
static int		aac_return_aif(struct aac_softc *sc, caddr_t uptr);
static int		aac_query_disk(struct aac_softc *sc, caddr_t uptr);

#define AAC_CDEV_MAJOR	150

static struct cdevsw aac_cdevsw = {
	aac_open,		/* open */
	aac_close,		/* close */
	noread,			/* read */
	nowrite,		/* write */
	aac_ioctl,		/* ioctl */
	nopoll,			/* poll */
	nommap,			/* mmap */
	nostrategy,		/* strategy */
	"aac",			/* name */
	AAC_CDEV_MAJOR,		/* major */
	nodump,			/* dump */
	nopsize,		/* psize */
	0,			/* flags */
#if __FreeBSD_version < 500005
	-1,			/* bmaj */
#endif
};

MALLOC_DEFINE(M_AACBUF, "aacbuf", "Buffers for the AAC driver");

/* sysctl node */
SYSCTL_NODE(_hw, OID_AUTO, aac, CTLFLAG_RD, 0, "AAC driver parameters");

/*
 * Device Interface
 */

/*
 * Initialise the controller and softc
 */
int
aac_attach(struct aac_softc *sc)
{
	int error, unit;

	debug_called(1);

	/*
	 * Initialise per-controller queues.
	 */
	aac_initq_free(sc);
	aac_initq_ready(sc);
	aac_initq_busy(sc);
	aac_initq_complete(sc);
	aac_initq_bio(sc);

#if __FreeBSD_version >= 500005
	/*
	 * Initialise command-completion task.
	 */
	TASK_INIT(&sc->aac_task_complete, 0, aac_complete, sc);
#endif

	/* disable interrupts before we enable anything */
	AAC_MASK_INTERRUPTS(sc);

	/* mark controller as suspended until we get ourselves organised */
	sc->aac_state |= AAC_STATE_SUSPEND;

	/*
	 * Allocate command structures.
	 */
	if ((error = aac_alloc_commands(sc)) != 0)
		return(error);

	/*
	 * Initialise the adapter.
	 */
	if ((error = aac_init(sc)) != 0)
		return(error);

	/* 
	 * Print a little information about the controller.
	 */
	aac_describe_controller(sc);

	/*
	 * Register to probe our containers later.
	 */
	TAILQ_INIT(&sc->aac_container_tqh);
	AAC_LOCK_INIT(&sc->aac_container_lock);

	sc->aac_ich.ich_func = aac_startup;
	sc->aac_ich.ich_arg = sc;
	if (config_intrhook_establish(&sc->aac_ich) != 0) {
		device_printf(sc->aac_dev,
			      "can't establish configuration hook\n");
		return(ENXIO);
	}

	/*
	 * Make the control device.
	 */
	unit = device_get_unit(sc->aac_dev);
	sc->aac_dev_t = make_dev(&aac_cdevsw, unit, UID_ROOT, GID_WHEEL, 0644,
				 "aac%d", unit);
#if __FreeBSD_version > 500005
	(void)make_dev_alias(sc->aac_dev_t, "afa%d", unit);
	(void)make_dev_alias(sc->aac_dev_t, "hpn%d", unit);
#endif
	sc->aac_dev_t->si_drv1 = sc;

	/* Create the AIF thread */
#if __FreeBSD_version > 500005
	if (kthread_create((void(*)(void *))aac_host_command, sc,
			   &sc->aifthread, 0, "aac%daif", unit))
#else
	if (kthread_create((void(*)(void *))aac_host_command, sc,
			   &sc->aifthread, "aac%daif", unit))
#endif
		panic("Could not create AIF thread\n");

	/* Register the shutdown method to only be called post-dump */
	if ((EVENTHANDLER_REGISTER(shutdown_final, aac_shutdown, sc->aac_dev,
				   SHUTDOWN_PRI_DEFAULT)) == NULL)
	device_printf(sc->aac_dev, "shutdown event registration failed\n");

	return(0);
}

/*
 * Probe for containers, create disks.
 */
static void
aac_startup(void *arg)
{
	struct aac_softc *sc;
	struct aac_mntinfo mi;
	struct aac_mntinforesponse mir;
	u_int16_t rsize;	
	int i = 0;

	debug_called(1);

	sc = (struct aac_softc *)arg;

	/* disconnect ourselves from the intrhook chain */
	config_intrhook_disestablish(&sc->aac_ich);

	/* loop over possible containers */
	mi.Command = VM_NameServe;
	mi.MntType = FT_FILESYS;
	do {
		/* request information on this container */
		mi.MntCount = i;
		rsize = sizeof(mir);
		if (aac_sync_fib(sc, ContainerCommand, 0, &mi,
				 sizeof(struct aac_mntinfo), &mir, &rsize)) {
			debug(2, "error probing container %d", i);
			continue;
		}
		/* check response size */
		if (rsize != sizeof(mir)) {
			debug(2, "container info response wrong size "
			      "(%d should be %d)", rsize, sizeof(mir));
			continue;
		}

		aac_add_container(sc, &mir, 0);
		i++;
	} while ((i < mir.MntRespCount) && (i < AAC_MAX_CONTAINERS));

	/* poke the bus to actually attach the child devices */
	if (bus_generic_attach(sc->aac_dev))
		device_printf(sc->aac_dev, "bus_generic_attach failed\n");

	/* mark the controller up */
	sc->aac_state &= ~AAC_STATE_SUSPEND;

	/* enable interrupts now */
	AAC_UNMASK_INTERRUPTS(sc);

	/* enable the timeout watchdog */
	timeout((timeout_t*)aac_timeout, sc, AAC_PERIODIC_INTERVAL * hz);
}

/*
 * Create a device to respresent a new container
 */
static void
aac_add_container(struct aac_softc *sc, struct aac_mntinforesponse *mir, int f)
{
	struct aac_container *co;
	device_t child;

	/* 
	 * Check container volume type for validity.  Note that many of
	 * the possible types may never show up.
	 */
	if ((mir->Status == ST_OK) && (mir->MntTable[0].VolType != CT_NONE)) {
		MALLOC(co, struct aac_container *, sizeof *co, M_AACBUF,
		       M_NOWAIT);
		if (co == NULL)
			panic("Out of memory?!\n");
		debug(1, "id %x  name '%.16s'  size %u  type %d", 
		      mir->MntTable[0].ObjectId,
		      mir->MntTable[0].FileSystemName,
		      mir->MntTable[0].Capacity, mir->MntTable[0].VolType);
	
		if ((child = device_add_child(sc->aac_dev, NULL, -1)) == NULL)
			device_printf(sc->aac_dev, "device_add_child failed\n");
		else
			device_set_ivars(child, co);
		device_set_desc(child, aac_describe_code(aac_container_types,
				mir->MntTable[0].VolType));
		co->co_disk = child;
		co->co_found = f;
		bcopy(&mir->MntTable[0], &co->co_mntobj,
		      sizeof(struct aac_mntobj));
		AAC_LOCK_AQUIRE(&sc->aac_container_lock);
		TAILQ_INSERT_TAIL(&sc->aac_container_tqh, co, co_link);
		AAC_LOCK_RELEASE(&sc->aac_container_lock);
	}
}

/*
 * Free all of the resources associated with (sc)
 *
 * Should not be called if the controller is active.
 */
void
aac_free(struct aac_softc *sc)
{
	debug_called(1);

	/* remove the control device */
	if (sc->aac_dev_t != NULL)
		destroy_dev(sc->aac_dev_t);

	/* throw away any FIB buffers, discard the FIB DMA tag */
	if (sc->aac_fibs != NULL)
		aac_free_commands(sc);
	if (sc->aac_fib_dmat)
		bus_dma_tag_destroy(sc->aac_fib_dmat);

	/* destroy the common area */
	if (sc->aac_common) {
		bus_dmamap_unload(sc->aac_common_dmat, sc->aac_common_dmamap);
		bus_dmamem_free(sc->aac_common_dmat, sc->aac_common,
				sc->aac_common_dmamap);
	}
	if (sc->aac_common_dmat)
		bus_dma_tag_destroy(sc->aac_common_dmat);

	/* disconnect the interrupt handler */
	if (sc->aac_intr)
		bus_teardown_intr(sc->aac_dev, sc->aac_irq, sc->aac_intr);
	if (sc->aac_irq != NULL)
		bus_release_resource(sc->aac_dev, SYS_RES_IRQ, sc->aac_irq_rid,
				     sc->aac_irq);

	/* destroy data-transfer DMA tag */
	if (sc->aac_buffer_dmat)
		bus_dma_tag_destroy(sc->aac_buffer_dmat);

	/* destroy the parent DMA tag */
	if (sc->aac_parent_dmat)
		bus_dma_tag_destroy(sc->aac_parent_dmat);

	/* release the register window mapping */
	if (sc->aac_regs_resource != NULL)
		bus_release_resource(sc->aac_dev, SYS_RES_MEMORY,
				     sc->aac_regs_rid, sc->aac_regs_resource);
}

/*
 * Disconnect from the controller completely, in preparation for unload.
 */
int
aac_detach(device_t dev)
{
	struct aac_softc *sc;
#if AAC_BROKEN
	int error;
#endif

	debug_called(1);

	sc = device_get_softc(dev);

	if (sc->aac_state & AAC_STATE_OPEN)
	return(EBUSY);

#if AAC_BROKEN
	if (sc->aifflags & AAC_AIFFLAGS_RUNNING) {
		sc->aifflags |= AAC_AIFFLAGS_EXIT;
		wakeup(sc->aifthread);
		tsleep(sc->aac_dev, PUSER | PCATCH, "aacdch", 30 * hz);
	}

	if (sc->aifflags & AAC_AIFFLAGS_RUNNING)
		panic("Cannot shutdown AIF thread\n");

	if ((error = aac_shutdown(dev)))
		return(error);

	aac_free(sc);

	return(0);
#else
	return (EBUSY);
#endif
}

/*
 * Bring the controller down to a dormant state and detach all child devices.
 *
 * This function is called before detach or system shutdown.
 *
 * Note that we can assume that the bioq on the controller is empty, as we won't
 * allow shutdown if any device is open.
 */
int
aac_shutdown(device_t dev)
{
	struct aac_softc *sc;
	struct aac_close_command cc;
	int s, i;

	debug_called(1);

	sc = device_get_softc(dev);

	s = splbio();

	sc->aac_state |= AAC_STATE_SUSPEND;

	/* 
	 * Send a Container shutdown followed by a HostShutdown FIB to the
	 * controller to convince it that we don't want to talk to it anymore.
	 * We've been closed and all I/O completed already
	 */
	device_printf(sc->aac_dev, "shutting down controller...");

	cc.Command = VM_CloseAll;
	cc.ContainerId = 0xffffffff;
	if (aac_sync_fib(sc, ContainerCommand, 0, &cc, sizeof(cc), NULL, NULL))
		printf("FAILED.\n");
	else {
		i = 0;
		/*
		 * XXX Issuing this command to the controller makes it shut down
		 * but also keeps it from coming back up without a reset of the
		 * PCI bus.  This is not desirable if you are just unloading the
		 * driver module with the intent to reload it later.
		 */
		if (aac_sync_fib(sc, FsaHostShutdown, AAC_FIBSTATE_SHUTDOWN, &i,
				 sizeof(i), NULL, NULL)) {
			printf("FAILED.\n");
		} else {
			printf("done.\n");
		}
	}

	AAC_MASK_INTERRUPTS(sc);

	splx(s);
	return(0);
}

/*
 * Bring the controller to a quiescent state, ready for system suspend.
 */
int
aac_suspend(device_t dev)
{
	struct aac_softc *sc;
	int s;

	debug_called(1);

	sc = device_get_softc(dev);

	s = splbio();

	sc->aac_state |= AAC_STATE_SUSPEND;
	
	AAC_MASK_INTERRUPTS(sc);
	splx(s);
	return(0);
}

/*
 * Bring the controller back to a state ready for operation.
 */
int
aac_resume(device_t dev)
{
	struct aac_softc *sc;

	debug_called(1);

	sc = device_get_softc(dev);

	sc->aac_state &= ~AAC_STATE_SUSPEND;
	AAC_UNMASK_INTERRUPTS(sc);
	return(0);
}

/*
 * Take an interrupt.
 */
void
aac_intr(void *arg)
{
	struct aac_softc *sc;
	u_int16_t reason;

	debug_called(2);

	sc = (struct aac_softc *)arg;

	reason = AAC_GET_ISTATUS(sc);

	/* controller wants to talk to the log?  Defer it to the AIF thread */
	if (reason & AAC_DB_PRINTF) {
		AAC_CLEAR_ISTATUS(sc, AAC_DB_PRINTF);
		if (sc->aifflags & AAC_AIFFLAGS_RUNNING) {
			sc->aifflags |= AAC_AIFFLAGS_PENDING;
			wakeup(sc->aifthread);
		} else
			aac_print_printf(sc);
	}

	/* controller has a message for us? */
	if (reason & AAC_DB_COMMAND_READY) {
		AAC_CLEAR_ISTATUS(sc, AAC_DB_COMMAND_READY);
		/* XXX What happens if the thread is already awake? */
		if (sc->aifflags & AAC_AIFFLAGS_RUNNING) {
			sc->aifflags |= AAC_AIFFLAGS_PENDING;
			wakeup(sc->aifthread);
		}
	}
	
	/* controller has a response for us? */
	if (reason & AAC_DB_RESPONSE_READY) {
		AAC_CLEAR_ISTATUS(sc, AAC_DB_RESPONSE_READY);
		aac_host_response(sc);
	}

	/*
	 * spurious interrupts that we don't use - reset the mask and clear the
	 * interrupts
	 */
	if (reason & (AAC_DB_COMMAND_NOT_FULL | AAC_DB_RESPONSE_NOT_FULL)) {
		AAC_UNMASK_INTERRUPTS(sc);
		AAC_CLEAR_ISTATUS(sc, AAC_DB_COMMAND_NOT_FULL |
				  AAC_DB_RESPONSE_NOT_FULL);
	}
};

/*
 * Command Processing
 */

/*
 * Start as much queued I/O as possible on the controller
 */
static void
aac_startio(struct aac_softc *sc)
{
	struct aac_command *cm;

	debug_called(2);

	for (;;) {
		/*
		 * Try to get a command that's been put off for lack of 
		 * resources
		 */
		cm = aac_dequeue_ready(sc);

		/*
		 * Try to build a command off the bio queue (ignore error 
		 * return)
		 */
		if (cm == NULL)
			aac_bio_command(sc, &cm);

		/* nothing to do? */
		if (cm == NULL)
			break;

		/* try to give the command to the controller */
		if (aac_start(cm) == EBUSY) {
			/* put it on the ready queue for later */
			aac_requeue_ready(cm);
			break;
		}
	}
}

/*
 * Deliver a command to the controller; allocate controller resources at the
 * last moment when possible.
 */
static int
aac_start(struct aac_command *cm)
{
	struct aac_softc *sc;
	int error;

	debug_called(2);

	sc = cm->cm_sc;

	/* get the command mapped */
	aac_map_command(cm);

	/* fix up the address values in the FIB */
	cm->cm_fib->Header.SenderFibAddress = (u_int32_t)cm->cm_fib;
	cm->cm_fib->Header.ReceiverFibAddress = cm->cm_fibphys;

	/* save a pointer to the command for speedy reverse-lookup */
	cm->cm_fib->Header.SenderData = (u_int32_t)cm;	/* XXX 64-bit physical
							 * address issue */

	/* put the FIB on the outbound queue */
	error = aac_enqueue_fib(sc, cm->cm_queue, cm);
	return(error);
}

/*
 * Handle notification of one or more FIBs coming from the controller.
 */
static void
aac_host_command(struct aac_softc *sc)
{
	struct aac_fib *fib;
	u_int32_t fib_size;
	int size;

	debug_called(2);

	sc->aifflags |= AAC_AIFFLAGS_RUNNING;

	while (!(sc->aifflags & AAC_AIFFLAGS_EXIT)) {
		if (!(sc->aifflags & AAC_AIFFLAGS_PENDING))
			tsleep(sc->aifthread, PRIBIO, "aifthd", 15 * hz);

		sc->aifflags &= ~AAC_AIFFLAGS_PENDING;
		for (;;) {
			if (aac_dequeue_fib(sc, AAC_HOST_NORM_CMD_QUEUE,
					    &fib_size, &fib))
				break;	/* nothing to do */
	
			AAC_PRINT_FIB(sc, fib);
	
			switch (fib->Header.Command) {
			case AifRequest:
				aac_handle_aif(sc, fib);
				break;
			default:
				device_printf(sc->aac_dev, "unknown command "
					      "from controller\n");
				break;
			}

			/* Return the AIF to the controller. */
			if ((fib->Header.XferState == 0) ||
			    (fib->Header.StructType != AAC_FIBTYPE_TFIB))
				break;

			if (fib->Header.XferState & AAC_FIBSTATE_FROMADAP) {
				fib->Header.XferState |= AAC_FIBSTATE_DONEHOST;
				*(AAC_FSAStatus*)fib->data = ST_OK;

				/* XXX Compute the Size field? */
				size = fib->Header.Size;
				if (size > sizeof(struct aac_fib)) {
	 				size = sizeof(struct aac_fib);
					fib->Header.Size = size;
				}
				/*
				 * Since we did not generate this command, it
				 * cannot go through the normal
				 * enqueue->startio chain.
				 */
				aac_enqueue_response(sc,
						     AAC_ADAP_NORM_RESP_QUEUE,
						     fib);
			}
		}
		aac_print_printf(sc);

	}
	sc->aifflags &= ~AAC_AIFFLAGS_RUNNING;
	wakeup(sc->aac_dev);

#if __FreeBSD_version > 500005
	mtx_lock(&Giant);
#endif
	kthread_exit(0);
}

/*
 * Handle notification of one or more FIBs completed by the controller
 */
static void
aac_host_response(struct aac_softc *sc)
{
	struct aac_command *cm;
	struct aac_fib *fib;
	u_int32_t fib_size;

	debug_called(2);

	for (;;) {
		/* look for completed FIBs on our queue */
		if (aac_dequeue_fib(sc, AAC_HOST_NORM_RESP_QUEUE, &fib_size,
				    &fib))
			break;	/* nothing to do */
	
		/* get the command, unmap and queue for later processing */
		cm = (struct aac_command *)fib->Header.SenderData;
		if (cm == NULL) {
			AAC_PRINT_FIB(sc, fib);
		} else {
			aac_remove_busy(cm);
			aac_unmap_command(cm);		/* XXX defer? */
			aac_enqueue_complete(cm);
		}
	}

	/* handle completion processing */
#if __FreeBSD_version >= 500005
	taskqueue_enqueue(taskqueue_swi, &sc->aac_task_complete);
#else
	aac_complete(sc, 0);
#endif
}

/*
 * Process completed commands.
 */
static void
aac_complete(void *context, int pending)
{
	struct aac_softc *sc;
	struct aac_command *cm;
	
	debug_called(2);

	sc = (struct aac_softc *)context;

	/* pull completed commands off the queue */
	for (;;) {
		cm = aac_dequeue_complete(sc);
		if (cm == NULL)
			break;
		cm->cm_flags |= AAC_CMD_COMPLETED;

		/* is there a completion handler? */
		if (cm->cm_complete != NULL) {
			cm->cm_complete(cm);
		} else {
			/* assume that someone is sleeping on this command */
			wakeup(cm);
		}
	}

	/* see if we can start some more I/O */
	aac_startio(sc);
}

/*
 * Handle a bio submitted from a disk device.
 */
void
aac_submit_bio(struct bio *bp)
{
	struct aac_disk *ad;
	struct aac_softc *sc;

	debug_called(2);

	ad = (struct aac_disk *)bp->bio_dev->si_drv1;
	sc = ad->ad_controller;

	/* queue the BIO and try to get some work done */
	aac_enqueue_bio(sc, bp);
	aac_startio(sc);
}

/*
 * Get a bio and build a command to go with it.
 */
static int
aac_bio_command(struct aac_softc *sc, struct aac_command **cmp)
{
	struct aac_command *cm;
	struct aac_fib *fib;
	struct aac_blockread *br;
	struct aac_blockwrite *bw;
	struct aac_disk *ad;
	struct bio *bp;

	debug_called(2);

	/* get the resources we will need */
	cm = NULL;
	if ((bp = aac_dequeue_bio(sc)) == NULL)
		goto fail;
	if (aac_alloc_command(sc, &cm))	/* get a command */
		goto fail;

	/* fill out the command */
	cm->cm_data = (void *)bp->bio_data;
	cm->cm_datalen = bp->bio_bcount;
	cm->cm_complete = aac_bio_complete;
	cm->cm_private = bp;
	cm->cm_timestamp = time_second;
	cm->cm_queue = AAC_ADAP_NORM_CMD_QUEUE;

	/* build the FIB */
	fib = cm->cm_fib;
	fib->Header.XferState =  
	AAC_FIBSTATE_HOSTOWNED   | 
	AAC_FIBSTATE_INITIALISED | 
	AAC_FIBSTATE_FROMHOST	 |
	AAC_FIBSTATE_REXPECTED   |
	AAC_FIBSTATE_NORM;
	fib->Header.Command = ContainerCommand;
	fib->Header.Size = sizeof(struct aac_fib_header);

	/* build the read/write request */
	ad = (struct aac_disk *)bp->bio_dev->si_drv1;
	if (BIO_IS_READ(bp)) {
		br = (struct aac_blockread *)&fib->data[0];
		br->Command = VM_CtBlockRead;
		br->ContainerId = ad->ad_container->co_mntobj.ObjectId;
		br->BlockNumber = bp->bio_pblkno;
		br->ByteCount = bp->bio_bcount;
		fib->Header.Size += sizeof(struct aac_blockread);
		cm->cm_sgtable = &br->SgMap;
		cm->cm_flags |= AAC_CMD_DATAIN;
	} else {
		bw = (struct aac_blockwrite *)&fib->data[0];
		bw->Command = VM_CtBlockWrite;
		bw->ContainerId = ad->ad_container->co_mntobj.ObjectId;
		bw->BlockNumber = bp->bio_pblkno;
		bw->ByteCount = bp->bio_bcount;
		bw->Stable = CUNSTABLE;	/* XXX what's appropriate here? */
		fib->Header.Size += sizeof(struct aac_blockwrite);
		cm->cm_flags |= AAC_CMD_DATAOUT;
		cm->cm_sgtable = &bw->SgMap;
	}

	*cmp = cm;
	return(0);

fail:
	if (bp != NULL)
		aac_enqueue_bio(sc, bp);
	if (cm != NULL)
		aac_release_command(cm);
	return(ENOMEM);
}

/*
 * Handle a bio-instigated command that has been completed.
 */
static void
aac_bio_complete(struct aac_command *cm)
{
	struct aac_blockread_response *brr;
	struct aac_blockwrite_response *bwr;
	struct bio *bp;
	AAC_FSAStatus status;

	/* fetch relevant status and then release the command */
	bp = (struct bio *)cm->cm_private;
	if (BIO_IS_READ(bp)) {
		brr = (struct aac_blockread_response *)&cm->cm_fib->data[0];
		status = brr->Status;
	} else {
		bwr = (struct aac_blockwrite_response *)&cm->cm_fib->data[0];
		status = bwr->Status;
	}
	aac_release_command(cm);

	/* fix up the bio based on status */
	if (status == ST_OK) {
		bp->bio_resid = 0;
	} else {
		bp->bio_error = EIO;
		bp->bio_flags |= BIO_ERROR;
		/* pass an error string out to the disk layer */
		bp->bio_driver1 = aac_describe_code(aac_command_status_table,
						    status);
	}
	aac_biodone(bp);
}

/*
 * Dump a block of data to the controller.  If the queue is full, tell the
 * caller to hold off and wait for the queue to drain.
 */
int
aac_dump_enqueue(struct aac_disk *ad, u_int32_t lba, void *data, int dumppages)
{
	struct aac_softc *sc;
	struct aac_command *cm;
	struct aac_fib *fib;
	struct aac_blockwrite *bw;

	sc = ad->ad_controller;
	cm = NULL;

	if (aac_alloc_command(sc, &cm))
		return (EBUSY);

	/* fill out the command */
	cm->cm_data = data;
	cm->cm_datalen = dumppages * PAGE_SIZE;
	cm->cm_complete = NULL;
	cm->cm_private = NULL;
	cm->cm_timestamp = time_second;
	cm->cm_queue = AAC_ADAP_NORM_CMD_QUEUE;

	/* build the FIB */
	fib = cm->cm_fib;
	fib->Header.XferState =  
	AAC_FIBSTATE_HOSTOWNED   | 
	AAC_FIBSTATE_INITIALISED | 
	AAC_FIBSTATE_FROMHOST	 |
	AAC_FIBSTATE_REXPECTED   |
	AAC_FIBSTATE_NORM;
	fib->Header.Command = ContainerCommand;
	fib->Header.Size = sizeof(struct aac_fib_header);

	bw = (struct aac_blockwrite *)&fib->data[0];
	bw->Command = VM_CtBlockWrite;
	bw->ContainerId = ad->ad_container->co_mntobj.ObjectId;
	bw->BlockNumber = lba;
	bw->ByteCount = dumppages * PAGE_SIZE;
	bw->Stable = CUNSTABLE;		/* XXX what's appropriate here? */
	fib->Header.Size += sizeof(struct aac_blockwrite);
	cm->cm_flags |= AAC_CMD_DATAOUT;
	cm->cm_sgtable = &bw->SgMap;

	return (aac_start(cm));
}

/*
 * Wait for the card's queue to drain when dumping.  Also check for monitor
 * printf's
 */
void
aac_dump_complete(struct aac_softc *sc)
{
	struct aac_fib *fib;
	struct aac_command *cm;
	u_int16_t reason;
	u_int32_t pi, ci, fib_size;

	do {
		reason = AAC_GET_ISTATUS(sc);
		if (reason & AAC_DB_RESPONSE_READY) {
			AAC_CLEAR_ISTATUS(sc, AAC_DB_RESPONSE_READY);
			for (;;) {
				if (aac_dequeue_fib(sc,
						    AAC_HOST_NORM_RESP_QUEUE,
						    &fib_size, &fib))
					break;
				cm = (struct aac_command *)
					fib->Header.SenderData;
				if (cm == NULL)
					AAC_PRINT_FIB(sc, fib);
				else {
					aac_remove_busy(cm);
					aac_unmap_command(cm);
					aac_enqueue_complete(cm);
					aac_release_command(cm);
				}
			}
		}
		if (reason & AAC_DB_PRINTF) {
			AAC_CLEAR_ISTATUS(sc, AAC_DB_PRINTF);
			aac_print_printf(sc);
		}
		pi = sc->aac_queues->qt_qindex[AAC_ADAP_NORM_CMD_QUEUE][
			AAC_PRODUCER_INDEX];
		ci = sc->aac_queues->qt_qindex[AAC_ADAP_NORM_CMD_QUEUE][	
			AAC_CONSUMER_INDEX];
	} while (ci != pi);

	return;
}

/*
 * Submit a command to the controller, return when it completes.
 */
static int
aac_wait_command(struct aac_command *cm, int timeout)
{
	int s, error = 0;

	debug_called(2);

	/* Put the command on the ready queue and get things going */
	cm->cm_queue = AAC_ADAP_NORM_CMD_QUEUE;
	aac_enqueue_ready(cm);
	aac_startio(cm->cm_sc);
	s = splbio();
	while (!(cm->cm_flags & AAC_CMD_COMPLETED) && (error != EWOULDBLOCK)) {
		error = tsleep(cm, PRIBIO | PCATCH, "aacwait", 0);
		if ((error == ERESTART) || (error == EINTR))
			break;
	}
	splx(s);
	return(error);
}

/*
 *Command Buffer Management
 */

/*
 * Allocate a command.
 */
static int
aac_alloc_command(struct aac_softc *sc, struct aac_command **cmp)
{
	struct aac_command *cm;

	debug_called(3);

	if ((cm = aac_dequeue_free(sc)) == NULL)
		return(ENOMEM);

	*cmp = cm;
	return(0);
}

/*
 * Release a command back to the freelist.
 */
static void
aac_release_command(struct aac_command *cm)
{
	debug_called(3);

	/* (re)initialise the command/FIB */
	cm->cm_sgtable = NULL;
	cm->cm_flags = 0;
	cm->cm_complete = NULL;
	cm->cm_private = NULL;
	cm->cm_fib->Header.XferState = AAC_FIBSTATE_EMPTY;
	cm->cm_fib->Header.StructType = AAC_FIBTYPE_TFIB;
	cm->cm_fib->Header.Flags = 0;
	cm->cm_fib->Header.SenderSize = sizeof(struct aac_fib);

	/* 
	 * These are duplicated in aac_start to cover the case where an
	 * intermediate stage may have destroyed them.  They're left
	 * initialised here for debugging purposes only.
	 */
	cm->cm_fib->Header.SenderFibAddress = (u_int32_t)cm->cm_fib;
	cm->cm_fib->Header.ReceiverFibAddress = cm->cm_fibphys;

	aac_enqueue_free(cm);
}

/*
 * Map helper for command/FIB allocation.
 */
static void
aac_map_command_helper(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct aac_softc *sc;

	sc = (struct aac_softc *)arg;

	debug_called(3);

	sc->aac_fibphys = segs[0].ds_addr;
}

/*
 * Allocate and initialise commands/FIBs for this adapter.
 */
static int
aac_alloc_commands(struct aac_softc *sc)
{
	struct aac_command *cm;
	int i;
 
	debug_called(1);

	/* allocate the FIBs in DMAable memory and load them */
	if (bus_dmamem_alloc(sc->aac_fib_dmat, (void **)&sc->aac_fibs,
			 BUS_DMA_NOWAIT, &sc->aac_fibmap)) {
		return(ENOMEM);
	}
	bus_dmamap_load(sc->aac_fib_dmat, sc->aac_fibmap, sc->aac_fibs, 
			AAC_FIB_COUNT * sizeof(struct aac_fib),
			aac_map_command_helper, sc, 0);

	/* initialise constant fields in the command structure */
	for (i = 0; i < AAC_FIB_COUNT; i++) {
		cm = &sc->aac_command[i];
		cm->cm_sc = sc;
		cm->cm_fib = sc->aac_fibs + i;
		cm->cm_fibphys = sc->aac_fibphys + (i * sizeof(struct aac_fib));

		if (!bus_dmamap_create(sc->aac_buffer_dmat, 0, &cm->cm_datamap))
			aac_release_command(cm);
	}
	return(0);
}

/*
 * Free FIBs owned by this adapter.
 */
static void
aac_free_commands(struct aac_softc *sc)
{
	int i;

	debug_called(1);

	for (i = 0; i < AAC_FIB_COUNT; i++)
		bus_dmamap_destroy(sc->aac_buffer_dmat,
				   sc->aac_command[i].cm_datamap);

	bus_dmamap_unload(sc->aac_fib_dmat, sc->aac_fibmap);
	bus_dmamem_free(sc->aac_fib_dmat, sc->aac_fibs, sc->aac_fibmap);
}

/*
 * Command-mapping helper function - populate this command's s/g table.
 */
static void
aac_map_command_sg(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct aac_command *cm;
	struct aac_fib *fib;
	struct aac_sg_table *sg;
	int i;

	debug_called(3);

	cm = (struct aac_command *)arg;
	fib = cm->cm_fib;

	/* find the s/g table */
	sg = cm->cm_sgtable;

	/* copy into the FIB */
	if (sg != NULL) {
		sg->SgCount = nseg;
		for (i = 0; i < nseg; i++) {
			sg->SgEntry[i].SgAddress = segs[i].ds_addr;
			sg->SgEntry[i].SgByteCount = segs[i].ds_len;
		}
		/* update the FIB size for the s/g count */
		fib->Header.Size += nseg * sizeof(struct aac_sg_entry);
	}

}

/*
 * Map a command into controller-visible space.
 */
static void
aac_map_command(struct aac_command *cm)
{
	struct aac_softc *sc;

	debug_called(2);

	sc = cm->cm_sc;

	/* don't map more than once */
	if (cm->cm_flags & AAC_CMD_MAPPED)
		return;

	if (cm->cm_datalen != 0) {
		bus_dmamap_load(sc->aac_buffer_dmat, cm->cm_datamap,
				cm->cm_data, cm->cm_datalen,
				aac_map_command_sg, cm, 0);

	if (cm->cm_flags & AAC_CMD_DATAIN)
		bus_dmamap_sync(sc->aac_buffer_dmat, cm->cm_datamap,
				BUS_DMASYNC_PREREAD);
	if (cm->cm_flags & AAC_CMD_DATAOUT)
		bus_dmamap_sync(sc->aac_buffer_dmat, cm->cm_datamap,
				BUS_DMASYNC_PREWRITE);
	}
	cm->cm_flags |= AAC_CMD_MAPPED;
}

/*
 * Unmap a command from controller-visible space.
 */
static void
aac_unmap_command(struct aac_command *cm)
{
	struct aac_softc *sc;

	debug_called(2);

	sc = cm->cm_sc;

	if (!(cm->cm_flags & AAC_CMD_MAPPED))
		return;

	if (cm->cm_datalen != 0) {
		if (cm->cm_flags & AAC_CMD_DATAIN)
			bus_dmamap_sync(sc->aac_buffer_dmat, cm->cm_datamap,
					BUS_DMASYNC_POSTREAD);
		if (cm->cm_flags & AAC_CMD_DATAOUT)
			bus_dmamap_sync(sc->aac_buffer_dmat, cm->cm_datamap,
					BUS_DMASYNC_POSTWRITE);

		bus_dmamap_unload(sc->aac_buffer_dmat, cm->cm_datamap);
	}
	cm->cm_flags &= ~AAC_CMD_MAPPED;
}

/*
 * Hardware Interface
 */

/*
 * Initialise the adapter.
 */
static void
aac_common_map(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct aac_softc *sc;

	debug_called(1);

	sc = (struct aac_softc *)arg;

	sc->aac_common_busaddr = segs[0].ds_addr;
}

static int
aac_init(struct aac_softc *sc)
{
	struct aac_adapter_init	*ip;
	time_t then;
	u_int32_t code;
	u_int8_t *qaddr;

	debug_called(1);

	/*
	 * First wait for the adapter to come ready.
	 */
	then = time_second;
	do {
		code = AAC_GET_FWSTATUS(sc);
		if (code & AAC_SELF_TEST_FAILED) {
			device_printf(sc->aac_dev, "FATAL: selftest failed\n");
			return(ENXIO);
		}
		if (code & AAC_KERNEL_PANIC) {
			device_printf(sc->aac_dev,
				      "FATAL: controller kernel panic\n");
			return(ENXIO);
		}
		if (time_second > (then + AAC_BOOT_TIMEOUT)) {
			device_printf(sc->aac_dev,
				      "FATAL: controller not coming ready, "
					   "status %x\n", code);
			return(ENXIO);
		}
	} while (!(code & AAC_UP_AND_RUNNING));

	/*
	 * Create DMA tag for the common structure and allocate it.
	 */
	if (bus_dma_tag_create(sc->aac_parent_dmat, 	/* parent */
			       1, 0, 			/* algnmnt, boundary */
			       BUS_SPACE_MAXADDR,	/* lowaddr */
			       BUS_SPACE_MAXADDR, 	/* highaddr */
			       NULL, NULL, 		/* filter, filterarg */
			       sizeof(struct aac_common), /* maxsize */
			       1,			/* nsegments */
			       BUS_SPACE_MAXSIZE_32BIT,	/* maxsegsize */
			       0,			/* flags */
			       &sc->aac_common_dmat)) {
		device_printf(sc->aac_dev,
			      "can't allocate common structure DMA tag\n");
		return(ENOMEM);
	}
	if (bus_dmamem_alloc(sc->aac_common_dmat, (void **)&sc->aac_common,
			     BUS_DMA_NOWAIT, &sc->aac_common_dmamap)) {
		device_printf(sc->aac_dev, "can't allocate common structure\n");
		return(ENOMEM);
	}
	bus_dmamap_load(sc->aac_common_dmat, sc->aac_common_dmamap,
			sc->aac_common, sizeof(*sc->aac_common), aac_common_map,
		        sc, 0);
	bzero(sc->aac_common, sizeof(*sc->aac_common));
	
	/*
	 * Fill in the init structure.  This tells the adapter about the
	 * physical location of various important shared data structures.
	 */
	ip = &sc->aac_common->ac_init;
	ip->InitStructRevision = AAC_INIT_STRUCT_REVISION;

	ip->AdapterFibsPhysicalAddress = sc->aac_common_busaddr +
					 offsetof(struct aac_common, ac_fibs);
	ip->AdapterFibsVirtualAddress = &sc->aac_common->ac_fibs[0];
	ip->AdapterFibsSize = AAC_ADAPTER_FIBS * sizeof(struct aac_fib);
	ip->AdapterFibAlign = sizeof(struct aac_fib);

	ip->PrintfBufferAddress = sc->aac_common_busaddr +
				  offsetof(struct aac_common, ac_printf);
	ip->PrintfBufferSize = AAC_PRINTF_BUFSIZE;

	ip->HostPhysMemPages = 0;		/* not used? */
	ip->HostElapsedSeconds = time_second;	/* reset later if invalid */

	/*
	 * Initialise FIB queues.  Note that it appears that the layout of the
	 * indexes and the segmentation of the entries may be mandated by the
	 * adapter, which is only told about the base of the queue index fields.
	 *
	 * The initial values of the indices are assumed to inform the adapter
	 * of the sizes of the respective queues, and theoretically it could 
	 * work out the entire layout of the queue structures from this.  We
	 * take the easy route and just lay this area out like everyone else
	 * does.
	 *
	 * The Linux driver uses a much more complex scheme whereby several 
	 * header records are kept for each queue.  We use a couple of generic 
	 * list manipulation functions which 'know' the size of each list by
	 * virtue of a table.
	 */
	qaddr = &sc->aac_common->ac_qbuf[0] + AAC_QUEUE_ALIGN;
	qaddr -= (u_int32_t)qaddr % AAC_QUEUE_ALIGN;
	sc->aac_queues = (struct aac_queue_table *)qaddr;
	ip->CommHeaderAddress = sc->aac_common_busaddr +
				((u_int32_t)sc->aac_queues -
				(u_int32_t)sc->aac_common);
	bzero(sc->aac_queues, sizeof(struct aac_queue_table));

	sc->aac_queues->qt_qindex[AAC_HOST_NORM_CMD_QUEUE][AAC_PRODUCER_INDEX] =
		AAC_HOST_NORM_CMD_ENTRIES;
	sc->aac_queues->qt_qindex[AAC_HOST_NORM_CMD_QUEUE][AAC_CONSUMER_INDEX] =
		AAC_HOST_NORM_CMD_ENTRIES;
	sc->aac_queues->qt_qindex[AAC_HOST_HIGH_CMD_QUEUE][AAC_PRODUCER_INDEX] =
		AAC_HOST_HIGH_CMD_ENTRIES;
	sc->aac_queues->qt_qindex[AAC_HOST_HIGH_CMD_QUEUE][AAC_CONSUMER_INDEX] =
		AAC_HOST_HIGH_CMD_ENTRIES;
	sc->aac_queues->qt_qindex[AAC_ADAP_NORM_CMD_QUEUE][AAC_PRODUCER_INDEX] =
		AAC_ADAP_NORM_CMD_ENTRIES;
	sc->aac_queues->qt_qindex[AAC_ADAP_NORM_CMD_QUEUE][AAC_CONSUMER_INDEX] =
		AAC_ADAP_NORM_CMD_ENTRIES;
	sc->aac_queues->qt_qindex[AAC_ADAP_HIGH_CMD_QUEUE][AAC_PRODUCER_INDEX] =
		AAC_ADAP_HIGH_CMD_ENTRIES;
	sc->aac_queues->qt_qindex[AAC_ADAP_HIGH_CMD_QUEUE][AAC_CONSUMER_INDEX] =
		AAC_ADAP_HIGH_CMD_ENTRIES;
	sc->aac_queues->qt_qindex[AAC_HOST_NORM_RESP_QUEUE][AAC_PRODUCER_INDEX]=
		AAC_HOST_NORM_RESP_ENTRIES;
	sc->aac_queues->qt_qindex[AAC_HOST_NORM_RESP_QUEUE][AAC_CONSUMER_INDEX]=
		AAC_HOST_NORM_RESP_ENTRIES;
	sc->aac_queues->qt_qindex[AAC_HOST_HIGH_RESP_QUEUE][AAC_PRODUCER_INDEX]=
		AAC_HOST_HIGH_RESP_ENTRIES;
	sc->aac_queues->qt_qindex[AAC_HOST_HIGH_RESP_QUEUE][AAC_CONSUMER_INDEX]=
		AAC_HOST_HIGH_RESP_ENTRIES;
	sc->aac_queues->qt_qindex[AAC_ADAP_NORM_RESP_QUEUE][AAC_PRODUCER_INDEX]=
		AAC_ADAP_NORM_RESP_ENTRIES;
	sc->aac_queues->qt_qindex[AAC_ADAP_NORM_RESP_QUEUE][AAC_CONSUMER_INDEX]=
		AAC_ADAP_NORM_RESP_ENTRIES;
	sc->aac_queues->qt_qindex[AAC_ADAP_HIGH_RESP_QUEUE][AAC_PRODUCER_INDEX]=
		AAC_ADAP_HIGH_RESP_ENTRIES;
	sc->aac_queues->qt_qindex[AAC_ADAP_HIGH_RESP_QUEUE][AAC_CONSUMER_INDEX]=
		AAC_ADAP_HIGH_RESP_ENTRIES;
	sc->aac_qentries[AAC_HOST_NORM_CMD_QUEUE] =
		&sc->aac_queues->qt_HostNormCmdQueue[0];
	sc->aac_qentries[AAC_HOST_HIGH_CMD_QUEUE] =
		&sc->aac_queues->qt_HostHighCmdQueue[0];
	sc->aac_qentries[AAC_ADAP_NORM_CMD_QUEUE] =
		&sc->aac_queues->qt_AdapNormCmdQueue[0];
	sc->aac_qentries[AAC_ADAP_HIGH_CMD_QUEUE] =
		&sc->aac_queues->qt_AdapHighCmdQueue[0];
	sc->aac_qentries[AAC_HOST_NORM_RESP_QUEUE] =
		&sc->aac_queues->qt_HostNormRespQueue[0];
	sc->aac_qentries[AAC_HOST_HIGH_RESP_QUEUE] =
		&sc->aac_queues->qt_HostHighRespQueue[0];
	sc->aac_qentries[AAC_ADAP_NORM_RESP_QUEUE] =
		&sc->aac_queues->qt_AdapNormRespQueue[0];
	sc->aac_qentries[AAC_ADAP_HIGH_RESP_QUEUE] =
		&sc->aac_queues->qt_AdapHighRespQueue[0];

	/*
	 * Do controller-type-specific initialisation
	 */
	switch (sc->aac_hwif) {
	case AAC_HWIF_I960RX:
		AAC_SETREG4(sc, AAC_RX_ODBR, ~0);
		break;
	}

	/*
	 * Give the init structure to the controller.
	 */
	if (aac_sync_command(sc, AAC_MONKER_INITSTRUCT, 
			     sc->aac_common_busaddr +
			     offsetof(struct aac_common, ac_init), 0, 0, 0,
			     NULL)) {
		device_printf(sc->aac_dev,
			      "error establishing init structure\n");
		return(EIO);
	}

	return(0);
}

/*
 * Send a synchronous command to the controller and wait for a result.
 */
static int
aac_sync_command(struct aac_softc *sc, u_int32_t command,
		 u_int32_t arg0, u_int32_t arg1, u_int32_t arg2, u_int32_t arg3,
		 u_int32_t *sp)
{
	time_t then;
	u_int32_t status;

	debug_called(3);

	/* populate the mailbox */
	AAC_SET_MAILBOX(sc, command, arg0, arg1, arg2, arg3);

	/* ensure the sync command doorbell flag is cleared */
	AAC_CLEAR_ISTATUS(sc, AAC_DB_SYNC_COMMAND);

	/* then set it to signal the adapter */
	AAC_QNOTIFY(sc, AAC_DB_SYNC_COMMAND);

	/* spin waiting for the command to complete */
	then = time_second;
	do {
		if (time_second > (then + AAC_IMMEDIATE_TIMEOUT)) {
			debug(2, "timed out");
			return(EIO);
		}
	} while (!(AAC_GET_ISTATUS(sc) & AAC_DB_SYNC_COMMAND));

	/* clear the completion flag */
	AAC_CLEAR_ISTATUS(sc, AAC_DB_SYNC_COMMAND);

	/* get the command status */
	status = AAC_GET_MAILBOXSTATUS(sc);
	if (sp != NULL)
		*sp = status;
	return(0);
}

/*
 * Send a synchronous FIB to the controller and wait for a result.
 */
static int
aac_sync_fib(struct aac_softc *sc, u_int32_t command, u_int32_t xferstate, 
		 void *data, u_int16_t datasize,
		 void *result, u_int16_t *resultsize)
{
	struct aac_fib *fib;

	debug_called(3);

	fib = &sc->aac_common->ac_sync_fib;

	if (datasize > AAC_FIB_DATASIZE)
		return(EINVAL);

	/*
	 * Set up the sync FIB
	 */
	fib->Header.XferState = AAC_FIBSTATE_HOSTOWNED |
				AAC_FIBSTATE_INITIALISED |
				AAC_FIBSTATE_EMPTY;
	fib->Header.XferState |= xferstate;
	fib->Header.Command = command;
	fib->Header.StructType = AAC_FIBTYPE_TFIB;
	fib->Header.Size = sizeof(struct aac_fib) + datasize;
	fib->Header.SenderSize = sizeof(struct aac_fib);
	fib->Header.SenderFibAddress = (u_int32_t)fib;
	fib->Header.ReceiverFibAddress = sc->aac_common_busaddr +
					 offsetof(struct aac_common,
						  ac_sync_fib);

	/*
	 * Copy in data.
	 */
	if (data != NULL) {
		KASSERT(datasize <= sizeof(fib->data),
			("aac_sync_fib: datasize to large"));
		bcopy(data, fib->data, datasize);
		fib->Header.XferState |= AAC_FIBSTATE_FROMHOST |
					 AAC_FIBSTATE_NORM;
	}

	/*
	 * Give the FIB to the controller, wait for a response.
	 */
	if (aac_sync_command(sc, AAC_MONKER_SYNCFIB,
			     fib->Header.ReceiverFibAddress, 0, 0, 0, NULL)) {
		debug(2, "IO error");
		return(EIO);
	}

	/* 
	 * Copy out the result
	 */
	if (result != NULL) {
		u_int copysize;

		copysize = fib->Header.Size - sizeof(struct aac_fib_header);
		if (copysize > *resultsize)
			copysize = *resultsize;
		*resultsize = fib->Header.Size - sizeof(struct aac_fib_header);
		bcopy(fib->data, result, copysize);
	}
	return(0);
}

/*
 * Adapter-space FIB queue manipulation
 *
 * Note that the queue implementation here is a little funky; neither the PI or
 * CI will ever be zero.  This behaviour is a controller feature.
 */
static struct {
	int		size;
	int		notify;
} aac_qinfo[] = {
	{AAC_HOST_NORM_CMD_ENTRIES, AAC_DB_COMMAND_NOT_FULL},
	{AAC_HOST_HIGH_CMD_ENTRIES, 0},
	{AAC_ADAP_NORM_CMD_ENTRIES, AAC_DB_COMMAND_READY},
	{AAC_ADAP_HIGH_CMD_ENTRIES, 0},
	{AAC_HOST_NORM_RESP_ENTRIES, AAC_DB_RESPONSE_NOT_FULL},
	{AAC_HOST_HIGH_RESP_ENTRIES, 0},
	{AAC_ADAP_NORM_RESP_ENTRIES, AAC_DB_RESPONSE_READY},
	{AAC_ADAP_HIGH_RESP_ENTRIES, 0}
};

/*
 * Atomically insert an entry into the nominated queue, returns 0 on success or
 * EBUSY if the queue is full.
 *
 * Note: it would be more efficient to defer notifying the controller in
 *	 the case where we may be inserting several entries in rapid succession,
 *	 but implementing this usefully may be difficult (it would involve a
 *	 separate queue/notify interface).
 */
static int
aac_enqueue_fib(struct aac_softc *sc, int queue, struct aac_command *cm)
{
	u_int32_t pi, ci;
	int s, error;
	u_int32_t fib_size;
	u_int32_t fib_addr;

	debug_called(3);

	fib_size = cm->cm_fib->Header.Size; 
	fib_addr = cm->cm_fib->Header.ReceiverFibAddress;

	s = splbio();

	/* get the producer/consumer indices */
	pi = sc->aac_queues->qt_qindex[queue][AAC_PRODUCER_INDEX];
	ci = sc->aac_queues->qt_qindex[queue][AAC_CONSUMER_INDEX];

	/* wrap the queue? */
	if (pi >= aac_qinfo[queue].size)
		pi = 0;

	/* check for queue full */
	if ((pi + 1) == ci) {
		error = EBUSY;
		goto out;
	}

	/* populate queue entry */
	(sc->aac_qentries[queue] + pi)->aq_fib_size = fib_size;
	(sc->aac_qentries[queue] + pi)->aq_fib_addr = fib_addr;

	/* update producer index */
	sc->aac_queues->qt_qindex[queue][AAC_PRODUCER_INDEX] = pi + 1;

	/*
	 * To avoid a race with its completion interrupt, place this command on
	 * the busy queue prior to advertising it to the controller.
	 */
	aac_enqueue_busy(cm);

	/* notify the adapter if we know how */
	if (aac_qinfo[queue].notify != 0)
		AAC_QNOTIFY(sc, aac_qinfo[queue].notify);

	error = 0;

out:
	splx(s);
	return(error);
}

/*
 * Atomically remove one entry from the nominated queue, returns 0 on
 * success or ENOENT if the queue is empty.
 */
static int
aac_dequeue_fib(struct aac_softc *sc, int queue, u_int32_t *fib_size,
		struct aac_fib **fib_addr)
{
	u_int32_t pi, ci;
	int s, error;
	int notify;

	debug_called(3);

	s = splbio();

	/* get the producer/consumer indices */
	pi = sc->aac_queues->qt_qindex[queue][AAC_PRODUCER_INDEX];
	ci = sc->aac_queues->qt_qindex[queue][AAC_CONSUMER_INDEX];

	/* check for queue empty */
	if (ci == pi) {
		error = ENOENT;
		goto out;
	}
	
	notify = 0;
	if (ci == pi + 1)
		notify++;

	/* wrap the queue? */
	if (ci >= aac_qinfo[queue].size)
		ci = 0;

	/* fetch the entry */
	*fib_size = (sc->aac_qentries[queue] + ci)->aq_fib_size;
	*fib_addr = (struct aac_fib *)(sc->aac_qentries[queue] +
				       ci)->aq_fib_addr;

	/* update consumer index */
	sc->aac_queues->qt_qindex[queue][AAC_CONSUMER_INDEX] = ci + 1;

	/* if we have made the queue un-full, notify the adapter */
	if (notify && (aac_qinfo[queue].notify != 0))
		AAC_QNOTIFY(sc, aac_qinfo[queue].notify);
	error = 0;

out:
	splx(s);
	return(error);
}

/*
 * Put our response to an Adapter Initialed Fib on the response queue
 */
static int
aac_enqueue_response(struct aac_softc *sc, int queue, struct aac_fib *fib)
{
	u_int32_t pi, ci;
	int s, error;
	u_int32_t fib_size;
	u_int32_t fib_addr;

	debug_called(1);

	/* Tell the adapter where the FIB is */
	fib_size = fib->Header.Size; 
	fib_addr = fib->Header.SenderFibAddress;
	fib->Header.ReceiverFibAddress = fib_addr;

	s = splbio();

	/* get the producer/consumer indices */
	pi = sc->aac_queues->qt_qindex[queue][AAC_PRODUCER_INDEX];
	ci = sc->aac_queues->qt_qindex[queue][AAC_CONSUMER_INDEX];

	/* wrap the queue? */
	if (pi >= aac_qinfo[queue].size)
		pi = 0;

	/* check for queue full */
	if ((pi + 1) == ci) {
		error = EBUSY;
		goto out;
	}

	/* populate queue entry */
	(sc->aac_qentries[queue] + pi)->aq_fib_size = fib_size;
	(sc->aac_qentries[queue] + pi)->aq_fib_addr = fib_addr;

	/* update producer index */
	sc->aac_queues->qt_qindex[queue][AAC_PRODUCER_INDEX] = pi + 1;

	/* notify the adapter if we know how */
	if (aac_qinfo[queue].notify != 0)
		AAC_QNOTIFY(sc, aac_qinfo[queue].notify);

	error = 0;

out:
	splx(s);
	return(error);
}

/*
 * Check for commands that have been outstanding for a suspiciously long time,
 * and complain about them.
 */
static void
aac_timeout(struct aac_softc *sc)
{
	int s;
	struct aac_command *cm;
	time_t deadline;

#if 0
	/* simulate an interrupt to handle possibly-missed interrupts */
	/*
	 * XXX This was done to work around another bug which has since been
	 * fixed.  It is dangerous anyways because you don't want multiple
	 * threads in the interrupt handler at the same time!  If calling
	 * is deamed neccesary in the future, proper mutexes must be used.
	 */
	s = splbio();
	aac_intr(sc);
	splx(s);

	/* kick the I/O queue to restart it in the case of deadlock */
	aac_startio(sc);
#endif

	/*
	 * traverse the busy command list, bitch about late commands once
	 * only.
	 */
	deadline = time_second - AAC_CMD_TIMEOUT;
	s = splbio();
	TAILQ_FOREACH(cm, &sc->aac_busy, cm_link) {
		if ((cm->cm_timestamp  < deadline)
			/* && !(cm->cm_flags & AAC_CMD_TIMEDOUT) */) {
			cm->cm_flags |= AAC_CMD_TIMEDOUT;
			device_printf(sc->aac_dev,
				      "COMMAND %p TIMEOUT AFTER %d SECONDS\n",
				      cm, (int)(time_second-cm->cm_timestamp));
			AAC_PRINT_FIB(sc, cm->cm_fib);
		}
	}
	splx(s);

	/* reset the timer for next time */
	timeout((timeout_t*)aac_timeout, sc, AAC_PERIODIC_INTERVAL * hz);
	return;
}

/*
 * Interface Function Vectors
 */

/*
 * Read the current firmware status word.
 */
static int
aac_sa_get_fwstatus(struct aac_softc *sc)
{
	debug_called(3);

	return(AAC_GETREG4(sc, AAC_SA_FWSTATUS));
}

static int
aac_rx_get_fwstatus(struct aac_softc *sc)
{
	debug_called(3);

	return(AAC_GETREG4(sc, AAC_RX_FWSTATUS));
}

/*
 * Notify the controller of a change in a given queue
 */

static void
aac_sa_qnotify(struct aac_softc *sc, int qbit)
{
	debug_called(3);

	AAC_SETREG2(sc, AAC_SA_DOORBELL1_SET, qbit);
}

static void
aac_rx_qnotify(struct aac_softc *sc, int qbit)
{
	debug_called(3);

	AAC_SETREG4(sc, AAC_RX_IDBR, qbit);
}

/*
 * Get the interrupt reason bits
 */
static int
aac_sa_get_istatus(struct aac_softc *sc)
{
	debug_called(3);

	return(AAC_GETREG2(sc, AAC_SA_DOORBELL0));
}

static int
aac_rx_get_istatus(struct aac_softc *sc)
{
	debug_called(3);

	return(AAC_GETREG4(sc, AAC_RX_ODBR));
}

/*
 * Clear some interrupt reason bits
 */
static void
aac_sa_clear_istatus(struct aac_softc *sc, int mask)
{
	debug_called(3);

	AAC_SETREG2(sc, AAC_SA_DOORBELL0_CLEAR, mask);
}

static void
aac_rx_clear_istatus(struct aac_softc *sc, int mask)
{
	debug_called(3);

	AAC_SETREG4(sc, AAC_RX_ODBR, mask);
}

/*
 * Populate the mailbox and set the command word
 */
static void
aac_sa_set_mailbox(struct aac_softc *sc, u_int32_t command,
		u_int32_t arg0, u_int32_t arg1, u_int32_t arg2, u_int32_t arg3)
{
	debug_called(4);

	AAC_SETREG4(sc, AAC_SA_MAILBOX, command);
	AAC_SETREG4(sc, AAC_SA_MAILBOX + 4, arg0);
	AAC_SETREG4(sc, AAC_SA_MAILBOX + 8, arg1);
	AAC_SETREG4(sc, AAC_SA_MAILBOX + 12, arg2);
	AAC_SETREG4(sc, AAC_SA_MAILBOX + 16, arg3);
}

static void
aac_rx_set_mailbox(struct aac_softc *sc, u_int32_t command,
		u_int32_t arg0, u_int32_t arg1, u_int32_t arg2, u_int32_t arg3)
{
	debug_called(4);

	AAC_SETREG4(sc, AAC_RX_MAILBOX, command);
	AAC_SETREG4(sc, AAC_RX_MAILBOX + 4, arg0);
	AAC_SETREG4(sc, AAC_RX_MAILBOX + 8, arg1);
	AAC_SETREG4(sc, AAC_RX_MAILBOX + 12, arg2);
	AAC_SETREG4(sc, AAC_RX_MAILBOX + 16, arg3);
}

/*
 * Fetch the immediate command status word
 */
static int
aac_sa_get_mailboxstatus(struct aac_softc *sc)
{
	debug_called(4);

	return(AAC_GETREG4(sc, AAC_SA_MAILBOX));
}

static int
aac_rx_get_mailboxstatus(struct aac_softc *sc)
{
	debug_called(4);

	return(AAC_GETREG4(sc, AAC_RX_MAILBOX));
}

/*
 * Set/clear interrupt masks
 */
static void
aac_sa_set_interrupts(struct aac_softc *sc, int enable)
{
	debug(2, "%sable interrupts", enable ? "en" : "dis");

	if (enable) {
		AAC_SETREG2((sc), AAC_SA_MASK0_CLEAR, AAC_DB_INTERRUPTS);
	} else {
		AAC_SETREG2((sc), AAC_SA_MASK0_SET, ~0);
	}
}

static void
aac_rx_set_interrupts(struct aac_softc *sc, int enable)
{
	debug(2, "%sable interrupts", enable ? "en" : "dis");

	if (enable) {
		AAC_SETREG4(sc, AAC_RX_OIMR, ~AAC_DB_INTERRUPTS);
	} else {
		AAC_SETREG4(sc, AAC_RX_OIMR, ~0);
	}
}

/*
 * Debugging and Diagnostics
 */

/*
 * Print some information about the controller.
 */
static void
aac_describe_controller(struct aac_softc *sc)
{
	u_int8_t buf[AAC_FIB_DATASIZE];	/* XXX really a bit big
					 * for the stack */
	u_int16_t bufsize;
	struct aac_adapter_info	*info;
	u_int8_t arg;

	debug_called(2);

	arg = 0;
	bufsize = sizeof(buf);
	if (aac_sync_fib(sc, RequestAdapterInfo, 0, &arg, sizeof(arg), &buf,
			 &bufsize)) {
		device_printf(sc->aac_dev, "RequestAdapterInfo failed\n");
		return;
	}
	if (bufsize != sizeof(*info)) {
		device_printf(sc->aac_dev,
			      "RequestAdapterInfo returned wrong data size "
			      "(%d != %d)\n", bufsize, sizeof(*info));
		/*return;*/
	}
	info = (struct aac_adapter_info *)&buf[0];

	device_printf(sc->aac_dev, "%s %dMHz, %dMB cache memory, %s\n", 
		      aac_describe_code(aac_cpu_variant, info->CpuVariant),
		      info->ClockSpeed, info->BufferMem / (1024 * 1024), 
		      aac_describe_code(aac_battery_platform,
					info->batteryPlatform));

	/* save the kernel revision structure for later use */
	sc->aac_revision = info->KernelRevision;
	device_printf(sc->aac_dev, "Kernel %d.%d-%d, Build %d, S/N %6X\n",
		      info->KernelRevision.external.comp.major,
		      info->KernelRevision.external.comp.minor,
		      info->KernelRevision.external.comp.dash,
		      info->KernelRevision.buildNumber,
		      (u_int32_t)(info->SerialNumber & 0xffffff));
}

/*
 * Look up a text description of a numeric error code and return a pointer to
 * same.
 */
static char *
aac_describe_code(struct aac_code_lookup *table, u_int32_t code)
{
	int i;

	for (i = 0; table[i].string != NULL; i++)
		if (table[i].code == code)
			return(table[i].string);
	return(table[i + 1].string);
}

/*
 * Management Interface
 */

static int
aac_open(dev_t dev, int flags, int fmt, struct thread *td)
{
	struct aac_softc *sc;

	debug_called(2);

	sc = dev->si_drv1;

	/* Check to make sure the device isn't already open */
	if (sc->aac_state & AAC_STATE_OPEN) {
		return EBUSY;
	}
	sc->aac_state |= AAC_STATE_OPEN;

	return 0;
}

static int
aac_close(dev_t dev, int flags, int fmt, struct thread *td)
{
	struct aac_softc *sc;

	debug_called(2);

	sc = dev->si_drv1;

	/* Mark this unit as no longer open  */
	sc->aac_state &= ~AAC_STATE_OPEN;

	return 0;
}

static int
aac_ioctl(dev_t dev, u_long cmd, caddr_t arg, int flag, struct thread *td)
{
	union aac_statrequest *as;
	struct aac_softc *sc;
	int error = 0;
	int i;

	debug_called(2);

	as = (union aac_statrequest *)arg;
	sc = dev->si_drv1;

	switch (cmd) {
	case AACIO_STATS:
		switch (as->as_item) {
		case AACQ_FREE:
		case AACQ_BIO:
		case AACQ_READY:
		case AACQ_BUSY:
		case AACQ_COMPLETE:
			bcopy(&sc->aac_qstat[as->as_item], &as->as_qstat,
			      sizeof(struct aac_qstat));
			break;
		default:
			error = ENOENT;
			break;
		}
	break;
	
	case FSACTL_SENDFIB:
		arg = *(caddr_t*)arg;
	case FSACTL_LNX_SENDFIB:
		debug(1, "FSACTL_SENDFIB");
		error = aac_ioctl_sendfib(sc, arg);
		break;
	case FSACTL_AIF_THREAD:
	case FSACTL_LNX_AIF_THREAD:
		debug(1, "FSACTL_AIF_THREAD");
		error = EINVAL;
		break;
	case FSACTL_OPEN_GET_ADAPTER_FIB:
		arg = *(caddr_t*)arg;
		case FSACTL_LNX_OPEN_GET_ADAPTER_FIB:
		debug(1, "FSACTL_OPEN_GET_ADAPTER_FIB");
		/*
		 * Pass the caller out an AdapterFibContext.
		 *
		 * Note that because we only support one opener, we
		 * basically ignore this.  Set the caller's context to a magic
		 * number just in case.
		 *
		 * The Linux code hands the driver a pointer into kernel space,
		 * and then trusts it when the caller hands it back.  Aiee!
		 * Here, we give it the proc pointer of the per-adapter aif 
		 * thread. It's only used as a sanity check in other calls.
		 */
		i = (int)sc->aifthread;
		error = copyout(&i, arg, sizeof(i));
		break;
	case FSACTL_GET_NEXT_ADAPTER_FIB:
		arg = *(caddr_t*)arg;
	case FSACTL_LNX_GET_NEXT_ADAPTER_FIB:
		debug(1, "FSACTL_GET_NEXT_ADAPTER_FIB");
		error = aac_getnext_aif(sc, arg);
		break;
	case FSACTL_CLOSE_GET_ADAPTER_FIB:
	case FSACTL_LNX_CLOSE_GET_ADAPTER_FIB:
		debug(1, "FSACTL_CLOSE_GET_ADAPTER_FIB");
		/* don't do anything here */
		break;
	case FSACTL_MINIPORT_REV_CHECK:
		arg = *(caddr_t*)arg;
	case FSACTL_LNX_MINIPORT_REV_CHECK:
		debug(1, "FSACTL_MINIPORT_REV_CHECK");
		error = aac_rev_check(sc, arg);
		break;
	case FSACTL_QUERY_DISK:
		arg = *(caddr_t*)arg;
	case FSACTL_LNX_QUERY_DISK:
		debug(1, "FSACTL_QUERY_DISK");
		error = aac_query_disk(sc, arg);
			break;
	case FSACTL_DELETE_DISK:
	case FSACTL_LNX_DELETE_DISK:
		/*
		 * We don't trust the underland to tell us when to delete a
		 * container, rather we rely on an AIF coming from the 
		 * controller
		 */
		error = 0;
		break;
	default:
		device_printf(sc->aac_dev, "unsupported cmd 0x%lx\n", cmd);
		error = EINVAL;
		break;
	}
	return(error);
}

/*
 * Send a FIB supplied from userspace
 */
static int
aac_ioctl_sendfib(struct aac_softc *sc, caddr_t ufib)
{
	struct aac_command *cm;
	int size, error;

	debug_called(2);

	cm = NULL;

	/*
	 * Get a command
	 */
	if (aac_alloc_command(sc, &cm)) {
		error = EBUSY;
		goto out;
	}

	/*
	 * Fetch the FIB header, then re-copy to get data as well.
	 */
	if ((error = copyin(ufib, cm->cm_fib,
			    sizeof(struct aac_fib_header))) != 0)
		goto out;
	size = cm->cm_fib->Header.Size + sizeof(struct aac_fib_header);
	if (size > sizeof(struct aac_fib)) {
		device_printf(sc->aac_dev, "incoming FIB oversized (%d > %d)\n",
			      size, sizeof(struct aac_fib));
		size = sizeof(struct aac_fib);
	}
	if ((error = copyin(ufib, cm->cm_fib, size)) != 0)
		goto out;
	cm->cm_fib->Header.Size = size;
	cm->cm_timestamp = time_second;

	/*
	 * Pass the FIB to the controller, wait for it to complete.
	 */
	if ((error = aac_wait_command(cm, 30)) != 0)	/* XXX user timeout? */
		goto out;

	/*
	 * Copy the FIB and data back out to the caller.
	 */
	size = cm->cm_fib->Header.Size;
	if (size > sizeof(struct aac_fib)) {
		device_printf(sc->aac_dev, "outbound FIB oversized (%d > %d)\n",
			      size, sizeof(struct aac_fib));
		size = sizeof(struct aac_fib);
	}
	error = copyout(cm->cm_fib, ufib, size);

out:
	if (cm != NULL) {
		aac_release_command(cm);
	}
	return(error);
}

/*
 * Handle an AIF sent to us by the controller; queue it for later reference.
 * If the queue fills up, then drop the older entries.
 */
static void
aac_handle_aif(struct aac_softc *sc, struct aac_fib *fib)
{
	struct aac_aif_command *aif;
	struct aac_container *co, *co_next;
	struct aac_mntinfo mi;
	struct aac_mntinforesponse mir;
	u_int16_t rsize;
	int next, s, found;
	int added = 0, i = 0;

	debug_called(2);

	aif = (struct aac_aif_command*)&fib->data[0];
	aac_print_aif(sc, aif);

	/* Is it an event that we should care about? */
	switch (aif->command) {
	case AifCmdEventNotify:
		switch (aif->data.EN.type) {
		case AifEnAddContainer:
		case AifEnDeleteContainer:
			/*
			 * A container was added or deleted, but the message 
			 * doesn't tell us anything else!  Re-enumerate the
			 * containers and sort things out.
			 */
			mi.Command = VM_NameServe;
			mi.MntType = FT_FILESYS;
			do {
				/*
				 * Ask the controller for its containers one at
				 * a time.
				 * XXX What if the controller's list changes
				 * midway through this enumaration?
				 * XXX This should be done async.
				 */
				mi.MntCount = i;
				rsize = sizeof(mir);
				if (aac_sync_fib(sc, ContainerCommand, 0, &mi,
						 sizeof(mi), &mir, &rsize)) {
					debug(2, "Error probing container %d\n",
					      i);
					continue;
				}
				if (rsize != sizeof(mir)) {
					debug(2, "Container response size too "
						 "large\n");
					continue;
				}
				/*
				 * Check the container against our list.
				 * co->co_found was already set to 0 in a
				 * previous run.
				 */
				if ((mir.Status == ST_OK) &&
				    (mir.MntTable[0].VolType != CT_NONE)) {
					found = 0;
					TAILQ_FOREACH(co,
						      &sc->aac_container_tqh, 
						      co_link) {
						if (co->co_mntobj.ObjectId ==
						    mir.MntTable[0].ObjectId) {
							co->co_found = 1;
							found = 1;
							break;
						}
					}
					/*
					 * If the container matched, continue
					 * in the list.
					 */
					if (found) {
						i++;
						continue;
					}

					/*
					 * This is a new container.  Do all the
					 * appropriate things to set it up.						 */
					aac_add_container(sc, &mir, 1);
					added = 1;
				}
				i++;
			} while ((i < mir.MntRespCount) &&
				 (i < AAC_MAX_CONTAINERS));

			/*
			 * Go through our list of containers and see which ones
			 * were not marked 'found'.  Since the controller didn't
			 * list them they must have been deleted.  Do the
			 * appropriate steps to destroy the device.  Also reset
			 * the co->co_found field.
			 */
			co = TAILQ_FIRST(&sc->aac_container_tqh);
			while (co != NULL) {
				if (co->co_found == 0) {
					device_delete_child(sc->aac_dev,
							    co->co_disk);
					co_next = TAILQ_NEXT(co, co_link);
					AAC_LOCK_AQUIRE(&sc->
							aac_container_lock);
					TAILQ_REMOVE(&sc->aac_container_tqh, co,
						     co_link);
					AAC_LOCK_RELEASE(&sc->
							 aac_container_lock);
					FREE(co, M_AACBUF);
					co = co_next;
				} else {
					co->co_found = 0;
					co = TAILQ_NEXT(co, co_link);
				}
			}

			/* Attach the newly created containers */
			if (added)
				bus_generic_attach(sc->aac_dev);
	
				break;

		default:
			break;
		}

	default:
		break;
	}

	/* Copy the AIF data to the AIF queue for ioctl retrieval */
	s = splbio();
	next = (sc->aac_aifq_head + 1) % AAC_AIFQ_LENGTH;
	if (next != sc->aac_aifq_tail) {
		bcopy(aif, &sc->aac_aifq[next], sizeof(struct aac_aif_command));
	sc->aac_aifq_head = next;
	if (sc->aac_state & AAC_STATE_AIF_SLEEPER)
		wakeup(sc->aac_aifq);
	}
	splx(s);

	return;
}

/*
 * Linux Management Interface
 * This is soon to be removed!
 */

#ifdef AAC_COMPAT_LINUX

#include <sys/proc.h>
#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#include <compat/linux/linux_ioctl.h>

/* There are multiple ioctl number ranges that need to be handled */
#define AAC_LINUX_IOCTL_MIN  0x0000
#define AAC_LINUX_IOCTL_MAX  0x21ff

static linux_ioctl_function_t aac_linux_ioctl;
static struct linux_ioctl_handler aac_handler = {aac_linux_ioctl,
						 AAC_LINUX_IOCTL_MIN,
						 AAC_LINUX_IOCTL_MAX};

SYSINIT  (aac_register,   SI_SUB_KLD, SI_ORDER_MIDDLE,
	  linux_ioctl_register_handler, &aac_handler);
SYSUNINIT(aac_unregister, SI_SUB_KLD, SI_ORDER_MIDDLE,
	  linux_ioctl_unregister_handler, &aac_handler);

MODULE_DEPEND(aac, linux, 1, 1, 1);

static int
aac_linux_ioctl(struct thread *td, struct linux_ioctl_args *args)
{
	struct file *fp;
	u_long cmd;

	debug_called(2);

	fp = td->td_proc->p_fd->fd_ofiles[args->fd];
	cmd = args->cmd;

	/*
	 * Pass the ioctl off to our standard handler.
	 */
	return(fo_ioctl(fp, cmd, (caddr_t)args->arg, td));
}

#endif

/*
 * Return the Revision of the driver to userspace and check to see if the
 * userspace app is possibly compatible.  This is extremely bogus since
 * our driver doesn't follow Adaptec's versioning system.  Cheat by just
 * returning what the card reported.
 */
static int
aac_rev_check(struct aac_softc *sc, caddr_t udata)
{
	struct aac_rev_check rev_check;
	struct aac_rev_check_resp rev_check_resp;
	int error = 0;

	debug_called(2);

	/*
	 * Copyin the revision struct from userspace
	 */
	if ((error = copyin(udata, (caddr_t)&rev_check,
			sizeof(struct aac_rev_check))) != 0) {
		return error;
	}

	debug(2, "Userland revision= %d\n",
	      rev_check.callingRevision.buildNumber);

	/*
	 * Doctor up the response struct.
	 */
	rev_check_resp.possiblyCompatible = 1;
	rev_check_resp.adapterSWRevision.external.ul =
	    sc->aac_revision.external.ul;
	rev_check_resp.adapterSWRevision.buildNumber =
	    sc->aac_revision.buildNumber;

	return(copyout((caddr_t)&rev_check_resp, udata,
			sizeof(struct aac_rev_check_resp)));
}

/*
 * Pass the caller the next AIF in their queue
 */
static int
aac_getnext_aif(struct aac_softc *sc, caddr_t arg)
{
	struct get_adapter_fib_ioctl agf;
	int error, s;

	debug_called(2);

	if ((error = copyin(arg, &agf, sizeof(agf))) == 0) {

		/*
		 * Check the magic number that we gave the caller.
		 */
		if (agf.AdapterFibContext != (int)sc->aifthread) {
			error = EFAULT;
		} else {
	
			s = splbio();
			error = aac_return_aif(sc, agf.AifFib);
	
			if ((error == EAGAIN) && (agf.Wait)) {
				sc->aac_state |= AAC_STATE_AIF_SLEEPER;
				while (error == EAGAIN) {
					error = tsleep(sc->aac_aifq, PRIBIO |
						       PCATCH, "aacaif", 0);
					if (error == 0)
						error = aac_return_aif(sc,
						    agf.AifFib);
				}
				sc->aac_state &= ~AAC_STATE_AIF_SLEEPER;
			}
		splx(s);
		}
	}
	return(error);
}

/*
 * Hand the next AIF off the top of the queue out to userspace.
 */
static int
aac_return_aif(struct aac_softc *sc, caddr_t uptr)
{
	int error, s;

	debug_called(2);

	s = splbio();
	if (sc->aac_aifq_tail == sc->aac_aifq_head) {
		error = EAGAIN;
	} else {
		error = copyout(&sc->aac_aifq[sc->aac_aifq_tail], uptr,
				sizeof(struct aac_aif_command));
		if (error)
			printf("aac_return_aif: copyout returned %d\n", error);
		if (!error)
			sc->aac_aifq_tail = (sc->aac_aifq_tail + 1) %
					    AAC_AIFQ_LENGTH;
	}
	splx(s);
	return(error);
}

/*
 * Give the userland some information about the container.  The AAC arch
 * expects the driver to be a SCSI passthrough type driver, so it expects
 * the containers to have b:t:l numbers.  Fake it.
 */
static int
aac_query_disk(struct aac_softc *sc, caddr_t uptr)
{
	struct aac_query_disk query_disk;
	struct aac_container *co;
	struct aac_disk	*disk;
	int error, id;

	debug_called(2);

	disk = NULL;

	error = copyin(uptr, (caddr_t)&query_disk,
		       sizeof(struct aac_query_disk));
	if (error)
		return (error);

	id = query_disk.ContainerNumber;
	if (id == -1)
		return (EINVAL);

	AAC_LOCK_AQUIRE(&sc->aac_container_lock);
	TAILQ_FOREACH(co, &sc->aac_container_tqh, co_link) {
		if (co->co_mntobj.ObjectId == id)
			break;
		}

		if (co == NULL) {
			query_disk.Valid = 0;
			query_disk.Locked = 0;
			query_disk.Deleted = 1;		/* XXX is this right? */
		} else {
			disk = device_get_softc(co->co_disk);
			query_disk.Valid = 1;
			query_disk.Locked =
			    (disk->ad_flags & AAC_DISK_OPEN) ? 1 : 0;
			query_disk.Deleted = 0;
			query_disk.Bus = 0;
			query_disk.Target = disk->unit;
			query_disk.Lun = 0;
			query_disk.UnMapped = 0;
			bcopy(disk->ad_dev_t->si_name,
			      &query_disk.diskDeviceName[0], 10);
		}
	AAC_LOCK_RELEASE(&sc->aac_container_lock);

	error = copyout((caddr_t)&query_disk, uptr,
			sizeof(struct aac_query_disk));

	return (error);
}

