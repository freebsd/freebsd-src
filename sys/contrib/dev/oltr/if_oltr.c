/* 
 * Copyright (c) 1998, Larry Lile
 * All rights reserved.
 *
 * For latest sources and information on this driver, please
 * go to http://anarchy.stdio.com.
 *
 * Questions, comments or suggestions should be directed to
 * Larry Lile <lile@stdio.com>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 * $Id: if_oltr.c,v 1.2 1999/03/10 17:45:26 julian Exp $
 */


#include "pci.h"
#include "oltr.h"
#include "opt_inet.h"
#include "bpfilter.h"

#if (NOLTR + NPCI) > 0

/*#define TRlldInlineIO*/

#define ISA_ADAPTERS  (OC_3115 | OC_3117 | OC_3118)
#define PCI_ADAPTERS  (OC_3133 | OC_3136 | OC_3137 | \
                       OC_3139 | OC_3140 | OC_3141 | \
                       OC_3250 | OC_3540 )

#define PCI_VENDOR_OLICOM 0x108D

char *AdapterName[] = {
   /*  0 */ "Olicom XT Adapter [unsupported]",
   /*  1 */ "Olicom OC-3115",
   /*  2 */ "Olicom ISA 16/4 Adapter (OC-3117)",
   /*  3 */ "Olicom ISA 16/4 Adapter (OC-3118)",
   /*  4 */ "Olicom MCA 16/4 Adapter (OC-3129) [unsupported]",
   /*  5 */ "Olicom MCA 16/4 Adapter (OC-3129) [unsupported]",
   /*  6 */ "Olicom MCA 16/4 Adapter (OC-3129) [unsupported]",
   /*  7 */ "Olicom EISA 16/4 Adapter (OC-3133)",
   /*  8 */ "Olicom EISA 16/4 Adapter (OC-3133)",
   /*  9 */ "Olicom EISA 16/4 Server Adapter (OC-3135)",
   /* 10 */ "Olicom PCI 16/4 Adapter (OC-3136)",
   /* 11 */ "Olicom PCI 16/4 Adapter (OC-3136)",
   /* 12 */ "Olicom PCI/II 16/4 Adapter (OC-3137)",
   /* 13 */ "Olicom PCI 16/4 Adapter (OC-3139)",
   /* 14 */ "Olicom RapidFire 3140 16/4 PCI Adapter (OC-3140)",
   /* 15 */ "Olicom RapidFire 3141 Fiber Adapter (OC-3141)",
   /* 16 */ "Olicom PCMCIA 16/4 Adapter (OC-3220) [unsupported]",
   /* 17 */ "Olicom PCMCIA 16/4 Adapter (OC-3121, OC-3230, OC-3232) [unsupported]",
   /* 18 */ "Olicom PCMCIA 16/4 Adapter (OC-3250)",
   /* 19 */ "Olicom RapidFire 3540 4/16/100 Adapter (OC-3540)"
};

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/sockio.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/kernel.h>
#include <sys/interrupt.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/iso88025.h>
#include <net/if_media.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif
 
#if NPNP > 0
#include <i386/isa/pnp.h>
#endif

#include <machine/clock.h>
#include <machine/md_var.h>
#include <i386/isa/isa_device.h>

#if NPCI > 0
#include <pci/pcivar.h>
#include <pci/pcireg.h>
#endif

#include "contrib/dev/oltr/trlld.h"

#ifndef TRLLD_SPEED_AUTO
#define TRLLD_SPEED_AUTO 0
#endif

#define MIN(A,B) (((A) < (B)) ? (A) : (B))
#define MIN3(A,B,C) (MIN(A, (MIN(B, C))))

void *oltr_malloc(ssize_t, TRlldAdapterConfig_t *);

/*
 * Glue functions prototypes for PMW kit IO
 */

#ifndef TRlldInlineIO
static void DriverOutByte           __P((unsigned short, unsigned char));
static void DriverOutWord           __P((unsigned short, unsigned short));
static void DriverOutDword          __P((unsigned short, unsigned long));
static void DriverRepOutByte        __P((unsigned short, unsigned char  *, int));
static void DriverRepOutWord        __P((unsigned short, unsigned short *, int));
static void DriverRepOutDword       __P((unsigned short, unsigned long  *, int));
static unsigned char  DriverInByte  __P((unsigned short));
static unsigned short DriverInWord  __P((unsigned short));
static unsigned long  DriverInDword __P((unsigned short));
static void DriverRepInByte         __P((unsigned short, unsigned char  *, int));
static void DriverRepInWord         __P((unsigned short, unsigned short *, int));
static void DriverRepInDword        __P((unsigned short, unsigned long  *, int));
#endif /*TRlldInlineIO*/
static void DriverSuspend                __P((unsigned short));
static void DriverStatus                 __P((void *, TRlldStatus_t *));
static void DriverCloseCompleted         __P((void *));
static void DriverStatistics             __P((void *, TRlldStatistics_t *));
static void DriverTransmitFrameCompleted __P((void *, void *, int));
static void DriverReceiveFrameCompleted  __P((void *, int, int, void *, int));

typedef struct tx_buf {
    int         index;
    int         count;
    char        *buf;
    struct mbuf *m;
} tx_buf_t;

typedef struct rx_buf {
    int         index;
    char        *buf;
} rx_buf_t;

#ifndef EXTRA_OLTR
#if NPCI > 0
#define EXTRA_OLTR	8
#else 
#define EXTRA_OLTR	0
#endif /* NPCI */
#endif /* EXTRA_OLTR */

#ifndef OLTR_PROMISC_MODE
#define OLTR_PROMISC_MODE (TRLLD_PROM_LLC)
#endif

#define ALL_OPTIONS (IFM_TOK_ETR | IFM_TOK_SRCRT | IFM_TOK_ALLR | IFM_TOK_DTR | IFM_TOK_CLASSIC | IFM_TOK_AUTO)

/* List sizes MUST be a power of 2 */
#define TX_LIST_SIZE    16
#define RX_LIST_SIZE    16 
#define TX_LIST_MASK    (TX_LIST_SIZE - 1)
#define RX_LIST_MASK    (RX_LIST_SIZE - 1)
#define RX_BUFFER_LEN   (8*1024)
#define TX_BUFFER_LEN   (8*1024)

struct oltr_softc {
    struct arpcom arpcom;
    struct ifmedia ifmedia;
    TRlldAdapterConfig_t *config;
    TRlldAdapter_t *TRlldAdapter;
    int unit;
    u_short PromiscMode;
    u_short AdapterMode;
    int hw_state;
#define HW_UNKNOWN      0  /* initial/absent state */
#define HW_FOUND        1  /* found, not initialized */
#define HW_BAD          2  /* fatal error */
#define HW_FAILED       3  /* closed eg. by remove, allow manual reopen */
#define HW_LOADING      4
#define HW_CLOSING      5
#define HW_CLOSING2     6
#define HW_CLOSED       7
#define HW_OPENING      8
#define HW_OPEN         9
#define HW_ERROR        10 /* temporary error */

    u_long GroupAddress;
    u_long FunctionalAddress;
    int poll_adapter; 

    int tx_next;
    int tx_avail;
    tx_buf_t tx_buffer[TX_LIST_SIZE];
    TRlldTransmit_t tx_frame;

    int rx_next;
    int rx_avail;
    rx_buf_t rx_buffer[RX_LIST_SIZE];

    struct callout_handle oltr_ch;
    struct callout_handle poll_ch;

};

static struct oltr_softc oltr_softc[NOLTR + EXTRA_OLTR];

/*
 * Driver function prototypes
 */

static int  oltr_probe   __P((struct isa_device *));
static int  oltr_attach  __P((struct isa_device *));  
static void oltr_init    __P((struct oltr_softc *));
static void oltr_intr    __P((int));
static void oltr_start   __P((struct ifnet *));
static void oltr_stop    __P((struct oltr_softc *));
static int  oltr_ioctl   __P((struct ifnet *, u_long, caddr_t));

static int oltr_attach_common   __P((struct oltr_softc *));

void oltr_timeout __P((void *));
void adapter_poll __P((void *));

struct isa_driver oltrdriver = {
    oltr_probe,
    oltr_attach,
    "oltr",
    0
};

int isa_cards = 0;

#if NPCI > 0
static u_long oltr_count = NOLTR;
static const char *oltr_pci_probe	__P((pcici_t, pcidi_t));
static void oltr_pci_attach		__P((pcici_t, int));
static void oltr_pci_intr    		__P((void *));
static void oltr_pci_shutdown		__P((int, void *));

static struct pci_device oltr_device = {
    "oltr",
    oltr_pci_probe,
    oltr_pci_attach,
    &oltr_count,
    NULL
};

DATA_SET(pcidevice_set, oltr_device);
int pci_cards = 0;
#endif /* NPCI */

static int  oltr_ifmedia_upd	__P((struct ifnet *));
static void oltr_ifmedia_sts    __P((struct ifnet *, struct ifmediareq *));

static TRlldDriver_t oltrLldDriver = {
    TRLLD_VERSION,
#ifndef TRlldInlineIO
    DriverOutByte,
    DriverOutWord,
    DriverOutDword,
    DriverRepOutByte,
    DriverRepOutWord,
    DriverRepOutDword,
    DriverInByte,
    DriverInWord,
    DriverInDword,
    DriverRepInByte,
    DriverRepInWord,
    DriverRepInDword,
#endif /*TRlldInlineIO*/
    DriverSuspend,
    DriverStatus,
    DriverCloseCompleted,
    DriverStatistics,
    DriverTransmitFrameCompleted,
    DriverReceiveFrameCompleted,
};

TRlldAdapterConfig_t oltr_config[NOLTR + EXTRA_OLTR];

void *
oltr_malloc(Size, Adapter)
    ssize_t Size;
    TRlldAdapterConfig_t *Adapter;
{

    /* If the adapter needs memory below 16M for DMA then use contigmalloc */
    if (Adapter->mode & TRLLD_MODE_16M)  /* Adapter using ISA DMA buffer below 16M */
        return(contigmalloc(Size, M_DEVBUF, M_NOWAIT, 0ul, 0xfffffful, 1ul, 0x10000ul));
    else
        return(malloc(Size, M_DEVBUF, M_NOWAIT));
}
    
/*
 * Driver Functions 
 */

static int
oltr_probe(is)
    struct isa_device *is;
{
    static int find_completed = 0, assigned[NOLTR];
    struct oltr_softc *sc = &oltr_softc[is->id_unit];
    int i;

    printf("oltr%d: oltr_probe\n", is->id_unit);

    /* Make life easy, use the Olicom supplied find function on the first probe
     * to probe all of the ISA adapters.  Then give them to each unit as requested.
     * Try to match the adapters to units based on the iobase, but if iobase? then
     * just give out the next available adapter.
     */
    if (!find_completed) {
        isa_cards = TRlldFind(&oltrLldDriver, &oltr_config[0], ISA_ADAPTERS, NOLTR);
        /*for (i = 0; i < isa_cards; i++) {
            printf("TRlldFind: card %d - %s MAC %6D\n", i + 1, AdapterName[oltr_config[i].type], oltr_config[i].macaddress, ":");
        }*/
        for (i = 0; i < NOLTR; i++)
            assigned[i] = 0;
        find_completed = 1;
    }

    sc->unit = is->id_unit;
    sc->hw_state = HW_UNKNOWN;

    if (find_completed && ((isa_cards == 0) || (is->id_unit > isa_cards))) 
        return(0);

    if (((is->id_iobase < 0xa00) || (is->id_iobase > 0xbe0)) && (is->id_iobase != 0xffffffff)) {
        printf("oltr%d: port address impossible (0x%X)\n", is->id_unit, is->id_iobase);
        return(0);
    }

    /* Auto assign lowest available card not already in use */
    if (is->id_iobase == 0xffffffff) {
        printf("oltr%d: auto assigning card.\n", is->id_unit);
        for (i = 0; assigned[i]; i++);
        assigned[i] = 1;
        sc->config = &oltr_config[i];
        is->id_iobase = sc->config->iobase0;                        /* Claim our port space */
        if (!is->id_irq) 
            is->id_irq = (1 << sc->config->interruptlevel);         /* Claim our interrupt */
        is->id_intr = (inthand2_t *)oltr_intr;
        if ((is->id_drq == 0xffffffff) && (sc->config->dmalevel != TRLLD_DMA_PIO))
            is->id_drq = sc->config->dmalevel;                      /* Claim our dma channel */
        printf("oltr%d: <%s> [%6D]\n", is->id_unit, AdapterName[sc->config->type], sc->config->macaddress, ":");
        sc->hw_state = HW_FOUND;
        return(1);
    } else {
    /* Assign based on iobase address provided in kernel config */
        for (i = 0; i < NOLTR; i++) {
            if (is->id_iobase == oltr_config[i].iobase0) {
                if (assigned[i]) {
                    printf("oltr%d: adapter (0x%X) already assigned.\n", is->id_unit, is->id_iobase);
                    return(0);
                }
                assigned[i] = 1;
                sc->config = &oltr_config[i];
                if (is->id_irq == 0)
                    is->id_irq = (1 << sc->config->interruptlevel);         /* Claim our interrupt */
                is->id_intr = (inthand2_t *)oltr_intr;
                if ((is->id_drq == 0xffffffff) && (sc->config->dmalevel != TRLLD_DMA_PIO))
                    is->id_drq = sc->config->dmalevel;                      /* Claim our dma channel */
                printf("oltr%d: <%s> [%6D]\n", is->id_unit, AdapterName[sc->config->type], sc->config->macaddress, ":");
                sc->hw_state = HW_FOUND;
                return(1);
            }
        }
    }
    return(0); /* Card was not found */
}

#if NPCI > 0
static const char *
oltr_pci_probe(config_id, device_id)
    pcici_t config_id;
    pcidi_t device_id;
{
    u_char PCIConfigurationSpace[64];
    u_long command;
    int i, j, rc;

    printf("oltr: oltr_pci_probe\n");

    j = NOLTR + pci_cards;

    if (pci_cards == EXTRA_OLTR)
        return(NULL);

    if (((device_id & 0xffff) == PCI_VENDOR_OLICOM) && 
          ((((device_id >> 16) & 0xffff) == 0x0001) || 
           (((device_id >> 16) & 0xffff) == 0x0004) || 
           (((device_id >> 16) & 0xffff) == 0x0005) || 
           (((device_id >> 16) & 0xffff) == 0x0007) || 
           (((device_id >> 16) & 0xffff) == 0x0008))) {

        for (i = 0; i < 64; i++)
            PCIConfigurationSpace[i] = pci_cfgread(config_id, i, /*bytes*/1);

        rc = TRlldPCIConfig(&oltrLldDriver, &oltr_config[j], PCIConfigurationSpace);

        if ((rc == TRLLD_PCICONFIG_OK) || (rc == TRLLD_PCICONFIG_SET_COMMAND)) {
            if (rc == TRLLD_PCICONFIG_SET_COMMAND) {
                printf("oltr: setting bus-master mode\n");
                command = pci_conf_read(config_id, PCIR_COMMAND);
                pci_conf_write(config_id, PCIR_COMMAND, (command | PCIM_CMD_BUSMASTEREN));
            }
            pci_cards++;
            return (AdapterName[oltr_config[j].type]);
        } else {
            if (rc == TRLLD_PCICONFIG_FAIL)
                printf("oltr: TRlldPCIConfig failed!\n");
            if (rc == TRLLD_PCICONFIG_VERSION)
                printf("oltr: wrong LLD version\n");
        }
    }
    return(NULL);
}
#endif /* NPCI */

static int
oltr_attach(is)
    struct isa_device *is;
{
    struct oltr_softc *sc = &oltr_softc[is->id_unit];
    int rc;

    sc->unit = is->id_unit;

    if (!oltr_attach_common(sc))
        return(0);

    /* If the kernel config does not match the current card configuration then
     * adjust the card settings to match the kernel.
     */
    if ((ffs(is->id_irq) - 1) != sc->config->interruptlevel) {
        rc = TRlldSetInterrupt(sc->TRlldAdapter, is->id_irq); 
        if (rc != TRLLD_CONFIG_OK) {  
            printf("oltr%d: Unable to change adapter interrupt level (%x)\n", sc->unit, rc);
            return(0);
        }
    }   
        
    /* Set dma level, fall back to pio if possible. (following SCO driver example) */
    if (is->id_drq != sc->config->dmalevel) {
        rc = TRlldSetDMA(sc->TRlldAdapter, is->id_drq, &sc->config->mode);
        if (rc != TRLLD_CONFIG_OK) {
            if ((sc->config->dmalevel != TRLLD_DMA_PIO) &&
                (TRlldSetDMA(sc->TRlldAdapter, TRLLD_DMA_PIO, &sc->config->mode) != TRLLD_CONFIG_OK)) {
                printf("oltr%d: unable to change dma level from %d to %d (%x)\n", sc->unit,
                        sc->config->dmalevel, is->id_drq, rc);
            }
            printf("oltr%d: Unable to change adapter dma level, using PIO mode (%x)\n", sc->unit, rc);
            sc->config->dmalevel = TRLLD_DMA_PIO;
            rc = TRlldSetDMA(sc->TRlldAdapter, is->id_drq, &sc->config->mode);
        }
        is->id_irq = sc->config->dmalevel;
    }
    return(1);
}

#if NPCI > 0
static void
oltr_pci_attach(config_id, unit)
    pcici_t config_id;
    int unit;
{
    struct oltr_softc *sc = &oltr_softc[unit];

    sc->unit = unit;
    sc->config = &oltr_config[unit];
    sc->hw_state = HW_FOUND;

    printf("oltr%d: mac address [%6D]\n", sc->unit, sc->config->macaddress, ":");

    if (!oltr_attach_common(sc))
        return;

    /* Map our interrupt */
    if (!pci_map_int(config_id, oltr_pci_intr, sc, &net_imask)) {
        printf("oltr%d: couldn't map interrupt\n", unit);
        return;
    }
}
#endif /* NPCI */

static int
oltr_attach_common(sc)
    struct oltr_softc *sc;
{
    struct ifnet *ifp = &sc->arpcom.ac_if;
    u_int  bufsize;
    int    rc, i, j;
  
    /*printf("oltr%d: attach_common called\n", sc->unit);*/

    /* Allocate adapter memory buffer */
    bufsize = TRlldAdapterSize();
    sc->TRlldAdapter = (TRlldAdapter_t *)oltr_malloc(bufsize, sc->config);
    if (sc->TRlldAdapter == NULL) {
        printf("oltr%d: Unable to allocate adapter memory block (%d bytes)\n", sc->unit, bufsize);
    } 
    /*printf("oltr%d: Adapter memory block (%p %d bytes)\n", sc->unit, sc->TRlldAdapter, bufsize);*/

    /* Setup transmit pool */
    for (i = 0; i < TX_LIST_SIZE; i++) {
        sc->tx_buffer[i].index = i;
        sc->tx_buffer[i].buf = (char *)oltr_malloc(TX_BUFFER_LEN, sc->config);
        /* If we have a failure then free everything and get out */
        if (!sc->tx_buffer[i].buf) {
            printf("oltr%d: Unable to allocate transmit buffers.\n", sc->unit);
            for (j = 0; j < i; j++)
                free(sc->tx_buffer[j].buf, M_DEVBUF);
            return(0);
        }
    }
    sc->tx_next = 0;
    sc->tx_avail = TX_LIST_SIZE;
    sc->tx_frame.FragmentCount = 0;

    /* Setup receive pool */
    for (i = 0; i < RX_LIST_SIZE; i++) {
        sc->rx_buffer[i].index = i;
        sc->rx_buffer[i].buf = (char *)oltr_malloc(RX_BUFFER_LEN, sc->config);
        /* If we have a failure then free everything and get out */
        if (!sc->rx_buffer[i].buf) {          
            printf("oltr%d: Unable to allocate receive buffers.\n", sc->unit);
            for (j = 0; j < i; j++)
                free(sc->rx_buffer[j].buf, M_DEVBUF);
            return(0);
        }
    }   
    sc->rx_next = 0;
    sc->rx_avail = RX_LIST_SIZE; 
    /*printf("oltr%d: Allocated receive buffers\n", sc->unit); */
    
    /* Set up adapter polling mechanism */
    sc->poll_adapter = 1;
    callout_handle_init(&sc->poll_ch);
    sc->poll_ch = timeout(adapter_poll, (void *)sc->unit, (1*hz)/1000);
    callout_handle_init(&sc->oltr_ch);

    /* Initialize adapter */
    rc = TRlldAdapterInit(&oltrLldDriver, sc->TRlldAdapter, kvtop(sc->TRlldAdapter), 
                          (void *)sc->unit, sc->config);
    if (rc != TRLLD_INIT_OK) {
        switch (rc) {
            case TRLLD_INIT_NOT_FOUND:
                printf("oltr%d: Adapter not found or malfunctioning.\n", sc->unit);
                sc->hw_state = HW_BAD;
                return(0);
            case TRLLD_INIT_UNSUPPORTED:
                printf("oltr%d: Adapter not supported by low level driver.\n", sc->unit);
                sc->hw_state = HW_UNKNOWN;
                return(0);
            case TRLLD_INIT_PHYS16:
                printf("oltr%d: Adapter memory block above 16M, must be below 16M.\n", sc->unit);
                return(0);
            case TRLLD_INIT_VERSION:
                printf("oltr%d: Low level driver version mismatch.\n", sc->unit);
                return(0);
            default:
                printf("oltr%d: Unknown initilization error occoured (%x).\n", sc->unit, rc);
                return(0);
        }
    }

    /* Download Adapter Microcode */
    /*printf("oltr%d: Downloading adapter microcode...", sc->unit);*/
    sc->hw_state = HW_LOADING;
    switch(sc->config->mactype) {
        case TRLLD_MAC_TMS:           /* TMS microcode */
            rc = TRlldDownload(sc->TRlldAdapter, TRlldMacCode);
            break;
        case TRLLD_MAC_HAWKEYE:        /* Hawkeye microcode */
            rc = TRlldDownload(sc->TRlldAdapter, TRlldHawkeyeMac);
            break;
        case TRLLD_MAC_BULLSEYE:      /* Bullseye microcode */
            rc = TRlldDownload(sc->TRlldAdapter, TRlldBullseyeMac);
            break;
        default:
            printf("oltr%d: unknown mactype %d\n", sc->unit, sc->config->mactype);
            return(0);
    }
    /*if (rc == TRLLD_DOWNLOAD_OK) 
        printf("done\n");*/
    if ((rc == TRLLD_DOWNLOAD_ERROR) || (rc == TRLLD_STATE)) {
        printf("oltr%d: Adapter microcode download failed! (rc = %x)\n", sc->unit, rc);
        sc->hw_state = HW_BAD;
        return(0);     
    }

    TRlldSetSpeed(sc->TRlldAdapter, TRLLD_SPEED_AUTO);

    sc->PromiscMode = 0;
    sc->AdapterMode = 0;

    /* Do the ifnet initialization */
    ifp->if_softc   = sc;
    ifp->if_unit    = sc->unit;
    ifp->if_name    = "oltr";
    ifp->if_output  = iso88025_output;
    ifp->if_init    = (if_init_f_t *)oltr_init;
    ifp->if_start   = oltr_start;
    ifp->if_ioctl   = oltr_ioctl;
    ifp->if_flags   = IFF_BROADCAST | IFF_MULTICAST | IFF_SIMPLEX;
    bcopy(sc->config->macaddress, sc->arpcom.ac_enaddr, sizeof(sc->config->macaddress));

    /* Set up common ifmedia options */
    ifmedia_init(&sc->ifmedia, 0, oltr_ifmedia_upd, oltr_ifmedia_sts);

    ifmedia_add(&sc->ifmedia, IFM_TOKEN | IFM_AUTO, 0 , NULL);
    ifmedia_add(&sc->ifmedia, IFM_TOKEN | IFM_TOK_UTP4, 0 , NULL);
    ifmedia_add(&sc->ifmedia, IFM_TOKEN | IFM_TOK_UTP16, 0 , NULL);
    
    ifmedia_set(&sc->ifmedia, IFM_TOKEN | IFM_AUTO);

    if_attach(ifp);
    iso88025_ifattach(ifp);

#if NBPFILTER > 0
    bpfattach(ifp, DLT_IEEE802, sizeof(struct iso88025_header));
#endif

    printf("oltr%d: Adapter modes - ", sc->unit);
    if (sc->config->mode & TRLLD_MODE_16M) printf("TRLLD_MODE_16M ");
    if (sc->config->mode & TRLLD_MODE_PHYSICAL) printf("TRLLD_MODE_PHYSICAL ");
    if (sc->config->mode & TRLLD_MODE_FIXED_CFG) printf("TRLLD_MODE_FIXED_CFG ");
    if (sc->config->mode & TRLLD_MODE_SHORT_SLOT) printf("TRLLD_MODE_SHORT_SLOT ");
    if (sc->config->mode & TRLLD_MODE_CANNOT_DISABLE) printf("TRLLD_MODE_CANNOT_DISABLE ");
    if (sc->config->mode & TRLLD_MODE_SHARE_INTERRUPT) printf("TRLLD_MODE_SHARE_INTERRUPT ");
    if (sc->config->mode & TRLLD_MODE_MEMORY) printf("TRLLD_MODE_MEMORY ");
    printf("\n");

    return(1);
}

#if NPCI > 0
static void
oltr_pci_shutdown(howto, sc)
    int howto;
    void *sc;
{
    printf("oltr: oltr_pci_shutdown called\n");
}
#endif /* NPCI */

static int
oltr_ifmedia_upd(ifp)
    struct ifnet *ifp;
{
    struct oltr_softc *sc = ifp->if_softc;
    struct ifmedia *ifm   = &sc->ifmedia;

    if (IFM_TYPE(ifm->ifm_media) != IFM_TOKEN)
        return(EINVAL);
    
    switch(IFM_SUBTYPE(ifm->ifm_media)) {
        case IFM_AUTO:
            TRlldSetSpeed(sc->TRlldAdapter, TRLLD_SPEED_AUTO);
            break;
        case IFM_TOK_UTP4:
            TRlldSetSpeed(sc->TRlldAdapter, TRLLD_SPEED_4MBPS);
            break;
        case IFM_TOK_UTP16:
            TRlldSetSpeed(sc->TRlldAdapter, TRLLD_SPEED_16MBPS);
            break;
        default:
            return(EINVAL);
    }

    if (IFM_TYPE_OPTIONS(ifm->ifm_media) & IFM_TOK_ETR)
        printf("oltr%d: ETR not implemented\n", sc->unit);
    if (IFM_TYPE_OPTIONS(ifm->ifm_media) & IFM_TOK_SRCRT)
        printf("oltr%d: source-routing not implemented\n", sc->unit);
    if (IFM_TYPE_OPTIONS(ifm->ifm_media) & IFM_TOK_ALLR)
        printf("oltr%d: all source routes not implemented\n", sc->unit);
    if (IFM_TYPE_OPTIONS(ifm->ifm_media) & IFM_TOK_DTR) {
        sc->AdapterMode |= TRLLD_MODE_FORCE_TXI;
        sc->AdapterMode &= ~TRLLD_MODE_FORCE_TKP;
    }
    if (IFM_TYPE_OPTIONS(ifm->ifm_media) & IFM_TOK_CLASSIC) {
        sc->AdapterMode |= TRLLD_MODE_FORCE_TKP;
        sc->AdapterMode &= ~TRLLD_MODE_FORCE_TXI;
    }
    if (IFM_TYPE_OPTIONS(ifm->ifm_media) & IFM_TOK_AUTO)
        sc->AdapterMode &= ~(TRLLD_MODE_FORCE_TXI | TRLLD_MODE_FORCE_TKP);

    if (IFM_TYPE_OPTIONS(ifm->ifm_media) & ~ALL_OPTIONS)
        return(EINVAL);

    return(0);
}

static void
oltr_ifmedia_sts(ifp, ifmr)
    struct ifnet *ifp;
    struct ifmediareq *ifmr;
{
    struct oltr_softc *sc = ifp->if_softc;
    struct ifmedia *ifm = &sc->ifmedia;

    ifmr->ifm_active = IFM_TYPE(ifm->ifm_media)|IFM_SUBTYPE(ifm->ifm_media)|IFM_TYPE_OPTIONS(ifm->ifm_media);

    return;
}

void
oltr_timeout(token)
    void *token;
{
    struct oltr_softc *sc = &oltr_softc[(int)token];
    int unit = (int)token, s;

    s = splimp();

    printf("oltr%d: adapter timed out (%x)\n", unit, sc->hw_state);

    splx(s);
}


void
adapter_poll(token)
    void *token;
{
    int unit = (int)token, poll_timeout = 0, s;
    struct oltr_softc *sc = &oltr_softc[unit];
#if 0
    static int rx_buffers = 0, tx_buffers = 0, rc; 
#endif
    
    s = splimp();

    /* Check to make sure we are not polling a dead card */
    if ((sc->hw_state == HW_BAD) || (sc->hw_state == HW_UNKNOWN)) {
        sc->poll_adapter = -1;
        splx(s);
        return;
    }

    /*printf("oltr%d: adapter poll.\n", unit);*/
    
    /* If the adapter is to be polled again, then set up
     * next timeout poll 
     */
    if (sc->poll_adapter) {
        poll_timeout = TRlldPoll(sc->TRlldAdapter);
        sc->poll_ch = timeout(adapter_poll, (void *)unit, (poll_timeout * hz)/1000);
    }
#if 0
    rc = TRlldReceiveFree(sc->TRlldAdapter);
    if (rx_buffers != rc) {
        printf("oltr%d: %d receive buffers available\n", sc->unit, rc);
        rx_buffers = rc;
    } 
    rc = TRlldTransmitFree(sc->TRlldAdapter);
    if (tx_buffers != rc) {
        printf("oltr%d: %d transmit buffers available\n", sc->unit, rc);
        tx_buffers = rc;
    } 
#endif

    splx(s);            
}

static void 
oltr_init(sc)
    struct oltr_softc *sc;
{
    struct ifnet *ifp = &sc->arpcom.ac_if;
    int i, rc;

    /*printf("oltr%d: oltr_init\n", sc->unit);*/
    
    /* 
     * Adapter should be freshly downloaded or previously closed before
     * bringing it back on line.
     */
    if ((sc->hw_state != HW_CLOSED) && (sc->hw_state != HW_LOADING) && (sc->hw_state != HW_CLOSING2)) {
        printf("oltr%d: adapter not ready to be opened (%d).\n", sc->unit, sc->hw_state);
        return;
    }

    /* Allocate and set up the DMA channel */
    if (sc->config->dmalevel != TRLLD_DMA_PIO) {
        rc = isa_dma_acquire(sc->config->dmalevel);
        isa_dmacascade(sc->config->dmalevel);
    }

    /* Open the adapter */
    sc->hw_state = HW_OPENING;
    rc = TRlldOpen(sc->TRlldAdapter, sc->arpcom.ac_enaddr, sc->GroupAddress, 
                   sc->FunctionalAddress, ifp->if_mtu + 52, sc->AdapterMode);
    if (rc != TRLLD_OPEN_OK) {
        printf("oltr%d: Adapter failed to open (rc = %x)\n", sc->unit, rc);
        sc->hw_state = HW_FAILED;
    } else {
        /*printf("oltr%d: adapter opening...\n", sc->unit);*/
        /*ifp->if_flags |= (IFF_UP | IFF_RUNNING);*/
        ifp->if_flags &= ~IFF_OACTIVE;
    }
    sc->oltr_ch = timeout(oltr_timeout, (void *)sc->unit, 30*hz);
    tsleep((void *)sc->unit, 1, "oltrop", 30*hz);

    /* Give the receive buffers to the adapter */
    for (i = 0; i < RX_LIST_SIZE; i++) { 
        rc = TRlldReceiveFragment(sc->TRlldAdapter,
                                  (void *)sc->rx_buffer[sc->rx_next & RX_LIST_MASK].buf,
                                  kvtop(sc->rx_buffer[sc->rx_next & RX_LIST_MASK].buf),
                                  RX_BUFFER_LEN,
                                  (void *)sc->rx_buffer[sc->rx_next & RX_LIST_MASK].index);
        if (rc != TRLLD_RECEIVE_OK) {
            printf("oltr%d: Adapter refused fragment %d (rc = %d).\n", sc->unit, i, rc);
            break;
        }  else {
            sc->rx_avail--;
        }
        sc->rx_next++;
    }
    sc->tx_frame.FragmentCount = 0;
    
    return;
}
    
static void
oltr_intr(unit)
    int unit;
{
    struct oltr_softc *sc = &oltr_softc[unit];
    int rc;

    /*printf("oltr%d: oltr_intr\n", unit);*/ /* Too noisy */
    rc= TRlldInterruptService(sc->TRlldAdapter);
    if (rc == TRLLD_NO_INTERRUPT) 
        printf("oltr%d: interrupt not serviced.\n", unit);
}

#if NPCI > 0
static void
oltr_pci_intr(psc)
    void *psc;
{
    struct oltr_softc *sc = (struct oltr_softc *)psc;
    int rc = 0;

    /*printf("oltr%d: oltr_pci_intr\n", sc->unit);*/ /* Too noisy */
    rc = TRlldInterruptService(sc->TRlldAdapter);
    if (rc == TRLLD_NO_INTERRUPT)
        printf("oltr%d: pci interrupt not serviced.\n", sc->unit);
}
#endif /* NPCI */

static void 
oltr_start(ifp)
    struct ifnet *ifp;
{
    struct oltr_softc *sc = &oltr_softc[ifp->if_unit];
    struct mbuf *m0, *m;
    int  len, i, k, rc;

    /*printf("oltr%d: oltr_start\n", sc->unit);*/

outloop:

    i = (sc->tx_next  & TX_LIST_MASK); /* Just to shorten thing up */

    /* Check to see if we have enough room to transmit */
    if (sc->tx_avail <= 0) {
        /* No free buffers, hold off the upper layers */
        /*printf("oltr%d: transmit queue full.\n", sc->unit);*/
        ifp->if_flags |= IFF_OACTIVE;
        return;
    }

    if (sc->tx_frame.FragmentCount > 0) {
        if (!(sc->config->mode & TRLLD_MODE_16M)) {
            sc->tx_next++;
            m0 = sc->tx_buffer[i].m;
            goto restart;
        }
    }

    IF_DEQUEUE(&ifp->if_snd, m);
    if (m == 0) {
        /*printf("oltr%d: oltr_start NULL packet dequeued.\n", sc->unit);*/
        ifp->if_flags &= ~IFF_OACTIVE;
        return;
    }

    /* Keep a pointer to the head of the packet */
    m0 = m;

    if (sc->config->mode & TRLLD_MODE_16M) {  /*  ISA Adapters - bounce buffers */

        for (len = 0; m != 0; m = m->m_next) {
            sc->tx_frame.TransmitFragment[0].VirtualAddress = sc->tx_buffer[i].buf;
            sc->tx_frame.TransmitFragment[0].PhysicalAddress = kvtop(sc->tx_buffer[i].buf);
            bcopy(mtod(m, caddr_t), sc->tx_buffer[i].buf + len, m->m_len);
            len += m->m_len;
        }
        sc->tx_frame.FragmentCount = 1;
        sc->tx_frame.TransmitFragment[0].count = len;

        sc->tx_next++;
        sc->tx_avail--;

    } else {                                  /* PCI Adapters w/DMA */

        for (k = 0; m!= 0; m = m->m_next) {
            sc->tx_frame.TransmitFragment[k].VirtualAddress = mtod(m, caddr_t);
            sc->tx_frame.TransmitFragment[k].PhysicalAddress = kvtop(mtod(m, caddr_t));
            sc->tx_frame.TransmitFragment[k].count = m->m_len;
            k++;
            sc->tx_avail--;
        }
        sc->tx_frame.FragmentCount = k;
        sc->tx_buffer[i].count = k;
        sc->tx_buffer[i].m = m0;

        if (sc->tx_avail < 0) {
            /*printf("oltr%d: transmit buffers exhausted.\n", sc->unit);*/
            goto nobuffers;
        }
        sc->tx_next++;
    }

restart:
    rc = TRlldTransmitFrame(sc->TRlldAdapter, &sc->tx_frame, (void *)sc->tx_buffer[i].index);
    sc->tx_frame.FragmentCount = 0;

    if (rc != TRLLD_TRANSMIT_OK) {
        printf("oltr%d: TRlldTransmitFrame returned (%x)\n", sc->unit, rc);
        ifp->if_oerrors++;
        goto bad;
    }

#if NBPFILTER > 0
    if (ifp->if_bpf)
        bpf_mtap(ifp, m0);
#endif

bad:

    if (sc->config->mode & TRLLD_MODE_16M) {
        m_freem(m0);
    }

    goto outloop;

nobuffers:

    ifp->if_flags |= IFF_OACTIVE;

    return;
}

static void
oltr_stop(sc)
    struct oltr_softc *sc;
{
    struct ifnet *ifp = &sc->arpcom.ac_if;
    printf("oltr%d: otlr_stop\n", sc->unit);
    ifp->if_flags &= ~(IFF_UP | IFF_RUNNING | IFF_OACTIVE);
    sc->hw_state = HW_CLOSING;
    TRlldClose(sc->TRlldAdapter, 0);
    sc->oltr_ch = timeout(oltr_timeout, (void *)sc->unit, 30*hz);
    tsleep((void *)sc->unit, 1, "oltrcl", 30*hz);
}

static int
oltr_ioctl(ifp, cmd, data)
    struct ifnet *ifp;
    u_long cmd;
    caddr_t data;
{
    struct oltr_softc *sc = &oltr_softc[ifp->if_unit];
    struct ifreq *ifr = (struct ifreq *)data;
    int error = 0, s;

    /*printf("oltr%d: oltr_ioctl\n", ifp->if_unit);*/

    s = splimp();

    switch (cmd) {
   
        case SIOCSIFADDR:
        case SIOCGIFADDR:
        case SIOCSIFMTU:
            error = iso88025_ioctl(ifp, cmd, data);
            break;

        case SIOCSIFFLAGS:
            /*
             * If the interface is marked up and stopped, then start it.
             * If it is marked down and running, then stop it.
             */
            if (ifp->if_flags & IFF_UP) {
                    if ((ifp->if_flags & IFF_RUNNING) == 0)
                            oltr_init(sc);
            } else {
                    if (ifp->if_flags & IFF_RUNNING) {
                            oltr_stop(sc);
                            ifp->if_flags &= ~IFF_RUNNING;
                    }
            }

            if ((ifp->if_flags & IFF_PROMISC) != sc->PromiscMode) {
                if (ifp->if_flags & IFF_PROMISC)
                    TRlldSetPromiscuousMode(sc->TRlldAdapter, OLTR_PROMISC_MODE);
                else
                    TRlldSetPromiscuousMode(sc->TRlldAdapter, 0);
                sc->PromiscMode = (ifp->if_flags & IFF_PROMISC);
            }
            
            break;
        case SIOCGIFMEDIA:
        case SIOCSIFMEDIA:
            error = ifmedia_ioctl(ifp, ifr, &sc->ifmedia, cmd);
            break;
        default:
            error = EINVAL;
    }
    splx(s);
    return(error);
}

/*
 * PMW Callback functions ----------------------------------------------------
 */

static void
DriverSuspend(MicroSeconds)
    unsigned short MicroSeconds;
{
    DELAY(MicroSeconds);
}


static void
DriverStatus(DriverHandle, Status)
    void *DriverHandle;
    TRlldStatus_t *Status;
{
    struct oltr_softc *sc = &oltr_softc[(int)DriverHandle];
    struct ifnet *ifp = &sc->arpcom.ac_if;

    switch (Status->Type) {
        case TRLLD_STS_ON_WIRE:
            if (sc->hw_state == HW_OPENING) {
                sc->hw_state = HW_OPEN;
                ifp->if_flags |= (IFF_UP | IFF_RUNNING);
                /*printf("oltr%d: Adapter inserted.\n", sc->unit);*/
                untimeout(oltr_timeout, (void *)sc->unit, sc->oltr_ch);
                wakeup_one((void *)sc->unit);
            }
            break;
        case TRLLD_STS_SELFTEST_STATUS:
            if (Status->Specification.SelftestStatus == TRLLD_ST_OK) {
                printf("oltr%d: adapter status good. (close completed/self-test)\n", sc->unit);
                if ((sc->hw_state == HW_LOADING) || (sc->hw_state == HW_CLOSING) || (sc->hw_state == HW_CLOSING2)) {
                    sc->hw_state = HW_CLOSED;
                    break;
                }
            } else {
                printf("oltr%d: Self test failed: ", sc->unit);
                switch (Status->Specification.SelftestStatus) {
                    case TRLLD_ST_ERROR + 0: printf("Initial Test Error\n"); break;
                    case TRLLD_ST_ERROR + 1: printf("Adapter Software Checksum Error\n"); break;
                    case TRLLD_ST_ERROR + 2: printf("Adapter RAM Error\n"); break;
                    case TRLLD_ST_ERROR + 4: printf("Instruction Test Error\n"); break;
                    case TRLLD_ST_ERROR + 5: printf("Protocol Handler/RI Hw Error\n"); break;
                    case TRLLD_ST_ERROR + 6: printf("System Interface Register Error\n"); break;
                    case TRLLD_ST_TIMEOUT:   printf("Selftest did not complete\n"); break;
                    default: printf("Unknown error (%x)\n", Status->Specification.SelftestStatus);
                }
            }
            break;
        case TRLLD_STS_INIT_STATUS:
            printf("oltr%d: Adapter initialization failed: ", sc->unit);
            switch(Status->Specification.InitStatus) {
                case TRLLD_INIT_ERROR + 0x01: printf("Invalid init block (LLD error)\n"); break;
                case TRLLD_INIT_ERROR + 0x02: printf("Invalid options (LLD error)\n"); break;
                case TRLLD_INIT_ERROR + 0x03: printf("Invalid rcv burst (LLD error)\n"); break;
                case TRLLD_INIT_ERROR + 0x04: printf("Invalid xmt burst (LLD error)\n"); break;
                case TRLLD_INIT_ERROR + 0x05: printf("Invalid DMA threshold (LLD error)\n"); break;
                case TRLLD_INIT_ERROR + 0x06: printf("Invalid scb addr\n"); break;
                case TRLLD_INIT_ERROR + 0x07: printf("Invalid ssb addr\n"); break;
                case TRLLD_INIT_ERROR + 0x08: printf("DIO parity error (HW error)\n"); break;
                case TRLLD_INIT_ERROR + 0x09: printf("DMA timeout (May be interrupt failing if PIO mode or PCI2)\n"); break;
                case TRLLD_INIT_ERROR + 0x0A: printf("DMA parity error (HW error)\n"); break;
                case TRLLD_INIT_ERROR + 0x0B: printf("DMA bus error (HW error)\n"); break;
                case TRLLD_INIT_ERROR + 0x0C: printf("DMA data error\n"); break;
                case TRLLD_INIT_ERROR + 0x0D: printf("Adapter Check\n"); break;
                case TRLLD_INIT_TIMEOUT:      printf("Adapter initialization did not complete\n"); break;
                case TRLLD_INIT_DMA_ERROR:    printf("Adapter cannot access system memory\n"); break;
                case TRLLD_INIT_INTR_ERROR:   printf("Adapter cannot interrupt\n"); break;
                case TRLLD_OPEN_TIMEOUT:      printf("Adapter did not complete open within 30 seconds\n"); break;
                case TRLLD_OPEN_ERROR + 0x01: printf("Invalid open options (LLD error)\n"); break;
                case TRLLD_OPEN_ERROR + 0x04: printf("TxBuffer count error (LLD error)\n"); break;
                case TRLLD_OPEN_ERROR + 0x10: printf("Buffer size error (LLD error)\n"); break;
                case TRLLD_OPEN_ERROR + 0x20: printf("List size error (LLD error)\n"); break;
                default: 
                    if (Status->Specification.InitStatus & 0x700) {
                        switch (Status->Specification.InitStatus & 0x70F) {
                            case TRLLD_OPEN_REPEAT + 0x01: printf("Lobe media test - "); break;
                            case TRLLD_OPEN_REPEAT + 0x02: printf("Physical insertion - "); break;
                            case TRLLD_OPEN_REPEAT + 0x03: printf("Address verification - "); break;
                            case TRLLD_OPEN_REPEAT + 0x04: printf("Participation in ring poll - "); break;
                            case TRLLD_OPEN_REPEAT + 0x05: printf("Request initialization - "); break;
                            case TRLLD_OPEN_REPEAT + 0x09: printf("Request registration (TXI) - "); break;
                            case TRLLD_OPEN_REPEAT + 0x0A: printf("Lobe media test (TXI) - "); break;
                            default:                       printf("Unknown phase (%x) - ", Status->Specification.InitStatus & 0x00F);
                        }
                        switch (Status->Specification.InitStatus & 0x7F0) {
                            case TRLLD_OPEN_REPEAT + 0x10: printf("Function failure (No cable?)\n"); break;
                            case TRLLD_OPEN_REPEAT + 0x20: printf("Signal loss\n"); break;
                            case TRLLD_OPEN_REPEAT + 0x50: printf("Timeout\n"); break;
                            case TRLLD_OPEN_REPEAT + 0x60: printf("Ring failure (TKP) / Protocol error (TXI)\n"); break;
                            case TRLLD_OPEN_REPEAT + 0x70: printf("Ring beaconing\n"); break;
                            case TRLLD_OPEN_REPEAT + 0x80: printf("Duplicate node address (TKP) / Insert denied (TXI)\n"); break;
                            case TRLLD_OPEN_REPEAT + 0x90: printf("Request initialization (TKP)\n"); break;
                            case TRLLD_OPEN_REPEAT + 0xa0: printf("Remove received\n"); break;
                            case TRLLD_OPEN_REPEAT + 0xb0: printf("C-port address changed (TXI)\n"); break;
                            default:                       printf("Unknown type (%x)\n", Status->Specification.InitStatus & 0x0F0);
                        }
                    } else {
                        printf("Unknown error (%x)\n", Status->Specification.InitStatus); 
                    }
            }
            break;
        case TRLLD_STS_RING_STATUS:
            if (Status->Specification.RingStatus != 0) {
                printf("oltr%d: Ring status change: ", sc->unit);
                if (Status->Specification.RingStatus & TRLLD_RS_HARD_ERROR)         printf("[Hard error] ");
                if (Status->Specification.RingStatus & TRLLD_RS_SOFT_ERROR)         printf("[Soft error] ");
                if (Status->Specification.RingStatus & TRLLD_RS_TRANSMIT_BEACON)    printf("[Transmit beacon] ");
                if (Status->Specification.RingStatus & TRLLD_RS_LOBE_WIRE_FAULT)    printf("[Wire fault] ");
                if (Status->Specification.RingStatus & TRLLD_RS_AUTO_REMOVAL_ERROR) printf("[Auto removal] ");
                if (Status->Specification.RingStatus & TRLLD_RS_REMOVE_RECEIVED)    printf("[Remove received] ");
                if (Status->Specification.RingStatus & TRLLD_RS_COUNTER_OVERFLOW)   printf("[Counter overflow] ");
                if (Status->Specification.RingStatus & TRLLD_RS_SINGLE_STATION)     printf("[Single station] ");
                if (Status->Specification.RingStatus & TRLLD_RS_RING_RECOVERY)      printf("[Ring recovery] ");
                printf("\n");
            }
            break;
        case TRLLD_STS_ADAPTER_CHECK:
            printf("oltr%d: Adapter check (%x %x %x %x)\n", sc->unit, Status->Specification.AdapterCheck[0], 
                    Status->Specification.AdapterCheck[1], Status->Specification.AdapterCheck[2], 
                    Status->Specification.AdapterCheck[3]); 
            break;
        case TRLLD_STS_PROMISCUOUS_STOPPED:
            printf("oltr%d: Promiscuous mode stopped: ", sc->unit);
            switch(Status->Specification.PromRemovedCause) {
                case TRLLD_PROM_REMOVE_RECEIVED: printf("Remove received\n"); break;
                case TRLLD_PROM_POLL_FAILURE:    printf("Poll failure\n"); break;
                default:                         printf("Unknown (%x)\n", Status->Specification.PromRemovedCause);
            }
            break;
        case TRLLD_STS_LLD_ERROR:
            printf("oltr%d: LLD error (%x %x %x %x) ", sc->unit, Status->Specification.InternalError[0], 
                    Status->Specification.InternalError[1], Status->Specification.InternalError[2], 
                    Status->Specification.InternalError[3]);
            break;
        case TRLLD_STS_ADAPTER_TIMEOUT:
            printf("oltr%d: Adapter operation timed out: ", sc->unit);
            switch(Status->Specification.AdapterTimeout) {
                case TRLLD_COMMAND_TIMEOUT:   printf("Command\n");
                case TRLLD_TRANSMIT_TIMEOUT:  printf("Transmit\n");
                case TRLLD_INTERRUPT_TIMEOUT: printf("Interrupt\n");
                default:                      printf("Unknown (%x)\n", Status->Specification.AdapterTimeout);
            }
            break;
        default:
            printf("oltr%d: Unknown status type (%x)\n", sc->unit, Status->Type);

    }
    if (Status->Closed) {
        if (sc->hw_state > HW_BAD) {
            sc->hw_state = HW_FAILED;
            printf("oltr%d: closing adapter due to failure.\n", sc->unit);
            oltr_stop(sc);
        }
    }
}

static void
DriverCloseCompleted(DriverHandle)
    void *DriverHandle;
{
    struct oltr_softc *sc = &oltr_softc[(int)DriverHandle];

    printf("oltr%d: DriverCloseCompleted\n", sc->unit);

    untimeout(oltr_timeout, (void *)sc->unit, sc->oltr_ch);
    wakeup_one((void *)sc->unit);

    if ((sc->hw_state != HW_CLOSING) && (sc->hw_state != HW_CLOSING2) && (sc->hw_state != HW_CLOSED)) {
        printf("oltr%d: adapter close complete called in wrong state (%d)\n", sc->unit, sc->hw_state);
        return;
    }
    sc->hw_state = HW_CLOSING2;
    if (sc->config->dmalevel != TRLLD_DMA_PIO)
        isa_dma_release(sc->config->dmalevel);
    
}

static void
DriverStatistics(DriverHandle, Statistics)
    void *DriverHandle;
    TRlldStatistics_t *Statistics;
{
    printf("oltr: DriverStatistics\n");
}

static void
DriverTransmitFrameCompleted(DriverHandle, FrameHandle, TransmitStatus)
    void *DriverHandle;
    void *FrameHandle;
    int TransmitStatus;
{
    int frame = (int)FrameHandle;
    struct oltr_softc *sc = &oltr_softc[(int)DriverHandle];
    struct ifnet *ifp = &sc->arpcom.ac_if;

    /*printf("oltr%d: transmit complete frame %d\n", sc->unit, frame);*/
    if (TransmitStatus == TRLLD_TRANSMIT_OK) {
        ifp->if_opackets++;
    } else {
        printf("oltr%d: DriverTransmitFrameCompleted (frame %d status %x)\n", sc->unit, frame, TransmitStatus);
        ifp->if_oerrors++;
    }

    if ((frame < 0) || (frame > TX_LIST_SIZE)) {
        printf("oltr%d: bogus transmit frame. (%d)\n", sc->unit, frame);
        return;
    }

    if (sc->config->mode & TRLLD_MODE_16M) {
        sc->tx_avail++;
    } else {
        m_freem(sc->tx_buffer[frame].m);
        sc->tx_avail += sc->tx_buffer[frame].count;
    }

    if ((ifp->if_flags & IFF_OACTIVE) && (sc->tx_avail > 0)) {
        ifp->if_flags &= ~(IFF_OACTIVE);
        oltr_start(ifp);
    }

}

static void
DriverReceiveFrameCompleted(DriverHandle, ByteCount, FragmentCount, FragmentHandle, ReceiveStatus)
    void *DriverHandle;
    int ByteCount;
    int FragmentCount;
    void *FragmentHandle;
    int ReceiveStatus;
{
    struct oltr_softc *sc = &oltr_softc[(int)DriverHandle];
    struct ifnet *ifp = &sc->arpcom.ac_if;
    struct iso88025_header *th;
    struct mbuf *m0, *m1, *m;
    int j = (int)FragmentHandle, rc, frame_len = ByteCount, mac_hdr_len;
    int mbuf_offset, mbuf_size, frag_offset, length;
    char *frag = sc->rx_buffer[j].buf;

    /*printf("oltr%d: ReceiveFrameCompleted (Size %d Count %d Start %d)\n", sc->unit, ByteCount, FragmentCount, j);*/

    if (sc->hw_state >=  HW_OPEN) {             /* Hardware operating normally */
        if (frag != sc->rx_buffer[sc->rx_next & RX_LIST_MASK].buf) {
            printf("oltr%d: ring buffer pointer blown\n", sc->unit);
            oltr_stop(sc);
            return;
        }
        if (ReceiveStatus == TRLLD_RCV_OK) {    /* Receive good frame */
            MGETHDR(m0, M_DONTWAIT, MT_DATA);
            mbuf_size = MHLEN;
            if (m0 == NULL) {
                ifp->if_ierrors++;
                goto out;
            }
            if (ByteCount + 2 > MHLEN) {
                MCLGET(m0, M_DONTWAIT);
                mbuf_size = MCLBYTES;
                if ((m0->m_flags & M_EXT) == 0) {
                    m_freem(m0);
                    ifp->if_ierrors++;
                    goto out;
                }
            }

            m0->m_pkthdr.rcvif = &sc->arpcom.ac_if;
            m0->m_pkthdr.len = ByteCount;
            m0->m_len = 0;
            m0->m_data += 2;
            mbuf_size -=2;
            th = mtod(m0, struct iso88025_header *);
            m0->m_pkthdr.header = (void *)th;

            m = m0; mbuf_offset = 0; frag_offset = 0;
            while (frame_len > 0) {
                length = MIN3(frame_len, (RX_BUFFER_LEN - frag_offset), (mbuf_size - mbuf_offset));
                bcopy(frag + frag_offset, mtod(m, char *) + mbuf_offset, length);
                m->m_len += length;
                mbuf_offset += length;
                frag_offset += length;
                frame_len -= length;
                if (frag_offset == RX_BUFFER_LEN) {
                    frag = sc->rx_buffer[++j].buf;
                    frag_offset = 0;
                }
                if ((mbuf_offset == mbuf_size) && (frame_len > 0)) {
                    MGET(m1, M_DONTWAIT, MT_DATA);
                    mbuf_size = MHLEN;
                    if (m1 == NULL) {
                        ifp->if_ierrors++;
                        m_freem(m0);
                        goto out;
                    }  
                    if (frame_len > MHLEN) {
                        MCLGET(m1, M_DONTWAIT);
                        mbuf_size = MCLBYTES;
                        if ((m1->m_flags & M_EXT) == 0) {
                            m_freem(m0);
                            m_freem(m1);
                            ifp->if_ierrors++;
                            goto out;
                        }
                    }
                    m->m_next = m1;
                    m = m1;
                    mbuf_offset = 0;
                    m->m_len = 0;
                }
            }
            ifp->if_ipackets++;
        
#if NBPFILTER > 0
            if (ifp->if_bpf)
                bpf_mtap(ifp, m0);
#endif

            if (ifp->if_flags & IFF_PROMISC)
                if (bcmp(th->iso88025_dhost, etherbroadcastaddr, sizeof(th->iso88025_dhost)) != 0) {
                    if (((th->iso88025_dhost[0] & 0x7f) != sc->arpcom.ac_enaddr[0]) ||
                        (bcmp(th->iso88025_dhost + 1, sc->arpcom.ac_enaddr + 1, ISO88025_ADDR_LEN - 1))) {
	                m_freem(m0);
                        goto out;
                    }
            }

            mac_hdr_len = ISO88025_HDR_LEN;
            if (th->iso88025_shost[0] & 0x80) /* Check for source routing info */
                mac_hdr_len +=  (ntohs(th->rcf) & 0x1f00) >> 8;
            
            m0->m_pkthdr.len -= mac_hdr_len;
            m0->m_len -= mac_hdr_len;
            m0->m_data += mac_hdr_len;

            iso88025_input(&sc->arpcom.ac_if, th, m0);

        } else {
            if (ReceiveStatus != TRLLD_RCV_NO_DATA) {
                printf("oltr%d: receive error. (ReceiveStatus=%d)\n", sc->unit, ReceiveStatus);
                ifp->if_ierrors++;
            }
        }
out:
        while (FragmentCount > 0) {
            rc = TRlldReceiveFragment(sc->TRlldAdapter, 
                                     (void *)sc->rx_buffer[sc->rx_next & RX_LIST_MASK].buf,
                                     kvtop(sc->rx_buffer[sc->rx_next & RX_LIST_MASK].buf),
                                     RX_BUFFER_LEN,
                                     (void *)sc->rx_buffer[sc->rx_next & RX_LIST_MASK].index);
            if (rc == TRLLD_RECEIVE_OK) {
                sc->rx_next++;
                FragmentCount--;
            } else {
                printf("oltr%d: Adapter refused fragment (%d).\n", sc->unit, sc->rx_next - 1);
                sc->rx_avail += FragmentCount;
                break;
            }
        }
    } else {                                    /* Hardware being closed */
        if (frag != sc->rx_buffer[sc->rx_next++ & RX_LIST_MASK].buf) {
            printf("oltr%d: ring buffer pointer blown\n", sc->unit);  
        }           
        sc->rx_avail += FragmentCount;
    }
    
}


/*
 * ---------------------------- PMW Glue -------------------------------
 */

#ifndef TRlldInlineIO

static void 
DriverOutByte(IOAddress, value)
    unsigned short IOAddress;
    unsigned char value;
{
    outb(IOAddress, value);
}

static void
DriverOutWord(IOAddress, value)
    unsigned short IOAddress;
    unsigned short value;
{
    outw(IOAddress, value);
}

static void
DriverOutDword(IOAddress, value)
    unsigned short IOAddress;
    unsigned long value;
{
    outl(IOAddress, value);
}

static void
DriverRepOutByte(IOAddress, DataPointer, ByteCount)
    unsigned short IOAddress;
    unsigned char  *DataPointer;
    int ByteCount;
{
    outsb(IOAddress, (void *)DataPointer, ByteCount);
}

static void
DriverRepOutWord(IOAddress, DataPointer, WordCount)
    unsigned short IOAddress;
    unsigned short *DataPointer;
    int WordCount;
{
    outsw(IOAddress, (void *)DataPointer, WordCount);
}

static void
DriverRepOutDword(IOAddress, DataPointer, DWordCount)
    unsigned short IOAddress;
    unsigned long  *DataPointer;
    int DWordCount;
{
    outsl(IOAddress, (void *)DataPointer, DWordCount);
}

static unsigned char
DriverInByte(IOAddress)
    unsigned short IOAddress;
{
    return(inb(IOAddress));
}

static unsigned short
DriverInWord(IOAddress)
    unsigned short IOAddress;
{
   return(inw(IOAddress));
}

static unsigned long
DriverInDword(IOAddress)
    unsigned short IOAddress;
{
    return(inl(IOAddress));
}

static void
DriverRepInByte(IOAddress, DataPointer, ByteCount)
    unsigned short IOAddress;
    unsigned char  *DataPointer;
    int ByteCount;
{
    insb(IOAddress, (void *)DataPointer, ByteCount);
}

static void
DriverRepInWord(IOAddress, DataPointer, WordCount)
    unsigned short IOAddress;
    unsigned short *DataPointer;
    int WordCount;
{
    insw(IOAddress, (void *)DataPointer, WordCount);
}
static void
DriverRepInDword(IOAddress, DataPointer, DWordCount)
    unsigned short IOAddress;
    unsigned long  *DataPointer;
    int DWordCount;
{
    insl(IOAddress, (void *)DataPointer, DWordCount);
}
#endif /* TRlldInlineIO */

#endif /* NOLTR */
