/*	$FreeBSD$	*/
/*	$KAME: key.c,v 1.191 2001/06/27 10:46:49 sakane Exp $	*/

/*-
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * This code is referd to RFC 2367
 */

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/refcount.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/route.h>
#include <net/raw_cb.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_var.h>

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/in6_var.h>
#include <netinet6/ip6_var.h>
#endif /* INET6 */

#if defined(INET) || defined(INET6)
#include <netinet/in_pcb.h>
#endif
#ifdef INET6
#include <netinet6/in6_pcb.h>
#endif /* INET6 */

#include <net/pfkeyv2.h>
#include <netipsec/keydb.h>
#include <netipsec/key.h>
#include <netipsec/keysock.h>
#include <netipsec/key_debug.h>

#include <netipsec/ipsec.h>
#ifdef INET6
#include <netipsec/ipsec6.h>
#endif

#include <netipsec/xform.h>

#include <machine/stdarg.h>

/* randomness */
#include <sys/random.h>

#define FULLMASK	0xff
#define	_BITS(bytes)	((bytes) << 3)

/*
 * Note on SA reference counting:
 * - SAs that are not in DEAD state will have (total external reference + 1)
 *   following value in reference count field.  they cannot be freed and are
 *   referenced from SA header.
 * - SAs that are in DEAD state will have (total external reference)
 *   in reference count field.  they are ready to be freed.  reference from
 *   SA header will be removed in key_delsav(), when the reference count
 *   field hits 0 (= no external reference other than from SA header.
 */

VNET_DEFINE(u_int32_t, key_debug_level) = 0;
static VNET_DEFINE(u_int, key_spi_trycnt) = 1000;
static VNET_DEFINE(u_int32_t, key_spi_minval) = 0x100;
static VNET_DEFINE(u_int32_t, key_spi_maxval) = 0x0fffffff;	/* XXX */
static VNET_DEFINE(u_int32_t, policy_id) = 0;
/*interval to initialize randseed,1(m)*/
static VNET_DEFINE(u_int, key_int_random) = 60;
/* interval to expire acquiring, 30(s)*/
static VNET_DEFINE(u_int, key_larval_lifetime) = 30;
/* counter for blocking SADB_ACQUIRE.*/
static VNET_DEFINE(int, key_blockacq_count) = 10;
/* lifetime for blocking SADB_ACQUIRE.*/
static VNET_DEFINE(int, key_blockacq_lifetime) = 20;
/* preferred old sa rather than new sa.*/
static VNET_DEFINE(int, key_preferred_oldsa) = 1;
#define	V_key_spi_trycnt	VNET(key_spi_trycnt)
#define	V_key_spi_minval	VNET(key_spi_minval)
#define	V_key_spi_maxval	VNET(key_spi_maxval)
#define	V_policy_id		VNET(policy_id)
#define	V_key_int_random	VNET(key_int_random)
#define	V_key_larval_lifetime	VNET(key_larval_lifetime)
#define	V_key_blockacq_count	VNET(key_blockacq_count)
#define	V_key_blockacq_lifetime	VNET(key_blockacq_lifetime)
#define	V_key_preferred_oldsa	VNET(key_preferred_oldsa)

static VNET_DEFINE(u_int32_t, acq_seq) = 0;
#define	V_acq_seq		VNET(acq_seq)

								/* SPD */
static VNET_DEFINE(LIST_HEAD(_sptree, secpolicy), sptree[IPSEC_DIR_MAX]);
#define	V_sptree		VNET(sptree)
static struct mtx sptree_lock;
#define	SPTREE_LOCK_INIT() \
	mtx_init(&sptree_lock, "sptree", \
		"fast ipsec security policy database", MTX_DEF)
#define	SPTREE_LOCK_DESTROY()	mtx_destroy(&sptree_lock)
#define	SPTREE_LOCK()		mtx_lock(&sptree_lock)
#define	SPTREE_UNLOCK()	mtx_unlock(&sptree_lock)
#define	SPTREE_LOCK_ASSERT()	mtx_assert(&sptree_lock, MA_OWNED)

static VNET_DEFINE(LIST_HEAD(_sahtree, secashead), sahtree);	/* SAD */
#define	V_sahtree		VNET(sahtree)
static struct mtx sahtree_lock;
#define	SAHTREE_LOCK_INIT() \
	mtx_init(&sahtree_lock, "sahtree", \
		"fast ipsec security association database", MTX_DEF)
#define	SAHTREE_LOCK_DESTROY()	mtx_destroy(&sahtree_lock)
#define	SAHTREE_LOCK()		mtx_lock(&sahtree_lock)
#define	SAHTREE_UNLOCK()	mtx_unlock(&sahtree_lock)
#define	SAHTREE_LOCK_ASSERT()	mtx_assert(&sahtree_lock, MA_OWNED)

							/* registed list */
static VNET_DEFINE(LIST_HEAD(_regtree, secreg), regtree[SADB_SATYPE_MAX + 1]);
#define	V_regtree		VNET(regtree)
static struct mtx regtree_lock;
#define	REGTREE_LOCK_INIT() \
	mtx_init(&regtree_lock, "regtree", "fast ipsec regtree", MTX_DEF)
#define	REGTREE_LOCK_DESTROY()	mtx_destroy(&regtree_lock)
#define	REGTREE_LOCK()		mtx_lock(&regtree_lock)
#define	REGTREE_UNLOCK()	mtx_unlock(&regtree_lock)
#define	REGTREE_LOCK_ASSERT()	mtx_assert(&regtree_lock, MA_OWNED)

static VNET_DEFINE(LIST_HEAD(_acqtree, secacq), acqtree); /* acquiring list */
#define	V_acqtree		VNET(acqtree)
static struct mtx acq_lock;
#define	ACQ_LOCK_INIT() \
	mtx_init(&acq_lock, "acqtree", "fast ipsec acquire list", MTX_DEF)
#define	ACQ_LOCK_DESTROY()	mtx_destroy(&acq_lock)
#define	ACQ_LOCK()		mtx_lock(&acq_lock)
#define	ACQ_UNLOCK()		mtx_unlock(&acq_lock)
#define	ACQ_LOCK_ASSERT()	mtx_assert(&acq_lock, MA_OWNED)

							/* SP acquiring list */
static VNET_DEFINE(LIST_HEAD(_spacqtree, secspacq), spacqtree);
#define	V_spacqtree		VNET(spacqtree)
static struct mtx spacq_lock;
#define	SPACQ_LOCK_INIT() \
	mtx_init(&spacq_lock, "spacqtree", \
		"fast ipsec security policy acquire list", MTX_DEF)
#define	SPACQ_LOCK_DESTROY()	mtx_destroy(&spacq_lock)
#define	SPACQ_LOCK()		mtx_lock(&spacq_lock)
#define	SPACQ_UNLOCK()		mtx_unlock(&spacq_lock)
#define	SPACQ_LOCK_ASSERT()	mtx_assert(&spacq_lock, MA_OWNED)

/* search order for SAs */
static const u_int saorder_state_valid_prefer_old[] = {
	SADB_SASTATE_DYING, SADB_SASTATE_MATURE,
};
static const u_int saorder_state_valid_prefer_new[] = {
	SADB_SASTATE_MATURE, SADB_SASTATE_DYING,
};
static const u_int saorder_state_alive[] = {
	/* except DEAD */
	SADB_SASTATE_MATURE, SADB_SASTATE_DYING, SADB_SASTATE_LARVAL
};
static const u_int saorder_state_any[] = {
	SADB_SASTATE_MATURE, SADB_SASTATE_DYING,
	SADB_SASTATE_LARVAL, SADB_SASTATE_DEAD
};

static const int minsize[] = {
	sizeof(struct sadb_msg),	/* SADB_EXT_RESERVED */
	sizeof(struct sadb_sa),		/* SADB_EXT_SA */
	sizeof(struct sadb_lifetime),	/* SADB_EXT_LIFETIME_CURRENT */
	sizeof(struct sadb_lifetime),	/* SADB_EXT_LIFETIME_HARD */
	sizeof(struct sadb_lifetime),	/* SADB_EXT_LIFETIME_SOFT */
	sizeof(struct sadb_address),	/* SADB_EXT_ADDRESS_SRC */
	sizeof(struct sadb_address),	/* SADB_EXT_ADDRESS_DST */
	sizeof(struct sadb_address),	/* SADB_EXT_ADDRESS_PROXY */
	sizeof(struct sadb_key),	/* SADB_EXT_KEY_AUTH */
	sizeof(struct sadb_key),	/* SADB_EXT_KEY_ENCRYPT */
	sizeof(struct sadb_ident),	/* SADB_EXT_IDENTITY_SRC */
	sizeof(struct sadb_ident),	/* SADB_EXT_IDENTITY_DST */
	sizeof(struct sadb_sens),	/* SADB_EXT_SENSITIVITY */
	sizeof(struct sadb_prop),	/* SADB_EXT_PROPOSAL */
	sizeof(struct sadb_supported),	/* SADB_EXT_SUPPORTED_AUTH */
	sizeof(struct sadb_supported),	/* SADB_EXT_SUPPORTED_ENCRYPT */
	sizeof(struct sadb_spirange),	/* SADB_EXT_SPIRANGE */
	0,				/* SADB_X_EXT_KMPRIVATE */
	sizeof(struct sadb_x_policy),	/* SADB_X_EXT_POLICY */
	sizeof(struct sadb_x_sa2),	/* SADB_X_SA2 */
	sizeof(struct sadb_x_nat_t_type),/* SADB_X_EXT_NAT_T_TYPE */
	sizeof(struct sadb_x_nat_t_port),/* SADB_X_EXT_NAT_T_SPORT */
	sizeof(struct sadb_x_nat_t_port),/* SADB_X_EXT_NAT_T_DPORT */
	sizeof(struct sadb_address),	/* SADB_X_EXT_NAT_T_OAI */
	sizeof(struct sadb_address),	/* SADB_X_EXT_NAT_T_OAR */
	sizeof(struct sadb_x_nat_t_frag),/* SADB_X_EXT_NAT_T_FRAG */
};
static const int maxsize[] = {
	sizeof(struct sadb_msg),	/* SADB_EXT_RESERVED */
	sizeof(struct sadb_sa),		/* SADB_EXT_SA */
	sizeof(struct sadb_lifetime),	/* SADB_EXT_LIFETIME_CURRENT */
	sizeof(struct sadb_lifetime),	/* SADB_EXT_LIFETIME_HARD */
	sizeof(struct sadb_lifetime),	/* SADB_EXT_LIFETIME_SOFT */
	0,				/* SADB_EXT_ADDRESS_SRC */
	0,				/* SADB_EXT_ADDRESS_DST */
	0,				/* SADB_EXT_ADDRESS_PROXY */
	0,				/* SADB_EXT_KEY_AUTH */
	0,				/* SADB_EXT_KEY_ENCRYPT */
	0,				/* SADB_EXT_IDENTITY_SRC */
	0,				/* SADB_EXT_IDENTITY_DST */
	0,				/* SADB_EXT_SENSITIVITY */
	0,				/* SADB_EXT_PROPOSAL */
	0,				/* SADB_EXT_SUPPORTED_AUTH */
	0,				/* SADB_EXT_SUPPORTED_ENCRYPT */
	sizeof(struct sadb_spirange),	/* SADB_EXT_SPIRANGE */
	0,				/* SADB_X_EXT_KMPRIVATE */
	0,				/* SADB_X_EXT_POLICY */
	sizeof(struct sadb_x_sa2),	/* SADB_X_SA2 */
	sizeof(struct sadb_x_nat_t_type),/* SADB_X_EXT_NAT_T_TYPE */
	sizeof(struct sadb_x_nat_t_port),/* SADB_X_EXT_NAT_T_SPORT */
	sizeof(struct sadb_x_nat_t_port),/* SADB_X_EXT_NAT_T_DPORT */
	0,				/* SADB_X_EXT_NAT_T_OAI */
	0,				/* SADB_X_EXT_NAT_T_OAR */
	sizeof(struct sadb_x_nat_t_frag),/* SADB_X_EXT_NAT_T_FRAG */
};

static VNET_DEFINE(int, ipsec_esp_keymin) = 256;
static VNET_DEFINE(int, ipsec_esp_auth) = 0;
static VNET_DEFINE(int, ipsec_ah_keymin) = 128;

#define	V_ipsec_esp_keymin	VNET(ipsec_esp_keymin)
#define	V_ipsec_esp_auth	VNET(ipsec_esp_auth)
#define	V_ipsec_ah_keymin	VNET(ipsec_ah_keymin)

#ifdef SYSCTL_DECL
SYSCTL_DECL(_net_key);
#endif

SYSCTL_VNET_INT(_net_key, KEYCTL_DEBUG_LEVEL,	debug,
	CTLFLAG_RW, &VNET_NAME(key_debug_level),	0,	"");

/* max count of trial for the decision of spi value */
SYSCTL_VNET_INT(_net_key, KEYCTL_SPI_TRY, spi_trycnt,
	CTLFLAG_RW, &VNET_NAME(key_spi_trycnt),	0,	"");

/* minimum spi value to allocate automatically. */
SYSCTL_VNET_INT(_net_key, KEYCTL_SPI_MIN_VALUE,
	spi_minval,	CTLFLAG_RW, &VNET_NAME(key_spi_minval),	0,	"");

/* maximun spi value to allocate automatically. */
SYSCTL_VNET_INT(_net_key, KEYCTL_SPI_MAX_VALUE,
	spi_maxval,	CTLFLAG_RW, &VNET_NAME(key_spi_maxval),	0,	"");

/* interval to initialize randseed */
SYSCTL_VNET_INT(_net_key, KEYCTL_RANDOM_INT,
	int_random,	CTLFLAG_RW, &VNET_NAME(key_int_random),	0,	"");

/* lifetime for larval SA */
SYSCTL_VNET_INT(_net_key, KEYCTL_LARVAL_LIFETIME,
	larval_lifetime, CTLFLAG_RW, &VNET_NAME(key_larval_lifetime),	0, "");

/* counter for blocking to send SADB_ACQUIRE to IKEd */
SYSCTL_VNET_INT(_net_key, KEYCTL_BLOCKACQ_COUNT,
	blockacq_count,	CTLFLAG_RW, &VNET_NAME(key_blockacq_count),	0, "");

/* lifetime for blocking to send SADB_ACQUIRE to IKEd */
SYSCTL_VNET_INT(_net_key, KEYCTL_BLOCKACQ_LIFETIME,
	blockacq_lifetime, CTLFLAG_RW, &VNET_NAME(key_blockacq_lifetime), 0, "");

/* ESP auth */
SYSCTL_VNET_INT(_net_key, KEYCTL_ESP_AUTH,	esp_auth,
	CTLFLAG_RW, &VNET_NAME(ipsec_esp_auth),	0,	"");

/* minimum ESP key length */
SYSCTL_VNET_INT(_net_key, KEYCTL_ESP_KEYMIN,
	esp_keymin, CTLFLAG_RW, &VNET_NAME(ipsec_esp_keymin),	0,	"");

/* minimum AH key length */
SYSCTL_VNET_INT(_net_key, KEYCTL_AH_KEYMIN,	ah_keymin,
	CTLFLAG_RW, &VNET_NAME(ipsec_ah_keymin),	0,	"");

/* perfered old SA rather than new SA */
SYSCTL_VNET_INT(_net_key, KEYCTL_PREFERED_OLDSA,
	preferred_oldsa, CTLFLAG_RW, &VNET_NAME(key_preferred_oldsa),	0, "");

#define __LIST_CHAINED(elm) \
	(!((elm)->chain.le_next == NULL && (elm)->chain.le_prev == NULL))
#define LIST_INSERT_TAIL(head, elm, type, field) \
do {\
	struct type *curelm = LIST_FIRST(head); \
	if (curelm == NULL) {\
		LIST_INSERT_HEAD(head, elm, field); \
	} else { \
		while (LIST_NEXT(curelm, field)) \
			curelm = LIST_NEXT(curelm, field);\
		LIST_INSERT_AFTER(curelm, elm, field);\
	}\
} while (0)

#define KEY_CHKSASTATE(head, sav, name) \
do { \
	if ((head) != (sav)) {						\
		ipseclog((LOG_DEBUG, "%s: state mismatched (TREE=%d SA=%d)\n", \
			(name), (head), (sav)));			\
		continue;						\
	}								\
} while (0)

#define KEY_CHKSPDIR(head, sp, name) \
do { \
	if ((head) != (sp)) {						\
		ipseclog((LOG_DEBUG, "%s: direction mismatched (TREE=%d SP=%d), " \
			"anyway continue.\n",				\
			(name), (head), (sp)));				\
	}								\
} while (0)

MALLOC_DEFINE(M_IPSEC_SA, "secasvar", "ipsec security association");
MALLOC_DEFINE(M_IPSEC_SAH, "sahead", "ipsec sa head");
MALLOC_DEFINE(M_IPSEC_SP, "ipsecpolicy", "ipsec security policy");
MALLOC_DEFINE(M_IPSEC_SR, "ipsecrequest", "ipsec security request");
MALLOC_DEFINE(M_IPSEC_MISC, "ipsec-misc", "ipsec miscellaneous");
MALLOC_DEFINE(M_IPSEC_SAQ, "ipsec-saq", "ipsec sa acquire");
MALLOC_DEFINE(M_IPSEC_SAR, "ipsec-reg", "ipsec sa acquire");

/*
 * set parameters into secpolicyindex buffer.
 * Must allocate secpolicyindex buffer passed to this function.
 */
#define KEY_SETSECSPIDX(_dir, s, d, ps, pd, ulp, idx) \
do { \
	bzero((idx), sizeof(struct secpolicyindex));                         \
	(idx)->dir = (_dir);                                                 \
	(idx)->prefs = (ps);                                                 \
	(idx)->prefd = (pd);                                                 \
	(idx)->ul_proto = (ulp);                                             \
	bcopy((s), &(idx)->src, ((const struct sockaddr *)(s))->sa_len);     \
	bcopy((d), &(idx)->dst, ((const struct sockaddr *)(d))->sa_len);     \
} while (0)

/*
 * set parameters into secasindex buffer.
 * Must allocate secasindex buffer before calling this function.
 */
#define KEY_SETSECASIDX(p, m, r, s, d, idx) \
do { \
	bzero((idx), sizeof(struct secasindex));                             \
	(idx)->proto = (p);                                                  \
	(idx)->mode = (m);                                                   \
	(idx)->reqid = (r);                                                  \
	bcopy((s), &(idx)->src, ((const struct sockaddr *)(s))->sa_len);     \
	bcopy((d), &(idx)->dst, ((const struct sockaddr *)(d))->sa_len);     \
} while (0)

/* key statistics */
struct _keystat {
	u_long getspi_count; /* the avarage of count to try to get new SPI */
} keystat;

struct sadb_msghdr {
	struct sadb_msg *msg;
	struct sadb_ext *ext[SADB_EXT_MAX + 1];
	int extoff[SADB_EXT_MAX + 1];
	int extlen[SADB_EXT_MAX + 1];
};

static struct secasvar *key_allocsa_policy __P((const struct secasindex *));
static void key_freesp_so __P((struct secpolicy **));
static struct secasvar *key_do_allocsa_policy __P((struct secashead *, u_int));
static void key_delsp __P((struct secpolicy *));
static struct secpolicy *key_getsp __P((struct secpolicyindex *));
static void _key_delsp(struct secpolicy *sp);
static struct secpolicy *key_getspbyid __P((u_int32_t));
static u_int32_t key_newreqid __P((void));
static struct mbuf *key_gather_mbuf __P((struct mbuf *,
	const struct sadb_msghdr *, int, int, ...));
static int key_spdadd __P((struct socket *, struct mbuf *,
	const struct sadb_msghdr *));
static u_int32_t key_getnewspid __P((void));
static int key_spddelete __P((struct socket *, struct mbuf *,
	const struct sadb_msghdr *));
static int key_spddelete2 __P((struct socket *, struct mbuf *,
	const struct sadb_msghdr *));
static int key_spdget __P((struct socket *, struct mbuf *,
	const struct sadb_msghdr *));
static int key_spdflush __P((struct socket *, struct mbuf *,
	const struct sadb_msghdr *));
static int key_spddump __P((struct socket *, struct mbuf *,
	const struct sadb_msghdr *));
static struct mbuf *key_setdumpsp __P((struct secpolicy *,
	u_int8_t, u_int32_t, u_int32_t));
static u_int key_getspreqmsglen __P((struct secpolicy *));
static int key_spdexpire __P((struct secpolicy *));
static struct secashead *key_newsah __P((struct secasindex *));
static void key_delsah __P((struct secashead *));
static struct secasvar *key_newsav __P((struct mbuf *,
	const struct sadb_msghdr *, struct secashead *, int *,
	const char*, int));
#define	KEY_NEWSAV(m, sadb, sah, e)				\
	key_newsav(m, sadb, sah, e, __FILE__, __LINE__)
static void key_delsav __P((struct secasvar *));
static struct secashead *key_getsah __P((struct secasindex *));
static struct secasvar *key_checkspidup __P((struct secasindex *, u_int32_t));
static struct secasvar *key_getsavbyspi __P((struct secashead *, u_int32_t));
static int key_setsaval __P((struct secasvar *, struct mbuf *,
	const struct sadb_msghdr *));
static int key_mature __P((struct secasvar *));
static struct mbuf *key_setdumpsa __P((struct secasvar *, u_int8_t,
	u_int8_t, u_int32_t, u_int32_t));
static struct mbuf *key_setsadbmsg __P((u_int8_t, u_int16_t, u_int8_t,
	u_int32_t, pid_t, u_int16_t));
static struct mbuf *key_setsadbsa __P((struct secasvar *));
static struct mbuf *key_setsadbaddr __P((u_int16_t,
	const struct sockaddr *, u_int8_t, u_int16_t));
#ifdef IPSEC_NAT_T
static struct mbuf *key_setsadbxport(u_int16_t, u_int16_t);
static struct mbuf *key_setsadbxtype(u_int16_t);
#endif
static void key_porttosaddr(struct sockaddr *, u_int16_t);
#define	KEY_PORTTOSADDR(saddr, port)				\
	key_porttosaddr((struct sockaddr *)(saddr), (port))
static struct mbuf *key_setsadbxsa2 __P((u_int8_t, u_int32_t, u_int32_t));
static struct mbuf *key_setsadbxpolicy __P((u_int16_t, u_int8_t,
	u_int32_t));
static struct seckey *key_dup_keymsg(const struct sadb_key *, u_int, 
				     struct malloc_type *);
static struct seclifetime *key_dup_lifemsg(const struct sadb_lifetime *src,
					    struct malloc_type *type);
#ifdef INET6
static int key_ismyaddr6 __P((struct sockaddr_in6 *));
#endif

/* flags for key_cmpsaidx() */
#define CMP_HEAD	1	/* protocol, addresses. */
#define CMP_MODE_REQID	2	/* additionally HEAD, reqid, mode. */
#define CMP_REQID	3	/* additionally HEAD, reaid. */
#define CMP_EXACTLY	4	/* all elements. */
static int key_cmpsaidx
	__P((const struct secasindex *, const struct secasindex *, int));

static int key_cmpspidx_exactly
	__P((struct secpolicyindex *, struct secpolicyindex *));
static int key_cmpspidx_withmask
	__P((struct secpolicyindex *, struct secpolicyindex *));
static int key_sockaddrcmp __P((const struct sockaddr *, const struct sockaddr *, int));
static int key_bbcmp __P((const void *, const void *, u_int));
static u_int16_t key_satype2proto __P((u_int8_t));
static u_int8_t key_proto2satype __P((u_int16_t));

static int key_getspi __P((struct socket *, struct mbuf *,
	const struct sadb_msghdr *));
static u_int32_t key_do_getnewspi __P((struct sadb_spirange *,
					struct secasindex *));
static int key_update __P((struct socket *, struct mbuf *,
	const struct sadb_msghdr *));
#ifdef IPSEC_DOSEQCHECK
static struct secasvar *key_getsavbyseq __P((struct secashead *, u_int32_t));
#endif
static int key_add __P((struct socket *, struct mbuf *,
	const struct sadb_msghdr *));
static int key_setident __P((struct secashead *, struct mbuf *,
	const struct sadb_msghdr *));
static struct mbuf *key_getmsgbuf_x1 __P((struct mbuf *,
	const struct sadb_msghdr *));
static int key_delete __P((struct socket *, struct mbuf *,
	const struct sadb_msghdr *));
static int key_get __P((struct socket *, struct mbuf *,
	const struct sadb_msghdr *));

static void key_getcomb_setlifetime __P((struct sadb_comb *));
static struct mbuf *key_getcomb_esp __P((void));
static struct mbuf *key_getcomb_ah __P((void));
static struct mbuf *key_getcomb_ipcomp __P((void));
static struct mbuf *key_getprop __P((const struct secasindex *));

static int key_acquire __P((const struct secasindex *, struct secpolicy *));
static struct secacq *key_newacq __P((const struct secasindex *));
static struct secacq *key_getacq __P((const struct secasindex *));
static struct secacq *key_getacqbyseq __P((u_int32_t));
static struct secspacq *key_newspacq __P((struct secpolicyindex *));
static struct secspacq *key_getspacq __P((struct secpolicyindex *));
static int key_acquire2 __P((struct socket *, struct mbuf *,
	const struct sadb_msghdr *));
static int key_register __P((struct socket *, struct mbuf *,
	const struct sadb_msghdr *));
static int key_expire __P((struct secasvar *));
static int key_flush __P((struct socket *, struct mbuf *,
	const struct sadb_msghdr *));
static int key_dump __P((struct socket *, struct mbuf *,
	const struct sadb_msghdr *));
static int key_promisc __P((struct socket *, struct mbuf *,
	const struct sadb_msghdr *));
static int key_senderror __P((struct socket *, struct mbuf *, int));
static int key_validate_ext __P((const struct sadb_ext *, int));
static int key_align __P((struct mbuf *, struct sadb_msghdr *));
static struct mbuf *key_setlifetime(struct seclifetime *src, 
				     u_int16_t exttype);
static struct mbuf *key_setkey(struct seckey *src, u_int16_t exttype);

#if 0
static const char *key_getfqdn __P((void));
static const char *key_getuserfqdn __P((void));
#endif
static void key_sa_chgstate __P((struct secasvar *, u_int8_t));

static __inline void
sa_initref(struct secasvar *sav)
{

	refcount_init(&sav->refcnt, 1);
}
static __inline void
sa_addref(struct secasvar *sav)
{

	refcount_acquire(&sav->refcnt);
	IPSEC_ASSERT(sav->refcnt != 0, ("SA refcnt overflow"));
}
static __inline int
sa_delref(struct secasvar *sav)
{

	IPSEC_ASSERT(sav->refcnt > 0, ("SA refcnt underflow"));
	return (refcount_release(&sav->refcnt));
}

#define	SP_ADDREF(p) do {						\
	(p)->refcnt++;							\
	IPSEC_ASSERT((p)->refcnt != 0, ("SP refcnt overflow"));		\
} while (0)
#define	SP_DELREF(p) do {						\
	IPSEC_ASSERT((p)->refcnt > 0, ("SP refcnt underflow"));		\
	(p)->refcnt--;							\
} while (0)
 

/*
 * Update the refcnt while holding the SPTREE lock.
 */
void
key_addref(struct secpolicy *sp)
{
	SPTREE_LOCK();
	SP_ADDREF(sp);
	SPTREE_UNLOCK();
}

/*
 * Return 0 when there are known to be no SP's for the specified
 * direction.  Otherwise return 1.  This is used by IPsec code
 * to optimize performance.
 */
int
key_havesp(u_int dir)
{

	return (dir == IPSEC_DIR_INBOUND || dir == IPSEC_DIR_OUTBOUND ?
		LIST_FIRST(&V_sptree[dir]) != NULL : 1);
}

/* %%% IPsec policy management */
/*
 * allocating a SP for OUTBOUND or INBOUND packet.
 * Must call key_freesp() later.
 * OUT:	NULL:	not found
 *	others:	found and return the pointer.
 */
struct secpolicy *
key_allocsp(struct secpolicyindex *spidx, u_int dir, const char* where, int tag)
{
	struct secpolicy *sp;

	IPSEC_ASSERT(spidx != NULL, ("null spidx"));
	IPSEC_ASSERT(dir == IPSEC_DIR_INBOUND || dir == IPSEC_DIR_OUTBOUND,
		("invalid direction %u", dir));

	KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
		printf("DP %s from %s:%u\n", __func__, where, tag));

	/* get a SP entry */
	KEYDEBUG(KEYDEBUG_IPSEC_DATA,
		printf("*** objects\n");
		kdebug_secpolicyindex(spidx));

	SPTREE_LOCK();
	LIST_FOREACH(sp, &V_sptree[dir], chain) {
		KEYDEBUG(KEYDEBUG_IPSEC_DATA,
			printf("*** in SPD\n");
			kdebug_secpolicyindex(&sp->spidx));

		if (sp->state == IPSEC_SPSTATE_DEAD)
			continue;
		if (key_cmpspidx_withmask(&sp->spidx, spidx))
			goto found;
	}
	sp = NULL;
found:
	if (sp) {
		/* sanity check */
		KEY_CHKSPDIR(sp->spidx.dir, dir, __func__);

		/* found a SPD entry */
		sp->lastused = time_second;
		SP_ADDREF(sp);
	}
	SPTREE_UNLOCK();

	KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
		printf("DP %s return SP:%p (ID=%u) refcnt %u\n", __func__,
			sp, sp ? sp->id : 0, sp ? sp->refcnt : 0));
	return sp;
}

/*
 * allocating a SP for OUTBOUND or INBOUND packet.
 * Must call key_freesp() later.
 * OUT:	NULL:	not found
 *	others:	found and return the pointer.
 */
struct secpolicy *
key_allocsp2(u_int32_t spi,
	     union sockaddr_union *dst,
	     u_int8_t proto,
	     u_int dir,
	     const char* where, int tag)
{
	struct secpolicy *sp;

	IPSEC_ASSERT(dst != NULL, ("null dst"));
	IPSEC_ASSERT(dir == IPSEC_DIR_INBOUND || dir == IPSEC_DIR_OUTBOUND,
		("invalid direction %u", dir));

	KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
		printf("DP %s from %s:%u\n", __func__, where, tag));

	/* get a SP entry */
	KEYDEBUG(KEYDEBUG_IPSEC_DATA,
		printf("*** objects\n");
		printf("spi %u proto %u dir %u\n", spi, proto, dir);
		kdebug_sockaddr(&dst->sa));

	SPTREE_LOCK();
	LIST_FOREACH(sp, &V_sptree[dir], chain) {
		KEYDEBUG(KEYDEBUG_IPSEC_DATA,
			printf("*** in SPD\n");
			kdebug_secpolicyindex(&sp->spidx));

		if (sp->state == IPSEC_SPSTATE_DEAD)
			continue;
		/* compare simple values, then dst address */
		if (sp->spidx.ul_proto != proto)
			continue;
		/* NB: spi's must exist and match */
		if (!sp->req || !sp->req->sav || sp->req->sav->spi != spi)
			continue;
		if (key_sockaddrcmp(&sp->spidx.dst.sa, &dst->sa, 1) == 0)
			goto found;
	}
	sp = NULL;
found:
	if (sp) {
		/* sanity check */
		KEY_CHKSPDIR(sp->spidx.dir, dir, __func__);

		/* found a SPD entry */
		sp->lastused = time_second;
		SP_ADDREF(sp);
	}
	SPTREE_UNLOCK();

	KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
		printf("DP %s return SP:%p (ID=%u) refcnt %u\n", __func__,
			sp, sp ? sp->id : 0, sp ? sp->refcnt : 0));
	return sp;
}

#if 0
/*
 * return a policy that matches this particular inbound packet.
 * XXX slow
 */
struct secpolicy *
key_gettunnel(const struct sockaddr *osrc,
	      const struct sockaddr *odst,
	      const struct sockaddr *isrc,
	      const struct sockaddr *idst,
	      const char* where, int tag)
{
	struct secpolicy *sp;
	const int dir = IPSEC_DIR_INBOUND;
	struct ipsecrequest *r1, *r2, *p;
	struct secpolicyindex spidx;

	KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
		printf("DP %s from %s:%u\n", __func__, where, tag));

	if (isrc->sa_family != idst->sa_family) {
		ipseclog((LOG_ERR, "%s: protocol family mismatched %d != %d\n.",
			__func__, isrc->sa_family, idst->sa_family));
		sp = NULL;
		goto done;
	}

	SPTREE_LOCK();
	LIST_FOREACH(sp, &V_sptree[dir], chain) {
		if (sp->state == IPSEC_SPSTATE_DEAD)
			continue;

		r1 = r2 = NULL;
		for (p = sp->req; p; p = p->next) {
			if (p->saidx.mode != IPSEC_MODE_TUNNEL)
				continue;

			r1 = r2;
			r2 = p;

			if (!r1) {
				/* here we look at address matches only */
				spidx = sp->spidx;
				if (isrc->sa_len > sizeof(spidx.src) ||
				    idst->sa_len > sizeof(spidx.dst))
					continue;
				bcopy(isrc, &spidx.src, isrc->sa_len);
				bcopy(idst, &spidx.dst, idst->sa_len);
				if (!key_cmpspidx_withmask(&sp->spidx, &spidx))
					continue;
			} else {
				if (key_sockaddrcmp(&r1->saidx.src.sa, isrc, 0) ||
				    key_sockaddrcmp(&r1->saidx.dst.sa, idst, 0))
					continue;
			}

			if (key_sockaddrcmp(&r2->saidx.src.sa, osrc, 0) ||
			    key_sockaddrcmp(&r2->saidx.dst.sa, odst, 0))
				continue;

			goto found;
		}
	}
	sp = NULL;
found:
	if (sp) {
		sp->lastused = time_second;
		SP_ADDREF(sp);
	}
	SPTREE_UNLOCK();
done:
	KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
		printf("DP %s return SP:%p (ID=%u) refcnt %u\n", __func__,
			sp, sp ? sp->id : 0, sp ? sp->refcnt : 0));
	return sp;
}
#endif

/*
 * allocating an SA entry for an *OUTBOUND* packet.
 * checking each request entries in SP, and acquire an SA if need.
 * OUT:	0: there are valid requests.
 *	ENOENT: policy may be valid, but SA with REQUIRE is on acquiring.
 */
int
key_checkrequest(struct ipsecrequest *isr, const struct secasindex *saidx)
{
	u_int level;
	int error;
	struct secasvar *sav;

	IPSEC_ASSERT(isr != NULL, ("null isr"));
	IPSEC_ASSERT(saidx != NULL, ("null saidx"));
	IPSEC_ASSERT(saidx->mode == IPSEC_MODE_TRANSPORT ||
		saidx->mode == IPSEC_MODE_TUNNEL,
		("unexpected policy %u", saidx->mode));

	/*
	 * XXX guard against protocol callbacks from the crypto
	 * thread as they reference ipsecrequest.sav which we
	 * temporarily null out below.  Need to rethink how we
	 * handle bundled SA's in the callback thread.
	 */
	IPSECREQUEST_LOCK_ASSERT(isr);

	/* get current level */
	level = ipsec_get_reqlevel(isr);

	/*
	 * We check new SA in the IPsec request because a different
	 * SA may be involved each time this request is checked, either
	 * because new SAs are being configured, or this request is
	 * associated with an unconnected datagram socket, or this request
	 * is associated with a system default policy.
	 *
	 * key_allocsa_policy should allocate the oldest SA available.
	 * See key_do_allocsa_policy(), and draft-jenkins-ipsec-rekeying-03.txt.
	 */
	sav = key_allocsa_policy(saidx);
	if (sav != isr->sav) {
		/* SA need to be updated. */
		if (!IPSECREQUEST_UPGRADE(isr)) {
			/* Kick everyone off. */
			IPSECREQUEST_UNLOCK(isr);
			IPSECREQUEST_WLOCK(isr);
		}
		if (isr->sav != NULL)
			KEY_FREESAV(&isr->sav);
		isr->sav = sav;
		IPSECREQUEST_DOWNGRADE(isr);
	} else if (sav != NULL)
		KEY_FREESAV(&sav);

	/* When there is SA. */
	if (isr->sav != NULL) {
		if (isr->sav->state != SADB_SASTATE_MATURE &&
		    isr->sav->state != SADB_SASTATE_DYING)
			return EINVAL;
		return 0;
	}

	/* there is no SA */
	error = key_acquire(saidx, isr->sp);
	if (error != 0) {
		/* XXX What should I do ? */
		ipseclog((LOG_DEBUG, "%s: error %d returned from key_acquire\n",
			__func__, error));
		return error;
	}

	if (level != IPSEC_LEVEL_REQUIRE) {
		/* XXX sigh, the interface to this routine is botched */
		IPSEC_ASSERT(isr->sav == NULL, ("unexpected SA"));
		return 0;
	} else {
		return ENOENT;
	}
}

/*
 * allocating a SA for policy entry from SAD.
 * NOTE: searching SAD of aliving state.
 * OUT:	NULL:	not found.
 *	others:	found and return the pointer.
 */
static struct secasvar *
key_allocsa_policy(const struct secasindex *saidx)
{
#define	N(a)	_ARRAYLEN(a)
	struct secashead *sah;
	struct secasvar *sav;
	u_int stateidx, arraysize;
	const u_int *state_valid;

	state_valid = NULL;	/* silence gcc */
	arraysize = 0;		/* silence gcc */

	SAHTREE_LOCK();
	LIST_FOREACH(sah, &V_sahtree, chain) {
		if (sah->state == SADB_SASTATE_DEAD)
			continue;
		if (key_cmpsaidx(&sah->saidx, saidx, CMP_MODE_REQID)) {
			if (V_key_preferred_oldsa) {
				state_valid = saorder_state_valid_prefer_old;
				arraysize = N(saorder_state_valid_prefer_old);
			} else {
				state_valid = saorder_state_valid_prefer_new;
				arraysize = N(saorder_state_valid_prefer_new);
			}
			break;
		}
	}
	SAHTREE_UNLOCK();
	if (sah == NULL)
		return NULL;

	/* search valid state */
	for (stateidx = 0; stateidx < arraysize; stateidx++) {
		sav = key_do_allocsa_policy(sah, state_valid[stateidx]);
		if (sav != NULL)
			return sav;
	}

	return NULL;
#undef N
}

/*
 * searching SAD with direction, protocol, mode and state.
 * called by key_allocsa_policy().
 * OUT:
 *	NULL	: not found
 *	others	: found, pointer to a SA.
 */
static struct secasvar *
key_do_allocsa_policy(struct secashead *sah, u_int state)
{
	struct secasvar *sav, *nextsav, *candidate, *d;

	/* initilize */
	candidate = NULL;

	SAHTREE_LOCK();
	for (sav = LIST_FIRST(&sah->savtree[state]);
	     sav != NULL;
	     sav = nextsav) {

		nextsav = LIST_NEXT(sav, chain);

		/* sanity check */
		KEY_CHKSASTATE(sav->state, state, __func__);

		/* initialize */
		if (candidate == NULL) {
			candidate = sav;
			continue;
		}

		/* Which SA is the better ? */

		IPSEC_ASSERT(candidate->lft_c != NULL,
			("null candidate lifetime"));
		IPSEC_ASSERT(sav->lft_c != NULL, ("null sav lifetime"));

		/* What the best method is to compare ? */
		if (V_key_preferred_oldsa) {
			if (candidate->lft_c->addtime >
					sav->lft_c->addtime) {
				candidate = sav;
			}
			continue;
			/*NOTREACHED*/
		}

		/* preferred new sa rather than old sa */
		if (candidate->lft_c->addtime <
				sav->lft_c->addtime) {
			d = candidate;
			candidate = sav;
		} else
			d = sav;

		/*
		 * prepared to delete the SA when there is more
		 * suitable candidate and the lifetime of the SA is not
		 * permanent.
		 */
		if (d->lft_h->addtime != 0) {
			struct mbuf *m, *result;
			u_int8_t satype;

			key_sa_chgstate(d, SADB_SASTATE_DEAD);

			IPSEC_ASSERT(d->refcnt > 0, ("bogus ref count"));

			satype = key_proto2satype(d->sah->saidx.proto);
			if (satype == 0)
				goto msgfail;

			m = key_setsadbmsg(SADB_DELETE, 0,
			    satype, 0, 0, d->refcnt - 1);
			if (!m)
				goto msgfail;
			result = m;

			/* set sadb_address for saidx's. */
			m = key_setsadbaddr(SADB_EXT_ADDRESS_SRC,
				&d->sah->saidx.src.sa,
				d->sah->saidx.src.sa.sa_len << 3,
				IPSEC_ULPROTO_ANY);
			if (!m)
				goto msgfail;
			m_cat(result, m);

			/* set sadb_address for saidx's. */
			m = key_setsadbaddr(SADB_EXT_ADDRESS_DST,
				&d->sah->saidx.dst.sa,
				d->sah->saidx.dst.sa.sa_len << 3,
				IPSEC_ULPROTO_ANY);
			if (!m)
				goto msgfail;
			m_cat(result, m);

			/* create SA extension */
			m = key_setsadbsa(d);
			if (!m)
				goto msgfail;
			m_cat(result, m);

			if (result->m_len < sizeof(struct sadb_msg)) {
				result = m_pullup(result,
						sizeof(struct sadb_msg));
				if (result == NULL)
					goto msgfail;
			}

			result->m_pkthdr.len = 0;
			for (m = result; m; m = m->m_next)
				result->m_pkthdr.len += m->m_len;
			mtod(result, struct sadb_msg *)->sadb_msg_len =
				PFKEY_UNIT64(result->m_pkthdr.len);

			if (key_sendup_mbuf(NULL, result,
					KEY_SENDUP_REGISTERED))
				goto msgfail;
		 msgfail:
			KEY_FREESAV(&d);
		}
	}
	if (candidate) {
		sa_addref(candidate);
		KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
			printf("DP %s cause refcnt++:%d SA:%p\n",
				__func__, candidate->refcnt, candidate));
	}
	SAHTREE_UNLOCK();

	return candidate;
}

/*
 * allocating a usable SA entry for a *INBOUND* packet.
 * Must call key_freesav() later.
 * OUT: positive:	pointer to a usable sav (i.e. MATURE or DYING state).
 *	NULL:		not found, or error occured.
 *
 * In the comparison, no source address is used--for RFC2401 conformance.
 * To quote, from section 4.1:
 *	A security association is uniquely identified by a triple consisting
 *	of a Security Parameter Index (SPI), an IP Destination Address, and a
 *	security protocol (AH or ESP) identifier.
 * Note that, however, we do need to keep source address in IPsec SA.
 * IKE specification and PF_KEY specification do assume that we
 * keep source address in IPsec SA.  We see a tricky situation here.
 */
struct secasvar *
key_allocsa(
	union sockaddr_union *dst,
	u_int proto,
	u_int32_t spi,
	const char* where, int tag)
{
	struct secashead *sah;
	struct secasvar *sav;
	u_int stateidx, arraysize, state;
	const u_int *saorder_state_valid;
	int chkport;

	IPSEC_ASSERT(dst != NULL, ("null dst address"));

	KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
		printf("DP %s from %s:%u\n", __func__, where, tag));

#ifdef IPSEC_NAT_T
        chkport = (dst->sa.sa_family == AF_INET &&
	    dst->sa.sa_len == sizeof(struct sockaddr_in) &&
	    dst->sin.sin_port != 0);
#else
	chkport = 0;
#endif

	/*
	 * searching SAD.
	 * XXX: to be checked internal IP header somewhere.  Also when
	 * IPsec tunnel packet is received.  But ESP tunnel mode is
	 * encrypted so we can't check internal IP header.
	 */
	SAHTREE_LOCK();
	if (V_key_preferred_oldsa) {
		saorder_state_valid = saorder_state_valid_prefer_old;
		arraysize = _ARRAYLEN(saorder_state_valid_prefer_old);
	} else {
		saorder_state_valid = saorder_state_valid_prefer_new;
		arraysize = _ARRAYLEN(saorder_state_valid_prefer_new);
	}
	LIST_FOREACH(sah, &V_sahtree, chain) {
		/* search valid state */
		for (stateidx = 0; stateidx < arraysize; stateidx++) {
			state = saorder_state_valid[stateidx];
			LIST_FOREACH(sav, &sah->savtree[state], chain) {
				/* sanity check */
				KEY_CHKSASTATE(sav->state, state, __func__);
				/* do not return entries w/ unusable state */
				if (sav->state != SADB_SASTATE_MATURE &&
				    sav->state != SADB_SASTATE_DYING)
					continue;
				if (proto != sav->sah->saidx.proto)
					continue;
				if (spi != sav->spi)
					continue;
#if 0	/* don't check src */
				/* check src address */
				if (key_sockaddrcmp(&src->sa, &sav->sah->saidx.src.sa, chkport) != 0)
					continue;
#endif
				/* check dst address */
				if (key_sockaddrcmp(&dst->sa, &sav->sah->saidx.dst.sa, chkport) != 0)
					continue;
				sa_addref(sav);
				goto done;
			}
		}
	}
	sav = NULL;
done:
	SAHTREE_UNLOCK();

	KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
		printf("DP %s return SA:%p; refcnt %u\n", __func__,
			sav, sav ? sav->refcnt : 0));
	return sav;
}

/*
 * Must be called after calling key_allocsp().
 * For both the packet without socket and key_freeso().
 */
void
_key_freesp(struct secpolicy **spp, const char* where, int tag)
{
	struct secpolicy *sp = *spp;

	IPSEC_ASSERT(sp != NULL, ("null sp"));

	SPTREE_LOCK();
	SP_DELREF(sp);

	KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
		printf("DP %s SP:%p (ID=%u) from %s:%u; refcnt now %u\n",
			__func__, sp, sp->id, where, tag, sp->refcnt));

	if (sp->refcnt == 0) {
		*spp = NULL;
		key_delsp(sp);
	}
	SPTREE_UNLOCK();
}

/*
 * Must be called after calling key_allocsp().
 * For the packet with socket.
 */
void
key_freeso(struct socket *so)
{
	IPSEC_ASSERT(so != NULL, ("null so"));

	switch (so->so_proto->pr_domain->dom_family) {
#if defined(INET) || defined(INET6)
#ifdef INET
	case PF_INET:
#endif
#ifdef INET6
	case PF_INET6:
#endif
	    {
		struct inpcb *pcb = sotoinpcb(so);

		/* Does it have a PCB ? */
		if (pcb == NULL)
			return;
		key_freesp_so(&pcb->inp_sp->sp_in);
		key_freesp_so(&pcb->inp_sp->sp_out);
	    }
		break;
#endif /* INET || INET6 */
	default:
		ipseclog((LOG_DEBUG, "%s: unknown address family=%d.\n",
		    __func__, so->so_proto->pr_domain->dom_family));
		return;
	}
}

static void
key_freesp_so(struct secpolicy **sp)
{
	IPSEC_ASSERT(sp != NULL && *sp != NULL, ("null sp"));

	if ((*sp)->policy == IPSEC_POLICY_ENTRUST ||
	    (*sp)->policy == IPSEC_POLICY_BYPASS)
		return;

	IPSEC_ASSERT((*sp)->policy == IPSEC_POLICY_IPSEC,
		("invalid policy %u", (*sp)->policy));
	KEY_FREESP(sp);
}

void
key_addrefsa(struct secasvar *sav, const char* where, int tag)
{

	IPSEC_ASSERT(sav != NULL, ("null sav"));
	IPSEC_ASSERT(sav->refcnt > 0, ("refcount must exist"));

	sa_addref(sav);
}

/*
 * Must be called after calling key_allocsa().
 * This function is called by key_freesp() to free some SA allocated
 * for a policy.
 */
void
key_freesav(struct secasvar **psav, const char* where, int tag)
{
	struct secasvar *sav = *psav;

	IPSEC_ASSERT(sav != NULL, ("null sav"));

	if (sa_delref(sav)) {
		KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
			printf("DP %s SA:%p (SPI %u) from %s:%u; refcnt now %u\n",
				__func__, sav, ntohl(sav->spi), where, tag, sav->refcnt));
		*psav = NULL;
		key_delsav(sav);
	} else {
		KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
			printf("DP %s SA:%p (SPI %u) from %s:%u; refcnt now %u\n",
				__func__, sav, ntohl(sav->spi), where, tag, sav->refcnt));
	}
}

/* %%% SPD management */
/*
 * free security policy entry.
 */
static void
key_delsp(struct secpolicy *sp)
{
	struct ipsecrequest *isr, *nextisr;

	IPSEC_ASSERT(sp != NULL, ("null sp"));
	SPTREE_LOCK_ASSERT();

	sp->state = IPSEC_SPSTATE_DEAD;

	IPSEC_ASSERT(sp->refcnt == 0,
		("SP with references deleted (refcnt %u)", sp->refcnt));

	/* remove from SP index */
	if (__LIST_CHAINED(sp))
		LIST_REMOVE(sp, chain);

	for (isr = sp->req; isr != NULL; isr = nextisr) {
		if (isr->sav != NULL) {
			KEY_FREESAV(&isr->sav);
			isr->sav = NULL;
		}

		nextisr = isr->next;
		ipsec_delisr(isr);
	}
	_key_delsp(sp);
}

/*
 * search SPD
 * OUT:	NULL	: not found
 *	others	: found, pointer to a SP.
 */
static struct secpolicy *
key_getsp(struct secpolicyindex *spidx)
{
	struct secpolicy *sp;

	IPSEC_ASSERT(spidx != NULL, ("null spidx"));

	SPTREE_LOCK();
	LIST_FOREACH(sp, &V_sptree[spidx->dir], chain) {
		if (sp->state == IPSEC_SPSTATE_DEAD)
			continue;
		if (key_cmpspidx_exactly(spidx, &sp->spidx)) {
			SP_ADDREF(sp);
			break;
		}
	}
	SPTREE_UNLOCK();

	return sp;
}

/*
 * get SP by index.
 * OUT:	NULL	: not found
 *	others	: found, pointer to a SP.
 */
static struct secpolicy *
key_getspbyid(u_int32_t id)
{
	struct secpolicy *sp;

	SPTREE_LOCK();
	LIST_FOREACH(sp, &V_sptree[IPSEC_DIR_INBOUND], chain) {
		if (sp->state == IPSEC_SPSTATE_DEAD)
			continue;
		if (sp->id == id) {
			SP_ADDREF(sp);
			goto done;
		}
	}

	LIST_FOREACH(sp, &V_sptree[IPSEC_DIR_OUTBOUND], chain) {
		if (sp->state == IPSEC_SPSTATE_DEAD)
			continue;
		if (sp->id == id) {
			SP_ADDREF(sp);
			goto done;
		}
	}
done:
	SPTREE_UNLOCK();

	return sp;
}

struct secpolicy *
key_newsp(const char* where, int tag)
{
	struct secpolicy *newsp = NULL;

	newsp = (struct secpolicy *)
		malloc(sizeof(struct secpolicy), M_IPSEC_SP, M_NOWAIT|M_ZERO);
	if (newsp) {
		SECPOLICY_LOCK_INIT(newsp);
		newsp->refcnt = 1;
		newsp->req = NULL;
	}

	KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
		printf("DP %s from %s:%u return SP:%p\n", __func__,
			where, tag, newsp));
	return newsp;
}

static void
_key_delsp(struct secpolicy *sp)
{
	SECPOLICY_LOCK_DESTROY(sp);
	free(sp, M_IPSEC_SP);
}

/*
 * create secpolicy structure from sadb_x_policy structure.
 * NOTE: `state', `secpolicyindex' in secpolicy structure are not set,
 * so must be set properly later.
 */
struct secpolicy *
key_msg2sp(xpl0, len, error)
	struct sadb_x_policy *xpl0;
	size_t len;
	int *error;
{
	struct secpolicy *newsp;

	IPSEC_ASSERT(xpl0 != NULL, ("null xpl0"));
	IPSEC_ASSERT(len >= sizeof(*xpl0), ("policy too short: %zu", len));

	if (len != PFKEY_EXTLEN(xpl0)) {
		ipseclog((LOG_DEBUG, "%s: Invalid msg length.\n", __func__));
		*error = EINVAL;
		return NULL;
	}

	if ((newsp = KEY_NEWSP()) == NULL) {
		*error = ENOBUFS;
		return NULL;
	}

	newsp->spidx.dir = xpl0->sadb_x_policy_dir;
	newsp->policy = xpl0->sadb_x_policy_type;

	/* check policy */
	switch (xpl0->sadb_x_policy_type) {
	case IPSEC_POLICY_DISCARD:
	case IPSEC_POLICY_NONE:
	case IPSEC_POLICY_ENTRUST:
	case IPSEC_POLICY_BYPASS:
		newsp->req = NULL;
		break;

	case IPSEC_POLICY_IPSEC:
	    {
		int tlen;
		struct sadb_x_ipsecrequest *xisr;
		struct ipsecrequest **p_isr = &newsp->req;

		/* validity check */
		if (PFKEY_EXTLEN(xpl0) < sizeof(*xpl0)) {
			ipseclog((LOG_DEBUG, "%s: Invalid msg length.\n",
				__func__));
			KEY_FREESP(&newsp);
			*error = EINVAL;
			return NULL;
		}

		tlen = PFKEY_EXTLEN(xpl0) - sizeof(*xpl0);
		xisr = (struct sadb_x_ipsecrequest *)(xpl0 + 1);

		while (tlen > 0) {
			/* length check */
			if (xisr->sadb_x_ipsecrequest_len < sizeof(*xisr)) {
				ipseclog((LOG_DEBUG, "%s: invalid ipsecrequest "
					"length.\n", __func__));
				KEY_FREESP(&newsp);
				*error = EINVAL;
				return NULL;
			}

			/* allocate request buffer */
			/* NB: data structure is zero'd */
			*p_isr = ipsec_newisr();
			if ((*p_isr) == NULL) {
				ipseclog((LOG_DEBUG,
				    "%s: No more memory.\n", __func__));
				KEY_FREESP(&newsp);
				*error = ENOBUFS;
				return NULL;
			}

			/* set values */
			switch (xisr->sadb_x_ipsecrequest_proto) {
			case IPPROTO_ESP:
			case IPPROTO_AH:
			case IPPROTO_IPCOMP:
				break;
			default:
				ipseclog((LOG_DEBUG,
				    "%s: invalid proto type=%u\n", __func__,
				    xisr->sadb_x_ipsecrequest_proto));
				KEY_FREESP(&newsp);
				*error = EPROTONOSUPPORT;
				return NULL;
			}
			(*p_isr)->saidx.proto = xisr->sadb_x_ipsecrequest_proto;

			switch (xisr->sadb_x_ipsecrequest_mode) {
			case IPSEC_MODE_TRANSPORT:
			case IPSEC_MODE_TUNNEL:
				break;
			case IPSEC_MODE_ANY:
			default:
				ipseclog((LOG_DEBUG,
				    "%s: invalid mode=%u\n", __func__,
				    xisr->sadb_x_ipsecrequest_mode));
				KEY_FREESP(&newsp);
				*error = EINVAL;
				return NULL;
			}
			(*p_isr)->saidx.mode = xisr->sadb_x_ipsecrequest_mode;

			switch (xisr->sadb_x_ipsecrequest_level) {
			case IPSEC_LEVEL_DEFAULT:
			case IPSEC_LEVEL_USE:
			case IPSEC_LEVEL_REQUIRE:
				break;
			case IPSEC_LEVEL_UNIQUE:
				/* validity check */
				/*
				 * If range violation of reqid, kernel will
				 * update it, don't refuse it.
				 */
				if (xisr->sadb_x_ipsecrequest_reqid
						> IPSEC_MANUAL_REQID_MAX) {
					ipseclog((LOG_DEBUG,
					    "%s: reqid=%d range "
					    "violation, updated by kernel.\n",
					    __func__,
					    xisr->sadb_x_ipsecrequest_reqid));
					xisr->sadb_x_ipsecrequest_reqid = 0;
				}

				/* allocate new reqid id if reqid is zero. */
				if (xisr->sadb_x_ipsecrequest_reqid == 0) {
					u_int32_t reqid;
					if ((reqid = key_newreqid()) == 0) {
						KEY_FREESP(&newsp);
						*error = ENOBUFS;
						return NULL;
					}
					(*p_isr)->saidx.reqid = reqid;
					xisr->sadb_x_ipsecrequest_reqid = reqid;
				} else {
				/* set it for manual keying. */
					(*p_isr)->saidx.reqid =
						xisr->sadb_x_ipsecrequest_reqid;
				}
				break;

			default:
				ipseclog((LOG_DEBUG, "%s: invalid level=%u\n",
					__func__,
					xisr->sadb_x_ipsecrequest_level));
				KEY_FREESP(&newsp);
				*error = EINVAL;
				return NULL;
			}
			(*p_isr)->level = xisr->sadb_x_ipsecrequest_level;

			/* set IP addresses if there */
			if (xisr->sadb_x_ipsecrequest_len > sizeof(*xisr)) {
				struct sockaddr *paddr;

				paddr = (struct sockaddr *)(xisr + 1);

				/* validity check */
				if (paddr->sa_len
				    > sizeof((*p_isr)->saidx.src)) {
					ipseclog((LOG_DEBUG, "%s: invalid "
						"request address length.\n",
						__func__));
					KEY_FREESP(&newsp);
					*error = EINVAL;
					return NULL;
				}
				bcopy(paddr, &(*p_isr)->saidx.src,
					paddr->sa_len);

				paddr = (struct sockaddr *)((caddr_t)paddr
							+ paddr->sa_len);

				/* validity check */
				if (paddr->sa_len
				    > sizeof((*p_isr)->saidx.dst)) {
					ipseclog((LOG_DEBUG, "%s: invalid "
						"request address length.\n",
						__func__));
					KEY_FREESP(&newsp);
					*error = EINVAL;
					return NULL;
				}
				bcopy(paddr, &(*p_isr)->saidx.dst,
					paddr->sa_len);
			}

			(*p_isr)->sp = newsp;

			/* initialization for the next. */
			p_isr = &(*p_isr)->next;
			tlen -= xisr->sadb_x_ipsecrequest_len;

			/* validity check */
			if (tlen < 0) {
				ipseclog((LOG_DEBUG, "%s: becoming tlen < 0.\n",
					__func__));
				KEY_FREESP(&newsp);
				*error = EINVAL;
				return NULL;
			}

			xisr = (struct sadb_x_ipsecrequest *)((caddr_t)xisr
			                 + xisr->sadb_x_ipsecrequest_len);
		}
	    }
		break;
	default:
		ipseclog((LOG_DEBUG, "%s: invalid policy type.\n", __func__));
		KEY_FREESP(&newsp);
		*error = EINVAL;
		return NULL;
	}

	*error = 0;
	return newsp;
}

static u_int32_t
key_newreqid()
{
	static u_int32_t auto_reqid = IPSEC_MANUAL_REQID_MAX + 1;

	auto_reqid = (auto_reqid == ~0
			? IPSEC_MANUAL_REQID_MAX + 1 : auto_reqid + 1);

	/* XXX should be unique check */

	return auto_reqid;
}

/*
 * copy secpolicy struct to sadb_x_policy structure indicated.
 */
struct mbuf *
key_sp2msg(sp)
	struct secpolicy *sp;
{
	struct sadb_x_policy *xpl;
	int tlen;
	caddr_t p;
	struct mbuf *m;

	IPSEC_ASSERT(sp != NULL, ("null policy"));

	tlen = key_getspreqmsglen(sp);

	m = m_get2(tlen, M_NOWAIT, MT_DATA, 0);
	if (m == NULL)
		return (NULL);
	m_align(m, tlen);
	m->m_len = tlen;
	xpl = mtod(m, struct sadb_x_policy *);
	bzero(xpl, tlen);

	xpl->sadb_x_policy_len = PFKEY_UNIT64(tlen);
	xpl->sadb_x_policy_exttype = SADB_X_EXT_POLICY;
	xpl->sadb_x_policy_type = sp->policy;
	xpl->sadb_x_policy_dir = sp->spidx.dir;
	xpl->sadb_x_policy_id = sp->id;
	p = (caddr_t)xpl + sizeof(*xpl);

	/* if is the policy for ipsec ? */
	if (sp->policy == IPSEC_POLICY_IPSEC) {
		struct sadb_x_ipsecrequest *xisr;
		struct ipsecrequest *isr;

		for (isr = sp->req; isr != NULL; isr = isr->next) {

			xisr = (struct sadb_x_ipsecrequest *)p;

			xisr->sadb_x_ipsecrequest_proto = isr->saidx.proto;
			xisr->sadb_x_ipsecrequest_mode = isr->saidx.mode;
			xisr->sadb_x_ipsecrequest_level = isr->level;
			xisr->sadb_x_ipsecrequest_reqid = isr->saidx.reqid;

			p += sizeof(*xisr);
			bcopy(&isr->saidx.src, p, isr->saidx.src.sa.sa_len);
			p += isr->saidx.src.sa.sa_len;
			bcopy(&isr->saidx.dst, p, isr->saidx.dst.sa.sa_len);
			p += isr->saidx.src.sa.sa_len;

			xisr->sadb_x_ipsecrequest_len =
				PFKEY_ALIGN8(sizeof(*xisr)
					+ isr->saidx.src.sa.sa_len
					+ isr->saidx.dst.sa.sa_len);
		}
	}

	return m;
}

/* m will not be freed nor modified */
static struct mbuf *
#ifdef __STDC__
key_gather_mbuf(struct mbuf *m, const struct sadb_msghdr *mhp,
	int ndeep, int nitem, ...)
#else
key_gather_mbuf(m, mhp, ndeep, nitem, va_alist)
	struct mbuf *m;
	const struct sadb_msghdr *mhp;
	int ndeep;
	int nitem;
	va_dcl
#endif
{
	va_list ap;
	int idx;
	int i;
	struct mbuf *result = NULL, *n;
	int len;

	IPSEC_ASSERT(m != NULL, ("null mbuf"));
	IPSEC_ASSERT(mhp != NULL, ("null msghdr"));

	va_start(ap, nitem);
	for (i = 0; i < nitem; i++) {
		idx = va_arg(ap, int);
		if (idx < 0 || idx > SADB_EXT_MAX)
			goto fail;
		/* don't attempt to pull empty extension */
		if (idx == SADB_EXT_RESERVED && mhp->msg == NULL)
			continue;
		if (idx != SADB_EXT_RESERVED  &&
		    (mhp->ext[idx] == NULL || mhp->extlen[idx] == 0))
			continue;

		if (idx == SADB_EXT_RESERVED) {
			len = PFKEY_ALIGN8(sizeof(struct sadb_msg));

			IPSEC_ASSERT(len <= MHLEN, ("header too big %u", len));

			MGETHDR(n, M_NOWAIT, MT_DATA);
			if (!n)
				goto fail;
			n->m_len = len;
			n->m_next = NULL;
			m_copydata(m, 0, sizeof(struct sadb_msg),
			    mtod(n, caddr_t));
		} else if (i < ndeep) {
			len = mhp->extlen[idx];
			n = m_get2(len, M_NOWAIT, MT_DATA, 0);
			if (n == NULL)
				goto fail;
			m_align(n, len);
			n->m_len = len;
			m_copydata(m, mhp->extoff[idx], mhp->extlen[idx],
			    mtod(n, caddr_t));
		} else {
			n = m_copym(m, mhp->extoff[idx], mhp->extlen[idx],
			    M_NOWAIT);
		}
		if (n == NULL)
			goto fail;

		if (result)
			m_cat(result, n);
		else
			result = n;
	}
	va_end(ap);

	if ((result->m_flags & M_PKTHDR) != 0) {
		result->m_pkthdr.len = 0;
		for (n = result; n; n = n->m_next)
			result->m_pkthdr.len += n->m_len;
	}

	return result;

fail:
	m_freem(result);
	va_end(ap);
	return NULL;
}

/*
 * SADB_X_SPDADD, SADB_X_SPDSETIDX or SADB_X_SPDUPDATE processing
 * add an entry to SP database, when received
 *   <base, address(SD), (lifetime(H),) policy>
 * from the user(?).
 * Adding to SP database,
 * and send
 *   <base, address(SD), (lifetime(H),) policy>
 * to the socket which was send.
 *
 * SPDADD set a unique policy entry.
 * SPDSETIDX like SPDADD without a part of policy requests.
 * SPDUPDATE replace a unique policy entry.
 *
 * m will always be freed.
 */
static int
key_spdadd(so, m, mhp)
	struct socket *so;
	struct mbuf *m;
	const struct sadb_msghdr *mhp;
{
	struct sadb_address *src0, *dst0;
	struct sadb_x_policy *xpl0, *xpl;
	struct sadb_lifetime *lft = NULL;
	struct secpolicyindex spidx;
	struct secpolicy *newsp;
	int error;

	IPSEC_ASSERT(so != NULL, ("null socket"));
	IPSEC_ASSERT(m != NULL, ("null mbuf"));
	IPSEC_ASSERT(mhp != NULL, ("null msghdr"));
	IPSEC_ASSERT(mhp->msg != NULL, ("null msg"));

	if (mhp->ext[SADB_EXT_ADDRESS_SRC] == NULL ||
	    mhp->ext[SADB_EXT_ADDRESS_DST] == NULL ||
	    mhp->ext[SADB_X_EXT_POLICY] == NULL) {
		ipseclog((LOG_DEBUG, "key_spdadd: invalid message is passed.\n"));
		return key_senderror(so, m, EINVAL);
	}
	if (mhp->extlen[SADB_EXT_ADDRESS_SRC] < sizeof(struct sadb_address) ||
	    mhp->extlen[SADB_EXT_ADDRESS_DST] < sizeof(struct sadb_address) ||
	    mhp->extlen[SADB_X_EXT_POLICY] < sizeof(struct sadb_x_policy)) {
		ipseclog((LOG_DEBUG, "%s: invalid message is passed.\n",
			__func__));
		return key_senderror(so, m, EINVAL);
	}
	if (mhp->ext[SADB_EXT_LIFETIME_HARD] != NULL) {
		if (mhp->extlen[SADB_EXT_LIFETIME_HARD]
			< sizeof(struct sadb_lifetime)) {
			ipseclog((LOG_DEBUG, "%s: invalid message is passed.\n",
				__func__));
			return key_senderror(so, m, EINVAL);
		}
		lft = (struct sadb_lifetime *)mhp->ext[SADB_EXT_LIFETIME_HARD];
	}

	src0 = (struct sadb_address *)mhp->ext[SADB_EXT_ADDRESS_SRC];
	dst0 = (struct sadb_address *)mhp->ext[SADB_EXT_ADDRESS_DST];
	xpl0 = (struct sadb_x_policy *)mhp->ext[SADB_X_EXT_POLICY];

	/* 
	 * Note: do not parse SADB_X_EXT_NAT_T_* here:
	 * we are processing traffic endpoints.
	 */

	/* make secindex */
	/* XXX boundary check against sa_len */
	KEY_SETSECSPIDX(xpl0->sadb_x_policy_dir,
	                src0 + 1,
	                dst0 + 1,
	                src0->sadb_address_prefixlen,
	                dst0->sadb_address_prefixlen,
	                src0->sadb_address_proto,
	                &spidx);

	/* checking the direciton. */
	switch (xpl0->sadb_x_policy_dir) {
	case IPSEC_DIR_INBOUND:
	case IPSEC_DIR_OUTBOUND:
		break;
	default:
		ipseclog((LOG_DEBUG, "%s: Invalid SP direction.\n", __func__));
		mhp->msg->sadb_msg_errno = EINVAL;
		return 0;
	}

	/* check policy */
	/* key_spdadd() accepts DISCARD, NONE and IPSEC. */
	if (xpl0->sadb_x_policy_type == IPSEC_POLICY_ENTRUST
	 || xpl0->sadb_x_policy_type == IPSEC_POLICY_BYPASS) {
		ipseclog((LOG_DEBUG, "%s: Invalid policy type.\n", __func__));
		return key_senderror(so, m, EINVAL);
	}

	/* policy requests are mandatory when action is ipsec. */
        if (mhp->msg->sadb_msg_type != SADB_X_SPDSETIDX
	 && xpl0->sadb_x_policy_type == IPSEC_POLICY_IPSEC
	 && mhp->extlen[SADB_X_EXT_POLICY] <= sizeof(*xpl0)) {
		ipseclog((LOG_DEBUG, "%s: some policy requests part required\n",
			__func__));
		return key_senderror(so, m, EINVAL);
	}

	/*
	 * checking there is SP already or not.
	 * SPDUPDATE doesn't depend on whether there is a SP or not.
	 * If the type is either SPDADD or SPDSETIDX AND a SP is found,
	 * then error.
	 */
	newsp = key_getsp(&spidx);
	if (mhp->msg->sadb_msg_type == SADB_X_SPDUPDATE) {
		if (newsp) {
			SPTREE_LOCK();
			newsp->state = IPSEC_SPSTATE_DEAD;
			SPTREE_UNLOCK();
			KEY_FREESP(&newsp);
		}
	} else {
		if (newsp != NULL) {
			KEY_FREESP(&newsp);
			ipseclog((LOG_DEBUG, "%s: a SP entry exists already.\n",
				__func__));
			return key_senderror(so, m, EEXIST);
		}
	}

	/* allocation new SP entry */
	if ((newsp = key_msg2sp(xpl0, PFKEY_EXTLEN(xpl0), &error)) == NULL) {
		return key_senderror(so, m, error);
	}

	if ((newsp->id = key_getnewspid()) == 0) {
		_key_delsp(newsp);
		return key_senderror(so, m, ENOBUFS);
	}

	/* XXX boundary check against sa_len */
	KEY_SETSECSPIDX(xpl0->sadb_x_policy_dir,
	                src0 + 1,
	                dst0 + 1,
	                src0->sadb_address_prefixlen,
	                dst0->sadb_address_prefixlen,
	                src0->sadb_address_proto,
	                &newsp->spidx);

	/* sanity check on addr pair */
	if (((struct sockaddr *)(src0 + 1))->sa_family !=
			((struct sockaddr *)(dst0+ 1))->sa_family) {
		_key_delsp(newsp);
		return key_senderror(so, m, EINVAL);
	}
	if (((struct sockaddr *)(src0 + 1))->sa_len !=
			((struct sockaddr *)(dst0+ 1))->sa_len) {
		_key_delsp(newsp);
		return key_senderror(so, m, EINVAL);
	}
#if 1
	if (newsp->req && newsp->req->saidx.src.sa.sa_family && newsp->req->saidx.dst.sa.sa_family) {
		if (newsp->req->saidx.src.sa.sa_family != newsp->req->saidx.dst.sa.sa_family) {
			_key_delsp(newsp);
			return key_senderror(so, m, EINVAL);
		}
	}
#endif

	newsp->created = time_second;
	newsp->lastused = newsp->created;
	newsp->lifetime = lft ? lft->sadb_lifetime_addtime : 0;
	newsp->validtime = lft ? lft->sadb_lifetime_usetime : 0;

	newsp->refcnt = 1;	/* do not reclaim until I say I do */
	newsp->state = IPSEC_SPSTATE_ALIVE;
	LIST_INSERT_TAIL(&V_sptree[newsp->spidx.dir], newsp, secpolicy, chain);

	/* delete the entry in spacqtree */
	if (mhp->msg->sadb_msg_type == SADB_X_SPDUPDATE) {
		struct secspacq *spacq = key_getspacq(&spidx);
		if (spacq != NULL) {
			/* reset counter in order to deletion by timehandler. */
			spacq->created = time_second;
			spacq->count = 0;
			SPACQ_UNLOCK();
		}
    	}

    {
	struct mbuf *n, *mpolicy;
	struct sadb_msg *newmsg;
	int off;

	/*
	 * Note: do not send SADB_X_EXT_NAT_T_* here:
	 * we are sending traffic endpoints.
	 */

	/* create new sadb_msg to reply. */
	if (lft) {
		n = key_gather_mbuf(m, mhp, 2, 5, SADB_EXT_RESERVED,
		    SADB_X_EXT_POLICY, SADB_EXT_LIFETIME_HARD,
		    SADB_EXT_ADDRESS_SRC, SADB_EXT_ADDRESS_DST);
	} else {
		n = key_gather_mbuf(m, mhp, 2, 4, SADB_EXT_RESERVED,
		    SADB_X_EXT_POLICY,
		    SADB_EXT_ADDRESS_SRC, SADB_EXT_ADDRESS_DST);
	}
	if (!n)
		return key_senderror(so, m, ENOBUFS);

	if (n->m_len < sizeof(*newmsg)) {
		n = m_pullup(n, sizeof(*newmsg));
		if (!n)
			return key_senderror(so, m, ENOBUFS);
	}
	newmsg = mtod(n, struct sadb_msg *);
	newmsg->sadb_msg_errno = 0;
	newmsg->sadb_msg_len = PFKEY_UNIT64(n->m_pkthdr.len);

	off = 0;
	mpolicy = m_pulldown(n, PFKEY_ALIGN8(sizeof(struct sadb_msg)),
	    sizeof(*xpl), &off);
	if (mpolicy == NULL) {
		/* n is already freed */
		return key_senderror(so, m, ENOBUFS);
	}
	xpl = (struct sadb_x_policy *)(mtod(mpolicy, caddr_t) + off);
	if (xpl->sadb_x_policy_exttype != SADB_X_EXT_POLICY) {
		m_freem(n);
		return key_senderror(so, m, EINVAL);
	}
	xpl->sadb_x_policy_id = newsp->id;

	m_freem(m);
	return key_sendup_mbuf(so, n, KEY_SENDUP_ALL);
    }
}

/*
 * get new policy id.
 * OUT:
 *	0:	failure.
 *	others: success.
 */
static u_int32_t
key_getnewspid()
{
	u_int32_t newid = 0;
	int count = V_key_spi_trycnt;	/* XXX */
	struct secpolicy *sp;

	/* when requesting to allocate spi ranged */
	while (count--) {
		newid = (V_policy_id = (V_policy_id == ~0 ? 1 : V_policy_id + 1));

		if ((sp = key_getspbyid(newid)) == NULL)
			break;

		KEY_FREESP(&sp);
	}

	if (count == 0 || newid == 0) {
		ipseclog((LOG_DEBUG, "%s: to allocate policy id is failed.\n",
			__func__));
		return 0;
	}

	return newid;
}

/*
 * SADB_SPDDELETE processing
 * receive
 *   <base, address(SD), policy(*)>
 * from the user(?), and set SADB_SASTATE_DEAD,
 * and send,
 *   <base, address(SD), policy(*)>
 * to the ikmpd.
 * policy(*) including direction of policy.
 *
 * m will always be freed.
 */
static int
key_spddelete(so, m, mhp)
	struct socket *so;
	struct mbuf *m;
	const struct sadb_msghdr *mhp;
{
	struct sadb_address *src0, *dst0;
	struct sadb_x_policy *xpl0;
	struct secpolicyindex spidx;
	struct secpolicy *sp;

	IPSEC_ASSERT(so != NULL, ("null so"));
	IPSEC_ASSERT(m != NULL, ("null mbuf"));
	IPSEC_ASSERT(mhp != NULL, ("null msghdr"));
	IPSEC_ASSERT(mhp->msg != NULL, ("null msg"));

	if (mhp->ext[SADB_EXT_ADDRESS_SRC] == NULL ||
	    mhp->ext[SADB_EXT_ADDRESS_DST] == NULL ||
	    mhp->ext[SADB_X_EXT_POLICY] == NULL) {
		ipseclog((LOG_DEBUG, "%s: invalid message is passed.\n",
			__func__));
		return key_senderror(so, m, EINVAL);
	}
	if (mhp->extlen[SADB_EXT_ADDRESS_SRC] < sizeof(struct sadb_address) ||
	    mhp->extlen[SADB_EXT_ADDRESS_DST] < sizeof(struct sadb_address) ||
	    mhp->extlen[SADB_X_EXT_POLICY] < sizeof(struct sadb_x_policy)) {
		ipseclog((LOG_DEBUG, "%s: invalid message is passed.\n",
			__func__));
		return key_senderror(so, m, EINVAL);
	}

	src0 = (struct sadb_address *)mhp->ext[SADB_EXT_ADDRESS_SRC];
	dst0 = (struct sadb_address *)mhp->ext[SADB_EXT_ADDRESS_DST];
	xpl0 = (struct sadb_x_policy *)mhp->ext[SADB_X_EXT_POLICY];

	/*
	 * Note: do not parse SADB_X_EXT_NAT_T_* here:
	 * we are processing traffic endpoints.
	 */

	/* make secindex */
	/* XXX boundary check against sa_len */
	KEY_SETSECSPIDX(xpl0->sadb_x_policy_dir,
	                src0 + 1,
	                dst0 + 1,
	                src0->sadb_address_prefixlen,
	                dst0->sadb_address_prefixlen,
	                src0->sadb_address_proto,
	                &spidx);

	/* checking the direciton. */
	switch (xpl0->sadb_x_policy_dir) {
	case IPSEC_DIR_INBOUND:
	case IPSEC_DIR_OUTBOUND:
		break;
	default:
		ipseclog((LOG_DEBUG, "%s: Invalid SP direction.\n", __func__));
		return key_senderror(so, m, EINVAL);
	}

	/* Is there SP in SPD ? */
	if ((sp = key_getsp(&spidx)) == NULL) {
		ipseclog((LOG_DEBUG, "%s: no SP found.\n", __func__));
		return key_senderror(so, m, EINVAL);
	}

	/* save policy id to buffer to be returned. */
	xpl0->sadb_x_policy_id = sp->id;

	SPTREE_LOCK();
	sp->state = IPSEC_SPSTATE_DEAD;
	SPTREE_UNLOCK();
	KEY_FREESP(&sp);

    {
	struct mbuf *n;
	struct sadb_msg *newmsg;

	/*
	 * Note: do not send SADB_X_EXT_NAT_T_* here:
	 * we are sending traffic endpoints.
	 */

	/* create new sadb_msg to reply. */
	n = key_gather_mbuf(m, mhp, 1, 4, SADB_EXT_RESERVED,
	    SADB_X_EXT_POLICY, SADB_EXT_ADDRESS_SRC, SADB_EXT_ADDRESS_DST);
	if (!n)
		return key_senderror(so, m, ENOBUFS);

	newmsg = mtod(n, struct sadb_msg *);
	newmsg->sadb_msg_errno = 0;
	newmsg->sadb_msg_len = PFKEY_UNIT64(n->m_pkthdr.len);

	m_freem(m);
	return key_sendup_mbuf(so, n, KEY_SENDUP_ALL);
    }
}

/*
 * SADB_SPDDELETE2 processing
 * receive
 *   <base, policy(*)>
 * from the user(?), and set SADB_SASTATE_DEAD,
 * and send,
 *   <base, policy(*)>
 * to the ikmpd.
 * policy(*) including direction of policy.
 *
 * m will always be freed.
 */
static int
key_spddelete2(so, m, mhp)
	struct socket *so;
	struct mbuf *m;
	const struct sadb_msghdr *mhp;
{
	u_int32_t id;
	struct secpolicy *sp;

	IPSEC_ASSERT(so != NULL, ("null socket"));
	IPSEC_ASSERT(m != NULL, ("null mbuf"));
	IPSEC_ASSERT(mhp != NULL, ("null msghdr"));
	IPSEC_ASSERT(mhp->msg != NULL, ("null msg"));

	if (mhp->ext[SADB_X_EXT_POLICY] == NULL ||
	    mhp->extlen[SADB_X_EXT_POLICY] < sizeof(struct sadb_x_policy)) {
		ipseclog((LOG_DEBUG, "%s: invalid message is passed.\n", __func__));
		return key_senderror(so, m, EINVAL);
	}

	id = ((struct sadb_x_policy *)mhp->ext[SADB_X_EXT_POLICY])->sadb_x_policy_id;

	/* Is there SP in SPD ? */
	if ((sp = key_getspbyid(id)) == NULL) {
		ipseclog((LOG_DEBUG, "%s: no SP found id:%u.\n", __func__, id));
		return key_senderror(so, m, EINVAL);
	}

	SPTREE_LOCK();
	sp->state = IPSEC_SPSTATE_DEAD;
	SPTREE_UNLOCK();
	KEY_FREESP(&sp);

    {
	struct mbuf *n, *nn;
	struct sadb_msg *newmsg;
	int off, len;

	/* create new sadb_msg to reply. */
	len = PFKEY_ALIGN8(sizeof(struct sadb_msg));

	MGETHDR(n, M_NOWAIT, MT_DATA);
	if (n && len > MHLEN) {
		MCLGET(n, M_NOWAIT);
		if ((n->m_flags & M_EXT) == 0) {
			m_freem(n);
			n = NULL;
		}
	}
	if (!n)
		return key_senderror(so, m, ENOBUFS);

	n->m_len = len;
	n->m_next = NULL;
	off = 0;

	m_copydata(m, 0, sizeof(struct sadb_msg), mtod(n, caddr_t) + off);
	off += PFKEY_ALIGN8(sizeof(struct sadb_msg));

	IPSEC_ASSERT(off == len, ("length inconsistency (off %u len %u)",
		off, len));

	n->m_next = m_copym(m, mhp->extoff[SADB_X_EXT_POLICY],
	    mhp->extlen[SADB_X_EXT_POLICY], M_NOWAIT);
	if (!n->m_next) {
		m_freem(n);
		return key_senderror(so, m, ENOBUFS);
	}

	n->m_pkthdr.len = 0;
	for (nn = n; nn; nn = nn->m_next)
		n->m_pkthdr.len += nn->m_len;

	newmsg = mtod(n, struct sadb_msg *);
	newmsg->sadb_msg_errno = 0;
	newmsg->sadb_msg_len = PFKEY_UNIT64(n->m_pkthdr.len);

	m_freem(m);
	return key_sendup_mbuf(so, n, KEY_SENDUP_ALL);
    }
}

/*
 * SADB_X_GET processing
 * receive
 *   <base, policy(*)>
 * from the user(?),
 * and send,
 *   <base, address(SD), policy>
 * to the ikmpd.
 * policy(*) including direction of policy.
 *
 * m will always be freed.
 */
static int
key_spdget(so, m, mhp)
	struct socket *so;
	struct mbuf *m;
	const struct sadb_msghdr *mhp;
{
	u_int32_t id;
	struct secpolicy *sp;
	struct mbuf *n;

	IPSEC_ASSERT(so != NULL, ("null socket"));
	IPSEC_ASSERT(m != NULL, ("null mbuf"));
	IPSEC_ASSERT(mhp != NULL, ("null msghdr"));
	IPSEC_ASSERT(mhp->msg != NULL, ("null msg"));

	if (mhp->ext[SADB_X_EXT_POLICY] == NULL ||
	    mhp->extlen[SADB_X_EXT_POLICY] < sizeof(struct sadb_x_policy)) {
		ipseclog((LOG_DEBUG, "%s: invalid message is passed.\n",
			__func__));
		return key_senderror(so, m, EINVAL);
	}

	id = ((struct sadb_x_policy *)mhp->ext[SADB_X_EXT_POLICY])->sadb_x_policy_id;

	/* Is there SP in SPD ? */
	if ((sp = key_getspbyid(id)) == NULL) {
		ipseclog((LOG_DEBUG, "%s: no SP found id:%u.\n", __func__, id));
		return key_senderror(so, m, ENOENT);
	}

	n = key_setdumpsp(sp, SADB_X_SPDGET, 0, mhp->msg->sadb_msg_pid);
	KEY_FREESP(&sp);
	if (n != NULL) {
		m_freem(m);
		return key_sendup_mbuf(so, n, KEY_SENDUP_ONE);
	} else
		return key_senderror(so, m, ENOBUFS);
}

/*
 * SADB_X_SPDACQUIRE processing.
 * Acquire policy and SA(s) for a *OUTBOUND* packet.
 * send
 *   <base, policy(*)>
 * to KMD, and expect to receive
 *   <base> with SADB_X_SPDACQUIRE if error occured,
 * or
 *   <base, policy>
 * with SADB_X_SPDUPDATE from KMD by PF_KEY.
 * policy(*) is without policy requests.
 *
 *    0     : succeed
 *    others: error number
 */
int
key_spdacquire(sp)
	struct secpolicy *sp;
{
	struct mbuf *result = NULL, *m;
	struct secspacq *newspacq;

	IPSEC_ASSERT(sp != NULL, ("null secpolicy"));
	IPSEC_ASSERT(sp->req == NULL, ("policy exists"));
	IPSEC_ASSERT(sp->policy == IPSEC_POLICY_IPSEC,
		("policy not IPSEC %u", sp->policy));

	/* Get an entry to check whether sent message or not. */
	newspacq = key_getspacq(&sp->spidx);
	if (newspacq != NULL) {
		if (V_key_blockacq_count < newspacq->count) {
			/* reset counter and do send message. */
			newspacq->count = 0;
		} else {
			/* increment counter and do nothing. */
			newspacq->count++;
			return 0;
		}
		SPACQ_UNLOCK();
	} else {
		/* make new entry for blocking to send SADB_ACQUIRE. */
		newspacq = key_newspacq(&sp->spidx);
		if (newspacq == NULL)
			return ENOBUFS;
	}

	/* create new sadb_msg to reply. */
	m = key_setsadbmsg(SADB_X_SPDACQUIRE, 0, 0, 0, 0, 0);
	if (!m)
		return ENOBUFS;

	result = m;

	result->m_pkthdr.len = 0;
	for (m = result; m; m = m->m_next)
		result->m_pkthdr.len += m->m_len;

	mtod(result, struct sadb_msg *)->sadb_msg_len =
	    PFKEY_UNIT64(result->m_pkthdr.len);

	return key_sendup_mbuf(NULL, m, KEY_SENDUP_REGISTERED);
}

/*
 * SADB_SPDFLUSH processing
 * receive
 *   <base>
 * from the user, and free all entries in secpctree.
 * and send,
 *   <base>
 * to the user.
 * NOTE: what to do is only marking SADB_SASTATE_DEAD.
 *
 * m will always be freed.
 */
static int
key_spdflush(so, m, mhp)
	struct socket *so;
	struct mbuf *m;
	const struct sadb_msghdr *mhp;
{
	struct sadb_msg *newmsg;
	struct secpolicy *sp;
	u_int dir;

	IPSEC_ASSERT(so != NULL, ("null socket"));
	IPSEC_ASSERT(m != NULL, ("null mbuf"));
	IPSEC_ASSERT(mhp != NULL, ("null msghdr"));
	IPSEC_ASSERT(mhp->msg != NULL, ("null msg"));

	if (m->m_len != PFKEY_ALIGN8(sizeof(struct sadb_msg)))
		return key_senderror(so, m, EINVAL);

	for (dir = 0; dir < IPSEC_DIR_MAX; dir++) {
		SPTREE_LOCK();
		LIST_FOREACH(sp, &V_sptree[dir], chain)
			sp->state = IPSEC_SPSTATE_DEAD;
		SPTREE_UNLOCK();
	}

	if (sizeof(struct sadb_msg) > m->m_len + M_TRAILINGSPACE(m)) {
		ipseclog((LOG_DEBUG, "%s: No more memory.\n", __func__));
		return key_senderror(so, m, ENOBUFS);
	}

	if (m->m_next)
		m_freem(m->m_next);
	m->m_next = NULL;
	m->m_pkthdr.len = m->m_len = PFKEY_ALIGN8(sizeof(struct sadb_msg));
	newmsg = mtod(m, struct sadb_msg *);
	newmsg->sadb_msg_errno = 0;
	newmsg->sadb_msg_len = PFKEY_UNIT64(m->m_pkthdr.len);

	return key_sendup_mbuf(so, m, KEY_SENDUP_ALL);
}

/*
 * SADB_SPDDUMP processing
 * receive
 *   <base>
 * from the user, and dump all SP leaves
 * and send,
 *   <base> .....
 * to the ikmpd.
 *
 * m will always be freed.
 */
static int
key_spddump(so, m, mhp)
	struct socket *so;
	struct mbuf *m;
	const struct sadb_msghdr *mhp;
{
	struct secpolicy *sp;
	int cnt;
	u_int dir;
	struct mbuf *n;

	IPSEC_ASSERT(so != NULL, ("null socket"));
	IPSEC_ASSERT(m != NULL, ("null mbuf"));
	IPSEC_ASSERT(mhp != NULL, ("null msghdr"));
	IPSEC_ASSERT(mhp->msg != NULL, ("null msg"));

	/* search SPD entry and get buffer size. */
	cnt = 0;
	SPTREE_LOCK();
	for (dir = 0; dir < IPSEC_DIR_MAX; dir++) {
		LIST_FOREACH(sp, &V_sptree[dir], chain) {
			cnt++;
		}
	}

	if (cnt == 0) {
		SPTREE_UNLOCK();
		return key_senderror(so, m, ENOENT);
	}

	for (dir = 0; dir < IPSEC_DIR_MAX; dir++) {
		LIST_FOREACH(sp, &V_sptree[dir], chain) {
			--cnt;
			n = key_setdumpsp(sp, SADB_X_SPDDUMP, cnt,
			    mhp->msg->sadb_msg_pid);

			if (n)
				key_sendup_mbuf(so, n, KEY_SENDUP_ONE);
		}
	}

	SPTREE_UNLOCK();
	m_freem(m);
	return 0;
}

static struct mbuf *
key_setdumpsp(struct secpolicy *sp, u_int8_t type, u_int32_t seq, u_int32_t pid)
{
	struct mbuf *result = NULL, *m;
	struct seclifetime lt;

	m = key_setsadbmsg(type, 0, SADB_SATYPE_UNSPEC, seq, pid, sp->refcnt);
	if (!m)
		goto fail;
	result = m;

	/*
	 * Note: do not send SADB_X_EXT_NAT_T_* here:
	 * we are sending traffic endpoints.
	 */
	m = key_setsadbaddr(SADB_EXT_ADDRESS_SRC,
	    &sp->spidx.src.sa, sp->spidx.prefs,
	    sp->spidx.ul_proto);
	if (!m)
		goto fail;
	m_cat(result, m);

	m = key_setsadbaddr(SADB_EXT_ADDRESS_DST,
	    &sp->spidx.dst.sa, sp->spidx.prefd,
	    sp->spidx.ul_proto);
	if (!m)
		goto fail;
	m_cat(result, m);

	m = key_sp2msg(sp);
	if (!m)
		goto fail;
	m_cat(result, m);

	if(sp->lifetime){
		lt.addtime=sp->created;
		lt.usetime= sp->lastused;
		m = key_setlifetime(&lt, SADB_EXT_LIFETIME_CURRENT);
		if (!m)
			goto fail;
		m_cat(result, m);
		
		lt.addtime=sp->lifetime;
		lt.usetime= sp->validtime;
		m = key_setlifetime(&lt, SADB_EXT_LIFETIME_HARD);
		if (!m)
			goto fail;
		m_cat(result, m);
	}

	if ((result->m_flags & M_PKTHDR) == 0)
		goto fail;

	if (result->m_len < sizeof(struct sadb_msg)) {
		result = m_pullup(result, sizeof(struct sadb_msg));
		if (result == NULL)
			goto fail;
	}

	result->m_pkthdr.len = 0;
	for (m = result; m; m = m->m_next)
		result->m_pkthdr.len += m->m_len;

	mtod(result, struct sadb_msg *)->sadb_msg_len =
	    PFKEY_UNIT64(result->m_pkthdr.len);

	return result;

fail:
	m_freem(result);
	return NULL;
}

/*
 * get PFKEY message length for security policy and request.
 */
static u_int
key_getspreqmsglen(sp)
	struct secpolicy *sp;
{
	u_int tlen;

	tlen = sizeof(struct sadb_x_policy);

	/* if is the policy for ipsec ? */
	if (sp->policy != IPSEC_POLICY_IPSEC)
		return tlen;

	/* get length of ipsec requests */
    {
	struct ipsecrequest *isr;
	int len;

	for (isr = sp->req; isr != NULL; isr = isr->next) {
		len = sizeof(struct sadb_x_ipsecrequest)
			+ isr->saidx.src.sa.sa_len
			+ isr->saidx.dst.sa.sa_len;

		tlen += PFKEY_ALIGN8(len);
	}
    }

	return tlen;
}

/*
 * SADB_SPDEXPIRE processing
 * send
 *   <base, address(SD), lifetime(CH), policy>
 * to KMD by PF_KEY.
 *
 * OUT:	0	: succeed
 *	others	: error number
 */
static int
key_spdexpire(sp)
	struct secpolicy *sp;
{
	struct mbuf *result = NULL, *m;
	int len;
	int error = -1;
	struct sadb_lifetime *lt;

	/* XXX: Why do we lock ? */

	IPSEC_ASSERT(sp != NULL, ("null secpolicy"));

	/* set msg header */
	m = key_setsadbmsg(SADB_X_SPDEXPIRE, 0, 0, 0, 0, 0);
	if (!m) {
		error = ENOBUFS;
		goto fail;
	}
	result = m;

	/* create lifetime extension (current and hard) */
	len = PFKEY_ALIGN8(sizeof(*lt)) * 2;
	m = m_get2(len, M_NOWAIT, MT_DATA, 0);
	if (m == NULL) {
		error = ENOBUFS;
		goto fail;
	}
	m_align(m, len);
	m->m_len = len;
	bzero(mtod(m, caddr_t), len);
	lt = mtod(m, struct sadb_lifetime *);
	lt->sadb_lifetime_len = PFKEY_UNIT64(sizeof(struct sadb_lifetime));
	lt->sadb_lifetime_exttype = SADB_EXT_LIFETIME_CURRENT;
	lt->sadb_lifetime_allocations = 0;
	lt->sadb_lifetime_bytes = 0;
	lt->sadb_lifetime_addtime = sp->created;
	lt->sadb_lifetime_usetime = sp->lastused;
	lt = (struct sadb_lifetime *)(mtod(m, caddr_t) + len / 2);
	lt->sadb_lifetime_len = PFKEY_UNIT64(sizeof(struct sadb_lifetime));
	lt->sadb_lifetime_exttype = SADB_EXT_LIFETIME_HARD;
	lt->sadb_lifetime_allocations = 0;
	lt->sadb_lifetime_bytes = 0;
	lt->sadb_lifetime_addtime = sp->lifetime;
	lt->sadb_lifetime_usetime = sp->validtime;
	m_cat(result, m);

	/*
	 * Note: do not send SADB_X_EXT_NAT_T_* here:
	 * we are sending traffic endpoints.
	 */

	/* set sadb_address for source */
	m = key_setsadbaddr(SADB_EXT_ADDRESS_SRC,
	    &sp->spidx.src.sa,
	    sp->spidx.prefs, sp->spidx.ul_proto);
	if (!m) {
		error = ENOBUFS;
		goto fail;
	}
	m_cat(result, m);

	/* set sadb_address for destination */
	m = key_setsadbaddr(SADB_EXT_ADDRESS_DST,
	    &sp->spidx.dst.sa,
	    sp->spidx.prefd, sp->spidx.ul_proto);
	if (!m) {
		error = ENOBUFS;
		goto fail;
	}
	m_cat(result, m);

	/* set secpolicy */
	m = key_sp2msg(sp);
	if (!m) {
		error = ENOBUFS;
		goto fail;
	}
	m_cat(result, m);

	if ((result->m_flags & M_PKTHDR) == 0) {
		error = EINVAL;
		goto fail;
	}

	if (result->m_len < sizeof(struct sadb_msg)) {
		result = m_pullup(result, sizeof(struct sadb_msg));
		if (result == NULL) {
			error = ENOBUFS;
			goto fail;
		}
	}

	result->m_pkthdr.len = 0;
	for (m = result; m; m = m->m_next)
		result->m_pkthdr.len += m->m_len;

	mtod(result, struct sadb_msg *)->sadb_msg_len =
	    PFKEY_UNIT64(result->m_pkthdr.len);

	return key_sendup_mbuf(NULL, result, KEY_SENDUP_REGISTERED);

 fail:
	if (result)
		m_freem(result);
	return error;
}

/* %%% SAD management */
/*
 * allocating a memory for new SA head, and copy from the values of mhp.
 * OUT:	NULL	: failure due to the lack of memory.
 *	others	: pointer to new SA head.
 */
static struct secashead *
key_newsah(saidx)
	struct secasindex *saidx;
{
	struct secashead *newsah;

	IPSEC_ASSERT(saidx != NULL, ("null saidx"));

	newsah = malloc(sizeof(struct secashead), M_IPSEC_SAH, M_NOWAIT|M_ZERO);
	if (newsah != NULL) {
		int i;
		for (i = 0; i < sizeof(newsah->savtree)/sizeof(newsah->savtree[0]); i++)
			LIST_INIT(&newsah->savtree[i]);
		newsah->saidx = *saidx;

		/* add to saidxtree */
		newsah->state = SADB_SASTATE_MATURE;

		SAHTREE_LOCK();
		LIST_INSERT_HEAD(&V_sahtree, newsah, chain);
		SAHTREE_UNLOCK();
	}
	return(newsah);
}

/*
 * delete SA index and all SA registerd.
 */
static void
key_delsah(sah)
	struct secashead *sah;
{
	struct secasvar *sav, *nextsav;
	u_int stateidx;
	int zombie = 0;

	IPSEC_ASSERT(sah != NULL, ("NULL sah"));
	SAHTREE_LOCK_ASSERT();

	/* searching all SA registerd in the secindex. */
	for (stateidx = 0;
	     stateidx < _ARRAYLEN(saorder_state_any);
	     stateidx++) {
		u_int state = saorder_state_any[stateidx];
		LIST_FOREACH_SAFE(sav, &sah->savtree[state], chain, nextsav) {
			if (sav->refcnt == 0) {
				/* sanity check */
				KEY_CHKSASTATE(state, sav->state, __func__);
				/* 
				 * do NOT call KEY_FREESAV here:
				 * it will only delete the sav if refcnt == 1,
				 * where we already know that refcnt == 0
				 */
				key_delsav(sav);
			} else {
				/* give up to delete this sa */
				zombie++;
			}
		}
	}
	if (!zombie) {		/* delete only if there are savs */
		/* remove from tree of SA index */
		if (__LIST_CHAINED(sah))
			LIST_REMOVE(sah, chain);
		if (sah->route_cache.sa_route.ro_rt) {
			RTFREE(sah->route_cache.sa_route.ro_rt);
			sah->route_cache.sa_route.ro_rt = (struct rtentry *)NULL;
		}
		free(sah, M_IPSEC_SAH);
	}
}

/*
 * allocating a new SA with LARVAL state.  key_add() and key_getspi() call,
 * and copy the values of mhp into new buffer.
 * When SAD message type is GETSPI:
 *	to set sequence number from acq_seq++,
 *	to set zero to SPI.
 *	not to call key_setsava().
 * OUT:	NULL	: fail
 *	others	: pointer to new secasvar.
 *
 * does not modify mbuf.  does not free mbuf on error.
 */
static struct secasvar *
key_newsav(m, mhp, sah, errp, where, tag)
	struct mbuf *m;
	const struct sadb_msghdr *mhp;
	struct secashead *sah;
	int *errp;
	const char* where;
	int tag;
{
	struct secasvar *newsav;
	const struct sadb_sa *xsa;

	IPSEC_ASSERT(m != NULL, ("null mbuf"));
	IPSEC_ASSERT(mhp != NULL, ("null msghdr"));
	IPSEC_ASSERT(mhp->msg != NULL, ("null msg"));
	IPSEC_ASSERT(sah != NULL, ("null secashead"));

	newsav = malloc(sizeof(struct secasvar), M_IPSEC_SA, M_NOWAIT|M_ZERO);
	if (newsav == NULL) {
		ipseclog((LOG_DEBUG, "%s: No more memory.\n", __func__));
		*errp = ENOBUFS;
		goto done;
	}

	switch (mhp->msg->sadb_msg_type) {
	case SADB_GETSPI:
		newsav->spi = 0;

#ifdef IPSEC_DOSEQCHECK
		/* sync sequence number */
		if (mhp->msg->sadb_msg_seq == 0)
			newsav->seq =
				(V_acq_seq = (V_acq_seq == ~0 ? 1 : ++V_acq_seq));
		else
#endif
			newsav->seq = mhp->msg->sadb_msg_seq;
		break;

	case SADB_ADD:
		/* sanity check */
		if (mhp->ext[SADB_EXT_SA] == NULL) {
			free(newsav, M_IPSEC_SA);
			newsav = NULL;
			ipseclog((LOG_DEBUG, "%s: invalid message is passed.\n",
				__func__));
			*errp = EINVAL;
			goto done;
		}
		xsa = (const struct sadb_sa *)mhp->ext[SADB_EXT_SA];
		newsav->spi = xsa->sadb_sa_spi;
		newsav->seq = mhp->msg->sadb_msg_seq;
		break;
	default:
		free(newsav, M_IPSEC_SA);
		newsav = NULL;
		*errp = EINVAL;
		goto done;
	}


	/* copy sav values */
	if (mhp->msg->sadb_msg_type != SADB_GETSPI) {
		*errp = key_setsaval(newsav, m, mhp);
		if (*errp) {
			free(newsav, M_IPSEC_SA);
			newsav = NULL;
			goto done;
		}
	}

	SECASVAR_LOCK_INIT(newsav);

	/* reset created */
	newsav->created = time_second;
	newsav->pid = mhp->msg->sadb_msg_pid;

	/* add to satree */
	newsav->sah = sah;
	sa_initref(newsav);
	newsav->state = SADB_SASTATE_LARVAL;

	SAHTREE_LOCK();
	LIST_INSERT_TAIL(&sah->savtree[SADB_SASTATE_LARVAL], newsav,
			secasvar, chain);
	SAHTREE_UNLOCK();
done:
	KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
		printf("DP %s from %s:%u return SP:%p\n", __func__,
			where, tag, newsav));

	return newsav;
}

/*
 * free() SA variable entry.
 */
static void
key_cleansav(struct secasvar *sav)
{
	/*
	 * Cleanup xform state.  Note that zeroize'ing causes the
	 * keys to be cleared; otherwise we must do it ourself.
	 */
	if (sav->tdb_xform != NULL) {
		sav->tdb_xform->xf_zeroize(sav);
		sav->tdb_xform = NULL;
	} else {
		KASSERT(sav->iv == NULL, ("iv but no xform"));
		if (sav->key_auth != NULL)
			bzero(sav->key_auth->key_data, _KEYLEN(sav->key_auth));
		if (sav->key_enc != NULL)
			bzero(sav->key_enc->key_data, _KEYLEN(sav->key_enc));
	}
	if (sav->key_auth != NULL) {
		if (sav->key_auth->key_data != NULL)
			free(sav->key_auth->key_data, M_IPSEC_MISC);
		free(sav->key_auth, M_IPSEC_MISC);
		sav->key_auth = NULL;
	}
	if (sav->key_enc != NULL) {
		if (sav->key_enc->key_data != NULL)
			free(sav->key_enc->key_data, M_IPSEC_MISC);
		free(sav->key_enc, M_IPSEC_MISC);
		sav->key_enc = NULL;
	}
	if (sav->sched) {
		bzero(sav->sched, sav->schedlen);
		free(sav->sched, M_IPSEC_MISC);
		sav->sched = NULL;
	}
	if (sav->replay != NULL) {
		free(sav->replay, M_IPSEC_MISC);
		sav->replay = NULL;
	}
	if (sav->lft_c != NULL) {
		free(sav->lft_c, M_IPSEC_MISC);
		sav->lft_c = NULL;
	}
	if (sav->lft_h != NULL) {
		free(sav->lft_h, M_IPSEC_MISC);
		sav->lft_h = NULL;
	}
	if (sav->lft_s != NULL) {
		free(sav->lft_s, M_IPSEC_MISC);
		sav->lft_s = NULL;
	}
}

/*
 * free() SA variable entry.
 */
static void
key_delsav(sav)
	struct secasvar *sav;
{
	IPSEC_ASSERT(sav != NULL, ("null sav"));
	IPSEC_ASSERT(sav->refcnt == 0, ("reference count %u > 0", sav->refcnt));

	/* remove from SA header */
	if (__LIST_CHAINED(sav))
		LIST_REMOVE(sav, chain);
	key_cleansav(sav);
	SECASVAR_LOCK_DESTROY(sav);
	free(sav, M_IPSEC_SA);
}

/*
 * search SAD.
 * OUT:
 *	NULL	: not found
 *	others	: found, pointer to a SA.
 */
static struct secashead *
key_getsah(saidx)
	struct secasindex *saidx;
{
	struct secashead *sah;

	SAHTREE_LOCK();
	LIST_FOREACH(sah, &V_sahtree, chain) {
		if (sah->state == SADB_SASTATE_DEAD)
			continue;
		if (key_cmpsaidx(&sah->saidx, saidx, CMP_REQID))
			break;
	}
	SAHTREE_UNLOCK();

	return sah;
}

/*
 * check not to be duplicated SPI.
 * NOTE: this function is too slow due to searching all SAD.
 * OUT:
 *	NULL	: not found
 *	others	: found, pointer to a SA.
 */
static struct secasvar *
key_checkspidup(saidx, spi)
	struct secasindex *saidx;
	u_int32_t spi;
{
	struct secashead *sah;
	struct secasvar *sav;

	/* check address family */
	if (saidx->src.sa.sa_family != saidx->dst.sa.sa_family) {
		ipseclog((LOG_DEBUG, "%s: address family mismatched.\n",
			__func__));
		return NULL;
	}

	sav = NULL;
	/* check all SAD */
	SAHTREE_LOCK();
	LIST_FOREACH(sah, &V_sahtree, chain) {
		if (!key_ismyaddr((struct sockaddr *)&sah->saidx.dst))
			continue;
		sav = key_getsavbyspi(sah, spi);
		if (sav != NULL)
			break;
	}
	SAHTREE_UNLOCK();

	return sav;
}

/*
 * search SAD litmited alive SA, protocol, SPI.
 * OUT:
 *	NULL	: not found
 *	others	: found, pointer to a SA.
 */
static struct secasvar *
key_getsavbyspi(sah, spi)
	struct secashead *sah;
	u_int32_t spi;
{
	struct secasvar *sav;
	u_int stateidx, state;

	sav = NULL;
	SAHTREE_LOCK_ASSERT();
	/* search all status */
	for (stateidx = 0;
	     stateidx < _ARRAYLEN(saorder_state_alive);
	     stateidx++) {

		state = saorder_state_alive[stateidx];
		LIST_FOREACH(sav, &sah->savtree[state], chain) {

			/* sanity check */
			if (sav->state != state) {
				ipseclog((LOG_DEBUG, "%s: "
				    "invalid sav->state (queue: %d SA: %d)\n",
				    __func__, state, sav->state));
				continue;
			}

			if (sav->spi == spi)
				return sav;
		}
	}

	return NULL;
}

/*
 * copy SA values from PF_KEY message except *SPI, SEQ, PID, STATE and TYPE*.
 * You must update these if need.
 * OUT:	0:	success.
 *	!0:	failure.
 *
 * does not modify mbuf.  does not free mbuf on error.
 */
static int
key_setsaval(sav, m, mhp)
	struct secasvar *sav;
	struct mbuf *m;
	const struct sadb_msghdr *mhp;
{
	int error = 0;

	IPSEC_ASSERT(m != NULL, ("null mbuf"));
	IPSEC_ASSERT(mhp != NULL, ("null msghdr"));
	IPSEC_ASSERT(mhp->msg != NULL, ("null msg"));

	/* initialization */
	sav->replay = NULL;
	sav->key_auth = NULL;
	sav->key_enc = NULL;
	sav->sched = NULL;
	sav->schedlen = 0;
	sav->iv = NULL;
	sav->lft_c = NULL;
	sav->lft_h = NULL;
	sav->lft_s = NULL;
	sav->tdb_xform = NULL;		/* transform */
	sav->tdb_encalgxform = NULL;	/* encoding algorithm */
	sav->tdb_authalgxform = NULL;	/* authentication algorithm */
	sav->tdb_compalgxform = NULL;	/* compression algorithm */
	/*  Initialize even if NAT-T not compiled in: */
	sav->natt_type = 0;
	sav->natt_esp_frag_len = 0;

	/* SA */
	if (mhp->ext[SADB_EXT_SA] != NULL) {
		const struct sadb_sa *sa0;

		sa0 = (const struct sadb_sa *)mhp->ext[SADB_EXT_SA];
		if (mhp->extlen[SADB_EXT_SA] < sizeof(*sa0)) {
			error = EINVAL;
			goto fail;
		}

		sav->alg_auth = sa0->sadb_sa_auth;
		sav->alg_enc = sa0->sadb_sa_encrypt;
		sav->flags = sa0->sadb_sa_flags;

		/* replay window */
		if ((sa0->sadb_sa_flags & SADB_X_EXT_OLD) == 0) {
			sav->replay = (struct secreplay *)
				malloc(sizeof(struct secreplay)+sa0->sadb_sa_replay, M_IPSEC_MISC, M_NOWAIT|M_ZERO);
			if (sav->replay == NULL) {
				ipseclog((LOG_DEBUG, "%s: No more memory.\n",
					__func__));
				error = ENOBUFS;
				goto fail;
			}
			if (sa0->sadb_sa_replay != 0)
				sav->replay->bitmap = (caddr_t)(sav->replay+1);
			sav->replay->wsize = sa0->sadb_sa_replay;
		}
	}

	/* Authentication keys */
	if (mhp->ext[SADB_EXT_KEY_AUTH] != NULL) {
		const struct sadb_key *key0;
		int len;

		key0 = (const struct sadb_key *)mhp->ext[SADB_EXT_KEY_AUTH];
		len = mhp->extlen[SADB_EXT_KEY_AUTH];

		error = 0;
		if (len < sizeof(*key0)) {
			error = EINVAL;
			goto fail;
		}
		switch (mhp->msg->sadb_msg_satype) {
		case SADB_SATYPE_AH:
		case SADB_SATYPE_ESP:
		case SADB_X_SATYPE_TCPSIGNATURE:
			if (len == PFKEY_ALIGN8(sizeof(struct sadb_key)) &&
			    sav->alg_auth != SADB_X_AALG_NULL)
				error = EINVAL;
			break;
		case SADB_X_SATYPE_IPCOMP:
		default:
			error = EINVAL;
			break;
		}
		if (error) {
			ipseclog((LOG_DEBUG, "%s: invalid key_auth values.\n",
				__func__));
			goto fail;
		}

		sav->key_auth = (struct seckey *)key_dup_keymsg(key0, len,
								M_IPSEC_MISC);
		if (sav->key_auth == NULL ) {
			ipseclog((LOG_DEBUG, "%s: No more memory.\n",
				  __func__));
			error = ENOBUFS;
			goto fail;
		}
	}

	/* Encryption key */
	if (mhp->ext[SADB_EXT_KEY_ENCRYPT] != NULL) {
		const struct sadb_key *key0;
		int len;

		key0 = (const struct sadb_key *)mhp->ext[SADB_EXT_KEY_ENCRYPT];
		len = mhp->extlen[SADB_EXT_KEY_ENCRYPT];

		error = 0;
		if (len < sizeof(*key0)) {
			error = EINVAL;
			goto fail;
		}
		switch (mhp->msg->sadb_msg_satype) {
		case SADB_SATYPE_ESP:
			if (len == PFKEY_ALIGN8(sizeof(struct sadb_key)) &&
			    sav->alg_enc != SADB_EALG_NULL) {
				error = EINVAL;
				break;
			}
			sav->key_enc = (struct seckey *)key_dup_keymsg(key0,
								       len,
								       M_IPSEC_MISC);
			if (sav->key_enc == NULL) {
				ipseclog((LOG_DEBUG, "%s: No more memory.\n",
					__func__));
				error = ENOBUFS;
				goto fail;
			}
			break;
		case SADB_X_SATYPE_IPCOMP:
			if (len != PFKEY_ALIGN8(sizeof(struct sadb_key)))
				error = EINVAL;
			sav->key_enc = NULL;	/*just in case*/
			break;
		case SADB_SATYPE_AH:
		case SADB_X_SATYPE_TCPSIGNATURE:
		default:
			error = EINVAL;
			break;
		}
		if (error) {
			ipseclog((LOG_DEBUG, "%s: invalid key_enc value.\n",
				__func__));
			goto fail;
		}
	}

	/* set iv */
	sav->ivlen = 0;

	switch (mhp->msg->sadb_msg_satype) {
	case SADB_SATYPE_AH:
		error = xform_init(sav, XF_AH);
		break;
	case SADB_SATYPE_ESP:
		error = xform_init(sav, XF_ESP);
		break;
	case SADB_X_SATYPE_IPCOMP:
		error = xform_init(sav, XF_IPCOMP);
		break;
	case SADB_X_SATYPE_TCPSIGNATURE:
		error = xform_init(sav, XF_TCPSIGNATURE);
		break;
	}
	if (error) {
		ipseclog((LOG_DEBUG, "%s: unable to initialize SA type %u.\n",
		        __func__, mhp->msg->sadb_msg_satype));
		goto fail;
	}

	/* reset created */
	sav->created = time_second;

	/* make lifetime for CURRENT */
	sav->lft_c = malloc(sizeof(struct seclifetime), M_IPSEC_MISC, M_NOWAIT);
	if (sav->lft_c == NULL) {
		ipseclog((LOG_DEBUG, "%s: No more memory.\n", __func__));
		error = ENOBUFS;
		goto fail;
	}

	sav->lft_c->allocations = 0;
	sav->lft_c->bytes = 0;
	sav->lft_c->addtime = time_second;
	sav->lft_c->usetime = 0;

	/* lifetimes for HARD and SOFT */
    {
	const struct sadb_lifetime *lft0;

	lft0 = (struct sadb_lifetime *)mhp->ext[SADB_EXT_LIFETIME_HARD];
	if (lft0 != NULL) {
		if (mhp->extlen[SADB_EXT_LIFETIME_HARD] < sizeof(*lft0)) {
			error = EINVAL;
			goto fail;
		}
		sav->lft_h = key_dup_lifemsg(lft0, M_IPSEC_MISC);
		if (sav->lft_h == NULL) {
			ipseclog((LOG_DEBUG, "%s: No more memory.\n",__func__));
			error = ENOBUFS;
			goto fail;
		}
		/* to be initialize ? */
	}

	lft0 = (struct sadb_lifetime *)mhp->ext[SADB_EXT_LIFETIME_SOFT];
	if (lft0 != NULL) {
		if (mhp->extlen[SADB_EXT_LIFETIME_SOFT] < sizeof(*lft0)) {
			error = EINVAL;
			goto fail;
		}
		sav->lft_s = key_dup_lifemsg(lft0, M_IPSEC_MISC);
		if (sav->lft_s == NULL) {
			ipseclog((LOG_DEBUG, "%s: No more memory.\n",__func__));
			error = ENOBUFS;
			goto fail;
		}
		/* to be initialize ? */
	}
    }

	return 0;

 fail:
	/* initialization */
	key_cleansav(sav);

	return error;
}

/*
 * validation with a secasvar entry, and set SADB_SATYPE_MATURE.
 * OUT:	0:	valid
 *	other:	errno
 */
static int
key_mature(struct secasvar *sav)
{
	int error;

	/* check SPI value */
	switch (sav->sah->saidx.proto) {
	case IPPROTO_ESP:
	case IPPROTO_AH:
		/*
		 * RFC 4302, 2.4. Security Parameters Index (SPI), SPI values
		 * 1-255 reserved by IANA for future use,
		 * 0 for implementation specific, local use.
		 */
		if (ntohl(sav->spi) <= 255) {
			ipseclog((LOG_DEBUG, "%s: illegal range of SPI %u.\n",
			    __func__, (u_int32_t)ntohl(sav->spi)));
			return EINVAL;
		}
		break;
	}

	/* check satype */
	switch (sav->sah->saidx.proto) {
	case IPPROTO_ESP:
		/* check flags */
		if ((sav->flags & (SADB_X_EXT_OLD|SADB_X_EXT_DERIV)) ==
		    (SADB_X_EXT_OLD|SADB_X_EXT_DERIV)) {
			ipseclog((LOG_DEBUG, "%s: invalid flag (derived) "
				"given to old-esp.\n", __func__));
			return EINVAL;
		}
		error = xform_init(sav, XF_ESP);
		break;
	case IPPROTO_AH:
		/* check flags */
		if (sav->flags & SADB_X_EXT_DERIV) {
			ipseclog((LOG_DEBUG, "%s: invalid flag (derived) "
				"given to AH SA.\n", __func__));
			return EINVAL;
		}
		if (sav->alg_enc != SADB_EALG_NONE) {
			ipseclog((LOG_DEBUG, "%s: protocol and algorithm "
				"mismated.\n", __func__));
			return(EINVAL);
		}
		error = xform_init(sav, XF_AH);
		break;
	case IPPROTO_IPCOMP:
		if (sav->alg_auth != SADB_AALG_NONE) {
			ipseclog((LOG_DEBUG, "%s: protocol and algorithm "
				"mismated.\n", __func__));
			return(EINVAL);
		}
		if ((sav->flags & SADB_X_EXT_RAWCPI) == 0
		 && ntohl(sav->spi) >= 0x10000) {
			ipseclog((LOG_DEBUG, "%s: invalid cpi for IPComp.\n",
				__func__));
			return(EINVAL);
		}
		error = xform_init(sav, XF_IPCOMP);
		break;
	case IPPROTO_TCP:
		if (sav->alg_enc != SADB_EALG_NONE) {
			ipseclog((LOG_DEBUG, "%s: protocol and algorithm "
				"mismated.\n", __func__));
			return(EINVAL);
		}
		error = xform_init(sav, XF_TCPSIGNATURE);
		break;
	default:
		ipseclog((LOG_DEBUG, "%s: Invalid satype.\n", __func__));
		error = EPROTONOSUPPORT;
		break;
	}
	if (error == 0) {
		SAHTREE_LOCK();
		key_sa_chgstate(sav, SADB_SASTATE_MATURE);
		SAHTREE_UNLOCK();
	}
	return (error);
}

/*
 * subroutine for SADB_GET and SADB_DUMP.
 */
static struct mbuf *
key_setdumpsa(struct secasvar *sav, u_int8_t type, u_int8_t satype,
    u_int32_t seq, u_int32_t pid)
{
	struct mbuf *result = NULL, *tres = NULL, *m;
	int i;
	int dumporder[] = {
		SADB_EXT_SA, SADB_X_EXT_SA2,
		SADB_EXT_LIFETIME_HARD, SADB_EXT_LIFETIME_SOFT,
		SADB_EXT_LIFETIME_CURRENT, SADB_EXT_ADDRESS_SRC,
		SADB_EXT_ADDRESS_DST, SADB_EXT_ADDRESS_PROXY, SADB_EXT_KEY_AUTH,
		SADB_EXT_KEY_ENCRYPT, SADB_EXT_IDENTITY_SRC,
		SADB_EXT_IDENTITY_DST, SADB_EXT_SENSITIVITY,
#ifdef IPSEC_NAT_T
		SADB_X_EXT_NAT_T_TYPE,
		SADB_X_EXT_NAT_T_SPORT, SADB_X_EXT_NAT_T_DPORT,
		SADB_X_EXT_NAT_T_OAI, SADB_X_EXT_NAT_T_OAR,
		SADB_X_EXT_NAT_T_FRAG,
#endif
	};

	m = key_setsadbmsg(type, 0, satype, seq, pid, sav->refcnt);
	if (m == NULL)
		goto fail;
	result = m;

	for (i = sizeof(dumporder)/sizeof(dumporder[0]) - 1; i >= 0; i--) {
		m = NULL;
		switch (dumporder[i]) {
		case SADB_EXT_SA:
			m = key_setsadbsa(sav);
			if (!m)
				goto fail;
			break;

		case SADB_X_EXT_SA2:
			m = key_setsadbxsa2(sav->sah->saidx.mode,
					sav->replay ? sav->replay->count : 0,
					sav->sah->saidx.reqid);
			if (!m)
				goto fail;
			break;

		case SADB_EXT_ADDRESS_SRC:
			m = key_setsadbaddr(SADB_EXT_ADDRESS_SRC,
			    &sav->sah->saidx.src.sa,
			    FULLMASK, IPSEC_ULPROTO_ANY);
			if (!m)
				goto fail;
			break;

		case SADB_EXT_ADDRESS_DST:
			m = key_setsadbaddr(SADB_EXT_ADDRESS_DST,
			    &sav->sah->saidx.dst.sa,
			    FULLMASK, IPSEC_ULPROTO_ANY);
			if (!m)
				goto fail;
			break;

		case SADB_EXT_KEY_AUTH:
			if (!sav->key_auth)
				continue;
			m = key_setkey(sav->key_auth, SADB_EXT_KEY_AUTH);
			if (!m)
				goto fail;
			break;

		case SADB_EXT_KEY_ENCRYPT:
			if (!sav->key_enc)
				continue;
			m = key_setkey(sav->key_enc, SADB_EXT_KEY_ENCRYPT);
			if (!m)
				goto fail;
			break;

		case SADB_EXT_LIFETIME_CURRENT:
			if (!sav->lft_c)
				continue;
			m = key_setlifetime(sav->lft_c, 
					    SADB_EXT_LIFETIME_CURRENT);
			if (!m)
				goto fail;
			break;

		case SADB_EXT_LIFETIME_HARD:
			if (!sav->lft_h)
				continue;
			m = key_setlifetime(sav->lft_h, 
					    SADB_EXT_LIFETIME_HARD);
			if (!m)
				goto fail;
			break;

		case SADB_EXT_LIFETIME_SOFT:
			if (!sav->lft_s)
				continue;
			m = key_setlifetime(sav->lft_s, 
					    SADB_EXT_LIFETIME_SOFT);

			if (!m)
				goto fail;
			break;

#ifdef IPSEC_NAT_T
		case SADB_X_EXT_NAT_T_TYPE:
			m = key_setsadbxtype(sav->natt_type);
			if (!m)
				goto fail;
			break;
		
		case SADB_X_EXT_NAT_T_DPORT:
			m = key_setsadbxport(
			    KEY_PORTFROMSADDR(&sav->sah->saidx.dst),
			    SADB_X_EXT_NAT_T_DPORT);
			if (!m)
				goto fail;
			break;

		case SADB_X_EXT_NAT_T_SPORT:
			m = key_setsadbxport(
			    KEY_PORTFROMSADDR(&sav->sah->saidx.src),
			    SADB_X_EXT_NAT_T_SPORT);
			if (!m)
				goto fail;
			break;

		case SADB_X_EXT_NAT_T_OAI:
		case SADB_X_EXT_NAT_T_OAR:
		case SADB_X_EXT_NAT_T_FRAG:
			/* We do not (yet) support those. */
			continue;
#endif

		case SADB_EXT_ADDRESS_PROXY:
		case SADB_EXT_IDENTITY_SRC:
		case SADB_EXT_IDENTITY_DST:
			/* XXX: should we brought from SPD ? */
		case SADB_EXT_SENSITIVITY:
		default:
			continue;
		}

		if (!m)
			goto fail;
		if (tres)
			m_cat(m, tres);
		tres = m;
		  
	}

	m_cat(result, tres);
	if (result->m_len < sizeof(struct sadb_msg)) {
		result = m_pullup(result, sizeof(struct sadb_msg));
		if (result == NULL)
			goto fail;
	}

	result->m_pkthdr.len = 0;
	for (m = result; m; m = m->m_next)
		result->m_pkthdr.len += m->m_len;

	mtod(result, struct sadb_msg *)->sadb_msg_len =
	    PFKEY_UNIT64(result->m_pkthdr.len);

	return result;

fail:
	m_freem(result);
	m_freem(tres);
	return NULL;
}

/*
 * set data into sadb_msg.
 */
static struct mbuf *
key_setsadbmsg(u_int8_t type, u_int16_t tlen, u_int8_t satype, u_int32_t seq,
    pid_t pid, u_int16_t reserved)
{
	struct mbuf *m;
	struct sadb_msg *p;
	int len;

	len = PFKEY_ALIGN8(sizeof(struct sadb_msg));
	if (len > MCLBYTES)
		return NULL;
	MGETHDR(m, M_NOWAIT, MT_DATA);
	if (m && len > MHLEN) {
		MCLGET(m, M_NOWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_freem(m);
			m = NULL;
		}
	}
	if (!m)
		return NULL;
	m->m_pkthdr.len = m->m_len = len;
	m->m_next = NULL;

	p = mtod(m, struct sadb_msg *);

	bzero(p, len);
	p->sadb_msg_version = PF_KEY_V2;
	p->sadb_msg_type = type;
	p->sadb_msg_errno = 0;
	p->sadb_msg_satype = satype;
	p->sadb_msg_len = PFKEY_UNIT64(tlen);
	p->sadb_msg_reserved = reserved;
	p->sadb_msg_seq = seq;
	p->sadb_msg_pid = (u_int32_t)pid;

	return m;
}

/*
 * copy secasvar data into sadb_address.
 */
static struct mbuf *
key_setsadbsa(sav)
	struct secasvar *sav;
{
	struct mbuf *m;
	struct sadb_sa *p;
	int len;

	len = PFKEY_ALIGN8(sizeof(struct sadb_sa));
	m = m_get2(len, M_NOWAIT, MT_DATA, 0);
	if (m == NULL)
		return (NULL);
	m_align(m, len);
	m->m_len = len;
	p = mtod(m, struct sadb_sa *);
	bzero(p, len);
	p->sadb_sa_len = PFKEY_UNIT64(len);
	p->sadb_sa_exttype = SADB_EXT_SA;
	p->sadb_sa_spi = sav->spi;
	p->sadb_sa_replay = (sav->replay != NULL ? sav->replay->wsize : 0);
	p->sadb_sa_state = sav->state;
	p->sadb_sa_auth = sav->alg_auth;
	p->sadb_sa_encrypt = sav->alg_enc;
	p->sadb_sa_flags = sav->flags;

	return m;
}

/*
 * set data into sadb_address.
 */
static struct mbuf *
key_setsadbaddr(u_int16_t exttype, const struct sockaddr *saddr, u_int8_t prefixlen, u_int16_t ul_proto)
{
	struct mbuf *m;
	struct sadb_address *p;
	size_t len;

	len = PFKEY_ALIGN8(sizeof(struct sadb_address)) +
	    PFKEY_ALIGN8(saddr->sa_len);
	m = m_get2(len, M_NOWAIT, MT_DATA, 0);
	if (m == NULL)
		return (NULL);
	m_align(m, len);
	m->m_len = len;
	p = mtod(m, struct sadb_address *);

	bzero(p, len);
	p->sadb_address_len = PFKEY_UNIT64(len);
	p->sadb_address_exttype = exttype;
	p->sadb_address_proto = ul_proto;
	if (prefixlen == FULLMASK) {
		switch (saddr->sa_family) {
		case AF_INET:
			prefixlen = sizeof(struct in_addr) << 3;
			break;
		case AF_INET6:
			prefixlen = sizeof(struct in6_addr) << 3;
			break;
		default:
			; /*XXX*/
		}
	}
	p->sadb_address_prefixlen = prefixlen;
	p->sadb_address_reserved = 0;

	bcopy(saddr,
	    mtod(m, caddr_t) + PFKEY_ALIGN8(sizeof(struct sadb_address)),
	    saddr->sa_len);

	return m;
}

/*
 * set data into sadb_x_sa2.
 */
static struct mbuf *
key_setsadbxsa2(u_int8_t mode, u_int32_t seq, u_int32_t reqid)
{
	struct mbuf *m;
	struct sadb_x_sa2 *p;
	size_t len;

	len = PFKEY_ALIGN8(sizeof(struct sadb_x_sa2));
	m = m_get2(len, M_NOWAIT, MT_DATA, 0);
	if (m == NULL)
		return (NULL);
	m_align(m, len);
	m->m_len = len;
	p = mtod(m, struct sadb_x_sa2 *);

	bzero(p, len);
	p->sadb_x_sa2_len = PFKEY_UNIT64(len);
	p->sadb_x_sa2_exttype = SADB_X_EXT_SA2;
	p->sadb_x_sa2_mode = mode;
	p->sadb_x_sa2_reserved1 = 0;
	p->sadb_x_sa2_reserved2 = 0;
	p->sadb_x_sa2_sequence = seq;
	p->sadb_x_sa2_reqid = reqid;

	return m;
}

#ifdef IPSEC_NAT_T
/*
 * Set a type in sadb_x_nat_t_type.
 */
static struct mbuf *
key_setsadbxtype(u_int16_t type)
{
	struct mbuf *m;
	size_t len;
	struct sadb_x_nat_t_type *p;

	len = PFKEY_ALIGN8(sizeof(struct sadb_x_nat_t_type));

	m = m_get2(len, M_NOWAIT, MT_DATA, 0);
	if (m == NULL)
		return (NULL);
	m_align(m, len);
	m->m_len = len;
	p = mtod(m, struct sadb_x_nat_t_type *);

	bzero(p, len);
	p->sadb_x_nat_t_type_len = PFKEY_UNIT64(len);
	p->sadb_x_nat_t_type_exttype = SADB_X_EXT_NAT_T_TYPE;
	p->sadb_x_nat_t_type_type = type;

	return (m);
}
/*
 * Set a port in sadb_x_nat_t_port.
 * In contrast to default RFC 2367 behaviour, port is in network byte order.
 */
static struct mbuf *
key_setsadbxport(u_int16_t port, u_int16_t type)
{
	struct mbuf *m;
	size_t len;
	struct sadb_x_nat_t_port *p;

	len = PFKEY_ALIGN8(sizeof(struct sadb_x_nat_t_port));

	m = m_get2(len, M_NOWAIT, MT_DATA, 0);
	if (m == NULL)
		return (NULL);
	m_align(m, len);
	m->m_len = len;
	p = mtod(m, struct sadb_x_nat_t_port *);

	bzero(p, len);
	p->sadb_x_nat_t_port_len = PFKEY_UNIT64(len);
	p->sadb_x_nat_t_port_exttype = type;
	p->sadb_x_nat_t_port_port = port;

	return (m);
}

/* 
 * Get port from sockaddr. Port is in network byte order.
 */
u_int16_t 
key_portfromsaddr(struct sockaddr *sa)
{

	switch (sa->sa_family) {
#ifdef INET
	case AF_INET:
		return ((struct sockaddr_in *)sa)->sin_port;
#endif
#ifdef INET6
	case AF_INET6:
		return ((struct sockaddr_in6 *)sa)->sin6_port;
#endif
	}
	KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
		printf("DP %s unexpected address family %d\n",
			__func__, sa->sa_family));
	return (0);
}
#endif /* IPSEC_NAT_T */

/*
 * Set port in struct sockaddr. Port is in network byte order.
 */
static void
key_porttosaddr(struct sockaddr *sa, u_int16_t port)
{

	switch (sa->sa_family) {
#ifdef INET
	case AF_INET:
		((struct sockaddr_in *)sa)->sin_port = port;
		break;
#endif
#ifdef INET6
	case AF_INET6:
		((struct sockaddr_in6 *)sa)->sin6_port = port;
		break;
#endif
	default:
		ipseclog((LOG_DEBUG, "%s: unexpected address family %d.\n",
			__func__, sa->sa_family));
		break;
	}
}

/*
 * set data into sadb_x_policy
 */
static struct mbuf *
key_setsadbxpolicy(u_int16_t type, u_int8_t dir, u_int32_t id)
{
	struct mbuf *m;
	struct sadb_x_policy *p;
	size_t len;

	len = PFKEY_ALIGN8(sizeof(struct sadb_x_policy));
	m = m_get2(len, M_NOWAIT, MT_DATA, 0);
	if (m == NULL)
		return (NULL);
	m_align(m, len);
	m->m_len = len;
	p = mtod(m, struct sadb_x_policy *);

	bzero(p, len);
	p->sadb_x_policy_len = PFKEY_UNIT64(len);
	p->sadb_x_policy_exttype = SADB_X_EXT_POLICY;
	p->sadb_x_policy_type = type;
	p->sadb_x_policy_dir = dir;
	p->sadb_x_policy_id = id;

	return m;
}

/* %%% utilities */
/* Take a key message (sadb_key) from the socket and turn it into one
 * of the kernel's key structures (seckey).
 *
 * IN: pointer to the src
 * OUT: NULL no more memory
 */
struct seckey *
key_dup_keymsg(const struct sadb_key *src, u_int len,
	       struct malloc_type *type)
{
	struct seckey *dst;
	dst = (struct seckey *)malloc(sizeof(struct seckey), type, M_NOWAIT);
	if (dst != NULL) {
		dst->bits = src->sadb_key_bits;
		dst->key_data = (char *)malloc(len, type, M_NOWAIT);
		if (dst->key_data != NULL) {
			bcopy((const char *)src + sizeof(struct sadb_key), 
			      dst->key_data, len);
		} else {
			ipseclog((LOG_DEBUG, "%s: No more memory.\n", 
				  __func__));
			free(dst, type);
			dst = NULL;
		}
	} else {
		ipseclog((LOG_DEBUG, "%s: No more memory.\n", 
			  __func__));

	}
	return dst;
}

/* Take a lifetime message (sadb_lifetime) passed in on a socket and
 * turn it into one of the kernel's lifetime structures (seclifetime).
 *
 * IN: pointer to the destination, source and malloc type
 * OUT: NULL, no more memory
 */

static struct seclifetime *
key_dup_lifemsg(const struct sadb_lifetime *src,
		 struct malloc_type *type)
{
	struct seclifetime *dst = NULL;

	dst = (struct seclifetime *)malloc(sizeof(struct seclifetime), 
					   type, M_NOWAIT);
	if (dst == NULL) {
		/* XXX counter */
		ipseclog((LOG_DEBUG, "%s: No more memory.\n", __func__));
	} else {
		dst->allocations = src->sadb_lifetime_allocations;
		dst->bytes = src->sadb_lifetime_bytes;
		dst->addtime = src->sadb_lifetime_addtime;
		dst->usetime = src->sadb_lifetime_usetime;
	}
	return dst;
}

/* compare my own address
 * OUT:	1: true, i.e. my address.
 *	0: false
 */
int
key_ismyaddr(sa)
	struct sockaddr *sa;
{
#ifdef INET
	struct sockaddr_in *sin;
	struct in_ifaddr *ia;
#endif

	IPSEC_ASSERT(sa != NULL, ("null sockaddr"));

	switch (sa->sa_family) {
#ifdef INET
	case AF_INET:
		sin = (struct sockaddr_in *)sa;
		IN_IFADDR_RLOCK();
		TAILQ_FOREACH(ia, &V_in_ifaddrhead, ia_link)
		{
			if (sin->sin_family == ia->ia_addr.sin_family &&
			    sin->sin_len == ia->ia_addr.sin_len &&
			    sin->sin_addr.s_addr == ia->ia_addr.sin_addr.s_addr)
			{
				IN_IFADDR_RUNLOCK();
				return 1;
			}
		}
		IN_IFADDR_RUNLOCK();
		break;
#endif
#ifdef INET6
	case AF_INET6:
		return key_ismyaddr6((struct sockaddr_in6 *)sa);
#endif
	}

	return 0;
}

#ifdef INET6
/*
 * compare my own address for IPv6.
 * 1: ours
 * 0: other
 * NOTE: derived ip6_input() in KAME. This is necessary to modify more.
 */
#include <netinet6/in6_var.h>

static int
key_ismyaddr6(sin6)
	struct sockaddr_in6 *sin6;
{
	struct in6_ifaddr *ia;
#if 0
	struct in6_multi *in6m;
#endif

	IN6_IFADDR_RLOCK();
	TAILQ_FOREACH(ia, &V_in6_ifaddrhead, ia_link) {
		if (key_sockaddrcmp((struct sockaddr *)&sin6,
		    (struct sockaddr *)&ia->ia_addr, 0) == 0) {
			IN6_IFADDR_RUNLOCK();
			return 1;
		}

#if 0
		/*
		 * XXX Multicast
		 * XXX why do we care about multlicast here while we don't care
		 * about IPv4 multicast??
		 * XXX scope
		 */
		in6m = NULL;
		IN6_LOOKUP_MULTI(sin6->sin6_addr, ia->ia_ifp, in6m);
		if (in6m) {
			IN6_IFADDR_RUNLOCK();
			return 1;
		}
#endif
	}
	IN6_IFADDR_RUNLOCK();

	/* loopback, just for safety */
	if (IN6_IS_ADDR_LOOPBACK(&sin6->sin6_addr))
		return 1;

	return 0;
}
#endif /*INET6*/

/*
 * compare two secasindex structure.
 * flag can specify to compare 2 saidxes.
 * compare two secasindex structure without both mode and reqid.
 * don't compare port.
 * IN:  
 *      saidx0: source, it can be in SAD.
 *      saidx1: object.
 * OUT: 
 *      1 : equal
 *      0 : not equal
 */
static int
key_cmpsaidx(
	const struct secasindex *saidx0,
	const struct secasindex *saidx1,
	int flag)
{
	int chkport = 0;

	/* sanity */
	if (saidx0 == NULL && saidx1 == NULL)
		return 1;

	if (saidx0 == NULL || saidx1 == NULL)
		return 0;

	if (saidx0->proto != saidx1->proto)
		return 0;

	if (flag == CMP_EXACTLY) {
		if (saidx0->mode != saidx1->mode)
			return 0;
		if (saidx0->reqid != saidx1->reqid)
			return 0;
		if (bcmp(&saidx0->src, &saidx1->src, saidx0->src.sa.sa_len) != 0 ||
		    bcmp(&saidx0->dst, &saidx1->dst, saidx0->dst.sa.sa_len) != 0)
			return 0;
	} else {

		/* CMP_MODE_REQID, CMP_REQID, CMP_HEAD */
		if (flag == CMP_MODE_REQID
		  ||flag == CMP_REQID) {
			/*
			 * If reqid of SPD is non-zero, unique SA is required.
			 * The result must be of same reqid in this case.
			 */
			if (saidx1->reqid != 0 && saidx0->reqid != saidx1->reqid)
				return 0;
		}

		if (flag == CMP_MODE_REQID) {
			if (saidx0->mode != IPSEC_MODE_ANY
			 && saidx0->mode != saidx1->mode)
				return 0;
		}

#ifdef IPSEC_NAT_T
		/*
		 * If NAT-T is enabled, check ports for tunnel mode.
		 * Do not check ports if they are set to zero in the SPD.
		 * Also do not do it for native transport mode, as there
		 * is no port information available in the SP.
		 */
		if ((saidx1->mode == IPSEC_MODE_TUNNEL ||
		     (saidx1->mode == IPSEC_MODE_TRANSPORT &&
		      saidx1->proto == IPPROTO_ESP)) &&
		    saidx1->src.sa.sa_family == AF_INET &&
		    saidx1->dst.sa.sa_family == AF_INET &&
		    ((const struct sockaddr_in *)(&saidx1->src))->sin_port &&
		    ((const struct sockaddr_in *)(&saidx1->dst))->sin_port)
			chkport = 1;
#endif /* IPSEC_NAT_T */

		if (key_sockaddrcmp(&saidx0->src.sa, &saidx1->src.sa, chkport) != 0) {
			return 0;
		}
		if (key_sockaddrcmp(&saidx0->dst.sa, &saidx1->dst.sa, chkport) != 0) {
			return 0;
		}
	}

	return 1;
}

/*
 * compare two secindex structure exactly.
 * IN:
 *	spidx0: source, it is often in SPD.
 *	spidx1: object, it is often from PFKEY message.
 * OUT:
 *	1 : equal
 *	0 : not equal
 */
static int
key_cmpspidx_exactly(
	struct secpolicyindex *spidx0,
	struct secpolicyindex *spidx1)
{
	/* sanity */
	if (spidx0 == NULL && spidx1 == NULL)
		return 1;

	if (spidx0 == NULL || spidx1 == NULL)
		return 0;

	if (spidx0->prefs != spidx1->prefs
	 || spidx0->prefd != spidx1->prefd
	 || spidx0->ul_proto != spidx1->ul_proto)
		return 0;

	return key_sockaddrcmp(&spidx0->src.sa, &spidx1->src.sa, 1) == 0 &&
	       key_sockaddrcmp(&spidx0->dst.sa, &spidx1->dst.sa, 1) == 0;
}

/*
 * compare two secindex structure with mask.
 * IN:
 *	spidx0: source, it is often in SPD.
 *	spidx1: object, it is often from IP header.
 * OUT:
 *	1 : equal
 *	0 : not equal
 */
static int
key_cmpspidx_withmask(
	struct secpolicyindex *spidx0,
	struct secpolicyindex *spidx1)
{
	/* sanity */
	if (spidx0 == NULL && spidx1 == NULL)
		return 1;

	if (spidx0 == NULL || spidx1 == NULL)
		return 0;

	if (spidx0->src.sa.sa_family != spidx1->src.sa.sa_family ||
	    spidx0->dst.sa.sa_family != spidx1->dst.sa.sa_family ||
	    spidx0->src.sa.sa_len != spidx1->src.sa.sa_len ||
	    spidx0->dst.sa.sa_len != spidx1->dst.sa.sa_len)
		return 0;

	/* if spidx.ul_proto == IPSEC_ULPROTO_ANY, ignore. */
	if (spidx0->ul_proto != (u_int16_t)IPSEC_ULPROTO_ANY
	 && spidx0->ul_proto != spidx1->ul_proto)
		return 0;

	switch (spidx0->src.sa.sa_family) {
	case AF_INET:
		if (spidx0->src.sin.sin_port != IPSEC_PORT_ANY
		 && spidx0->src.sin.sin_port != spidx1->src.sin.sin_port)
			return 0;
		if (!key_bbcmp(&spidx0->src.sin.sin_addr,
		    &spidx1->src.sin.sin_addr, spidx0->prefs))
			return 0;
		break;
	case AF_INET6:
		if (spidx0->src.sin6.sin6_port != IPSEC_PORT_ANY
		 && spidx0->src.sin6.sin6_port != spidx1->src.sin6.sin6_port)
			return 0;
		/*
		 * scope_id check. if sin6_scope_id is 0, we regard it
		 * as a wildcard scope, which matches any scope zone ID. 
		 */
		if (spidx0->src.sin6.sin6_scope_id &&
		    spidx1->src.sin6.sin6_scope_id &&
		    spidx0->src.sin6.sin6_scope_id != spidx1->src.sin6.sin6_scope_id)
			return 0;
		if (!key_bbcmp(&spidx0->src.sin6.sin6_addr,
		    &spidx1->src.sin6.sin6_addr, spidx0->prefs))
			return 0;
		break;
	default:
		/* XXX */
		if (bcmp(&spidx0->src, &spidx1->src, spidx0->src.sa.sa_len) != 0)
			return 0;
		break;
	}

	switch (spidx0->dst.sa.sa_family) {
	case AF_INET:
		if (spidx0->dst.sin.sin_port != IPSEC_PORT_ANY
		 && spidx0->dst.sin.sin_port != spidx1->dst.sin.sin_port)
			return 0;
		if (!key_bbcmp(&spidx0->dst.sin.sin_addr,
		    &spidx1->dst.sin.sin_addr, spidx0->prefd))
			return 0;
		break;
	case AF_INET6:
		if (spidx0->dst.sin6.sin6_port != IPSEC_PORT_ANY
		 && spidx0->dst.sin6.sin6_port != spidx1->dst.sin6.sin6_port)
			return 0;
		/*
		 * scope_id check. if sin6_scope_id is 0, we regard it
		 * as a wildcard scope, which matches any scope zone ID. 
		 */
		if (spidx0->dst.sin6.sin6_scope_id &&
		    spidx1->dst.sin6.sin6_scope_id &&
		    spidx0->dst.sin6.sin6_scope_id != spidx1->dst.sin6.sin6_scope_id)
			return 0;
		if (!key_bbcmp(&spidx0->dst.sin6.sin6_addr,
		    &spidx1->dst.sin6.sin6_addr, spidx0->prefd))
			return 0;
		break;
	default:
		/* XXX */
		if (bcmp(&spidx0->dst, &spidx1->dst, spidx0->dst.sa.sa_len) != 0)
			return 0;
		break;
	}

	/* XXX Do we check other field ?  e.g. flowinfo */

	return 1;
}

/* returns 0 on match */
static int
key_sockaddrcmp(
	const struct sockaddr *sa1,
	const struct sockaddr *sa2,
	int port)
{
#ifdef satosin
#undef satosin
#endif
#define satosin(s) ((const struct sockaddr_in *)s)
#ifdef satosin6
#undef satosin6
#endif
#define satosin6(s) ((const struct sockaddr_in6 *)s)
	if (sa1->sa_family != sa2->sa_family || sa1->sa_len != sa2->sa_len)
		return 1;

	switch (sa1->sa_family) {
	case AF_INET:
		if (sa1->sa_len != sizeof(struct sockaddr_in))
			return 1;
		if (satosin(sa1)->sin_addr.s_addr !=
		    satosin(sa2)->sin_addr.s_addr) {
			return 1;
		}
		if (port && satosin(sa1)->sin_port != satosin(sa2)->sin_port)
			return 1;
		break;
	case AF_INET6:
		if (sa1->sa_len != sizeof(struct sockaddr_in6))
			return 1;	/*EINVAL*/
		if (satosin6(sa1)->sin6_scope_id !=
		    satosin6(sa2)->sin6_scope_id) {
			return 1;
		}
		if (!IN6_ARE_ADDR_EQUAL(&satosin6(sa1)->sin6_addr,
		    &satosin6(sa2)->sin6_addr)) {
			return 1;
		}
		if (port &&
		    satosin6(sa1)->sin6_port != satosin6(sa2)->sin6_port) {
			return 1;
		}
		break;
	default:
		if (bcmp(sa1, sa2, sa1->sa_len) != 0)
			return 1;
		break;
	}

	return 0;
#undef satosin
#undef satosin6
}

/*
 * compare two buffers with mask.
 * IN:
 *	addr1: source
 *	addr2: object
 *	bits:  Number of bits to compare
 * OUT:
 *	1 : equal
 *	0 : not equal
 */
static int
key_bbcmp(const void *a1, const void *a2, u_int bits)
{
	const unsigned char *p1 = a1;
	const unsigned char *p2 = a2;

	/* XXX: This could be considerably faster if we compare a word
	 * at a time, but it is complicated on LSB Endian machines */

	/* Handle null pointers */
	if (p1 == NULL || p2 == NULL)
		return (p1 == p2);

	while (bits >= 8) {
		if (*p1++ != *p2++)
			return 0;
		bits -= 8;
	}

	if (bits > 0) {
		u_int8_t mask = ~((1<<(8-bits))-1);
		if ((*p1 & mask) != (*p2 & mask))
			return 0;
	}
	return 1;	/* Match! */
}

static void
key_flush_spd(time_t now)
{
	static u_int16_t sptree_scangen = 0;
	u_int16_t gen = sptree_scangen++;
	struct secpolicy *sp;
	u_int dir;

	/* SPD */
	for (dir = 0; dir < IPSEC_DIR_MAX; dir++) {
restart:
		SPTREE_LOCK();
		LIST_FOREACH(sp, &V_sptree[dir], chain) {
			if (sp->scangen == gen)		/* previously handled */
				continue;
			sp->scangen = gen;
			if (sp->state == IPSEC_SPSTATE_DEAD &&
			    sp->refcnt == 1) {
				/*
				 * Ensure that we only decrease refcnt once,
				 * when we're the last consumer.
				 * Directly call SP_DELREF/key_delsp instead
				 * of KEY_FREESP to avoid unlocking/relocking
				 * SPTREE_LOCK before key_delsp: may refcnt
				 * be increased again during that time ?
				 * NB: also clean entries created by
				 * key_spdflush
				 */
				SP_DELREF(sp);
				key_delsp(sp);
				SPTREE_UNLOCK();
				goto restart;
			}
			if (sp->lifetime == 0 && sp->validtime == 0)
				continue;
			if ((sp->lifetime && now - sp->created > sp->lifetime)
			 || (sp->validtime && now - sp->lastused > sp->validtime)) {
				sp->state = IPSEC_SPSTATE_DEAD;
				SPTREE_UNLOCK();
				key_spdexpire(sp);
				goto restart;
			}
		}
		SPTREE_UNLOCK();
	}
}

static void
key_flush_sad(time_t now)
{
	struct secashead *sah, *nextsah;
	struct secasvar *sav, *nextsav;

	/* SAD */
	SAHTREE_LOCK();
	LIST_FOREACH_SAFE(sah, &V_sahtree, chain, nextsah) {
		/* if sah has been dead, then delete it and process next sah. */
		if (sah->state == SADB_SASTATE_DEAD) {
			key_delsah(sah);
			continue;
		}

		/* if LARVAL entry doesn't become MATURE, delete it. */
		LIST_FOREACH_SAFE(sav, &sah->savtree[SADB_SASTATE_LARVAL], chain, nextsav) {
			/* Need to also check refcnt for a larval SA ??? */
			if (now - sav->created > V_key_larval_lifetime)
				KEY_FREESAV(&sav);
		}

		/*
		 * check MATURE entry to start to send expire message
		 * whether or not.
		 */
		LIST_FOREACH_SAFE(sav, &sah->savtree[SADB_SASTATE_MATURE], chain, nextsav) {
			/* we don't need to check. */
			if (sav->lft_s == NULL)
				continue;

			/* sanity check */
			if (sav->lft_c == NULL) {
				ipseclog((LOG_DEBUG,"%s: there is no CURRENT "
					"time, why?\n", __func__));
				continue;
			}

			/* check SOFT lifetime */
			if (sav->lft_s->addtime != 0 &&
			    now - sav->created > sav->lft_s->addtime) {
				key_sa_chgstate(sav, SADB_SASTATE_DYING);
				/* 
				 * Actually, only send expire message if
				 * SA has been used, as it was done before,
				 * but should we always send such message,
				 * and let IKE daemon decide if it should be
				 * renegotiated or not ?
				 * XXX expire message will actually NOT be
				 * sent if SA is only used after soft
				 * lifetime has been reached, see below
				 * (DYING state)
				 */
				if (sav->lft_c->usetime != 0)
					key_expire(sav);
			}
			/* check SOFT lifetime by bytes */
			/*
			 * XXX I don't know the way to delete this SA
			 * when new SA is installed.  Caution when it's
			 * installed too big lifetime by time.
			 */
			else if (sav->lft_s->bytes != 0 &&
			    sav->lft_s->bytes < sav->lft_c->bytes) {

				key_sa_chgstate(sav, SADB_SASTATE_DYING);
				/*
				 * XXX If we keep to send expire
				 * message in the status of
				 * DYING. Do remove below code.
				 */
				key_expire(sav);
			}
		}

		/* check DYING entry to change status to DEAD. */
		LIST_FOREACH_SAFE(sav, &sah->savtree[SADB_SASTATE_DYING], chain, nextsav) {
			/* we don't need to check. */
			if (sav->lft_h == NULL)
				continue;

			/* sanity check */
			if (sav->lft_c == NULL) {
				ipseclog((LOG_DEBUG, "%s: there is no CURRENT "
					"time, why?\n", __func__));
				continue;
			}

			if (sav->lft_h->addtime != 0 &&
			    now - sav->created > sav->lft_h->addtime) {
				key_sa_chgstate(sav, SADB_SASTATE_DEAD);
				KEY_FREESAV(&sav);
			}
#if 0	/* XXX Should we keep to send expire message until HARD lifetime ? */
			else if (sav->lft_s != NULL
			      && sav->lft_s->addtime != 0
			      && now - sav->created > sav->lft_s->addtime) {
				/*
				 * XXX: should be checked to be
				 * installed the valid SA.
				 */

				/*
				 * If there is no SA then sending
				 * expire message.
				 */
				key_expire(sav);
			}
#endif
			/* check HARD lifetime by bytes */
			else if (sav->lft_h->bytes != 0 &&
			    sav->lft_h->bytes < sav->lft_c->bytes) {
				key_sa_chgstate(sav, SADB_SASTATE_DEAD);
				KEY_FREESAV(&sav);
			}
		}

		/* delete entry in DEAD */
		LIST_FOREACH_SAFE(sav, &sah->savtree[SADB_SASTATE_DEAD], chain, nextsav) {
			/* sanity check */
			if (sav->state != SADB_SASTATE_DEAD) {
				ipseclog((LOG_DEBUG, "%s: invalid sav->state "
					"(queue: %d SA: %d): kill it anyway\n",
					__func__,
					SADB_SASTATE_DEAD, sav->state));
			}
			/*
			 * do not call key_freesav() here.
			 * sav should already be freed, and sav->refcnt
			 * shows other references to sav
			 * (such as from SPD).
			 */
		}
	}
	SAHTREE_UNLOCK();
}

static void
key_flush_acq(time_t now)
{
	struct secacq *acq, *nextacq;

	/* ACQ tree */
	ACQ_LOCK();
	for (acq = LIST_FIRST(&V_acqtree); acq != NULL; acq = nextacq) {
		nextacq = LIST_NEXT(acq, chain);
		if (now - acq->created > V_key_blockacq_lifetime
		 && __LIST_CHAINED(acq)) {
			LIST_REMOVE(acq, chain);
			free(acq, M_IPSEC_SAQ);
		}
	}
	ACQ_UNLOCK();
}

static void
key_flush_spacq(time_t now)
{
	struct secspacq *acq, *nextacq;

	/* SP ACQ tree */
	SPACQ_LOCK();
	for (acq = LIST_FIRST(&V_spacqtree); acq != NULL; acq = nextacq) {
		nextacq = LIST_NEXT(acq, chain);
		if (now - acq->created > V_key_blockacq_lifetime
		 && __LIST_CHAINED(acq)) {
			LIST_REMOVE(acq, chain);
			free(acq, M_IPSEC_SAQ);
		}
	}
	SPACQ_UNLOCK();
}

/*
 * time handler.
 * scanning SPD and SAD to check status for each entries,
 * and do to remove or to expire.
 * XXX: year 2038 problem may remain.
 */
void
key_timehandler(void)
{
	VNET_ITERATOR_DECL(vnet_iter);
	time_t now = time_second;

	VNET_LIST_RLOCK_NOSLEEP();
	VNET_FOREACH(vnet_iter) {
		CURVNET_SET(vnet_iter);
		key_flush_spd(now);
		key_flush_sad(now);
		key_flush_acq(now);
		key_flush_spacq(now);
		CURVNET_RESTORE();
	}
	VNET_LIST_RUNLOCK_NOSLEEP();

#ifndef IPSEC_DEBUG2
	/* do exchange to tick time !! */
	(void)timeout((void *)key_timehandler, (void *)0, hz);
#endif /* IPSEC_DEBUG2 */
}

u_long
key_random()
{
	u_long value;

	key_randomfill(&value, sizeof(value));
	return value;
}

void
key_randomfill(p, l)
	void *p;
	size_t l;
{
	size_t n;
	u_long v;
	static int warn = 1;

	n = 0;
	n = (size_t)read_random(p, (u_int)l);
	/* last resort */
	while (n < l) {
		v = random();
		bcopy(&v, (u_int8_t *)p + n,
		    l - n < sizeof(v) ? l - n : sizeof(v));
		n += sizeof(v);

		if (warn) {
			printf("WARNING: pseudo-random number generator "
			    "used for IPsec processing\n");
			warn = 0;
		}
	}
}

/*
 * map SADB_SATYPE_* to IPPROTO_*.
 * if satype == SADB_SATYPE then satype is mapped to ~0.
 * OUT:
 *	0: invalid satype.
 */
static u_int16_t
key_satype2proto(u_int8_t satype)
{
	switch (satype) {
	case SADB_SATYPE_UNSPEC:
		return IPSEC_PROTO_ANY;
	case SADB_SATYPE_AH:
		return IPPROTO_AH;
	case SADB_SATYPE_ESP:
		return IPPROTO_ESP;
	case SADB_X_SATYPE_IPCOMP:
		return IPPROTO_IPCOMP;
	case SADB_X_SATYPE_TCPSIGNATURE:
		return IPPROTO_TCP;
	default:
		return 0;
	}
	/* NOTREACHED */
}

/*
 * map IPPROTO_* to SADB_SATYPE_*
 * OUT:
 *	0: invalid protocol type.
 */
static u_int8_t
key_proto2satype(u_int16_t proto)
{
	switch (proto) {
	case IPPROTO_AH:
		return SADB_SATYPE_AH;
	case IPPROTO_ESP:
		return SADB_SATYPE_ESP;
	case IPPROTO_IPCOMP:
		return SADB_X_SATYPE_IPCOMP;
	case IPPROTO_TCP:
		return SADB_X_SATYPE_TCPSIGNATURE;
	default:
		return 0;
	}
	/* NOTREACHED */
}

/* %%% PF_KEY */
/*
 * SADB_GETSPI processing is to receive
 *	<base, (SA2), src address, dst address, (SPI range)>
 * from the IKMPd, to assign a unique spi value, to hang on the INBOUND
 * tree with the status of LARVAL, and send
 *	<base, SA(*), address(SD)>
 * to the IKMPd.
 *
 * IN:	mhp: pointer to the pointer to each header.
 * OUT:	NULL if fail.
 *	other if success, return pointer to the message to send.
 */
static int
key_getspi(so, m, mhp)
	struct socket *so;
	struct mbuf *m;
	const struct sadb_msghdr *mhp;
{
	struct sadb_address *src0, *dst0;
	struct secasindex saidx;
	struct secashead *newsah;
	struct secasvar *newsav;
	u_int8_t proto;
	u_int32_t spi;
	u_int8_t mode;
	u_int32_t reqid;
	int error;

	IPSEC_ASSERT(so != NULL, ("null socket"));
	IPSEC_ASSERT(m != NULL, ("null mbuf"));
	IPSEC_ASSERT(mhp != NULL, ("null msghdr"));
	IPSEC_ASSERT(mhp->msg != NULL, ("null msg"));

	if (mhp->ext[SADB_EXT_ADDRESS_SRC] == NULL ||
	    mhp->ext[SADB_EXT_ADDRESS_DST] == NULL) {
		ipseclog((LOG_DEBUG, "%s: invalid message is passed.\n",
			__func__));
		return key_senderror(so, m, EINVAL);
	}
	if (mhp->extlen[SADB_EXT_ADDRESS_SRC] < sizeof(struct sadb_address) ||
	    mhp->extlen[SADB_EXT_ADDRESS_DST] < sizeof(struct sadb_address)) {
		ipseclog((LOG_DEBUG, "%s: invalid message is passed.\n",
			__func__));
		return key_senderror(so, m, EINVAL);
	}
	if (mhp->ext[SADB_X_EXT_SA2] != NULL) {
		mode = ((struct sadb_x_sa2 *)mhp->ext[SADB_X_EXT_SA2])->sadb_x_sa2_mode;
		reqid = ((struct sadb_x_sa2 *)mhp->ext[SADB_X_EXT_SA2])->sadb_x_sa2_reqid;
	} else {
		mode = IPSEC_MODE_ANY;
		reqid = 0;
	}

	src0 = (struct sadb_address *)(mhp->ext[SADB_EXT_ADDRESS_SRC]);
	dst0 = (struct sadb_address *)(mhp->ext[SADB_EXT_ADDRESS_DST]);

	/* map satype to proto */
	if ((proto = key_satype2proto(mhp->msg->sadb_msg_satype)) == 0) {
		ipseclog((LOG_DEBUG, "%s: invalid satype is passed.\n",
			__func__));
		return key_senderror(so, m, EINVAL);
	}

	/*
	 * Make sure the port numbers are zero.
	 * In case of NAT-T we will update them later if needed.
	 */
	switch (((struct sockaddr *)(src0 + 1))->sa_family) {
	case AF_INET:
		if (((struct sockaddr *)(src0 + 1))->sa_len !=
		    sizeof(struct sockaddr_in))
			return key_senderror(so, m, EINVAL);
		((struct sockaddr_in *)(src0 + 1))->sin_port = 0;
		break;
	case AF_INET6:
		if (((struct sockaddr *)(src0 + 1))->sa_len !=
		    sizeof(struct sockaddr_in6))
			return key_senderror(so, m, EINVAL);
		((struct sockaddr_in6 *)(src0 + 1))->sin6_port = 0;
		break;
	default:
		; /*???*/
	}
	switch (((struct sockaddr *)(dst0 + 1))->sa_family) {
	case AF_INET:
		if (((struct sockaddr *)(dst0 + 1))->sa_len !=
		    sizeof(struct sockaddr_in))
			return key_senderror(so, m, EINVAL);
		((struct sockaddr_in *)(dst0 + 1))->sin_port = 0;
		break;
	case AF_INET6:
		if (((struct sockaddr *)(dst0 + 1))->sa_len !=
		    sizeof(struct sockaddr_in6))
			return key_senderror(so, m, EINVAL);
		((struct sockaddr_in6 *)(dst0 + 1))->sin6_port = 0;
		break;
	default:
		; /*???*/
	}

	/* XXX boundary check against sa_len */
	KEY_SETSECASIDX(proto, mode, reqid, src0 + 1, dst0 + 1, &saidx);

#ifdef IPSEC_NAT_T
	/*
	 * Handle NAT-T info if present.
	 * We made sure the port numbers are zero above, so we do
	 * not have to worry in case we do not update them.
	 */
	if (mhp->ext[SADB_X_EXT_NAT_T_OAI] != NULL)
		ipseclog((LOG_DEBUG, "%s: NAT-T OAi present\n", __func__));
	if (mhp->ext[SADB_X_EXT_NAT_T_OAR] != NULL)
		ipseclog((LOG_DEBUG, "%s: NAT-T OAr present\n", __func__));

	if (mhp->ext[SADB_X_EXT_NAT_T_TYPE] != NULL &&
	    mhp->ext[SADB_X_EXT_NAT_T_SPORT] != NULL &&
	    mhp->ext[SADB_X_EXT_NAT_T_DPORT] != NULL) {
		struct sadb_x_nat_t_type *type;
		struct sadb_x_nat_t_port *sport, *dport;

		if (mhp->extlen[SADB_X_EXT_NAT_T_TYPE] < sizeof(*type) ||
		    mhp->extlen[SADB_X_EXT_NAT_T_SPORT] < sizeof(*sport) ||
		    mhp->extlen[SADB_X_EXT_NAT_T_DPORT] < sizeof(*dport)) {
			ipseclog((LOG_DEBUG, "%s: invalid nat-t message "
			    "passed.\n", __func__));
			return key_senderror(so, m, EINVAL);
		}

		sport = (struct sadb_x_nat_t_port *)
		    mhp->ext[SADB_X_EXT_NAT_T_SPORT];
		dport = (struct sadb_x_nat_t_port *)
		    mhp->ext[SADB_X_EXT_NAT_T_DPORT];

		if (sport)
			KEY_PORTTOSADDR(&saidx.src, sport->sadb_x_nat_t_port_port);
		if (dport)
			KEY_PORTTOSADDR(&saidx.dst, dport->sadb_x_nat_t_port_port);
	}
#endif

	/* SPI allocation */
	spi = key_do_getnewspi((struct sadb_spirange *)mhp->ext[SADB_EXT_SPIRANGE],
	                       &saidx);
	if (spi == 0)
		return key_senderror(so, m, EINVAL);

	/* get a SA index */
	if ((newsah = key_getsah(&saidx)) == NULL) {
		/* create a new SA index */
		if ((newsah = key_newsah(&saidx)) == NULL) {
			ipseclog((LOG_DEBUG, "%s: No more memory.\n",__func__));
			return key_senderror(so, m, ENOBUFS);
		}
	}

	/* get a new SA */
	/* XXX rewrite */
	newsav = KEY_NEWSAV(m, mhp, newsah, &error);
	if (newsav == NULL) {
		/* XXX don't free new SA index allocated in above. */
		return key_senderror(so, m, error);
	}

	/* set spi */
	newsav->spi = htonl(spi);

	/* delete the entry in acqtree */
	if (mhp->msg->sadb_msg_seq != 0) {
		struct secacq *acq;
		if ((acq = key_getacqbyseq(mhp->msg->sadb_msg_seq)) != NULL) {
			/* reset counter in order to deletion by timehandler. */
			acq->created = time_second;
			acq->count = 0;
		}
    	}

    {
	struct mbuf *n, *nn;
	struct sadb_sa *m_sa;
	struct sadb_msg *newmsg;
	int off, len;

	/* create new sadb_msg to reply. */
	len = PFKEY_ALIGN8(sizeof(struct sadb_msg)) +
	    PFKEY_ALIGN8(sizeof(struct sadb_sa));

	MGETHDR(n, M_NOWAIT, MT_DATA);
	if (len > MHLEN) {
		MCLGET(n, M_NOWAIT);
		if ((n->m_flags & M_EXT) == 0) {
			m_freem(n);
			n = NULL;
		}
	}
	if (!n)
		return key_senderror(so, m, ENOBUFS);

	n->m_len = len;
	n->m_next = NULL;
	off = 0;

	m_copydata(m, 0, sizeof(struct sadb_msg), mtod(n, caddr_t) + off);
	off += PFKEY_ALIGN8(sizeof(struct sadb_msg));

	m_sa = (struct sadb_sa *)(mtod(n, caddr_t) + off);
	m_sa->sadb_sa_len = PFKEY_UNIT64(sizeof(struct sadb_sa));
	m_sa->sadb_sa_exttype = SADB_EXT_SA;
	m_sa->sadb_sa_spi = htonl(spi);
	off += PFKEY_ALIGN8(sizeof(struct sadb_sa));

	IPSEC_ASSERT(off == len,
		("length inconsistency (off %u len %u)", off, len));

	n->m_next = key_gather_mbuf(m, mhp, 0, 2, SADB_EXT_ADDRESS_SRC,
	    SADB_EXT_ADDRESS_DST);
	if (!n->m_next) {
		m_freem(n);
		return key_senderror(so, m, ENOBUFS);
	}

	if (n->m_len < sizeof(struct sadb_msg)) {
		n = m_pullup(n, sizeof(struct sadb_msg));
		if (n == NULL)
			return key_sendup_mbuf(so, m, KEY_SENDUP_ONE);
	}

	n->m_pkthdr.len = 0;
	for (nn = n; nn; nn = nn->m_next)
		n->m_pkthdr.len += nn->m_len;

	newmsg = mtod(n, struct sadb_msg *);
	newmsg->sadb_msg_seq = newsav->seq;
	newmsg->sadb_msg_errno = 0;
	newmsg->sadb_msg_len = PFKEY_UNIT64(n->m_pkthdr.len);

	m_freem(m);
	return key_sendup_mbuf(so, n, KEY_SENDUP_ONE);
    }
}

/*
 * allocating new SPI
 * called by key_getspi().
 * OUT:
 *	0:	failure.
 *	others: success.
 */
static u_int32_t
key_do_getnewspi(spirange, saidx)
	struct sadb_spirange *spirange;
	struct secasindex *saidx;
{
	u_int32_t newspi;
	u_int32_t min, max;
	int count = V_key_spi_trycnt;

	/* set spi range to allocate */
	if (spirange != NULL) {
		min = spirange->sadb_spirange_min;
		max = spirange->sadb_spirange_max;
	} else {
		min = V_key_spi_minval;
		max = V_key_spi_maxval;
	}
	/* IPCOMP needs 2-byte SPI */
	if (saidx->proto == IPPROTO_IPCOMP) {
		u_int32_t t;
		if (min >= 0x10000)
			min = 0xffff;
		if (max >= 0x10000)
			max = 0xffff;
		if (min > max) {
			t = min; min = max; max = t;
		}
	}

	if (min == max) {
		if (key_checkspidup(saidx, min) != NULL) {
			ipseclog((LOG_DEBUG, "%s: SPI %u exists already.\n",
				__func__, min));
			return 0;
		}

		count--; /* taking one cost. */
		newspi = min;

	} else {

		/* init SPI */
		newspi = 0;

		/* when requesting to allocate spi ranged */
		while (count--) {
			/* generate pseudo-random SPI value ranged. */
			newspi = min + (key_random() % (max - min + 1));

			if (key_checkspidup(saidx, newspi) == NULL)
				break;
		}

		if (count == 0 || newspi == 0) {
			ipseclog((LOG_DEBUG, "%s: to allocate spi is failed.\n",
				__func__));
			return 0;
		}
	}

	/* statistics */
	keystat.getspi_count =
		(keystat.getspi_count + V_key_spi_trycnt - count) / 2;

	return newspi;
}

/*
 * SADB_UPDATE processing
 * receive
 *   <base, SA, (SA2), (lifetime(HSC),) address(SD), (address(P),)
 *       key(AE), (identity(SD),) (sensitivity)>
 * from the ikmpd, and update a secasvar entry whose status is SADB_SASTATE_LARVAL.
 * and send
 *   <base, SA, (SA2), (lifetime(HSC),) address(SD), (address(P),)
 *       (identity(SD),) (sensitivity)>
 * to the ikmpd.
 *
 * m will always be freed.
 */
static int
key_update(so, m, mhp)
	struct socket *so;
	struct mbuf *m;
	const struct sadb_msghdr *mhp;
{
	struct sadb_sa *sa0;
	struct sadb_address *src0, *dst0;
#ifdef IPSEC_NAT_T
	struct sadb_x_nat_t_type *type;
	struct sadb_x_nat_t_port *sport, *dport;
	struct sadb_address *iaddr, *raddr;
	struct sadb_x_nat_t_frag *frag;
#endif
	struct secasindex saidx;
	struct secashead *sah;
	struct secasvar *sav;
	u_int16_t proto;
	u_int8_t mode;
	u_int32_t reqid;
	int error;

	IPSEC_ASSERT(so != NULL, ("null socket"));
	IPSEC_ASSERT(m != NULL, ("null mbuf"));
	IPSEC_ASSERT(mhp != NULL, ("null msghdr"));
	IPSEC_ASSERT(mhp->msg != NULL, ("null msg"));

	/* map satype to proto */
	if ((proto = key_satype2proto(mhp->msg->sadb_msg_satype)) == 0) {
		ipseclog((LOG_DEBUG, "%s: invalid satype is passed.\n",
			__func__));
		return key_senderror(so, m, EINVAL);
	}

	if (mhp->ext[SADB_EXT_SA] == NULL ||
	    mhp->ext[SADB_EXT_ADDRESS_SRC] == NULL ||
	    mhp->ext[SADB_EXT_ADDRESS_DST] == NULL ||
	    (mhp->msg->sadb_msg_satype == SADB_SATYPE_ESP &&
	     mhp->ext[SADB_EXT_KEY_ENCRYPT] == NULL) ||
	    (mhp->msg->sadb_msg_satype == SADB_SATYPE_AH &&
	     mhp->ext[SADB_EXT_KEY_AUTH] == NULL) ||
	    (mhp->ext[SADB_EXT_LIFETIME_HARD] != NULL &&
	     mhp->ext[SADB_EXT_LIFETIME_SOFT] == NULL) ||
	    (mhp->ext[SADB_EXT_LIFETIME_HARD] == NULL &&
	     mhp->ext[SADB_EXT_LIFETIME_SOFT] != NULL)) {
		ipseclog((LOG_DEBUG, "%s: invalid message is passed.\n",
			__func__));
		return key_senderror(so, m, EINVAL);
	}
	if (mhp->extlen[SADB_EXT_SA] < sizeof(struct sadb_sa) ||
	    mhp->extlen[SADB_EXT_ADDRESS_SRC] < sizeof(struct sadb_address) ||
	    mhp->extlen[SADB_EXT_ADDRESS_DST] < sizeof(struct sadb_address)) {
		ipseclog((LOG_DEBUG, "%s: invalid message is passed.\n",
			__func__));
		return key_senderror(so, m, EINVAL);
	}
	if (mhp->ext[SADB_X_EXT_SA2] != NULL) {
		mode = ((struct sadb_x_sa2 *)mhp->ext[SADB_X_EXT_SA2])->sadb_x_sa2_mode;
		reqid = ((struct sadb_x_sa2 *)mhp->ext[SADB_X_EXT_SA2])->sadb_x_sa2_reqid;
	} else {
		mode = IPSEC_MODE_ANY;
		reqid = 0;
	}
	/* XXX boundary checking for other extensions */

	sa0 = (struct sadb_sa *)mhp->ext[SADB_EXT_SA];
	src0 = (struct sadb_address *)(mhp->ext[SADB_EXT_ADDRESS_SRC]);
	dst0 = (struct sadb_address *)(mhp->ext[SADB_EXT_ADDRESS_DST]);

	/* XXX boundary check against sa_len */
	KEY_SETSECASIDX(proto, mode, reqid, src0 + 1, dst0 + 1, &saidx);

	/*
	 * Make sure the port numbers are zero.
	 * In case of NAT-T we will update them later if needed.
	 */
	KEY_PORTTOSADDR(&saidx.src, 0);
	KEY_PORTTOSADDR(&saidx.dst, 0);

#ifdef IPSEC_NAT_T
	/*
	 * Handle NAT-T info if present.
	 */
	if (mhp->ext[SADB_X_EXT_NAT_T_TYPE] != NULL &&
	    mhp->ext[SADB_X_EXT_NAT_T_SPORT] != NULL &&
	    mhp->ext[SADB_X_EXT_NAT_T_DPORT] != NULL) {

		if (mhp->extlen[SADB_X_EXT_NAT_T_TYPE] < sizeof(*type) ||
		    mhp->extlen[SADB_X_EXT_NAT_T_SPORT] < sizeof(*sport) ||
		    mhp->extlen[SADB_X_EXT_NAT_T_DPORT] < sizeof(*dport)) {
			ipseclog((LOG_DEBUG, "%s: invalid message.\n",
			    __func__));
			return key_senderror(so, m, EINVAL);
		}

		type = (struct sadb_x_nat_t_type *)
		    mhp->ext[SADB_X_EXT_NAT_T_TYPE];
		sport = (struct sadb_x_nat_t_port *)
		    mhp->ext[SADB_X_EXT_NAT_T_SPORT];
		dport = (struct sadb_x_nat_t_port *)
		    mhp->ext[SADB_X_EXT_NAT_T_DPORT];
	} else {
		type = 0;
		sport = dport = 0;
	}
	if (mhp->ext[SADB_X_EXT_NAT_T_OAI] != NULL &&
	    mhp->ext[SADB_X_EXT_NAT_T_OAR] != NULL) {
		if (mhp->extlen[SADB_X_EXT_NAT_T_OAI] < sizeof(*iaddr) ||
		    mhp->extlen[SADB_X_EXT_NAT_T_OAR] < sizeof(*raddr)) {
			ipseclog((LOG_DEBUG, "%s: invalid message\n",
			    __func__));
			return key_senderror(so, m, EINVAL);
		}
		iaddr = (struct sadb_address *)mhp->ext[SADB_X_EXT_NAT_T_OAI];
		raddr = (struct sadb_address *)mhp->ext[SADB_X_EXT_NAT_T_OAR];
		ipseclog((LOG_DEBUG, "%s: NAT-T OAi/r present\n", __func__));
	} else {
		iaddr = raddr = NULL;
	}
	if (mhp->ext[SADB_X_EXT_NAT_T_FRAG] != NULL) {
		if (mhp->extlen[SADB_X_EXT_NAT_T_FRAG] < sizeof(*frag)) {
			ipseclog((LOG_DEBUG, "%s: invalid message\n",
			    __func__));
			return key_senderror(so, m, EINVAL);
		}
		frag = (struct sadb_x_nat_t_frag *)
		    mhp->ext[SADB_X_EXT_NAT_T_FRAG];
	} else {
		frag = 0;
	}
#endif

	/* get a SA header */
	if ((sah = key_getsah(&saidx)) == NULL) {
		ipseclog((LOG_DEBUG, "%s: no SA index found.\n", __func__));
		return key_senderror(so, m, ENOENT);
	}

	/* set spidx if there */
	/* XXX rewrite */
	error = key_setident(sah, m, mhp);
	if (error)
		return key_senderror(so, m, error);

	/* find a SA with sequence number. */
#ifdef IPSEC_DOSEQCHECK
	if (mhp->msg->sadb_msg_seq != 0
	 && (sav = key_getsavbyseq(sah, mhp->msg->sadb_msg_seq)) == NULL) {
		ipseclog((LOG_DEBUG, "%s: no larval SA with sequence %u "
			"exists.\n", __func__, mhp->msg->sadb_msg_seq));
		return key_senderror(so, m, ENOENT);
	}
#else
	SAHTREE_LOCK();
	sav = key_getsavbyspi(sah, sa0->sadb_sa_spi);
	SAHTREE_UNLOCK();
	if (sav == NULL) {
		ipseclog((LOG_DEBUG, "%s: no such a SA found (spi:%u)\n",
			__func__, (u_int32_t)ntohl(sa0->sadb_sa_spi)));
		return key_senderror(so, m, EINVAL);
	}
#endif

	/* validity check */
	if (sav->sah->saidx.proto != proto) {
		ipseclog((LOG_DEBUG, "%s: protocol mismatched "
			"(DB=%u param=%u)\n", __func__,
			sav->sah->saidx.proto, proto));
		return key_senderror(so, m, EINVAL);
	}
#ifdef IPSEC_DOSEQCHECK
	if (sav->spi != sa0->sadb_sa_spi) {
		ipseclog((LOG_DEBUG, "%s: SPI mismatched (DB:%u param:%u)\n",
		    __func__,
		    (u_int32_t)ntohl(sav->spi),
		    (u_int32_t)ntohl(sa0->sadb_sa_spi)));
		return key_senderror(so, m, EINVAL);
	}
#endif
	if (sav->pid != mhp->msg->sadb_msg_pid) {
		ipseclog((LOG_DEBUG, "%s: pid mismatched (DB:%u param:%u)\n",
		    __func__, sav->pid, mhp->msg->sadb_msg_pid));
		return key_senderror(so, m, EINVAL);
	}

	/* copy sav values */
	error = key_setsaval(sav, m, mhp);
	if (error) {
		KEY_FREESAV(&sav);
		return key_senderror(so, m, error);
	}

#ifdef IPSEC_NAT_T
	/*
	 * Handle more NAT-T info if present,
	 * now that we have a sav to fill.
	 */
	if (type)
		sav->natt_type = type->sadb_x_nat_t_type_type;

	if (sport)
		KEY_PORTTOSADDR(&sav->sah->saidx.src,
		    sport->sadb_x_nat_t_port_port);
	if (dport)
		KEY_PORTTOSADDR(&sav->sah->saidx.dst,
		    dport->sadb_x_nat_t_port_port);

#if 0
	/*
	 * In case SADB_X_EXT_NAT_T_FRAG was not given, leave it at 0.
	 * We should actually check for a minimum MTU here, if we
	 * want to support it in ip_output.
	 */
	if (frag)
		sav->natt_esp_frag_len = frag->sadb_x_nat_t_frag_fraglen;
#endif
#endif

	/* check SA values to be mature. */
	if ((mhp->msg->sadb_msg_errno = key_mature(sav)) != 0) {
		KEY_FREESAV(&sav);
		return key_senderror(so, m, 0);
	}

    {
	struct mbuf *n;

	/* set msg buf from mhp */
	n = key_getmsgbuf_x1(m, mhp);
	if (n == NULL) {
		ipseclog((LOG_DEBUG, "%s: No more memory.\n", __func__));
		return key_senderror(so, m, ENOBUFS);
	}

	m_freem(m);
	return key_sendup_mbuf(so, n, KEY_SENDUP_ALL);
    }
}

/*
 * search SAD with sequence for a SA which state is SADB_SASTATE_LARVAL.
 * only called by key_update().
 * OUT:
 *	NULL	: not found
 *	others	: found, pointer to a SA.
 */
#ifdef IPSEC_DOSEQCHECK
static struct secasvar *
key_getsavbyseq(sah, seq)
	struct secashead *sah;
	u_int32_t seq;
{
	struct secasvar *sav;
	u_int state;

	state = SADB_SASTATE_LARVAL;

	/* search SAD with sequence number ? */
	LIST_FOREACH(sav, &sah->savtree[state], chain) {

		KEY_CHKSASTATE(state, sav->state, __func__);

		if (sav->seq == seq) {
			sa_addref(sav);
			KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
				printf("DP %s cause refcnt++:%d SA:%p\n",
					__func__, sav->refcnt, sav));
			return sav;
		}
	}

	return NULL;
}
#endif

/*
 * SADB_ADD processing
 * add an entry to SA database, when received
 *   <base, SA, (SA2), (lifetime(HSC),) address(SD), (address(P),)
 *       key(AE), (identity(SD),) (sensitivity)>
 * from the ikmpd,
 * and send
 *   <base, SA, (SA2), (lifetime(HSC),) address(SD), (address(P),)
 *       (identity(SD),) (sensitivity)>
 * to the ikmpd.
 *
 * IGNORE identity and sensitivity messages.
 *
 * m will always be freed.
 */
static int
key_add(so, m, mhp)
	struct socket *so;
	struct mbuf *m;
	const struct sadb_msghdr *mhp;
{
	struct sadb_sa *sa0;
	struct sadb_address *src0, *dst0;
#ifdef IPSEC_NAT_T
	struct sadb_x_nat_t_type *type;
	struct sadb_address *iaddr, *raddr;
	struct sadb_x_nat_t_frag *frag;
#endif
	struct secasindex saidx;
	struct secashead *newsah;
	struct secasvar *newsav;
	u_int16_t proto;
	u_int8_t mode;
	u_int32_t reqid;
	int error;

	IPSEC_ASSERT(so != NULL, ("null socket"));
	IPSEC_ASSERT(m != NULL, ("null mbuf"));
	IPSEC_ASSERT(mhp != NULL, ("null msghdr"));
	IPSEC_ASSERT(mhp->msg != NULL, ("null msg"));

	/* map satype to proto */
	if ((proto = key_satype2proto(mhp->msg->sadb_msg_satype)) == 0) {
		ipseclog((LOG_DEBUG, "%s: invalid satype is passed.\n",
			__func__));
		return key_senderror(so, m, EINVAL);
	}

	if (mhp->ext[SADB_EXT_SA] == NULL ||
	    mhp->ext[SADB_EXT_ADDRESS_SRC] == NULL ||
	    mhp->ext[SADB_EXT_ADDRESS_DST] == NULL ||
	    (mhp->msg->sadb_msg_satype == SADB_SATYPE_ESP &&
	     mhp->ext[SADB_EXT_KEY_ENCRYPT] == NULL) ||
	    (mhp->msg->sadb_msg_satype == SADB_SATYPE_AH &&
	     mhp->ext[SADB_EXT_KEY_AUTH] == NULL) ||
	    (mhp->ext[SADB_EXT_LIFETIME_HARD] != NULL &&
	     mhp->ext[SADB_EXT_LIFETIME_SOFT] == NULL) ||
	    (mhp->ext[SADB_EXT_LIFETIME_HARD] == NULL &&
	     mhp->ext[SADB_EXT_LIFETIME_SOFT] != NULL)) {
		ipseclog((LOG_DEBUG, "%s: invalid message is passed.\n",
			__func__));
		return key_senderror(so, m, EINVAL);
	}
	if (mhp->extlen[SADB_EXT_SA] < sizeof(struct sadb_sa) ||
	    mhp->extlen[SADB_EXT_ADDRESS_SRC] < sizeof(struct sadb_address) ||
	    mhp->extlen[SADB_EXT_ADDRESS_DST] < sizeof(struct sadb_address)) {
		/* XXX need more */
		ipseclog((LOG_DEBUG, "%s: invalid message is passed.\n",
			__func__));
		return key_senderror(so, m, EINVAL);
	}
	if (mhp->ext[SADB_X_EXT_SA2] != NULL) {
		mode = ((struct sadb_x_sa2 *)mhp->ext[SADB_X_EXT_SA2])->sadb_x_sa2_mode;
		reqid = ((struct sadb_x_sa2 *)mhp->ext[SADB_X_EXT_SA2])->sadb_x_sa2_reqid;
	} else {
		mode = IPSEC_MODE_ANY;
		reqid = 0;
	}

	sa0 = (struct sadb_sa *)mhp->ext[SADB_EXT_SA];
	src0 = (struct sadb_address *)mhp->ext[SADB_EXT_ADDRESS_SRC];
	dst0 = (struct sadb_address *)mhp->ext[SADB_EXT_ADDRESS_DST];

	/* XXX boundary check against sa_len */
	KEY_SETSECASIDX(proto, mode, reqid, src0 + 1, dst0 + 1, &saidx);

	/*
	 * Make sure the port numbers are zero.
	 * In case of NAT-T we will update them later if needed.
	 */
	KEY_PORTTOSADDR(&saidx.src, 0);
	KEY_PORTTOSADDR(&saidx.dst, 0);

#ifdef IPSEC_NAT_T
	/*
	 * Handle NAT-T info if present.
	 */
	if (mhp->ext[SADB_X_EXT_NAT_T_TYPE] != NULL &&
	    mhp->ext[SADB_X_EXT_NAT_T_SPORT] != NULL &&
	    mhp->ext[SADB_X_EXT_NAT_T_DPORT] != NULL) {
		struct sadb_x_nat_t_port *sport, *dport;

		if (mhp->extlen[SADB_X_EXT_NAT_T_TYPE] < sizeof(*type) ||
		    mhp->extlen[SADB_X_EXT_NAT_T_SPORT] < sizeof(*sport) ||
		    mhp->extlen[SADB_X_EXT_NAT_T_DPORT] < sizeof(*dport)) {
			ipseclog((LOG_DEBUG, "%s: invalid message.\n",
			    __func__));
			return key_senderror(so, m, EINVAL);
		}

		type = (struct sadb_x_nat_t_type *)
		    mhp->ext[SADB_X_EXT_NAT_T_TYPE];
		sport = (struct sadb_x_nat_t_port *)
		    mhp->ext[SADB_X_EXT_NAT_T_SPORT];
		dport = (struct sadb_x_nat_t_port *)
		    mhp->ext[SADB_X_EXT_NAT_T_DPORT];

		if (sport)
			KEY_PORTTOSADDR(&saidx.src,
			    sport->sadb_x_nat_t_port_port);
		if (dport)
			KEY_PORTTOSADDR(&saidx.dst,
			    dport->sadb_x_nat_t_port_port);
	} else {
		type = 0;
	}
	if (mhp->ext[SADB_X_EXT_NAT_T_OAI] != NULL &&
	    mhp->ext[SADB_X_EXT_NAT_T_OAR] != NULL) {
		if (mhp->extlen[SADB_X_EXT_NAT_T_OAI] < sizeof(*iaddr) ||
		    mhp->extlen[SADB_X_EXT_NAT_T_OAR] < sizeof(*raddr)) {
			ipseclog((LOG_DEBUG, "%s: invalid message\n",
			    __func__));
			return key_senderror(so, m, EINVAL);
		}
		iaddr = (struct sadb_address *)mhp->ext[SADB_X_EXT_NAT_T_OAI];
		raddr = (struct sadb_address *)mhp->ext[SADB_X_EXT_NAT_T_OAR];
		ipseclog((LOG_DEBUG, "%s: NAT-T OAi/r present\n", __func__));
	} else {
		iaddr = raddr = NULL;
	}
	if (mhp->ext[SADB_X_EXT_NAT_T_FRAG] != NULL) {
		if (mhp->extlen[SADB_X_EXT_NAT_T_FRAG] < sizeof(*frag)) {
			ipseclog((LOG_DEBUG, "%s: invalid message\n",
			    __func__));
			return key_senderror(so, m, EINVAL);
		}
		frag = (struct sadb_x_nat_t_frag *)
		    mhp->ext[SADB_X_EXT_NAT_T_FRAG];
	} else {
		frag = 0;
	}
#endif

	/* get a SA header */
	if ((newsah = key_getsah(&saidx)) == NULL) {
		/* create a new SA header */
		if ((newsah = key_newsah(&saidx)) == NULL) {
			ipseclog((LOG_DEBUG, "%s: No more memory.\n",__func__));
			return key_senderror(so, m, ENOBUFS);
		}
	}

	/* set spidx if there */
	/* XXX rewrite */
	error = key_setident(newsah, m, mhp);
	if (error) {
		return key_senderror(so, m, error);
	}

	/* create new SA entry. */
	/* We can create new SA only if SPI is differenct. */
	SAHTREE_LOCK();
	newsav = key_getsavbyspi(newsah, sa0->sadb_sa_spi);
	SAHTREE_UNLOCK();
	if (newsav != NULL) {
		ipseclog((LOG_DEBUG, "%s: SA already exists.\n", __func__));
		return key_senderror(so, m, EEXIST);
	}
	newsav = KEY_NEWSAV(m, mhp, newsah, &error);
	if (newsav == NULL) {
		return key_senderror(so, m, error);
	}

#ifdef IPSEC_NAT_T
	/*
	 * Handle more NAT-T info if present,
	 * now that we have a sav to fill.
	 */
	if (type)
		newsav->natt_type = type->sadb_x_nat_t_type_type;

#if 0
	/*
	 * In case SADB_X_EXT_NAT_T_FRAG was not given, leave it at 0.
	 * We should actually check for a minimum MTU here, if we
	 * want to support it in ip_output.
	 */
	if (frag)
		newsav->natt_esp_frag_len = frag->sadb_x_nat_t_frag_fraglen;
#endif
#endif

	/* check SA values to be mature. */
	if ((error = key_mature(newsav)) != 0) {
		KEY_FREESAV(&newsav);
		return key_senderror(so, m, error);
	}

	/*
	 * don't call key_freesav() here, as we would like to keep the SA
	 * in the database on success.
	 */

    {
	struct mbuf *n;

	/* set msg buf from mhp */
	n = key_getmsgbuf_x1(m, mhp);
	if (n == NULL) {
		ipseclog((LOG_DEBUG, "%s: No more memory.\n", __func__));
		return key_senderror(so, m, ENOBUFS);
	}

	m_freem(m);
	return key_sendup_mbuf(so, n, KEY_SENDUP_ALL);
    }
}

/* m is retained */
static int
key_setident(sah, m, mhp)
	struct secashead *sah;
	struct mbuf *m;
	const struct sadb_msghdr *mhp;
{
	const struct sadb_ident *idsrc, *iddst;
	int idsrclen, iddstlen;

	IPSEC_ASSERT(sah != NULL, ("null secashead"));
	IPSEC_ASSERT(m != NULL, ("null mbuf"));
	IPSEC_ASSERT(mhp != NULL, ("null msghdr"));
	IPSEC_ASSERT(mhp->msg != NULL, ("null msg"));

	/* don't make buffer if not there */
	if (mhp->ext[SADB_EXT_IDENTITY_SRC] == NULL &&
	    mhp->ext[SADB_EXT_IDENTITY_DST] == NULL) {
		sah->idents = NULL;
		sah->identd = NULL;
		return 0;
	}
	
	if (mhp->ext[SADB_EXT_IDENTITY_SRC] == NULL ||
	    mhp->ext[SADB_EXT_IDENTITY_DST] == NULL) {
		ipseclog((LOG_DEBUG, "%s: invalid identity.\n", __func__));
		return EINVAL;
	}

	idsrc = (const struct sadb_ident *)mhp->ext[SADB_EXT_IDENTITY_SRC];
	iddst = (const struct sadb_ident *)mhp->ext[SADB_EXT_IDENTITY_DST];
	idsrclen = mhp->extlen[SADB_EXT_IDENTITY_SRC];
	iddstlen = mhp->extlen[SADB_EXT_IDENTITY_DST];

	/* validity check */
	if (idsrc->sadb_ident_type != iddst->sadb_ident_type) {
		ipseclog((LOG_DEBUG, "%s: ident type mismatch.\n", __func__));
		return EINVAL;
	}

	switch (idsrc->sadb_ident_type) {
	case SADB_IDENTTYPE_PREFIX:
	case SADB_IDENTTYPE_FQDN:
	case SADB_IDENTTYPE_USERFQDN:
	default:
		/* XXX do nothing */
		sah->idents = NULL;
		sah->identd = NULL;
	 	return 0;
	}

	/* make structure */
	sah->idents = malloc(sizeof(struct secident), M_IPSEC_MISC, M_NOWAIT);
	if (sah->idents == NULL) {
		ipseclog((LOG_DEBUG, "%s: No more memory.\n", __func__));
		return ENOBUFS;
	}
	sah->identd = malloc(sizeof(struct secident), M_IPSEC_MISC, M_NOWAIT);
	if (sah->identd == NULL) {
		free(sah->idents, M_IPSEC_MISC);
		sah->idents = NULL;
		ipseclog((LOG_DEBUG, "%s: No more memory.\n", __func__));
		return ENOBUFS;
	}
	sah->idents->type = idsrc->sadb_ident_type;
	sah->idents->id = idsrc->sadb_ident_id;

	sah->identd->type = iddst->sadb_ident_type;
	sah->identd->id = iddst->sadb_ident_id;

	return 0;
}

/*
 * m will not be freed on return.
 * it is caller's responsibility to free the result. 
 */
static struct mbuf *
key_getmsgbuf_x1(m, mhp)
	struct mbuf *m;
	const struct sadb_msghdr *mhp;
{
	struct mbuf *n;

	IPSEC_ASSERT(m != NULL, ("null mbuf"));
	IPSEC_ASSERT(mhp != NULL, ("null msghdr"));
	IPSEC_ASSERT(mhp->msg != NULL, ("null msg"));

	/* create new sadb_msg to reply. */
	n = key_gather_mbuf(m, mhp, 1, 9, SADB_EXT_RESERVED,
	    SADB_EXT_SA, SADB_X_EXT_SA2,
	    SADB_EXT_ADDRESS_SRC, SADB_EXT_ADDRESS_DST,
	    SADB_EXT_LIFETIME_HARD, SADB_EXT_LIFETIME_SOFT,
	    SADB_EXT_IDENTITY_SRC, SADB_EXT_IDENTITY_DST);
	if (!n)
		return NULL;

	if (n->m_len < sizeof(struct sadb_msg)) {
		n = m_pullup(n, sizeof(struct sadb_msg));
		if (n == NULL)
			return NULL;
	}
	mtod(n, struct sadb_msg *)->sadb_msg_errno = 0;
	mtod(n, struct sadb_msg *)->sadb_msg_len =
	    PFKEY_UNIT64(n->m_pkthdr.len);

	return n;
}

static int key_delete_all __P((struct socket *, struct mbuf *,
	const struct sadb_msghdr *, u_int16_t));

/*
 * SADB_DELETE processing
 * receive
 *   <base, SA(*), address(SD)>
 * from the ikmpd, and set SADB_SASTATE_DEAD,
 * and send,
 *   <base, SA(*), address(SD)>
 * to the ikmpd.
 *
 * m will always be freed.
 */
static int
key_delete(so, m, mhp)
	struct socket *so;
	struct mbuf *m;
	const struct sadb_msghdr *mhp;
{
	struct sadb_sa *sa0;
	struct sadb_address *src0, *dst0;
	struct secasindex saidx;
	struct secashead *sah;
	struct secasvar *sav = NULL;
	u_int16_t proto;

	IPSEC_ASSERT(so != NULL, ("null socket"));
	IPSEC_ASSERT(m != NULL, ("null mbuf"));
	IPSEC_ASSERT(mhp != NULL, ("null msghdr"));
	IPSEC_ASSERT(mhp->msg != NULL, ("null msg"));

	/* map satype to proto */
	if ((proto = key_satype2proto(mhp->msg->sadb_msg_satype)) == 0) {
		ipseclog((LOG_DEBUG, "%s: invalid satype is passed.\n",
			__func__));
		return key_senderror(so, m, EINVAL);
	}

	if (mhp->ext[SADB_EXT_ADDRESS_SRC] == NULL ||
	    mhp->ext[SADB_EXT_ADDRESS_DST] == NULL) {
		ipseclog((LOG_DEBUG, "%s: invalid message is passed.\n",
			__func__));
		return key_senderror(so, m, EINVAL);
	}

	if (mhp->extlen[SADB_EXT_ADDRESS_SRC] < sizeof(struct sadb_address) ||
	    mhp->extlen[SADB_EXT_ADDRESS_DST] < sizeof(struct sadb_address)) {
		ipseclog((LOG_DEBUG, "%s: invalid message is passed.\n",
			__func__));
		return key_senderror(so, m, EINVAL);
	}

	if (mhp->ext[SADB_EXT_SA] == NULL) {
		/*
		 * Caller wants us to delete all non-LARVAL SAs
		 * that match the src/dst.  This is used during
		 * IKE INITIAL-CONTACT.
		 */
		ipseclog((LOG_DEBUG, "%s: doing delete all.\n", __func__));
		return key_delete_all(so, m, mhp, proto);
	} else if (mhp->extlen[SADB_EXT_SA] < sizeof(struct sadb_sa)) {
		ipseclog((LOG_DEBUG, "%s: invalid message is passed.\n",
			__func__));
		return key_senderror(so, m, EINVAL);
	}

	sa0 = (struct sadb_sa *)mhp->ext[SADB_EXT_SA];
	src0 = (struct sadb_address *)(mhp->ext[SADB_EXT_ADDRESS_SRC]);
	dst0 = (struct sadb_address *)(mhp->ext[SADB_EXT_ADDRESS_DST]);

	/* XXX boundary check against sa_len */
	KEY_SETSECASIDX(proto, IPSEC_MODE_ANY, 0, src0 + 1, dst0 + 1, &saidx);

	/*
	 * Make sure the port numbers are zero.
	 * In case of NAT-T we will update them later if needed.
	 */
	KEY_PORTTOSADDR(&saidx.src, 0);
	KEY_PORTTOSADDR(&saidx.dst, 0);

#ifdef IPSEC_NAT_T
	/*
	 * Handle NAT-T info if present.
	 */
	if (mhp->ext[SADB_X_EXT_NAT_T_SPORT] != NULL &&
	    mhp->ext[SADB_X_EXT_NAT_T_DPORT] != NULL) {
		struct sadb_x_nat_t_port *sport, *dport;

		if (mhp->extlen[SADB_X_EXT_NAT_T_SPORT] < sizeof(*sport) ||
		    mhp->extlen[SADB_X_EXT_NAT_T_DPORT] < sizeof(*dport)) {
			ipseclog((LOG_DEBUG, "%s: invalid message.\n",
			    __func__));
			return key_senderror(so, m, EINVAL);
		}

		sport = (struct sadb_x_nat_t_port *)
		    mhp->ext[SADB_X_EXT_NAT_T_SPORT];
		dport = (struct sadb_x_nat_t_port *)
		    mhp->ext[SADB_X_EXT_NAT_T_DPORT];

		if (sport)
			KEY_PORTTOSADDR(&saidx.src,
			    sport->sadb_x_nat_t_port_port);
		if (dport)
			KEY_PORTTOSADDR(&saidx.dst,
			    dport->sadb_x_nat_t_port_port);
	}
#endif

	/* get a SA header */
	SAHTREE_LOCK();
	LIST_FOREACH(sah, &V_sahtree, chain) {
		if (sah->state == SADB_SASTATE_DEAD)
			continue;
		if (key_cmpsaidx(&sah->saidx, &saidx, CMP_HEAD) == 0)
			continue;

		/* get a SA with SPI. */
		sav = key_getsavbyspi(sah, sa0->sadb_sa_spi);
		if (sav)
			break;
	}
	if (sah == NULL) {
		SAHTREE_UNLOCK();
		ipseclog((LOG_DEBUG, "%s: no SA found.\n", __func__));
		return key_senderror(so, m, ENOENT);
	}

	key_sa_chgstate(sav, SADB_SASTATE_DEAD);
	KEY_FREESAV(&sav);
	SAHTREE_UNLOCK();

    {
	struct mbuf *n;
	struct sadb_msg *newmsg;

	/* create new sadb_msg to reply. */
	/* XXX-BZ NAT-T extensions? */
	n = key_gather_mbuf(m, mhp, 1, 4, SADB_EXT_RESERVED,
	    SADB_EXT_SA, SADB_EXT_ADDRESS_SRC, SADB_EXT_ADDRESS_DST);
	if (!n)
		return key_senderror(so, m, ENOBUFS);

	if (n->m_len < sizeof(struct sadb_msg)) {
		n = m_pullup(n, sizeof(struct sadb_msg));
		if (n == NULL)
			return key_senderror(so, m, ENOBUFS);
	}
	newmsg = mtod(n, struct sadb_msg *);
	newmsg->sadb_msg_errno = 0;
	newmsg->sadb_msg_len = PFKEY_UNIT64(n->m_pkthdr.len);

	m_freem(m);
	return key_sendup_mbuf(so, n, KEY_SENDUP_ALL);
    }
}

/*
 * delete all SAs for src/dst.  Called from key_delete().
 */
static int
key_delete_all(struct socket *so, struct mbuf *m, const struct sadb_msghdr *mhp,
    u_int16_t proto)
{
	struct sadb_address *src0, *dst0;
	struct secasindex saidx;
	struct secashead *sah;
	struct secasvar *sav, *nextsav;
	u_int stateidx, state;

	src0 = (struct sadb_address *)(mhp->ext[SADB_EXT_ADDRESS_SRC]);
	dst0 = (struct sadb_address *)(mhp->ext[SADB_EXT_ADDRESS_DST]);

	/* XXX boundary check against sa_len */
	KEY_SETSECASIDX(proto, IPSEC_MODE_ANY, 0, src0 + 1, dst0 + 1, &saidx);

	/*
	 * Make sure the port numbers are zero.
	 * In case of NAT-T we will update them later if needed.
	 */
	KEY_PORTTOSADDR(&saidx.src, 0);
	KEY_PORTTOSADDR(&saidx.dst, 0);

#ifdef IPSEC_NAT_T
	/*
	 * Handle NAT-T info if present.
	 */

	if (mhp->ext[SADB_X_EXT_NAT_T_SPORT] != NULL &&
	    mhp->ext[SADB_X_EXT_NAT_T_DPORT] != NULL) {
		struct sadb_x_nat_t_port *sport, *dport;

		if (mhp->extlen[SADB_X_EXT_NAT_T_SPORT] < sizeof(*sport) ||
		    mhp->extlen[SADB_X_EXT_NAT_T_DPORT] < sizeof(*dport)) {
			ipseclog((LOG_DEBUG, "%s: invalid message.\n",
			    __func__));
			return key_senderror(so, m, EINVAL);
		}

		sport = (struct sadb_x_nat_t_port *)
		    mhp->ext[SADB_X_EXT_NAT_T_SPORT];
		dport = (struct sadb_x_nat_t_port *)
		    mhp->ext[SADB_X_EXT_NAT_T_DPORT];

		if (sport)
			KEY_PORTTOSADDR(&saidx.src,
			    sport->sadb_x_nat_t_port_port);
		if (dport)
			KEY_PORTTOSADDR(&saidx.dst,
			    dport->sadb_x_nat_t_port_port);
	}
#endif

	SAHTREE_LOCK();
	LIST_FOREACH(sah, &V_sahtree, chain) {
		if (sah->state == SADB_SASTATE_DEAD)
			continue;
		if (key_cmpsaidx(&sah->saidx, &saidx, CMP_HEAD) == 0)
			continue;

		/* Delete all non-LARVAL SAs. */
		for (stateidx = 0;
		     stateidx < _ARRAYLEN(saorder_state_alive);
		     stateidx++) {
			state = saorder_state_alive[stateidx];
			if (state == SADB_SASTATE_LARVAL)
				continue;
			for (sav = LIST_FIRST(&sah->savtree[state]);
			     sav != NULL; sav = nextsav) {
				nextsav = LIST_NEXT(sav, chain);
				/* sanity check */
				if (sav->state != state) {
					ipseclog((LOG_DEBUG, "%s: invalid "
						"sav->state (queue %d SA %d)\n",
						__func__, state, sav->state));
					continue;
				}
				
				key_sa_chgstate(sav, SADB_SASTATE_DEAD);
				KEY_FREESAV(&sav);
			}
		}
	}
	SAHTREE_UNLOCK();
    {
	struct mbuf *n;
	struct sadb_msg *newmsg;

	/* create new sadb_msg to reply. */
	/* XXX-BZ NAT-T extensions? */
	n = key_gather_mbuf(m, mhp, 1, 3, SADB_EXT_RESERVED,
	    SADB_EXT_ADDRESS_SRC, SADB_EXT_ADDRESS_DST);
	if (!n)
		return key_senderror(so, m, ENOBUFS);

	if (n->m_len < sizeof(struct sadb_msg)) {
		n = m_pullup(n, sizeof(struct sadb_msg));
		if (n == NULL)
			return key_senderror(so, m, ENOBUFS);
	}
	newmsg = mtod(n, struct sadb_msg *);
	newmsg->sadb_msg_errno = 0;
	newmsg->sadb_msg_len = PFKEY_UNIT64(n->m_pkthdr.len);

	m_freem(m);
	return key_sendup_mbuf(so, n, KEY_SENDUP_ALL);
    }
}

/*
 * SADB_GET processing
 * receive
 *   <base, SA(*), address(SD)>
 * from the ikmpd, and get a SP and a SA to respond,
 * and send,
 *   <base, SA, (lifetime(HSC),) address(SD), (address(P),) key(AE),
 *       (identity(SD),) (sensitivity)>
 * to the ikmpd.
 *
 * m will always be freed.
 */
static int
key_get(so, m, mhp)
	struct socket *so;
	struct mbuf *m;
	const struct sadb_msghdr *mhp;
{
	struct sadb_sa *sa0;
	struct sadb_address *src0, *dst0;
	struct secasindex saidx;
	struct secashead *sah;
	struct secasvar *sav = NULL;
	u_int16_t proto;

	IPSEC_ASSERT(so != NULL, ("null socket"));
	IPSEC_ASSERT(m != NULL, ("null mbuf"));
	IPSEC_ASSERT(mhp != NULL, ("null msghdr"));
	IPSEC_ASSERT(mhp->msg != NULL, ("null msg"));

	/* map satype to proto */
	if ((proto = key_satype2proto(mhp->msg->sadb_msg_satype)) == 0) {
		ipseclog((LOG_DEBUG, "%s: invalid satype is passed.\n",
			__func__));
		return key_senderror(so, m, EINVAL);
	}

	if (mhp->ext[SADB_EXT_SA] == NULL ||
	    mhp->ext[SADB_EXT_ADDRESS_SRC] == NULL ||
	    mhp->ext[SADB_EXT_ADDRESS_DST] == NULL) {
		ipseclog((LOG_DEBUG, "%s: invalid message is passed.\n",
			__func__));
		return key_senderror(so, m, EINVAL);
	}
	if (mhp->extlen[SADB_EXT_SA] < sizeof(struct sadb_sa) ||
	    mhp->extlen[SADB_EXT_ADDRESS_SRC] < sizeof(struct sadb_address) ||
	    mhp->extlen[SADB_EXT_ADDRESS_DST] < sizeof(struct sadb_address)) {
		ipseclog((LOG_DEBUG, "%s: invalid message is passed.\n",
			__func__));
		return key_senderror(so, m, EINVAL);
	}

	sa0 = (struct sadb_sa *)mhp->ext[SADB_EXT_SA];
	src0 = (struct sadb_address *)mhp->ext[SADB_EXT_ADDRESS_SRC];
	dst0 = (struct sadb_address *)mhp->ext[SADB_EXT_ADDRESS_DST];

	/* XXX boundary check against sa_len */
	KEY_SETSECASIDX(proto, IPSEC_MODE_ANY, 0, src0 + 1, dst0 + 1, &saidx);

	/*
	 * Make sure the port numbers are zero.
	 * In case of NAT-T we will update them later if needed.
	 */
	KEY_PORTTOSADDR(&saidx.src, 0);
	KEY_PORTTOSADDR(&saidx.dst, 0);

#ifdef IPSEC_NAT_T
	/*
	 * Handle NAT-T info if present.
	 */

	if (mhp->ext[SADB_X_EXT_NAT_T_SPORT] != NULL &&
	    mhp->ext[SADB_X_EXT_NAT_T_DPORT] != NULL) {
		struct sadb_x_nat_t_port *sport, *dport;

		if (mhp->extlen[SADB_X_EXT_NAT_T_SPORT] < sizeof(*sport) ||
		    mhp->extlen[SADB_X_EXT_NAT_T_DPORT] < sizeof(*dport)) {
			ipseclog((LOG_DEBUG, "%s: invalid message.\n",
			    __func__));
			return key_senderror(so, m, EINVAL);
		}

		sport = (struct sadb_x_nat_t_port *)
		    mhp->ext[SADB_X_EXT_NAT_T_SPORT];
		dport = (struct sadb_x_nat_t_port *)
		    mhp->ext[SADB_X_EXT_NAT_T_DPORT];

		if (sport)
			KEY_PORTTOSADDR(&saidx.src,
			    sport->sadb_x_nat_t_port_port);
		if (dport)
			KEY_PORTTOSADDR(&saidx.dst,
			    dport->sadb_x_nat_t_port_port);
	}
#endif

	/* get a SA header */
	SAHTREE_LOCK();
	LIST_FOREACH(sah, &V_sahtree, chain) {
		if (sah->state == SADB_SASTATE_DEAD)
			continue;
		if (key_cmpsaidx(&sah->saidx, &saidx, CMP_HEAD) == 0)
			continue;

		/* get a SA with SPI. */
		sav = key_getsavbyspi(sah, sa0->sadb_sa_spi);
		if (sav)
			break;
	}
	SAHTREE_UNLOCK();
	if (sah == NULL) {
		ipseclog((LOG_DEBUG, "%s: no SA found.\n", __func__));
		return key_senderror(so, m, ENOENT);
	}

    {
	struct mbuf *n;
	u_int8_t satype;

	/* map proto to satype */
	if ((satype = key_proto2satype(sah->saidx.proto)) == 0) {
		ipseclog((LOG_DEBUG, "%s: there was invalid proto in SAD.\n",
			__func__));
		return key_senderror(so, m, EINVAL);
	}

	/* create new sadb_msg to reply. */
	n = key_setdumpsa(sav, SADB_GET, satype, mhp->msg->sadb_msg_seq,
	    mhp->msg->sadb_msg_pid);
	if (!n)
		return key_senderror(so, m, ENOBUFS);

	m_freem(m);
	return key_sendup_mbuf(so, n, KEY_SENDUP_ONE);
    }
}

/* XXX make it sysctl-configurable? */
static void
key_getcomb_setlifetime(comb)
	struct sadb_comb *comb;
{

	comb->sadb_comb_soft_allocations = 1;
	comb->sadb_comb_hard_allocations = 1;
	comb->sadb_comb_soft_bytes = 0;
	comb->sadb_comb_hard_bytes = 0;
	comb->sadb_comb_hard_addtime = 86400;	/* 1 day */
	comb->sadb_comb_soft_addtime = comb->sadb_comb_soft_addtime * 80 / 100;
	comb->sadb_comb_soft_usetime = 28800;	/* 8 hours */
	comb->sadb_comb_hard_usetime = comb->sadb_comb_hard_usetime * 80 / 100;
}

/*
 * XXX reorder combinations by preference
 * XXX no idea if the user wants ESP authentication or not
 */
static struct mbuf *
key_getcomb_esp()
{
	struct sadb_comb *comb;
	struct enc_xform *algo;
	struct mbuf *result = NULL, *m, *n;
	int encmin;
	int i, off, o;
	int totlen;
	const int l = PFKEY_ALIGN8(sizeof(struct sadb_comb));

	m = NULL;
	for (i = 1; i <= SADB_EALG_MAX; i++) {
		algo = esp_algorithm_lookup(i);
		if (algo == NULL)
			continue;

		/* discard algorithms with key size smaller than system min */
		if (_BITS(algo->maxkey) < V_ipsec_esp_keymin)
			continue;
		if (_BITS(algo->minkey) < V_ipsec_esp_keymin)
			encmin = V_ipsec_esp_keymin;
		else
			encmin = _BITS(algo->minkey);

		if (V_ipsec_esp_auth)
			m = key_getcomb_ah();
		else {
			IPSEC_ASSERT(l <= MLEN,
				("l=%u > MLEN=%lu", l, (u_long) MLEN));
			MGET(m, M_NOWAIT, MT_DATA);
			if (m) {
				M_ALIGN(m, l);
				m->m_len = l;
				m->m_next = NULL;
				bzero(mtod(m, caddr_t), m->m_len);
			}
		}
		if (!m)
			goto fail;

		totlen = 0;
		for (n = m; n; n = n->m_next)
			totlen += n->m_len;
		IPSEC_ASSERT((totlen % l) == 0, ("totlen=%u, l=%u", totlen, l));

		for (off = 0; off < totlen; off += l) {
			n = m_pulldown(m, off, l, &o);
			if (!n) {
				/* m is already freed */
				goto fail;
			}
			comb = (struct sadb_comb *)(mtod(n, caddr_t) + o);
			bzero(comb, sizeof(*comb));
			key_getcomb_setlifetime(comb);
			comb->sadb_comb_encrypt = i;
			comb->sadb_comb_encrypt_minbits = encmin;
			comb->sadb_comb_encrypt_maxbits = _BITS(algo->maxkey);
		}

		if (!result)
			result = m;
		else
			m_cat(result, m);
	}

	return result;

 fail:
	if (result)
		m_freem(result);
	return NULL;
}

static void
key_getsizes_ah(
	const struct auth_hash *ah,
	int alg,
	u_int16_t* min,
	u_int16_t* max)
{

	*min = *max = ah->keysize;
	if (ah->keysize == 0) {
		/*
		 * Transform takes arbitrary key size but algorithm
		 * key size is restricted.  Enforce this here.
		 */
		switch (alg) {
		case SADB_X_AALG_MD5:	*min = *max = 16; break;
		case SADB_X_AALG_SHA:	*min = *max = 20; break;
		case SADB_X_AALG_NULL:	*min = 1; *max = 256; break;
		case SADB_X_AALG_SHA2_256: *min = *max = 32; break;
		case SADB_X_AALG_SHA2_384: *min = *max = 48; break;
		case SADB_X_AALG_SHA2_512: *min = *max = 64; break;
		default:
			DPRINTF(("%s: unknown AH algorithm %u\n",
				__func__, alg));
			break;
		}
	}
}

/*
 * XXX reorder combinations by preference
 */
static struct mbuf *
key_getcomb_ah()
{
	struct sadb_comb *comb;
	struct auth_hash *algo;
	struct mbuf *m;
	u_int16_t minkeysize, maxkeysize;
	int i;
	const int l = PFKEY_ALIGN8(sizeof(struct sadb_comb));

	m = NULL;
	for (i = 1; i <= SADB_AALG_MAX; i++) {
#if 1
		/* we prefer HMAC algorithms, not old algorithms */
		if (i != SADB_AALG_SHA1HMAC &&
		    i != SADB_AALG_MD5HMAC  &&
		    i != SADB_X_AALG_SHA2_256 &&
		    i != SADB_X_AALG_SHA2_384 &&
		    i != SADB_X_AALG_SHA2_512)
			continue;
#endif
		algo = ah_algorithm_lookup(i);
		if (!algo)
			continue;
		key_getsizes_ah(algo, i, &minkeysize, &maxkeysize);
		/* discard algorithms with key size smaller than system min */
		if (_BITS(minkeysize) < V_ipsec_ah_keymin)
			continue;

		if (!m) {
			IPSEC_ASSERT(l <= MLEN,
				("l=%u > MLEN=%lu", l, (u_long) MLEN));
			MGET(m, M_NOWAIT, MT_DATA);
			if (m) {
				M_ALIGN(m, l);
				m->m_len = l;
				m->m_next = NULL;
			}
		} else
			M_PREPEND(m, l, M_NOWAIT);
		if (!m)
			return NULL;

		comb = mtod(m, struct sadb_comb *);
		bzero(comb, sizeof(*comb));
		key_getcomb_setlifetime(comb);
		comb->sadb_comb_auth = i;
		comb->sadb_comb_auth_minbits = _BITS(minkeysize);
		comb->sadb_comb_auth_maxbits = _BITS(maxkeysize);
	}

	return m;
}

/*
 * not really an official behavior.  discussed in pf_key@inner.net in Sep2000.
 * XXX reorder combinations by preference
 */
static struct mbuf *
key_getcomb_ipcomp()
{
	struct sadb_comb *comb;
	struct comp_algo *algo;
	struct mbuf *m;
	int i;
	const int l = PFKEY_ALIGN8(sizeof(struct sadb_comb));

	m = NULL;
	for (i = 1; i <= SADB_X_CALG_MAX; i++) {
		algo = ipcomp_algorithm_lookup(i);
		if (!algo)
			continue;

		if (!m) {
			IPSEC_ASSERT(l <= MLEN,
				("l=%u > MLEN=%lu", l, (u_long) MLEN));
			MGET(m, M_NOWAIT, MT_DATA);
			if (m) {
				M_ALIGN(m, l);
				m->m_len = l;
				m->m_next = NULL;
			}
		} else
			M_PREPEND(m, l, M_NOWAIT);
		if (!m)
			return NULL;

		comb = mtod(m, struct sadb_comb *);
		bzero(comb, sizeof(*comb));
		key_getcomb_setlifetime(comb);
		comb->sadb_comb_encrypt = i;
		/* what should we set into sadb_comb_*_{min,max}bits? */
	}

	return m;
}

/*
 * XXX no way to pass mode (transport/tunnel) to userland
 * XXX replay checking?
 * XXX sysctl interface to ipsec_{ah,esp}_keymin
 */
static struct mbuf *
key_getprop(saidx)
	const struct secasindex *saidx;
{
	struct sadb_prop *prop;
	struct mbuf *m, *n;
	const int l = PFKEY_ALIGN8(sizeof(struct sadb_prop));
	int totlen;

	switch (saidx->proto)  {
	case IPPROTO_ESP:
		m = key_getcomb_esp();
		break;
	case IPPROTO_AH:
		m = key_getcomb_ah();
		break;
	case IPPROTO_IPCOMP:
		m = key_getcomb_ipcomp();
		break;
	default:
		return NULL;
	}

	if (!m)
		return NULL;
	M_PREPEND(m, l, M_NOWAIT);
	if (!m)
		return NULL;

	totlen = 0;
	for (n = m; n; n = n->m_next)
		totlen += n->m_len;

	prop = mtod(m, struct sadb_prop *);
	bzero(prop, sizeof(*prop));
	prop->sadb_prop_len = PFKEY_UNIT64(totlen);
	prop->sadb_prop_exttype = SADB_EXT_PROPOSAL;
	prop->sadb_prop_replay = 32;	/* XXX */

	return m;
}

/*
 * SADB_ACQUIRE processing called by key_checkrequest() and key_acquire2().
 * send
 *   <base, SA, address(SD), (address(P)), x_policy,
 *       (identity(SD),) (sensitivity,) proposal>
 * to KMD, and expect to receive
 *   <base> with SADB_ACQUIRE if error occured,
 * or
 *   <base, src address, dst address, (SPI range)> with SADB_GETSPI
 * from KMD by PF_KEY.
 *
 * XXX x_policy is outside of RFC2367 (KAME extension).
 * XXX sensitivity is not supported.
 * XXX for ipcomp, RFC2367 does not define how to fill in proposal.
 * see comment for key_getcomb_ipcomp().
 *
 * OUT:
 *    0     : succeed
 *    others: error number
 */
static int
key_acquire(const struct secasindex *saidx, struct secpolicy *sp)
{
	struct mbuf *result = NULL, *m;
	struct secacq *newacq;
	u_int8_t satype;
	int error = -1;
	u_int32_t seq;

	IPSEC_ASSERT(saidx != NULL, ("null saidx"));
	satype = key_proto2satype(saidx->proto);
	IPSEC_ASSERT(satype != 0, ("null satype, protocol %u", saidx->proto));

	/*
	 * We never do anything about acquirng SA.  There is anather
	 * solution that kernel blocks to send SADB_ACQUIRE message until
	 * getting something message from IKEd.  In later case, to be
	 * managed with ACQUIRING list.
	 */
	/* Get an entry to check whether sending message or not. */
	if ((newacq = key_getacq(saidx)) != NULL) {
		if (V_key_blockacq_count < newacq->count) {
			/* reset counter and do send message. */
			newacq->count = 0;
		} else {
			/* increment counter and do nothing. */
			newacq->count++;
			return 0;
		}
	} else {
		/* make new entry for blocking to send SADB_ACQUIRE. */
		if ((newacq = key_newacq(saidx)) == NULL)
			return ENOBUFS;
	}


	seq = newacq->seq;
	m = key_setsadbmsg(SADB_ACQUIRE, 0, satype, seq, 0, 0);
	if (!m) {
		error = ENOBUFS;
		goto fail;
	}
	result = m;

	/*
	 * No SADB_X_EXT_NAT_T_* here: we do not know
	 * anything related to NAT-T at this time.
	 */

	/* set sadb_address for saidx's. */
	m = key_setsadbaddr(SADB_EXT_ADDRESS_SRC,
	    &saidx->src.sa, FULLMASK, IPSEC_ULPROTO_ANY);
	if (!m) {
		error = ENOBUFS;
		goto fail;
	}
	m_cat(result, m);

	m = key_setsadbaddr(SADB_EXT_ADDRESS_DST,
	    &saidx->dst.sa, FULLMASK, IPSEC_ULPROTO_ANY);
	if (!m) {
		error = ENOBUFS;
		goto fail;
	}
	m_cat(result, m);

	/* XXX proxy address (optional) */

	/* set sadb_x_policy */
	if (sp) {
		m = key_setsadbxpolicy(sp->policy, sp->spidx.dir, sp->id);
		if (!m) {
			error = ENOBUFS;
			goto fail;
		}
		m_cat(result, m);
	}

	/* XXX identity (optional) */
#if 0
	if (idexttype && fqdn) {
		/* create identity extension (FQDN) */
		struct sadb_ident *id;
		int fqdnlen;

		fqdnlen = strlen(fqdn) + 1;	/* +1 for terminating-NUL */
		id = (struct sadb_ident *)p;
		bzero(id, sizeof(*id) + PFKEY_ALIGN8(fqdnlen));
		id->sadb_ident_len = PFKEY_UNIT64(sizeof(*id) + PFKEY_ALIGN8(fqdnlen));
		id->sadb_ident_exttype = idexttype;
		id->sadb_ident_type = SADB_IDENTTYPE_FQDN;
		bcopy(fqdn, id + 1, fqdnlen);
		p += sizeof(struct sadb_ident) + PFKEY_ALIGN8(fqdnlen);
	}

	if (idexttype) {
		/* create identity extension (USERFQDN) */
		struct sadb_ident *id;
		int userfqdnlen;

		if (userfqdn) {
			/* +1 for terminating-NUL */
			userfqdnlen = strlen(userfqdn) + 1;
		} else
			userfqdnlen = 0;
		id = (struct sadb_ident *)p;
		bzero(id, sizeof(*id) + PFKEY_ALIGN8(userfqdnlen));
		id->sadb_ident_len = PFKEY_UNIT64(sizeof(*id) + PFKEY_ALIGN8(userfqdnlen));
		id->sadb_ident_exttype = idexttype;
		id->sadb_ident_type = SADB_IDENTTYPE_USERFQDN;
		/* XXX is it correct? */
		if (curproc && curproc->p_cred)
			id->sadb_ident_id = curproc->p_cred->p_ruid;
		if (userfqdn && userfqdnlen)
			bcopy(userfqdn, id + 1, userfqdnlen);
		p += sizeof(struct sadb_ident) + PFKEY_ALIGN8(userfqdnlen);
	}
#endif

	/* XXX sensitivity (optional) */

	/* create proposal/combination extension */
	m = key_getprop(saidx);
#if 0
	/*
	 * spec conformant: always attach proposal/combination extension,
	 * the problem is that we have no way to attach it for ipcomp,
	 * due to the way sadb_comb is declared in RFC2367.
	 */
	if (!m) {
		error = ENOBUFS;
		goto fail;
	}
	m_cat(result, m);
#else
	/*
	 * outside of spec; make proposal/combination extension optional.
	 */
	if (m)
		m_cat(result, m);
#endif

	if ((result->m_flags & M_PKTHDR) == 0) {
		error = EINVAL;
		goto fail;
	}

	if (result->m_len < sizeof(struct sadb_msg)) {
		result = m_pullup(result, sizeof(struct sadb_msg));
		if (result == NULL) {
			error = ENOBUFS;
			goto fail;
		}
	}

	result->m_pkthdr.len = 0;
	for (m = result; m; m = m->m_next)
		result->m_pkthdr.len += m->m_len;

	mtod(result, struct sadb_msg *)->sadb_msg_len =
	    PFKEY_UNIT64(result->m_pkthdr.len);

	return key_sendup_mbuf(NULL, result, KEY_SENDUP_REGISTERED);

 fail:
	if (result)
		m_freem(result);
	return error;
}

static struct secacq *
key_newacq(const struct secasindex *saidx)
{
	struct secacq *newacq;

	/* get new entry */
	newacq = malloc(sizeof(struct secacq), M_IPSEC_SAQ, M_NOWAIT|M_ZERO);
	if (newacq == NULL) {
		ipseclog((LOG_DEBUG, "%s: No more memory.\n", __func__));
		return NULL;
	}

	/* copy secindex */
	bcopy(saidx, &newacq->saidx, sizeof(newacq->saidx));
	newacq->seq = (V_acq_seq == ~0 ? 1 : ++V_acq_seq);
	newacq->created = time_second;
	newacq->count = 0;

	/* add to acqtree */
	ACQ_LOCK();
	LIST_INSERT_HEAD(&V_acqtree, newacq, chain);
	ACQ_UNLOCK();

	return newacq;
}

static struct secacq *
key_getacq(const struct secasindex *saidx)
{
	struct secacq *acq;

	ACQ_LOCK();
	LIST_FOREACH(acq, &V_acqtree, chain) {
		if (key_cmpsaidx(saidx, &acq->saidx, CMP_EXACTLY))
			break;
	}
	ACQ_UNLOCK();

	return acq;
}

static struct secacq *
key_getacqbyseq(seq)
	u_int32_t seq;
{
	struct secacq *acq;

	ACQ_LOCK();
	LIST_FOREACH(acq, &V_acqtree, chain) {
		if (acq->seq == seq)
			break;
	}
	ACQ_UNLOCK();

	return acq;
}

static struct secspacq *
key_newspacq(spidx)
	struct secpolicyindex *spidx;
{
	struct secspacq *acq;

	/* get new entry */
	acq = malloc(sizeof(struct secspacq), M_IPSEC_SAQ, M_NOWAIT|M_ZERO);
	if (acq == NULL) {
		ipseclog((LOG_DEBUG, "%s: No more memory.\n", __func__));
		return NULL;
	}

	/* copy secindex */
	bcopy(spidx, &acq->spidx, sizeof(acq->spidx));
	acq->created = time_second;
	acq->count = 0;

	/* add to spacqtree */
	SPACQ_LOCK();
	LIST_INSERT_HEAD(&V_spacqtree, acq, chain);
	SPACQ_UNLOCK();

	return acq;
}

static struct secspacq *
key_getspacq(spidx)
	struct secpolicyindex *spidx;
{
	struct secspacq *acq;

	SPACQ_LOCK();
	LIST_FOREACH(acq, &V_spacqtree, chain) {
		if (key_cmpspidx_exactly(spidx, &acq->spidx)) {
			/* NB: return holding spacq_lock */
			return acq;
		}
	}
	SPACQ_UNLOCK();

	return NULL;
}

/*
 * SADB_ACQUIRE processing,
 * in first situation, is receiving
 *   <base>
 * from the ikmpd, and clear sequence of its secasvar entry.
 *
 * In second situation, is receiving
 *   <base, address(SD), (address(P),) (identity(SD),) (sensitivity,) proposal>
 * from a user land process, and return
 *   <base, address(SD), (address(P),) (identity(SD),) (sensitivity,) proposal>
 * to the socket.
 *
 * m will always be freed.
 */
static int
key_acquire2(so, m, mhp)
	struct socket *so;
	struct mbuf *m;
	const struct sadb_msghdr *mhp;
{
	const struct sadb_address *src0, *dst0;
	struct secasindex saidx;
	struct secashead *sah;
	u_int16_t proto;
	int error;

	IPSEC_ASSERT(so != NULL, ("null socket"));
	IPSEC_ASSERT(m != NULL, ("null mbuf"));
	IPSEC_ASSERT(mhp != NULL, ("null msghdr"));
	IPSEC_ASSERT(mhp->msg != NULL, ("null msg"));

	/*
	 * Error message from KMd.
	 * We assume that if error was occured in IKEd, the length of PFKEY
	 * message is equal to the size of sadb_msg structure.
	 * We do not raise error even if error occured in this function.
	 */
	if (mhp->msg->sadb_msg_len == PFKEY_UNIT64(sizeof(struct sadb_msg))) {
		struct secacq *acq;

		/* check sequence number */
		if (mhp->msg->sadb_msg_seq == 0) {
			ipseclog((LOG_DEBUG, "%s: must specify sequence "
				"number.\n", __func__));
			m_freem(m);
			return 0;
		}

		if ((acq = key_getacqbyseq(mhp->msg->sadb_msg_seq)) == NULL) {
			/*
			 * the specified larval SA is already gone, or we got
			 * a bogus sequence number.  we can silently ignore it.
			 */
			m_freem(m);
			return 0;
		}

		/* reset acq counter in order to deletion by timehander. */
		acq->created = time_second;
		acq->count = 0;
		m_freem(m);
		return 0;
	}

	/*
	 * This message is from user land.
	 */

	/* map satype to proto */
	if ((proto = key_satype2proto(mhp->msg->sadb_msg_satype)) == 0) {
		ipseclog((LOG_DEBUG, "%s: invalid satype is passed.\n",
			__func__));
		return key_senderror(so, m, EINVAL);
	}

	if (mhp->ext[SADB_EXT_ADDRESS_SRC] == NULL ||
	    mhp->ext[SADB_EXT_ADDRESS_DST] == NULL ||
	    mhp->ext[SADB_EXT_PROPOSAL] == NULL) {
		/* error */
		ipseclog((LOG_DEBUG, "%s: invalid message is passed.\n",
			__func__));
		return key_senderror(so, m, EINVAL);
	}
	if (mhp->extlen[SADB_EXT_ADDRESS_SRC] < sizeof(struct sadb_address) ||
	    mhp->extlen[SADB_EXT_ADDRESS_DST] < sizeof(struct sadb_address) ||
	    mhp->extlen[SADB_EXT_PROPOSAL] < sizeof(struct sadb_prop)) {
		/* error */
		ipseclog((LOG_DEBUG, "%s: invalid message is passed.\n",	
			__func__));
		return key_senderror(so, m, EINVAL);
	}

	src0 = (struct sadb_address *)mhp->ext[SADB_EXT_ADDRESS_SRC];
	dst0 = (struct sadb_address *)mhp->ext[SADB_EXT_ADDRESS_DST];

	/* XXX boundary check against sa_len */
	KEY_SETSECASIDX(proto, IPSEC_MODE_ANY, 0, src0 + 1, dst0 + 1, &saidx);

	/*
	 * Make sure the port numbers are zero.
	 * In case of NAT-T we will update them later if needed.
	 */
	KEY_PORTTOSADDR(&saidx.src, 0);
	KEY_PORTTOSADDR(&saidx.dst, 0);

#ifndef IPSEC_NAT_T
	/*
	 * Handle NAT-T info if present.
	 */

	if (mhp->ext[SADB_X_EXT_NAT_T_SPORT] != NULL &&
	    mhp->ext[SADB_X_EXT_NAT_T_DPORT] != NULL) {
		struct sadb_x_nat_t_port *sport, *dport;

		if (mhp->extlen[SADB_X_EXT_NAT_T_SPORT] < sizeof(*sport) ||
		    mhp->extlen[SADB_X_EXT_NAT_T_DPORT] < sizeof(*dport)) {
			ipseclog((LOG_DEBUG, "%s: invalid message.\n",
			    __func__));
			return key_senderror(so, m, EINVAL);
		}

		sport = (struct sadb_x_nat_t_port *)
		    mhp->ext[SADB_X_EXT_NAT_T_SPORT];
		dport = (struct sadb_x_nat_t_port *)
		    mhp->ext[SADB_X_EXT_NAT_T_DPORT];

		if (sport)
			KEY_PORTTOSADDR(&saidx.src,
			    sport->sadb_x_nat_t_port_port);
		if (dport)
			KEY_PORTTOSADDR(&saidx.dst,
			    dport->sadb_x_nat_t_port_port);
	}
#endif

	/* get a SA index */
	SAHTREE_LOCK();
	LIST_FOREACH(sah, &V_sahtree, chain) {
		if (sah->state == SADB_SASTATE_DEAD)
			continue;
		if (key_cmpsaidx(&sah->saidx, &saidx, CMP_MODE_REQID))
			break;
	}
	SAHTREE_UNLOCK();
	if (sah != NULL) {
		ipseclog((LOG_DEBUG, "%s: a SA exists already.\n", __func__));
		return key_senderror(so, m, EEXIST);
	}

	error = key_acquire(&saidx, NULL);
	if (error != 0) {
		ipseclog((LOG_DEBUG, "%s: error %d returned from key_acquire\n",
			__func__, mhp->msg->sadb_msg_errno));
		return key_senderror(so, m, error);
	}

	return key_sendup_mbuf(so, m, KEY_SENDUP_REGISTERED);
}

/*
 * SADB_REGISTER processing.
 * If SATYPE_UNSPEC has been passed as satype, only return sabd_supported.
 * receive
 *   <base>
 * from the ikmpd, and register a socket to send PF_KEY messages,
 * and send
 *   <base, supported>
 * to KMD by PF_KEY.
 * If socket is detached, must free from regnode.
 *
 * m will always be freed.
 */
static int
key_register(so, m, mhp)
	struct socket *so;
	struct mbuf *m;
	const struct sadb_msghdr *mhp;
{
	struct secreg *reg, *newreg = 0;

	IPSEC_ASSERT(so != NULL, ("null socket"));
	IPSEC_ASSERT(m != NULL, ("null mbuf"));
	IPSEC_ASSERT(mhp != NULL, ("null msghdr"));
	IPSEC_ASSERT(mhp->msg != NULL, ("null msg"));

	/* check for invalid register message */
	if (mhp->msg->sadb_msg_satype >= sizeof(V_regtree)/sizeof(V_regtree[0]))
		return key_senderror(so, m, EINVAL);

	/* When SATYPE_UNSPEC is specified, only return sabd_supported. */
	if (mhp->msg->sadb_msg_satype == SADB_SATYPE_UNSPEC)
		goto setmsg;

	/* check whether existing or not */
	REGTREE_LOCK();
	LIST_FOREACH(reg, &V_regtree[mhp->msg->sadb_msg_satype], chain) {
		if (reg->so == so) {
			REGTREE_UNLOCK();
			ipseclog((LOG_DEBUG, "%s: socket exists already.\n",
				__func__));
			return key_senderror(so, m, EEXIST);
		}
	}

	/* create regnode */
	newreg =  malloc(sizeof(struct secreg), M_IPSEC_SAR, M_NOWAIT|M_ZERO);
	if (newreg == NULL) {
		REGTREE_UNLOCK();
		ipseclog((LOG_DEBUG, "%s: No more memory.\n", __func__));
		return key_senderror(so, m, ENOBUFS);
	}

	newreg->so = so;
	((struct keycb *)sotorawcb(so))->kp_registered++;

	/* add regnode to regtree. */
	LIST_INSERT_HEAD(&V_regtree[mhp->msg->sadb_msg_satype], newreg, chain);
	REGTREE_UNLOCK();

  setmsg:
    {
	struct mbuf *n;
	struct sadb_msg *newmsg;
	struct sadb_supported *sup;
	u_int len, alen, elen;
	int off;
	int i;
	struct sadb_alg *alg;

	/* create new sadb_msg to reply. */
	alen = 0;
	for (i = 1; i <= SADB_AALG_MAX; i++) {
		if (ah_algorithm_lookup(i))
			alen += sizeof(struct sadb_alg);
	}
	if (alen)
		alen += sizeof(struct sadb_supported);
	elen = 0;
	for (i = 1; i <= SADB_EALG_MAX; i++) {
		if (esp_algorithm_lookup(i))
			elen += sizeof(struct sadb_alg);
	}
	if (elen)
		elen += sizeof(struct sadb_supported);

	len = sizeof(struct sadb_msg) + alen + elen;

	if (len > MCLBYTES)
		return key_senderror(so, m, ENOBUFS);

	MGETHDR(n, M_NOWAIT, MT_DATA);
	if (len > MHLEN) {
		MCLGET(n, M_NOWAIT);
		if ((n->m_flags & M_EXT) == 0) {
			m_freem(n);
			n = NULL;
		}
	}
	if (!n)
		return key_senderror(so, m, ENOBUFS);

	n->m_pkthdr.len = n->m_len = len;
	n->m_next = NULL;
	off = 0;

	m_copydata(m, 0, sizeof(struct sadb_msg), mtod(n, caddr_t) + off);
	newmsg = mtod(n, struct sadb_msg *);
	newmsg->sadb_msg_errno = 0;
	newmsg->sadb_msg_len = PFKEY_UNIT64(len);
	off += PFKEY_ALIGN8(sizeof(struct sadb_msg));

	/* for authentication algorithm */
	if (alen) {
		sup = (struct sadb_supported *)(mtod(n, caddr_t) + off);
		sup->sadb_supported_len = PFKEY_UNIT64(alen);
		sup->sadb_supported_exttype = SADB_EXT_SUPPORTED_AUTH;
		off += PFKEY_ALIGN8(sizeof(*sup));

		for (i = 1; i <= SADB_AALG_MAX; i++) {
			struct auth_hash *aalgo;
			u_int16_t minkeysize, maxkeysize;

			aalgo = ah_algorithm_lookup(i);
			if (!aalgo)
				continue;
			alg = (struct sadb_alg *)(mtod(n, caddr_t) + off);
			alg->sadb_alg_id = i;
			alg->sadb_alg_ivlen = 0;
			key_getsizes_ah(aalgo, i, &minkeysize, &maxkeysize);
			alg->sadb_alg_minbits = _BITS(minkeysize);
			alg->sadb_alg_maxbits = _BITS(maxkeysize);
			off += PFKEY_ALIGN8(sizeof(*alg));
		}
	}

	/* for encryption algorithm */
	if (elen) {
		sup = (struct sadb_supported *)(mtod(n, caddr_t) + off);
		sup->sadb_supported_len = PFKEY_UNIT64(elen);
		sup->sadb_supported_exttype = SADB_EXT_SUPPORTED_ENCRYPT;
		off += PFKEY_ALIGN8(sizeof(*sup));

		for (i = 1; i <= SADB_EALG_MAX; i++) {
			struct enc_xform *ealgo;

			ealgo = esp_algorithm_lookup(i);
			if (!ealgo)
				continue;
			alg = (struct sadb_alg *)(mtod(n, caddr_t) + off);
			alg->sadb_alg_id = i;
			alg->sadb_alg_ivlen = ealgo->blocksize;
			alg->sadb_alg_minbits = _BITS(ealgo->minkey);
			alg->sadb_alg_maxbits = _BITS(ealgo->maxkey);
			off += PFKEY_ALIGN8(sizeof(struct sadb_alg));
		}
	}

	IPSEC_ASSERT(off == len,
		("length assumption failed (off %u len %u)", off, len));

	m_freem(m);
	return key_sendup_mbuf(so, n, KEY_SENDUP_REGISTERED);
    }
}

/*
 * free secreg entry registered.
 * XXX: I want to do free a socket marked done SADB_RESIGER to socket.
 */
void
key_freereg(struct socket *so)
{
	struct secreg *reg;
	int i;

	IPSEC_ASSERT(so != NULL, ("NULL so"));

	/*
	 * check whether existing or not.
	 * check all type of SA, because there is a potential that
	 * one socket is registered to multiple type of SA.
	 */
	REGTREE_LOCK();
	for (i = 0; i <= SADB_SATYPE_MAX; i++) {
		LIST_FOREACH(reg, &V_regtree[i], chain) {
			if (reg->so == so && __LIST_CHAINED(reg)) {
				LIST_REMOVE(reg, chain);
				free(reg, M_IPSEC_SAR);
				break;
			}
		}
	}
	REGTREE_UNLOCK();
}

/*
 * SADB_EXPIRE processing
 * send
 *   <base, SA, SA2, lifetime(C and one of HS), address(SD)>
 * to KMD by PF_KEY.
 * NOTE: We send only soft lifetime extension.
 *
 * OUT:	0	: succeed
 *	others	: error number
 */
static int
key_expire(struct secasvar *sav)
{
	int satype;
	struct mbuf *result = NULL, *m;
	int len;
	int error = -1;
	struct sadb_lifetime *lt;

	IPSEC_ASSERT (sav != NULL, ("null sav"));
	IPSEC_ASSERT (sav->sah != NULL, ("null sa header"));

	/* set msg header */
	satype = key_proto2satype(sav->sah->saidx.proto);
	IPSEC_ASSERT(satype != 0, ("invalid proto, satype %u", satype));
	m = key_setsadbmsg(SADB_EXPIRE, 0, satype, sav->seq, 0, sav->refcnt);
	if (!m) {
		error = ENOBUFS;
		goto fail;
	}
	result = m;

	/* create SA extension */
	m = key_setsadbsa(sav);
	if (!m) {
		error = ENOBUFS;
		goto fail;
	}
	m_cat(result, m);

	/* create SA extension */
	m = key_setsadbxsa2(sav->sah->saidx.mode,
			sav->replay ? sav->replay->count : 0,
			sav->sah->saidx.reqid);
	if (!m) {
		error = ENOBUFS;
		goto fail;
	}
	m_cat(result, m);

	/* create lifetime extension (current and soft) */
	len = PFKEY_ALIGN8(sizeof(*lt)) * 2;
	m = m_get2(len, M_NOWAIT, MT_DATA, 0);
	if (m == NULL) {
		error = ENOBUFS;
		goto fail;
	}
	m_align(m, len);
	m->m_len = len;
	bzero(mtod(m, caddr_t), len);
	lt = mtod(m, struct sadb_lifetime *);
	lt->sadb_lifetime_len = PFKEY_UNIT64(sizeof(struct sadb_lifetime));
	lt->sadb_lifetime_exttype = SADB_EXT_LIFETIME_CURRENT;
	lt->sadb_lifetime_allocations = sav->lft_c->allocations;
	lt->sadb_lifetime_bytes = sav->lft_c->bytes;
	lt->sadb_lifetime_addtime = sav->lft_c->addtime;
	lt->sadb_lifetime_usetime = sav->lft_c->usetime;
	lt = (struct sadb_lifetime *)(mtod(m, caddr_t) + len / 2);
	lt->sadb_lifetime_len = PFKEY_UNIT64(sizeof(struct sadb_lifetime));
	lt->sadb_lifetime_exttype = SADB_EXT_LIFETIME_SOFT;
	lt->sadb_lifetime_allocations = sav->lft_s->allocations;
	lt->sadb_lifetime_bytes = sav->lft_s->bytes;
	lt->sadb_lifetime_addtime = sav->lft_s->addtime;
	lt->sadb_lifetime_usetime = sav->lft_s->usetime;
	m_cat(result, m);

	/* set sadb_address for source */
	m = key_setsadbaddr(SADB_EXT_ADDRESS_SRC,
	    &sav->sah->saidx.src.sa,
	    FULLMASK, IPSEC_ULPROTO_ANY);
	if (!m) {
		error = ENOBUFS;
		goto fail;
	}
	m_cat(result, m);

	/* set sadb_address for destination */
	m = key_setsadbaddr(SADB_EXT_ADDRESS_DST,
	    &sav->sah->saidx.dst.sa,
	    FULLMASK, IPSEC_ULPROTO_ANY);
	if (!m) {
		error = ENOBUFS;
		goto fail;
	}
	m_cat(result, m);

	/*
	 * XXX-BZ Handle NAT-T extensions here.
	 */

	if ((result->m_flags & M_PKTHDR) == 0) {
		error = EINVAL;
		goto fail;
	}

	if (result->m_len < sizeof(struct sadb_msg)) {
		result = m_pullup(result, sizeof(struct sadb_msg));
		if (result == NULL) {
			error = ENOBUFS;
			goto fail;
		}
	}

	result->m_pkthdr.len = 0;
	for (m = result; m; m = m->m_next)
		result->m_pkthdr.len += m->m_len;

	mtod(result, struct sadb_msg *)->sadb_msg_len =
	    PFKEY_UNIT64(result->m_pkthdr.len);

	return key_sendup_mbuf(NULL, result, KEY_SENDUP_REGISTERED);

 fail:
	if (result)
		m_freem(result);
	return error;
}

/*
 * SADB_FLUSH processing
 * receive
 *   <base>
 * from the ikmpd, and free all entries in secastree.
 * and send,
 *   <base>
 * to the ikmpd.
 * NOTE: to do is only marking SADB_SASTATE_DEAD.
 *
 * m will always be freed.
 */
static int
key_flush(so, m, mhp)
	struct socket *so;
	struct mbuf *m;
	const struct sadb_msghdr *mhp;
{
	struct sadb_msg *newmsg;
	struct secashead *sah, *nextsah;
	struct secasvar *sav, *nextsav;
	u_int16_t proto;
	u_int8_t state;
	u_int stateidx;

	IPSEC_ASSERT(so != NULL, ("null socket"));
	IPSEC_ASSERT(mhp != NULL, ("null msghdr"));
	IPSEC_ASSERT(mhp->msg != NULL, ("null msg"));

	/* map satype to proto */
	if ((proto = key_satype2proto(mhp->msg->sadb_msg_satype)) == 0) {
		ipseclog((LOG_DEBUG, "%s: invalid satype is passed.\n",
			__func__));
		return key_senderror(so, m, EINVAL);
	}

	/* no SATYPE specified, i.e. flushing all SA. */
	SAHTREE_LOCK();
	for (sah = LIST_FIRST(&V_sahtree);
	     sah != NULL;
	     sah = nextsah) {
		nextsah = LIST_NEXT(sah, chain);

		if (mhp->msg->sadb_msg_satype != SADB_SATYPE_UNSPEC
		 && proto != sah->saidx.proto)
			continue;

		for (stateidx = 0;
		     stateidx < _ARRAYLEN(saorder_state_alive);
		     stateidx++) {
			state = saorder_state_any[stateidx];
			for (sav = LIST_FIRST(&sah->savtree[state]);
			     sav != NULL;
			     sav = nextsav) {

				nextsav = LIST_NEXT(sav, chain);

				key_sa_chgstate(sav, SADB_SASTATE_DEAD);
				KEY_FREESAV(&sav);
			}
		}

		sah->state = SADB_SASTATE_DEAD;
	}
	SAHTREE_UNLOCK();

	if (m->m_len < sizeof(struct sadb_msg) ||
	    sizeof(struct sadb_msg) > m->m_len + M_TRAILINGSPACE(m)) {
		ipseclog((LOG_DEBUG, "%s: No more memory.\n", __func__));
		return key_senderror(so, m, ENOBUFS);
	}

	if (m->m_next)
		m_freem(m->m_next);
	m->m_next = NULL;
	m->m_pkthdr.len = m->m_len = sizeof(struct sadb_msg);
	newmsg = mtod(m, struct sadb_msg *);
	newmsg->sadb_msg_errno = 0;
	newmsg->sadb_msg_len = PFKEY_UNIT64(m->m_pkthdr.len);

	return key_sendup_mbuf(so, m, KEY_SENDUP_ALL);
}

/*
 * SADB_DUMP processing
 * dump all entries including status of DEAD in SAD.
 * receive
 *   <base>
 * from the ikmpd, and dump all secasvar leaves
 * and send,
 *   <base> .....
 * to the ikmpd.
 *
 * m will always be freed.
 */
static int
key_dump(so, m, mhp)
	struct socket *so;
	struct mbuf *m;
	const struct sadb_msghdr *mhp;
{
	struct secashead *sah;
	struct secasvar *sav;
	u_int16_t proto;
	u_int stateidx;
	u_int8_t satype;
	u_int8_t state;
	int cnt;
	struct sadb_msg *newmsg;
	struct mbuf *n;

	IPSEC_ASSERT(so != NULL, ("null socket"));
	IPSEC_ASSERT(m != NULL, ("null mbuf"));
	IPSEC_ASSERT(mhp != NULL, ("null msghdr"));
	IPSEC_ASSERT(mhp->msg != NULL, ("null msg"));

	/* map satype to proto */
	if ((proto = key_satype2proto(mhp->msg->sadb_msg_satype)) == 0) {
		ipseclog((LOG_DEBUG, "%s: invalid satype is passed.\n",
			__func__));
		return key_senderror(so, m, EINVAL);
	}

	/* count sav entries to be sent to the userland. */
	cnt = 0;
	SAHTREE_LOCK();
	LIST_FOREACH(sah, &V_sahtree, chain) {
		if (mhp->msg->sadb_msg_satype != SADB_SATYPE_UNSPEC
		 && proto != sah->saidx.proto)
			continue;

		for (stateidx = 0;
		     stateidx < _ARRAYLEN(saorder_state_any);
		     stateidx++) {
			state = saorder_state_any[stateidx];
			LIST_FOREACH(sav, &sah->savtree[state], chain) {
				cnt++;
			}
		}
	}

	if (cnt == 0) {
		SAHTREE_UNLOCK();
		return key_senderror(so, m, ENOENT);
	}

	/* send this to the userland, one at a time. */
	newmsg = NULL;
	LIST_FOREACH(sah, &V_sahtree, chain) {
		if (mhp->msg->sadb_msg_satype != SADB_SATYPE_UNSPEC
		 && proto != sah->saidx.proto)
			continue;

		/* map proto to satype */
		if ((satype = key_proto2satype(sah->saidx.proto)) == 0) {
			SAHTREE_UNLOCK();
			ipseclog((LOG_DEBUG, "%s: there was invalid proto in "
				"SAD.\n", __func__));
			return key_senderror(so, m, EINVAL);
		}

		for (stateidx = 0;
		     stateidx < _ARRAYLEN(saorder_state_any);
		     stateidx++) {
			state = saorder_state_any[stateidx];
			LIST_FOREACH(sav, &sah->savtree[state], chain) {
				n = key_setdumpsa(sav, SADB_DUMP, satype,
				    --cnt, mhp->msg->sadb_msg_pid);
				if (!n) {
					SAHTREE_UNLOCK();
					return key_senderror(so, m, ENOBUFS);
				}
				key_sendup_mbuf(so, n, KEY_SENDUP_ONE);
			}
		}
	}
	SAHTREE_UNLOCK();

	m_freem(m);
	return 0;
}

/*
 * SADB_X_PROMISC processing
 *
 * m will always be freed.
 */
static int
key_promisc(so, m, mhp)
	struct socket *so;
	struct mbuf *m;
	const struct sadb_msghdr *mhp;
{
	int olen;

	IPSEC_ASSERT(so != NULL, ("null socket"));
	IPSEC_ASSERT(m != NULL, ("null mbuf"));
	IPSEC_ASSERT(mhp != NULL, ("null msghdr"));
	IPSEC_ASSERT(mhp->msg != NULL, ("null msg"));

	olen = PFKEY_UNUNIT64(mhp->msg->sadb_msg_len);

	if (olen < sizeof(struct sadb_msg)) {
#if 1
		return key_senderror(so, m, EINVAL);
#else
		m_freem(m);
		return 0;
#endif
	} else if (olen == sizeof(struct sadb_msg)) {
		/* enable/disable promisc mode */
		struct keycb *kp;

		if ((kp = (struct keycb *)sotorawcb(so)) == NULL)
			return key_senderror(so, m, EINVAL);
		mhp->msg->sadb_msg_errno = 0;
		switch (mhp->msg->sadb_msg_satype) {
		case 0:
		case 1:
			kp->kp_promisc = mhp->msg->sadb_msg_satype;
			break;
		default:
			return key_senderror(so, m, EINVAL);
		}

		/* send the original message back to everyone */
		mhp->msg->sadb_msg_errno = 0;
		return key_sendup_mbuf(so, m, KEY_SENDUP_ALL);
	} else {
		/* send packet as is */

		m_adj(m, PFKEY_ALIGN8(sizeof(struct sadb_msg)));

		/* TODO: if sadb_msg_seq is specified, send to specific pid */
		return key_sendup_mbuf(so, m, KEY_SENDUP_ALL);
	}
}

static int (*key_typesw[]) __P((struct socket *, struct mbuf *,
		const struct sadb_msghdr *)) = {
	NULL,		/* SADB_RESERVED */
	key_getspi,	/* SADB_GETSPI */
	key_update,	/* SADB_UPDATE */
	key_add,	/* SADB_ADD */
	key_delete,	/* SADB_DELETE */
	key_get,	/* SADB_GET */
	key_acquire2,	/* SADB_ACQUIRE */
	key_register,	/* SADB_REGISTER */
	NULL,		/* SADB_EXPIRE */
	key_flush,	/* SADB_FLUSH */
	key_dump,	/* SADB_DUMP */
	key_promisc,	/* SADB_X_PROMISC */
	NULL,		/* SADB_X_PCHANGE */
	key_spdadd,	/* SADB_X_SPDUPDATE */
	key_spdadd,	/* SADB_X_SPDADD */
	key_spddelete,	/* SADB_X_SPDDELETE */
	key_spdget,	/* SADB_X_SPDGET */
	NULL,		/* SADB_X_SPDACQUIRE */
	key_spddump,	/* SADB_X_SPDDUMP */
	key_spdflush,	/* SADB_X_SPDFLUSH */
	key_spdadd,	/* SADB_X_SPDSETIDX */
	NULL,		/* SADB_X_SPDEXPIRE */
	key_spddelete2,	/* SADB_X_SPDDELETE2 */
};

/*
 * parse sadb_msg buffer to process PFKEYv2,
 * and create a data to response if needed.
 * I think to be dealed with mbuf directly.
 * IN:
 *     msgp  : pointer to pointer to a received buffer pulluped.
 *             This is rewrited to response.
 *     so    : pointer to socket.
 * OUT:
 *    length for buffer to send to user process.
 */
int
key_parse(m, so)
	struct mbuf *m;
	struct socket *so;
{
	struct sadb_msg *msg;
	struct sadb_msghdr mh;
	u_int orglen;
	int error;
	int target;

	IPSEC_ASSERT(so != NULL, ("null socket"));
	IPSEC_ASSERT(m != NULL, ("null mbuf"));

#if 0	/*kdebug_sadb assumes msg in linear buffer*/
	KEYDEBUG(KEYDEBUG_KEY_DUMP,
		ipseclog((LOG_DEBUG, "%s: passed sadb_msg\n", __func__));
		kdebug_sadb(msg));
#endif

	if (m->m_len < sizeof(struct sadb_msg)) {
		m = m_pullup(m, sizeof(struct sadb_msg));
		if (!m)
			return ENOBUFS;
	}
	msg = mtod(m, struct sadb_msg *);
	orglen = PFKEY_UNUNIT64(msg->sadb_msg_len);
	target = KEY_SENDUP_ONE;

	if ((m->m_flags & M_PKTHDR) == 0 ||
	    m->m_pkthdr.len != m->m_pkthdr.len) {
		ipseclog((LOG_DEBUG, "%s: invalid message length.\n",__func__));
		PFKEYSTAT_INC(out_invlen);
		error = EINVAL;
		goto senderror;
	}

	if (msg->sadb_msg_version != PF_KEY_V2) {
		ipseclog((LOG_DEBUG, "%s: PF_KEY version %u is mismatched.\n",
		    __func__, msg->sadb_msg_version));
		PFKEYSTAT_INC(out_invver);
		error = EINVAL;
		goto senderror;
	}

	if (msg->sadb_msg_type > SADB_MAX) {
		ipseclog((LOG_DEBUG, "%s: invalid type %u is passed.\n",
		    __func__, msg->sadb_msg_type));
		PFKEYSTAT_INC(out_invmsgtype);
		error = EINVAL;
		goto senderror;
	}

	/* for old-fashioned code - should be nuked */
	if (m->m_pkthdr.len > MCLBYTES) {
		m_freem(m);
		return ENOBUFS;
	}
	if (m->m_next) {
		struct mbuf *n;

		MGETHDR(n, M_NOWAIT, MT_DATA);
		if (n && m->m_pkthdr.len > MHLEN) {
			MCLGET(n, M_NOWAIT);
			if ((n->m_flags & M_EXT) == 0) {
				m_free(n);
				n = NULL;
			}
		}
		if (!n) {
			m_freem(m);
			return ENOBUFS;
		}
		m_copydata(m, 0, m->m_pkthdr.len, mtod(n, caddr_t));
		n->m_pkthdr.len = n->m_len = m->m_pkthdr.len;
		n->m_next = NULL;
		m_freem(m);
		m = n;
	}

	/* align the mbuf chain so that extensions are in contiguous region. */
	error = key_align(m, &mh);
	if (error)
		return error;

	msg = mh.msg;

	/* check SA type */
	switch (msg->sadb_msg_satype) {
	case SADB_SATYPE_UNSPEC:
		switch (msg->sadb_msg_type) {
		case SADB_GETSPI:
		case SADB_UPDATE:
		case SADB_ADD:
		case SADB_DELETE:
		case SADB_GET:
		case SADB_ACQUIRE:
		case SADB_EXPIRE:
			ipseclog((LOG_DEBUG, "%s: must specify satype "
			    "when msg type=%u.\n", __func__,
			    msg->sadb_msg_type));
			PFKEYSTAT_INC(out_invsatype);
			error = EINVAL;
			goto senderror;
		}
		break;
	case SADB_SATYPE_AH:
	case SADB_SATYPE_ESP:
	case SADB_X_SATYPE_IPCOMP:
	case SADB_X_SATYPE_TCPSIGNATURE:
		switch (msg->sadb_msg_type) {
		case SADB_X_SPDADD:
		case SADB_X_SPDDELETE:
		case SADB_X_SPDGET:
		case SADB_X_SPDDUMP:
		case SADB_X_SPDFLUSH:
		case SADB_X_SPDSETIDX:
		case SADB_X_SPDUPDATE:
		case SADB_X_SPDDELETE2:
			ipseclog((LOG_DEBUG, "%s: illegal satype=%u\n",
				__func__, msg->sadb_msg_type));
			PFKEYSTAT_INC(out_invsatype);
			error = EINVAL;
			goto senderror;
		}
		break;
	case SADB_SATYPE_RSVP:
	case SADB_SATYPE_OSPFV2:
	case SADB_SATYPE_RIPV2:
	case SADB_SATYPE_MIP:
		ipseclog((LOG_DEBUG, "%s: type %u isn't supported.\n",
			__func__, msg->sadb_msg_satype));
		PFKEYSTAT_INC(out_invsatype);
		error = EOPNOTSUPP;
		goto senderror;
	case 1:	/* XXX: What does it do? */
		if (msg->sadb_msg_type == SADB_X_PROMISC)
			break;
		/*FALLTHROUGH*/
	default:
		ipseclog((LOG_DEBUG, "%s: invalid type %u is passed.\n",
			__func__, msg->sadb_msg_satype));
		PFKEYSTAT_INC(out_invsatype);
		error = EINVAL;
		goto senderror;
	}

	/* check field of upper layer protocol and address family */
	if (mh.ext[SADB_EXT_ADDRESS_SRC] != NULL
	 && mh.ext[SADB_EXT_ADDRESS_DST] != NULL) {
		struct sadb_address *src0, *dst0;
		u_int plen;

		src0 = (struct sadb_address *)(mh.ext[SADB_EXT_ADDRESS_SRC]);
		dst0 = (struct sadb_address *)(mh.ext[SADB_EXT_ADDRESS_DST]);

		/* check upper layer protocol */
		if (src0->sadb_address_proto != dst0->sadb_address_proto) {
			ipseclog((LOG_DEBUG, "%s: upper layer protocol "
				"mismatched.\n", __func__));
			PFKEYSTAT_INC(out_invaddr);
			error = EINVAL;
			goto senderror;
		}

		/* check family */
		if (PFKEY_ADDR_SADDR(src0)->sa_family !=
		    PFKEY_ADDR_SADDR(dst0)->sa_family) {
			ipseclog((LOG_DEBUG, "%s: address family mismatched.\n",
				__func__));
			PFKEYSTAT_INC(out_invaddr);
			error = EINVAL;
			goto senderror;
		}
		if (PFKEY_ADDR_SADDR(src0)->sa_len !=
		    PFKEY_ADDR_SADDR(dst0)->sa_len) {
			ipseclog((LOG_DEBUG, "%s: address struct size "
				"mismatched.\n", __func__));
			PFKEYSTAT_INC(out_invaddr);
			error = EINVAL;
			goto senderror;
		}

		switch (PFKEY_ADDR_SADDR(src0)->sa_family) {
		case AF_INET:
			if (PFKEY_ADDR_SADDR(src0)->sa_len !=
			    sizeof(struct sockaddr_in)) {
				PFKEYSTAT_INC(out_invaddr);
				error = EINVAL;
				goto senderror;
			}
			break;
		case AF_INET6:
			if (PFKEY_ADDR_SADDR(src0)->sa_len !=
			    sizeof(struct sockaddr_in6)) {
				PFKEYSTAT_INC(out_invaddr);
				error = EINVAL;
				goto senderror;
			}
			break;
		default:
			ipseclog((LOG_DEBUG, "%s: unsupported address family\n",
				__func__));
			PFKEYSTAT_INC(out_invaddr);
			error = EAFNOSUPPORT;
			goto senderror;
		}

		switch (PFKEY_ADDR_SADDR(src0)->sa_family) {
		case AF_INET:
			plen = sizeof(struct in_addr) << 3;
			break;
		case AF_INET6:
			plen = sizeof(struct in6_addr) << 3;
			break;
		default:
			plen = 0;	/*fool gcc*/
			break;
		}

		/* check max prefix length */
		if (src0->sadb_address_prefixlen > plen ||
		    dst0->sadb_address_prefixlen > plen) {
			ipseclog((LOG_DEBUG, "%s: illegal prefixlen.\n",
				__func__));
			PFKEYSTAT_INC(out_invaddr);
			error = EINVAL;
			goto senderror;
		}

		/*
		 * prefixlen == 0 is valid because there can be a case when
		 * all addresses are matched.
		 */
	}

	if (msg->sadb_msg_type >= sizeof(key_typesw)/sizeof(key_typesw[0]) ||
	    key_typesw[msg->sadb_msg_type] == NULL) {
		PFKEYSTAT_INC(out_invmsgtype);
		error = EINVAL;
		goto senderror;
	}

	return (*key_typesw[msg->sadb_msg_type])(so, m, &mh);

senderror:
	msg->sadb_msg_errno = error;
	return key_sendup_mbuf(so, m, target);
}

static int
key_senderror(so, m, code)
	struct socket *so;
	struct mbuf *m;
	int code;
{
	struct sadb_msg *msg;

	IPSEC_ASSERT(m->m_len >= sizeof(struct sadb_msg),
		("mbuf too small, len %u", m->m_len));

	msg = mtod(m, struct sadb_msg *);
	msg->sadb_msg_errno = code;
	return key_sendup_mbuf(so, m, KEY_SENDUP_ONE);
}

/*
 * set the pointer to each header into message buffer.
 * m will be freed on error.
 * XXX larger-than-MCLBYTES extension?
 */
static int
key_align(m, mhp)
	struct mbuf *m;
	struct sadb_msghdr *mhp;
{
	struct mbuf *n;
	struct sadb_ext *ext;
	size_t off, end;
	int extlen;
	int toff;

	IPSEC_ASSERT(m != NULL, ("null mbuf"));
	IPSEC_ASSERT(mhp != NULL, ("null msghdr"));
	IPSEC_ASSERT(m->m_len >= sizeof(struct sadb_msg),
		("mbuf too small, len %u", m->m_len));

	/* initialize */
	bzero(mhp, sizeof(*mhp));

	mhp->msg = mtod(m, struct sadb_msg *);
	mhp->ext[0] = (struct sadb_ext *)mhp->msg;	/*XXX backward compat */

	end = PFKEY_UNUNIT64(mhp->msg->sadb_msg_len);
	extlen = end;	/*just in case extlen is not updated*/
	for (off = sizeof(struct sadb_msg); off < end; off += extlen) {
		n = m_pulldown(m, off, sizeof(struct sadb_ext), &toff);
		if (!n) {
			/* m is already freed */
			return ENOBUFS;
		}
		ext = (struct sadb_ext *)(mtod(n, caddr_t) + toff);

		/* set pointer */
		switch (ext->sadb_ext_type) {
		case SADB_EXT_SA:
		case SADB_EXT_ADDRESS_SRC:
		case SADB_EXT_ADDRESS_DST:
		case SADB_EXT_ADDRESS_PROXY:
		case SADB_EXT_LIFETIME_CURRENT:
		case SADB_EXT_LIFETIME_HARD:
		case SADB_EXT_LIFETIME_SOFT:
		case SADB_EXT_KEY_AUTH:
		case SADB_EXT_KEY_ENCRYPT:
		case SADB_EXT_IDENTITY_SRC:
		case SADB_EXT_IDENTITY_DST:
		case SADB_EXT_SENSITIVITY:
		case SADB_EXT_PROPOSAL:
		case SADB_EXT_SUPPORTED_AUTH:
		case SADB_EXT_SUPPORTED_ENCRYPT:
		case SADB_EXT_SPIRANGE:
		case SADB_X_EXT_POLICY:
		case SADB_X_EXT_SA2:
#ifdef IPSEC_NAT_T
		case SADB_X_EXT_NAT_T_TYPE:
		case SADB_X_EXT_NAT_T_SPORT:
		case SADB_X_EXT_NAT_T_DPORT:
		case SADB_X_EXT_NAT_T_OAI:
		case SADB_X_EXT_NAT_T_OAR:
		case SADB_X_EXT_NAT_T_FRAG:
#endif
			/* duplicate check */
			/*
			 * XXX Are there duplication payloads of either
			 * KEY_AUTH or KEY_ENCRYPT ?
			 */
			if (mhp->ext[ext->sadb_ext_type] != NULL) {
				ipseclog((LOG_DEBUG, "%s: duplicate ext_type "
					"%u\n", __func__, ext->sadb_ext_type));
				m_freem(m);
				PFKEYSTAT_INC(out_dupext);
				return EINVAL;
			}
			break;
		default:
			ipseclog((LOG_DEBUG, "%s: invalid ext_type %u\n",
				__func__, ext->sadb_ext_type));
			m_freem(m);
			PFKEYSTAT_INC(out_invexttype);
			return EINVAL;
		}

		extlen = PFKEY_UNUNIT64(ext->sadb_ext_len);

		if (key_validate_ext(ext, extlen)) {
			m_freem(m);
			PFKEYSTAT_INC(out_invlen);
			return EINVAL;
		}

		n = m_pulldown(m, off, extlen, &toff);
		if (!n) {
			/* m is already freed */
			return ENOBUFS;
		}
		ext = (struct sadb_ext *)(mtod(n, caddr_t) + toff);

		mhp->ext[ext->sadb_ext_type] = ext;
		mhp->extoff[ext->sadb_ext_type] = off;
		mhp->extlen[ext->sadb_ext_type] = extlen;
	}

	if (off != end) {
		m_freem(m);
		PFKEYSTAT_INC(out_invlen);
		return EINVAL;
	}

	return 0;
}

static int
key_validate_ext(ext, len)
	const struct sadb_ext *ext;
	int len;
{
	const struct sockaddr *sa;
	enum { NONE, ADDR } checktype = NONE;
	int baselen = 0;
	const int sal = offsetof(struct sockaddr, sa_len) + sizeof(sa->sa_len);

	if (len != PFKEY_UNUNIT64(ext->sadb_ext_len))
		return EINVAL;

	/* if it does not match minimum/maximum length, bail */
	if (ext->sadb_ext_type >= sizeof(minsize) / sizeof(minsize[0]) ||
	    ext->sadb_ext_type >= sizeof(maxsize) / sizeof(maxsize[0]))
		return EINVAL;
	if (!minsize[ext->sadb_ext_type] || len < minsize[ext->sadb_ext_type])
		return EINVAL;
	if (maxsize[ext->sadb_ext_type] && len > maxsize[ext->sadb_ext_type])
		return EINVAL;

	/* more checks based on sadb_ext_type XXX need more */
	switch (ext->sadb_ext_type) {
	case SADB_EXT_ADDRESS_SRC:
	case SADB_EXT_ADDRESS_DST:
	case SADB_EXT_ADDRESS_PROXY:
		baselen = PFKEY_ALIGN8(sizeof(struct sadb_address));
		checktype = ADDR;
		break;
	case SADB_EXT_IDENTITY_SRC:
	case SADB_EXT_IDENTITY_DST:
		if (((const struct sadb_ident *)ext)->sadb_ident_type ==
		    SADB_X_IDENTTYPE_ADDR) {
			baselen = PFKEY_ALIGN8(sizeof(struct sadb_ident));
			checktype = ADDR;
		} else
			checktype = NONE;
		break;
	default:
		checktype = NONE;
		break;
	}

	switch (checktype) {
	case NONE:
		break;
	case ADDR:
		sa = (const struct sockaddr *)(((const u_int8_t*)ext)+baselen);
		if (len < baselen + sal)
			return EINVAL;
		if (baselen + PFKEY_ALIGN8(sa->sa_len) != len)
			return EINVAL;
		break;
	}

	return 0;
}

void
key_init(void)
{
	int i;

	for (i = 0; i < IPSEC_DIR_MAX; i++)
		LIST_INIT(&V_sptree[i]);

	LIST_INIT(&V_sahtree);

	for (i = 0; i <= SADB_SATYPE_MAX; i++)
		LIST_INIT(&V_regtree[i]);

	LIST_INIT(&V_acqtree);
	LIST_INIT(&V_spacqtree);

	/* system default */
	V_ip4_def_policy.policy = IPSEC_POLICY_NONE;
	V_ip4_def_policy.refcnt++;	/*never reclaim this*/

	if (!IS_DEFAULT_VNET(curvnet))
		return;

	SPTREE_LOCK_INIT();
	REGTREE_LOCK_INIT();
	SAHTREE_LOCK_INIT();
	ACQ_LOCK_INIT();
	SPACQ_LOCK_INIT();

#ifndef IPSEC_DEBUG2
	timeout((void *)key_timehandler, (void *)0, hz);
#endif /*IPSEC_DEBUG2*/

	/* initialize key statistics */
	keystat.getspi_count = 1;

	printf("IPsec: Initialized Security Association Processing.\n");
}

#ifdef VIMAGE
void
key_destroy(void)
{
	struct secpolicy *sp, *nextsp;
	struct secacq *acq, *nextacq;
	struct secspacq *spacq, *nextspacq;
	struct secashead *sah, *nextsah;
	struct secreg *reg;
	int i;

	SPTREE_LOCK();
	for (i = 0; i < IPSEC_DIR_MAX; i++) {
		for (sp = LIST_FIRST(&V_sptree[i]); 
		    sp != NULL; sp = nextsp) {
			nextsp = LIST_NEXT(sp, chain);
			if (__LIST_CHAINED(sp)) {
				LIST_REMOVE(sp, chain);
				free(sp, M_IPSEC_SP);
			}
		}
	}
	SPTREE_UNLOCK();

	SAHTREE_LOCK();
	for (sah = LIST_FIRST(&V_sahtree); sah != NULL; sah = nextsah) {
		nextsah = LIST_NEXT(sah, chain);
		if (__LIST_CHAINED(sah)) {
			LIST_REMOVE(sah, chain);
			free(sah, M_IPSEC_SAH);
		}
	}
	SAHTREE_UNLOCK();

	REGTREE_LOCK();
	for (i = 0; i <= SADB_SATYPE_MAX; i++) {
		LIST_FOREACH(reg, &V_regtree[i], chain) {
			if (__LIST_CHAINED(reg)) {
				LIST_REMOVE(reg, chain);
				free(reg, M_IPSEC_SAR);
				break;
			}
		}
	}
	REGTREE_UNLOCK();

	ACQ_LOCK();
	for (acq = LIST_FIRST(&V_acqtree); acq != NULL; acq = nextacq) {
		nextacq = LIST_NEXT(acq, chain);
		if (__LIST_CHAINED(acq)) {
			LIST_REMOVE(acq, chain);
			free(acq, M_IPSEC_SAQ);
		}
	}
	ACQ_UNLOCK();

	SPACQ_LOCK();
	for (spacq = LIST_FIRST(&V_spacqtree); spacq != NULL;
	    spacq = nextspacq) {
		nextspacq = LIST_NEXT(spacq, chain);
		if (__LIST_CHAINED(spacq)) {
			LIST_REMOVE(spacq, chain);
			free(spacq, M_IPSEC_SAQ);
		}
	}
	SPACQ_UNLOCK();
}
#endif

/*
 * XXX: maybe This function is called after INBOUND IPsec processing.
 *
 * Special check for tunnel-mode packets.
 * We must make some checks for consistency between inner and outer IP header.
 *
 * xxx more checks to be provided
 */
int
key_checktunnelsanity(sav, family, src, dst)
	struct secasvar *sav;
	u_int family;
	caddr_t src;
	caddr_t dst;
{
	IPSEC_ASSERT(sav->sah != NULL, ("null SA header"));

	/* XXX: check inner IP header */

	return 1;
}

/* record data transfer on SA, and update timestamps */
void
key_sa_recordxfer(sav, m)
	struct secasvar *sav;
	struct mbuf *m;
{
	IPSEC_ASSERT(sav != NULL, ("Null secasvar"));
	IPSEC_ASSERT(m != NULL, ("Null mbuf"));
	if (!sav->lft_c)
		return;

	/*
	 * XXX Currently, there is a difference of bytes size
	 * between inbound and outbound processing.
	 */
	sav->lft_c->bytes += m->m_pkthdr.len;
	/* to check bytes lifetime is done in key_timehandler(). */

	/*
	 * We use the number of packets as the unit of
	 * allocations.  We increment the variable
	 * whenever {esp,ah}_{in,out}put is called.
	 */
	sav->lft_c->allocations++;
	/* XXX check for expires? */

	/*
	 * NOTE: We record CURRENT usetime by using wall clock,
	 * in seconds.  HARD and SOFT lifetime are measured by the time
	 * difference (again in seconds) from usetime.
	 *
	 *	usetime
	 *	v     expire   expire
	 * -----+-----+--------+---> t
	 *	<--------------> HARD
	 *	<-----> SOFT
	 */
	sav->lft_c->usetime = time_second;
	/* XXX check for expires? */

	return;
}

/* dumb version */
void
key_sa_routechange(dst)
	struct sockaddr *dst;
{
	struct secashead *sah;
	struct route *ro;

	SAHTREE_LOCK();
	LIST_FOREACH(sah, &V_sahtree, chain) {
		ro = &sah->route_cache.sa_route;
		if (ro->ro_rt && dst->sa_len == ro->ro_dst.sa_len
		 && bcmp(dst, &ro->ro_dst, dst->sa_len) == 0) {
			RTFREE(ro->ro_rt);
			ro->ro_rt = (struct rtentry *)NULL;
		}
	}
	SAHTREE_UNLOCK();
}

static void
key_sa_chgstate(struct secasvar *sav, u_int8_t state)
{
	IPSEC_ASSERT(sav != NULL, ("NULL sav"));
	SAHTREE_LOCK_ASSERT();

	if (sav->state != state) {
		if (__LIST_CHAINED(sav))
			LIST_REMOVE(sav, chain);
		sav->state = state;
		LIST_INSERT_HEAD(&sav->sah->savtree[state], sav, chain);
	}
}

void
key_sa_stir_iv(sav)
	struct secasvar *sav;
{

	IPSEC_ASSERT(sav->iv != NULL, ("null IV"));
	key_randomfill(sav->iv, sav->ivlen);
}

/*
 * Take one of the kernel's security keys and convert it into a PF_KEY
 * structure within an mbuf, suitable for sending up to a waiting
 * application in user land.
 * 
 * IN: 
 *    src: A pointer to a kernel security key.
 *    exttype: Which type of key this is. Refer to the PF_KEY data structures.
 * OUT:
 *    a valid mbuf or NULL indicating an error
 *
 */

static struct mbuf *
key_setkey(struct seckey *src, u_int16_t exttype) 
{
	struct mbuf *m;
	struct sadb_key *p;
	int len;

	if (src == NULL)
		return NULL;

	len = PFKEY_ALIGN8(sizeof(struct sadb_key) + _KEYLEN(src));
	m = m_get2(len, M_NOWAIT, MT_DATA, 0);
	if (m == NULL)
		return NULL;
	m_align(m, len);
	m->m_len = len;
	p = mtod(m, struct sadb_key *);
	bzero(p, len);
	p->sadb_key_len = PFKEY_UNIT64(len);
	p->sadb_key_exttype = exttype;
	p->sadb_key_bits = src->bits;
	bcopy(src->key_data, _KEYBUF(p), _KEYLEN(src));

	return m;
}

/*
 * Take one of the kernel's lifetime data structures and convert it
 * into a PF_KEY structure within an mbuf, suitable for sending up to
 * a waiting application in user land.
 * 
 * IN: 
 *    src: A pointer to a kernel lifetime structure.
 *    exttype: Which type of lifetime this is. Refer to the PF_KEY 
 *             data structures for more information.
 * OUT:
 *    a valid mbuf or NULL indicating an error
 *
 */

static struct mbuf *
key_setlifetime(struct seclifetime *src, u_int16_t exttype)
{
	struct mbuf *m = NULL;
	struct sadb_lifetime *p;
	int len = PFKEY_ALIGN8(sizeof(struct sadb_lifetime));

	if (src == NULL)
		return NULL;

	m = m_get2(len, M_NOWAIT, MT_DATA, 0);
	if (m == NULL)
		return m;
	m_align(m, len);
	m->m_len = len;
	p = mtod(m, struct sadb_lifetime *);

	bzero(p, len);
	p->sadb_lifetime_len = PFKEY_UNIT64(len);
	p->sadb_lifetime_exttype = exttype;
	p->sadb_lifetime_allocations = src->allocations;
	p->sadb_lifetime_bytes = src->bytes;
	p->sadb_lifetime_addtime = src->addtime;
	p->sadb_lifetime_usetime = src->usetime;
	
	return m;

}
