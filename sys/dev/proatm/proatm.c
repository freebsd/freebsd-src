/*
 * Copyright (c) 2002, 2003 Christian Bucari, Prosum 
 * Copyright (c) 2002, 2003 6wind
 * Copyright (c) 2000, 2001 Richard Hodges and Matriplex, inc.
 * Copyright (c) 1996, 1997, 1998, 1999 Mark Tinguely
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *	must display the following acknowledgement:
 *	This product includes software developed by Prosum, 6wind and 
 *  Matriplex, inc
 * 4. The name of the authors may not be used to endorse or promote products 
 *	derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************
 *
 *  This driver supports the PROATM-155 and PROATM-25 cards based on the IDT77252.
 *
 *  UBR, CBR and VBR connections are supported  
 *
 *  You must have FreeBSD 3.5, 4.1 or later.
 *
 * in sys/i386/conf/YOUR_NAME add:
 *	options		ATM_CORE	#core ATM protocol family
 *	options		ATM_IP		#IP over ATM support
 *  at least one (and usually one of the following:
 *	options		ATM_SIGPVC	#SIGPVC signalling manager
 *	options		ATM_SPANS	#SPANS signalling manager
 *	options		ATM_UNI		#UNI signalling manager
 *  and the device driver:
 *	device		proatm      #PROATM device driver (this file)
 *
 * Add the following line to /usr/src/sys/conf/files:
 *   pci/proatm.c  optional proatm pci
 *
 ******************************************************************************
 *
 *  The following sysctl variables are used:
 *
 * hw.proatm.log_bufstat  (0)   Log free buffers (every few minutes)
 * hw.proatm.log_vcs      (0)   Log VC opens, closes, and other events
 * hw.proatm.bufs_large  (500)  Max/target number of free 2k buffers
 * hw.proatm.bufs_small  (500)  Max/target number of free mbufs
 * hw.proatm.cur_large   (R/O)  Current number of free 2k buffers
 * hw.proatm.cur_small   (R/O)  Current number of free mbufs
 * hw.proatm.qptr_hold    (1)   Optimize TX queue buffer for lowest overhead
 *
 * Note that the read-only buffer counts will not work with multiple cards.
 *
 ******************************************************************************
 *
 *  Assumption: All mbuf clusters are 2048 bytes.
 *
 ******************************************************************************
 *
 *  Date: 25-06-2003
 *  Version: 1.06
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/* replace with #define XDEBUG 1 or #define XDEBUG 2 for extra debug printing */
#define XDEBUG 0

#define PROATM_VERSION      "PROATM 1.0060"
/* Don't touch these **********************************************************/
#if XDEBUG == 1
#define XPRINT(arg...) printf(arg)
#elif XDEBUG == 2
#define XPRINT(arg...) bprintf(arg);
#define PROATM_LOG_SIZE 0x4000
static char proatm_log[PROATM_LOG_SIZE + 256];
static char* proatm_logptr= proatm_log;
#else
#define XPRINT(arg...)
#endif
/******************************************************************************/

#include <dev/proatm/proatm.h>
#include <dev/proatm/proatm_rtbl.c>

#if __FreeBSD_version == 350000 || __FreeBSD_version == 350001 
#define FBSD35
#elif __FreeBSD_version >= 410000
#define FBSD41
#else
#error "This driver is for FreeBSD 3.5, 3.51 or 4.1 and later"
#endif

static u_int32_t   proatm_found = 0;

static int32_t  proatm_phys_init __P((proatm_reg_t *const));
static int32_t  proatm_init __P((proatm_reg_t *const));
static int32_t  proatm_sram_wr __P((proatm_reg_t *const, u_int32_t,
                          int32_t, u_int32_t, u_int32_t, u_int32_t, u_int32_t));
static int32_t  proatm_sram_rd __P((proatm_reg_t *const, u_int32_t, u_int32_t *));
static int32_t  proatm_add_buf __P((proatm_reg_t *const, struct mbuf *,
                                     struct mbuf *, u_int32_t));
static int32_t  proatm_util_rd __P((proatm_reg_t *const, u_int32_t, u_int32_t *));
static int32_t  proatm_util_wr __P((proatm_reg_t *const, u_int32_t, u_int32_t));
void            proatm_ld_rcv_buf __P((proatm_reg_t *const, int32_t, int32_t));
static void     proatm_itrx __P((proatm_reg_t *));
static void     proatm_rawc __P((proatm_reg_t *));
static void     proatm_recv __P((proatm_reg_t *));
static void     proatm_phys __P((proatm_reg_t *));
static void     proatm_intr __P((void *));
static int32_t  proatm_atm_bearerclass(struct attr_bearer *);
CONNECTION      *proatm_atm_harpconn(Cmn_unit *, Cmn_vcc *);
int32_t         proatm_atm_ioctl __P((int32_t, caddr_t, caddr_t));
static void     proatm_harp_init __P((proatm_reg_t *const, void*));
void            proatm_output __P((Cmn_unit *, Cmn_vcc *, KBuffer *));
int32_t         proatm_openvcc __P((Cmn_unit *, Cmn_vcc *));
int32_t         proatm_closevcc __P((Cmn_unit *, Cmn_vcc *));
int32_t         proatm_instvcc __P((Cmn_unit *, Cmn_vcc *));
static void     proatm_recv_stack __P((void *, KBuffer *));
static struct   mbuf *proatm_mbufcl_get(void);
static CONNECTION*      proatm_connect_find(PROATM *, int32_t, int32_t);
static int32_t  proatm_connect_init(PROATM *);
static void     proatm_dtst_init(PROATM *proatm);
static int32_t  proatm_tst_init(PROATM *proatm);
static void     proatm_rate_init(PROATM *proatm);
static int32_t  proatm_connect_rxopen (PROATM *, CONNECTION *);
static int32_t  proatm_connect_rxclose (PROATM *, CONNECTION *);
static int32_t  proatm_connect_txclose(PROATM *, CONNECTION *, int32_t);
static int32_t  proatm_connect_txopen(PROATM *, CONNECTION *);
static void     proatm_device_stop(PROATM *);
static void     proatm_intr_tsq(PROATM *);
static void *   proatm_malloc_contig(int32_t);
static caddr_t  proatm_mbuf_base(struct mbuf *);
static int32_t  proatm_mcheck_add(PROATM *, struct mbuf *);
static int32_t  proatm_mcheck_rem(PROATM *, struct mbuf *);
static int32_t  proatm_mcheck_init(PROATM *);
static int32_t  proatm_queue_flush(CONNECTION *, int32_t *);
static struct   mbuf *proatm_queue_get(TX_QUEUE *);
static int32_t  proatm_queue_init(PROATM *);
static int32_t  proatm_queue_put(CONNECTION *, struct mbuf *);
static void     proatm_receive(PROATM *, struct mbuf *, int32_t, int32_t);
static int32_t  proatm_receive_aal5(PROATM *, struct mbuf *, struct mbuf *);
static void     proatm_release_mem(PROATM *);
static void     proatm_transmit(PROATM *, struct mbuf *, int32_t, int32_t);
static void     proatm_transmit_drop(PROATM *, CONNECTION *, struct mbuf *);
static void     proatm_transmit_top(PROATM *, TX_QUEUE *);
static int32_t  proatm_slots_add(PROATM *, TX_QUEUE *, int32_t, int32_t);
static int32_t  proatm_slots_cbr(PROATM *, int32_t);
static int32_t  proatm_slots_rem(PROATM *, TX_QUEUE *);
static int32_t  proatm_phys_detect(PROATM *);
static void     proatm_status_bufs(PROATM *);
static int32_t  proatm_status_wait(PROATM *);
static u_int32_t   proatm_reg_rd ( proatm_reg_t * const, vm_offset_t);
static void     proatm_reg_wr ( proatm_reg_t * const , u_int32_t , vm_offset_t);
static int32_t  proatm_tct_wr (PROATM *, int32_t);
u_int32_t       nicsr (proatm_reg_t *const , u_int32_t);
static int32_t  proatm_connect_txstop (PROATM *, int32_t);
static void     proatm_process_tsr (PROATM *, u_int32_t * );
static void     proatm_connect_txclose_cb(PROATM *, CONNECTION *);
static void     proatm_process_rcqe (proatm_reg_t *, rcqe *);
static int32_t  proatm_check_vc (PROATM *, u_int8_t, ushort);
static void     proatm_get_stats (proatm_reg_t *proatm);

/* Our own proatm malloc type */
MALLOC_DEFINE(M_PROATM, "proatm", "Prosum's ATM buffers");

#if XDEBUG == 2
void print_proatm_log (void);
void bprintf (char *format,...);
#endif

#ifdef FBSD35

/*****************************************************************************
 * 
 * FreeBSD 3.5 specific section 
 *
 *****************************************************************************/


 /*  FreeBSD glue */
static u_int32_t proatm_count;
static const char *proatm_probe		__P((pcici_t, pcidi_t));
static void proatm_attach		    __P((pcici_t, int32_t));
static void proatm_shutdown	__P((int32_t, void *));

static struct pci_device proatm_device = {
	"proatm",
	proatm_probe,
	proatm_attach,
	&proatm_count,
	NULL
};
DATA_SET(pcidevice_set, proatm_device);


/******************************************************************************
 *
 *  PROATM Nicstar probe   (3.5)
 *
 *  Return identification string if this is device is IDT77252.
 */

static const char *
proatm_probe(pcici_t config_id, pcidi_t device_id)
{
	if (((device_id & 0xffff) == 0x111d) &&
	    ((device_id >> 16) & 0xffff) == 3)  /* IDT77252 device ID */
		return ("PROSUM PROATM adapter");

   	return NULL;
}


/******************************************************************************
 *
 *  Attach device    (3.5)
 *
 */

static void proatm_attach(pcici_t config_id, int32_t unit)
{
    PROATM          *proatm;
    u_int32_t       val;
	vm_offset_t		pbase, vbase;
    int32_t             latency;
    int32_t             retval;
    int32_t             s;

    s = splimp();
    proatm = malloc(sizeof(PROATM), M_PROATM, M_NOWAIT);
	if (proatm == NULL) {
		printf("proatm%d: no memory for PROATM struct!\n", unit);
		splx(s);
        return;
	}    
    
    bzero(proatm, sizeof (PROATM));

    val = pci_conf_read(config_id, PCIR_COMMAND);      /* enable bus mastering */
    val |= PCIM_CMD_MEMEN | PCIM_CMD_BUSMASTEREN;
    pci_conf_write(config_id, PCIR_COMMAND, val);

    val = pci_conf_read(config_id, PCIR_COMMAND);
    if (!(val & PCIM_CMD_MEMEN)) {
		printf("proatm%d: failed to enable memory mapping!\n", unit);
		goto fail;
	}

	/*  Map IDT registers */
    if (!pci_map_mem(config_id, 0x14, &vbase, &pbase)) {
		printf ("proatm%d: couldn't map registers\n", unit);
		goto fail;
	}

    proatm->bustag = I386_BUS_SPACE_MEM;
    proatm->bushandle = vbase;
    proatm->virt_baseaddr = (vm_offset_t)vbase;

    /* Map interrupt */
    if (!pci_map_int(config_id, proatm_intr, proatm, &net_imask)) {
		printf("proatm%d: couldn't map interrupt\n", unit);
		goto fail;
	}

    val = pci_conf_read(config_id, PCIR_REVID);    /* device class code register */
    proatm->pci_rev = val & 255;
    latency = pci_conf_read(config_id, PCIR_CACHELNSZ);
    latency &= 0xffff00ff;
    latency |= 0x2000;                         /* count = 2 (times 32 PCI clocks) */
    pci_conf_write(config_id, PCIR_CACHELNSZ, latency);
    proatm->timer_wrap = 0;
    proatm->unit = unit;
    nicstar[proatm->unit] = proatm;
    retval = proatm_phys_init(proatm);        /* initialize the hardware */
    if (retval)
        goto fail;

    retval = proatm_init(proatm);             /* allocate and initialize */
    if (retval)
        goto fail;

    proatm_harp_init(proatm, NULL);

    at_shutdown(proatm_shutdown, proatm, SHUTDOWN_POST_SYNC);
    splx(s);
    return;

fail:
    free(proatm, M_DEVBUF);
	splx(s);
	return;
}


/******************************************************************************
 *
 *  Shutdown device    (3.5)
 *
 */

static void proatm_shutdown(int32_t howto, void *arg)
{
    PROATM  *proatm = (PROATM*) arg;

    proatm_device_stop(proatm);               /*  Stop the device */
    return;
}


#else
/*****************************************************************************
 * 
 * FreeBSD 4.1 and above specific section 
 *
 *****************************************************************************/


 /*  FreeBSD glue */

static int32_t      proatm_probe(device_t);             
static int32_t      proatm_attach(device_t);    
static int32_t      proatm_detach(device_t);    
static int32_t      proatm_shutdown(device_t);
static  device_method_t proatm_methods[] = 
{
    DEVMETHOD(device_probe, proatm_probe), 
    DEVMETHOD(device_attach, proatm_attach), 
    DEVMETHOD(device_detach, proatm_detach), 
    DEVMETHOD(device_shutdown, proatm_shutdown), 
    { 0, 0}
}
;
static driver_t proatm_driver = {"proatm", 
    proatm_methods, 
    sizeof (PROATM)
}
;
static          devclass_t proatm_devclass;

DRIVER_MODULE(proatm, pci, proatm_driver, proatm_devclass, 0, 0); 

/******************************************************************************
 *
 *  PROATM Nicstar probe (4.xx)
 *
 */
static int32_t 
proatm_probe(device_t dev)
{
    if (pci_get_vendor(dev) != 0x111d)
        return (ENXIO);

    if (pci_get_device(dev) != 3)
                                                      /* IDT77252 device ID */
        return (ENXIO);

    device_set_desc(dev, "PROSUM PROATM adapter");
    return (0);
}

/******************************************************************************
 *
 *  Attach device (4.xx)
 *
 */

static int32_t 
proatm_attach(device_t dev)
{       
    PROATM          *proatm;
    int32_t         val;
    int32_t             rid;                              /* resource ID */
    int32_t             latency;
    int32_t             retval;
    int32_t             s;

    proatm = device_get_softc(dev);
    bzero(proatm, sizeof (PROATM));
    callout_handle_init(&proatm->stat_ch);
    s = splimp();
    val = pci_read_config(dev, PCIR_COMMAND, 2);      /* enable bus mastering */
    val |= PCIM_CMD_MEMEN | PCIM_CMD_BUSMASTEREN;
    pci_write_config(dev, PCIR_COMMAND, val, 2);
                                                      /*  Map IDT registers */
    rid = 0x14;
    proatm->mem = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid, 
                                  0, ~0, 1, RF_ACTIVE);
    if (proatm->mem == NULL) {
        device_printf(dev, "could not map registers.\n");
        splx(s);
        return (ENXIO);
    }
    proatm->bustag = rman_get_bustag(proatm->mem);
    proatm->bushandle = rman_get_bushandle(proatm->mem);
                                                      /* Map interrupt */
    rid = 0;
    proatm->irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 
                                  0, ~0, 1, RF_ACTIVE | RF_SHAREABLE);
    if (proatm->irq == NULL) {
        device_printf(dev, "could not map interrupt.\n");
        splx(s);
        return (ENXIO);
    }
    retval = bus_setup_intr(dev, proatm->irq, INTR_TYPE_NET, proatm_intr, 
                            proatm, &proatm->irqcookie);
    if (retval) {
        device_printf(dev, "could not setup irq.\n");
        splx(s);
        return (retval);
    }
    proatm->virt_baseaddr = (vm_offset_t) rman_get_virtual(proatm->mem);
    val = pci_read_config(dev, 8, 4);                 /* device class code register */
    proatm->pci_rev = val & 255;
    latency = pci_read_config(dev, 12, 4);
    latency &= 0xffff00ff;
    latency |= 0x2000;                                /* count = 2 (times 32 PCI clocks) */
    pci_write_config(dev, 12, latency, 4);
    proatm->timer_wrap = 0;
    proatm->unit = device_get_unit(dev);
    nicstar[proatm->unit] = proatm;
    retval = proatm_phys_init(proatm);                /* initialize the hardware */
    if (retval) {
        splx(s);
        return (retval);
    }  
    retval = proatm_init(proatm);             /* allocate and initialize */
    if (retval) {
        splx(s);
        return (retval);
    }        
    proatm_harp_init(proatm, dev);
    return (0);
}

/******************************************************************************
 *
 *  Detach device (4.xx)
 *
 */

static int32_t 
proatm_detach(device_t dev)
{       
    PROATM          *proatm;
    int32_t             s, i;
    CONNECTION      *connection;
    struct mbuf     *m;

    proatm = device_get_softc(dev);
    s = splimp();
    proatm_device_stop(proatm);                     /*  Stop the device */

    for (i=0; i< proatm->max_connection; i++) {
        connection = &proatm->connection[i]; 
        if (connection->flg_open) {
            proatm_connect_rxclose(proatm, connection);   
            if (connection->recv != NULL)
                m_freem(connection->recv);
            proatm_connect_txclose(proatm, connection, 1);
        }
    }
    for (i=0; i<1024; i++) {
        m = proatm->mcheck[i];
        while (m)
            m = m_free (m);
    }

    bus_teardown_intr(dev, proatm->irq, proatm->irqcookie);
    bus_release_resource(dev, SYS_RES_IRQ, 0, proatm->irq);
    bus_release_resource(dev, SYS_RES_MEMORY, 0x14, proatm->mem);

    proatm_release_mem(proatm);
    splx(s);
    return (0);
}

/******************************************************************************
 *
 *  Shutdown device (4.xx)
 *
 */

static int32_t 
proatm_shutdown(device_t dev)
{       
    PROATM             *proatm;

    proatm = device_get_softc(dev);
    proatm_device_stop(proatm);                             /*  Stop the device */
    return (0);
}


#endif  
/*****************************************************************************
 * 
 * common section
 *
 *****************************************************************************/


/******************************************************************************
 *
 *  Stop device (shutdown)
 *
 *  in:  PROATM device
 *
 */

static void 
proatm_device_stop(PROATM *proatm)
{       
    int32_t             s;

    s = splimp();
    proatm_reg_wr (proatm, IDT_CFG_SWRST, REGCFG);      /* put chip into reset */
    DELAY(50);
    proatm_reg_wr (proatm, 0, REGCFG);                  /* out of reset */
    splx(s);
}

/******************************************************************************
 *
 *  Initialize hardware
 */
static int32_t 
proatm_phys_init(proatm_reg_t *const proatm)
{       
    int32_t            i;
    u_int32_t          t;


    u_int32_t x, s1, s2, z;
    volatile  u_int32_t stat_val;

    /* clean status bits */
    stat_val = proatm_reg_rd (proatm, REGSTAT);
    proatm_reg_wr (proatm, stat_val | CLEAR_FLAGS, REGSTAT);  /* clear ints */

    proatm->flg_25 = 0;                                  /* is this PROATM-25 with 77105 PHY? */
    proatm->flg_igcrc = 0;                               /* ignore receive CRC errors? */
    strcpy (proatm->hardware, "?");

    /* start signalling SAR reset*/
    proatm_reg_wr (proatm, IDT_CFG_SWRST, REGCFG);

    /* SAR reset--clear occurs at lease 2 PCI cycles after setting */
    DELAY(50);                                          /* wait */
    proatm_reg_wr (proatm, 0, REGCFG);                  /* clear reset */
    proatm_reg_wr (proatm, 0, REGGP);                   /* clear PHYS reset */
    proatm_reg_wr (proatm, 8, REGGP);                   /* set PHYS reset */
    DELAY(50);                                          /* wait */
    proatm_reg_wr (proatm, 3, REGGP);                   /* clear PHYS reset and set EECS and EED0*/
    DELAY(50);                                          /* wait */

    proatm->flg_25 = proatm_phys_detect(proatm);
    if (proatm->flg_25) {
        proatm->max_pcr = ATM_PCR_25;
        proatm->txslots_cur = 0;

        /* initialize the 25.6 Mbps IDT77105 */
        proatm_util_wr(proatm, IDT77105_MCR_REG, 0x09);  /* enable interrupts
		                                                  * and Discard Received Idle Cells */
        proatm_util_wr(proatm, IDT77105_DIAG_REG, 0x10); /* 77105 RFLUSH, clear receive FIFO */
        proatm_util_rd(proatm, IDT77105_ISTAT_REG, &t);  /* read/clear interrupt flag */
    } else {
        proatm->max_pcr = ATM_PCR_OC3C;
        proatm->txslots_cur = 0;

        /* initialize the 155Mb SUNI */
        proatm_util_wr(proatm, SUNI_MASTER_REG, 0x00);   /* clear SW reset */

#ifdef PROATM_LOOPT
        proatm_util_rd(proatm, SUNI_MSTR_CTRL_REG, &t);
        proatm_util_wr(proatm, SUNI_MSTR_CTRL_REG, t | 1 );
#endif  

#ifdef PROATM_FULL_SDH
        proatm_util_rd(proatm, SUNI_TPOP_MSB_REG, &t);
        proatm_util_rd(proatm, SUNI_TPOP_MSB_REG, t | 0x8);
#endif

    }
    /* this will work with 32K and 128K word RAM  because
             * the pattern repeats every 4 words */
    proatm->sram_size = 0x20000;           /* to allow SRAM write */
    for (i = 0; i < 0x20000; i += 4)
             (void) proatm_sram_wr(proatm, i, 4, 0xa5a5a5a5, 0x5a5a5a5a,
                                    0xa5a5a5a5, 0x5a5a5a5a);

    for (i = 0; i < 0x20000; i += 2) {
        s1 = proatm_sram_rd(proatm, i, &x);
        s2 = proatm_sram_rd(proatm, i + 1, &z);
        if (s1 || s2 || x != 0xa5a5a5a5 || z != 0x5a5a5a5a) {
            printf("proatm%d: sram fail1 %d 0x%08x 0x%08x\n", 
                        proatm->unit, i, (u_int32_t)x, (u_int32_t)z);
            return (ENXIO);
        }
    }
    for (i = 0; i < 0x20000; i += 4)
             (void) proatm_sram_wr(proatm, i, 4, 0x5a5a5a5a, 0xa5a5a5a5, 
                                    0x5a5a5a5a, 0xa5a5a5a5);

    for (i = 0; i < 0x20000; i += 2) {
        s1 = proatm_sram_rd(proatm, i, &z);
        s2 = proatm_sram_rd(proatm, i + 1, &x);
        if (s1 || s2 || x != 0xa5a5a5a5 || z != 0x5a5a5a5a) {
            printf("proatm%d: sram fail2 %d 0x%08x 0x%08x\n", 
                        proatm->unit, i, (u_int32_t)x, (u_int32_t)z);
            return (ENXIO);
        }
    } 

    /* flush SRAM */
    for (i = 0; i < 0x20000; i += 4)
             (void) proatm_sram_wr(proatm, i, 4, 0, 0, 0, 0);

    /* write in the 0 word, see if we read it at 0x10000 */
    (void) proatm_sram_wr(proatm, 0x0, 1, 0xa5a5a5a5, 0, 0, 0);

    s1 = proatm_sram_rd(proatm, 0x10000, &x);
    (void) proatm_sram_wr(proatm, 0x0, 1, 0, 0, 0, 0);

    if (!s1 && x == 0xa5a5a5a5) {
        printf("proatm%d: 32K words of RAM ", proatm->unit);
        proatm->sram_size = 0x8000;
    } else {
        printf("proatm%d: 128K words of RAM ", proatm->unit);
        proatm->sram_size = 0x20000;
    }
	/* else TODO: 512K support */

#ifdef NICSTAR_FORCE32K
    proatm->sram_size = 0x8000;
    printf("forced to 32K ", proatm->unit);
#endif
    if (proatm->sram_size == 0x8000) {
        proatm->max_connection = 1024;
        proatm->tst_num = 2047;
	} else {
        proatm->max_connection = 4096;
        proatm->tst_num = 4095;
	}
	/* else TODO: 512K support */

    printf("up to %d PVC\n", proatm->max_connection);

    /* dynamic SRAM map */
    proatm->rct = IDT_TCT_ENTRY_SIZE * proatm->max_connection;     
    proatm->rate = proatm->rct + IDT_RCT_ENTRY_SIZE * proatm->max_connection + 2*PROATM_RFBQ_SIZE;    
    proatm->tst =  proatm->rate + abr_vbr_rate_tables_len;    
    proatm->dtst = proatm->tst + proatm->tst_num + 1;
    proatm->scd = proatm->dtst + PROATM_DTST_SIZE ;
    proatm->rxfifo = proatm->sram_size - PROATM_RXFIFO_SIZE;    
    proatm->scd_size = ((proatm->rxfifo - proatm->scd)/IDT_STRUCT_SCD_SIZE) - 1;
    if (proatm->scd_size >= (PROATM_MAX_QUEUE - 1))
        proatm->scd_size = PROATM_MAX_QUEUE - 1;     
    proatm->scd_ubr0 = proatm->scd + proatm->scd_size * IDT_STRUCT_SCD_SIZE;

    XPRINT ("proatm: SRAM Map for %d connections:\n", proatm->max_connection);
    XPRINT ("TCT =      0x0\n");
    XPRINT ("RCT =      0x%x\n", proatm->rct);
    XPRINT ("RATE_TBL = 0x%x\n", proatm->rate);
    XPRINT ("TST =      0x%x\n", proatm->tst);
    XPRINT ("DTST =     0x%x\n", proatm->dtst);
    XPRINT ("SCD =      0x%x\n", proatm->scd);
    XPRINT ("SCD_UBR0 = 0x%x\n", proatm->scd_ubr0);
    XPRINT ("RXFIFO =   0x%x\n", proatm->rxfifo);

    XPRINT ("proatm_phys_init: SCD number = %d\n", proatm->scd_size); 

    return 0; 
}

/******************************************************************************
 *
 *  Physical layer detect
 *
 *  in:  PROATM device
 * out:  zero = PROATM-155, NZ = PROATM-25
 *
 */

static int32_t 
proatm_phys_detect(PROATM *proatm)
{       
    u_int32_t          t;

    proatm_reg_wr (proatm, 0xa, REGGP);             /* PHY reset */
    DELAY(100);
    proatm_reg_wr (proatm, 0x2, REGGP);
    DELAY(100);
    proatm_status_wait(proatm);
               
    proatm_util_rd(proatm, SUNI_MASTER_REG, &t);    /* get Master Control Register */
/*
 *  Both MCR are 0x00
 *  proatm_util_rd(proatm, IDT77105_MCR_REG, &t);
 */

    /*  Register 0 values:
     *       IDT77105  = 0x09
     *       PM5346 = 0x30
     *       PM5250 = 0x31  (actually observed)
     *       IDT77155 = 0x70
     *       PM5350 or idt77155 = 0x70 or 0x78 (according to docs)
     */

    if (t == 0x09) {
        printf("proatm%d: ATM card is PROATM-25, PHY=IDT77105\n", proatm->unit);
        strcpy (proatm->hardware, "PROATM-25");
        return (1);
    }
    
    if (t == 0x70 || t == 0x30 || t == 0x31) {
        printf("proatm%d: ATM card is PROATM-155 or IDT, PHY=PM5346 or IDT77155\n", proatm->unit);
        strcpy (proatm->hardware, "PROATM-155");
        return (0);
    }
    printf("proatm%d: unknown card, assuming PROATM-155 compatible model (reg0= 0x%x).\n",
           proatm->unit, (int32_t) t);
    strcpy (proatm->hardware, "unknown" );
    return (0);
}

/******************************************************************************
 *
 *  Initialize the data structures
 */

static int32_t 
proatm_init(proatm_reg_t *const proatm)
{       
    int32_t            i;
    vm_offset_t        buf;
    u_int32_t          *p;
    u_int32_t          cfg;

    proatm->vpibits = PROATM_VPIBITS;       /* compilation constant */
    if (proatm->max_connection == 1024) {
        cfg = IDT_CFG_RCTSIZE_1024_ENTRIES;
        proatm->vcibits = 10 - proatm->vpibits;
    }
    else {
        proatm->vcibits = 12 - proatm->vpibits;
        cfg = IDT_CFG_RCTSIZE_4096_ENTRIES;
    }
                                    /* compute the vpm register value */
    proatm->vpm = (VCI_BASE>>proatm->vcibits) |
                 ((VPI_BASE>>proatm->vpibits) << (16 - proatm->vcibits));   
    proatm->vpm &= 0x7fff;
    XPRINT ("proatm_init: VPM = 0x%x\n", proatm->vpm);
     
    proatm_reg_wr (proatm, cfg, REGCFG);      /* initialize RCT and TCT sizes */

    if (proatm_connect_init(proatm)) {         /* initialize for 0, 1, 2 or 8 VPI bits */
        printf ("proatm%d: Cannot allocate connection table memory\n", proatm->unit);
        goto freemem;
    }           
   
    /* allocate space for TSQ, RSQ */
    proatm->fixbuf = (vm_offset_t) proatm_malloc_contig (PROATM_FIXPAGES * PAGE_SIZE);
    if (proatm->fixbuf == NULL) {
        printf ("proatm%d: Cannot allocate buffer memory\n", proatm->unit);
        goto freemem;
    }                        
    proatm->raw_headm = NULL;               /* proatm_add_buf() will initialize */
    proatm->raw_headp = 0;
    proatm->raw_ch = NULL;

    if (proatm_mcheck_init(proatm))
        goto freemem;

    proatm_found++;                        /* number of cards found on machine */

    if (bootverbose) {
        printf("proatm%d: buffer size %d\n", proatm->unit,  PROATM_FIXPAGES * PAGE_SIZE);
    }
    if (proatm_queue_init (proatm))               /* initialize all TX_QUEUE structures */
        goto freemem;

    proatm_reg_wr(proatm, 0, REGNOW);
    proatm_dtst_init (proatm);                /* initialize the ABR/VBR  dynamic TST*/
    proatm_tst_init (proatm);                 /* initialize TST tables */
    proatm_rate_init (proatm);                /* initialize  the rate table */
    proatm_reg_wr(proatm, 0, REGABRRQ);
    proatm_reg_wr(proatm, 0, REGVBRRQ);

    /* clear mbuf queues */
    bzero((caddr_t) proatm->fixbuf, PROATM_FIXPAGES * PAGE_SIZE);

     /* Clear the TCT (must be located at SRAM address 0x00000 */
     for (i = 0; i < proatm->max_connection; i++) {
        proatm_sram_wr (proatm, i*8,     4, 0,0,0,0);
        proatm_sram_wr (proatm, i*8 + 4, 4, 0,0,0,0);
    }

    /* Initialize the  RCT */
    for (i = 0; i < proatm->max_connection; i ++) {
#ifdef RCQ_SUPPORT
        proatm_sram_wr(proatm, proatm->rct + i*4, 4, IDT_RCTE_RAWCELLINTEN, 0x0, 0x0, 0xffffffff);
#else
        proatm_sram_wr(proatm, proatm->rct + i*4, 4, 0x0, 0x0, 0x0, 0xffffffff);
#endif
    }

    /* Initialize RAWHND */
    proatm->raw_hnd[0] = 0;
    proatm_reg_wr (proatm, vtophys (proatm->raw_hnd), REGRAWHND);


    /* Initialize the TSQs */
    for (p = (u_int32_t *) proatm->fixbuf+ PROATM_TSQ_OFFSET;
        p < (u_int32_t *)(proatm->fixbuf + PROATM_TSQ_OFFSET + PROATM_TSQ_SIZE); ) {
        *p++ = 0x00000000;
        *p++ = IDT_TSI_EMPTY;                        /* set empty bit */
    }

    buf = vtophys(proatm->fixbuf + PROATM_TSQ_OFFSET);
    proatm_reg_wr (proatm, buf, REGTSQB);            /* TSQ Base */
    proatm->tsq_base = (u_int32_t *) proatm->fixbuf;
    proatm->tsq_head = (u_int32_t *) proatm->fixbuf;    /* TSQ Head */
    proatm_reg_wr (proatm, 0, REGTSQH);
    proatm->tsq_size = PROATM_TSQ_SIZE /8;           /* TSQ entries */

    /* Initialize the receive FIFO */
    proatm_reg_wr (proatm, (proatm->rxfifo << 2) | PROATM_RXFD_SIZE, REGRXFD);
    proatm_reg_wr (proatm, 0, REGRXFT);
    proatm_reg_wr (proatm, 0, REGRXFH);

    /* RSQ initialization */
    buf = vtophys(proatm->fixbuf + PROATM_RSQ_OFFSET);
    proatm_reg_wr (proatm, buf , REGRSQB);    /* RSQ Base */
    proatm_reg_wr (proatm, 0, REGRSQH);       /* RSQ Head, 8k aligned*/
    proatm->rsqh =  0;                        /* reflect the register */

    /* Initialize RFBQ pointers */
    proatm_reg_wr (proatm, 0, REGFBQP0);
    proatm_reg_wr (proatm, 0, REGFBQP1);
    proatm_reg_wr (proatm, 0, REGFBQP2);
    proatm_reg_wr (proatm, 0, REGFBQP3);

    /* Initialize RFBQ thresholds */
#ifdef FBSD35
	proatm_reg_wr (proatm, IDT_B0THLD | IDT_B1SIZE, REGFBQS0);
#else
    proatm_reg_wr (proatm, IDT_B0THLD | IDT_B0SIZE, REGFBQS0);
#endif
    proatm_reg_wr (proatm, IDT_B1THLD | IDT_B1SIZE, REGFBQS1);
    proatm_reg_wr (proatm, 0, REGFBQS2);      /* unused */
    proatm_reg_wr (proatm, 0, REGFBQS3);      /* unused */

    /* Allocate RFBQ buffers */
    proatm_ld_rcv_buf(proatm, proatm_sysctl_bufsmall, proatm_sysctl_buflarge);

    /* Check that buffer allocation worked well */
    i = idt_fbqc_get(proatm_reg_rd (proatm, REGFBQP0));
    if (i < proatm_sysctl_bufsmall) {
        printf("proatm%d: Wanted %d small buffers but got %d only.\n", 
                            proatm->unit, proatm_sysctl_bufsmall, i);
    }
    i = idt_fbqc_get(proatm_reg_rd (proatm, REGFBQP1));
    if (i < proatm_sysctl_buflarge) {
        printf("proatm%d: Wanted %d large buffers but got %d only.\n", 
                            proatm->unit, proatm_sysctl_buflarge, i);
    }

    /* VPI/VCI mask VMSK */
    proatm_reg_wr (proatm, proatm->vpm, REGVMSK);

    /* Configuration Register */
    proatm_reg_wr (proatm, cfg | IDT_DEFAULT_CFG, REGCFG);

    return 0;

freemem:
    /* free memory and return */
    proatm_release_mem(proatm);
    printf("proatm%d: Cannot allocate memory\n", proatm->unit);
    return (ENOMEM);                                           /* no space card disabled */
}

/******************************************************************************
 *
 *  Release all allocated memory
 *
 *  in:  PROATM device
 *
 */

static void 
proatm_release_mem(PROATM *proatm)
{
#ifdef FBSD35
    #define contigfree(addr, size, type) \
       kmem_free(kernel_map, (vm_offset_t)addr, size);
#endif

    if (proatm->mcheck != NULL) {
        vm_size_t size;

        size = round_page(sizeof (struct mbuf *) * PROATM_MCHECK_COUNT);
        contigfree(proatm->mcheck, size, M_PROATM);
        proatm->mcheck = NULL;
    }

    if (proatm->connection != NULL) {
        vm_size_t pages;

        pages = (sizeof(CONNECTION) * proatm->max_connection) + PAGE_SIZE - 1;
        pages /= PAGE_SIZE;
        contigfree(proatm->connection, pages *PAGE_SIZE, M_PROATM);
        proatm->connection = NULL;
    }

    if (proatm->fixbuf != NULL)
        contigfree((void *)proatm->fixbuf, (PROATM_FIXPAGES *PAGE_SIZE), M_PROATM);
    proatm->fixbuf = NULL;

    if (proatm->scq_cluster_size != NULL)
        contigfree((void *)proatm->scq_cluster_base, proatm->scq_cluster_size, M_PROATM);
    proatm->scq_cluster_base = NULL;
}

/******************************************************************************
 *
 *  Write to Nicstar Register
 *
 */

static void 
proatm_reg_wr ( proatm_reg_t *const proatm, u_int32_t data, vm_offset_t reg)
{ 
   *(volatile u_int32_t *)(proatm->virt_baseaddr + reg) = data;
}

/******************************************************************************
 *
 *  Read from Nicstar Register
 *
 */

static u_int32_t 
proatm_reg_rd ( proatm_reg_t *const proatm, vm_offset_t reg)
{
    return  *(volatile u_int32_t *)(proatm->virt_baseaddr + reg);
}

/******************************************************************************
 *
 *  Write one to four words to SRAM
 *
 *    writes one to four words into sram starting at "sram_location"
 *    the SRAM address is a word (4-byte) address 
 *
 *    returns -1 if sram location is out of range.
 *    returns count, if count is not in the range from 1-4.
 *    returns 0 if parameters were acceptable
 */

static int32_t 
proatm_sram_wr(proatm_reg_t *const proatm, u_int32_t address, int count,
                           u_int32_t data0, u_int32_t data1, u_int32_t data2, u_int32_t data3)
{       
    if (address >= proatm->sram_size)
         return (- 1); /* bad address */

    if (proatm_status_wait(proatm))
        return (- 1);

    switch (--count) {
      case 3:
        proatm_reg_wr(proatm,data3, REGDR3);           /* drop down to do others */
      case 2:
        proatm_reg_wr(proatm,data2, REGDR2);           /* drop down to do others */
      case 1:
        proatm_reg_wr(proatm,data1, REGDR1);           /* drop down to do others */
      case 0:
        proatm_reg_wr(proatm,data0, REGDR0);            /* load last data item */
        break;                                        /* done loading values */
      default:
        return (count);                               /* nothing to do */
    }
    proatm_reg_wr (proatm, IDT_CMD_WRITE_SRAM | (address << 2) | count, REGCMD);

    return (0);
}

/*******************************************************************************
 *
 *  Read one word from SRAM
 *
 *    reads one word of sram at "sram_location" and places the value
 *    in "answer_pointer"
 *
 *    returns -1 if sram location is out of range.
 *    returns 0 if parameters were acceptable
 */

static int32_t
proatm_sram_rd(proatm_reg_t *const proatm, u_int32_t address, u_int32_t *data0)
{       
    if (address >= proatm->sram_size)
        return (- 1);               /* bad address */

    if (proatm_status_wait(proatm))
        return (- 1);

    proatm_reg_wr(proatm, IDT_CMD_READ_SRAM | (address << 2), REGCMD);

    if (proatm_status_wait(proatm))
        return (- 1);

    *data0 = proatm_reg_rd (proatm, REGDR0);            /* save word */

    return (0);
}

/* for debug purpose */

u_int32_t
nicsr (proatm_reg_t *const proatm, u_int32_t address)
{
    proatm_reg_wr(proatm, IDT_CMD_READ_SRAM | (address << 2), REGCMD);
    if (proatm_status_wait(proatm))
        return -1;

    return proatm_reg_rd (proatm, REGDR0);
}

#if XDEBUG == 2
void bprintf (char *format,...)
{
    va_list args;
    char    buf[0x400];
    int32_t     len = 0;

    buf[0x3ff] = '\0';
    va_start (args, format);
    vsprintf (buf, format, args);
    len = strlen (buf);
    if (proatm_logptr + len + 1 < proatm_log + PROATM_LOG_SIZE) {
        memcpy (proatm_logptr, buf, len + 1);
        proatm_logptr += len;
    }
    va_end (args);
}

void print_proatm_log (void)
{
    printf ("\n************ PROATM LOG ************\n");
    printf ("%s", proatm_log);
    proatm_logptr = proatm_log;
}
#endif

    

/*******************************************************************************
 *
 *  Open connection in PROATM Receive Connection Table
 *
 *  in:  PROATM device, VPI, VCI, opflag
 * out:  zero = success
 *
 */

static int32_t
proatm_connect_rxopen (PROATM *proatm, CONNECTION *connection)
{
    int32_t             addr;
    int32_t             word1;

    if (proatm_check_vc(proatm, connection->vpi, connection->vci))
        return (1);

    addr = proatm->rct + connection->number * IDT_RCT_ENTRY_SIZE;
/* 
    switch (connection->aal) {
    case IDTAAL0:
        word1 = IDT_RCTE_CONNECTOPEN | IDT_RCTE_AAL0;
        break;
    case IDTAAL1:
        word1 = IDT_RCTE_CONNECTOPEN | IDT_RCTE_AAL0;
        break;
    case IDTAAL3_4:
        word1 = IDT_RCTE_CONNECTOPEN | IDT_RCTE_AAL34;
        break;
    case IDTAAL5:
        word1 = IDT_RCTE_CONNECTOPEN | IDT_RCTE_AAL5;
        break;
    default:
        return (1);
    }
*/
    word1 = IDT_RCTE_CONNECTOPEN | IDT_RCTE_AAL5;   /* no other AAL supported at this time */

#ifdef RCQ_SUPPORT
    word1 |= IDT_RCTE_RAWCELLINTEN;
#endif
    proatm_sram_wr(proatm, addr, 4, word1, 0, 0, 0xffffffff);

return 0;
}

/*******************************************************************************
 *
 *  Close connection in PROATM Receive Connection Table
 *
 *  in:  PROATM device, VPI, VCI
 * out:  zero = success
 *
 */

static int32_t
proatm_connect_rxclose (PROATM *proatm, CONNECTION *connection)
{
    int32_t             addr;

    if (proatm_check_vc(proatm, connection->vpi, connection->vci))
        return (1);

    addr = proatm->rct + connection->number * IDT_RCT_ENTRY_SIZE;
 
    if (proatm_status_wait(proatm))
        return (- 1);

    proatm_reg_wr (proatm, IDT_CMD_CLOSE_CONNECTION | addr<<2, REGCMD);
    return 0;
}


/*******************************************************************************
 *
 *    proatm_add_buf    ( card, mbuf1, mbuf2, which_queue)
 * 
 *    This adds two buffers to the specified queue. This uses the
 *    mbuf address as handle and the buffer physical address must be
 *    the DMA address.
 *
 *    returns -1 if queue is full, the address is not word aligned, or
 *    a invalid queue is specified
 *    returns 0 if parameters were acceptable
 */

static int32_t 
proatm_add_buf(proatm_reg_t *const proatm, struct mbuf *buf0,
                    struct mbuf *buf1, u_int32_t rfbq)
{       
    u_int32_t          stat_val;
    u_int32_t          val0, val1, val2, val3;

    stat_val = proatm_reg_rd (proatm, REGSTAT);

    switch (rfbq) {
        case 0:
            if ((stat_val & IDT_STAT_FRAC0_MASK) == IDT_STAT_FRAC0_MASK)
                return -1;          /*small-buffer queue is full */
            break;
        case 1:
            if ((stat_val & IDT_STAT_FRAC1_MASK) == IDT_STAT_FRAC1_MASK)
                return -1;          /*large-buffer queue is full */
            break;            
        default: 
            return -1;              /* we manage RFBQ0 and RFBQ1 only */
    }
    if (!buf0 || !buf1 || ((u_int32_t)(buf0->m_data)& 0x3)
        || ((u_int32_t)(buf1->m_data)& 0x3)) {
        return (- 1);               /* buffers must be word aligned */
    }
    if (proatm->raw_headm == NULL)
                                    /* raw cell buffer pointer not initialized */
        if (rfbq == 1) {
            proatm->raw_headm = buf0;
            proatm->raw_ch = (rcqe*)buf0->m_data;
            proatm->raw_headp = vtophys(buf0->m_data);
        }
    if (proatm_status_wait(proatm))
        return (- 1);

    val0 = (u_int32_t) buf0;           /* mbuf address is handle */
    val1 = vtophys(buf0->m_data);   /* DMA addr of buff1 */
    val2 = (u_int32_t) buf1;           /* mbuf address is handle */
    val3 = vtophys(buf1->m_data);   /* DMA addr of buff2 */

    proatm_reg_wr(proatm, val0, REGDR0);
    proatm_reg_wr(proatm, val1, REGDR1);
    proatm_reg_wr(proatm, val2, REGDR2);
    proatm_reg_wr(proatm, val3, REGDR3);
    proatm_reg_wr(proatm, IDT_CMD_WRITE_FREEBUFQ | rfbq, REGCMD);
    proatm_mcheck_add(proatm, buf0);
    proatm_mcheck_add(proatm, buf1);
    return (0);
}

/******************************************************************************
 *
 *    proatm_util_rd    ( card, util_location, answer_pointer )
 *
 *    reads one byte from the utility bus at "util_location" and places the
 *    value in "answer_pointer"
 *
 *    returns -1 if util location is out of range.
 *    returns 0 if parameters were acceptable
 */
static int32_t
proatm_util_rd(proatm_reg_t *const proatm, u_int32_t address, u_int32_t *data)
{       
    if (address >= 0x81)
        return (- 1);                        /* bad address */
    if (proatm_status_wait(proatm))
        return (- 1);
    proatm_reg_wr (proatm, IDT_CMD_READ_UTILITY_CS0 | address, REGCMD); 
    if (proatm_status_wait(proatm))
        return (- 1);
    *data = proatm_reg_rd (proatm, REGDR0) & 0xff;    /* return word */
    return (0);
}

/******************************************************************************
 *
 *    proatm_util_wr    ( card, util location, data )
 *
 *    writes one byte to the utility bus at "util_location" 
 *
 *    returns -1 if util location is out of range.
 *    returns 0 if parameters were acceptable
 */
static int32_t
proatm_util_wr(proatm_reg_t *const proatm, u_int32_t address, u_int32_t data)
{       
    if (address >= 0x81)                                                      
        return (- 1);               /* bad address */
    if (proatm_status_wait(proatm))
        return (- 1);
    proatm_reg_wr (proatm, data & 0xff, REGDR0);    
    proatm_reg_wr (proatm, IDT_CMD_WRITE_UTILITY_CS0 | address, REGCMD);
    return (0);
}

/******************************************************************************
 *
 *    proatm_eeprom_byte_rd ( card , byte_location, len )
 *
 *    reads one byte from the utility bus at "byte_location" and return the
 *    value as an integer. 
 */
static u_int8_t
proatm_eeprom_byte_rd(proatm_reg_t *const proatm, u_int32_t address)
{       
    volatile        u_int32_t gp = proatm_reg_rd (proatm, REGGP) & 0xfffffff8;
    int32_t             i, value = 0, command = 3;

    DELAY(5);
                                             /* make sure idle */
    proatm_reg_wr (proatm, gp | IDT_GP_EECLK | IDT_GP_EECS, REGGP);     /* CS and Clock high */        
    DELAY(5);
    /* toggle in  READ CMD (00000011) */
    for (i=7; i >= 0; i--) {
        proatm_reg_wr (proatm, gp | ((command >> i)& 1), REGGP);           /* Clock low */
        DELAY(5);
        proatm_reg_wr (proatm, gp | IDT_GP_EECLK | ((command >> i)& 1), REGGP);    /* Clock high */
        DELAY(5);
    }
    /* toggle in address */
    for (i = 7; i >= 0; i--) {
        proatm_reg_wr (proatm, gp | ((address >> i)& 1), REGGP);          /* Clock low */
        DELAY(5);
        proatm_reg_wr (proatm, gp | IDT_GP_EECLK | ((address >> i)& 1), REGGP);   /* Clock high */
        DELAY(5);
    }
    /* read EEPROM data */
    for (i = 7; i >= 0; i--) {
        proatm_reg_wr (proatm, gp, REGGP);                                  /* Clock low */
        DELAY(5);
        value |= (proatm_reg_rd(proatm, REGGP) & IDT_GP_EEDI) >> (16 - i);
        proatm_reg_wr (proatm, gp | IDT_GP_EECLK, REGGP);                     /* Clock high */
        DELAY(5);
    }
    proatm_reg_wr (proatm, gp, REGGP);                                      /* CS and Clock low */
    return ((u_int8_t)value);
}

/******************************************************************************
 *
 *    proatm_eeprom_rd  ( card , eeprom location, memory location, len )
 *
 *    reads len bytes from the utility bus at "eeprom location" and write them
 *    at memory location. this routine is only used to read the MAC address
 *    from the EEPROM at boot time.
 */

static void
proatm_eeprom_rd(proatm_reg_t *const proatm, u_int32_t ee_add, u_int8_t *memptr, u_int32_t len)
{
    while (len-- >0)
        *memptr++ = proatm_eeprom_byte_rd(proatm, ee_add++);
}

/*******************************************************************************
 *
 *  Load the card receive buffers
 *
 *  in:  PROATM device
 *
 */

void 
proatm_ld_rcv_buf(PROATM *proatm, int32_t want_small, int32_t want_large)
{       
    struct mbuf         *m1, *m2;
    u_int32_t           reg_stat;
    int32_t             card_small;
    int32_t             card_large;
    int32_t             s;

    s = splimp();

    /* read card current amount of free buffer available */
    reg_stat = proatm_reg_rd (proatm, REGSTAT);
    card_small = (reg_stat & IDT_STAT_FRAC0_MASK) >> 11;
    card_large = (reg_stat & IDT_STAT_FRAC1_MASK) >> 15;

#ifdef FBSD35
	/* 
     * With FreeBSD versions 3.5 and 351, we allocate 2K clusters since
     * mbufs are too small (128 Bytes only) 
     */
    while (card_small < want_small) {
        m1 = proatm_mbufcl_get();      
        if (m1 == NULL)
            break;
                                            /* Prepend 32-byte space */
        m1->m_data += (MCLBYTES - (IDT_B1SIZE * 48));
        m1->m_len -= (MCLBYTES - (IDT_B1SIZE * 48));

        m2 = proatm_mbufcl_get();
        if (m2 == NULL) {
            m_free(m1);
            break;
        }
                                             /* Prepend 32-byte space */
        m2->m_data += (MCLBYTES - (IDT_B1SIZE * 48));
        m2->m_len -= (MCLBYTES - (IDT_B1SIZE * 48));

        if (proatm_add_buf(proatm, m1, m2, 0)) {
            printf("proatm%d: Cannot add buffers to FBQ0, size=%d.\n", 
                   proatm->unit, card_small);
            m_free(m1);
            m_free(m2);
            break;
        }
        card_small += 2;
    }
	
#else		
	/* get 2-cell buffers if current level of small buffers is below desired level */   
	while (card_small < want_small) {
        MGETHDR(m1, M_DONTWAIT, MT_DATA);
        if (m1 == NULL)
            break;
        MGETHDR(m2, M_DONTWAIT, MT_DATA);
        if (m2 == NULL) {
            m_free(m1);
            break;
        }
        MH_ALIGN(m1, 96);          /* word align & allow lots of prepending */
        MH_ALIGN(m2, 96);
        if (proatm_add_buf(proatm, m1, m2, 0)) {
            printf("proatm%d: Cannot add small buffers, size=%d.\n", 
                   proatm->unit, card_small);
            m_free(m1);
            m_free(m2);
            break;
        }
        card_small += 2;
    }
#endif

    /* get 2K clusters if current level of small buffers is below desired level */
    while (card_large < want_large) {
        m1 = proatm_mbufcl_get();      
        if (m1 == NULL)
            break;
        m2 = proatm_mbufcl_get();
        if (m2 == NULL) {
            m_free(m1);
            break;
        }
        if (proatm_add_buf(proatm, m1, m2, 1)) {
            printf("proatm%d: Cannot add large buffers, size=%d.\n", 
                   proatm->unit, card_large);
            m_free(m1);
            m_free(m2);
            break;
        }
        card_large += 2;
    }
    splx(s);
}

/*******************************************************************************
 *
 *  Wait for command to finish
 *
 *  in:  PROATM device
 * out:  zero = success
 *
 */

static int32_t 
proatm_status_wait(PROATM *proatm)
{       
    int32_t             timeout;

    timeout = 33 * 100;                                /* allow 100 microseconds timeout */

    while (proatm_reg_rd (proatm, REGSTAT) & 0x200)
        if (--timeout == 0) {
            printf("proatm%d: timeout waiting for device status.\n", proatm->unit);
            proatm->stats_cmderrors++;
            return (1);
        }
    return (0);
}

/*******************************************************************************
 *
 *  Log status of system buffers
 *
 *  in:  PROATM device
 *
 */

static void 
proatm_status_bufs(PROATM *proatm)
{
    int32_t             card_small;
    int32_t             card_large;
    int32_t             s;

    s = splimp();
    card_small = idt_fbqc_get(proatm_reg_rd (proatm, REGFBQP0));
    card_large = idt_fbqc_get(proatm_reg_rd (proatm, REGFBQP1));
    splx(s);
    printf("proatm%d BUFFER STATUS: small=%d/%d, large=%d/%d.\n",
           proatm->unit, 
           card_small, proatm_sysctl_bufsmall,
           card_large, proatm_sysctl_buflarge);
}

/*******************************************************************************
 *
 *  Add mbuf into "owned" list
 *
 *  in:  PROATM device, mbuf
 * out:  zero = success
 *
 */

static int32_t 
proatm_mcheck_add(PROATM *proatm, struct mbuf *m)
{       
    int32_t             hpos;
    int32_t             s;

    hpos = (((int32_t) m) >> 8)& 1023;
    s = splimp();
    m->m_next = proatm->mcheck[hpos];
    proatm->mcheck[hpos] = m;
    splx(s);
    return (0);
}

/******************************************************************************
 *
 *  Remove mbuf from "owned" list
 *
 *  in:  PROATM device, mbuf
 * out:  zero = success
 *
 */

static int32_t 
proatm_mcheck_rem(PROATM *proatm, struct mbuf *m)
{       
    struct mbuf     *nbuf;
    int32_t             hpos;
    int32_t             s;

    hpos = (((int32_t) m) >> 8)& 1023;
    s = splimp();
    nbuf = proatm->mcheck[hpos];
    if (nbuf == m) {
        proatm->mcheck[hpos] = m->m_next;
        splx(s);
        m->m_next = NULL;
        return (0);
    }
    while (nbuf != NULL) {
        if (nbuf->m_next != m) {
            nbuf = nbuf->m_next;
            continue;
        }
        nbuf->m_next = m->m_next;
        splx(s);
        m->m_next = NULL;
        return (0);
    }
    splx(s);
    printf("proatm%d: Card should not have this mbuf! %x\n", proatm->unit, (int) m);
    return (1);
}

/******************************************************************************
 *
 *  Initialize mbuf "owned" list
 *
 *  in:  PROATM device
 * out:  zero = success
 *
 */

static int32_t 
proatm_mcheck_init(PROATM *proatm)
{       
    int32_t             size;
    int32_t             x;

    size = round_page(sizeof (struct mbuf *) * PROATM_MCHECK_COUNT);
    proatm->mcheck = (struct mbuf * *) proatm_malloc_contig (size);
    if (proatm->mcheck == NULL)
        return (ENOMEM);

    for (x = 0; x < 1024; x++)
             proatm->mcheck[x] = NULL;

    return (0);
}

/******************************************************************************
 *
 *  Allocate contiguous, fixed memory
 *
 *  in:  size in bytes
 * out:  pointer, NULL = failure
 *
 */

static void * 
proatm_malloc_contig (int32_t size)
{       
    void* retval;

    retval = contigmalloc (size, M_PROATM, M_NOWAIT, 0x100000, 0xffffffff,
                           0x2000, 0ul);
    XPRINT("proatm: contig memory allocated %d bytes at %0x\n", size, retval);

    return (retval);
}

/*******************************************************************************
 *
 *  Initialize all TX_QUEUE structures 
 *
 *  in:  PROATM device
 * out:  zero = succes
 *
 */
static int32_t
proatm_queue_init(PROATM *proatm)
{       
    TX_QUEUE        *txqueue;
    vm_offset_t     scq_base;
    int32_t             x;

    proatm->scq_cluster_size = proatm->scd_size * IDT_SCQ_SIZE;    
    proatm->scq_cluster_base = (vm_offset_t) proatm_malloc_contig(proatm->scq_cluster_size);
    scq_base = proatm->scq_cluster_base;
    if (scq_base == NULL)
        return (ENOMEM);

    proatm->txqueue_free_count = proatm->scd_size;
    for (x = 0; x < proatm->txqueue_free_count; x++) {
        txqueue = &proatm->txqueue[x];
        txqueue->mget = NULL;
        txqueue->mput = NULL;
        txqueue->scd = proatm->scd + x * IDT_STRUCT_SCD_SIZE;  /*word address */
        txqueue->scq_base = (u_int32_t *) scq_base;
        txqueue->scq_next = txqueue->scq_base;
        txqueue->scq_last = txqueue->scq_next;
        txqueue->scq_cur = 0;
        txqueue->rate = 0;
        proatm->txqueue_free[x] = txqueue;
                                            /* initialize SCD */
        proatm_sram_wr(proatm, txqueue->scd, 4,
                         vtophys (txqueue->scq_base), 0, 0xffffffff, 0);
        proatm_sram_wr(proatm, txqueue->scd + 4, 4, 0, 0, 0, 0);
        proatm_sram_wr(proatm, txqueue->scd + 8, 4, 0, 0, 0, 0);

        scq_base += IDT_SCQ_SIZE;
    }
                                            /* UBR0 queue */
    txqueue = &proatm->txqueue_ubr0;          
    txqueue->mget = NULL;
    txqueue->mput = NULL;
    txqueue->scd = proatm->scd_ubr0;
    txqueue->scq_base = (u_int32_t*)ALIGN_ADDR(proatm->scq_ubr0_cluster, IDT_SCQ_SIZE);
    txqueue->scq_next = txqueue->scq_base;
    txqueue->scq_last = txqueue->scq_next;
    txqueue->scq_cur = 0;
    txqueue->rate = 0;
    proatm_sram_wr(proatm, proatm->scd_ubr0, 4,
            vtophys (proatm->txqueue_ubr0.scq_base), 0, 0xffffffff, 0);
    proatm_sram_wr(proatm, proatm->scd_ubr0+4, 4, 0, 0, 0, 0);
    proatm_sram_wr(proatm, proatm->scd_ubr0+8, 4, 0, 0, 0, 0);
    proatm_tct_wr (proatm, 0);

    return (0);
}

/*******************************************************************************
 *
 *  Get mbuf chain from TX_QUEUE
 *
 *  called by proatm_transmit_top at splimp()
 *
 *  in:  CONNECTION
 * out:  mbuf, NULL = empty
 *
 */
static struct mbuf *
proatm_queue_get (TX_QUEUE *txqueue)
{       
    struct mbuf     *m;

    if (txqueue == NULL)
        return (NULL);

    /* s = splimp(); */
    m = txqueue->mget;
    if (m != NULL) {
         txqueue->mget = m->m_nextpkt;
         if (txqueue->mget == NULL)    /* is queue empty now? */                                                     
            txqueue->mput = NULL;
    }
	/* splx(s); */
    return (m);
}

/*******************************************************************************
 *
 *  Add mbuf chain to connection TX_QUEUE
 *
 *  in:  CONNECTION, mbuf chain
 * out:  zero = succes
 *
 */
static int32_t
proatm_queue_put(CONNECTION *connection, struct mbuf *m)
{       
    TX_QUEUE        *txqueue;
    int32_t             s;

    if (connection == NULL) {
        /*
         * it has already been checked by proatm_transmit,
         * that has updated the AAL's counters.
         */
        m_freem(m);
        return (1);
    }
    txqueue = connection->queue;
    if (txqueue == NULL) {
        /*
         * it has already been checked by proatm_transmit,
         * that has updated the AAL's counters.
         */
        m_freem(m);
        return (1);
    }
    m->m_nextpkt = NULL;
    if (!KB_ISPKT(m))
        panic(__FUNCTION__" missing M_PKTHDR");

    m->m_pkthdr.rcvif = (struct ifnet *) connection;
    s = splimp();
    if (txqueue->mput != NULL) {
        txqueue->mput->m_nextpkt = m;
        txqueue->mput = m;
    } else {
                                                      /* queue is empty */
        txqueue->mget = m;
        txqueue->mput = m;
    }
    splx(s);
    return (0);
}

/*******************************************************************************
 *
 *  Flush all connection mbufs from TX_QUEUE
 *
 *  in:  CONNECTION
 * out:  zero = succes
 *
 */
static int32_t
proatm_queue_flush(CONNECTION *connection, int32_t *count)
{       
    TX_QUEUE        *txqueue;
    struct mbuf     *m, **mp, *mprev;
    int32_t             s, cnt=0;

    if (connection == NULL)
        return (1);

    txqueue = connection->queue;
    if (txqueue == NULL)
        return (1);

    s = splimp();
    mprev = NULL;
    mp = &txqueue->mget;
    m = txqueue->mget;

    while (m != NULL) {
        if (m->m_pkthdr.rcvif == (struct ifnet *) connection) {
            *mp = m->m_nextpkt;
            m_freem(m);
            m = *mp;
            cnt++;
            continue;
        }
        mprev = m;
        mp = &mprev->m_nextpkt;
        m = mprev->m_nextpkt;
    }
    txqueue->mput = mprev;
    splx(s);
    *count = cnt;
    return (0);
}

/*******************************************************************************
 *
 *  Calculate number of table positions for CBR connection
 *
 *  in:  PROATM device, PCR (cells/second)
 * out:  table positions needed (minimum = 1)
 *
 */
static int32_t
proatm_slots_cbr(PROATM *proatm, int32_t pcr)
{       
    u_int32_t    slots;

    if (pcr <= proatm->max_pcr / proatm->tst_num) {
        if (proatm_sysctl_logvcs)
            printf("proatm_slots_cbr:  CBR pcr %d rounded up to 1 slot\n", pcr);
        return (1);
    }
    slots = (pcr * proatm->tst_num)/proatm->max_pcr;
    if (proatm_sysctl_logvcs)
        printf("proatm_slots_cbr: CBR cell rate rounded down to %d from %d\n",
               (int32_t)((slots * proatm->max_pcr)/proatm->tst_num), pcr);

    return (slots);
}

/*******************************************************************************
 *
 *  Add TX QUEUE pointer to slots in CBR table
 *
 *  in:  PROATM device, TX_QUEUE, number slots
 * out:  zero = success
 *
 */

static int32_t
proatm_slots_add (PROATM *proatm, TX_QUEUE *queue, int32_t slots, int32_t conn)
{
    TX_QUEUE        *curval;
    int32_t             p_max;                            /* extra precision slots maximum */
    int32_t             p_spc;                            /* extra precision spacing value */
    int32_t             p_ptr;                            /* extra precision pointer */
    int32_t             qptr, qmax;
    int32_t             qlast;

    if (slots < 1)
        return -1;

    qmax = proatm->tst_num;
    p_max = qmax << 8;
    p_spc = p_max / slots;
    p_ptr = p_spc >> 1;                               /* use half spacing for start point */
    qptr = p_ptr >> 8;
    qlast = qptr;


    XPRINT("proatm_slots_add: p_max = %d\n", p_max);
    XPRINT("proatm_slots_add: p_spc = %d\n", p_spc);
    XPRINT("proatm_slots_add: p_ptr = %d\n", p_ptr);
    XPRINT("proatm_slots_add: qptr  = %d\n", qptr);


    while (slots) {
        if (qptr >= qmax)
            qptr -= qmax;                           /* handle wrap for empty slot choosing */
        curval = proatm->tst_slot[qptr];
        if (curval != NULL) {
                                                      /* this slot has CBR, so try next */
            qptr++;                                   /* next slot */
            continue;
        }


        XPRINT("proatm_slots_add: using qptr %d (%d)\n", qptr, qptr - qlast);
        qlast = qptr;

        proatm->tst_slot[qptr] = queue;
        proatm_sram_wr(proatm, proatm->tst + qptr, 1, IDT_TST_OPCODE_FIXED | conn, 0, 0, 0);
        slots--;
        p_ptr += p_spc;
        if (p_ptr >= p_max)                                                     
            p_ptr -= p_max;                           /* main pointer wrap */
        qptr = p_ptr >> 8;
    }
    return 0;
}

/*       Extra precision pointer is used in order to handle cases where
 *       fractional slot spacing causes a large area of slots to be filled.
 *       This can cause further CBR circuits to get slots that have very
 *       poor spacing.
 *
 */

/*******************************************************************************
 *
 *  Remove TX QUEUE pointer from slots in CBR table
 *
 *  in:  PROATM device, TX_QUEUE
 * out:  number of CBR slots released
 *
 */
static int32_t
proatm_slots_rem(PROATM *proatm, TX_QUEUE *queue)
{       
    int32_t             qptr, qmax;
    int32_t             slots;

    qmax = proatm->tst_num;
    slots = 0;
    for (qptr = 0; qptr < qmax; qptr++) {
        if (proatm->tst_slot[qptr] != queue)
            continue;

        proatm->tst_slot[qptr] = NULL;
        proatm_sram_wr(proatm, proatm->tst + qptr, 1, IDT_TST_OPCODE_VARIABLE, 0, 0, 0);
        slots++;
    }
    return (slots);
}

/*******************************************************************************
 *
 *  Initialize the ABRVBR DTST tables
 *
 *  in:  PROATM device
 */

static void 
proatm_dtst_init(PROATM *proatm)
{
    u_int32_t j;
          
    proatm_reg_wr(proatm, PROATM_ABRSTD_SIZE | (proatm->dtst<<2), REGABRSTD);
    for (j = 0; j < PROATM_DTST_SIZE; j++) {
        proatm_sram_wr (proatm, proatm->dtst + j, 1, 0, 0, 0, 0);
    }
}


/*******************************************************************************
 *
 *  Initialize the TST table
 *
 *  in:  PROATM device
 *  out:  zero = success
 */
static int32_t 
proatm_tst_init(PROATM *proatm)
{
    int32_t             j;
    u_int32_t          data;

    proatm->tst_free_entries = proatm->tst_num;
    for (j = 0; j < proatm->tst_num; j++)
        proatm_sram_wr (proatm, proatm->tst + j, 1, IDT_TST_OPCODE_VARIABLE, 0,0,0);

    data = IDT_TST_OPCODE_END | (proatm->tst<<2);
    proatm_sram_wr(proatm,  proatm->tst + proatm->tst_num, 1, data, 0,0,0);

    for (j = 0; j < proatm->tst_num; j++)
        proatm->tst_slot[j] = NULL;

    proatm_reg_wr (proatm, ( proatm->tst<<2), REGTSTB);

    return (0);
}

/*******************************************************************************
 *
 *  Initialize the Rate Tables
 *
 *  in:  PROATM device
 *  out:  zero = success
 */

static void 
proatm_rate_init(PROATM *proatm)
{
    u_int32_t *  abr_vbr_rate_tables;
    int32_t j;

    if (proatm->flg_25)
        abr_vbr_rate_tables = abr_vbr_rate_tables_25Mb;
    else
        abr_vbr_rate_tables = abr_vbr_rate_tables_155Mb;

    for (j = 0; j < abr_vbr_rate_tables_len; j++) {
        proatm_sram_wr (proatm, (proatm->rate + j), 1, abr_vbr_rate_tables[j],0,0,0);
    }
    proatm_reg_wr(proatm, proatm->rate << 2, REGRTBL);
}



/* Compute (scr<<16)/pcr */
static u_int16_t 
scr_on_pcr(u_int32_t scr, u_int32_t pcr)
{
    u_int32_t q = 0;
    
    scr <<= 8;
    q = scr/pcr;
    scr = (scr % pcr) << 8;
    return  (q << 8) | (scr/pcr) ;
}

/* translate rate to ATM Forum rate 
 * 2^e (1+m/512) = rate   => m = rate * 2^(9-e) - 512
 */
static u_int16_t 
u32_to_afr(u_int32_t rate)
{
	u_int16_t m, e;
	u_int32_t mask;

	if (rate == 0) return 0;
	for (mask = 0x80000000, e = 31; mask; mask >>= 1, e--)
		if (rate & mask) break;
    m = e >= 9 ? rate >> (e-9): rate << (9-e);
    m -= 512;

	return (1<<14 | e<<9 | m);
}

/* 
 * First transform rate to ATM fixed point format (afr)
 * Then find the index corresponding to afr into log_to_conv table
 */
static u_int8_t 
ns_rate2log (PROATM *proatm, u_int32_t rate)
{
	u_int16_t afr_rate = u32_to_afr(rate), afr_value;
	u_int32_t data;
	u_int8_t lower =0, upper =255, mid;
    u_int32_t * rate_table;

    if(proatm->flg_25)
		rate_table = abr_vbr_rate_tables_25Mb;
	else
		rate_table = abr_vbr_rate_tables_155Mb;
 
    /* We could use the rate2log table but this content research gives better precision */
	while (1) {
		mid = (u_int8_t) ((lower + upper) >> 1);
		data = rate_table [mid];
		afr_value = (u_int16_t) (data >> 17);  /* extract cps from table */

		if ((afr_rate == afr_value) || (upper <= lower))
			break;
        else if (afr_rate > afr_value)
			lower = mid + 1;
		else
			upper = mid - 1;		
	}
	if (afr_value > afr_rate)
		mid--;

	return (mid);
}


/*******************************************************************************
 *
 *  Write TCT entry
 *
 *  in:  PROATM device, connection descriptor, connection number
 *  out:  zero = success
 *
 */
static int32_t 
proatm_tct_wr (PROATM *proatm, int32_t conn)
{
    CONNECTION      *connection = &proatm->connection[conn];
    u_int32_t          w0, w1, w2, w3, w4, w5, w6, w7;
/*    u_int32_t             air_tbl, rdf_tbl, cdf_tbl;*/
    u_int16_t         pcr_token;

    switch (connection->class) {
    case NICUBR0:           
        w0 = IDT_TCTE_TYPE_UBR | proatm->scd_ubr0;
        w1 = 0x00000000;
        w2 = 0x00000000;
        w3 = (IDT_TCTE_HALT | IDT_TCTE_IDLE);
        w4 = 0x00000000;
        w5 = 0x00000000;
        w6 = 0x00000000;
        w7 = 0x80000000;
        break;

    case NICUBR:
        if (connection->traf_pcr == 0) {
            connection->init_er = 0xff;  /* max values if nothing specified */
            connection->lacr = 0xff;
        }
        else {
            connection->init_er = ns_rate2log (proatm, connection->traf_pcr);
	        connection->lacr    = ns_rate2log (proatm, connection->traf_scr);
        }
        w0 = (IDT_TCTE_TYPE_UBR | connection->queue->scd);
        w1 = 0x00000000;
        w2 = 0x00000000;
        w3 = (IDT_TCTE_HALT | IDT_TCTE_IDLE);
        w4 = 0x00000000;
        w5 = 0x00000000;
        w6 = 0x00000000;
        w7 = 0x80000000;
        break;

    case NICCBR:
        w0 = IDT_TCTE_TYPE_CBR | connection->queue->scd;
        w1 = 0x00000000;
        w2 = 0x00000000;
        w3 = 0x00000000;
        w4 = 0x00000000;
        w5 = 0x00000000;
        w6 = 0x00000000;
        w7 = 0x00000000;
        break;

    case NICVBR:
        pcr_token = scr_on_pcr (connection->traf_scr, connection->traf_pcr);
        if (pcr_token == 0) 
            pcr_token++;
        connection->init_er = ns_rate2log (proatm, connection->traf_pcr);
        connection->lacr    = ns_rate2log (proatm, connection->traf_scr);

        w0 = IDT_TCTE_TYPE_VBR | connection->queue->scd;
        w1 = 0x00000000;
        w2 = IDT_TCTE_TSIF;             /* interrupt when connection become idle*/
        w3 = IDT_TCTE_HALT | IDT_TCTE_IDLE;
        w4 = 0x7F<<24;
        w5 = 0x01<<24;
        w6 = (connection->traf_mbs << 16) | pcr_token;
        w7 = 0x00000000;
        break;

    case NICABR:
        /* 
         * We will add ABR as soon as FreeBSD provides support for it 
         */ 
               
    default:
        return -1;
    }
    proatm_sram_wr (proatm, conn*8,   4, w0,w1,w2,w3);
    proatm_sram_wr (proatm, conn*8+4, 4, w4,w5,w6,w7);
    return 0;
}


/*******************************************************************************
 *
 *  Open output queue for connection
 *
 *  in:  PROATM device, connection (class, traf_pcr, & traf_scr fields valid)
 * out:  zero = success
 *
 */

static int32_t
proatm_connect_txopen(PROATM *proatm, CONNECTION *connection)
{       
    TX_QUEUE        *txqueue;
    int32_t             cellrate = connection->traf_scr; /* use SCR instead of PCR */
    int32_t             cbr_slots;
    int32_t             s=0;
    int32_t             conn = connection->number;
    u_int8_t          class = connection->class;

    switch (class) {

    case NICCBR:
        cbr_slots = proatm_slots_cbr(proatm, cellrate);
        s = splimp();
        if ((cbr_slots + proatm->txslots_cur) > (proatm->tst_num - PROATM_TST_RESERVED) ||
            (proatm->txqueue_free_count < 1)) {
            splx(s);
            return 1;            /* requested rate not available */
        }
        proatm->txslots_cur += cbr_slots;
        proatm->cellrate_tcur += cellrate;
        proatm->txqueue_free_count--;
        txqueue = proatm->txqueue_free[proatm->txqueue_free_count];
        txqueue->rate = cellrate;
        connection->queue = txqueue;

        proatm_tct_wr (proatm, conn);      /* write TCT connection info */

        if (proatm_slots_add(proatm, txqueue, cbr_slots, conn)) {
            proatm->txslots_cur -= cbr_slots;            /* cannot add CBR slots */
            proatm->cellrate_tcur -= cellrate;
            proatm->txqueue_free [proatm->txqueue_free_count] = txqueue;
            proatm->txqueue_free_count++;
            splx(s);
            return 1;
        }
        splx(s);
        if (proatm_sysctl_logvcs)
            printf("proatm_connect_txopen: CBR connection for %d/%d\n",
                   connection->vpi, connection->vci);
        return 0;

    case NICUBR0:
                            /* UBR0 takes whatever is left over or setup for full speed */                                            
        s = splimp();
        if (proatm->txslots_cur == proatm->tst_num) {
            splx(s);
            return 1;       /* no more possibility for this rate */
        }
        proatm->txslots_cur++;
        connection->queue = &proatm->txqueue_ubr0;     /* single tx queue for all UBR0 connections */
        proatm_tct_wr (proatm, conn);             /* write TCT connection info */
        proatm_reg_wr(proatm, IDT_TCMDQ_UPD_INITER | 0xff<<16 | conn, REGTCMDQ);
        proatm_reg_wr(proatm, IDT_TCMDQ_START_ULACR | 0xff<<16 | conn, REGTCMDQ);
        splx(s);
        if (proatm_sysctl_logvcs)
            printf("proatm_connect_txopen: UBR with unspecified rate connection for %d/%d\n",
                   connection->vpi, connection->vci);          
        return 0;

    case NICVBR:
    case NICABR:
    case NICUBR:
        if (connection->flg_closing){
            printf("proatm%d: connection is still closing\n", proatm->unit);
            return 1;
        }

        s = splimp();
                            /* make sure connection is idle */
        if (proatm_connect_txstop (proatm, conn)) {
            splx(s);
            return 1;
        }

        if ((proatm->txslots_cur == proatm->tst_num) || proatm->txqueue_free_count < 1) {
            splx(s);
            return 1;       /* no more possibility for this rate */
        }

        proatm->txslots_cur++;
        proatm->txqueue_free_count--;
        txqueue = proatm->txqueue_free[proatm->txqueue_free_count];
        connection->queue = txqueue;
        txqueue->mget = NULL;
        txqueue->mput = NULL;
        txqueue->scq_next = txqueue->scq_base;
        txqueue->scq_last = txqueue->scq_next;
        txqueue->scq_cur = 0;
        txqueue->rate = cellrate;
                             /* init SCD */
        proatm_sram_wr(proatm, txqueue->scd, 4,
                         vtophys (txqueue->scq_base), 0, 0xffffffff, 0);
        proatm_sram_wr(proatm, txqueue->scd + 4, 4, 0, 0, 0, 0);
        proatm_sram_wr(proatm, txqueue->scd + 8, 4, 0, 0, 0, 0);

                           /* write TCT connection info */
        proatm_tct_wr (proatm, conn);
                           /* start scheduling */                                 
        proatm_reg_wr(proatm, IDT_TCMDQ_UPD_INITER | connection->init_er<<16 | conn, REGTCMDQ);
        proatm_reg_wr(proatm, IDT_TCMDQ_START_ULACR | connection->lacr <<16 | conn, REGTCMDQ);     
        connection->flg_active = 1;
          
        splx(s);
        if (proatm_sysctl_logvcs)
            printf("proatm%d: txopen %s connection for %d/%d\n", proatm->unit,
            class == NICUBR? "UBR": class == NICVBR? "VBR":"ABR", connection->vpi, connection->vci);

        return 0;

    }
    return (1);          /* unknown class */
}



/*******************************************************************************
 *
 *  Halt hardware connection
 *
 *  Called at splimp
 *
 *  in:  connection number
 * out:  zero = success
 *
 */

static int32_t proatm_connect_txstop (PROATM *proatm, int32_t conn)
{
    int32_t i;
    u_int32_t addr, w;

    addr = conn * IDT_TCT_ENTRY_SIZE;
    proatm_sram_rd (proatm, addr + 3, &w);
	if (!(w & (IDT_TCTE_IDLE | IDT_TCTE_HALT))) {
        proatm_reg_wr(proatm, IDT_TCMDQ_HALT | conn, REGTCMDQ);
        for (i=0; i<1000; i++) {              
            DELAY(50);
            proatm_sram_rd (proatm, addr + 3, &w);
            if (w & (IDT_TCTE_IDLE | IDT_TCTE_HALT))
                return 0;
        }
        if (i == 1000) {       
            printf ("proatm%d: timeout when halting transmission on connection %d\n",
                                                                   proatm->unit, conn);
            return 1;
        }
    }
    return 0;
}

/*******************************************************************************
 *
 *  Close connection output queue when empty.
 *
 *  Called at splimp by proatm_intr_tsq()
 *
 *  in:  PROATM device
 *
 */
static void proatm_connect_txclose_cb(PROATM *proatm, CONNECTION *connection)
{
    TX_QUEUE        *txqueue = connection->queue;
    int32_t             s, count = 0;
    u_int32_t          conn = connection->number;

    if (proatm_sysctl_logvcs)
        printf ("proatm%d: %d/%d closed\n", proatm->unit, connection->vpi, connection->vci);

    /*
     * All the UBR0 shares the same queue txqueue_ubr0. It means that txqueue_ubr0
     * cannot be set back to txqueue_free.
     */
    if (connection->class == NICUBR0) {
        printf ("proatm%d: "__FUNCTION__" called while VPI/VCI=%d/%d is UBR0\n", proatm->unit,
                connection->vpi, connection->vci);
        return;
    }

    if (connection->flg_closing == 0)
        return;

    if (proatm_check_vc (proatm, connection->vpi, connection->vci))
        return;

    proatm->pu_stats.proatm_st_drv.drv_xm_closing--;

    proatm_queue_flush(connection, &count);     /* free mbufs remaining in transmit queue*/
    if (count)
        printf("proatm%d: %d PDUs have been trashed on UBR/VBR close\n", 
                            proatm->unit, count);
    s = splimp();
    if (txqueue != NULL) {
        proatm->txqueue_free[proatm->txqueue_free_count] = txqueue;
        proatm->txqueue_free_count++;
    }
    proatm->txslots_cur--;
    connection->queue = NULL;
    connection->recv = NULL;
    connection->rlen = 0;
    connection->maxpdu = 0;
    connection->aal = 0;
    connection->traf_pcr = 0;
    connection->traf_scr = 0;
    connection->traf_mbs = 0;
    connection->flg_closing = 0;
    connection->flg_open = 0;
    connection->flg_clp = 0;
    proatm_connect_txstop (proatm, conn);
    splx (s);
    
}

/*******************************************************************************
 *
 *  Close connection output queue
 *
 *  in:  PROATM device, connection (class, traf_pcr, & traf_scr fields valid)
 * out:  zero = success
 *
 */
static int32_t
proatm_connect_txclose(PROATM *proatm, CONNECTION *connection, int32_t force)
{
    TX_QUEUE        *txqueue;
    int32_t             slots;
    int32_t             s, count = 0;
    u_int32_t          conn = connection->number;
    u_int32_t          addr = conn * IDT_TCT_ENTRY_SIZE;
    struct mbuf     *m;

    if (proatm_check_vc(proatm, connection->vpi, connection->vci))
        return 1;

    txqueue = connection->queue;
    if (proatm_sysctl_logvcs) 
        printf("proatm%d: closing %d/%d\n", proatm->unit, connection->vpi, connection->vci);
    
    switch (connection->class) {
    case NICCBR:
        slots = proatm_slots_rem(proatm, txqueue);  /* remove this queue from CBR slots */
        proatm_queue_flush(connection, &count);     /* free mbufs remaining in transmit queue*/
        if (count)
            printf("proatm%d: %d PDUs have been trashed on CBR close on VPI/VCI=%d/%d\n",
                   proatm->unit, count, connection->vpi, connection->vci);
        s = splimp();
        proatm->txslots_cur -= slots;
        proatm->cellrate_tcur -= connection->traf_scr;
        if (txqueue != NULL) {                                                     
            proatm->txqueue_free[proatm->txqueue_free_count] = txqueue;
            proatm->txqueue_free_count++;
        }

        connection->queue = NULL;
        connection->flg_open = 0;
        connection->recv = NULL;
        connection->rlen = 0;
        connection->maxpdu = 0;
        connection->aal = 0;
        connection->traf_pcr = 0;
        connection->traf_scr = 0;
        connection->traf_mbs = 0;
        splx(s);
        return 0;

    case NICUBR:
    case NICVBR:
        if (!force) {
            s = splimp();
            connection->flg_closing = 1;    /* blocks open() while closing */
            /* 
             * Do nothing here. Just post a special end of transmission mbuf 
             */

            MGETHDR(m, M_WAIT, MT_DATA);
            if (m == NULL) {
                splx(s);        /* thanks to Vincent Jardin */
                return (1);
            }

            /* 
             * Delay the call to proatm_connect_txstop (proatm, conn).
             * proatm_connect_txclose_cb will do the job.
             */
            proatm->pu_stats.proatm_st_drv.drv_xm_closing++;
            m->m_pkthdr.header = PROATM_CLOSE_CONNECTION;
            m->m_pkthdr.len = 0;
            m->m_len = 4;                           /* fake length for proatm_transmit_top */
            proatm_queue_put (connection, m);
            proatm_transmit_top (proatm, txqueue);
            splx (s);
        }
        else {
            /*
             * Flush everything in case of detach. Don't care if packets may be lost.
             */
            connection->flg_closing = 0;
            proatm_connect_txclose_cb (proatm, connection);            
        }
        return 0;

    case NICUBR0:
        proatm_queue_flush(connection, &count);     /* free mbufs remaining in transmit queue*/
        if (count)
            printf("proatm%d: %d PDUs have been trashed on UBR0 close on VPI/VCI=%d/%d\n",
                   proatm->unit, count, connection->vpi, connection->vci);
        s = splimp();
        proatm->txslots_cur--;
        connection->queue = NULL;
        connection->flg_open = 0;
        connection->recv = NULL;
        connection->rlen = 0;
        connection->maxpdu = 0;
        connection->aal = 0;
        connection->traf_pcr = 0;
        connection->traf_scr = 0;
        connection->traf_mbs = 0;
                                    /* halt hardware connection*/
        proatm_connect_txstop (proatm, conn);
                                           /* Clear TCT entry */
        proatm_sram_wr (proatm, addr,   4, 0,0,0,0);
        proatm_sram_wr (proatm, addr+4, 4, 0,0,0,0);
        splx (s);

        return 0;

    default:
        return 1;                          /* unsupported class */

    }
/*  print_proatm_log (); */
}


/*******************************************************************************
 *
 *  Get large buffer from kernel pool
 *
 * out:  mbuf, NULL = error
 *
 */

static struct mbuf 
*proatm_mbufcl_get(void)
{       
    struct mbuf     *m;

    MGETHDR(m, M_DONTWAIT, MT_DATA);
    if (m == NULL)
        return (NULL);

    MCLGET(m, M_DONTWAIT);
    if (m->m_flags & M_EXT)
        return (m);

    m_freem(m);
    return (NULL);
}

/*******************************************************************************
 *
 *  Initialize connection table
 *
 *  in:  PROATM, number of VPI bits (0, 1, or 2)
 *  out:  zero = success
 *
 */

static int32_t 
proatm_connect_init(PROATM *proatm)
{       
    CONNECTION      *connection;
    int32_t             pages;
    int32_t             vpi;
    int32_t             vci;

    proatm->conn_maxvpi = 1 << proatm->vpibits;
    proatm->conn_maxvci = proatm->max_connection / proatm->conn_maxvpi;
      
    pages = (sizeof(CONNECTION) * proatm->max_connection) + PAGE_SIZE - 1;
    pages /= PAGE_SIZE;
    proatm->connection = (CONNECTION *) proatm_malloc_contig (pages * PAGE_SIZE); 
    if (proatm->connection == NULL)
        return (ENOMEM);

    for (vpi = 0; vpi < proatm->conn_maxvpi; vpi++)
        for (vci = 0; vci < proatm->conn_maxvci; vci++) {
            connection = &proatm->connection[vpi *proatm->conn_maxvci + vci];
            connection->vccinf = NULL;                /* may want to change to "unclaimed" */
            connection->flg_open = 0;                  /* closed */
            connection->vpi = vpi;
            connection->vci = vci;
            connection->queue = NULL;                 /* no current TX queue */
            connection->recv = NULL;                  /* no current receive mbuf */
            connection->rlen = 0;
            connection->maxpdu = 0;
            connection->traf_pcr = 0;
            connection->traf_scr = 0;
            connection->aal = 0;
            connection->class = 0;
            connection->number = (vpi << proatm->vcibits) | vci;
            connection->flg_clp = 0;
            connection->flg_active = 0;
            connection->flg_closing = 0;
        }
    return (0);
}

/*******************************************************************************
 *
 *  Look up a connection
 *
 *  in:  PROATM, vpi, vci
 * out:  connection number, 0=invalid vpi/vci
 *
 */

static CONNECTION *
proatm_connect_find(PROATM *proatm, int32_t vpi, int32_t vci)
{ 
    int32_t conn;      
    vpi &= proatm->conn_maxvpi - 1;
    vci &= proatm->conn_maxvci - 1;
 
    conn =  vpi <<proatm->vcibits | vci;
    return  &(proatm->connection[conn]);

}
/*******************************************************************************
 *
 *  Check VPI and VCI
 *
 *  in:  PROATM, vpi, vci
 * out:  0= OK, 1=invalid vpi/vci
 *
 */

static int32_t proatm_check_vc (PROATM *proatm, u_int8_t vpi, ushort vci)
{   
    u_int32_t mask;
    u_int8_t vp = vpi;
    u_int16_t vc = vci;   
    mask = 0xffff >> proatm->vcibits;
    
    /* vci check */        
    if ((vc >> proatm->vcibits) != (proatm->vpm & mask)) {
		printf  ("proatm%d: vci %d out of bounds (max vpi= %d, max vci= %d, mask = 0x%x)\n", 
            proatm->unit, (int32_t)vci, (int32_t)proatm->conn_maxvpi, 
                            (int32_t)proatm->conn_maxvci, (int32_t)proatm->vpm);
        return 1;
    }
    /* vpi check */      
    if ((vp >> proatm->vpibits) != (proatm->vpm >> (16 - proatm->vcibits))) {
 		printf ("proatm%d: vpi %d out of bounds (max vpi= %d, max vci= %d, mask = 0x%x)\n", 
            proatm->unit, (int32_t)vpi, (int32_t)proatm->conn_maxvpi, 
                            (int32_t)proatm->conn_maxvci, (int32_t)proatm->vpm);
        return 1;
    }
    return 0;
}    

     

/*******************************************************************************
 *
 *  Get current base of data storage
 *
 *  in:  mbuf
 * out:  base
 *
 */

static caddr_t 
proatm_mbuf_base(struct mbuf *m)
{
    if (m == NULL)
        return (NULL);

    if (m->m_flags & M_EXT)                                                      
        return (m->m_ext.ext_buf);                    /* external storage */

    if (m->m_flags & M_PKTHDR)                                                      
        return (m->m_pktdat);                         /* internal storage, packet header */

    return (m->m_dat);                                /* internal storage, no packet header */
}


/*******************************************************************************
 *
 *  Put mbuf chain on transmit queue
 *
 *  Note on mbuf usage in the transmit queue:
 *
 *  m_pkthdr.rcvif       Connection pointer (set by proatm_queue_put)
 *  m_pkthdr.len         Length of PDU
 *  m_pkthdr.header      NULL for normal packet, 1 if end of transmission packet
 *  m_pkthdr.csum_flags  Unused, keep zero	(does not apply to 3.5 version)
 *  m_pkthdr.csum_data   unused, keep zero  (does not apply to 3.5 version)
 *  m_pkthdr.aux         Unused, keep NULL
 *
 *  This mbuf is m_freem(ed) when a TSQ interrupt is catched
 *  and processed by proatm_process_tsr()
 *
 *  in:  PROATM device, mbuf chain, vpi, vci, flags (2 MPEG2 TS == 8 AAL5 cells)
 *  out:  (nothing)
 *
 */

static void 
proatm_transmit(PROATM *proatm, struct mbuf *mfirst, int32_t vpi, int32_t vci)
{       
    CONNECTION      *connection = proatm_connect_find (proatm, vpi, vci);
    struct mbuf     *m, *top = mfirst;
    struct mbuf     **pnext = &top;
    int32_t             ntbd = 0;

    if (connection == NULL) {
        XPRINT ("proatm_transmit: this VPI/VCI is not open\n");
        proatm_transmit_drop(proatm, connection, mfirst);         /* this VPI/VCI is not open */
        return;
    }
    if (connection->queue == NULL) {
        XPRINT ("proatm_transmit: queue = NULL\n");
        proatm_transmit_drop(proatm, connection, mfirst);
        return;
    } 
                        /* Sanity check to suppress null-length buffers */ 
    do {
        m = *pnext;
        if (m->m_len == 0) {
            *pnext = m_free(m);
        } else {
            pnext = &m->m_next;
            ntbd++;
        }
    } 
    while (*pnext);

    if (ntbd == 0)
        return;         /* empty packet */

    if (proatm_queue_put(connection, top))          /* put packet on TX queue */                                                    
        printf("proatm%d: Cannot queue packet for %d/%d.\n", proatm->unit, vpi, vci);
  
    /* Put on SCQ if the TX queue was empty */
    if (connection->queue->mget == top)                                                          
        proatm_transmit_top(proatm, connection->queue);     

}

/*******************************************************************************
 *
 *  Drop transmit mbuf chain and update counters
 *
 *  in:  PROATM device, CONNECTION if any, mbuf chain
 * out:  (nothing)
 *
 */

static void 
proatm_transmit_drop (PROATM *proatm, CONNECTION *connection, struct mbuf *mfirst)
{       
    struct mbuf     *next;
    int32_t             mesglen;

    mesglen = 0;
    while (mfirst != NULL) {
        mesglen += mfirst->m_len;
        next = m_free(mfirst);
        mfirst = next;
    }
    printf("proatm%d: dropping transmit packet, size=%d\n", proatm->unit, mesglen);
    proatm->stats_oerrors++;                             
    if (connection) {
        connection->vccinf->vc_oerrors++;
        switch (connection->aal) {
        case IDTAAL5:   
            proatm->pu_stats.proatm_st_aal5.aal5_pdu_drops++;
            proatm->pu_stats.proatm_st_aal5.aal5_drops += (mesglen+47) / BYTES_PER_CELL;
            break;
        case IDTAAL3_4:
            proatm->pu_stats.proatm_st_aal4.aal4_pdu_drops++;
            proatm->pu_stats.proatm_st_aal4.aal4_drops += (mesglen+47) / BYTES_PER_CELL;
            break;
        case IDTAAL0:
            proatm->pu_stats.proatm_st_aal0.aal0_drops++;
            break;
        default:
            printf("proatm%d: bad AAL for %d/%d\n", proatm->unit,
                   connection->vpi, connection->vci);
            break;
        }
    }
}

/*******************************************************************************
 *
 *  Put mbuf chain on SCQ
 *
 *  in:  PROATM device, TX_QUEUE
 * out:  (nothing)
 *
 */


static void 
proatm_transmit_top (PROATM *proatm, TX_QUEUE *txqueue)
{
    CONNECTION      *connection = NULL;
    struct mbuf     *top, *m;
    int32_t             val1, val4;
    int32_t             tlen;
    int32_t             vci, vpi;
    int32_t             scq_space, scq_pending;
    int32_t             count, s, ntbd, i, conn = 0;

    
    if (txqueue == NULL) {
        printf ("proatm%d: error: txqueue = NULL\n", proatm->unit);
        return;
    }

    s = splimp();
    /*  
     * Now we can add the queue entries for the PDUs 
     */
    scq_space = IDT_SCQ_ENTRIES - txqueue->scq_cur;
    count = 0;
    while (1) {
        top = txqueue->mget;            /* read the PDU pointer */
        if (top == NULL)
            break;                      /* no more PDU in the TX queue */

        /* 
         * First pass to calculate the total PDU length and TBD number.
         * Empty packets have been filtered by proatm_transmit ()
         */                
        for (m = top, ntbd = 0, tlen = 0; m != NULL; m = m->m_next) {
            tlen += m->m_len;
            ntbd++;
        }

        /* 
         * Exit if no more room for the PDU. It will be processed later 
         */
        if (scq_space < ntbd + 1)
            break;
        /* 
         * Now extract this PDU from txqueue 
         */
        top = proatm_queue_get(txqueue);
        if (!KB_ISPKT(top))
            panic(__FUNCTION__" missing M_PKTHDR");

        connection = (CONNECTION *) top->m_pkthdr.rcvif;

        if (top->m_pkthdr.header == PROATM_CLOSE_CONNECTION) {
            /* 
             * Insert a Transmit Status Request with interruption
             * this is a special end-of-connection mbuf
             * we must insert a special end-of-connection TSR
             */
            *txqueue->scq_next++ = IDT_TSR | IDT_TBD_TSIF | (PROATM_CLOSE_TAG<<20);
            *txqueue->scq_next++ = (u_int32_t) connection;
            *txqueue->scq_next++ = 0;
            *txqueue->scq_next++ = 0;
            if (txqueue->scq_next >= txqueue->scq_base + (IDT_SCQ_ENTRIES*4))
                txqueue->scq_next = txqueue->scq_base;
            top->m_pkthdr.header = NULL;
            top->m_pkthdr.len = 0;
            m_free (top);
            txqueue->scq_cur++;
            count++;
            break;
        }             

        vpi = connection->vpi;
        vci = connection->vci;
        conn =  connection->number;

        switch (connection->aal) {
            case IDTAAL5:   
                val1 = IDT_TBD_AAL5;  
                break;
            case IDTAAL3_4:
                printf("proatm%d: AAL3/4 is not supported\n", proatm->unit);
                m_freem(top);
                proatm->pu_stats.proatm_st_aal4.aal4_pdu_drops++;
                proatm->pu_stats.proatm_st_aal4.aal4_drops += (tlen+47) / BYTES_PER_CELL;
                connection->vccinf->vc_oerrors++;
                continue; /* process the next PDU */
            case IDTAAL0:
                printf("proatm%d: AAL0 is not supported\n", proatm->unit);
                m_freem(top);
                proatm->pu_stats.proatm_st_aal0.aal0_drops += (tlen+47) / BYTES_PER_CELL;
                connection->vccinf->vc_oerrors++;
                continue; /* process the next PDU */
            default:
                printf("proatm%d: bad AAL for %d/%d\n", proatm->unit, vpi, vci);
                m_freem(top);
                connection->vccinf->vc_oerrors++;
                continue; /* process the next PDU */
        }

        val4 = ((vpi << 20) & IDT_TBD_VPI_MASK) | ((vci << 4) & IDT_TBD_VCI_MASK);
        if (connection->flg_clp)
            val4 |= IDT_TBD_LOW_PRIORITY;
                                                           
        for (i=ntbd, m=top; i>0; i--, m=m->m_next) {
            if (i == 1) 
                *txqueue->scq_next++ = IDT_TBD_EOPDU | val1 | m->m_len;
            else
                *txqueue->scq_next++ = val1 | m->m_len;

            *txqueue->scq_next++ = vtophys(m->m_data);
            *txqueue->scq_next++ = tlen;
            *txqueue->scq_next++ = val4;
            if (txqueue->scq_next >= txqueue->scq_base + (IDT_SCQ_ENTRIES*4))
                txqueue->scq_next = txqueue->scq_base;
        }

       /*
        * Increment ATM stats
        */
        proatm->pu_stats.proatm_st_atm.atm_xmit += (tlen+47) / BYTES_PER_CELL;

       /*
        * Increment AAL stats
        */
        switch (connection->aal) {
            case IDTAAL5:
                proatm->pu_stats.proatm_st_aal5.aal5_pdu_xmit++;
                proatm->pu_stats.proatm_st_aal5.aal5_xmit += (tlen+47) / BYTES_PER_CELL;
                break;
            case IDTAAL3_4:
                proatm->pu_stats.proatm_st_aal4.aal4_pdu_xmit++;
                proatm->pu_stats.proatm_st_aal4.aal4_xmit += (tlen+47) / BYTES_PER_CELL;
                break;
            case IDTAAL0:
                proatm->pu_stats.proatm_st_aal0.aal0_xmit += (tlen+47) / BYTES_PER_CELL;
                break;
        }

       /*
        * Count the PDU stats for this interface.
        */
        proatm->stats_opdus++;               
        proatm->stats_obytes += tlen;      

       /*
        * Counts the stats for this VC.
        */
        connection->vccinf->vc_opdus++;
        connection->vccinf->vc_obytes += tlen;

        /*  
         * Insert a Transmit Status Request with interrupt
         */
        *txqueue->scq_next++ = IDT_TSR | IDT_TBD_TSIF | (PROATM_PDU_TAG<<20);
        *txqueue->scq_next++ = (u_int32_t) top;
        *txqueue->scq_next++ = 0;
        *txqueue->scq_next++ = 0;
        if (txqueue->scq_next >= txqueue->scq_base + (IDT_SCQ_ENTRIES*4))
            txqueue->scq_next = txqueue->scq_base;

        ntbd++;
        top->m_pkthdr.header = NULL;
        top->m_pkthdr.len = tlen;
        scq_space -= ntbd;
        txqueue->scq_cur += ntbd;
        count += ntbd;
    }

    /*
     * Stats about the number of TBDs that have been added
     * to TX_QUEUE->scq_next
     */
    proatm->pu_stats.proatm_st_drv.drv_xm_ntbd += count;

    /*
     * Calculate the number of pending TBDs
     */
    scq_pending = (txqueue->scq_next - txqueue->scq_last)/4;
    if(scq_pending < 0)
        scq_pending += IDT_SCQ_ENTRIES;
    /*
     * We don't update the SCD if less than half pending and queue is active
     */
    if(scq_pending < IDT_SCQ_ENTRIES/2 &&
        (txqueue->scq_cur - scq_pending) > 6) {
        XPRINT("block:    txqueue->scq_cur= %d, scq_pending= %d\n",
                        txqueue->scq_cur, scq_pending);
    }
    else {
        XPRINT("transmit: txqueue->scq_cur= %d, scq_pending= %d\n",
                        txqueue->scq_cur, scq_pending);

        /*
         * If PDUs have been added to the SCQ, give information to the SAR and
         * restart the connection if needed.
         */
#if XDEBUG
        if (count){
            u_int32_t *wp;

            /*
             * Dump the SCQ entries.
             */
            XPRINT("Add %d SCQEs from 0x%x,\n", count,
                   (unsigned int32_t)txqueue->scq_last);
            wp = txqueue->scq_last;
            while ( wp != txqueue->scq_next) {
                XPRINT("0x%x  0x%x  0x%x  0x%x \n", *wp, *(wp+1), *(wp+2), *(wp+3));
                wp += 4;
                if (wp >= txqueue->scq_base + (IDT_SCQ_ENTRIES*4))
                    wp = txqueue->scq_base;
            }
        }
#endif
        XPRINT("Write 0x%x to SCD 0x%x\n", vtophys(txqueue->scq_next), txqueue->scd);

        /* 
         * we need to update the SCD queue pointer
         */
         proatm_sram_wr(proatm, txqueue->scd, 1, vtophys(txqueue->scq_next), 0, 0, 0);

        /*
         * restart VBR connection if idle
         * we don't need to do that in the loop because in VBR there can be only
         * one connection per queue
         */
        if (connection) {
            if ((connection->class == NICVBR) && !connection->flg_active){
                proatm_reg_wr(proatm, IDT_TCMDQ_START | 0xff<<16 | conn, REGTCMDQ);
                connection->flg_active = 1;
                proatm->pu_stats.proatm_st_drv.drv_xm_idlevbr--;
            }
        }

        if (conn) {
            XPRINT("SCD 0x%x\n", txqueue->scd);
            XPRINT("0x%x 0x%x 0x%x 0x%x\n", nicsr(proatm, txqueue->scd),
                                                nicsr(proatm, txqueue->scd + 1),
                                                nicsr(proatm, txqueue->scd + 2),
                                                nicsr(proatm, txqueue->scd + 3));
            XPRINT("0x%x 0x%x 0x%x 0x%x\n", nicsr(proatm, txqueue->scd + 4),
                                                nicsr(proatm, txqueue->scd + 5),
                                                nicsr(proatm, txqueue->scd + 6),
                                                nicsr(proatm, txqueue->scd + 7));
            XPRINT("0x%x 0x%x 0x%x 0x%x\n", nicsr(proatm, txqueue->scd + 8),
                                                nicsr(proatm, txqueue->scd + 9),
                                                nicsr(proatm, txqueue->scd + 10),
                                                nicsr(proatm, txqueue->scd + 11));
            XPRINT("TCT, connection = %d:\n", conn);
            XPRINT("0x%x 0x%x 0x%x 0x%x\n", nicsr(proatm, conn*8),
                                        nicsr(proatm, conn*8 + 1),
                                        nicsr(proatm, conn*8 + 2),
                                        nicsr(proatm, conn*8 + 3));
            XPRINT("0x%x 0x%x 0x%x 0x%x\n", nicsr(proatm, conn*8 + 4),
                                        nicsr(proatm, conn*8 + 5),
                                        nicsr(proatm, conn*8 + 6),
                                        nicsr(proatm, conn*8 + 7));
        }

        txqueue->scq_last = txqueue->scq_next;
    }
    splx(s);
    return;
}

/******************************************************************************
 *
 *  Handle a TSR.
 *  m_freem the mbuf that had been sent by proatm_transmit.
 *
 *  Called at splimp() by proatm_intr_tsq()
 *
 *  in:  PROATM device, TSR pointer
 *
 */
static void proatm_process_tsr (PROATM *proatm, u_int32_t* tsr_ptr)
{
    struct mbuf     *m, *mw;
    int32_t             ntbd;
    TX_QUEUE        *txqueue;
    CONNECTION      *connection;

    m = (struct mbuf *) tsr_ptr[0];    /* top of chain */

    if (m != NULL) {
       /*
        * look for TBD number since we cannot store it inside
        * m_pkthdr.csum_data to be compatible with kernel version 3.5
        */
        for (mw = m, ntbd = 1; mw != NULL; mw = mw->m_next)
            ntbd++;

        if (!KB_ISPKT(m))
            panic(__FUNCTION__" missing M_PKTHDR");

        connection = (CONNECTION *) m->m_pkthdr.rcvif;
        if (connection == NULL) {
            printf ("proatm%d: Error, NULL connection\n", proatm->unit);
        }
        else {
            txqueue = connection->queue;
            if (txqueue == NULL) 
                printf ("proatm%d: Error, NULL txqueue\n", proatm->unit);
            else { 
                /* s = splimp() set by the caller */
                txqueue->scq_cur -= ntbd;
                if (txqueue->scq_cur < 0 || txqueue->scq_cur > IDT_SCQ_ENTRIES)
                    printf("proatm%d: DANGER! scq_cur is %d\n", 
                                        proatm->unit, IDT_SCQ_ENTRIES);

                proatm_transmit_top (proatm, txqueue);       /* move more into SCQ */           
                /* splx(x) */
            }
        }
        m->m_pkthdr.header = NULL;
        m_freem(m);	/* the one provided to proatm_transmit() */

    }
    else
        printf ("proatm%d: Error, NULL pointer in TSQE\n", proatm->unit);
}


/******************************************************************************
 *
 *  Handle entries in Transmit Status Queue (end of PDU interrupt or TSQ full)
 *
 *  in:  PROATM device
 *
 */
static void  
proatm_intr_tsq (PROATM *proatm)
{
    CONNECTION      *connection;
    u_int32_t          *tsq_ptr;
    u_int32_t          val;
    int32_t             count, s;

    s = splimp();
    tsq_ptr = proatm->tsq_head;
    count = 0;

    while ((tsq_ptr[1] & IDT_TSI_EMPTY) == 0) {
        /* This should never occur */
        if (count > (PROATM_TSQ_SIZE / 8)) {
            printf("proatm%d: TSQ Buffer Overflow (count=%d)\n",
                   proatm->unit, count);
            break;
        }
        XPRINT("Processed TSQE= 0x%x 0x%x\n", tsq_ptr[0], tsq_ptr[1]);

        switch (tsq_ptr[1] & IDT_TSI_TYPE_MASK) {
				
          case IDT_TSI_TYPE_TSR: {
            if ((tsq_ptr[1] & IDT_TSI_TAG_MASK) == 0) {
                int32_t i =0;
                while ((tsq_ptr[1] & IDT_TSI_TAG_MASK) == 0 && i< 1000) {
                    DELAY(5);    /* race condition */
                    i++;
                }
                if (i == 1000)
                    printf ("proatm%d, TSI update error\n", proatm->unit);
            }

            if ((tsq_ptr[1] & IDT_TSI_TAG_MASK) == (PROATM_CLOSE_TAG<<24)) {
                                    /* we must close this connection */
                proatm_connect_txclose_cb (proatm, (CONNECTION *)tsq_ptr[0]);
            } 
            else if ((tsq_ptr[1] & IDT_TSI_TAG_MASK) == (PROATM_PDU_TAG<<24)){
                    /* we must free the TBDs and the mbuf chain related to this TSR*/
                proatm_process_tsr (proatm, tsq_ptr);
            }
            else {
                printf ("proatm%d: Unknown TSR: 0x%x 0x%x\n", proatm->unit, 
                        (u_int32_t)tsq_ptr[0], (u_int32_t)tsq_ptr[1]);
            }
          }
          break;

          case IDT_TSI_TYPE_IDLE: {
            int32_t conn = tsq_ptr[0] & 0x3fff;
            if (conn < proatm->max_connection) {
                connection = &proatm->connection[conn];
                if (!connection->flg_active)
                    printf ("proatm%d: Double idle of VPI=%d/VCI=%d\n",
                            proatm->unit, connection->vpi, connection->vci);
                connection->flg_active = 0;
                if (connection->class == NICVBR)
                    proatm->pu_stats.proatm_st_drv.drv_xm_idlevbr++;
            }
          }
          break;

          case IDT_TSI_TYPE_TMROF:
            XPRINT ("proatm%d: TSI Timer Overflow\n", proatm->unit);
            break;

          case IDT_TSI_TYPE_TBD:
            printf ("proatm%d: TSI TBD\n", proatm->unit);
            break;

          default:
            printf ("proatm%d: Unknown TSI type\n", proatm->unit);
            break;
        }

        tsq_ptr[0] = 0;
        tsq_ptr[1] = IDT_TSI_EMPTY;                     /* reset TSQ entry */
        tsq_ptr += 2;
        if (tsq_ptr >= proatm->tsq_base + proatm->tsq_size *2)
            tsq_ptr = proatm->tsq_base;

        count++;
    }
    proatm->tsq_head = tsq_ptr;
    if (count) {
        val = (int32_t) tsq_ptr - (int32_t) proatm->tsq_base;
        val -= 8;                                     /* always stay one behind */
        val &= 0x001ffc;
        proatm_reg_wr (proatm, val, REGTSQH);
    }
    splx(s);
}

/******************************************************************************
 *
 *    proatm_itrx ( card )
 *
 *    service error in transmitting PDU interrupt.
 *
*/
static void
proatm_itrx(proatm_reg_t *proatm)
{       
    /* trace mbuf and release */
    proatm->pu_stats.proatm_st_drv.drv_xm_txicp++;
    proatm->stats_oerrors++;
    /* XXX: How should the mbuf be released ?? */
}




/******************************************************************************
 *
 *  Raw cell process
 *
 *    Do something with raw cell 
 *
 */
void proatm_process_rcqe (proatm_reg_t *proatm, rcqe *rawcell)
{
    struct mbuf *m;

    MGETHDR(m, M_DONTWAIT, MT_DATA);
    if (!m) {
        /* out of mbuf */
        proatm->pu_stats.proatm_st_drv.drv_rv_rnobufs++;
        proatm->stats_ierrors++;
        return;
    }

    m->m_len = BYTES_PER_CELL + 5;          /* 53 */
    m->m_pkthdr.len = BYTES_PER_CELL + 5;   /* 53 */
    m->m_pkthdr.rcvif = NULL;
    m->m_data = (caddr_t) rawcell; /* XXX */

    /* XXX: m_data must not be freeed */

    proatm->pu_stats.proatm_st_atm.atm_rcvd++;

    /* consume */
    m_freem(m); /* else process it */
    XPRINT ("proatm_process_rcqe: nothing to do with raw cells for now\n");
}


/******************************************************************************
 *
 *  Raw cell receive interrupt
 *
 *    service raw cell reception interrupt.
 *
 */

static void 
proatm_rawc(proatm_reg_t *proatm)
{       
    rcqe            *rawcell;
    u_int32_t          tail;

    if (proatm->raw_headm == NULL ||
        proatm->raw_headp == 0) {
        printf("proatm%d: RAW cell received, buffers not ready (%x/%x.\n", 
               proatm->unit, (int32_t) proatm->raw_headm, (int32_t) proatm->raw_headp);
        proatm->pu_stats.proatm_st_drv.drv_rv_rnotrdy++;
        proatm->stats_ierrors++;
        return;
    }
    if (proatm->raw_hnd[0] == 0)
        return;

    tail = proatm->raw_hnd[0] + IDT_RCQE_SIZE;
     
    while (vtophys(proatm->raw_ch) !=  tail) {
        rawcell = proatm->raw_ch;
        if (rawcell->word_2 != 0) {
                                        /* last cell */
            struct mbuf *oldbuf = proatm->raw_headm;
            proatm->raw_headm = (struct mbuf *)rawcell->word_2;
            proatm->raw_ch = (rcqe *)(proatm->raw_headm->m_data);
            proatm->raw_headp = rawcell->word_1;
            proatm_mcheck_rem(proatm, oldbuf);
            m_free (oldbuf);
        }     
        else {
                  /* this is not the last buffer cell, so it is not empty */
        proatm_process_rcqe (proatm, rawcell);
        proatm->raw_ch++;
        }
    }    
}

/*****************************************************************************
 *
 *  Handle AAL5 PDU length
 *
 *  in:  PROATM device, first mbuf in chain, last mbuf
 * out:  zero = success, nz = failure (mbuf chain freed)
 *
 */

static int32_t 
proatm_receive_aal5(PROATM *proatm, struct mbuf *mfirst, struct mbuf *mdata)
{       
    struct mbuf     *m2;
    unsigned char   *aal5len;
    int32_t             plen;
    int32_t             diff;

    aal5len = mdata->m_data + mdata->m_len - 6;       /* aal5 length = 16 bits */
    plen = aal5len[0] *256 + aal5len[1];
    diff = mfirst->m_pkthdr.len - plen;               /* number of bytes to trim */

    if (diff == 0)
        return (0);

    if (diff < 0) {
        printf("proatm%d: AAL5 PDU length (%d) greater than cells (%d), discarding\n", 
               proatm->unit, plen, mfirst->m_pkthdr.len);
        m_freem(mfirst);
        return (1);
    }
    while (mdata->m_len < diff) {
                                                      /* last mbuf not big enough */
        diff -= mdata->m_len;
        m2 = mdata;
        m_free(mdata);
        if (mdata == mfirst) {
                                                      /* we just tossed the whole PDU */
            printf("proatm%d: AAL5 PDU length failed, discarding.\n", proatm->unit);
            return (1);                               /* the packetheadr length was bad! */
        }
        for (mdata = mfirst; mdata->m_next != m2; mdata = mdata->m_next);
        mdata->m_next = NULL;                         /* remove old link to free'd mbuf */
    }
    mdata->m_len -= diff;                             /* trim last mbuf */
    mfirst->m_pkthdr.len = plen;
    return (0);
}

/*****************************************************************************
 *
 *    proatm_recv ( card )
 *
 *    rebuilds PDUs from entries in the Receive Status Queue.
 *
 */

struct rsq_entry {
    u_int32_t          vpivci;
    struct mbuf     *mdata;
    u_int32_t          crc;
    u_int32_t          flags;
    }
;

static void 
proatm_recv(proatm_reg_t *proatm)
{       
    CONNECTION      *connection=NULL;
    struct rsq_entry *rsq;
    struct mbuf     *mdata, *mptr;
    u_int32_t          flags;
    u_int32_t          crc;
    int32_t             vpi;
    int32_t             vci;
    int32_t             clen;
    int32_t             x, s;

    s = splimp();
    rsq = (struct rsq_entry *)(proatm->fixbuf + PROATM_RSQ_OFFSET + (proatm->rsqh & 0x1ffc));
    if ((rsq->flags & IDT_RSQE_VALID) == 0) {
        splx(s);
        return;
    }
    while (rsq->flags & IDT_RSQE_VALID) {
        vpi = rsq->vpivci >> 16;                      /* first, grab the RSQ data */
        vci = rsq->vpivci & 0xffff;
        mdata = rsq->mdata;
        crc = rsq->crc;
        flags = rsq->flags;
        clen = (flags & 0x1ff) * 48;
        rsq->vpivci = 0;                              /* now recycle the RSQ entry */
        rsq->mdata = NULL;
        rsq->crc = 0;
        rsq->flags = 0;                               /* turn off valid bit */
        rsq++;
        if (rsq == (struct rsq_entry *)(proatm->fixbuf + PROATM_RSQ_OFFSET + PROATM_RSQ_SIZE))
            rsq = (struct rsq_entry *)(proatm->fixbuf + PROATM_RSQ_OFFSET);

        proatm_mcheck_rem(proatm, mdata);
        connection = proatm_connect_find (proatm, vpi, vci);
        if (connection == NULL) {
                                                      /* we don't want this PDU */
            printf("proatm%d: no connection %d/%d - discarding packet.\n", 
                   proatm->unit, vpi, vci);
            proatm->pu_stats.proatm_st_drv.drv_rv_nocx++;
            proatm->stats_ierrors++;
            m_free(mdata);                            /* throw mbuf away */
            continue;
        }
        mdata->m_len = clen;
        mptr = connection->recv;
        if (mptr == NULL) {
            if (mdata->m_flags & M_PKTHDR)
                connection->recv = mdata;
            else {
                proatm->pu_stats.proatm_st_drv.drv_rv_nopkthdr++;
                proatm->stats_ierrors++;                 
                connection->vccinf->vc_ierrors++;
                m_free(mdata);
                continue;
            }
        } else {
            x = 0;
            while (mptr->m_next != NULL) {
                                                      /* find last mbuf in chain */
                mptr = mptr->m_next;
                x++;
                if (x > 25)
                    break;

            }
            if (x > 25) {
                mptr = connection->recv;
                printf("proatm%d: invalid mbuf chain - probable corruption!\n",
                                                    proatm->unit);
                m_free(mdata);
                proatm->pu_stats.proatm_st_drv.drv_rv_invchain++;
                proatm->stats_ierrors++;                 
                connection->vccinf->vc_ierrors++;
                connection->recv = NULL;
                connection->rlen = 0;
                continue;
            }
            mptr->m_next = mdata;
        }
        connection->rlen += clen;
        if (flags & 0x2000) {
                                                      /* end of PDU */
            mptr = connection->recv;                  /* one or more mbufs will be here */
            clen = connection->rlen;                  /* length based on cell count */
            connection->recv = NULL;
            connection->rlen = 0;
            mptr->m_pkthdr.len = clen;
            mptr->m_pkthdr.rcvif = NULL;
            mptr->m_nextpkt = NULL;
#ifndef FBSD35
            if (mptr->m_pkthdr.csum_flags) {
                printf("proatm%d: received pkthdr.csum_flags=%x\n", 
                       proatm->unit, mptr->m_pkthdr.csum_flags);
                mptr->m_pkthdr.csum_flags = 0;
            }
#endif
            if (flags & 0x200 &&                      /* bad CRC */
                proatm->flg_igcrc == 0) {
                printf("proatm%d: bad CRC - discarding PDU: %d/%d\n",
                        proatm->unit, vpi, vci);
                /*
                 * Update interface stats
                 */
                proatm->stats_ierrors++;                 
                /*
                 * Update VC stats
                 */
                connection->vccinf->vc_ierrors++;

                if (connection->aal == IDTAAL5) {
                    /*
                     * Update AAL5 stats
                     */
                    proatm->pu_stats.proatm_st_aal5.aal5_pdu_crc++;
                    proatm->pu_stats.proatm_st_aal5.aal5_pdu_drops++;
                    proatm->pu_stats.proatm_st_aal5.aal5_drops +=
                        mptr->m_pkthdr.len / BYTES_PER_CELL;
                    proatm->pu_stats.proatm_st_aal5.aal5_crc_len +=
                        mptr->m_pkthdr.len / BYTES_PER_CELL;
                }
                /* else
                 * what's about IDTAAL3_4 ?
                 *  proatm->pu_stats.proatm_st_aal4.aal4_pdu_crc++;
                 *  proatm->pu_stats.proatm_st_aal4.aal4_pdu_errs++;
                 *  proatm->pu_stats.proatm_st_aal4.aal4_pdu_drops++;
                 *  proatm->pu_stats.proatm_st_aal4.aal4_drops +=
                 *      mptr->m_pkthdr.len / BYTES_PER_CELL;
                 *  proatm->pu_stats.proatm_st_aal4.aal4_crc +=
                 *       mptr->m_pkthdr.len / BYTES_PER_CELL;
                 */

                m_freem(mptr);
                continue;
            }

            /*
             * OK, it is a nice looking PDU. Forward to the
             * upper layers.
             */

            /*
             * Increment cells/pdu received stats.
             */
            proatm->pu_stats.proatm_st_atm.atm_rcvd +=
                mptr->m_pkthdr.len / BYTES_PER_CELL;

            switch (connection->aal) {
                case IDTAAL5:
                    if (proatm_receive_aal5(proatm, mptr, mdata))   /* adjust for AAL5 length */
                        continue;

                    proatm->pu_stats.proatm_st_aal5.aal5_pdu_rcvd++;
                    proatm->pu_stats.proatm_st_aal5.aal5_rcvd += 
                        mptr->m_pkthdr.len / BYTES_PER_CELL;
                    break;
                case IDTAAL3_4:
                    proatm->pu_stats.proatm_st_aal4.aal4_pdu_rcvd++;
                    proatm->pu_stats.proatm_st_aal4.aal4_rcvd +=
                        mptr->m_pkthdr.len / BYTES_PER_CELL;
                    break;
                case IDTAAL0:
                    proatm->pu_stats.proatm_st_aal0.aal0_rcvd +=
                        mptr->m_pkthdr.len / BYTES_PER_CELL;
                    break;
                case IDTAAL1:
                default:
                    break;
            }

            /*
             * Count the PDU stats for this interface.
             */
            proatm->stats_ipdus++;                       
            proatm->stats_ibytes += mptr->m_pkthdr.len;  

            /*
             * Counts the stats for this VC.
             */
            connection->vccinf->vc_ipdus++;
            connection->vccinf->vc_ibytes += mptr->m_pkthdr.len;

            proatm_receive(proatm, mptr, vpi, vci);
        }
        else if (connection->rlen > connection->maxpdu) {
                                                      /* this packet is insane */
            printf("proatm%d: bad packet, len=%d - discarding.\n", 
                   proatm->unit, connection->rlen);
            connection->recv = NULL;
            connection->rlen = 0;
            proatm->pu_stats.proatm_st_drv.drv_rv_toobigpdu++;
            proatm->stats_ierrors++;                     
            connection->vccinf->vc_ierrors++;
            if (connection->aal == IDTAAL5) {
                /*
                 * Update AAL5 stats
                 */
                proatm->pu_stats.proatm_st_aal5.aal5_pdu_drops++;
                proatm->pu_stats.proatm_st_aal5.aal5_drops +=
                    connection->rlen / BYTES_PER_CELL;
            }
            /* else
             * what's about IDTAAL3_4 ?
             *  proatm->pu_stats.proatm_st_aal4.aal4_pdu_drops++;
             *  proatm->pu_stats.proatm_st_aal4.aal4_pdu_errs++;
             *  proatm->pu_stats.proatm_st_aal4.aal4_drops +=
             *      connection->rlen / BYTES_PER_CELL;
             */

            m_freem(mptr);
        }
                                                      /* end of PDU */
    }

    proatm->rsqh = vtophys((u_int32_t) rsq) & 0x1ffc;
    proatm_reg_wr (proatm, (proatm->rsqh - sizeof(struct rsq_entry)) & 0x1ff0, REGRSQH);
    splx(s);
}

/******************************************************************************
 *
 *  Physical Interrupt handler
 *
 *  service physical interrupt.
 *
 */

static void 
proatm_phys(proatm_reg_t *proatm)
{       
    u_int32_t          t;

    if (proatm->flg_25) {
        proatm_util_rd(proatm, IDT77105_ISTAT_REG, &t);         /* get interrupt cause */
        if (t & 0x01) { /* Is it due to Rx FIFO Overrun  ? */
            proatm_util_wr(proatm, IDT77105_DIAG_REG, 0x10);    /* reset rx fifo, RFLUSH
			                                                     * clear receive FIFO */
            printf("proatm%d: PHY cleared.\n", proatm->unit);
        }
    } else {
        proatm_util_rd(proatm, SUNI_RSOP_SIS_REG, &t);
        if (t & 0x01)
            log(LOG_WARNING, "proatm%d: PHYS out of frame", proatm->unit);
        if (t & 0x02)
            log(LOG_WARNING, "proatm%d: PHYS loss of frame", proatm->unit);
        if (t & 0x04) {
            log(LOG_WARNING, "proatm%d: PHYS loss of signal", proatm->unit);
#ifdef __not_yet__
            proatm->XXX->phys = ATM_PHY_SIG_DOWN;
#endif
        }
    }

}


/******************************************************************************
 *
 *  Interrupt handler
 *
 *    service card interrupt.
 *
 *    proatm_intr ( card )
 */

static void 
proatm_intr(void *arg)
{       
    PROATM          *proatm= (PROATM *) arg;
    volatile        u_int32_t stat_val, config_val;
    volatile int32_t    i=0;

    config_val = proatm_reg_rd (proatm, REGCFG);
    stat_val = proatm_reg_rd (proatm, REGSTAT);

    /* loop until no more interrupts to service */
    while (stat_val & INT_FLAGS) {
        i++;
        if (i < 0 || i > 100)
            break;

        /* clear status bits that must be cleared by writing 1 */
        proatm_reg_wr (proatm, stat_val & CLEAR_FLAGS, REGSTAT);

        if (stat_val & IDT_STAT_EOPDU) {
            int32_t nsmall, nlarge;
                                                      /* receive PDU */
            XPRINT ("proatm_intr: receive PDU \n");
            proatm_recv(proatm);
                                                      /* replace buffers */
            nsmall = proatm_sysctl_bufsmall - ((stat_val & IDT_STAT_FRAC0_MASK) >> 11);
            if (nsmall < 0) nsmall = 0;
            nlarge = proatm_sysctl_buflarge - ((stat_val & IDT_STAT_FRAC1_MASK) >> 15);
            if (nlarge < 0) nlarge = 0;

            proatm_ld_rcv_buf(proatm, nsmall, nlarge);
        }

        if (stat_val & IDT_STAT_RAWCF){
            XPRINT ("proatm_intr: receive raw cell\n"); 
            proatm_rawc(proatm);                      /* raw cell */
        }    
       
        if (stat_val & IDT_STAT_TSIF) {
                                                      /* transmit complete */
            XPRINT ("proatm_intr: transmit complete \n");
            proatm_intr_tsq(proatm);
        }
        if (stat_val & IDT_STAT_TXICP) {
                                                      /* bad transmit */
            proatm_itrx(proatm);
            printf("proatm%d: Bad transmit.\n", proatm->unit);
        }
        if (stat_val & (IDT_STAT_FBQ0A | IDT_STAT_FBQ1A)) {
                                                      /* low level of FBQ0 */
            if (stat_val & IDT_STAT_FBQ0A){
                printf("proatm%d: not enough small buffers.\n", proatm->unit);
                proatm_ld_rcv_buf(proatm, 64, 0);
            }
                                                      /* low level of FBQ1 */
            if (stat_val & IDT_STAT_FBQ1A) {
                printf("proatm%d: not enough large buffers.\n", proatm->unit);
                proatm_ld_rcv_buf(proatm, 0, 64);
            }

            if (proatm_reg_rd(proatm, REGSTAT) & (IDT_STAT_FBQ0A | IDT_STAT_FBQ1A)) {
                                                      /* still missing, so disable IRQ */
                config_val &= ~IDT_CFG_FBIE;
                proatm_reg_wr (proatm, config_val, REGCFG);
            }
        }
        if (stat_val & IDT_STAT_TMROF) {
                                                      /* timer wrap */
            XPRINT ("proatm_intr: timer wrap \n");
            proatm->timer_wrap++;
            proatm_intr_tsq(proatm);                   /* check the TSQ */
            proatm_recv(proatm);                        /* check the receive queue */
            if (vtophys(proatm->raw_ch) !=  proatm->raw_hnd[0])
                proatm_rawc (proatm);                 /* check the raw cell queue */
            if (proatm_sysctl_logbufs)
                proatm_status_bufs(proatm);                 /* show the buffer status */
        }
        if (stat_val & IDT_STAT_PHYI) {
                                                 /* physical interface interrupt */
            XPRINT ("proatm_intr: phy interrupt \n");
            proatm_phys(proatm);
            proatm_reg_wr (proatm, IDT_STAT_PHYI, REGSTAT);    /* clear again the int32_t flag */
        }
        if (stat_val & IDT_STAT_RSQAF) {
                                                      /* RSQ almost full */
            proatm_recv(proatm);
            printf("proatm%d: warning, RSQ almost full.\n", proatm->unit);
            if (proatm_reg_rd (proatm, REGSTAT) & IDT_STAT_RSQAF) {
                                                      /* RSQ full */
                printf("      RSQ is full, disabling interrupt.\n");
                config_val &= ~IDT_CFG_RSQAFIE;
                proatm_reg_wr (proatm, config_val, REGCFG);
            }
        }
        if (stat_val & IDT_STAT_TSQF) {
                                                      /* TSQ almost full */
            proatm_intr_tsq(proatm);
            printf("proatm%d: warning, TSQ almost full.\n", proatm->unit);
            if (proatm_reg_rd(proatm, IDT_STAT_TSQF)) {
                printf("      TSQ is full, disabling interrupt.\n");
                config_val &= ~IDT_CFG_TSQFIE;
                proatm_reg_wr (proatm, config_val, REGCFG);
            }
        }
        stat_val = proatm_reg_rd (proatm, REGSTAT);
    }

#if 0
            /* detect spurious interrupts if the interrupt is not shared */
            /* do not use when the PCI interrupt is shared */
    if (i < 1 )                
            printf("proatm%d: i=%3d, unknown interrupt, status=%08x\n", 
                                proatm->unit, i, (int) stat_val);
#endif

    if (i > 50)
        printf("proatm%d: i=%3d, status=%08x\n", proatm->unit, i, (int) stat_val);
}


/*
 * Retrieve the value of one of the IDT77105's counters.
 * `counter' is one of 
 *   - SEC: symbol errors,  0x08
 *   - TCC: TX cells,       0x04
 *   - RCC: RX cells,       0x02
 *   - RHEC: RX HEC errors, 0x01
 */
static uint16_t
idt77105_get_counter(proatm_reg_t *proatm, u_int32_t counter)
{
    uint16_t val_low, val_high;
    u_int32_t t;

    switch (counter) {
        case 0x08: 
        case 0x04: 
        case 0x02: 
        case 0x01: 
          break;

        default:
          return 0; 
          break;
    }

    /* Select counter register */
    proatm_util_wr(proatm, IDT77105_CTRSEL_REG, counter);
    /* Read the counter */
    /*   read the low 8 bits */
    proatm_util_rd(proatm, IDT77105_CTRLO_REG, &t);
    val_low = t & 0xff;
    /*   read the high 8 bits */
    proatm_util_rd(proatm, IDT77105_CTRHI_REG, &t);
    val_high = t & 0xff;

    return (val_low | (val_high << 8) );
}

/*
 * Retrieve SUNI stats
 *
 */
static void
proatm_get_stats ( proatm_reg_t *proatm)
{
    if (proatm->flg_25) {
        /* 
         * IDT77105 stats 
         * The counters overflow when not fetched about every 
         * (65535 / ATM_PCR_UTP25) ~ 1 second.
         */
        proatm->pu_stats.proatm_st_utp25.utp25_symbol_errors +=
            idt77105_get_counter(proatm, 0x08);
        proatm->pu_stats.proatm_st_utp25.utp25_tx_cells      +=
            idt77105_get_counter(proatm, 0x04);
        proatm->pu_stats.proatm_st_utp25.utp25_rx_cells      +=
            idt77105_get_counter(proatm, 0x02);
        proatm->pu_stats.proatm_st_utp25.utp25_rx_hec_errors +=
            idt77105_get_counter(proatm, 0x01);
    } else {
        /* SUNI stats */
        u_int32_t t, val;

        /*
         * Write the SUNI master control register which
         * will cause all the statistics counters to be
         * loaded.
         */
        proatm_util_rd(proatm, SUNI_MASTER_REG, &t);
        proatm_util_wr(proatm, SUNI_MASTER_REG, t );    

        /*
         * Delay to allow for counter load time...
         */
        DELAY ( 10 ); /* SUNI_DELAY */

        /*
         * Statistics counters contain the number of events
         * since the last time the counter was read.
         */
        READ_TWO (val,  SUNI_SECT_BIP_REG );        /* oc3_sect_bip8 */
        proatm->pu_stats.proatm_st_oc3.oc3_sect_bip8 += val;
        READ_TWO (val,  SUNI_PATH_BIP_REG );        /* oc3_path_bip8 */
        proatm->pu_stats.proatm_st_oc3.oc3_path_bip8 += val;
        READ_THREE (val,  SUNI_LINE_BIP_REG );      /* oc3_line_bip24 */
        proatm->pu_stats.proatm_st_oc3.oc3_line_bip24 += val;
        READ_THREE (val,  SUNI_LINE_FEBE_REG );     /* oc3_line_febe */
        proatm->pu_stats.proatm_st_oc3.oc3_line_febe += val;
        READ_TWO (val,  SUNI_PATH_FEBE_REG );       /* oc3_path_febe */
        proatm->pu_stats.proatm_st_oc3.oc3_path_febe += val;
        READ_ONE (val,  SUNI_HECS_REG );            /* oc3_hec_corr */
        proatm->pu_stats.proatm_st_oc3.oc3_hec_corr += val;
        READ_ONE (val,  SUNI_UHECS_REG );           /* oc3_hec_uncorr */
        proatm->pu_stats.proatm_st_oc3.oc3_hec_uncorr += val;

        /* The counters overflow when not fetched about every 
         * (2^24 - 1) / ATM_PCR_OC3C ~ 47 seconds.
         */
        READ_THREE (val,  SUNI_RACP_RX_REG );       /* oc3_rx_cells */
        proatm->pu_stats.proatm_st_oc3.oc3_rx_cells += val;
        READ_THREE (val,  SUNI_TACP_TX_REG );       /* oc3_tx_cells */
        proatm->pu_stats.proatm_st_oc3.oc3_tx_cells += val;
    }
}


/******************************************************************************
 *
 *                       HARP GLUE SECTION
 *
 ******************************************************************************
 *
 * Handle netatm core service interface ioctl requests
 *
 * Called at splnet.
 *
 * Arguments:
 *    code       ioctl function (sub)code
 *    data       data to/from ioctl
 *    arg        optional code-specific argument
 *
 * Returns:
 *    0          request processed successfully
 *    error      request failed - UNIX reason code
 *
 */

int32_t
proatm_atm_ioctl(int32_t code, caddr_t data, caddr_t arg)
{  
    PROATM*                 proatm = (PROATM*)arg ;     
    struct atm_pif		    *pip = (struct atm_pif *)proatm;
    struct atminfreq        *aip = (struct atminfreq *)data;
	caddr_t			        buf = aip->air_buf_addr;
    struct air_vinfo_rsp	*avr; /*rsp*/
	int32_t			        count, len, buf_len = aip->air_buf_len;
    int32_t			        err = 0;
    char		            ifname[2*IFNAMSIZ];

    XPRINT("proatm_atm_ioctl: code=%d, opcode=%d\n",
           code, aip->air_opcode);

    if (proatm == NULL)
        return ENXIO;

	switch ( aip->air_opcode ) {

    case AIOCS_INF_VST:
        /* Get vendor statistics */		
        snprintf (ifname, sizeof(ifname), "%s%d", pip->pif_name, pip->pif_unit);
        avr = (struct air_vinfo_rsp *)buf;
        len = sizeof(struct air_vinfo_rsp);
        if (buf_len < len )
            return ENOSPC;
        if ((err = copyout(ifname, avr->avsp_intf, IFNAMSIZ)) != 0)
            break;
        buf += len;  buf_len -= len;
                            /* retrieve the SUNI stats */
        proatm_get_stats (proatm);

        /* Get misc stats */
                            /* current CBR TX cellrate */
        proatm->pu_stats.proatm_st_drv.drv_xm_cbrbw = proatm->cellrate_tcur;
                            /* Unused TX QUEUE  */
        proatm->pu_stats.proatm_st_drv.drv_xm_qufree = proatm->txqueue_free_count;
                            /* Free slots within the UBR0 queue */
        proatm->pu_stats.proatm_st_drv.drv_xm_ubr0free =
            IDT_SCQ_ENTRIES - proatm->txqueue_ubr0.scq_cur;

        count = MIN ( sizeof(Proatm_stats), buf_len );

        if ((err = copyout((void *)&proatm->pu_stats, buf, count)) != 0)
            break;
        buf += count; buf_len -= count;
        if ( count < sizeof(Proatm_stats) )
            err = ENOSPC;

        /*
         * Record amount we're returning as vendor info...
         * ie : arv->avsp_len = sizeof (Proatm_stats);
         */
        if ((err = copyout(&count, &avr->avsp_len, sizeof(int32_t))) != 0)
            break;   
                            /* Update the reply length and pointer*/
        aip->air_buf_addr = buf;
        aip->air_buf_len = buf_len;
        break;

    default:
        err = ENOSYS;		/* Operation not supported */
        break;
    }

    return err;
}

/*
 * ATM Interface services
 */
static struct stack_defn proatm_svaal5 = {
    NULL, 
    SAP_CPCS_AAL5, 
    SDF_TERM, 
    atm_dev_inst, 
    atm_dev_lower, 
    NULL, 
    0, 
}
;
static struct stack_defn proatm_svaal4 = {
    & proatm_svaal5, 
    SAP_CPCS_AAL3_4, 
    SDF_TERM, 
    atm_dev_inst, 
    atm_dev_lower, 
    NULL, 
    0, 
}
;
static struct stack_defn proatm_svaal0 = {
    & proatm_svaal4, 
    SAP_ATM, 
    SDF_TERM, 
    atm_dev_inst, 
    atm_dev_lower, 
    NULL, 
    0, 
}
;
struct stack_defn *proatm_services = &proatm_svaal0;
/*
 * Storage pools
 */
struct sp_info proatm_nif_pool = {"proatm nif pool",        /* si_name */
    sizeof (struct atm_nif),                          /* si_blksiz */
    5,                                                /* si_blkcnt */
    20                                                /* si_maxallow */
}
;
struct sp_info proatm_vcc_pool = {"proatm vcc pool",        /* si_name */
    sizeof (Idt_vcc),                                 /* si_blksiz */
    10,                                               /* si_blkcnt */
    500                                               /* si_maxallow */
}
;
/*******************************************************************************
 *
 *  Get connection pointer from Cmn_unit and Cmn_vcc
 *
 *  in:  Cmn_unit and Cmn_vcc
 * out:  connection (NULL=error)
 *
 */
CONNECTION *
proatm_atm_harpconn(Cmn_unit *cup, Cmn_vcc *cvp)
{       
    struct vccb     *vccinf;                          /* from HARP struct */
    PROATM             *proatm;
    int32_t             vpi;
    int32_t             vci;

    proatm = (PROATM *) cup;
    if (proatm == NULL || cvp == NULL)
        return (NULL);

    if (cvp->cv_connvc == NULL)
        return (NULL);

    vccinf = cvp->cv_connvc->cvc_vcc;
    if (vccinf == NULL)
        return (NULL);

    vpi = vccinf->vc_vpi;
    vci = vccinf->vc_vci;
    return proatm_connect_find (proatm, vpi, vci);

}

/*******************************************************************************
 *
 *  Get CBR/VBR/UBR class from bearer attribute
 *
 *  in:  
 * out:  NICCBR/NICVBR/NICABR/NICUBR
 *
 */


/*  
 *  Note that CLASS_X is typically UBR, but the traffic type information
 *  element may still specify CBR or VBR.
 *  the default class is UBR0
 *  At this time it is not possible to specify ABR 
 */

static int32_t
proatm_atm_bearerclass(struct attr_bearer *bearer)
{       
    switch (bearer->v.bearer_class) {
    case T_ATM_CLASS_A:
        return (NICCBR);
    case T_ATM_CLASS_C:
        return (NICVBR);
    case T_ATM_CLASS_X:
        switch (bearer->v.traffic_type) {
        case T_ATM_CBR:
            return (NICCBR);
        case T_ATM_VBR:
            return (NICVBR);
        }
        break;          
    }
    return (NICUBR);
}

/*******************************************************************************
 *
 *  Initialize HARP service
 *  called from device attach
 */

static void 
proatm_harp_init(proatm_reg_t *const proatm, void* arg2)
{       
    long long       tsc_val;
    u_int8_t          mac[6];

   /*
    * Start initializing it
    */
    proatm->iu_unit = proatm->unit;
    proatm->iu_mtu = PROATM_IFF_MTU;
    proatm->iu_ioctl = proatm_atm_ioctl;
    proatm->iu_openvcc = proatm_openvcc;
    proatm->iu_instvcc = proatm_instvcc;
    proatm->iu_closevcc = proatm_closevcc;
    proatm->iu_output = proatm_output;
    proatm->iu_vcc_pool = &proatm_vcc_pool;
    proatm->iu_nif_pool = &proatm_nif_pool;

   /*
    * Copy serial number into config space
    */
    proatm->iu_config.ac_serial = 0;
    proatm->iu_config.ac_vendor = VENDOR_PROSUM;
    proatm->iu_config.ac_vendapi = VENDAPI_IDT_2;
    proatm->iu_config.ac_device = proatm->flg_25? DEV_PROATM_25: DEV_PROATM_155;
    proatm->iu_config.ac_media = proatm->flg_25? MEDIA_UTP25: MEDIA_OC3C ;
    proatm->iu_config.ac_bustype = BUS_PCI;
    proatm->iu_pif.pif_pcr = proatm->max_pcr;         
    proatm->iu_pif.pif_maxvpi = 0xff;
    proatm->iu_pif.pif_maxvci = 0xffff;
    snprintf(proatm->iu_config.ac_hard_vers, 
             sizeof (proatm->iu_config.ac_hard_vers), 
             proatm->hardware);
    snprintf(proatm->iu_config.ac_firm_vers, 
             sizeof (proatm->iu_config.ac_firm_vers), 
             PROATM_VERSION);
    /*
     * Save device ram info for user-level programs
     * NOTE: This really points to start of EEPROM
     * and includes all the device registers in the
     * lower 2 Megabytes.
     */

    proatm_eeprom_rd(proatm, EPROM_PROSUM_MAC_ADDR_OFFSET, mac, 6);
        if (mac[0] != PROSUM_MAC_0 || mac[1] != PROSUM_MAC_1 || mac[2] != PROSUM_MAC_2)
            proatm_eeprom_rd(proatm, EPROM_IDT_MAC_ADDR_OFFSET, mac, 6);

    proatm->iu_config.ac_ram = NULL;
    proatm->iu_config.ac_ramsize = 0;
    if ((mac[3] | mac[4] | mac[5]) == 0) {
       /* looks like bad MAC */
        GET_RDTSC(tsc_val);             /* 24 bits on 500mhz CPU is about 30msec */

        mac[0] = PROSUM_MAC_0;
        mac[1] = PROSUM_MAC_1;
        mac[2] = PROSUM_MAC_2;          /* use Prosum prefix */
        mac[3] = (tsc_val >> 16)& 0xff;
        mac[4] = (tsc_val >> 8)& 0xff;
        mac[5] = (tsc_val)& 0xff;
        printf("proatm%d: Cannot read MAC address from EEPROM, generating it.\n", 
               proatm->unit);
    }
    memcpy ( proatm->iu_pif.pif_macaddr.ma_data, mac, 6);
    printf("proatm%d: MAC address %02x:%02x:%02x:%02x:%02x:%02x, HWrev=%d\n", 
           proatm->unit, 
           proatm->iu_pif.pif_macaddr.ma_data[0], 
           proatm->iu_pif.pif_macaddr.ma_data[1], 
           proatm->iu_pif.pif_macaddr.ma_data[2], 
           proatm->iu_pif.pif_macaddr.ma_data[3], 
           proatm->iu_pif.pif_macaddr.ma_data[4], 
           proatm->iu_pif.pif_macaddr.ma_data[5], 
           proatm->pci_rev);
    proatm->iu_config.ac_macaddr = proatm->iu_pif.pif_macaddr;

    /*
     * Register this interface with ATM core services
     */
    if (atm_physif_register
        ((Cmn_unit *) proatm, PROATM_DEV_NAME, proatm_services) != 0) {
        /*
         * Registration failed - back everything out
         */
#ifdef FBSD35
		proatm_device_stop(proatm);
#else
        proatm_detach((device_t) arg2);
#endif
        log(LOG_ERR, 
            "proatm_pci_attach: atm_physif_register failed\n");
        return;
    }
    proatm->iu_flags |= CUF_INITED;

#if BSD >= 199506
    /*
     * Add hook to out shutdown function
     * at_shutdown ( (bootlist_fn)proatm_pci_shutdown, proatm, SHUTDOWN_POST_SYNC );
     */
#endif 

    return;
}

/*******************************************************************************
 *
 *  Output data
 */

void 
proatm_output(Cmn_unit *cmnunit, Cmn_vcc *cmnvcc, KBuffer *m)
{       
    struct vccb     *vccinf;                          /* from HARP struct */
    PROATM             *proatm;
    int32_t             vpi;
    int32_t             vci;

    proatm = (PROATM *) cmnunit;
    if (cmnvcc == NULL) {
        printf("proatm%d: proatm_output arg error #1\n", proatm->unit);
        m_free(m);                                    /* throw away packet, no VCC */
        return;
    }
    if (cmnvcc->cv_connvc == NULL) {
        printf("proatm%d: proatm_output arg error #2\n", proatm->unit);
        m_free(m);                                    /* throw away packet, no VCC */
        return;
    }
    vccinf = cmnvcc->cv_connvc->cvc_vcc;
    if (vccinf == NULL) {
        printf("proatm%d: proatm_output arg error #3\n", proatm->unit);
        m_free(m);                                    /* throw away packet, no VCC */
        return;
    }
    vpi = vccinf->vc_vpi;
    vci = vccinf->vc_vci;

    proatm_transmit (proatm, m, vpi, vci);
}

/*******************************************************************************
 *
 *  Open VCC
 */

int32_t 
proatm_openvcc(Cmn_unit *cmnunit, Cmn_vcc *cmnvcc)
{       
    Atm_attributes  *attrib;                          /* from HARP struct */
    struct vccb     *vccinf;                          /* from HARP struct */
    CONNECTION      *connection = NULL;
    PROATM          *proatm;
    int32_t             vpi;
    int32_t             vci;
    int32_t             class;

    /* NICCBR, NICVBR or NICUBR */
    proatm = (PROATM *) cmnunit;
    if (cmnvcc == NULL || cmnvcc->cv_connvc == NULL) {
        printf("proatm%d: proatm_openvcc: bad request #1.\n", proatm->unit);
        return (EFAULT);
    }
    attrib = &cmnvcc->cv_connvc->cvc_attr;
    vccinf = cmnvcc->cv_connvc->cvc_vcc;
    if (attrib == NULL || vccinf == NULL) {
        printf("proatm%d: proatm_openvcc: bad request #2.\n", proatm->unit);
        return (EFAULT);
    }
    vpi = vccinf->vc_vpi;
    vci = vccinf->vc_vci;

    if (proatm_check_vc (proatm, vpi, vci)) {
        printf("proatm%d: proatm_openvcc: vpi/vci invalid: %d/%d\n", 
                                           proatm->unit, vpi, vci);
        proatm->pu_stats.proatm_st_drv.drv_vc_badrng++;
        return (EFAULT);
    }
                   
    connection = proatm_connect_find(proatm, vpi, vci);

    if (connection->flg_open) {
        printf("proatm%d: proatm_openvcc: connection %s %d/%d\n",
           proatm->unit, connection->flg_closing?"closing":"already open", 
                            connection->vpi, connection->vci);
        return (EEXIST);
    }
    connection->flg_open = 1;
    connection->vpi = vpi;
    connection->vci = vci;
    connection->recv = NULL;
    connection->rlen = 0;
    connection->maxpdu = 20000;
    switch (attrib->aal.type) {
      case ATM_AAL0:
        connection->aal = IDTAAL0;
      case ATM_AAL3_4:
        connection->aal = IDTAAL3_4;
        return (EINVAL);
      case ATM_AAL5:
        connection->aal = IDTAAL5;      /* No other AAL supported at this time */
        break;
      default:
        return (EINVAL);
    }
    connection->traf_pcr = attrib->traffic.v.forward.PCR_all_traffic;
    connection->traf_scr = attrib->traffic.v.forward.SCR_all_traffic;
    connection->traf_mbs = attrib->traffic.v.forward.MBS_all_traffic;
    /*
     * According to me the Cell Loss Priority could be set
     * when the best effort service is required, because
     * best effort means low priority.
     */
    connection->flg_clp = (attrib->traffic.v.best_effort == T_YES) ? 1 : 0;
    connection->vccinf = vccinf;    

    if (connection->traf_pcr <= 0)
        connection->traf_pcr = connection->traf_scr;

    if (connection->traf_scr <= 0)
        connection->traf_scr = connection->traf_pcr;

    if (connection->traf_mbs <= 0 || connection->traf_mbs > 255)
        connection->traf_mbs = 255;

    class = proatm_atm_bearerclass (&attrib->bearer);

    if (connection->traf_pcr <= 0 || connection->traf_pcr >= proatm->max_pcr) {
        connection->traf_pcr = proatm->max_pcr;
        connection->traf_scr = proatm->max_pcr;
        class = NICUBR0;        /* single transmit queue, full speed */                 
    } 

    connection->class = class;

    if (proatm_connect_txopen(proatm, connection)) {
        printf("proatm%d: cannot open connection for %d/%d\n", proatm->unit, vpi, vci);
        return (1);
    }
    if (proatm_sysctl_logvcs)
        printf("proatm%d: proatm_openvcc: %d/%d, PCR=%d, SCR=%d\n", 
                proatm->unit, vpi, vci, connection->traf_pcr, connection->traf_scr);

    proatm_connect_rxopen (proatm, connection);          /* open entry in rcv connect table */

    return (0);
}


/*******************************************************************************
 *
 *  Close VCC
 */

int32_t 
proatm_closevcc(Cmn_unit *cmnunit, Cmn_vcc *cmnvcc)
{       
    CONNECTION      *connection = NULL;
    proatm_reg_t   *proatm = (proatm_reg_t *) cmnunit;
    int32_t             vpi;
    int32_t             vci;

    if (cmnvcc && cmnvcc->cv_connvc && cmnvcc->cv_connvc->cvc_vcc) {
        vpi = cmnvcc->cv_connvc->cvc_vcc->vc_vpi;
        vci = cmnvcc->cv_connvc->cvc_vcc->vc_vci;
    } else {
        printf("proatm%d: proatm_closevcc: bad vcivpi\n", proatm->unit);
        return (0);
    }
    connection = proatm_connect_find(proatm, vpi, vci);
    if (connection == NULL) {
        printf("proatm%d: proatm_closevcc: vpi/vci invalid: %d/%d\n", 
                                    proatm->unit, vpi, vci);
        return (0);
    }
    proatm_connect_rxclose(proatm, connection);          /* close entry in rcv connect table */

    if (connection->flg_open == 0)
        printf("proatm%d: proatm_closevcc: close on empty connection %d/%d\n", 
                                    proatm->unit, vpi, vci);
    if (connection->recv != NULL)
        m_freem(connection->recv);                      /* recycle mbuf of partial PDU */    
                                                      
    proatm_connect_txclose(proatm, connection, 0);
    if (proatm_sysctl_logvcs)
        printf("proatm%d: proatm_closevcc: vpi=%d vci=%d\n",proatm->unit, vpi, vci);

    return 0;
}

/*
 *
 * VCC Stack Instantiation
 *
 * This function is called via the common driver code during a device VCC
 * stack instantiation.  The common code has already validated some of
 * the request so we just need to check a few more IDT-specific details.
 *
 * Called at splnet.
 *
 * Arguments:
 *    cup    pointer to device common unit
 *    cvp    pointer to common VCC entry
 *
 * Returns:
 *    0    instantiation successful
 *    err     instantiation failed - reason indicated
 *
 */
int32_t
proatm_instvcc(Cmn_unit *cmnunit, Cmn_vcc *cmnvcc)
{
    Atm_attributes  *attrib;                          /* from HARP struct */
    PROATM             *proatm;
    int32_t             class, pcr, scr, mbs;
    int32_t             slots_vc, slots_cur, slots_max;

    if (cmnvcc == NULL)
        return (EINVAL);

    if (cmnvcc->cv_connvc == NULL)
        return (EINVAL);

    proatm = (PROATM *) cmnunit;
    if (proatm == NULL)
        return (EINVAL);

    attrib = &cmnvcc->cv_connvc->cvc_attr;
    if (attrib == NULL)
        return (EINVAL);

    pcr = attrib->traffic.v.forward.PCR_all_traffic;
    scr = attrib->traffic.v.forward.SCR_all_traffic;
    mbs = attrib->traffic.v.forward.MBS_all_traffic;

    if (pcr <= 0)
        pcr = scr;
                                                      /* if PCR missing, default to SCR */
    if (pcr <= 0)
        pcr = 1;

    if (scr <= 0)
        scr = pcr;

    class = proatm_atm_bearerclass(&attrib->bearer);
    if (class == NICCBR) {
        slots_max = proatm->tst_num;
        slots_cur = proatm->txslots_cur;
        slots_vc = proatm_slots_cbr(proatm, scr);                 
        if (slots_vc + slots_cur > slots_max) {
            proatm->pu_stats.proatm_st_drv.drv_vc_outofbw++;
            if (proatm_sysctl_logvcs)
                printf("proatm%d: Insufficient bandwproatmh (vc=%d cur=%d max=%d)\n", 
                       proatm->unit, slots_vc, slots_cur, slots_max);

            return (EINVAL);
        }
    }
    /* This part was taken from /sys/dev/hfa/fore_vcm.c */

    switch (attrib->aal.type) {
      case ATM_AAL0:
        break;
      case ATM_AAL3_4:
        if ((attrib->aal.v.aal4.forward_max_SDU_size > PROATM_IFF_MTU) || 
            (attrib->aal.v.aal4.backward_max_SDU_size > PROATM_IFF_MTU)) {
            proatm->pu_stats.proatm_st_drv.drv_vc_maxpdu++;
            return (EINVAL);
        }

        break;
      case ATM_AAL5:
        if ((attrib->aal.v.aal5.forward_max_SDU_size > PROATM_IFF_MTU) || 
            (attrib->aal.v.aal5.backward_max_SDU_size > PROATM_IFF_MTU)) {
            proatm->pu_stats.proatm_st_drv.drv_vc_maxpdu++;
            return (EINVAL);
        }

        break;
      default:
        return (EINVAL);
    }
    return (0);
}

/*
 * Pass Incoming PDU up Stack
 *
 * This function is called via the core ATM interrupt queue callback 
 * set in proatm_receive().  It will pass the supplied incoming 
 * PDU up the incoming VCC's stack.
 *
 * Called at splnet by the ATM's ISR.
 *
 * Arguments:
 *    tok        token to identify stack instantiation
 *    m        pointer to incoming PDU buffer chain
 *
 * Returns:
 *    none
 */
static void
proatm_recv_stack(void *tok, KBuffer *m)
{       
    Idt_vcc         *ivp = (Idt_vcc *) tok;
    int32_t             err;

    /*
     * This should never happen now but if it does and we don't stop it,
     * we end up panic'ing in netatm when trying to pull a function
     * pointer and token value out of a buffer with address zero.
     */
    if (m == NULL) {
        printf("proatm: proatm_recv_stack: Warning - null mbuf pointer.\n");
        return;
    }

    if ((m->m_flags & M_PKTHDR) == 0) {
        printf("proatm: proatm_recv_stack: Warning - mbuf chain has no header.\n");
        KB_FREEALL(m);
        return;
    }
    /*
         * Send the data up the stack
         */
    STACK_CALL(CPCS_UNITDATA_SIG, ivp->iv_upper, 
               ivp->iv_toku, ivp->iv_vccb, (int32_t) m, 0, err);
    if (err)
        KB_FREEALL(m);

    return;
}

/******************************************************************************
 *
 *  Enqueue received PDU for HARP to handle
 *
 *  in:  PROATM device, mbuf, vpi, vci
 *
 */

static void
proatm_receive(proatm_reg_t *proatm, struct mbuf *m, int32_t vpi, int32_t vci)
{       
    caddr_t         cp;
    Cmn_vcc         *vcc;
    int32_t             space;

   /*
    * The STACK_CALL needs to happen at splnet() in order
    * for the stack sequence processing to work.  Schedule an
    * interrupt queue callback at splnet() since we are 
    * currently at device level.
    */
   /*
    * Prepend callback function pointer and token value to buffer.
    * We have already guaranteed that the space is available
    * in the first buffer.
    */

    /*
     * Locate incoming VCC for this PDU
     */
    vcc = atm_dev_vcc_find((Cmn_unit *) proatm, vpi, vci, VCC_IN);
    if (vcc == NULL) {
        /* harp stack not ready or no vcc */
        printf("proatm%d: proatm_receive: no VCC %d/%d\n", proatm->unit, vpi, vci);
        proatm->pu_stats.proatm_st_drv.drv_rv_novcc++;
        proatm->stats_ierrors++;
        KB_FREEALL(m);
        return;
    }
    space = m->m_data - proatm_mbuf_base(m);
    if (space < sizeof (atm_intr_func_t) + sizeof (int)) {
        printf("proatm%d: proatm_receive: NOT enough buffer space (%d)\n", proatm->unit, space);
        proatm->pu_stats.proatm_st_drv.drv_rv_nobufs++;
        proatm->stats_ierrors++;
        KB_FREEALL(m);
        return;
    }

    KB_HEADADJ(m, sizeof (atm_intr_func_t) + sizeof (int));
    KB_DATASTART(m, cp, caddr_t);
    /*
     * Free zero-length buffer
     */
    if (m->m_len == 0) {
        proatm->pu_stats.proatm_st_drv.drv_rv_null++;
        proatm->stats_ierrors++;
        KB_FREEALL(m);
        return;
    }
    * ((atm_intr_func_t *) cp) = proatm_recv_stack;
    cp += sizeof (atm_intr_func_t);
    * ((void * *) cp) = (void *) vcc;
    /*
     * Schedule callback
     */
    if (!IF_QFULL(&atm_intrq)) {
        IF_ENQUEUE(&atm_intrq, m);
        SCHED_ATM;
    } else {
        proatm->pu_stats.proatm_st_drv.drv_rv_intrq++;
        proatm->stats_ierrors++;
        /* atm_intrq is full. Unable to pass up to the HARP stack */
        KB_FREEALL(m);
    }
}



