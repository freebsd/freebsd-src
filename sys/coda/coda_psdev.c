/*
 *             Coda: an Experimental Distributed File System
 *                              Release 3.1
 * 
 *           Copyright (c) 1987-1998 Carnegie Mellon University
 *                          All Rights Reserved
 * 
 * Permission  to  use, copy, modify and distribute this software and its
 * documentation is hereby granted,  provided  that  both  the  copyright
 * notice  and  this  permission  notice  appear  in  all  copies  of the
 * software, derivative works or  modified  versions,  and  any  portions
 * thereof, and that both notices appear in supporting documentation, and
 * that credit is given to Carnegie Mellon University  in  all  documents
 * and publicity pertaining to direct or indirect use of this code or its
 * derivatives.
 * 
 * CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
 * SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
 * FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
 * DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
 * RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
 * ANY DERIVATIVE WORK.
 * 
 * Carnegie  Mellon  encourages  users  of  this  software  to return any
 * improvements or extensions that  they  make,  and  to  grant  Carnegie
 * Mellon the rights to redistribute these changes without encumbrance.
 * 
 * 	@(#) src/sys/coda/coda_psdev.c,v 1.1.1.1 1998/08/29 21:14:52 rvb Exp $
 */
/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 * This code was written for the Coda filesystem at Carnegie Mellon
 * University.  Contributers include David Steere, James Kistler, and
 * M. Satyanarayanan.  */

/* 
 * These routines define the psuedo device for communication between
 * Coda's Venus and Minicache in Mach 2.6. They used to be in cfs_subr.c, 
 * but I moved them to make it easier to port the Minicache without 
 * porting coda. -- DCS 10/12/94
 */

/* These routines are the device entry points for Venus. */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");


extern int coda_nc_initialized;    /* Set if cache has been initialized */

#include <vcoda.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioccom.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/file.h>		/* must come after sys/malloc.h */
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/poll.h>
#include <sys/proc.h>

#include <coda/coda.h>
#include <coda/cnode.h>
#include <coda/coda_namecache.h>
#include <coda/coda_io.h>
#include <coda/coda_psdev.h>

#define CTL_C

#ifdef CTL_C
#include <sys/signalvar.h>
#endif

int coda_psdev_print_entry = 0;
static
int outstanding_upcalls = 0;
int coda_call_sleep = PZERO - 1;
#ifdef	CTL_C
int coda_pcatch = PCATCH;
#else
#endif

#define ENTRY if(coda_psdev_print_entry) myprintf(("Entered %s\n",__func__))

void vcodaattach(int n);

struct vmsg {
    struct queue vm_chain;
    caddr_t	 vm_data;
    u_short	 vm_flags;
    u_short      vm_inSize;	/* Size is at most 5000 bytes */
    u_short	 vm_outSize;
    u_short	 vm_opcode; 	/* copied from data to save ptr lookup */
    int		 vm_unique;
    caddr_t	 vm_sleep;	/* Not used by Mach. */
};

#define	VM_READ	    1
#define	VM_WRITE    2
#define	VM_INTR	    4

/* vcodaattach: do nothing */
void
vcodaattach(n)
    int n;
{
}

int 
vc_nb_open(dev, flag, mode, td)    
    dev_t        dev;      
    int          flag;     
    int          mode;     
    struct thread *td;             /* NetBSD only */
{
    register struct vcomm *vcp;
    
    ENTRY;

    if (minor(dev) >= NVCODA || minor(dev) < 0)
	return(ENXIO);
    
    if (!coda_nc_initialized)
	coda_nc_init();
    
    vcp = &coda_mnttbl[minor(dev)].mi_vcomm;
    if (VC_OPEN(vcp))
	return(EBUSY);
    
    bzero(&(vcp->vc_selproc), sizeof (struct selinfo));
    INIT_QUEUE(vcp->vc_requests);
    INIT_QUEUE(vcp->vc_replys);
    MARK_VC_OPEN(vcp);
    
    coda_mnttbl[minor(dev)].mi_vfsp = NULL;
    coda_mnttbl[minor(dev)].mi_rootvp = NULL;

    return(0);
}

int 
vc_nb_close (dev, flag, mode, td)    
    dev_t        dev;      
    int          flag;     
    int          mode;     
    struct thread *td;
{
    register struct vcomm *vcp;
    register struct vmsg *vmp, *nvmp = NULL;
    struct coda_mntinfo *mi;
    int                 err;
	
    ENTRY;

    if (minor(dev) >= NVCODA || minor(dev) < 0)
	return(ENXIO);

    mi = &coda_mnttbl[minor(dev)];
    vcp = &(mi->mi_vcomm);
    
    if (!VC_OPEN(vcp))
	panic("vcclose: not open");
    
    /* prevent future operations on this vfs from succeeding by auto-
     * unmounting any vfs mounted via this device. This frees user or
     * sysadm from having to remember where all mount points are located.
     * Put this before WAKEUPs to avoid queuing new messages between
     * the WAKEUP and the unmount (which can happen if we're unlucky)
     */
    if (!mi->mi_rootvp) {
	/* just a simple open/close w no mount */
	MARK_VC_CLOSED(vcp);
	return 0;
    }

    /* Let unmount know this is for real */
    VTOC(mi->mi_rootvp)->c_flags |= C_UNMOUNTING;
    coda_unmounting(mi->mi_vfsp);

    outstanding_upcalls = 0;
    /* Wakeup clients so they can return. */
    for (vmp = (struct vmsg *)GETNEXT(vcp->vc_requests);
	 !EOQ(vmp, vcp->vc_requests);
	 vmp = nvmp)
    {
    	nvmp = (struct vmsg *)GETNEXT(vmp->vm_chain);
	/* Free signal request messages and don't wakeup cause
	   no one is waiting. */
	if (vmp->vm_opcode == CODA_SIGNAL) {
	    CODA_FREE((caddr_t)vmp->vm_data, (u_int)VC_IN_NO_DATA);
	    CODA_FREE((caddr_t)vmp, (u_int)sizeof(struct vmsg));
	    continue;
	}
	outstanding_upcalls++;	
	wakeup(&vmp->vm_sleep);
    }

    for (vmp = (struct vmsg *)GETNEXT(vcp->vc_replys);
	 !EOQ(vmp, vcp->vc_replys);
	 vmp = (struct vmsg *)GETNEXT(vmp->vm_chain))
    {
	outstanding_upcalls++;	
	wakeup(&vmp->vm_sleep);
    }

    MARK_VC_CLOSED(vcp);

    if (outstanding_upcalls) {
#ifdef	CODA_VERBOSE
	printf("presleep: outstanding_upcalls = %d\n", outstanding_upcalls);
    	(void) tsleep(&outstanding_upcalls, coda_call_sleep, "coda_umount", 0);
	printf("postsleep: outstanding_upcalls = %d\n", outstanding_upcalls);
#else
    	(void) tsleep(&outstanding_upcalls, coda_call_sleep, "coda_umount", 0);
#endif
    }

    err = dounmount(mi->mi_vfsp, flag, td);
    if (err)
	myprintf(("Error %d unmounting vfs in vcclose(%d)\n", 
	           err, minor(dev)));
    return 0;
}

int 
vc_nb_read(dev, uiop, flag)   
    dev_t        dev;  
    struct uio  *uiop; 
    int          flag;
{
    register struct vcomm *	vcp;
    register struct vmsg *vmp;
    int error = 0;
    
    ENTRY;

    if (minor(dev) >= NVCODA || minor(dev) < 0)
	return(ENXIO);
    
    vcp = &coda_mnttbl[minor(dev)].mi_vcomm;
    /* Get message at head of request queue. */
    if (EMPTY(vcp->vc_requests))
	return(0);	/* Nothing to read */
    
    vmp = (struct vmsg *)GETNEXT(vcp->vc_requests);
    
    /* Move the input args into userspace */
    uiop->uio_rw = UIO_READ;
    error = uiomove(vmp->vm_data, vmp->vm_inSize, uiop);
    if (error) {
	myprintf(("vcread: error (%d) on uiomove\n", error));
	error = EINVAL;
    }

#ifdef OLD_DIAGNOSTIC    
    if (vmp->vm_chain.forw == 0 || vmp->vm_chain.back == 0)
	panic("vc_nb_read: bad chain");
#endif

    REMQUE(vmp->vm_chain);
    
    /* If request was a signal, free up the message and don't
       enqueue it in the reply queue. */
    if (vmp->vm_opcode == CODA_SIGNAL) {
	if (codadebug)
	    myprintf(("vcread: signal msg (%d, %d)\n", 
		      vmp->vm_opcode, vmp->vm_unique));
	CODA_FREE((caddr_t)vmp->vm_data, (u_int)VC_IN_NO_DATA);
	CODA_FREE((caddr_t)vmp, (u_int)sizeof(struct vmsg));
	return(error);
    }
    
    vmp->vm_flags |= VM_READ;
    INSQUE(vmp->vm_chain, vcp->vc_replys);
    
    return(error);
}

int
vc_nb_write(dev, uiop, flag)   
    dev_t        dev;  
    struct uio  *uiop; 
    int          flag;
{
    register struct vcomm *	vcp;
    register struct vmsg *vmp;
    struct coda_out_hdr *out;
    u_long seq;
    u_long opcode;
    int buf[2];
    int error = 0;

    ENTRY;

    if (minor(dev) >= NVCODA || minor(dev) < 0)
	return(ENXIO);
    
    vcp = &coda_mnttbl[minor(dev)].mi_vcomm;
    
    /* Peek at the opcode, unique without transfering the data. */
    uiop->uio_rw = UIO_WRITE;
    error = uiomove((caddr_t)buf, sizeof(int) * 2, uiop);
    if (error) {
	myprintf(("vcwrite: error (%d) on uiomove\n", error));
	return(EINVAL);
    }
    
    opcode = buf[0];
    seq = buf[1];
	
    if (codadebug)
	myprintf(("vcwrite got a call for %ld.%ld\n", opcode, seq));
    
    if (DOWNCALL(opcode)) {
	union outputArgs pbuf;
	
	/* get the rest of the data. */
	uiop->uio_rw = UIO_WRITE;
	error = uiomove((caddr_t)&pbuf.coda_purgeuser.oh.result, sizeof(pbuf) - (sizeof(int)*2), uiop);
	if (error) {
	    myprintf(("vcwrite: error (%d) on uiomove (Op %ld seq %ld)\n", 
		      error, opcode, seq));
	    return(EINVAL);
	    }
	
	return handleDownCall(opcode, &pbuf);
    }
    
    /* Look for the message on the (waiting for) reply queue. */
    for (vmp = (struct vmsg *)GETNEXT(vcp->vc_replys);
	 !EOQ(vmp, vcp->vc_replys);
	 vmp = (struct vmsg *)GETNEXT(vmp->vm_chain))
    {
	if (vmp->vm_unique == seq) break;
    }
    
    if (EOQ(vmp, vcp->vc_replys)) {
	if (codadebug)
	    myprintf(("vcwrite: msg (%ld, %ld) not found\n", opcode, seq));
	
	return(ESRCH);
	}
    
    /* Remove the message from the reply queue */
    REMQUE(vmp->vm_chain);
    
    /* move data into response buffer. */
    out = (struct coda_out_hdr *)vmp->vm_data;
    /* Don't need to copy opcode and uniquifier. */
    
    /* get the rest of the data. */
    if (vmp->vm_outSize < uiop->uio_resid) {
	myprintf(("vcwrite: more data than asked for (%d < %d)\n",
		  vmp->vm_outSize, uiop->uio_resid));
	wakeup(&vmp->vm_sleep); 	/* Notify caller of the error. */
	return(EINVAL);
    } 
    
    buf[0] = uiop->uio_resid; 	/* Save this value. */
    uiop->uio_rw = UIO_WRITE;
    error = uiomove((caddr_t) &out->result, vmp->vm_outSize - (sizeof(int) * 2), uiop);
    if (error) {
	myprintf(("vcwrite: error (%d) on uiomove (op %ld seq %ld)\n", 
		  error, opcode, seq));
	return(EINVAL);
    }
    
    /* I don't think these are used, but just in case. */
    /* XXX - aren't these two already correct? -bnoble */
    out->opcode = opcode;
    out->unique = seq;
    vmp->vm_outSize	= buf[0];	/* Amount of data transferred? */
    vmp->vm_flags |= VM_WRITE;
    wakeup(&vmp->vm_sleep);
    
    return(0);
}

int
vc_nb_ioctl(dev, cmd, addr, flag, td) 
    dev_t         dev;       
    u_long        cmd;       
    caddr_t       addr;      
    int           flag;      
    struct thread *td;
{
    ENTRY;

    switch(cmd) {
    case CODARESIZE: {
	struct coda_resize *data = (struct coda_resize *)addr;
	return(coda_nc_resize(data->hashsize, data->heapsize, IS_DOWNCALL));
	break;
    }
    case CODASTATS:
	if (coda_nc_use) {
	    coda_nc_gather_stats();
	    return(0);
	} else {
	    return(ENODEV);
	}
	break;
    case CODAPRINT:
	if (coda_nc_use) {
	    print_coda_nc();
	    return(0);
	} else {
	    return(ENODEV);
	}
	break;
    case CIOC_KERNEL_VERSION:
	switch (*(u_int *)addr) {
	case 0:
		*(u_int *)addr = coda_kernel_version;
		return 0;
		break;
	case 1:
	case 2:
		if (coda_kernel_version != *(u_int *)addr)
		    return ENOENT;
		else
		    return 0;
	default:
		return ENOENT;
	}
    	break;
    default :
	return(EINVAL);
	break;
    }
}

int
vc_nb_poll(dev, events, td)         
    dev_t         dev;    
    int           events;   
    struct thread *td;
{
    register struct vcomm *vcp;
    int event_msk = 0;

    ENTRY;
    
    if (minor(dev) >= NVCODA || minor(dev) < 0)
	return(ENXIO);
    
    vcp = &coda_mnttbl[minor(dev)].mi_vcomm;
    
    event_msk = events & (POLLIN|POLLRDNORM);
    if (!event_msk)
	return(0);
    
    if (!EMPTY(vcp->vc_requests))
	return(events & (POLLIN|POLLRDNORM));

    selrecord(td, &(vcp->vc_selproc));
    
    return(0);
}

/*
 * Statistics
 */
struct coda_clstat coda_clstat;

/* 
 * Key question: whether to sleep interuptably or uninteruptably when
 * waiting for Venus.  The former seems better (cause you can ^C a
 * job), but then GNU-EMACS completion breaks. Use tsleep with no
 * timeout, and no longjmp happens. But, when sleeping
 * "uninterruptibly", we don't get told if it returns abnormally
 * (e.g. kill -9).  
 */

int
coda_call(mntinfo, inSize, outSize, buffer) 
     struct coda_mntinfo *mntinfo; int inSize; int *outSize; caddr_t buffer;
{
	struct vcomm *vcp;
	struct vmsg *vmp;
	int error;
#ifdef	CTL_C
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	sigset_t psig_omask;
	sigset_t tempset;
	int i;
#endif
	if (mntinfo == NULL) {
	    /* Unlikely, but could be a race condition with a dying warden */
	    return ENODEV;
	}

	vcp = &(mntinfo->mi_vcomm);
	
	coda_clstat.ncalls++;
	coda_clstat.reqs[((struct coda_in_hdr *)buffer)->opcode]++;

	if (!VC_OPEN(vcp))
	    return(ENODEV);

	CODA_ALLOC(vmp,struct vmsg *,sizeof(struct vmsg));
	/* Format the request message. */
	vmp->vm_data = buffer;
	vmp->vm_flags = 0;
	vmp->vm_inSize = inSize;
	vmp->vm_outSize 
	    = *outSize ? *outSize : inSize; /* |buffer| >= inSize */
	vmp->vm_opcode = ((struct coda_in_hdr *)buffer)->opcode;
	vmp->vm_unique = ++vcp->vc_seq;
	if (codadebug)
	    myprintf(("Doing a call for %d.%d\n", 
		      vmp->vm_opcode, vmp->vm_unique));
	
	/* Fill in the common input args. */
	((struct coda_in_hdr *)buffer)->unique = vmp->vm_unique;

	/* Append msg to request queue and poke Venus. */
	INSQUE(vmp->vm_chain, vcp->vc_requests);
	selwakeup(&(vcp->vc_selproc));

	/* We can be interrupted while we wait for Venus to process
	 * our request.  If the interrupt occurs before Venus has read
	 * the request, we dequeue and return. If it occurs after the
	 * read but before the reply, we dequeue, send a signal
	 * message, and return. If it occurs after the reply we ignore
	 * it. In no case do we want to restart the syscall.  If it
	 * was interrupted by a venus shutdown (vcclose), return
	 * ENODEV.  */

	/* Ignore return, We have to check anyway */
#ifdef	CTL_C
	/* This is work in progress.  Setting coda_pcatch lets tsleep reawaken
	   on a ^c or ^z.  The problem is that emacs sets certain interrupts
	   as SA_RESTART.  This means that we should exit sleep handle the
	   "signal" and then go to sleep again.  Mostly this is done by letting
	   the syscall complete and be restarted.  We are not idempotent and 
	   can not do this.  A better solution is necessary.
	 */
	i = 0;
	PROC_LOCK(p);
	psig_omask = td->td_sigmask;
	do {
		error = msleep(&vmp->vm_sleep, &p->p_mtx,
			       (coda_call_sleep|coda_pcatch), "coda_call",
			       hz*2);
		if (error == 0)
			break;
		else if (error == EWOULDBLOCK) {
#ifdef	CODA_VERBOSE
			printf("coda_call: tsleep TIMEOUT %d sec\n", 2+2*i);
#endif
		}
		else {
			SIGEMPTYSET(tempset);
			SIGADDSET(tempset, SIGIO);
			if (SIGSETEQ(td->td_siglist, tempset)) {
				SIGADDSET(td->td_sigmask, SIGIO);
#ifdef	CODA_VERBOSE
				printf("coda_call: tsleep returns %d SIGIO, cnt %d\n",
				       error, i);
#endif
			} else {
				SIGDELSET(tempset, SIGIO);
				SIGADDSET(tempset, SIGALRM);
				if (SIGSETEQ(td->td_siglist, tempset)) {
					SIGADDSET(td->td_sigmask, SIGALRM);
#ifdef	CODA_VERBOSE
					printf("coda_call: tsleep returns %d SIGALRM, cnt %d\n",
					       error, i);
#endif
				}
				else {
					printf("coda_call: tsleep returns %d, cnt %d\n",
					       error, i);

#if notyet
					tempset = td->td_siglist;
					SIGSETNAND(tempset, td->td_sigmask);
					printf("coda_call: siglist = %p, sigmask = %p, mask %p\n",
					       td->td_siglist, td->td_sigmask,
					       tempset);
					break;
					SIGSETOR(td->td_sigmask, td->td_siglist);
					tempset = td->td_siglist;
					SIGSETNAND(tempset, td->td_sigmask);
					printf("coda_call: new mask, siglist = %p, sigmask = %p, mask %p\n",
					       td->td_siglist, td->td_sigmask,
					       tempset);
#endif
				}
			}
		}
	} while (error && i++ < 128 && VC_OPEN(vcp));
	td->td_sigmask = psig_omask;
	signotify(td);
	PROC_UNLOCK(p);
#else
	(void) tsleep(&vmp->vm_sleep, coda_call_sleep, "coda_call", 0);
#endif
	if (VC_OPEN(vcp)) {	/* Venus is still alive */
 	/* Op went through, interrupt or not... */
	    if (vmp->vm_flags & VM_WRITE) {
		error = 0;
		*outSize = vmp->vm_outSize;
	    }

	    else if (!(vmp->vm_flags & VM_READ)) { 
		/* Interrupted before venus read it. */
#ifdef	CODA_VERBOSE
		if (1)
#else
		if (codadebug)
#endif
		    myprintf(("interrupted before read: op = %d.%d, flags = %x\n",
			   vmp->vm_opcode, vmp->vm_unique, vmp->vm_flags));
		REMQUE(vmp->vm_chain);
		error = EINTR;
	    }
	    
	    else { 	
		/* (!(vmp->vm_flags & VM_WRITE)) means interrupted after
                   upcall started */
		/* Interrupted after start of upcall, send venus a signal */
		struct coda_in_hdr *dog;
		struct vmsg *svmp;
		
#ifdef	CODA_VERBOSE
		if (1)
#else
		if (codadebug)
#endif
		    myprintf(("Sending Venus a signal: op = %d.%d, flags = %x\n",
			   vmp->vm_opcode, vmp->vm_unique, vmp->vm_flags));
		
		REMQUE(vmp->vm_chain);
		error = EINTR;
		
		CODA_ALLOC(svmp, struct vmsg *, sizeof (struct vmsg));

		CODA_ALLOC((svmp->vm_data), char *, sizeof (struct coda_in_hdr));
		dog = (struct coda_in_hdr *)svmp->vm_data;
		
		svmp->vm_flags = 0;
		dog->opcode = svmp->vm_opcode = CODA_SIGNAL;
		dog->unique = svmp->vm_unique = vmp->vm_unique;
		svmp->vm_inSize = sizeof (struct coda_in_hdr);
/*??? rvb */	svmp->vm_outSize = sizeof (struct coda_in_hdr);
		
		if (codadebug)
		    myprintf(("coda_call: enqueing signal msg (%d, %d)\n",
			   svmp->vm_opcode, svmp->vm_unique));
		
		/* insert at head of queue! */
		INSQUE(svmp->vm_chain, vcp->vc_requests);
		selwakeup(&(vcp->vc_selproc));
	    }
	}

	else {	/* If venus died (!VC_OPEN(vcp)) */
	    if (codadebug)
		myprintf(("vcclose woke op %d.%d flags %d\n",
		       vmp->vm_opcode, vmp->vm_unique, vmp->vm_flags));
	    
		error = ENODEV;
	}

	CODA_FREE(vmp, sizeof(struct vmsg));

	if (outstanding_upcalls > 0 && (--outstanding_upcalls == 0))
		wakeup(&outstanding_upcalls);

	if (!error)
		error = ((struct coda_out_hdr *)buffer)->result;
	return(error);
}
