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
 *	$FreeBSD$
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
#include <pci/wd82371reg.h>

static void *piix_candma(int, int);
static int piix_dmasetup(void *, char *, u_long, int);
static void piix_dmastart(void *);
static int piix_dmadone(void *);
static int piix_status(void *);

struct piix_cookie {
	LIST_ENTRY(piix_cookie) le;
	int ctlr;
	int unit;
	struct piix_prd *prd;
};

struct piix_softc {
	unsigned iobase;
	pcici_t tag;
	LIST_HEAD(, piix_cookie) cookies;
};

static struct piix_softc softc;

static struct piix_cookie *
mkcookie(int ctlr, int unit)
{
	struct piix_cookie *cp;

	cp = malloc(sizeof *cp, M_DEVBUF, M_NOWAIT);
	if (!cp) return cp;
	cp->ctlr = ctlr;
	cp->unit = unit;
	cp->prd = malloc(PRD_ALLOC_SIZE, M_DEVBUF, M_NOWAIT);
	if (!cp->prd) {
		FREE(cp, M_DEVBUF);
		return 0;
	}
	LIST_INSERT_HEAD(&softc.cookies, cp, le);
	return cp;
}

static char *
piix_probe(pcici_t tag, pcidi_t type)
{
	if (type == 0x12308086)
		return ("Intel 82371 (Triton) Bus-master IDE controller");

	return 0;
}

static void
piix_attach(pcici_t tag, int unit)
{
	u_long idetm;
	int bmista;
	int iobase;

	if (unit) return;

	softc.tag = tag;
	iobase = softc.iobase = pci_conf_read(tag, 0x20) & 0xfff0;
	idetm = pci_conf_read(tag, 0x40);

	LIST_INIT(&softc.cookies);

	if (IDETM_CTLR_0(idetm) & IDETM_ENABLE) {
		bmista = inb(iobase + BMISTA_PORT);
		if (bmista & BMISTA_DMA0CAP)
			mkcookie(0, 0);

		if (bmista & BMISTA_DMA1CAP)
			mkcookie(0, 1);
	}

	if (IDETM_CTLR_1(idetm) & IDETM_ENABLE) {
		bmista = inb(iobase + PIIX_CTLR_1 + BMISTA_PORT);
		if (bmista & BMISTA_DMA0CAP)
			mkcookie(1, 0);
		if (bmista & BMISTA_DMA1CAP)
			mkcookie(1, 1);
	}

	wddma.wdd_candma = piix_candma;
	wddma.wdd_dmaprep = piix_dmasetup;
	wddma.wdd_dmastart = piix_dmastart;
	wddma.wdd_dmadone = piix_dmadone;
	wddma.wdd_dmastatus = piix_status;
}

static u_long piix_count;

static struct pci_device piix_device = {
	"piix",
	piix_probe,
	piix_attach,
	&piix_count,
	0
};

DATA_SET(pcidevice_set, piix_device);

/*
 * Return a cookie if we can do DMA on the specified (ctlr, unit).
 */
static void *
piix_candma(int ctlr, int unit)
{
	struct piix_cookie *cp;

	cp = softc.cookies.lh_first;
	while(cp) {
		if (cp->unit == unit && cp->ctlr == ctlr)
			break;
		cp = cp->le.le_next;
	}

	return cp;
}

/*
 * Set up DMA for cp.  It is the responsibility of the caller
 * to ensure that the controller is idle before this routine
 * is called.
 */
static int
piix_dmasetup(void *xcp, char *vaddr, u_long count, int dir)
{
	struct piix_cookie *cp = xcp;
	struct piix_prd *prd;
	int i;
	u_long pgresid;
	int iobase;

	prd = cp->prd;
	i = 0;
	iobase = softc.iobase + cp->ctlr ? PIIX_CTLR_1 : 0;

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
		prd[i].prd_eot |= PRD_EOT_BIT;
		i++;
	}

	/*
	 * We have now ensured that vaddr is page-aligned, so just
	 * step through the pages adding each one onto the list.
	 */
	while(count) {
		u_long phys, n;

		phys = vtophys(vaddr);
		n = (count > PAGE_SIZE) ? PAGE_SIZE : count;
		/*
		 * If the current page is physically contiguous with
		 * whatever we have in the previous PRD, just tack it
		 * onto the end.
		 * CAVEAT: due to a hardware deficiency, PRDs
		 * cannot cross a 64K boundary.
		 */
		if (i > 0 
		    && phys == prd[i - 1].prd_base + prd[i - 1].prd_count
		    && ((prd[i - 1].prd_base & 0xffff) 
			+ prd[i - 1].prd_count + n) <= 65535) {

			prd[i - 1].prd_count += n;
		} else {
			if (i > 0)
				prd[i - 1].prd_eot &= ~PRD_EOT_BIT;
			prd[i].prd_base = phys;
			prd[i].prd_count = n;
			prd[i].prd_eot |= PRD_EOT_BIT;
			i++;
			if (i >= PRD_MAX_SEGS)
				panic("wd82371: too many segments\n");
		}
		count -= n;
		vaddr += n;
	}

	/* Set up PRD base register */
	outl(iobase + BMIDTP_PORT, vtophys(prd));

	/* Set direction of transfer */
	if (dir == B_READ) {
		outb(iobase + BMICOM_PORT, 0);
	} else {
		outb(iobase + BMICOM_PORT, BMICOM_READ_WRITE);
	}

	/* Clear interrupt and error bits */
	outb(iobase + BMISTA_PORT,
	     (inb(iobase + BMISTA_PORT) 
	      & ~(BMISTA_INTERRUPT | BMISTA_DMA_ERROR)));

	return 0;
}		

static void
piix_dmastart(void *xcp)
{
	struct piix_cookie *cp = xcp;
	int iobase;

	iobase = softc.iobase + cp->ctlr ? PIIX_CTLR_1 : 0;

	outb(iobase + BMICOM_PORT,
	     inb(iobase + BMICOM_PORT) | BMICOM_STOP_START);
}

static int
piix_dmadone(void *xcp)
{
	struct piix_cookie *cp = xcp;
	int iobase, status;

	status = piix_status(xcp);
	iobase = softc.iobase + cp->ctlr ? PIIX_CTLR_1 : 0;

	outb(iobase + BMICOM_PORT,
	     inb(iobase + BMICOM_PORT) & ~BMICOM_STOP_START);

	return status;
}

static int
piix_status(void *xcp)
{
	struct piix_cookie *cp = xcp;
	int iobase, status, bmista;

	status = 0;
	iobase = softc.iobase + cp->ctlr ? PIIX_CTLR_1 : 0;

	bmista = inb(iobase + BMISTA_PORT);

	if (bmista & BMISTA_INTERRUPT)
		status |= WDDS_INTERRUPT;
	if (bmista & BMISTA_DMA_ERROR)
		status |= WDDS_ERROR;
	if (bmista & BMISTA_DMA_ACTIVE)
		status |= WDDS_ACTIVE;

	return status;
}

#endif /* NPCI > 0 */
