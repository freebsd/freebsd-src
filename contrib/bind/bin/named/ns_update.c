#if !defined(lint) && !defined(SABER)
static const char rcsid[] = "$Id: ns_update.c,v 8.103 2002/01/03 00:25:02 marka Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1996-2000 by Internet Software Consortium.
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

/*
 * Portions Copyright (c) 1999 by Check Point Software Technologies, Inc.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Check Point Software Technologies Incorporated not be used 
 * in advertising or publicity pertaining to distribution of the document 
 * or software without specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND CHECK POINT SOFTWARE TECHNOLOGIES 
 * INCORPORATED DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.   
 * IN NO EVENT SHALL CHECK POINT SOFTWARE TECHNOLOGIES INCORPRATED
 * BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR 
 * ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT 
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Based on the Dynamic DNS reference implementation by Viraj Bais
 * <viraj_bais@ccm.fm.intel.com>
 */

#include "port_before.h"

#include <sys/param.h>
#include <sys/uio.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <resolv.h>
#include <res_update.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include <isc/eventlib.h>
#include <isc/logging.h>
#include <isc/memcluster.h>
#include <isc/dst.h>
#include <isc/misc.h>

#include "port_after.h"

#include "named.h"

#define WRITEABLE_MASK (S_IWUSR | S_IWGRP | S_IWOTH)

/* XXXRTH almost all funcs. in here should be static!
   map rdata_dump to db_to_textual
   map rdata_expand to wire_to_db
   make a textual_to_db and use it in merge_logs?
   replace all this "map" stuff with the new routines (from 4.9.5 I think)
 */

/* from ns_req.c */

static struct map m_opcode[] = {
	{ "nxdomain",	NXDOMAIN },
	{ "yxdomain",	YXDOMAIN },
	{ "nxrrset", 	NXRRSET },
	{ "yxrrset",	YXRRSET },
	{ "delete",	DELETE },
	{ "add",	ADD },
};
#define M_OPCODE_CNT (sizeof(m_opcode) / sizeof(struct map))

/* XXXRTH workaround map difficulties */
#define M_CLASS_CNT m_class_cnt
#define M_TYPE_CNT m_type_cnt

static const char *opcodes[] = {
	"delete",
	"add",
	"",
	"nxdomain",
	"",
	"",
	"yxdomain",
	"yxrrset",
	"nxrrset",
	"",
	"",
};


/* from db_load.c */

static struct map m_section[] = {
	{ "zone",	S_ZONE },
	{ "prereq",	S_PREREQ },
	{ "update", 	S_UPDATE },
	{ "reserved",	S_ADDT },
};
#define M_SECTION_CNT (sizeof(m_section) / sizeof(struct map))

/* Forward. */

static int rdata_expand(const u_char *, const u_char *, const u_char *,
			u_int, size_t, u_char *, size_t);


static FILE *
open_transaction_log(struct zoneinfo *zp) {
	FILE *fp = fopen(zp->z_updatelog, "a+");

	if (fp == NULL) {
		ns_error(ns_log_update, "can't open %s: %s", zp->z_updatelog,
			 strerror(errno));
		return (NULL);
	}
	(void) fchown(fileno(fp), user_id, group_id);
	if (fseek(fp, 0L, SEEK_END) != 0) {
		ns_error(ns_log_update, "can't fseek(%s, 0, SEEK_END)",
			 zp->z_updatelog);
		fclose(fp);
		return (NULL);
	}
	if (ftell(fp) == 0L) {
		fprintf(fp, "%s", LogSignature);
		zp->z_serial_ixfr_start = get_serial(zp);
	}
	else
		zp->z_serial_ixfr_start = 0;
	return (fp);
}

static FILE *
open_ixfr_log(struct zoneinfo *zp) {
	FILE *fp = fopen(zp->z_ixfr_base, "a+");

	if (fp == NULL) {
		ns_error(ns_log_update, "can't open %s: %s", zp->z_ixfr_base,
			 strerror(errno));
		return (NULL);
	}
	(void) fchown(fileno(fp), user_id, group_id);
	if (fseek(fp, 0L, SEEK_END) != 0) {
		ns_error(ns_log_update, "can't fseek(%s, 0, SEEK_END)",
			 zp->z_ixfr_base);
		fclose(fp);
		return (NULL);
	}
	if (ftell(fp) == 0L) {
		fprintf(fp, "%s", LogSignature);
	}
	return (fp);
}

static int
close_transaction_log(struct zoneinfo *zp, FILE *fp) {
	if (fflush(fp) == EOF) {
		ns_error(ns_log_update, "fflush() of %s failed: %s",
			 zp->z_updatelog, strerror(errno));
		return (-1);
	}
	if (fsync(fileno(fp)) < 0) {
		ns_error(ns_log_update, "fsync() of %s failed: %s",
			 zp->z_updatelog, strerror(errno));
		return (-1);
	}
	if (fclose(fp) == EOF) {
		ns_error(ns_log_update, "fclose() of %s failed: %s",
			 zp->z_updatelog, strerror(errno));
		return (-1);
	}
	return (0);
}

static int
close_ixfr_log(struct zoneinfo *zp, FILE *fp) {
	if (fflush(fp) == EOF) {
		ns_error(ns_log_update, "fflush() of %s failed: %s",
			 zp->z_ixfr_base, strerror(errno));
		fclose(fp);
		return (-1);
	}
	if (fsync(fileno(fp)) < 0) {
		ns_error(ns_log_update, "fsync() of %s failed: %s",
			 zp->z_ixfr_base, strerror(errno));
		fclose(fp);
		return (-1);
	}
	if (fclose(fp) == EOF) {
		ns_error(ns_log_update, "fclose() of %s failed: %s",
			 zp->z_ixfr_base, strerror(errno));
		return (-1);
	}
	return (0);
}

/*
 * return true if 'db' had been added.
 */
static int
was_added(const ns_updque *updlist, struct databuf *dp) {
	ns_updrec *rrecp;

	for (rrecp = HEAD(*updlist); rrecp != NULL; rrecp = NEXT(rrecp, r_link))
		if (rrecp->r_section == S_UPDATE && rrecp->r_dp == dp)
			return (1);
	return (0);
}

/*
 * return true if 'db' had been deleted.
 */
static int
was_deleted(const ns_updque *updlist, struct databuf *dp) {
	ns_updrec *rrecp;
	struct databuf *adp;


	for (rrecp = HEAD(*updlist); rrecp != NULL; rrecp = NEXT(rrecp, r_link))
		if (rrecp->r_section == S_UPDATE &&
		    rrecp->r_deldp != NULL) {
			adp = rrecp->r_deldp;
			do {
				if (adp == dp)
					return (1);
			} while ((adp = adp->d_next) != NULL);
		}
	return (0);
}

/*
 * printupdatelog(srcaddr, updlist, hp, zp, old_serial)
 *	append an ascii form to the zone's transaction log file.
 */
static void
printupdatelog(struct sockaddr_in srcaddr,
	       const ns_updque *updlist,
	       HEADER *hp,
	       struct zoneinfo *zp,
	       u_int32_t old_serial)
{
	struct databuf *dp;
	ns_updrec *rrecp;
	int opcode;
	char time[25];
	FILE *fp, *ifp;

	if (EMPTY(*updlist))
		return;

	fp = open_transaction_log(zp);
	if (fp == NULL)
		return;

	if (zp->z_maintain_ixfr_base == 1) {
		ifp = open_ixfr_log(zp);
		if (ifp == NULL) {
			(void) close_transaction_log(zp, fp);
			return;
		}
	}
	else
		ifp = NULL;

	sprintf(time, "at %lu", (u_long)tt.tv_sec);
	fprintf(fp, "[DYNAMIC_UPDATE] id %u from %s %s (named pid %ld):\n",
	        ntohs(hp->id), sin_ntoa(srcaddr), time, (long)getpid());
	if (ifp)
		fprintf(ifp,
			"[DYNAMIC_UPDATE] id %u from %s %s (named pid %ld):\n",
			ntohs(hp->id), sin_ntoa(srcaddr), time,
			(long)getpid());
	for (rrecp = HEAD(*updlist); rrecp != NULL; rrecp = NEXT(rrecp, r_link)) {
		INSIST(zp == &zones[rrecp->r_zone]);
		switch (rrecp->r_section) {
		case S_ZONE:
			fprintf(fp, "zone:\torigin %s class %s serial %u\n",
				zp->z_origin, p_class(zp->z_class),
				old_serial);
			if (ifp)
				fprintf(ifp,
				       "zone:\torigin %s class %s serial %lu\n",
					zp->z_origin, p_class(zp->z_class),
					(u_long)old_serial);
			break;
		case S_PREREQ:
			opcode = rrecp->r_opcode;
			fprintf(fp, "prereq:\t{%s} %s. %s ",
				opcodes[opcode], rrecp->r_dname,
				p_class(zp->z_class));
			if (opcode == NXRRSET || opcode == YXRRSET) {
				fprintf(fp, "%s ", p_type(rrecp->r_type));
				if ((dp = rrecp->r_dp) && dp->d_size > 0) {
					dp->d_class = zp->z_class;
					(void) rdata_dump(dp, fp);
				}
			}
			fprintf(fp, "\n");
			break;
		case S_UPDATE:
			opcode = rrecp->r_opcode;
			/*
			 * Translate all deletes into explict actions by
			 * looking at what was actually deleted from the
			 * zone for the ixfr log. 
			 */
			dp = rrecp->r_deldp;
			while (dp != NULL) {
				if (dp->d_rcode == 0 &&
				    !was_added(updlist, dp)) {
					if (ifp) {
				        	fprintf(ifp,
					        	"update:\t{%s} %s. %u %s %s ",
						        "delete",
						        rrecp->r_dname,
						        dp->d_ttl,
						        p_class(dp->d_class),
						        p_type(dp->d_type));
					        (void) rdata_dump(dp, ifp);
					        fprintf(ifp, "\n");
					}
				}
				dp = dp->d_next;
			}
			/*
			 * Only successful adds should be recorded.
			 * Don't add changes that are undone later.
			 * SOA additions performed later.
			 */
			if (opcode == ADD && (dp = rrecp->r_dp) != NULL &&
			    dp->d_type != T_SOA &&
			    (dp->d_mark & D_MARK_ADDED) != 0 &&
			    !was_deleted(updlist, dp)) {
				if (ifp) {
				        fprintf(ifp, "update:\t{%s} %s. ",
					        opcodes[opcode], rrecp->r_dname);
				        fprintf(ifp, "%u ", rrecp->r_ttl);
				        fprintf(ifp, "%s ", p_class(zp->z_class));
				        fprintf(ifp, "%s ", p_type(rrecp->r_type));
				        (void) rdata_dump(dp, ifp);
				        fprintf(ifp, "\n");
				}
			}
			/* Update log. */
			fprintf(fp, "update:\t{%s} %s. ",
				opcodes[opcode], rrecp->r_dname);
			if (opcode == ADD)
				fprintf(fp, "%u ", rrecp->r_ttl);
			fprintf(fp, "%s ", p_class(zp->z_class));
			if (rrecp->r_type != T_ANY)
				fprintf(fp, "%s ", p_type(rrecp->r_type));
			if ((dp = rrecp->r_dp) && dp->d_size > 0) {
				dp->d_class = zp->z_class;
				(void) rdata_dump(dp, fp);
			}
			fprintf(fp, "\n");
			break;
		case S_ADDT:
			break;
		default:
			ns_panic(ns_log_update, 1,
				 "printupdatelog - impossible condition");
			/*NOTREACHED*/
		}
	}
	/*
	 * SOA additions must be last in this update as they
	 * (or [INCR_SERIAL]) terminate an IXFR chunk.  Only the last SOA
	 * addition will be emitted for any dynamic update regardless
	 * of the number of SOA changes in the update.
	 */
	for (rrecp = HEAD(*updlist); rrecp != NULL; rrecp = NEXT(rrecp, r_link)) {
		INSIST(zp == &zones[rrecp->r_zone]);
		switch (rrecp->r_section) {
		case S_UPDATE:
			opcode = rrecp->r_opcode;
			if (opcode == ADD && (dp = rrecp->r_dp) != NULL &&
			    dp->d_type == T_SOA &&
			    (dp->d_mark & D_MARK_ADDED) != 0 &&
			    !was_deleted(updlist, dp)) {
				if (ifp) {
				        fprintf(ifp, "update:\t{%s} %s. ",
					        opcodes[opcode], rrecp->r_dname);
				        fprintf(ifp, "%u ", rrecp->r_ttl);
				        fprintf(ifp, "%s ", p_class(zp->z_class));
				        fprintf(ifp, "%s ", p_type(rrecp->r_type));
				        (void) rdata_dump(dp, ifp);
				        fprintf(ifp, "\n[END_DELTA]\n");
				}
			}
			break;
		default:
			break;
		}
	}
	fprintf(fp, "\n");
	(void) close_transaction_log(zp, fp);
	if (ifp)
		(void) close_ixfr_log(zp, ifp);
}

static void
cancel_soa_update(struct zoneinfo *zp) {
	ns_debug(ns_log_update, 3, "cancel_soa_update for %s", zp->z_origin);
	zp->z_flags &= ~Z_NEED_SOAUPDATE;
	zp->z_soaincrtime = 0;
	zp->z_updatecnt = 0;
}

/*
 * Figure out when a SOA serial number update should happen.
 * Returns non-zero if the caller should call sched_zone_maint(zp).
 */
int
schedule_soa_update(struct zoneinfo *zp, int numupdated) {
	(void) gettime(&tt);

	zp->z_flags |= Z_NEED_SOAUPDATE;
	
	/*
	 * Only z_deferupdcnt updates are allowed before we force
	 * a serial update.
	 */
	zp->z_updatecnt += numupdated;
	if (zp->z_updatecnt >= zp->z_deferupdcnt) {
		if (zp->z_soaincrtime > tt.tv_sec) {
			zp->z_soaincrtime = tt.tv_sec;
			return (1);
		}
	}

	if (zp->z_soaincrintvl > 0) {
		/* We want automatic updates in this zone. */
		if (zp->z_soaincrtime > 0) {
			/* Already scheduled. */
			ns_debug(ns_log_update, 3,
			 "schedule_soa_update('%s'): already scheduled",
				 zp->z_origin);
			return (0);
		} else {
			/* First update since the soa was last incremented. */
			zp->z_updatecnt = numupdated;
			zp->z_soaincrtime = tt.tv_sec + zp->z_soaincrintvl;
			/*
			 * Never schedule soaincrtime to occur after
			 * dumptime.
			 */
			if (zp->z_soaincrtime > zp->z_dumptime)
				zp->z_soaincrtime = zp->z_dumptime;
			ns_debug(ns_log_update, 3,
			 "schedule_soa_update('%s'): scheduled for %lu",
				 zp->z_origin, (u_long)zp->z_soaincrtime);
			return (1);
		}
	}
	return (0);
}

/*
 * Figure out when a zone dump should happen.
 * Returns non-zero if the caller should call sched_zone_maint(zp).
 */
int
schedule_dump(struct zoneinfo *zp) { 
	time_t half;

	(void) gettime(&tt);

	zp->z_flags |= Z_NEED_DUMP;
	
	if (zp->z_dumpintvl > 0) {
		/* We want automatic dumping in this zone. */
		if (zp->z_dumptime > 0) {
			/* Already scheduled. */
			ns_debug(ns_log_update, 3,
				 "schedule_dump('%s'): already scheduled",
				 zp->z_origin);
			return (0);
		} else {
			/*
			 * Set new dump time for dynamic zone.  Use a random
			 * number in the last half of the dump limit; we want
			 * it to be substantially correct while still
			 * preventing dump synchronization among various
			 * dynamic zones.
			 */
			half = (zp->z_dumpintvl + 1) / 2;
			zp->z_dumptime = tt.tv_sec + half + (rand() % half);
			/*
			 * Never schedule soaincrtime to occur after
			 * dumptime.
			 */
			if (zp->z_soaincrtime > zp->z_dumptime)
				zp->z_soaincrtime = zp->z_dumptime;
			ns_debug(ns_log_update, 3,
				 "schedule_dump('%s'): scheduled for %lu",
				 zp->z_origin, (u_long)zp->z_dumptime);
			return (1);
		}
	}
	return (0);
}

/*
 * int
 * process_prereq(rec, rcodep)
 *	Process one prerequisite.
 * returns:
 *	>0 prerequisite was satisfied.
 *	=0 prerequisite was not satisfied, or an error occurred.
 * side effects:
 *	sets *rcodep if an error occurs or prerequisite isn't satisfied.
 */
static int
process_prereq(ns_updrec *ur, int *rcodep, u_int16_t zclass) {
	const char *dname = ur->r_dname;
	u_int16_t class = ur->r_class;
	u_int16_t type = ur->r_type;
	u_int32_t ttl = ur->r_ttl;
	struct databuf *rdp = ur->r_dp;
	const char *fname;
	struct hashbuf *htp;
	struct namebuf *np;
	struct databuf *dp;

	/*
	 * An element in the list might have already been
	 * processed if it is in the same RRset as a previous
	 * RRset Exists (value dependent) prerequisite.
	 */
	if (rdp && (rdp->d_mark & D_MARK_FOUND) != 0) {
		/* Already processed. */
		return (1);
	}
	if (ttl != 0) {
		ns_debug(ns_log_update, 1,
			 "process_prereq: ttl!=0 in prereq section");
		*rcodep = FORMERR;
		return (0);
	}
	htp = hashtab;
	np = nlookup(dname, &htp, &fname, 0);
	/*
	 * Matching by wildcard not allowed here. 
	 * We need to post check for a wildcard match.
	 */
	if (fname != dname ||
	    (np != NULL && ns_wildcard(NAME(*np)) &&
	     (dname[0] != '*' || (dname[1] != '.' && dname[1] != '\0'))))
		np = NULL;

	if (class == C_ANY) {
		if (rdp->d_size) {
			ns_debug(ns_log_update, 1,
   "process_prereq: empty rdata required in prereq section with class=ANY");
			*rcodep = FORMERR;
			return (0);
		}
		if (type == T_ANY) {
			/* Name is in use. */
			ur->r_opcode = YXDOMAIN;
			if (np == NULL || np->n_data == NULL) {
				/*
				 * Name does not exist or is
				 * an empty nonterminal.
				 */
				ns_debug(ns_log_update, 1,
					 "process_prereq: %s not in use",
					 dname);
				*rcodep = NXDOMAIN;
				return (0);
			}
		} else {
			/* RRset exists (value independent). */
			int found = 0;

			ur->r_opcode = YXRRSET;
			if (np != NULL)
				for (dp = np->n_data;
				     dp && !found;
				     dp = dp->d_next)
					if (match(dp, class, type) &&
					    dp->d_type == type)
						found = 1;
			if (!found) {
				ns_debug(ns_log_update, 1,
			  "process_prereq: RRset (%s,%s,%s) does not exist",
					 dname, p_type(type), p_class(zclass));
				*rcodep = NXRRSET;
				return (0);
			}
		}
	} else if (class == C_NONE) {
		if (rdp->d_size) {
			ns_debug(ns_log_update, 1,
  "process_prereq: empty rdata required in prereq section with class=NONE");
			*rcodep = FORMERR;
			return (0);
		}
		if (type == T_ANY) {
			/* Name is not in use. */
			ur->r_opcode = NXDOMAIN;
			if (np != NULL && np->n_data != NULL) {
				/*
				 * Name exists and is not an
				 * empty nonterminal.
				 */
				ns_debug(ns_log_update, 1,
					 "process_prereq: %s exists",
					 dname);
				*rcodep = YXDOMAIN;
				return (0);
			}
		} else {
			/* RRset does not exist. */
			int found = 0;

			ur->r_opcode = NXRRSET;
			class = zclass;
			if (np != NULL)
				for (dp = np->n_data;
				     dp && !found;
				     dp = dp->d_next)
					if (match(dp, class, type))
						found = 1;
			if (found) {
				ns_debug(ns_log_update, 1,
				     "process_prereq: RRset (%s,%s) exists",
					 dname, p_type(type));
				*rcodep = YXRRSET;
				return (0);
			}
		}
	} else if (class == zclass) {
		/*
		 * RRset exists (value dependent).
		 *
		 * Check for RRset equality also.
		 */
		ns_updrec *tmp;

		ur->r_opcode = YXRRSET;
		if (!rdp) {
			ns_debug(ns_log_update, 1,
  "process_prereq: nonempty rdata required in prereq section with class=%s",
				 p_class(class));
			*rcodep = FORMERR;
			return (0);
		}
		if (np == NULL || fname != dname) {
			*rcodep = NXRRSET;
			return (0);
		}
		for (dp = np->n_data; dp; dp = dp->d_next) {
			if (match(dp, class, type) && dp->d_type == type) {
				int found = 0;

				for (tmp = ur;
				     tmp != NULL && !found;
				     tmp = NEXT(tmp, r_link)) {
					if (tmp->r_section != S_PREREQ)
						break;
					if (!db_cmp(dp, tmp->r_dp)) {
						tmp->r_dp->d_mark |=
							D_MARK_FOUND;
						found = 1;
					}
				}
				if (!found) {
					*rcodep = NXRRSET;
					return (0);
				}
			}
		}
		for (tmp = ur; tmp != NULL; tmp = NEXT(tmp, r_link))
			if (tmp->r_section == S_PREREQ &&
			    ns_samename(dname, tmp->r_dname) == 1 &&
			    tmp->r_class == class &&
			    tmp->r_type == type &&
			    (ur->r_dp->d_mark & D_MARK_FOUND) == 0) {
				*rcodep = NXRRSET;
				return (0);
			} else {
				tmp->r_opcode = YXRRSET;
			}
	} else {
		ns_debug(ns_log_update, 1,
			 "process_prereq: incorrect class %s",
			 p_class(class));
		*rcodep = FORMERR;
		return (0);
	}
	/* Through the gauntlet, and out. */
	return (1);
}

static int
prescan_nameok(ns_updrec *ur, int *rcodep, u_int16_t zclass,
	       struct zoneinfo *zp)
{
	const char *owner = ur->r_dname;
	u_int16_t class = ur->r_class;
	u_int16_t type = ur->r_type;
	char *cp = (char *)ur->r_dp->d_data;
	enum context context;
	enum transport transport;

	/* We don't care about deletes */
	if (ur->r_class != zclass)
		return (1);

	if (zp->z_type == Z_PRIMARY)
		transport = primary_trans;
	else
		transport = secondary_trans;

	context = ns_ownercontext(type, transport);
	if (!ns_nameok(NULL, owner, class, zp, transport, context, owner,
		       inaddr_any))
		goto refused;

	switch (type) {
	case ns_t_soa:
		context = hostname_ctx;
		if (!ns_nameok(NULL, cp, class, zp, transport, context, owner,
			       inaddr_any))
			goto refused;
		cp += strlen(cp) + 1;
		context = mailname_ctx;
		if (!ns_nameok(NULL, cp, class, zp, transport, context, owner,
			       inaddr_any))
			goto refused;
		break;
	case ns_t_rp:
		context = mailname_ctx;
		if (!ns_nameok(NULL, cp, class, zp, transport, context, owner,
			       inaddr_any))
			goto refused;
		cp += strlen(cp) + 1;
		context = domain_ctx;
		if (!ns_nameok(NULL, cp, class, zp, transport, context, owner,
			       inaddr_any))
			goto refused;
		break;
	case ns_t_minfo:
		context = mailname_ctx;
		if (!ns_nameok(NULL, cp, class, zp, transport, context, owner,
			       inaddr_any))
			goto refused;
		cp += strlen(cp) + 1;
		context = mailname_ctx;
		if (!ns_nameok(NULL, cp, class, zp, transport, context, owner,
			       inaddr_any))
			goto refused;
		break;
	case ns_t_ns:
		context = hostname_ctx;
		if (!ns_nameok(NULL, cp, class, zp, transport, context, owner,
			       inaddr_any))
			goto refused;
		break;
	case ns_t_cname:
	case ns_t_mb:
	case ns_t_mg:
	case ns_t_mr:
		context = domain_ctx;
		if (!ns_nameok(NULL, cp, class, zp, transport, context, owner,
			       inaddr_any))
			goto refused;
		break;
	case ns_t_ptr:
		context = ns_ptrcontext(owner);
		if (!ns_nameok(NULL, cp, class, zp, transport, context, owner,
			       inaddr_any))
			goto refused;
		break;
	case ns_t_naptr:
		/*
		 * Order (2)
		 * Preference (2)
		 */
		cp += 4;
		/* Flags (txt) */
		cp += (*cp&0xff) + 1;
		/* Service (txt) */
		cp += (*cp&0xff) + 1;
		/* Pattern (txt) */
		cp += (*cp&0xff) + 1;
		context = domain_ctx;
		if (!ns_nameok(NULL, cp, class, zp, transport,
			       context, owner, inaddr_any))
			goto refused;
		break;
	case ns_t_srv:
		cp += 4;
		/* FALLTHROUGH */
	case ns_t_mx:
	case ns_t_afsdb:
	case ns_t_rt:
	case ns_t_kx:
		cp += 2;
		context = hostname_ctx;
		if (!ns_nameok(NULL, cp, class, zp, transport, context, owner,
			       inaddr_any))
			goto refused;
		break;
	case ns_t_px:
		cp += 2;
		context = domain_ctx;
		if (!ns_nameok(NULL, cp, class, zp, transport, context, owner,
			       inaddr_any))
			goto refused;
		cp += strlen(cp) + 1;
		if (!ns_nameok(NULL, cp, class, zp, transport, context, owner,
			       inaddr_any))
			goto refused;
		break;
	case ns_t_sig:
		/*
		 * Type covered (2)
		 * Alg (1) *
		 * Labels (1)
		 * ttl (4)
		 * expires (4)
		 * signed (4)
		 * footprint (2)
		 */
		cp += 18;
		context = domain_ctx;
		if (!ns_nameok(NULL, cp, class, zp, transport, context, owner,
			       inaddr_any))
			goto refused;
		break;
	case ns_t_nxt:
		context = domain_ctx;
		if (!ns_nameok(NULL, cp, class, zp, transport, context, owner,
			       inaddr_any))
			goto refused;
		break;
	default:
		break;
	}
	return (1);
 refused:
	*rcodep = REFUSED;
	return (0);
}

/*
 * int
 * prescan_update(ur, rcodep)
 *	Process one prerequisite.
 * returns:
 *	>0 update looks OK (format wise; who knows if it will succeed?)
 *	=0 update has something wrong with it.
 * side effects:
 *	sets *rcodep if an error occurs or prerequisite isn't satisfied.
 */
static int
prescan_update(ns_updrec *ur, int *rcodep, u_int16_t zclass) {
	u_int16_t class = ur->r_class;
	u_int16_t type = ur->r_type;
	u_int32_t ttl = ur->r_ttl;
	struct databuf *rdp = ur->r_dp;

	if (class == zclass) {
		if (!ns_t_rr_p(type)) {
			ns_debug(ns_log_update, 1,
				 "prescan_update: invalid type (%s)",
				 p_type(type));
			*rcodep = FORMERR;
			return (0);
		}
		if (ttl > MAXIMUM_TTL) {
			ns_debug(ns_log_update, 1,
				"prescan_update: invalid ttl (%u)", ttl);
			*rcodep = FORMERR;
			return (0);
		}
	} else if (class == C_ANY) {
		if (ttl != 0 || rdp->d_size ||
		    (!ns_t_rr_p(type) && type != T_ANY))
		{
			ns_debug(ns_log_update, 1,
				 "prescan_update: formerr(#2)");
			*rcodep = FORMERR;
			return (0);
		}
	} else if (class == C_NONE) {
		if (ttl != 0 || !ns_t_rr_p(type)) {
			ns_debug(ns_log_update, 1,
				 "prescan_update: formerr(#3) %d %s",
				 ttl, p_type(type));
			*rcodep = FORMERR;
			return (0);
		}
	} else {
		ns_debug(ns_log_update, 1,
			 "prescan_update: invalid class (%s)",
			 p_class(class));
		*rcodep = FORMERR;
		return (0);
	}
	/* No format errors found. */
	return (1);
}

/*
 * int
 * process_updates(updlist, rcodep, from)
 *	Process prerequisites and apply updates from the list to the database.
 * returns:
 *	number of successful updates, 0 if none were successful.
 * side effects:
 *	*rcodep gets the transaction return code.
 *	can schedule maintainance for zone dumps and soa.serial# increments.
 */
static int
process_updates(const ns_updque *updlist, int *rcodep,
		struct sockaddr_in from)
{
	int j, n, dbflags, matches, zonenum;
	int numupdated = 0, soaupdated = 0, schedmaint = 0;
	u_int16_t zclass;
	ns_updrec *ur;
	struct databuf *dp, *savedp;
	struct zoneinfo *zp;
	int zonelist[MAXDNAME];

	*rcodep = SERVFAIL;
	if (EMPTY(*updlist))
		return (0);
	ur = HEAD(*updlist);
	if (ur->r_section == S_ZONE) {
		zclass = ur->r_class;
		zonenum = ur->r_zone;
		zp = &zones[zonenum];
	} else {
		ns_debug(ns_log_update, 1,
			 "process_updates: missing zone record");
		return (0);
	}

	/* Process prereq records and prescan update records. */
	for (ur = HEAD(*updlist); ur != NULL; ur = NEXT(ur, r_link)) {
		const char *	dname = ur->r_dname;
		u_int16_t	class = ur->r_class;
		u_int16_t	type = ur->r_type;
		u_int32_t	ttl = ur->r_ttl;
		struct databuf *rdp = ur->r_dp;
		u_int		section = ur->r_section;

		ns_debug(ns_log_update, 3,
"process_update: record section=%s, dname=%s, \
class=%s, type=%s, ttl=%d, dp=%p",
			 p_section(section, ns_o_update), dname,
			 p_class(class), p_type(type), ttl, rdp);

		matches = findzone(dname, zclass, MAXDNAME, 
				   zonelist, MAXDNAME);
		ur->r_zone = 0;
		for (j = 0; j < matches && !ur->r_zone; j++)
			if (zonelist[j] == zonenum)
				ur->r_zone = zonelist[j];
		if (!ur->r_zone || 
		    (section != S_ADDT && type == T_SOA &&
		     ns_samename(dname, zp->z_origin) != 1)) {
			ns_debug(ns_log_update, 1,
		    "process_updates: record does not belong to the zone %s",
				 zones[zonenum].z_origin);
			*rcodep = NOTZONE;
			return (0);
		}

		switch (section) {
		case S_ZONE:
			break;
		case S_PREREQ:
			if (!process_prereq(ur, rcodep, zclass))
				return (0);	/* *rcodep has been set. */
			ns_debug(ns_log_update, 3, "prerequisite satisfied");
			break;
		case S_UPDATE:
			if (!prescan_update(ur, rcodep, zclass))
				return (0);	/* *rcodep has been set. */
			if (!prescan_nameok(ur, rcodep, zclass, zp))
				return (0);	/* *rcodep has been set. */
			ns_debug(ns_log_update, 3, "update prescan succeeded");
			break;
		case S_ADDT:
			break;
		default:
			ns_panic(ns_log_update, 1,
				 "process_updates: impossible section");
			/* NOTREACHED */
		}
	}

	/* Now process the records in update section. */
	for (ur = HEAD(*updlist); ur != NULL; ur = NEXT(ur, r_link)) {
		const char *	dname = ur->r_dname;
		u_int16_t	class = ur->r_class;

		if (ur->r_section != S_UPDATE)
			continue;
		dbflags = 0;
		savedp = NULL;
		dp = ur->r_dp;
		if (class == zp->z_class) {
			/* ADD databuf dp to hash table */
			/*
			 * Handling of various SOA/WKS/CNAME scenarios
			 * is done in db_update().
			 */
			ur->r_opcode = ADD;
			dbflags |= DB_NODATA | DB_REPLACE;
			n = db_update(dname, dp, dp, &savedp,
				      dbflags, hashtab, from);
			if (!((n == OK) || 
			  ((zp->z_xferpid == XFER_ISIXFR) && (n == DATAEXISTS))))  {
				ns_debug(ns_log_update, 3,
			       "process_updates: failed to add databuf (%d)",
					 n);
			} else {
				ns_debug(ns_log_update, 3,
				      "process_updates: added databuf %p",
					 dp);
				dp->d_mark = D_MARK_ADDED;
				numupdated++;
				if (dp->d_type == T_SOA)
					soaupdated = 1;
			}
		} else if (class == C_ANY || class == C_NONE) {
			/*
			 * DELETE databuf's matching dp from the hash table.
			 *
			 * handling of various SOA/NS scenarios done
			 * in db_update().
			 */
			ur->r_opcode = DELETE;
			/*
			 * we know we're deleting now, and db_update won't
			 * match with class==C_NONE, so we use the zone's
			 * class.
			 */
			if (class == C_NONE)
				ur->r_dp->d_class = zp->z_class;
			dbflags |= DB_DELETE;
			n = db_update(dname, dp, NULL, &savedp,
				      dbflags, hashtab, from);
			if (!((n == OK) ||
			  ((zp->z_xferpid == XFER_ISIXFR) && (n == NODATA))))  {
				ns_debug(ns_log_update, 3,
					 "process_updates: delete failed");
			} else {
				ns_debug(ns_log_update, 3,
					 "process_updates: delete succeeded");
				numupdated++;
			}
		}
		/*
		 * Even an addition could have caused some deletions like
		 * replacing old SOA or CNAME or WKS record or records of
		 * lower cred/clev.
		 *
		 * We need to save the deleted databuf's in case we wish to
		 * abort this update transaction and roll back all updates
		 * applied from this packet.
		 */
		ur->r_deldp = savedp;
	}

	/*
	 * If we got here, things are OK, so set rcodep to indicate so.
	 */
	*rcodep = NOERROR;

	if (!numupdated)
		return (0);

	/*
	 * schedule maintenance for dumps and SOA.serial# increment 
	 * (this also sets Z_NEED_DUMP and Z_NEED_SOAUPDATE appropriately)
	 */
	schedmaint = 0;
	if (schedule_dump(zp))
		schedmaint = 1;
	if (soaupdated) {
		/*
		 * SOA updated by this update transaction, so
		 * we need to set the zone serial number, stop any
		 * automatic updates that may be pending, and send out
		 * a NOTIFY message.
		 */
		zp->z_serial = get_serial_unchecked(zp);
 	        cancel_soa_update(zp);
		schedmaint = 1;
#ifdef BIND_NOTIFY
		if (!loading)
			ns_notify(zp->z_origin, zp->z_class, ns_t_soa);
#endif
	} else {
		if (schedule_soa_update(zp, numupdated))
			schedmaint = 1;
	}
	if (schedmaint)
		sched_zone_maint(zp);
	return (numupdated);
}

static enum req_action
req_update_private(HEADER *hp, u_char *cp, u_char *eom, u_char *msg, 
		   struct sockaddr_in from, struct tsig_record *in_tsig,
		   ns_updque *curupd)
{
	char dnbuf[MAXDNAME], *dname;
	u_int zocount, prcount, upcount, adcount, class, type, dlen;
	u_int32_t ttl;
	int i, n, matches, zonenum, numupdated = 0;
	int rcode = NOERROR;
	u_int section;
	u_char rdata[MAXDATA];
	struct databuf *dp, *nsp[NSMAX];
	struct zoneinfo *zp;
	ns_updrec *rrecp;
	int zonelist[MAXDNAME];
	u_int32_t old_serial;
	DST_KEY *in_key = (in_tsig != NULL) ? in_tsig->key : NULL; 

	nsp[0] = NULL;

	zocount = ntohs(hp->qdcount);
	prcount = ntohs(hp->ancount);
	upcount = ntohs(hp->nscount);
	adcount = ntohs(hp->arcount);

	/* Process zone section. */
	ns_debug(ns_log_update, 3, "req_update: section ZONE, count %d",
		 zocount);
	if ((n = dn_expand(msg, eom, cp, dnbuf, sizeof(dnbuf))) < 0) {
		ns_debug(ns_log_update, 1, "req_update: expand name failed");
		hp->rcode = FORMERR;
		return (Finish);
	}
	dname = dnbuf;
 	cp += n;
	if (cp + 2 * INT16SZ > eom) {
		ns_debug(ns_log_update, 1, "req_update: too short");
		hp->rcode = FORMERR;
		return (Finish);
	}		
	GETSHORT(type, cp);
	GETSHORT(class, cp);
	if (zocount != 1 || type != T_SOA) {
		ns_debug(ns_log_update, 1,
		  "req_update: incorrect count or type for zone section: %d",
			 zocount);
		hp->rcode = FORMERR;
		return (Finish);
	}

	matches = findzone(dname, class, 0, zonelist, MAXDNAME);
	if (matches == 1) {
		zonenum = zonelist[0];
		zp = &zones[zonenum];
		if (zp->z_class != (int)class ||
		    (zp->z_type != z_master && zp->z_type != z_slave))
			matches = 0;
	}
	if (matches != 1) {
		ns_debug(ns_log_update, 1,
			 "req_update: non-authoritative server for %s",
			 dname);
		hp->rcode = NOTAUTH;
		return (Finish);
	}

	/*
	 * Begin Access Control Point
	 */

	if (!ip_addr_or_key_allowed(zp->z_update_acl, from.sin_addr, in_key)) {
		ns_notice(ns_log_security,
			  "denied update from %s for \"%s\" %s",
			  sin_ntoa(from), *dname ? dname : ".", p_class(class));
		nameserIncr(from.sin_addr, nssRcvdUUpd);
		return (Refuse);
	}

	/*
	 * End Access Control Point
	 */

	/* we should be authoritative */
	if (!(zp->z_flags & Z_AUTH)) {
		ns_debug(ns_log_update, 1,
			 "req_update: zone %s: Z_AUTH not set",
			 dname);
		hp->rcode = NOTAUTH;
		return (Finish);
	}

	if (zp->z_type == Z_SECONDARY) {
		/*
		 * XXX	The code below is broken.
		 *	Until fixed, we just return NOTIMPL.
		 */
#if 1
		hp->rcode = ns_r_notimpl;
		return (Finish);
#else		
		/* We are a slave for this zone, forward it to the master. */
		for (cnt = 0; cnt < zp->z_addrcnt; cnt++)
			*nspp++ = savedata(zp->z_class, T_A, USE_MINIMUM,
					   (u_char *)&zp->z_addr[cnt].s_addr,
					   INT32SZ);
		*nspp = NULL;
		/*
		 * If the request came in over TCP, forward it over TCP
		 */
		should_use_tcp = (qsp != NULL);
		if (in_tsig != NULL) {
			tsig_len = ns_skiprr(eom, eom + TSIG_BUF_SIZE,
					     ns_s_ar, 1);
			eom += tsig_len;
		}
		n = ns_forw(nsp, msg, eom-msg, from, qsp, dfd, &qp,
			    dname, class, type, NULL, should_use_tcp, NULL);
		if (in_tsig != NULL)
			eom -= tsig_len;
		free_nsp(nsp);
		switch (n) {
		case FW_OK:
		case FW_DUP:
			return (Return);
		case FW_NOSERVER:
			/* should not happen */
		case FW_SERVFAIL:
			hp->rcode = SERVFAIL;
			return (Finish);
		}
#endif
	}
	/*
	 * We are the primary master server for this zone,
	 * proceed further and process update packet
	 */
	if (!(zp->z_flags & Z_DYNAMIC)) {
		ns_debug(ns_log_update, 1,
			 "req_update: dynamic flag not set for zone %s",
			 dname);
		return (Refuse);
	}
	old_serial = get_serial(zp);
	ns_debug(ns_log_update, 3,
		 "req_update: update request for zone %s, class %s",
		 zp->z_origin, p_class(class));
	rrecp = res_mkupdrec(S_ZONE, dname, class, type, 0);
	rrecp->r_zone = zonenum;

	APPEND(*curupd, rrecp, r_link);

	/*
	 * Parse the prerequisite and update sections for format errors.
	 */
	for (i = 0; (u_int)i < prcount + upcount; i++) {
		if ((n = dn_expand(msg, eom, cp, dnbuf, sizeof(dnbuf))) < 0) {
			ns_debug(ns_log_update, 1,
				 "req_update: expand name failed");
			hp->rcode = FORMERR;
			return (Finish);
	  	}
	   	dname = dnbuf;
 	   	cp += n;
	   	if (cp + RRFIXEDSZ > eom) {
			ns_debug(ns_log_update, 1,
				 "req_update: overrun in answer");
			hp->rcode = FORMERR;
			return (Finish);
	   	}
	   	GETSHORT(type, cp);
	   	GETSHORT(class, cp);
		if (class > CLASS_MAX) {
			ns_debug(ns_log_update, 1,
				 "req_update: bad class");
			hp->rcode = FORMERR;
			return (Finish);
		}
	   	GETLONG(ttl, cp);
	   	GETSHORT(dlen, cp);
		n = 0;
		dp = NULL;
	   	if (dlen > 0) {
			if (cp + dlen > eom) {
				ns_debug(ns_log_update, 1,
					 "req_update: bad dlen");
				hp->rcode = FORMERR;
				return (Finish);
			}
			n = rdata_expand(msg, eom, cp, type, dlen,
					 rdata, sizeof rdata);
			if (n == 0 || n > MAXDATA) {
				ns_debug(ns_log_update, 1, 
				     "req_update: failed to expand record");
				hp->rcode = FORMERR;
				return (Finish);
			}
			cp += dlen;
	   	}
		section = ((u_int)i < prcount) ? S_PREREQ : S_UPDATE;
		rrecp = res_mkupdrec(section, dname, class, type, ttl);
		dp = savedata(class, type, ttl, rdata, n);
		dp->d_zone = zonenum;
		dp->d_cred = DB_C_ZONE;
		dp->d_secure = DB_S_INSECURE; /* should be UNCHECKED */
		dp->d_clev = nlabels(zp->z_origin);
		/* XXX - also record in dp->d_ns, which host this came from */
		rrecp->r_dp = dp;
		/* Append the current record to the end of list of records. */
		APPEND(*curupd, rrecp, r_link);
		if (cp > eom) {
			ns_info(ns_log_update,
				"Malformed response from %s (overrun)",
				inet_ntoa(from.sin_addr));
			hp->rcode = FORMERR;
			return (Finish);
		}
	}

	/* Now process all parsed records in the prereq and update sections. */
	numupdated = process_updates(curupd, &rcode, from);
	hp->rcode = rcode;
	if (numupdated <= 0) {
		if (rcode != NOERROR)
			ns_error(ns_log_update,
				 "error processing update packet (%s) id %d from %s",
				 p_rcode(rcode), ntohs(hp->id), sin_ntoa(from));
		return (Finish);
	}

	/*
	 * Stop any outbound zone transfers.
	 * (Eventlib is synchronous for this.)
	 */
	ns_stopxfrs(zp);

	/* Make a log of the update. */
	(void) printupdatelog(from, curupd, hp, zp, old_serial);

	return (Finish);
}

void
free_rrecp(ns_updque *updlist, int rcode, struct sockaddr_in from) {
	ns_updrec *rrecp, *first_rrecp, *next_rrecp;
	struct databuf *dp, *tmpdp;
	char *dname;
	const char *msg;

	if (rcode == NOERROR) {
		first_rrecp = HEAD(*updlist);
		msg = "free_rrecp: update transaction succeeded, cleaning up";
	} else {
		first_rrecp = TAIL(*updlist);
		msg = "free_rrecp: update transaction aborted, rolling back";
	}
	ns_debug(ns_log_update, 1, "%s", msg);
	for (rrecp = first_rrecp; rrecp != NULL; rrecp = next_rrecp) {
		if (rcode == NOERROR)
			next_rrecp = NEXT(rrecp, r_link);
		else
			next_rrecp = PREV(rrecp, r_link);
		UNLINK(*updlist, rrecp, r_link);
		if (rrecp->r_section != S_UPDATE) {
			if (rrecp->r_dp)
				db_detach(&rrecp->r_dp);
			INSIST(rrecp->r_deldp == NULL);
			res_freeupdrec(rrecp);
			continue;
		}
		dname = rrecp->r_dname;
		dp = rrecp->r_dp;
		rrecp->r_dp = NULL;
		if ((dp->d_mark & D_MARK_ADDED) != 0) {
			if (rcode == NOERROR) {
				/*
				 * This databuf is now a part of hashtab,
				 * or has been deleted by a subsequent update.
				 * Either way, we must not free it.
				 */
				dp->d_mark &= ~D_MARK_ADDED;
			} else {
				/* Delete the databuf. */
				if (db_update(dname, dp, NULL, NULL,
					      DB_DELETE, hashtab, from)
				    != OK) {
					ns_error(ns_log_update,
		     "free_rrecp: failed to delete databuf: dname=%s, type=%s",
						 dname, p_type(dp->d_type));
				} else {
					ns_debug(ns_log_update, 3,
				         "free_rrecp: deleted databuf %p",
						 dp);
				}
			}
		}
		db_detach(&dp);

		/* Process deleted databuf's. */
		dp = rrecp->r_deldp;
		rrecp->r_deldp = NULL;
		while (dp != NULL) {
			tmpdp = dp;
			dp = dp->d_next;
			tmpdp->d_next = NULL;
			if (rcode != NOERROR) {
				/* Add the databuf back. */
				tmpdp->d_mark &= ~D_MARK_DELETED;
				if (db_update(dname, tmpdp, tmpdp, NULL,
					      DB_REPLACE, hashtab, from) != OK) {
					ns_error(ns_log_update,
	           "free_rrecp: failed to add back databuf: dname=%s, type=%s",
						 dname, p_type(tmpdp->d_type));
				} else {
					ns_debug(ns_log_update, 3,
				      "free_rrecp: added back databuf %p",
						 tmpdp);
				}
			}
			db_detach(&tmpdp);
		}
		res_freeupdrec(rrecp);
	}
}

enum req_action
req_update(HEADER *hp, u_char *cp, u_char *eom, u_char *msg, 
	   struct sockaddr_in from, struct tsig_record *in_tsig)
{
	enum req_action ret;
	ns_updque curupd;

	INIT_LIST(curupd);
	ret = req_update_private(hp, cp, eom, msg, from, in_tsig, &curupd);
	free_rrecp(&curupd, ret == Refuse ? ns_r_refused : hp->rcode, from);
	if (ret == Finish) {
		hp->qdcount = hp->ancount = hp->nscount = hp->arcount = 0;
		memset(msg + HFIXEDSZ, 0, (eom - msg) - HFIXEDSZ);
	}
	return (ret);
}

/*
 * expand rdata portion of a compressed resource record at cp into cp1
 * and return the length of the expanded rdata (length of the compressed
 * rdata is "dlen").
 */
static int
rdata_expand(const u_char *msg, const u_char *eom, const u_char *cp,
	     u_int type, size_t dlen, u_char *cp1, size_t size)
{
	const u_char *cpinit = cp;
	const u_char *cp1init = cp1;
	int n, i, n1;

	switch (type) {
	case T_A:
	case T_AAAA:
		if ((type == T_A && dlen != INT32SZ) ||
		    (type == T_AAAA && dlen != NS_IN6ADDRSZ))
			return (0);
		/*FALLTHROUGH*/
	case T_WKS:
	case T_HINFO:
	case T_TXT:
	case T_X25:
	case T_ISDN:
	case T_NSAP:
	case T_LOC:
	case T_KEY:
	case ns_t_cert:
		if (size < dlen)
			return (0);
		memcpy(cp1, cp, dlen);
		return (dlen);
	case T_CNAME:
	case T_MB:
	case T_MG:
	case T_MR:
	case T_NS:
	case T_PTR:
		n = dn_expand(msg, eom, cp, (char *)cp1, size);
		if (n < 0 || (u_int)n != dlen)
			return (0);
		return (strlen((char *)cp1) + 1);
	case T_MINFO:
	case T_SOA:
	case T_RP:
		/* Get two compressed domain names. */
		for (i = 0; i < 2; i++) {
			n = dn_expand(msg, eom, cp, (char *)cp1, size);
			if (n < 0)
				return (0);
			cp += n;
			n = strlen((char *)cp1) + 1;
			cp1 += n;
			size -= n;
		}
		if (type == T_SOA) {
			n = 5 * INT32SZ;
			if (size < (size_t)n || cp + n > eom)
				return(0);
			size -= n;
			memcpy(cp1, cp, n);
			cp += n;
			cp1 += n;
		}
		if (cp != cpinit + dlen)
			return (0);
		return (cp1 - cp1init);
	case T_MX:
	case T_AFSDB:
	case T_RT:
	case T_SRV:
		/* Grab preference. */
		if (size < INT16SZ || cp + INT16SZ > eom)
			return (0);
		size -= INT16SZ;
		memcpy(cp1, cp, INT16SZ);
		cp += INT16SZ;
		cp1 += INT16SZ;

		if (type == T_SRV) {
			if (size < INT16SZ*2 || cp + INT16SZ*2 > eom)
				return (0);
			size -= INT16SZ*2;
			/* Grab weight and port. */
			memcpy(cp1, cp, INT16SZ*2);
			cp1 += INT16SZ*2;
			cp += INT16SZ*2;
		}

		/* Get name. */
		n = dn_expand(msg, eom, cp, (char *)cp1, size);
		if (n < 0)
			return (0);
		cp += n;
		n = strlen((char *)cp1) + 1;
		cp1 += n;
		if (cp != cpinit + dlen)
			return (0);
		return (cp1 - cp1init);
	case T_PX:
		/* Grab preference. */
		if (size < INT16SZ || cp + INT16SZ > eom)
			return (0);
		size -= INT16SZ;
		memcpy(cp1, cp, INT16SZ);
		cp += INT16SZ;
		cp1 += INT16SZ;
		/* Get MAP822 name. */
		n = dn_expand(msg, eom, cp, (char *)cp1, size);
		if (n < 0)
			return (0);
		cp += n;
		n = strlen((char *)cp1) + 1;
		cp1 += n;
		size -= n;
		n = dn_expand(msg, eom, cp, (char *)cp1, size);
		if (n < 0)
			return (0);
		cp += n;
		n = strlen((char *)cp1) + 1;
		cp1 += n;
		if (cp != cpinit + dlen)
			return (0);
		return (cp1 - cp1init);
	case T_SIG:
		if (dlen < SIG_HDR_SIZE || size < dlen)
			return (0);
		memcpy(cp1, cp, SIG_HDR_SIZE);
		size -= SIG_HDR_SIZE;
		cp += SIG_HDR_SIZE;
		cp1 += SIG_HDR_SIZE;
		n = dn_expand(msg, eom, cp, (char *)cp1, size);
		if (n < 0 || n + SIG_HDR_SIZE > (int)dlen)
			return (0);
		cp += n;
		n1 = dlen - n - SIG_HDR_SIZE;
		n = strlen((char *)cp1) + 1;
		cp1 += n;
		if ((int)size < n1)
			return (0);
		memcpy(cp1, cp, n1);
		cp1 += n1;
		return (cp1 - cp1init);
	case T_NXT:
		n = dn_expand(msg, eom, cp, (char *)cp1, size);
		if (n < 0 || (u_int)n >= dlen)
			return (0);
		size -= n;
		cp += n;
		n1 = dlen - n;
		n = strlen((char *)cp1) + 1;
		cp1 += n;
		/*
		 * The first bit of the first octet determines the format
		 * of the NXT record.  A format for types >= 128 has not
		 * yet been defined, so if bit zero is set, we just copy
		 * what's there because we don't understand it.
		 */
		if ((*cp & 0x80) == 0) {
			/*
			 * Bit zero is not set; this is an ordinary NXT
			 * record.  The bitmap must be at least 4 octets
			 * because the NXT bit should be set.  It should be
			 * less than or equal to 16 octets because this NXT
			 * format is only defined for types < 128.
			 */
			if (n1 < 4 || n1 > 16)
				return (0);
		}
		if (n1 > (int)size)
			return (0);
		memcpy(cp1, cp, n1);
		cp1 += n1;
		return (cp1 - cp1init);
	default:
		if (size < dlen)
			return (0);
		memcpy(cp1, cp, dlen);
		return (dlen);
	}
}

/*
 * Print out rdata portion of a resource record from a databuf into a file.
 *
 * XXX - similar code in db_dump() should be replaced by a call to this
 * function.
 */
void
rdata_dump(struct databuf *dp, FILE *fp) {
	u_int32_t n, addr;
	u_char *cp, *end;
	int i, j;
	const char *proto;
	u_char *savecp;
	char temp_base64[NS_MD5RSA_MAX_BASE64];
	u_int16_t keyflags;
	u_char *sigdata, *certdata;

	cp = (u_char *)dp->d_data;
	switch (dp->d_type) {
	case T_A:
		switch (dp->d_class) {
		case C_IN:
		case C_HS:
			GETLONG(n, cp);
			n = htonl(n);
			fputs(inet_ntoa(*(struct in_addr *)&n), fp);
			break;
		}
		if (dp->d_nstime)
			fprintf(fp, ";\tNT=%d", dp->d_nstime);
		break;
	case T_CNAME:
	case T_MB:
	case T_MG:
	case T_MR:
	case T_PTR:
		fprintf(fp, "%s.", cp);
		break;
	case T_NS:
		cp = (u_char *)dp->d_data;
		if (cp[0] == '\0')
			fprintf(fp, ".\t");
		else
			fprintf(fp, "%s.", cp);
		break;
	case T_HINFO:
	case T_ISDN:
		if ((n = *cp++) != '\0') {
			fprintf(fp, "\"%.*s\"", (int)n, cp);
			cp += n;
		} else
			fprintf(fp, "\"\"");
		if ((n = *cp++) != '\0')
			fprintf(fp, " \"%.*s\"", (int)n, cp);
		else
			fprintf(fp, " \"\"");
		break;
	case T_SOA:
		fprintf(fp, "%s.", cp);
		cp += strlen((char *)cp) + 1;
		fprintf(fp, " %s. ( ", cp);
#if defined(RETURNSOA) && defined(NCACHE)
		if (dp->d_rcode == NXDOMAIN)
			fputs(";", fp);
#endif
		cp += strlen((char *)cp) + 1;
		GETLONG(n, cp);
		fprintf(fp, "%u", n);
		GETLONG(n, cp);
		fprintf(fp, " %u", n);
		GETLONG(n, cp);
		fprintf(fp, " %u", n);
		GETLONG(n, cp);
		fprintf(fp, " %u", n);
		GETLONG(n, cp);
		fprintf(fp, " %u )", n);
#if defined(RETURNSOA) && defined(NCACHE)
		if (dp->d_rcode == NXDOMAIN)
			fprintf(fp, ";%s.;NXDOMAIN;\t-$", cp);
#endif
		break;
	case T_MX:
	case T_AFSDB:
	case T_RT:
		GETSHORT(n, cp);
		fprintf(fp, "%u", n);
		fprintf(fp, " %s.", cp);
		break;
	case T_SRV:
		GETSHORT(n, cp);	/* priority */
		fprintf(fp, "%u ", n);
		GETSHORT(n, cp);	/* weight */
		fprintf(fp, "%u ", n);
		GETSHORT(n, cp);	/* port */
		fprintf(fp, "%u ", n);
		fprintf(fp, " %s.", cp);
		break;
	case T_PX:
		GETSHORT(n, cp);
		fprintf(fp, "%u", n);
		fprintf(fp, " %s.", cp);
		cp += strlen((char *)cp) + 1;
		fprintf(fp, " %s.", cp);
		break;
	case T_TXT:
	case T_X25:
		end = (u_char *)dp->d_data + dp->d_size;
		(void) putc('"', fp);
		while (cp < end) {
			if ((n = *cp++) != '\0') {
				for (j = n; j > 0 && cp < end; j--)
					if ((*cp < ' ') || (*cp > '~')) {
						fprintf(fp, "\\%03d", *cp++);
					} else if (*cp == '\\' || *cp =='"') {
						putc('\\', fp);
						putc(*cp++, fp);
					} else
						(void) putc(*cp++, fp);
			}
			if (cp != end) 
				fputs("\" \"", fp);
		}
		/* XXXVIX need to keep the segmentation (see 4.9.5). */
		(void) fputs("\"", fp);
		break;
	case T_NSAP:
		(void) fputs(inet_nsap_ntoa(dp->d_size, dp->d_data, NULL), fp);
		break;
	case T_LOC:
		(void) fputs(loc_ntoa(dp->d_data, NULL), fp);
		break;
	case T_WKS:
		GETLONG(addr, cp);	
		addr = htonl(addr);	
		fputs(inet_ntoa(*(struct in_addr *)&addr), fp);
		proto = protocolname((u_char)*cp);
		cp += sizeof(char); 
		fprintf(fp, "%s ", proto);
		i = 0;
		while(cp < (u_char *)dp->d_data + dp->d_size) {
			j = *cp++;
			do {
				if (j & 0200)
					fprintf(fp, " %s",
						servicename(i, proto));
				j <<= 1;
			} while (++i & 07);
		} 
		break;
	case T_MINFO:
	case T_RP:
		fprintf(fp, "%s.", cp);
		cp += strlen((char *)cp) + 1;
		fprintf(fp, " %s.", cp);
		break;
	case T_KEY:
		savecp = cp;  /* save the beginning */
		/*>>> Flags (unsigned_16) */
		NS_GET16(keyflags,cp);
		fprintf(fp, "0x%04x ", keyflags);
		/*>>> Protocol (8-bit decimal) */
		fprintf(fp, "%3u ", *cp++);
		/*>>> Algorithm id (8-bit decimal) */
		fprintf(fp, "%3u ", *cp++);

		/*>>> Public-Key Data (multidigit BASE64) */
		/*    containing ExponentLen, Exponent, and Modulus */
		i = b64_ntop(cp, dp->d_size - (cp - savecp),
			     temp_base64, sizeof temp_base64);
		if (i < 0)
			fprintf(fp, "; BAD BASE64");
		else
			fprintf(fp, "%s", temp_base64);
		break;
	case T_SIG:
		sigdata = cp;
		/* RRtype (char *) */
		NS_GET16(n,cp);
		fprintf(fp, "%s ", p_type(n));
		/* Algorithm id (8-bit decimal) */
		fprintf(fp, "%d ", *cp++);
		/* Labels (8-bit decimal) (not saved in file) */
		/* XXXX FIXME -- check value and print err if bad */
		cp++;
		/* OTTL (u_long) */
		NS_GET32(n, cp);
		fprintf(fp, "%u ", n);
		/* Texp (u_long) */
		NS_GET32(n, cp);
		fprintf(fp, "%s ", p_secstodate (n));
		/* Tsig (u_long) */
		NS_GET32(n, cp);
		fprintf(fp, "%s ", p_secstodate (n));
		/* Kfootprint (unsigned_16) */
		NS_GET16(n, cp);
		fprintf(fp, "%u ", n);
		/* Signer's Name (char *)  */
		fprintf(fp, "%s ", cp);
		cp += strlen((char *)cp) + 1;
		/* Signature (base64 of any length) */
		i = b64_ntop(cp, dp->d_size - (cp - sigdata),
			     temp_base64, sizeof temp_base64);
		if (i < 0)
			fprintf(fp, "; BAD BASE64");
		else
			fprintf(fp, "%s", temp_base64);
		break;

	case T_NXT:
		fprintf(fp, "%s.", cp);
		n = strlen ((char *)cp) + 1;
		cp += n;
		i = 8 * (dp->d_size - n);  /* How many bits? */
		for (n = 0; n < (u_int32_t)i; n++) {
			if (NS_NXT_BIT_ISSET(n, cp))
				fprintf(fp," %s",__p_type(n));
		}
		break;
	case ns_t_cert:
		certdata = cp;
		NS_GET16(n,cp);
		fprintf(fp, "%d ", n); /* cert type */

		NS_GET16(n,cp);
		fprintf(fp, "%d %d ", n, *cp++); /* tag & alg */

		/* Certificate (base64 of any length) */
		i = b64_ntop(cp, dp->d_size - (cp - certdata),
			     temp_base64, sizeof(temp_base64));
		if (i < 0)
			fprintf(fp, "; BAD BASE64");
		else
			fprintf(fp, "%s", temp_base64);
		break;
	case ns_t_aaaa: {
		char t[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"];

		(void) fputs(inet_ntop(AF_INET6, dp->d_data, t, sizeof t), fp);
		break;
	}
	default:
		fprintf(fp, "\\# %u", dp->d_size);
		if (dp->d_size) {
			fputs(" ( ", fp);
			isc_puthexstring(fp, dp->d_data, dp->d_size,	
					 28, 48, "\n\t\t\t\t");
			fputs(" )", fp);
		}
	}
}

/*
 * Return the number of authoritative zones that "dname" could belong to by
 * stripping up to "depth" labels from dname.  Up to the first "maxzones"
 * authoritative zone numbers will be stored in "zonelist", ordered
 * deepest match first.
 */
int
findzone(const char *dname, int class, int depth, int *zonelist, int maxzones){
	char *tmpdname;
	char tmpdnamebuf[MAXDNAME];
	char *zonename;
	int tmpdnamelen, zonenamelen, zonenum, i, j, c;
	int matches = 0;
	int escaped, found, done;

	ns_debug(ns_log_update, 4, "findzone(dname=%s, class=%d, depth=%d, \
zonelist=%p, maxzones=%d)",
		 dname, class, depth, zonelist, maxzones);
#ifdef DEBUG
	if (debug >= 5) {
		ns_debug(ns_log_update, 5, "zone dump:");
		for (zonenum = 1; zonenum < nzones; zonenum++)
			printzoneinfo(zonenum, ns_log_update, 5);
	}
#endif

	strcpy(tmpdnamebuf, dname);
	tmpdname = tmpdnamebuf;
	/*
	 * The code to handle trailing dots and escapes is adapted
	 * from ns_samedomain().
	 */
	tmpdnamelen = strlen(tmpdname);
	/* 
	 * Ignore a trailing label separator (i.e. an unescaped dot)
	 * in 'tmpdname'.
	 */
	if (tmpdnamelen && tmpdname[tmpdnamelen-1] == '.') {
		escaped = 0;
		/* note this loop doesn't get executed if tmpdnamelen==1 */
		for (j = tmpdnamelen - 2; j >= 0; j--)
			if (tmpdname[j] == '\\') {
				if (escaped)
					escaped = 0;
				else
					escaped = 1;
			} else {
				break;
			}
		if (!escaped) {
			tmpdnamelen--;
			tmpdname[tmpdnamelen] = '\0';
		}
	}

	for (done = i = 0; i <= depth && !done; i++) {
		for (zonenum = 1; zonenum < nzones; zonenum++) {
			if (zones[zonenum].z_type == z_nil)
				continue;
			if (zones[zonenum].z_class != class)
				continue;
			zonename = zones[zonenum].z_origin;
			zonenamelen = strlen(zonename);
			/* 
			 * Ignore a trailing label separator 
			 * (i.e. an unescaped dot) in 'zonename'.
			 */
			if (zonenamelen && zonename[zonenamelen-1] == '.') {
				escaped = 0;
				for (j = zonenamelen - 2; j >= 0; j--)
					if (zonename[j] == '\\') {
						if (escaped)
							escaped = 0;
						else
							escaped = 1;
					} else {
						break;
					}
				if (!escaped)
					zonenamelen--;
			}
			
			if (tmpdnamelen != zonenamelen)
				continue;
			ns_debug(ns_log_update, 5,
				 "about to strncasecmp('%s', '%s', %d)",
				 tmpdname, zonename, tmpdnamelen);
			/* XXXRTH I'm doing a special test for zonenamelen == 0
			   because I worry that some implementations of 
			   strncasecmp might not handle comparisions where
			   n==0 correctly */
			if (zonenamelen == 0 ||
			    !strncasecmp(tmpdname, zonename, tmpdnamelen)) {
				ns_debug(ns_log_update, 5, "match");
				zonelist[matches++] = zonenum;
				if (matches == maxzones) {
					/* XXX should signal error */
					return (matches);
				}
			}
		}

		/*
		 * Strip off the first label if we're not already at
		 * the root label.
		 */
		if (*tmpdname != '\0') {
			for (escaped = found = 0;
			     (c = *tmpdname) && !found;
			     tmpdname++) {
				if (!escaped && (c == '.'))
					/*
					 * Note the loop increment will
					 * make tmpdname point past the '.'
					 * before the '!found' test causes
					 * us to exit the loop.
					 */
					found = 1;
				
				if (escaped)
					escaped = 0;
				else if (c == '\\')
					escaped = 1;
			}
		} else
			done = 1;

		tmpdnamelen = strlen(tmpdname);
	}
	ns_debug(ns_log_update, 4,
		 "findzone: returning %d match(es)", matches);
	return (matches);
}

/*
 * reapply lost updates from log file for the zone to the zone
 *
 * returns -1 on error, 0 on success, 1 if dump reload needed
 */
int
merge_logs(struct zoneinfo *zp, char *logname) {
	char origin[MAXDNAME], data[MAXDATA], dnbuf[MAXDNAME], sclass[3];
	char buf[BUFSIZ];
	FILE *fp;
	u_int32_t serial, ttl, old_serial, new_serial;
	char *dname, *cp, *cp1;
	int type, class;
	int i, c, section, opcode, matches, zonenum = 0, err, multiline;
	int nonempty_lineno = -1, prev_pktdone = 0, cont = 0, inside_next = 0;
	int id, rcode = NOERROR;
	u_int32_t n;
	struct map *mp;
	ns_updrec *rrecp;
	struct databuf *dp;
	struct in_addr ina;
	int zonelist[MAXDNAME];
	struct stat st;
	struct sockaddr_in empty_from;
	int datasize;
	unsigned long l;
	ns_updque curupd;
	unsigned long lutmp;

	empty_from.sin_family = AF_INET;
	empty_from.sin_addr.s_addr = htonl(INADDR_ANY);
	empty_from.sin_port = htons(0);

	/* XXX - much of this stuff is similar to that in nsupdate.c
	 * getword_str() was used in nsupdate.c for reasons described there
	 * getword() is used here just to be consistent with db_load()
	 */
	
	ns_debug(ns_log_update, 3, "merge_logs(%s)", logname);

	/* If there is no log file, just return. */
	if (stat(logname, &st) < 0) {
		if (errno != ENOENT)
			ns_error(ns_log_update,
				 "unexpected stat(%s) failure: %s",
				 logname, strerror(errno));
		return (-1);
	}
	fp = fopen(logname, "r");
	if (fp == NULL) {
		ns_error(ns_log_update, "fopen(%s) failed: %s",
			 logname, strerror(errno));
		return (-1);
	}

	/*
	 * See if we really have a log file -- it might be a zone dump
	 * that was in the process of being isc_movefiled, or it might
	 * be garbage!
	 */

	if (fgets(buf, sizeof(buf), fp)==NULL) {
		ns_error(ns_log_update, "fgets() from %s failed: %s",
			 logname, strerror(errno));
		fclose(fp);
		return (-1);
	}
	if (strcmp(buf, DumpSignature) == 0) {
		/* It's a dump; finish isc_movefile that was interrupted. */
		ns_info(ns_log_update,
			"completing interrupted dump isc_movefile for %s",
			zp->z_source);
		fclose(fp);
		if (isc_movefile(logname, zp->z_source) < 0) {
			ns_error(ns_log_update,
				 "isc_movefile(%s,%s) failed: %s :1",
				 logname, zp->z_source,
				 strerror(errno));
			return (-1);
		}
		/* Finally, tell caller to reload zone. */
		return (1);
	}
	if (strcmp(buf, LogSignature) != 0) {
		/* Not a dump and not a log; complain and then bail out. */
		ns_error(ns_log_update, "invalid log file %s",
			 logname);
		fclose(fp);
		return (-1);
	}

	ns_debug(ns_log_update, 3, "merging logs for %s from %s",
		 zp->z_origin, logname);
	lineno = 1;
	INIT_LIST(curupd);
	for (;;) {
		err = 0;
		dname = NULL;
		if (!getword(buf, sizeof buf, fp, 0)) {
			if (lineno == (nonempty_lineno + 1) && !(feof(fp))) {
				/*
				 * End of a nonempty line inside an update
				 * packet or not inside an update packet.
				 */
				continue;
			}
			/*
			 * Empty line or EOF.
			 *
			 * Marks completion of current update packet.
			 */
			inside_next = 0;
			prev_pktdone = 1;
			cont = 1;
		} else {
			nonempty_lineno = lineno;
		}

		if (!strcasecmp(buf, "[DYNAMIC_UPDATE]") ||
		    !strcasecmp(buf, "[IXFR_UPDATE]")) {
			err = 0;
			rcode = NOERROR;
			cp = fgets(buf, sizeof buf, fp);
			if (cp != NULL)
				lineno++;
			if (cp == NULL || !sscanf((char *)cp, "id %d", &id))
				id = -1;
			inside_next = 1;
			prev_pktdone = 1;
			cont = 1;
		} else if (!strcasecmp(buf, "[INCR_SERIAL]")) {
			/* XXXRTH not enough error checking here */
			cp = fgets(buf, sizeof buf, fp);
			if (cp != NULL)
				lineno++;
			if (cp == NULL || 
			    !sscanf((char *)cp, "from %u to %u",
				    &old_serial, &new_serial)) {
				ns_error(ns_log_update,
					 "incr_serial problem with %s",
					 logname);
			} else {
				serial = get_serial(zp);
				if (serial != old_serial) {
					ns_error(ns_log_update,
		  "serial number mismatch (log=%u, zone=%u) in %s", old_serial,
						 serial, logname);
				} else {
					set_serial(zp, new_serial);
					/*
					 * The zone has changed; make sure
					 * a dump is scheduled.
					 */
					(void)schedule_dump(zp);
					sched_zone_maint(zp);
					ns_info(ns_log_update,
					      "set serial to %u (log file %s)",
						new_serial, logname);
				}
			}
			prev_pktdone = 1;
			cont = 1;
		} else if (!strcasecmp(buf, "[END_DELTA]")) {
			prev_pktdone = 1;
			cont = 1;
		}
		if (prev_pktdone) {
			if (!EMPTY(curupd)) {
				n = process_updates(&curupd, &rcode,
						    empty_from);
				free_rrecp(&curupd, rcode, empty_from);
				if (n > 0)
					ns_info(ns_log_update,
			   "successfully merged update id %d from log file %s",
						id, logname);
				else {
					ns_error(ns_log_update,
				 "error merging update id %d from log file %s",
						 id, logname);
					fclose(fp);
					return(-1);
				}
			}
			prev_pktdone = 0;
			if (feof(fp))
				break;
		}
		if (cont) {
			cont = 0;
			continue;
		}
		if (!inside_next)
			continue;
		/*
		 * inside the same update packet,
		 * continue accumulating records.
		 */
		section = -1;
		n = strlen(buf);
		if (buf[n-1] == ':')
			buf[--n] = '\0';
		for (mp = m_section; mp < m_section+M_SECTION_CNT; mp++)
			if (!strcasecmp(buf, mp->token)) {
				section = mp->val;
				break;
			}
		ttl = 0;
		type = -1;
		class = zp->z_class;
		n = 0;
		data[0] = '\0';
		switch (section) {
		case S_ZONE:
			cp = fgets(buf, sizeof buf, fp);
			if (!cp)
				*buf = '\0';
			n = sscanf(cp, "origin %s class %s serial %lu",
				   origin, sclass, &lutmp);
			serial = lutmp;
			if (n != 3 || ns_samename(origin, zp->z_origin) != 1)
				err++;
			if (cp)
				lineno++;
			if (!err && serial != zp->z_serial) {
				ns_error(ns_log_update,
	      "serial number mismatch in update id %d (log=%u, zone=%u) in %s",
					 id, serial, zp->z_serial,
					 logname);
				inside_next = 0;
				err++;
			}
			if (!err && inside_next) {
			        int success;

				dname = origin;
				type = T_SOA;
				class = res_nametoclass(sclass, &success);
				if (!success) {
				        err++;
					break;
				}
				matches = findzone(dname, class, 0, 
						   zonelist, MAXDNAME);
				if (matches)
					zonenum = zonelist[0];
				else
					err++;
			}
			break;
		case S_PREREQ:
		case S_UPDATE:
			/* Operation code. */
			if (!getword(buf, sizeof buf, fp, 0)) {
				err++;
				break;
			}
			opcode = -1;
			if (buf[0] == '{') {
				n = strlen(buf);
				for (i = 0; (u_int32_t)i < n; i++)
					buf[i] = buf[i+1];
				if (buf[n-2] == '}')
					buf[n-2] = '\0';
			}
			for (mp = m_opcode; mp < m_opcode+M_OPCODE_CNT; mp++)
				if (!strcasecmp(buf, mp->token)) {
					opcode = mp->val;
					break;
				}
			if (opcode == -1) {
				err++;
				break;
			}
			/* Owner's domain name. */
			if (!getword((char *)dnbuf, sizeof dnbuf, fp, 1)) {
				err++;
				break;
			}
			n = strlen((char *)dnbuf) - 1;
			if (dnbuf[n] == '.')
				dnbuf[n] = '\0';
			dname = dnbuf;
			ttl = 0;
			type = -1;
			class = zp->z_class;
			n = 0;
			data[0] = '\0';
			(void) getword(buf, sizeof buf, fp, 1);
			if (isdigit(buf[0])) {	/* ttl */
				if (ns_parse_ttl(buf, &l) < 0) {
					err++;
					break;
				}
				ttl = l;
				(void) getword(buf, sizeof buf, fp, 1);
			}

			/* possibly class */
			if (buf[0] != '\0') {
				int success;
				int maybe_class;
				
				maybe_class = res_nametoclass(buf, &success);
				if (success) {
					class = maybe_class;
					(void) getword(buf,
						       sizeof buf,
						       fp, 1);
				}
			}
			/* possibly type */
			if (buf[0] != '\0') {
				int success;
				int maybe_type;

				maybe_type = res_nametotype(buf, &success);

				if (success) {
					type = maybe_type;
					(void) getword(buf,
						       sizeof buf,
						       fp, 1);
				}
			}
			if (buf[0] != '\0') /* possibly rdata */
				/*
				 * Convert the ascii data 'buf' to the proper
				 * format based on the type and pack into
				 * 'data'.
				 *
				 * XXX - same as in db_load(),
				 * consolidation needed
				 */
				switch (type) {
				case T_A:
					if (!inet_aton(buf, &ina)) {
						err++;
						break;
					}
					n = ntohl(ina.s_addr);
					cp = data;
					PUTLONG(n, cp);
					n = INT32SZ;
					break;
				case T_HINFO:
				case T_ISDN:
					n = strlen(buf);
					data[0] = n;
					memcpy(data+1, buf, n);
					n++;
					if (!getword(buf, sizeof buf,
						     fp, 0)) {
						i = 0;
					} else {
						endline(fp);
						i = strlen(buf);
					}
					data[n] = i;
					memcpy(data+n+1, buf, i);
					break;
				case T_SOA:
				case T_MINFO:
				case T_RP:
					(void) strcpy(data, buf);
					cp = data + strlen(data) -1;
					*(cp++) = 0; /* ditch dot */
					if (!getword((char *)cp,
						     sizeof data - (cp - data),
						     fp, 1)) {
						err++;
						break;
					}
					cp += strlen((char *)cp) -1;
					*(cp++) = 0; /* ditch dot */
					if (type != T_SOA) {
						n = cp - data;
						break;
					}
					else
						n = cp - data;
					if (class != zp->z_class ||
					    ns_samename(dname, zp->z_origin) != 1) {
						err++;
						break;
					}
					c = getnonblank(fp, logname, 1);
					if (c == '(') {
						multiline = 1;
					} else {
						multiline = 0;
						ungetc(c, fp);
					}
					n = getnum(fp, logname, GETNUM_SERIAL,
						   &multiline);
					if (getnum_error) {
						err++;
						break;
					}
					PUTLONG(n, cp);
					for (i = 0; i < 4; i++) {
						if (getttl(fp, logname, lineno,
					   		&n, &multiline) <= 0) 
						{
							err++;
							break;
						}
						PUTLONG(n, cp);
					}
					if (multiline) {
						c = getnonblank(fp, logname, 1);
						if (c != ')') {
							ungetc(c, fp);
							err++;
							break;
						}
					}
					n = cp - data;
					endline(fp);
					break;
				case T_WKS:
					if (!inet_aton(buf, &ina)) {
						err++;
						break;
					}
					n = ntohl(ina.s_addr);
					cp = data;
					PUTLONG(n, cp);
					*cp = (char)getprotocol(fp,
								logname
								);
					n = INT32SZ + sizeof(char);
					n = getservices((int)n, data,
							fp, logname);
					break;
				case T_NS:
				case T_CNAME:
				case T_MB:
				case T_MG:
				case T_MR:
				case T_PTR:
					(void) strcpy(data, buf);
					if (makename(data, origin,
						     sizeof(data)) == -1) {
						err++;
						break;
					}
					n = strlen(data) + 1;
					break;
				case T_MX:
				case T_AFSDB:
				case T_RT:
					n = 0;
					cp = buf;
					while (isdigit(*cp))
						n = n * 10 + (*cp++ - '0');
					/* catch bad values */
					cp = data;
					PUTSHORT((u_int16_t)n, cp);
					if (!getword(buf, sizeof(buf),
						     fp, 1)) {
						err++;
						break;
					}
					(void) strcpy((char *)cp, buf);
					if (makename((char *)cp, origin,
						     sizeof(data) - (cp-data))
					    == -1) {
						err++;
						break;
					}
					/* advance pointer to end of data */
					cp += strlen((char *)cp) +1;
					/* now save length */
					n = (cp - data);
					break;
				case T_PX:
					n = 0;
					data[0] = '\0';
					cp = buf;
					while (isdigit(*cp))
						n = n * 10 + (*cp++ - '0');
					cp = data;
					PUTSHORT((u_int16_t)n, cp);
					for (i = 0; i < 2; i++) {
						if (!getword(buf,
							     sizeof(buf),
							     fp, 0)) {
							err++;
							break;
						}
						(void) strcpy((char *)cp,
							      buf);
						cp += strlen((char *)cp) + 1;
					}
					n = cp - data;
					break;
				case T_TXT:
				case T_X25:
					i = strlen(buf);
					cp = data;
					datasize = sizeof data;
					cp1 = buf;
					while (i > MAXCHARSTRING) {
						if (datasize <= MAXCHARSTRING){
							ns_error(ns_log_update,
							     "record too big");
							fclose(fp);
							return (-1);
						}
						datasize -= MAXCHARSTRING;
						*cp++ = (char)MAXCHARSTRING;
						memcpy(cp, cp1, MAXCHARSTRING);
						cp += MAXCHARSTRING;
						cp1 += MAXCHARSTRING;
						i -= MAXCHARSTRING;
					}
					if (datasize < i + 1) {
						ns_error(ns_log_update,
							 "record too big");
						fclose(fp);
						return (-1);
					}
					*cp++ = i;
					memcpy(cp, cp1, i);
					cp += i;
					n = cp - data;
					endline(fp);
					/* XXXVIX: segmented texts 4.9.5 */
					break;
				case T_NSAP:
					n = inet_nsap_addr(buf,
							   (u_char *)data,
							   sizeof data);
					endline(fp);
					break;
				case T_LOC:
					cp = buf + (n = strlen(buf));
					*cp = ' ';
					cp++;
					while ((i = getc(fp), *cp = i,
						i != EOF)
					       && *cp != '\n'
					       && (n < MAXDATA)) {
						cp++;
						n++;
					}
					if (*cp == '\n')
						ungetc(*cp, fp);
					*cp = '\0';
					n = loc_aton(buf, (u_char *)data);
					if (n == 0) {
						err++;
						break;
					}
					endline(fp);
					break;
				case ns_t_sig:
				case ns_t_key:
				case ns_t_nxt:
				case ns_t_cert:
				    {
					const char * errmsg = NULL;
					int s;

					s = parse_sec_rdata(buf, sizeof(buf),
							    1,
							    (u_char *)data,
							    sizeof(data),
							    fp, zp, dnbuf,
							    ttl, type,
							    domain_ctx,
							    primary_trans,
							    &errmsg);
					if (s < 0) {
						err++;
						break;
					}
					break;
				    }
				default:
					if (strcmp(buf, "\\#") != 0) {
						err++;
						break;
					}
					if (!getword(buf, sizeof buf, fp, 0) ||
					    !isdigit((unsigned char)buf[0])) {
						err++;
						break;
					}
					n = strtoul(buf, &cp, 10);
					if (n > 0xffff || *cp != '\0') {
						err++;
						break;
					}
					multiline = 0;
					i = isc_gethexstring((u_char *)data,
							     sizeof(data),
							     n, fp, &multiline);
					if (i == -1) {
						err++;
						break;
					}
					if (multiline) {
						c = getnonblank(fp,
								zp->z_updatelog,
								1);
						if (c != ')') {
							ungetc(c, fp);
							err++;
							break;
						}
						multiline = 0;
					}
					endline(fp);
				}
			if (section == S_PREREQ) {
				ttl = 0;
				if (opcode == NXDOMAIN) {
					class = C_NONE;
					type = T_ANY;
					n = 0;
				} else if (opcode == YXDOMAIN) {
					class = C_ANY;
					type = T_ANY;
					n = 0;
				} else if (opcode == NXRRSET) {
					class = C_NONE;
					n = 0;
				} else if (opcode == YXRRSET) {
					if (n == 0)
						class = C_ANY;
				}
			} else { /* section == S_UPDATE */
				if (opcode == DELETE) {
					ttl = 0;
					if (n == 0) {
						class = C_ANY;
						if (type == -1)
							type = T_ANY;
		/* WTF?  C_NONE or C_ANY _must_ be the case if
		 *       we really are to delete this.  If
                 *       C_NONE is used, according to process_updates(),
                 *       the class is gotten from the zone's class.
                 *       This still isn't perfect, but it will at least
                 *       work.  
                 *       
                 *       Question: What is so special about the class 
                 *       of the update while we are deleting??
                 */
					} else /* if (zp->z_xferpid != XFER_ISIXFR) */ {
						class = C_NONE;
					}
				}
			}
			break;
		case S_ADDT:
		default:
			ns_debug(ns_log_update, 1,
				 "cannot interpret section: %d", section);
			inside_next = 0;
			err++;
		}
		if (err) {
			ns_debug(ns_log_update, 1, 
		      "merge of update id %d failed due to error at line %d",
				 id, lineno);
			free_rrecp(&curupd, FORMERR, empty_from);
			fclose(fp);
			return(-1);
		}
		rrecp = res_mkupdrec(section, dname, class, type, ttl);
		if (section != S_ZONE) {
			dp = savedata(class, type, ttl, (u_char *)data, n);
			dp->d_zone = zonenum;
			dp->d_cred = DB_C_ZONE;
			dp->d_clev = nlabels(zp->z_origin);
			dp->d_secure = DB_S_INSECURE; /* should be UNCHECKED */
			rrecp->r_dp = dp;
		} else {
			rrecp->r_zone = zonenum;
		}
		APPEND(curupd, rrecp, r_link);
	} /* for (;;) */
	INSIST(EMPTY(curupd));
	fclose(fp);
	return (0);
}


/*
 * Create a disk database to back up zones 
 */
int
zonedump(struct zoneinfo *zp, int mode) {
	FILE		*fp;
	const char	*fname;
	struct hashbuf	*htp;
	char		*op;
	struct stat	st;
	char            tmp_name[MAXPATHLEN];
	int		escaped;
	char		c;

	/*
	 * We must check to see if Z_NEED_SOAUPDATE is set, and if so
	 * we must do it.  This won't be the case normally
	 * (when called from ns_maint()), but it is possible if we're 
	 * exiting named. 
	 */

	if (zp->z_flags & Z_NEED_SOAUPDATE) {
		u_int32_t serial, old_serial;

		old_serial = get_serial(zp);
		serial = old_serial + 1;
		if (serial == 0)
			serial = 1;
		set_serial(zp, serial);
	}

	/* Only dump zone if there is a cache specified */
	if (zp->z_source && *(zp->z_source)) {
		ns_debug(ns_log_update, 1, "zonedump(%s)", zp->z_source);

		if (strlen(zp->z_source)+strlen(DumpSuffix) >= MAXPATHLEN) {
			ns_error(ns_log_update,
				 "filename %s too long in zonedump",
				 zp->z_source);
			/* 
			 * This problem won't ever get better, so we
			 * clear the "need dump" flag.
			 */
			zp->z_flags &= ~Z_NEED_DUMP;
			return (-1);
		}
		(void)sprintf(tmp_name, "%s%s", zp->z_source, DumpSuffix);
		if ((fp = write_open(tmp_name)) == NULL) {
			ns_error(ns_log_update, "fopen() of %s failed: %s",
				 tmp_name, strerror(errno));
			return (-1);
		}
		fprintf(fp, "%s", DumpSignature);
		op = zp->z_origin;
		escaped = 0;
		while (*op && (((c = *op++) != '.') || escaped))
			escaped = (c == '\\') && !escaped;
		gettime(&tt);
		htp = hashtab;
		if (nlookup(zp->z_origin, &htp, &fname, 0) != NULL) {
			if (db_dump(htp, fp, zp-zones, op) != OK) {
				ns_error(ns_log_update,
					 "error dumping zone file %s",
					 zp->z_source); 
				(void)fclose(fp);
				return (-1);
			}
		}
		if (fflush(fp) == EOF) {
			ns_error(ns_log_update, "fflush() of %s failed: %s",
				 tmp_name, strerror(errno));
			fclose(fp);
			return (-1);
		}
		if (fsync(fileno(fp)) < 0) {
			ns_error(ns_log_update, "fsync() of %s failed: %s",
				 tmp_name, strerror(errno));
			fclose(fp);
			return (-1);
		}
		if (fclose(fp) == EOF) {
			ns_error(ns_log_update, "fclose() of %s failed: %s",
				 tmp_name, strerror(errno));
			return (-1);
		}
		/*
		 * Try to make read only, so people will be less likely to
		 * edit dynamic domains.
		 */
		if (stat(tmp_name, &st) < 0) {
			ns_error(ns_log_update,
				 "stat(%s) failed, pressing on: %s",
				 tmp_name, strerror(errno));
		} else {
			zp->z_ftime = st.st_mtime;
			st.st_mode &= ~WRITEABLE_MASK;
			if (chmod(tmp_name, st.st_mode) < 0)
				ns_error(ns_log_update,
					"chmod(%s,%o) failed, pressing on: %s",
					 tmp_name, st.st_mode,
					 strerror(errno));
		}

		if (mode == ISIXFR) {
			if (isc_movefile(tmp_name, zp->z_ixfr_tmp) < 0) {
				ns_error(ns_log_update,
					 "isc_movefile(%s,%s) failed: %s :2",
					 tmp_name, zp->z_ixfr_tmp,
					 strerror(errno));
				return (-1);
			}
			if (chmod(zp->z_source, 0644) < 0)
				ns_error(ns_log_update,
					"chmod(%s,%o) failed, pressing on: %s",
					 zp->z_source, st.st_mode,
					 strerror(errno));
			if (isc_movefile(zp->z_ixfr_tmp, zp->z_source) < 0) {
				ns_error(ns_log_update,
					 "isc_movefile(%s,%s) failed: %s :3",
					 zp->z_ixfr_tmp, zp->z_source,
					 strerror(errno));
				return (-1);
			}
			st.st_mode &= ~WRITEABLE_MASK;
			if (chmod(zp->z_source, st.st_mode) < 0)
				ns_error(ns_log_update,
					"chmod(%s,%o) failed, pressing on: %s",
					 zp->z_source, st.st_mode,
					 strerror(errno));
		} else if (mode == ISNOTIXFR) {
			if (isc_movefile(tmp_name, zp->z_updatelog) < 0) {
				ns_error(ns_log_update,
					 "isc_movefile(%s,%s) failed: %s :4",
				 	 tmp_name, zp->z_updatelog,
					 strerror(errno));
				return (-1);
			}
			if (isc_movefile(zp->z_updatelog, zp->z_source) < 0) {
				ns_error(ns_log_update,
					 "isc_movefile(%s,%s) failed: %s :5",
				 	 zp->z_updatelog, zp->z_source,
				 	 strerror(errno));
				return (-1);
			}
		} else {
			if (isc_movefile(tmp_name, zp->z_source) < 0) {
				ns_error(ns_log_update,
					 "isc_movefile(%s,%s) failed: %s :6",
					 tmp_name, zp->z_source,
					 strerror(errno));
				return (-1);
			}
		}
	} else
		ns_debug(ns_log_update, 1, "zonedump: no zone to dump");

	zp->z_flags &= ~Z_NEED_DUMP;
	zp->z_dumptime = 0;
	return (0);
}

struct databuf *
findzonesoa(struct zoneinfo *zp) {
	struct hashbuf *htp;
	struct namebuf *np;
	struct databuf *dp;
	const char *fname;

	htp = hashtab;
	np = nlookup(zp->z_origin, &htp, &fname, 0);
	if (np == NULL || fname != zp->z_origin)
		return (NULL);
	foreach_rr(dp, np, T_SOA, zp->z_class, zp - zones)
		return (dp);
	return (NULL);
}

u_char *
findsoaserial(u_char *data) {
	char *cp = (char *)data;

	cp += strlen(cp) + 1;	/* Nameserver. */
	cp += strlen(cp) + 1;	/* Mailbox. */
	return ((u_char *)cp);
}

u_int32_t
get_serial_unchecked(struct zoneinfo *zp) {
	struct databuf *dp;
	u_char *cp;
	u_int32_t ret;

	dp = findzonesoa(zp);
	if (!dp)
		ns_panic(ns_log_update, 1,
			 "get_serial_unchecked(%s): can't locate zone SOA",
			 zp->z_origin);
	cp = findsoaserial(dp->d_data);
	GETLONG(ret, cp);
	return (ret);
}

u_int32_t
get_serial(struct zoneinfo *zp) {
	u_int32_t ret;

	ret = get_serial_unchecked(zp);
	if (ret != zp->z_serial)
		ns_panic(ns_log_update, 1,
			 "get_serial(%s): db and zone serial numbers differ",
			 zp->z_origin);
	return (ret);
}

void
set_serial(struct zoneinfo *zp, u_int32_t serial) {
	struct databuf *dp;
	u_char *cp;

	dp = findzonesoa(zp);
	if (!dp)
		ns_panic(ns_log_update, 1,
			 "set_serial(%s): can't locate zone SOA",
			 zp->z_origin);
	cp = findsoaserial(dp->d_data);
	PUTLONG(serial, cp);
	zp->z_serial = serial;
	zp->z_flags &= ~Z_NEED_SOAUPDATE;
	zp->z_soaincrtime = 0;
	zp->z_updatecnt = 0;
#ifdef BIND_NOTIFY
	if (!loading)
		ns_notify(zp->z_origin, zp->z_class, ns_t_soa);
#endif
	/*
	 * Note: caller is responsible for scheduling a dump.
	 */
}

/*
 * Increment serial number in zoneinfo structure and hash table SOA databuf
 */
 
int
incr_serial(struct zoneinfo *zp) {
	u_int32_t serial, old_serial;
	FILE *fp, *ifp;
	time_t t;
	struct databuf *dp, *olddp;
	unsigned char *cp;
 
	old_serial = get_serial(zp);
	serial = old_serial + 1;
	if (serial == 0)
	        serial = 1;
	set_serial(zp, serial);

	(void) gettime(&tt);
	t = (time_t)tt.tv_sec;
	fp = open_transaction_log(zp);
	if (fp == NULL)
		return (-1);
	fprintf(fp, "[INCR_SERIAL] from %u to %u %s\n",
		old_serial, serial, checked_ctime(&t));
	if (close_transaction_log(zp, fp)<0)
		return (-1);
	if (zp->z_maintain_ixfr_base) {
		ifp = open_ixfr_log(zp);
		if (ifp == NULL)
			return (-1);
		dp = findzonesoa(zp);
		if (dp) {
			olddp = memget(DATASIZE(dp->d_size));
			if (olddp != NULL) {
				memcpy(olddp, dp, DATASIZE(dp->d_size));
				cp = findsoaserial(olddp->d_data);
				PUTLONG(old_serial, cp);
				fprintf(ifp, "update: {delete} %s. %u %s %s ",
					zp->z_origin, dp->d_ttl,
					p_class(dp->d_class),
					p_type(dp->d_type));
				(void) rdata_dump(olddp, ifp);
				fprintf(ifp, "\n");
				memput(olddp, DATASIZE(dp->d_size));
			}
			fprintf(ifp, "update: {add} %s. %u %s %s ",
				zp->z_origin, dp->d_ttl,
    				p_class(dp->d_class), p_type(dp->d_type));
			(void) rdata_dump(dp, ifp);
			fprintf(ifp, "\n");
		}
		fprintf(ifp, "[END_DELTA]\n");
		if (close_ixfr_log(zp, ifp) < 0)
			return (-1);
	}

	/*
	 * This shouldn't happen, but we check to be sure.
	 */
	if (!(zp->z_flags & Z_NEED_DUMP)) {
		ns_warning(ns_log_update,
			   "incr_serial: Z_NEED_DUMP not set for zone '%s'",
			   zp->z_origin);
		(void)schedule_dump(zp);
	}

	sched_zone_maint(zp);

	return (0);
}

void
dynamic_about_to_exit(void) {
	struct zoneinfo *zp;

	ns_debug(ns_log_update, 1,
		 "shutting down; dumping zones that need it");
	for (zp = zones; zp < &zones[nzones]; zp++) {
		if ((zp->z_flags & Z_DYNAMIC) &&
		    ((zp->z_flags & Z_NEED_SOAUPDATE) ||
		     (zp->z_flags & Z_NEED_DUMP)))
			(void)zonedump(zp, ISNOTIXFR);
	}
}
