/* $FreeBSD$ */
/*
 * Qlogic Host Adapter Inline Functions
 *
 * Copyright (c) 1999, 2000, 2001 by Matthew Jacob
 * Feral Software
 * All rights reserved.
 * mjacob@feral.com
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#ifndef	_ISP_INLINE_H
#define	_ISP_INLINE_H

/*
 * Handle Functions.
 * For each outstanding command there will be a non-zero handle.
 * There will be at most isp_maxcmds handles, and isp_lasthdls
 * will be a seed for the last handled allocated.
 */

static INLINE int isp_save_xs(struct ispsoftc *, XS_T *, u_int16_t *);
static INLINE XS_T *isp_find_xs(struct ispsoftc *, u_int16_t);
static INLINE u_int16_t isp_find_handle(struct ispsoftc *, XS_T *);
static INLINE int isp_handle_index(u_int16_t);
static INLINE void isp_destroy_handle(struct ispsoftc *, u_int16_t);
static INLINE void isp_remove_handle(struct ispsoftc *, XS_T *);

static INLINE int
isp_save_xs(struct ispsoftc *isp, XS_T *xs, u_int16_t *handlep)
{
	int i, j;

	for (j = isp->isp_lasthdls, i = 0; i < (int) isp->isp_maxcmds; i++) {
		if (isp->isp_xflist[j] == NULL) {
			break;
		}
		if (++j == isp->isp_maxcmds) {
			j = 0;
		}
	}
	if (i == isp->isp_maxcmds) {
		return (-1);
	}
	isp->isp_xflist[j] = xs;
	*handlep = j+1;
	if (++j == isp->isp_maxcmds)
		j = 0;
	isp->isp_lasthdls = (u_int16_t)j;
	return (0);
}

static INLINE XS_T *
isp_find_xs(struct ispsoftc *isp, u_int16_t handle)
{
	if (handle < 1 || handle > (u_int16_t) isp->isp_maxcmds) {
		return (NULL);
	} else {
		return (isp->isp_xflist[handle - 1]);
	}
}

static INLINE u_int16_t
isp_find_handle(struct ispsoftc *isp, XS_T *xs)
{
	int i;
	if (xs != NULL) {
		for (i = 0; i < isp->isp_maxcmds; i++) {
			if (isp->isp_xflist[i] == xs) {
				return ((u_int16_t) i+1);
			}
		}
	}
	return (0);
}

static INLINE int
isp_handle_index(u_int16_t handle)
{
	return (handle-1);
}

static INLINE void
isp_destroy_handle(struct ispsoftc *isp, u_int16_t handle)
{
	if (handle > 0 && handle <= (u_int16_t) isp->isp_maxcmds) {
		isp->isp_xflist[isp_handle_index(handle)] = NULL;
	}
}

static INLINE void
isp_remove_handle(struct ispsoftc *isp, XS_T *xs)
{
	isp_destroy_handle(isp, isp_find_handle(isp, xs));
}

static INLINE int
isp_getrqentry(struct ispsoftc *, u_int16_t *, u_int16_t *, void **);

static INLINE int
isp_getrqentry(struct ispsoftc *isp, u_int16_t *iptrp,
    u_int16_t *optrp, void **resultp)
{
	volatile u_int16_t iptr, optr;

	optr = isp->isp_reqodx = READ_REQUEST_QUEUE_OUT_POINTER(isp);
	iptr = isp->isp_reqidx;
	*resultp = ISP_QUEUE_ENTRY(isp->isp_rquest, iptr);
	iptr = ISP_NXT_QENTRY(iptr, RQUEST_QUEUE_LEN(isp));
	if (iptr == optr) {
		return (1);
	}
	if (optrp)
		*optrp = optr;
	if (iptrp)
		*iptrp = iptr;
	return (0);
}

static INLINE void isp_print_qentry (struct ispsoftc *, char *, int, void *);


#define	TBA	(4 * (((QENTRY_LEN >> 2) * 3) + 1) + 1)
static INLINE void
isp_print_qentry(struct ispsoftc *isp, char *msg, int idx, void *arg)
{
	char buf[TBA];
	int amt, i, j;
	u_int8_t *ptr = arg;

	isp_prt(isp, ISP_LOGALL, "%s index %d=>", msg, idx);
	for (buf[0] = 0, amt = i = 0; i < 4; i++) {
		buf[0] = 0;
		SNPRINTF(buf, TBA, "  ");
		for (j = 0; j < (QENTRY_LEN >> 2); j++) {
			SNPRINTF(buf, TBA, "%s %02x", buf, ptr[amt++] & 0xff);
		}
		isp_prt(isp, ISP_LOGALL, buf);
	}
}

static INLINE void isp_print_bytes(struct ispsoftc *, char *, int, void *);

static INLINE void
isp_print_bytes(struct ispsoftc *isp, char *msg, int amt, void *arg)
{
	char buf[128];
	u_int8_t *ptr = arg;
	int off;

	if (msg)
		isp_prt(isp, ISP_LOGALL, "%s:", msg);
	off = 0;
	buf[0] = 0;
	while (off < amt) {
		int j, to;
		to = off;
		for (j = 0; j < 16; j++) {
			SNPRINTF(buf, 128, "%s %02x", buf, ptr[off++] & 0xff);
			if (off == amt)
				break;
		}
		isp_prt(isp, ISP_LOGALL, "0x%08x:%s", to, buf);
		buf[0] = 0;
	}
}

/*
 * Do the common path to try and ensure that link is up, we've scanned
 * the fabric (if we're on a fabric), and that we've synchronized this
 * all with our own database and done the appropriate logins.
 *
 * We repeatedly check for firmware state and loop state after each
 * action because things may have changed while we were doing this.
 * Any failure or change of state causes us to return a nonzero value.
 *
 * We honor HBA roles in that if we're not in Initiator mode, we don't
 * attempt to sync up the database (that's for somebody else to do,
 * if ever).
 *
 * We assume we enter here with any locks held.
 */

static INLINE int isp_fc_runstate(struct ispsoftc *, int);

static INLINE int
isp_fc_runstate(struct ispsoftc *isp, int tval)
{
	fcparam *fcp;
	int *tptr;

	if (IS_SCSI(isp))
		return (0);

	tptr = tval? &tval : NULL;
	if (isp_control(isp, ISPCTL_FCLINK_TEST, tptr) != 0) {
		return (-1);
	}
	fcp = FCPARAM(isp);
	if (fcp->isp_fwstate != FW_READY || fcp->isp_loopstate < LOOP_PDB_RCVD)
		return (-1);
	if (isp_control(isp, ISPCTL_SCAN_FABRIC, NULL) != 0) {
		return (-1);
	}
	if (isp_control(isp, ISPCTL_SCAN_LOOP, NULL) != 0) {
		return (-1);
	}
	if ((isp->isp_role & ISP_ROLE_INITIATOR) == 0) {
		return (0);
	}
	if (isp_control(isp, ISPCTL_PDB_SYNC, NULL) != 0) {
		return (-1);
	}
	if (fcp->isp_fwstate != FW_READY || fcp->isp_loopstate != LOOP_READY) {
		return (-1);
	}
	return (0);
}
#endif	/* _ISP_INLINE_H */
