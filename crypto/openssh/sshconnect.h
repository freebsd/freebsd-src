/* $OpenBSD: sshconnect.h,v 1.47 2023/10/12 02:18:18 djm Exp $ */

/*
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

typedef struct Sensitive Sensitive;
struct Sensitive {
	struct sshkey	**keys;
	int		nkeys;
};

struct ssh_conn_info {
	char *conn_hash_hex;
	char *shorthost;
	char *uidstr;
	char *keyalias;
	char *thishost;
	char *host_arg;
	char *portstr;
	char *remhost;
	char *remuser;
	char *homedir;
	char *locuser;
	char *jmphost;
};

struct addrinfo;
struct ssh;
struct hostkeys;
struct ssh_conn_info;

/* default argument for client percent expansions */
#define DEFAULT_CLIENT_PERCENT_EXPAND_ARGS(conn_info) \
	"C", conn_info->conn_hash_hex, \
	"L", conn_info->shorthost, \
	"i", conn_info->uidstr, \
	"k", conn_info->keyalias, \
	"l", conn_info->thishost, \
	"n", conn_info->host_arg, \
	"p", conn_info->portstr, \
	"d", conn_info->homedir, \
	"h", conn_info->remhost, \
	"r", conn_info->remuser, \
	"u", conn_info->locuser, \
	"j", conn_info->jmphost

int	 ssh_connect(struct ssh *, const char *, const char *,
	    struct addrinfo *, struct sockaddr_storage *, u_short,
	    int, int *, int);
void	 ssh_kill_proxy_command(void);

void	 ssh_login(struct ssh *, Sensitive *, const char *,
    struct sockaddr *, u_short, struct passwd *, int,
    const struct ssh_conn_info *);

int	 verify_host_key(char *, struct sockaddr *, struct sshkey *,
    const struct ssh_conn_info *);

void	 get_hostfile_hostname_ipaddr(char *, struct sockaddr *, u_short,
    char **, char **);

void	 ssh_kex2(struct ssh *ssh, char *, struct sockaddr *, u_short,
    const struct ssh_conn_info *);

void	 ssh_userauth2(struct ssh *ssh, const char *, const char *,
    char *, Sensitive *);

int	 ssh_local_cmd(const char *);

void	 maybe_add_key_to_agent(const char *, struct sshkey *,
    const char *, const char *);

void	 load_hostkeys_command(struct hostkeys *, const char *,
    const char *, const struct ssh_conn_info *,
    const struct sshkey *, const char *);
