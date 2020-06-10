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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <sys/uio.h>
#include <machine/bus.h>
#include <machine/md_var.h>
#include <machine/cpuregs.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <opencrypto/cryptodev.h>

#include <mips/nlm/hal/haldefs.h>
#include <mips/nlm/hal/cop2.h>
#include <mips/nlm/hal/fmn.h>
#include <mips/nlm/hal/mips-extns.h>
#include <mips/nlm/hal/nlmsaelib.h>
#include <mips/nlm/dev/sec/nlmseclib.h>

static int
nlm_crypto_complete_sec_request(struct xlp_sec_softc *sc,
    struct xlp_sec_command *cmd)
{
	unsigned int fbvc;
	struct nlm_fmn_msg m;
	int ret;

	fbvc = nlm_cpuid() / CMS_MAX_VCPU_VC;
	m.msg[0] = m.msg[1] = m.msg[2] = m.msg[3] = 0;

	m.msg[0] = nlm_crypto_form_pkt_fmn_entry0(fbvc, 0, 0,
	    cmd->ctrlp->cipherkeylen, vtophys(cmd->ctrlp));

	m.msg[1] = nlm_crypto_form_pkt_fmn_entry1(0, cmd->ctrlp->hashkeylen,
	    NLM_CRYPTO_PKT_DESC_SIZE(cmd->nsegs), vtophys(cmd->paramp));

	/* Software scratch pad */
	m.msg[2] = (uintptr_t)cmd;
	sc->sec_msgsz = 3;

	/* Send the message to sec/rsa engine vc */
	ret = nlm_fmn_msgsend(sc->sec_vc_start, sc->sec_msgsz,
	    FMN_SWCODE_CRYPTO, &m);
	if (ret != 0) {
#ifdef NLM_SEC_DEBUG
		printf("%s: msgsnd failed (%x)\n", __func__, ret);
#endif
		return (ERESTART);
	}
	return (0);
}

int
nlm_crypto_form_srcdst_segs(struct xlp_sec_command *cmd,
    const struct crypto_session_params *csp)
{
	unsigned int srcseg = 0, dstseg = 0;
	struct cryptop *crp = NULL;

	crp = cmd->crp;

	if (csp->csp_mode != CSP_MODE_DIGEST) {
		/* IV is given as ONE segment to avoid copy */
		if (crp->crp_flags & CRYPTO_F_IV_SEPARATE) {
			srcseg = nlm_crypto_fill_src_seg(cmd->paramp, srcseg,
			    cmd->iv, cmd->ivlen);
			dstseg = nlm_crypto_fill_dst_seg(cmd->paramp, dstseg,
			    cmd->iv, cmd->ivlen);
		}
	}

	switch (crp->crp_buf.cb_type) {
	case CRYPTO_BUF_MBUF:
	{
		struct mbuf *m = NULL;

		m = crp->crp_buf.cb_mbuf;
		while (m != NULL) {
			srcseg = nlm_crypto_fill_src_seg(cmd->paramp, srcseg,
			    mtod(m,caddr_t), m->m_len);
			if (csp->csp_mode != CSP_MODE_DIGEST) {
				dstseg = nlm_crypto_fill_dst_seg(cmd->paramp,
				    dstseg, mtod(m,caddr_t), m->m_len);
			}
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
			srcseg = nlm_crypto_fill_src_seg(cmd->paramp, srcseg,
			    (caddr_t)iov->iov_base, iov->iov_len);
			if (csp->csp_mode != CSP_MODE_DIGEST) {
				dstseg = nlm_crypto_fill_dst_seg(cmd->paramp,
				    dstseg, (caddr_t)iov->iov_base,
				    iov->iov_len);
			}
			iov++;
			iol--;
		}
	}
	case CRYPTO_BUF_CONTIG:
		srcseg = nlm_crypto_fill_src_seg(cmd->paramp, srcseg,
		    crp->crp_buf.cb_buf, crp->crp_buf.cb_buf_len);
		if (csp->csp_mode != CSP_MODE_DIGEST) {
			dstseg = nlm_crypto_fill_dst_seg(cmd->paramp, dstseg,
			    crp->crp_buf.cb_buf, crp->crp_buf.cb_buf_len);
		}
		break;
	default:
		__assert_unreachable();
	}
	return (0);
}

int
nlm_crypto_do_cipher(struct xlp_sec_softc *sc, struct xlp_sec_command *cmd,
    const struct crypto_session_params *csp)
{
	const unsigned char *cipkey = NULL;
	int ret = 0;

	if (cmd->crp->crp_cipher_key != NULL)
		cipkey = cmd->crp->crp_cipher_key;
	else
		cipkey = csp->csp_cipher_key;
	nlm_crypto_fill_pkt_ctrl(cmd->ctrlp, 0, NLM_HASH_BYPASS, 0,
	    cmd->cipheralg, cmd->ciphermode, cipkey,
	    csp->csp_cipher_klen, NULL, 0);

	nlm_crypto_fill_cipher_pkt_param(cmd->ctrlp, cmd->paramp,
	    CRYPTO_OP_IS_ENCRYPT(cmd->crp->crp_op) ? 1 : 0, cmd->ivoff,
	    cmd->ivlen, cmd->cipheroff, cmd->cipherlen);
	nlm_crypto_form_srcdst_segs(cmd, csp);

	ret = nlm_crypto_complete_sec_request(sc, cmd);
	return (ret);
}

int
nlm_crypto_do_digest(struct xlp_sec_softc *sc, struct xlp_sec_command *cmd,
    const struct crypto_session_params *csp)
{
	const char *key;
	int ret=0;

	if (cmd->crp->crp_auth_key != NULL)
		key = cmd->crp->crp_auth_key;
	else
		key = csp->csp_auth_key;
	nlm_crypto_fill_pkt_ctrl(cmd->ctrlp, csp->csp_auth_klen ? 1 : 0,
	    cmd->hashalg, cmd->hashmode, NLM_CIPHER_BYPASS, 0,
	    NULL, 0, key, csp->csp_auth_klen);

	nlm_crypto_fill_auth_pkt_param(cmd->ctrlp, cmd->paramp,
	    cmd->hashoff, cmd->hashlen, cmd->hmacpad,
	    (unsigned char *)cmd->hashdest);

	nlm_crypto_form_srcdst_segs(cmd, csp);

	ret = nlm_crypto_complete_sec_request(sc, cmd);

	return (ret);
}

int
nlm_crypto_do_cipher_digest(struct xlp_sec_softc *sc,
    struct xlp_sec_command *cmd, const struct crypto_session_params *csp)
{
	const unsigned char *cipkey = NULL;
	const char *authkey;
	int ret=0;

	if (cmd->crp->crp_cipher_key != NULL)
		cipkey = cmd->crp->crp_cipher_key;
	else
		cipkey = csp->csp_cipher_key;
	if (cmd->crp->crp_auth_key != NULL)
		authkey = cmd->crp->crp_auth_key;
	else
		authkey = csp->csp_auth_key;
	nlm_crypto_fill_pkt_ctrl(cmd->ctrlp, csp->csp_auth_klen ? 1 : 0,
	    cmd->hashalg, cmd->hashmode, cmd->cipheralg, cmd->ciphermode,
	    cipkey, csp->csp_cipher_klen,
	    authkey, csp->csp_auth_klen);

	nlm_crypto_fill_cipher_auth_pkt_param(cmd->ctrlp, cmd->paramp,
	    CRYPTO_OP_IS_ENCRYPT(cmd->crp->crp_op) ? 1 : 0, cmd->hashsrc,
	    cmd->ivoff, cmd->ivlen, cmd->hashoff, cmd->hashlen,
	    cmd->hmacpad, cmd->cipheroff, cmd->cipherlen,
	    (unsigned char *)cmd->hashdest);

	nlm_crypto_form_srcdst_segs(cmd, csp);

	ret = nlm_crypto_complete_sec_request(sc, cmd);
	return (ret);
}

int
nlm_get_digest_param(struct xlp_sec_command *cmd,
    const struct crypto_session_params *csp)
{
	switch(csp->csp_auth_alg) {
	case CRYPTO_SHA1:
		cmd->hashalg  = NLM_HASH_SHA;
		cmd->hashmode = NLM_HASH_MODE_SHA1;
		break;
	case CRYPTO_SHA1_HMAC:
		cmd->hashalg  = NLM_HASH_SHA;
		cmd->hashmode = NLM_HASH_MODE_SHA1;
		break;
	default:
		/* Not supported */
		return (-1);
	}
	return (0);
}
int
nlm_get_cipher_param(struct xlp_sec_command *cmd,
    const struct crypto_session_params *csp)
{
	switch(csp->csp_cipher_alg) {
	case CRYPTO_AES_CBC:
		cmd->cipheralg  = NLM_CIPHER_AES128;
		cmd->ciphermode = NLM_CIPHER_MODE_CBC;
		cmd->ivlen	= XLP_SEC_AES_IV_LENGTH;
		break;
	default:
		/* Not supported */
		return (-1);
	}
	return (0);
}
