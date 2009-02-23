
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/endian.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/lockmgr.h>

#include <machine/bus.h>
#include <machine/endian.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>

#include <dev/usb/ehcireg.h>
#include <dev/usb/ehcivar.h>

#ifdef DDB
#include <ddb/ddb.h>
#include <ddb/db_sym.h>
#else
#define	db_printf	printf
#endif

extern	ehci_softc_t *theehci;		/* XXX */

void
ehci_dump_regs(ehci_softc_t *sc)
{
	int i;
	db_printf("cmd=0x%08x, sts=0x%08x, ien=0x%08x\n",
	       EOREAD4(sc, EHCI_USBCMD),
	       EOREAD4(sc, EHCI_USBSTS),
	       EOREAD4(sc, EHCI_USBINTR));
	db_printf("frindex=0x%08x ctrdsegm=0x%08x periodic=0x%08x async=0x%08x\n",
	       EOREAD4(sc, EHCI_FRINDEX),
	       EOREAD4(sc, EHCI_CTRLDSSEGMENT),
	       EOREAD4(sc, EHCI_PERIODICLISTBASE),
	       EOREAD4(sc, EHCI_ASYNCLISTADDR));
	for (i = 1; i <= sc->sc_noport; i++)
		db_printf("port %d status=0x%08x\n", i,
		       EOREAD4(sc, EHCI_PORTSC(i)));
}

static void
ehci_dump_link(ehci_softc_t *sc, ehci_link_t link, int type)
{
	link = hc32toh(sc, link);
	db_printf("0x%08x", link);
	if (link & EHCI_LINK_TERMINATE)
		db_printf("<T>");
	else {
		db_printf("<");
		if (type) {
			switch (EHCI_LINK_TYPE(link)) {
			case EHCI_LINK_ITD: db_printf("ITD"); break;
			case EHCI_LINK_QH: db_printf("QH"); break;
			case EHCI_LINK_SITD: db_printf("SITD"); break;
			case EHCI_LINK_FSTN: db_printf("FSTN"); break;
			}
		}
		db_printf(">");
	}
}

void
ehci_dump_sqtds(ehci_softc_t *sc, ehci_soft_qtd_t *sqtd)
{
	int i;
	u_int32_t stop;

	stop = 0;
	for (i = 0; sqtd && i < 20 && !stop; sqtd = sqtd->nextqtd, i++) {
		ehci_dump_sqtd(sc, sqtd);
		stop = sqtd->qtd.qtd_next & htohc32(sc, EHCI_LINK_TERMINATE);
	}
	if (sqtd)
		db_printf("dump aborted, too many TDs\n");
}

void
ehci_dump_qtd(ehci_softc_t *sc, ehci_qtd_t *qtd)
{
	u_int32_t s;

	db_printf("  next="); ehci_dump_link(sc, qtd->qtd_next, 0);
	db_printf(" altnext="); ehci_dump_link(sc, qtd->qtd_altnext, 0);
	db_printf("\n");
	s = hc32toh(sc, qtd->qtd_status);
	db_printf("  status=0x%08x: toggle=%d bytes=0x%x ioc=%d c_page=0x%x\n",
	       s, EHCI_QTD_GET_TOGGLE(s), EHCI_QTD_GET_BYTES(s),
	       EHCI_QTD_GET_IOC(s), EHCI_QTD_GET_C_PAGE(s));
	db_printf("    cerr=%d pid=%d stat=%b\n", EHCI_QTD_GET_CERR(s),
	       EHCI_QTD_GET_PID(s),
	       EHCI_QTD_GET_STATUS(s), EHCI_QTD_STATUS_BITS);
	for (s = 0; s < 5; s++)
		db_printf("  buffer[%d]=0x%08x\n", s, hc32toh(sc, qtd->qtd_buffer[s]));
}

void
ehci_dump_sqtd(ehci_softc_t *sc, ehci_soft_qtd_t *sqtd)
{
	db_printf("QTD(%p) at 0x%08x:\n", sqtd, sqtd->physaddr);
	ehci_dump_qtd(sc, &sqtd->qtd);
}

void
ehci_dump_sqh(ehci_softc_t *sc, ehci_soft_qh_t *sqh)
{
	ehci_qh_t *qh = &sqh->qh;
	u_int32_t endp, endphub;

	db_printf("QH(%p) at 0x%08x:\n", sqh, sqh->physaddr);
	db_printf("  sqtd=%p inactivesqtd=%p\n", sqh->sqtd, sqh->inactivesqtd);
	db_printf("  link="); ehci_dump_link(sc, qh->qh_link, 1); db_printf("\n");
	endp = hc32toh(sc, qh->qh_endp);
	db_printf("  endp=0x%08x\n", endp);
	db_printf("    addr=0x%02x inact=%d endpt=%d eps=%d dtc=%d hrecl=%d\n",
	       EHCI_QH_GET_ADDR(endp), EHCI_QH_GET_INACT(endp),
	       EHCI_QH_GET_ENDPT(endp),  EHCI_QH_GET_EPS(endp),
	       EHCI_QH_GET_DTC(endp), EHCI_QH_GET_HRECL(endp));
	db_printf("    mpl=0x%x ctl=%d nrl=%d\n",
	       EHCI_QH_GET_MPL(endp), EHCI_QH_GET_CTL(endp),
	       EHCI_QH_GET_NRL(endp));
	endphub = hc32toh(sc, qh->qh_endphub);
	db_printf("  endphub=0x%08x\n", endphub);
	db_printf("    smask=0x%02x cmask=0x%02x huba=0x%02x port=%d mult=%d\n",
	       EHCI_QH_GET_SMASK(endphub), EHCI_QH_GET_CMASK(endphub),
	       EHCI_QH_GET_HUBA(endphub), EHCI_QH_GET_PORT(endphub),
	       EHCI_QH_GET_MULT(endphub));
	db_printf("  curqtd="); ehci_dump_link(sc, qh->qh_curqtd, 0); db_printf("\n");
	db_printf("Overlay qTD:\n");
	ehci_dump_qtd(sc, &qh->qh_qtd);
}

void
ehci_dump_itd(ehci_softc_t *sc, struct ehci_soft_itd *itd)
{
	ehci_isoc_trans_t t;
	ehci_isoc_bufr_ptr_t b, b2, b3;
	int i;

	db_printf("ITD: next phys=%X\n", itd->itd.itd_next);

	for (i = 0; i < 8;i++) {
		t = hc32toh(sc, itd->itd.itd_ctl[i]);
		db_printf("ITDctl %d: stat=%X len=%X ioc=%X pg=%X offs=%X\n", i,
		    EHCI_ITD_GET_STATUS(t), EHCI_ITD_GET_LEN(t),
		    EHCI_ITD_GET_IOC(t), EHCI_ITD_GET_PG(t),
		    EHCI_ITD_GET_OFFS(t));
	}
	db_printf("ITDbufr: ");
	for (i = 0; i < 7; i++)
		db_printf("%X,", EHCI_ITD_GET_BPTR(hc32toh(sc, itd->itd.itd_bufr[i])));

	b = hc32toh(sc, itd->itd.itd_bufr[0]);
	b2 = hc32toh(sc, itd->itd.itd_bufr[1]);
	b3 = hc32toh(sc, itd->itd.itd_bufr[2]);
	db_printf("\nep=%X daddr=%X dir=%d maxpkt=%X multi=%X\n",
	    EHCI_ITD_GET_EP(b), EHCI_ITD_GET_DADDR(b), EHCI_ITD_GET_DIR(b2),
	    EHCI_ITD_GET_MAXPKT(b2), EHCI_ITD_GET_MULTI(b3));
}

void
ehci_dump_sitd(ehci_softc_t *sc, struct ehci_soft_itd *itd)
{
	db_printf("SITD %p next=%p prev=%p xfernext=%p physaddr=%X slot=%d\n",
	    itd, itd->u.frame_list.next, itd->u.frame_list.prev,
	    itd->xfer_next, itd->physaddr, itd->slot);
}

void
ehci_dump_exfer(struct ehci_xfer *ex)
{
#ifdef DIAGNOSTIC
	db_printf("%p: sqtdstart %p end %p itdstart %p end %p isdone %d\n",
	    ex, ex->sqtdstart, ex->sqtdend, ex->itdstart,
	    ex->itdend, ex->isdone);
#else
	db_printf("%p: sqtdstart %p end %p itdstart %p end %p\n",
	    ex, ex->sqtdstart, ex->sqtdend, ex->itdstart, ex->itdend);
#endif
}

#ifdef DDB
DB_SHOW_COMMAND(ehci, db_show_ehci)
{
	if (!have_addr) {
		db_printf("usage: show ehci <addr>\n");
		return;
	}
	ehci_dump_regs((ehci_softc_t *) addr);
}

DB_SHOW_COMMAND(ehci_sqtds, db_show_ehci_sqtds)
{
	if (!have_addr) {
		db_printf("usage: show ehci_sqtds <addr>\n");
		return;
	}
	ehci_dump_sqtds(theehci, (ehci_soft_qtd_t *) addr);
}

DB_SHOW_COMMAND(ehci_qtd, db_show_ehci_qtd)
{
	if (!have_addr) {
		db_printf("usage: show ehci_qtd <addr>\n");
		return;
	}
	ehci_dump_qtd(theehci, (ehci_qtd_t *) addr);
}

DB_SHOW_COMMAND(ehci_sqh, db_show_ehci_sqh)
{
	if (!have_addr) {
		db_printf("usage: show ehci_sqh <addr>\n");
		return;
	}
	ehci_dump_sqh(theehci, (ehci_soft_qh_t *) addr);
}

DB_SHOW_COMMAND(ehci_itd, db_show_ehci_itd)
{
	if (!have_addr) {
		db_printf("usage: show ehci_itd <addr>\n");
		return;
	}
	ehci_dump_itd(theehci, (struct ehci_soft_itd *) addr);
}

DB_SHOW_COMMAND(ehci_sitd, db_show_ehci_sitd)
{
	if (!have_addr) {
		db_printf("usage: show ehci_sitd <addr>\n");
		return;
	}
	ehci_dump_itd(theehci, (struct ehci_soft_itd *) addr);
}

DB_SHOW_COMMAND(ehci_xfer, db_show_ehci_xfer)
{
	if (!have_addr) {
		db_printf("usage: show ehci_xfer <addr>\n");
		return;
	}
	ehci_dump_exfer((struct ehci_xfer *) addr);
}
#endif /* DDB */
