/*
 * Copyright (c) 1998-2002 Katsushi Kobayashi and Hidetoshi SHimokawa
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the acknowledgement as bellow:
 *
 *    This product includes software developed by K. Kobayashi and H. Shimokawa
 *
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * $FreeBSD$
 *
 */
typedef struct fwohci_softc {
	struct firewire_comm fc;
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	void *ih;
#if __FreeBSD_version < 500000
	void *ih_cam;
#endif

	struct resource *bsr;
	struct resource *irq_res;
	struct fwohci_dbch{
		u_int ndb;
		u_int ndesc;
		caddr_t dummy;
		STAILQ_HEAD(, fwohcidb_tr) db_trq;
		struct fwohcidb_tr *top, *bottom, *pdb_tr;
		struct fw_xferq xferq;
		struct {
			int len;
			int hlen;
			int plen;
			caddr_t buf;
		} frag;
		int flags;
#define	FWOHCI_DBCH_INIT	(1<<0)
#define	FWOHCI_DBCH_FULL	(1<<1)
		int buf_offset;
#define FWOHCI_DBCH_MAX_PAGES	32
		int npages;
		void *pages[FWOHCI_DBCH_MAX_PAGES];
	} arrq, arrs, atrq, atrs, it[OHCI_DMA_ITCH], ir[OHCI_DMA_IRCH];
	u_int maxrec;
	u_int32_t *cromptr;
	u_int32_t intmask;
} fwohci_softc_t;
void fwohci_intr __P((void *arg));
int fwohci_init __P((struct fwohci_softc *, device_t));
void fwohci_reset __P((struct fwohci_softc *, device_t));
int fwohci_detach __P((struct fwohci_softc *, device_t));
int fwohci_resume __P((struct fwohci_softc *, device_t));
int fwohci_shutdown __P((struct fwohci_softc *, device_t dev));
