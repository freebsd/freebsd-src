
#if !defined(lint) && !defined(SABER)
static const char rcsid[] = "$Id: db_sec.c,v 8.35 2001/06/18 14:42:57 marka Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1986, 1990
 *    The Regents of the University of California.  All rights reserved.
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
 *    must display the following acknowledgement:
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * Portions Copyright (c) 1996-2000 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#include "port_before.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <ctype.h>
#include <resolv.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

#include <isc/eventlib.h>
#include <isc/logging.h>
#include <isc/memcluster.h>
#include <isc/tree.h>

#include <isc/dst.h>

#include "port_after.h"

#include "named.h"

struct zpubkey {
	struct dst_key *zpk_key; /* Should be DST_KEY */
	char *zpk_name;
	struct zpubkey *zpk_next;
};

typedef struct zpubkey *zpubkey_list;

static int nxt_match_rrset(struct databuf *dp, struct db_rrset *rrset);

/*
 * A converted databuf is a stripped down databuf after converting the 
 * data to wire format.
 */
struct converted_databuf {
	struct converted_databuf *cd_next;
	u_char *cd_data;
	int cd_size, cd_alloc;
};

/* All of the trusted keys and zone keys */
static tree *trusted_keys = NULL;

static int
compare_pubkey (struct zpubkey *zpk1, struct zpubkey *zpk2) {
	char ta[NS_MAXDNAME], tb[NS_MAXDNAME];

	if (ns_makecanon(zpk1->zpk_name, ta, sizeof ta) < 0 ||
	    ns_makecanon(zpk2->zpk_name, tb, sizeof tb) < 0)
                return (-1);
        return (strcasecmp(ta, tb));
}

static struct zpubkey *
tree_srch_pubkey (const char *name) {
	struct zpubkey tkey, *key;

	DE_CONST(name, tkey.zpk_name);
	if (trusted_keys == NULL) {
		tree_init(&trusted_keys);
		return (NULL);
	}
	key = (struct zpubkey *)tree_srch(&trusted_keys, compare_pubkey,
					  &tkey);
	return (key);
}

static DST_KEY *
find_public_key (const char *name, u_int16_t key_id) {
	struct namebuf *knp;
	struct hashbuf *htp;
	struct databuf *dp;
	const char *fname;
	DST_KEY *key;

	ns_debug(ns_log_default, 5, "find_public_key(%s, %d)", name, key_id);

	htp = hashtab;
	knp = nlookup (name, &htp, &fname, 0);
	if (fname != name)
		/* The name doesn't exist, so there's no key */
		return (NULL);

	for (dp = knp->n_data; dp != NULL; dp = dp->d_next) {
		if (dp->d_type != ns_t_key || dp->d_secure < DB_S_SECURE)
			continue;
		key = dst_dnskey_to_key(name, dp->d_data, dp->d_size);
		/* XXX what about multiple keys with same footprint? */
		if (key) {
			if (key->dk_id == ntohs(key_id))
				return (key);
			else
				dst_free_key(key);
		}
	}
	return (NULL);
}


static DST_KEY *
find_trusted_key (const char *name, u_int16_t key_id) {
	struct zpubkey *zpk;
	zpubkey_list keylist = tree_srch_pubkey (name);

	ns_debug(ns_log_default, 5, "find_trusted_key(%s, %d)", name, key_id);

	for (zpk = keylist; zpk; zpk = zpk->zpk_next)
		if (zpk->zpk_key->dk_id == ntohs(key_id))
			return (zpk->zpk_key);

	return (NULL);
}

int
add_trusted_key (const char *name, const int flags, const int proto,
		 const int alg, const char *str)
{
	zpubkey_list keylist;
	struct zpubkey *zpk;
	u_char buf[1024];
	int n;

	keylist = tree_srch_pubkey (name);

	zpk = (struct zpubkey *) memget (sizeof (struct zpubkey));
	if (zpk == NULL)
		ns_panic(ns_log_default, 1,
			 "add_trusted_key: memget failed(%s)", name);
	n = b64_pton(str, buf, sizeof(buf));
	if (n < 0)
		goto failure;
	zpk->zpk_key = dst_buffer_to_key(name, alg, flags, proto, buf, n);
	if (zpk->zpk_key == NULL) {
		ns_warning(ns_log_default,
			   "add_trusted_key: dst_buffer_to_key(%s) failed",
			   name);
		goto failure;
	}
	zpk->zpk_name = zpk->zpk_key->dk_key_name;
	zpk->zpk_next = NULL;

	if (keylist == NULL) {
		if (tree_add (&trusted_keys, compare_pubkey, zpk, NULL) == NULL)
			goto failure;
	}
	else {
		struct zpubkey *tkey = keylist;
		while (tkey->zpk_next)
			tkey = tkey->zpk_next;
		tkey->zpk_next = zpk;
	}

	return (1);
 failure:
	memput(zpk, sizeof (struct zpubkey));
	return (0);
}

/* Can the signer sign records for this name?  This is a heuristic. */
static int
can_sign(const char *name, const char *signer) {
	return (ns_samedomain(name, signer) &&
		dn_count_labels(name) - dn_count_labels(signer) <= 2);
}

static int
rrset_set_security(struct db_rrset *rrset, int slev) {
	struct dnode *dnp;

	for (dnp = rrset->rr_list; dnp != NULL; dnp = dnp->dn_next)
		dnp->dp->d_secure = slev;
	for (dnp = rrset->rr_sigs; dnp != NULL; dnp = dnp->dn_next)
		dnp->dp->d_secure = slev;
	return (slev);
}

static int
convert_databuf(struct databuf *dp, struct converted_databuf *cdp) {
	u_char *bp = cdp->cd_data;
	u_char *cp = dp->d_data;
	u_char *eob = cdp->cd_data + cdp->cd_alloc;
	int len;
	u_char buf[MAXDNAME];

	switch (dp->d_type) {
	    case ns_t_soa:
	    case ns_t_minfo:
	    case ns_t_rp:
		if (eob - bp < (int)strlen((char *)cp) + 1)
			return (-1);
		if (ns_name_pton((char *)cp, buf, sizeof buf) < 0)
			return (-1);
		len = ns_name_ntol(buf, bp, eob - bp);
		if (len < 0)
			return (-1);
		bp += len;
		cp += strlen((char *)cp) + 1;

		if (eob - bp < (int)strlen((char *)cp) + 1)
			return (-1);
		if (ns_name_pton((char *)cp, buf, sizeof buf) < 0)
			return (-1);
		len = ns_name_ntol(buf, bp, eob - bp);
		if (len < 0)
			return (-1);
		bp += len;
		cp += strlen((char *)cp) + 1;

		if (dp->d_type == ns_t_soa) {
			if (eob - bp < 5 * INT32SZ)
				return (-1);
			memcpy(bp, cp, 5 * INT32SZ);
			bp += (5 * INT32SZ);
			cp += (5 * INT32SZ);
		}

		break;

	    case ns_t_ns:
	    case ns_t_cname:
	    case ns_t_mb:
	    case ns_t_mg:
	    case ns_t_mr:
	    case ns_t_ptr:
	    case ns_t_nxt:
		if (eob - bp < (int)strlen((char *)cp) + 1)
			return (-1);
		if (ns_name_pton((char *)cp, buf, sizeof buf) < 0)
			return (-1);
		len = ns_name_ntol(buf, bp, eob - bp);
		if (len < 0)
			return (-1);
		bp += len;
		cp += (len = strlen((char *)cp) + 1);

		if (dp->d_type == ns_t_nxt) {
			if (eob - bp < dp->d_size - len)
				return (-1);
			memcpy(bp, cp, dp->d_size - len);
			bp += (dp->d_size - len);
			cp += (dp->d_size - len);
		}
		break;

	    case ns_t_srv:
		if (eob - bp < 2 * INT16SZ)
			return (-1);
		memcpy(bp, cp, 2 * INT16SZ);
		bp += (2 * INT16SZ);
		cp += (2 * INT16SZ);
		/* no break */
	    case ns_t_rt:
	    case ns_t_mx:
	    case ns_t_afsdb:
	    case ns_t_px:
		if (eob - bp < INT16SZ)
			return (-1);
		memcpy (bp, cp, INT16SZ);
		bp += INT16SZ;
		cp += INT16SZ;

		if (eob - bp < (int)strlen((char *)cp) + 1)
			return (-1);
		if (ns_name_pton((char *)cp, buf, sizeof buf) < 0)
			return (-1);
		len = ns_name_ntol(buf, bp, eob - bp);
		if (len < 0)
			return (-1);
		bp += len;
		cp += strlen((char *)cp) + 1;

		if (dp->d_type == ns_t_px) {
			if (eob - bp < (int)strlen((char *)cp) + 1)
				return (-1);
			if (ns_name_pton((char *)cp, buf, sizeof buf) < 0)
				return (-1);
			len = ns_name_ntol(buf, bp, eob - bp);
			if (len < 0)
				return (-1);
			bp += len;
			cp += strlen((char *)cp) + 1;
		}
		break;

	    default:
		if (eob - bp < dp->d_size)
			return (-1);
		memcpy(bp, cp, dp->d_size);
		bp += dp->d_size;
	}
	cdp->cd_size = bp - cdp->cd_data;
	return (cdp->cd_size);
}

static int
digest_rr(char *envelope, int elen, struct converted_databuf *cdp,
	  char *buffer, int blen)
{
	char *bp = buffer, *eob = buffer + blen;

	if (eob - bp < elen)
		return (-1);
	memcpy (bp, envelope, elen);
	bp += elen;

	if (eob - bp < INT16SZ)
		return (-1);
	PUTSHORT(cdp->cd_size, bp);

	if (eob - bp < cdp->cd_size)
		return (-1);
	memcpy (bp, cdp->cd_data, cdp->cd_size);
	bp += cdp->cd_size;

	return (bp - buffer);
}

/* Sorts the converted databuf in the list */
static void
insert_converted_databuf(struct converted_databuf *cdp,
			struct converted_databuf **clist)
{
	struct converted_databuf *tcdp, *next;
	int t;

#define compare_cdatabuf(c1, c2, t) \
	(t = memcmp(c1->cd_data, c2->cd_data, MIN(c1->cd_size, c2->cd_size)), \
	 t == 0 ? c1->cd_size - c2->cd_size : t)

	if (*clist == NULL) {
		*clist = cdp;
		return;
	}

	tcdp = *clist;
	if (compare_cdatabuf(cdp, tcdp, t) < 0) {
		cdp->cd_next = tcdp;
		*clist = cdp;
		return;
	}

	next = tcdp->cd_next;
	while (next) {
		if (compare_cdatabuf(cdp, next, t) < 0) {
			cdp->cd_next = next;
			tcdp->cd_next = cdp;
			return;
		}
		tcdp = next;
		next = next->cd_next;
	}
	tcdp->cd_next = cdp;
#undef compare_cdatabuf
}

static void
free_clist(struct converted_databuf *clist) {
	struct converted_databuf *cdp;

	while (clist != NULL) {
		cdp = clist;
		clist = clist->cd_next;
		memput(cdp->cd_data, cdp->cd_alloc);
		memput(cdp, sizeof(struct converted_databuf));
	}
}

/* Removes all empty nodes from an rrset's SIG list. */
static void
rrset_trim_sigs(struct db_rrset *rrset) {
	struct dnode *dnp, *odnp, *ndnp;

	odnp = NULL;
	dnp = rrset->rr_sigs;
	while (dnp != NULL) {
		if (dnp->dp != NULL) {
			odnp = dnp;
			dnp = dnp->dn_next;
		}
		else {
			if (odnp != NULL)
				odnp->dn_next = dnp->dn_next;
			else
				rrset->rr_sigs = dnp->dn_next;
			ndnp = dnp->dn_next;
			memput(dnp, sizeof(struct dnode));
			dnp = ndnp;
		}
	}
}

static int
verify_set(struct db_rrset *rrset) {
	DST_KEY *key = NULL;
	struct sig_record *sigdata;
	struct dnode *sigdn;
	struct databuf *sigdp;
	time_t now;
	char *signer;
	u_char name_n[MAXDNAME];
	u_char *sig, *eom;
	int trustedkey = 0, siglen, labels, len = 0, ret;
	u_char *buffer = NULL, *bp;
	u_char envelope[MAXDNAME+32], *ep;
	struct dnode *dnp;
	int bufsize = 2048; /* Large enough for MAXDNAME + SIG_HDR_SIZE */
	struct converted_databuf *clist = NULL, *cdp;
	int dnssec_failed = 0, dnssec_succeeded = 0;
	int return_value;
	int i;

	if (rrset == NULL || rrset->rr_name == NULL) {
		ns_warning (ns_log_default, "verify_set: missing rrset/name");
		return (rrset_set_security(rrset, DB_S_FAILED));
	}

	if (rrset->rr_sigs == NULL)
		return (rrset_set_security(rrset, DB_S_INSECURE));

	ns_debug(ns_log_default, 5, "verify_set(%s, %s, %s)", rrset->rr_name,
		 p_type(rrset->rr_type), p_class(rrset->rr_class));

	now = time(NULL);

	for (sigdn = rrset->rr_sigs; sigdn != NULL; sigdn = sigdn->dn_next) {
		u_int32_t namefield;
		struct sig_record sigrec;

		sigdp = sigdn->dp;

		eom = sigdp->d_data + sigdp->d_size;
		if (sigdp->d_size < SIG_HDR_SIZE) {
			return_value = DB_S_FAILED;
			goto end;
		}
		memcpy(&sigrec, sigdp->d_data, SIG_HDR_SIZE);
		sigdata = &sigrec;
		signer = (char *)sigdp->d_data + SIG_HDR_SIZE;
		sig = (u_char *)signer + strlen(signer) + 1;
		siglen = eom - sig;

		/*
		 * Don't verify a set if the SIG inception time is in
		 * the future.  This should be fixed before 2038 (BEW)
		 */
		if ((time_t)ntohl(sigdata->sig_time_n) > now)
			continue;

		/* An expired set is dropped, but the data is not. */
		if ((time_t)ntohl(sigdata->sig_exp_n) < now) {
			db_detach(&sigdn->dp);
			sigdp = NULL;
			continue;
		}

		/* Cleanup from the last iteration if we continue'd */
		if (trustedkey == 0 && key != NULL)
			dst_free_key(key);

		key = find_trusted_key(signer, sigdata->sig_keyid_n);

		if (key == NULL) {
			trustedkey = 0;
			key = find_public_key(signer, sigdata->sig_keyid_n);
		}
		else    
			trustedkey = 1;

		/* if we don't have the key, either
		 *   - the data should be considered insecure
		 *   - the sig is not a dnssec signature
		 */
		if (key == NULL)
			continue;

		/* Can a key with this name sign the data? */
		if (!can_sign(rrset->rr_name, signer))
			continue;

		/* Check the protocol and flags of the key */
		if (key->dk_proto != NS_KEY_PROT_DNSSEC &&
		    key->dk_proto != NS_KEY_PROT_ANY)
			continue;
		if (key->dk_flags & NS_KEY_NO_AUTH)
			continue;
		namefield = key->dk_flags & NS_KEY_NAME_TYPE;
		if (namefield == NS_KEY_NAME_USER ||
		    namefield == NS_KEY_NAME_RESERVED)
			continue;
		if (namefield == NS_KEY_NAME_ENTITY &&
		    (key->dk_flags & NS_KEY_SIGNATORYMASK) == 0)
			continue;

		/*
		 * If we're still here, we have a non-null key that's either
		 * a zone key or an entity key with signing authority.
		 */

		if (buffer == NULL) {
			bp = buffer = memget(bufsize);
			if (bp == NULL) {
				return_value = DB_S_FAILED;
				goto end;
			}
		}
		else
			bp = buffer;


		/* Digest the fixed portion of the SIG record */
		memcpy(bp, (char *) sigdata, SIG_HDR_SIZE);
		bp += SIG_HDR_SIZE;

		/* Digest the signer's name, canonicalized */
		if (ns_name_pton(signer, name_n, sizeof name_n) < 0) {
			return_value = DB_S_FAILED;
			goto end;
		}
		i = ns_name_ntol(name_n, (u_char *)bp, bufsize - SIG_HDR_SIZE);
		if (i < 0) {
			return_value = DB_S_FAILED;
			goto end;
		}
		bp += i;

		/* create the dns record envelope:
		 *     <name><type><class><Original TTL>
		 */
		if (ns_name_pton(rrset->rr_name, name_n, sizeof name_n) < 0 ||
		    ns_name_ntol(name_n, (u_char *)envelope, sizeof envelope) < 0) {
			return_value = DB_S_FAILED;
			goto end;
		}

		labels = dn_count_labels(rrset->rr_name);
		if (labels > sigdata->sig_labels_n) {
			ep = envelope;
			for (i=0; i < (labels - 1 - sigdata->sig_labels_n); i++)
				ep += (*ep+1);
			i = dn_skipname(ep, envelope + sizeof envelope);
			if (i < 0) {
				return_value = DB_S_FAILED;
				goto end;
			}
			envelope[0] = '\001';
			envelope[1] = '*';
			memmove(envelope + 2, ep, i);
		}
		i = dn_skipname(envelope, envelope + sizeof envelope);
		if (i < 0) {
			return_value = DB_S_FAILED;
			goto end;
		}
		ep = envelope + i;
		PUTSHORT (rrset->rr_type, ep);
		PUTSHORT (rrset->rr_class, ep);
		if (envelope + sizeof(envelope) - ep < INT32SZ) {
			return_value = DB_S_FAILED;
			goto end;
		}
		memcpy (ep, &sigdata->sig_ottl_n, INT32SZ);
		ep += INT32SZ;

		if (clist == NULL) {
			for (dnp = rrset->rr_list;
			     dnp != NULL;
			     dnp = dnp->dn_next)
			{
				struct databuf *dp = dnp->dp;

				cdp = memget(sizeof(struct converted_databuf));
				if (cdp == NULL) {
					return_value = DB_S_FAILED;
					goto end;
				}
				memset(cdp, 0, sizeof(*cdp));
				/* Should be large enough... */
				cdp->cd_alloc = dp->d_size + 8;
				cdp->cd_data = memget(cdp->cd_alloc);
				if (cdp->cd_data == NULL) {
					memput(cdp, sizeof(*cdp));
					return_value = DB_S_FAILED;
					goto end;
				}
				while (convert_databuf(dp, cdp) < 0) {
					memput(cdp->cd_data, cdp->cd_alloc);
					cdp->cd_alloc *= 2;
					cdp->cd_data = memget(cdp->cd_alloc);
					if (cdp->cd_data == NULL) {
						memput(cdp, sizeof(*cdp));
						return_value = DB_S_FAILED;
						goto end;
					}
				}
				insert_converted_databuf(cdp, &clist);
			}
		}

		for (cdp = clist; cdp != NULL; cdp = cdp->cd_next) {
			len = digest_rr((char *)envelope, ep-envelope, cdp,
					(char *)bp, bufsize - (bp - buffer));
			while (len < 0) {
				u_char *newbuf;

				/* Double the buffer size */
				newbuf = memget(bufsize*2);
				if (newbuf == NULL) {
					return_value = DB_S_FAILED;
					goto end;
				}
				memcpy(newbuf, buffer, bp - buffer);
				bp = (bp - buffer) + newbuf;
				memput(buffer, bufsize);
				buffer = newbuf;
				bufsize *= 2;

				len = digest_rr((char *)envelope, ep-envelope,
						cdp, (char *)bp,
						bufsize - (bp - buffer));
			}
			bp += len;
		}

		if (len < 0) {
			return_value = DB_S_FAILED;
			goto end;
		}

		ret = dst_verify_data(SIG_MODE_ALL, key, NULL, buffer,
				      bp - buffer, sig, siglen);

		if (ret < 0) {
			dnssec_failed++;
			db_detach(&sigdn->dp);
			sigdp = NULL;
		}
		else
			dnssec_succeeded++;
	}

end:
	if (dnssec_failed > 0)
		rrset_trim_sigs(rrset);
	if (trustedkey == 0 && key != NULL)
		dst_free_key(key);

	if (dnssec_failed > 0 && dnssec_succeeded == 0) {
		ns_warning (ns_log_default,
			    "verify_set(%s, %s, %s) failed",
			    rrset->rr_name, p_type(rrset->rr_type),
			    p_class(rrset->rr_class));
		return_value = DB_S_FAILED;
	}
	else if (dnssec_succeeded > 0)
		return_value = DB_S_SECURE;
	else
		return_value = DB_S_INSECURE;
	free_clist(clist);
	if (buffer != NULL)
		memput(buffer, bufsize);
	return (rrset_set_security(rrset, return_value));
}

static void
rrset_free(struct db_rrset *rrset) {
	struct dnode *dnp;

	ns_debug(ns_log_default, 5, "rrset_free(%s)", rrset->rr_name);

	while (rrset->rr_list) {
		dnp = rrset->rr_list;
		rrset->rr_list = rrset->rr_list->dn_next;
		if (dnp->dp != NULL)
			db_detach(&dnp->dp);
		memput(dnp, sizeof(struct dnode));
	}
	while (rrset->rr_sigs) {
		dnp = rrset->rr_sigs;
		rrset->rr_sigs = rrset->rr_sigs->dn_next;
		if (dnp->dp != NULL)
			db_detach(&dnp->dp);
		memput(dnp, sizeof(struct dnode));
	}
}

/*
 * This is called when we have an rrset with SIGs and no other data.
 * Returns 1 if we either found the necessary data or if the SIG can be added
 * with no other data.  0 indicates that the SIG cannot be added.
 */
static int
attach_data(struct db_rrset *rrset) {
	int type, class;
	struct databuf *dp, *newdp, *sigdp;
	struct dnode *dnp;
	struct namebuf *np;
	struct hashbuf *htp;
	char *signer;
	const char *fname;
	char *name = rrset->rr_name;

	sigdp = rrset->rr_sigs->dp;

	type = SIG_COVERS(sigdp);
	class = sigdp->d_class;
	signer = (char *)(sigdp + SIG_HDR_SIZE);

	/* First, see if the signer can sign data for the name.  If not,
	 * it's not a DNSSEC signature, so we can insert it with no
	 * corresponding data.
	 */
	if (!can_sign(name, signer))
		return (1);

	htp = hashtab;
	np = nlookup (name, &htp, &fname, 0);
	if (fname != name)
		return (0);

	for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
		if (dp->d_type == type && dp->d_class == class) {
			newdp = savedata(class, type, dp->d_ttl, dp->d_data,
					 dp->d_size);
			dnp = (struct dnode *) memget (sizeof (struct dnode));
			if (dnp == NULL)
				ns_panic(ns_log_default, 1,
					 "attach_data: memget failed");
			dnp->dp = newdp;
			dnp->dn_next = rrset->rr_list;
			rrset->rr_list = dnp;
		}
	}
	if (rrset->rr_list != NULL)
		return (1);
	else
		return (0);
}

static int
rrset_db_update(struct db_rrset *rrset, int flags, struct hashbuf **htpp,
		struct sockaddr_in from, int *rrcount)
{
	struct dnode *dnp;
	int ret;

	/* If we have any unattached SIG records that are DNSSEC signatures,
	 * don't cache them unless we already have the corresponding data.
	 * If we do cache unattached SIGs, we run into problems later if we
	 * have a SIG X and get a query for type X.
	 */ 
	if (rrset->rr_list == NULL) {
		if (attach_data(rrset) == 0) {
			rrset_free(rrset);
			return (OK);
		}

		if (rrset->rr_list != NULL &&
		    verify_set(rrset) == DB_S_FAILED)
		{
			rrset_free(rrset);
			return (OK);
		}
	}

	for (dnp = rrset->rr_list; dnp != NULL; dnp = dnp->dn_next) {
		ret = db_update(rrset->rr_name, dnp->dp, dnp->dp, NULL,
				flags, (*htpp), from);
		if (ret != OK) {
			/* XXX Probably should do rollback.  */
			db_err(ret, rrset->rr_name, dnp->dp->d_type,
			       dnp->file, dnp->line);
			if (ret != DATAEXISTS) {
				rrset_free(rrset);
				return (ret);
			}
		}
		if (rrcount != NULL)
			(*rrcount)++;
	}
	for (dnp = rrset->rr_sigs; dnp != NULL; dnp = dnp->dn_next) {
		if (dnp->dp == NULL) /* verifyset() can remove sigs */
			continue;
		ret = db_update(rrset->rr_name, dnp->dp, dnp->dp, NULL,
				flags, (*htpp), from);
		if (ret != OK) {
			/* XXX Probably should do rollback.  */
			db_err(ret, rrset->rr_name, dnp->dp->d_type,
			       dnp->file, dnp->line);
			if (ret != DATAEXISTS) {
				rrset_free(rrset);
				return (ret);
			}
		}
		if (rrcount != NULL)
			(*rrcount)++;
	}
	rrset_free(rrset);
	return (OK);
}

static int
rr_in_set(struct databuf *rr, struct dnode *set) {
	struct dnode *dnp;

	if (set == NULL)
		return (0);

	for(dnp = set; dnp != NULL; dnp = dnp->dn_next) {
		if (dnp->dp->d_size == rr->d_size &&
		    memcmp(dnp->dp->d_data, rr->d_data, dnp->dp->d_size) == 0)
			return (1);
	}
	return (0);
}

static int
add_to_rrset_list(struct db_rrset **rrsets, char *name, struct databuf *dp,
		  int line, const char *file)
{
	struct db_rrset *rrset = *rrsets;
	struct dnode *dnp;

	while (rrset != NULL) {
		if (rrset->rr_type != ns_t_nxt || dp->d_type != ns_t_nxt) {
			if (dp->d_type == ns_t_sig) {
				if ((int)SIG_COVERS(dp) == rrset->rr_type)
					break;
			} else {
				if (dp->d_type == rrset->rr_type)
					break;
			}
		}
		else if (nxt_match_rrset(dp, rrset))
			break;
		rrset = rrset->rr_next;
	}

	if (rrset != NULL) {
		if ((dp->d_type == ns_t_sig && rr_in_set(dp, rrset->rr_sigs)) ||
		    (dp->d_type != ns_t_sig && rr_in_set(dp, rrset->rr_list)))
			return (DATAEXISTS);
	} else {
		rrset = (struct db_rrset *) memget(sizeof(struct db_rrset));
		if (rrset == NULL)
			 ns_panic(ns_log_default, 1,
				  "add_to_rrset_list: memget failed(%s)", name);
		memset(rrset, 0, sizeof(struct db_rrset));
		rrset->rr_name = savestr(name, 1);
		rrset->rr_class = dp->d_class;
		if (dp->d_type == ns_t_sig)
			rrset->rr_type = SIG_COVERS(dp);
		else
			rrset->rr_type = dp->d_type;
		rrset->rr_next = *rrsets;
		*rrsets = rrset;
	}

	dnp = (struct dnode *) memget(sizeof(struct dnode));
	if (dnp == NULL)
		ns_panic(ns_log_default, 1,
			 "add_to_rrset_list: memget failed(%s)", name);
	memset(dnp, 0, sizeof(struct dnode));
	dnp->dp = dp;
	DRCNTINC(dnp->dp);
	if (dp->d_type == ns_t_sig) {
		if (rrset->rr_sigs != NULL) {
			struct dnode *fdnp;

			/* Preserve the order of the RRs */
			/* Add this one to the end of the list */
			for (fdnp = rrset->rr_sigs;
			     fdnp->dn_next != NULL;
			     fdnp = fdnp->dn_next)
				/* NULL */ ;
			fdnp->dn_next = dnp;
		} else
			rrset->rr_sigs = dnp;
	} else {
		if (rrset->rr_list != NULL) {
			struct dnode *fdnp;

			/* Preserve the order of the RRs */
			/* Add this one to the end of the list */
			for (fdnp = rrset->rr_list;
			     fdnp->dn_next != NULL;
			     fdnp = fdnp->dn_next)
				/* NULL */ ;
			fdnp->dn_next = dnp;
		} else
			rrset->rr_list = dnp;
	}
	dnp->file = file;
	dnp->line = line;
	return (0);
}

static int
update_rrset_list(struct db_rrset **rrsets, int flags, struct hashbuf **htpp,
		  struct sockaddr_in from, int *rrcount)
{
	struct db_rrset *rrset = *rrsets, *next = NULL, *last = NULL;
	int result = 0, tresult, cnameandother = 0;

	while (rrset != NULL) {
		if (rrset->rr_type == ns_t_key)
			break;
		last = rrset;
		rrset = rrset->rr_next;
	}

	if (rrset != NULL && last != NULL) {
		last->rr_next = rrset->rr_next;
		rrset->rr_next = *rrsets;
		*rrsets = rrset;
	}

	rrset = *rrsets;

	while (rrset != NULL) {
		if (verify_set(rrset) > DB_S_FAILED) {
			ns_debug(ns_log_default, 10,
				 "update_rrset_list(%s, %s): set verified",
				  rrset->rr_name, p_type(rrset->rr_type));
			tresult = rrset_db_update(rrset, flags, htpp,
						  from, rrcount);
			if (tresult == CNAMEANDOTHER)
				cnameandother++;
			if (tresult != OK)
				result = tresult;
		}
		else {
			rrset_free(rrset);
			result = DNSSECFAIL;
		}
		rrset->rr_name = freestr(rrset->rr_name);
		next = rrset->rr_next;
		memput(rrset, sizeof(struct db_rrset));
		rrset = next;
	}
	*rrsets = NULL;
	if (cnameandother != 0)
		return (CNAMEANDOTHER);
	return (result);
}

int
db_set_update(char *name, struct databuf *dp, void **state,
	      int flags, struct hashbuf **htpp, struct sockaddr_in from,
	      int *rrcount, int line, const char *file)
{
	struct db_rrset **rrsets;
	struct db_rrset *rrset;
	int result = 0;

	ns_debug(ns_log_default, 5, "db_set_update(%s)",
		 (name == NULL) ? "<NULL>" : (*name == 0) ? "." : name);

	if (state == NULL)
		ns_panic(ns_log_default, 1,
			 "Called db_set_update with state == NULL");

	rrsets = (struct db_rrset **) state;

	if (*rrsets != NULL) {
		rrset = *rrsets;
		if (rrset->rr_name != NULL && dp != NULL &&
		    name != NULL && ns_samename(name, rrset->rr_name) == 1 &&
		    dp->d_class == rrset->rr_class)
			return (add_to_rrset_list(rrsets, name, dp,
						  line, file));
	}

	if (*rrsets != NULL)
		result = update_rrset_list(rrsets, flags, htpp, from, rrcount);

	if (dp != NULL) {
		ns_debug(ns_log_default, 10,
			 "db_set_update(%s), creating new list", name);

		(void) add_to_rrset_list(rrsets, name, dp, line, file);
	}
	return (result);
}

static int
nxt_match_rrset(struct databuf *dp, struct db_rrset *rrset) {
	if (rrset->rr_list != NULL)
		return (nxtmatch(rrset->rr_name, dp, rrset->rr_list->dp));
	else
		return (nxtmatch(rrset->rr_name, dp, rrset->rr_sigs->dp));
}
