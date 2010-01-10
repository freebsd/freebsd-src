/*-
 * Copyright (c) 2003-2009 RMI Corporation
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
 * 3. Neither the name of RMI Corporation, nor the names of its contributors,
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 * RMI_BSD */


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

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <opencrypto/cryptodev.h>
#include <sys/random.h>

#include <dev/rmi/sec/rmilib.h>

/*#define RMI_SEC_DEBUG */


void xlr_sec_print_data(struct cryptop *crp);

static int xlr_sec_newsession(void *arg, uint32_t * sidp, struct cryptoini *cri);
static int xlr_sec_freesession(void *arg, uint64_t tid);
static int xlr_sec_process(void *arg, struct cryptop *crp, int hint);


static int xlr_sec_probe(device_t);
static int xlr_sec_attach(device_t);
static int xlr_sec_detach(device_t);


static device_method_t xlr_sec_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe, xlr_sec_probe),
	DEVMETHOD(device_attach, xlr_sec_attach),
	DEVMETHOD(device_detach, xlr_sec_detach),

	/* bus interface */
	DEVMETHOD(bus_print_child, bus_generic_print_child),
	DEVMETHOD(bus_driver_added, bus_generic_driver_added),

	{0, 0}
};

static driver_t xlr_sec_driver = {
	"rmisec",
	xlr_sec_methods,
	sizeof(struct xlr_sec_softc)
};
static devclass_t xlr_sec_devclass;

DRIVER_MODULE(rmisec, iodi, xlr_sec_driver, xlr_sec_devclass, 0, 0);
MODULE_DEPEND(rmisec, crypto, 1, 1, 1);



static int
xlr_sec_probe(device_t dev)
{
	return (BUS_PROBE_DEFAULT);

}


/*
 * Attach an interface that successfully probed.
 */
static int
xlr_sec_attach(device_t dev)
{

	struct xlr_sec_softc *sc = device_get_softc(dev);

	bzero(sc, sizeof(*sc));
	sc->sc_dev = dev;


	mtx_init(&sc->sc_mtx, device_get_nameunit(dev), "rmi crypto driver", MTX_DEF);

	sc->sc_cid = crypto_get_driverid(0);
	if (sc->sc_cid < 0) {
		printf("xlr_sec - error : could not get the driver id\n");
		goto error_exit;
	}
	if (crypto_register(sc->sc_cid, CRYPTO_DES_CBC, 0, 0,
	    xlr_sec_newsession, xlr_sec_freesession, xlr_sec_process, sc) != 0)
		printf("register failed for CRYPTO_DES_CBC\n");

	if (crypto_register(sc->sc_cid, CRYPTO_3DES_CBC, 0, 0,
	    xlr_sec_newsession, xlr_sec_freesession, xlr_sec_process, sc) != 0)
		printf("register failed for CRYPTO_3DES_CBC\n");

	if (crypto_register(sc->sc_cid, CRYPTO_AES_CBC, 0, 0,
	    xlr_sec_newsession, xlr_sec_freesession,
	    xlr_sec_process, sc) != 0)
		printf("register failed for CRYPTO_AES_CBC\n");

	if (crypto_register(sc->sc_cid, CRYPTO_ARC4, 0, 0,
	    xlr_sec_newsession, xlr_sec_freesession, xlr_sec_process, sc) != 0)
		printf("register failed for CRYPTO_ARC4\n");


	if (crypto_register(sc->sc_cid, CRYPTO_MD5, 0, 0,
	    xlr_sec_newsession, xlr_sec_freesession, xlr_sec_process, sc) != 0)
		printf("register failed for CRYPTO_MD5\n");

	if (crypto_register(sc->sc_cid, CRYPTO_SHA1, 0, 0,
	    xlr_sec_newsession, xlr_sec_freesession, xlr_sec_process, sc) != 0)
		printf("register failed for CRYPTO_SHA1\n");

	if (crypto_register(sc->sc_cid, CRYPTO_MD5_HMAC, 0, 0,
	    xlr_sec_newsession, xlr_sec_freesession, xlr_sec_process, sc) != 0)
		printf("register failed for CRYPTO_MD5_HMAC\n");

	if (crypto_register(sc->sc_cid, CRYPTO_SHA1_HMAC, 0, 0,
	    xlr_sec_newsession, xlr_sec_freesession, xlr_sec_process, sc) != 0)
		printf("register failed for CRYPTO_SHA1_HMAC\n");


	xlr_sec_init(sc);
	return (0);


error_exit:
	return (ENXIO);

}


/*
 * Detach an interface that successfully probed.
 */
static int
xlr_sec_detach(device_t dev)
{
	int sesn;
	struct xlr_sec_softc *sc = device_get_softc(dev);
	struct xlr_sec_session *ses = NULL;
	symkey_desc_pt desc;

	for (sesn = 0; sesn < sc->sc_nsessions; sesn++) {
		ses = &sc->sc_sessions[sesn];
		desc = (symkey_desc_pt) ses->desc_ptr;
		free(desc->user.kern_src, M_DEVBUF);
		free(desc->user.kern_dest, M_DEVBUF);
		free(desc->next_src_buf, M_DEVBUF);
		free(desc->next_dest_buf, M_DEVBUF);
		free(ses->desc_ptr, M_DEVBUF);
	}

	return (0);
}




/*
 * Allocate a new 'session' and return an encoded session id.  'sidp'
 * contains our registration id, and should contain an encoded session
 * id on successful allocation.
 */
static int
xlr_sec_newsession(void *arg, u_int32_t * sidp, struct cryptoini *cri)
{
	struct cryptoini *c;
	struct xlr_sec_softc *sc = arg;
	int mac = 0, cry = 0, sesn;
	struct xlr_sec_session *ses = NULL;


	if (sidp == NULL || cri == NULL || sc == NULL)
		return (EINVAL);


	if (sc->sc_sessions == NULL) {
		ses = sc->sc_sessions = (struct xlr_sec_session *)malloc(
		    sizeof(struct xlr_sec_session), M_DEVBUF, M_NOWAIT);
		if (ses == NULL)
			return (ENOMEM);

		ses->desc_ptr = (void *)xlr_sec_allocate_desc((void *)ses);
		if (ses->desc_ptr == NULL)
			return (ENOMEM);

		sesn = 0;
		ses->sessionid = sesn;
		sc->sc_nsessions = 1;
	} else {
		for (sesn = 0; sesn < sc->sc_nsessions; sesn++) {
			if (!sc->sc_sessions[sesn].hs_used) {
				ses = &sc->sc_sessions[sesn];
				break;
			}
		}

		if (ses == NULL) {
			sesn = sc->sc_nsessions;
			ses = (struct xlr_sec_session *)malloc((sesn + 1) *
			    sizeof(struct xlr_sec_session), M_DEVBUF, M_NOWAIT);
			if (ses == NULL)
				return (ENOMEM);
			bcopy(sc->sc_sessions, ses, sesn * sizeof(struct xlr_sec_session));
			bzero(sc->sc_sessions, sesn * sizeof(struct xlr_sec_session));
			free(sc->sc_sessions, M_DEVBUF);
			sc->sc_sessions = ses;
			ses = &sc->sc_sessions[sesn];
			ses->sessionid = sesn;
			ses->desc_ptr = (void *)xlr_sec_allocate_desc((void *)ses);
			if (ses->desc_ptr == NULL)
				return (ENOMEM);
			sc->sc_nsessions++;
		}
	}
	ses->hs_used = 1;


	for (c = cri; c != NULL; c = c->cri_next) {

		switch (c->cri_alg) {
		case CRYPTO_MD5:
		case CRYPTO_SHA1:
		case CRYPTO_MD5_HMAC:
		case CRYPTO_SHA1_HMAC:
			if (mac)
				return (EINVAL);
			mac = 1;
			ses->hs_mlen = c->cri_mlen;
			if (ses->hs_mlen == 0) {
				switch (c->cri_alg) {
				case CRYPTO_MD5:
				case CRYPTO_MD5_HMAC:
					ses->hs_mlen = 16;
					break;
				case CRYPTO_SHA1:
				case CRYPTO_SHA1_HMAC:
					ses->hs_mlen = 20;
					break;
				}
			}
			break;
		case CRYPTO_DES_CBC:
		case CRYPTO_3DES_CBC:
		case CRYPTO_AES_CBC:
			/* XXX this may read fewer, does it matter? */
			/*
			 * read_random(ses->hs_iv, c->cri_alg ==
			 * CRYPTO_AES_CBC ? XLR_SEC_AES_IV_LENGTH :
			 * XLR_SEC_IV_LENGTH);
			 */
			/* FALLTHROUGH */
		case CRYPTO_ARC4:
			if (cry)
				return (EINVAL);
			cry = 1;
			break;
		default:
			return (EINVAL);
		}
	}
	if (mac == 0 && cry == 0)
		return (EINVAL);

	*sidp = XLR_SEC_SID(device_get_unit(sc->sc_dev), sesn);
	return (0);
}

/*
 * Deallocate a session.
 * XXX this routine should run a zero'd mac/encrypt key into context ram.
 * XXX to blow away any keys already stored there.
 */
static int
xlr_sec_freesession(void *arg, u_int64_t tid)
{
	struct xlr_sec_softc *sc = arg;
	int session;
	u_int32_t sid = CRYPTO_SESID2LID(tid);

	if (sc == NULL)
		return (EINVAL);

	session = XLR_SEC_SESSION(sid);
	if (session >= sc->sc_nsessions)
		return (EINVAL);

	sc->sc_sessions[session].hs_used = 0;

	return (0);
}

#ifdef RMI_SEC_DEBUG

void 
xlr_sec_print_data(struct cryptop *crp)
{
	int i, key_len;
	struct cryptodesc *crp_desc;

	printf("session id = 0x%llx, crp_ilen = %d, crp_olen=%d \n",
	    crp->crp_sid, crp->crp_ilen, crp->crp_olen);

	printf("crp_flags = 0x%x\n", crp->crp_flags);


	printf("crp buf:\n");
	for (i = 0; i < crp->crp_ilen; i++) {
		printf("%c  ", crp->crp_buf[i]);
		if (i % 10 == 0)
			printf("\n");
	}

	printf("\n");
	printf("****************** desc ****************\n");
	crp_desc = crp->crp_desc;
	printf("crd_skip=%d, crd_len=%d, crd_flags=0x%x, crd_alg=%d\n",
	    crp_desc->crd_skip, crp_desc->crd_len, crp_desc->crd_flags, crp_desc->crd_alg);

	key_len = crp_desc->crd_klen / 8;
	printf("key(%d) :\n", key_len);
	for (i = 0; i < key_len; i++)
		printf("%d", crp_desc->crd_key[i]);
	printf("\n");

	printf(" IV : \n");
	for (i = 0; i < EALG_MAX_BLOCK_LEN; i++)
		printf("%d", crp_desc->crd_iv[i]);
	printf("\n");

	printf("crd_next=%p\n", crp_desc->crd_next);
	return;
}

#endif


static int
xlr_sec_process(void *arg, struct cryptop *crp, int hint)
{
	struct xlr_sec_softc *sc = arg;
	struct xlr_sec_command *cmd = NULL;
	int session, err;
	struct cryptodesc *crd1, *crd2, *maccrd, *enccrd;
	struct xlr_sec_session *ses;

	if (crp == NULL || crp->crp_callback == NULL) {
		return (EINVAL);
	}
	session = XLR_SEC_SESSION(crp->crp_sid);
	if (sc == NULL || session >= sc->sc_nsessions) {
		err = EINVAL;
		goto errout;
	}
	ses = &sc->sc_sessions[session];

	cmd = &ses->cmd;
	if (cmd == NULL) {
		err = ENOMEM;
		goto errout;
	}
	crd1 = crp->crp_desc;
	if (crd1 == NULL) {
		err = EINVAL;
		goto errout;
	}
	crd2 = crd1->crd_next;

	if (crd2 == NULL) {
		if (crd1->crd_alg == CRYPTO_MD5_HMAC ||
		    crd1->crd_alg == CRYPTO_SHA1_HMAC ||
		    crd1->crd_alg == CRYPTO_SHA1 ||
		    crd1->crd_alg == CRYPTO_MD5) {
			maccrd = crd1;
			enccrd = NULL;
		} else if (crd1->crd_alg == CRYPTO_DES_CBC ||
			    crd1->crd_alg == CRYPTO_3DES_CBC ||
			    crd1->crd_alg == CRYPTO_AES_CBC ||
		    crd1->crd_alg == CRYPTO_ARC4) {
			maccrd = NULL;
			enccrd = crd1;
		} else {
			err = EINVAL;
			goto errout;
		}
	} else {
		if ((crd1->crd_alg == CRYPTO_MD5_HMAC ||
		    crd1->crd_alg == CRYPTO_SHA1_HMAC ||
		    crd1->crd_alg == CRYPTO_MD5 ||
		    crd1->crd_alg == CRYPTO_SHA1) &&
		    (crd2->crd_alg == CRYPTO_DES_CBC ||
		    crd2->crd_alg == CRYPTO_3DES_CBC ||
		    crd2->crd_alg == CRYPTO_AES_CBC ||
		    crd2->crd_alg == CRYPTO_ARC4)) {
			maccrd = crd1;
			enccrd = crd2;
		} else if ((crd1->crd_alg == CRYPTO_DES_CBC ||
			    crd1->crd_alg == CRYPTO_ARC4 ||
			    crd1->crd_alg == CRYPTO_3DES_CBC ||
			    crd1->crd_alg == CRYPTO_AES_CBC) &&
			    (crd2->crd_alg == CRYPTO_MD5_HMAC ||
			    crd2->crd_alg == CRYPTO_SHA1_HMAC ||
			    crd2->crd_alg == CRYPTO_MD5 ||
			    crd2->crd_alg == CRYPTO_SHA1) &&
		    (crd1->crd_flags & CRD_F_ENCRYPT)) {
			enccrd = crd1;
			maccrd = crd2;
		} else {
			err = EINVAL;
			goto errout;
		}
	}

	bzero(&cmd->op, sizeof(xlr_sec_io_t));

	cmd->op.source_buf = (uint64_t) (unsigned long)crp->crp_buf;
	cmd->op.source_buf_size = crp->crp_ilen;
	if (crp->crp_flags & CRYPTO_F_REL) {
		cmd->op.dest_buf = (uint64_t) (unsigned long)crp->crp_buf;
		cmd->op.dest_buf_size = crp->crp_ilen;
	} else {
		cmd->op.dest_buf = (uint64_t) (unsigned long)crp->crp_buf;
		cmd->op.dest_buf_size = crp->crp_ilen;
	}
	cmd->op.num_packets = 1;
	cmd->op.num_fragments = 1;


	if (cmd->op.source_buf_size > SEC_MAX_FRAG_LEN) {
		ses->multi_frag_flag = 1;
	} else {
		ses->multi_frag_flag = 0;
	}

	if (maccrd) {
		cmd->maccrd = maccrd;
		cmd->op.cipher_op = XLR_SEC_CIPHER_MODE_PASS;
		cmd->op.cipher_mode = XLR_SEC_CIPHER_MODE_NONE;
		cmd->op.cipher_type = XLR_SEC_CIPHER_TYPE_NONE;
		cmd->op.cipher_init = 0;
		cmd->op.cipher_offset = 0;

		switch (maccrd->crd_alg) {
		case CRYPTO_MD5:
			cmd->op.digest_type = XLR_SEC_DIGEST_TYPE_MD5;
			cmd->op.digest_init = XLR_SEC_DIGEST_INIT_NEWKEY;
			cmd->op.digest_src = XLR_SEC_DIGEST_SRC_DMA;
			cmd->op.digest_offset = 0;

			cmd->op.cksum_type = XLR_SEC_CKSUM_TYPE_NOP;
			cmd->op.cksum_src = XLR_SEC_CKSUM_SRC_CIPHER;
			cmd->op.cksum_offset = 0;

			cmd->op.pkt_hmac = XLR_SEC_LOADHMACKEY_MODE_OLD;
			cmd->op.pkt_hash = XLR_SEC_PADHASH_PAD;
			cmd->op.pkt_hashbytes = XLR_SEC_HASHBYTES_ALL8;
			cmd->op.pkt_next = XLR_SEC_NEXT_FINISH;
			cmd->op.pkt_iv = XLR_SEC_PKT_IV_OLD;
			cmd->op.pkt_lastword = XLR_SEC_LASTWORD_128;


		default:
			printf("currently not handled\n");
		}
	}
	if (enccrd) {
		cmd->enccrd = enccrd;

#ifdef RMI_SEC_DEBUG
		xlr_sec_print_data(crp);
#endif

		if (enccrd->crd_flags & CRD_F_ENCRYPT) {
			cmd->op.cipher_op = XLR_SEC_CIPHER_OP_ENCRYPT;
		} else
			cmd->op.cipher_op = XLR_SEC_CIPHER_OP_DECRYPT;

		switch (enccrd->crd_alg) {
		case CRYPTO_DES_CBC:
		case CRYPTO_3DES_CBC:
			if (enccrd->crd_alg == CRYPTO_DES_CBC) {
				cmd->op.cipher_type = XLR_SEC_CIPHER_TYPE_DES;
				memcpy(&cmd->op.crypt_key[0], enccrd->crd_key, XLR_SEC_DES_KEY_LENGTH);
			} else {
				cmd->op.cipher_type = XLR_SEC_CIPHER_TYPE_3DES;
				//if (enccrd->crd_flags & CRD_F_KEY_EXPLICIT) {
					memcpy(&cmd->op.crypt_key[0], enccrd->crd_key, XLR_SEC_3DES_KEY_LENGTH);
				}
			}

			cmd->op.cipher_mode = XLR_SEC_CIPHER_MODE_CBC;
			cmd->op.cipher_init = XLR_SEC_CIPHER_INIT_NK;
			cmd->op.cipher_offset = XLR_SEC_DES_IV_LENGTH;

			cmd->op.digest_type = XLR_SEC_DIGEST_TYPE_NONE;
			cmd->op.digest_init = XLR_SEC_DIGEST_INIT_OLDKEY;
			cmd->op.digest_src = XLR_SEC_DIGEST_SRC_DMA;
			cmd->op.digest_offset = 0;

			cmd->op.cksum_type = XLR_SEC_CKSUM_TYPE_NOP;
			cmd->op.cksum_src = XLR_SEC_CKSUM_SRC_CIPHER;
			cmd->op.cksum_offset = 0;

			cmd->op.pkt_hmac = XLR_SEC_LOADHMACKEY_MODE_OLD;
			cmd->op.pkt_hash = XLR_SEC_PADHASH_PAD;
			cmd->op.pkt_hashbytes = XLR_SEC_HASHBYTES_ALL8;
			cmd->op.pkt_next = XLR_SEC_NEXT_FINISH;
			cmd->op.pkt_iv = XLR_SEC_PKT_IV_NEW;
			cmd->op.pkt_lastword = XLR_SEC_LASTWORD_128;

			//if ((!(enccrd->crd_flags & CRD_F_IV_PRESENT)) &&
				    if ((enccrd->crd_flags & CRD_F_IV_EXPLICIT)) {
				memcpy(&cmd->op.initial_vector[0], enccrd->crd_iv, XLR_SEC_DES_IV_LENGTH);
				}
			break;

		case CRYPTO_AES_CBC:
			if (enccrd->crd_alg == CRYPTO_AES_CBC) {
				cmd->op.cipher_type = XLR_SEC_CIPHER_TYPE_AES128;
				//if (enccrd->crd_flags & CRD_F_KEY_EXPLICIT) {
					memcpy(&cmd->op.crypt_key[0], enccrd->crd_key, XLR_SEC_AES128_KEY_LENGTH);
				}
			}
			cmd->op.cipher_mode = XLR_SEC_CIPHER_MODE_CBC;
			cmd->op.cipher_init = XLR_SEC_CIPHER_INIT_NK;
			cmd->op.cipher_offset = XLR_SEC_AES_BLOCK_SIZE;

			cmd->op.digest_type = XLR_SEC_DIGEST_TYPE_NONE;
			cmd->op.digest_init = XLR_SEC_DIGEST_INIT_OLDKEY;
			cmd->op.digest_src = XLR_SEC_DIGEST_SRC_DMA;
			cmd->op.digest_offset = 0;

			cmd->op.cksum_type = XLR_SEC_CKSUM_TYPE_NOP;
			cmd->op.cksum_src = XLR_SEC_CKSUM_SRC_CIPHER;
			cmd->op.cksum_offset = 0;

			cmd->op.pkt_hmac = XLR_SEC_LOADHMACKEY_MODE_OLD;
			cmd->op.pkt_hash = XLR_SEC_PADHASH_PAD;
			cmd->op.pkt_hashbytes = XLR_SEC_HASHBYTES_ALL8;
			cmd->op.pkt_next = XLR_SEC_NEXT_FINISH;
			cmd->op.pkt_iv = XLR_SEC_PKT_IV_NEW;
			cmd->op.pkt_lastword = XLR_SEC_LASTWORD_128;

			//if (!(enccrd->crd_flags & CRD_F_IV_PRESENT)) {
				if ((enccrd->crd_flags & CRD_F_IV_EXPLICIT)) {
					memcpy(&cmd->op.initial_vector[0], enccrd->crd_iv, XLR_SEC_AES_BLOCK_SIZE);
				}
				//
			}
			break;
		}
	}
	cmd->crp = crp;
	cmd->session_num = session;
	xlr_sec_setup(ses, cmd, (symkey_desc_pt) ses->desc_ptr);

	return (0);

errout:
	if (cmd != NULL)
		free(cmd, M_DEVBUF);
	crp->crp_etype = err;
	crypto_done(crp);
	return (err);
}
