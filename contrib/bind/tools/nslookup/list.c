/*
 * ++Copyright++ 1985, 1989
 * -
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
 * -
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
 * -
 * --Copyright--
 */

#ifndef lint
static char sccsid[] = "@(#)list.c	5.23 (Berkeley) 3/21/91";
static char rcsid[] = "$Id: list.c,v 8.5 1996/05/21 07:04:38 vixie Exp $";
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

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <netdb.h>
#include <stdio.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include "res.h"
#include "../../conf/portability.h"

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
ListHostsByType(string, putToFile)
	char *string;
	int putToFile;
{
	int	i, qtype, result;
	char	*namePtr;
	char	name[NAME_LEN];
	char	option[NAME_LEN];

	/*
	 *  Parse the command line. It maybe of the form "ls -t domain"
	 *  or "ls -t type domain".
	 */

	i = sscanf(string, " ls -t %s %s", option, name);
	if (putToFile && i == 2 && name[0] == '>') {
	    i--;
	}
	if (i == 2) {
	    qtype = StringToType(option, -1, stderr);
	    if (qtype == -1)
		return;
	    namePtr = name;
	} else if (i == 1) {
	    namePtr = option;
	    qtype = queryType;
	} else {
	    fprintf(stderr, "*** ls: invalid request %s\n",string);
	    return;
	}
	result = ListSubr(qtype, namePtr, putToFile ? string : NULL);
	if (result != SUCCESS)
	    fprintf(stderr, "*** Can't list domain %s: %s\n", 
			namePtr, DecodeError(result));
}

void
ListHosts(string, putToFile)
	char *string;
	int  putToFile;
{
	int	i, qtype, result;
	char	*namePtr;
	char	name[NAME_LEN];
	char	option[NAME_LEN];

	/*
	 *  Parse the command line. It maybe of the form "ls domain",
	 *  "ls -X domain".
	 */
	i = sscanf(string, " ls %s %s", option, name);
	if (putToFile && i == 2 && name[0] == '>') {
	    i--;
	}
	if (i == 2) {
	    if (strcmp("-a", option) == 0) {
		qtype = T_CNAME;
	    } else if (strcmp("-h", option) == 0) {
		qtype = T_HINFO;
	    } else if (strcmp("-m", option) == 0) {
		qtype = T_MX;
	    } else if (strcmp("-p", option) == 0) {
		qtype = T_PX;
	    } else if (strcmp("-s", option) == 0) {
		qtype = T_WKS;
	    } else if (strcmp("-d", option) == 0) {
		qtype = T_ANY;
	    } else {
		qtype = T_A;
	    }
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
ListSubr(qtype, domain, cmd)
	int qtype;
	char *domain;
	char *cmd;
{
	querybuf		buf;
	struct sockaddr_in	sin;
	HEADER			*headerPtr;
	int			msglen;
	int			amtToRead;
	int			numRead, n;
	int			numAnswers = 0;
	int			numRecords = 0;
	int			result;
	int			soacnt = 0;
	int			count, done;
	u_short			len;
	u_char			*cp;
	char			dname[2][NAME_LEN];
	char			file[NAME_LEN];
	static u_char		*answer = NULL;
	static int		answerLen = 0;
	enum {
	    NO_ERRORS,
	    ERR_READING_LEN,
	    ERR_READING_MSG,
	    ERR_PRINTING
	} error = NO_ERRORS;

	/*
	 *  Create a query packet for the requested domain name.
	 */
	msglen = res_mkquery(QUERY, domain, queryClass, T_AXFR,
			     NULL, 0, 0, buf.qb2, sizeof buf);
	if (msglen < 0) {
	    if (_res.options & RES_DEBUG) {
		fprintf(stderr, "*** ls: res_mkquery failed\n");
	    }
	    return (ERROR);
	}

	bzero((char *)&sin, sizeof(sin));
	sin.sin_family	= AF_INET;
	sin.sin_port	= htons(nsport);

	/*
	 *  Check to see if we have the address of the server or the
	 *  address of a server who knows about this domain.
	 *
	 *  For now, just use the first address in the list.
	 */

	if (defaultPtr->addrList != NULL) {
	  sin.sin_addr = *(struct in_addr *) defaultPtr->addrList[0];
	} else {
	  sin.sin_addr = *(struct in_addr *)defaultPtr->servers[0]->addrList[0];
	}

	/*
	 *  Set up a virtual circuit to the server.
	 */
	if ((sockFD = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	    perror("ls: socket");
	    return(ERROR);
	}
	if (connect(sockFD, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
	    int e;
	    if (errno == ECONNREFUSED) {
		e = NO_RESPONSE;
	    } else {
		perror("ls: connect");
		e = ERROR;
	    }
	    (void) close(sockFD);
	    sockFD = -1;
	    return e;
	}

	/*
	 * Send length & message for zone transfer
	 */

	__putshort(msglen, (u_char *)&len);

        if (write(sockFD, (char *)&len, INT16SZ) != INT16SZ ||
            write(sockFD, (char *) &buf, msglen) != msglen) {
		perror("ls: write");
		(void) close(sockFD);
		sockFD = -1;
		return(ERROR);
	}

	fprintf(stdout,"[%s]\n",
		(defaultPtr->addrList != NULL) ? defaultPtr->name :
		 defaultPtr->servers[0]->name);

	if (cmd == NULL) {
	    filePtr = stdout;
	} else {
	    filePtr = OpenFile(cmd, file);
            if (filePtr == NULL) {
                fprintf(stderr, "*** Can't open %s for writing\n", file);
		(void) close(sockFD);
		sockFD = -1;
                return(ERROR);
            }
	    fprintf(filePtr, "> %s\n", cmd);
	    fprintf(filePtr,"[%s]\n",
		(defaultPtr->addrList != NULL) ? defaultPtr->name :
		 defaultPtr->servers[0]->name);
	}

#if 0
	if (qtype == T_CNAME) {
	    fprintf(filePtr, "%-30s", "Alias");
	} else if (qtype == T_TXT) {
	    fprintf(filePtr, "%-30s", "Key");
	} else {
	    fprintf(filePtr, "%-30s", "Host or domain name");
	}
	switch (qtype) {
	    case T_A:
		    fprintf(filePtr, " %-30s\n", "Internet Address");
		    break;
	    case T_AAAA:
		    fprintf(filePtr, " %-30s\n", "IPv6 Address");
		    break;
	    case T_HINFO:
		    fprintf(filePtr, " %-30s\n", "CPU & OS");
		    break;
	    case T_CNAME:
		    fprintf(filePtr, " %-30s\n", "Canonical Name");
		    break;
	    case T_MX:
		    fprintf(filePtr, " %-30s\n", "Metric & Host");
		    break;
	    case T_PX:
		    fprintf(filePtr, " %-30s\n", "Mapping information");
		    break;
	    case T_AFSDB:
		    fprintf(filePtr, " %-30s\n", "Subtype & Host");
		    break;
	    case T_X25:
		    fprintf(filePtr, " %-30s\n", "X25 Address");
		    break;
	    case T_ISDN:
		    fprintf(filePtr, " %-30s\n", "ISDN Address");
		    break;
	    case T_WKS:
		    fprintf(filePtr, " %-4s %s\n", "Protocol", "Services");
		    break;
	    case T_MB:
		    fprintf(filePtr, " %-30s\n", "Mailbox");
		    break;
	    case T_MG:
		    fprintf(filePtr, " %-30s\n", "Mail Group");
		    break;
	    case T_MR:
		    fprintf(filePtr, " %-30s\n", "Mail Rename");
		    break;
	    case T_MINFO:
		    fprintf(filePtr, " %-30s\n", "Mail List Requests & Errors");
		    break;
	    case T_UINFO:
		    fprintf(filePtr, " %-30s\n", "User Information");
		    break;
	    case T_UID:
		    fprintf(filePtr, " %-30s\n", "User ID");
		    break;
	    case T_GID:
		    fprintf(filePtr, " %-30s\n", "Group ID");
		    break;
	    case T_TXT:
		    fprintf(filePtr, " %-30s\n", "Text");
		    break;
	    case T_RP:
		    fprintf(filePtr, " %-30s\n", "Responsible Person");
		    break;
	    case T_RT:
		    fprintf(filePtr, " %-30s\n", "Router");
		    break;
	    case T_NSAP:
		    fprintf(filePtr, " %-30s\n", "NSAP address");
		    break;
	    case T_NSAP_PTR:
		    fprintf(filePtr, " %-30s\n", "NSAP pointer");
		    break;
	    case T_NS:
		    fprintf(filePtr, " %-30s\n", "Name Servers");
		    break;
	    case T_PTR:
		    fprintf(filePtr, " %-30s\n", "Pointers");
		    break;
	    case T_SOA:
		    fprintf(filePtr, " %-30s\n", "Start of Authority");
		    break;
	    case T_ANY:
		    fprintf(filePtr, " %-30s\n", "Resource Record Info.");
		    break;
	}
#endif


	dname[0][0] = '\0';
	for (done = 0; !done; NULL) {
	    unsigned short tmp;

	    /*
	     * Read the length of the response.
	     */

	    cp = (u_char *)&tmp;
	    amtToRead = INT16SZ;
	    while ((numRead = read(sockFD, cp, amtToRead)) > 0) {
		cp += numRead;
		if ((amtToRead -= numRead) <= 0)
			break;
	    }
	    if (numRead <= 0) {
		error = ERR_READING_LEN;
		break;
	    }

	    if ((len = _getshort((u_char*)&tmp)) == 0) {
		break;	/* nothing left to read */
	    }

	    /*
	     * The server sent too much data to fit the existing buffer --
	     * allocate a new one.
	     */
	    if (len > (u_int)answerLen) {
		if (answerLen != 0) {
		    free(answer);
		}
		answerLen = len;
		answer = (u_char *)Malloc(answerLen);
	    }

	    /*
	     * Read the response.
	     */

	    amtToRead = len;
	    cp = answer;
	    while (amtToRead > 0 && (numRead=read(sockFD, cp, amtToRead)) > 0) {
		cp += numRead;
		amtToRead -= numRead;
	    }
	    if (numRead <= 0) {
		error = ERR_READING_MSG;
		break;
	    }

	    result = PrintListInfo(filePtr, answer, cp, qtype, dname[0]);
	    if (result != SUCCESS) {
		error = ERR_PRINTING;
		break;
	    }
	    numRecords += htons(((HEADER *)answer)->ancount);
	    numAnswers++;
	    if (cmd != NULL && ((numAnswers % HASH_SIZE) == 0)) {
		fprintf(stdout, "#");
		fflush(stdout);
	    }
	    /* Header. */
	    cp = answer + HFIXEDSZ;
	    /* Question. */
	    for (count = ntohs(((HEADER* )answer)->qdcount);
		 count > 0;
		 count--)
		    cp += dn_skipname(cp, answer + len) + QFIXEDSZ;
	    /* Answer. */
	    for (count = ntohs(((HEADER* )answer)->ancount);
		 count > 0;
		 count--) {
		int type, class, rlen;

		n = dn_expand(answer, answer + len, cp,
			      dname[soacnt], sizeof dname[0]);
		if (n < 0) {
		    error = ERR_PRINTING;
		    done++;
		    break;
		}
		cp += n;
		GETSHORT(type, cp);
		GETSHORT(class, cp);
		cp += INT32SZ;	/* ttl */
		GETSHORT(rlen, cp);
		cp += rlen;
		if (type == T_SOA && soacnt++ &&
		    !strcasecmp(dname[0], dname[1])) {
		    done++;
		    break;
		}
	    }
	}

	if (cmd != NULL) {
	    fprintf(stdout, "%sReceived %d answer%s (%d record%s).\n",
		(numAnswers >= HASH_SIZE) ? "\n" : "",
		numAnswers, (numAnswers != 1) ? "s" : "",
		numRecords, (numRecords != 1) ? "s" : "");
	}

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
		return(ERROR);

	    case ERR_PRINTING:
		return(result);

	    case ERR_READING_MSG:
		headerPtr = (HEADER *) answer;
		fprintf(stderr,"*** ls: error receiving zone transfer:\n");
		fprintf(stderr,
	       "  result: %s, answers = %d, authority = %d, additional = %d\n",
			_res_resultcodes[headerPtr->rcode],
			ntohs(headerPtr->ancount), ntohs(headerPtr->nscount),
			ntohs(headerPtr->arcount));
		return(ERROR);
	    default:
		return(ERROR);
	}
}


/*
 *******************************************************************************
 *
 *  PrintListInfo --
 *
 *	Used by the ListInfo routine to print the answer
 *	received from the name server. Only the desired
 *	information is printed.
 *
 *  Results:
 *	SUCCESS		the answer was printed without a problem.
 *	NO_INFO		the answer packet did not contain an answer.
 *	ERROR		the answer was malformed.
 *      Misc. errors	returned in the packet header.
 *
 *******************************************************************************
 */

#define NAME_FORMAT " %-30s"

static Boolean
strip_domain(string, domain)
    char *string, *domain;
{
    register char *dot;

    if (*domain != '\0') {
	dot = string;
	while ((dot = strchr(dot, '.')) != NULL && strcasecmp(domain, ++dot))
		;
	if (dot != NULL) {
	    dot[-1] = '\0';
	    return TRUE;
	}
    }
    return FALSE;
}


PrintListInfo(file, msg, eom, qtype, domain)
    FILE	*file;
    u_char	*msg, *eom;
    int		qtype;
    char	*domain;
{
    register u_char	*cp;
    HEADER		*headerPtr;
    int			type, class, dlen, nameLen;
    u_int32_t		ttl;
    int			n, pref, count;
    struct in_addr	inaddr;
    char		name[NAME_LEN];
    char		name2[NAME_LEN];
    Boolean		stripped;

    /*
     * Read the header fields.
     */
    headerPtr = (HEADER *)msg;
    cp = msg + HFIXEDSZ;
    if (headerPtr->rcode != NOERROR) {
	return(headerPtr->rcode);
    }

    /*
     *  We are looking for info from answer resource records.
     *  If there aren't any, return with an error. We assume
     *  there aren't any question records.
     */

    if (ntohs(headerPtr->ancount) == 0) {
	return(NO_INFO);
    }
    for (n = ntohs(headerPtr->qdcount); n > 0; n--) {
	nameLen = dn_skipname(cp, eom);
	if (nameLen < 0)
	    return (ERROR);
	cp += nameLen + QFIXEDSZ;
    }
    for (count = ntohs(headerPtr->ancount); count > 0; count--) {
	nameLen = dn_expand(msg, eom, cp, name, sizeof name);
	if (nameLen < 0)
	    return (ERROR);
	cp += nameLen;

	type = _getshort((u_char*)cp);
	cp += INT16SZ;

	if (!(type == qtype || qtype == T_ANY) &&
	    !((type == T_NS || type == T_PTR) && qtype == T_A))
		return(SUCCESS);

	class = _getshort((u_char*)cp);
	cp += INT16SZ;
	ttl = _getlong((u_char*)cp);
	cp += INT32SZ;
	dlen = _getshort((u_char*)cp);
	cp += INT16SZ;

	if (name[0] == 0)
		strcpy(name, "(root)");

	/* Strip the domain name from the data, if desired. */
	stripped = FALSE;
	if ((_res.options & RES_DEBUG) == 0) {
	    if (type != T_SOA) {
		stripped = strip_domain(name, domain);
	    }
	}
	if (!stripped && nameLen < sizeof(name)-1) {
	    strcat(name, ".");
	}

	fprintf(file, NAME_FORMAT, name);

	if (qtype == T_ANY) {
	    if (_res.options & RES_DEBUG) {
		fprintf(file,"\t%lu %-5s", ttl, p_class(queryClass));
	    }
	    fprintf(file," %-5s", p_type(type));
	}

	/* XXX merge this into debug.c's print routines */

	switch (type) {
	    case T_A:
		if (class == C_IN) {
		    bcopy(cp, (char *)&inaddr, INADDRSZ);
		    if (dlen == 4) {
			fprintf(file," %s", inet_ntoa(inaddr));
		    } else if (dlen == 7) {
			fprintf(file," %s", inet_ntoa(inaddr));
			fprintf(file," (%d, %d)", cp[4],(cp[5] << 8) + cp[6]);
		    } else
			fprintf(file, " (dlen = %d?)", dlen);
		}
		cp += dlen;
		break;

	    case T_CNAME:
	    case T_MB:
	    case T_MG:
	    case T_MR:
		nameLen = dn_expand(msg, eom, cp, name2, sizeof name2);
		if (nameLen < 0) {
		    fprintf(file, " ***\n");
		    return (ERROR);
		}
		fprintf(file, " %s", name2);
		cp += nameLen;
		break;

	    case T_NS:
	    case T_PTR:
	    case T_NSAP_PTR:
		putc(' ', file);
		if (qtype != T_ANY)
		    fprintf(file,"%s = ", type == T_PTR ? "host" : "server");
		cp = (u_char *)Print_cdname2(cp, msg, eom, file);
		if (!cp) {
		    fprintf(file, " ***\n");
		    return (ERROR);
		}
		break;

	    case T_HINFO:
	    case T_ISDN:
		{
		    u_char *cp2 = cp + dlen;
		    if (n = *cp++) {
			(void)sprintf(name,"%.*s", n, cp);
			fprintf(file," %-10s", name);
			cp += n;
		    } else {
			fprintf(file," %-10s", " ");
		    }
		    if (cp == cp2)
			break;
		    if (n = *cp++) {
			fprintf(file,"  %.*s", n, cp);
			cp += n;
		    }
		}
		break;

	    case T_SOA:
		nameLen = dn_expand(msg, eom, cp, name2, sizeof name2);
		if (nameLen < 0) {
		    fprintf(file, " ***\n");
		    return (ERROR);
		}
		cp += nameLen;
		fprintf(file, " %s", name2);
		nameLen = dn_expand(msg, eom, cp, name2, sizeof name2);
		if (nameLen < 0) {
		    fprintf(file, " ***\n");
		    return (ERROR);
		}
		cp += nameLen;
		fprintf(file, " %s. (", name2);
		for (n = 0; n < 5; n++) {
		    u_int32_t u;

		    u = _getlong((u_char*)cp);
		    cp += INT32SZ;
		    fprintf(file,"%s%lu", n? " " : "", u);
		}
		fprintf(file, ")");
		break;

	    case T_MX:
	    case T_AFSDB:
		case T_RT:
		pref = _getshort((u_char*)cp);
		cp += INT16SZ;
		fprintf(file," %-3d ",pref);
		nameLen = dn_expand(msg, eom, cp, name2, sizeof name2);
		if (nameLen < 0) {
		    fprintf(file, " ***\n");
		    return (ERROR);
		}
		fprintf(file, " %s", name2);
		cp += nameLen;
		break;

	    case T_PX:
		pref = _getshort((u_char*)cp);
		cp += INT16SZ;
		fprintf(file," %-3d ",pref);
		nameLen = dn_expand(msg, eom, cp, name2, sizeof name2);
		if (nameLen < 0) {
			fprintf(file, " ***\n");
			return (ERROR);
		}
		fprintf(file, " %s", name2);
		cp += nameLen;
		nameLen = dn_expand(msg, eom, cp, name2, sizeof name2);
		if (nameLen < 0) {
		    fprintf(file, " ***\n");
		    return (ERROR);
		}
		fprintf(file, " %s", name2);
		cp += nameLen;
		break;

	    case T_TXT:
	    case T_X25:
		{
		    u_char *cp2 = cp + dlen;
		    int c;

		    (void) fputs(" \"", file);
		    while (cp < cp2)
			if (n = (unsigned char) *cp++)
			    for (c = n; c > 0 && cp < cp2; c--)
				if (strchr("\n\"\\", *cp)) {
				    (void) putc('\\', file);
				    (void) putc(*cp++, file);
				} else
				    (void) putc(*cp++, file);
		    (void) putc('"', file);
		}
		break;

	    case T_NSAP:
		fprintf(file, " %s", inet_nsap_ntoa(dlen, cp, NULL));
		cp += dlen;
		break;

	    case T_AAAA: {
		char t[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"];

		fprintf(file, " %s", inet_ntop(AF_INET6, cp, t, sizeof t));
		break;
	    }

	    case T_MINFO:
	    case T_RP:
		(void) putc(' ', file);
		cp = (u_char *)Print_cdname(cp, msg, eom, file);
		if (!cp) {
		    fprintf(file, " ***\n");
		    return (ERROR);
		}
		fprintf(file, "  ");
		cp = (u_char *)Print_cdname(cp, msg, eom, file);
		if (!cp) {
		    fprintf(file, " ***\n");
		    return (ERROR);
		}
		break;

	    case T_UINFO:
		fprintf(file, " %s", cp);
		cp += dlen;
		break;

	    case T_UID:
	    case T_GID:
		fprintf(file, " %lu", _getlong((u_char*)cp));
		cp += dlen;
		break;

	    case T_WKS:
		if (class == C_IN) {
		    struct protoent *pp;
		    struct servent *ss;
		    u_short port;

		    cp += 4; 	/* skip inet address */
		    dlen -= 4;

		    setprotoent(1);
		    setservent(1);
		    n = *cp & 0377;
		    pp = getprotobynumber(n);
		    if (pp == 0)
			fprintf(file," %-3d ", n);
		    else
			fprintf(file," %-3s ", pp->p_name);
		    cp++; dlen--;

		    port = 0;
		    while (dlen-- > 0) {
			n = *cp++;
			do {
			    if (n & 0200) {
				ss = getservbyport((int)htons(port),
					    pp->p_name);
				if (ss == 0)
				    fprintf(file," %u", port);
				else
				    fprintf(file," %s", ss->s_name);
			    }
				n <<= 1;
			} while (++port & 07);
		    }
		    endprotoent();
		    endservent();
		}
		break;
	}
	fprintf(file,"\n");
    }
    return(SUCCESS);
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
	register FILE		*f;
	register int		c;
	register int		lastc;
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

	bzero((char *)&sin, sizeof(sin));
	sin.sin_family	= curHostInfo.addrType;
	sin.sin_port	= sp->s_port;
	bcopy(curHostInfo.addrList[0], (char *)&sin.sin_addr,
		curHostInfo.addrLen);

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
