/*
 *  hosts.c Copyright (C) 1992 Drew Eckhardt
 *          Copyright (C) 1993, 1994, 1995 Eric Youngdale
 *
 *  mid to lowlevel SCSI driver interface
 *      Initial versions: Drew Eckhardt
 *      Subsequent revisions: Eric Youngdale
 *
 *  <drew@colorado.edu>
 *
 *  Jiffies wrap fixes (host->resetting), 3 Dec 1998 Andrea Arcangeli
 *  Added QLOGIC QLA1280 SCSI controller kernel host support. 
 *     August 4, 1999 Fred Lewis, Intel DuPont
 *
 *  Updated to reflect the new initialization scheme for the higher 
 *  level of scsi drivers (sd/sr/st)
 *  September 17, 2000 Torben Mathiasen <tmm@image.dk>
 */


/*
 *  This file contains the medium level SCSI
 *  host interface initialization, as well as the scsi_hosts array of SCSI
 *  hosts currently present in the system.
 */

#define __NO_VERSION__
#include <linux/module.h>
#include <linux/blk.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/proc_fs.h>
#include <linux/init.h>

#define __KERNEL_SYSCALLS__

#include <linux/unistd.h>

#include "scsi.h"
#include "hosts.h"

/*
static const char RCSid[] = "$Header: /vger/u4/cvs/linux/drivers/scsi/hosts.c,v 1.20 1996/12/12 19:18:32 davem Exp $";
*/

/*
 *  The scsi host entries should be in the order you wish the
 *  cards to be detected.  A driver may appear more than once IFF
 *  it can deal with being detected (and therefore initialized)
 *  with more than one simultaneous host number, can handle being
 *  reentrant, etc.
 *
 *  They may appear in any order, as each SCSI host is told which host 
 *  number it is during detection.
 */

/* This is a placeholder for controllers that are not configured into
 * the system - we do this to ensure that the controller numbering is
 * always consistent, no matter how the kernel is configured. */

#define NO_CONTROLLER {NULL, NULL, NULL, NULL, NULL, NULL, NULL, \
			   NULL, NULL, 0, 0, 0, 0, 0, 0}

/*
 *  When figure is run, we don't want to link to any object code.  Since
 *  the macro for each host will contain function pointers, we cannot
 *  use it and instead must use a "blank" that does no such
 *  idiocy.
 */

Scsi_Host_Template * scsi_hosts;


/*
 *  Our semaphores and timeout counters, where size depends on 
 *      MAX_SCSI_HOSTS here.
 */

Scsi_Host_Name * scsi_host_no_list;
struct Scsi_Host * scsi_hostlist;
struct Scsi_Device_Template * scsi_devicelist;

int max_scsi_hosts;	/* host_no for next new host */
int next_scsi_host;	/* count of registered scsi hosts */

void
scsi_unregister(struct Scsi_Host * sh){
    struct Scsi_Host * shpnt;
    Scsi_Host_Name *shn;
        
    if(scsi_hostlist == sh)
	scsi_hostlist = sh->next;
    else {
	shpnt = scsi_hostlist;
	while(shpnt->next != sh) shpnt = shpnt->next;
	shpnt->next = shpnt->next->next;
    }

    /*
     * We have to unregister the host from the scsi_host_no_list as well.
     * Decide by the host_no not by the name because most host drivers are
     * able to handle more than one adapters from the same kind (or family).
     */
    for ( shn=scsi_host_no_list; shn && (sh->host_no != shn->host_no);
	  shn=shn->next);
    if (shn) shn->host_registered = 0;
    /* else {} : This should not happen, we should panic here... */
    
    next_scsi_host--;

    kfree((char *) sh);
}

/* We call this when we come across a new host adapter. We only do this
 * once we are 100% sure that we want to use this host adapter -  it is a
 * pain to reverse this, so we try to avoid it 
 */
extern int blk_nohighio;
struct Scsi_Host * scsi_register(Scsi_Host_Template * tpnt, int j){
    struct Scsi_Host * retval, *shpnt, *o_shp;
    Scsi_Host_Name *shn, *shn2;
    int flag_new = 1;
    const char * hname;
    size_t hname_len;
    retval = (struct Scsi_Host *)kmalloc(sizeof(struct Scsi_Host) + j,
					 (tpnt->unchecked_isa_dma && j ? 
					  GFP_DMA : 0) | GFP_ATOMIC);
    if(retval == NULL)
    {
        printk("scsi: out of memory in scsi_register.\n");
    	return NULL;
    }
    	
    memset(retval, 0, sizeof(struct Scsi_Host) + j);

    /* trying to find a reserved entry (host_no) */
    hname = (tpnt->proc_name) ?  tpnt->proc_name : "";
    hname_len = strlen(hname);
    for (shn = scsi_host_no_list;shn;shn = shn->next) {
	if (!(shn->host_registered) && 
	    (hname_len > 0) && (0 == strncmp(hname, shn->name, hname_len))) {
	    flag_new = 0;
	    retval->host_no = shn->host_no;
	    shn->host_registered = 1;
	    shn->loaded_as_module = 1;
	    break;
	}
    }
    atomic_set(&retval->host_active,0);
    retval->host_busy = 0;
    retval->host_failed = 0;
    if(j > 0xffff) panic("Too many extra bytes requested\n");
    retval->extra_bytes = j;
    retval->loaded_as_module = 1;
    if (flag_new) {
	shn = (Scsi_Host_Name *) kmalloc(sizeof(Scsi_Host_Name), GFP_ATOMIC);
        if (!shn) {
                kfree(retval);
                printk(KERN_ERR "scsi: out of memory(2) in scsi_register.\n");
                return NULL;
        }
	shn->name = kmalloc(hname_len + 1, GFP_ATOMIC);
	if (hname_len > 0)
	    strncpy(shn->name, hname, hname_len);
	shn->name[hname_len] = 0;
	shn->host_no = max_scsi_hosts++;
	shn->host_registered = 1;
	shn->loaded_as_module = 1;
	shn->next = NULL;
	if (scsi_host_no_list) {
	    for (shn2 = scsi_host_no_list;shn2->next;shn2 = shn2->next)
		;
	    shn2->next = shn;
	}
	else
	    scsi_host_no_list = shn;
	retval->host_no = shn->host_no;
    }
    next_scsi_host++;
    retval->host_queue = NULL;
    init_waitqueue_head(&retval->host_wait);
    retval->resetting = 0;
    retval->last_reset = 0;
    retval->irq = 0;
    retval->dma_channel = 0xff;

    /* These three are default values which can be overridden */
    retval->max_channel = 0; 
    retval->max_id = 8;      
    retval->max_lun = 8;

    /*
     * All drivers right now should be able to handle 12 byte commands.
     * Every so often there are requests for 16 byte commands, but individual
     * low-level drivers need to certify that they actually do something
     * sensible with such commands.
     */
    retval->max_cmd_len = 12;

    retval->unique_id = 0;
    retval->io_port = 0;
    retval->hostt = tpnt;
    retval->next = NULL;
    retval->in_recovery = 0;
    retval->ehandler = NULL;    /* Initial value until the thing starts up. */
    retval->eh_notify   = NULL;    /* Who we notify when we exit. */


    retval->host_blocked = FALSE;
    retval->host_self_blocked = FALSE;

#ifdef DEBUG
    printk("Register %x %x: %d\n", (int)retval, (int)retval->hostt, j);
#endif

    /* The next six are the default values which can be overridden
     * if need be */
    retval->this_id = tpnt->this_id;
    retval->can_queue = tpnt->can_queue;
    retval->sg_tablesize = tpnt->sg_tablesize;
    retval->cmd_per_lun = tpnt->cmd_per_lun;
    retval->unchecked_isa_dma = tpnt->unchecked_isa_dma;
    retval->use_clustering = tpnt->use_clustering;   
    if (!blk_nohighio)
	retval->highmem_io = tpnt->highmem_io;

    retval->select_queue_depths = tpnt->select_queue_depths;
    retval->max_sectors = tpnt->max_sectors;

    if(!scsi_hostlist)
	scsi_hostlist = retval;
    else {
	shpnt = scsi_hostlist;
	if (retval->host_no < shpnt->host_no) {
	    retval->next = shpnt;
	    wmb(); /* want all to see these writes in this order */
	    scsi_hostlist = retval;
	}
	else {
	    for (o_shp = shpnt, shpnt = shpnt->next; shpnt; 
		 o_shp = shpnt, shpnt = shpnt->next) {
		if (retval->host_no < shpnt->host_no) {
		    retval->next = shpnt;
		    wmb();
		    o_shp->next = retval;
		    break;
		}
	    }
	    if (! shpnt)
		o_shp->next = retval;
        }
    }
    
    return retval;
}

int
scsi_register_device(struct Scsi_Device_Template * sdpnt)
{
    if(sdpnt->next) panic("Device already registered");
    sdpnt->next = scsi_devicelist;
    scsi_devicelist = sdpnt;
    return 0;
}

void
scsi_deregister_device(struct Scsi_Device_Template * tpnt)
{
    struct Scsi_Device_Template *spnt;
    struct Scsi_Device_Template *prev_spnt;

    spnt = scsi_devicelist;
    prev_spnt = NULL;
    while (spnt != tpnt) {
	prev_spnt = spnt;
	spnt = spnt->next;
    }
    if (prev_spnt == NULL)
        scsi_devicelist = tpnt->next;
    else
        prev_spnt->next = spnt->next;
}

/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
 * Emacs will notice this stuff at the end of the file and automatically
 * adjust the settings for this buffer only.  This must remain at the end
 * of the file.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-indent-level: 4
 * c-brace-imaginary-offset: 0
 * c-brace-offset: -4
 * c-argdecl-indent: 4
 * c-label-offset: -4
 * c-continued-statement-offset: 4
 * c-continued-brace-offset: 0
 * indent-tabs-mode: nil
 * tab-width: 8
 * End:
 */
