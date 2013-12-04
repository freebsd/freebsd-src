#include <config.h>
#include "networking.h"

char adr_buf[INET6_ADDRSTRLEN];


/* resolve_hosts consumes an array of hostnames/addresses and its length, stores a pointer
 * to the array with the resolved hosts in res and returns the size of the array res.
 * pref_family enforces IPv4 or IPv6 depending on commandline options and system 
 * capability. If pref_family is NULL or PF_UNSPEC any compatible family will be accepted.
 * Check here: Probably getaddrinfo() can do without ISC's IPv6 availability check? 
 */
int 
resolve_hosts (
		const char **hosts, 
		int hostc, 
		struct addrinfo ***res,
		int pref_family
		) 
{
	register unsigned int a;
	unsigned int resc;
	struct addrinfo **tres;

	if (hostc < 1 || NULL == res)
		return 0;
	
	tres = emalloc(sizeof(struct addrinfo *) * hostc);
	for (a = 0, resc = 0; a < hostc; a++) {
		struct addrinfo hints;
		int error;

		tres[resc] = NULL;
#ifdef DEBUG
		printf("sntp resolve_hosts: Starting host resolution for %s...\n", hosts[a]); 
#endif
		memset(&hints, 0, sizeof(hints));
		if (AF_UNSPEC == pref_family)
			hints.ai_family = PF_UNSPEC;
		else 
			hints.ai_family = pref_family;
		hints.ai_socktype = SOCK_DGRAM;
		error = getaddrinfo(hosts[a], "123", &hints, &tres[resc]);
		if (error) {
			msyslog(LOG_DEBUG, "Error looking up %s%s: %s",
				(AF_UNSPEC == hints.ai_family)
				    ? ""
				    : (AF_INET == hints.ai_family)
					  ? "(A) "
					  : "(AAAA) ",
				hosts[a], gai_strerror(error));
		} else {
#ifdef DEBUG
			for (dres = tres[resc]; dres; dres = dres->ai_next) {
				getnameinfo(dres->ai_addr, dres->ai_addrlen, adr_buf, sizeof(adr_buf), NULL, 0, NI_NUMERICHOST);
				STDLINE
				printf("Resolv No.: %i Result of getaddrinfo for %s:\n", resc, hosts[a]);
				printf("socktype: %i ", dres->ai_socktype); 
				printf("protocol: %i ", dres->ai_protocol);
				printf("Prefered socktype: %i IP: %s\n", dres->ai_socktype, adr_buf);
				STDLINE
			}
#endif
			resc++;
		}
	}

	if (resc)
		*res = realloc(tres, sizeof(struct addrinfo *) * resc);
	else {
		free(tres);
		*res = NULL;
	}
	return resc;
}

/* Creates a socket and returns. */
void 
create_socket (
		SOCKET *rsock,
		sockaddr_u *dest
		)
{
	*rsock = socket(AF(dest), SOCK_DGRAM, 0);

	if (-1 == *rsock && ENABLED_OPT(NORMALVERBOSE))
		printf("Failed to create UDP socket with family %d\n", AF(dest));
}

/* Send a packet */
void
sendpkt (
	SOCKET rsock,
	sockaddr_u *dest,
	struct pkt *pkt,
	int len
	)
{
	int cc;

#ifdef DEBUG
	printf("sntp sendpkt: Packet data:\n");
	pkt_output(pkt, len, stdout);
#endif

	if (ENABLED_OPT(NORMALVERBOSE)) {
		getnameinfo(&dest->sa, SOCKLEN(dest), adr_buf, sizeof(adr_buf), NULL, 0, NI_NUMERICHOST);
		printf("sntp sendpkt: Sending packet to %s... ", adr_buf);
	}

	cc = sendto(rsock, (void *)pkt, len, 0, &dest->sa, SOCKLEN(dest));
	if (cc == SOCKET_ERROR) {
#ifdef DEBUG
		printf("\n sntp sendpkt: Socket error: %i. Couldn't send packet!\n", cc);
#endif
		if (errno != EWOULDBLOCK && errno != ENOBUFS) {
			/* oh well */
		}
	} else if (ENABLED_OPT(NORMALVERBOSE)) {
		printf("Packet sent.\n");
	}
}

/* Receive raw data */
int
recvdata(
	SOCKET rsock,
	sockaddr_u *sender,
	char *rdata,
	int rdata_length
	)
{
	GETSOCKNAME_SOCKLEN_TYPE slen;
	int recvc;

#ifdef DEBUG
	printf("sntp recvdata: Trying to receive data from...\n");
#endif
	slen = sizeof(*sender);
	recvc = recvfrom(rsock, rdata, rdata_length, 0, 
			 &sender->sa, &slen);
#ifdef DEBUG
	if (recvc > 0) {
		printf("Received %d bytes from %s:\n", recvc, stoa(sender));
		pkt_output((struct pkt *) rdata, recvc, stdout);
	} else {
		saved_errno = errno;
		printf("recvfrom error %d (%s)\n", errno, strerror(errno));
		errno = saved_errno;
	}
#endif
	return recvc;
}

/* Receive data from broadcast. Couldn't finish that. Need to do some digging
 * here, especially for protocol independence and IPv6 multicast */
int 
recv_bcst_data (
	SOCKET rsock,
	char *rdata,
	int rdata_len,
	sockaddr_u *sas,
	sockaddr_u *ras
	)
{
	char *buf;
	int btrue = 1;
	int recv_bytes = 0;
	int rdy_socks;
	GETSOCKNAME_SOCKLEN_TYPE ss_len;
	struct timeval timeout_tv;
	fd_set bcst_fd;
#ifdef MCAST
	struct ip_mreq mdevadr;
	TYPEOF_IP_MULTICAST_LOOP mtrue = 1;
#endif
#ifdef INCLUDE_IPV6_MULTICAST_SUPPORT
	struct ipv6_mreq mdevadr6;
#endif

	setsockopt(rsock, SOL_SOCKET, SO_REUSEADDR, &btrue, sizeof(btrue));
	if (IS_IPV4(sas)) {
		if (bind(rsock, &sas->sa, SOCKLEN(sas)) < 0) {
			if (ENABLED_OPT(NORMALVERBOSE))
				printf("sntp recv_bcst_data: Couldn't bind() address %s:%d.\n",
				       stoa(sas), SRCPORT(sas));
		}

#ifdef MCAST
		if (setsockopt(rsock, IPPROTO_IP, IP_MULTICAST_LOOP, &mtrue, sizeof(mtrue)) < 0) {
			/* some error message regarding setting up multicast loop */
			return BROADCAST_FAILED;
		}
		mdevadr.imr_multiaddr.s_addr = NSRCADR(sas); 
		mdevadr.imr_interface.s_addr = htonl(INADDR_ANY);
		if (mdevadr.imr_multiaddr.s_addr == -1) {
			if (ENABLED_OPT(NORMALVERBOSE)) {
				printf("sntp recv_bcst_data: %s:%d is not a broad-/multicast address, aborting...\n",
				       stoa(sas), SRCPORT(sas));
			}
			return BROADCAST_FAILED;
		}
		if (setsockopt(rsock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mdevadr, sizeof(mdevadr)) < 0) {
			if (ENABLED_OPT(NORMALVERBOSE)) {
				buf = ss_to_str(sas);
				printf("sntp recv_bcst_data: Couldn't add IP membership for %s\n", buf);
				free(buf);
			}
		}
#endif	/* MCAST */
	}
#ifdef ISC_PLATFORM_HAVEIPV6
	else if (IS_IPV6(sas)) {
		if (bind(rsock, &sas->sa, SOCKLEN(sas)) < 0) {
			if (ENABLED_OPT(NORMALVERBOSE))
				printf("sntp recv_bcst_data: Couldn't bind() address.\n");
		}
#ifdef INCLUDE_IPV6_MULTICAST_SUPPORT
		if (setsockopt(rsock, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &btrue, sizeof (btrue)) < 0) {
			/* some error message regarding setting up multicast loop */
			return BROADCAST_FAILED;
		}
		memset(&mdevadr6, 0, sizeof(mdevadr6));
		mdevadr6.ipv6mr_multiaddr = SOCK_ADDR6(sas);
		if (!IN6_IS_ADDR_MULTICAST(&mdevadr6.ipv6mr_multiaddr)) {
			if (ENABLED_OPT(NORMALVERBOSE)) {
				buf = ss_to_str(sas); 
				printf("sntp recv_bcst_data: %s is not a broad-/multicast address, aborting...\n", buf);
				free(buf);
			}
			return BROADCAST_FAILED;
		}
		if (setsockopt(rsock, IPPROTO_IPV6, IPV6_JOIN_GROUP,
			       &mdevadr6, sizeof(mdevadr6)) < 0) {
			if (ENABLED_OPT(NORMALVERBOSE)) {
				buf = ss_to_str(sas); 
				printf("sntp recv_bcst_data: Couldn't join group for %s\n", buf);
				free(buf);
			}
		}
#endif	/* INCLUDE_IPV6_MULTICAST_SUPPORT */
	}
#endif	/* ISC_PLATFORM_HAVEIPV6 */
	FD_ZERO(&bcst_fd);
	FD_SET(rsock, &bcst_fd);
	if (ENABLED_OPT(TIMEOUT)) 
		timeout_tv.tv_sec = (int) OPT_ARG(TIMEOUT);
	else 
		timeout_tv.tv_sec = 68; /* ntpd broadcasts every 64s */
	timeout_tv.tv_usec = 0;
	rdy_socks = select(rsock + 1, &bcst_fd, 0, 0, &timeout_tv);
	switch (rdy_socks) {
	case -1: 
		if (ENABLED_OPT(NORMALVERBOSE)) 
			perror("sntp recv_bcst_data: select()");
		return BROADCAST_FAILED;
		break;
	case 0:
		if (ENABLED_OPT(NORMALVERBOSE))
			printf("sntp recv_bcst_data: select() reached timeout (%u sec), aborting.\n", 
			       (unsigned)timeout_tv.tv_sec);
		return BROADCAST_FAILED;
		break;
	default:
		ss_len = sizeof(*ras);
		recv_bytes = recvfrom(rsock, rdata, rdata_len, 0, &ras->sa, &ss_len);
		break;
	}
	if (recv_bytes == -1) {
		if (ENABLED_OPT(NORMALVERBOSE))
			perror("sntp recv_bcst_data: recvfrom:");
		recv_bytes = BROADCAST_FAILED;
	}
#ifdef MCAST
	if (IS_IPV4(sas)) 
		setsockopt(rsock, IPPROTO_IP, IP_DROP_MEMBERSHIP, &btrue, sizeof(btrue));
#endif
#ifdef INCLUDE_IPV6_MULTICAST_SUPPORT
	if (IS_IPV6(sas))
		setsockopt(rsock, IPPROTO_IPV6, IPV6_LEAVE_GROUP, &btrue, sizeof(btrue));
#endif
	return recv_bytes;
}

int
process_pkt (
	struct pkt *rpkt,
	sockaddr_u *sas,
	int pkt_len,
	int mode,
	struct pkt *spkt,
	char * func_name
	)
{
	unsigned int key_id = 0;
	struct key *pkt_key = NULL;
	int is_authentic = 0;
	unsigned int exten_words, exten_words_used = 0;
	int mac_size;
	/*
	 * Parse the extension field if present. We figure out whether
	 * an extension field is present by measuring the MAC size. If
	 * the number of words following the packet header is 0, no MAC
	 * is present and the packet is not authenticated. If 1, the
	 * packet is a crypto-NAK; if 3, the packet is authenticated
	 * with DES; if 5, the packet is authenticated with MD5; if 6,
	 * the packet is authenticated with SHA. If 2 or 4, the packet
	 * is a runt and discarded forthwith. If greater than 6, an
	 * extension field is present, so we subtract the length of the
	 * field and go around again.
	 */
	if (pkt_len < LEN_PKT_NOMAC || (pkt_len & 3) != 0) {
unusable:
		if (ENABLED_OPT(NORMALVERBOSE))
			printf("sntp %s: Funny packet length: %i. Discarding package.\n", func_name, pkt_len);
		return PACKET_UNUSEABLE;
	}
	/* skip past the extensions, if any */
	exten_words = ((unsigned)pkt_len - LEN_PKT_NOMAC) >> 2;
	while (exten_words > 6) {
		unsigned int exten_len;
		exten_len = ntohl(rpkt->exten[exten_words_used]) & 0xffff;
		exten_len = (exten_len + 7) >> 2; /* convert to words, add 1 */
		if (exten_len > exten_words || exten_len < 5)
			goto unusable;
		exten_words -= exten_len;
		exten_words_used += exten_len;
	}

	switch (exten_words) {
	case 1:
		key_id = ntohl(rpkt->exten[exten_words_used]);
		printf("Crypto NAK = 0x%08x\n", key_id);
		break;
	case 5:
	case 6:
		/* Look for the key used by the server in the specified keyfile
		 * and if existent, fetch it or else leave the pointer untouched */
		key_id = ntohl(rpkt->exten[exten_words_used]);
		get_key(key_id, &pkt_key);
		if (!pkt_key) {
			printf("unrecognized key ID = 0x%08x\n", key_id);
			break;
		}
		/* Seems like we've got a key with matching keyid */
		/* Generate a md5sum of the packet with the key from our keyfile
		 * and compare those md5sums */
		mac_size = exten_words << 2;
		if (!auth_md5((char *)rpkt, pkt_len - mac_size, mac_size - 4, pkt_key)) {
			break;
		}
		/* Yay! Things worked out! */
		if (ENABLED_OPT(NORMALVERBOSE)) {
			char *hostname = ss_to_str(sas);
			printf("sntp %s: packet received from %s successfully authenticated using key id %i.\n",
				func_name, hostname, key_id);
			free(hostname);
		}
		is_authentic = 1;
		break;
	case 0:
		break;
	default:
		goto unusable;
		break;
	}
	if (!is_authentic) {
		if (ENABLED_OPT(AUTHENTICATION)) {
			/* We want a authenticated packet */
			if (ENABLED_OPT(NORMALVERBOSE)) {
				char *hostname = ss_to_str(sas);
				printf("sntp %s: packet received from %s is not authentic. Will discard it.\n",
					func_name, hostname);
				free(hostname);
			}
			return SERVER_AUTH_FAIL;
		}
		/* We don't know if the user wanted authentication so let's 
		 * use it anyways */
		if (ENABLED_OPT(NORMALVERBOSE)) {
			char *hostname = ss_to_str(sas);
			printf("sntp %s: packet received from %s is not authentic. Authentication not enforced.\n",
				func_name, hostname);
			free(hostname);
		}
	}
	/* Check for server's ntp version */
	if (PKT_VERSION(rpkt->li_vn_mode) < NTP_OLDVERSION ||
		PKT_VERSION(rpkt->li_vn_mode) > NTP_VERSION) {
		if (ENABLED_OPT(NORMALVERBOSE))
			printf("sntp %s: Packet shows wrong version (%i)\n",
				func_name, PKT_VERSION(rpkt->li_vn_mode));
		return SERVER_UNUSEABLE;
	} 
	/* We want a server to sync with */
	if (PKT_MODE(rpkt->li_vn_mode) != mode &&
	    PKT_MODE(rpkt->li_vn_mode) != MODE_PASSIVE) {
		if (ENABLED_OPT(NORMALVERBOSE))
			printf("sntp %s: mode %d stratum %i\n", func_name, 
			       PKT_MODE(rpkt->li_vn_mode), rpkt->stratum);
		return SERVER_UNUSEABLE;
	}
	/* Stratum is unspecified (0) check what's going on */
	if (STRATUM_PKT_UNSPEC == rpkt->stratum) {
		char *ref_char;
		if (ENABLED_OPT(NORMALVERBOSE))
			printf("sntp %s: Stratum unspecified, going to check for KOD (stratum: %i)\n", 
				func_name, rpkt->stratum);
		ref_char = (char *) &rpkt->refid;
		if (ENABLED_OPT(NORMALVERBOSE))
			printf("sntp %s: Packet refid: %c%c%c%c\n", func_name,
			       ref_char[0], ref_char[1], ref_char[2], ref_char[3]);
		/* If it's a KOD packet we'll just use the KOD information */
		if (ref_char[0] != 'X') {
			if (strncmp(ref_char, "DENY", 4) == 0)
				return KOD_DEMOBILIZE;
			if (strncmp(ref_char, "RSTR", 4) == 0)
				return KOD_DEMOBILIZE;
			if (strncmp(ref_char, "RATE", 4) == 0)
				return KOD_RATE;
			/* There are other interesting kiss codes which might be interesting for authentication */
		}
	}
	/* If the server is not synced it's not really useable for us */
	if (LEAP_NOTINSYNC == PKT_LEAP(rpkt->li_vn_mode)) {
		if (ENABLED_OPT(NORMALVERBOSE)) 
			printf("sntp %s: Server not in sync, skipping this server\n", func_name);
		return SERVER_UNUSEABLE;
	}

	/*
	 * Decode the org timestamp and make sure we're getting a response
	 * to our last request, but only if we're not in broadcast mode.
	 */
#ifdef DEBUG
	printf("rpkt->org:\n");
	l_fp_output(&rpkt->org, stdout);
	printf("spkt->xmt:\n");
	l_fp_output(&spkt->xmt, stdout);
#endif
	if (mode != MODE_BROADCAST && !L_ISEQU(&rpkt->org, &spkt->xmt)) {
		if (ENABLED_OPT(NORMALVERBOSE))
			printf("sntp process_pkt: pkt.org and peer.xmt differ\n");
		return PACKET_UNUSEABLE;
	}

	return pkt_len;
}

int 
recv_bcst_pkt (
	SOCKET rsock,
	struct pkt *rpkt,
	unsigned int rsize,
	sockaddr_u *sas
	)
{
	sockaddr_u sender;
	int pkt_len = recv_bcst_data(rsock, (char *)rpkt, rsize, sas, &sender);
	if (pkt_len < 0) {
		return BROADCAST_FAILED;
	}
	pkt_len = process_pkt(rpkt, sas, pkt_len, MODE_BROADCAST, NULL, "recv_bcst_pkt");
	return pkt_len;
}

/* Fetch data, check if it's data for us and whether it's useable or not. If not, return
 * a failure code so we can delete this server from our list and continue with another one.
 */
int
recvpkt (
	SOCKET rsock,
	struct pkt *rpkt,    /* received packet (response) */
	unsigned int rsize,  /* size of rpkt buffer */
	struct pkt *spkt     /* sent     packet (request) */
	)
{
	int rdy_socks;
	int pkt_len;
	sockaddr_u sender;
	struct timeval timeout_tv;
	fd_set recv_fd;

	FD_ZERO(&recv_fd);
	FD_SET(rsock, &recv_fd);
	if (ENABLED_OPT(TIMEOUT)) 
		timeout_tv.tv_sec = (int) OPT_ARG(TIMEOUT);
	else 
		timeout_tv.tv_sec = 68; /* ntpd broadcasts every 64s */
	timeout_tv.tv_usec = 0;
	rdy_socks = select(rsock + 1, &recv_fd, 0, 0, &timeout_tv);
	switch (rdy_socks) {
	case -1: 
		if (ENABLED_OPT(NORMALVERBOSE)) 
			perror("sntp recvpkt: select()");
		return PACKET_UNUSEABLE;
		break;
	case 0:
		if (ENABLED_OPT(NORMALVERBOSE))
			printf("sntp recvpkt: select() reached timeout (%u sec), aborting.\n", 
			       (unsigned)timeout_tv.tv_sec);
		return PACKET_UNUSEABLE;
		break;
	default:
		break;
	}
	pkt_len = recvdata(rsock, &sender, (char *)rpkt, rsize);
	if (pkt_len > 0)
		pkt_len = process_pkt(rpkt, &sender, pkt_len, MODE_SERVER, spkt, "recvpkt");

	return pkt_len;
}

/*
 * is_reachable - check to see if we have a route to given destination
 */
int
is_reachable (
	struct addrinfo *dst
	)
{
	SOCKET sockfd = socket(dst->ai_family, SOCK_DGRAM, 0);

	if (-1 == sockfd) {
#ifdef DEBUG
		printf("is_reachable: Couldn't create socket\n");
#endif
		return 0;
	}
	if (connect(sockfd, dst->ai_addr, SOCKLEN((sockaddr_u *)dst->ai_addr))) {
		closesocket(sockfd);
		return 0;
	}
	closesocket(sockfd);
	return 1;
}
