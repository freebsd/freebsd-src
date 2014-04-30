/*
******************************************************************************************
**        O.S   : FreeBSD
**   FILE NAME  : arcmsr.c
**        BY    : Erich Chen   
**   Description: SCSI RAID Device Driver for 
**                ARECA (ARC11XX/ARC12XX) SATA RAID HOST Adapter
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
**     1.20.00.03    4/19/2005         Erich Chen        add SATA 24 Ports adapter type support
**                                                       clean unused function
**     1.20.00.12    9/12/2005         Erich Chen        bug fix with abort command handling,firmware version check
**                                                       and firmware update notify for hardware bug fix
**                                                       handling if none zero high part physical address 
**                                                       of srb resource 
******************************************************************************************
** $FreeBSD$
*/
#define ARCMSR_DEBUG            0
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
static struct _SRB * arcmsr_get_freesrb(struct _ACB * pACB);
static u_int8_t arcmsr_seek_cmd2abort(union ccb * pabortccb);
static u_int8_t arcmsr_wait_msgint_ready(struct _ACB * pACB);
static u_int32_t arcmsr_probe(device_t dev);
static u_int32_t arcmsr_attach(device_t dev);
static u_int32_t arcmsr_detach(device_t dev);
static u_int32_t arcmsr_iop_ioctlcmd(struct _ACB * pACB,u_int32_t ioctl_cmd,caddr_t arg);
static void arcmsr_iop_parking(struct _ACB *pACB);
static void arcmsr_shutdown(device_t dev);
static void arcmsr_interrupt(void *arg);
static void arcmsr_polling_srbdone(struct _ACB *pACB,struct _SRB *poll_srb);
static void arcmsr_free_resource(struct _ACB * pACB);
static void arcmsr_bus_reset(struct _ACB * pACB);
static void arcmsr_stop_adapter_bgrb(struct _ACB * pACB);
static void arcmsr_start_adapter_bgrb(struct _ACB * pACB);
static void arcmsr_iop_init(struct _ACB * pACB);
static void arcmsr_flush_adapter_cache(struct _ACB * pACB);
static void arcmsr_queue_wait2go_srb(struct _ACB * pACB,struct _SRB * pSRB);
static void arcmsr_post_wait2go_srb(struct _ACB * pACB);
static void arcmsr_post_Qbuffer(struct _ACB * pACB);
static void arcmsr_abort_allcmd(struct _ACB * pACB);
static void arcmsr_srb_complete(struct _SRB * pSRB);
static void arcmsr_iop_reset(struct _ACB * pACB);
static void arcmsr_report_sense_info(struct _SRB * pSRB);
static void arcmsr_build_srb(struct _SRB * pSRB, bus_dma_segment_t * dm_segs, u_int32_t nseg);
static int arcmsr_resume(device_t dev);
static int arcmsr_suspend(device_t dev);
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
** static void MDELAY(u_int32_t ms) { while (ms--) UDELAY(1000); }
**************************************************************************
*/
static void UDELAY(u_int32_t us) { DELAY(us); }
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
    DEVMETHOD(device_suspend,	arcmsr_suspend),
    DEVMETHOD(device_resume,	arcmsr_resume),

    DEVMETHOD(bus_print_child,	bus_generic_print_child),
    DEVMETHOD(bus_driver_added,	bus_generic_driver_added),
	{ 0,0 }
};

static driver_t arcmsr_driver={
	"arcmsr",arcmsr_methods,sizeof(struct _ACB)
};

static devclass_t arcmsr_devclass;
DRIVER_MODULE(arcmsr,pci,arcmsr_driver,arcmsr_devclass,0,0);
#ifndef BUS_DMA_COHERENT		
    #define	BUS_DMA_COHERENT	0x04	/* hint: map memory in a coherent way */
#endif
#if __FreeBSD_version >= 501000
    #ifndef D_NEEDGIANT
        #define D_NEEDGIANT	0x00400000	/* driver want Giant */
    #endif
	#ifndef D_VERSION
        #define D_VERSION	0x20011966
    #endif
	static struct cdevsw arcmsr_cdevsw={
	#if __FreeBSD_version > 502010
	    .d_version = D_VERSION,
    #endif
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
	    struct _ACB * pACB=dev->si_drv1;
    #else
		int	unit = minor(dev);
		struct _ACB * pACB = devclass_get_softc(arcmsr_devclass, unit);   
    #endif

	if(pACB==NULL)
	{
		return ENXIO;
	}
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
	    struct _ACB * pACB=dev->si_drv1;
    #else
		int	unit = minor(dev);
		struct _ACB * pACB = devclass_get_softc(arcmsr_devclass, unit);   
    #endif

	if(pACB==NULL)
	{
		return ENXIO;
	}
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
	    struct _ACB * pACB=dev->si_drv1;
    #else
		int	unit = minor(dev);
		struct _ACB * pACB = devclass_get_softc(arcmsr_devclass, unit);   
    #endif

	if(pACB==NULL)
	{
		return ENXIO;
	}
    return(arcmsr_iop_ioctlcmd(pACB,ioctl_cmd,arg));
}
/*
*******************************************************************************
** Bring the controller to a quiescent state, ready for system suspend.
*******************************************************************************
*/
static int arcmsr_suspend(device_t dev)
{
    struct _ACB	*pACB = device_get_softc(dev);
    u_int32_t intmask_org;
    int	s;

    s = splbio();
    /* disable all outbound interrupt */
    intmask_org=readl(&pACB->pmu->outbound_intmask);
    writel(&pACB->pmu->outbound_intmask,(intmask_org|ARCMSR_MU_OUTBOUND_ALL_INTMASKENABLE));
    /* flush controller */
	printf("arcmsr%d: flushing cache...\n",pACB->pci_unit);
	arcmsr_iop_parking(pACB);
    splx(s);
    return(0);
}
/*
*******************************************************************************
** Bring the controller back to a state ready for operation.
*******************************************************************************
*/
static int arcmsr_resume(device_t dev)
{
    struct _ACB	*pACB = device_get_softc(dev);

    arcmsr_iop_init(pACB);
    return(0);
}
/*
*********************************************************************************
**  Asynchronous notification handler.
*********************************************************************************
*/
static void arcmsr_async(void *cb_arg, u_int32_t code, struct cam_path *path, void *arg)
{
	struct _ACB * pACB;
	u_int8_t target_id,target_lun;
	struct cam_sim * sim;
	u_int32_t s;
   
	s=splcam();
	sim=(struct cam_sim *) cb_arg;
	pACB =(struct _ACB *) cam_sim_softc(sim);
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
************************************************************************
**
**
************************************************************************
*/
static void arcmsr_flush_adapter_cache(struct _ACB * pACB)
{
	writel(&pACB->pmu->inbound_msgaddr0,ARCMSR_INBOUND_MESG0_FLUSH_CACHE);
	return;
}
/*
**********************************************************************
** 
**  
**
**********************************************************************
*/
static u_int8_t arcmsr_wait_msgint_ready(struct _ACB * pACB)
{
	u_int32_t Index;
	u_int8_t Retries=0x00;
	do
	{
		for(Index=0; Index < 100; Index++)
		{
			if(readl(&pACB->pmu->outbound_intstatus) & ARCMSR_MU_OUTBOUND_MESSAGE0_INT)
			{
				writel(&pACB->pmu->outbound_intstatus, ARCMSR_MU_OUTBOUND_MESSAGE0_INT);/*clear interrupt*/
				return 0x00;
			}
			/* one us delay	*/
			UDELAY(10000);
		}/*max 1 seconds*/
	}while(Retries++ < 20);/*max 20 sec*/
	return 0xff;
}
/*
**********************************************************************
**
**  Q back this SRB into ACB ArraySRB
**
**********************************************************************
*/
static void arcmsr_srb_complete(struct _SRB * pSRB)
{
	u_int32_t s;
	struct _ACB * pACB=pSRB->pACB;
    union ccb * pccb=pSRB->pccb;

	if((pccb->ccb_h.flags & CAM_DIR_MASK) != CAM_DIR_NONE)
	{
		bus_dmasync_op_t op;

		if((pccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN)
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
static void arcmsr_report_sense_info(struct _SRB * pSRB)
{
	union ccb * pccb=pSRB->pccb;
	PSENSE_DATA  psenseBuffer=(PSENSE_DATA)&pccb->csio.sense_data;

    pccb->ccb_h.status |= CAM_SCSI_STATUS_ERROR;
	pccb->csio.scsi_status = SCSI_STATUS_CHECK_COND;
    if(psenseBuffer) 
	{
		memset(psenseBuffer, 0, sizeof(pccb->csio.sense_data));
		memcpy(psenseBuffer,pSRB->arcmsr_cdb.SenseData,get_min(sizeof(struct _SENSE_DATA),sizeof(pccb->csio.sense_data)));
	    psenseBuffer->ErrorCode=0x70;
        psenseBuffer->Valid=1;
		pccb->ccb_h.status |= CAM_AUTOSNS_VALID;
    }
    return;
}
/*
*********************************************************************
** to insert pSRB into tail of pACB wait exec srbQ 
*********************************************************************
*/
static void arcmsr_queue_wait2go_srb(struct _ACB * pACB,struct _SRB * pSRB)
{
    u_int32_t s;
	u_int32_t i=0;

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
static void arcmsr_abort_allcmd(struct _ACB * pACB)
{
	writel(&pACB->pmu->inbound_msgaddr0,ARCMSR_INBOUND_MESG0_ABORT_CMD);
	return;
}

/*
****************************************************************************
** Routine Description: Reset 80331 iop.
**           Arguments: 
**        Return Value: Nothing.
****************************************************************************
*/
static void arcmsr_iop_reset(struct _ACB * pACB)
{
	struct _SRB * pSRB;
	u_int32_t intmask_org,mask;
    u_int32_t i=0;

	if(pACB->srboutstandingcount!=0)
	{
		printf("arcmsr%d: iop reset srboutstandingcount=%d \n",pACB->pci_unit,pACB->srboutstandingcount);
        /* disable all outbound interrupt */
		intmask_org=readl(&pACB->pmu->outbound_intmask);
        writel(&pACB->pmu->outbound_intmask,intmask_org|ARCMSR_MU_OUTBOUND_ALL_INTMASKENABLE);
        /* talk to iop 331 outstanding command aborted*/
		arcmsr_abort_allcmd(pACB);
		if(arcmsr_wait_msgint_ready(pACB))
		{
            printf("arcmsr%d: iop reset wait 'abort all outstanding command' timeout \n",pACB->pci_unit);
		}
		/*clear all outbound posted Q*/
		for(i=0;i<ARCMSR_MAX_OUTSTANDING_CMD;i++)
		{
			readl(&pACB->pmu->outbound_queueport);
		}
		for(i=0;i<ARCMSR_MAX_FREESRB_NUM;i++)
		{
			pSRB=pACB->psrb_pool[i];
			if(pSRB->startdone==ARCMSR_SRB_START)
			{
				pSRB->startdone=ARCMSR_SRB_ABORTED;
                pSRB->pccb->ccb_h.status=CAM_REQ_ABORTED;
                arcmsr_srb_complete(pSRB);
			}
		}
		/* enable all outbound interrupt */
		mask=~(ARCMSR_MU_OUTBOUND_POSTQUEUE_INTMASKENABLE|ARCMSR_MU_OUTBOUND_DOORBELL_INTMASKENABLE|ARCMSR_MU_OUTBOUND_MESSAGE0_INTMASKENABLE);
        writel(&pACB->pmu->outbound_intmask,intmask_org & mask);
		/* post abort all outstanding command message to RAID controller */
	}
	i=0;
	while(pACB->srbwait2gocount > 0)
	{
		pSRB=pACB->psrbwait2go[i];
		if(pSRB!=NULL)
		{
			printf("arcmsr%d:iop reset abort command srbwait2gocount=%d \n",pACB->pci_unit,pACB->srbwait2gocount);
		    pACB->psrbwait2go[i]=NULL;
            pSRB->startdone=ARCMSR_SRB_ABORTED;
			pSRB->pccb->ccb_h.status=CAM_REQ_ABORTED;
            arcmsr_srb_complete(pSRB);
			atomic_subtract_int(&pACB->srbwait2gocount,1);
		}
		i++;
		i%=ARCMSR_MAX_OUTSTANDING_CMD;
	}
	atomic_set_int(&pACB->srboutstandingcount,0);
	return;
}
/*
**********************************************************************
** 
** PAGE_SIZE=4096 or 8192,PAGE_SHIFT=12
**********************************************************************
*/
static void arcmsr_build_srb(struct _SRB * pSRB, bus_dma_segment_t *dm_segs, u_int32_t nseg)
{
    struct _ARCMSR_CDB * pARCMSR_CDB=&pSRB->arcmsr_cdb;
	u_int8_t * psge=(u_int8_t *)&pARCMSR_CDB->u;
	u_int32_t address_lo,address_hi;
	union ccb * pccb=pSRB->pccb;
	struct ccb_scsiio * pcsio=&pccb->csio;
	u_int32_t arccdbsize=0x30;

	memset(pARCMSR_CDB,0,sizeof(struct _ARCMSR_CDB));
    pARCMSR_CDB->Bus=0;
    pARCMSR_CDB->TargetID=pccb->ccb_h.target_id;
    pARCMSR_CDB->LUN=pccb->ccb_h.target_lun;
    pARCMSR_CDB->Function=1;
	pARCMSR_CDB->CdbLength=(u_int8_t)pcsio->cdb_len;
    pARCMSR_CDB->Context=(unsigned long)pARCMSR_CDB;
	bcopy(pcsio->cdb_io.cdb_bytes, pARCMSR_CDB->Cdb, pcsio->cdb_len);
	if(nseg != 0) 
	{
		struct _ACB * pACB=pSRB->pACB;
		bus_dmasync_op_t op;	
		u_int32_t length,i,cdb_sgcount=0;

		/* map stor port SG list to our iop SG List.*/
		for(i=0;i<nseg;i++) 
		{
			/* Get the physical address of the current data pointer */
			length=(u_int32_t) dm_segs[i].ds_len;
            address_lo=dma_addr_lo32(dm_segs[i].ds_addr);
			address_hi=dma_addr_hi32(dm_segs[i].ds_addr);
			if(address_hi==0)
			{
				struct _SG32ENTRY * pdma_sg=(struct _SG32ENTRY *)psge;
				pdma_sg->address=address_lo;
				pdma_sg->length=length;
				psge += sizeof(struct _SG32ENTRY);
				arccdbsize += sizeof(struct _SG32ENTRY);
			}
			else
			{
				u_int32_t sg64s_size=0,tmplength=length;

     			#if ARCMSR_DEBUG
				printf("arcmsr%d: !!!!!!!!!!! address_hi=%x \n",pACB->pci_unit,address_hi);
				#endif
				while(1)
				{
					u_int64_t span4G,length0;
					struct _SG64ENTRY * pdma_sg=(struct _SG64ENTRY *)psge;

					span4G=(u_int64_t)address_lo + tmplength;
					pdma_sg->addresshigh=address_hi;
					pdma_sg->address=address_lo;
					if(span4G > 0x100000000)
					{   
						/*see if cross 4G boundary*/
						length0=0x100000000-address_lo;
						pdma_sg->length=(u_int32_t)length0|IS_SG64_ADDR;
						address_hi=address_hi+1;
						address_lo=0;
						tmplength=tmplength-(u_int32_t)length0;
						sg64s_size += sizeof(struct _SG64ENTRY);
						psge += sizeof(struct _SG64ENTRY);
						cdb_sgcount++;
					}
					else
					{
    					pdma_sg->length=tmplength|IS_SG64_ADDR;
						sg64s_size += sizeof(struct _SG64ENTRY);
						psge += sizeof(struct _SG64ENTRY);
						break;
					}
				}
				arccdbsize += sg64s_size;
			}
			cdb_sgcount++;
		}
		pARCMSR_CDB->sgcount=(u_int8_t)cdb_sgcount;
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
static void arcmsr_post_srb(struct _ACB * pACB,struct _SRB * pSRB)
{
	u_int32_t cdb_shifted_phyaddr=(u_int32_t) pSRB->cdb_shifted_phyaddr;
	struct _ARCMSR_CDB * pARCMSR_CDB=(struct _ARCMSR_CDB *)&pSRB->arcmsr_cdb;

    atomic_add_int(&pACB->srboutstandingcount,1);
	pSRB->startdone=ARCMSR_SRB_START;
	if(pARCMSR_CDB->Flags & ARCMSR_CDB_FLAG_SGL_BSIZE)
	{
	    writel(&pACB->pmu->inbound_queueport,cdb_shifted_phyaddr|ARCMSR_SRBPOST_FLAG_SGL_BSIZE);
	}
	else
	{
	    writel(&pACB->pmu->inbound_queueport,cdb_shifted_phyaddr);
	}
	return;
}
/*
**************************************************************************
**
**
**************************************************************************
*/
static void arcmsr_post_wait2go_srb(struct _ACB * pACB)
{
	u_int32_t s;
	struct _SRB * pSRB;
	u_int32_t i=0;

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
static void arcmsr_post_Qbuffer(struct _ACB * pACB)
{
    u_int32_t s;
	u_int8_t * pQbuffer;
	struct _QBUFFER * pwbuffer=(struct _QBUFFER *)&pACB->pmu->ioctl_wbuffer;
    u_int8_t * iop_data=(u_int8_t *)pwbuffer->data;
	u_int32_t allxfer_len=0;

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
 	writel(&pACB->pmu->inbound_doorbell,ARCMSR_INBOUND_DRIVER_DATA_WRITE_OK);
	splx(s);
	return;
}
/*
************************************************************************
************************************************************************
*/
static void arcmsr_stop_adapter_bgrb(struct _ACB * pACB)
{
	pACB->acb_flags |= ACB_F_MSG_STOP_BGRB;
	pACB->acb_flags &= ~ACB_F_MSG_START_BGRB;
	writel(&pACB->pmu->inbound_msgaddr0,ARCMSR_INBOUND_MESG0_STOP_BGRB);
	return;
}
/*
************************************************************************
************************************************************************
*/
static void arcmsr_poll(struct cam_sim * psim)
{
	arcmsr_interrupt(cam_sim_softc(psim));
	return;
}
/*
**********************************************************************
**   Function:  arcmsr_interrupt
**     Output:  void
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
static void arcmsr_interrupt(void *arg)
{
	struct _ACB * pACB=(struct _ACB *)arg;
	struct _SRB * pSRB;
	u_int32_t flag_srb,outbound_intstatus,outbound_doorbell;

	/*
	*********************************************
	**   check outbound intstatus 檢察有無郵差按門鈴
	*********************************************
	*/
	outbound_intstatus=readl(&pACB->pmu->outbound_intstatus) & pACB->outbound_int_enable;
    writel(&pACB->pmu->outbound_intstatus, outbound_intstatus);/*clear interrupt*/
	if(outbound_intstatus & ARCMSR_MU_OUTBOUND_DOORBELL_INT)
	{
		/*
		*********************************************
		**  DOORBELL 叮噹! 是否有郵件要簽收
		*********************************************
		*/
		outbound_doorbell=readl(&pACB->pmu->outbound_doorbell);
		writel(&pACB->pmu->outbound_doorbell,outbound_doorbell);/*clear interrupt */
		if(outbound_doorbell & ARCMSR_OUTBOUND_IOP331_DATA_WRITE_OK)
		{
			struct _QBUFFER * prbuffer=(struct _QBUFFER *)&pACB->pmu->ioctl_rbuffer;
			u_int8_t * iop_data=(u_int8_t *)prbuffer->data;
			u_int8_t * pQbuffer;
			u_int32_t my_empty_len,iop_len,rqbuf_firstindex,rqbuf_lastindex;

            /*check this iop data if overflow my rqbuffer*/
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
				writel(&pACB->pmu->inbound_doorbell, ARCMSR_INBOUND_DRIVER_DATA_READ_OK);/*signature, let IOP331 know data has been readed */
			}
			else
			{
				pACB->acb_flags|=ACB_F_IOPDATA_OVERFLOW;
			}
		}
		if(outbound_doorbell & ARCMSR_OUTBOUND_IOP331_DATA_READ_OK)
		{
			/*
			*********************************************
			**           看看是否還有郵件要順道寄出
			*********************************************
			*/
			if(pACB->wqbuf_firstindex!=pACB->wqbuf_lastindex)
			{
				u_int8_t * pQbuffer;
				struct _QBUFFER * pwbuffer=(struct _QBUFFER *)&pACB->pmu->ioctl_wbuffer;
				u_int8_t * iop_data=(u_int8_t *)pwbuffer->data;
				u_int32_t allxfer_len=0;

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
				writel(&pACB->pmu->inbound_doorbell,ARCMSR_INBOUND_DRIVER_DATA_WRITE_OK);
 			}
			else
			{
				pACB->acb_flags |= ACB_F_IOCTL_WQBUFFER_CLEARED;
			}
		}
	}
	if(outbound_intstatus & ARCMSR_MU_OUTBOUND_POSTQUEUE_INT)
	{
		int target,lun;
 		/*
		*****************************************************************************
		**               areca cdb command done
		*****************************************************************************
		*/
		while(1)
		{
			if((flag_srb=readl(&pACB->pmu->outbound_queueport)) == 0xFFFFFFFF)
			{
				break;/*chip FIFO no srb for completion already*/
			}
			/* check if command done with no error*/
			pSRB=(struct _SRB *)(pACB->vir2phy_offset+(flag_srb << 5));/*frame must be 32 bytes aligned*/
			if((pSRB->pACB!=pACB) || (pSRB->startdone!=ARCMSR_SRB_START))
			{
				if(pSRB->startdone==ARCMSR_SRB_ABORTED)
				{
					printf("arcmsr%d: scsi id=%d lun=%d srb='%p' isr command abort successfully \n",pACB->pci_unit,pSRB->pccb->ccb_h.target_id,pSRB->pccb->ccb_h.target_lun,pSRB);
					pSRB->pccb->ccb_h.status=CAM_REQ_ABORTED;
					arcmsr_srb_complete(pSRB);
					continue;
				}
				printf("arcmsr%d: isr get an illegal srb command done acb='%p' srb='%p' srbacb='%p' startdone=0x%x srboutstandingcount=%d \n",pACB->pci_unit,pACB,pSRB,pSRB->pACB,pSRB->startdone,pACB->srboutstandingcount);
				continue;
			}
			target=pSRB->pccb->ccb_h.target_id;
		    lun=pSRB->pccb->ccb_h.target_lun;
			if((flag_srb & ARCMSR_SRBREPLY_FLAG_ERROR)==0)
			{
				if(pACB->devstate[target][lun]==ARECA_RAID_GONE)
				{
					pACB->devstate[target][lun]=ARECA_RAID_GOOD;
				}
				pSRB->pccb->ccb_h.status=CAM_REQ_CMP;
				arcmsr_srb_complete(pSRB);
			} 
			else 
			{   
				switch(pSRB->arcmsr_cdb.DeviceStatus)
				{
				case ARCMSR_DEV_SELECT_TIMEOUT:
					{
						pACB->devstate[target][lun]=ARECA_RAID_GONE;
 						pSRB->pccb->ccb_h.status=CAM_SEL_TIMEOUT;
						arcmsr_srb_complete(pSRB);
					}
					break;
				case ARCMSR_DEV_ABORTED:
				case ARCMSR_DEV_INIT_FAIL:
					{
						pACB->devstate[target][lun]=ARECA_RAID_GONE;
 						pSRB->pccb->ccb_h.status=CAM_DEV_NOT_THERE;
						arcmsr_srb_complete(pSRB);
					}
					break;
				case SCSISTAT_CHECK_CONDITION:
					{
						pACB->devstate[target][lun]=ARECA_RAID_GOOD;
                        arcmsr_report_sense_info(pSRB);
						arcmsr_srb_complete(pSRB);
					}
					break;
				default:
					/* error occur Q all error srb to errorsrbpending Q*/
					printf("arcmsr%d: scsi id=%d lun=%d isr get command error done, but got unknow DeviceStatus=0x%x \n",pACB->pci_unit,target,lun,pSRB->arcmsr_cdb.DeviceStatus);
					pACB->devstate[target][lun]=ARECA_RAID_GONE;
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
*******************************************************************************
*******************************************************************************
*/
static void arcmsr_iop_parking(struct _ACB *pACB)
{
	if(pACB!=NULL)
	{
		/* stop adapter background rebuild */
		if(pACB->acb_flags & ACB_F_MSG_START_BGRB)
		{
			pACB->acb_flags &= ~ACB_F_MSG_START_BGRB;
			arcmsr_stop_adapter_bgrb(pACB);
			if(arcmsr_wait_msgint_ready(pACB))
			{
  				printf("arcmsr%d:iop parking wait 'stop adapter rebulid' timeout \n",pACB->pci_unit);
			}
			arcmsr_flush_adapter_cache(pACB);
			if(arcmsr_wait_msgint_ready(pACB))
			{
  				printf("arcmsr%d:iop parking wait 'flush adapter cache' timeout \n",pACB->pci_unit);
			}
		}
	}
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
u_int32_t arcmsr_iop_ioctlcmd(struct _ACB * pACB,u_int32_t ioctl_cmd,caddr_t arg)
{
	struct _CMD_IO_CONTROL * pccbioctl=(struct _CMD_IO_CONTROL *) arg;

	if(memcmp(pccbioctl->Signature,"ARCMSR",6)!=0)
    {
        return EINVAL;
	}
	switch(ioctl_cmd)
	{
	case ARCMSR_IOCTL_READ_RQBUFFER:
		{
			u_int32_t s;			
			struct _CMD_IOCTL_FIELD * pccbioctlfld=(struct _CMD_IOCTL_FIELD *)arg;
			u_int8_t * pQbuffer;
			u_int8_t * ptmpQbuffer=pccbioctlfld->ioctldatabuffer;			
			u_int32_t allxfer_len=0;
     
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
                struct _QBUFFER * prbuffer=(struct _QBUFFER *)&pACB->pmu->ioctl_rbuffer;
                u_int8_t * pQbuffer;
				u_int8_t * iop_data=(u_int8_t *)prbuffer->data;
                u_int32_t iop_len;

                pACB->acb_flags &= ~ACB_F_IOPDATA_OVERFLOW;
			    iop_len=(u_int32_t)prbuffer->data_len;
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
				writel(&pACB->pmu->inbound_doorbell, ARCMSR_INBOUND_DRIVER_DATA_READ_OK);/*signature, let IOP331 know data has been readed */
			}
			pccbioctl->Length=allxfer_len;
			pccbioctl->ReturnCode=ARCMSR_IOCTL_RETURNCODE_OK;
			splx(s);
			return ARC_IOCTL_SUCCESS;
 		}
		break;
	case ARCMSR_IOCTL_WRITE_WQBUFFER:
		{
			u_int32_t s;
            struct _CMD_IOCTL_FIELD * pccbioctlfld=(struct _CMD_IOCTL_FIELD *)arg;
			u_int32_t my_empty_len,user_len,wqbuf_firstindex,wqbuf_lastindex;
			u_int8_t * pQbuffer;
			u_int8_t * ptmpuserbuffer=pccbioctlfld->ioctldatabuffer;

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
			u_int32_t s;
			u_int8_t * pQbuffer=pACB->rqbuffer;
            s=splcam();
			if(pACB->acb_flags & ACB_F_IOPDATA_OVERFLOW)
			{
                pACB->acb_flags &= ~ACB_F_IOPDATA_OVERFLOW;
                writel(&pACB->pmu->inbound_doorbell, ARCMSR_INBOUND_DRIVER_DATA_READ_OK);/*signature, let IOP331 know data has been readed */
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
			u_int32_t s;
			u_int8_t * pQbuffer=pACB->wqbuffer;
 
            s=splcam();
			if(pACB->acb_flags & ACB_F_IOPDATA_OVERFLOW)
			{
                pACB->acb_flags &= ~ACB_F_IOPDATA_OVERFLOW;
                writel(&pACB->pmu->inbound_doorbell, ARCMSR_INBOUND_DRIVER_DATA_READ_OK);/*signature, let IOP331 know data has been readed */
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
			u_int32_t s;
			u_int8_t * pQbuffer;
 
            s=splcam();
			if(pACB->acb_flags & ACB_F_IOPDATA_OVERFLOW)
			{
                pACB->acb_flags &= ~ACB_F_IOPDATA_OVERFLOW;
                writel(&pACB->pmu->inbound_doorbell, ARCMSR_INBOUND_DRIVER_DATA_READ_OK);/*signature, let IOP331 know data has been readed */
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
	case ARCMSR_IOCTL_REQUEST_RETURNCODE_3F:
		{
			pccbioctl->ReturnCode=ARCMSR_IOCTL_RETURNCODE_3F;
			return ARC_IOCTL_SUCCESS;
		}
		break;
	case ARCMSR_IOCTL_SAY_HELLO:
		{
			struct _CMD_IOCTL_FIELD * pccbioctlfld=(struct _CMD_IOCTL_FIELD *)arg;
			u_int8_t * hello_string="Hello! I am ARCMSR";
			u_int8_t * puserbuffer=(u_int8_t *)pccbioctlfld->ioctldatabuffer;
  
			if(memcpy(puserbuffer,hello_string,(int16_t)strlen(hello_string)))
			{
				pccbioctl->ReturnCode=ARCMSR_IOCTL_RETURNCODE_ERROR;
                return ENOIOCTL;
			}
            pccbioctl->ReturnCode=ARCMSR_IOCTL_RETURNCODE_OK;
		    return ARC_IOCTL_SUCCESS;
		}
		break;
	case ARCMSR_IOCTL_SAY_GOODBYE:
		{
            arcmsr_iop_parking(pACB);
			return ARC_IOCTL_SUCCESS;
		}
		break;
    case ARCMSR_IOCTL_FLUSH_ADAPTER_CACHE:
		{
			arcmsr_flush_adapter_cache(pACB);
			if(arcmsr_wait_msgint_ready(pACB))
			{
				printf("arcmsr%d: ioctl flush cache wait 'flush adapter cache' timeout \n",pACB->pci_unit);
			}
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
struct _SRB * arcmsr_get_freesrb(struct _ACB * pACB)
{
    struct _SRB * pSRB=NULL;
  	u_int32_t s;
	u_int32_t srb_startindex,srb_doneindex;

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
static void arcmsr_executesrb(void *arg,bus_dma_segment_t *dm_segs,int nseg,int error)
{
	struct _SRB * pSRB=(struct _SRB *)arg;
    struct _ACB * pACB=(struct _ACB *)pSRB->pACB;
	union ccb * pccb;
	int target,lun; 

 	pccb=pSRB->pccb;
	target=pccb->ccb_h.target_id;
	lun=pccb->ccb_h.target_lun;
	if(error != 0) 
	{
		if(error != EFBIG)
		{
			printf("arcmsr%d: unexpected error %x returned from 'bus_dmamap_load' \n",pACB->pci_unit,error);
		}
		if(pccb->ccb_h.status == CAM_REQ_INPROG) 
		{
			xpt_freeze_devq(pccb->ccb_h.path,/*count*/1);
			pccb->ccb_h.status=CAM_REQ_TOO_BIG|CAM_DEV_QFRZN;
		}
		arcmsr_srb_complete(pSRB);
		return;
	}
	if(pACB->acb_flags & ACB_F_BUS_RESET)
	{
		printf("arcmsr%d: bus reset and return busy \n",pACB->pci_unit);
	    pccb->ccb_h.status|=CAM_SCSI_BUS_RESET;
		arcmsr_srb_complete(pSRB);
		return;
	}
	if(pACB->devstate[target][lun]==ARECA_RAID_GONE)
	{
		u_int8_t block_cmd;

        block_cmd=pccb->csio.cdb_io.cdb_bytes[0] & 0x0f;
		if(block_cmd==0x08 || block_cmd==0x0a)
		{
			printf("arcmsr%d:block 'read/write' command with gone raid volume Cmd=%2x,TargetId=%d,Lun=%d \n",pACB->pci_unit,block_cmd,target,lun);
			pccb->ccb_h.status=CAM_DEV_NOT_THERE;
			arcmsr_srb_complete(pSRB);
			return;
		}
	}
    arcmsr_build_srb(pSRB,dm_segs,nseg);
	if(pccb->ccb_h.status != CAM_REQ_INPROG)
	{
		if(nseg != 0)
		{
			bus_dmamap_unload(pACB->buffer_dmat,pSRB->dmamap);
		}
		arcmsr_srb_complete(pSRB);
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
static u_int8_t arcmsr_seek_cmd2abort(union ccb * pabortccb)
{
 	struct _SRB * pSRB;
    struct _ACB * pACB=(struct _ACB *) pabortccb->ccb_h.arcmsr_ccbacb_ptr;
	u_int32_t s,intmask_org,mask;
    u_int32_t i=0;

	s=splcam();
	pACB->num_aborts++;
	/* 
	***************************************************************************
	** It is the upper layer do abort command this lock just prior to calling us.
	** First determine if we currently own this command.
	** Start by searching the device queue. If not found
	** at all,and the system wanted us to just abort the
	** command return success.
	***************************************************************************
	*/
	if(pACB->srboutstandingcount!=0)
	{
		for(i=0;i<ARCMSR_MAX_FREESRB_NUM;i++)
		{
			pSRB=pACB->psrb_pool[i];
			if(pSRB->startdone==ARCMSR_SRB_START)
			{
				if(pSRB->pccb==pabortccb)
				{
                    pSRB->startdone=ARCMSR_SRB_ABORTED;
					printf("arcmsr%d:scsi id=%d lun=%d abort srb '%p' outstanding command \n",pACB->pci_unit,pabortccb->ccb_h.target_id,pabortccb->ccb_h.target_lun,pSRB);
                    goto abort_outstanding_cmd;
				}
			}
		}
	}
	/*
	********************************************************
	** seek this command at our command list 
	** if command found then remove,abort it and free this SRB
	********************************************************
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
					printf("arcmsr%d:scsi id=%d lun=%d abort ccb '%p' pending command \n",pACB->pci_unit,pabortccb->ccb_h.target_id,pabortccb->ccb_h.target_lun,pSRB);
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
	return(FALSE);
abort_outstanding_cmd:
    /* disable all outbound interrupt */
	intmask_org=readl(&pACB->pmu->outbound_intmask);
    writel(&pACB->pmu->outbound_intmask,intmask_org|ARCMSR_MU_OUTBOUND_ALL_INTMASKENABLE);
    /* do not talk to iop 331 abort command */
	arcmsr_polling_srbdone(pACB,pSRB);
 	/* enable all outbound interrupt */
	mask=~(ARCMSR_MU_OUTBOUND_POSTQUEUE_INTMASKENABLE|ARCMSR_MU_OUTBOUND_DOORBELL_INTMASKENABLE|ARCMSR_MU_OUTBOUND_MESSAGE0_INTMASKENABLE);
    writel(&pACB->pmu->outbound_intmask,intmask_org & mask);
	atomic_set_int(&pACB->srboutstandingcount,0);
 	splx(s);
	return (TRUE);
}
/*
****************************************************************************
** 
****************************************************************************
*/
static void arcmsr_bus_reset(struct _ACB * pACB)
{
	int retry=0;

	pACB->num_resets++;
	pACB->acb_flags |=ACB_F_BUS_RESET;
	while(pACB->srboutstandingcount!=0 && retry < 400)
	{
		arcmsr_interrupt((void *)pACB);
        UDELAY(25000);
		retry++;
	}
	arcmsr_iop_reset(pACB);
    pACB->acb_flags &= ~ACB_F_BUS_RESET;
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
static void arcmsr_action(struct cam_sim * psim,union ccb * pccb)
{
	struct _ACB *  pACB;

	pACB=(struct _ACB *) cam_sim_softc(psim);
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
	    	struct _SRB * pSRB;

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
						u_int32_t error,s;

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
						struct bus_dma_segment seg;

						seg.ds_addr=(bus_addr_t)pccb->csio.data_ptr;
						seg.ds_len=pccb->csio.dxfer_len;
						arcmsr_executesrb(pSRB,&seg,1,0);
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
			u_int32_t     i;

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
			pccb->ccb_h.status=CAM_REQ_INVALID;
			xpt_done(pccb);
			break;
		}
	case XPT_GET_TRAN_SETTINGS:
		{
			struct ccb_trans_settings *cts;
			u_int32_t s;

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
		    pccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
		    xpt_done(pccb);
			break;
		}
	case XPT_CALC_GEOMETRY:
		{
			struct ccb_calc_geometry *ccg;
			u_int32_t size_mb;
			u_int32_t secs_per_cylinder;

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
		printf("arcmsr%d: invalid XPT function CAM_REQ_INVALID\n",pACB->pci_unit);
    	pccb->ccb_h.status=CAM_REQ_INVALID;
		xpt_done(pccb);
		break;
	}
	return;
}
/*
**********************************************************************
**  start background rebulid
**********************************************************************
*/
static void arcmsr_start_adapter_bgrb(struct _ACB * pACB)
{
	pACB->acb_flags |= ACB_F_MSG_START_BGRB;
	pACB->acb_flags &= ~ACB_F_MSG_STOP_BGRB;
    writel(&pACB->pmu->inbound_msgaddr0,ARCMSR_INBOUND_MESG0_START_BGRB);
	return;
}
/*
**********************************************************************
**********************************************************************
*/
static void arcmsr_polling_srbdone(struct _ACB *pACB,struct _SRB *poll_srb)
{
	struct _SRB *pSRB;
	uint32_t flag_srb,outbound_intstatus,poll_srb_done=0,poll_count=0;
    int id,lun;

polling_srb_retry:
	poll_count++;
	outbound_intstatus=readl(&pACB->pmu->outbound_intstatus) & pACB->outbound_int_enable;
	writel(&pACB->pmu->outbound_intstatus,outbound_intstatus);/*clear interrupt*/
	while(1)
	{
		if((flag_srb=readl(&pACB->pmu->outbound_queueport))==0xFFFFFFFF)
		{
			if(poll_srb_done)
			{
				break;/*chip FIFO no ccb for completion already*/
			}
			else
			{
                UDELAY(25000);
                if(poll_count > 100)
				{
                    break;
				}
				goto polling_srb_retry;
			}
		}
		/* check ifcommand done with no error*/
		pSRB=(struct _SRB *)(pACB->vir2phy_offset+(flag_srb << 5));/*frame must be 32 bytes aligned*/
		if((pSRB->pACB!=pACB) || (pSRB->startdone!=ARCMSR_SRB_START))
		{
			if((pSRB->startdone==ARCMSR_SRB_ABORTED) && (pSRB==poll_srb))
			{
				printf("arcmsr%d: scsi id=%d lun=%d srb='%p' poll command abort successfully \n",pACB->pci_unit,pSRB->pccb->ccb_h.target_id,pSRB->pccb->ccb_h.target_lun,pSRB);
				pSRB->pccb->ccb_h.status=CAM_REQ_ABORTED;
				arcmsr_srb_complete(pSRB);
				poll_srb_done=1;
				continue;
			}
			printf("arcmsr%d: polling get an illegal srb command done srb='%p' srboutstandingcount=%d \n",pACB->pci_unit,pSRB,pACB->srboutstandingcount);
			continue;
		}
		id=pSRB->pccb->ccb_h.target_id;
		lun=pSRB->pccb->ccb_h.target_lun;
		if((flag_srb & ARCMSR_SRBREPLY_FLAG_ERROR)==0)
		{
			if(pACB->devstate[id][lun]==ARECA_RAID_GONE)
			{
                pACB->devstate[id][lun]=ARECA_RAID_GOOD;
			}
 			pSRB->pccb->ccb_h.status=CAM_REQ_CMP;
			arcmsr_srb_complete(pSRB);
		} 
		else 
		{   
			switch(pSRB->arcmsr_cdb.DeviceStatus)
			{
			case ARCMSR_DEV_SELECT_TIMEOUT:
				{
					pACB->devstate[id][lun]=ARECA_RAID_GONE;
					pSRB->pccb->ccb_h.status=CAM_SEL_TIMEOUT;
					arcmsr_srb_complete(pSRB);
				}
				break;
			case ARCMSR_DEV_ABORTED:
			case ARCMSR_DEV_INIT_FAIL:
				{
				    pACB->devstate[id][lun]=ARECA_RAID_GONE;
					pSRB->pccb->ccb_h.status=CAM_DEV_NOT_THERE;
					arcmsr_srb_complete(pSRB);
				}
				break;
			case SCSISTAT_CHECK_CONDITION:
				{
				    pACB->devstate[id][lun]=ARECA_RAID_GOOD;
		    		arcmsr_report_sense_info(pSRB);
					arcmsr_srb_complete(pSRB);
				}
				break;
			default:
				/* error occur Q all error ccb to errorccbpending Q*/
			    printf("arcmsr%d: scsi id=%d lun=%d polling and getting command error done, but got unknow DeviceStatus=0x%x \n",pACB->pci_unit,id,lun,pSRB->arcmsr_cdb.DeviceStatus);
				pACB->devstate[id][lun]=ARECA_RAID_GONE;
				pSRB->pccb->ccb_h.status=CAM_UNCOR_PARITY;/*unknow error or crc error just for retry*/
				arcmsr_srb_complete(pSRB);
				break;
			}
		}
	}	/*drain reply FIFO*/
	return;
}
/*
**********************************************************************
**  get firmware miscellaneous data
**********************************************************************
*/
static void arcmsr_get_firmware_spec(struct _ACB *pACB)
{
    char *acb_firm_model=pACB->firm_model;
    char *acb_firm_version=pACB->firm_version;
    char *iop_firm_model=(char *) (&pACB->pmu->message_rwbuffer[15]);    /*firm_model,15,60-67*/
    char *iop_firm_version=(char *) (&pACB->pmu->message_rwbuffer[17]);  /*firm_version,17,68-83*/
	int count;

    writel(&pACB->pmu->inbound_msgaddr0,ARCMSR_INBOUND_MESG0_GET_CONFIG);
	if(arcmsr_wait_msgint_ready(pACB))
	{
		printf("arcmsr%d: wait 'get adapter firmware miscellaneous data' timeout \n",pACB->pci_unit);
	}
	count=8;
	while(count)
	{
        *acb_firm_model=readb(iop_firm_model);
        acb_firm_model++;
		iop_firm_model++;
		count--;
	}
	count=16;
	while(count)
	{
        *acb_firm_version=readb(iop_firm_version);
        acb_firm_version++;
		iop_firm_version++;
		count--;
	}
	printf("ARECA RAID ADAPTER%d: FIRMWARE VERSION %s \n",pACB->pci_unit,pACB->firm_version);
	if(strncmp(pACB->firm_version,"V1.37",5) < 0)
	{
        printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
        printf("!!!!!!   PLEASE UPDATE RAID FIRMWARE VERSION EQUAL OR MORE THAN 'V1.37'   !!!!!!\n");
		printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	}
    pACB->firm_request_len=readl(&pACB->pmu->message_rwbuffer[1]);   /*firm_request_len,1,04-07*/
	pACB->firm_numbers_queue=readl(&pACB->pmu->message_rwbuffer[2]); /*firm_numbers_queue,2,08-11*/
	pACB->firm_sdram_size=readl(&pACB->pmu->message_rwbuffer[3]);    /*firm_sdram_size,3,12-15*/
    pACB->firm_ide_channels=readl(&pACB->pmu->message_rwbuffer[4]);  /*firm_ide_channels,4,16-19*/
	return;
}
/*
**********************************************************************
**  start background rebulid
**********************************************************************
*/
static void arcmsr_iop_init(struct _ACB * pACB)
{
    u_int32_t intmask_org,mask,outbound_doorbell,firmware_state=0;

	do
	{
        firmware_state=readl(&pACB->pmu->outbound_msgaddr1);
	}while((firmware_state & ARCMSR_OUTBOUND_MESG1_FIRMWARE_OK)==0);
    intmask_org=readl(&pACB->pmu->outbound_intmask);
	arcmsr_get_firmware_spec(pACB);
	/*start background rebuild*/
	arcmsr_start_adapter_bgrb(pACB);
	if(arcmsr_wait_msgint_ready(pACB))
	{
		printf("arcmsr%d: iop init wait 'start adapter background rebulid' timeout \n",pACB->pci_unit);
	}
	/* clear Qbuffer if door bell ringed */
	outbound_doorbell=readl(&pACB->pmu->outbound_doorbell);
	if(outbound_doorbell & ARCMSR_OUTBOUND_IOP331_DATA_WRITE_OK)
	{
		writel(&pACB->pmu->outbound_doorbell,outbound_doorbell);/*clear interrupt */
        writel(&pACB->pmu->inbound_doorbell,ARCMSR_INBOUND_DRIVER_DATA_READ_OK);
	}
	/* enable outbound Post Queue,outbound message0,outbell doorbell Interrupt */
	mask=~(ARCMSR_MU_OUTBOUND_POSTQUEUE_INTMASKENABLE|ARCMSR_MU_OUTBOUND_DOORBELL_INTMASKENABLE|ARCMSR_MU_OUTBOUND_MESSAGE0_INTMASKENABLE);
    writel(&pACB->pmu->outbound_intmask,intmask_org & mask);
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
	struct _ACB * pACB=arg;
	struct _SRB * psrb_tmp;
    u_int8_t * dma_memptr;
	u_int32_t i,cdb_phyaddr,srb_phyaddr_hi32;
	unsigned long srb_phyaddr=(unsigned long)segs->ds_addr;

    dma_memptr=pACB->uncacheptr;
	cdb_phyaddr=(u_int32_t)segs->ds_addr; /* We suppose bus_addr_t high part always 0 here*/
	if(((unsigned long)dma_memptr & 0x1F)!=0)
	{
		dma_memptr=dma_memptr+(0x20-((unsigned long)dma_memptr & 0x1F));
		cdb_phyaddr=cdb_phyaddr+(0x20-((unsigned long)cdb_phyaddr & 0x1F));
	}
    psrb_tmp=(struct _SRB *)dma_memptr;
	for(i=0;i<ARCMSR_MAX_FREESRB_NUM;i++)
	{
		if(((unsigned long)psrb_tmp & 0x1F)==0) /*srb address must 32 (0x20) boundary*/
		{
            if(bus_dmamap_create(pACB->buffer_dmat, /*flags*/0, &psrb_tmp->dmamap)!=0)
			{
				pACB->acb_flags |= ACB_F_MAPFREESRB_FAILD;
			    printf("arcmsr%d: srb dmamap bus_dmamap_create error\n",pACB->pci_unit);
			    return;
			}
			psrb_tmp->cdb_shifted_phyaddr=cdb_phyaddr >> 5;
            psrb_tmp->pACB=pACB;
			pACB->psrbringQ[i]=pACB->psrb_pool[i]=psrb_tmp;
			cdb_phyaddr=cdb_phyaddr+sizeof(struct _SRB);
 		}
		else
		{
			pACB->acb_flags |= ACB_F_MAPFREESRB_FAILD;
			printf("arcmsr%d: dma_memptr=%p i=%d this srb cross 32 bytes boundary ignored psrb_tmp=%p \n",pACB->pci_unit,dma_memptr,i,psrb_tmp);
			return;
		}
		psrb_tmp++;
	}
	/*
	********************************************************************
	** here we need to tell iop 331 our freesrb.HighPart 
	** if freesrb.HighPart is not zero
	********************************************************************
	*/
	srb_phyaddr_hi32=(uint32_t) ((srb_phyaddr>>16)>>16);
	if(srb_phyaddr_hi32!=0)
	{
        writel(&pACB->pmu->message_rwbuffer[0],ARCMSR_SIGNATURE_SET_CONFIG);
        writel(&pACB->pmu->message_rwbuffer[1],srb_phyaddr_hi32);
		writel(&pACB->pmu->inbound_msgaddr0,ARCMSR_INBOUND_MESG0_SET_CONFIG);
		if(arcmsr_wait_msgint_ready(pACB))
		{
			printf("arcmsr%d: 'set srb high part physical address' timeout \n",pACB->pci_unit);
		}
	}
	pACB->vir2phy_offset=(unsigned long)psrb_tmp-(unsigned long)cdb_phyaddr;
    return;
}
/*
************************************************************************
**
**
************************************************************************
*/
static void arcmsr_free_resource(struct _ACB * pACB)
{
	/* remove the control device */
	if(pACB->ioctl_dev != NULL)
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
static u_int32_t arcmsr_initialize(device_t dev)
{
	struct _ACB * pACB=device_get_softc(dev);
	u_int32_t intmask_org,rid=PCIR_BAR(0);
	vm_offset_t	mem_base;
	u_int16_t pci_command;
	int i,j;

#if __FreeBSD_version >= 502010
	if(bus_dma_tag_create( /*parent*/NULL, 
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
	if(bus_dma_tag_create( /*parent*/NULL, 
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
		printf("arcmsr%d: parent_dmat bus_dma_tag_create failure!\n",pACB->pci_unit);
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
		printf("arcmsr%d: buffer_dmat bus_dma_tag_create failure!\n",pACB->pci_unit);
		return ENOMEM;
    }
	/* DMA tag for our srb structures.... Allocate the freesrb memory */
#if __FreeBSD_version >= 502010
	if(bus_dma_tag_create( /*parent_dmat*/pACB->parent_dmat, 
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
	if(bus_dma_tag_create( /*parent_dmat*/pACB->parent_dmat, 
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
		printf("arcmsr%d: srb_dmat bus_dma_tag_create failure!\n",pACB->pci_unit);
		return ENXIO;
    }
	/* Allocation for our srbs */
	if(bus_dmamem_alloc(pACB->srb_dmat, (void **)&pACB->uncacheptr, BUS_DMA_WAITOK | BUS_DMA_COHERENT, &pACB->srb_dmamap) != 0) 
	{
        bus_dma_tag_destroy(pACB->srb_dmat);
		bus_dma_tag_destroy(pACB->buffer_dmat);
		bus_dma_tag_destroy(pACB->parent_dmat);
		printf("arcmsr%d: srb_dmat bus_dmamem_alloc failure!\n",pACB->pci_unit);
		return ENXIO;
	}
	/* And permanently map them */
	if(bus_dmamap_load(pACB->srb_dmat, pACB->srb_dmamap,pACB->uncacheptr,(sizeof(struct _SRB) * ARCMSR_MAX_FREESRB_NUM)+0x20,arcmsr_map_freesrb, pACB, /*flags*/0))
	{
        bus_dma_tag_destroy(pACB->srb_dmat);
		bus_dma_tag_destroy(pACB->buffer_dmat);
		bus_dma_tag_destroy(pACB->parent_dmat);
		printf("arcmsr%d: srb_dmat bus_dmamap_load failure!\n",pACB->pci_unit);
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
		printf("arcmsr%d: bus_alloc_resource failure!\n",pACB->pci_unit);
		return ENOMEM;
	}
	if(rman_get_start(pACB->sys_res_arcmsr) <= 0)
	{
		arcmsr_free_resource(pACB);
		printf("arcmsr%d: rman_get_start failure!\n",pACB->pci_unit);
        return ENXIO;
	}
	mem_base=(vm_offset_t) rman_get_virtual(pACB->sys_res_arcmsr);
	if(mem_base==0)
	{
		arcmsr_free_resource(pACB);
		printf("arcmsr%d: rman_get_virtual failure!\n",pACB->pci_unit);
		return ENXIO;
	}
	if(pACB->acb_flags & ACB_F_MAPFREESRB_FAILD)
	{
		arcmsr_free_resource(pACB);
		printf("arcmsr%d: map free srb failure!\n",pACB->pci_unit);
		return ENXIO;
	}
	pACB->btag=rman_get_bustag(pACB->sys_res_arcmsr);
	pACB->bhandle=rman_get_bushandle(pACB->sys_res_arcmsr);
    pACB->pmu=(struct _MU *)mem_base;
    pACB->acb_flags |= (ACB_F_IOCTL_WQBUFFER_CLEARED|ACB_F_IOCTL_RQBUFFER_CLEARED);
	pACB->acb_flags &= ~ACB_F_SCSISTOPADAPTER;
	/*
	********************************************************************
	** init raid volume state
	********************************************************************
	*/
	for(i=0;i<ARCMSR_MAX_TARGETID;i++)
	{
		for(j=0;j<ARCMSR_MAX_TARGETLUN;j++)
		{
			pACB->devstate[i][j]=ARECA_RAID_GOOD;
		}
	}
    /* disable iop all outbound interrupt */
    intmask_org=readl(&pACB->pmu->outbound_intmask);
    writel(&pACB->pmu->outbound_intmask,intmask_org|ARCMSR_MU_OUTBOUND_ALL_INTMASKENABLE);
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
static u_int32_t arcmsr_attach(device_t dev)
{
	struct _ACB * pACB=(struct _ACB *)device_get_softc(dev);
	u_int32_t unit=device_get_unit(dev);
	struct ccb_setasync csa;
	struct cam_devq	*devq;	/* Device Queue to use for this SIM */
	struct resource	*irqres;
	int	rid;

	if(pACB == NULL) 
	{
		printf("arcmsr%d: cannot allocate softc\n",unit);
		return (ENOMEM);
	}
	bzero(pACB, sizeof(struct _ACB));
	if(arcmsr_initialize(dev)) 
	{
		printf("arcmsr%d: initialize failure!\n",unit);
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
		printf("arcmsr%d: cam_simq_alloc failure!\n",unit);
		return ENXIO;
	}
	pACB->psim=cam_sim_alloc(arcmsr_action,arcmsr_poll,"arcmsr",pACB,unit,1,ARCMSR_MAX_OUTSTANDING_CMD,devq);
	if(pACB->psim == NULL) 
	{
		arcmsr_free_resource(pACB);
		bus_release_resource(dev, SYS_RES_IRQ, 0, pACB->irqres);
		cam_simq_free(devq);
		printf("arcmsr%d: cam_sim_alloc failure!\n",unit);
		return ENXIO;
	}
	if(xpt_bus_register(pACB->psim,0) != CAM_SUCCESS) 
	{
		arcmsr_free_resource(pACB);
		bus_release_resource(dev, SYS_RES_IRQ, 0, pACB->irqres);
		cam_sim_free(pACB->psim,/*free_devq*/TRUE);
		printf("arcmsr%d: xpt_bus_register failure!\n",unit);
		return ENXIO;
	}
 	if(xpt_create_path(&pACB->ppath,/* periph */ NULL,cam_sim_path(pACB->psim),CAM_TARGET_WILDCARD,CAM_LUN_WILDCARD) != CAM_REQ_CMP) 
	{
		arcmsr_free_resource(pACB);
		bus_release_resource(dev, SYS_RES_IRQ, 0, pACB->irqres);
		xpt_bus_deregister(cam_sim_path(pACB->psim));
		cam_sim_free(pACB->psim,/* free_simq */ TRUE);
		printf("arcmsr%d: xpt_create_path failure!\n",unit);
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
 	return 0;
}
/*
************************************************************************
**
**                     
**
************************************************************************
*/
static u_int32_t arcmsr_probe(device_t dev)
{
	u_int32_t id;

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
    case PCIDevVenIDARC1170:
		device_set_desc(dev,"ARECA ARC1170 PCI-X 24 PORTS SATA RAID CONTROLLER (RAID6-ENGINE Inside) \n" ARCMSR_DRIVER_VERSION);
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
    case PCIDevVenIDARC1270:
		device_set_desc(dev,"ARECA ARC1270 PCI-EXPRESS 24 PORTS SATA RAID CONTROLLER (RAID6-ENGINE Inside) \n" ARCMSR_DRIVER_VERSION);
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
static void arcmsr_shutdown(device_t dev)
{
	u_int32_t  i,poll_count=0;
	u_int32_t s,intmask_org;
	struct _SRB * pSRB;
    struct _ACB * pACB=(struct _ACB *)device_get_softc(dev);

	s=splcam();
    /* disable all outbound interrupt */
    intmask_org=readl(&pACB->pmu->outbound_intmask);
    writel(&pACB->pmu->outbound_intmask,(intmask_org|ARCMSR_MU_OUTBOUND_ALL_INTMASKENABLE));
	/* stop adapter background rebuild */
	arcmsr_stop_adapter_bgrb(pACB);
	if(arcmsr_wait_msgint_ready(pACB))
	{
		printf("arcmsr%d: shutdown wait 'stop adapter rebulid' timeout \n",pACB->pci_unit);
	}
	arcmsr_flush_adapter_cache(pACB);
	if(arcmsr_wait_msgint_ready(pACB))
	{
		printf("arcmsr%d: shutdown wait 'flush adapter cache' timeout \n",pACB->pci_unit);
	}
	/* abort all outstanding command */
	pACB->acb_flags |= ACB_F_SCSISTOPADAPTER;
	pACB->acb_flags &= ~ACB_F_IOP_INITED;
	if(pACB->srboutstandingcount!=0)
	{  
		while((pACB->srboutstandingcount!=0) && (poll_count < 256))
		{
			arcmsr_interrupt((void *)pACB);
            UDELAY(25000);
			poll_count++;
		}
		if(pACB->srboutstandingcount!=0)
		{
			printf("arcmsr%d: shutdown srboutstandingcount!=0 \n",pACB->pci_unit);
			arcmsr_abort_allcmd(pACB);
			if(arcmsr_wait_msgint_ready(pACB))
			{
				printf("arcmsr%d: shutdown wait 'abort all outstanding command' timeout \n",pACB->pci_unit);
			}
			for(i=0;i<ARCMSR_MAX_FREESRB_NUM;i++)
			{
	    		pSRB=pACB->psrb_pool[i];
				if(pSRB->startdone==ARCMSR_SRB_START)
				{
					pSRB->startdone=ARCMSR_SRB_ABORTED;
					pSRB->pccb->ccb_h.status=CAM_REQ_ABORTED;
					arcmsr_srb_complete(pSRB);
				}
			}
		}
 	}
	if(pACB->srbwait2gocount!=0)
	{	/*remove first wait2go srb and abort it*/
		for(i=0;i<ARCMSR_MAX_OUTSTANDING_CMD;i++)
		{
			pSRB=pACB->psrbwait2go[i];
			if(pSRB!=NULL)
			{
				pACB->psrbwait2go[i]=NULL;
                pSRB->startdone=ARCMSR_SRB_ABORTED;
				pSRB->pccb->ccb_h.status=CAM_REQ_ABORTED; 
				arcmsr_srb_complete(pSRB);
				atomic_subtract_int(&pACB->srbwait2gocount,1);
			}
		}
	}
	atomic_set_int(&pACB->srboutstandingcount,0);
	splx(s);
    return;
}
/*
************************************************************************
**
**                     
**
************************************************************************
*/
static u_int32_t arcmsr_detach(device_t dev)
{
	struct _ACB * pACB=(struct _ACB *)device_get_softc(dev);

	arcmsr_shutdown(dev);
	arcmsr_free_resource(pACB);
	bus_release_resource(dev, SYS_RES_MEMORY, PCIR_BAR(0), pACB->sys_res_arcmsr);
	bus_teardown_intr(dev, pACB->irqres, pACB->ih);
	bus_release_resource(dev, SYS_RES_IRQ, 0, pACB->irqres);
	xpt_async(AC_LOST_DEVICE, pACB->ppath, NULL);
	xpt_free_path(pACB->ppath);
	xpt_bus_deregister(cam_sim_path(pACB->psim));
	cam_sim_free(pACB->psim, TRUE);
	return (0);
}



