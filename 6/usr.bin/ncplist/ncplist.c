/*
 * Copyright (c) 1999, Boris Popov
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <netncp/ncp_lib.h>

extern char *__progname;

static struct ncp_conn_stat conndesc;

static void help(void);
static void show_connlist(void);
static void show_serverlist(char *server);
static void show_userlist(char *server);
static void list_volumes(char *server);
static void str_trim_right(char *s, char c);


static int
ncp_get_connid(char *server, int justattach)
{
	int connid, error;
	struct ncp_conn_loginfo li;

	connid = ncp_conn_find(server, NULL);
	if (connid > 0) {
		ncp_conn_getinfo(connid, &conndesc);
		return connid;
	}
	if (!justattach) {
		if (connid == -1) {
			printf("You are not attached to server %s\n",server);
			return -1;
		}
		printf("You are not attached to any server\n");
		return -1;
	}
	ncp_li_init(&li, 0, NULL);
	if (server) {
		ncp_li_setserver(&li, server);
		error = ncp_find_fileserver(&li, AF_IPX, NULL);
		if (error) {
			printf("Could not find server %s\n", li.server);
			return -1;
		}
	} else {
		error = ncp_find_fileserver(&li, AF_IPX, NULL);
		if (error) {
			printf("Can't find any file server\n");
			return -1;
		}
	}
	error = ncp_connect(&li, &connid);
	if (error) {
		printf("Can't attach to a nearest server\n");
		return -1;
	}
	ncp_conn_getinfo(connid, &conndesc);
	return connid;
}

static struct ncp_bitname conn_statenames [] = {
	{NCPFL_INVALID, "invalid"},
	{NCPFL_LOGGED,	"active"},
	{NCPFL_PERMANENT, "permanent"},
	{NCPFL_PRIMARY,	"primary"},
	{0, NULL}
};

static void
str_trim_right(char *s, char c)
{
	int len;

	for (len = strlen(s) - 1; len > 0 && s[len] == c; len--)
		s[len] = '\0';
}

void
show_connlist(void)
{
	void *p;
	int cnt;
	char buf[200];
	struct ncp_conn_stat *ncsp;

	printf("Active NCP connections:\n");
	p = ncp_conn_list();
	if (p == NULL) {
		printf("None\n");
		return;
	}
	printf(" refid server:user(connid), owner:group(mode), refs, <state>\n");
	cnt = *(int*)p;
	ncsp = (struct ncp_conn_stat*)(((int*)p)+1);
	while (cnt--) {
		printf("%6d %s:%s(%d), %s:%s(%o), %d, %s",
		    ncsp->connRef, ncsp->li.server,ncsp->user,ncsp->connid,
		    user_from_uid(ncsp->owner, 0), 
		    group_from_gid(ncsp->group, 0), 
		    ncsp->li.access_mode,
		    ncsp->ref_cnt,
		    ncp_printb(buf, ncsp->flags, conn_statenames));
		printf("\n");
		ncsp++;
	}
	free(p);
	printf("\n");
}

void
show_serverlist(char *server)
{
	int found = 0, connid;
	struct ncp_bindery_object obj;
	char *pattern = "*";

	connid = ncp_get_connid(server, 1);
	if (connid < 0)
		return;
	printf("Visible servers (from %s):\n", conndesc.li.server);
	printf("Name                                            Network    Node       Port\n");
	printf("----------------------------------------------- -------- ------------ ----\n");
	obj.object_id = 0xffffffff;

	while (ncp_scan_bindery_object(connid, obj.object_id, NCP_BINDERY_FSERVER, 
	    pattern, &obj) == 0) {
		struct nw_property prop;
		struct ipx_addr *naddr = (struct ipx_addr *) &prop;

		found = 1;
		printf("%-48s", obj.object_name);

		if (ncp_read_property_value(connid, NCP_BINDERY_FSERVER,
					    obj.object_name, 1, "NET_ADDRESS",
					    &prop) == 0) {
			ipx_print_addr(naddr);
		}
		printf("\n");
	}

	if (!found) {
		printf("No servers found\n");
	}
	printf("\n");
}


void
show_userlist(char *server)
{
	int connid, error, i;
	struct ncp_file_server_info info;
	struct ncp_bindery_object user;
	time_t login_time;
	struct ipx_addr addr;
	u_int8_t conn_type;

	connid = ncp_get_connid(server, 0);
	if (connid < 0) return;
	if (ncp_get_file_server_information(connid, &info) != 0) {
		perror("Could not get server information");
		return;
	}
	printf("User information for server %s\n",info.ServerName);
	printf("\n%-6s%-21s%-27s%-12s\n"
	       "---------------------------------------------"
	       "---------------------------------\n",
	       "Conn",
	       "User name",
	       "Station Address",
	       "Login time");
	for (i = 1; i <= info.MaximumServiceConnections; i++) {
		char name[49];
		name[48] = '\0';
		error = ncp_get_stations_logged_info(connid, i, &user, &login_time);
		if (error) continue;
		memset(&addr, 0, sizeof(addr));
		error = ncp_get_internet_address(connid, i, &addr, &conn_type);
		if (error) continue;
		memcpy(name, user.object_name, 48);
		str_trim_right(name, ' ');
		printf("%4d: %-20s ", i, name);
		ipx_print_addr(&addr);
		printf(" ");
		printf("%s", ctime(&login_time));
	}

	return;
}

void
show_queuelist(char *server, char *patt)
{
	struct ncp_bindery_object q;
	int found = 0, connid;
	char default_pattern[] = "*";
	char *pattern = default_pattern;

	connid = ncp_get_connid(server, 1);
	if (connid < 0) return;
	if (patt != NULL)
		pattern = patt;
	ncp_str_upper(pattern);

	printf("\nServer: %s\n", server);
	printf("%-52s%-10s\n"
	       "-----------------------------------------------"
	       "-------------\n",
	       "Print queue name",
	       "Queue ID");
	q.object_id = 0xffffffff;

	while (ncp_scan_bindery_object(connid, q.object_id,
				       NCP_BINDERY_PQUEUE, pattern, &q) == 0)
	{
		found = 1;
		printf("%-52s", q.object_name);
		printf("%08X\n", (unsigned int) q.object_id);
	}

	if (!found) {
		printf("No queues found\n");
	}
	return;
}

void
list_volumes(char *server)
{
	int found = 0, connid, i, error;
	struct ncp_file_server_info si;
	char volname[NCP_VOLNAME_LEN+1];

	connid = ncp_get_connid(server, 1);
	if (connid < 0) return;
	
	error = ncp_get_file_server_information(connid, &si);
	if (error) {
		ncp_error("Can't get information for server %s", error, server);
		return;
	}

	printf("\nMounted volumes on server %s:\n", server);
	printf("Number Name\n");
	printf("------ -----------------------------------------------\n");

	for(i = 0; i < si.NumberMountedVolumes; i++) {
		if (NWGetVolumeName(connid, i, volname))
			continue;
		found = 1;
		printf("%6d %s\n", i, volname);
	}

	if (!found)
		printf("No volumes found ?\n");
	return;
}

struct ncp_bind_type {
	u_long	type;
	char	*name;
};

static struct ncp_bind_type btypes[] = {
	{NCP_BINDERY_USER,	"USER"},
	{NCP_BINDERY_UGROUP,	"GROUP"},
	{NCP_BINDERY_PSERVER,	"PSERVER"},
	{0x278,			"TREE"},
	{0, NULL}
};

void
list_bindery(char *server, char *type, char *patt)
{
	struct ncp_bindery_object q;
	int i, found = 0, connid;
	char default_pattern[] = "*";
	char *pattern = default_pattern;
	u_long objtype;

	ncp_str_upper(type);
	objtype = 0;

	for(i = 0; btypes[i].type; i++) {
		if (strcmp(btypes[i].name, type) == 0) {
			objtype = btypes[i].type;
			break;
		}
	}
	if (!objtype) {
		printf("Bindery object of type %s is unknown\n", type);
		return;
	}
	if (patt != NULL)
		pattern = patt;
	ncp_str_upper(pattern);
	connid = ncp_get_connid(server, 1);
	if (connid < 0) return;

	connid = ncp_get_connid(server, 1);
	if (connid < 0) return;


	printf("\nServer: %s\n", server);
	printf("%-52s%-10s\n"
	       "-----------------------------------------------"
	       "-------------\n",
	       "Object name",
	       "Object ID");

	q.object_id = 0xffffffff;
	while (ncp_scan_bindery_object(connid, q.object_id,
				       objtype, pattern, &q) == 0)
	{
		found = 1;
		printf("%-52s", q.object_name);
		printf("%08X\n", (unsigned int) q.object_id);
	}

	if (!found) {
		printf("No bindery objects found\n");
	}
	return;
}

enum listop {
	LO_NONE, LO_SERVERS, LO_QUEUES, LO_BINDERY, LO_USERS, LO_VOLUMES
};

#define MAX_ARGS	10

int
main(int argc, char *argv[])
{
	int opt, wdone = 0, nargs = 0, i;
	enum listop what;
	char *args[MAX_ARGS];

	bzero(args, sizeof(args));

	what = LO_NONE;
	while ((opt = getopt(argc, argv, "h")) != EOF) {
		switch (opt) {
		    case 'h': case '?':
			help();
			/*NOTREACHED */
		    default:
			help();
			return 1;
		}
	}
	if (optind >= argc)
		help();

	if(ncp_initlib())
		exit(1);

	switch(argv[optind++][0]) {
	    case 'b':
		what = LO_BINDERY;
		nargs = 2;
		break;
	    case 'c':
		show_connlist();
		return 0;
	    case 's':
		what = LO_SERVERS;
		break;
	    case 'u':
		what = LO_USERS;
		nargs = 1;
		break;
	    case 'q':
		what = LO_QUEUES;
		nargs = 1;
		break;
	    case 'v':
		what = LO_VOLUMES;
		nargs = 1;
		break;
	    default:
		printf("Unknown command %s\n", argv[optind-1]);
		help();
	}
	for (i = 0; i < MAX_ARGS; i++) {
		if (optind < argc) {
			args[i] = argv[optind++];
		} else if (i < nargs) {
			printf("Not enough arguments\n");
			help();
			return 1;
		} else
			break;
	}
	switch(what) {
	    case LO_SERVERS:
		show_serverlist(args[0]);
		wdone = 1;
		break;
	    case LO_USERS:
		show_userlist(args[0]);
		wdone = 1;
		break;
	    case LO_QUEUES:
		show_queuelist(args[0], args[1]);
		wdone = 1;
		break;
	    case LO_VOLUMES:
		list_volumes(args[0]);
		wdone = 1;
		break;
	    case LO_BINDERY:
		list_bindery(args[0], args[1], args[2]);
		wdone = 1;
		break;
	    default:
		help();
	}
	return 0;
}

static void
help(void)
{
	printf("\n");
	printf("usage: %s command [args]\n", __progname);
	printf("where commands are:\n"
	" b server user|group [pattern]	list bindery objects on server\n"
	" c 				display opened connections\n"
	" s [server]			display known servers\n"
	" u server			list logged-in users on server\n"
	" q server [pattern]		list print queues on server\n"
	" v server			list mounted volumes on a specified server\n"
	"\n");
	exit(1);
}
