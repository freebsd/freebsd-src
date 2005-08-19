/*	$FreeBSD$	*/
/*	$OpenBSD: cryptodev.c,v 1.52 2002/06/19 07:22:46 deraadt Exp $	*/

/*
 * Copyright (c) 2001 Theo de Raadt
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/sysctl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/random.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>

#include <opencrypto/cryptodev.h>
#include <opencrypto/xform.h>

struct csession {
	TAILQ_ENTRY(csession) next;
	u_int64_t	sid;
	u_int32_t	ses;

	u_int32_t	cipher;
	struct enc_xform *txform;
	u_int32_t	mac;
	struct auth_hash *thash;

	caddr_t		key;
	int		keylen;
	u_char		tmp_iv[EALG_MAX_BLOCK_LEN];

	caddr_t		mackey;
	int		mackeylen;
	u_char		tmp_mac[CRYPTO_MAX_MAC_LEN];

	struct iovec	iovec[UIO_MAXIOV];
	struct uio	uio;
	int		error;
};

struct fcrypt {
	TAILQ_HEAD(csessionlist, csession) csessions;
	int		sesn;
};

static	int cryptof_rw(struct file *fp, struct uio *uio,
		    struct ucred *cred, int flags, struct proc *);
static	int cryptof_ioctl(struct file *, u_long, caddr_t, struct proc *);
static	int cryptof_poll(struct file *, int, struct ucred *, struct proc *);
static	int cryptof_kqfilter(struct file *, struct knote *);
static	int cryptof_stat(struct file *, struct stat *, struct proc *);
static	int cryptof_close(struct file *, struct proc *);

static struct fileops cryptofops = {
    cryptof_rw,
    cryptof_rw,
    cryptof_ioctl,
    cryptof_poll,
    cryptof_kqfilter,
    cryptof_stat,
    cryptof_close
};

static struct csession *csefind(struct fcrypt *, u_int);
static int csedelete(struct fcrypt *, struct csession *);
static struct csession *cseadd(struct fcrypt *, struct csession *);
static struct csession *csecreate(struct fcrypt *, u_int64_t, caddr_t,
    u_int64_t, caddr_t, u_int64_t, u_int32_t, u_int32_t, struct enc_xform *,
    struct auth_hash *);
static int csefree(struct csession *);

static	int cryptodev_op(struct csession *, struct crypt_op *,
			struct proc *p);
static	int cryptodev_key(struct crypt_kop *);

static int
cryptof_rw(
	struct file *fp,
	struct uio *uio,
	struct ucred *active_cred,
	int flags,
	struct proc *p)
{

	return (EIO);
}

/* ARGSUSED */
static int
cryptof_ioctl(
	struct file *fp,
	u_long cmd,
	caddr_t data,
	struct proc *p)
{
	struct cryptoini cria, crie;
	struct fcrypt *fcr = (struct fcrypt *)fp->f_data;
	struct csession *cse;
	struct session_op *sop;
	struct crypt_op *cop;
	struct enc_xform *txform = NULL;
	struct auth_hash *thash = NULL;
	u_int64_t sid;
	u_int32_t ses;
	int error = 0;

	switch (cmd) {
	case CIOCGSESSION:
		sop = (struct session_op *)data;
		switch (sop->cipher) {
		case 0:
			break;
		case CRYPTO_DES_CBC:
			txform = &enc_xform_des;
			break;
		case CRYPTO_3DES_CBC:
			txform = &enc_xform_3des;
			break;
		case CRYPTO_BLF_CBC:
			txform = &enc_xform_blf;
			break;
		case CRYPTO_CAST_CBC:
			txform = &enc_xform_cast5;
			break;
		case CRYPTO_SKIPJACK_CBC:
			txform = &enc_xform_skipjack;
			break;
		case CRYPTO_AES_CBC:
			txform = &enc_xform_rijndael128;
			break;
		case CRYPTO_NULL_CBC:
			txform = &enc_xform_null;
			break;
		case CRYPTO_ARC4:
			txform = &enc_xform_arc4;
			break;
		default:
			return (EINVAL);
		}

		switch (sop->mac) {
		case 0:
			break;
		case CRYPTO_MD5_HMAC:
			thash = &auth_hash_hmac_md5_96;
			break;
		case CRYPTO_SHA1_HMAC:
			thash = &auth_hash_hmac_sha1_96;
			break;
		case CRYPTO_SHA2_HMAC:
			if (sop->mackeylen == auth_hash_hmac_sha2_256.keysize)
				thash = &auth_hash_hmac_sha2_256;
			else if (sop->mackeylen == auth_hash_hmac_sha2_384.keysize)
				thash = &auth_hash_hmac_sha2_384;
			else if (sop->mackeylen == auth_hash_hmac_sha2_512.keysize)
				thash = &auth_hash_hmac_sha2_512;
			else
				return (EINVAL);
			break;
		case CRYPTO_RIPEMD160_HMAC:
			thash = &auth_hash_hmac_ripemd_160_96;
			break;
#ifdef notdef
		case CRYPTO_MD5:
			thash = &auth_hash_md5;
			break;
		case CRYPTO_SHA1:
			thash = &auth_hash_sha1;
			break;
#endif
		case CRYPTO_NULL_HMAC:
			thash = &auth_hash_null;
			break;
		default:
			return (EINVAL);
		}

		bzero(&crie, sizeof(crie));
		bzero(&cria, sizeof(cria));

		if (txform) {
			crie.cri_alg = txform->type;
			crie.cri_klen = sop->keylen * 8;
			if (sop->keylen > txform->maxkey ||
			    sop->keylen < txform->minkey) {
				error = EINVAL;
				goto bail;
			}

			MALLOC(crie.cri_key, u_int8_t *,
			    crie.cri_klen / 8, M_XDATA, M_WAITOK);
			if ((error = copyin(sop->key, crie.cri_key,
			    crie.cri_klen / 8)))
				goto bail;
			if (thash)
				crie.cri_next = &cria;
		}

		if (thash) {
			cria.cri_alg = thash->type;
			cria.cri_klen = sop->mackeylen * 8;
			if (sop->mackeylen != thash->keysize) {
				error = EINVAL;
				goto bail;
			}

			if (cria.cri_klen) {
				MALLOC(cria.cri_key, u_int8_t *,
				    cria.cri_klen / 8, M_XDATA, M_WAITOK);
				if ((error = copyin(sop->mackey, cria.cri_key,
				    cria.cri_klen / 8)))
					goto bail;
			}
		}

		error = crypto_newsession(&sid, (txform ? &crie : &cria), 1);
		if (error)
			goto bail;

		cse = csecreate(fcr, sid, crie.cri_key, crie.cri_klen,
		    cria.cri_key, cria.cri_klen, sop->cipher, sop->mac, txform,
		    thash);

		if (cse == NULL) {
			crypto_freesession(sid);
			error = EINVAL;
			goto bail;
		}
		sop->ses = cse->ses;

bail:
		if (error) {
			if (crie.cri_key)
				FREE(crie.cri_key, M_XDATA);
			if (cria.cri_key)
				FREE(cria.cri_key, M_XDATA);
		}
		break;
	case CIOCFSESSION:
		ses = *(u_int32_t *)data;
		cse = csefind(fcr, ses);
		if (cse == NULL)
			return (EINVAL);
		csedelete(fcr, cse);
		error = csefree(cse);
		break;
	case CIOCCRYPT:
		cop = (struct crypt_op *)data;
		cse = csefind(fcr, cop->ses);
		if (cse == NULL)
			return (EINVAL);
		error = cryptodev_op(cse, cop, p);
		break;
	case CIOCKEY:
		error = cryptodev_key((struct crypt_kop *)data);
		break;
	case CIOCASYMFEAT:
		error = crypto_getfeat((int *)data);
		break;
	default:
		error = EINVAL;
	}
	return (error);
}

static int cryptodev_cb(void *);


static int
cryptodev_op(
	struct csession *cse,
	struct crypt_op *cop,
	struct proc *p)
{
	struct cryptop *crp = NULL;
	struct cryptodesc *crde = NULL, *crda = NULL;
	int i, error, s;

	if (cop->len > 256*1024-4)
		return (E2BIG);

	if (cse->txform) {
		if (cop->len == 0 || (cop->len % cse->txform->blocksize) != 0)
			return (EINVAL);
	}

	bzero(&cse->uio, sizeof(cse->uio));
	cse->uio.uio_iovcnt = 1;
	cse->uio.uio_resid = 0;
	cse->uio.uio_segflg = UIO_SYSSPACE;
	cse->uio.uio_rw = UIO_WRITE;
	cse->uio.uio_procp = p;
	cse->uio.uio_iov = cse->iovec;
	bzero(&cse->iovec, sizeof(cse->iovec));
	cse->uio.uio_iov[0].iov_len = cop->len;
	cse->uio.uio_iov[0].iov_base = malloc(cop->len, M_XDATA, M_WAITOK);
	for (i = 0; i < cse->uio.uio_iovcnt; i++)
		cse->uio.uio_resid += cse->uio.uio_iov[0].iov_len;

	crp = crypto_getreq((cse->txform != NULL) + (cse->thash != NULL));
	if (crp == NULL) {
		error = ENOMEM;
		goto bail;
	}

	if (cse->thash) {
		crda = crp->crp_desc;
		if (cse->txform)
			crde = crda->crd_next;
	} else {
		if (cse->txform)
			crde = crp->crp_desc;
		else {
			error = EINVAL;
			goto bail;
		}
	}

	if ((error = copyin(cop->src, cse->uio.uio_iov[0].iov_base, cop->len)))
		goto bail;

	if (crda) {
		crda->crd_skip = 0;
		crda->crd_len = cop->len;
		crda->crd_inject = 0;	/* ??? */

		crda->crd_alg = cse->mac;
		crda->crd_key = cse->mackey;
		crda->crd_klen = cse->mackeylen * 8;
	}

	if (crde) {
		if (cop->op == COP_ENCRYPT)
			crde->crd_flags |= CRD_F_ENCRYPT;
		else
			crde->crd_flags &= ~CRD_F_ENCRYPT;
		crde->crd_len = cop->len;
		crde->crd_inject = 0;

		crde->crd_alg = cse->cipher;
		crde->crd_key = cse->key;
		crde->crd_klen = cse->keylen * 8;
	}

	crp->crp_ilen = cop->len;
	crp->crp_flags = CRYPTO_F_IOV | CRYPTO_F_CBIMM
		       | (cop->flags & COP_F_BATCH);
	crp->crp_buf = (caddr_t)&cse->uio;
	crp->crp_callback = (int (*) (struct cryptop *)) cryptodev_cb;
	crp->crp_sid = cse->sid;
	crp->crp_opaque = (void *)cse;

	if (cop->iv) {
		if (crde == NULL) {
			error = EINVAL;
			goto bail;
		}
		if (cse->cipher == CRYPTO_ARC4) { /* XXX use flag? */
			error = EINVAL;
			goto bail;
		}
		if ((error = copyin(cop->iv, cse->tmp_iv, cse->txform->blocksize)))
			goto bail;
		bcopy(cse->tmp_iv, crde->crd_iv, cse->txform->blocksize);
		crde->crd_flags |= CRD_F_IV_EXPLICIT | CRD_F_IV_PRESENT;
		crde->crd_skip = 0;
	} else if (cse->cipher == CRYPTO_ARC4) { /* XXX use flag? */
		crde->crd_skip = 0;
	} else if (crde) {
		crde->crd_flags |= CRD_F_IV_PRESENT;
		crde->crd_skip = cse->txform->blocksize;
		crde->crd_len -= cse->txform->blocksize;
	}

	if (cop->mac) {
		if (crda == NULL) {
			error = EINVAL;
			goto bail;
		}
		crp->crp_mac=cse->tmp_mac;
	}

	s = splcrypto();	/* NB: only needed with CRYPTO_F_CBIMM */
	error = crypto_dispatch(crp);
	if (error == 0 && (crp->crp_flags & CRYPTO_F_DONE) == 0)
		error = tsleep(crp, PSOCK, "crydev", 0);
	splx(s);
	if (error)
		goto bail;

	if (crp->crp_etype != 0) {
		error = crp->crp_etype;
		goto bail;
	}

	if (cse->error) {
		error = cse->error;
		goto bail;
	}

	if (cop->dst &&
	    (error = copyout(cse->uio.uio_iov[0].iov_base, cop->dst, cop->len)))
		goto bail;

	if (cop->mac &&
	    (error = copyout(crp->crp_mac, cop->mac, cse->thash->authsize)))
		goto bail;

bail:
	if (crp)
		crypto_freereq(crp);
	if (cse->uio.uio_iov[0].iov_base)
		free(cse->uio.uio_iov[0].iov_base, M_XDATA);

	return (error);
}

static int
cryptodev_cb(void *op)
{
	struct cryptop *crp = (struct cryptop *) op;
	struct csession *cse = (struct csession *)crp->crp_opaque;

	cse->error = crp->crp_etype;
	if (crp->crp_etype == EAGAIN)
		return crypto_dispatch(crp);
	wakeup_one(crp);
	return (0);
}

static int
cryptodevkey_cb(void *op)
{
	struct cryptkop *krp = (struct cryptkop *) op;

	wakeup_one(krp);
	return (0);
}

static int
cryptodev_key(struct crypt_kop *kop)
{
	struct cryptkop *krp = NULL;
	int error = EINVAL;
	int in, out, size, i;

	if (kop->crk_iparams + kop->crk_oparams > CRK_MAXPARAM) {
		return (EFBIG);
	}

	in = kop->crk_iparams;
	out = kop->crk_oparams;
	switch (kop->crk_op) {
	case CRK_MOD_EXP:
		if (in == 3 && out == 1)
			break;
		return (EINVAL);
	case CRK_MOD_EXP_CRT:
		if (in == 6 && out == 1)
			break;
		return (EINVAL);
	case CRK_DSA_SIGN:
		if (in == 5 && out == 2)
			break;
		return (EINVAL);
	case CRK_DSA_VERIFY:
		if (in == 7 && out == 0)
			break;
		return (EINVAL);
	case CRK_DH_COMPUTE_KEY:
		if (in == 3 && out == 1)
			break;
		return (EINVAL);
	default:
		return (EINVAL);
	}

	krp = (struct cryptkop *)malloc(sizeof *krp, M_XDATA, M_WAITOK);
	if (!krp)
		return (ENOMEM);
	bzero(krp, sizeof *krp);
	krp->krp_op = kop->crk_op;
	krp->krp_status = kop->crk_status;
	krp->krp_iparams = kop->crk_iparams;
	krp->krp_oparams = kop->crk_oparams;
	krp->krp_status = 0;
	krp->krp_callback = (int (*) (struct cryptkop *)) cryptodevkey_cb;

	for (i = 0; i < CRK_MAXPARAM; i++)
		krp->krp_param[i].crp_nbits = kop->crk_param[i].crp_nbits;
	for (i = 0; i < krp->krp_iparams + krp->krp_oparams; i++) {
		size = (krp->krp_param[i].crp_nbits + 7) / 8;
		if (size == 0)
			continue;
		MALLOC(krp->krp_param[i].crp_p, caddr_t, size, M_XDATA, M_WAITOK);
		if (i >= krp->krp_iparams)
			continue;
		error = copyin(kop->crk_param[i].crp_p, krp->krp_param[i].crp_p, size);
		if (error)
			goto fail;
	}

	error = crypto_kdispatch(krp);
	if (error == 0)
		error = tsleep(krp, PSOCK, "crydev", 0);
	if (error)
		goto fail;
	
	if (krp->krp_status != 0) {
		error = krp->krp_status;
		goto fail;
	}

	for (i = krp->krp_iparams; i < krp->krp_iparams + krp->krp_oparams; i++) {
		size = (krp->krp_param[i].crp_nbits + 7) / 8;
		if (size == 0)
			continue;
		error = copyout(krp->krp_param[i].crp_p, kop->crk_param[i].crp_p, size);
		if (error)
			goto fail;
	}

fail:
	if (krp) {
		kop->crk_status = krp->krp_status;
		for (i = 0; i < CRK_MAXPARAM; i++) {
			if (krp->krp_param[i].crp_p)
				FREE(krp->krp_param[i].crp_p, M_XDATA);
		}
		free(krp, M_XDATA);
	}
	return (error);
}

/* ARGSUSED */
static int
cryptof_poll(
	struct file *fp,
	int events,
	struct ucred *active_cred,
	struct proc *p)
{

	return (0);
}

/* ARGSUSED */
static int
cryptof_kqfilter(struct file *fp, struct knote *kn)
{

	return (0);
}

/* ARGSUSED */
static int
cryptof_stat(
	struct file *fp,
	struct stat *sb,
	struct proc *p)
{

	return (EOPNOTSUPP);
}

/* ARGSUSED */
static int
cryptof_close(struct file *fp, struct proc *p)
{
	struct fcrypt *fcr = (struct fcrypt *)fp->f_data;
	struct csession *cse;

	while ((cse = TAILQ_FIRST(&fcr->csessions))) {
		TAILQ_REMOVE(&fcr->csessions, cse, next);
		(void)csefree(cse);
	}
	FREE(fcr, M_XDATA);
	fp->f_data = NULL;
	return 0;
}

static struct csession *
csefind(struct fcrypt *fcr, u_int ses)
{
	struct csession *cse;

	TAILQ_FOREACH(cse, &fcr->csessions, next)
		if (cse->ses == ses)
			return (cse);
	return (NULL);
}

static int
csedelete(struct fcrypt *fcr, struct csession *cse_del)
{
	struct csession *cse;

	TAILQ_FOREACH(cse, &fcr->csessions, next) {
		if (cse == cse_del) {
			TAILQ_REMOVE(&fcr->csessions, cse, next);
			return (1);
		}
	}
	return (0);
}
	
static struct csession *
cseadd(struct fcrypt *fcr, struct csession *cse)
{
	TAILQ_INSERT_TAIL(&fcr->csessions, cse, next);
	cse->ses = fcr->sesn++;
	return (cse);
}

struct csession *
csecreate(struct fcrypt *fcr, u_int64_t sid, caddr_t key, u_int64_t keylen,
    caddr_t mackey, u_int64_t mackeylen, u_int32_t cipher, u_int32_t mac,
    struct enc_xform *txform, struct auth_hash *thash)
{
	struct csession *cse;

	MALLOC(cse, struct csession *, sizeof(struct csession),
	    M_XDATA, M_NOWAIT);
	if (cse == NULL)
		return NULL;
	cse->key = key;
	cse->keylen = keylen/8;
	cse->mackey = mackey;
	cse->mackeylen = mackeylen/8;
	cse->sid = sid;
	cse->cipher = cipher;
	cse->mac = mac;
	cse->txform = txform;
	cse->thash = thash;
	cseadd(fcr, cse);
	return (cse);
}

static int
csefree(struct csession *cse)
{
	int error;

	error = crypto_freesession(cse->sid);
	if (cse->key)
		FREE(cse->key, M_XDATA);
	if (cse->mackey)
		FREE(cse->mackey, M_XDATA);
	FREE(cse, M_XDATA);
	return (error);
}

static int
cryptoopen(dev_t dev, int oflags, int devtype, struct proc *p)
{
	if (crypto_usercrypto == 0)
		return (ENXIO);
	return (0);
}

static int
cryptoread(dev_t dev, struct uio *uio, int ioflag)
{
	return (EIO);
}

static int
cryptowrite(dev_t dev, struct uio *uio, int ioflag)
{
	return (EIO);
}

static int
cryptoioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct file *f;
	struct fcrypt *fcr;
	int fd, error;
	switch (cmd) {
	case CRIOGET:
		MALLOC(fcr, struct fcrypt *,
		    sizeof(struct fcrypt), M_XDATA, M_WAITOK);
		TAILQ_INIT(&fcr->csessions);
		fcr->sesn = 0;

		error = falloc(p, &f, &fd);

		if (error) {
			FREE(fcr, M_XDATA);
			return (error);
		}
		fhold(f);
		f->f_flag = FREAD | FWRITE;
		f->f_type = DTYPE_CRYPTO;
		f->f_ops = &cryptofops;
		f->f_data = (caddr_t) fcr;
		*(u_int32_t *)data = fd;
		fdrop(f, p);
		break;
	default:
		error = EINVAL;
		break;
	}
	return (error);
}

#define	CRYPTO_MAJOR	70		/* from openbsd */
static struct cdevsw crypto_cdevsw = {
	/* open */	cryptoopen,
	/* close */	nullclose,
	/* read */	cryptoread,
	/* write */	cryptowrite,
	/* ioctl */	cryptoioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* dev name */	"crypto",
	/* dev major */	CRYPTO_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
	/* kqfilter */	NULL
};
static dev_t crypto_dev;

/*
 * Initialization code, both for static and dynamic loading.
 */
static int
cryptodev_modevent(module_t mod, int type, void *unused)
{
	switch (type) {
	case MOD_LOAD:
		if (bootverbose)
			printf("crypto: <crypto device>\n");
		crypto_dev = make_dev(&crypto_cdevsw, 0, 
				      UID_ROOT, GID_WHEEL, 0666,
				      "crypto");
		return 0;
	case MOD_UNLOAD:
		/*XXX disallow if active sessions */
		destroy_dev(crypto_dev);
		return 0;
	}
	return EINVAL;
}

static moduledata_t cryptodev_mod = {
	"cryptodev",
	cryptodev_modevent,
	0
};
MODULE_VERSION(cryptodev, 1);
DECLARE_MODULE(cryptodev, cryptodev_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
#if 0
MODULE_DEPEND(cryptodev, crypto, 1, 1, 1);
#endif
