/*
 * Copyright 1996 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	From: wd82371.c,v 1.5.2.1 1996/11/16 21:19:51 phk Exp $
 *	$Id$
 */

#include "pci.h"
#if NPCI > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/pmap.h>	/* for vtophys */

#include <i386/isa/wdreg.h>

#include <pci/pcivar.h>
#include <pci/pcireg.h>
#include <pci/ide_pcireg.h>

struct ide_pci_cookie;  /* structs vendor_fns, ide_pci_cookie are recursive */

struct vendor_fns {
	int (*vendor_dmainit)   /* initialize DMA controller and drive */
	(struct ide_pci_cookie *cookie,
	 struct wdparams *wp, 
	 int(*wdcmd)(int, void *),
	 void *);
	
	void (*vendor_status) /* prints off DMA timing info */
	(int iobase_wd, 
	 int unit, 
	 int iobase_bm, 
	 pcici_t tag, 
	 pcidi_t type);
};

/*
 * XXX the fact that this list keeps all kinds of info on PCI controllers
 * is pretty grotty-- much of this should be replaced by a proper integration
 * of PCI probes into the wd driver.
 * XXX if we're going to support native-PCI controllers, we also need to
 * keep the address of the IDE control block register, which is something wd.c
 * needs to know, which is why this info is in the wrong place.
 */

struct ide_pci_cookie {
    LIST_ENTRY(ide_pci_cookie) le;
    int iobase_wd;
    int unit;
    int iobase_bm; /* SFF-8038 control registers */
    pcici_t tag;
    pcidi_t type;
    struct ide_pci_prd *prd;
    struct vendor_fns vs;
};

struct ide_pci_softc {
    LIST_HEAD(, ide_pci_cookie) cookies;
};

static int
generic_dmainit(struct ide_pci_cookie *cookie, 
		struct wdparams *wp, 
		int(*wdcmd)(int, void *),
		void *wdinfo);
static void
generic_status(int iobase_wd,
	       int unit,
	       int iobase_bm,
	       pcici_t tag,
	       pcidi_t type);
static void
via_571_status(int iobase_wd,
	       int unit,
	       int iobase_bm,
	       pcici_t tag,
	       pcidi_t type);
static void
intel_piix_dump_drive(char *ctlr,
		      int sitre,
		      int word40,
		      int word44,
		      int drive);
static void
intel_piix_status(int iobase_wd,
		  int unit,
		  int iobase_bm,
		  pcici_t tag,
		  pcidi_t type);

static struct ide_pci_cookie *
mkcookie(int iobase_wd, 
	 int unit, 
	 int iobase_bm, 
	 pcici_t tag, 
	 pcidi_t type, 
	 struct vendor_fns *vp);



static void ide_pci_attach(pcici_t tag, int unit);
static void *ide_pci_candma(int, int);
static int ide_pci_dmainit(void *, 
			   struct wdparams *, 
			   int (*)(int, void *),
			   void *);
static int ide_pci_dmaverify(void *, char *, u_long, int);
static int ide_pci_dmasetup(void *, char *, u_long, int);
static void ide_pci_dmastart(void *);
static int ide_pci_dmadone(void *);
static int ide_pci_status(void *);
static int ide_pci_timing(void *, int);

static struct ide_pci_softc softc;

static int ide_pci_softc_cookies_initted = 0;

/*
 * PRD_ALLOC_SIZE should be something that will not be allocated across a 64k
 * boundary.
 * PRD_MAX_SEGS is defined to be the maximum number of segments required for
 * a transfer on an IDE drive, for an xfer that is linear in virtual memory.
 * PRD_BUF_SIZE is the size of the buffer needed for a PRD table.
 */
#define	PRD_ALLOC_SIZE		PAGE_SIZE
#define PRD_MAX_SEGS		((256 * 512 / PAGE_SIZE) + 1)
#define PRD_BUF_SIZE		PRD_MAX_SEGS * 8

static void *prdbuf = 0;
static void *prdbuf_next = 0;

/* 
 * Hardware specific IDE controller code.  All vendor-specific code
 * for handling IDE timing and other chipset peculiarities should be
 * encapsulated here.
 */

/* Generic busmastering PCI-IDE */

static int
generic_dmainit(struct ide_pci_cookie *cookie, 
		struct wdparams *wp, 
		int(*wdcmd)(int, void *),
		void *wdinfo)
{
    int mode, r;
    /*
     * XXX punt on the whole timing issue by looking for either a
     * drive programmed for both PIO4 and mDMA2 (which use similar
     * timing) or a drive in an UltraDMA mode (hopefully all
     * controllers have separate timing for UDMA).  one hopes that if
     * the drive's DMA mode has been configured by the BIOS, the
     * controller's has also.  this code may eventually be replaced
     * by gunk in the hw-specific code to deal with specific
     * controllers.
     */
    /* XXX way too sick and twisted conditional */
    if (!((((wp->wdp_atavalid & 2) == 2) && 
	   ((wp->wdp_dmamword & 0x404) == 0x404) &&
	   ((wp->wdp_eidepiomodes & 2) == 2)) ||
	  (((wp->wdp_atavalid & 4) == 4) &&
	   (wp->wdp_udmamode == 4))))
	return 0;

#if 0
    /*
     * XXX flesh this out into real code that actually
     * does something-- this was just testing gunk.
     */
    if (((wp->wdp_atavalid & 0x4) == 0x4) &&
	(wp->wdp_udmamode == 4)) {
	printf("UDMA mode\n");
	mode = 0x42;  /* XXX where's the #defines... */
    }
    else {
	printf("MDMA mode\n");
	mode = 0x24;
    }

    r = wdcmd(mode, wdinfo);
    printf("dmainit out like we expect\n");
    if (!r)
	return 0;
#endif
    return 1;
}

static void
generic_status(int iobase_wd,
	       int unit,
	       int iobase_bm,
	       pcici_t tag,
	       pcidi_t type)
{
    printf("generic_status: no PCI IDE timing info available\n");
}

static struct vendor_fns vs_generic = 
{ 
    generic_dmainit, 
    generic_status
};

/* VIA Technologies "82C571" PCI-IDE controller core */

static void
via_571_status(int iobase_wd,
	       int unit,
	       int iobase_bm,
	       pcici_t tag,
	       pcidi_t type)
{
    unsigned int word40[5];
    int i;

    /* XXX how to handle four calls for one controller? */
    if (iobase_wd != 0x1f0 || unit != 0)
	return;

    for (i=0; i<5; i++) {
	word40[i] = pci_conf_read(tag, i * 4 + 0x40);
    }

    printf("via_571_status: Primary IDE prefetch/postwrite %s/%s\n",
	   word40[0] & 0x8000 ? "enabled" : "disabled",
	   word40[0] & 0x4000 ? "enabled" : "disabled");
    printf("via_571_status: Secondary IDE prefetch/postwrite %s/%s\n",
	   word40[0] & 0x2000 ? "enabled" : "disabled",
	   word40[0] & 0x1000 ? "enabled" : "disabled");

    printf("via_571_status: Master %d read/%d write IRDY# wait states\n",
	   (word40[1] & 0x40) >> 6,
	   (word40[1] & 0x20) >> 5);
    printf("via_571_status: busmaster status read retry %s\n",
	   (word40[1] & 0x10) ? "enabled" : "disabled");

    for (i=0; i<4; i++)
	printf("via_571_status: %s drive %d setup=%d active=%d recovery=%d\n",
	       i < 2 ? "primary" : "secondary", 
	       i & 1,
	       ((word40[3] >> ((3 - i) * 2)) & 3) + 1,
	       ((word40[2] >> (((3 - i) * 8) + 4)) & 0x0f) + 1,
	       ((word40[2] >> ((3 - i) * 8)) & 0x0f) + 1);


    /* XXX could go on and do UDMA status for '586B */
}

static struct vendor_fns vs_via_571 = 
{ 
    generic_dmainit, 
    via_571_status
};

/* Intel PIIX, PIIX3, and PIIX4 IDE controller subfunctions */

static void
intel_piix_dump_drive(char *ctlr,
		      int sitre,
		      int word40,
		      int word44,
		      int drive)
{
    char *ms;

    if (!sitre)
	ms = "master/slave";
    else if (drive == 0)
	ms = "master";
    else
	ms = "slave";

    if (sitre || drive == 0)
	printf("intel_piix_status: %s %s sample = %d, %s recovery = %d\n", 
	       ctlr, 
	       ms, 
	       5 - ((sitre && drive) ?
		    ((word44 >> 2) & 3) :
		    ((word40 >> 12) & 3)),
	       ms,
	       4 - ((sitre && drive) ?
		    ((word44 >> 0) & 3) :
		    ((word40 >> 8) & 3)));

    word40 >>= (drive * 4);
    printf("\
intel_piix_status: %s %s fastDMAonly %s, pre/post %s,\n\
intel_piix_status:  IORDY sampling %s,\n\
intel_piix_status:  fast PIO %s%s\n", 
	   ctlr,
	   (drive == 0) ? "master" : "slave",
	   (word40 & 8) ? "enabled" : "disabled",
	   (word40 & 4) ? "enabled" : "disabled",
	   (word40 & 2) ? "enabled" : "disabled",
	   (word40 & 1) ? "enabled" : "disabled",
	   ((word40 & 9) == 9) ? " (overridden by fastDMAonly)" : "" );

    /* XXX extend to dump 82371AB's UltraDMA modes */
}

static void
intel_piix_status(int iobase_wd,
		  int unit,
		  int iobase_bm,
		  pcici_t tag,
		  pcidi_t type)
{
    unsigned int word40, word44;
    int sitre;

    /* XXX how to handle four calls for one controller? */
    if (iobase_wd != 0x1f0 || unit != 0)
	return;

    word40 = pci_conf_read(tag, 0x40);
    word44 = pci_conf_read(tag, 0x44);

    sitre = word40 & 0x4000;

    intel_piix_dump_drive("primary", sitre, word40 & 0xffff, word44 & 0x0f, 0);
    intel_piix_dump_drive("primary", sitre, word40 & 0xffff, word44 & 0x0f, 1);
    intel_piix_dump_drive("secondary", 
			  sitre, 
			  (word40 >> 16) & 0xffff, 
			  (word44 >> 4) & 0x0f,0);
    intel_piix_dump_drive("secondary", 
			  sitre, 
			  (word40 >> 16) & 0xffff, 
			  (word44 >> 4) & 0x0f,1);
}

static struct vendor_fns vs_intel_piix = 
{ 
    generic_dmainit, 
    intel_piix_status
};

/* Generic SFF-8038i code-- all code below here, except for PCI probes,
 * more or less conforms to the SFF-8038i spec as extended for PCI.
 * There should be no code that goes beyond that feature set below.
 */

/* XXX mkcookie is overloaded with too many parameters */

static struct ide_pci_cookie *
mkcookie(int iobase_wd, 
	 int unit, 
	 int iobase_bm, 
	 pcici_t tag, 
	 pcidi_t type, 
	 struct vendor_fns *vp)
{
    struct ide_pci_cookie *cp;

    cp = malloc(sizeof *cp, M_DEVBUF, M_NOWAIT);
    if (!cp) return cp;

    cp->iobase_wd = iobase_wd;
    cp->unit = unit;
    cp->tag = tag;
    cp->type = type;
    cp->iobase_bm = iobase_bm;
    bcopy(vp, &cp->vs, sizeof(struct vendor_fns));

    if (!prdbuf) {
	prdbuf = malloc(PRD_ALLOC_SIZE, M_DEVBUF, M_NOWAIT);
	if (!prdbuf) {
	    FREE(cp, M_DEVBUF);
	    return 0;
	}
	if (((int)prdbuf >> PAGE_SHIFT) ^
	    (((int)prdbuf + PRD_ALLOC_SIZE - 1) >> PAGE_SHIFT)) {
	    printf("ide_pci: prdbuf straddles page boundary, no DMA");
	    FREE(cp, M_DEVBUF);
	    FREE(prdbuf, M_DEVBUF);
	    return 0;
	}

	prdbuf_next = prdbuf;
    }
    cp->prd = prdbuf_next;
    (char *)prdbuf_next += PRD_BUF_SIZE;

    if ((char *)prdbuf_next > ((char *)prdbuf + PRD_ALLOC_SIZE))
	panic("ide_pci: too many prdbufs allocated");

    if (bootverbose)
	printf("ide_pci: mkcookie %04x:%d: PRD vstart = %08x vend = %08x\n",
	       iobase_wd, unit, (int)cp->prd, ((int)cp->prd)+PRD_BUF_SIZE);
    LIST_INSERT_HEAD(&softc.cookies, cp, le);
    return cp;
}

static char *
ide_pci_probe(pcici_t tag, pcidi_t type)
{
    int data = pci_conf_read(tag, PCI_CLASS_REG);

    switch (data & PCI_CLASS_MASK) {

    case PCI_CLASS_MASS_STORAGE:
	if ((data & PCI_SUBCLASS_MASK) == 0x00010000) {
	    if (type == 0x71118086)
		return ("Intel PIIX4 Bus-master IDE controller");
	    if (type == 0x70108086)
		return ("Intel PIIX3 Bus-master IDE controller");
	    if (type == 0x12308086)
		return ("Intel PIIX Bus-master IDE controller");
	    if (type == 0x05711106)
		return ("VIA 82C586x (Apollo) Bus-master IDE controller");
	    if (data & 0x8000)
		return ("PCI IDE controller (busmaster capable)");
/*
 * XXX leave this out for now, to allow CMD640B hack to work.  said
 * hack should be better integrated, or something.
 */
#if 0
	    else
		return ("PCI IDE controller (not busmaster capable)");
#endif
	}
    };
    return ((char*)0);
}

static void
ide_pci_attach(pcici_t tag, int unit)
{
    u_long idetm;
    int class;
    int bmista;
    int iobase_wd, iobase_bm;
    int cmd;
    struct vendor_fns *vp;
    pcidi_t type;

    if (unit) return;

    /* is it busmaster capable?  bail if not */
    class = pci_conf_read(tag, PCI_CLASS_REG);
    if (!(class & 0x8000)) return;

    /* is it enabled and is busmastering turned on? */
    cmd = pci_conf_read(tag, PCI_COMMAND_STATUS_REG);
    if ((cmd & 5) != 5) return;

    /* set up vendor-specific stuff */
    type = pci_conf_read(tag, PCI_ID_REG);

    switch (type) {
    case 0x71118086:
    case 0x70108086:
    case 0x12308086:
	/* Intel PIIX, PIIX3, PIIX4 */
	vp = &vs_intel_piix;
	break;

    case 0x5711106:
	/* VIA Apollo chipset family */
	vp = &vs_via_571;
	break;

    default:
	/* everybody else */
	vp = &vs_generic;
	break;
    }

    iobase_wd = (class & 0x100) ? 
	(pci_conf_read(tag, 0x10)  & 0xfffc) : 
	0x1f0;
    iobase_bm = pci_conf_read(tag, 0x20) & 0xfffc;

    if (!ide_pci_softc_cookies_initted) {
	LIST_INIT(&softc.cookies);
	ide_pci_softc_cookies_initted = 1;
    }

    bmista = inb(iobase_bm + BMISTA_PORT);

    if (bootverbose)
	printf("ide_pci: busmaster 0 status: %02x from port: %08x\n", 
	       bmista, iobase_bm+BMISTA_PORT);

    if (!(bmista & BMISTA_DMA0CAP))
	printf("ide_pci: warning, ide0:0 not configured for DMA?\n");
    mkcookie(iobase_wd, 0, iobase_bm, tag, type, vp);
    if (bootverbose)
	vp->vendor_status(iobase_wd, 0, iobase_bm, tag, type);

    if (!(bmista & BMISTA_DMA1CAP))
	printf("ide_pci: warning, ide0:1 not configured for DMA?\n");
    mkcookie(iobase_wd, 1, iobase_bm, tag, type, vp);
    if (bootverbose)
	vp->vendor_status(iobase_wd, 1, iobase_bm, tag, type);

    if (bmista & BMISTA_SIMPLEX) {
	printf("ide_pci: primary is simplex-only, no DMA on secondary\n");
    } else {
	iobase_wd = (class & 0x400) ? 
	    (pci_conf_read(tag, 0x10)  & 0xfffc) : 
	    0x170;
	iobase_bm += SFF8038_CTLR_1;
	bmista = inb(iobase_bm + BMISTA_PORT);
		
	if (bootverbose)
	    printf("ide_pci: busmaster 1 status: %02x from port: %08x\n",
		   bmista, iobase_bm+BMISTA_PORT);

	if (bmista & BMISTA_SIMPLEX) {
	    printf("ide_pci: secondary is simplex-only, no DMA on secondary\n");
	} else {
	    if (!(bmista & BMISTA_DMA0CAP))
		printf("ide_pci: warning, ide1:0 not configured for DMA?\n");
	    mkcookie(iobase_wd, 0, iobase_bm, tag, type, vp);
	    if (bootverbose) 
		vp->vendor_status(iobase_wd, 0, iobase_bm, tag, type);
	    if (!(bmista & BMISTA_DMA1CAP))
		printf("ide_pci: warning, ide1:1 not configured for DMA?\n");
	    mkcookie(iobase_wd, 1, iobase_bm, tag, type, vp);
	    if (bootverbose)
		vp->vendor_status(iobase_wd, 1, iobase_bm, tag, type);
	}
    }

    wddma.wdd_candma = ide_pci_candma;
    wddma.wdd_dmainit = ide_pci_dmainit;
    wddma.wdd_dmaverify = ide_pci_dmaverify;
    wddma.wdd_dmaprep = ide_pci_dmasetup;
    wddma.wdd_dmastart = ide_pci_dmastart;
    wddma.wdd_dmadone = ide_pci_dmadone;
    wddma.wdd_dmastatus = ide_pci_status;
}

static u_long ide_pci_count;

static struct pci_device ide_pci_device = {
    "ide_pci",
    ide_pci_probe,
    ide_pci_attach,
    &ide_pci_count,
    0
};

DATA_SET(pcidevice_set, ide_pci_device);

/*
 * Return a cookie if we can do DMA on the specified (iobase_wd, unit).
 */
static void *
ide_pci_candma(int iobase_wd, int unit)
{
    struct ide_pci_cookie *cp;

    cp = softc.cookies.lh_first;
    while(cp) {
	if (cp->unit == unit && cp->iobase_wd == iobase_wd)
	    break;
	cp = cp->le.le_next;
    }

    return cp;
}

/*
 * Initialize controller and drive for DMA operation, including timing modes.
 * Uses data passed from the wd driver and a callback function to initialize
 * timing modes on the drive.
 */
static int
ide_pci_dmainit(void *cookie,
		struct wdparams *wp,
		int(*wdcmd)(int, void *),
		void *wdinfo)
{
    struct ide_pci_cookie *cp = cookie;

    return(cp->vs.vendor_dmainit(cp, wp, wdcmd, wdinfo));
}
/*
 * Verify that controller can handle a dma request for cp.  Should
 * not affect any hardware or driver state.
 */
static int
ide_pci_dmaverify(void *xcp, char *vaddr, u_long count, int dir)
{
    int badfu;

    /*
     * check for nonaligned or odd-length Stuff
     */
    badfu = ((unsigned int)vaddr & 1) || (count & 1);
#if 1
    if (badfu) {
	printf("ide_pci: dmaverify odd vaddr or length, ");
	printf("vaddr = %08x length = %08x\n", (int)vaddr, count);
    }
#endif
    /* 
     * XXX should perhaps be checking that length of generated table
     * does not exceed space available, but that Would Be Hairy 
     */
    return (!badfu);
}

/*
 * Set up DMA for cp.  It is the responsibility of the caller
 * to ensure that the controller is idle before this routine
 * is called.
 */
static int
ide_pci_dmasetup(void *xcp, char *vaddr, u_long count, int dir)
{
    struct ide_pci_cookie *cp = xcp;
    struct ide_pci_prd *prd;
    int i;
    u_long pgresid;
    int iobase_bm;
    static int trashmore;
    static int *trashmore_p = 0;


    prd = cp->prd;
    i = 0;

    iobase_bm = cp->iobase_bm;
    /*
     * ensure that 0-length transfers get a PRD that won't smash much
     */
    if (!trashmore_p)
	trashmore_p = (void *)vtophys(&trashmore);
	
    prd[0].prd_base = (unsigned int)trashmore_p;
    prd[0].prd_count = 0x80000002;

    if (count == 0) {
	printf("ide_pci: dmasetup 0-length transfer, ");
	printf("vaddr = %08x length = %08x\n", (int)vaddr, count);
    }

    /* 
     * XXX the PRD generation code is somewhat ugly and will not
     * port easily to big endian systems.  
     *
     * but it works.
     */

    /*
     * Deal with transfers that don't start on a page
     * boundary.
     */
    pgresid = (u_long)vaddr % PAGE_SIZE;
    if (pgresid) {
	prd[i].prd_base = vtophys(vaddr);
	if (count >= (PAGE_SIZE - pgresid))
	    prd[i].prd_count = PAGE_SIZE - pgresid;
	else
	    prd[i].prd_count = count;
	vaddr += prd[i].prd_count;
	count -= prd[i].prd_count;
	i++;
    }

    /*
     * We have now ensured that vaddr is page-aligned, so just
     * step through the pages adding each one onto the list.
     */
    while(count) {
	u_long phys, n;

	phys = vtophys(vaddr);
	n = ((count > PAGE_SIZE) ? PAGE_SIZE : count);
	/*
	 * If the current page is physically contiguous with
	 * whatever we have in the previous PRD, just tack it
	 * onto the end.
	 * CAVEAT: due to a hardware deficiency, PRDs
	 * cannot cross a 64K boundary.
	 * XXX should we bother with this collapsing?  scattered
	 * pages appear to be the common case anyway.
	 */
	if (i > 0 
	    && (phys == prd[i - 1].prd_base + prd[i - 1].prd_count)
	    && ((prd[i - 1].prd_base & 0xffff)
		+ prd[i - 1].prd_count + n) <= 65535) {

	    prd[i - 1].prd_count += n;
	} else {
	    prd[i].prd_base = phys;
	    prd[i].prd_count = n;
	    i++;
	    if (i >= PRD_MAX_SEGS)
		panic("wd82371: too many segments\n");
	}
	count -= n;
	vaddr += n;
    }

    /* put a sign at the edge of the cliff... */
    prd[(i>0) ? (i-1) : 0].prd_count |= PRD_EOT_BIT;

    if (i == 0)
	printf("ide_pci: dmasetup 0-length PRD???\n");

    /* Set up PRD base register */
    outl(iobase_bm + BMIDTP_PORT, vtophys(prd));

    /* Set direction of transfer */
    if (dir == B_READ) {
	outb(iobase_bm + BMICOM_PORT, BMICOM_READ_WRITE);
    } else {
	outb(iobase_bm + BMICOM_PORT, 0);
    }

    /* Clear interrupt and error bits */
    outb(iobase_bm + BMISTA_PORT,
	 (inb(iobase_bm + BMISTA_PORT) 
	  | (BMISTA_INTERRUPT | BMISTA_DMA_ERROR)));

    /* printf("dma enable: iobase_bm = %08x command/status = %08x pointer = %08x\n", iobase_bm, inl(iobase_bm + BMICOM_PORT), inl(iobase_bm + BMIDTP_PORT)); */

    /* printf("P"); */

    return 0;
}		

static void
ide_pci_dmastart(void *xcp)
{
    struct ide_pci_cookie *cp = xcp;
    int iobase_bm;

    iobase_bm = cp->iobase_bm;

    outb(iobase_bm + BMICOM_PORT,
	 inb(iobase_bm + BMICOM_PORT) | BMICOM_STOP_START);

    /* printf("["); */
}

static int
ide_pci_dmadone(void *xcp)
{
    struct ide_pci_cookie *cp = xcp;
    int iobase_bm, status;

    status = ide_pci_status(xcp);
    iobase_bm = cp->iobase_bm;

    outb(iobase_bm + BMICOM_PORT,
	 inb(iobase_bm + BMICOM_PORT) & ~BMICOM_STOP_START);

    /* printf("]"); */

    return status;
}

static int
ide_pci_status(void *xcp)
{
    struct ide_pci_cookie *cp = xcp;
    int iobase_bm, status, bmista;

    status = 0;
    iobase_bm = cp->iobase_bm;

    bmista = inb(iobase_bm + BMISTA_PORT);

    /* printf("dmastatus: iobase_bm = %08x status = %02x command/status = %08x pointer = %08x\n", iobase_bm, bmista, inl(iobase_bm + BMICOM_PORT), inl(iobase_bm + BMIDTP_PORT)); */

    if (bmista & BMISTA_INTERRUPT)
	status |= WDDS_INTERRUPT;
    if (bmista & BMISTA_DMA_ERROR)
	status |= WDDS_ERROR;
    if (bmista & BMISTA_DMA_ACTIVE)
	status |= WDDS_ACTIVE;

    /* printf( (bmista == BMISTA_INTERRUPT)? "?":"!"); */

    return status;
}

#endif /* NPCI > 0 */
