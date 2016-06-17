/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 1992-1997,2000-2003 Silicon Graphics, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <asm/sn/sgi.h>
#include <asm/delay.h>
#include <asm/sn/intr.h>
#include <asm/sn/sn2/sn_private.h>
#include <asm/sn/sn2/shubio.h>
#include <asm/sn/iograph.h>
#include <asm/sn/invent.h>
#include <asm/sn/hcl.h>
#include <asm/sn/labelcl.h>
#include <asm/sn/pci/bridge.h>
#include <asm/sn/xtalk/xtalk_private.h>
#include <asm/sn/simulator.h>

/* #define DEBUG		1 */
/* #define XBOW_DEBUG	1 */
/* #define DEBUG_ERROR	1 */

#define kdebug 0


/*
 * Files needed to get the device driver entry points
 */

#include <asm/sn/xtalk/xbow.h>
#include <asm/sn/xtalk/xtalk.h>
#include <asm/sn/xtalk/xswitch.h>
#include <asm/sn/xtalk/xwidget.h>

#include <asm/sn/prio.h>
#include <asm/sn/hcl_util.h>


#define NEW(ptr)	(ptr = kmalloc(sizeof (*(ptr)), GFP_KERNEL))
#define DEL(ptr)	(kfree(ptr))

/*
 * This file supports the Xbow chip.  Main functions: initializtion,
 * error handling, and GBR.
 */

/*
 * each vertex corresponding to an xbow chip
 * has a "fastinfo" pointer pointing at one
 * of these things.
 */
typedef struct xbow_soft_s *xbow_soft_t;

struct xbow_soft_s {
    vertex_hdl_t            conn;	/* our connection point */
    vertex_hdl_t            vhdl;	/* xbow's private vertex */
    vertex_hdl_t            busv;	/* the xswitch vertex */
    xbow_t                 *base;	/* PIO pointer to crossbow chip */
    char                   *name;	/* hwgraph name */

    xbow_perf_t             xbow_perfcnt[XBOW_PERF_COUNTERS];
    xbow_perf_link_t        xbow_perflink[MAX_XBOW_PORTS];
    xbow_link_status_t      xbow_link_status[MAX_XBOW_PORTS];
    spinlock_t              xbow_perf_lock;
    int                     link_monitor;
    widget_cfg_t	   *wpio[MAX_XBOW_PORTS];	/* cached PIO pointer */

    /* Bandwidth allocation state. Bandwidth values are for the
     * destination port since contention happens there.
     * Implicit mapping from xbow ports (8..f) -> (0..7) array indices.
     */
    spinlock_t		    xbow_bw_alloc_lock;		/* bw allocation lock */
    unsigned long long	    bw_hiwm[MAX_XBOW_PORTS];	/* hiwater mark values */
    unsigned long long      bw_cur_used[MAX_XBOW_PORTS]; /* bw used currently */
};

#define xbow_soft_set(v,i)	hwgraph_fastinfo_set((v), (arbitrary_info_t)(i))
#define xbow_soft_get(v)	((xbow_soft_t)hwgraph_fastinfo_get((v)))

/*
 * Function Table of Contents
 */

void                    xbow_mlreset(xbow_t *);
int                     xbow_attach(vertex_hdl_t);

int                     xbow_widget_present(xbow_t *, int);
static int              xbow_link_alive(xbow_t *, int);
vertex_hdl_t            xbow_widget_lookup(vertex_hdl_t, int);

void                    xbow_intr_preset(void *, int, xwidgetnum_t, iopaddr_t, xtalk_intr_vector_t);



void                    xbow_update_perf_counters(vertex_hdl_t);
xbow_perf_link_t       *xbow_get_perf_counters(vertex_hdl_t);
int                     xbow_enable_perf_counter(vertex_hdl_t, int, int, int);
xbow_link_status_t     *xbow_get_llp_status(vertex_hdl_t);
void                    xbow_update_llp_status(vertex_hdl_t);

int                     xbow_disable_llp_monitor(vertex_hdl_t);
int                     xbow_enable_llp_monitor(vertex_hdl_t);
int                     xbow_prio_bw_alloc(vertex_hdl_t, xwidgetnum_t, xwidgetnum_t,
                                unsigned long long, unsigned long long);
static void		xbow_setwidint(xtalk_intr_t);

xswitch_reset_link_f    xbow_reset_link;

xswitch_provider_t      xbow_provider =
{
    xbow_reset_link,
};


static int
xbow_mmap(struct file * file, struct vm_area_struct * vma)
{
        unsigned long           phys_addr;
        int                     error = 0;

        phys_addr = (unsigned long)file->private_data & ~0xc000000000000000; /* Mask out the Uncache bits */
        vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
        vma->vm_flags |= VM_NONCACHED | VM_RESERVED | VM_IO;
        error = io_remap_page_range(vma->vm_start, phys_addr,
                                   vma->vm_end-vma->vm_start,
                                   vma->vm_page_prot);
        return(error);
}

/*
 * This is the file operation table for the pcibr driver.
 * As each of the functions are implemented, put the
 * appropriate function name below.
 */
struct file_operations xbow_fops = {
        .owner		= THIS_MODULE,
        .mmap		= xbow_mmap,
};

/*
 *    xbow_mlreset: called at mlreset time if the
 *      platform specific code determines that there is
 *      a crossbow in a critical path that must be
 *      functional before the driver would normally get
 *      the device properly set up.
 *
 *      what do we need to do, that the boot prom can
 *      not be counted on to have already done, that is
 *      generic across all platforms using crossbows?
 */
/*ARGSUSED */
void
xbow_mlreset(xbow_t * xbow)
{
}

#ifdef XBRIDGE_REGS_SIM
/*    xbow_set_simulated_regs: sets xbow regs as needed
 *	for powering through the boot
 */
void
xbow_set_simulated_regs(xbow_t *xbow, int port)
{
    /*
     * turn on link
     */
    xbow->xb_link(port).link_status = (1<<31);
    /*
     * and give it a live widget too
     */
    xbow->xb_link(port).link_aux_status = XB_AUX_STAT_PRESENT;
    /*
     * zero the link control reg
     */
    xbow->xb_link(port).link_control = 0x0;
}
#endif /* XBRIDGE_REGS_SIM */

/*
 *    xbow_attach: the crosstalk provider has
 *      determined that there is a crossbow widget
 *      present, and has handed us the connection
 *      point for that vertex.
 *
 *      We not only add our own vertex, but add
 *      some "xtalk switch" data to the switch
 *      vertex (at the connect point's parent) if
 *      it does not have any.
 */

/*ARGSUSED */
int
xbow_attach(vertex_hdl_t conn)
{
    /*REFERENCED */
    vertex_hdl_t            vhdl;
    vertex_hdl_t            busv;
    xbow_t                 *xbow;
    xbow_soft_t             soft;
    int                     port;
    xswitch_info_t          info;
    xtalk_intr_t            intr_hdl;
    char                    devnm[MAXDEVNAME], *s;
    xbowreg_t               id;
    int                     rev;
    int			    i;
    int			    xbow_num;
    static void xbow_errintr_handler(int, void *, struct pt_regs *);

	
#if DEBUG && ATTACH_DEBUG
#if defined(SUPPORT_PRINTING_V_FORMAT)
    printk("%v: xbow_attach\n", conn);
#else
    printk("0x%x: xbow_attach\n", conn);
#endif
#endif

    /*
     * Get a PIO pointer to the base of the crossbow
     * chip.
     */
#ifdef XBRIDGE_REGS_SIM
    printk("xbow_attach: XBRIDGE_REGS_SIM FIXME: allocating %ld bytes for xbow_s\n", sizeof(xbow_t));
    xbow = (xbow_t *) kmalloc(sizeof(xbow_t), GFP_KERNEL);
    /*
     * turn on ports e and f like in a real live ibrick
     */
    xbow_set_simulated_regs(xbow, 0xe);
    xbow_set_simulated_regs(xbow, 0xf);
#else
    xbow = (xbow_t *) xtalk_piotrans_addr(conn, 0, 0, sizeof(xbow_t), 0);
#endif /* XBRIDGE_REGS_SIM */

    /*
     * Locate the "switch" vertex: it is the parent
     * of our connection point.
     */
    busv = hwgraph_connectpt_get(conn);
#if DEBUG && ATTACH_DEBUG
    printk("xbow_attach: Bus Vertex 0x%p, conn 0x%p, xbow register 0x%p wid= 0x%x\n", busv, conn, xbow, *(volatile u32 *)xbow);
#endif

    ASSERT(busv != GRAPH_VERTEX_NONE);

    /*
     * Create our private vertex, and connect our
     * driver information to it. This makes it possible
     * for diagnostic drivers to open the crossbow
     * vertex for access to registers.
     */

    /*
     * Register a xbow driver with devfs.
     * file ops.
     */
    vhdl = NULL;
    vhdl = hwgraph_register(conn, EDGE_LBL_XBOW, 0,
	   DEVFS_FL_AUTO_DEVNUM, 0, 0,
	   S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP, 0, 0,
	   (struct file_operations *)&xbow_fops, (void *)xbow);
    if (!vhdl) {
        printk(KERN_WARNING "xbow_attach: Unable to create char device for xbow conn %p\n",
                (void *)conn);
    }

    /*
     * Allocate the soft state structure and attach
     * it to the xbow's vertex
     */
    NEW(soft);
    soft->conn = conn;
    soft->vhdl = vhdl;
    soft->busv = busv;
    soft->base = xbow;
    /* does the universe really need another macro?  */
    /* xbow_soft_set(vhdl, (arbitrary_info_t) soft); */
    /* hwgraph_fastinfo_set(vhdl, (arbitrary_info_t) soft); */

#define XBOW_NUM_SUFFIX_FORMAT	"[xbow# %d]"

    /* Add xbow number as a suffix to the hwgraph name of the xbow.
     * This is helpful while looking at the error/warning messages.
     */
    xbow_num = 0;

    /*
     * get the name of this xbow vertex and keep the info.
     * This is needed during errors and interupts, but as
     * long as we have it, we can use it elsewhere.
     */
    s = dev_to_name(vhdl, devnm, MAXDEVNAME);
    soft->name = kmalloc(strlen(s) + strlen(XBOW_NUM_SUFFIX_FORMAT) + 1, 
			    GFP_KERNEL);
    sprintf(soft->name,"%s"XBOW_NUM_SUFFIX_FORMAT, s,xbow_num);

#ifdef XBRIDGE_REGS_SIM
    /* my o200/ibrick has id=0x2d002049, but XXBOW_WIDGET_PART_NUM is defined
     * as 0xd000, so I'm using that for the partnum bitfield.
     */
    printk("xbow_attach: XBRIDGE_REGS_SIM FIXME: need xb_wid_id value!!\n");
    id = 0x2d000049;
#else
    id = xbow->xb_wid_id;
#endif /* XBRIDGE_REGS_SIM */
    rev = XWIDGET_PART_REV_NUM(id);

    spin_lock_init(&soft->xbow_perf_lock);
    soft->xbow_perfcnt[0].xp_perf_reg = &xbow->xb_perf_ctr_a;
    soft->xbow_perfcnt[1].xp_perf_reg = &xbow->xb_perf_ctr_b;

    /* Initialization for GBR bw allocation */
    spin_lock_init(&soft->xbow_bw_alloc_lock);

#define	XBOW_8_BIT_PORT_BW_MAX		(400 * 1000 * 1000)	/* 400 MB/s */
#define XBOW_16_BIT_PORT_BW_MAX		(800 * 1000 * 1000)	/* 800 MB/s */

    /* Set bandwidth hiwatermark and current values */
    for (i = 0; i < MAX_XBOW_PORTS; i++) {
	soft->bw_hiwm[i] = XBOW_16_BIT_PORT_BW_MAX;	/* for now */
	soft->bw_cur_used[i] = 0;
    }

     /*
      * attach the crossbow error interrupt.
      */
     intr_hdl = xtalk_intr_alloc(conn, (device_desc_t)0, vhdl);
     ASSERT(intr_hdl != NULL);

        {
                int irq = ((hub_intr_t)intr_hdl)->i_bit;
                int cpu = ((hub_intr_t)intr_hdl)->i_cpuid;

                intr_unreserve_level(cpu, irq);
                ((hub_intr_t)intr_hdl)->i_bit = SGI_XBOW_ERROR;
        }
 
     xtalk_intr_connect(intr_hdl,
                        (intr_func_t) xbow_errintr_handler,
                        (intr_arg_t) soft,
                        (xtalk_intr_setfunc_t) xbow_setwidint,
                        (void *) xbow);

     request_irq(SGI_XBOW_ERROR, (void *)xbow_errintr_handler, SA_SHIRQ, "XBOW error",
			(intr_arg_t) soft);

 
    /*
     * Enable xbow error interrupts
     */
    xbow->xb_wid_control = (XB_WID_CTRL_REG_ACC_IE | XB_WID_CTRL_XTALK_IE);

    /*
     * take a census of the widgets present,
     * leaving notes at the switch vertex.
     */
    info = xswitch_info_new(busv);

    for (port = MAX_PORT_NUM - MAX_XBOW_PORTS;
	 port < MAX_PORT_NUM; ++port) {
	if (!xbow_link_alive(xbow, port)) {
#if DEBUG && XBOW_DEBUG
	    printk(KERN_INFO "0x%p link %d is not alive\n",
		    (void *)busv, port);
#endif
	    continue;
	}
	if (!xbow_widget_present(xbow, port)) {
#if DEBUG && XBOW_DEBUG
	    printk(KERN_INFO "0x%p link %d is alive but no widget is present\n", (void *)busv, port);
#endif
	    continue;
	}
#if DEBUG && XBOW_DEBUG
	printk(KERN_INFO "0x%p link %d has a widget\n",
		(void *)busv, port);
#endif

	xswitch_info_link_is_ok(info, port);
	/*
	 * Turn some error interrupts on
	 * and turn others off. The PROM has
	 * some things turned on we don't
	 * want to see (bandwidth allocation
	 * errors for instance); so if it
	 * is not listed here, it is not on.
	 */
	xbow->xb_link(port).link_control =
	    ( (xbow->xb_link(port).link_control
	/*
	 * Turn off these bits; they are non-fatal,
	 * but we might want to save some statistics
	 * on the frequency of these errors.
	 * XXX FIXME XXX
	 */
	    & ~XB_CTRL_RCV_CNT_OFLOW_IE
	    & ~XB_CTRL_XMT_CNT_OFLOW_IE
	    & ~XB_CTRL_BNDWDTH_ALLOC_IE
	    & ~XB_CTRL_RCV_IE)
	/*
	 * These are the ones we want to turn on.
	 */
	    | (XB_CTRL_ILLEGAL_DST_IE
	    | XB_CTRL_OALLOC_IBUF_IE
	    | XB_CTRL_XMT_MAX_RTRY_IE
	    | XB_CTRL_MAXREQ_TOUT_IE
	    | XB_CTRL_XMT_RTRY_IE
	    | XB_CTRL_SRC_TOUT_IE) );
    }

    xswitch_provider_register(busv, &xbow_provider);

    return 0;				/* attach successful */
}

/*
 * xbow_widget_present: See if a device is present
 * on the specified port of this crossbow.
 */
int
xbow_widget_present(xbow_t *xbow, int port)
{
	if ( IS_RUNNING_ON_SIMULATOR() ) {
		if ( (port == 14) || (port == 15) ) {
			return 1;
		}
		else {
			return 0;
		}
	}
	else {
		/* WAR: port 0xf on PIC is missing present bit */
		if (XBOW_WAR_ENABLED(PV854827, xbow->xb_wid_id) &&
					IS_PIC_XBOW(xbow->xb_wid_id) && port==0xf) {
			return 1;
		}
		else if ( IS_PIC_XBOW(xbow->xb_wid_id) && port==0xb ) {
			/* for opus the present bit doesn't work on port 0xb */
			return 1;
		}
		return xbow->xb_link(port).link_aux_status & XB_AUX_STAT_PRESENT;
	}
}

static int
xbow_link_alive(xbow_t * xbow, int port)
{
    xbwX_stat_t             xbow_linkstat;

    xbow_linkstat.linkstatus = xbow->xb_link(port).link_status;
    return (xbow_linkstat.link_alive);
}

/*
 * xbow_widget_lookup
 *      Lookup the edges connected to the xbow specified, and
 *      retrieve the handle corresponding to the widgetnum
 *      specified.
 *      If not found, return 0.
 */
vertex_hdl_t
xbow_widget_lookup(vertex_hdl_t vhdl,
		   int widgetnum)
{
    xswitch_info_t          xswitch_info;
    vertex_hdl_t            conn;

    xswitch_info = xswitch_info_get(vhdl);
    conn = xswitch_info_vhdl_get(xswitch_info, widgetnum);
    return conn;
}

/*
 * xbow_setwidint: called when xtalk
 * is establishing or migrating our
 * interrupt service.
 */
static void
xbow_setwidint(xtalk_intr_t intr)
{
    xwidgetnum_t            targ = xtalk_intr_target_get(intr);
    iopaddr_t               addr = xtalk_intr_addr_get(intr);
    xtalk_intr_vector_t     vect = xtalk_intr_vector_get(intr);
    xbow_t                 *xbow = (xbow_t *) xtalk_intr_sfarg_get(intr);

    xbow_intr_preset((void *) xbow, 0, targ, addr, vect);
}

/*
 * xbow_intr_preset: called during mlreset time
 * if the platform specific code needs to route
 * an xbow interrupt before the xtalk infrastructure
 * is available for use.
 *
 * Also called from xbow_setwidint, so we don't
 * replicate the guts of the routine.
 *
 * XXX- probably should be renamed xbow_wid_intr_set or
 * something to reduce confusion.
 */
/*ARGSUSED3 */
void
xbow_intr_preset(void *which_widget,
		 int which_widget_intr,
		 xwidgetnum_t targ,
		 iopaddr_t addr,
		 xtalk_intr_vector_t vect)
{
    xbow_t                 *xbow = (xbow_t *) which_widget;

    xbow->xb_wid_int_upper = ((0xFF000000 & (vect << 24)) |
			      (0x000F0000 & (targ << 16)) |
			      XTALK_ADDR_TO_UPPER(addr));
    xbow->xb_wid_int_lower = XTALK_ADDR_TO_LOWER(addr);

}

#define	XEM_ADD_STR(s)		printk("%s", (s))
#define	XEM_ADD_NVAR(n,v)	printk("\t%20s: 0x%llx\n", (n), ((unsigned long long)v))
#define	XEM_ADD_VAR(v)		XEM_ADD_NVAR(#v,(v))
#define XEM_ADD_IOEF(p,n)	if (IOERROR_FIELDVALID(ioe,n)) {	\
				    IOERROR_GETVALUE(p,ioe,n);		\
				    XEM_ADD_NVAR("ioe." #n, p);		\
				}

int                     xbow_xmit_retry_errors;

int
xbow_xmit_retry_error(xbow_soft_t soft,
		      int port)
{
    xswitch_info_t          info;
    vertex_hdl_t            vhdl;
    widget_cfg_t           *wid;
    widgetreg_t             id;
    int                     part;
    int                     mfgr;

    wid = soft->wpio[port - BASE_XBOW_PORT];
    if (wid == NULL) {
	/* If we can't track down a PIO
	 * pointer to our widget yet,
	 * leave our caller knowing that
	 * we are interested in this
	 * interrupt if it occurs in
	 * the future.
	 */
	info = xswitch_info_get(soft->busv);
	if (!info)
	    return 1;
	vhdl = xswitch_info_vhdl_get(info, port);
	if (vhdl == GRAPH_VERTEX_NONE)
	    return 1;
	wid = (widget_cfg_t *) xtalk_piotrans_addr
	    (vhdl, 0, 0, sizeof *wid, 0);
	if (!wid)
	    return 1;
	soft->wpio[port - BASE_XBOW_PORT] = wid;
    }
    id = wid->w_id;
    part = XWIDGET_PART_NUM(id);
    mfgr = XWIDGET_MFG_NUM(id);

    /* If this thing is not a Bridge,
     * do not activate the WAR, and
     * tell our caller we do not need
     * to be called again.
     */
    if ((part != BRIDGE_WIDGET_PART_NUM) ||
	(mfgr != BRIDGE_WIDGET_MFGR_NUM)) {
		/* FIXME: add Xbridge to the WAR.
		 * Shouldn't hurt anything.  Later need to
		 * check if we can remove this.
                 */
    		if ((part != XBRIDGE_WIDGET_PART_NUM) ||
		    (mfgr != XBRIDGE_WIDGET_MFGR_NUM))
			return 0;
    }

    /* count how many times we
     * have picked up after
     * LLP Transmit problems.
     */
    xbow_xmit_retry_errors++;

    /* rewrite the control register
     * to fix things up.
     */
    wid->w_control = wid->w_control;
    wid->w_control;

    return 1;
}

/*
 * xbow_errintr_handler will be called if the xbow
 * sends an interrupt request to report an error.
 */
static void
xbow_errintr_handler(int irq, void *arg, struct pt_regs *ep)
{
    ioerror_t               ioe[1];
    xbow_soft_t             soft = (xbow_soft_t) arg;
    xbow_t                 *xbow = soft->base;
    xbowreg_t               wid_control;
    xbowreg_t               wid_stat;
    xbowreg_t               wid_err_cmdword;
    xbowreg_t               wid_err_upper;
    xbowreg_t               wid_err_lower;
    w_err_cmd_word_u        wid_err;
    unsigned long long      wid_err_addr;

    int                     fatal = 0;
    int                     dump_ioe = 0;
    static int xbow_error_handler(void *, int, ioerror_mode_t, ioerror_t *);

    wid_control = xbow->xb_wid_control;
    wid_stat = xbow->xb_wid_stat_clr;
    wid_err_cmdword = xbow->xb_wid_err_cmdword;
    wid_err_upper = xbow->xb_wid_err_upper;
    wid_err_lower = xbow->xb_wid_err_lower;
    xbow->xb_wid_err_cmdword = 0;

    wid_err_addr = wid_err_lower | (((iopaddr_t) wid_err_upper & WIDGET_ERR_UPPER_ADDR_ONLY) << 32);

    if (wid_stat & XB_WID_STAT_LINK_INTR_MASK) {
	int                     port;

	wid_err.r = wid_err_cmdword;

	for (port = MAX_PORT_NUM - MAX_XBOW_PORTS;
	     port < MAX_PORT_NUM; port++) {
	    if (wid_stat & XB_WID_STAT_LINK_INTR(port)) {
		xb_linkregs_t          *link = &(xbow->xb_link(port));
		xbowreg_t               link_control = link->link_control;
		xbowreg_t               link_status = link->link_status_clr;
		xbowreg_t               link_aux_status = link->link_aux_status;
		xbowreg_t               link_pend;

		link_pend = link_status & link_control &
		    (XB_STAT_ILLEGAL_DST_ERR
		     | XB_STAT_OALLOC_IBUF_ERR
		     | XB_STAT_RCV_CNT_OFLOW_ERR
		     | XB_STAT_XMT_CNT_OFLOW_ERR
		     | XB_STAT_XMT_MAX_RTRY_ERR
		     | XB_STAT_RCV_ERR
		     | XB_STAT_XMT_RTRY_ERR
		     | XB_STAT_MAXREQ_TOUT_ERR
		     | XB_STAT_SRC_TOUT_ERR
		    );

		if (link_pend & XB_STAT_ILLEGAL_DST_ERR) {
		    if (wid_err.f.sidn == port) {
			IOERROR_INIT(ioe);
			IOERROR_SETVALUE(ioe, widgetnum, port);
			IOERROR_SETVALUE(ioe, xtalkaddr, wid_err_addr);
			if (IOERROR_HANDLED ==
			    xbow_error_handler(soft,
					       IOECODE_DMA,
					       MODE_DEVERROR,
					       ioe)) {
			    link_pend &= ~XB_STAT_ILLEGAL_DST_ERR;
			} else {
			    dump_ioe++;
			}
		    }
		}
		/* Xbow/Bridge WAR:
		 * if the bridge signals an LLP Transmitter Retry,
		 * rewrite its control register.
		 * If someone else triggers this interrupt,
		 * ignore (and disable) the interrupt.
		 */
		if (link_pend & XB_STAT_XMT_RTRY_ERR) {
		    if (!xbow_xmit_retry_error(soft, port)) {
			link_control &= ~XB_CTRL_XMT_RTRY_IE;
			link->link_control = link_control;
			link->link_control;	/* stall until written */
		    }
		    link_pend &= ~XB_STAT_XMT_RTRY_ERR;
		}
		if (link_pend) {
		    vertex_hdl_t	xwidget_vhdl;
		    char		*xwidget_name;
		    
		    /* Get the widget name corresponding to the current
		     * xbow link.
		     */
		    xwidget_vhdl = xbow_widget_lookup(soft->busv,port);
		    xwidget_name = xwidget_name_get(xwidget_vhdl);

		    printk("%s port %X[%s] XIO Bus Error",
			    soft->name, port, xwidget_name);
		    if (link_status & XB_STAT_MULTI_ERR)
			XEM_ADD_STR("\tMultiple Errors\n");
		    if (link_status & XB_STAT_ILLEGAL_DST_ERR)
			XEM_ADD_STR("\tInvalid Packet Destination\n");
		    if (link_status & XB_STAT_OALLOC_IBUF_ERR)
			XEM_ADD_STR("\tInput Overallocation Error\n");
		    if (link_status & XB_STAT_RCV_CNT_OFLOW_ERR)
			XEM_ADD_STR("\tLLP receive error counter overflow\n");
		    if (link_status & XB_STAT_XMT_CNT_OFLOW_ERR)
			XEM_ADD_STR("\tLLP transmit retry counter overflow\n");
		    if (link_status & XB_STAT_XMT_MAX_RTRY_ERR)
			XEM_ADD_STR("\tLLP Max Transmitter Retry\n");
		    if (link_status & XB_STAT_RCV_ERR)
			XEM_ADD_STR("\tLLP Receiver error\n");
		    if (link_status & XB_STAT_XMT_RTRY_ERR)
			XEM_ADD_STR("\tLLP Transmitter Retry\n");
		    if (link_status & XB_STAT_MAXREQ_TOUT_ERR)
			XEM_ADD_STR("\tMaximum Request Timeout\n");
		    if (link_status & XB_STAT_SRC_TOUT_ERR)
			XEM_ADD_STR("\tSource Timeout Error\n");

		    {
			int                     other_port;

			for (other_port = 8; other_port < 16; ++other_port) {
			    if (link_aux_status & (1 << other_port)) {
				/* XXX- need to go to "other_port"
				 * and clean up after the timeout?
				 */
				XEM_ADD_VAR(other_port);
			    }
			}
		    }

#if !DEBUG
		    if (kdebug) {
#endif
			XEM_ADD_VAR(link_control);
			XEM_ADD_VAR(link_status);
			XEM_ADD_VAR(link_aux_status);

#if !DEBUG
		    }
#endif
		    fatal++;
		}
	    }
	}
    }
    if (wid_stat & wid_control & XB_WID_STAT_WIDGET0_INTR) {
	/* we have a "widget zero" problem */

	if (wid_stat & (XB_WID_STAT_MULTI_ERR
			| XB_WID_STAT_XTALK_ERR
			| XB_WID_STAT_REG_ACC_ERR)) {

	    printk("%s Port 0 XIO Bus Error",
		    soft->name);
	    if (wid_stat & XB_WID_STAT_MULTI_ERR)
		XEM_ADD_STR("\tMultiple Error\n");
	    if (wid_stat & XB_WID_STAT_XTALK_ERR)
		XEM_ADD_STR("\tXIO Error\n");
	    if (wid_stat & XB_WID_STAT_REG_ACC_ERR)
		XEM_ADD_STR("\tRegister Access Error\n");

	    fatal++;
	}
    }
    if (fatal) {
	XEM_ADD_VAR(wid_stat);
	XEM_ADD_VAR(wid_control);
	XEM_ADD_VAR(wid_err_cmdword);
	XEM_ADD_VAR(wid_err_upper);
	XEM_ADD_VAR(wid_err_lower);
	XEM_ADD_VAR(wid_err_addr);
	panic("XIO Bus Error");
    }
}

/*
 * XBOW ERROR Handling routines.
 * These get invoked as part of walking down the error handling path
 * from hub/heart towards the I/O device that caused the error.
 */

/*
 * xbow_error_handler
 *      XBow error handling dispatch routine.
 *      This is the primary interface used by external world to invoke
 *      in case of an error related to a xbow.
 *      Only functionality in this layer is to identify the widget handle
 *      given the widgetnum. Otherwise, xbow does not gathers any error
 *      data.
 */
static int
xbow_error_handler(
		      void *einfo,
		      int error_code,
		      ioerror_mode_t mode,
		      ioerror_t *ioerror)
{
    int                     retval = IOERROR_WIDGETLEVEL;

    xbow_soft_t             soft = (xbow_soft_t) einfo;
    int                     port;
    vertex_hdl_t          conn;
    vertex_hdl_t          busv;

    xbow_t                 *xbow = soft->base;
    xbowreg_t               wid_stat;
    xbowreg_t               wid_err_cmdword;
    xbowreg_t               wid_err_upper;
    xbowreg_t               wid_err_lower;
    unsigned long long      wid_err_addr;

    xb_linkregs_t          *link;
    xbowreg_t               link_control;
    xbowreg_t               link_status;
    xbowreg_t               link_aux_status;

    ASSERT(soft != 0);
    busv = soft->busv;

#if DEBUG && ERROR_DEBUG
    printk("%s: xbow_error_handler\n", soft->name, busv);
#endif

    IOERROR_GETVALUE(port, ioerror, widgetnum);

    if (port == 0) {
	/* error during access to xbow:
	 * do NOT attempt to access xbow regs.
	 */
	if (mode == MODE_DEVPROBE)
	    return IOERROR_HANDLED;

	if (error_code & IOECODE_DMA) {
	    printk(KERN_ALERT
		    "DMA error blamed on Crossbow at %s\n"
		    "\tbut Crosbow never initiates DMA!",
		    soft->name);
	}
	if (error_code & IOECODE_PIO) {
	    iopaddr_t tmp;
	    IOERROR_GETVALUE(tmp, ioerror, xtalkaddr);
	    printk(KERN_ALERT "PIO Error on XIO Bus %s\n"
		    "\tattempting to access XIO controller\n"
		    "\twith offset 0x%lx",
		    soft->name, tmp);
	}
	/* caller will dump contents of ioerror
	 * in DEBUG and kdebug kernels.
	 */

	return retval;
    }
    /*
     * error not on port zero:
     * safe to read xbow registers.
     */
    wid_stat = xbow->xb_wid_stat;
    wid_err_cmdword = xbow->xb_wid_err_cmdword;
    wid_err_upper = xbow->xb_wid_err_upper;
    wid_err_lower = xbow->xb_wid_err_lower;

    wid_err_addr =
	wid_err_lower
	| (((iopaddr_t) wid_err_upper
	    & WIDGET_ERR_UPPER_ADDR_ONLY)
	   << 32);

    if ((port < BASE_XBOW_PORT) ||
	(port >= MAX_PORT_NUM)) {

	if (mode == MODE_DEVPROBE)
	    return IOERROR_HANDLED;

	if (error_code & IOECODE_DMA) {
	    printk(KERN_ALERT
		    "DMA error blamed on XIO port at %s/%d\n"
		    "\tbut Crossbow does not support that port",
		    soft->name, port);
	}
	if (error_code & IOECODE_PIO) {
	    iopaddr_t tmp;
	    IOERROR_GETVALUE(tmp, ioerror, xtalkaddr);
	    printk(KERN_ALERT
		    "PIO Error on XIO Bus %s\n"
		    "\tattempting to access XIO port %d\n"
		    "\t(which Crossbow does not support)"
		    "\twith offset 0x%lx",
		    soft->name, port, tmp);
	}
#if !DEBUG
	if (kdebug) {
#endif
	    XEM_ADD_STR("Raw status values for Crossbow:\n");
	    XEM_ADD_VAR(wid_stat);
	    XEM_ADD_VAR(wid_err_cmdword);
	    XEM_ADD_VAR(wid_err_upper);
	    XEM_ADD_VAR(wid_err_lower);
	    XEM_ADD_VAR(wid_err_addr);
#if !DEBUG
	}
#endif

	/* caller will dump contents of ioerror
	 * in DEBUG and kdebug kernels.
	 */

	return retval;
    }
    /* access to valid port:
     * ok to check port status.
     */

    link = &(xbow->xb_link(port));
    link_control = link->link_control;
    link_status = link->link_status;
    link_aux_status = link->link_aux_status;

    /* Check that there is something present
     * in that XIO port.
     */
    /* WAR: PIC widget 0xf is missing prescense bit */
    if (XBOW_WAR_ENABLED(PV854827, xbow->xb_wid_id) &&
		IS_PIC_XBOW(xbow->xb_wid_id) && (port==0xf))
		;
    else if (IS_PIC_XBOW(xbow->xb_wid_id) && (port==0xb))
		;	/* WAR for opus this is missing on 0xb */
    else if (!(link_aux_status & XB_AUX_STAT_PRESENT)) {
	/* nobody connected. */
	if (mode == MODE_DEVPROBE)
	    return IOERROR_HANDLED;

	if (error_code & IOECODE_DMA) {
	    printk(KERN_ALERT
		    "DMA error blamed on XIO port at %s/%d\n"
		    "\tbut there is no device connected there.",
		    soft->name, port);
	}
	if (error_code & IOECODE_PIO) {
	    iopaddr_t tmp;
	    IOERROR_GETVALUE(tmp, ioerror, xtalkaddr);
	    printk(KERN_ALERT
		    "PIO Error on XIO Bus %s\n"
		    "\tattempting to access XIO port %d\n"
		    "\t(which has no device connected)"
		    "\twith offset 0x%lx",
		    soft->name, port, tmp);
	}
#if !DEBUG
	if (kdebug) {
#endif
	    XEM_ADD_STR("Raw status values for Crossbow:\n");
	    XEM_ADD_VAR(wid_stat);
	    XEM_ADD_VAR(wid_err_cmdword);
	    XEM_ADD_VAR(wid_err_upper);
	    XEM_ADD_VAR(wid_err_lower);
	    XEM_ADD_VAR(wid_err_addr);
	    XEM_ADD_VAR(port);
	    XEM_ADD_VAR(link_control);
	    XEM_ADD_VAR(link_status);
	    XEM_ADD_VAR(link_aux_status);
#if !DEBUG
	}
#endif
	return retval;

    }
    /* Check that the link is alive.
     */
    if (!(link_status & XB_STAT_LINKALIVE)) {
	iopaddr_t tmp;
	/* nobody connected. */
	if (mode == MODE_DEVPROBE)
	    return IOERROR_HANDLED;

	printk(KERN_ALERT
		"%s%sError on XIO Bus %s port %d",
		(error_code & IOECODE_DMA) ? "DMA " : "",
		(error_code & IOECODE_PIO) ? "PIO " : "",
		soft->name, port);

	IOERROR_GETVALUE(tmp, ioerror, xtalkaddr);
	if ((error_code & IOECODE_PIO) &&
	    (IOERROR_FIELDVALID(ioerror, xtalkaddr))) {
		printk("\tAccess attempted to offset 0x%lx\n", tmp);
	}
	if (link_aux_status & XB_AUX_LINKFAIL_RST_BAD)
	    XEM_ADD_STR("\tLink never came out of reset\n");
	else
	    XEM_ADD_STR("\tLink failed while transferring data\n");

    }
    /* get the connection point for the widget
     * involved in this error; if it exists and
     * is not our connectpoint, cycle back through
     * xtalk_error_handler to deliver control to
     * the proper handler (or to report a generic
     * crosstalk error).
     *
     * If the downstream handler won't handle
     * the problem, we let our upstream caller
     * deal with it, after (in DEBUG and kdebug
     * kernels) dumping the xbow state for this
     * port.
     */
    conn = xbow_widget_lookup(busv, port);
    if ((conn != GRAPH_VERTEX_NONE) &&
	(conn != soft->conn)) {
	retval = xtalk_error_handler(conn, error_code, mode, ioerror);
	if (retval == IOERROR_HANDLED)
	    return IOERROR_HANDLED;
    }
    if (mode == MODE_DEVPROBE)
	return IOERROR_HANDLED;

    if (retval == IOERROR_UNHANDLED) {
	iopaddr_t tmp;
	retval = IOERROR_PANIC;

	printk(KERN_ALERT
		"%s%sError on XIO Bus %s port %d",
		(error_code & IOECODE_DMA) ? "DMA " : "",
		(error_code & IOECODE_PIO) ? "PIO " : "",
		soft->name, port);

	IOERROR_GETVALUE(tmp, ioerror, xtalkaddr);
	if ((error_code & IOECODE_PIO) &&
	    (IOERROR_FIELDVALID(ioerror, xtalkaddr))) {
	    printk("\tAccess attempted to offset 0x%lx\n", tmp);
	}
    }

#if !DEBUG
    if (kdebug) {
#endif
	XEM_ADD_STR("Raw status values for Crossbow:\n");
	XEM_ADD_VAR(wid_stat);
	XEM_ADD_VAR(wid_err_cmdword);
	XEM_ADD_VAR(wid_err_upper);
	XEM_ADD_VAR(wid_err_lower);
	XEM_ADD_VAR(wid_err_addr);
	XEM_ADD_VAR(port);
	XEM_ADD_VAR(link_control);
	XEM_ADD_VAR(link_status);
	XEM_ADD_VAR(link_aux_status);
#if !DEBUG
    }
#endif
    /* caller will dump raw ioerror data
     * in DEBUG and kdebug kernels.
     */

    return retval;
}

void
xbow_update_perf_counters(vertex_hdl_t vhdl)
{
    xbow_soft_t             xbow_soft = xbow_soft_get(vhdl);
    xbow_perf_t            *xbow_perf = xbow_soft->xbow_perfcnt;
    xbow_perf_link_t       *xbow_plink = xbow_soft->xbow_perflink;
    xbow_perfcount_t        perf_reg;
    int                     link, i;

    for (i = 0; i < XBOW_PERF_COUNTERS; i++, xbow_perf++) {
	if (xbow_perf->xp_mode == XBOW_MONITOR_NONE)
	    continue;

	spin_lock(&xbow_soft->xbow_perf_lock);

	perf_reg.xb_counter_val = *(xbowreg_t *) xbow_perf->xp_perf_reg;

	link = perf_reg.xb_perf.link_select;

	(xbow_plink + link)->xlp_cumulative[xbow_perf->xp_curmode] +=
	    ((perf_reg.xb_perf.count - xbow_perf->xp_current) & XBOW_COUNTER_MASK);
	xbow_perf->xp_current = perf_reg.xb_perf.count;

	spin_unlock(&xbow_soft->xbow_perf_lock);
    }
}

xbow_perf_link_t       *
xbow_get_perf_counters(vertex_hdl_t vhdl)
{
    xbow_soft_t             xbow_soft = xbow_soft_get(vhdl);
    xbow_perf_link_t       *xbow_perf_link = xbow_soft->xbow_perflink;

    return xbow_perf_link;
}

int
xbow_enable_perf_counter(vertex_hdl_t vhdl, int link, int mode, int counter)
{
    xbow_soft_t             xbow_soft = xbow_soft_get(vhdl);
    xbow_perf_t            *xbow_perf = xbow_soft->xbow_perfcnt;
    xbow_linkctrl_t         xbow_link_ctrl;
    xbow_t                 *xbow = xbow_soft->base;
    xbow_perfcount_t        perf_reg;
    int                     i;

    link -= BASE_XBOW_PORT;
    if ((link < 0) || (link >= MAX_XBOW_PORTS))
	return -1;

    if ((mode < XBOW_MONITOR_NONE) || (mode > XBOW_MONITOR_DEST_LINK))
	return -1;

    if ((counter < 0) || (counter >= XBOW_PERF_COUNTERS))
	return -1;

    spin_lock(&xbow_soft->xbow_perf_lock);

    if ((xbow_perf + counter)->xp_mode && mode) {
	spin_unlock(&xbow_soft->xbow_perf_lock);
	return -1;
    }
    for (i = 0; i < XBOW_PERF_COUNTERS; i++) {
	if (i == counter)
	    continue;
	if (((xbow_perf + i)->xp_link == link) &&
	    ((xbow_perf + i)->xp_mode)) {
	    spin_unlock(&xbow_soft->xbow_perf_lock);
	    return -1;
	}
    }
    xbow_perf += counter;

    xbow_perf->xp_curlink = xbow_perf->xp_link = link;
    xbow_perf->xp_curmode = xbow_perf->xp_mode = mode;

    xbow_link_ctrl.xbl_ctrlword = xbow->xb_link_raw[link].link_control;
    xbow_link_ctrl.xb_linkcontrol.perf_mode = mode;
    xbow->xb_link_raw[link].link_control = xbow_link_ctrl.xbl_ctrlword;

    perf_reg.xb_counter_val = *(xbowreg_t *) xbow_perf->xp_perf_reg;
    perf_reg.xb_perf.link_select = link;
    *(xbowreg_t *) xbow_perf->xp_perf_reg = perf_reg.xb_counter_val;
    xbow_perf->xp_current = perf_reg.xb_perf.count;

    spin_unlock(&xbow_soft->xbow_perf_lock);
    return 0;
}

xbow_link_status_t     *
xbow_get_llp_status(vertex_hdl_t vhdl)
{
    xbow_soft_t             xbow_soft = xbow_soft_get(vhdl);
    xbow_link_status_t     *xbow_llp_status = xbow_soft->xbow_link_status;

    return xbow_llp_status;
}

void
xbow_update_llp_status(vertex_hdl_t vhdl)
{
    xbow_soft_t             xbow_soft = xbow_soft_get(vhdl);
    xbow_link_status_t     *xbow_llp_status = xbow_soft->xbow_link_status;
    xbow_t                 *xbow;
    xbwX_stat_t             lnk_sts;
    xbow_aux_link_status_t  aux_sts;
    int                     link;
    vertex_hdl_t	    xwidget_vhdl;
    char		   *xwidget_name;	

    xbow = (xbow_t *) xbow_soft->base;
    for (link = 0; link < MAX_XBOW_PORTS; link++, xbow_llp_status++) {
	/* Get the widget name corresponding the current link.
	 * Note : 0 <= link < MAX_XBOW_PORTS(8).
	 * 	  BASE_XBOW_PORT(0x8) <= xwidget number < MAX_PORT_NUM (0x10)
	 */
	xwidget_vhdl = xbow_widget_lookup(xbow_soft->busv,link+BASE_XBOW_PORT);
	xwidget_name = xwidget_name_get(xwidget_vhdl);
	aux_sts.aux_linkstatus
	    = xbow->xb_link_raw[link].link_aux_status;
	lnk_sts.linkstatus = xbow->xb_link_raw[link].link_status_clr;

	if (lnk_sts.link_alive == 0)
	    continue;

	xbow_llp_status->rx_err_count +=
	    aux_sts.xb_aux_linkstatus.rx_err_cnt;

	xbow_llp_status->tx_retry_count +=
	    aux_sts.xb_aux_linkstatus.tx_retry_cnt;

	if (lnk_sts.linkstatus & ~(XB_STAT_RCV_ERR | XB_STAT_XMT_RTRY_ERR | XB_STAT_LINKALIVE)) {
#ifdef	LATER
	    printk(KERN_WARNING  "link %d[%s]: bad status 0x%x\n",
		    link, xwidget_name, lnk_sts.linkstatus);
#endif
	}
    }
}

int
xbow_disable_llp_monitor(vertex_hdl_t vhdl)
{
    xbow_soft_t             xbow_soft = xbow_soft_get(vhdl);
    int                     port;

    for (port = 0; port < MAX_XBOW_PORTS; port++) {
	xbow_soft->xbow_link_status[port].rx_err_count = 0;
	xbow_soft->xbow_link_status[port].tx_retry_count = 0;
    }

    xbow_soft->link_monitor = 0;
    return 0;
}

int
xbow_enable_llp_monitor(vertex_hdl_t vhdl)
{
    xbow_soft_t             xbow_soft = xbow_soft_get(vhdl);

    xbow_soft->link_monitor = 1;
    return 0;
}


int
xbow_reset_link(vertex_hdl_t xconn_vhdl)
{
    xwidget_info_t          widget_info;
    xwidgetnum_t            port;
    xbow_t                 *xbow;
    xbowreg_t               ctrl;
    xbwX_stat_t             stat;
    unsigned                long itick;
    unsigned                dtick;
    static long             ticks_to_wait = HZ / 1000;

    widget_info = xwidget_info_get(xconn_vhdl);
    port = xwidget_info_id_get(widget_info);

#ifdef XBOW_K1PTR			/* defined if we only have one xbow ... */
    xbow = XBOW_K1PTR;
#else
    {
	vertex_hdl_t            xbow_vhdl;
	xbow_soft_t             xbow_soft;

	hwgraph_traverse(xconn_vhdl, ".master/xtalk/0/xbow", &xbow_vhdl);
	xbow_soft = xbow_soft_get(xbow_vhdl);
	xbow = xbow_soft->base;
    }
#endif

    /*
     * This requires three PIOs (reset the link, check for the
     * reset, restore the control register for the link) plus
     * 10us to wait for the reset. We allow up to 1ms for the
     * widget to come out of reset before giving up and
     * returning a failure.
     */
    ctrl = xbow->xb_link(port).link_control;
    xbow->xb_link(port).link_reset = 0;
    itick = jiffies;
    while (1) {
	stat.linkstatus = xbow->xb_link(port).link_status;
	if (stat.link_alive)
	    break;
	dtick = jiffies - itick;
	if (dtick > ticks_to_wait) {
	    return -1;			/* never came out of reset */
	}
	udelay(2);			/* don't beat on link_status */
    }
    xbow->xb_link(port).link_control = ctrl;
    return 0;
}

#define XBOW_ARB_RELOAD_TICKS		25
					/* granularity: 4 MB/s, max: 124 MB/s */
#define GRANULARITY			((100 * 1000000) / XBOW_ARB_RELOAD_TICKS)

#define XBOW_BYTES_TO_GBR(BYTES_per_s)	(int) (BYTES_per_s / GRANULARITY)

#define XBOW_GBR_TO_BYTES(cnt)		(bandwidth_t) ((cnt) * GRANULARITY)

#define CEILING_BYTES_TO_GBR(gbr, bytes_per_sec)	\
			((XBOW_GBR_TO_BYTES(gbr) < bytes_per_sec) ? gbr+1 : gbr)

#define XBOW_ARB_GBR_MAX		31

#define ABS(x)				((x > 0) ? (x) : (-1 * x))
					/* absolute value */

int
xbow_bytes_to_gbr(bandwidth_t old_bytes_per_sec, bandwidth_t bytes_per_sec)
{
    int                     gbr_granted;
    int                     new_total_gbr;
    int                     change_gbr;
    bandwidth_t             new_total_bw;

#ifdef GRIO_DEBUG
    printk("xbow_bytes_to_gbr: old_bytes_per_sec %lld bytes_per_sec %lld\n",
		old_bytes_per_sec, bytes_per_sec);
#endif	/* GRIO_DEBUG */

    gbr_granted = CEILING_BYTES_TO_GBR((XBOW_BYTES_TO_GBR(old_bytes_per_sec)),
			old_bytes_per_sec);
    new_total_bw = old_bytes_per_sec + bytes_per_sec;
    new_total_gbr = CEILING_BYTES_TO_GBR((XBOW_BYTES_TO_GBR(new_total_bw)),
			new_total_bw);

    change_gbr = new_total_gbr - gbr_granted;

#ifdef GRIO_DEBUG
    printk("xbow_bytes_to_gbr: gbr_granted %d new_total_gbr %d change_gbr %d\n",
		gbr_granted, new_total_gbr, change_gbr);
#endif	/* GRIO_DEBUG */

    return (change_gbr);
}

/* Conversion from GBR to bytes */
bandwidth_t
xbow_gbr_to_bytes(int gbr)
{
    return (XBOW_GBR_TO_BYTES(gbr));
}

/* Given the vhdl for the desired xbow, the src and dest. widget ids
 * and the req_bw value, this xbow driver entry point accesses the
 * xbow registers and allocates the desired bandwidth if available.
 *
 * If bandwidth allocation is successful, return success else return failure.
 */
int
xbow_prio_bw_alloc(vertex_hdl_t vhdl,
		xwidgetnum_t src_wid,
		xwidgetnum_t dest_wid,
		unsigned long long old_alloc_bw,
		unsigned long long req_bw)
{
    xbow_soft_t             soft = xbow_soft_get(vhdl);
    volatile xbowreg_t     *xreg;
    xbowreg_t               mask;
    int                     error = 0;
    bandwidth_t             old_bw_BYTES, req_bw_BYTES;
    xbowreg_t               old_xreg;
    int                     old_bw_GBR, req_bw_GBR, new_bw_GBR;

#ifdef GRIO_DEBUG
    printk("xbow_prio_bw_alloc: vhdl %d src_wid %d dest_wid %d req_bw %lld\n",
		(int) vhdl, (int) src_wid, (int) dest_wid, req_bw);
#endif

    ASSERT(XBOW_WIDGET_IS_VALID(src_wid));
    ASSERT(XBOW_WIDGET_IS_VALID(dest_wid));

    spin_lock(&soft->xbow_bw_alloc_lock);

    /* Get pointer to the correct register */
    xreg = XBOW_PRIO_ARBREG_PTR(soft->base, dest_wid, src_wid);

    /* Get mask for GBR count value */
    mask = XB_ARB_GBR_MSK << XB_ARB_GBR_SHFT(src_wid);

    req_bw_GBR = xbow_bytes_to_gbr(old_alloc_bw, req_bw);
    req_bw_BYTES = (req_bw_GBR < 0) ? (-1 * xbow_gbr_to_bytes(ABS(req_bw_GBR)))
		: xbow_gbr_to_bytes(req_bw_GBR);

#ifdef GRIO_DEBUG
    printk("req_bw %lld req_bw_BYTES %lld req_bw_GBR %d\n",
		req_bw, req_bw_BYTES, req_bw_GBR);
#endif	/* GRIO_DEBUG */

    old_bw_BYTES = soft->bw_cur_used[(int) dest_wid - MAX_XBOW_PORTS];
    old_xreg = *xreg;
    old_bw_GBR = (((*xreg) & mask) >> XB_ARB_GBR_SHFT(src_wid));

#ifdef GRIO_DEBUG
    ASSERT(XBOW_BYTES_TO_GBR(old_bw_BYTES) == old_bw_GBR);

    printk("old_bw_BYTES %lld old_bw_GBR %d\n", old_bw_BYTES, old_bw_GBR);

    printk("req_bw_BYTES %lld old_bw_BYTES %lld soft->bw_hiwm %lld\n",
		req_bw_BYTES, old_bw_BYTES,
		soft->bw_hiwm[(int) dest_wid - MAX_XBOW_PORTS]);
	   
#endif				/* GRIO_DEBUG */

    /* Accept the request only if we don't exceed the destination
     * port HIWATER_MARK *AND* the max. link GBR arbitration count
     */
    if (((old_bw_BYTES + req_bw_BYTES) <=
		soft->bw_hiwm[(int) dest_wid - MAX_XBOW_PORTS]) &&
		(req_bw_GBR + old_bw_GBR <= XBOW_ARB_GBR_MAX)) {

	new_bw_GBR = (old_bw_GBR + req_bw_GBR);

	/* Set this in the xbow link register */
	*xreg = (old_xreg & ~mask) | \
	    (new_bw_GBR << XB_ARB_GBR_SHFT(src_wid) & mask);

	soft->bw_cur_used[(int) dest_wid - MAX_XBOW_PORTS] =
			xbow_gbr_to_bytes(new_bw_GBR);
    } else {
	error = 1;
    }

    spin_unlock(&soft->xbow_bw_alloc_lock);

    return (error);
}
