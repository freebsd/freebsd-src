/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2003-2012 Broadcom Corporation
 * All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY BROADCOM ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL BROADCOM OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mbuf.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/bus.h>
#include <sys/random.h>
#include <sys/rman.h>
#include <sys/uio.h>
#include <sys/kobj.h>

#include <dev/pci/pcivar.h>

#include <opencrypto/cryptodev.h>
#include <opencrypto/xform_auth.h>

#include "cryptodev_if.h"

#include <vm/vm.h>
#include <vm/pmap.h>

#include <mips/nlm/hal/haldefs.h>
#include <mips/nlm/hal/iomap.h>
#include <mips/nlm/xlp.h>
#include <mips/nlm/hal/sys.h>
#include <mips/nlm/hal/fmn.h>
#include <mips/nlm/hal/nlmsaelib.h>
#include <mips/nlm/dev/sec/nlmseclib.h>
#include <mips/nlm/hal/cop2.h>
#include <mips/nlm/hal/mips-extns.h>
#include <mips/nlm/msgring.h>

unsigned int creditleft;

static	int xlp_sec_init(struct xlp_sec_softc *sc);
static	int xlp_sec_probesession(device_t,
    const struct crypto_session_params *);
static	int xlp_sec_newsession(device_t , crypto_session_t,
    const struct crypto_session_params *);
static	int xlp_sec_process(device_t , struct cryptop *, int);
static void xlp_copyiv(struct xlp_sec_softc *, struct xlp_sec_command *,
    const struct crypto_session_params *);
static int xlp_get_nsegs(struct cryptop *, unsigned int *);
static int xlp_alloc_cmd_params(struct xlp_sec_command *, unsigned int);
static void  xlp_free_cmd_params(struct xlp_sec_command *);

static	int xlp_sec_probe(device_t);
static	int xlp_sec_attach(device_t);
static	int xlp_sec_detach(device_t);

static device_method_t xlp_sec_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe, xlp_sec_probe),
	DEVMETHOD(device_attach, xlp_sec_attach),
	DEVMETHOD(device_detach, xlp_sec_detach),

	/* bus interface */
	DEVMETHOD(bus_print_child, bus_generic_print_child),
	DEVMETHOD(bus_driver_added, bus_generic_driver_added),

	/* crypto device methods */
	DEVMETHOD(cryptodev_probesession, xlp_sec_probesession),
	DEVMETHOD(cryptodev_newsession, xlp_sec_newsession),
	DEVMETHOD(cryptodev_process,    xlp_sec_process),

	DEVMETHOD_END
};

static driver_t xlp_sec_driver = {
	"nlmsec",
	xlp_sec_methods,
	sizeof(struct xlp_sec_softc)
};
static devclass_t xlp_sec_devclass;

DRIVER_MODULE(nlmsec, pci, xlp_sec_driver, xlp_sec_devclass, 0, 0);
MODULE_DEPEND(nlmsec, crypto, 1, 1, 1);

void
nlm_xlpsec_msgring_handler(int vc, int size, int code, int src_id,
    struct nlm_fmn_msg *msg, void *data);

#ifdef NLM_SEC_DEBUG

#define extract_bits(x, bitshift, bitcnt)				\
    (((unsigned long long)x >> bitshift) & ((1ULL << bitcnt) - 1))

void
print_crypto_params(struct xlp_sec_command *cmd, struct nlm_fmn_msg m)
{
	unsigned long long msg0,msg1,msg2,msg3,msg4,msg5,msg6,msg7,msg8;

	msg0 = cmd->ctrlp->desc0;
	msg1 = cmd->paramp->desc0;
	msg2 = cmd->paramp->desc1;
	msg3 = cmd->paramp->desc2;
	msg4 = cmd->paramp->desc3;
	msg5 = cmd->paramp->segment[0][0];
	msg6 = cmd->paramp->segment[0][1];
	msg7 = m.msg[0];
	msg8 = m.msg[1];

	printf("msg0 %llx msg1 %llx msg2 %llx msg3 %llx msg4 %llx msg5 %llx"
	    "msg6 %llx msg7 %llx msg8 %llx\n", msg0, msg1, msg2, msg3, msg4,
	    msg5, msg6, msg7, msg8);

	printf("c0: hmac %d htype %d hmode %d ctype %d cmode %d arc4 %x\n",
	    (unsigned int)extract_bits(msg0, 61, 1),
	    (unsigned int)extract_bits(msg0, 52, 8),
	    (unsigned int)extract_bits(msg0, 43, 8),
	    (unsigned int)extract_bits(msg0, 34, 8),
	    (unsigned int)extract_bits(msg0, 25, 8),
	    (unsigned int)extract_bits(msg0, 0, 23));

	printf("p0: tls %d hsrc %d hl3 %d enc %d ivl %d hd %llx\n",
	    (unsigned int)extract_bits(msg1, 63, 1),
	    (unsigned int)extract_bits(msg1,62,1),
	    (unsigned int)extract_bits(msg1,60,1),
	    (unsigned int)extract_bits(msg1,59,1),
	    (unsigned int)extract_bits(msg1,41,16), extract_bits(msg1,0,40));

	printf("p1: clen %u hl %u\n",  (unsigned int)extract_bits(msg2, 32, 32),
	    (unsigned int)extract_bits(msg2,0,32));

	printf("p2: ivoff %d cbit %d coff %d hbit %d hclb %d hoff %d\n",
	    (unsigned int)extract_bits(msg3, 45, 17),
	    (unsigned int)extract_bits(msg3, 42,3),
	    (unsigned int)extract_bits(msg3, 22,16),
	    (unsigned int)extract_bits(msg3, 19,3),
	    (unsigned int)extract_bits(msg3, 18,1),
	    (unsigned int)extract_bits(msg3, 0, 16));

	printf("p3: desfbid %d tlen %d arc4 %x hmacpad %d\n",
	    (unsigned int)extract_bits(msg4, 48,16),
	    (unsigned int)extract_bits(msg4,11,16),
	    (unsigned int)extract_bits(msg4,6,3),
	    (unsigned int)extract_bits(msg4,5,1));

	printf("p4: sflen %d sddr %llx \n",
	    (unsigned int)extract_bits(msg5, 48, 16),extract_bits(msg5, 0, 40));

	printf("p5: dflen %d cl3 %d cclob %d cdest %llx \n",
	    (unsigned int)extract_bits(msg6, 48, 16),
	    (unsigned int)extract_bits(msg6, 46, 1),
	    (unsigned int)extract_bits(msg6, 41, 1), extract_bits(msg6, 0, 40));

	printf("fmn0: fbid %d dfrlen %d dfrv %d cklen %d cdescaddr %llx\n",
	    (unsigned int)extract_bits(msg7, 48, 16),
	    (unsigned int)extract_bits(msg7,46,2),
	    (unsigned int)extract_bits(msg7,45,1),
	    (unsigned int)extract_bits(msg7,40,5),
	    (extract_bits(msg7,0,34)<< 6));

	printf("fmn1: arc4 %d hklen %d pdesclen %d pktdescad %llx\n",
	    (unsigned int)extract_bits(msg8, 63, 1),
	    (unsigned int)extract_bits(msg8,56,5),
	    (unsigned int)extract_bits(msg8,43,12),
	    (extract_bits(msg8,0,34) << 6));

	return;
}

void
print_cmd(struct xlp_sec_command *cmd)
{
	printf("session_num		:%d\n",cmd->session_num);
	printf("crp			:0x%x\n",(uint32_t)cmd->crp);
	printf("enccrd			:0x%x\n",(uint32_t)cmd->enccrd);
	printf("maccrd			:0x%x\n",(uint32_t)cmd->maccrd);
	printf("ses			:%d\n",(uint32_t)cmd->ses);
	printf("ctrlp			:0x%x\n",(uint32_t)cmd->ctrlp);
	printf("paramp			:0x%x\n",(uint32_t)cmd->paramp);
	printf("hashdest		:0x%x\n",(uint32_t)cmd->hashdest);
	printf("hashsrc			:%d\n",cmd->hashsrc);
	printf("hmacpad			:%d\n",cmd->hmacpad);
	printf("hashoff			:%d\n",cmd->hashoff);
	printf("hashlen			:%d\n",cmd->hashlen);
	printf("cipheroff		:%d\n",cmd->cipheroff);
	printf("cipherlen		:%d\n",cmd->cipherlen);
	printf("ivoff			:%d\n",cmd->ivoff);
	printf("ivlen			:%d\n",cmd->ivlen);
	printf("hashalg			:%d\n",cmd->hashalg);
	printf("hashmode		:%d\n",cmd->hashmode);
	printf("cipheralg		:%d\n",cmd->cipheralg);
	printf("ciphermode		:%d\n",cmd->ciphermode);
	printf("nsegs     		:%d\n",cmd->nsegs);
	printf("hash_dst_len		:%d\n",cmd->hash_dst_len);
}
#endif /* NLM_SEC_DEBUG */

static int
xlp_sec_init(struct xlp_sec_softc *sc)
{

	/* Register interrupt handler for the SEC CMS messages */
	if (register_msgring_handler(sc->sec_vc_start,
	    sc->sec_vc_end, nlm_xlpsec_msgring_handler, sc) != 0) {
		printf("Couldn't register sec msgring handler\n");
		return (-1);
	}

	/* Do the CMS credit initialization */
	/* Currently it is configured by default to 50 when kernel comes up */

	return (0);
}

/* This function is called from an interrupt handler */
void
nlm_xlpsec_msgring_handler(int vc, int size, int code, int src_id,
    struct nlm_fmn_msg *msg, void *data)
{
	struct xlp_sec_command *cmd = NULL;
	struct xlp_sec_softc *sc = NULL;
	uint8_t hash[HASH_MAX_LEN];

	KASSERT(code == FMN_SWCODE_CRYPTO,
	    ("%s: bad code = %d, expected code = %d\n", __FUNCTION__,
	    code, FMN_SWCODE_CRYPTO));

	sc = (struct xlp_sec_softc *)data;
	KASSERT(src_id >= sc->sec_vc_start && src_id <= sc->sec_vc_end,
	    ("%s: bad src_id = %d, expect %d - %d\n", __FUNCTION__,
	    src_id, sc->sec_vc_start, sc->sec_vc_end));

	cmd = (struct xlp_sec_command *)(uintptr_t)msg->msg[0];
	KASSERT(cmd != NULL && cmd->crp != NULL,
		("%s :cmd not received properly\n",__FUNCTION__));

	KASSERT(CRYPTO_ERROR(msg->msg[1]) == 0,
	    ("%s: Message rcv msg0 %llx msg1 %llx err %x \n", __FUNCTION__,
	    (unsigned long long)msg->msg[0], (unsigned long long)msg->msg[1],
	    (int)CRYPTO_ERROR(msg->msg[1])));

	/* If there are not enough credits to send, then send request
	 * will fail with ERESTART and the driver will be blocked until it is
	 * unblocked here after knowing that there are sufficient credits to
	 * send the request again.
	 */
	if (sc->sc_needwakeup) {
		atomic_add_int(&creditleft, sc->sec_msgsz);
		if (creditleft >= (NLM_CRYPTO_LEFT_REQS)) {
			crypto_unblock(sc->sc_cid, sc->sc_needwakeup);
			sc->sc_needwakeup &= (~(CRYPTO_SYMQ | CRYPTO_ASYMQ));
		}
	}
	if (cmd->hash_dst_len != 0) {
		if (cmd->crp->crp_op & CRYPTO_OP_VERIFY_DIGEST) {
			crypto_copydata(cmd->crp, cmd->crp->crp_digest_start,
			    cmd->hash_dst_len, hash);
			if (timingsafe_bcmp(cmd->hashdest, hash,
			    cmd->hash_dst_len) != 0)
				cmd->crp->crp_etype = EBADMSG;
		} else
			crypto_copyback(cmd->crp, cmd->crp->crp_digest_start,
			    cmd->hash_dst_len, cmd->hashdest);
	}

	/* This indicates completion of the crypto operation */
	crypto_done(cmd->crp);

	xlp_free_cmd_params(cmd);

	return;
}

static int
xlp_sec_probe(device_t dev)
{
	struct xlp_sec_softc *sc;

	if (pci_get_vendor(dev) == PCI_VENDOR_NETLOGIC &&
	    pci_get_device(dev) == PCI_DEVICE_ID_NLM_SAE) {
		sc = device_get_softc(dev);
		return (BUS_PROBE_DEFAULT);
	}
	return (ENXIO);
}

/*
 * Attach an interface that successfully probed.
 */
static int
xlp_sec_attach(device_t dev)
{
	struct xlp_sec_softc *sc = device_get_softc(dev);
	uint64_t base;
	int qstart, qnum;
	int freq, node;

	sc->sc_dev = dev;

	node = nlm_get_device_node(pci_get_slot(dev));
	freq = nlm_set_device_frequency(node, DFS_DEVICE_SAE, 250);
	if (bootverbose)
		device_printf(dev, "SAE Freq: %dMHz\n", freq);
	if(pci_get_device(dev) == PCI_DEVICE_ID_NLM_SAE) {
		device_set_desc(dev, "XLP Security Accelerator");
		sc->sc_cid = crypto_get_driverid(dev,
		    sizeof(struct xlp_sec_session), CRYPTOCAP_F_HARDWARE);
		if (sc->sc_cid < 0) {
			printf("xlp_sec - error : could not get the driver"
			    " id\n");
			goto error_exit;
		}

		base = nlm_get_sec_pcibase(node);
		qstart = nlm_qidstart(base);
		qnum = nlm_qnum(base);
		sc->sec_vc_start = qstart;
		sc->sec_vc_end = qstart + qnum - 1;
	}

	if (xlp_sec_init(sc) != 0)
		goto error_exit;
	if (bootverbose)
		device_printf(dev, "SEC Initialization complete!\n");
	return (0);

error_exit:
	return (ENXIO);

}

/*
 * Detach an interface that successfully probed.
 */
static int
xlp_sec_detach(device_t dev)
{
	return (0);
}

static bool
xlp_sec_auth_supported(const struct crypto_session_params *csp)
{

	switch (csp->csp_auth_alg) {
	case CRYPTO_SHA1:
	case CRYPTO_SHA1_HMAC:
		break;
	default:
		return (false);
	}
	return (true);
}

static bool
xlp_sec_cipher_supported(const struct crypto_session_params *csp)
{

	switch (csp->csp_cipher_alg) {
	case CRYPTO_AES_CBC:
		if (csp->csp_ivlen != XLP_SEC_AES_IV_LENGTH)
			return (false);
		break;
	default:
		return (false);
	}

	return (true);
}

static int
xlp_sec_probesession(device_t dev, const struct crypto_session_params *csp)
{

	if (csp->csp_flags != 0)
		return (EINVAL);
	switch (csp->csp_mode) {
	case CSP_MODE_DIGEST:
		if (!xlp_sec_auth_supported(csp))
			return (EINVAL);
		break;
	case CSP_MODE_CIPHER:
		if (!xlp_sec_cipher_supported(csp))
			return (EINVAL);
		break;
	case CSP_MODE_ETA:
		if (!xlp_sec_auth_supported(csp) ||
		    !xlp_sec_cipher_supported(csp))
			return (EINVAL);
		break;
	default:
		return (EINVAL);
	}
	return (CRYPTODEV_PROBE_HARDWARE);
}

static int
xlp_sec_newsession(device_t dev, crypto_session_t cses,
    const struct crypto_session_params *csp)
{
	struct xlp_sec_session *ses;

	ses = crypto_get_driver_session(cses);

	if (csp->csp_auth_alg != 0) {
		if (csp->csp_auth_mlen == 0)
			ses->hs_mlen = crypto_auth_hash(csp)->hashsize;
		else
			ses->hs_mlen = csp->csp_auth_mlen;
	}

	return (0);
}

/*
 * XXX freesession routine should run a zero'd mac/encrypt key into context
 * ram.  to blow away any keys already stored there.
 */

static void
xlp_copyiv(struct xlp_sec_softc *sc, struct xlp_sec_command *cmd,
    const struct crypto_session_params *csp)
{
	struct cryptop *crp = NULL;

	crp = cmd->crp;

	if (crp->crp_flags & CRYPTO_F_IV_SEPARATE)
		memcpy(cmd->iv, crp->crp_iv, csp->csp_ivlen);
}

static int
xlp_get_nsegs(struct cryptop *crp, unsigned int *nsegs)
{

	switch (crp->crp_buf.cb_type) {
	case CRYPTO_BUF_MBUF:
	{
		struct mbuf *m = NULL;

		m = crp->crp_buf.cb_mbuf;
		while (m != NULL) {
			*nsegs += NLM_CRYPTO_NUM_SEGS_REQD(m->m_len);
			m = m->m_next;
		}
		break;
	}
	case CRYPTO_BUF_UIO:
	{
		struct uio *uio = NULL;
		struct iovec *iov = NULL;
		int iol = 0;

		uio = crp->crp_buf.cb_uio;
		iov = uio->uio_iov;
		iol = uio->uio_iovcnt;
		while (iol > 0) {
			*nsegs += NLM_CRYPTO_NUM_SEGS_REQD(iov->iov_len);
			iol--;
			iov++;
		}
		break;
	}
	case CRYPTO_BUF_CONTIG:
		*nsegs = NLM_CRYPTO_NUM_SEGS_REQD(crp->crp_buf.cb_buf_len);
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

static int
xlp_alloc_cmd_params(struct xlp_sec_command *cmd, unsigned int nsegs)
{
	int err = 0;

	if(cmd == NULL) {
		err = EINVAL;
		goto error;
	}
	if ((cmd->ctrlp = malloc(sizeof(struct nlm_crypto_pkt_ctrl), M_DEVBUF,
	    M_NOWAIT | M_ZERO)) == NULL) {
		err = ENOMEM;
		goto error;
	}
	if (((uintptr_t)cmd->ctrlp & (XLP_L2L3_CACHELINE_SIZE - 1))) {
		err = EINVAL;
		goto error;
	}
	/* (nsegs - 1) because one seg is part of the structure already */
	if ((cmd->paramp = malloc(sizeof(struct nlm_crypto_pkt_param) +
	    (16 * (nsegs - 1)), M_DEVBUF, M_NOWAIT | M_ZERO)) == NULL) {
		err = ENOMEM;
		goto error;
	}
	if (((uintptr_t)cmd->paramp & (XLP_L2L3_CACHELINE_SIZE - 1))) {
		err = EINVAL;
		goto error;
	}
	if ((cmd->iv = malloc(EALG_MAX_BLOCK_LEN, M_DEVBUF,
	    M_NOWAIT | M_ZERO)) == NULL) {
		err = ENOMEM;
		goto error;
	}
	if ((cmd->hashdest = malloc(HASH_MAX_LEN, M_DEVBUF,
	    M_NOWAIT | M_ZERO)) == NULL) {
		err = ENOMEM;
		goto error;
	}
error:
	return (err);
}

static void
xlp_free_cmd_params(struct xlp_sec_command *cmd)
{
	if (cmd->ctrlp != NULL)
		free(cmd->ctrlp, M_DEVBUF);
	if (cmd->paramp != NULL)
		free(cmd->paramp, M_DEVBUF);
	if (cmd->iv != NULL)
		free(cmd->iv, M_DEVBUF);
	if (cmd->hashdest != NULL)
		free(cmd->hashdest, M_DEVBUF);
	if (cmd != NULL)
		free(cmd, M_DEVBUF);
	return;
}

static int
xlp_sec_process(device_t dev, struct cryptop *crp, int hint)
{
	struct xlp_sec_softc *sc = device_get_softc(dev);
	const struct crypto_session_params *csp;
	struct xlp_sec_command *cmd = NULL;
	int err = -1, ret = 0;
	struct xlp_sec_session *ses;
	unsigned int nsegs = 0;

	ses = crypto_get_driver_session(crp->crp_session);
	csp = crypto_get_params(crp->crp_session);

	/*
	 * This device only support AAD requests where the AAD is
	 * adjacent to the payload.
	 */
	if (crp->crp_aad_length != 0 && crp->crp_payload_start !=
	    crp->crp_aad_start + crp->crp_aad_length) {
		err = EFBIG;
		goto errout;
	}

	if ((cmd = malloc(sizeof(struct xlp_sec_command), M_DEVBUF,
	    M_NOWAIT | M_ZERO)) == NULL) {
		err = ENOMEM;
		goto errout;
	}

	cmd->crp = crp;
	cmd->ses = ses;
	cmd->hash_dst_len = ses->hs_mlen;

	if ((ret = xlp_get_nsegs(crp, &nsegs)) != 0) {
		err = EINVAL;
		goto errout;
	}

	if (crp->crp_flags & CRYPTO_F_IV_SEPARATE) {
		/* Since IV is given as separate segment to avoid copy */
		nsegs += 1;
	}
	cmd->nsegs = nsegs;

	if ((err = xlp_alloc_cmd_params(cmd, nsegs)) != 0)
		goto errout;

	switch (csp->csp_mode) {
	case CSP_MODE_CIPHER:
		if ((ret = nlm_get_cipher_param(cmd, csp)) != 0) {
			err = EINVAL;
			goto errout;
		}
		cmd->cipheroff = crp->crp_payload_start;
		cmd->cipherlen = crp->crp_payload_length;
		if (crp->crp_flags & CRYPTO_F_IV_SEPARATE) {
			cmd->cipheroff += cmd->ivlen;
			cmd->ivoff = 0;
		} else
			cmd->ivoff = crp->crp_iv_start;
		xlp_copyiv(sc, cmd, csp);
		if ((err = nlm_crypto_do_cipher(sc, cmd, csp)) != 0)
			goto errout;
		break;
	case CSP_MODE_DIGEST:
		if ((ret = nlm_get_digest_param(cmd, csp)) != 0) {
			err = EINVAL;
			goto errout;
		}
		cmd->hashoff = crp->crp_payload_start;
		cmd->hashlen = crp->crp_payload_length;
		cmd->hmacpad = 0;
		cmd->hashsrc = 0;
		if ((err = nlm_crypto_do_digest(sc, cmd, csp)) != 0)
			goto errout;
		break;
	case CSP_MODE_ETA:
		if ((ret = nlm_get_cipher_param(cmd, csp)) != 0) {
			err = EINVAL;
			goto errout;
		}
		if ((ret = nlm_get_digest_param(cmd, csp)) != 0) {
			err = EINVAL;
			goto errout;
		}
		if (crp->crp_aad_length != 0) {
			cmd->hashoff = crp->crp_aad_start;
			cmd->hashlen = crp->crp_aad_length +
			    crp->crp_payload_length;
		} else {
			cmd->hashoff = crp->crp_payload_start;
			cmd->hashlen = crp->crp_payload_length;
		}
		cmd->hmacpad = 0;
		if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op))
			cmd->hashsrc = 1;
		else
			cmd->hashsrc = 0;
		cmd->cipheroff = crp->crp_payload_start;
		cmd->cipherlen = crp->crp_payload_length;
		if (crp->crp_flags & CRYPTO_F_IV_SEPARATE) {
			cmd->hashoff += cmd->ivlen;
			cmd->cipheroff += cmd->ivlen;
			cmd->ivoff = 0;
		} else
			cmd->ivoff = crp->crp_iv_start;
		xlp_copyiv(sc, cmd, csp);
		if ((err = nlm_crypto_do_cipher_digest(sc, cmd, csp)) != 0)
			goto errout;
		break;
	default:
		err = EINVAL;
		goto errout;
	}
	return (0);
errout:
	xlp_free_cmd_params(cmd);
	if (err == ERESTART) {
		sc->sc_needwakeup |= CRYPTO_SYMQ;
		creditleft = 0;
		return (err);
	}
	crp->crp_etype = err;
	crypto_done(crp);
	return (err);
}
