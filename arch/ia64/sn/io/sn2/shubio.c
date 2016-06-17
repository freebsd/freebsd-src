/* $Id: shubio.c,v 1.1 2002/02/28 17:31:25 marcelo Exp $
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000,2002-2003 Silicon Graphics, Inc. All rights reserved.
 */


#include <linux/types.h>
#include <linux/slab.h>
#include <asm/smp.h>
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


error_state_t error_state_get(vertex_hdl_t v);
error_return_code_t error_state_set(vertex_hdl_t v,error_state_t new_state);


/*
 * Get the xtalk provider function pointer for the
 * specified hub.
 */

/*ARGSUSED*/
int
hub_xp_error_handler(
	vertex_hdl_t 	hub_v, 
	nasid_t		nasid, 
	int		error_code, 
	ioerror_mode_t	mode, 
	ioerror_t	*ioerror)
{
	/*REFERENCED*/
	hubreg_t	iio_imem;
	vertex_hdl_t	xswitch;
	error_state_t	e_state;
	cnodeid_t	cnode;

	/*
	 * Before walking down to the next level, check if
	 * the I/O link is up. If it's been disabled by the 
	 * hub ii for some reason, we can't even touch the
	 * widget registers.
	 */
	iio_imem = REMOTE_HUB_L(nasid, IIO_IMEM);

	if (!(iio_imem & (IIO_IMEM_B0ESD|IIO_IMEM_W0ESD))){
		/* 
		 * IIO_IMEM_B0ESD getting set, indicates II shutdown
		 * on HUB0 parts.. Hopefully that's not true for 
		 * Hub1 parts..
		 *
		 *
		 * If either one of them is shut down, can't
		 * go any further.
		 */
		return IOERROR_XTALKLEVEL;
	}

	/* Get the error state of the hub */
	e_state = error_state_get(hub_v);

	cnode = NASID_TO_COMPACT_NODEID(nasid);

	xswitch = NODEPDA(cnode)->basew_xc;

	/* Set the error state of the crosstalk device to that of
	 * hub.
	 */
	if (error_state_set(xswitch , e_state) == 
	    ERROR_RETURN_CODE_CANNOT_SET_STATE)
		return(IOERROR_UNHANDLED);

	/* Clean the error state of the hub if we are in the action handling
	 * phase.
	 */
	if (e_state == ERROR_STATE_ACTION)
		(void)error_state_set(hub_v, ERROR_STATE_NONE);
	/* hand the error off to the switch or the directly
	 * connected crosstalk device.
	 */
	return  xtalk_error_handler(xswitch,
				    error_code, mode, ioerror);

}

/* 
 * Check if the widget in error has been enabled for PIO accesses
 */
int
is_widget_pio_enabled(ioerror_t *ioerror)
{
	cnodeid_t	src_node;
	nasid_t		src_nasid;
	hubreg_t	ii_iowa;
	xwidgetnum_t	widget;
	iopaddr_t	p;

	/* Get the node where the PIO error occurred */
	IOERROR_GETVALUE(p,ioerror, srcnode);
	src_node = p;
	if (src_node == CNODEID_NONE)
		return(0);

	/* Get the nasid for the cnode */
	src_nasid = COMPACT_TO_NASID_NODEID(src_node);
	if (src_nasid == INVALID_NASID)
		return(0);

	/* Read the Outbound widget access register for this hub */
	ii_iowa = REMOTE_HUB_L(src_nasid, IIO_IOWA);
	IOERROR_GETVALUE(p,ioerror, widgetnum);
	widget = p;

	/* Check if the PIOs to the widget with PIO error have been
	 * enabled.
	 */
	if (ii_iowa & IIO_IOWA_WIDGET(widget))
		return(1);

	return(0);
}

/*
 * Hub IO error handling.
 *
 *	Gets invoked for different types of errors found at the hub. 
 *	Typically this includes situations from bus error or due to 
 *	an error interrupt (mostly generated at the hub).
 */
int
hub_ioerror_handler(
	vertex_hdl_t 	hub_v, 
	int		error_code,
	int		mode,
	struct io_error_s	*ioerror)
{
	hubinfo_t 	hinfo; 		/* Hub info pointer */
	nasid_t		nasid;
	int		retval = 0;
	/*REFERENCED*/
	iopaddr_t 	p;
	caddr_t 	cp;

	IOERROR_DUMP("hub_ioerror_handler", error_code, mode, ioerror);

	hubinfo_get(hub_v, &hinfo);

	if (!hinfo){
		/* Print an error message and return */
		goto end;
	}
	nasid = hinfo->h_nasid;

	switch(error_code) {

	case PIO_READ_ERROR:
		/* 
		 * Cpu got a bus error while accessing IO space.
		 * hubaddr field in ioerror structure should have
		 * the IO address that caused access error.
		 */

		/*
		 * Identify if  the physical address in hub_error_data
		 * corresponds to small/large window, and accordingly,
		 * get the xtalk address.
		 */

		/*
		 * Evaluate the widget number and the widget address that
		 * caused the error. Use 'vaddr' if it's there.
		 * This is typically true either during probing
		 * or a kernel driver getting into trouble. 
		 * Otherwise, use paddr to figure out widget details
		 * This is typically true for user mode bus errors while
		 * accessing I/O space.
		 */
		 IOERROR_GETVALUE(cp,ioerror,vaddr);
		 if (cp){
		    /* 
		     * If neither in small window nor in large window range,
		     * outright reject it.
		     */
		    IOERROR_GETVALUE(cp,ioerror,vaddr);
		    if (NODE_SWIN_ADDR(nasid, (paddr_t)cp)){
			iopaddr_t	hubaddr;
			xwidgetnum_t	widgetnum;
			iopaddr_t	xtalkaddr;

			IOERROR_GETVALUE(p,ioerror,hubaddr);
			hubaddr = p;
			widgetnum = SWIN_WIDGETNUM(hubaddr);
			xtalkaddr = SWIN_WIDGETADDR(hubaddr);
			/* 
			 * differentiate local register vs IO space access
			 */
			IOERROR_SETVALUE(ioerror,widgetnum,widgetnum);
			IOERROR_SETVALUE(ioerror,xtalkaddr,xtalkaddr);


		    } else if (NODE_BWIN_ADDR(nasid, (paddr_t)cp)){
			/* 
			 * Address corresponds to large window space. 
			 * Convert it to xtalk address.
			 */
			int		bigwin;
			hub_piomap_t    bw_piomap;
			xtalk_piomap_t	xt_pmap = NULL;
			iopaddr_t	hubaddr;
			xwidgetnum_t	widgetnum;
			iopaddr_t	xtalkaddr;

			IOERROR_GETVALUE(p,ioerror,hubaddr);
			hubaddr = p;

			/*
			 * Have to loop to find the correct xtalk_piomap 
			 * because the're not allocated on a one-to-one
			 * basis to the window number.
			 */
			for (bigwin=0; bigwin < HUB_NUM_BIG_WINDOW; bigwin++) {
				bw_piomap = hubinfo_bwin_piomap_get(hinfo,
								    bigwin);

				if (bw_piomap->hpio_bigwin_num ==
				    (BWIN_WINDOWNUM(hubaddr) - 1)) {
					xt_pmap = hub_piomap_xt_piomap(bw_piomap);
					break;
				}
			}

			ASSERT(xt_pmap);

			widgetnum = xtalk_pio_target_get(xt_pmap);
			xtalkaddr = xtalk_pio_xtalk_addr_get(xt_pmap) + BWIN_WIDGETADDR(hubaddr);

			IOERROR_SETVALUE(ioerror,widgetnum,widgetnum);
			IOERROR_SETVALUE(ioerror,xtalkaddr,xtalkaddr);

			/* 
			 * Make sure that widgetnum doesnot map to hub 
			 * register widget number, as we never use
			 * big window to access hub registers. 
			 */
			ASSERT(widgetnum != HUB_REGISTER_WIDGET);
		    }
		} else if (IOERROR_FIELDVALID(ioerror,hubaddr)) {
			iopaddr_t	hubaddr;
			xwidgetnum_t	widgetnum;
			iopaddr_t	xtalkaddr;

			IOERROR_GETVALUE(p,ioerror,hubaddr);
			hubaddr = p;
			if (BWIN_WINDOWNUM(hubaddr)){
				int 	window = BWIN_WINDOWNUM(hubaddr) - 1;
				hubreg_t itte;
				itte = (hubreg_t)HUB_L(IIO_ITTE_GET(nasid, window));
				widgetnum =  (itte >> IIO_ITTE_WIDGET_SHIFT) & 
						IIO_ITTE_WIDGET_MASK;
				xtalkaddr = (((itte >> IIO_ITTE_OFFSET_SHIFT) &
					IIO_ITTE_OFFSET_MASK) << 
					     BWIN_SIZE_BITS) +
					BWIN_WIDGETADDR(hubaddr);
			} else {
				widgetnum = SWIN_WIDGETNUM(hubaddr);
				xtalkaddr = SWIN_WIDGETADDR(hubaddr);
			}
			IOERROR_SETVALUE(ioerror,widgetnum,widgetnum);
			IOERROR_SETVALUE(ioerror,xtalkaddr,xtalkaddr);
		} else {
			IOERROR_DUMP("hub_ioerror_handler", error_code, 
						mode, ioerror);
			IOERR_PRINTF(printk(
				"hub_ioerror_handler: Invalid address passed"));

			return IOERROR_INVALIDADDR;
		}


		IOERROR_GETVALUE(p,ioerror,widgetnum);
		if ((p) == HUB_REGISTER_WIDGET) {
			/* 
			 * Error in accessing Hub local register
			 * This should happen mostly in SABLE mode..
			 */
			retval = 0;
		} else {
			/* Make sure that the outbound widget access for this
			 * widget is enabled.
			 */
			if (!is_widget_pio_enabled(ioerror)) {
				if (error_state_get(hub_v) == 
				    ERROR_STATE_ACTION)
					snia_ioerror_dump("No outbound widget access - ", 
						     error_code, mode, ioerror);
				return(IOERROR_HANDLED);
			}
		  

			retval = hub_xp_error_handler(
				hub_v, nasid, error_code, mode, ioerror);

		}

		IOERR_PRINTF(printk(
			"hub_ioerror_handler:PIO_READ_ERROR return: %d",
				retval));

		break;

	case PIO_WRITE_ERROR:
		/*
		 * This hub received an interrupt indicating a widget 
		 * attached to this hub got a timeout. 
		 * widgetnum field should be filled to indicate the
		 * widget that caused error.
		 *
		 * NOTE: This hub may have nothing to do with this error.
		 * We are here since the widget attached to the xbow 
		 * gets its PIOs through this hub.
		 *
		 * There is nothing that can be done at this level. 
		 * Just invoke the xtalk error handling mechanism.
		 */
		IOERROR_GETVALUE(p,ioerror,widgetnum);
		if ((p) == HUB_REGISTER_WIDGET) {
		} else {
			/* Make sure that the outbound widget access for this
			 * widget is enabled.
			 */

			if (!is_widget_pio_enabled(ioerror)) {
				if (error_state_get(hub_v) == 
				    ERROR_STATE_ACTION)
					snia_ioerror_dump("No outbound widget access - ", 
						     error_code, mode, ioerror);
				return(IOERROR_HANDLED);
			}
		  
			retval = hub_xp_error_handler(
				hub_v, nasid, error_code, mode, ioerror);
		}
		break;
	
	case DMA_READ_ERROR:
		/* 
		 * DMA Read error always ends up generating an interrupt
		 * at the widget level, and never at the hub level. So,
		 * we don't expect to come here any time
		 */
		ASSERT(0);
		retval = IOERROR_UNHANDLED;
		break;

	case DMA_WRITE_ERROR:
		/*
		 * DMA Write error is generated when a write by an I/O 
		 * device could not be completed. Problem is, device is
		 * totally unaware of this problem, and would continue
		 * writing to system memory. So, hub has a way to send
		 * an error interrupt on the first error, and bitbucket
		 * all further write transactions.
		 * Coming here indicates that hub detected one such error,
		 * and we need to handle it.
		 *
		 * Hub interrupt handler would have extracted physaddr, 
		 * widgetnum, and widgetdevice from the CRB 
		 *
		 * There is nothing special to do here, since gathering
		 * data from crb's is done elsewhere. Just pass the 
		 * error to xtalk layer.
		 */
		retval = hub_xp_error_handler(hub_v, nasid, error_code, mode,
					      ioerror);
		break;
	
	default:
		ASSERT(0);
		return IOERROR_BADERRORCODE;
	
	}
	
	/*
	 * If error was not handled, we may need to take certain action
	 * based on the error code.
	 * For e.g. in case of PIO_READ_ERROR, we may need to release the
	 * PIO Read entry table (they are sticky after errors).
	 * Similarly other cases. 
	 *
	 * Further Action TBD 
	 */
end:	
	if (retval == IOERROR_HWGRAPH_LOOKUP) {
		/*
		 * If we get errors very early, we can't traverse
		 * the path using hardware graph. 
		 * To handle this situation, we need a functions
		 * which don't depend on the hardware graph vertex to 
		 * handle errors. This break the modularity of the
		 * existing code. Instead we print out the reason for
		 * not handling error, and return. On return, all the
		 * info collected would be dumped. This should provide 
		 * sufficient info to analyse the error.
		 */
		printk("Unable to handle IO error: hardware graph not setup\n");
	}

	return retval;
}

#define INFO_LBL_ERROR_STATE    "error_state"

#define v_error_state_get(v,s)                                          \
(hwgraph_info_get_LBL(v,INFO_LBL_ERROR_STATE, (arbitrary_info_t *)&s))

#define v_error_state_set(v,s,replace)                                  \
(replace ?                                                              \
hwgraph_info_replace_LBL(v,INFO_LBL_ERROR_STATE,(arbitrary_info_t)s,0) :\
hwgraph_info_add_LBL(v,INFO_LBL_ERROR_STATE, (arbitrary_info_t)s))


#define v_error_state_clear(v)                                          \
(hwgraph_info_remove_LBL(v,INFO_LBL_ERROR_STATE,0))

/*
 * error_state_get
 *              Get the state of the vertex.
 *              Returns ERROR_STATE_INVALID on failure
 *                      current state otherwise
 */
error_state_t
error_state_get(vertex_hdl_t v)
{
        error_state_t   s;

        /* Check if we have a valid hwgraph vertex */
        if ( v == (vertex_hdl_t)0 )
                return(ERROR_STATE_NONE);

        /* Get the labelled info hanging off the vertex which corresponds
         * to the state.
         */
        if (v_error_state_get(v, s) != GRAPH_SUCCESS) {
                return(ERROR_STATE_NONE);
        }
        return(s);
}


/*
 * error_state_set
 *              Set the state of the vertex
 *              Returns ERROR_RETURN_CODE_CANNOT_SET_STATE on failure
 *                      ERROR_RETURN_CODE_SUCCESS otherwise
 */
error_return_code_t
error_state_set(vertex_hdl_t v,error_state_t new_state)
{
        error_state_t   old_state;
        int       replace = 1;

        /* Check if we have a valid hwgraph vertex */
        if ( v == (vertex_hdl_t)0 )
                return(ERROR_RETURN_CODE_GENERAL_FAILURE);


        /* This means that the error state needs to be cleaned */
        if (new_state == ERROR_STATE_NONE) {
                /* Make sure that we have an error state */
                if (v_error_state_get(v,old_state) == GRAPH_SUCCESS)
                        v_error_state_clear(v);
                return(ERROR_RETURN_CODE_SUCCESS);
        }

        /* Check if the state information has been set at least once
         * for this vertex.
         */
        if (v_error_state_get(v,old_state) != GRAPH_SUCCESS)
                replace = 0;

        if (v_error_state_set(v,new_state,replace) != GRAPH_SUCCESS) {
                return(ERROR_RETURN_CODE_CANNOT_SET_STATE);
        }
        return(ERROR_RETURN_CODE_SUCCESS);
}
