/*
 *   Copyright (c) 1998,1999 Martin Husemann. All rights reserved.
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
 *	$Id: monitor.c,v 1.29 1999/12/13 21:25:25 hm Exp $
 *
 * $FreeBSD$
 *
 *      last edit-date: [Mon Dec 13 21:47:44 1999]
 *
 *---------------------------------------------------------------------------*/

#include "isdnd.h"

#ifndef I4B_EXTERNAL_MONITOR

/*
 * dummy version of routines needed by config file parser
 * (config files should be valid with and without external montioring
 * support compiled into the daemon)
 */

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
#include <sys/socket.h>
#include <sys/un.h>
#ifndef I4B_NOTCPIP_MONITOR
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif


static TAILQ_HEAD(rights_q, monitor_rights) rights = TAILQ_HEAD_INITIALIZER(rights);

static struct monitor_rights * local_rights = NULL;	/* entry for local socket */

/* for each active monitor connection we have one of this: */

struct monitor_connection {
	TAILQ_ENTRY(monitor_connection) connections;
	int sock;			/* socket for this connection */
	int rights;			/* active rights for this connection */
	int events;			/* bitmask of events client is interested in */
	char source[FILENAME_MAX];
};

static TAILQ_HEAD(connections_tq, monitor_connection) connections = TAILQ_HEAD_INITIALIZER(connections);

/* local prototypes */
static int cmp_rights(const struct monitor_rights *pa, const struct monitor_rights *pb);
static int monitor_command(struct monitor_connection *con, int fd, int rights);
static void cmd_dump_rights(int fd, int rights, u_int8_t *cmd, const char * source);
static void cmd_dump_mcons(int fd, int rights, u_int8_t *cmd, const char * source);
static void cmd_reread_cfg(int fd, int rights, u_int8_t *cmd, const char * source);
static void cmd_hangup(int fd, int rights, u_int8_t *cmd, const char * source);
static void monitor_broadcast(int mask, u_int8_t *pkt, size_t bytes);
static int anybody(int mask);
static void hangup_channel(int controller, int channel, const char *source);
static ssize_t sock_read(int fd, void *buf, size_t nbytes);
static ssize_t sock_write(int fd, void *buf, size_t nbytes);

/*
 * Due to the way we structure config files, the rights for an external
 * monitor might be stated in multiple steps. First a call to
 * monitor_start_rights opens an entry. Further (optional) calls to
 * montior_add_rights assemble additional rights for this "current"
 * entry. When closing the sys-file section of the config file, the
 * "current" entry becomes invalid.
 */
static struct monitor_rights * cur_add_entry = NULL;

/*---------------------------------------------------------------------------
 * Initialize the monitor server module. This affects only active
 * connections, the access rights are not modified here!
 *---------------------------------------------------------------------------*/
void
monitor_init(void)
{
	struct monitor_connection * con;
	accepted = 0;
	while ((con = TAILQ_FIRST(&connections)) != NULL)
	{
		TAILQ_REMOVE(&connections, con, connections);
		free(con);
	}
}

/*---------------------------------------------------------------------------
 * Prepare for exit
 *---------------------------------------------------------------------------*/
void
monitor_exit(void)
{
	struct monitor_connection *c;

	/* Close all open connections. */
	while((c = TAILQ_FIRST(&connections)) != NULL) {
		close(c->sock);
		TAILQ_REMOVE(&connections, c, connections);
		free(c);
	}
}

/*---------------------------------------------------------------------------
 * Initialize access rights. No active connections are affected!
 *---------------------------------------------------------------------------*/
void
monitor_clear_rights(void)
{
	struct monitor_rights *r;
	while ((r = TAILQ_FIRST(&rights)) != NULL) {
		TAILQ_REMOVE(&rights, r, list);
		free(r);
	}
	cur_add_entry = NULL;
	local_rights = NULL;
}

/*---------------------------------------------------------------------------
 * Add an entry to the access lists. The clientspec either is
 * the name of the local socket or a host- or networkname or
 * numeric ip/host-bit-len spec.
 *---------------------------------------------------------------------------*/
int
monitor_start_rights(const char *clientspec)
{
	struct monitor_rights r;

	/* initialize the new rights entry */

	memset(&r, 0, sizeof r);

	/* check clientspec */

	if (*clientspec == '/')
	{
		struct sockaddr_un sa;

		/* this is a local socket spec, check if we already have one */

		if (local_rights != NULL)
			return I4BMAR_DUP;

		/* does it fit in a local socket address? */

		if (strlen(clientspec) > sizeof sa.sun_path)
			return I4BMAR_LENGTH;

		r.local = 1;
		strcpy(r.name, clientspec);

#ifndef I4B_NOTCPIP_MONITOR

	}
	else
	{
		/* remote entry, parse host/net and cidr */

		struct monitor_rights * rp;
		char hostname[FILENAME_MAX];
		char *p;

		p = strchr(clientspec, '/');

		if (!p)
		{
			struct hostent *host;
			u_int32_t hn;

			/* must be a host spec */

			r.mask = ~0;
			host = gethostbyname(clientspec);

			if (!host)
				return I4BMAR_NOIP;

			memcpy(&hn, host->h_addr_list[0], sizeof hn);
			r.net = (u_int32_t)ntohl(hn);
		}
		else if(p[1])
		{
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
		}
		else
		{
			return I4BMAR_CIDR;
		}

		/* check for duplicate entry */

		for (rp = TAILQ_FIRST(&rights); rp != NULL; rp = TAILQ_NEXT(rp, list))
		{
			if (rp->mask == r.mask &&
			    rp->net == r.net &&
			    rp->local == r.local)
			{
				return I4BMAR_DUP;
			}
		}
#endif
	}

	r.rights = 0;

	/* entry ok, add it to the collection */

	cur_add_entry = malloc(sizeof(r));
	memcpy(cur_add_entry, &r, sizeof(r));
	TAILQ_INSERT_TAIL(&rights, cur_add_entry, list);

	if(r.local)
		local_rights = cur_add_entry;

	DBGL(DL_RCCF, (log(LL_DBG, "system: monitor = %s", clientspec)));
	
	return I4BMAR_OK;
}

/*---------------------------------------------------------------------------
 * Add rights to the currently constructed entry - if any.
 *---------------------------------------------------------------------------*/
void
monitor_add_rights(int rights_mask)
{
	if(cur_add_entry == NULL)
		return;		/* noone under construction */

	cur_add_entry->rights |= rights_mask;

	DBGL(DL_RCCF, (log(LL_DBG, "system: monitor-access = 0x%x", rights_mask)));
}

/*---------------------------------------------------------------------------
 * All rights have been added now. Sort the to get most specific
 * host/net masks first, so we can travel the list and use the first
 * match for actual rights.
 *---------------------------------------------------------------------------*/
void
monitor_fixup_rights(void)
{
	struct monitor_rights * cur, * test, * next;

	/* no more rights may be added to the current entry */

	cur_add_entry = NULL;
	
	/* sort the rights */
	for (next = NULL, cur = TAILQ_FIRST(&rights); cur != NULL; cur = next)
	{
		next = TAILQ_NEXT(cur, list);
		for (test = TAILQ_FIRST(&rights); test != NULL && test != cur; test = TAILQ_NEXT(test, list))
		{
			if (cmp_rights(cur, test) > 0) {
				/* move cur up the list and insert before test */
				TAILQ_REMOVE(&rights, cur, list);
				if (test == TAILQ_FIRST(&rights))
					TAILQ_INSERT_HEAD(&rights, cur, list);
				else
					TAILQ_INSERT_BEFORE(test, cur, list);
				break;
			}
		}
	}
}

/*---------------------------------------------------------------------------
 * comparator for rights
 *---------------------------------------------------------------------------*/
static int
cmp_rights(const struct monitor_rights *pa, const struct monitor_rights *pb)
{
	u_int32_t mask;

	/* local sorts first */

	if (pa->local)
		return -1;

	/* which is the less specific netmask? */

	mask = pa->mask;

	if ((pb->mask & mask) == 0)
		mask = pb->mask;

	/* are the entries disjunct? */

	if ((pa->net & mask) != (pb->net & mask))
	{
		/* simply compare net part of address */
		return ((pa->net & mask) < (pb->net & mask)) ? -1 : 1;
	}

	/* One entry is part of the others net. We already now "mask" is
	 * the netmask of the less specific (i.e. greater) one */

	return (pa->mask == mask) ? 1 : -1;
}

#ifndef I4B_NOTCPIP_MONITOR
/*---------------------------------------------------------------------------
 * Check if access rights for a remote socket are specified and
 * create this socket. Return -1 otherwise.
 *---------------------------------------------------------------------------*/
int
monitor_create_remote_socket(int portno)
{
	struct sockaddr_in sa;
	int val;
	int remotesockfd;

	remotesockfd = socket(AF_INET, SOCK_STREAM, 0);

	if(remotesockfd == -1)
	{
		log(LL_MER, "could not create remote monitor socket: %s", strerror(errno));
		return(-1);
	}

	val = 1;

	if(setsockopt(remotesockfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof val))
	{
		log(LL_MER, "could not setsockopt: %s", strerror(errno));
		return(-1);
	}

	memset(&sa, 0, sizeof sa);
	sa.sin_len = sizeof sa;
	sa.sin_family = AF_INET;
	sa.sin_port = htons(portno);
	sa.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(remotesockfd, (struct sockaddr *)&sa, sizeof sa) == -1)
	{
		log(LL_MER, "could not bind remote monitor socket to port %d: %s", portno, strerror(errno));
		return(-1);
	}

	if(listen(remotesockfd, 0))
	{
		log(LL_MER, "could not listen on monitor socket: %s", strerror(errno));
		return(-1);
	}

	return(remotesockfd);
}
#endif

/*---------------------------------------------------------------------------
 * Check if access rights for a local socket are specified and
 * create this socket. Return -1 otherwise.
 *---------------------------------------------------------------------------*/
int
monitor_create_local_socket(void)
{
	int s;
	struct sockaddr_un sa;

	/* check for a local entry */

	if (local_rights == NULL)
		return(-1);

	/* create and setup socket */

	s = socket(AF_LOCAL, SOCK_STREAM, 0);

	if (s == -1)
	{
		log(LL_MER, "could not create local monitor socket, errno = %d", errno);
		return(-1);
	}

	unlink(local_rights->name);

	memset(&sa, 0, sizeof sa);
	sa.sun_len = sizeof sa;
	sa.sun_family = AF_LOCAL;
	strcpy(sa.sun_path, local_rights->name);

	if (bind(s, (struct sockaddr *)&sa, SUN_LEN(&sa)))
	{
		log(LL_MER, "could not bind local monitor socket [%s], errno = %d", local_rights->name, errno);
		return(-1);
	}

	chmod(local_rights->name, 0500);

	if (listen(s, 0))
	{
		log(LL_MER, "could not listen on local monitor socket, errno = %d", errno);
		return(-1);
	}

	return(s);
}

/*---------------------------------------------------------------------------
 * Prepare a fd_set for a select call. Add all our local
 * filedescriptors to the set, increment max_fd if appropriate.
 *---------------------------------------------------------------------------*/
void
monitor_prepselect(fd_set *selset, int *max_fd)
{
	struct monitor_connection * con;

	for (con = TAILQ_FIRST(&connections); con != NULL; con = TAILQ_NEXT(con, connections))
	{
		int fd = con->sock;

		if (fd > *max_fd)
			*max_fd = fd;

		FD_SET(fd, selset);
	}
}

/*---------------------------------------------------------------------------
 * Check if the result from a select call indicates something
 * to do for us.
 *---------------------------------------------------------------------------*/
void
monitor_handle_input(fd_set *selset)
{
	struct monitor_connection * con, * next;

	for (next = NULL, con = TAILQ_FIRST(&connections); con != NULL; con = next)
	{
		int fd = con->sock;
		next = TAILQ_NEXT(con, connections);

		if (FD_ISSET(fd, selset))
		{
			/* handle command from this client */

			if (monitor_command(con, fd, con->rights) != 0)
			{
				/* broken or closed connection */

				char source[FILENAME_MAX];

				strcpy(source, con->source);
				TAILQ_REMOVE(&connections, con, connections);
				free(con);
				log(LL_DMN, "monitor closed from %s", source );
			}
		}
	}

	/* all connections gone? */

	if (TAILQ_FIRST(&connections) == NULL)
		accepted = 0;
}

/*---------------------------------------------------------------------------
 * Try new incoming connection on the given socket.
 * Setup client descriptor and send initial data.
 *---------------------------------------------------------------------------*/
void
monitor_handle_connect(int sockfd, int is_local)
{
	struct monitor_connection *con;
	struct monitor_rights *rp;

#ifndef I4B_NOTCPIP_MONITOR
	struct sockaddr_in ia;
	u_int32_t ha = 0;
#endif

	struct sockaddr_un ua;
	u_int8_t idata[I4B_MON_IDATA_SIZE];
	int fd = -1, s, i, r_mask, t_events;
	char source[FILENAME_MAX];

	/* accept the connection */

	if(is_local)
	{
		s = sizeof ua;
		fd = accept(sockfd, (struct sockaddr *)&ua, &s);
		strcpy(source, "local");

#ifndef I4B_NOTCPIP_MONITOR
	}
	else
	{
		struct hostent *hp;
		
		s = sizeof ia;
		fd = accept(sockfd, (struct sockaddr *)&ia, &s);

		hp = gethostbyaddr((char *)&ia.sin_addr, 4, AF_INET);

		if(hp == NULL)
			snprintf(source, sizeof source, "%s (%s)", inet_ntoa(ia.sin_addr), inet_ntoa(ia.sin_addr));
		else
			snprintf(source, sizeof source, "%s (%s)", hp->h_name, inet_ntoa(ia.sin_addr));

		memcpy(&ha, &ia.sin_addr.s_addr, sizeof ha);

		ha = ntohl(ha);
#endif
	}

	/* check the access rights of this connection */

	r_mask = 0;

	for (rp = TAILQ_FIRST(&rights); rp != NULL; rp = TAILQ_NEXT(rp, list))
	{
		if(rp->local)
		{
			if(is_local)
			{
				r_mask = rp->rights;
				break;
			}

#ifndef I4B_NOTCPIP_MONITOR
		}
		else
		{
			if((ha & rp->mask) == rp->net)
			{
				r_mask = rp->rights;
				break;
			}
#endif
		}
	}

	if(r_mask == 0)
	{
		/* no rights - go away */
		log(LL_MER, "monitor access denied from %s", source);
		close(fd);
		return;
	}

	accepted = 1;

	con = malloc(sizeof(struct monitor_connection));
	memset(con, 0, sizeof *con);
	TAILQ_INSERT_TAIL(&connections, con, connections);
	con->sock = fd;
	con->rights = r_mask;
	strcpy(con->source, source);
	
	log(LL_DMN, "monitor opened from %s rights 0x%x", source, r_mask);

	/* send initial data */
	I4B_PREP_CMD(idata, I4B_MON_IDATA_CODE);
	I4B_PUT_2B(idata, I4B_MON_IDATA_VERSMAJOR, MPROT_VERSION);
	I4B_PUT_2B(idata, I4B_MON_IDATA_VERSMINOR, MPROT_REL);
	I4B_PUT_2B(idata, I4B_MON_IDATA_NUMCTRL, ncontroller);
	I4B_PUT_2B(idata, I4B_MON_IDATA_NUMENTR, nentries);	
	I4B_PUT_4B(idata, I4B_MON_IDATA_CLACCESS, r_mask);

	if((sock_write(fd, idata, sizeof idata)) == -1)
	{
		log(LL_MER, "monitor_handle_connect: sock_write 1 error - %s", strerror(errno));
	}
		
	for (i = 0; i < ncontroller; i++)
	{
		u_int8_t ictrl[I4B_MON_ICTRL_SIZE];

		I4B_PREP_CMD(ictrl, I4B_MON_ICTRL_CODE);
		I4B_PUT_STR(ictrl, I4B_MON_ICTRL_NAME, name_of_controller(isdn_ctrl_tab[i].ctrl_type, isdn_ctrl_tab[i].card_type));
		I4B_PUT_2B(ictrl, I4B_MON_ICTRL_BUSID, 0);
		I4B_PUT_4B(ictrl, I4B_MON_ICTRL_FLAGS, 0);
		I4B_PUT_4B(ictrl, I4B_MON_ICTRL_NCHAN, 2);

		if((sock_write(fd, ictrl, sizeof ictrl)) == -1)
		{
			log(LL_MER, "monitor_handle_connect: sock_write 2 error - %s", strerror(errno));
		}
		
	}

	/* send device names from entries */
	
	for(i=0; i < nentries; i++)	/* walk thru all entries */
	{
		u_int8_t ictrl[I4B_MON_IDEV_SIZE];
		cfg_entry_t *p;
		char nbuf[64];		
		p = &cfg_entry_tab[i];		/* get ptr to enry */

		snprintf(nbuf, sizeof(nbuf), "%s%d ", bdrivername(p->usrdevicename), p->usrdeviceunit);

		I4B_PREP_CMD(ictrl, I4B_MON_IDEV_CODE);
/*XXX*/		I4B_PUT_2B(ictrl, I4B_MON_IDEV_STATE, 1);
		I4B_PUT_STR(ictrl, I4B_MON_IDEV_NAME, nbuf);

		if((sock_write(fd, ictrl, sizeof ictrl)) == -1)
		{
			log(LL_MER, "monitor_handle_connect: sock_write 3 error - %s", strerror(errno));
		}
	}

/*XXX*/	t_events = con->events;
/*XXX*/	con->events = -1;

	/* current state of controller(s) */
	
	for(i=0; i < ncontroller; i++)
	{
		monitor_evnt_tei(i, isdn_ctrl_tab[i].tei);
		monitor_evnt_l12stat(i, LAYER_ONE, isdn_ctrl_tab[i].l1stat);
		monitor_evnt_l12stat(i, LAYER_TWO, isdn_ctrl_tab[i].l2stat);
	}

	/* current state of entries */
	
	for(i=0; i < nentries; i++)
        {
		cfg_entry_t *cep = &cfg_entry_tab[i];

		if(cep->state == ST_CONNECTED)
		{
			monitor_evnt_connect(cep);
			monitor_evnt_acct(cep);
			monitor_evnt_charge(cep, cep->charge, 1);
		}
        }

/*XXX*/	con->events = t_events;
	
}

/*---------------------------------------------------------------------------
 * dump all monitor rights
 *---------------------------------------------------------------------------*/
static void
cmd_dump_rights(int fd, int r_mask, u_int8_t *cmd, const char *source)
{
	struct monitor_rights * r;
	int num_rights;
	u_int8_t drini[I4B_MON_DRINI_SIZE];
	u_int8_t dr[I4B_MON_DR_SIZE];

	for (num_rights = 0, r = TAILQ_FIRST(&rights); r != NULL; r = TAILQ_NEXT(r, list))
		num_rights++;

	I4B_PREP_EVNT(drini, I4B_MON_DRINI_CODE);
	I4B_PUT_2B(drini, I4B_MON_DRINI_COUNT, num_rights);

	if((sock_write(fd, drini, sizeof drini)) == -1)
	{
		log(LL_MER, "cmd_dump_rights: sock_write 1 error - %s", strerror(errno));
	}

	for (r = TAILQ_FIRST(&rights); r != NULL; r = TAILQ_NEXT(r, list))
	{
		I4B_PREP_EVNT(dr, I4B_MON_DR_CODE);
		I4B_PUT_4B(dr, I4B_MON_DR_RIGHTS, r->rights);
		I4B_PUT_4B(dr, I4B_MON_DR_NET, r->net);
		I4B_PUT_4B(dr, I4B_MON_DR_MASK, r->mask);
		I4B_PUT_1B(dr, I4B_MON_DR_LOCAL, r->local);
		if((sock_write(fd, dr, sizeof dr)) == -1)
		{
			log(LL_MER, "cmd_dump_rights: sock_write 2 error - %s", strerror(errno));
		}		
	}
}

/*---------------------------------------------------------------------------
 * rescan config file
 *---------------------------------------------------------------------------*/
static void
cmd_reread_cfg(int fd, int rights, u_int8_t *cmd, const char * source)
{
	rereadconfig(42);
}

/*---------------------------------------------------------------------------
 * drop one connection
 *---------------------------------------------------------------------------*/
static void
cmd_hangup(int fd, int rights, u_int8_t *cmd, const char * source)
{
	int channel = I4B_GET_4B(cmd, I4B_MON_HANGUP_CHANNEL);
	int ctrl = I4B_GET_4B(cmd, I4B_MON_HANGUP_CTRL);	

	hangup_channel(ctrl, channel, source);
}

/*---------------------------------------------------------------------------
 * dump all active monitor connections
 *---------------------------------------------------------------------------*/
static void
cmd_dump_mcons(int fd, int rights, u_int8_t *cmd, const char * source)
{
	int num_connections;
	struct monitor_connection *con;
	u_int8_t dcini[I4B_MON_DCINI_SIZE];

	for (num_connections = 0, con = TAILQ_FIRST(&connections); con != NULL; con = TAILQ_NEXT(con, connections))
		num_connections++;

	I4B_PREP_EVNT(dcini, I4B_MON_DCINI_CODE);
	I4B_PUT_2B(dcini, I4B_MON_DCINI_COUNT, num_connections);

	if((sock_write(fd, dcini, sizeof dcini)) == -1)
	{
		log(LL_MER, "cmd_dump_mcons: sock_write 1 error - %s", strerror(errno));
	}		

	for (con = TAILQ_FIRST(&connections); con != NULL; con = TAILQ_NEXT(con, connections))
	{
#ifndef I4B_NOTCPIP_MONITOR
		int namelen;
		struct sockaddr_in name;
#endif
		u_int8_t dc[I4B_MON_DC_SIZE];

		I4B_PREP_EVNT(dc, I4B_MON_DC_CODE);
		I4B_PUT_4B(dc, I4B_MON_DC_RIGHTS, con->rights);

#ifndef I4B_NOTCPIP_MONITOR
		namelen = sizeof name;

		if (getpeername(con->sock, (struct sockaddr*)&name, &namelen) == 0)
			memcpy(dc+I4B_MON_DC_WHO, &name.sin_addr, sizeof name.sin_addr);
#endif
		if((sock_write(fd, dc, sizeof dc)) == -1)
		{
			log(LL_MER, "cmd_dump_mcons: sock_write 2 error - %s", strerror(errno));
		}
	}
}

/*---------------------------------------------------------------------------
 * Handle a command from the given socket. The client
 * has rights as specified in the rights parameter.
 * Return non-zero if connection is closed.
 *---------------------------------------------------------------------------*/
static int
monitor_command(struct monitor_connection * con, int fd, int rights)
{
	char cmd[I4B_MAX_MON_CLIENT_CMD];
	u_int code;

	/* command dispatch table */
	typedef void (*cmd_func_t)(int fd, int rights, u_int8_t *cmd, const char *source);

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

	if (u < I4B_MON_CMD_HDR)
	{
		if (u == 0)
		{
			/* log(LL_MER, "monitor read 0 bytes"); */
			/* socket closed by peer */
			close(fd);
			return 1;
		}
		return 0;	/* not enough data there yet */
	}

	bytes = recv(fd, cmd, I4B_MON_CMD_HDR, MSG_PEEK);

	if (bytes < I4B_MON_CMD_HDR)
	{
		log(LL_MER, "monitor read only %d bytes", bytes);
		return 0;	/* errh? something must be wrong... */
	}

	bytes = I4B_GET_2B(cmd, I4B_MON_CMD_LEN);

	if (bytes >= sizeof cmd)
	{
		close(fd);
		log(LL_MER, "monitor: garbage on connection");
		return 1;
	}

	/* now we know the size, it fits, so lets read it! */

	if(sock_read(fd, cmd, bytes) <= 0)
	{
		log(LL_MER, "monitor: sock_read <= 0");
		close(fd);
		return 1;
	}

	/* decode command */
	code = I4B_GET_2B(cmd, I4B_MON_CMD);

	/* special case: may modify our connection descriptor, is
	 * beyound all rights checks */

	if (code == I4B_MON_CCMD_SETMASK)
	{
/*XXX*/
		/*
		u_int major = I4B_GET_2B(cmd, I4B_MON_ICLIENT_VERMAJOR);
		u_int minor = I4B_GET_2B(cmd, I4B_MON_ICLIENT_VERMINOR);
		*/

		int events = I4B_GET_4B(cmd, I4B_MON_ICLIENT_EVENTS);
		con->events = events & rights;
		return 0;
	}

	if (code < 0 || code >= NUMCMD)
	{
		log(LL_MER, "illegal command from client, code = %d\n",
			code);
		return 0;
	}

	if (cmd_tab[code].call == NULL)
		return 0;

	if ((cmd_tab[code].rights & rights) == cmd_tab[code].rights)
		cmd_tab[code].call(fd, rights, cmd, con->source);

	return 0;
}

/*---------------------------------------------------------------------------
 * Check if somebody would receive an event with this mask.
 * We are lazy and try to avoid assembling unneccesary packets.
 * Return 0 if no one interested, nonzero otherwise.
 *---------------------------------------------------------------------------*/
static int
anybody(int mask)
{
	struct monitor_connection * con;

	for (con = TAILQ_FIRST(&connections); con != NULL; con = TAILQ_NEXT(con, connections))
	{
		if ((con->events & mask) == mask)
			return 1;
	}
	return 0;
}

/*---------------------------------------------------------------------------
 * exec hangup command
 *---------------------------------------------------------------------------*/
static void
hangup_channel(int controller, int channel, const char *source)
{
	cfg_entry_t * cep = NULL;

	if(controller < ncontroller)
	{	
		if(isdn_ctrl_tab[controller].state != CTRL_UP)
			return;
		if(isdn_ctrl_tab[controller].stateb1 != CHAN_IDLE)
		{
			cep = get_cep_by_cc(controller, 0);
			if (cep != NULL && cep->isdnchannelused == channel &&
				cep->isdncontrollerused == controller)
				goto found;
		}
		if(isdn_ctrl_tab[controller].stateb2 != CHAN_IDLE)
		{
			cep = get_cep_by_cc(controller, 1);
			if (cep != NULL && cep->isdnchannelused == channel &&
				cep->isdncontrollerused == controller)
				goto found;
		}
	}
	/* not found */
	return;

found:
	log(LL_CHD, "%05d %s manual disconnect (remote from %s)", cep->cdid, cep->name, source);
	cep->hangup = 1;
	return;
}

/*---------------------------------------------------------------------------
 * Send an event to every connection interested in this kind of
 * event
 *---------------------------------------------------------------------------*/
static void
monitor_broadcast(int mask, u_int8_t *pkt, size_t bytes)
{
	struct monitor_connection *con;

	for (con = TAILQ_FIRST(&connections); con != NULL; con = TAILQ_NEXT(con, connections))
	{
		if ((con->events & mask) == mask)
		{
			int fd = con->sock;

			if((sock_write(fd, pkt, bytes)) == -1)
			{
				log(LL_MER, "monitor_broadcast: sock_write error - %s", strerror(errno));
			}
		}
	}
}

/*---------------------------------------------------------------------------
 * Post a logfile event
 *---------------------------------------------------------------------------*/
void
monitor_evnt_log(int prio, const char * what, const char * msg)
{
	u_int8_t evnt[I4B_MON_LOGEVNT_SIZE];
	time_t now;

	if (!anybody(I4B_CA_EVNT_I4B))
		return;

	time(&now);

	I4B_PREP_EVNT(evnt, I4B_MON_LOGEVNT_CODE);
	I4B_PUT_4B(evnt, I4B_MON_LOGEVNT_TSTAMP, (long)now);
	I4B_PUT_4B(evnt, I4B_MON_LOGEVNT_PRIO, prio);
	I4B_PUT_STR(evnt, I4B_MON_LOGEVNT_WHAT, what);
	I4B_PUT_STR(evnt, I4B_MON_LOGEVNT_MSG, msg);

	monitor_broadcast(I4B_CA_EVNT_I4B, evnt, sizeof evnt);
}

/*---------------------------------------------------------------------------
 * Post a charging event on the connection described
 * by the given config entry.
 *---------------------------------------------------------------------------*/
void
monitor_evnt_charge(cfg_entry_t *cep, int units, int estimate)
{
	int mask;
	time_t now;
	u_int8_t evnt[I4B_MON_CHRG_SIZE];
	
	mask = (cep->direction == DIR_IN) ? I4B_CA_EVNT_CALLIN : I4B_CA_EVNT_CALLOUT;

	if(!anybody(mask))
		return;

	time(&now);

	I4B_PREP_EVNT(evnt, I4B_MON_CHRG_CODE);
	I4B_PUT_4B(evnt, I4B_MON_CHRG_TSTAMP, (long)now);
	I4B_PUT_4B(evnt, I4B_MON_CHRG_CTRL, cep->isdncontrollerused);
	I4B_PUT_4B(evnt, I4B_MON_CHRG_CHANNEL, cep->isdnchannelused);
	I4B_PUT_4B(evnt, I4B_MON_CHRG_UNITS, units);
	I4B_PUT_4B(evnt, I4B_MON_CHRG_ESTIMATED, estimate ? 1 : 0);

	monitor_broadcast(mask, evnt, sizeof evnt);
}

/*---------------------------------------------------------------------------
 * Post a connection event
 *---------------------------------------------------------------------------*/
void
monitor_evnt_connect(cfg_entry_t *cep)
{
	u_int8_t evnt[I4B_MON_CONNECT_SIZE];
	char devname[I4B_MAX_MON_STRING];
	int mask;
	time_t now;
	
	mask = (cep->direction == DIR_IN) ? I4B_CA_EVNT_CALLIN : I4B_CA_EVNT_CALLOUT;

	if (!anybody(mask))
		return;

	time(&now);

	snprintf(devname, sizeof devname, "%s%d", bdrivername(cep->usrdevicename), cep->usrdeviceunit);

	I4B_PREP_EVNT(evnt, I4B_MON_CONNECT_CODE);
	I4B_PUT_4B(evnt, I4B_MON_CONNECT_TSTAMP, (long)now);
	I4B_PUT_4B(evnt, I4B_MON_CONNECT_DIR, cep->direction == DIR_OUT ? 1 : 0);
	I4B_PUT_4B(evnt, I4B_MON_CONNECT_CTRL, cep->isdncontrollerused);
	I4B_PUT_4B(evnt, I4B_MON_CONNECT_CHANNEL, cep->isdnchannelused);	
	I4B_PUT_STR(evnt, I4B_MON_CONNECT_CFGNAME, cep->name);
	I4B_PUT_STR(evnt, I4B_MON_CONNECT_DEVNAME, devname);

	if(cep->direction == DIR_OUT)
	{
		I4B_PUT_STR(evnt, I4B_MON_CONNECT_REMPHONE, cep->remote_phone_dialout);
		I4B_PUT_STR(evnt, I4B_MON_CONNECT_LOCPHONE, cep->local_phone_dialout);
	}
	else
	{
		I4B_PUT_STR(evnt, I4B_MON_CONNECT_REMPHONE, cep->real_phone_incoming);
		I4B_PUT_STR(evnt, I4B_MON_CONNECT_LOCPHONE, cep->local_phone_incoming);
	}
	monitor_broadcast(mask, evnt, sizeof evnt);
}

/*---------------------------------------------------------------------------
 * Post a disconnect event
 *---------------------------------------------------------------------------*/
void
monitor_evnt_disconnect(cfg_entry_t *cep)
{
	u_int8_t evnt[I4B_MON_DISCONNECT_SIZE];
	int mask;
	time_t now;
	
	mask = (cep->direction == DIR_IN) ? I4B_CA_EVNT_CALLIN : I4B_CA_EVNT_CALLOUT;

	if (!anybody(mask))
		return;

	time(&now);

	I4B_PREP_EVNT(evnt, I4B_MON_DISCONNECT_CODE);
	I4B_PUT_4B(evnt, I4B_MON_DISCONNECT_TSTAMP, (long)now);
	I4B_PUT_4B(evnt, I4B_MON_DISCONNECT_CTRL, cep->isdncontrollerused);
	I4B_PUT_4B(evnt, I4B_MON_DISCONNECT_CHANNEL, cep->isdnchannelused);

	monitor_broadcast(mask, evnt, sizeof evnt);
}

/*---------------------------------------------------------------------------
 * Post an up/down event
 *---------------------------------------------------------------------------*/
void
monitor_evnt_updown(cfg_entry_t *cep, int up)
{
	u_int8_t evnt[I4B_MON_UPDOWN_SIZE];
	int mask;
	time_t now;
	
	mask = (cep->direction == DIR_IN) ? I4B_CA_EVNT_CALLIN : I4B_CA_EVNT_CALLOUT;

	if (!anybody(mask))
		return;

	time(&now);

	I4B_PREP_EVNT(evnt, I4B_MON_UPDOWN_CODE);
	I4B_PUT_4B(evnt, I4B_MON_UPDOWN_TSTAMP, (long)now);
	I4B_PUT_4B(evnt, I4B_MON_UPDOWN_CTRL, cep->isdncontrollerused);
	I4B_PUT_4B(evnt, I4B_MON_UPDOWN_CHANNEL, cep->isdnchannelused);	
	I4B_PUT_4B(evnt, I4B_MON_UPDOWN_ISUP, up);

	monitor_broadcast(mask, evnt, sizeof evnt);
}

/*---------------------------------------------------------------------------
 * Post a Layer1/2 status change event
 *---------------------------------------------------------------------------*/
void
monitor_evnt_l12stat(int controller, int layer, int state)
{
	u_int8_t evnt[I4B_MON_L12STAT_SIZE];
	time_t now;

	if(!anybody(I4B_CA_EVNT_I4B))
		return;

	time(&now);
	
	I4B_PREP_EVNT(evnt, I4B_MON_L12STAT_CODE);
	I4B_PUT_4B(evnt, I4B_MON_L12STAT_TSTAMP, (long)now);
	I4B_PUT_4B(evnt, I4B_MON_L12STAT_CTRL, controller);
	I4B_PUT_4B(evnt, I4B_MON_L12STAT_LAYER, layer);
	I4B_PUT_4B(evnt, I4B_MON_L12STAT_STATE, state);

	monitor_broadcast(I4B_CA_EVNT_I4B, evnt, sizeof evnt);
}

/*---------------------------------------------------------------------------
 * Post a TEI change event
 *---------------------------------------------------------------------------*/
void
monitor_evnt_tei(int controller, int tei)
{
	u_int8_t evnt[I4B_MON_TEI_SIZE];
	time_t now;

	if(!anybody(I4B_CA_EVNT_I4B))
		return;

	time(&now);
	
	I4B_PREP_EVNT(evnt, I4B_MON_TEI_CODE);
	I4B_PUT_4B(evnt, I4B_MON_TEI_TSTAMP, (long)now);
	I4B_PUT_4B(evnt, I4B_MON_TEI_CTRL, controller);
	I4B_PUT_4B(evnt, I4B_MON_TEI_TEI, tei);

	monitor_broadcast(I4B_CA_EVNT_I4B, evnt, sizeof evnt);
}

/*---------------------------------------------------------------------------
 * Post an accounting event
 *---------------------------------------------------------------------------*/
void
monitor_evnt_acct(cfg_entry_t *cep)
{
	u_int8_t evnt[I4B_MON_ACCT_SIZE];
	time_t now;

	if(!anybody(I4B_CA_EVNT_I4B))
		return;

	time(&now);
	
	I4B_PREP_EVNT(evnt, I4B_MON_ACCT_CODE);
	I4B_PUT_4B(evnt, I4B_MON_ACCT_TSTAMP, (long)now);

	I4B_PUT_4B(evnt, I4B_MON_ACCT_CTRL,   cep->isdncontrollerused);
	I4B_PUT_4B(evnt, I4B_MON_ACCT_CHAN,   cep->isdnchannelused);
	I4B_PUT_4B(evnt, I4B_MON_ACCT_OBYTES, cep->outbytes);
	I4B_PUT_4B(evnt, I4B_MON_ACCT_OBPS,   cep->outbps);
	I4B_PUT_4B(evnt, I4B_MON_ACCT_IBYTES, cep->inbytes);
	I4B_PUT_4B(evnt, I4B_MON_ACCT_IBPS,   cep->inbps);

	monitor_broadcast(I4B_CA_EVNT_I4B, evnt, sizeof evnt);
}

/*---------------------------------------------------------------------------
 * read from a socket
 *---------------------------------------------------------------------------*/
static ssize_t
sock_read(int fd, void *buf, size_t nbytes)
{
	size_t nleft;
	ssize_t nread;
	unsigned char *ptr;

	ptr = buf;
	nleft = nbytes;

	while(nleft > 0)
	{
		if((nread = read(fd, ptr, nleft)) < 0)
		{
			if(errno == EINTR)
			{
				nread = 0;
			}
			else
			{
				return(-1);
			}
		}
		else if(nread == 0)
		{
			break; /* EOF */
		}

		nleft -= nread;
		ptr += nread;
	}
	return(nbytes - nleft);
}

/*---------------------------------------------------------------------------
 * write to a socket
 *---------------------------------------------------------------------------*/
static ssize_t
sock_write(int fd, void *buf, size_t nbytes)
{
	size_t nleft;
	ssize_t nwritten;
	unsigned char *ptr;

	ptr = buf;
	nleft = nbytes;

	while(nleft > 0)
	{
		if((nwritten = write(fd, ptr, nleft)) <= 0)
		{
			if(errno == EINTR)
			{
				nwritten = 0;
			}
			else
			{
				return(-1);
			}
		}

		nleft -= nwritten;
		ptr += nwritten;
	}
	return(nbytes);
}

struct monitor_rights * monitor_next_rights(const struct monitor_rights *r)
{
	if (r == NULL)
		return TAILQ_FIRST(&rights);
	else
		return TAILQ_NEXT(r, list);
}

#endif	/* I4B_EXTERNAL_MONITOR */
