/*	$NetBSD$	*/

/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * Added redirect stuff and a variety of bug fixes. (mcn@EnGarde.com)
 */

#include "ipf.h"
#include "kmem.h"


#if !defined(lint)
static const char rcsid[] = "@(#)Id: printaps.c,v 1.4 2004/01/08 13:34:32 darrenr Exp";
#endif


void printaps(aps, opts)
ap_session_t *aps;
int opts;
{
	ipsec_pxy_t ipsec;
	ap_session_t ap;
	ftpinfo_t ftp;
	aproxy_t apr;
	raudio_t ra;

	if (kmemcpy((char *)&ap, (long)aps, sizeof(ap)))
		return;
	if (kmemcpy((char *)&apr, (long)ap.aps_apr, sizeof(apr)))
		return;
	printf("\tproxy %s/%d use %d flags %x\n", apr.apr_label,
		apr.apr_p, apr.apr_ref, apr.apr_flags);
	printf("\t\tproto %d flags %#x bytes ", ap.aps_p, ap.aps_flags);
#ifdef	USE_QUAD_T
	printf("%qu pkts %qu", (unsigned long long)ap.aps_bytes,
		(unsigned long long)ap.aps_pkts);
#else
	printf("%lu pkts %lu", ap.aps_bytes, ap.aps_pkts);
#endif
	printf(" data %s size %d\n", ap.aps_data ? "YES" : "NO", ap.aps_psiz);
	if ((ap.aps_p == IPPROTO_TCP) && (opts & OPT_VERBOSE)) {
		printf("\t\tstate[%u,%u], sel[%d,%d]\n",
			ap.aps_state[0], ap.aps_state[1],
			ap.aps_sel[0], ap.aps_sel[1]);
#if (defined(NetBSD) && (NetBSD >= 199905) && (NetBSD < 1991011)) || \
    (__FreeBSD_version >= 300000) || defined(OpenBSD)
		printf("\t\tseq: off %hd/%hd min %x/%x\n",
			ap.aps_seqoff[0], ap.aps_seqoff[1],
			ap.aps_seqmin[0], ap.aps_seqmin[1]);
		printf("\t\tack: off %hd/%hd min %x/%x\n",
			ap.aps_ackoff[0], ap.aps_ackoff[1],
			ap.aps_ackmin[0], ap.aps_ackmin[1]);
#else
		printf("\t\tseq: off %hd/%hd min %lx/%lx\n",
			ap.aps_seqoff[0], ap.aps_seqoff[1],
			ap.aps_seqmin[0], ap.aps_seqmin[1]);
		printf("\t\tack: off %hd/%hd min %lx/%lx\n",
			ap.aps_ackoff[0], ap.aps_ackoff[1],
			ap.aps_ackmin[0], ap.aps_ackmin[1]);
#endif
	}

	if (!strcmp(apr.apr_label, "raudio") && ap.aps_psiz == sizeof(ra)) {
		if (kmemcpy((char *)&ra, (long)ap.aps_data, sizeof(ra)))
			return;
		printf("\tReal Audio Proxy:\n");
		printf("\t\tSeen PNA: %d\tVersion: %d\tEOS: %d\n",
			ra.rap_seenpna, ra.rap_version, ra.rap_eos);
		printf("\t\tMode: %#x\tSBF: %#x\n", ra.rap_mode, ra.rap_sbf);
		printf("\t\tPorts:pl %hu, pr %hu, sr %hu\n",
			ra.rap_plport, ra.rap_prport, ra.rap_srport);
	} else if (!strcmp(apr.apr_label, "ftp") &&
		   (ap.aps_psiz == sizeof(ftp))) {
		if (kmemcpy((char *)&ftp, (long)ap.aps_data, sizeof(ftp)))
			return;
		printf("\tFTP Proxy:\n");
		printf("\t\tpassok: %d\n", ftp.ftp_passok);
		ftp.ftp_side[0].ftps_buf[FTP_BUFSZ - 1] = '\0';
		ftp.ftp_side[1].ftps_buf[FTP_BUFSZ - 1] = '\0';
		printf("\tClient:\n");
		printf("\t\tseq %x (ack %x) len %d junk %d cmds %d\n",
			ftp.ftp_side[0].ftps_seq[0],
			ftp.ftp_side[0].ftps_seq[1],
			ftp.ftp_side[0].ftps_len, ftp.ftp_side[0].ftps_junk,
			ftp.ftp_side[0].ftps_cmds);
		printf("\t\tbuf [");
		printbuf(ftp.ftp_side[0].ftps_buf, FTP_BUFSZ, 1);
		printf("]\n\tServer:\n");
		printf("\t\tseq %x (ack %x) len %d junk %d cmds %d\n",
			ftp.ftp_side[1].ftps_seq[0],
			ftp.ftp_side[1].ftps_seq[1],
			ftp.ftp_side[1].ftps_len, ftp.ftp_side[1].ftps_junk,
			ftp.ftp_side[1].ftps_cmds);
		printf("\t\tbuf [");
		printbuf(ftp.ftp_side[1].ftps_buf, FTP_BUFSZ, 1);
		printf("]\n");
	} else if (!strcmp(apr.apr_label, "ipsec") &&
		   (ap.aps_psiz == sizeof(ipsec))) {
		if (kmemcpy((char *)&ipsec, (long)ap.aps_data, sizeof(ipsec)))
			return;
		printf("\tIPSec Proxy:\n");
		printf("\t\tICookie %08x%08x RCookie %08x%08x %s\n",
			(u_int)ntohl(ipsec.ipsc_icookie[0]),
			(u_int)ntohl(ipsec.ipsc_icookie[1]),
			(u_int)ntohl(ipsec.ipsc_rcookie[0]),
			(u_int)ntohl(ipsec.ipsc_rcookie[1]),
			ipsec.ipsc_rckset ? "(Set)" : "(Not set)");
	}
}
