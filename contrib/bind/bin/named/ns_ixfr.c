#if !defined(lint) && !defined(SABER)
static const char rcsid[] = "$Id: ns_ixfr.c,v 8.26.2.2 2001/08/10 03:00:08 marka Exp $";
#endif /* not lint */

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

#include "port_before.h"

#include <sys/param.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>

#include <errno.h>
#include <fcntl.h>
#include <resolv.h>
#include <res_update.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

#include <isc/eventlib.h>
#include <isc/logging.h>
#include <isc/memcluster.h>
#include <isc/misc.h>

#include "port_after.h"

#include "named.h"

static void	sx_new_ixfrmsg(struct qstream * qsp);
void		sx_send_ixfr(struct qstream * qsp);

static int	sx_flush(struct qstream * qsp),
		sx_addrr(struct qstream * qsp,
			 const char *dname,
			 struct databuf * dp);

/*
 * u_char * sx_new_ixfrmsg(msg) init the header of a message, reset the
 * compression pointers, and reset the write pointer to the first byte
 * following the header.
 */
static void
sx_new_ixfrmsg(struct qstream *qsp) {
	HEADER *	hp = (HEADER *) qsp->xfr.msg;

	memset(hp, 0, HFIXEDSZ);
	hp->id = htons(qsp->xfr.id);
	hp->opcode = qsp->xfr.opcode;
	hp->qr = 1;
	hp->aa = 1;
	hp->rcode = NOERROR;

	qsp->xfr.ptrs[0] = qsp->xfr.msg;
	qsp->xfr.ptrs[1] = NULL;

	qsp->xfr.cp = qsp->xfr.msg + HFIXEDSZ;
	if (qsp->xfr.ixfr_zone == 0) {
		int		count, n;
		int		buflen;
		struct namebuf *np;
		struct hashbuf *htp;
		struct zoneinfo *zp;
		const char *	fname;

		qsp->xfr.ixfr_zone = qsp->xfr.zone;
		zp = &zones[qsp->xfr.zone];
		n = dn_comp(zp->z_origin, qsp->xfr.cp,
		   XFER_BUFSIZE - (qsp->xfr.cp - qsp->xfr.msg), NULL, NULL);
		qsp->xfr.cp += n;
		PUTSHORT((u_int16_t) T_IXFR, qsp->xfr.cp);
		PUTSHORT((u_int16_t) zp->z_class, qsp->xfr.cp);
		hp->qdcount = htons(ntohs(hp->qdcount) + 1);
		count = qsp->xfr.cp - qsp->xfr.msg;
		htp = hashtab;
		np = nlookup(zp->z_origin, &htp, &fname, 0);
		buflen = XFER_BUFSIZE;
	}
}

/*
 * int
 * sx_flush(qsp)
 *	flush the intermediate buffer out to the stream IO system.
 * return:
 *	passed through from sq_write().
 */
static int
sx_flush(struct qstream *qsp) {
	int ret;

#ifdef DEBUG
	if (debug >= 10)
		fp_nquery(qsp->xfr.msg, qsp->xfr.cp - qsp->xfr.msg,
			  log_get_stream(packet_channel));
#endif
	if (qsp->xfr.tsig_state != NULL && qsp->xfr.tsig_skip == 0) {
		int msglen = qsp->xfr.cp - qsp->xfr.msg;

		ns_sign_tcp(qsp->xfr.msg, &msglen, qsp->xfr.eom - qsp->xfr.msg,
			    NOERROR, qsp->xfr.tsig_state,
			    qsp->xfr.state == s_x_done);

		if (qsp->xfr.state == s_x_done) {
			memput(qsp->xfr.tsig_state, sizeof(ns_tcp_tsig_state));
			qsp->xfr.tsig_state = NULL;
		}
		qsp->xfr.cp = qsp->xfr.msg + msglen;
	
	}
	if (qsp->xfr.cp - qsp->xfr.msg > 0)
		ret = sq_write(qsp, qsp->xfr.msg, qsp->xfr.cp - qsp->xfr.msg);
	else {
		ns_debug(ns_log_default, 3, " Flush negative number *********");
		ret = -1;
	}
	if (ret >= 0) {
		qsp->xfr.cp = NULL;
		qsp->xfr.tsig_skip = 0;
	}
	else
		qsp->xfr.tsig_skip = 1;
	return (ret);
}
/*
 * int sx_addrr(qsp, name, dp) add name/dp's RR to the current assembly
 * message.  if it won't fit, write current message out, renew the message,
 * and then RR should fit. return: -1 = the sq_write() failed so we could not
 * queue the full message. 0 = one way or another, everything is fine. side
 * effects: on success, the ANCOUNT is incremented and the pointers are
 * advanced.
 */
static int
sx_addrr(struct qstream *qsp, const char *dname, struct databuf *dp) {
	HEADER *hp = (HEADER *) qsp->xfr.msg;
	u_char **edp = qsp->xfr.ptrs + sizeof qsp->xfr.ptrs / sizeof(u_char *);
	int n;

	if (qsp->xfr.cp != NULL) {
		if (qsp->xfr.transfer_format == axfr_one_answer &&
		    sx_flush(qsp) < 0)
			return (-1);
	}
	if (qsp->xfr.cp == NULL)
		sx_new_ixfrmsg(qsp);
	n = make_rr(dname, dp, qsp->xfr.cp, qsp->xfr.eom - qsp->xfr.cp,
		    0, qsp->xfr.ptrs, edp, 0);
	if (n < 0) {
		if (sx_flush(qsp) < 0)
			return (-1);
		if (qsp->xfr.cp == NULL)
			sx_new_ixfrmsg(qsp);
		n = make_rr(dname, dp, qsp->xfr.cp, qsp->xfr.eom - qsp->xfr.cp,
			    0, qsp->xfr.ptrs, edp, 0);
		INSIST(n >= 0);
	}
	hp->ancount = htons(ntohs(hp->ancount) + 1);
	qsp->xfr.cp += n;
	return (0);
}

void
sx_send_ixfr(struct qstream *qsp) {
	char *		cp;
	struct zoneinfo *zp = NULL;
	struct databuf *soa_dp;
	struct databuf *old_soadp;
	ns_delta *dp;
	ns_updrec *rp;
	int		foundsoa;

	zp = &zones[qsp->xfr.zone];
	soa_dp = (struct databuf *) findzonesoa(zp);
	if (soa_dp == NULL) {
		/* XXX should be more graceful */
		ns_panic(ns_log_update, 1,
			 "sx_send_ixfr: unable to locate soa");
	}
	old_soadp = memget(DATASIZE(soa_dp->d_size));
	if (old_soadp == NULL)
		ns_panic(ns_log_update, 1, "sx_send_ixfr: out of memory");
	memcpy(old_soadp, soa_dp, DATASIZE(soa_dp->d_size));

 again:
	switch (qsp->xfr.state) {
	case s_x_firstsoa:
		ns_debug(ns_log_default, 3,
			 "IXFR: s_x_firstsoa (%s)", zp->z_origin);
		if (sx_addrr(qsp, zp->z_origin, soa_dp) < 0)
		 	goto cleanup;
		qsp->xfr.state = s_x_deletesoa;
		/* FALLTHROUGH */
	case s_x_deletesoa:
		ns_debug(ns_log_default, 3,
			 "IXFR: s_x_deletesoa (%s)", zp->z_origin);
		dp = NULL;
		if (qsp->xfr.top.ixfr != NULL && !EMPTY(*qsp->xfr.top.ixfr))
			dp = HEAD(*qsp->xfr.top.ixfr);
		if (dp != NULL) {
			foundsoa = 0;

			rp = HEAD(dp->d_changes);
			while (rp != NULL) {
				if (rp->r_opcode == DELETE &&
				    rp->r_dp != NULL &&
				    rp->r_dp->d_type == T_SOA) {
					if (sx_addrr(qsp, rp->r_dname,
						     rp->r_dp) < 0)
						goto cleanup;
					db_freedata(rp->r_dp);
					rp->r_dp = NULL;
					foundsoa = 1;
					break;
				}
				rp = NEXT(rp, r_link);
			} 

			if (!foundsoa) {
				cp = (char *)findsoaserial(old_soadp->d_data);
				PUTLONG(HEAD(dp->d_changes)->r_zone, cp);

				if (sx_addrr(qsp, zp->z_origin, old_soadp) < 0)
					goto cleanup;
			}
		}
		qsp->xfr.state = s_x_deleting;
		/* FALLTHROUGH */
	case s_x_deleting:
		ns_debug(ns_log_default, 3,
			 "IXFR: s_x_deleting (%s)", zp->z_origin);
		dp = NULL;
		if (qsp->xfr.top.ixfr != NULL && !EMPTY(*qsp->xfr.top.ixfr))
			dp = HEAD(*qsp->xfr.top.ixfr);
		if (dp != NULL) {
			rp = HEAD(dp->d_changes);
			while (rp != NULL) {
				if (rp->r_opcode == DELETE &&
				    rp->r_dp != NULL) {
					/*
					 * Drop any SOA deletes
					 */
					if (rp->r_dp->d_type != T_SOA &&
					    sx_addrr(qsp, rp->r_dname,
						     rp->r_dp) < 0)
						goto cleanup;
					db_freedata(rp->r_dp);
					rp->r_dp = NULL;
				}
				rp = NEXT(rp, r_link);
			}
		}
		qsp->xfr.state = s_x_addsoa;
		/* FALLTHROUGH */
	case s_x_addsoa:
		ns_debug(ns_log_default, 3,
			 "IXFR: s_x_addsoa (%s)", zp->z_origin);
		dp = NULL;
		if (qsp->xfr.top.ixfr != NULL && !EMPTY(*qsp->xfr.top.ixfr))
			dp = HEAD(*qsp->xfr.top.ixfr);
		if (dp != NULL) {
			foundsoa = 0;
			rp = HEAD(dp->d_changes);
			while (rp != NULL) {
				if (rp->r_opcode == ADD &&
				    rp->r_dp != NULL &&
				    rp->r_dp->d_type == T_SOA) {
					if (sx_addrr(qsp, rp->r_dname,
						     rp->r_dp) < 0)
						goto cleanup;
					db_freedata(rp->r_dp);
					rp->r_dp = NULL;
					foundsoa = 1;
					break;
				}
				rp = NEXT(rp, r_link);
			}

			if (!foundsoa) {
				cp = (char *)findsoaserial(old_soadp->d_data);
				if (NEXT(dp, d_link) != NULL) {
					PUTLONG(HEAD(dp->d_changes)->r_zone, cp);
					if (sx_addrr(qsp, zp->z_origin,
						     old_soadp) < 0)
						goto cleanup;
				} else {
					if (sx_addrr(qsp, zp->z_origin,
						     soa_dp) < 0)
						goto cleanup;
				}
			}
		}
		qsp->xfr.state = s_x_adding;
		/* FALLTHROUGH */
	case s_x_adding:
		ns_debug(ns_log_default, 3,
			 "IXFR: s_x_adding (%s)", zp->z_origin);
		dp = NULL;
		if (qsp->xfr.top.ixfr != NULL && !EMPTY(*qsp->xfr.top.ixfr)) {
			dp = HEAD(*qsp->xfr.top.ixfr);
			if (dp != NULL) {
				/* see s_x_deleting */
				rp = HEAD(dp->d_changes);
				while (rp != NULL) {
					if (rp->r_opcode == ADD &&
					    rp->r_dp != NULL &&
					    rp->r_dp->d_type != T_SOA) {
						if (sx_addrr(qsp, rp->r_dname,
							     rp->r_dp) < 0)
							goto cleanup;
						db_freedata(rp->r_dp);
						rp->r_dp = NULL;
					}
					rp = NEXT(rp, r_link);
				}

				/* move to next update */
				UNLINK(*qsp->xfr.top.ixfr, dp, d_link);

				/* clean up old update */
				while ((rp = HEAD(dp->d_changes)) != NULL) {
					UNLINK(dp->d_changes, rp, r_link);
					if (rp->r_dp != NULL) {
						db_freedata(rp->r_dp);
						rp->r_dp = NULL;
					}
					res_freeupdrec(rp);
				}
				memput(dp, sizeof (*dp));
				if (HEAD(*qsp->xfr.top.ixfr) != NULL) {
					qsp->xfr.state = s_x_deletesoa;
					goto again;
				}
			}
		}
		qsp->xfr.state = s_x_lastsoa;
		/* FALLTHROUGH */
	case s_x_lastsoa:
		ns_debug(ns_log_default, 3,
			 "IXFR: s_x_lastsoa (%s)", zp->z_origin);
		if (qsp->xfr.ixfr_zone != 0)
			sx_addrr(qsp, zp->z_origin, soa_dp);
		break;
	default:
		break;
	}
	ns_debug(ns_log_default, 3, "IXFR: flushing %s", zp->z_origin);
	qsp->xfr.state = s_x_done;
	sx_flush(qsp);
	sq_writeh(qsp, sq_flushw);
	if (qsp->xfr.top.ixfr != NULL) {
		if(!EMPTY(*qsp->xfr.top.ixfr)) {
			while ((dp = HEAD(*qsp->xfr.top.ixfr)) != NULL)  {
				UNLINK(*qsp->xfr.top.ixfr, dp, d_link);
				while ((rp = HEAD(dp->d_changes)) != NULL) {
					UNLINK(dp->d_changes, rp, r_link);
					if (rp->r_dp != NULL)
						db_freedata(rp->r_dp);
					rp->r_dp = NULL;
					res_freeupdrec(rp);
				} 
				memput(dp, sizeof *dp);
			}
		}
		memput(qsp->xfr.top.ixfr, sizeof *qsp->xfr.top.ixfr);
		qsp->xfr.top.ixfr = NULL;
	}
 cleanup:
	memput(old_soadp, DATASIZE(old_soadp->d_size));
}


#ifndef MAXBSIZE
#define MAXBSIZE 8192
#endif


/*
 * int ixfr_log_maint(struct zoneinfo *zp, int fast_trim)
 *
 * zp - pointer to the zone information
 */
int
ixfr_log_maint(struct zoneinfo *zp) {
	int fd, rcount, wcount;
	int found = 0;
	int error = 0;
	long seek = 0;
	FILE *to_fp, *from_fp, *db_fp;
	char *tmpname;
	int len;
	struct stat db_sb;
	struct stat sb;
	static char buf[MAXBSIZE];

	ns_debug(ns_log_default, 3, "ixfr_log_maint(%s)", zp->z_origin);

	/* find out how big the zone db file is */
	if ((db_fp = fopen(zp->z_source, "r")) == NULL) {
		ns_warning(ns_log_db, "%s: %s",
			   zp->z_source, strerror(errno));
	 	return (-1);
	}
	if (fstat(fileno(db_fp), &db_sb) < 0) {
		ns_warning(ns_log_db, "%s: %s",
			   zp->z_source, strerror(errno));
		(void) my_fclose(db_fp);
		return (-1);
	}
	(void) my_fclose(db_fp);
	ns_debug(ns_log_default, 3, "%s, size %d blk %d", 
	     zp->z_source, db_sb.st_size, 
	     db_sb.st_size);

	/* open up the zone ixfr log */
	if ((from_fp = fopen(zp->z_ixfr_base, "r")) == NULL) {
		ns_warning(ns_log_db, "%s: %s",
			   zp->z_ixfr_base, strerror(errno));
	 	return (-1);
	}

	if (fstat(fileno(from_fp), &sb) < 0) {
		ns_warning(ns_log_db, "%s: %s",
			   zp->z_ixfr_base, strerror(errno));
		(void) my_fclose(from_fp);
		return (-1);
	}
	ns_debug(ns_log_default, 3, "%s, size %d max %d\n", 
	     zp->z_ixfr_base, 
	     sb.st_size, 
	     zp->z_max_log_size_ixfr);
	if (zp->z_max_log_size_ixfr) {
		if (sb.st_size > zp->z_max_log_size_ixfr)
			seek = sb.st_size -
				(size_t)(zp->z_max_log_size_ixfr + 
				(zp->z_max_log_size_ixfr * 0.10) );
		else
			seek = 0;
	} else {
		if (sb.st_size > (db_sb.st_size * 0.50))
			seek = sb.st_size - (size_t)((db_sb.st_size * 0.50)
			 + ((db_sb.st_size * zp->z_max_log_size_ixfr) * 0.10));
		else 
			 seek = 0;
	}
	ns_debug(ns_log_default, 3, "seek: %d", seek);
	if (seek < 1) {
		ns_debug(ns_log_default, 3, "%s does not need to be reduced", 
			zp->z_ixfr_base);
		(void) my_fclose(from_fp);
		return (-1);
	}

	len = strlen(zp->z_ixfr_base) + sizeof(".XXXXXX") + 1;
	tmpname = memget(len);
	if (!tmpname) {
		ns_warning(ns_log_default, "memget failed");
			return (-1);
	}
#ifdef SHORT_FNAMES
	filenamecpy(tmpname, zp->z_ixfr_base);
#else
	(void) strcpy(tmpname, zp->z_ixfr_base);
#endif /* SHORT_FNAMES */

	(void) strcat(tmpname, ".XXXXXX");
	if ((fd = mkstemp(tmpname)) == -1) {
		ns_warning(ns_log_db, "can't make tmpfile (%s): %s",
			   tmpname, strerror(errno));
		memput(tmpname, len);
		(void) my_fclose(from_fp);
	 	return (-1);
	}
	if ((to_fp = fdopen(fd, "r+")) == NULL) {
		ns_warning(ns_log_db, "%s: %s",
			   tmpname, strerror(errno));
		(void) unlink(tmpname);
		memput(tmpname, len);
		(void) my_fclose(from_fp);
		(void) close(fd);
	 	return (-1);
	}

	if (fgets(buf, sizeof(buf), from_fp) == NULL) {
		ns_error(ns_log_update, "fgets() from %s failed: %s",
			 zp->z_ixfr_base, strerror(errno));
		error++;
		goto clean_up;
	}
	if (strcmp(buf, LogSignature) != 0) {
		ns_error(ns_log_update, "invalid log file %s",
			 zp->z_ixfr_base);
		error++;
		goto clean_up;
	}

	if (fseek( from_fp, seek, 0) < 0) {
		error++;
		goto clean_up;
	}

	found = 0;
	for (;;) {
		if (getword(buf, sizeof buf, from_fp, 0)) {
			if (strcasecmp(buf, "[END_DELTA]") == 0) {
				if (!(fgets(buf, 2, from_fp) == NULL)) /* eat <cr><lf> */
					found = 1;
				break;
			} 
		}
		if (feof(from_fp))
			break;
	}
	if (found) {
		ns_debug(ns_log_default, 1, "ixfr_log_maint(): found [END_DELTA]");
	
		fprintf(to_fp, "%s", LogSignature);

		while ((rcount = fread(buf, sizeof(char), MAXBSIZE, from_fp)) > 0) {
			wcount = fwrite(buf, sizeof(char), rcount, to_fp);
			if (rcount != wcount || wcount == -1) {
					ns_warning(ns_log_default,
				     "ixfr_log_maint: error in writting copy");
					break;
			}
		}
		if (rcount < 0)
			ns_warning(ns_log_default,
				   "ixfr_log_maint: error in reading copy");
	}
	clean_up:
	(void) my_fclose(to_fp);
	(void) my_fclose(from_fp);
	if (error == 0) {
		if (isc_movefile(tmpname, zp->z_ixfr_base) == -1) {
			ns_warning(ns_log_default, "can not rename %s to %s :%s",
				   tmpname, zp->z_ixfr_base, strerror(errno));
		}
		if ((from_fp = fopen(zp->z_ixfr_base, "r")) == NULL) {
			ns_warning(ns_log_db, "%s: %s",
				   zp->z_ixfr_base, strerror(errno));
			memput(tmpname, len);
			return (-1);
		}
		if (fstat(fileno(from_fp), &sb) < 0) {
			ns_warning(ns_log_db, "%s: %s",
				   zp->z_ixfr_base, strerror(errno));
			memput(tmpname, len);
			(void) my_fclose(from_fp);
			return (-1);
		}
		if (sb.st_size <= 0)
			(void) unlink(zp->z_ixfr_base);
		else if (chmod(zp->z_ixfr_base, 0644) < 0)
				ns_error(ns_log_update,
					"chmod(%s,%o) failed, pressing on: %s",
					 zp->z_source, sb.st_mode,
					 strerror(errno));
		(void) my_fclose(from_fp);
	}
	(void) unlink(tmpname);
	memput(tmpname, len);

	zp->z_serial_ixfr_start = 0; /* signal to read for lowest serial number */	

	ns_debug(ns_log_default, 3, "%s, size %d max %d\n", 
	     zp->z_ixfr_base, 
	     sb.st_size, 
	     zp->z_max_log_size_ixfr);

	if (error)
	 	return(-1);
	else
	    return (0);
}

