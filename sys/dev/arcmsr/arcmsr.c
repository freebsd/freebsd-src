/*
******************************************************************************************
**        O.S   : FreeBSD
**   FILE NAME  : arcmsr.c
**        BY    : Erich Chen   
**   Description: SCSI RAID Device Driver for 
**                ARECA (ARC1110/1120/1160/1210/1220/1260) SATA RAID HOST Adapter
**                ARCMSR RAID Host adapter[RAID controller:INTEL 331(PCI-X) 341(PCI-EXPRESS) chip set]
******************************************************************************************
************************************************************************
**
** Copyright (c) 2004-2006 ARECA Co. Ltd.
**        Erich Chen, Taipei Taiwan All rights reserved.
**
** Redistribution and use in source and binary forms,with or without
** modification,are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice,this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice,this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES,INCLUDING,BUT NOT LIMITED TO,THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,INDIRECT,
** INCIDENTAL,SPECIAL,EXEMPLARY,OR CONSEQUENTIAL DAMAGES(INCLUDING,BUT
** NOT LIMITED TO,PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA,OR PROFITS; OR BUSINESS INTERRUPTION)HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY,WHETHER IN CONTRACT,STRICT LIABILITY,OR TORT
**(INCLUDING NEGLIGENCE OR OTHERWISE)ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE,EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**************************************************************************
** History
**
**        REV#         DATE	            NAME	         DESCRIPTION
**     1.00.00.00    3/31/2004	       Erich Chen	     First release
**     1.20.00.02   11/29/2004         Erich Chen        bug fix with arcmsr_bus_reset when PHY error
******************************************************************************************
** $FreeBSD$
*/
#define ARCMSR_DEBUG            1
/*
**********************************
*/
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/devicestat.h>
#include <sys/kthread.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/sysctl.h>
#include <sys/poll.h>
#include <sys/ioccom.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <isa/rtc.h>

#include <machine/bus_memio.h>
#include <machine/bus.h>
#include <machine/clock.h>
#include <machine/resource.h>
#include <machine/atomic.h>
#include <sys/conf.h>
#include <sys/rman.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>
/* 
**************************************************************************
** Define the OS version specific locks 
**************************************************************************
*/
#if __FreeBSD_version >= 500005
    #include <sys/selinfo.h>
	#include <sys/mutex.h>
    #include <dev/pci/pcivar.h>
    #include <dev/pci/pcireg.h>
	#define ARCMSR_LOCK_INIT(l, s)	        mtx_init(l, s,NULL, MTX_DEF|MTX_RECURSE)
	#define ARCMSR_LOCK_ACQUIRE(l)	        mtx_lock(l)
	#define ARCMSR_LOCK_RELEASE(l)	        mtx_unlock(l)
	typedef struct mtx                      arcmsr_lock_t;
#else
    #include <sys/select.h>
    #include <pci/pcivar.h>
    #include <pci/pcireg.h>
	#define ARCMSR_LOCK_INIT(l, s)	        simple_lock_init(l)
	#define ARCMSR_LOCK_ACQUIRE(l)	        simple_lock(l)
	#define ARCMSR_LOCK_RELEASE(l)	        simple_unlock(l)
	typedef struct simplelock               arcmsr_lock_t;
#endif
#include <dev/arcmsr/arcmsr.h>
/*
**************************************************************************
** __FreeBSD_version 502010
**************************************************************************
*/
static VOID arcmsr_interrupt(VOID *arg);
static LONG arcmsr_probe(device_t dev);
static LONG arcmsr_attach(device_t dev);
static LONG arcmsr_detach(device_t dev);
static VOID arcmsr_shutdown(device_t dev);
#if 0
ULONG arcmsr_make_timespec(ULONG year,ULONG mon,ULONG day,ULONG hour,ULONG min,ULONG sec);
ULONG arcmsr_getcmos_time(VOID);
#endif
LONG arcmsr_queue_dpc(PACB pACB,DPCFUN dpcfun,VOID *arg);
LONG arcmsr_iop_ioctlcmd(PACB pACB,ULONG ioctl_cmd,caddr_t arg);
BOOLEAN arcmsr_seek_cmd2abort(union ccb * pabortccb);
BOOLEAN arcmsr_wait_msgint_ready(PACB pACB);
PSRB arcmsr_get_freesrb(PACB pACB);
VOID arcmsr_free_resource(PACB pACB);
VOID arcmsr_bus_reset(PACB pACB);
VOID arcmsr_stop_adapter_bgrb(PACB pACB);
VOID arcmsr_start_adapter_bgrb(PACB pACB);
VOID arcmsr_iop_init(PACB pACB);
VOID arcmsr_do_dpcQ(PACB pACB);
VOID arcmsr_flush_adapter_cache(PACB pACB);
VOID arcmsr_do_thread_works(VOID *arg);
VOID arcmsr_queue_wait2go_srb(PACB pACB,PSRB pSRB);
VOID arcmsr_post_wait2go_srb(PACB pACB);
VOID arcmsr_post_Qbuffer(PACB pACB);
VOID arcmsr_abort_allcmd(PACB pACB);
VOID arcmsr_srb_complete(PSRB pSRB);
VOID arcmsr_iop_reset(PACB pACB);
VOID arcmsr_report_SenseInfoBuffer(PSRB pSRB);
VOID arcmsr_build_srb(PSRB pSRB, bus_dma_segment_t *dm_segs, LONG nseg);
/*
*****************************************************************************************
** Character device switch table
**struct cdevsw {
**	d_open_t		*d_open;
**	d_close_t		*d_close;
**	d_read_t		*d_read;
**	d_write_t		*d_write;
**	d_ioctl_t		*d_ioctl;
**	d_poll_t		*d_poll;
**	d_mmap_t		*d_mmap;
**	d_strategy_t	*d_strategy;
**	const char	    *d_name;	   "" base device name, e.g. 'vn' 
**	int		         d_maj;
**	d_dump_t	    *d_dump;
**	d_psize_t	    *d_psize;
**	u_int		     d_flags;
**	int		         d_bmaj;
**	d_kqfilter_t	*d_kqfilter;   "" additions below are not binary compatible with 4.2 and below 
**};
******************************************************************************************
*/
/*
**************************************************************************
** Insert a delay in micro-seconds and milli-seconds.
** static void MDELAY(LONG ms) { while (ms--) UDELAY(1000); }
**************************************************************************
*/
static VOID UDELAY(LONG us) { DELAY(us); }
/*
**************************************************************************
** 
**************************************************************************
*/
static bus_dmamap_callback_t arcmsr_map_freesrb;
static bus_dmamap_callback_t arcmsr_executesrb;
/*
**************************************************************************
** 
**************************************************************************
*/
static d_open_t	arcmsr_open;
static d_close_t arcmsr_close;
static d_ioctl_t arcmsr_ioctl;

static device_method_t arcmsr_methods[]={
	DEVMETHOD(device_probe,		arcmsr_probe),
	DEVMETHOD(device_attach,	arcmsr_attach),
	DEVMETHOD(device_detach,	arcmsr_detach),
    DEVMETHOD(device_shutdown,	arcmsr_shutdown),
	{ 0,0 }
};

static driver_t arcmsr_driver={
	"arcmsr",arcmsr_methods,sizeof(struct _ACB)
};

static devclass_t arcmsr_devclass;
DRIVER_MODULE(arcmsr,pci,arcmsr_driver,arcmsr_devclass,0,0);

#if __FreeBSD_version >= 502010
	static struct cdevsw arcmsr_cdevsw={
	    .d_version = D_VERSION,
	    .d_flags   = D_NEEDGIANT,
		.d_open    = arcmsr_open,		/* open     */
		.d_close   = arcmsr_close,		/* close    */
		.d_ioctl   = arcmsr_ioctl,		/* ioctl    */
		.d_name    = "arcmsr",			/* name     */
	};
#else
	#define ARCMSR_CDEV_MAJOR	180

	static struct cdevsw arcmsr_cdevsw = {
		arcmsr_open,		        /* open     */
		arcmsr_close,		        /* close    */
		noread,			            /* read     */
		nowrite,		            /* write    */
		arcmsr_ioctl,		        /* ioctl    */
		nopoll,		                /* poll     */
		nommap,			            /* mmap     */
		nostrategy,		            /* strategy */
		"arcmsr",			        /* name     */
		ARCMSR_CDEV_MAJOR,		    /* major    */
		nodump,			            /* dump     */
		nopsize,		            /* psize    */
		0			                /* flags    */
	};
#endif

#if __FreeBSD_version < 500005
    static int arcmsr_open(dev_t dev, int flags, int fmt, struct proc *proc)
#else
    #if __FreeBSD_version < 503000
        static int arcmsr_open(dev_t dev, int flags, int fmt, struct thread *proc)
    #else
        static int arcmsr_open(struct cdev *dev, int flags, int fmt, d_thread_t *proc)
    #endif 
#endif
{
	#if __FreeBSD_version < 503000
	    PACB pACB=dev->si_drv1;
    #else
		int	unit = minor(dev);
		PACB pACB = devclass_get_softc(arcmsr_devclass, unit);   
    #endif

	if(pACB==NULL)
	{
		return ENXIO;
	}
	/* Check to make sure the device isn't already open */
	if (pACB->acb_flags & ACB_F_IOCTL_OPEN) 
	{
		return EBUSY;
	}
	pACB->acb_flags |= ACB_F_IOCTL_OPEN;
	return 0;
}
/*
**************************************************************************
**************************************************************************
*/
#if __FreeBSD_version < 500005
    static int arcmsr_close(dev_t dev, int flags, int fmt, struct proc *proc)
#else
    #if __FreeBSD_version < 503000
        static int arcmsr_close(dev_t dev, int flags, int fmt, struct thread *proc)
    #else
        static int arcmsr_close(struct cdev *dev, int flags, int fmt, d_thread_t *proc)
    #endif 
#endif
{
	#if __FreeBSD_version < 503000
	    PACB pACB=dev->si_drv1;
    #else
		int	unit = minor(dev);
		PACB pACB = devclass_get_softc(arcmsr_devclass, unit);   
    #endif

	if(pACB==NULL)
	{
		return ENXIO;
	}
	pACB->acb_flags &= ~ACB_F_IOCTL_OPEN;
	return 0;
}
/*
**************************************************************************
**ENOENT
**ENOIOCTL
**ENOMEM
**EINVAL
**************************************************************************
*/
#if __FreeBSD_version < 500005
    static int arcmsr_ioctl(dev_t dev, u_long ioctl_cmd, caddr_t arg, int flags, struct proc *proc)
#else
    #if __FreeBSD_version < 503000
        static int arcmsr_ioctl(dev_t dev, u_long ioctl_cmd, caddr_t arg, int flags, struct thread *proc)
    #else
        static int arcmsr_ioctl(struct cdev *dev, u_long ioctl_cmd, caddr_t arg,int flags, d_thread_t *proc)
    #endif 
#endif
{
	#if __FreeBSD_version < 503000
	    PACB pACB=dev->si_drv1;
    #else
		int	unit = minor(dev);
		PACB pACB = devclass_get_softc(arcmsr_devclass, unit);   
    #endif

	if(pACB==NULL)
	{
		return ENXIO;
	}
    return(arcmsr_iop_ioctlcmd(pACB,ioctl_cmd,arg));
}
/*
**************************************************************************
**************************************************************************
*/
LONG arcmsr_queue_dpc(PACB pACB,DPCFUN dpcfun,VOID *arg)
{
	ULONG s;
	UCHAR index_pointer;

	#if ARCMSR_DEBUG0
	printf("arcmsr_queue_dpc................. \n");
	#endif

    s=splcam();
	index_pointer=(pACB->dpcQ_tail + 1) % ARCMSR_MAX_DPC;
	if(index_pointer==pACB->dpcQ_head) 
	{
        splx(s);
		printf("DPC Queue full!\n");
		return -1;
	}
	pACB->dpcQ[pACB->dpcQ_tail].dpcfun=dpcfun;
	pACB->dpcQ[pACB->dpcQ_tail].arg=arg;
	pACB->dpcQ_tail=index_pointer;
	/* 
	*********************************************************
	*********************************************************
	*/
	wakeup(pACB->kthread_proc); 

    splx(s);
	return 0;
}
/*
**************************************************************************
**         arcmsr_do_dpcQ
**    execute dpc routine by kernel thread 
***************************************************************************
*/
VOID arcmsr_do_dpcQ(PACB pACB)
{
	#if ARCMSR_DEBUG0
	printf("arcmsr_do_dpcQ................. \n");
	#endif
	/*
	******************************************
	******************************************
	*/
	while (pACB->dpcQ_head!=pACB->dpcQ_tail) 
	{
		ULONG s;
		DPC dpc;

		/* got a "dpc routine" */
        s=splcam();
		dpc=pACB->dpcQ[pACB->dpcQ_head];
		pACB->dpcQ_head++;
		pACB->dpcQ_head %=ARCMSR_MAX_DPC;
        splx(s);
		/* execute this "dpc routine" */
		dpc.dpcfun(dpc.arg);
	}
	return;
}
#if 0
/*
**********************************************************************
** <second> bit 05,04,03,02,01,00: 0 - 59 
** <minute> bit 11,10,09,08,07,06: 0 - 59 
** <month>  bit       15,14,13,12: 1 - 12 
** <hour>   bit 21,20,19,18,17,16: 0 - 59 
** <day>    bit    26,25,24,23,22: 1 - 31 
** <year>   bit    31,30,29,28,27: 0=2000,31=2031 
**********************************************************************
*/
ULONG arcmsr_make_timespec(ULONG year,ULONG mon,ULONG day,ULONG hour,ULONG min,ULONG sec)
{
    return((year<<27)|(day<<22)|(hour<<16)|(mon<<12)|(min<<6)|(sec));
}
/*
********************************************************************
********************************************************************
*/
ULONG arcmsr_getcmos_time(VOID)
{
	ULONG year,mon,day,hour,min,sec;

    #if ARCMSR_DEBUG0
    printf("arcmsr_getcmos_time \n");
    #endif
	sec=bcd2bin(rtcin(RTC_SEC));
	min=bcd2bin(rtcin(RTC_MIN));
	hour=bcd2bin(rtcin(RTC_HRS));
	day=bcd2bin(rtcin(RTC_DAY));
	mon=bcd2bin(rtcin(RTC_MONTH));
	year=bcd2bin(rtcin(RTC_YEAR));
	if((year +=1900) < 1970)
		year +=100;
	return arcmsr_make_timespec(year,mon,day,hour,min,sec);
}
#endif
/*
*********************************************************************************
**  Asynchronous notification handler.
*********************************************************************************
*/
static VOID arcmsr_async(VOID *cb_arg, ULONG code, struct cam_path *path, VOID *arg)
{
	PACB pACB;
	UCHAR target_id,target_lun;
	struct cam_sim *sim;
	ULONG s;
    #if ARCMSR_DEBUG0
    printf("arcmsr_async.......................................... \n");
    #endif
	s=splcam();

	sim=(struct cam_sim *) cb_arg;
	pACB =(PACB) cam_sim_softc(sim);
	switch (code)
	{
	case AC_LOST_DEVICE:
		target_id=xpt_path_target_id(path);
        target_lun=xpt_path_lun_id(path);
		if((target_id > ARCMSR_MAX_TARGETID) || (target_lun > ARCMSR_MAX_TARGETLUN))
		{
			break;
		}
        printf("%s:scsi id%d lun%d device lost \n",device_get_name(pACB->pci_dev),target_id,target_lun);
		break;
	default:
		break;
	}
	splx(s);
}
/*
**************************************************************************
*         arcmsr_do_thread_works
*    execute programs schedule by kernel thread
*    execute programs schedule by kernel thread
*      :do background rebuilding 
*
* tsleep(void *ident,int priority,const char *wmesg,int timo)
* tsleep()
* General sleep call.  Suspends the current process until a wakeup is
* performed on the specified identifier.  The process will then be made
* runnable with the specified priority.  Sleeps at most timo/hz seconds
* (0 means no timeout).  If pri includes PCATCH flag, signals are checked
* before and after sleeping, else signals are not checked.  Returns 0 if
* awakened, EWOULDBLOCK if the timeout expires.  If PCATCH is set and a
* signal needs to be delivered, ERESTART is returned if the current system
* call should be restarted if possible, and EINTR is returned if the system
* call should be interrupted by the signal (return EINTR).
*
* await(int priority, int timo)
* await() - wait for async condition to occur.   The process blocks until
* wakeup() is called on the most recent asleep() address.  If wakeup is called
* priority to await(), await() winds up being a NOP.
*
* If await() is called more then once (without an intervening asleep() call),
* await() is still effectively a NOP but it calls mi_switch() to give other
* processes some cpu before returning.  The process is left runnable.
*
* <<<<<<<< EXPERIMENTAL, UNTESTED >>>>>>>>>>
* asleep(void *ident, int priority, const char *wmesg, int timo)
* asleep() - async sleep call.  Place process on wait queue and return 
* immediately without blocking.  The process stays runnable until await() 
* is called.  If ident is NULL, remove process from wait queue if it is still
* on one.
*
* Only the most recent sleep condition is effective when making successive
* calls to asleep() or when calling tsleep().
*
* The timeout, if any, is not initiated until await() is called.  The sleep
* priority, signal, and timeout is specified in the asleep() call but may be
* overriden in the await() call.
*
* <<<<<<<< EXPERIMENTAL, UNTESTED >>>>>>>>>>
*      :do background rebuilding 
***************************************************************************
*/
VOID arcmsr_do_thread_works(VOID *arg)
{
	PACB pACB=(PACB) arg;
	ARCMSR_LOCK_INIT(&pACB->arcmsr_kthread_lock, "arcmsr kthread lock");

	#if ARCMSR_DEBUG0
	printf("arcmsr_do_thread_works................. \n");
	#endif

	ARCMSR_LOCK_ACQUIRE(&pACB->arcmsr_kthread_lock);
	while(1) 
	{
		tsleep((caddr_t)pACB->kthread_proc, PRIBIO | PWAIT, "arcmsr",  hz/4);/*.25 sec*/
		/*
		** if do_dpcQ_semaphore is signal
		** do following works
		*/
        arcmsr_do_dpcQ(pACB); /*see if there were some dpc routine need to execute*/
		if(pACB->acb_flags & ACB_F_STOP_THREAD) 
		{
			ARCMSR_LOCK_RELEASE(&pACB->arcmsr_kthread_lock);
			break;
		}
	}
	kthread_exit(0);
	return;
}
/*
************************************************************************
**
**
************************************************************************
*/
VOID arcmsr_flush_adapter_cache(PACB pACB)
{
    #if ARCMSR_DEBUG0
    printf("arcmsr_flush_adapter_cache..............\n");
    #endif
	CHIP_REG_WRITE32(&pACB->pmu->inbound_msgaddr0,ARCMSR_INBOUND_MESG0_FLUSH_CACHE);
	return;
}
/*
**********************************************************************
** 
**  
**
**********************************************************************
*/
BOOLEAN arcmsr_wait_msgint_ready(PACB pACB)
{
	ULONG Index;
	UCHAR Retries=0x00;
	do
	{
		for(Index=0; Index < 500000; Index++)
		{
			if(CHIP_REG_READ32(&pACB->pmu->outbound_intstatus) & ARCMSR_MU_OUTBOUND_MESSAGE0_INT)
			{
				CHIP_REG_WRITE32(&pACB->pmu->outbound_intstatus, ARCMSR_MU_OUTBOUND_MESSAGE0_INT);/*clear interrupt*/
				return TRUE;
			}
			/* one us delay	*/
			UDELAY(10);
		}/*max 5 seconds*/
	}while(Retries++ < 24);/*max 2 minutes*/
	return FALSE;
}
/*
**********************************************************************
**
**  Q back this SRB into ACB ArraySRB
**
**********************************************************************
*/
VOID arcmsr_srb_complete(PSRB pSRB)
{
	ULONG s;
	PACB pACB=pSRB->pACB;
    union ccb *pccb=pSRB->pccb;

	#if ARCMSR_DEBUG0
	printf("arcmsr_srb_complete: pSRB=%p srb_doneindex=%x srb_startindex=%x\n",pSRB,pACB->srb_doneindex,pACB->srb_startindex);
	#endif

	if ((pccb->ccb_h.flags & CAM_DIR_MASK) != CAM_DIR_NONE)
	{
		bus_dmasync_op_t op;

		if ((pccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN)
		{
			op = BUS_DMASYNC_POSTREAD;
		}
		else
		{
			op = BUS_DMASYNC_POSTWRITE;
		}
		bus_dmamap_sync(pACB->buffer_dmat, pSRB->dmamap, op);
		bus_dmamap_unload(pACB->buffer_dmat, pSRB->dmamap);
	}
    s=splcam();
	atomic_subtract_int(&pACB->srboutstandingcount,1);
	pSRB->startdone=ARCMSR_SRB_DONE;
	pSRB->srb_flags=0;
	pACB->psrbringQ[pACB->srb_doneindex]=pSRB;
    pACB->srb_doneindex++;
    pACB->srb_doneindex %= ARCMSR_MAX_FREESRB_NUM;
    splx(s);
    xpt_done(pccb);
	return;
}
/*
**********************************************************************
**       if scsi error do auto request sense
**********************************************************************
*/
VOID arcmsr_report_SenseInfoBuffer(PSRB pSRB)
{
	union ccb *pccb=pSRB->pccb;
	PSENSE_DATA  psenseBuffer=(PSENSE_DATA)&pccb->csio.sense_data;
	#if ARCMSR_DEBUG0
    printf("arcmsr_report_SenseInfoBuffer...........\n");
	#endif

    pccb->ccb_h.status|=CAM_REQ_CMP;
    if(psenseBuffer) 
	{
		memset(psenseBuffer, 0, sizeof(pccb->csio.sense_data));
		memcpy(psenseBuffer,pSRB->arcmsr_cdb.SenseData,get_min(sizeof(struct _SENSE_DATA),sizeof(pccb->csio.sense_data)));
	    psenseBuffer->ErrorCode=0x70;
        psenseBuffer->Valid=1;
		pccb->ccb_h.status|=CAM_AUTOSNS_VALID;
    }
    return;
}
/*
*********************************************************************
** to insert pSRB into tail of pACB wait exec srbQ 
*********************************************************************
*/
VOID arcmsr_queue_wait2go_srb(PACB pACB,PSRB pSRB)
{
    ULONG s;
	LONG i=0;
    #if ARCMSR_DEBUG0
	printf("arcmsr_qtail_wait2go_srb:......................................... \n");
    #endif

	s=splcam();
	while(1)
	{
		if(pACB->psrbwait2go[i]==NULL)
		{
			pACB->psrbwait2go[i]=pSRB;
        	atomic_add_int(&pACB->srbwait2gocount,1);
            splx(s);
			return;
		}
		i++;
		i%=ARCMSR_MAX_OUTSTANDING_CMD;
	}
	return;
}
/*
*********************************************************************
** 
*********************************************************************
*/
VOID arcmsr_abort_allcmd(PACB pACB)
{
	CHIP_REG_WRITE32(&pACB->pmu->inbound_msgaddr0,ARCMSR_INBOUND_MESG0_ABORT_CMD);
	return;
}

/*
****************************************************************************
** Routine Description: Reset 80331 iop.
**           Arguments: 
**        Return Value: Nothing.
****************************************************************************
*/
VOID arcmsr_iop_reset(PACB pACB)
{
	PSRB pSRB,pfreesrb;
	ULONG intmask_org,mask;
    LONG i=0;

	#if ARCMSR_DEBUG0
	printf("arcmsr_iop_reset: reset iop controller......................................\n");
	#endif
	if(pACB->srboutstandingcount!=0)
	{
		/* Q back all outstanding srb into wait exec psrb Q*/
		#if ARCMSR_DEBUG0
		printf("arcmsr_iop_reset: srboutstandingcount=%d ...\n",pACB->srboutstandingcount);
		#endif
        /* disable all outbound interrupt */
		intmask_org=CHIP_REG_READ32(&pACB->pmu->outbound_intmask);
        CHIP_REG_WRITE32(&pACB->pmu->outbound_intmask,intmask_org|ARCMSR_MU_OUTBOUND_ALL_INTMASKENABLE);
        /* talk to iop 331 outstanding command aborted*/
		arcmsr_abort_allcmd(pACB);
		if(arcmsr_wait_msgint_ready(pACB)!=TRUE)
		{
            printf("arcmsr_iop_reset: wait 'abort all outstanding command' timeout.................in \n");
		}
		/*clear all outbound posted Q*/
		for(i=0;i<ARCMSR_MAX_OUTSTANDING_CMD;i++)
		{
			CHIP_REG_READ32(&pACB->pmu->outbound_queueport);
		}
		pfreesrb=pACB->pfreesrb;
		for(i=0;i<ARCMSR_MAX_FREESRB_NUM;i++)
		{
	    	pSRB=&pfreesrb[i];
			if(pSRB->startdone==ARCMSR_SRB_START)
			{
				pSRB->startdone=ARCMSR_SRB_ABORTED;
                pSRB->pccb->ccb_h.status=CAM_REQ_ABORTED;
                arcmsr_srb_complete(pSRB);
			}
		}
		/* enable all outbound interrupt */
		mask=~(ARCMSR_MU_OUTBOUND_POSTQUEUE_INTMASKENABLE|ARCMSR_MU_OUTBOUND_DOORBELL_INTMASKENABLE|ARCMSR_MU_OUTBOUND_MESSAGE0_INTMASKENABLE);
        CHIP_REG_WRITE32(&pACB->pmu->outbound_intmask,intmask_org & mask);
		atomic_set_int(&pACB->srboutstandingcount,0);
		/* post abort all outstanding command message to RAID controller */
	}
	i=0;
	while(pACB->srbwait2gocount > 0)
	{
		pSRB=pACB->psrbwait2go[i];
		if(pSRB!=NULL)
		{
			#if ARCMSR_DEBUG0
			printf("arcmsr_iop_reset:abort command... srbwait2gocount=%d ...\n",pACB->srbwait2gocount);
			#endif
		    pACB->psrbwait2go[i]=NULL;
            pSRB->startdone=ARCMSR_SRB_ABORTED;
			pSRB->pccb->ccb_h.status=CAM_REQ_ABORTED;
            arcmsr_srb_complete(pSRB);
			atomic_subtract_int(&pACB->srbwait2gocount,1);
		}
		i++;
		i%=ARCMSR_MAX_OUTSTANDING_CMD;
	}
	return;
}
/*
**********************************************************************
** 
** PAGE_SIZE=4096 or 8192,PAGE_SHIFT=12
**********************************************************************
*/
VOID arcmsr_build_srb(PSRB pSRB, bus_dma_segment_t *dm_segs, LONG nseg)
{
    PARCMSR_CDB pARCMSR_CDB=&pSRB->arcmsr_cdb;
	PCHAR psge=(PCHAR)&pARCMSR_CDB->u;
	ULONG address_lo,address_hi;
	union ccb *pccb=pSRB->pccb;
	struct ccb_scsiio *pcsio=&pccb->csio;
	LONG arccdbsize=0x30;

	#if ARCMSR_DEBUG0
	printf("arcmsr_build_srb........................... \n");
	#endif
	memset(pARCMSR_CDB,0,sizeof(struct _ARCMSR_CDB));
    pARCMSR_CDB->Bus=0;
    pARCMSR_CDB->TargetID=pccb->ccb_h.target_id;
    pARCMSR_CDB->LUN=pccb->ccb_h.target_lun;
    pARCMSR_CDB->Function=1;
	pARCMSR_CDB->CdbLength=(UCHAR)pcsio->cdb_len;
    pARCMSR_CDB->Context=(CPT2INT)pARCMSR_CDB;
	bcopy(pcsio->cdb_io.cdb_bytes, pARCMSR_CDB->Cdb, pcsio->cdb_len);
	if(nseg != 0) 
	{
		PACB pACB=pSRB->pACB;
		bus_dmasync_op_t   op;	
		LONG length,i,cdb_sgcount=0;

		/* map stor port SG list to our iop SG List.*/
		for(i=0;i<nseg;i++) 
		{
			/* Get the physical address of the current data pointer */
			length=(ULONG) dm_segs[i].ds_len;
            address_lo=dma_addr_lo32(dm_segs[i].ds_addr);
			address_hi=dma_addr_hi32(dm_segs[i].ds_addr);
			if(address_hi==0)
			{
				PSG32ENTRY pdma_sg=(PSG32ENTRY)psge;
				pdma_sg->address=address_lo;
				pdma_sg->length=length;
				psge += sizeof(SG32ENTRY);
				arccdbsize += sizeof(SG32ENTRY);
			}
			else
			{
				LONG sg64s_size=0,tmplength=length;

     			#if ARCMSR_DEBUG0
				printf("arcmsr_build_srb: !!!!!!!!!!!......address_hi=%x.... \n",address_hi);
				#endif
				while(1)
				{
					LONG64 span4G,length0;
					PSG64ENTRY pdma_sg=(PSG64ENTRY)psge;

					span4G=(LONG64)address_lo + tmplength;
					pdma_sg->addresshigh=address_hi;
					pdma_sg->address=address_lo;
					if(span4G > 0x100000000)
					{   
						/*see if cross 4G boundary*/
						length0=0x100000000-address_lo;
						pdma_sg->length=(ULONG)length0|IS_SG64_ADDR;
						address_hi=address_hi+1;
						address_lo=0;
						tmplength=tmplength-(LONG)length0;
						sg64s_size += sizeof(SG64ENTRY);
						psge += sizeof(SG64ENTRY);
						cdb_sgcount++;
					}
					else
					{
    					pdma_sg->length=tmplength|IS_SG64_ADDR;
						sg64s_size += sizeof(SG64ENTRY);
						psge += sizeof(SG64ENTRY);
						break;
					}
				}
				arccdbsize += sg64s_size;
			}
			cdb_sgcount++;
		}
		pARCMSR_CDB->sgcount=(UCHAR)cdb_sgcount;
		pARCMSR_CDB->DataLength=pcsio->dxfer_len;
		if( arccdbsize > 256)
		{
			pARCMSR_CDB->Flags|=ARCMSR_CDB_FLAG_SGL_BSIZE;
		}
		if((pccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN)
		{
			op=BUS_DMASYNC_PREREAD;
		}
		else
		{
			op=BUS_DMASYNC_PREWRITE;
			pARCMSR_CDB->Flags|=ARCMSR_CDB_FLAG_WRITE;
			pSRB->srb_flags|=SRB_FLAG_WRITE;
		}
    	bus_dmamap_sync(pACB->buffer_dmat, pSRB->dmamap, op);
	}
	#if ARCMSR_DEBUG0
	printf("arcmsr_build_srb: pSRB=%p cmd=%x xferlength=%d arccdbsize=%d sgcount=%d\n",pSRB,pcsio->cdb_io.cdb_bytes[0],pARCMSR_CDB->DataLength,arccdbsize,pARCMSR_CDB->sgcount);
	#endif
    return;
}
/*
**************************************************************************
**
**	arcmsr_post_srb - Send a protocol specific ARC send postcard to a AIOC .
**	handle: Handle of registered ARC protocol driver
**	adapter_id: AIOC unique identifier(integer)
**	pPOSTCARD_SEND: Pointer to ARC send postcard
**
**	This routine posts a ARC send postcard to the request post FIFO of a
**	specific ARC adapter.
**                             
**************************************************************************
*/ 
static VOID arcmsr_post_srb(PACB pACB,PSRB pSRB)
{
	ULONG cdb_shifted_phyaddr=(ULONG) pSRB->cdb_shifted_phyaddr;
	PARCMSR_CDB pARCMSR_CDB=(PARCMSR_CDB)&pSRB->arcmsr_cdb;

	#if ARCMSR_DEBUG0
	printf("arcmsr_post_srb: pSRB=%p  cdb_shifted_phyaddr=%x\n",pSRB,cdb_shifted_phyaddr);
	#endif
    atomic_add_int(&pACB->srboutstandingcount,1);
	pSRB->startdone=ARCMSR_SRB_START;
	if(pARCMSR_CDB->Flags & ARCMSR_CDB_FLAG_SGL_BSIZE)
	{
	    CHIP_REG_WRITE32(&pACB->pmu->inbound_queueport,cdb_shifted_phyaddr|ARCMSR_SRBPOST_FLAG_SGL_BSIZE);
	}
	else
	{
	    CHIP_REG_WRITE32(&pACB->pmu->inbound_queueport,cdb_shifted_phyaddr);
	}
	return;
}
/*
**************************************************************************
**
**
**************************************************************************
*/
VOID arcmsr_post_wait2go_srb(PACB pACB)
{
	ULONG s;
	PSRB pSRB;
	LONG i=0;
	#if ARCMSR_DEBUG0
	printf("arcmsr_post_wait2go_srb:srbwait2gocount=%d srboutstandingcount=%d\n",pACB->srbwait2gocount,pACB->srboutstandingcount);
	#endif
    s=splcam();
	while((pACB->srbwait2gocount > 0) && (pACB->srboutstandingcount < ARCMSR_MAX_OUTSTANDING_CMD))
	{
		pSRB=pACB->psrbwait2go[i];
		if(pSRB!=NULL)
		{
			pACB->psrbwait2go[i]=NULL;
			arcmsr_post_srb(pACB,pSRB);
			atomic_subtract_int(&pACB->srbwait2gocount,1);
		}
		i++;
		i%=ARCMSR_MAX_OUTSTANDING_CMD;
	}
	splx(s);
	return;
}
/*
**********************************************************************
**   Function: arcmsr_post_Qbuffer
**     Output: 
**********************************************************************
*/
VOID arcmsr_post_Qbuffer(PACB pACB)
{
    ULONG s;
	PUCHAR pQbuffer;
	PQBUFFER pwbuffer=(PQBUFFER)&pACB->pmu->ioctl_wbuffer;
    PUCHAR iop_data=(PUCHAR)pwbuffer->data;
	LONG allxfer_len=0;

    s=splcam();
	while((pACB->wqbuf_firstindex!=pACB->wqbuf_lastindex) && (allxfer_len<124))
	{
		pQbuffer=&pACB->wqbuffer[pACB->wqbuf_firstindex];
		memcpy(iop_data,pQbuffer,1);
		pACB->wqbuf_firstindex++;
		pACB->wqbuf_firstindex %= ARCMSR_MAX_QBUFFER; /*if last index number set it to 0 */
		iop_data++;
		allxfer_len++;
	}
	pwbuffer->data_len=allxfer_len;
	/*
	** push inbound doorbell and wait reply at hwinterrupt routine for next Qbuffer post
	*/
 	CHIP_REG_WRITE32(&pACB->pmu->inbound_doorbell,ARCMSR_INBOUND_DRIVER_DATA_WRITE_OK);
	splx(s);
	return;
}
/*
************************************************************************
**
**
************************************************************************
*/
VOID arcmsr_stop_adapter_bgrb(PACB pACB)
{
    #if ARCMSR_DEBUG0
    printf("arcmsr_stop_adapter_bgrb..............\n");
    #endif
	pACB->acb_flags |= ACB_F_MSG_STOP_BGRB;
	pACB->acb_flags &= ~ACB_F_MSG_START_BGRB;
	CHIP_REG_WRITE32(&pACB->pmu->inbound_msgaddr0,ARCMSR_INBOUND_MESG0_STOP_BGRB);
	return;
}
/*
************************************************************************
**  
**                  
************************************************************************
*/
static VOID arcmsr_poll(struct cam_sim * psim)
{
	arcmsr_interrupt(cam_sim_softc(psim));
	return;
}
/*
**********************************************************************
**   Function:  arcmsr_interrupt
**     Output:  VOID
**   CAM  Status field values   
**typedef enum {
**	CAM_REQ_INPROG,		   CCB request is in progress   
**	CAM_REQ_CMP,		   CCB request completed without error   
**	CAM_REQ_ABORTED,	   CCB request aborted by the host   
**	CAM_UA_ABORT,		   Unable to abort CCB request   
**	CAM_REQ_CMP_ERR,	   CCB request completed with an error   
**	CAM_BUSY,		       CAM subsytem is busy   
**	CAM_REQ_INVALID,	   CCB request was invalid   
**	CAM_PATH_INVALID,	   Supplied Path ID is invalid   
**	CAM_DEV_NOT_THERE,	   SCSI Device Not Installed/there   
**	CAM_UA_TERMIO,		   Unable to terminate I/O CCB request   
**	CAM_SEL_TIMEOUT,	   Target Selection Timeout   
**	CAM_CMD_TIMEOUT,	   Command timeout   
**	CAM_SCSI_STATUS_ERROR,	   SCSI error, look at error code in CCB   
**	CAM_MSG_REJECT_REC,	   Message Reject Received   
**	CAM_SCSI_BUS_RESET,	   SCSI Bus Reset Sent/Received   
**	CAM_UNCOR_PARITY,	   Uncorrectable parity error occurred   
**	CAM_AUTOSENSE_FAIL=0x10,   Autosense: request sense cmd fail   
**	CAM_NO_HBA,		   No HBA Detected error   
**	CAM_DATA_RUN_ERR,	   Data Overrun error   
**	CAM_UNEXP_BUSFREE,	   Unexpected Bus Free   
**	CAM_SEQUENCE_FAIL,	   Target Bus Phase Sequence Failure   
**	CAM_CCB_LEN_ERR,	   CCB length supplied is inadequate   
**	CAM_PROVIDE_FAIL,	   Unable to provide requested capability   
**	CAM_BDR_SENT,		   A SCSI BDR msg was sent to target   
**	CAM_REQ_TERMIO,		   CCB request terminated by the host   
**	CAM_UNREC_HBA_ERROR,	   Unrecoverable Host Bus Adapter Error   
**	CAM_REQ_TOO_BIG,	   The request was too large for this host   
**	CAM_REQUEUE_REQ,	  
**				 * This request should be requeued to preserve
**				 * transaction ordering.  This typically occurs
**				 * when the SIM recognizes an error that should
**				 * freeze the queue and must place additional
**				 * requests for the target at the sim level
**				 * back into the XPT queue.
**				   
**	CAM_IDE=0x33,		   Initiator Detected Error   
**	CAM_RESRC_UNAVAIL,	   Resource Unavailable   
**	CAM_UNACKED_EVENT,	   Unacknowledged Event by Host   
**	CAM_MESSAGE_RECV,	   Message Received in Host Target Mode   
**	CAM_INVALID_CDB,	   Invalid CDB received in Host Target Mode   
**	CAM_LUN_INVALID,	   Lun supplied is invalid   
**	CAM_TID_INVALID,	   Target ID supplied is invalid   
**	CAM_FUNC_NOTAVAIL,	   The requested function is not available   
**	CAM_NO_NEXUS,		   Nexus is not established   
**	CAM_IID_INVALID,	   The initiator ID is invalid   
**	CAM_CDB_RECVD,		   The SCSI CDB has been received   
**	CAM_LUN_ALRDY_ENA,	   The LUN is already eanbeld for target mode   
**	CAM_SCSI_BUSY,		   SCSI Bus Busy   
**
**	CAM_DEV_QFRZN=0x40,	   The DEV queue is frozen w/this err   
**
**				   Autosense data valid for target   
**	CAM_AUTOSNS_VALID=0x80,
**	CAM_RELEASE_SIMQ=0x100,   SIM ready to take more commands   
**	CAM_SIM_QUEUED  =0x200,   SIM has this command in it's queue   
**
**	CAM_STATUS_MASK=0x3F,	   Mask bits for just the status #   
**
**				   Target Specific Adjunct Status   
**	CAM_SENT_SENSE=0x40000000	   sent sense with status   
**} cam_status;
**********************************************************************
*/
static VOID arcmsr_interrupt(VOID *arg)
{
	PACB pACB=(PACB)arg;
	PSRB pSRB;
	ULONG flagpsrb,outbound_intstatus,outbound_doorbell;

    #if ARCMSR_DEBUG0
    printf("arcmsr_interrupt..............\n");
    #endif
	/*
	*********************************************
	**   check outbound intstatus 檢察有無郵差按門鈴
	*********************************************
	*/
	outbound_intstatus=CHIP_REG_READ32(&pACB->pmu->outbound_intstatus) & pACB->outbound_int_enable;
    CHIP_REG_WRITE32(&pACB->pmu->outbound_intstatus, outbound_intstatus);/*clear interrupt*/
	if(outbound_intstatus & ARCMSR_MU_OUTBOUND_DOORBELL_INT)
	{
		#if ARCMSR_DEBUG0
		printf("arcmsr_interrupt:..........ARCMSR_MU_OUTBOUND_DOORBELL_INT\n");
		#endif
		/*
		*********************************************
		**  DOORBELL 叮噹! 是否有郵件要簽收
		*********************************************
		*/
		outbound_doorbell=CHIP_REG_READ32(&pACB->pmu->outbound_doorbell);
		CHIP_REG_WRITE32(&pACB->pmu->outbound_doorbell,outbound_doorbell);/*clear interrupt */
		if(outbound_doorbell & ARCMSR_OUTBOUND_IOP331_DATA_WRITE_OK)
		{
			PQBUFFER prbuffer=(PQBUFFER)&pACB->pmu->ioctl_rbuffer;
			PUCHAR iop_data=(PUCHAR)prbuffer->data;
			PUCHAR pQbuffer;
			LONG my_empty_len,iop_len,rqbuf_firstindex,rqbuf_lastindex;
			ULONG s;
            /*check this iop data if overflow my rqbuffer*/
            s=splcam();
			rqbuf_lastindex=pACB->rqbuf_lastindex;
			rqbuf_firstindex=pACB->rqbuf_firstindex;
			iop_len=prbuffer->data_len;
            my_empty_len=(rqbuf_firstindex-rqbuf_lastindex-1)&(ARCMSR_MAX_QBUFFER-1);
			if(my_empty_len>=iop_len)
			{
				while(iop_len > 0)
				{
					pQbuffer=&pACB->rqbuffer[pACB->rqbuf_lastindex];
					memcpy(pQbuffer,iop_data,1);
					pACB->rqbuf_lastindex++;
					pACB->rqbuf_lastindex %= ARCMSR_MAX_QBUFFER;/*if last index number set it to 0 */
					iop_data++;
					iop_len--;
				}
				CHIP_REG_WRITE32(&pACB->pmu->inbound_doorbell, ARCMSR_INBOUND_DRIVER_DATA_READ_OK);/*signature, let IOP331 know data has been readed */
			}
			else
			{
				pACB->acb_flags|=ACB_F_IOPDATA_OVERFLOW;
			}
			splx(s);
		}
		if(outbound_doorbell & ARCMSR_OUTBOUND_IOP331_DATA_READ_OK)
		{
			ULONG s;
			/*
			*********************************************
			**           看看是否還有郵件要順道寄出
			*********************************************
			*/
			s=splcam();
			if(pACB->wqbuf_firstindex!=pACB->wqbuf_lastindex)
			{
				PUCHAR pQbuffer;
				PQBUFFER pwbuffer=(PQBUFFER)&pACB->pmu->ioctl_wbuffer;
				PUCHAR iop_data=(PUCHAR)pwbuffer->data;
				LONG allxfer_len=0;

				while((pACB->wqbuf_firstindex!=pACB->wqbuf_lastindex) && (allxfer_len<124))
				{
					pQbuffer=&pACB->wqbuffer[pACB->wqbuf_firstindex];
   					memcpy(iop_data,pQbuffer,1);
					pACB->wqbuf_firstindex++;
					pACB->wqbuf_firstindex %= ARCMSR_MAX_QBUFFER; /*if last index number set it to 0 */
					iop_data++;
					allxfer_len++;
				}
				pwbuffer->data_len=allxfer_len;
				/*
				** push inbound doorbell tell iop driver data write ok and wait reply on next hwinterrupt for next Qbuffer post
				*/
				CHIP_REG_WRITE32(&pACB->pmu->inbound_doorbell,ARCMSR_INBOUND_DRIVER_DATA_WRITE_OK);
 			}
			else
			{
				pACB->acb_flags |= ACB_F_IOCTL_WQBUFFER_CLEARED;
			}
			splx(s);
		}
	}
	if(outbound_intstatus & ARCMSR_MU_OUTBOUND_POSTQUEUE_INT)
	{
 		/*
		*****************************************************************************
		**               areca cdb command done
		*****************************************************************************
		*/
		while(1)
		{
			if((flagpsrb=CHIP_REG_READ32(&pACB->pmu->outbound_queueport)) == 0xFFFFFFFF)
			{
				break;/*chip FIFO no srb for completion already*/
			}
			/* check if command done with no error*/
			pSRB=(PSRB)(CINT2P)(pACB->vir2phy_offset+(flagpsrb << 5));/*frame must be 32 bytes aligned*/
			if((pSRB->pACB!=pACB) || (pSRB->startdone!=ARCMSR_SRB_START))
			{
				if(pSRB->startdone==ARCMSR_SRB_ABORTED)
				{
					pSRB->pccb->ccb_h.status=CAM_REQ_ABORTED;
					arcmsr_srb_complete(pSRB);
					break;
				}
  				printf("arcmsr_interrupt:got an illegal srb command done ...pACB=%p pSRB=%p srboutstandingcount=%d .....\n",pACB,pSRB,pACB->srboutstandingcount);
				break;
			}
			if((flagpsrb & ARCMSR_SRBREPLY_FLAG_ERROR)==0)
			{
				pSRB->pccb->ccb_h.status=CAM_REQ_CMP;
				arcmsr_srb_complete(pSRB);
			} 
			else 
			{   
				switch(pSRB->arcmsr_cdb.DeviceStatus)
				{
				case ARCMSR_DEV_SELECT_TIMEOUT:
					{
						#if ARCMSR_DEBUG0
						printf("pSRB=%p ......ARCMSR_DEV_SELECT_TIMEOUT\n",pSRB);
						#endif
 						pSRB->pccb->ccb_h.status=CAM_SEL_TIMEOUT;
						arcmsr_srb_complete(pSRB);
					}
					break;
				case ARCMSR_DEV_ABORTED:
					{
						#if ARCMSR_DEBUG0
						printf("pSRB=%p ......ARCMSR_DEV_ABORTED\n",pSRB);
						#endif
						pSRB->pccb->ccb_h.status=CAM_DEV_NOT_THERE;
						arcmsr_srb_complete(pSRB);
					}
					break;
				case ARCMSR_DEV_INIT_FAIL:
					{
						#if ARCMSR_DEBUG0
						printf("pSRB=%p .....ARCMSR_DEV_INIT_FAIL\n",pSRB);
						#endif
 						pSRB->pccb->ccb_h.status=CAM_DEV_NOT_THERE;
						arcmsr_srb_complete(pSRB);
					}
					break;
				case SCSISTAT_CHECK_CONDITION:
					{
						#if ARCMSR_DEBUG0
						printf("pSRB=%p .....SCSISTAT_CHECK_CONDITION\n",pSRB);
						#endif
                        arcmsr_report_SenseInfoBuffer(pSRB);
						arcmsr_srb_complete(pSRB);
					}
					break;
				default:
					/* error occur Q all error srb to errorsrbpending Q*/
 					printf("arcmsr_interrupt:command error done ......but got unknow DeviceStatus=%x....\n",pSRB->arcmsr_cdb.DeviceStatus);
					pSRB->pccb->ccb_h.status=CAM_UNCOR_PARITY;/*unknow error or crc error just for retry*/
					arcmsr_srb_complete(pSRB);
					break;
				}
			}
		}	/*drain reply FIFO*/
	}
	if(pACB->srbwait2gocount != 0)
	{
    	arcmsr_post_wait2go_srb(pACB);/*try to post all pending srb*/
 	}
	return;
}
/*
***********************************************************************
**
**int	copyin __P((const void *udaddr, void *kaddr, size_t len));
**int	copyout __P((const void *kaddr, void *udaddr, size_t len));
**
**ENOENT     "" No such file or directory ""
**ENOIOCTL   "" ioctl not handled by this layer ""
**ENOMEM     "" Cannot allocate memory ""
**EINVAL     "" Invalid argument ""
************************************************************************
*/
LONG arcmsr_iop_ioctlcmd(PACB pACB,ULONG ioctl_cmd,caddr_t arg)
{
	PCMD_IO_CONTROL pccbioctl=(PCMD_IO_CONTROL) arg;

 	#if ARCMSR_DEBUG0
	printf("arcmsr_iop_ioctlcmd................. \n");
	#endif

	if(memcmp(pccbioctl->Signature,"ARCMSR",6)!=0)
    {
        return EINVAL;
	}
	switch(ioctl_cmd)
	{
	case ARCMSR_IOCTL_READ_RQBUFFER:
		{
			ULONG s;			
			PCMD_IOCTL_FIELD pccbioctlfld=(PCMD_IOCTL_FIELD)arg;
			PUCHAR pQbuffer,ptmpQbuffer=pccbioctlfld->ioctldatabuffer;			
			LONG allxfer_len=0;
     
            s=splcam();
			while((pACB->rqbuf_firstindex!=pACB->rqbuf_lastindex) && (allxfer_len<1031))
			{
				/*copy READ QBUFFER to srb*/
                pQbuffer=&pACB->rqbuffer[pACB->rqbuf_firstindex];
				memcpy(ptmpQbuffer,pQbuffer,1);
				pACB->rqbuf_firstindex++;
				pACB->rqbuf_firstindex %= ARCMSR_MAX_QBUFFER; /*if last index number set it to 0 */
				ptmpQbuffer++;
				allxfer_len++;
			}
			if(pACB->acb_flags & ACB_F_IOPDATA_OVERFLOW)
			{
                PQBUFFER prbuffer=(PQBUFFER)&pACB->pmu->ioctl_rbuffer;
                PUCHAR pQbuffer;
				PUCHAR iop_data=(PUCHAR)prbuffer->data;
                LONG iop_len;

                pACB->acb_flags &= ~ACB_F_IOPDATA_OVERFLOW;
			    iop_len=(LONG)prbuffer->data_len;
				/*this iop data does no chance to make me overflow again here, so just do it*/
				while(iop_len>0)
				{
                    pQbuffer=&pACB->rqbuffer[pACB->rqbuf_lastindex];
					memcpy(pQbuffer,iop_data,1);
					pACB->rqbuf_lastindex++;
					pACB->rqbuf_lastindex %= ARCMSR_MAX_QBUFFER;/*if last index number set it to 0 */
					iop_data++;
					iop_len--;
				}
				CHIP_REG_WRITE32(&pACB->pmu->inbound_doorbell, ARCMSR_INBOUND_DRIVER_DATA_READ_OK);/*signature, let IOP331 know data has been readed */
			}
			pccbioctl->Length=allxfer_len;
			pccbioctl->ReturnCode=ARCMSR_IOCTL_RETURNCODE_OK;
			splx(s);
			return ARC_IOCTL_SUCCESS;
 		}
		break;
	case ARCMSR_IOCTL_WRITE_WQBUFFER:
		{
			ULONG s;
            PCMD_IOCTL_FIELD pccbioctlfld=(PCMD_IOCTL_FIELD)arg;
			LONG my_empty_len,user_len,wqbuf_firstindex,wqbuf_lastindex;
			PUCHAR pQbuffer,ptmpuserbuffer=pccbioctlfld->ioctldatabuffer;

            s=splcam();
            user_len=pccbioctl->Length;
              
 			/*check if data xfer length of this request will overflow my array qbuffer */
			wqbuf_lastindex=pACB->wqbuf_lastindex;
			wqbuf_firstindex=pACB->wqbuf_firstindex;
			my_empty_len=(wqbuf_firstindex-wqbuf_lastindex-1)&(ARCMSR_MAX_QBUFFER-1);
			if(my_empty_len>=user_len)
			{
				while(user_len>0)
				{
					/*copy srb data to wqbuffer*/
					pQbuffer=&pACB->wqbuffer[pACB->wqbuf_lastindex];
					memcpy(pQbuffer,ptmpuserbuffer,1);
					pACB->wqbuf_lastindex++;
					pACB->wqbuf_lastindex %= ARCMSR_MAX_QBUFFER;/*if last index number set it to 0 */
 					ptmpuserbuffer++;
					user_len--;
				}
				/*post fist Qbuffer*/
				if(pACB->acb_flags & ACB_F_IOCTL_WQBUFFER_CLEARED)
				{
					pACB->acb_flags &=~ACB_F_IOCTL_WQBUFFER_CLEARED;
 					arcmsr_post_Qbuffer(pACB);
				}
				pccbioctl->ReturnCode=ARCMSR_IOCTL_RETURNCODE_OK;
			}
			else
			{
				pccbioctl->ReturnCode=ARCMSR_IOCTL_RETURNCODE_ERROR;
			}
			splx(s);
			return ARC_IOCTL_SUCCESS;
		}
		break;
	case ARCMSR_IOCTL_CLEAR_RQBUFFER:
		{
			ULONG s;
			PUCHAR pQbuffer=pACB->rqbuffer;
            s=splcam();
			if(pACB->acb_flags & ACB_F_IOPDATA_OVERFLOW)
			{
                pACB->acb_flags &= ~ACB_F_IOPDATA_OVERFLOW;
                CHIP_REG_WRITE32(&pACB->pmu->inbound_doorbell, ARCMSR_INBOUND_DRIVER_DATA_READ_OK);/*signature, let IOP331 know data has been readed */
			}
            pACB->acb_flags |= ACB_F_IOCTL_RQBUFFER_CLEARED;
			pACB->rqbuf_firstindex=0;
			pACB->rqbuf_lastindex=0;
            memset(pQbuffer, 0, ARCMSR_MAX_QBUFFER);
			pccbioctl->ReturnCode=ARCMSR_IOCTL_RETURNCODE_OK;
			splx(s);
			return ARC_IOCTL_SUCCESS;
		}
		break;
	case ARCMSR_IOCTL_CLEAR_WQBUFFER:
		{
			ULONG s;
			PUCHAR pQbuffer=pACB->wqbuffer;
 
            s=splcam();
			if(pACB->acb_flags & ACB_F_IOPDATA_OVERFLOW)
			{
                pACB->acb_flags &= ~ACB_F_IOPDATA_OVERFLOW;
                CHIP_REG_WRITE32(&pACB->pmu->inbound_doorbell, ARCMSR_INBOUND_DRIVER_DATA_READ_OK);/*signature, let IOP331 know data has been readed */
			}
			pACB->acb_flags |= ACB_F_IOCTL_WQBUFFER_CLEARED;
			pACB->wqbuf_firstindex=0;
			pACB->wqbuf_lastindex=0;
            memset(pQbuffer, 0, ARCMSR_MAX_QBUFFER);
			pccbioctl->ReturnCode=ARCMSR_IOCTL_RETURNCODE_OK;
			splx(s);
			return ARC_IOCTL_SUCCESS;
		}
		break;
	case ARCMSR_IOCTL_CLEAR_ALLQBUFFER:
		{
			ULONG s;
			PUCHAR pQbuffer;
 
            s=splcam();
			if(pACB->acb_flags & ACB_F_IOPDATA_OVERFLOW)
			{
                pACB->acb_flags &= ~ACB_F_IOPDATA_OVERFLOW;
                CHIP_REG_WRITE32(&pACB->pmu->inbound_doorbell, ARCMSR_INBOUND_DRIVER_DATA_READ_OK);/*signature, let IOP331 know data has been readed */
			}
			pACB->acb_flags |= (ACB_F_IOCTL_WQBUFFER_CLEARED|ACB_F_IOCTL_RQBUFFER_CLEARED);
			pACB->rqbuf_firstindex=0;
			pACB->rqbuf_lastindex=0;
			pACB->wqbuf_firstindex=0;
			pACB->wqbuf_lastindex=0;
			pQbuffer=pACB->rqbuffer;
            memset(pQbuffer, 0, sizeof(struct _QBUFFER));
			pQbuffer=pACB->wqbuffer;
            memset(pQbuffer, 0, sizeof(struct _QBUFFER));
			pccbioctl->ReturnCode=ARCMSR_IOCTL_RETURNCODE_OK;
			splx(s);
			return ARC_IOCTL_SUCCESS;
		}
		break;
	case ARCMSR_IOCTL_RETURN_CODE_3F:
		{
			pccbioctl->ReturnCode=ARCMSR_IOCTL_RETURNCODE_3F;
			return ARC_IOCTL_SUCCESS;
		}
		break;
	case ARCMSR_IOCTL_SAY_HELLO:
		{
			PCMD_IOCTL_FIELD pccbioctlfld=(PCMD_IOCTL_FIELD)arg;
			PCHAR hello_string="Hello! I am ARCMSR";
			PCHAR puserbuffer=(PUCHAR)pccbioctlfld->ioctldatabuffer;
  
			if(memcpy(puserbuffer,hello_string,(SHORT)strlen(hello_string)))
			{
				pccbioctl->ReturnCode=ARCMSR_IOCTL_RETURNCODE_ERROR;
                return ENOIOCTL;
			}
            pccbioctl->ReturnCode=ARCMSR_IOCTL_RETURNCODE_OK;
		    return ARC_IOCTL_SUCCESS;
		}
		break;
	}
    return EINVAL;
}
/*
**************************************************************************
**
**************************************************************************
*/
PSRB arcmsr_get_freesrb(PACB pACB)
{
    PSRB pSRB=NULL;
  	ULONG s;
	LONG srb_startindex,srb_doneindex;

    #if ARCMSR_DEBUG0
	printf("arcmsr_get_freesrb: srb_startindex=%d srb_doneindex=%d\n",pACB->srb_startindex,pACB->srb_doneindex);
    #endif

	s=splcam();
	srb_doneindex=pACB->srb_doneindex;
	srb_startindex=pACB->srb_startindex;
	pSRB=pACB->psrbringQ[srb_startindex];
	srb_startindex++;
	srb_startindex %= ARCMSR_MAX_FREESRB_NUM;
	if(srb_doneindex!=srb_startindex)
	{
  		pACB->srb_startindex=srb_startindex;
  	}
	else
	{
        pSRB=NULL;
	}
	splx(s);
	return(pSRB);
}
/*
*********************************************************************
**
**
**
*********************************************************************
*/
static VOID arcmsr_executesrb(VOID *arg,bus_dma_segment_t *dm_segs,LONG nseg,LONG error)
{
	PSRB	  pSRB=(PSRB)arg;
    PACB      pACB;
	union ccb *pccb;

    #if ARCMSR_DEBUG0
    printf("arcmsr_executesrb........................................ \n" );
    #endif

	pccb=pSRB->pccb;
   	pACB=(PACB)pSRB->pACB;
	if(error != 0) 
	{
		if(error != EFBIG)
		{
			printf("arcmsr_executesrb:%d Unexepected error %x returned from "  "bus_dmamap_load\n",pACB->pci_unit,error);
		}
		if(pccb->ccb_h.status == CAM_REQ_INPROG) 
		{
			xpt_freeze_devq(pccb->ccb_h.path,/*count*/1);
			pccb->ccb_h.status=CAM_REQ_TOO_BIG|CAM_DEV_QFRZN;
		}
		xpt_done(pccb);
		return;
	}
    arcmsr_build_srb(pSRB,dm_segs,nseg);
	if(pccb->ccb_h.status != CAM_REQ_INPROG)
	{
		if(nseg != 0)
		{
			bus_dmamap_unload(pACB->buffer_dmat,pSRB->dmamap);
		}
		xpt_done(pccb);
		return;
	}
	pccb->ccb_h.status |= CAM_SIM_QUEUED;
	if(pACB->srboutstandingcount < ARCMSR_MAX_OUTSTANDING_CMD)
	{   
		/*
		******************************************************************
		** and we can make sure there were no pending srb in this duration
		******************************************************************
		*/
    	arcmsr_post_srb(pACB,pSRB);
	}
	else
	{
		/*
		******************************************************************
		** Q of srbwaitexec will be post out when any outstanding command complete
		******************************************************************
		*/
		arcmsr_queue_wait2go_srb(pACB,pSRB);
	}
	return;
}
/*
*****************************************************************************************
**
*****************************************************************************************
*/
BOOLEAN arcmsr_seek_cmd2abort(union ccb * pabortccb)
{
 	PSRB pSRB,pfreesrb;
    PACB pACB=(PACB) pabortccb->ccb_h.arcmsr_ccbacb_ptr;
	ULONG s,intmask_org,mask;
    LONG i=0;

    #if ARCMSR_DEBUG0
    printf("arcmsr_seek_cmd2abort.................. \n");
    #endif

	s=splcam();
	/* 
	** It is the upper layer do abort command this lock just prior to calling us.
	** First determine if we currently own this command.
	** Start by searching the device queue. If not found
	** at all,and the system wanted us to just abort the
	** command return success.
	*/
	if(pACB->srboutstandingcount!=0)
	{
		/* Q back all outstanding srb into wait exec psrb Q*/
		pfreesrb=pACB->pfreesrb;
		for(i=0;i<ARCMSR_MAX_FREESRB_NUM;i++)
		{
	    	pSRB=&pfreesrb[i];
			if(pSRB->startdone==ARCMSR_SRB_START)
			{
				if(pSRB->pccb==pabortccb)
				{
					/* disable all outbound interrupt */
					intmask_org=CHIP_REG_READ32(&pACB->pmu->outbound_intmask);
					CHIP_REG_WRITE32(&pACB->pmu->outbound_intmask,intmask_org|ARCMSR_MU_OUTBOUND_ALL_INTMASKENABLE);
				    /* talk to iop 331 outstanding command aborted*/
					arcmsr_abort_allcmd(pACB);
					if(arcmsr_wait_msgint_ready(pACB)!=TRUE)
					{
						printf("arcmsr_seek_cmd2abort: wait 'abort all outstanding command' timeout.................in \n");
					}
					/*clear all outbound posted Q*/
					for(i=0;i<ARCMSR_MAX_OUTSTANDING_CMD;i++)
					{
						CHIP_REG_READ32(&pACB->pmu->outbound_queueport);
					}
					pfreesrb=pACB->pfreesrb;
					for(i=0;i<ARCMSR_MAX_FREESRB_NUM;i++)
					{
	    				pSRB=&pfreesrb[i];
						if(pSRB->startdone==ARCMSR_SRB_START)
						{
							pSRB->startdone=ARCMSR_SRB_ABORTED;
							pSRB->pccb->ccb_h.status=CAM_REQ_ABORTED;
							arcmsr_srb_complete(pSRB);
						}
					}
    			    /* enable all outbound interrupt */
			        mask=~(ARCMSR_MU_OUTBOUND_POSTQUEUE_INTMASKENABLE|ARCMSR_MU_OUTBOUND_DOORBELL_INTMASKENABLE|ARCMSR_MU_OUTBOUND_MESSAGE0_INTMASKENABLE);
                    CHIP_REG_WRITE32(&pACB->pmu->outbound_intmask,intmask_org & mask);
					splx(s);
					return(TRUE);
				}
			}
		}
	}
	/*
	** seek this command at our command list 
	** if command found then remove,abort it and free this SRB
	*/
	if(pACB->srbwait2gocount!=0)
	{
		for(i=0;i<ARCMSR_MAX_OUTSTANDING_CMD;i++)
		{
			pSRB=pACB->psrbwait2go[i];
			if(pSRB!=NULL)
			{
				if(pSRB->pccb==pabortccb)
				{
					pACB->psrbwait2go[i]=NULL;
					pSRB->startdone=ARCMSR_SRB_ABORTED;
					pSRB->pccb->ccb_h.status=CAM_REQ_ABORTED; 
					arcmsr_srb_complete(pSRB);
	    			atomic_subtract_int(&pACB->srbwait2gocount,1);
                    splx(s);
					return(TRUE);
				}
			}
		}
	}
 	splx(s);
	return (FALSE);
}
/*
****************************************************************************
** 
****************************************************************************
*/
VOID arcmsr_bus_reset(PACB pACB)
{
	#if ARCMSR_DEBUG0
	printf("arcmsr_bus_reset.......................... \n");
	#endif

	arcmsr_iop_reset(pACB);
 	return;
} 
/*
*********************************************************************
**
**   CAM  Status field values   
**typedef enum {
**	CAM_REQ_INPROG,		   CCB request is in progress   
**	CAM_REQ_CMP,		   CCB request completed without error   
**	CAM_REQ_ABORTED,	   CCB request aborted by the host   
**	CAM_UA_ABORT,		   Unable to abort CCB request   
**	CAM_REQ_CMP_ERR,	   CCB request completed with an error   
**	CAM_BUSY,		       CAM subsytem is busy   
**	CAM_REQ_INVALID,	   CCB request was invalid   
**	CAM_PATH_INVALID,	   Supplied Path ID is invalid   
**	CAM_DEV_NOT_THERE,	   SCSI Device Not Installed/there   
**	CAM_UA_TERMIO,		   Unable to terminate I/O CCB request   
**	CAM_SEL_TIMEOUT,	   Target Selection Timeout   
**	CAM_CMD_TIMEOUT,	   Command timeout   
**	CAM_SCSI_STATUS_ERROR,	   SCSI error, look at error code in CCB   
**	CAM_MSG_REJECT_REC,	   Message Reject Received   
**	CAM_SCSI_BUS_RESET,	   SCSI Bus Reset Sent/Received   
**	CAM_UNCOR_PARITY,	   Uncorrectable parity error occurred   
**	CAM_AUTOSENSE_FAIL=0x10,   Autosense: request sense cmd fail   
**	CAM_NO_HBA,		   No HBA Detected error   
**	CAM_DATA_RUN_ERR,	   Data Overrun error   
**	CAM_UNEXP_BUSFREE,	   Unexpected Bus Free   
**	CAM_SEQUENCE_FAIL,	   Target Bus Phase Sequence Failure   
**	CAM_CCB_LEN_ERR,	   CCB length supplied is inadequate   
**	CAM_PROVIDE_FAIL,	   Unable to provide requested capability   
**	CAM_BDR_SENT,		   A SCSI BDR msg was sent to target   
**	CAM_REQ_TERMIO,		   CCB request terminated by the host   
**	CAM_UNREC_HBA_ERROR,	   Unrecoverable Host Bus Adapter Error   
**	CAM_REQ_TOO_BIG,	   The request was too large for this host   
**	CAM_REQUEUE_REQ,	  
**				 * This request should be requeued to preserve
**				 * transaction ordering.  This typically occurs
**				 * when the SIM recognizes an error that should
**				 * freeze the queue and must place additional
**				 * requests for the target at the sim level
**				 * back into the XPT queue.
**				   
**	CAM_IDE=0x33,		   Initiator Detected Error   
**	CAM_RESRC_UNAVAIL,	   Resource Unavailable   
**	CAM_UNACKED_EVENT,	   Unacknowledged Event by Host   
**	CAM_MESSAGE_RECV,	   Message Received in Host Target Mode   
**	CAM_INVALID_CDB,	   Invalid CDB received in Host Target Mode   
**	CAM_LUN_INVALID,	   Lun supplied is invalid   
**	CAM_TID_INVALID,	   Target ID supplied is invalid   
**	CAM_FUNC_NOTAVAIL,	   The requested function is not available   
**	CAM_NO_NEXUS,		   Nexus is not established   
**	CAM_IID_INVALID,	   The initiator ID is invalid   
**	CAM_CDB_RECVD,		   The SCSI CDB has been received   
**	CAM_LUN_ALRDY_ENA,	   The LUN is already eanbeld for target mode   
**	CAM_SCSI_BUSY,		   SCSI Bus Busy   
**
**	CAM_DEV_QFRZN=0x40,	   The DEV queue is frozen w/this err   
**
**				   Autosense data valid for target   
**	CAM_AUTOSNS_VALID=0x80,
**	CAM_RELEASE_SIMQ=0x100,   SIM ready to take more commands   
**	CAM_SIM_QUEUED  =0x200,   SIM has this command in it's queue   
**
**	CAM_STATUS_MASK=0x3F,	   Mask bits for just the status #   
**
**				   Target Specific Adjunct Status   
**	CAM_SENT_SENSE=0x40000000	   sent sense with status   
**} cam_status;
**
**union ccb {
**			struct	ccb_hdr			ccb_h;	 For convenience 
**			struct	ccb_scsiio		csio;
**			struct	ccb_getdev		cgd;
**			struct	ccb_getdevlist		cgdl;
**			struct	ccb_pathinq		cpi;
**			struct	ccb_relsim		crs;
**			struct	ccb_setasync		csa;
**			struct	ccb_setdev		csd;
**			struct	ccb_pathstats		cpis;
**			struct	ccb_getdevstats		cgds;
**			struct	ccb_dev_match		cdm;
**			struct	ccb_trans_settings	cts;
**			struct	ccb_calc_geometry	ccg;	
**			struct	ccb_abort		cab;
**			struct	ccb_resetbus		crb;
**			struct	ccb_resetdev		crd;
**			struct	ccb_termio		tio;
**			struct	ccb_accept_tio		atio;
**			struct	ccb_scsiio		ctio;
**			struct	ccb_en_lun		cel;
**			struct	ccb_immed_notify	cin;
**			struct	ccb_notify_ack		cna;
**			struct	ccb_eng_inq		cei;
**			struct	ccb_eng_exec		cee;
**			struct 	ccb_rescan		crcn;
**			struct  ccb_debug		cdbg;
**          }
**
**struct ccb_hdr {
**	cam_pinfo	    pinfo;	                                    "" Info for priority scheduling 
**	camq_entry	    xpt_links;	                                "" For chaining in the XPT layer 	
**	camq_entry	    sim_links;	                                "" For chaining in the SIM layer 	
**	camq_entry	    periph_links;                               "" For chaining in the type driver 
**	u_int32_t	    retry_count;
**	void		    (*cbfcnp)(struct cam_periph *, union ccb *);"" Callback on completion function 
**	xpt_opcode	    func_code;	                                "" XPT function code 
**	u_int32_t	    status;	                                    "" Status returned by CAM subsystem 
**	struct		    cam_path *path;                             "" Compiled path for this ccb 
**	path_id_t	    path_id;	                                "" Path ID for the request 
**	target_id_t	    target_id;	                                "" Target device ID 
**	lun_id_t	    target_lun;                              	"" Target LUN number 
**	u_int32_t	    flags;
**	ccb_ppriv_area	periph_priv;
**	ccb_spriv_area	sim_priv;
**	u_int32_t	    timeout;	                                "" Timeout value 
**	struct		    callout_handle timeout_ch;                  "" Callout handle used for timeouts 
**};
**
**typedef union {
**	u_int8_t  *cdb_ptr;		               "" Pointer to the CDB bytes to send 
**	u_int8_t  cdb_bytes[IOCDBLEN];         "" Area for the CDB send 
**} cdb_t;
**
** SCSI I/O Request CCB used for the XPT_SCSI_IO and XPT_CONT_TARGET_IO
** function codes.
**
**struct ccb_scsiio {
**	struct	   ccb_hdr ccb_h;
**	union	   ccb *next_ccb;	           "" Ptr for next CCB for action 
**	u_int8_t   *req_map;		           "" Ptr to mapping info 
**	u_int8_t   *data_ptr;		           "" Ptr to the data buf/SG list 
**	u_int32_t  dxfer_len;		           "" Data transfer length 
**	struct     scsi_sense_data sense_data; "" Autosense storage
**	u_int8_t   sense_len;		           "" Number of bytes to autosense
**	u_int8_t   cdb_len;		               "" Number of bytes for the CDB 
**	u_int16_t  sglist_cnt;		           "" Number of SG list entries
**	u_int8_t   scsi_status;		           "" Returned SCSI status 
**	u_int8_t   sense_resid;		           "" Autosense resid length: 2's comp 
**	u_int32_t  resid;		               "" Transfer residual length: 2's comp
**	cdb_t	   cdb_io;		               "" Union for CDB bytes/pointer 
**	u_int8_t   *msg_ptr;		           "" Pointer to the message buffer
**	u_int16_t  msg_len;		               "" Number of bytes for the Message 
**	u_int8_t   tag_action;		           "" What to do for tag queueing 
**#define	CAM_TAG_ACTION_NONE	0x00       "" The tag action should be either the define below (to send a non-tagged transaction) or one of the defined scsi tag messages from scsi_message.h.
**	u_int	   tag_id;		               "" tag id from initator (target mode) 
**	u_int	   init_id;		               "" initiator id of who selected
**}
*********************************************************************
*/
static VOID arcmsr_action(struct cam_sim * psim,union ccb * pccb)
{
	PACB  pACB;

	#if ARCMSR_DEBUG0
    printf("arcmsr_action ..................................\n" );
    #endif

	pACB=(PACB) cam_sim_softc(psim);
	if(pACB==NULL)
	{
    	pccb->ccb_h.status=CAM_REQ_INVALID;
		xpt_done(pccb);
		return;
	}
	switch (pccb->ccb_h.func_code) 
	{
	case XPT_SCSI_IO:
		{
	    	PSRB pSRB;
			#if ARCMSR_DEBUG0
			printf("arcmsr_action: XPT_SCSI_IO......................\n" );
			#endif

			if((pSRB=arcmsr_get_freesrb(pACB)) == NULL) 
			{
				pccb->ccb_h.status=CAM_RESRC_UNAVAIL;
				xpt_done(pccb);
				return;
			}
			pccb->ccb_h.arcmsr_ccbsrb_ptr=pSRB;
			pccb->ccb_h.arcmsr_ccbacb_ptr=pACB;
			pSRB->pccb=pccb;
			if((pccb->ccb_h.flags & CAM_DIR_MASK) != CAM_DIR_NONE) 
			{
				if((pccb->ccb_h.flags & CAM_SCATTER_VALID) == 0) 
				{
					if((pccb->ccb_h.flags & CAM_DATA_PHYS) == 0) 
					{
						LONG error,s;

						s=splsoftvm();
						error =	bus_dmamap_load(pACB->buffer_dmat,pSRB->dmamap,pccb->csio.data_ptr,pccb->csio.dxfer_len,arcmsr_executesrb,pSRB,/*flags*/0);
	         			if(error == EINPROGRESS)
						{
							xpt_freeze_simq(pACB->psim,1);
							pccb->ccb_h.status |= CAM_RELEASE_SIMQ;
						}
						splx(s);
					} 
					else 
					{
						panic("arcmsr: CAM_DATA_PHYS not supported");
					}
				} 
				else 
				{
					struct bus_dma_segment *segs;

					if((pccb->ccb_h.flags & CAM_SG_LIST_PHYS) == 0 || (pccb->ccb_h.flags & CAM_DATA_PHYS) != 0) 
					{
						pccb->ccb_h.status=CAM_PROVIDE_FAIL;
						xpt_done(pccb);
						free(pSRB,M_DEVBUF);
						return;
					}
					segs=(struct bus_dma_segment *)pccb->csio.data_ptr;
					arcmsr_executesrb(pSRB,segs,pccb->csio.sglist_cnt,0);
				}
			} 
			else
			{
				arcmsr_executesrb(pSRB,NULL,0,0);
			}
			break;
		}
	case XPT_TARGET_IO:	
		{
			#if ARCMSR_DEBUG0
			printf("arcmsr_action: XPT_TARGET_IO\n" );
			#endif
			/*
			** target mode not yet support vendor specific commands.
			*/
  			pccb->ccb_h.status=CAM_REQ_CMP;
			xpt_done(pccb);
			break;
		}
	case XPT_PATH_INQ:
		{
			struct ccb_pathinq *cpi=&pccb->cpi;

			#if ARCMSR_DEBUG0
			printf("arcmsr_action: XPT_PATH_INQ\n" );
			#endif
			cpi->version_num=1;
			cpi->hba_inquiry=PI_SDTR_ABLE | PI_TAG_ABLE;
			cpi->target_sprt=0;
			cpi->hba_misc=0;
			cpi->hba_eng_cnt=0;
			cpi->max_target=ARCMSR_MAX_TARGETID;
			cpi->max_lun=ARCMSR_MAX_TARGETLUN;	/* 7 or 0 */
			cpi->initiator_id=ARCMSR_SCSI_INITIATOR_ID;
			cpi->bus_id=cam_sim_bus(psim);
			strncpy(cpi->sim_vid,"FreeBSD",SIM_IDLEN);
			strncpy(cpi->hba_vid,"ARCMSR",HBA_IDLEN);
			strncpy(cpi->dev_name,cam_sim_name(psim),DEV_IDLEN);
			cpi->unit_number=cam_sim_unit(psim);
			cpi->ccb_h.status=CAM_REQ_CMP;
			xpt_done(pccb);
			break;
		}
	case XPT_ABORT: 
		{
			union ccb *pabort_ccb;

			#if ARCMSR_DEBUG0
			printf("arcmsr_action: XPT_ABORT\n" );
			#endif
			pabort_ccb=pccb->cab.abort_ccb;
			switch (pabort_ccb->ccb_h.func_code) 
			{
			case XPT_ACCEPT_TARGET_IO:
			case XPT_IMMED_NOTIFY:
			case XPT_CONT_TARGET_IO:
				if(arcmsr_seek_cmd2abort(pabort_ccb)==TRUE) 
				{
					pabort_ccb->ccb_h.status=CAM_REQ_ABORTED;
					xpt_done(pabort_ccb);
					pccb->ccb_h.status=CAM_REQ_CMP;
				} 
				else 
				{
					xpt_print_path(pabort_ccb->ccb_h.path);
					printf("Not found\n");
					pccb->ccb_h.status=CAM_PATH_INVALID;
				}
				break;
			case XPT_SCSI_IO:
				pccb->ccb_h.status=CAM_UA_ABORT;
				break;
			default:
				pccb->ccb_h.status=CAM_REQ_INVALID;
				break;
			}
			xpt_done(pccb);
			break;
		}
	case XPT_RESET_BUS:
	case XPT_RESET_DEV:
		{
			LONG     i;

			#if ARCMSR_DEBUG0
			printf("arcmsr_action: XPT_RESET_BUS\n" );
			#endif
            arcmsr_bus_reset(pACB);
			for (i=0; i < 500; i++)
			{
				DELAY(1000);	
			}
			pccb->ccb_h.status=CAM_REQ_CMP;
			xpt_done(pccb);
			break;
		}
	case XPT_TERM_IO:
		{
			#if ARCMSR_DEBUG0
			printf("arcmsr_action: XPT_TERM_IO\n" );
			#endif
			pccb->ccb_h.status=CAM_REQ_INVALID;
			xpt_done(pccb);
			break;
		}
	case XPT_GET_TRAN_SETTINGS:
		{
			struct ccb_trans_settings *cts;
			ULONG s;

			#if ARCMSR_DEBUG0
			printf("arcmsr_action: XPT_GET_TRAN_SETTINGS\n" );
			#endif

			cts=&pccb->cts;
			s=splcam();
			cts->flags=(CCB_TRANS_DISC_ENB | CCB_TRANS_TAG_ENB);
			cts->sync_period=3;
			cts->sync_offset=32;
			cts->bus_width=MSG_EXT_WDTR_BUS_16_BIT;
            cts->valid=CCB_TRANS_SYNC_RATE_VALID | CCB_TRANS_SYNC_OFFSET_VALID | CCB_TRANS_BUS_WIDTH_VALID | CCB_TRANS_DISC_VALID | CCB_TRANS_TQ_VALID;
			splx(s);
			pccb->ccb_h.status=CAM_REQ_CMP;
			xpt_done(pccb);
			break;
		}
	case XPT_SET_TRAN_SETTINGS:
		{
			#if ARCMSR_DEBUG0
			printf("arcmsr_action: XPT_SET_TRAN_SETTINGS\n" );
			#endif
		    pccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
		    xpt_done(pccb);
			break;
		}
	case XPT_CALC_GEOMETRY:
		{
			struct ccb_calc_geometry *ccg;
			ULONG size_mb;
			ULONG secs_per_cylinder;

			#if ARCMSR_DEBUG0
			printf("arcmsr_action: XPT_CALC_GEOMETRY\n" );
			#endif
			ccg=&pccb->ccg;
			size_mb=ccg->volume_size/((1024L * 1024L)/ccg->block_size);
			if(size_mb > 1024 ) 
			{
				ccg->heads=255;
				ccg->secs_per_track=63;
			} 
			else 
			{
				ccg->heads=64;
				ccg->secs_per_track=32;
			}
			secs_per_cylinder=ccg->heads * ccg->secs_per_track;
			ccg->cylinders=ccg->volume_size / secs_per_cylinder;
			pccb->ccb_h.status=CAM_REQ_CMP;
			xpt_done(pccb);
			break;
		}
	default:
		#if ARCMSR_DEBUG0
			printf("arcmsr_action: invalid XPT function CAM_REQ_INVALID\n" );
			#endif
    	pccb->ccb_h.status=CAM_REQ_INVALID;
		xpt_done(pccb);
		break;
	}
	return;
}
/*
**********************************************************************
** 
**  start background rebulid
**
**********************************************************************
*/
VOID arcmsr_start_adapter_bgrb(PACB pACB)
{
	#if ARCMSR_DEBUG0
	printf("arcmsr_start_adapter_bgrb.................................. \n");
	#endif
	pACB->acb_flags |= ACB_F_MSG_START_BGRB;
	pACB->acb_flags &= ~ACB_F_MSG_STOP_BGRB;
    CHIP_REG_WRITE32(&pACB->pmu->inbound_msgaddr0,ARCMSR_INBOUND_MESG0_START_BGRB);
	return;
}
/*
**********************************************************************
** 
**  start background rebulid
**
**********************************************************************
*/
VOID arcmsr_iop_init(PACB pACB)
{
    ULONG intmask_org,mask,outbound_doorbell,firmware_state=0;

	#if ARCMSR_DEBUG0
	printf("arcmsr_iop_init.................................. \n");
	#endif
	do
	{
        firmware_state=CHIP_REG_READ32(&pACB->pmu->outbound_msgaddr1);
	}while((firmware_state & ARCMSR_OUTBOUND_MESG1_FIRMWARE_OK)==0);
    /* disable all outbound interrupt */
    intmask_org=CHIP_REG_READ32(&pACB->pmu->outbound_intmask);
    CHIP_REG_WRITE32(&pACB->pmu->outbound_intmask,intmask_org|ARCMSR_MU_OUTBOUND_ALL_INTMASKENABLE);
	/*start background rebuild*/
	arcmsr_start_adapter_bgrb(pACB);
	if(arcmsr_wait_msgint_ready(pACB)!=TRUE)
	{
		printf("arcmsr_HwInitialize: wait 'start adapter background rebulid' timeout................. \n");
	}
	/* clear Qbuffer if door bell ringed */
	outbound_doorbell=CHIP_REG_READ32(&pACB->pmu->outbound_doorbell);
	if(outbound_doorbell & ARCMSR_OUTBOUND_IOP331_DATA_WRITE_OK)
	{
		CHIP_REG_WRITE32(&pACB->pmu->outbound_doorbell,outbound_doorbell);/*clear interrupt */
        CHIP_REG_WRITE32(&pACB->pmu->inbound_doorbell,ARCMSR_INBOUND_DRIVER_DATA_READ_OK);
	}
	/* enable outbound Post Queue,outbound message0,outbell doorbell Interrupt */
	mask=~(ARCMSR_MU_OUTBOUND_POSTQUEUE_INTMASKENABLE|ARCMSR_MU_OUTBOUND_DOORBELL_INTMASKENABLE|ARCMSR_MU_OUTBOUND_MESSAGE0_INTMASKENABLE);
    CHIP_REG_WRITE32(&pACB->pmu->outbound_intmask,intmask_org & mask);
	pACB->outbound_int_enable = ~(intmask_org & mask) & 0x000000ff;
	pACB->acb_flags |=ACB_F_IOP_INITED;
	return;
}
/*
**********************************************************************
** 
**  map freesrb
**
**********************************************************************
*/
static void arcmsr_map_freesrb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	PACB pACB=arg;
	PSRB psrb_tmp,pfreesrb;
	ULONG cdb_phyaddr;
	LONG i;

    pfreesrb=(PSRB)pACB->uncacheptr;
	cdb_phyaddr=segs->ds_addr; /* We suppose bus_addr_t high part always 0 here*/
	if(((CPT2INT)pACB->uncacheptr & 0x1F)!=0)
	{
		pfreesrb=pfreesrb+(0x20-((CPT2INT)pfreesrb & 0x1F));
		cdb_phyaddr=cdb_phyaddr+(0x20-((CPT2INT)cdb_phyaddr & 0x1F));
	}
	/*
	********************************************************************
	** here we need to tell iop 331 our freesrb.HighPart 
	** if freesrb.HighPart is not zero
	********************************************************************
	*/
	for(i=0;i<ARCMSR_MAX_FREESRB_NUM;i++)
	{
		psrb_tmp=&pfreesrb[i];
		if(((CPT2INT)psrb_tmp & 0x1F)==0) /*srb address must 32 (0x20) boundary*/
		{
            if(bus_dmamap_create(pACB->buffer_dmat, /*flags*/0, &psrb_tmp->dmamap)!=0)
			{
				pACB->acb_flags |= ACB_F_MAPFREESRB_FAILD;
			    printf(" arcmsr_map_freesrb: (pSRB->dmamap) bus_dmamap_create ..............error\n");
			    return;
			}
			psrb_tmp->cdb_shifted_phyaddr=cdb_phyaddr >> 5;
            psrb_tmp->pACB=pACB;
			pACB->psrbringQ[i]=psrb_tmp;
			cdb_phyaddr=cdb_phyaddr+sizeof(struct _SRB);
 		}
		else
		{
			pACB->acb_flags |= ACB_F_MAPFREESRB_FAILD;
			printf(" arcmsr_map_freesrb:pfreesrb=%p i=%d this srb cross 32 bytes boundary ignored ......psrb_tmp=%p \n",pfreesrb,i,psrb_tmp);
			return;
		}
	}
 	pACB->pfreesrb=pfreesrb;
	pACB->vir2phy_offset=(CPT2INT)psrb_tmp-(cdb_phyaddr-sizeof(struct _SRB));
    return;
}
/*
************************************************************************
**
**
************************************************************************
*/
VOID arcmsr_free_resource(PACB pACB)
{
	/* remove the control device */
	if (pACB->ioctl_dev != NULL)
	{
		destroy_dev(pACB->ioctl_dev);
	}
    bus_dmamap_unload(pACB->srb_dmat, pACB->srb_dmamap);
    bus_dmamap_destroy(pACB->srb_dmat, pACB->srb_dmamap);
    bus_dma_tag_destroy(pACB->srb_dmat);
	bus_dma_tag_destroy(pACB->buffer_dmat);
	bus_dma_tag_destroy(pACB->parent_dmat);
	return;
}
/*
************************************************************************
** PCI config header registers for all devices 
**
** #define PCIR_COMMAND	        0x04
** #define PCIM_CMD_PORTEN		0x0001
** #define PCIM_CMD_MEMEN		0x0002
** #define PCIM_CMD_BUSMASTEREN	0x0004
** #define PCIM_CMD_MWRICEN	    0x0010
** #define PCIM_CMD_PERRESPEN	0x0040    
**        
** Function      : arcmsr_initialize 
** Purpose       : initialize the internal structures for a given SCSI host
** Inputs        : host - pointer to this host adapter's structure
** Preconditions : when this function is called,the chip_type
**	               field of the pACB structure MUST have been set.
**
** 10h Base Address register #0
** 14h Base Address register #1
** 18h Base Address register #2
** 1Ch Base Address register #3
** 20h Base Address register #4
** 24h Base Address register #5
************************************************************************
*/
static LONG arcmsr_initialize(device_t dev)
{
	PACB pACB=device_get_softc(dev);
	LONG rid=PCI_BASE_ADDR0;
	vm_offset_t	mem_base;
	USHORT pci_command;

	#if ARCMSR_DEBUG0
	printf("arcmsr_initialize..............................\n");
	#endif
#if __FreeBSD_version >= 502010
	if (bus_dma_tag_create( /*parent*/NULL, 
		                    /*alignemnt*/1, 
				   			/*boundary*/0,
			       			/*lowaddr*/BUS_SPACE_MAXADDR_32BIT,
			       			/*highaddr*/BUS_SPACE_MAXADDR,
			       			/*filter*/NULL, 
							/*filterarg*/NULL,
			       			/*maxsize*/BUS_SPACE_MAXSIZE_32BIT,
			       			/*nsegments*/BUS_SPACE_UNRESTRICTED,
			       			/*maxsegsz*/BUS_SPACE_MAXSIZE_32BIT,
			       			/*flags*/0, 
							/*lockfunc*/NULL,
							/*lockarg*/NULL,
							&pACB->parent_dmat) != 0) 
#else
	if (bus_dma_tag_create( /*parent*/NULL, 
		                    /*alignemnt*/1, 
				   			/*boundary*/0,
			       			/*lowaddr*/BUS_SPACE_MAXADDR_32BIT,
			       			/*highaddr*/BUS_SPACE_MAXADDR,
			       			/*filter*/NULL, 
							/*filterarg*/NULL,
			       			/*maxsize*/BUS_SPACE_MAXSIZE_32BIT,
			       			/*nsegments*/BUS_SPACE_UNRESTRICTED,
			       			/*maxsegsz*/BUS_SPACE_MAXSIZE_32BIT,
			       			/*flags*/0, 
							&pACB->parent_dmat) != 0) 
#endif
	{
		printf("arcmsr_initialize: bus_dma_tag_create .......................failure!\n");
		return ENOMEM;
	}
    /* Create a single tag describing a region large enough to hold all of the s/g lists we will need. */
#if __FreeBSD_version >= 502010
	if(bus_dma_tag_create( /*parent_dmat*/pACB->parent_dmat,
		                   /*alignment*/1,
			               /*boundary*/0,
			               /*lowaddr*/BUS_SPACE_MAXADDR,
			               /*highaddr*/BUS_SPACE_MAXADDR,
			               /*filter*/NULL,
						   /*filterarg*/NULL,
			               /*maxsize*/MAXBSIZE,
						   /*nsegments*/ARCMSR_MAX_SG_ENTRIES,
			               /*maxsegsz*/BUS_SPACE_MAXSIZE_32BIT,
			               /*flags*/BUS_DMA_ALLOCNOW,
						   /*lockfunc*/busdma_lock_mutex,
						   /*lockarg*/&Giant,
			               &pACB->buffer_dmat) != 0) 
#else
	if(bus_dma_tag_create( /*parent_dmat*/pACB->parent_dmat,
		                   /*alignment*/1,
			               /*boundary*/0,
			               /*lowaddr*/BUS_SPACE_MAXADDR,
			               /*highaddr*/BUS_SPACE_MAXADDR,
			               /*filter*/NULL,
						   /*filterarg*/NULL,
			               /*maxsize*/MAXBSIZE,
						   /*nsegments*/ARCMSR_MAX_SG_ENTRIES,
			               /*maxsegsz*/BUS_SPACE_MAXSIZE_32BIT,
			               /*flags*/BUS_DMA_ALLOCNOW,
			               &pACB->buffer_dmat) != 0) 
#endif
	{
		bus_dma_tag_destroy(pACB->parent_dmat);
		printf("arcmsr_initialize: bus_dma_tag_create ............................failure!\n");
		return ENOMEM;
    }
	/* DMA tag for our srb structures.... Allocate the pfreesrb memory */
#if __FreeBSD_version >= 502010
	if (bus_dma_tag_create( /*parent_dmat*/pACB->parent_dmat, 
		                    /*alignment*/1, 
		                    /*boundary*/0,
			       			/*lowaddr*/BUS_SPACE_MAXADDR_32BIT,
			       			/*highaddr*/BUS_SPACE_MAXADDR,
			       			/*filter*/NULL, 
				   			/*filterarg*/NULL,
			       			/*maxsize*/((sizeof(struct _SRB) * ARCMSR_MAX_FREESRB_NUM)+0x20),
			       			/*nsegments*/1,
			       			/*maxsegsz*/BUS_SPACE_MAXSIZE_32BIT,
			       			/*flags*/BUS_DMA_ALLOCNOW,
							/*lockfunc*/NULL,
							/*lockarg*/NULL,
							&pACB->srb_dmat) != 0) 
#else
	if (bus_dma_tag_create( /*parent_dmat*/pACB->parent_dmat, 
		                    /*alignment*/1, 
		                    /*boundary*/0,
			       			/*lowaddr*/BUS_SPACE_MAXADDR_32BIT,
			       			/*highaddr*/BUS_SPACE_MAXADDR,
			       			/*filter*/NULL, 
				   			/*filterarg*/NULL,
			       			/*maxsize*/((sizeof(struct _SRB) * ARCMSR_MAX_FREESRB_NUM)+0x20),
			       			/*nsegments*/1,
			       			/*maxsegsz*/BUS_SPACE_MAXSIZE_32BIT,
			       			/*flags*/BUS_DMA_ALLOCNOW,
							&pACB->srb_dmat) != 0) 
#endif
	{
		bus_dma_tag_destroy(pACB->buffer_dmat);
		bus_dma_tag_destroy(pACB->parent_dmat);
		printf("arcmsr_initialize: pACB->srb_dmat bus_dma_tag_create .....................failure!\n");
		return ENXIO;
    }
	/* Allocation for our srbs */
	if (bus_dmamem_alloc(pACB->srb_dmat, (void **)&pACB->uncacheptr, BUS_DMA_WAITOK | BUS_DMA_COHERENT, &pACB->srb_dmamap) != 0) 
	{
        bus_dma_tag_destroy(pACB->srb_dmat);
		bus_dma_tag_destroy(pACB->buffer_dmat);
		bus_dma_tag_destroy(pACB->parent_dmat);
		printf("arcmsr_initialize: pACB->srb_dmat bus_dma_tag_create ...............failure!\n");
		return ENXIO;
	}
	/* And permanently map them */
	if(bus_dmamap_load(pACB->srb_dmat, pACB->srb_dmamap,pACB->uncacheptr,(sizeof(struct _SRB) * ARCMSR_MAX_FREESRB_NUM)+0x20,arcmsr_map_freesrb, pACB, /*flags*/0))
	{
        bus_dma_tag_destroy(pACB->srb_dmat);
		bus_dma_tag_destroy(pACB->buffer_dmat);
		bus_dma_tag_destroy(pACB->parent_dmat);
		printf("arcmsr_initialize: bus_dmamap_load................... failure!\n");
		return ENXIO;
	}
	pci_command=pci_read_config(dev,PCIR_COMMAND,2);
	pci_command |= PCIM_CMD_BUSMASTEREN;
	pci_command |= PCIM_CMD_PERRESPEN;
	pci_command |= PCIM_CMD_MWRICEN;
	/* Enable Busmaster/Mem */
	pci_command |= PCIM_CMD_MEMEN;
	pci_write_config(dev,PCIR_COMMAND,pci_command,2);
	pACB->sys_res_arcmsr=bus_alloc_resource(dev,SYS_RES_MEMORY,&rid,0,~0,0x1000,RF_ACTIVE);
	if(pACB->sys_res_arcmsr == NULL)
	{
		arcmsr_free_resource(pACB);
		printf("arcmsr_initialize: bus_alloc_resource .....................failure!\n");
		return ENOMEM;
	}
	if(rman_get_start(pACB->sys_res_arcmsr) <= 0)
	{
		arcmsr_free_resource(pACB);
		printf("arcmsr_initialize: rman_get_start ...........................failure!\n");
        return ENXIO;
	}
	mem_base=(vm_offset_t) rman_get_virtual(pACB->sys_res_arcmsr);
	if(mem_base==0)
	{
		arcmsr_free_resource(pACB);
		printf("arcmsr_initialize: rman_get_virtual ..........................failure!\n");
		return ENXIO;
	}
	if(pACB->acb_flags &  ACB_F_MAPFREESRB_FAILD)
	{
		arcmsr_free_resource(pACB);
		printf("arcmsr_initialize: arman_get_virtual ..........................failure!\n");
		return ENXIO;
	}
	pACB->btag=rman_get_bustag(pACB->sys_res_arcmsr);
	pACB->bhandle=rman_get_bushandle(pACB->sys_res_arcmsr);
    pACB->pmu=(PMU)mem_base;
    pACB->acb_flags |= (ACB_F_IOCTL_WQBUFFER_CLEARED|ACB_F_IOCTL_RQBUFFER_CLEARED);
	pACB->acb_flags &= ~ACB_F_SCSISTOPADAPTER;
	arcmsr_iop_init(pACB);
    return(0);
}
/*
************************************************************************
**
**        attach and init a host adapter               
**
************************************************************************
*/
static LONG arcmsr_attach(device_t dev)
{
	PACB pACB=device_get_softc(dev);
	LONG unit=device_get_unit(dev);
	struct ccb_setasync csa;
	struct cam_devq	*devq;	/* Device Queue to use for this SIM */
	struct resource	*irqres;
	int	rid;

    #if ARCMSR_DEBUG0
    printf("arcmsr_attach .............................\n" );
    #endif

	if(arcmsr_initialize(dev)) 
	{
		printf("arcmsr_attach: arcmsr_initialize failure!\n");
		return ENXIO;
	}
	/* After setting up the adapter,map our interrupt */
	rid=0;
	irqres=bus_alloc_resource(dev,SYS_RES_IRQ,&rid,0,~0,1,RF_SHAREABLE | RF_ACTIVE);
	if(irqres == NULL || bus_setup_intr(dev,irqres,INTR_TYPE_CAM,arcmsr_interrupt,pACB,&pACB->ih)) 
	{
		arcmsr_free_resource(pACB);
		printf("arcmsr%d: unable to register interrupt handler!\n",unit);
		return ENXIO;
	}
	pACB->irqres=irqres;
	pACB->pci_dev=dev;
	pACB->pci_unit=unit;
	/*
	 * Now let the CAM generic SCSI layer find the SCSI devices on
	 * the bus *  start queue to reset to the idle loop. *
	 * Create device queue of SIM(s) *  (MAX_START_JOB - 1) :
	 * max_sim_transactions
	*/
	devq=cam_simq_alloc(ARCMSR_MAX_START_JOB);
	if(devq == NULL) 
	{
	    arcmsr_free_resource(pACB);
		bus_release_resource(dev, SYS_RES_IRQ, 0, pACB->irqres);
		printf("arcmsr_attach: cam_simq_alloc failure!\n");
		return ENXIO;
	}
	pACB->psim=cam_sim_alloc(arcmsr_action,arcmsr_poll,"arcmsr",pACB,pACB->pci_unit,1,ARCMSR_MAX_OUTSTANDING_CMD,devq);
	if(pACB->psim == NULL) 
	{
		arcmsr_free_resource(pACB);
		bus_release_resource(dev, SYS_RES_IRQ, 0, pACB->irqres);
		cam_simq_free(devq);
		printf("arcmsr_attach: cam_sim_alloc ..................failure!\n");
		return ENXIO;
	}
	if(xpt_bus_register(pACB->psim,0) != CAM_SUCCESS) 
	{
		arcmsr_free_resource(pACB);
		bus_release_resource(dev, SYS_RES_IRQ, 0, pACB->irqres);
		cam_sim_free(pACB->psim,/*free_devq*/TRUE);
		printf("arcmsr_attach: xpt_bus_register .......................failure!\n");
		return ENXIO;
	}
 	if(xpt_create_path(&pACB->ppath,/* periph */ NULL,cam_sim_path(pACB->psim),CAM_TARGET_WILDCARD,CAM_LUN_WILDCARD) != CAM_REQ_CMP) 
	{
		arcmsr_free_resource(pACB);
		bus_release_resource(dev, SYS_RES_IRQ, 0, pACB->irqres);
		xpt_bus_deregister(cam_sim_path(pACB->psim));
		cam_sim_free(pACB->psim,/* free_simq */ TRUE);
		printf("arcmsr_attach: xpt_create_path .....................failure!\n");
		return ENXIO;
	}
    /*
	****************************************************
	*/
 	xpt_setup_ccb(&csa.ccb_h,pACB->ppath,/*priority*/5);
	csa.ccb_h.func_code=XPT_SASYNC_CB;
	csa.event_enable=AC_FOUND_DEVICE|AC_LOST_DEVICE;
	csa.callback=arcmsr_async;
	csa.callback_arg=pACB->psim;
	xpt_action((union ccb *)&csa);
    /* Create the control device.  */
    pACB->ioctl_dev=make_dev(&arcmsr_cdevsw, unit, UID_ROOT, GID_WHEEL /* GID_OPERATOR */, S_IRUSR | S_IWUSR, "arcmsr%d", unit);
#if __FreeBSD_version < 503000
	pACB->ioctl_dev->si_drv1=pACB;
#endif
#if __FreeBSD_version > 500005
	(void)make_dev_alias(pACB->ioctl_dev, "arc%d", unit);
#endif

#if 0
	#if __FreeBSD_version > 500005
		if(kthread_create(arcmsr_do_thread_works, pACB, &pACB->kthread_proc,0,"arcmsr%d: kthread",pACB->pci_unit))
		{
			device_printf(pACB->pci_dev,"cannot create kernel thread for this host adapetr\n");
			xpt_bus_deregister(cam_sim_path(pACB->psim));
			cam_sim_free(pACB->psim,/* free_simq */ TRUE);
			panic("arcmsr plunge kernel thread fail");
		}
	#else
		if(kthread_create(arcmsr_do_thread_works, pACB, &pACB->kthread_proc,"arcmsr%d: kthread", pACB->pci_unit))
		{
			device_printf(pACB->pci_dev,"cannot create kernel thread for this host adapetr\n");
			xpt_bus_deregister(cam_sim_path(pACB->psim));
			cam_sim_free(pACB->psim,/* free_simq */ TRUE);
			panic("arcmsr plunge kernel thread fail");
		}
	#endif
#endif
 	return 0;
}
/*
************************************************************************
**
**                     
**
************************************************************************
*/
static LONG arcmsr_probe(device_t dev)
{
	ULONG id;
	#if ARCMSR_DEBUG0
	printf("arcmsr_probe................. \n");
	#endif
    switch(id=pci_get_devid(dev))
	{
	case PCIDevVenIDARC1110:
		device_set_desc(dev,"ARECA ARC1110 PCI-X 4 PORTS SATA RAID CONTROLLER \n" ARCMSR_DRIVER_VERSION );
	    return 0;
    case PCIDevVenIDARC1120:
		device_set_desc(dev,"ARECA ARC1120 PCI-X 8 PORTS SATA RAID CONTROLLER (RAID6-ENGINE Inside) \n" ARCMSR_DRIVER_VERSION);
		return 0;
    case PCIDevVenIDARC1130:
		device_set_desc(dev,"ARECA ARC1130 PCI-X 12 PORTS SATA RAID CONTROLLER (RAID6-ENGINE Inside) \n" ARCMSR_DRIVER_VERSION);
		return 0;
    case PCIDevVenIDARC1160:
		device_set_desc(dev,"ARECA ARC1160 PCI-X 16 PORTS SATA RAID CONTROLLER (RAID6-ENGINE Inside) \n" ARCMSR_DRIVER_VERSION);
		return 0;
    case PCIDevVenIDARC1210:
		device_set_desc(dev,"ARECA ARC1210 PCI-EXPRESS 4 PORTS SATA RAID CONTROLLER \n" ARCMSR_DRIVER_VERSION);
		return 0;
    case PCIDevVenIDARC1220:
		device_set_desc(dev,"ARECA ARC1220 PCI-EXPRESS 8 PORTS SATA RAID CONTROLLER (RAID6-ENGINE Inside) \n" ARCMSR_DRIVER_VERSION);
		return 0;
   case PCIDevVenIDARC1230:
		device_set_desc(dev,"ARECA ARC1230 PCI-EXPRESS 12 PORTS SATA RAID CONTROLLER (RAID6-ENGINE Inside) \n" ARCMSR_DRIVER_VERSION);
		return 0;
    case PCIDevVenIDARC1260:
		device_set_desc(dev,"ARECA ARC1260 PCI-EXPRESS 16 PORTS SATA RAID CONTROLLER (RAID6-ENGINE Inside) \n" ARCMSR_DRIVER_VERSION);
		return 0;
	}
	return ENXIO;
}
/*
************************************************************************
**
**                     
**
************************************************************************
*/
static VOID arcmsr_shutdown(device_t dev)
{
	LONG  i,abort_cmd_cnt=0;
	ULONG s,intmask_org;
	PSRB pSRB;
    PACB pACB=device_get_softc(dev);

	#if ARCMSR_DEBUG0
	printf("arcmsr_shutdown................. \n");
	#endif
	s=splcam();
    /* disable all outbound interrupt */
    intmask_org=CHIP_REG_READ32(&pACB->pmu->outbound_intmask);
    CHIP_REG_WRITE32(&pACB->pmu->outbound_intmask,(intmask_org|ARCMSR_MU_OUTBOUND_ALL_INTMASKENABLE));
	/* stop adapter background rebuild */
	arcmsr_stop_adapter_bgrb(pACB);
	if(arcmsr_wait_msgint_ready(pACB)!=TRUE)
	{
		printf("arcmsr_pcidev_disattach: wait 'stop adapter rebulid' timeout.... \n");
	}
	arcmsr_flush_adapter_cache(pACB);
	if(arcmsr_wait_msgint_ready(pACB)!=TRUE)
	{
		printf("arcmsr_pcidev_disattach: wait 'flush adapter cache' timeout.... \n");
	}
	/* abort all outstanding command */
	pACB->acb_flags |= ACB_F_SCSISTOPADAPTER;
	pACB->acb_flags &= ~ACB_F_IOP_INITED;
	if(pACB->srboutstandingcount!=0)
	{  
		PSRB pfreesrb;
	#if ARCMSR_DEBUG0
	printf("arcmsr_pcidev_disattach: .....pACB->srboutstandingcount!=0 \n");
    #endif
		/* Q back all outstanding srb into wait exec psrb Q*/
        pfreesrb=pACB->pfreesrb;
		for(i=0;i<ARCMSR_MAX_FREESRB_NUM;i++)
		{
	    	pSRB=&pfreesrb[i];
			if(pSRB->startdone==ARCMSR_SRB_START)
			{
				pSRB->srb_flags|=SRB_FLAG_MASTER_ABORTED;
				pSRB->pccb->ccb_h.status=CAM_REQ_ABORTED;
				abort_cmd_cnt++;
			}
		}
		if(abort_cmd_cnt!=0)
		{
	#if ARCMSR_DEBUG0
	printf("arcmsr_pcidev_disattach: .....abort_cmd_cnt!=0 \n");
    #endif
			arcmsr_abort_allcmd(pACB);
			if(arcmsr_wait_msgint_ready(pACB)!=TRUE)
			{
				printf("arcmsr_pcidev_disattach: wait 'abort all outstanding command' timeout.................in \n");
			}
		}
		atomic_set_int(&pACB->srboutstandingcount,0);
 	}
	if(pACB->srbwait2gocount!=0)
	{	/*remove first wait2go srb and abort it*/
		for(i=0;i<ARCMSR_MAX_OUTSTANDING_CMD;i++)
		{
			pSRB=pACB->psrbwait2go[i];
			if(pSRB!=NULL)
			{
				pACB->psrbwait2go[i]=NULL;
				atomic_subtract_int(&pACB->srbwait2gocount,1);
				pSRB->pccb->ccb_h.status=CAM_REQ_ABORTED; 
				arcmsr_srb_complete(pSRB);
			}
		}
	}
	splx(s);
#if 0
	pACB->acb_flags |= ACB_F_STOP_THREAD;
	wakeup(pACB->kthread_proc);/* signal to kernel thread do_dpcQ: "stop thread" */
#endif
    return;
}
/*
************************************************************************
**
**                     
**
************************************************************************
*/
static LONG arcmsr_detach(device_t dev)
{
	PACB pACB=device_get_softc(dev);

	arcmsr_shutdown(dev);
	arcmsr_free_resource(pACB);
	bus_release_resource(dev, SYS_RES_MEMORY, PCIR_MAPS, pACB->sys_res_arcmsr);
	bus_teardown_intr(dev, pACB->irqres, pACB->ih);
	bus_release_resource(dev, SYS_RES_IRQ, 0, pACB->irqres);
	xpt_async(AC_LOST_DEVICE, pACB->ppath, NULL);
	xpt_free_path(pACB->ppath);
	xpt_bus_deregister(cam_sim_path(pACB->psim));
	cam_sim_free(pACB->psim, TRUE);
	return (0);
}



