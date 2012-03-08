/*-
 * Copyright (c) 2003-2007, Petar Zhivkov Petrov <pesho.petrov@gmail.com>
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "auth.h"
#include "config.h"
#include "misc.h"
#include "proto.h"
#include "stream.h"

#define MD5_BYTES			16

/* This should be at least 2 * MD5_BYTES + 6 (length of "$md5$" + 1) */
#define MD5_CHARS_MAX		(2*(MD5_BYTES)+6)

struct srvrecord {
	char server[MAXHOSTNAMELEN];
	char client[256];
	char password[256];
};

static int		auth_domd5auth(struct config *);
static int		auth_lookuprecord(char *, struct srvrecord *);
static int		auth_parsetoken(char **, char *, int);
static void		auth_makesecret(struct srvrecord *, char *);
static void		auth_makeresponse(char *, char *, char *);
static void		auth_readablesum(unsigned char *, char *);
static void		auth_makechallenge(struct config *, char *);
static int		auth_checkresponse(char *, char *, char *);

int auth_login(struct config *config)
{
	struct stream *s;
	char hostbuf[MAXHOSTNAMELEN];
	char *login, *host;
	int error;

	s = config->server;
	error = gethostname(hostbuf, sizeof(hostbuf));
	hostbuf[sizeof(hostbuf) - 1] = '\0';
	if (error)
		host = NULL;
	else
		host = hostbuf;
	login = getlogin();
	proto_printf(s, "USER %s %s\n", login != NULL ? login : "?",
	    host != NULL ? host : "?");
	stream_flush(s);
	error = auth_domd5auth(config);
	return (error);
}

static int
auth_domd5auth(struct config *config)
{
	struct stream *s;
	char *line, *cmd, *challenge, *realm, *client, *srvresponse, *msg;
	char shrdsecret[MD5_CHARS_MAX], response[MD5_CHARS_MAX];
	char clichallenge[MD5_CHARS_MAX];
	struct srvrecord auth;
	int error;

	lprintf(2, "MD5 authentication started\n");
	s = config->server;
	line = stream_getln(s, NULL);
	cmd = proto_get_ascii(&line);
	realm = proto_get_ascii(&line);
	challenge = proto_get_ascii(&line);
	if (challenge == NULL ||
	    line != NULL ||
	    (strcmp(cmd, "AUTHMD5") != 0)) {
		lprintf(-1, "Invalid server reply to USER\n");
		return (STATUS_FAILURE);
	}

	client = NULL;
	response[0] = clichallenge[0] = '.';
	response[1] = clichallenge[1] = 0;
	if (config->reqauth || (strcmp(challenge, ".") != 0)) {
		if (strcmp(realm, ".") == 0) {
			lprintf(-1, "Authentication required, but not enabled on server\n");
			return (STATUS_FAILURE);
		}
		error = auth_lookuprecord(realm, &auth);
		if (error != STATUS_SUCCESS)
			return (error);
		client = auth.client;
		auth_makesecret(&auth, shrdsecret);
	}

	if (strcmp(challenge, ".") != 0)
		auth_makeresponse(challenge, shrdsecret, response);
	if (config->reqauth)
		auth_makechallenge(config, clichallenge);
	proto_printf(s, "AUTHMD5 %s %s %s\n",
		client == NULL ? "." : client, response, clichallenge);
	stream_flush(s);
	line = stream_getln(s, NULL);
	cmd = proto_get_ascii(&line);
	if (cmd == NULL || line == NULL)
		goto bad;
	if (strcmp(cmd, "OK") == 0) {
		srvresponse = proto_get_ascii(&line);
		if (srvresponse == NULL)
			goto bad;
		if (config->reqauth &&
		    !auth_checkresponse(srvresponse, clichallenge, shrdsecret)) {
			lprintf(-1, "Server failed to authenticate itself to client\n");
			return (STATUS_FAILURE);
		}
		lprintf(2, "MD5 authentication successful\n");
		return (STATUS_SUCCESS);
	}
	if (strcmp(cmd, "!") == 0) {
		msg = proto_get_rest(&line);
		if (msg == NULL)
			goto bad;
		lprintf(-1, "Server error: %s\n", msg);
		return (STATUS_FAILURE);
	}
bad:
	lprintf(-1, "Invalid server reply to AUTHMD5\n");
	return (STATUS_FAILURE);
}

static int
auth_lookuprecord(char *server, struct srvrecord *auth)
{
	char *home, *line, authfile[FILENAME_MAX];
	struct stream *s;
	int linenum = 0, error;

	home = getenv("HOME");
	if (home == NULL) {
		lprintf(-1, "Environment variable \"HOME\" is not set\n");
		return (STATUS_FAILURE);
	}
	snprintf(authfile, sizeof(authfile), "%s/%s", home, AUTHFILE);
	s = stream_open_file(authfile, O_RDONLY);
	if (s == NULL) {
		lprintf(-1, "Could not open file %s\n", authfile);
		return (STATUS_FAILURE);
	}

	while ((line = stream_getln(s, NULL)) != NULL) {
		linenum++;
		if (line[0] == '#' || line[0] == '\0')
			continue;
		error = auth_parsetoken(&line, auth->server,
		    sizeof(auth->server));
		if (error != STATUS_SUCCESS) {
			lprintf(-1, "%s:%d Missing client name\n", authfile, linenum);
			goto close;
		}
		/* Skip the rest of this line, it isn't what we are looking for. */
		if (strcasecmp(auth->server, server) != 0)
			continue;
		error = auth_parsetoken(&line, auth->client,
		    sizeof(auth->client));
		if (error != STATUS_SUCCESS) {
			lprintf(-1, "%s:%d Missing password\n", authfile, linenum);
			goto close;
		}
		error = auth_parsetoken(&line, auth->password,
		    sizeof(auth->password));
		if (error != STATUS_SUCCESS) {
			lprintf(-1, "%s:%d Missing comment\n", authfile, linenum);
			goto close;
		}
		stream_close(s);
		lprintf(2, "Found authentication record for server \"%s\"\n",
		    server);
		return (STATUS_SUCCESS);
	}
	lprintf(-1, "Unknown server \"%s\". Fix your %s\n", server , authfile);
	memset(auth->password, 0, sizeof(auth->password));
close:
	stream_close(s);
	return (STATUS_FAILURE);
}

static int
auth_parsetoken(char **line, char *buf, int len)
{
	char *colon;

	colon = strchr(*line, ':');
	if (colon == NULL)
		return (STATUS_FAILURE);
	*colon = 0;
	buf[len - 1] = 0;
	strncpy(buf, *line, len - 1);
	*line = colon + 1;
	return (STATUS_SUCCESS);
}

static void
auth_makesecret(struct srvrecord *auth, char *secret)
{
	char *s, ch;
	const char *md5salt = "$md5$";
	unsigned char md5sum[MD5_BYTES];
	MD5_CTX md5;

	MD5_Init(&md5);
	for (s = auth->client; *s != 0; ++s) {
		ch = tolower(*s);
		MD5_Update(&md5, &ch, 1);
	}
	MD5_Update(&md5, ":", 1);
	for (s = auth->server; *s != 0; ++s) {
		ch = tolower(*s);
		MD5_Update(&md5, &ch, 1);
	}
	MD5_Update(&md5, ":", 1);
	MD5_Update(&md5, auth->password, strlen(auth->password));
	MD5_Final(md5sum, &md5);
	memset(secret, 0, MD5_CHARS_MAX);
	strcpy(secret, md5salt);
	auth_readablesum(md5sum, secret + strlen(md5salt));
}

static void
auth_makeresponse(char *challenge, char *sharedsecret, char *response)
{
	MD5_CTX md5;
	unsigned char md5sum[MD5_BYTES];

	MD5_Init(&md5);
	MD5_Update(&md5, sharedsecret, strlen(sharedsecret));
	MD5_Update(&md5, ":", 1);
	MD5_Update(&md5, challenge, strlen(challenge));
	MD5_Final(md5sum, &md5);
	auth_readablesum(md5sum, response);
}

/*
 * Generates a challenge string which is an MD5 sum
 * of a fairly random string. The purpose is to decrease
 * the possibility of generating the same challenge
 * string (even by different clients) more then once
 * for the same server.
 */
static void
auth_makechallenge(struct config *config, char *challenge)
{
	MD5_CTX md5;
	unsigned char md5sum[MD5_BYTES];
	char buf[128];
	struct timeval tv;
	struct sockaddr_in laddr;
	pid_t pid, ppid;
	int error, addrlen;

	gettimeofday(&tv, NULL);
	pid = getpid();
	ppid = getppid();
	srandom(tv.tv_usec ^ tv.tv_sec ^ pid);
	addrlen = sizeof(laddr);
	error = getsockname(config->socket, (struct sockaddr *)&laddr, &addrlen);
	if (error < 0) {
		memset(&laddr, 0, sizeof(laddr));
	}
	gettimeofday(&tv, NULL);
	MD5_Init(&md5);
	snprintf(buf, sizeof(buf), "%s:%jd:%ld:%ld:%d:%d",
	    inet_ntoa(laddr.sin_addr), (intmax_t)tv.tv_sec, tv.tv_usec,
	    random(), pid, ppid);
	MD5_Update(&md5, buf, strlen(buf));
	MD5_Final(md5sum, &md5);
	auth_readablesum(md5sum, challenge);
}

static int
auth_checkresponse(char *response, char *challenge, char *secret)
{
	char correctresponse[MD5_CHARS_MAX];

	auth_makeresponse(challenge, secret, correctresponse);
	return (strcmp(response, correctresponse) == 0);
}

static void
auth_readablesum(unsigned char *md5sum, char *readable)
{
	unsigned int i;
	char *s = readable;

	for (i = 0; i < MD5_BYTES; ++i, s+=2) {
		sprintf(s, "%.2x", md5sum[i]);
	}
}

