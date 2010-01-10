/*
 * octeon_rgmx.c     RGMII  Ethernet Interfaces on Octeon
 *
 */


/*
 * Driver for the Reduced Gigabit Media Independent Interface (RGMII)
 * present on the Cavium Networks' Octeon chip.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/power.h>
#include <sys/smp.h>
#include <sys/time.h>
#include <sys/timetc.h>
#include <sys/malloc.h>
#include <sys/kthread.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/taskqueue.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_mib.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <net/bpf.h>

#include <machine/clock.h>
#include <machine/locore.h>
#include <machine/md_var.h>

#include <mips/octeon1/octeon_pcmap_regs.h>

#include "octeon_fau.h"
#include "octeon_fpa.h"
#include "octeon_ipd.h"
#include "octeon_pko.h"
#include "octeon_pip.h"
#include "octeon_rgmx.h"


/* The "battleship" boards have 8 ports */
#define OCTEON_RGMX_NUM_PORTS_MAX	   8
#define NUM_TX_PACKETS			  80
#define NUM_RX_PACKETS			 300
#define MAX_RX_BUFS			(NUM_RX_PACKETS) * (OCTEON_RGMX_NUM_PORTS_MAX)
#define MAX_TX_BUFS			(NUM_TX_PACKETS)
#define OCTEON_RGMX_DEV_NAME		"rgmx"
#define OCTEON_RGMX_MIN_PORT		   0
#define OCTEON_RGMX_MAX_PORT		  19
#define OCTEON_RGMX_OQUEUE_PER_PORT	   8


#define OCTEON_RGMX_SCHEDULED_ISRS	1     /*  Use Scheduled ISRs from kernel tasks */


#ifndef POW_MAX_LOOP
#define POW_MAX_LOOP 0x800
#endif


/*
 * CIU related stuff for enabling POW interrupts
 */
#define OCTEON_RGMX_CIU_INTX	CIU_INT_0
#define OCTEON_RGMX_CIU_ENX	CIU_EN_0

MALLOC_DEFINE(M_RGMII_WQE, "rgmii_wqe", "FPA pool for WQEs");

/* Driver data */

struct rgmx_softc_dev {
	device_t		sc_dev;	/* Device ID */
    	uint64_t		link_status;
    	struct ifnet            *ifp;
        int                     sc_unit;

    	u_int			port;
	u_int			idx;
        u_char                  ieee[6];

        char const * typestr;   /* printable name of the interface.  */
        u_short txb_size;       /* size of TX buffer, in bytes  */

        /* Transmission buffer management.  */
        u_short txb_free;       /* free bytes in TX buffer  */
        u_char txb_count;       /* number of packets in TX buffer  */
        u_char txb_sched;       /* number of scheduled packets  */

    	/* Media information.  */
    	struct ifmedia media;   /* used by if_media.  */
        u_short mbitmap;        /* bitmap for supported media; see bit2media */
        int defmedia;           /* default media  */
    	struct ifqueue tx_pending_queue;    /* Queue of mbuf given to PKO currently */
    	octeon_pko_sw_queue_info_t	*outq_ptr;

	struct mtx	mtx;
};


/*
 * Device methods
 */
static int rgmii_probe(device_t);
static void rgmii_identify(driver_t *, device_t);
static int rgmii_attach(device_t);



/*
 * Octeon specific routines
 */
static int octeon_has_4ports(void);
static void octeon_config_rgmii_port(u_int port);
static void octeon_rgmx_config_pip(u_int port);
static void octeon_line_status_loop(void *);
static void octeon_rx_loop(void *);
static void octeon_config_hw_units_post_ports(void);
static void octeon_config_hw_units_pre_ports(void);
static void octeon_config_hw_units_port(struct rgmx_softc_dev *sc, u_int port);
static struct rgmx_softc_dev *get_rgmx_softc(u_int port);
static void octeon_rgmx_start_port(u_int port);
static u_int octeon_rgmx_stop_port(u_int port);
static u_int get_rgmx_port_ordinal(u_int port);
static void octeon_rgmx_set_mac(u_int port);
static void octeon_rgmx_init_sc(struct rgmx_softc_dev *sc, device_t dev, u_int port, u_int num_devices);
static int octeon_rgmx_init_ifnet(struct rgmx_softc_dev *sc);
static void octeon_rgmx_mark_ready(struct rgmx_softc_dev *sc);
static void octeon_rgmx_stop(struct rgmx_softc_dev *sc);
static void octeon_rgmx_config_speed(u_int port, u_int);
#ifdef DEBUG_RGMX_DUMP
static void octeon_dump_rgmx_stats(u_int port);
static void octeon_dump_pow_stats(void);
#endif
#ifdef __not_used__
static void rgmx_timer_periodic(void);
#endif
static void octeon_rgmx_enable_RED_all(int, int);

#ifdef OCTEON_RGMX_SCHEDULED_ISRS
static void octeon_rgmx_isr_link(void *context, int pending);
static void octeon_rgmx_isr_rxtx(void *context, int pending);
static int octeon_rgmx_intr_fast(void *arg);
#else
static int octeon_rgmx_intr(void *arg);
#endif







/* Standard driver entry points.  These can be static.  */
static void  octeon_rgmx_init	      (void *);
//static driver_intr_t    rgmx_intr;
static int   octeon_rgmx_ioctl        (struct ifnet *, u_long, caddr_t);
static void  octeon_rgmx_output_start (struct ifnet *);
static void  octeon_rgmx_output_start_locked (struct ifnet *);
#if 0
static void  octeon_rgmx_watchdog     (struct ifnet *);
#endif
static int   octeon_rgmx_medchange    (struct ifnet *);
static void  octeon_rgmx_medstat      (struct ifnet *, struct ifmediareq *);


/* Mapping between media bitmap (in fe_softc.mbitmap) and ifm_media.  */
static int const bit2media [] = {
                        IFM_ETHER | IFM_AUTO,
                        IFM_ETHER | IFM_MANUAL,
                        IFM_ETHER | IFM_10_T,
                        IFM_ETHER | IFM_10_2,
                        IFM_ETHER | IFM_10_5,
                        IFM_ETHER | IFM_10_FL,
                        IFM_ETHER | IFM_10_T,
        /* More can be added here... */
};

/* Mapping between media bitmap (in fe_softc.mbitmap) and ifm_media.  */
#define MB_HA   0x0001
#define MB_HM   0x0002
#define MB_HT   0x0004
#define MB_H2   0x0008
#define MB_H5   0x0010
#define MB_HF   0x0020
#define MB_FT   0x0040

#define LEBLEN          (ETHER_MAX_LEN + ETHER_VLAN_ENCAP_LEN)


static struct rgmx_softc_dev *rgmx_scdev_array[OCTEON_RGMX_NUM_PORTS_MAX] = {NULL};
static u_int port_array[OCTEON_RGMX_NUM_PORTS_MAX] = {0};
static u_int num_devices = 0;
static octeon_pko_sw_queue_info_t output_queues_array[OCTEON_RGMX_NUM_PORTS_MAX * OCTEON_RGMX_OQUEUE_PER_PORT];
static struct resource		*irq_res;       /* Interrupt resource. */
static void		*int_handler_tag;


#ifdef	OCTEON_RGMX_SCHEDULED_ISRS

struct task	link_isr_task;
struct task	rxtx_isr_task;
struct taskqueue *tq;		/* private task queue */

#endif



static u_int get_rgmx_port_ordinal (u_int port)
{
    u_int idx;

    for (idx = 0; idx < OCTEON_RGMX_NUM_PORTS_MAX; idx++) {
        if (port_array[idx] == port) {
            return (idx);
        }
    }
    return (-1);
}

static struct rgmx_softc_dev *get_rgmx_softc (u_int port)
{
    u_int idx;

    idx = get_rgmx_port_ordinal(port);
    if (idx != -1) {
            return (rgmx_scdev_array[idx]);
    }
    return (NULL);
}



static void octeon_rgmx_init_sc (struct rgmx_softc_dev *sc, device_t dev, u_int port, u_int num_devices)
{
    int ii;

    	/* No software-controllable media selection.  */
    	sc->mbitmap = MB_HM;
        sc->defmedia = MB_HM;

        sc->sc_dev = dev;
        sc->port = port;
        sc->idx = num_devices;
        sc->link_status = 0;
        sc->sc_unit = num_devices;
        sc->mbitmap = MB_HT;
        sc->defmedia = MB_HT;
        sc->tx_pending_queue.ifq_maxlen = NUM_TX_PACKETS;
        sc->tx_pending_queue.ifq_head = sc->tx_pending_queue.ifq_tail = NULL;
        sc->tx_pending_queue.ifq_len = sc->tx_pending_queue.ifq_drops = 0;
        mtx_init(&sc->tx_pending_queue.ifq_mtx, "if->sc->txpq.ifqmtx", NULL, MTX_DEF);

        sc->outq_ptr = &(output_queues_array[num_devices * OCTEON_RGMX_OQUEUE_PER_PORT]);

        for (ii = 0; ii < 6; ii++) {
            sc->ieee[ii] = octeon_mac_addr[ii];
        }
        sc->ieee[5] += get_rgmx_port_ordinal(port);

}

static int octeon_rgmx_init_ifnet (struct rgmx_softc_dev *sc)
{
        struct ifnet *ifp;

    	ifp = sc->ifp = if_alloc(IFT_ETHER);
        if (NULL == ifp) {
            device_printf(sc->sc_dev, "can not ifalloc for rgmx port\n");
            return (ENOSPC);
        }
        /*
         * Initialize ifnet structure
         */
        ifp->if_softc    = sc;
        if_initname(sc->ifp, device_get_name(sc->sc_dev), device_get_unit(sc->sc_dev));
        ifp->if_start    = octeon_rgmx_output_start;
        ifp->if_ioctl    = octeon_rgmx_ioctl;
        /* Watchdog interface is now deprecated.
        ifp->if_watchdog = octeon_rgmx_watchdog;
        */
	ifp->if_hwassist = CSUM_TCP | CSUM_UDP;
	ifp->if_capabilities = IFCAP_HWCSUM;
	ifp->if_capenable = ifp->if_capabilities;
        ifp->if_init     = octeon_rgmx_init;
        ifp->if_linkmib  = NULL;  // &sc->mibdata;
        ifp->if_linkmiblen = 0;   // sizeof (sc->mibdata);
        /*
         * Set fixed interface flags.
         */
        ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
//                        | IFF_NEEDSGIANT;
        if (ifp->if_snd.ifq_maxlen == 0)
                ifp->if_snd.ifq_maxlen = ifqmaxlen;

        ifmedia_init(&sc->media, 0, octeon_rgmx_medchange, octeon_rgmx_medstat);
        ifmedia_add(&sc->media, bit2media[0], 0, NULL);
        ifmedia_set(&sc->media, bit2media[0]);

        ether_ifattach(sc->ifp, sc->ieee);
        /* Print additional info when attached.  */
        device_printf(sc->sc_dev, "type %s, full duplex\n", sc->typestr);

        return (0);
}



/* Driver methods */


/* ------------------------------------------------------------------- *
 *                      rgmii_identify()                               *
 * ------------------------------------------------------------------- */
static void rgmii_identify (driver_t *drv, device_t parent)
{
	BUS_ADD_CHILD(parent, 0, "rgmii", 0);
}


/* ------------------------------------------------------------------- *
 *                      rgmii_probe()                                  *
 * ------------------------------------------------------------------- */
static int rgmii_probe (device_t dev)
{
	if (device_get_unit(dev) != 0)
		panic("can't probe/attach more rgmii devices\n");

        device_set_desc(dev, "Octeon RGMII");
	return (0);
}



/* ------------------------------------------------------------------- *
 *                      rgmii_attach()                                 *
 * ------------------------------------------------------------------- */
static int rgmii_attach (device_t dev)
{
    	struct rgmx_softc_dev	*sc;
	device_t		 child;
	int			 iface, port, nr_ports, error;
        void			 *softc;
        int			irq_rid;

        octeon_config_hw_units_pre_ports();

	/* Count interfaces and ports*/
	octeon_gmxx_inf_mode_t iface_mode;
	iface_mode.word64 = 0;

	for (iface = 0; iface < 2; iface++) {
		iface_mode.word64 = oct_read64(OCTEON_RGMX_INF_MODE(iface));

		/* interface is either disabled or SPI */
		if (!iface_mode.bits.en)
			continue;
		if (octeon_get_chipid()  == OCTEON_CN3020_CHIP) {
			nr_ports = 2;
		} else {
			nr_ports = (octeon_has_4ports()) ? 4 : 3;
			if (iface_mode.bits.type ) {
				if (octeon_get_chipid() == OCTEON_CN5020_CHIP) 
					nr_ports = 2;
				else
					continue;
			}
		}

		oct_write64(OCTEON_RGMX_TX_PRTS(iface), nr_ports);

		for (port = iface * 16; port < iface * 16 + nr_ports; port++) {

                        child = device_add_child(dev, OCTEON_RGMX_DEV_NAME, num_devices);
                        if (child == NULL)
                            	panic("%s: device_add_child() failed\n", __func__);

                        softc = malloc(sizeof(struct rgmx_softc_dev), M_DEVBUF, M_NOWAIT | M_ZERO);
                        if (!softc) {
                            	panic("%s malloc failed for softc\n", __func__);
                        }
                        device_set_softc(child, softc);
                        device_set_desc(child, "Octeon RGMII");
                        sc = device_get_softc(child);
                        if (!sc) {
                            	printf(" No sc\n");
                                num_devices++;
                                continue;
                        }
                        port_array[num_devices] = port;
                        rgmx_scdev_array[num_devices] = sc;
                        RGMX_LOCK_INIT(sc, device_get_nameunit(child));
                        octeon_rgmx_init_sc(sc, child, port, num_devices);
                        octeon_config_hw_units_port(sc, port);
                        if (octeon_rgmx_init_ifnet(sc)) {
                            	device_printf(dev, "  ifinit failed for rgmx port %u\n", port);
                                return (ENOSPC);
                        }
/*
 * Don't call octeon_rgmx_mark_ready()
 * ifnet will call it indirectly via  octeon_rgmx_init()
 *
 *                         octeon_rgmx_mark_ready(sc);
 */
                        num_devices++;
                }
	}

        octeon_config_hw_units_post_ports();

	irq_rid = 0;
	irq_res = bus_alloc_resource(dev, SYS_RES_IRQ, &irq_rid, 0, 0, 1, RF_SHAREABLE | RF_ACTIVE);
	if (irq_res == NULL) {
		device_printf(dev, "failed to allocate irq\n");
		return (ENXIO);
	}


#ifdef	OCTEON_RGMX_SCHEDULED_ISRS
        /*
         * Single task queues for all child devices. Since POW gives us a unified
         * interrupt based on POW groups, not based on PORTs.
         */
	TASK_INIT(&rxtx_isr_task, 0, octeon_rgmx_isr_rxtx, NULL);
	TASK_INIT(&link_isr_task, 0, octeon_rgmx_isr_link, NULL);
	tq = taskqueue_create_fast("octeon_rgmx_taskq", M_NOWAIT,
                                   taskqueue_thread_enqueue, &tq);
	taskqueue_start_threads(&tq, 1, PI_NET, "%s taskq", device_get_nameunit(dev));

	error = bus_setup_intr(dev, irq_res, INTR_TYPE_NET, octeon_rgmx_intr_fast, NULL,
                               NULL, &int_handler_tag);
	if (error != 0) {
		device_printf(dev, "bus_setup_intr returned %d\n", error);
		taskqueue_free(tq);
                tq = NULL;
		return (error);
	}

#else  /* OCTEON_RGMX_SCHEDULED_ISRS */

	error = bus_setup_intr(dev, irq_res, INTR_TYPE_NET, octeon_rgmx_intr, NULL,
                               NULL, &int_handler_tag);

        if (error != 0) {
		device_printf(dev, "bus_setup_intr returned %d\n", error);
                tq = NULL;
		return (error);
	}

#endif  /* OCTEON_RGMX_SCHEDULED_ISRS */

	return (bus_generic_attach(dev));
}




#define OCTEON_MAX_RGMX_PORT_NUMS 32



#define	OCTEON_POW_RX_GROUP_NUM		0
#define	OCTEON_POW_TX_GROUP_NUM		1	/* If using TX WQE from PKO */

#define	OCTEON_POW_RX_GROUP_MASK	(1 << OCTEON_POW_RX_GROUP_NUM)
#define	OCTEON_POW_TX_GROUP_MASK	(1 << OCTEON_POW_TX_GROUP_NUM)
#define OCTEON_POW_ALL_OUR_GROUPS_MASK  (OCTEON_POW_RX_GROUP_MASK | OCTEON_POW_RX_GROUP_MASK)
#define OCTEON_POW_ALL_GROUPS_MASK	0xffff
#define	OCTEON_POW_WORKQUEUE_INT       (0x8001670000000200ull)
#define	OCTEON_POW_WORKQUEUE_INT_PC    (0x8001670000000208ull)
#define	OCTEON_POW_WORKQUEUE_INT_THRESHOLD(group_num)  ((0x8001670000000080ull+((group_num)*0x8)))
#define OCTEON_RGMX_POW_NOS_CNT		(0x8001670000000228ull)
#define OCTEON_POW_INT_CNTR(core)	(0x8001670000000100ull+((core)*0x8))
#define OCTEON_POW_INPT_Q_ALL_QOS	(0x8001670000000388ull)
#define OCTEON_POW_INPT_QOS_GRP(grp)	(0x8001670000000340ull + ((grp) * 0x8))




#define NUM_RX_PACKETS_CTL		(MAX_RX_BUFS + 3000)
#define NUM_TX_PACKETS_CTL		40

#define FPA_NOPOOL			0

#define OCTEON_FPA_RX_PACKET_POOL		0
#define OCTEON_FPA_RX_PACKET_POOL_WORDS		208		/* 2048 bytes */
#define OCTEON_FPA_RX_PACKET_POOL_ELEM_SIZE	(OCTEON_FPA_RX_PACKET_POOL_WORDS)
#define OCTEON_FPA_RX_PACKET_POOL_ELEMENTS	(MAX_RX_BUFS)
#define OCTEON_RX_MAX_SIZE			(OCTEON_FPA_RX_PACKET_POOL_WORDS * sizeof(uint64_t))

#define OCTEON_FPA_WQE_RX_POOL			1
#define OCTEON_FPA_WQE_RX_WORDS			(OCTEON_CACHE_LINE_SIZE/8)
#define OCTEON_FPA_WQE_RX_POOL_ELEM_SIZE	(OCTEON_FPA_WQE_RX_WORDS)
#define OCTEON_FPA_WQE_RX_POOL_ELEMENTS      	(NUM_RX_PACKETS_CTL)

#define OCTEON_FPA_TX_PACKET_POOL		2
#define OCTEON_FPA_TX_PACKET_POOL_WORDS		208		/* 2048 bytes */
#define OCTEON_FPA_TX_PACKET_POOL_ELEM_SIZE	(OCTEON_FPA_TX_PACKET_POOL_WORDS)
#define OCTEON_FPA_TX_PACKET_POOL_ELEMENTS	(MAX_TX_BUFS)
#define OCTEON_TX_MAX_SIZE			(OCTEON_FPA_TX_PACKET_POOL_WORDS * sizeof(uint64_t))

#define OCTEON_FPA_TX_CMDBUF_POOL		3
#define OCTEON_FPA_TX_CMD_SIZE			2
#define OCTEON_FPA_TX_CMD_NUM			300
#define	OCTEON_FPA_TX_CMDBUF_POOL_WORDS		(OCTEON_FPA_TX_CMD_SIZE * OCTEON_FPA_TX_CMD_NUM)
#define OCTEON_FPA_TX_CMDBUF_POOL_ELEM_SIZE	(OCTEON_FPA_TX_CMDBUF_POOL_WORDS +1)
#define OCTEON_FPA_TX_CMDBUF_POOL_ELEMENTS	(30 * OCTEON_RGMX_NUM_PORTS_MAX)

#define FIRST_PARTICLE_SKIP		0
#define NOT_FIRST_PARTICLE_SKIP		0

#define ENABLE_BACK_PRESSURE		0
#define RGMX_MAX_PAK_RECEIVE		5000000


#ifdef	OCTEON_RGMX_SCHEDULED_ISRS


static void octeon_rgmx_isr_link (void *context, int pending)
{
    	octeon_line_status_loop(NULL);
}


static void octeon_rgmx_isr_rxtx (void *context, int pending)
{
    	octeon_rx_loop(NULL);
}


/*********************************************************************
 *
 *  Fast Interrupt Service routine
 *
 *********************************************************************/

//#define OCTEON_RGMX_POW_TIME_THR_INTS 1


static int octeon_rgmx_intr_fast(void *arg)
{

    	int handled_flag = 0;
        uint64_t ciu_summary;

        ciu_summary = ciu_get_int_summary(CIU_THIS_CORE, OCTEON_RGMX_CIU_INTX,
                                          OCTEON_RGMX_CIU_ENX);

        if (ciu_summary & CIU_GENTIMER_BITS_ENABLE(CIU_GENTIMER_NUM_1)) {

            	/*
                 * Timer Interrupt for link status checks
                 * Acknowledging it will mask it for this cycle.
                 */
            	ciu_clear_int_summary(CIU_THIS_CORE, OCTEON_RGMX_CIU_INTX,
                                      OCTEON_RGMX_CIU_ENX,
                                      CIU_GENTIMER_BITS_ENABLE(CIU_GENTIMER_NUM_1));

		taskqueue_enqueue(taskqueue_fast, &link_isr_task);
                handled_flag = 1;
        }

        if (ciu_summary & OCTEON_POW_ALL_GROUPS_MASK) {
#ifndef OCTEON_RGMX_POW_TIME_THR_INTS
		/*
                 * When using POW IQ/DSQ size based interrupts, then
                 *    ack the interrupts right away.  So they don't interrupt
                 *    until the queue size goes to 0 again.
                 */
                oct_write64(OCTEON_POW_WORKQUEUE_INT,
                            0x10001 << OCTEON_POW_RX_GROUP_NUM);

#else
            	/*
                 * We use POW thresholds based interrupt signalled on timer
                 *   countdown. Acknowledge it now so that it doesn't
                 *   interrupt us until next countdown to zero.
                 */
                oct_write64(OCTEON_POW_WORKQUEUE_INT,
                            0x1 << OCTEON_POW_RX_GROUP_NUM);
#endif

            	taskqueue_enqueue(tq, &rxtx_isr_task);
                handled_flag = 1;
        }

	return ((handled_flag) ? FILTER_HANDLED : FILTER_STRAY);
}


#else   /*  ! OCTEON_RGMX_SCHEDULED_ISRS */


/*
 * octeon_rgmx_intr
 *
 * This is direct inline isr. Will do all its work and heavy-lifting in interrupt context.
 *
 * Also note that the  RGMX_LOCK/UNLOCK code will have to checked/added, since that is new and
 * was not supported with this model.
 */
static int octeon_rgmx_intr (void *arg)
{
    	int flag = 0;
        uint64_t ciu_summary;

        /*
         * read ciu to see if any bits are pow
         */
        while (1) {
            	ciu_summary = ciu_get_int_summary(CIU_THIS_CORE, OCTEON_RGMX_CIU_INTX,
                                                  OCTEON_RGMX_CIU_ENX);

                if ((ciu_summary & (OCTEON_POW_ALL_GROUPS_MASK | CIU_GENTIMER_BITS_ENABLE(CIU_GENTIMER_NUM_1))) == 0) {
                    	break;
                }

                flag = 1;

                if (ciu_summary & OCTEON_POW_ALL_GROUPS_MASK) {
                        octeon_rx_loop(NULL);
                        /*
                         * Acknowledge the interrupt after processing queues.
                         */
                    	oct_write64(OCTEON_POW_WORKQUEUE_INT, OCTEON_POW_RX_GROUP_MASK);
                }
                if (ciu_summary & CIU_GENTIMER_BITS_ENABLE(CIU_GENTIMER_NUM_1)) {
                    	octeon_line_status_loop(NULL);
                    	ciu_clear_int_summary(CIU_THIS_CORE, OCTEON_RGMX_CIU_INTX,
                                              OCTEON_RGMX_CIU_ENX,
                                              CIU_GENTIMER_BITS_ENABLE(CIU_GENTIMER_NUM_1));
                }
        }

	return ((flag) ? FILTER_HANDLED : FILTER_STRAY);
}


#endif   /*  OCTEON_RGMX_SCHEDULED_ISRS */



static struct mbuf *octeon_rgmx_build_new_rx_mbuf(struct ifnet *ifp, void *data_start, u_int totlen);

static struct mbuf *octeon_rgmx_build_new_rx_mbuf (struct ifnet *ifp, void *data_start, u_int totlen)
{
	struct mbuf *m, *m0, *newm;
	caddr_t newdata;
	int len;

	if (totlen <= ETHER_HDR_LEN || totlen > LEBLEN - ETHER_CRC_LEN) {
#ifdef LEDEBUG
		if_printf(ifp, "invalid packet size %d; dropping\n", totlen);
#endif
		return (NULL);
	}

	MGETHDR(m0, M_DONTWAIT, MT_DATA);
	if (m0 == NULL) {
		return (NULL);
        }

        /* Initialize packet header info.  */
	m0->m_pkthdr.rcvif = ifp;
	m0->m_pkthdr.len = totlen;
        m0->m_pkthdr.csum_flags = CSUM_IP_CHECKED | CSUM_IP_VALID | CSUM_DATA_VALID | CSUM_PSEUDO_HDR;
        m0->m_pkthdr.csum_data = 0xffff;
	len = MHLEN;
	m = m0;

	while (totlen > 0) {
		if (totlen >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0)
				goto octeon_rgmx_build_new_rx_mbuf_bad;
			len = MCLBYTES;
		}

		if (m == m0) {
			newdata = (caddr_t)ALIGN(m->m_data + ETHER_HDR_LEN) - ETHER_HDR_LEN;
			len -= newdata - m->m_data;
			m->m_data = newdata;
		}

                /* Set the length of this mbuf.  */
		m->m_len = len = min(totlen, len);
                bcopy(data_start, mtod(m, caddr_t), len);
		data_start = (void *) (((u_long) (data_start)) + len);

		totlen -= len;
		if (totlen > 0) {
			MGET(newm, M_DONTWAIT, MT_DATA);
			if (newm == 0)
				goto octeon_rgmx_build_new_rx_mbuf_bad;
			len = MLEN;
			m = m->m_next = newm;
		}
	}

	return (m0);

octeon_rgmx_build_new_rx_mbuf_bad:

	m_freem(m0);
	return (NULL);
}



//#define DEBUG_RX 1

static void  octeon_rgmx_rx_process_work (octeon_wqe_t *work, u_int port)
{
    	struct rgmx_softc_dev  *sc;
        struct ifnet *ifp;
        u_int	len;
        void	*data_start, *new_data_start;
        struct mbuf *mbuf;

//#define DEBUG_RX_PKT_DUMP 1
#ifdef DEBUG_RX_PKT_DUMP
        int i; u_char *dc;
#endif
        
        data_start = octeon_pow_pktptr_to_kbuffer(work->packet_ptr);

//#define DEBUG_RX2
#ifdef DEBUG_RX2
        printf(" WQE 0x%X: port:%u  ", work, port);
        printf(" Grp: %u, %llX  Tag: %u  %llX  type: %u 0x%llx\n",
               work->grp, work->grp, work->tag, work->tag, work->tag_type, work->tag_type);
#endif

        if ((port >= OCTEON_RGMX_MIN_PORT) || (port <= OCTEON_RGMX_MAX_PORT)) {

            	sc = get_rgmx_softc(port);

                if (!sc || !sc->ifp) {

                    	printf(" octeon_rgmx_rx_process_work No sc or sc->ifp -  port:%u", port);
                } else {

                    	ifp = sc->ifp;

                    	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {

                                if (!work->word2.bits.rcv_error) {

                                    len = work->len;

                                    /*
                                     * We cannot pass the same FPA phys-buffer higher up.
                                     * User space will not be able to use this phys-buffer.
                                     *
                                     * Start building a mbuf packet here using  data_start & len.
                                     */

                                    new_data_start = data_start;
                                    if (!work->word2.bits.not_IP) {
                                    	new_data_start = (void *) (((unsigned long) (new_data_start)) + 14);
                                        /* mark it as checksum checked */
                                    } else {
                                    	new_data_start = (void *) (((unsigned long) (new_data_start)) + 8);
                                    }

#ifdef DEBUG_RX_PKT_DUMP
                                    dc = new_data_start; printf("In:\n");
                                    for (i = 0; i < len; i++) { if (!(i % 16)) printf ("\n"); printf(" %02X", dc[i]); }
#endif
                                
                                    mbuf = octeon_rgmx_build_new_rx_mbuf(ifp, new_data_start, len);
                                    if (mbuf) {
//                                    	printf(" Passing pkt to ifp: pkt_len: %u len: %u ", mbuf->m_pkthdr.len, mbuf->m_len);
#ifdef DEBUG_RX_PKT_DUMP

                                        dc = mtod(mbuf, u_char *); printf("\n"); printf("In: ");
                                        for (i = 0; i < mbuf->m_len; i++) { if (!(i % 16)) printf ("\n"); printf(" %02X", dc[i]); }

#endif

                                    	/* Feed the packet to upper layer.  */
                                    	(*ifp->if_input)(ifp, mbuf);
                                        ifp->if_ipackets++;

                                    } else {  /* mbuf error */
                                    	if_printf(ifp, "mbuf rx construct error\n");
                                        printf(" mbuf rx construct error\n");
                                        ifp->if_ierrors++;
                                    }	      /*  mbuf error */

                                } else {      /*  rcv_error */
                                    ifp->if_ierrors++;
                                }            /*  rcv_error */

                        } /* IFF_DRV_RUNNING */

                }   /*  sc && sc->ifp */

        } else {    /* port number */
            	printf(" rgmx_rx:%u bad port\n", port);
        }

        octeon_fpa_free(data_start, OCTEON_FPA_RX_PACKET_POOL, 0);
        octeon_fpa_free((void *)work, OCTEON_FPA_WQE_RX_POOL, 0);
}




/* ------------------------------------------------------------------- *
 *                      octeon_rx_loop()                               *
 * ------------------------------------------------------------------- */


//#define OCTEON_VISUAL_RGMX 1
#ifdef OCTEON_VISUAL_RGMX
static int where0 = 0;
static int where1 = 0;
#endif

static void octeon_rx_loop (void *unused)
{
	u_int		core_id;
	uint64_t 	prev_grp_mask;
	u_int		pak_count;
	octeon_wqe_t 	*work;

	core_id = octeon_get_core_num();
	pak_count = 0;

	/* Only allow work for our group */
	prev_grp_mask = oct_read64(OCTEON_POW_CORE_GROUP_MASK(core_id));
	oct_write64(OCTEON_POW_CORE_GROUP_MASK(core_id), OCTEON_POW_ALL_GROUPS_MASK);


#ifdef OCTEON_VISUAL_RGMX
        octeon_led_run_wheel(&where0, 3);
#endif
	while(1) {

                if (pak_count++ > RGMX_MAX_PAK_RECEIVE) {
                    break;
                }

        	work = octeon_pow_work_request_sync(OCTEON_POW_WAIT);

		if (work == NULL) {
                    	/*
                         * No more incoming packets. We can take a break now.
                         */
                    	break;
		}

#ifdef OCTEON_VISUAL_RGMX
                octeon_led_run_wheel(&where1, 4);
#endif
                octeon_rgmx_rx_process_work(work, work->ipprt);

	}

	oct_write64(OCTEON_POW_CORE_GROUP_MASK(core_id), prev_grp_mask);
}


static void *octeon_rgmx_write_mbufs_to_fpa_buff (struct rgmx_softc_dev *sc, struct mbuf *m, u_int len)
{
        struct mbuf *mp;
        void *data_area;
        u_char *write_offset;

        /*
         * FIXME
         *
         * Compare len with max FPA-tx-packet size. Or else we will possibly corrupt the next pkt.
         */


        /*
         * Get an FPA buffer from Xmit-packets FPA pool
         */
        data_area = octeon_fpa_alloc(OCTEON_FPA_TX_PACKET_POOL);
        if (!data_area) {
            	/*
                 * Fail.  No room. No resources.
                 */
            	return (NULL);
        }

        /*
         * Transfer the data from mbuf chain to the transmission buffer.
         */
        write_offset = data_area;
        for (mp = m; mp != 0; mp = mp->m_next) {
            	if (mp->m_len) {
                    	bcopy(mtod(mp, caddr_t), write_offset, mp->m_len);
                        write_offset = (u_char *) (((u_long) write_offset) + mp->m_len);
                }
        }
        return (data_area);
}


static u_int octeon_rgmx_pko_xmit_packet (struct rgmx_softc_dev *sc, void *out_buff, u_int len, u_int checksum)
{
    octeon_pko_command_word0_t	pko_cmd;
    octeon_pko_packet_ptr_t	pko_pkt_word;
    u_long temp;
    u_short xmit_cmd_index;
    uint64_t *xmit_cmd_ptr;
    uint64_t xmit_cmd_state;
    int queue = 0;		// we should randomize queue # based on core num. Using same
				// queue 0 for this port, by all cores on is less efficient.

    /*
     * Prepare the PKO buffer and command word.
     *   Cmd Buf Word 0
     *   No FAU
     *   Set #-segs and #-bytes
     */
    pko_cmd.word64 = 0;
    pko_cmd.bits.segs = 1;
    pko_cmd.bits.total_bytes = len;
    if (checksum) {
        pko_cmd.bits.ipoffp1 = ETHER_HDR_LEN + 1;	/* IPOffP1 is +1 based.  1 means offset 0 */
    }

    /*
     * Build the PKO buffer pointer. PKO Cmd Buf Word 1
     */
    pko_pkt_word.word64 = 0;
    pko_pkt_word.bits.addr =  OCTEON_PTR2PHYS(out_buff);
    pko_pkt_word.bits.pool =  OCTEON_FPA_TX_PACKET_POOL;
    pko_pkt_word.bits.size =  2048; // dummy. Actual len is above.

#ifdef DEBUG_TX
    printf(" PKO: 0x%llX  0x%llX ", pko_cmd.word64, pko_pkt_word.word64);
#endif

    /*
     * Get the queue command ptr location from the per port per queue, pko info struct.
     */
    octeon_spinlock_lock(&(sc->outq_ptr[queue].lock));
#ifdef DEBUG_TX
    printf(" xmit: sc->outq_ptr[queue].xmit_command_state: 0x%llX  ", sc->outq_ptr[queue].xmit_command_state);
#endif
    xmit_cmd_state = sc->outq_ptr[queue].xmit_command_state;
    sc->outq_ptr[queue].xmit_command_state = xmit_cmd_state + 2;

    temp = (u_long) (xmit_cmd_state >> OCTEON_PKO_INDEX_BITS);
#ifdef DEBUG_TX
    printf(" temp: 0x%X ", temp);
#endif
    xmit_cmd_ptr = (uint64_t *) MIPS_PHYS_TO_KSEG0(temp);
    xmit_cmd_index = xmit_cmd_state & OCTEON_PKO_INDEX_MASK;
    xmit_cmd_ptr += xmit_cmd_index;

    /*
     * We end the PKO cmd buffer at odd boundary. Towards the end we will have
     * 4 or 3 or 2 or 1 or 0 word remaining.  Case of 4, 2, or 0 can never happen.
     * We only care when we have 3 words remaining. In this case we write our 2 words
     * for PKO command and 3rd word as chain for next PKO cmd buffer.
     */
    xmit_cmd_ptr[0] = pko_cmd.word64;

    if (xmit_cmd_index < (OCTEON_FPA_TX_CMDBUF_POOL_WORDS - 2)) {
        /*
         * Plenty of space left. Write our 2nd word and worry the next time.
         */
        xmit_cmd_ptr[1] = pko_pkt_word.word64;

    } else {
        /*
         * 3 words or less are left. We write our 2nd word now and then put in a chain link
         * to new PKO cmd buf.
         */
        void *pko_cmd_buf = octeon_fpa_alloc(OCTEON_FPA_TX_CMDBUF_POOL);
        uint64_t phys_cmd_buf;

        if (!pko_cmd_buf) {
            /*
             * FPA pool for xmit-buffer-commands is empty.
             */
            sc->outq_ptr[queue].xmit_command_state -= 2;
            octeon_spinlock_unlock(&(sc->outq_ptr[queue].lock));
            return (0);
        }
        phys_cmd_buf = OCTEON_PTR2PHYS(pko_cmd_buf);

        xmit_cmd_ptr[1] = pko_pkt_word.word64;
        xmit_cmd_ptr[2] = phys_cmd_buf;

        sc->outq_ptr[queue].xmit_command_state = (phys_cmd_buf << OCTEON_PKO_INDEX_BITS);
    }
    /*
     * Unlock queue structures.
     */
    octeon_spinlock_unlock(&(sc->outq_ptr[queue].lock));

    /*
     * 2 words incremented in PKO. Ring the doorbell.
     */
#ifdef DEBUG_TX
    printf(" Ringing doorbell: Port %u  Queue %u  words 2", sc->port, octeon_pko_get_base_queue(sc->port) + queue);
#endif
    octeon_pko_ring_doorbell(sc->port, octeon_pko_get_base_queue(sc->port) + queue, 2);

    return (1);
}


static void octeon_rgmx_xmit_mark_buffers_done(struct rgmx_softc_dev *sc, u_int n);

static void octeon_rgmx_xmit_mark_buffers_done (struct rgmx_softc_dev *sc, u_int n)
{
        struct mbuf *m;
        u_int i;

        for (i = 0; i < n; i++) {
            	/*
                 * Remove packets in queue. Leaving a lag of 3, to allow for PKO in-flight xmission
                 */
            	if (_IF_QLEN(&sc->tx_pending_queue) > 4) {
                    	IF_DEQUEUE(&sc->tx_pending_queue, m);
                        if (!m) {
                                break;		// Queue became empty now. Break out.
                        }
                        /*
                         * Return the mbuf to system.
                         */
                        m_freem(m);
                }
        }
        if (!i) {
            	return;	// Nothing removed from queue.
        }

        /*
         * The transmitter is no more active.
         * Reset output active flag and watchdog timer.
         */
        sc->ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
        sc->ifp->if_timer = 0;
}


#define OCTEON_RGMX_FLUSH_N_XMIT_MBUFS_EACH_LOOP	5
#define OCTEON_RGMX_FLUSH_PENDING_MBUFS_MAX		1000

#ifdef __not_used__
/*
 * octeon_rgmx_output_flush
 *
 * Drop all packets queued at ifnet layer.
 */
static void octeon_rgmx_output_flush (struct ifnet *ifp)
{
        struct mbuf *m;
        u_int max_flush = OCTEON_RGMX_FLUSH_PENDING_MBUFS_MAX;	/* Arbitrarily high number */

        while (max_flush-- && _IF_QLEN(&ifp->if_snd)) {
                /*
                 * Get the next mbuf Packet chain to flush.
                 */
                IF_DEQUEUE(&ifp->if_snd, m);
                if (m == NULL) {
                        /* No more packets to flush */
                    	break;
                }
                _IF_DROP(&ifp->if_snd);
                m_freem(m);
                ifp->if_oerrors++;
        }
}
#endif

/*
 * octeon_rgmx_output_start
 *
 * Start output on interface.
 */
static void octeon_rgmx_output_start (struct ifnet *ifp)
{
    	struct rgmx_softc_dev *sc = ifp->if_softc;

	RGMX_LOCK(sc);
        octeon_rgmx_output_start_locked(ifp);
	RGMX_UNLOCK(sc);
}



/*
 * octeon_rgmx_output_start_locked
 *
 * Start output on interface.  Assume Driver locked
 */
static void octeon_rgmx_output_start_locked (struct ifnet *ifp)
{
    	struct rgmx_softc_dev *sc = ifp->if_softc;
        struct mbuf *m;
        u_int len, need_l4_checksum;
        void *out_buff;

	/*
         * Take out some of the last queued mbuf's from xmit-pending queue
         */
        octeon_rgmx_xmit_mark_buffers_done(sc, OCTEON_RGMX_FLUSH_N_XMIT_MBUFS_EACH_LOOP);

        while (1) {
                /*
                 * See if there is room to put another packet in the buffer.
                 * We *could* do better job by peeking the send queue to
                 * know the length of the next packet.  Current version just
                 * tests against the worst case (i.e., longest packet).  FIXME.
                 *
                 * When adding the packet-peek feature, don't forget adding a
                 * test on txb_count against QUEUEING_MAX.
                 * There is a little chance the packet count exceeds
                 * the limit.  Assume transmission buffer is 8KB (2x8KB
                 * configuration) and an application sends a bunch of small
                 * (i.e., minimum packet sized) packets rapidly.  An 8KB
                 * buffer can hold 130 blocks of 62 bytes long...
                 */

            	/*
                 * If unable to send more.
                 */
            	if (_IF_QLEN(&sc->tx_pending_queue) >= MAX_TX_BUFS) {
                    printf(" Xmit not possible. NO room %u", _IF_QLEN(&sc->tx_pending_queue));
                    goto indicate_active;
                }


                /*
                 * Get the next mbuf chain for a packet to send.
                 */
                IF_DEQUEUE(&ifp->if_snd, m);
                if (m == NULL) {
                        /* No more packets to send.  */
                        goto indicate_inactive;
                }

                len = m->m_pkthdr.len;
                /*
                 * Should never send big packets.  If such a packet is passed,
                 * it should be a bug of upper layer.  We just ignore it.
                 * ... Partial (too short) packets, neither.
                 */
                if (len < ETHER_HDR_LEN ||
                    len > ETHER_MAX_LEN - ETHER_CRC_LEN) {
                    	/*
                         * Fail.  Bad packet size.  Return the mbuf to system.
                         */
                    	if_printf(ifp,
                                  "got an out-of-spec packet (%u bytes) to send\n", len);
                        m_freem(m);
                        goto indicate_active;
                }

                /*
                 * Copy the mbuf chain into the transmission buffer.
                 * txb_* variables are updated as necessary.
                 */
                out_buff = octeon_rgmx_write_mbufs_to_fpa_buff(sc, m, len);
                if (!out_buff) {
                    	/*
                         * No FPA physical buf resource.
                         * Let's requeue it back.  And slow it down for a while.
                         */
                    	IF_PREPEND(&ifp->if_snd, m);
                    	goto indicate_active;
                }

                need_l4_checksum = (m->m_pkthdr.csum_flags & (CSUM_TCP | CSUM_UDP)) ? 1 : 0;

                /*
                 * put the mbuf onto pending queue
                 */
//#define DEBUG_TX_PKT_DUMP 1
#ifdef DEBUG_TX_PKT_DUMP
                int ii;
                u_char *dc = out_buff;

                printf("\n"); printf("Out: ");
                for (ii = 0; ii < len; ii++) printf(" %X", dc[ii]); printf("\n");
#endif

        	IF_ENQUEUE(&sc->tx_pending_queue, m);

                /*
                 * Pass the mbuf data packet to PKO for xmission.
                 */
                octeon_rgmx_pko_xmit_packet(sc, out_buff, len, need_l4_checksum);

                ifp->if_opackets++;
        }

indicate_inactive:
        /*
         * We are using the !OACTIVE flag to indicate to
         * the outside world that we can accept an
         * additional packet rather than that the
         * transmitter is _actually_ active.  Indeed, the
         * transmitter may be active, but if we haven't
         * filled all the buffers with data then we still
         * want to accept more.
         */
        ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
        return;


indicate_active:
        /*
         * The transmitter is active, and there are no room for
         * more outgoing packets in the transmission buffer.
         */
	ifp->if_oerrors++;
//	sc->mibdata.dot3StatsInternalMacTransmitErrors++;
        ifp->if_drv_flags |= IFF_DRV_OACTIVE;
        return;
}




/* ------------------------------------------------------------------- *
 *                      octeon_config_hw_units()                       *
 * ------------------------------------------------------------------- *
 *
 * Initialize Octeon hardware components. To get the RGMX going.
 *
 */
static void octeon_config_hw_units_pre_ports (void)
{

        /* Enable FPA */
	octeon_enable_fpa();

        /* Enable PKO */
	octeon_pko_enable();

        /* Init PKO */
	octeon_pko_init();


	/* Fill FPA */

        /*
         * Input Buffers Pool
         * Pool 0
         */
        octeon_fpa_fill_pool_mem(OCTEON_FPA_RX_PACKET_POOL, OCTEON_FPA_RX_PACKET_POOL_ELEM_SIZE,
                                 OCTEON_FPA_RX_PACKET_POOL_ELEMENTS);

        /*
         * WQE Blocks Pool
         * Pool 1
         */
        octeon_fpa_fill_pool_mem(OCTEON_FPA_WQE_RX_POOL, OCTEON_FPA_WQE_RX_POOL_ELEM_SIZE,
                                 OCTEON_FPA_WQE_RX_POOL_ELEMENTS);

        /*
         * PKO Command Pool
         * Pool  3
         */
        octeon_fpa_fill_pool_mem(OCTEON_FPA_TX_CMDBUF_POOL, OCTEON_FPA_TX_CMDBUF_POOL_ELEM_SIZE,
                                 OCTEON_FPA_TX_CMDBUF_POOL_ELEMENTS);

        /*
         * Output Buffers Pool
         * Pool 2
         */
        octeon_fpa_fill_pool_mem(OCTEON_FPA_TX_PACKET_POOL, OCTEON_FPA_TX_PACKET_POOL_ELEM_SIZE,
                                 OCTEON_FPA_TX_PACKET_POOL_ELEMENTS);



        octeon_rgmx_enable_RED_all(OCTEON_FPA_RX_PACKET_POOL_ELEMENTS >> 2, OCTEON_FPA_RX_PACKET_POOL_ELEMENTS >> 3);

	/* Configure IPD */
	octeon_ipd_config(OCTEON_FPA_RX_PACKET_POOL_WORDS,
                          FIRST_PARTICLE_SKIP / 8,
                          NOT_FIRST_PARTICLE_SKIP / 8,
                          FIRST_PARTICLE_SKIP / 128,
                          NOT_FIRST_PARTICLE_SKIP / 128,
                          OCTEON_FPA_WQE_RX_POOL,
                          OCTEON_IPD_OPC_MODE_STF,
                          ENABLE_BACK_PRESSURE);

        /*
         * PKO setup Output Command Buffers
         */
        octeon_pko_config_cmdbuf_global_defaults(OCTEON_FPA_TX_CMDBUF_POOL,
                                                 OCTEON_FPA_TX_CMDBUF_POOL_ELEM_SIZE);

}



static void octeon_config_hw_units_port (struct rgmx_softc_dev *sc, u_int port)
{
	const u_int priorities[8] = {8,8,8,8,8,8,8,8};
        u_int total_queues, base_queue;

    	octeon_config_rgmii_port(port);

        total_queues = octeon_pko_get_num_queues(port);
        base_queue = octeon_pko_get_base_queue(port);
        /* Packet output configures Queue and Ports */
        octeon_pko_config_port(port, base_queue,
                               total_queues,
                               priorities,
                               OCTEON_FPA_TX_CMDBUF_POOL,
                               sc->outq_ptr);

        octeon_rgmx_set_mac(port);

        /* Setup Port input tagging */
        octeon_rgmx_config_pip(port);
}


typedef union
{
    uint64_t        word64;
    struct
    {
        uint64_t    rsvd3	: 35;
        uint64_t    enable	: 1;
        uint64_t    time_thr	: 4;
        uint64_t    rsvd2	: 1;
        uint64_t    ds_thr	: 11;
        uint64_t    rsvd	: 1;
        uint64_t    iq_thr	: 11;
    } bits;
} octeon_rgmx_pow_int_threshold_t;

typedef union
{
    uint64_t        word64;
    struct
    {
        uint64_t    rsvd	: 36;
        uint64_t    tc_cnt	: 4;
        uint64_t    ds_cnt	: 12;
        uint64_t    iq_cnt	: 12;
    } bits;
} octeon_rgmx_pow_int_cnt_t;

typedef union
{
    uint64_t        word64;
    struct
    {
        uint64_t    rsvd3	: 4;
        uint64_t    thr_freq	: 28;  // R/O
        uint64_t    rsvd2	: 4;
        uint64_t    thr_period	: 20;
        uint64_t    rsvd	: 8;
    } bits;
} octeon_rgmx_pow_int_pc_t;


typedef union
{
    uint64_t        word64;
    struct
    {
        uint64_t    rsvd	: 52;
        uint64_t    nos_cnt	: 12;
    } bits;
} octeon_rgmx_pow_nos_cnt;



typedef union
{
    uint64_t        word64;
    struct
    {
        uint64_t    rsvd	: 32;
        uint64_t    inb_pkts	: 32;
    } bits;
} octeon_rgmx_pip_inb_pkts;

typedef union
{
    uint64_t        word64;
    struct
    {
        uint64_t    rsvd	: 48;
        uint64_t    inb_errs	: 16;
    } bits;
} octeon_rgmx_pip_inb_errs;



typedef union
{
    uint64_t        word64;
    struct
    {
        uint64_t    rsvd	: 32;
        uint64_t    iq_cnt	: 32;
    } bits;
} octeon_pow_inpt_q_all_qos;



typedef union
{
    uint64_t        word64;
    struct
    {
        uint64_t    rsvd	: 32;
        uint64_t    iq_cnt	: 32;
    } bits;
} octeon_pow_inpt_q_grp_qos;


static void octeon_config_hw_units_post_ports (void)
{

    	octeon_rgmx_pow_int_threshold_t thr;
        octeon_rgmx_pow_int_pc_t intpc;
                          
    	thr.word64 = 0;
        intpc.word64 = 0;
        intpc.bits.thr_freq = (500 * 1000 * 1000) / (1000 * 16 * 256);

#ifdef OCTEON_RGMX_POW_TIME_THR_INTS
        thr.bits.enable = 1;
        thr.bits.time_thr = 0xf;
        oct_write64(OCTEON_POW_WORKQUEUE_INT_THRESHOLD(OCTEON_POW_RX_GROUP_NUM), thr.word64);

        oct_write64(OCTEON_POW_WORKQUEUE_INT_PC, intpc.word64);

#else
	thr.bits.ds_thr = thr.bits.iq_thr = 1;  // Only if doing absolute queue-cnt interrupts.
        oct_write64(OCTEON_POW_WORKQUEUE_INT_THRESHOLD(OCTEON_POW_RX_GROUP_NUM), thr.word64);
#endif

        ciu_enable_interrupts(OCTEON_CORE_ID, OCTEON_RGMX_CIU_INTX, OCTEON_RGMX_CIU_ENX,
                              (OCTEON_POW_RX_GROUP_MASK |
                               CIU_GENTIMER_BITS_ENABLE(CIU_GENTIMER_NUM_1)), CIU_MIPS_IP2);

        ciu_clear_int_summary(CIU_THIS_CORE, OCTEON_RGMX_CIU_INTX,
                              OCTEON_RGMX_CIU_ENX, CIU_GENTIMER_BITS_ENABLE(CIU_GENTIMER_NUM_1));

        octeon_ciu_start_gtimer(CIU_GENTIMER_NUM_1, OCTEON_GENTIMER_PERIODIC,
                                OCTEON_GENTIMER_LEN_1SEC);
        /*
         * Enable IPD
         */
        octeon_ipd_enable();
}





static void octeon_rgmx_config_pip (u_int port)
{
    octeon_pip_gbl_cfg_t	pip_config;
    octeon_pip_port_cfg_t	pip_port_config;
    octeon_pip_port_tag_cfg_t	pip_tag_config;

    /*
     * PIP Global config
     */
    pip_config.word64 = 0;
    pip_config.bits.max_l2 = 1;
    oct_write64(OCTEON_PIP_GBL_CFG, pip_config.word64);

    /*
     * PIP Port config
     */
    pip_port_config.word64 = 0;
    pip_port_config.bits.mode	= OCTEON_PIP_PORT_CFG_MODE_SKIPL2;
    pip_port_config.bits.qos	= port & 0x7;
    pip_port_config.bits.crc_en = 1;


    /*
     * PIP -> POW tags config
     *
     * We don't use any pkt input fields for tag hash, except for Port#
     */
    pip_tag_config.word64 = 0;

    pip_tag_config.bits.grptag = 0;
    pip_tag_config.bits.grptagmask = 0xf;
    pip_tag_config.bits.grptagbase = 1;

    pip_tag_config.bits.ip6_src_flag  = 0;
    pip_tag_config.bits.ip6_dst_flag  = 0;
    pip_tag_config.bits.ip6_sprt_flag = 0;
    pip_tag_config.bits.ip6_dprt_flag = 0;
    pip_tag_config.bits.ip6_nxth_flag = 0;

    pip_tag_config.bits.ip4_src_flag  = 1;
    pip_tag_config.bits.ip4_dst_flag  = 1;
    pip_tag_config.bits.ip4_sprt_flag = 1;
    pip_tag_config.bits.ip4_dprt_flag = 1;
    pip_tag_config.bits.ip4_pctl_flag = 1;

    pip_tag_config.bits.tcp6_tag_type = 0;
    pip_tag_config.bits.tcp4_tag_type = 0;
    pip_tag_config.bits.ip6_tag_type  = 0;
    pip_tag_config.bits.ip4_tag_type  = 0;
    pip_tag_config.bits.inc_prt_flag  = 1;
    pip_tag_config.bits.non_tag_type  = OCTEON_POW_TAG_TYPE_NULL;
    pip_tag_config.bits.grp	      = OCTEON_POW_RX_GROUP_NUM;

    octeon_pip_config_port(port, pip_port_config, pip_tag_config);

    oct_write64(OCTEON_POW_CORE_GROUP_MASK(OUR_CORE), OCTEON_POW_ALL_GROUPS_MASK);

}


/*
 * octeon_rgmx_stop_port
 *
 */
static u_int octeon_rgmx_stop_port (u_int port)
{
    int interface = INTERFACE(port);
    int index = INDEX(port);
    octeon_rgmx_prtx_cfg_t gmx_cfg;
    u_int last_enabled = 0;

    gmx_cfg.word64 = oct_read64(OCTEON_RGMX_PRTX_CFG(index, interface));
    last_enabled = (gmx_cfg.bits.en == 1);
    gmx_cfg.bits.en = 0;
    oct_write64(OCTEON_RGMX_PRTX_CFG(index, interface), gmx_cfg.word64);
    return (last_enabled);
}

static void octeon_rgmx_start_port(u_int port)
{
    int interface = INTERFACE(port);
    int index = INDEX(port);
    octeon_rgmx_prtx_cfg_t gmx_cfg;

    gmx_cfg.word64 = oct_read64(OCTEON_RGMX_PRTX_CFG(index, interface));
    gmx_cfg.bits.en = 1;
    oct_write64(OCTEON_RGMX_PRTX_CFG(index, interface), gmx_cfg.word64);
}


static void octeon_rgmx_stop (struct rgmx_softc_dev *sc)
{
    	octeon_rgmx_stop_port(sc->port);

        /* Reset transmitter variables and interface flags.  */
        sc->ifp->if_drv_flags &= ~(IFF_DRV_OACTIVE | IFF_DRV_RUNNING);
        sc->ifp->if_timer = 0;
        sc->txb_count = 0;
        sc->txb_sched = 0;
}


/* Change the media selection.  */
static int octeon_rgmx_medchange (struct ifnet *ifp)
{
    	struct rgmx_softc_dev *sc = ifp->if_softc;

#ifdef DIAGNOSTIC
        /* If_media should not pass any request for a media which this
           interface doesn't support.  */
        int b;

        for (b = 0; bit2media[b] != 0; b++) {
                if (bit2media[b] == sc->media.ifm_media) break;
        }
        if (((1 << b) & sc->mbitmap) == 0) {
                if_printf(sc->ifp,
                    "got an unsupported media request (0x%x)\n",
                    sc->media.ifm_media);
                return EINVAL;
        }
#endif

        /* We don't actually change media when the interface is down.
           fe_init() will do the job, instead.  Should we also wait
           until the transmission buffer being empty?  Changing the
           media when we are sending a frame will cause two garbages
           on wires, one on old media and another on new.  FIXME */
        if (sc->ifp->if_flags & IFF_UP) {
            	printf(" Media change requested while IF is up\n");
        } else  {
            printf(" Media change requested while IF is Down\n");
        }

        return 0;
}


static void octeon_rgmx_medstat (struct ifnet *ifp, struct ifmediareq *ifm)
{
    /*
     * No support for Media Status callback
     */
}

static int octeon_rgmx_ioctl (struct ifnet * ifp, u_long command, caddr_t data)
{
    	struct rgmx_softc_dev *sc = ifp->if_softc;
        struct ifreq *ifr = (struct ifreq *)data;
    	int error = 0;

        if (!sc) {
            printf(" octeon_rgmx_ioctl. No sc\n");
            return (0);
        }
        switch (command) {

          	case SIOCSIFFLAGS:
                    /*
                     * Switch interface state between "running" and
                     * "stopped", reflecting the UP flag.
                     */
                    if (ifp->if_flags & IFF_UP) {


                        /*
                         * New state is IFF_UP
                         * Restart or Start now, if driver is not running currently.
                         */
                        if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
                            printf(" SIOCSTIFFLAGS  UP/Not-running\n"); break;
                            octeon_rgmx_init(sc);
                        } else {
                            printf(" SIOCSTIFFLAGS  UP/Running\n"); break;
                        }
                    } else {
                        /*
                         * New state is IFF_DOWN.
                         * Stop & shut it down now, if driver is running currently.
                         */
                        if ((ifp->if_drv_flags & IFF_DRV_RUNNING) != 0) {
                            printf(" SIOCSTIFFLAGS  Down/Running\n"); break;
                            octeon_rgmx_stop(sc);
                        } else {
                            printf(" SIOCSTIFFLAGS  Down/Not-Running\n"); break;
                        }
                    }
                    break;

        	case SIOCADDMULTI:
        	case SIOCDELMULTI:
                    break;

        	case SIOCSIFMEDIA:
        	case SIOCGIFMEDIA:
                    /* Let if_media to handle these commands and to call
                       us back.  */
                    error = ifmedia_ioctl(ifp, ifr, &sc->media, command);
                    break;

		case SIOCSIFCAP:
                {
                    int mask;

                    ifp->if_hwassist &= ~CSUM_TSO;
                    ifp->if_capenable &= ~IFCAP_VLAN_HWTAGGING;
                    mask = ifr->ifr_reqcap ^ ifp->if_capenable;
                    if (mask & IFCAP_HWCSUM) {
			ifp->if_capenable ^= IFCAP_HWCSUM;
			if (ifp->if_capenable & IFCAP_TXCSUM) {
                            ifp->if_hwassist |= (CSUM_TCP | CSUM_UDP);
                        } else {
                            ifp->if_hwassist &= ~(CSUM_TCP | CSUM_UDP);
                        }
                    }
                }
		break;

        	default:
                    error = ether_ioctl(ifp, command, data);
                    break;
        }

        return (error);
}




/*
 * octeon_rgmx_mark_ready
 *
 * Initialize the rgmx driver for this instance
 * Initialize device.
 */
static void octeon_rgmx_mark_ready (struct rgmx_softc_dev *sc)
{

        /* Enable interrupts.  */
    	/* For RGMX they are already enabled earlier */

        /* Enable transmitter and receiver.  */
    	/* For RGMX they are already enabled earlier */

        /* Flush out all HW receive buffers for this interface. */
    	/* For RGMX, no means to flush an individual port */

        /* Set 'running' flag, because we are now running.   */
        sc->ifp->if_drv_flags |= IFF_DRV_RUNNING;

        /* Set the HW Address filter. aka program Mac-addr & Multicast filters */
    	/* For RGMX this was taken care of via set_mac_addr() */

        /* Kick start the output */
        /* Hopefully PKO is running and will pick up packets via the timer  or receive loop */
}


static void  octeon_rgmx_init (void *xsc)
{

    /*
     * Called mostly from ifnet interface  ifp->if_init();
     * I think we can anchor most of our iniialization here and
     * not do it in different places  from driver_attach().
     */
    /*
     * For now, we only mark the interface ready
     */
    octeon_rgmx_mark_ready((struct rgmx_softc_dev *) xsc);
}



static void octeon_rgmx_config_speed (u_int port, u_int report_link)
{
    	int index = INDEX(port);
        int iface = INTERFACE(port);
        struct rgmx_softc_dev		*sc;
        octeon_rgmx_rxx_rx_inbnd_t	link_status, old_link_status;
        octeon_rgmx_prtx_cfg_t		gmx_cfg;
        uint64_t			val64_tx_clk, val64_tx_slot, val64_tx_burst;
        u_int				last_enabled;


        sc = get_rgmx_softc(port);
        if (!sc) {
            printf(" config_speed didn't find sc int:%u port:%u", iface, port);
            return;
        }

        /*
         * Look up interface-port speed params
         */
        link_status.word64 = oct_read64(OCTEON_RGMX_RXX_RX_INBND(index, iface));

        /*
         * Compre to prev known state. If same then nothing to do.
         */
        if (link_status.word64 == sc->link_status) {
            	return;
        }

        RGMX_LOCK(sc);

        old_link_status.word64 = sc->link_status;

        sc->link_status = link_status.word64;

        last_enabled = octeon_rgmx_stop_port(port);

        gmx_cfg.word64 = oct_read64(OCTEON_RGMX_PRTX_CFG(index, iface));

        /*
         * Duplex
         */
        gmx_cfg.bits.duplex = 1;

        switch (link_status.bits.speed) {
        case 0:	/* 10Mbps */
            gmx_cfg.bits.speed = 0;
            gmx_cfg.bits.slottime = 0;
            val64_tx_clk = 50; val64_tx_slot = 0x40; val64_tx_burst = 0;
            break;
        case 1:	/* 100Mbps */
            gmx_cfg.bits.speed = 0;
            gmx_cfg.bits.slottime = 0;
            val64_tx_clk = 5; val64_tx_slot = 0x40; val64_tx_burst = 0;
            break;

        case 2:	/* 1Gbps */
            gmx_cfg.bits.speed = 1;
            gmx_cfg.bits.slottime = 1;
            val64_tx_clk = 1; val64_tx_slot = 0x200; val64_tx_burst = 0x2000;
            break;

        case 3:	/* ?? */
        default:
            gmx_cfg.bits.speed = 1;
            gmx_cfg.bits.slottime = 1;
            val64_tx_clk = 1; val64_tx_slot = 0x200; val64_tx_burst = 0x2000;
            break;
        }

        oct_write64(OCTEON_RGMX_TXX_CLK(index, iface), val64_tx_clk);
        oct_write64(OCTEON_RGMX_TXX_SLOT(index, iface), val64_tx_slot);
        oct_write64(OCTEON_RGMX_TXX_BURST(index, iface), val64_tx_burst);

        oct_write64(OCTEON_RGMX_PRTX_CFG(index, iface), gmx_cfg.word64);

        if (last_enabled) octeon_rgmx_start_port(port);

        if (link_status.bits.status != old_link_status.bits.status) {

//#define DEBUG_LINESTATUS
            if (link_status.bits.status) {
#ifdef DEBUG_LINESTATUS
                printf(" %u/%u: Interface is now alive\n", iface, port);
#endif
                if (report_link)  if_link_state_change(sc->ifp, LINK_STATE_UP);
            } else {
#ifdef DEBUG_LINESTATUS
                printf(" %u/%u: Interface went down\n", iface, port);
#endif
                if (report_link)  if_link_state_change(sc->ifp, LINK_STATE_DOWN);
            }
        }
        RGMX_UNLOCK(sc);

}



#ifdef DEBUG_RGMX_DUMP
static void octeon_dump_rgmx_stats (u_int port)
{

}
#endif

#ifdef __not_used__
static void rgmx_timer_periodic (void)
{
    u_int port;
    int index;
    struct rgmx_softc_dev *sc;
    struct ifnet *ifp;

        for (index = 0; index < OCTEON_RGMX_NUM_PORTS_MAX; index ++) {

            	port = port_array[index];
                sc = rgmx_scdev_array[index];

                /*
                 * Skip over ports/slots not in service.
                 */
                if ((port < OCTEON_RGMX_MIN_PORT) || (port > OCTEON_RGMX_MAX_PORT)) {
                    continue;
                }
                if ((NULL == sc) || (((struct rgmx_softc_dev *)-1) == sc)) {
                    continue;
                }

                /*
                 * Now look for anamolous conditions
                 */
                if (sc != get_rgmx_softc(port)) {
                    printf(" port %u  sc %p not in sync with index: %u\n",
                           port, sc, index);
                    continue;
                }

                if (sc->port != port) {
                    printf(" port %u  sc %p port-> %u  not in sync with index: %u\n",
                           port, sc, sc->port, index);
                    continue;
                }

                ifp = sc->ifp;
                if (ifp == NULL) {
                    printf(" port %u  sc %p . Bad ifp %p\n", port, sc, ifp);
                    continue;
                }

                /*
                 * Check if packets queued at ifnet layer. Kick start output if we can.
                 */
                if (sc->ifp->if_flags & IFF_UP) {
                    octeon_rgmx_output_start(ifp);
                } else {
                    octeon_rgmx_output_flush(ifp);
                }

                /*
                 * Check if line status changed ?  Adjust ourselves.
                 */
                octeon_rgmx_config_speed(port, 1);
        }
}
#endif

#ifdef DEBUG_RGMX_DUMP
static void octeon_dump_pow_stats(void)
{
    octeon_rgmx_pow_nos_cnt nos_cnt;
    octeon_rgmx_pow_int_pc_t intpc;
    octeon_rgmx_pow_int_threshold_t thr;
    octeon_rgmx_pow_int_cnt_t int_cnt;
    int core = octeon_get_core_num();
    octeon_pow_inpt_q_all_qos inpt_q_all;
    octeon_pow_inpt_q_grp_qos inpt_q_grp;
    octeon_rgmx_pip_inb_pkts pkts;
    octeon_rgmx_pip_inb_errs errs;
    static u_int pkts0 = 0;
    static u_int pkts1 = 0;
    static u_int errs0 = 0;
    static u_int errs1 = 0;
    int i;


    nos_cnt.word64 = oct_read64(OCTEON_RGMX_POW_NOS_CNT);
    if (nos_cnt.bits.nos_cnt) printf(" *** No sched cnt %u\n", nos_cnt.bits.nos_cnt);
    printf(" \nGroup mask: 0x%llX     WorkQueue Int :    0x%llX\n", oct_read64(OCTEON_POW_CORE_GROUP_MASK(OUR_CORE)), oct_read64(OCTEON_POW_WORKQUEUE_INT));
    intpc.word64 = oct_read64(OCTEON_POW_WORKQUEUE_INT_PC);
    printf(" Intr Periodic Cntr: PC %u  thr:  %u\n", intpc.bits.thr_freq, intpc.bits.thr_period);
    thr.word64 = oct_read64(OCTEON_POW_WORKQUEUE_INT_THRESHOLD(OCTEON_POW_RX_GROUP_NUM));
    printf(" Thresholds iq %u  ds %u  time %u  enable %u\n",
           thr.bits.iq_thr, thr.bits.ds_thr, thr.bits.time_thr, thr.bits.enable);
    int_cnt.word64 = oct_read64(OCTEON_POW_INT_CNTR(core));
    printf(" Int_cnt  iq_cnt %u  ds_cnt %u  tc_cnt %u\n",
           int_cnt.bits.iq_cnt, int_cnt.bits.ds_cnt, int_cnt.bits.tc_cnt);
    pkts.word64 = oct_read64(OCTEON_PIP_STAT_INB_PKTS(16)); pkts0 += pkts.bits.inb_pkts;
    errs.word64 = oct_read64(OCTEON_PIP_STAT_INB_ERRS(16)); errs0 += errs.bits.inb_errs;
    pkts.word64 = oct_read64(OCTEON_PIP_STAT_INB_PKTS(17)); pkts1 += pkts.bits.inb_pkts;
    errs.word64 = oct_read64(OCTEON_PIP_STAT_INB_ERRS(17)); errs1 += errs.bits.inb_errs;
    printf(" PIP inbound pkts(16): %u   Errors: %u    inbound(17): %u   Errors: %u\n", pkts0, errs0, pkts1, errs1);
    inpt_q_all.word64 = oct_read64(OCTEON_POW_INPT_Q_ALL_QOS);
    printf(" All queued pkt in qos Levels: %u -- ", inpt_q_all.bits.iq_cnt);
    for (i = 0 ; i < 7; i++) {
        inpt_q_grp.word64 = oct_read64(OCTEON_POW_INPT_QOS_GRP(i));
        if (inpt_q_grp.bits.iq_cnt)  printf(" Grp-%u:  %u ", i, inpt_q_grp.bits.iq_cnt);
    }
}
#endif

/* ------------------------------------------------------------------- *
 *                      octeon_line_status_loop()                      *
 * ------------------------------------------------------------------- */
static void octeon_line_status_loop (void *unused)
{
    	struct rgmx_softc_dev *sc;
        u_int idx;

                for (idx = 0; idx < num_devices; idx++) {
                    	sc = rgmx_scdev_array[idx];
                        if (sc && sc->ifp) {
                                if ((sc->ifp->if_drv_flags & IFF_DRV_RUNNING)) {
                                    	octeon_rgmx_config_speed(sc->port, 1);

                                        octeon_rgmx_output_start(sc->ifp);
                                }
                        }
                }

//#define DEBUG_RGMX_DUMP
#ifdef DEBUG_RGMX_DUMP
        static int count = 0;

                if (++count > 5) {
                    count = 0;
//                    octeon_dump_fpa_pool(OCTEON_FPA_RX_PACKET_POOL);
//                    octeon_dump_fpa_pool(OCTEON_FPA_WQE_RX_POOL);
//                    octeon_dump_fpa_pool(OCTEON_FPA_TX_PACKET_POOL);
                    octeon_dump_rgmx_stats(16);
                    octeon_dump_pow_stats();
                }
#endif
}


/* ------------------------------------------------------------------- *
 *                      octeon_rgmx_set_mac	                       *
 * ------------------------------------------------------------------- *
 *
 * octeon_rgmx_set_mac
 *
 * Program the ethernet HW address
 *
 */
static void octeon_rgmx_set_mac (u_int port)
{
    struct rgmx_softc_dev *sc;
    u_int iface = INTERFACE(port);
    u_int index = INDEX(port);
    int ii;
    uint64_t mac = 0;
    u_int last_enabled;

    sc = get_rgmx_softc(port);
    if (!sc) {
        printf(" octeon_rgmx_set_mac Missing sc.  port:%u", port);
        return;
    }

    for (ii = 0; ii < 6; ii++) {
        mac = (mac << 8) | (uint64_t)(sc->ieee[ii]);
    }

    last_enabled = octeon_rgmx_stop_port(port);

    oct_write64(OCTEON_RGMX_SMACX(index, iface), mac);
    oct_write64(OCTEON_RGMX_RXX_ADR_CAM0(index, iface), sc->ieee[0]);
    oct_write64(OCTEON_RGMX_RXX_ADR_CAM1(index, iface), sc->ieee[1]);
    oct_write64(OCTEON_RGMX_RXX_ADR_CAM2(index, iface), sc->ieee[2]);
    oct_write64(OCTEON_RGMX_RXX_ADR_CAM3(index, iface), sc->ieee[3]);
    oct_write64(OCTEON_RGMX_RXX_ADR_CAM4(index, iface), sc->ieee[4]);
    oct_write64(OCTEON_RGMX_RXX_ADR_CAM5(index, iface), sc->ieee[5]);
    oct_write64(OCTEON_RGMX_RXX_ADR_CTL(index, iface),
                OCTEON_RGMX_ADRCTL_ACCEPT_BROADCAST | 
                OCTEON_RGMX_ADRCTL_ACCEPT_ALL_MULTICAST |
                OCTEON_RGMX_ADRCTL_CAM_MODE_ACCEPT_DMAC);
    oct_write64(OCTEON_RGMX_RXX_ADR_CAM_EN(index, iface), 1);
    if (last_enabled)  octeon_rgmx_start_port(port);
}


/* ------------------------------------------------------------------- *
 *                      octeon_config_rgmii_port()                     *
 * ------------------------------------------------------------------- */
static void octeon_config_rgmii_port (u_int port)
{
    	u_int iface = INTERFACE(port);
        u_int index = INDEX(port);

	/* 
	 * Configure an RGMII port
	 */
        octeon_rgmx_prtx_cfg_t gmx_cfg;

	/* Enable ASX */
	oct_write64(OCTEON_ASXX_RX_PRT_EN(iface), oct_read64(OCTEON_ASXX_RX_PRT_EN(iface)) | (1<<index));
        oct_write64(OCTEON_ASXX_TX_PRT_EN(iface), oct_read64(OCTEON_ASXX_TX_PRT_EN(iface)) | (1<<index));

        /* Enable RGMX */
        gmx_cfg.word64 = oct_read64(OCTEON_RGMX_PRTX_CFG(index, iface));
        gmx_cfg.bits.en = 1;
        oct_write64(OCTEON_RGMX_PRTX_CFG(index, iface), gmx_cfg.word64);


        octeon_rgmx_config_speed(port, 0);

        oct_write64(OCTEON_RGMX_TXX_THRESH(index, iface), 32);

        /*
         * Set hi water mark
         */
        oct_write64(OCTEON_ASXX_TX_HI_WATERX(index, iface), 10);
	if (octeon_get_chipid() == OCTEON_CN5020_CHIP) {
        	oct_write64(OCTEON_ASXX_TX_CLK_SETX(index, iface), 16);
        	oct_write64(OCTEON_ASXX_RX_CLK_SETX(index, iface), 16);
	} else {
        	oct_write64(OCTEON_ASXX_TX_CLK_SETX(index, iface), 24);
        	oct_write64(OCTEON_ASXX_RX_CLK_SETX(index, iface), 24);
	}
}



static void octeon_rgmx_enable_RED_queue (int queue, int slow_drop, int all_drop)
{
    octeon_rgmx_ipd_queue_red_marks_t red_marks;
    octeon_rgmx_ipd_red_q_param_t red_param;

    if (slow_drop == all_drop) { printf("Bad val in %s", __FUNCTION__); return; }
    red_marks.word64 = 0;
    red_marks.bits.all_drop = all_drop;
    red_marks.bits.slow_drop = slow_drop;
    oct_write64(OCTEON_IPD_QOSX_RED_MARKS(queue), red_marks.word64);

    /* Use the actual queue 0 counter, not the average */
    red_param.word64 = 0;
    red_param.bits.prb_con = (255ul << 24) / (slow_drop - all_drop);
    red_param.bits.avg_con = 1;
    red_param.bits.new_con = 255;
    red_param.bits.use_pagecount = 1;
    oct_write64(OCTEON_IPD_RED_Q_PARAM(queue), red_param.word64);
}


static void octeon_rgmx_enable_RED_all (int slow_drop, int all_drop)
{

    int port, queue;
    octeon_ipd_port_bp_page_count_t ipd_bp_page_count;
    octeon_ipd_red_port_enable_t red_port_enable;

    /*
     * First remove BP settings
     */
    ipd_bp_page_count.word64 = 0;
    ipd_bp_page_count.bits.bp_enable = 0;
    ipd_bp_page_count.bits.page_count = 100;

    for (port = 0; port < OCTEON_RGMX_MAX_PORT; port++) {
        oct_write64(OCTEON_IPD_PORT_BP_PAGE_COUNT(port), ipd_bp_page_count.word64);
    }

    /*
     * Enable RED for each individual queue
     */
    for (queue = 0; queue < 8; queue++) {
        octeon_rgmx_enable_RED_queue(queue, slow_drop, all_drop);
    }

    oct_write64(OCTEON_IPD_BP_PORT_RED_END, 0);

    red_port_enable.word64 = 0;
    red_port_enable.bits.port_enable = 0xfffffffffull;
    red_port_enable.bits.avg_dly = 10000;
    red_port_enable.bits.prb_dly = 10000;
    oct_write64(OCTEON_IPD_RED_PORT_ENABLE, red_port_enable.word64);
}



/* ------------------------------------------------------------------- *
 *                    octeon_has_4ports()                                 *
 * ------------------------------------------------------------------- */
static int octeon_has_4ports (void)
{
    u_int chipid;
    int retcode = 1;

    chipid = octeon_get_chipid() & 0xffffff00;

    switch (chipid) {
        case OCTEON_CN31XX_CHIP:
        case OCTEON_CN30XX_CHIP:
	case OCTEON_CN5020_CHIP:
            retcode = 0;
            break;

        default:
            break;
    }
    return (retcode);
}


#ifdef __not_used__
/*
 * octeon_rgmx_free_intr
 *
 * We have 4 child and one parent device.
 * It's tricky and unexpected that anyone will detach the device that is built'in on
 * the chip.
 * We will not support  detachment for now. But keep adding good code that will be used
 * someday.
 */
static void octeon_rgmx_free_intr (struct rgmx_softc_dev *sc)
{
	device_t dev = sc->sc_dev;

        /*
         * Make sure that sc/dev  are the parent Root structs. Not one
         * of the rgmxN childs.
         */
	if (int_handler_tag != NULL) {
		bus_teardown_intr(dev, irq_res, int_handler_tag);
		int_handler_tag = NULL;
	}

#ifdef OCTEON_RGMX_SCHEDULED_ISRS
	if (tq != NULL) {
		taskqueue_drain(tq, &rxtx_isr_task);
		taskqueue_drain(taskqueue_fast, &link_isr_task);
		taskqueue_free(tq);
		tq = NULL;
	}
#endif

}
#endif

static device_method_t rgmii_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		rgmii_probe),
	DEVMETHOD(device_identify,	rgmii_identify),
	DEVMETHOD(device_attach,	rgmii_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

	{ 0, 0 }
};

static driver_t rgmii_driver = {
    	"rgmii", rgmii_methods, sizeof(struct rgmx_softc_dev)
};

static devclass_t rgmii_devclass;

DRIVER_MODULE(rgmii, nexus, rgmii_driver, rgmii_devclass, 0, 0);
