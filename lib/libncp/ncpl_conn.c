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
 *
 *  $FreeBSD$
 */

/*
 *
 * Current scheme to create/open connection:
 * 1. ncp_li_init() - lookup -S [-U] options in command line
 * 2. ncp_li_init() - try to find existing connection
 * 3. ncp_li_init() - if no server name and no accessible connections - bail out
 * 4. This is connection candidate, read .rc file, override with command line
 *    and go ahead
 * Note: connection referenced only via ncp_login() call. Although it is 
 * possible to get connection handle in other way, it will be unwise to use
 * it, since conn can be destroyed at any time.
 * 
 */
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>

#include <netncp/ncp_lib.h>
#include <netncp/ncp_rcfile.h>
#include <nwfs/nwfs.h>

static char *server_name; /* need a better way ! */



int
ncp_li_setserver(struct ncp_conn_loginfo *li, const char *arg) {
	if (strlen(arg) >= NCP_BINDERY_NAME_LEN) {
		ncp_error("server name '%s' too long", 0, arg);
		return ENAMETOOLONG;
	}
	ncp_str_upper(strcpy(li->server, arg));
	return 0;
}

int
ncp_li_setuser(struct ncp_conn_loginfo *li, char *arg) {
	if (arg && strlen(arg) >= NCP_BINDERY_NAME_LEN) {
		ncp_error("user name '%s' too long", 0, arg);
		return ENAMETOOLONG;
	}
	if (li->user)
		free(li->user);
	if (arg) {
		li->user = strdup(arg);
		if (li->user == NULL)
			return ENOMEM;
		ncp_str_upper(li->user);
	} else
		li->user = NULL;
	return 0;
}

int
ncp_li_setpassword(struct ncp_conn_loginfo *li, const char *passwd) {
	if (passwd && strlen(passwd) >= 127) {
		ncp_error("password too long", 0);
		return ENAMETOOLONG;
	}
	if (li->password) {
		bzero(li->password, strlen(li->password));
		free(li->password);
	}
	if (passwd) {
		li->password = strdup(passwd);
		if (li->password == NULL)
			return ENOMEM;
	} else
		li->password = NULL;
	return 0;
}
/*
 * Prescan command line for [-S server] [-U user] arguments
 * and fill li structure with defaults
 */
int
ncp_li_init(struct ncp_conn_loginfo *li, int argc, char *argv[]) {
	int  opt, error = 0;
	char *arg;

	bzero(li,sizeof(*li));
	li->timeout = 15;	/* these values should be large enough to handle */
	li->retry_count = 4;	/* slow servers, even on ethernet */
	li->access_mode = 0;
	li->password = NULL;
	li->sig_level = 1;
	li->objtype = NCP_BINDERY_USER;
	li->owner = NCP_DEFAULT_OWNER;
	li->group = NCP_DEFAULT_GROUP;
	server_name = NULL;
	if (argv == NULL) return 0;
	while (error == 0 && (opt = ncp_getopt(argc, argv, ":S:U:")) != -1) {
		arg = ncp_optarg;
		switch (opt) {
		    case 'S':
			error = ncp_li_setserver(li, arg);
			break;
		    case 'U':
			error = ncp_li_setuser(li, arg);
			break;
		}
	}
	ncp_optind = ncp_optreset = 1;
	return error;
}

void
ncp_li_done(struct ncp_conn_loginfo *li) {
	if (li->user)
		free(li->user);
	if (li->password)
		free(li->password);
}

/*
 * Lookup existing connection based on li structure, if connection
 * found, it will be referenced. Otherwise full login sequence performed.
 */
int
ncp_li_login(struct ncp_conn_loginfo *li, int *aconnid) {
	int connHandle, error;

	if ((error = ncp_conn_scan(li, &connHandle)) == 0) {
		*aconnid = connHandle;
		return 0;
	}
	error = ncp_connect(li, &connHandle);
	if (error) return errno;
	error = ncp_login(connHandle, li->user, li->objtype, li->password);
	if (error) {
		ncp_disconnect(connHandle);
	} else
		*aconnid = connHandle;
	return error;
}

/*
 * read rc file as follows:
 * 1. read [server] section
 * 2. override with [server:user] section
 * Since abcence of rcfile is not a bug, silently ignore that fact.
 * rcfile never closed to reduce number of open/close operations.
 */
int
ncp_li_readrc(struct ncp_conn_loginfo *li) {
	int i, val, error;
	char uname[NCP_BINDERY_NAME_LEN*2+1];
	char *sect = NULL, *p;

	/*
	 * if info from cmd line incomplete, try to find existing
	 * connection and fill server/user from it.
	 */
	if (li->server[0] == 0 || li->user == NULL) {
		int connHandle;
		struct ncp_conn_stat cs;
		
		if ((error = ncp_conn_scan(li, &connHandle)) != 0) {
			ncp_error("no default connection found", errno);
			return error;
		}
		ncp_conn_getinfo(connHandle, &cs);
		ncp_li_setserver(li, cs.li.server);
		ncp_li_setuser(li, cs.user);
		ncp_li_setpassword(li, "");
		ncp_disconnect(connHandle);
	}
	if (ncp_open_rcfile()) 	return 0;
	
	for (i = 0; i < 2; i++) {
		switch (i) {
		    case 0:
			sect = li->server;
			break;
		    case 1:
			strcat(strcat(strcpy(uname,li->server),":"),li->user ? li->user : "default");
			sect = uname;
			break;
		}
		rc_getstringptr(ncp_rc, sect, "password", &p);
		if (p)
			ncp_li_setpassword(li, p);
		rc_getint(ncp_rc,sect, "timeout", &li->timeout);
		rc_getint(ncp_rc,sect, "retry_count", &li->retry_count);
		rc_getint(ncp_rc,sect, "sig_level", &li->sig_level);
		if (rc_getint(ncp_rc,sect,"access_mode",&val) == 0)
			li->access_mode = val;
		if(rc_getbool(ncp_rc,sect,"bindery",&val) == 0 && val) {
			li->opt |= NCP_OPT_BIND;
		}
	}
	return 0;
}

/*
 * check for all uncompleted fields
 */
int
ncp_li_check(struct ncp_conn_loginfo *li) {
	int error = 0;
	char *p;
	
	do {
		if (li->server[0] == 0) {
			ncp_error("no server name specified", 0);
			error = 1;
			break;
		}
		error = ncp_find_fileserver(li,
		    (server_name==NULL) ? AF_IPX : AF_INET, server_name);
		if (error) {
			ncp_error("can't find server %s", error, li->server);
			break;
		}
		if (li->user == NULL || li->user[0] == 0) {
			ncp_error("no user name specified for server %s",
			    0, li->server);
			error = 1;
			break;
		}
		if (li->password == NULL) {
			p = getpass("Netware password:");
			error = ncp_li_setpassword(li, p) ? 1 : 0;
		}
	} while (0);
	return error;
}

int
ncp_conn_cnt(void) {
	int error, cnt = 0, len = sizeof(cnt);
	
#if __FreeBSD_version < 400001
	error = sysctlbyname("net.ipx.ncp.conn_cnt", &cnt, &len, NULL, 0);
#else
	error = sysctlbyname("net.ncp.conn_cnt", &cnt, &len, NULL, 0);
#endif
	if (error) cnt = 0;
	return cnt;
}

/*
 * Find an existing connection and reference it
 */
int
ncp_conn_find(char *server,char *user) {
	struct ncp_conn_args ca;
	int connid, error;

	if (server == NULL && user == NULL) {
		error = ncp_conn_scan(NULL,&connid);
		if (error) return -2;
		return connid;
	}
	if (server == NULL)
		return -2;
	ncp_str_upper(server);
	if (user) ncp_str_upper(user);
	bzero(&ca, sizeof(ca));
	ncp_li_setserver(&ca, server);
	ncp_li_setuser(&ca, user);
	error = ncp_conn_scan(&ca,&connid);
	if (error)
		connid = -1;
	return connid;
}

int
ncp_li_arg(struct ncp_conn_loginfo *li, int opt, char *arg) {
	int error = 0, sig_level;
	char *p, *cp;
	struct group *gr;
	struct passwd *pw;

	switch(opt) {
	    case 'S': /* we already fill server/[user] pair */
	    case 'U':
		break;
	    case 'A':
		server_name = arg;
		break;
	    case 'B':
		li->opt |= NCP_OPT_BIND;
		break;
	    case 'C':
		li->opt |= NCP_OPT_NOUPCASEPASS;
		break;
	    case 'I':
		sig_level = atoi(arg);
		if (sig_level < 0 || sig_level > 3) {
			ncp_error("invalid NCP signature level option `%s'\
			    (must be a number between 0 and 3)", 0, arg);
			error = 1;
		}
		li->sig_level = sig_level;
		if (sig_level > 1) li->opt |= NCP_OPT_SIGN;
		break;
	    case 'M':
		li->access_mode = strtol(arg, NULL, 8);
		break;
	    case 'N':
		ncp_li_setpassword(li, "");
		break;
	    case 'O':
		p = strdup(arg);
		cp = strchr(p, ':');
		if (cp) {
			*cp++ = '\0';
			if (*cp) {
				gr = getgrnam(cp);
				if (gr) {
					li->group = gr->gr_gid;
				} else
					ncp_error("Invalid group name %s, ignored",
					    0, cp);
			}
		}
		if (*p) {
			pw = getpwnam(p);
			if (pw) {
				li->owner = pw->pw_uid;
			} else
				ncp_error("Invalid user name %s, ignored", 0, p);
		}
		endpwent();
		free(p);
		break;
	    case 'P':
		li->opt |= NCP_OPT_PERMANENT;
		break;
	    case 'R':
		li->retry_count = atoi(arg);
		break;
	    case 'W':
		li->timeout = atoi(arg);
		break;
	}
	return error;
}

void *
ncp_conn_list(void) {
	int error, cnt = 0, len;
	void *p;
	
	cnt = ncp_conn_cnt();
	if (cnt == 0) return NULL;
	len = cnt*(sizeof(struct ncp_conn_stat))+sizeof(int);
	p = malloc(len);
	if (p == NULL) return NULL;
#if __FreeBSD_version < 400001
	error = sysctlbyname("net.ipx.ncp.conn_stat", p, &len, NULL, 0);
#else
	error = sysctlbyname("net.ncp.conn_stat", p, &len, NULL, 0);
#endif
	if (error) {
		free(p);
		p = NULL;
	}
	return p;
}


int
ncp_conn_setflags(int connid, u_int16_t mask, u_int16_t flags) {
	int error;
	DECLARE_RQ;

	ncp_init_request(conn);
	ncp_add_byte(conn, NCP_CONN_SETFLAGS);
	ncp_add_word_lh(conn, mask);
	ncp_add_word_lh(conn, flags);
	if ((error = ncp_conn_request(connid, conn)) < 0) 
		return -1;
	return error;
}

int
ncp_login(int connHandle, const char *user, int objtype, const char *password) {
	int error;
	struct ncp_conn_login *p;
	DECLARE_RQ;

	ncp_init_request(conn);
	ncp_add_byte(conn, NCP_CONN_LOGIN);
	p = (struct ncp_conn_login *)&conn->packet[conn->rqsize];
	(const char*)p->username = user;
	p->objtype = objtype;
	(const char*)p->password = password;
	conn->rqsize += sizeof(*p);
	if ((error = ncp_conn_request(connHandle, conn)) < 0) 
		return -1;
	return error;
}

int
ncp_connect_addr(struct sockaddr *sa, NWCONN_HANDLE *chp) {
	int error;
	struct ncp_conn_args li;

	bzero(&li, sizeof(li));
	bcopy(sa, &li.addr, sa->sa_len);
	/*
	 * XXX Temporary !!!. server will be filled in kernel !!!
	 */
	strcpy(li.server,ipx_ntoa(li.ipxaddr.sipx_addr));
	error = ncp_connect(&li, chp);
	return error;
}

int
ncp_conn_getinfo(int connHandle, struct ncp_conn_stat *ps) {
	int error;
	DECLARE_RQ;

	ncp_init_request(conn);
	ncp_add_byte(conn, NCP_CONN_GETINFO);
	if ((error = ncp_conn_request(connHandle, conn)) < 0) 
		return -1;
	memcpy(ps, ncp_reply_data(conn,0), sizeof(*ps));
	return error;
}

int
ncp_conn_getuser(int connHandle, char **user) {
	int error;
	DECLARE_RQ;

	ncp_init_request(conn);
	ncp_add_byte(conn, NCP_CONN_GETUSER);
	if ((error = ncp_conn_request(connHandle, conn)) < 0) 
		return -1;
	*user = strdup(ncp_reply_data(conn,0));
	return error;
}

int
ncp_conn2ref(int connHandle, int *connRef) {
	int error;
	DECLARE_RQ;

	ncp_init_request(conn);
	ncp_add_byte(conn, NCP_CONN_CONN2REF);
	if ((error = ncp_conn_request(connHandle, conn)) < 0) 
		return -1;
	*connRef = *((int*)ncp_reply_data(conn,0));
	return error;
}

int
ncp_path2conn(char *path, int *connHandle) {
	struct statfs st;
	int d, error;

	if ((error = statfs(path, &st)) != 0) return errno;
	if (strcmp(st.f_fstypename,"nwfs") != 0) return EINVAL;
	if ((d = open(path, O_RDONLY)) < 0) return errno;
	if ((error = ioctl(d,NWFSIOC_GETCONN, connHandle)) != 0) return errno;
	close(d);
	return 0;
}

int
ncp_conn_dup(NWCONN_HANDLE org, NWCONN_HANDLE *res) {
	int error;
	DECLARE_RQ;

	ncp_init_request(conn);
	ncp_add_byte(conn, NCP_CONN_DUP);
	if ((error = ncp_conn_request(org, conn)) < 0) 
		return errno;
	*res = *((int*)ncp_reply_data(conn, 0));
	return 0;
}
