/* $Id: shuberror.c,v 1.1 2002/02/28 17:31:25 marcelo Exp $
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000,2002-2003 Silicon Graphics, Inc. All rights reserved.
 */


#include <linux/types.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/smp.h>
#include <asm/delay.h>
#include <asm/sn/sgi.h>
#include <asm/sn/io.h>
#include <asm/sn/iograph.h>
#include <asm/sn/invent.h>
#include <asm/sn/hcl.h>
#include <asm/sn/labelcl.h>
#include <asm/sn/sn_private.h>
#include <asm/sn/klconfig.h>
#include <asm/sn/sn_cpuid.h>
#include <asm/sn/pci/pciio.h>
#include <asm/sn/pci/pcibr.h>
#include <asm/sn/xtalk/xtalk.h>
#include <asm/sn/pci/pcibr_private.h>
#include <asm/sn/intr.h>
#include <asm/sn/ioerror_handling.h>
#include <asm/sn/ioerror.h>
#include <asm/sn/sn2/shubio.h>
#include <asm/sn/bte.h>

extern void hubni_eint_init(cnodeid_t cnode);
extern void hubii_eint_init(cnodeid_t cnode);
extern void hubii_eint_handler (int irq, void *arg, struct pt_regs *ep);
int hubiio_crb_error_handler(vertex_hdl_t hub_v, hubinfo_t hinfo);
int hubiio_prb_error_handler(vertex_hdl_t hub_v, hubinfo_t hinfo);
extern void bte_crb_error_handler(vertex_hdl_t hub_v, int btenum, int crbnum, ioerror_t *ioe, int bteop);
void print_crb_fields(int crb_num, ii_icrb0_a_u_t icrba,
	ii_icrb0_b_u_t icrbb, ii_icrb0_c_u_t icrbc,
	ii_icrb0_d_u_t icrbd, ii_icrb0_e_u_t icrbe);

extern int maxcpus;
extern error_return_code_t error_state_set(vertex_hdl_t v,error_state_t new_state);

#define HUB_ERROR_PERIOD        (120 * HZ)      /* 2 minutes */

void
hub_error_clear(nasid_t nasid)
{
	int i;

    /*
     * Make sure spurious write response errors are cleared
     * (values are from hub_set_prb())
     */
    for (i = 0; i <= HUB_WIDGET_ID_MAX - HUB_WIDGET_ID_MIN + 1; i++) {
        iprb_t prb;

	prb.iprb_regval = REMOTE_HUB_L(nasid, IIO_IOPRB_0 + (i * sizeof(hubreg_t)));

        /* Clear out some fields */
        prb.iprb_ovflow = 1;
        prb.iprb_bnakctr = 0;
        prb.iprb_anakctr = 0;

        prb.iprb_xtalkctr = 3;  /* approx. PIO credits for the widget */

        REMOTE_HUB_S(nasid, IIO_IOPRB_0 + (i * sizeof(hubreg_t)), prb.iprb_regval);
    }

    REMOTE_HUB_S(nasid, IIO_IECLR, -1);

}


/*
 * Function	: hub_error_init
 * Purpose	: initialize the error handling requirements for a given hub.
 * Parameters	: cnode, the compact nodeid.
 * Assumptions	: Called only once per hub, either by a local cpu. Or by a 
 *			remote cpu, when this hub is headless.(cpuless)
 * Returns	: None
 */

void
hub_error_init(cnodeid_t cnode)
{
	nasid_t nasid;

    nasid = cnodeid_to_nasid(cnode);
    hub_error_clear(nasid);


    /*
     * Now setup the hub ii error interrupt handler.
     */

    hubii_eint_init(cnode);

    return;
}

/*
 * Function	: hubii_eint_init
 * Parameters	: cnode
 * Purpose	: to initialize the hub iio error interrupt.
 * Assumptions	: Called once per hub, by the cpu which will ultimately
 *			handle this interrupt.
 * Returns	: None.
 */

void
hubii_eint_init(cnodeid_t cnode)
{
    int			bit, rv;
    ii_iidsr_u_t    	hubio_eint;
    hubinfo_t		hinfo; 
    cpuid_t		intr_cpu;
    vertex_hdl_t 	hub_v;
    int bit_pos_to_irq(int bit);
    ii_ilcsr_u_t	ilcsr;


    hub_v = (vertex_hdl_t)cnodeid_to_vertex(cnode);
    ASSERT_ALWAYS(hub_v);
    hubinfo_get(hub_v, &hinfo);

    ASSERT(hinfo);
    ASSERT(hinfo->h_cnodeid == cnode);

    ilcsr.ii_ilcsr_regval = REMOTE_HUB_L(hinfo->h_nasid, IIO_ILCSR);
    if ((ilcsr.ii_ilcsr_fld_s.i_llp_stat & 0x2) == 0) {
	/*
	 * HUB II link is not up.  Disable LLP. Clear old errors.
	 * Enable interrupts to handle BTE errors.
	 */
	ilcsr.ii_ilcsr_fld_s.i_llp_en = 0;
	REMOTE_HUB_S(hinfo->h_nasid, IIO_ILCSR, ilcsr.ii_ilcsr_regval);
    }

    /* Select a possible interrupt target where there is a free interrupt
     * bit and also reserve the interrupt bit for this IO error interrupt
     */
    intr_cpu = intr_heuristic(hub_v,0,SGI_II_ERROR,0,hub_v,
			      "HUB IO error interrupt",&bit);
    if (intr_cpu == CPU_NONE) {
	printk("hubii_eint_init: intr_reserve_level failed, cnode %d", cnode);
	return;
    }
	
    rv = intr_connect_level(intr_cpu, SGI_II_ERROR, 0, NULL);
    request_irq(SGI_II_ERROR, hubii_eint_handler, SA_SHIRQ, "SN_hub_error", (void *)hub_v);
    irq_desc(bit)->status |= SN2_IRQ_PER_HUB;
    ASSERT_ALWAYS(rv >= 0);
    hubio_eint.ii_iidsr_regval = 0;
    hubio_eint.ii_iidsr_fld_s.i_enable = 1;
    hubio_eint.ii_iidsr_fld_s.i_level = bit;/* Take the least significant bits*/
    hubio_eint.ii_iidsr_fld_s.i_node = COMPACT_TO_NASID_NODEID(cnode);
    hubio_eint.ii_iidsr_fld_s.i_pi_id = cpuid_to_subnode(intr_cpu);
    REMOTE_HUB_S(hinfo->h_nasid, IIO_IIDSR, hubio_eint.ii_iidsr_regval);

}


/*ARGSUSED*/
void
hubii_eint_handler (int irq, void *arg, struct pt_regs *ep)
{
    vertex_hdl_t	hub_v;
    hubinfo_t		hinfo; 
    ii_wstat_u_t	wstat;
    hubreg_t		idsr;
    ii_ilcsr_u_t	ilcsr;


    /* two levels of casting avoids compiler warning.!! */
    hub_v = (vertex_hdl_t)(long)(arg); 
    ASSERT(hub_v);

    hubinfo_get(hub_v, &hinfo);
    
    idsr = REMOTE_HUB_L(hinfo->h_nasid, IIO_ICMR);
#if 0
    if (idsr & 0x1) {
	/* ICMR bit is set .. we are getting into "Spurious Interrupts condition. */
	printk("Cnode %d II has seen the ICMR condition\n", hinfo->h_cnodeid);
	printk("***** Please file PV with the above messages *****\n");
	/* panic("We have to panic to prevent further unknown states ..\n"); */
    }
#endif
	
    /* 
     * Identify the reason for error. 
     */
    wstat.ii_wstat_regval = REMOTE_HUB_L(hinfo->h_nasid, IIO_WSTAT);

    if (wstat.ii_wstat_fld_s.w_crazy) {
	char	*reason;
	/*
	 * We can do a couple of things here. 
	 * Look at the fields TX_MX_RTY/XT_TAIL_TO/XT_CRD_TO to check
	 * which of these caused the CRAZY bit to be set. 
	 * You may be able to check if the Link is up really.
	 */
	if (wstat.ii_wstat_fld_s.w_tx_mx_rty)
		reason = "Micro Packet Retry Timeout";
	else if (wstat.ii_wstat_fld_s.w_xt_tail_to)
		reason = "Crosstalk Tail Timeout";
	else if (wstat.ii_wstat_fld_s.w_xt_crd_to)
		reason = "Crosstalk Credit Timeout";
	else {
		hubreg_t	hubii_imem;
		/*
		 * Check if widget 0 has been marked as shutdown, or
		 * if BTE 0/1 has been marked.
		 */
		hubii_imem = REMOTE_HUB_L(hinfo->h_nasid, IIO_IMEM);
		if (hubii_imem & IIO_IMEM_W0ESD)
			reason = "Hub Widget 0 has been Shutdown";
		else if (hubii_imem & IIO_IMEM_B0ESD)
			reason = "BTE 0 has been shutdown";
		else if (hubii_imem & IIO_IMEM_B1ESD)
			reason = "BTE 1 has been shutdown";
		else	reason = "Unknown";
	
	}
	/*
	 * Note: we may never be able to print this, if the II talking
	 * to Xbow which hosts the console is dead. 
	 */
	ilcsr.ii_ilcsr_regval = REMOTE_HUB_L(hinfo->h_nasid, IIO_ILCSR);
	if (ilcsr.ii_ilcsr_fld_s.i_llp_en == 1) {	/* Link is enabled */
	    printk("Hub %d, cnode %d to Xtalk Link failed (II_ECRAZY) Reason: %s", 
		hinfo->h_nasid, hinfo->h_cnodeid, reason);
	}
    }


    /*
     * Before processing any interrupt related information, clear all
     * error indication and reenable interrupts.  This will prevent
     * lost interrupts due to the interrupt handler scanning past a PRB/CRB
     * which has not errorred yet and then the PRB/CRB goes into error.
     * Note, PRB errors are cleared individually.
     */
    REMOTE_HUB_S(hinfo->h_nasid, IIO_IECLR, 0xff0000);
    idsr = REMOTE_HUB_L(hinfo->h_nasid, IIO_IIDSR) & ~IIO_IIDSR_SENT_MASK;
    REMOTE_HUB_S(hinfo->h_nasid, IIO_IIDSR, idsr);


    /* 
     * It's a toss as to which one among PRB/CRB to check first. 
     * Current decision is based on the severity of the errors. 
     * IO CRB errors tend to be more severe than PRB errors.
     *
     * It is possible for BTE errors to have been handled already, so we
     * may not see any errors handled here. 
     */
    (void)hubiio_crb_error_handler(hub_v, hinfo);
    (void)hubiio_prb_error_handler(hub_v, hinfo);
}

/*
 * Free the hub CRB "crbnum" which encountered an error.
 * Assumption is, error handling was successfully done,
 * and we now want to return the CRB back to Hub for normal usage.
 *
 * In order to free the CRB, all that's needed is to de-allocate it
 *
 * Assumption:
 *      No other processor is mucking around with the hub control register.
 *      So, upper layer has to single thread this.
 */
void
hubiio_crb_free(hubinfo_t hinfo, int crbnum)
{
	ii_icrb0_b_u_t         icrbb;

	/*
	* The hardware does NOT clear the mark bit, so it must get cleared
	* here to be sure the error is not processed twice.
	*/
	icrbb.ii_icrb0_b_regval = REMOTE_HUB_L(hinfo->h_nasid, IIO_ICRB_B(crbnum));
	icrbb.b_mark   = 0;
	REMOTE_HUB_S(hinfo->h_nasid, IIO_ICRB_B(crbnum), icrbb.ii_icrb0_b_regval);

	/*
	* Deallocate the register.
	*/

	REMOTE_HUB_S(hinfo->h_nasid, IIO_ICDR, (IIO_ICDR_PND | crbnum));

	/*
	* Wait till hub indicates it's done.
	*/
	while (REMOTE_HUB_L(hinfo->h_nasid, IIO_ICDR) & IIO_ICDR_PND)
		udelay(1);

}


/*
 * Array of error names  that get logged in CRBs
 */ 
char *hubiio_crb_errors[] = {
	"Directory Error",
	"CRB Poison Error",
	"I/O Write Error",
	"I/O Access Error",
	"I/O Partial Write Error",
	"I/O Partial Read Error",
	"I/O Timeout Error",
	"Xtalk Error Packet"
};

void
print_crb_fields(int crb_num, ii_icrb0_a_u_t icrba,
	ii_icrb0_b_u_t icrbb, ii_icrb0_c_u_t icrbc,
	ii_icrb0_d_u_t icrbd, ii_icrb0_e_u_t icrbe)
{
    printk("CRB %d regA\n\t"
	    "a_iow 0x%x\n\t"
	    "valid0x%x\n\t"
	    "Address0x%lx\n\t"
	    "a_tnum 0x%x\n\t"
	    "a_sidn 0x%x\n",
	    crb_num,
	    icrba.a_iow, 
	    icrba.a_valid, 
	    icrba.a_addr, 
	    icrba.a_tnum, 
	    icrba.a_sidn);
    printk("CRB %d regB\n\t"
	    "b_imsgtype 0x%x\n\t"
	    "b_imsg 0x%x\n"
	    "\tb_use_old 0x%x\n\t"
	    "b_initiator 0x%x\n\t"
	    "b_exc 0x%x\n"
	    "\tb_ackcnt 0x%x\n\t"
	    "b_resp 0x%x\n\t"
	    "b_ack 0x%x\n"
	    "\tb_hold 0x%x\n\t"
	    "b_wb 0x%x\n\t"
	    "b_intvn 0x%x\n"
	    "\tb_stall_ib 0x%x\n\t"
	    "b_stall_int 0x%x\n"
	    "\tb_stall_bte_0 0x%x\n\t"
	    "b_stall_bte_1 0x%x\n"
	    "\tb_error 0x%x\n\t"
	    "b_lnetuce 0x%x\n\t"
	    "b_mark 0x%x\n\t"
	    "b_xerr 0x%x\n",
	    crb_num,
	    icrbb.b_imsgtype, 
	    icrbb.b_imsg, 
	    icrbb.b_use_old, 
	    icrbb.b_initiator,
	    icrbb.b_exc, 
	    icrbb.b_ackcnt, 
	    icrbb.b_resp, 
	    icrbb.b_ack, 
	    icrbb.b_hold,
	    icrbb.b_wb, 
	    icrbb.b_intvn, 
	    icrbb.b_stall_ib, 
	    icrbb.b_stall_int,
	    icrbb.b_stall_bte_0, 
	    icrbb.b_stall_bte_1, 
	    icrbb.b_error,
	    icrbb.b_lnetuce, 
	    icrbb.b_mark, 
	    icrbb.b_xerr);
    printk("CRB %d regC\n\t"
	    "c_source 0x%x\n\t"
	    "c_xtsize 0x%x\n\t"
	    "c_cohtrans 0x%x\n\t"
	    "c_btenum 0x%x\n\t"
	    "c_gbr 0x%x\n\t"
	    "c_doresp 0x%x\n\t"
	    "c_barrop 0x%x\n\t"
	    "c_suppl 0x%x\n",
	    crb_num,
	    icrbc.c_source,
	    icrbc.c_xtsize,
	    icrbc.c_cohtrans,
	    icrbc.c_btenum,
	    icrbc.c_gbr,
	    icrbc.c_doresp,
	    icrbc.c_barrop,
	    icrbc.c_suppl);
    printk("CRB %d regD\n\t"
	    "d_bteaddr 0x%lx\n\t"
	    "d_bteop 0x%x\n\t"
	    "d_pripsc 0x%x\n\t"
	    "d_pricnt 0x%x\n\t"
	    "d_sleep 0x%x\n\t",
	    crb_num,
	    icrbd.d_bteaddr,
	    icrbd.d_bteop,
	    icrbd.d_pripsc,
	    icrbd.d_pricnt,
	    icrbd.d_sleep);
    printk("CRB %d regE\n\t"
	    "icrbe_timeout 0x%x\n\t"
	    "icrbe_context 0x%x\n\t"
	    "icrbe_toutvld 0x%x\n\t"
	    "icrbe_ctxtvld 0x%x\n\t",
	    crb_num,
	    icrbe.icrbe_timeout,
	    icrbe.icrbe_context,
	    icrbe.icrbe_toutvld,
	    icrbe.icrbe_ctxtvld);
}

/*
 * hubiio_crb_error_handler
 *
 *	This routine gets invoked when a hub gets an error 
 *	interrupt. So, the routine is running in interrupt context
 *	at error interrupt level.
 * Action:
 *	It's responsible for identifying ALL the CRBs that are marked
 *	with error, and process them. 
 *	
 * 	If you find the CRB that's marked with error, map this to the
 *	reason it caused error, and invoke appropriate error handler.
 *
 *	XXX Be aware of the information in the context register.
 *
 * NOTE:
 *	Use REMOTE_HUB_* macro instead of LOCAL_HUB_* so that the interrupt
 *	handler can be run on any node. (not necessarily the node 
 *	corresponding to the hub that encountered error).
 */

int
hubiio_crb_error_handler(vertex_hdl_t hub_v, hubinfo_t hinfo)
{
	cnodeid_t	cnode;
	nasid_t		nasid;
	ii_icrb0_a_u_t		icrba;		/* II CRB Register A */
	ii_icrb0_b_u_t		icrbb;		/* II CRB Register B */
	ii_icrb0_c_u_t		icrbc;		/* II CRB Register C */
	ii_icrb0_d_u_t		icrbd;		/* II CRB Register D */
	ii_icrb0_e_u_t		icrbe;		/* II CRB Register D */
	int		i;
	int		num_errors = 0;	/* Num of errors handled */
	ioerror_t	ioerror;
	int		rc;

	nasid = hinfo->h_nasid;
	cnode = NASID_TO_COMPACT_NODEID(nasid);

	/*
	 * XXX - Add locking for any recovery actions
	 */
	/*
	 * Scan through all CRBs in the Hub, and handle the errors
	 * in any of the CRBs marked.
	 */
	for (i = 0; i < IIO_NUM_CRBS; i++) {
		/* Check this crb entry to see if it is in error. */
		icrbb.ii_icrb0_b_regval = REMOTE_HUB_L(nasid, IIO_ICRB_B(i));

		if (icrbb.b_mark == 0) {
			continue;
		}

		icrba.ii_icrb0_a_regval = REMOTE_HUB_L(nasid, IIO_ICRB_A(i));

		IOERROR_INIT(&ioerror);

		/* read other CRB error registers. */
		icrbc.ii_icrb0_c_regval = REMOTE_HUB_L(nasid, IIO_ICRB_C(i));
		icrbd.ii_icrb0_d_regval = REMOTE_HUB_L(nasid, IIO_ICRB_D(i));
		icrbe.ii_icrb0_e_regval = REMOTE_HUB_L(nasid, IIO_ICRB_E(i));

		IOERROR_SETVALUE(&ioerror,errortype,icrbb.b_ecode);

		/* Check if this error is due to BTE operation,
		* and handle it separately.
		*/
		if (icrbd.d_bteop ||
			((icrbb.b_initiator == IIO_ICRB_INIT_BTE0 ||
			icrbb.b_initiator == IIO_ICRB_INIT_BTE1) &&
			(icrbb.b_imsgtype == IIO_ICRB_IMSGT_BTE ||
			icrbb.b_imsgtype == IIO_ICRB_IMSGT_SN1NET))){

			int bte_num;

			if (icrbd.d_bteop)
				bte_num = icrbc.c_btenum;
			else /* b_initiator bit 2 gives BTE number */
				bte_num = (icrbb.b_initiator & 0x4) >> 2;

			hubiio_crb_free(hinfo, i);

			bte_crb_error_handler(hub_v, bte_num,
					      i, &ioerror,
					      icrbd.d_bteop);
			num_errors++;
			continue;
		}

		/*
		 * XXX
		 * Assuming the only other error that would reach here is
		 * crosstalk errors. 
		 * If CRB times out on a message from Xtalk, it changes 
		 * the message type to CRB. 
		 *
		 * If we get here due to other errors (SN0net/CRB)
		 * what's the action ?
		 */

		/*
		 * Pick out the useful fields in CRB, and
		 * tuck them away into ioerror structure.
		 */
		IOERROR_SETVALUE(&ioerror,xtalkaddr,icrba.a_addr << IIO_ICRB_ADDR_SHFT);
		IOERROR_SETVALUE(&ioerror,widgetnum,icrba.a_sidn);


		if (icrba.a_iow){
			/*
			 * XXX We shouldn't really have BRIDGE-specific code
			 * here, but alas....
			 *
			 * The BRIDGE (or XBRIDGE) sets the upper bit of TNUM
			 * to indicate a WRITE operation.  It sets the next
			 * bit to indicate an INTERRUPT operation.  The bottom
			 * 3 bits of TNUM indicate which device was responsible.
			 */
			IOERROR_SETVALUE(&ioerror,widgetdev,
					 TNUM_TO_WIDGET_DEV(icrba.a_tnum));
			/*
			* The encoding of TNUM (see comments above) is
			* different for PIC. So we'll save TNUM here and
			* deal with the differences later when we can
			* determine if we're using a Bridge or the PIC.
			*
			* XXX:  We may be able to remove saving the widgetdev
			* above and just sort it out of TNUM later.
			*/
			IOERROR_SETVALUE(&ioerror, tnum, icrba.a_tnum);

		}
		if (icrbb.b_error) {
		    /*
		     * CRB 'i' has some error. Identify the type of error,
		     * and try to handle it.
		     *
		     */
		    switch(icrbb.b_ecode) {
			case IIO_ICRB_ECODE_PERR:
			case IIO_ICRB_ECODE_WERR:
			case IIO_ICRB_ECODE_AERR:
			case IIO_ICRB_ECODE_PWERR:
			case IIO_ICRB_ECODE_TOUT:
			case IIO_ICRB_ECODE_XTERR:
			    printk("Shub II CRB %d: error %s on hub cnodeid: %d",
				    i, hubiio_crb_errors[icrbb.b_ecode], cnode);
			    /*
			     * Any sort of write error is mostly due
			     * bad programming (Note it's not a timeout.)
			     * So, invoke hub_iio_error_handler with
			     * appropriate information.
			     */
			    IOERROR_SETVALUE(&ioerror,errortype,icrbb.b_ecode);

			    /* Go through the error bit lookup phase */
			    if (error_state_set(hub_v, ERROR_STATE_LOOKUP) ==
				    ERROR_RETURN_CODE_CANNOT_SET_STATE)
				return(IOERROR_UNHANDLED);
			    rc = hub_ioerror_handler(
				    hub_v,
				    DMA_WRITE_ERROR,
				    MODE_DEVERROR,
				    &ioerror);
			    if (rc == IOERROR_HANDLED) {
				rc = hub_ioerror_handler(
					hub_v,
					DMA_WRITE_ERROR,
					MODE_DEVREENABLE,
					&ioerror);
			    }else {
				printk("Unable to handle %s on hub %d",
					hubiio_crb_errors[icrbb.b_ecode],
					cnode);
				/* panic; */
			    }
			    /* Go to Next error */
			    print_crb_fields(i, icrba, icrbb, icrbc,
				    icrbd, icrbe);
			    hubiio_crb_free(hinfo, i);
			    continue;
			case IIO_ICRB_ECODE_PRERR:
			case IIO_ICRB_ECODE_DERR:
			    printk("Shub II CRB %d: error %s on hub : %d",
				    i, hubiio_crb_errors[icrbb.b_ecode], cnode);
			    /* panic */
			default:
			    printk("Shub II CRB error (code : %d) on hub : %d",
				    icrbb.b_ecode, cnode);
			    /* panic */
		    }
		} 
		/*
		 * Error is not indicated via the errcode field
		 * Check other error indications in this register.
		 */
		if (icrbb.b_xerr) {
		    printk("Shub II CRB %d: Xtalk Packet with error bit set to hub %d",
			    i, cnode);
		    /* panic */
		}
		if (icrbb.b_lnetuce) {
		    printk("Shub II CRB %d: Uncorrectable data error detected on data "
			    " from NUMAlink to node %d",
			    i, cnode);
		    /* panic */
		}
		print_crb_fields(i, icrba, icrbb, icrbc, icrbd, icrbe);





		if (icrbb.b_error) {
		/* 
		 * CRB 'i' has some error. Identify the type of error,
		 * and try to handle it.
		 */
		switch(icrbb.b_ecode) {
		case IIO_ICRB_ECODE_PERR:
		case IIO_ICRB_ECODE_WERR:
		case IIO_ICRB_ECODE_AERR:
		case IIO_ICRB_ECODE_PWERR:

			printk("%s on hub cnodeid: %d",
				hubiio_crb_errors[icrbb.b_ecode], cnode);
			/*
			 * Any sort of write error is mostly due
			 * bad programming (Note it's not a timeout.)
			 * So, invoke hub_iio_error_handler with
			 * appropriate information.
			 */
			IOERROR_SETVALUE(&ioerror,errortype,icrbb.b_ecode);

			rc = hub_ioerror_handler(
					hub_v, 
					DMA_WRITE_ERROR, 
					MODE_DEVERROR, 
					&ioerror);

                        if (rc == IOERROR_HANDLED) {
                                rc = hub_ioerror_handler(
                                        hub_v,
                                        DMA_WRITE_ERROR,
                                        MODE_DEVREENABLE,
                                        &ioerror);
                                ASSERT(rc == IOERROR_HANDLED);
                        }else {

				panic("Unable to handle %s on hub %d",
					hubiio_crb_errors[icrbb.b_ecode],
					cnode);
				/*NOTREACHED*/
			}
			/* Go to Next error */
			hubiio_crb_free(hinfo, i);
			continue;

		case IIO_ICRB_ECODE_PRERR:

                case IIO_ICRB_ECODE_TOUT:
                case IIO_ICRB_ECODE_XTERR:

		case IIO_ICRB_ECODE_DERR:
			panic("Fatal %s on hub : %d",
				hubiio_crb_errors[icrbb.b_ecode], cnode);
			/*NOTREACHED*/
		
		default:
			panic("Fatal error (code : %d) on hub : %d",
				icrbb.b_ecode, cnode);
			/*NOTREACHED*/

		}
		} 	/* if (icrbb.b_error) */	

		/*
		 * Error is not indicated via the errcode field 
		 * Check other error indications in this register.
		 */
		
		if (icrbb.b_xerr) {
			panic("Xtalk Packet with error bit set to hub %d",
				cnode);
			/*NOTREACHED*/
		}

		if (icrbb.b_lnetuce) {
			panic("Uncorrectable data error detected on data "
				" from Craylink to node %d",
				cnode);
			/*NOTREACHED*/
		}

	}
	return	num_errors;
}

/*
 * hubii_check_widget_disabled
 *
 *	Check if PIO access to the specified widget is disabled due
 *	to any II errors that are currently set.
 *
 *	The specific error bits checked are:
 *		IPRBx register: SPUR_RD (51)
 *				SPUR_WR (50)
 *				RD_TO (49)
 *				ERROR (48)
 *
 *		WSTAT register: CRAZY (32)
 */

int
hubii_check_widget_disabled(nasid_t nasid, int wnum)
{
	iprb_t		iprb;
	ii_wstat_u_t	wstat;

	iprb.iprb_regval = REMOTE_HUB_L(nasid, IIO_IOPRB(wnum));
	if (iprb.iprb_regval & (IIO_PRB_SPUR_RD | IIO_PRB_SPUR_WR |
		IIO_PRB_RD_TO | IIO_PRB_ERROR)) {
#ifdef DEBUG
	    printk(KERN_WARNING "II error, IPRB%x=0x%lx\n", wnum, iprb.iprb_regval);
#endif
	    return(1);
	}

	wstat.ii_wstat_regval = REMOTE_HUB_L(nasid, IIO_WSTAT);
	if (wstat.ii_wstat_regval & IIO_WSTAT_ECRAZY) {
#ifdef DEBUG
	    printk(KERN_WARNING "II error, WSTAT=0x%lx\n", wstat.ii_wstat_regval);
#endif
	    return(1);
	}
	return(0);
}

/*ARGSUSED*/
/*
 * hubii_prb_handler
 *      Handle the error reported in the PRB for wiget number wnum.
 *      This typically happens on a PIO write error.
 *      There is nothing much we can do in this interrupt context for
 *      PIO write errors. For e.g. QL scsi controller has the
 *      habit of flaking out on PIO writes.
 *      Print a message and try to continue for now
 *      Cleanup involes freeing the PRB register
 */
static void
hubii_prb_handler(vertex_hdl_t hub_v, hubinfo_t hinfo, int wnum)
{
        nasid_t         nasid;

        nasid = hinfo->h_nasid;
        /*
         * Clear error bit by writing to IECLR register.
         */
        REMOTE_HUB_S(nasid, IIO_IECLR, (1 << wnum));
        /*
         * PIO Write to Widget 'i' got into an error.
         * Invoke hubiio_error_handler with this information.
         */
        printk( "Hub nasid %d got a PIO Write error from widget %d, "
				"cleaning up and continuing", nasid, wnum);
        /*
         * XXX
         * It may be necessary to adjust IO PRB counter
         * to account for any lost credits.
         */
}

int
hubiio_prb_error_handler(vertex_hdl_t hub_v, hubinfo_t hinfo)
{
        int             wnum;
        nasid_t         nasid;
        int             num_errors = 0;
        iprb_t          iprb;

        nasid = hinfo->h_nasid;
        /*
         * Check if IPRB0 has any error first.
         */
        iprb.iprb_regval = REMOTE_HUB_L(nasid, IIO_IOPRB(0));
        if (iprb.iprb_error) {
                num_errors++;
                hubii_prb_handler(hub_v, hinfo, 0);
        }
        /*
         * Look through PRBs 8 - F to see if any of them has error bit set.
         * If true, invoke hub iio error handler for this widget.
         */
        for (wnum = HUB_WIDGET_ID_MIN; wnum <= HUB_WIDGET_ID_MAX; wnum++) {
                iprb.iprb_regval = REMOTE_HUB_L(nasid, IIO_IOPRB(wnum));

                if (!iprb.iprb_error)
                        continue;

                num_errors++;
                hubii_prb_handler(hub_v, hinfo, wnum);
        }

        return num_errors;
}

