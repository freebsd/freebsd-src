/*
 * Copyright (c) 1985, 1989
 *    The Regents of the University of California.  All rights reserved.
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
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#ifndef lint
static char sccsid[] = "@(#)list.c	5.23 (Berkeley) 3/21/91";
static char rcsid[] = "$Id: list.c,v 8.13 1997/11/18 00:32:33 halley Exp $";
#endif /* not lint */

/*
 *******************************************************************************
 *
 *  list.c --
 *
 *	Routines to obtain info from name and finger servers.
 *
 *	Adapted from 4.3BSD BIND ns_init.c and from finger.c.
 *
 *******************************************************************************
 */

#include "port_before.h"

#include <sys/param.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "port_after.h"

#include "res.h"

extern char *_res_resultcodes[];	/* res_debug.c */
extern char *pager;

typedef union {
	HEADER qb1;
	u_char qb2[PACKETSZ];
} querybuf;

extern HostInfo	*defaultPtr;
extern HostInfo	curHostInfo;
extern int	curHostValid;
extern int	queryType;
extern int	queryClass;

static int sockFD = -1;
int ListSubr();

/*
 *  During a listing to a file, hash marks are printed
 *  every HASH_SIZE records.
 */

#define HASH_SIZE 50


/*
 *******************************************************************************
 *
 *  ListHosts --
 *  ListHostsByType --
 *
 *	Requests the name server to do a zone transfer so we
 *	find out what hosts it knows about.
 *
 *	For ListHosts, there are five types of output:
 *	- Internet addresses (default)
 *	- cpu type and operating system (-h option)
 *	- canonical and alias names  (-a option)
 *	- well-known service names  (-s option)
 *	- ALL records (-d option)
 *	ListHostsByType prints records of the default type or of a speicific
 *	type.
 *
 *	To see all types of information sorted by name, do the following:
 *	  ls -d domain.edu > file
 *	  view file
 *
 *  Results:
 *	SUCCESS		the listing was successful.
 *	ERROR		the server could not be contacted because
 *			a socket could not be obtained or an error
 *			occured while receiving, or the output file
 *			could not be opened.
 *
 *******************************************************************************
 */

void
ListHostsByType(char *string, int putToFile) {
	char *namePtr, name[NAME_LEN], option[NAME_LEN];
	int i, qtype, result;

	/*
	 * Parse the command line. It maybe of the form "ls -t domain"
	 * or "ls -t type domain".
	 */

	i = sscanf(string, " ls -t %s %s", option, name);
	if (putToFile && i == 2 && name[0] == '>')
		i--;
	if (i == 2) {
		qtype = StringToType(option, -1, stderr);
		if (qtype == -1)
			return;
		namePtr = name;
	} else if (i == 1) {
		namePtr = option;
		qtype = queryType;
	} else {
		fprintf(stderr, "*** ls: invalid request %s\n", string);
		return;
	}
	result = ListSubr(qtype, namePtr, putToFile ? string : NULL);
	if (result != SUCCESS)
		fprintf(stderr, "*** Can't list domain %s: %s\n", 
			namePtr, DecodeError(result));
}

void
ListHosts(char *string, int putToFile) {
	char *namePtr, name[NAME_LEN], option[NAME_LEN];
	int i, qtype, result;

	/*
	 *  Parse the command line. It maybe of the form "ls domain",
	 *  "ls -X domain".
	 */
	i = sscanf(string, " ls %s %s", option, name);
	if (putToFile && i == 2 && name[0] == '>')
		i--;
	if (i == 2) {
		if (strcmp("-a", option) == 0)
			qtype = T_CNAME;
		else if (strcmp("-h", option) == 0)
			qtype = T_HINFO;
		else if (strcmp("-m", option) == 0)
			qtype = T_MX;
		else if (strcmp("-p", option) == 0)
			qtype = T_PX;
		else if (strcmp("-s", option) == 0)
			qtype = T_WKS;
		else if (strcmp("-d", option) == 0)
			qtype = T_ANY;
		else if (strcmp("-n", option) == 0)
			qtype = T_NAPTR;
		else
			qtype = T_A;
		namePtr = name;
	} else if (i == 1) {
		namePtr = option;
		qtype = T_A;
	} else {
		fprintf(stderr, "*** ls: invalid request %s\n",string);
		return;
	}
	result = ListSubr(qtype, namePtr, putToFile ? string : NULL);
	if (result != SUCCESS)
		fprintf(stderr, "*** Can't list domain %s: %s\n", 
			namePtr, DecodeError(result));
}

int
ListSubr(int qtype, char *domain, char *cmd) {
	static u_char *answer = NULL;
	static int answerLen = 0;

	ns_msg handle;
	querybuf buf;
	struct sockaddr_in sin;
	HEADER *headerPtr;
	int msglen, amtToRead, numRead, n, count, soacnt;
	u_int len;
	int numAnswers = 0;
	int numRecords = 0;
	u_char tmp[INT16SZ], *cp;
	char soaname[2][NAME_LEN], file[NAME_LEN];
	enum { NO_ERRORS, ERR_READING_LEN, ERR_READING_MSG, ERR_PRINTING }
		error = NO_ERRORS;

	/*
	 * Create a query packet for the requested domain name.
	 */
	msglen = res_mkquery(QUERY, domain, queryClass, T_AXFR,
			     NULL, 0, 0, buf.qb2, sizeof buf);
	if (msglen < 0) {
		if (_res.options & RES_DEBUG)
			fprintf(stderr, "*** ls: res_mkquery failed\n");
		return (ERROR);
	}

	memset(&sin, 0, sizeof sin);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(nsport);

	/*
	 * Check to see if we have the address of the server or the
	 * address of a server who knows about this domain.
	 *
	 * For now, just use the first address in the list. XXX.
	 */

	if (defaultPtr->addrList != NULL)
		sin.sin_addr = *(struct in_addr *) defaultPtr->addrList[0];
	else
		sin.sin_addr = *(struct in_addr *)
			defaultPtr->servers[0]->addrList[0];

	/*
	 *  Set up a virtual circuit to the server.
	 */
	sockFD = socket(AF_INET, SOCK_STREAM, 0);
	if (sockFD < 0) {
		perror("ls: socket");
		return (ERROR);
	}
	if (connect(sockFD, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		int e;

		if (errno == ECONNREFUSED)
			e = NO_RESPONSE;
		else {
			perror("ls: connect");
			e = ERROR;
		}
		(void) close(sockFD);
		sockFD = -1;
		return (e);
	}

	/*
	 * Send length & message for zone transfer
	 */
	ns_put16(msglen, tmp);
        if (write(sockFD, (char *)tmp, INT16SZ) != INT16SZ ||
            write(sockFD, (char *)buf.qb2, msglen) != msglen) {
		perror("ls: write");
		(void) close(sockFD);
		sockFD = -1;
		return(ERROR);
	}

	fprintf(stdout,"[%s]\n", (defaultPtr->addrList != NULL)
		? defaultPtr->name : defaultPtr->servers[0]->name);

	if (cmd == NULL) {
		filePtr = stdout;
	} else {
		filePtr = OpenFile(cmd, file);
		if (filePtr == NULL) {
			fprintf(stderr, "*** Can't open %s for writing\n",
				file);
			(void) close(sockFD);
			sockFD = -1;
			return (ERROR);
		}
		fprintf(filePtr, "> %s\n", cmd);
		fprintf(filePtr, "[%s]\n", (defaultPtr->addrList != NULL)
			? defaultPtr->name : defaultPtr->servers[0]->name);
	}

	soacnt = 0;
	while (soacnt < 2) {
		/*
		 * Read the length of the response.
		 */

		cp = tmp;  amtToRead = INT16SZ;
		while (amtToRead > 0 &&
		       (numRead = read(sockFD, cp, amtToRead)) > 0) {
			cp += numRead;
			amtToRead -= numRead;
		}
		if (numRead <= 0) {
			error = ERR_READING_LEN;
			break;
		}

		len = ns_get16(tmp);
		if (len == 0)
			break;	/* nothing left to read */

		/*
		 * If the server sent too much data to fit the existing
		 * buffer, allocate a new one.
		 */
		if (len > (u_int)answerLen) {
			if (answerLen != 0)
				free(answer);
			answerLen = len;
			answer = (u_char *)Malloc(answerLen);
		}

		/*
		 * Read the response.
		 */
		amtToRead = len;  cp = answer;
		while (amtToRead > 0 &&
		       (numRead = read(sockFD, cp, amtToRead)) > 0) {
			cp += numRead;
			amtToRead -= numRead;
		}
		if (numRead <= 0) {
			error = ERR_READING_MSG;
			break;
		}

		if (ns_initparse(answer, cp - answer, &handle) < 0) {
			perror("ns_initparse");
			error = ERR_PRINTING;
			break;
		}
		if (ns_msg_getflag(handle, ns_f_rcode) != ns_r_noerror ||
		    ns_msg_count(handle, ns_s_an) == 0) {
			/* Signalled protocol error, or empty message. */
			error = ERR_PRINTING;
			break;
		}

		for (;;) {
			static char origin[NS_MAXDNAME], name_ctx[NS_MAXDNAME];
			const char *name;
			char buf[2048];   /* XXX need to malloc/realloc. */
			ns_rr rr;

			if (ns_parserr(&handle, ns_s_an, -1, &rr)) {
				if (errno != ENODEV) {
					perror("ns_parserr");
					error = ERR_PRINTING;
				}
				break;
			}
			name = ns_rr_name(rr);
			if (origin[0] == '\0' && name[0] != '\0') {
				fprintf(filePtr, "$ORIGIN %s.\n", name);
				strcpy(origin, name);
			}
			if (qtype == T_ANY || ns_rr_type(rr) == qtype) {
				if (ns_sprintrr(&handle, &rr, name_ctx, origin,
						buf, sizeof buf) < 0) {
					perror("ns_sprintrr");
					error = ERR_PRINTING;
					break;
				}
				strcpy(name_ctx, name);
				numAnswers++;
				fputs(buf, filePtr);
				fputc('\n', filePtr);
			}
			if (ns_rr_type(rr) == T_SOA) {
				strcpy(soaname[soacnt], name);
				if (soacnt == 0)
					soacnt = 1;
				else if (strcasecmp(soaname[0],
						    soaname[1]) == 0) {
					soacnt = 2;
				}
			}
		}
		if (error != NO_ERRORS)
			break;
		numAnswers++;
		if (cmd != NULL && ((numAnswers % HASH_SIZE) == 0)) {
			fprintf(stdout, "#");
			fflush(stdout);
		}
	}

	if (cmd != NULL)
		fprintf(stdout, "%sReceived %d answer%s (%d record%s).\n",
			(numAnswers >= HASH_SIZE) ? "\n" : "",
			numAnswers, (numAnswers != 1) ? "s" : "",
			numRecords, (numRecords != 1) ? "s" : "");

	(void) close(sockFD);
	sockFD = -1;
	if (cmd != NULL && filePtr != NULL) {
		fclose(filePtr);
		filePtr = NULL;
	}

	switch (error) {
	case NO_ERRORS:
		return (SUCCESS);

	case ERR_READING_LEN:
		return (ERROR);

	case ERR_PRINTING:
		return (ERROR);

	case ERR_READING_MSG:
		headerPtr = (HEADER *) answer;
		fprintf(stderr,"*** ls: error receiving zone transfer:\n");
		fprintf(stderr,
	       "  result: %s, answers = %d, authority = %d, additional = %d\n",
			_res_resultcodes[headerPtr->rcode],
			ntohs(headerPtr->ancount), ntohs(headerPtr->nscount),
			ntohs(headerPtr->arcount));
		return (ERROR);
	default:
		return (ERROR);
	}
}

/*
 *******************************************************************************
 *
 *  ViewList --
 *
 *	A hack to view the output of the ls command in sorted
 *	order using more.
 *
 *******************************************************************************
 */

ViewList(string)
    char *string;
{
    char file[PATH_MAX];
    char command[PATH_MAX];

    sscanf(string, " view %s", file);
    (void)sprintf(command, "grep \"^ \" %s | sort | %s", file, pager);
    system(command);
}

/*
 *******************************************************************************
 *
 *   Finger --
 *
 *	Connects with the finger server for the current host
 *	to request info on the specified person (long form)
 *	who is on the system (short form).
 *
 *  Results:
 *	SUCCESS		the finger server was contacted.
 *	ERROR		the server could not be contacted because
 *			a socket could not be obtained or connected
 *			to or the service could not be found.
 *
 *******************************************************************************
 */

Finger(string, putToFile)
    char *string;
    int  putToFile;
{
	struct servent		*sp;
	struct sockaddr_in	sin;
	FILE		*f;
	int		c;
	int		lastc;
	char			name[NAME_LEN];
	char			file[NAME_LEN];

	/*
	 *  We need a valid current host info to get an inet address.
	 */
	if (!curHostValid) {
	    fprintf(stderr, "Finger: no current host defined.\n");
	    return (ERROR);
	}

	if (sscanf(string, " finger %s", name) == 1) {
	    if (putToFile && (name[0] == '>')) {
		name[0] = '\0';
	    }
	} else {
	    name[0] = '\0';
	}

	sp = getservbyname("finger", "tcp");
	if (sp == 0) {
	    fprintf(stderr, "Finger: unknown service\n");
	    return (ERROR);
	}

	memset(&sin, 0, sizeof sin);
	sin.sin_family	= curHostInfo.addrType;
	sin.sin_port	= sp->s_port;
	memcpy(&sin.sin_addr, curHostInfo.addrList[0], curHostInfo.addrLen);

	/*
	 *  Set up a virtual circuit to the host.
	 */

	sockFD = socket(curHostInfo.addrType, SOCK_STREAM, 0);
	if (sockFD < 0) {
	    fflush(stdout);
	    perror("finger: socket");
	    return (ERROR);
	}

	if (connect(sockFD, (struct sockaddr *)&sin, sizeof (sin)) < 0) {
	    fflush(stdout);
	    perror("finger: connect");
	    close(sockFD);
	    sockFD = -1;
	    return (ERROR);
	}

	if (!putToFile) {
	    filePtr = stdout;
	} else {
	    filePtr = OpenFile(string, file);
	    if (filePtr == NULL) {
		fprintf(stderr, "*** Can't open %s for writing\n", file);
		close(sockFD);
		sockFD = -1;
		return(ERROR);
	    }
	    fprintf(filePtr,"> %s\n", string);
	}
	fprintf(filePtr, "[%s]\n", curHostInfo.name);

	if (name[0] != '\0') {
	    write(sockFD, "/W ", 3);
	}
	write(sockFD, name, strlen(name));
	write(sockFD, "\r\n", 2);
	f = fdopen(sockFD, "r");
	lastc = '\n';
	while ((c = getc(f)) != EOF) {
	    switch (c) {
		case 0210:
		case 0211:
		case 0212:
		case 0214:
			c -= 0200;
			break;
		case 0215:
			c = '\n';
			break;
	    }
	    putc(lastc = c, filePtr);
	}
	if (lastc != '\n') {
	    putc('\n', filePtr);
	}
	putc('\n', filePtr);

	close(sockFD);
	sockFD = -1;

	if (putToFile) {
	    fclose(filePtr);
	    filePtr = NULL;
	}
	return (SUCCESS);
}

ListHost_close()
{
    if (sockFD != -1) {
	(void) close(sockFD);
	sockFD = -1;
    }
}
