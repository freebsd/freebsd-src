/*
 *   Copyright (c) 1998 Martin Husemann. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. Neither the name of the author nor the names of any co-contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *   4. Altered versions must be plainly marked as such, and must not be
 *      misrepresented as being the original software and/or documentation.
 *   
 *   THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 *   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *   SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *	i4b daemon - network monitor server module
 *	------------------------------------------
 *
 * $FreeBSD$
 *
 *      last edit-date: [Sun May 30 10:33:05 1999]
 *
 *	-mh	created
 *
 *---------------------------------------------------------------------------*/

#include "isdnd.h"

#ifndef I4B_EXTERNAL_MONITOR
/* dummy version of routines needed by config file parser
 * (config files should be valid with and without external montioring
 * support compiled into the daemon) */
void monitor_clear_rights()
{ }
int monitor_start_rights(const char *clientspec)
{ return I4BMAR_OK; }
void monitor_add_rights(int rights_mask)
{ }
void monitor_fixup_rights()
{ }
#else

#include "monitor.h"
#include "vararray.h"
#include <sys/socket.h>
#include <sys/un.h>
#ifndef I4B_NOTCPIP_MONITOR
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

VARA_DECL(struct monitor_rights) rights = VARA_INITIALIZER;
#define	INITIAL_RIGHTS_ALLOC	10	/* most users will have one or two entries */

static int local_rights = -1;	/* index of entry for local socket, -1 if none */

/* for each active monitor connection we have one of this: */
struct monitor_connection {
	int sock;			/* socket for this connection */
	int rights;			/* active rights for this connection */
	int events;			/* bitmask of events client is interested in */
};
static VARA_DECL(struct monitor_connection) connections = VARA_INITIALIZER;
#define	INITIAL_CONNECTIONS_ALLOC	30	/* high guess */

/* derive channel number from config pointer */
#define CHNO(cfgp) (((cfgp)->isdncontrollerused*2) + (cfgp)->isdnchannelused)

/* local prototypes */
static int cmp_rights(const void *a, const void *b);
static int monitor_command(int con_index, int fd, int rights);
static void cmd_dump_rights(int fd, int rights, BYTE *cmd);
static void cmd_dump_mcons(int fd, int rights, BYTE *cmd);
static void cmd_reread_cfg(int fd, int rights, BYTE *cmd);
static void cmd_hangup(int fd, int rights, BYTE *cmd);
static void monitor_broadcast(int mask, const BYTE *pkt, size_t bytes);
static int anybody(int mask);

/*
 * Due to the way we structure config files, the rights for an external
 * monitor might be stated in multiple steps. First a call to
 * monitor_start_rights opens an entry. Further (optional) calls to
 * montior_add_rights assemble additional rights for this "current"
 * entry. When closing the sys-file section of the config file, the
 * "current" entry becomes invalid.
 */
static int cur_add_entry = -1;

/*
 * Initialize the monitor server module. This affects only active
 * connections, the access rights are not modified here!
 */
void monitor_init()
{
	accepted = 0;
	VARA_EMPTY(connections);
}

/*
 * Prepare for exit
 */
void monitor_exit()
{
	int i;

	/* Close all open connections. */
	VARA_FOREACH(connections, i)
		close(VARA_AT(connections, i).sock);

	/* Remove their descriptions */
	VARA_EMPTY(connections);
}

/*
 * Initialize access rights. No active connections are affected!
 */
void monitor_clear_rights()
{
	VARA_EMPTY(rights);
	cur_add_entry = -1;
}

/*
 * Add an entry to the access lists. The clientspec either is
 * the name of the local socket or a host- or networkname or
 * numeric ip/host-bit-len spec.
 */
int monitor_start_rights(const char *clientspec)
{
	int i;
	struct monitor_rights r;

	/* initialize the new rights entry */
	memset(&r, 0, sizeof r);

	/* check clientspec */
	if (*clientspec == '/') {
		struct sockaddr_un sa;

		/* this is a local socket spec, check if we already have one */
		if (VARA_VALID(rights, local_rights))
			return I4BMAR_DUP;
		/* does it fit in a local socket address? */
		if (strlen(clientspec) > sizeof sa.sun_path)
			return I4BMAR_LENGTH;
		r.local = 1;
		strcpy(r.name, clientspec);
#ifndef I4B_NOTCPIP_MONITOR
	} else {
		/* remote entry, parse host/net and cidr */
		char hostname[FILENAME_MAX];
		char *p;
		p = strchr(clientspec, '/');
		if (!p) {
			struct hostent *host;
			u_int32_t hn;
			/* must be a host spec */
			r.mask = ~0;
			host = gethostbyname(clientspec);
			if (!host)
				return I4BMAR_NOIP;
			memcpy(&hn, host->h_addr_list[0], sizeof hn);
			r.net = (u_int32_t)ntohl(hn);
		} else if (p[1]) {
			/* must be net/cidr spec */
			int l;
			struct netent *net;
			u_int32_t s = ~0U;
			int num = strtol(p+1, NULL, 10);
			if (num < 0 || num > 32)
				return I4BMAR_CIDR;
			s >>= num;
			s ^= ~0U;
			l = p - clientspec;
			if (l >= sizeof hostname)
				return I4BMAR_LENGTH;
			strncpy(hostname, clientspec, l);
			hostname[l] = '\0';
			net = getnetbyname(hostname);
			if (net == NULL)
				r.net = (u_int32_t)inet_network(hostname);
			else
				r.net = (u_int32_t)net->n_net;
			r.mask = s;
			r.net &= s;
		} else
			return I4BMAR_CIDR;

		/* check for duplicate entry */
		VARA_FOREACH(rights, i)
			if (VARA_AT(rights, i).mask == r.mask &&
			    VARA_AT(rights, i).net == r.net &&
			    VARA_AT(rights, i).local == r.local)
				return I4BMAR_DUP;
#endif
	}
	r.rights = 0;

	/* entry ok, add it to the collection */
	cur_add_entry = i = VARA_NUM(rights);
	VARA_ADD_AT(rights, i, struct monitor_rights, INITIAL_RIGHTS_ALLOC);
	memcpy(&VARA_AT(rights, i), &r, sizeof r);
	if (r.local)
		local_rights = i;

	DBGL(DL_RCCF, (log(LL_DBG, "system: monitor = %s", clientspec)));
	
	return I4BMAR_OK;
}

/*
 * Add rights to the currently constructed entry - if any.
 */
void monitor_add_rights(int rights_mask)
{
	if (cur_add_entry < 0) return;	/* noone under construction */

	VARA_AT(rights, cur_add_entry).rights |= rights_mask;

	DBGL(DL_RCCF, (log(LL_DBG, "system: monitor-access = 0x%x", rights_mask)));
}

/*
 * All rights have been added now. Sort the to get most specific
 * host/net masks first, so we can travel the list and use the first
 * match for actual rights.
 */
void monitor_fixup_rights()
{
	int i;

	/* no more rights may be added to the current entry */
	cur_add_entry = -1;
	
	/* sort the rights array */
	qsort(VARA_PTR(rights), VARA_NUM(rights), sizeof(struct monitor_rights), cmp_rights);

	/* now the local entry may have moved, update its index */
	if (VARA_VALID(rights, local_rights)) {
		local_rights = -1;
		VARA_FOREACH(rights, i) {
			if (VARA_AT(rights, i).local) {
				local_rights = i;
				break;
			}
		}
	}	
}

/* comparator for rights */
static int cmp_rights(const void *a, const void *b)
{
	u_int32_t mask;
	struct monitor_rights const * pa = (struct monitor_rights const*)a;
	struct monitor_rights const * pb = (struct monitor_rights const*)b;

	/* local sorts first */
	if (pa->local)
		return -1;

	/* which is the less specific netmask? */
	mask = pa->mask;
	if ((pb->mask & mask) == 0)
		mask = pb->mask;
	/* are the entries disjunct? */
	if ((pa->net & mask) != (pb->net & mask)) {
		/* simply compare net part of address */
		return ((pa->net & mask) < (pb->net & mask)) ? -1 : 1;
	}
	/* One entry is part of the others net. We already now "mask" is
	 * the netmask of the less specific (i.e. greater) one */
	return (pa->mask == mask) ? 1 : -1;
}

#ifndef I4B_NOTCPIP_MONITOR
/*
 * Check if access rights for a remote socket are specified and
 * create this socket. Return -1 otherwise.
 */
int monitor_create_remote_socket(int portno)
{
	struct sockaddr_in sa;
	int val;
	int remotesockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (remotesockfd == -1) {
		log(LL_ERR, "could not create remote monitor socket, errno = %d", errno);
		exit(1);
	}
	val = 1;
	if (setsockopt(remotesockfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof val)) {
		log(LL_ERR, "could not setsockopt, errno = %d", errno);
		exit(1);
	}
	memset(&sa, 0, sizeof sa);
	sa.sin_len = sizeof sa;
	sa.sin_family = AF_INET;
	sa.sin_port = htons(portno);
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(remotesockfd, (struct sockaddr *)&sa, sizeof sa) == -1) {
		log(LL_ERR, "could not bind remote monitor socket to port %d, errno = %d", portno, errno);
		exit(1);
	}
	if (listen(remotesockfd, 0)) {
		log(LL_ERR, "could not listen on monitor socket, errno = %d", errno);
		exit(1);
	}

	return remotesockfd;
}
#endif

/*
 * Check if access rights for a local socket are specified and
 * create this socket. Return -1 otherwise.
 */
int monitor_create_local_socket()
{
	int s;
	struct sockaddr_un sa;

	/* check for a local entry */
	if (!VARA_VALID(rights, local_rights))
		return -1;

	/* create and setup socket */
	s = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (s == -1) {
		log(LL_ERR, "could not create local monitor socket, errno = %d", errno);
		exit(1);
	}
	unlink(VARA_AT(rights, local_rights).name);
	memset(&sa, 0, sizeof sa);
	sa.sun_len = sizeof sa;
	sa.sun_family = AF_LOCAL;
	strcpy(sa.sun_path, VARA_AT(rights, local_rights).name);
	if (bind(s, (struct sockaddr *)&sa, SUN_LEN(&sa))) {
		log(LL_ERR, "could not bind local monitor socket [%s], errno = %d", VARA_AT(rights, local_rights).name, errno);
		exit(1);
	}
	chmod(VARA_AT(rights, local_rights).name, 0500);
	if (listen(s, 0)) {
		log(LL_ERR, "could not listen on local monitor socket, errno = %d", errno);
		exit(1);
	}

	return s;
}

/*
 * Prepare a fd_set for a select call. Add all our local
 * filedescriptors to the set, increment max_fd if appropriate.
 */
void monitor_prepselect(fd_set *selset, int *max_fd)
{
	int i;

	VARA_FOREACH(connections, i) {
		int fd = VARA_AT(connections, i).sock;
		if (fd > *max_fd)
			*max_fd = fd;
		FD_SET(fd, selset);
	}
}

/*
 * Check if the result from a select call indicates something
 * to do for us.
 */
void monitor_handle_input(fd_set *selset)
{
	int i;

	VARA_FOREACH(connections, i) {
		int fd = VARA_AT(connections, i).sock;
		if (FD_ISSET(fd, selset)) {
			/* handle command from this client */
			if (monitor_command(i, fd, VARA_AT(connections, i).rights) != 0) {
				/* broken or closed connection */
				log(LL_DBG, "monitor connection #%d closed", i);
				VARA_REMOVEAT(connections, i);
				i--;
			}
		}
	}

	/* all connections gone? */
	if (VARA_NUM(connections) == 0)
		accepted = 0;
}

/*
 * Try new incoming connection on the given socket.
 * Setup client descriptor and send initial data.
 */
void monitor_handle_connect(int sockfd, int is_local)
{
	struct monitor_connection *con;
#ifndef I4B_NOTCPIP_MONITOR
	struct sockaddr_in ia;
	u_int32_t ha = 0;
#endif
	struct sockaddr_un ua;
	BYTE idata[I4B_MON_IDATA_SIZE];
	int fd = -1, s, i, r_mask;
	char source[FILENAME_MAX];

	/* accept the connection */
	if (is_local) {
		s = sizeof ua;
		fd = accept(sockfd, (struct sockaddr *)&ua, &s);
		strcpy(source, "local connection");
#ifndef I4B_NOTCPIP_MONITOR
	} else {
		s = sizeof ia;
		fd = accept(sockfd, (struct sockaddr *)&ia, &s);
		snprintf(source, sizeof source, "tcp/ip connection from %s\n",
			inet_ntoa(ia.sin_addr));
		memcpy(&ha, &ia.sin_addr.s_addr, sizeof ha);
		ha = ntohl(ha);
#endif
	}

	/* check the access rights of this connection */
	r_mask = 0;
	VARA_FOREACH(rights, i) {
		struct monitor_rights *r = &VARA_AT(rights, i);
		if (r->local) {
			if (is_local) {
				r_mask = r->rights;
				break;
			}
#ifndef I4B_NOTCPIP_MONITOR
		} else {
			if ((ha & r->mask) == r->net) {
				r_mask = r->rights;
				break;
			}
#endif
		}
	}

	if (r_mask == 0) {
		/* no rights - go away */
		log(LL_DBG, "monitor access denied: %s", source);
		close(fd);
		return;
	}

	accepted = 1;
	i = VARA_NUM(connections);
	VARA_ADD_AT(connections, i, struct monitor_connection, INITIAL_CONNECTIONS_ALLOC);
	con = &VARA_AT(connections, i);
	memset(con, 0, sizeof *con);
	con->sock = fd;
	con->rights = r_mask;
	log(LL_DBG, "monitor access granted, rights = %x, #%d, %s",
		r_mask, i, source);

	/* send initial data */
	I4B_PREP_CMD(idata, I4B_MON_IDATA_CODE);
	I4B_PUT_2B(idata, I4B_MON_IDATA_VERSMAJOR, MPROT_VERSION);
	I4B_PUT_2B(idata, I4B_MON_IDATA_VERSMINOR, MPROT_REL);
	I4B_PUT_2B(idata, I4B_MON_IDATA_NUMCTRL, ncontroller);
	I4B_PUT_4B(idata, I4B_MON_IDATA_CLACCESS, r_mask);
	write(fd, idata, sizeof idata);

	for (i = 0; i < ncontroller; i++) {
		BYTE ictrl[I4B_MON_ICTRL_SIZE];
		I4B_PREP_CMD(ictrl, I4B_MON_ICTRL_CODE);
		I4B_PUT_STR(ictrl, I4B_MON_ICTRL_NAME, name_of_controller(isdn_ctrl_tab[i].ctrl_type, isdn_ctrl_tab[i].card_type));
		I4B_PUT_2B(ictrl, I4B_MON_ICTRL_BUSID, 0);
		I4B_PUT_4B(ictrl, I4B_MON_ICTRL_FLAGS, 0);
		I4B_PUT_4B(ictrl, I4B_MON_ICTRL_NCHAN, 2);
		write(fd, ictrl, sizeof ictrl);
	}
}

/* dump all monitor rights */
static void cmd_dump_rights(int fd, int r_mask, BYTE *cmd)
{
	int i;
	BYTE drini[I4B_MON_DRINI_SIZE];
	BYTE dr[I4B_MON_DR_SIZE];

	I4B_PREP_EVNT(drini, I4B_MON_DRINI_CODE);
	I4B_PUT_2B(drini, I4B_MON_DRINI_COUNT, VARA_NUM(rights));
	write(fd, drini, sizeof drini);

	VARA_FOREACH(rights, i) {
		I4B_PREP_EVNT(dr, I4B_MON_DR_CODE);
		I4B_PUT_4B(dr, I4B_MON_DR_RIGHTS, VARA_AT(rights, i).rights);
		I4B_PUT_4B(dr, I4B_MON_DR_NET, VARA_AT(rights, i).net);
		I4B_PUT_4B(dr, I4B_MON_DR_MASK, VARA_AT(rights, i).mask);
		I4B_PUT_1B(dr, I4B_MON_DR_LOCAL, VARA_AT(rights, i).local);
		write(fd, dr, sizeof dr);
	}
}

/* rescan config file */
static void cmd_reread_cfg(int fd, int rights, BYTE *cmd)
{
	rereadconfig(42);
}

/* drop one connection */
static void cmd_hangup(int fd, int rights, BYTE *cmd)
{
	int channel = I4B_GET_4B(cmd, I4B_MON_HANGUP_CHANNEL);
	hangup_channel(channel);
}

/* dump all active monitor connections */
static void cmd_dump_mcons(int fd, int rights, BYTE *cmd)
{
	int i;
	BYTE dcini[I4B_MON_DCINI_SIZE];

	I4B_PREP_EVNT(dcini, I4B_MON_DCINI_CODE);
	I4B_PUT_2B(dcini, I4B_MON_DCINI_COUNT, VARA_NUM(connections));
	write(fd, dcini, sizeof dcini);

	VARA_FOREACH(connections, i) {
#ifndef I4B_NOTCPIP_MONITOR
		int namelen;
		struct sockaddr_in name;
#endif
		BYTE dc[I4B_MON_DC_SIZE];

		I4B_PREP_EVNT(dc, I4B_MON_DC_CODE);
		I4B_PUT_4B(dc, I4B_MON_DC_RIGHTS, VARA_AT(connections, i).rights);
#ifndef I4B_NOTCPIP_MONITOR
		namelen = sizeof name;
		if (getpeername(VARA_AT(connections, i).sock, (struct sockaddr*)&name, &namelen) == 0)
			memcpy(dc+I4B_MON_DC_WHO, &name.sin_addr, sizeof name.sin_addr);
#endif
		write(fd, dc, sizeof dc);
	}
}

/*
 * Handle a command from the given socket. The client
 * has rights as specified in the rights parameter.
 * Return non-zero if connection is closed.
 */
static int monitor_command(int con_index, int fd, int rights)
{
	char cmd[I4B_MAX_MON_CLIENT_CMD];
	u_int code;
	/* command dispatch table */
	typedef void (*cmd_func_t)(int fd, int rights, BYTE *cmd);
	static struct {
		cmd_func_t call;	/* function to execute */
		u_int rights;		/* necessary rights */
	} cmd_tab[] =
	{
	/* 0 */	{ NULL, 0 },
	/* 1 */	{ cmd_dump_rights, I4B_CA_COMMAND_FULL },
	/* 2 */	{ cmd_dump_mcons, I4B_CA_COMMAND_FULL },
	/* 3 */ { cmd_reread_cfg, I4B_CA_COMMAND_FULL },
	/* 4 */ { cmd_hangup, I4B_CA_COMMAND_FULL },
	};
#define	NUMCMD	(sizeof cmd_tab / sizeof cmd_tab[0])

	u_long u;
	int bytes;

	/* Network transfer may deliver two or more packets concatenated.
	 * Peek at the header and read only one event at a time... */
	ioctl(fd, FIONREAD, &u);
	if (u < I4B_MON_CMD_HDR) {
		if (u == 0) {
			log(LL_ERR, "monitor #%d, read 0 bytes", con_index);
			/* socket closed by peer */
			close(fd);
			return 1;
		}
		return 0;	/* not enough data there yet */
	}

	bytes = recv(fd, cmd, I4B_MON_CMD_HDR, MSG_PEEK);

	if (bytes < I4B_MON_CMD_HDR)
	{
		log(LL_ERR, "monitor #%d, read only %d bytes", con_index, bytes);
		return 0;	/* errh? something must be wrong... */
	}

	bytes = I4B_GET_2B(cmd, I4B_MON_CMD_LEN);

	if (bytes >= sizeof cmd)
	{
		close(fd);
		log(LL_ERR, "monitor #%d, garbage on connection", con_index);
		return 1;
	}

	/* now we know the size, it fits, so lets read it! */
	if (read(fd, cmd, bytes) <= 0)
	{
		log(LL_ERR, "monitor #%d, read <= 0", con_index);
		close(fd);
		return 1;
	}

	/* decode command */
	code = I4B_GET_2B(cmd, I4B_MON_CMD);

	/* special case: may modify our connection descriptor, is
	 * beyound all rights checks */
	if (code == I4B_MON_CCMD_SETMASK) {
		/*
		u_int major = I4B_GET_2B(cmd, I4B_MON_ICLIENT_VERMAJOR);
		u_int minor = I4B_GET_2B(cmd, I4B_MON_ICLIENT_VERMINOR);
		*/
		int events = I4B_GET_4B(cmd, I4B_MON_ICLIENT_EVENTS);
		VARA_AT(connections, con_index).events = events & rights;
		return 0;
	}

	if (code < 0 || code >= NUMCMD) {
		log(LL_ERR, "illegal command from client #%d: code = %d\n",
			con_index, code);
		return 0;
	}
	if (cmd_tab[code].call == NULL)
		return 0;
	if ((cmd_tab[code].rights & rights) == cmd_tab[code].rights)
		cmd_tab[code].call(fd, rights, cmd);

	return 0;
}

/*
 * Check if somebody would receive an event with this mask.
 * We are lazy and try to avoid assembling unneccesary packets.
 * Return 0 if no one interested, nonzero otherwise.
 */
static int anybody(int mask)
{
	int i;

	VARA_FOREACH(connections, i)
		if ((VARA_AT(connections, i).events & mask) == mask)
			return 1;

	return 0;
}

/*
 * Send an event to every connection interested in this kind of
 * event
 */
static void monitor_broadcast(int mask, const BYTE *pkt, size_t bytes)
{
	int i;

	VARA_FOREACH(connections, i) {
		if ((VARA_AT(connections, i).events & mask) == mask) {
			int fd = VARA_AT(connections,  i).sock;
			write(fd, pkt, bytes);
		}
	}
}

/*
 * Post a logfile event
 */
void monitor_evnt_log(int prio, const char * what, const char * msg)
{
	BYTE evnt[I4B_MON_LOGEVNT_SIZE];
	time_t now;

	if (!anybody(I4B_CA_EVNT_I4B)) return;

	time(&now);
	I4B_PREP_EVNT(evnt, I4B_MON_LOGEVNT_CODE);
	I4B_PUT_4B(evnt, I4B_MON_LOGEVNT_TSTAMP, (long)now);
	I4B_PUT_4B(evnt, I4B_MON_LOGEVNT_PRIO, prio);
	I4B_PUT_STR(evnt, I4B_MON_LOGEVNT_WHAT, what);
	I4B_PUT_STR(evnt, I4B_MON_LOGEVNT_MSG, msg);

	monitor_broadcast(I4B_CA_EVNT_I4B, evnt, sizeof evnt);
}

/*
 * Post a charging event on the connection described
 * by the given config entry.
 */
void monitor_evnt_charge(cfg_entry_t *cep, int units, int estimate)
{
	int chno = CHNO(cep);
	int mask = (cep->direction == DIR_IN) ? I4B_CA_EVNT_CALLIN : I4B_CA_EVNT_CALLOUT;
	time_t now;
	BYTE evnt[I4B_MON_CHRG_SIZE];

	if (!anybody(mask)) return;

	time(&now);
	I4B_PREP_EVNT(evnt, I4B_MON_CHRG_CODE);
	I4B_PUT_4B(evnt, I4B_MON_CHRG_TSTAMP, (long)now);
	I4B_PUT_4B(evnt, I4B_MON_CHRG_CHANNEL, chno);
	I4B_PUT_4B(evnt, I4B_MON_CHRG_UNITS, units);
	I4B_PUT_4B(evnt, I4B_MON_CHRG_ESTIMATED, estimate ? 1 : 0);

	monitor_broadcast(mask, evnt, sizeof evnt);
}

/*
 * Post a connection event
 */
void monitor_evnt_connect(cfg_entry_t *cep)
{
	BYTE evnt[I4B_MON_CONNECT_SIZE];
	char devname[I4B_MAX_MON_STRING];
	int chno = CHNO(cep);
	int mask = (cep->direction == DIR_IN) ? I4B_CA_EVNT_CALLIN : I4B_CA_EVNT_CALLOUT;
	time_t now;

	if (!anybody(mask)) return;

	time(&now);
	snprintf(devname, sizeof devname, "%s%d", bdrivername(cep->usrdevicename), cep->usrdeviceunit);
	I4B_PREP_EVNT(evnt, I4B_MON_CONNECT_CODE);
	I4B_PUT_4B(evnt, I4B_MON_CONNECT_TSTAMP, (long)now);
	I4B_PUT_4B(evnt, I4B_MON_CONNECT_DIR, cep->direction == DIR_OUT ? 1 : 0);
	I4B_PUT_4B(evnt, I4B_MON_CONNECT_CHANNEL, chno);
	I4B_PUT_STR(evnt, I4B_MON_CONNECT_CFGNAME, cep->name);
	I4B_PUT_STR(evnt, I4B_MON_CONNECT_DEVNAME, devname);
	I4B_PUT_STR(evnt, I4B_MON_CONNECT_REMPHONE, cep->real_phone_incoming);
	I4B_PUT_STR(evnt, I4B_MON_CONNECT_LOCPHONE, cep->remote_phone_dialout);

	monitor_broadcast(mask, evnt, sizeof evnt);
}

/*
 * Post a disconnect event
 */
void monitor_evnt_disconnect(cfg_entry_t *cep)
{
	BYTE evnt[I4B_MON_DISCONNECT_SIZE];
	int chno = CHNO(cep);
	int mask = (cep->direction == DIR_IN) ? I4B_CA_EVNT_CALLIN : I4B_CA_EVNT_CALLOUT;
	time_t now;

	if (!anybody(mask)) return;

	time(&now);
	I4B_PREP_EVNT(evnt, I4B_MON_DISCONNECT_CODE);
	I4B_PUT_4B(evnt, I4B_MON_DISCONNECT_TSTAMP, (long)now);
	I4B_PUT_4B(evnt, I4B_MON_DISCONNECT_CHANNEL, chno);

	monitor_broadcast(mask, evnt, sizeof evnt);
}

/*
 * Post an up/down event
 */
void monitor_evnt_updown(cfg_entry_t *cep, int up)
{
	BYTE evnt[I4B_MON_UPDOWN_SIZE];
	int chno = CHNO(cep);
	int mask = (cep->direction == DIR_IN) ? I4B_CA_EVNT_CALLIN : I4B_CA_EVNT_CALLOUT;
	time_t now;

	if (!anybody(mask)) return;

	time(&now);
	I4B_PREP_EVNT(evnt, I4B_MON_UPDOWN_CODE);
	I4B_PUT_4B(evnt, I4B_MON_UPDOWN_TSTAMP, (long)now);
	I4B_PUT_4B(evnt, I4B_MON_UPDOWN_CHANNEL, chno);
	I4B_PUT_4B(evnt, I4B_MON_UPDOWN_ISUP, up);

	monitor_broadcast(mask, evnt, sizeof evnt);
}

void hangup_channel(int channel)
{
	int i;
	cfg_entry_t * cep = NULL;
	
	for (i = 0; i < ncontroller; i++)
	{	
		if(isdn_ctrl_tab[i].state != CTRL_UP)
			continue;
		if(isdn_ctrl_tab[i].stateb1 != CHAN_IDLE)
		{
			cep = get_cep_by_cc(i, 0);
			if (cep != NULL && CHNO(cep) == channel)
				goto found;
		}
		if(isdn_ctrl_tab[i].stateb2 != CHAN_IDLE)
		{
			cep = get_cep_by_cc(i, 1);
			if (cep != NULL && CHNO(cep) == channel)
				goto found;
		}
	}
	/* not found */
	return;

found:
	cep->hangup = 1;
	return;
}

#endif	/* I4B_EXTERNAL_MONITOR */
