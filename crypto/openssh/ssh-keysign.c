/*
 * Copyright (c) 2002 Markus Friedl.  All rights reserved.
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
#include "includes.h"
RCSID("$OpenBSD: ssh-keysign.c,v 1.7 2002/07/03 14:21:05 markus Exp $");

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>

#include "log.h"
#include "key.h"
#include "ssh.h"
#include "ssh2.h"
#include "misc.h"
#include "xmalloc.h"
#include "buffer.h"
#include "bufaux.h"
#include "authfile.h"
#include "msg.h"
#include "canohost.h"
#include "pathnames.h"
#include "readconf.h"

uid_t original_real_uid;	/* XXX readconf.c needs this */

#ifdef HAVE___PROGNAME
extern char *__progname;
#else
char *__progname;
#endif

static int
valid_request(struct passwd *pw, char *host, Key **ret, u_char *data,
    u_int datalen)
{
	Buffer b;
	Key *key;
	u_char *pkblob;
	u_int blen, len;
	char *pkalg, *p;
	int pktype, fail;

	fail = 0;

	buffer_init(&b);
	buffer_append(&b, data, datalen);

	/* session id, currently limited to SHA1 (20 bytes) */
	p = buffer_get_string(&b, &len);
	if (len != 20)
		fail++;
	xfree(p);

	if (buffer_get_char(&b) != SSH2_MSG_USERAUTH_REQUEST)
		fail++;

	/* server user */
	buffer_skip_string(&b);

	/* service */
	p = buffer_get_string(&b, NULL);
	if (strcmp("ssh-connection", p) != 0)
		fail++;
	xfree(p);

	/* method */
	p = buffer_get_string(&b, NULL);
	if (strcmp("hostbased", p) != 0)
		fail++;
	xfree(p);

	/* pubkey */
	pkalg = buffer_get_string(&b, NULL);
	pkblob = buffer_get_string(&b, &blen);

	pktype = key_type_from_name(pkalg);
	if (pktype == KEY_UNSPEC)
		fail++;
	else if ((key = key_from_blob(pkblob, blen)) == NULL)
		fail++;
	else if (key->type != pktype)
		fail++;
	xfree(pkalg);
	xfree(pkblob);

	/* client host name, handle trailing dot */
	p = buffer_get_string(&b, &len);
	debug2("valid_request: check expect chost %s got %s", host, p);
	if (strlen(host) != len - 1)
		fail++;
	else if (p[len - 1] != '.')
		fail++;
	else if (strncasecmp(host, p, len - 1) != 0)
		fail++;
	xfree(p);

	/* local user */
	p = buffer_get_string(&b, NULL);

	if (strcmp(pw->pw_name, p) != 0)
		fail++;
	xfree(p);

	/* end of message */
	if (buffer_len(&b) != 0)
		fail++;

	debug3("valid_request: fail %d", fail);

	if (fail && key != NULL)
		key_free(key);
	else
		*ret = key;

	return (fail ? -1 : 0);
}

int
main(int argc, char **argv)
{
	Buffer b;
	Options options;
	Key *keys[2], *key;
	struct passwd *pw;
	int key_fd[2], i, found, version = 2, fd;
	u_char *signature, *data;
	char *host;
	u_int slen, dlen;
	u_int32_t rnd[256];

	key_fd[0] = open(_PATH_HOST_RSA_KEY_FILE, O_RDONLY);
	key_fd[1] = open(_PATH_HOST_DSA_KEY_FILE, O_RDONLY);

	seteuid(getuid());
	setuid(getuid());

	init_rng();
	seed_rng();
	arc4random_stir();

#ifdef DEBUG_SSH_KEYSIGN
	log_init("ssh-keysign", SYSLOG_LEVEL_DEBUG3, SYSLOG_FACILITY_AUTH, 0);
#endif

	/* verify that ssh-keysign is enabled by the admin */
	original_real_uid = getuid();	/* XXX readconf.c needs this */
	initialize_options(&options);
	(void)read_config_file(_PATH_HOST_CONFIG_FILE, "", &options);
	fill_default_options(&options);
	if (options.hostbased_authentication != 1)
		fatal("Hostbased authentication not enabled in %s",
		    _PATH_HOST_CONFIG_FILE);

	if (key_fd[0] == -1 && key_fd[1] == -1)
		fatal("could not open any host key");

	if ((pw = getpwuid(getuid())) == NULL)
		fatal("getpwuid failed");
	pw = pwcopy(pw);

	SSLeay_add_all_algorithms();
	for (i = 0; i < 256; i++)
		rnd[i] = arc4random();
	RAND_seed(rnd, sizeof(rnd));

	found = 0;
	for (i = 0; i < 2; i++) {
		keys[i] = NULL;
		if (key_fd[i] == -1)
			continue;
		keys[i] = key_load_private_pem(key_fd[i], KEY_UNSPEC,
		    NULL, NULL);
		close(key_fd[i]);
		if (keys[i] != NULL && keys[i]->type == KEY_RSA) {
			if (RSA_blinding_on(keys[i]->rsa, NULL) != 1) {
				error("RSA_blinding_on failed");
				key_free(keys[i]);
				keys[i] = NULL;
			}
		}
		if (keys[i] != NULL)
			found = 1;
	}
	if (!found)
		fatal("no hostkey found");

	buffer_init(&b);
	if (ssh_msg_recv(STDIN_FILENO, &b) < 0)
		fatal("ssh_msg_recv failed");
	if (buffer_get_char(&b) != version)
		fatal("bad version");
	fd = buffer_get_int(&b);
	if ((fd == STDIN_FILENO) || (fd == STDOUT_FILENO))
		fatal("bad fd");
	if ((host = get_local_name(fd)) == NULL)
		fatal("cannot get sockname for fd");

	data = buffer_get_string(&b, &dlen);
	if (valid_request(pw, host, &key, data, dlen) < 0)
		fatal("not a valid request");
	xfree(host);

	found = 0;
	for (i = 0; i < 2; i++) {
		if (keys[i] != NULL &&
		    key_equal(key, keys[i])) {
			found = 1;
			break;
		}
	}
	if (!found)
		fatal("no matching hostkey found");

	if (key_sign(keys[i], &signature, &slen, data, dlen) != 0)
		fatal("key_sign failed");
	xfree(data);

	/* send reply */
	buffer_clear(&b);
	buffer_put_string(&b, signature, slen);
	ssh_msg_send(STDOUT_FILENO, version, &b);

	return (0);
}
