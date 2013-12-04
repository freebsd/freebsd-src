#include <config.h>

#include "main.h"
#include "kod_management.h"
#include "networking.h"
#include "utilities.h"
#include "log.h"

int ai_fam_pref = AF_UNSPEC;

struct key *keys = NULL;

void set_li_vn_mode (struct pkt *spkt, char leap, char version, char mode); 
int sntp_main (int argc, char **argv);
int on_wire (struct addrinfo *host, struct addrinfo *bcastaddr);
int set_time (double offset);

#define NORMALIZE_TIMEVAL(tv)				\
do {							\
	while ((tv).tv_usec < 0) {			\
		(tv).tv_usec += 1000000;		\
		(tv).tv_sec--;				\
	}						\
	while ((tv).tv_usec > 999999) {			\
		(tv).tv_usec -= 1000000;		\
		(tv).tv_sec++;				\
	}						\
} while (0)


/*
 * The actual main function.
 */
int  
sntp_main (
	int argc, 
	char **argv
	) 
{
	register int c;
	struct kod_entry *reason = NULL;
	int optct;
	/* boolean, u_int quiets gcc4 signed overflow warning */
	u_int sync_data_suc;
	struct addrinfo **bcastaddr = NULL;
	struct addrinfo **resh = NULL;
	struct addrinfo *ai;
	int resc;
	int kodc;
	int ow_ret;
	int bcast = 0;
	char *hostname;

	optct = optionProcess(&sntpOptions, argc, argv);
	argc -= optct;
	argv += optct; 

	/* Initialize logging system */
	init_logging();
	if (HAVE_OPT(LOGFILE))
		open_logfile(OPT_ARG(LOGFILE));

	msyslog(LOG_NOTICE, "Started sntp");

	/* IPv6 available? */
	if (isc_net_probeipv6() != ISC_R_SUCCESS) {
		ai_fam_pref = AF_INET;
#ifdef DEBUG
		printf("No ipv6 support available, forcing ipv4\n");
#endif
	} else {
		/* Check for options -4 and -6 */
		if (HAVE_OPT(IPV4))
			ai_fam_pref = AF_INET;
		else if (HAVE_OPT(IPV6))
			ai_fam_pref = AF_INET6;
	}

	/* Parse config file if declared TODO */

	/* 
	 * If there's a specified KOD file init KOD system.  If not use
	 * default file.  For embedded systems with no writable
	 * filesystem, -K /dev/null can be used to disable KoD storage.
	 */
	if (HAVE_OPT(KOD))
		kod_init_kod_db(OPT_ARG(KOD));
	else
		kod_init_kod_db("/var/db/ntp-kod");

	if (HAVE_OPT(KEYFILE))
		auth_init(OPT_ARG(KEYFILE), &keys);

#ifdef EXERCISE_KOD_DB
	add_entry("192.168.169.170", "DENY");
	add_entry("192.168.169.171", "DENY");
	add_entry("192.168.169.172", "DENY");
	add_entry("192.168.169.173", "DENY");
	add_entry("192.168.169.174", "DENY");
	delete_entry("192.168.169.174", "DENY");
	delete_entry("192.168.169.172", "DENY");
	delete_entry("192.168.169.170", "DENY");
	if ((kodc = search_entry("192.168.169.173", &reason)) == 0)
		printf("entry for 192.168.169.173 not found but should have been!\n");
	else
		free(reason);
#endif

	/* Considering employing a variable that prevents functions of doing anything until 
	 * everything is initialized properly 
	 */
	resc = resolve_hosts((const char **)argv, argc, &resh, ai_fam_pref);
	if (resc < 1) {
		printf("Unable to resolve hostname(s)\n");
		return -1;
	}
	bcast = ENABLED_OPT(BROADCAST);
	if (bcast) {
		const char * myargv[2];

		myargv[0] = OPT_ARG(BROADCAST);
		myargv[1] = NULL;
		bcast = resolve_hosts(myargv, 1, &bcastaddr, ai_fam_pref);
	}

	/* Select a certain ntp server according to simple criteria? For now
	 * let's just pay attention to previous KoDs.
	 */
	sync_data_suc = FALSE;
	for (c = 0; c < resc && !sync_data_suc; c++) {
		ai = resh[c];
		do {
			hostname = addrinfo_to_str(ai);
			if ((kodc = search_entry(hostname, &reason)) == 0) {
				if (is_reachable(ai)) {
					ow_ret = on_wire(ai, bcast ? bcastaddr[0] : NULL);
					if (0 == ow_ret)
						sync_data_suc = TRUE;
				}
			} else {
				printf("%d prior KoD%s for %s, skipping.\n", 
					kodc, (kodc > 1) ? "s" : "", hostname);
				free(reason);
			}
			free(hostname);
			ai = ai->ai_next;
		} while (NULL != ai);
		freeaddrinfo(resh[c]);
	}
	free(resh);

	if (!sync_data_suc)
		return 1;
	return 0;
}

static union {
	struct pkt pkt;
	char   buf[1500];
} rbuf;

#define r_pkt  rbuf.pkt

int
generate_pkt (
	struct pkt *x_pkt,
	const struct timeval *tv_xmt,
	int key_id,
	struct key *pkt_key
	)
{
	l_fp xmt;
	int pkt_len = LEN_PKT_NOMAC;
	memset(x_pkt, 0, sizeof(struct pkt));
	TVTOTS(tv_xmt, &xmt);
	HTONL_FP(&xmt, &(x_pkt->xmt));
	x_pkt->stratum = STRATUM_TO_PKT(STRATUM_UNSPEC);
	x_pkt->ppoll = 8;
	/* FIXME! Modus broadcast + adr. check -> bdr. pkt */
	set_li_vn_mode(x_pkt, LEAP_NOTINSYNC, 4, 3);
	if (pkt_key != NULL) {
		int mac_size = 20; /* max room for MAC */
		x_pkt->exten[0] = htonl(key_id);
		mac_size = make_mac((char *)x_pkt, pkt_len, mac_size, pkt_key, (char *)&x_pkt->exten[1]);
		if (mac_size)
			pkt_len += mac_size + 4;
	}
	return pkt_len;
}

int
handle_pkt (
	int rpktl,
	struct pkt *rpkt,
	struct addrinfo *host
	)
{
	struct timeval tv_dst;
	int sw_case, digits;
	char *hostname = NULL, *ref, *ts_str = NULL;
	double offset, precision, root_dispersion;
	char addr_buf[INET6_ADDRSTRLEN];
	char *p_SNTP_PRETEND_TIME;
	time_t pretend_time;

	if(rpktl > 0)
		sw_case = 1;
	else
		sw_case = rpktl;

	switch(sw_case) {
	case SERVER_UNUSEABLE:
		return -1;
		break;

	case PACKET_UNUSEABLE:
		break;
 
	case SERVER_AUTH_FAIL:
		break;

	case KOD_DEMOBILIZE:
		/* Received a DENY or RESTR KOD packet */
		hostname = addrinfo_to_str(host);
		ref = (char *)&rpkt->refid;
		add_entry(hostname, ref);

		if (ENABLED_OPT(NORMALVERBOSE))
			printf("sntp handle_pkt: Received KOD packet with code: %c%c%c%c from %s, demobilizing all connections\n",
				   ref[0], ref[1], ref[2], ref[3],
				   hostname);

		msyslog(LOG_WARNING, "Received a KOD packet with code %c%c%c%c from %s, demobilizing all connections",
			ref[0], ref[1], ref[2], ref[3], hostname);
		break;

	case KOD_RATE:
		/* Hmm... probably we should sleep a bit here */
		break;

	case 1:
		if (ENABLED_OPT(NORMALVERBOSE)) {
			getnameinfo(host->ai_addr, host->ai_addrlen, addr_buf, 
				sizeof(addr_buf), NULL, 0, NI_NUMERICHOST);
			printf("sntp handle_pkt: Received %i bytes from %s\n", rpktl, addr_buf);
		}

		GETTIMEOFDAY(&tv_dst, (struct timezone *)NULL);

		p_SNTP_PRETEND_TIME = getenv("SNTP_PRETEND_TIME");
		if (p_SNTP_PRETEND_TIME) {
#if SIZEOF_TIME_T == 4
			sscanf(p_SNTP_PRETEND_TIME, "%ld", &pretend_time);
#elif SIZEOF_TIME_T == 8
			sscanf(p_SNTP_PRETEND_TIME, "%lld", &pretend_time);
#else
# include "GRONK: unexpected value for SIZEOF_TIME_T"
#endif
			tv_dst.tv_sec = pretend_time;
		}

		offset_calculation(rpkt, rpktl, &tv_dst, &offset,
				   &precision, &root_dispersion);

		for (digits = 0; (precision *= 10.) < 1.; ++digits)
			/* empty */ ;
		if (digits > 6)
			digits = 6;

		ts_str = tv_to_str(&tv_dst);
		printf("%s ", ts_str);
		if (offset > 0)
			printf("+");
		printf("%.*f", digits, offset);
		if (root_dispersion > 0.)
			printf(" +/- %f secs", root_dispersion);
		printf("\n");
		free(ts_str);

		if (p_SNTP_PRETEND_TIME)
			return 0;

		if (ENABLED_OPT(SETTOD) || ENABLED_OPT(ADJTIME))
			return set_time(offset); 

		return 0;
	}

	return 1;
}

void
offset_calculation (
	struct pkt *rpkt,
	int rpktl,
	struct timeval *tv_dst,
	double *offset,
	double *precision,
	double *root_dispersion
	)
{
	l_fp p_rec, p_xmt, p_ref, p_org, tmp, dst;
	u_fp p_rdly, p_rdsp;
	double t21, t34, delta;

	/* Convert timestamps from network to host byte order */
	p_rdly = NTOHS_FP(rpkt->rootdelay);
	p_rdsp = NTOHS_FP(rpkt->rootdisp);
	NTOHL_FP(&rpkt->reftime, &p_ref);
	NTOHL_FP(&rpkt->org, &p_org);
	NTOHL_FP(&rpkt->rec, &p_rec);
	NTOHL_FP(&rpkt->xmt, &p_xmt);

	*precision = LOGTOD(rpkt->precision);
#ifdef DEBUG
	printf("sntp precision: %f\n", *precision);
#endif /* DEBUG */

	*root_dispersion = FPTOD(p_rdsp);

#ifdef DEBUG
	printf("sntp rootdelay: %f\n", FPTOD(p_rdly));
	printf("sntp rootdisp: %f\n", *root_dispersion);

	pkt_output(rpkt, rpktl, stdout);

	printf("sntp offset_calculation: rpkt->reftime:\n");
	l_fp_output(&(rpkt->reftime), stdout);
	printf("sntp offset_calculation: rpkt->org:\n");
	l_fp_output(&(rpkt->org), stdout);
	printf("sntp offset_calculation: rpkt->rec:\n");
	l_fp_output(&(rpkt->rec), stdout);
	printf("sntp offset_calculation: rpkt->rec:\n");
	l_fp_output_bin(&(rpkt->rec), stdout);
	printf("sntp offset_calculation: rpkt->rec:\n");
	l_fp_output_dec(&(rpkt->rec), stdout);
	printf("sntp offset_calculation: rpkt->xmt:\n");
	l_fp_output(&(rpkt->xmt), stdout);
#endif

	/* Compute offset etc. */
	tmp = p_rec;
	L_SUB(&tmp, &p_org);
	LFPTOD(&tmp, t21);
	TVTOTS(tv_dst, &dst);
	dst.l_ui += JAN_1970;
	tmp = p_xmt;
	L_SUB(&tmp, &dst);
	LFPTOD(&tmp, t34);
	*offset = (t21 + t34) / 2.;
	delta = t21 - t34;

	if (ENABLED_OPT(NORMALVERBOSE))
		printf("sntp offset_calculation:\tt21: %.6f\t\t t34: %.6f\n\t\tdelta: %.6f\t offset: %.6f\n", 
			   t21, t34, delta, *offset);
}

/* The heart of (S)NTP, exchange NTP packets and compute values to correct the local clock */
int
on_wire (
	struct addrinfo *host,
	struct addrinfo *bcast
	)
{
	char addr_buf[INET6_ADDRSTRLEN];
	register int try;
	SOCKET sock;
	struct key *pkt_key = NULL;
	int key_id = 0;
	struct timeval tv_xmt;
	struct pkt x_pkt;
	int error, rpktl, handle_pkt_res;


	if (ENABLED_OPT(AUTHENTICATION)) {
		key_id = (int) OPT_ARG(AUTHENTICATION);
		get_key(key_id, &pkt_key);
	}
	for (try=0; try<5; try++) {
		memset(&r_pkt, 0, sizeof rbuf);
		
		error = GETTIMEOFDAY(&tv_xmt, (struct timezone *)NULL);
		tv_xmt.tv_sec += JAN_1970;

#ifdef DEBUG
		printf("sntp on_wire: Current time sec: %i msec: %i\n", (unsigned int) tv_xmt.tv_sec, 
				(unsigned int) tv_xmt.tv_usec);
#endif

		if (bcast) {
			create_socket(&sock, (sockaddr_u *)bcast->ai_addr);
			rpktl = recv_bcst_pkt(sock, &r_pkt, sizeof rbuf, (sockaddr_u *)bcast->ai_addr);
			closesocket(sock);
		} else {
			int pkt_len = generate_pkt(&x_pkt, &tv_xmt, key_id, pkt_key);

			create_socket(&sock, (sockaddr_u *)host->ai_addr);
			sendpkt(sock, (sockaddr_u *)host->ai_addr, &x_pkt, pkt_len);
			rpktl = recvpkt(sock, &r_pkt, sizeof rbuf, &x_pkt);
			closesocket(sock);
		}

		handle_pkt_res = handle_pkt(rpktl, &r_pkt, host);
		if (handle_pkt_res < 1)
			return handle_pkt_res;
	}

	getnameinfo(host->ai_addr, host->ai_addrlen, addr_buf, sizeof(addr_buf), NULL, 0, NI_NUMERICHOST);
	msyslog(LOG_DEBUG, "Received no useable packet from %s!", addr_buf);

	return -1;
}

/* Compute the 8 bits for li_vn_mode */
void
set_li_vn_mode (
	struct pkt *spkt,
	char leap,
	char version,
	char mode
	) 
{
	if (leap > 3) {
		msyslog(LOG_DEBUG, "set_li_vn_mode: leap > 3 using max. 3");
		leap = 3;
	}

	if (mode > 7) {
		msyslog(LOG_DEBUG, "set_li_vn_mode: mode > 7, using client mode 3");
		mode = 3;
	}

	spkt->li_vn_mode  = leap << 6;
	spkt->li_vn_mode |= version << 3;
	spkt->li_vn_mode |= mode;
}

/* set_time corrects the local clock by offset with either settimeofday() or by default 
 * with adjtime()/adjusttimeofday().
 */
int
set_time(
	double offset
	)
{
	struct timeval tp;

	if (ENABLED_OPT(SETTOD)) {
		GETTIMEOFDAY(&tp, NULL);

		tp.tv_sec += (long)offset;
		tp.tv_usec += 1e6 * (offset - (long)offset);
		NORMALIZE_TIMEVAL(tp);

		if (SETTIMEOFDAY(&tp, NULL) < 0) {
			msyslog(LOG_ERR, "Time not set: settimeofday(): %m");
			return -1;
		}
		return 0;
	}

	tp.tv_sec = (long)offset;
	tp.tv_usec = 1e6 * (offset - (long)offset);
	NORMALIZE_TIMEVAL(tp);

	if (ADJTIMEOFDAY(&tp, NULL) < 0) {
		msyslog(LOG_ERR, "Time not set: adjtime(): %m");
		return -1;
	}
	return 0;
}
