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

/*
 *	@(#)res.h	5.10 (Berkeley) 6/1/90
 *	$Id: res.h,v 8.9 2001/06/20 12:30:34 marka Exp $
 */

/*
 *******************************************************************************
 *
 *  res.h --
 *
 *	Definitions used by modules of the name server lookup program.
 *
 *	Copyright (c) 1985
 *	Andrew Cherenson
 *	U.C. Berkeley
 *	CS298-26  Fall 1985
 * 
 *******************************************************************************
 */

#define TRUE	1
#define FALSE	0
typedef int Boolean;

#define MAXALIASES	35
#define MAXADDRS	35
#define MAXDOMAINS	35
#define MAXSERVERS	10

/*
 *  Define return statuses in addtion to the ones defined in namserv.h
 *   let SUCCESS be a synonym for NOERROR
 *
 *	TIME_OUT	- a socket connection timed out.
 *	NO_INFO		- the server didn't find any info about the host.
 *	ERROR		- one of the following types of errors:
 *			   dn_expand, res_mkquery failed
 *			   bad command line, socket operation failed, etc.
 *	NONAUTH		- the server didn't have the desired info but
 *			  returned the name(s) of some servers who should.
 *	NO_RESPONSE	- the server didn't respond.
 *
 */

#ifdef ERROR
#undef ERROR
#endif

#define  SUCCESS		0
#define  TIME_OUT		-1
#define  NO_INFO		-2
#define  ERROR			-3
#define  NONAUTH		-4
#define  NO_RESPONSE		-5

/*
 *  Define additional options for the resolver state structure.
 *
 *   RES_DEBUG2		more verbose debug level
 */

#define RES_DEBUG2	0x80000000

/*
 *  Maximum length of server, host and file names.
 */

#define NAME_LEN 256


/*
 * Modified struct hostent from <netdb.h>
 *
 * "Structures returned by network data base library.  All addresses
 * are supplied in host order, and returned in network order (suitable
 * for use in system calls)."
 */

typedef struct	{
	char	*name;		/* official name of host */
	char	**domains;	/* domains it serves */
	char	**addrList;	/* list of addresses from name server */
} ServerInfo;

typedef struct	{
	char	*name;		/* official name of host */
	char	**aliases;	/* alias list */
	char	**addrList;	/* list of addresses from name server */
	int	addrType;	/* host address type */
	int	addrLen;	/* length of address */
	ServerInfo **servers;
} HostInfo;


/*
 *  FilePtr is used for directing listings to a file.
 *  It is global so the Control-C handler can close it.
 */

extern FILE *filePtr;

/*
 * TCP/UDP port of server.
 */
extern unsigned short nsport;

/*
 * Our resolver context.
 */
extern struct __res_state res;

/*
 *  External routines:
 */

/* XXX need prototypes */
void Print_query(const u_char *msg, const u_char *eom, int printHeader);
void Fprint_query(const u_char *msg, const u_char *eom, int printHeader,
		  FILE *file);
const u_char *Print_cdname(const u_char *cp, const u_char *msg,
			   const u_char *eom, FILE *file);
const u_char *Print_cdname2(const u_char *cp, const u_char *msg,
			    const u_char *eom, FILE *file);
const u_char *Print_rr(const u_char *ocp, const u_char *msg,
		       const u_char *eom, FILE *file);
extern const char *DecodeType();	/* descriptive version of p_type */
extern const char *DecodeError();
extern char *Calloc();
extern char *Malloc();
extern void NsError();
extern void PrintServer();
extern void PrintHostInfo();
extern void FreeHostInfoPtr();
extern FILE *OpenFile();
extern int pickString(const char *, char *, size_t);
extern int GetHostInfoByName(struct in_addr *, int, int, const char *,
			     HostInfo *, Boolean);
extern int GetHostInfoByAddr();
extern int GetHostDomain(struct in_addr *, int, int, const char *, char *,
		         HostInfo *, Boolean);
extern int matchString(const char *, const char *);
extern int StringToType(char *, int, FILE *);
extern int StringToClass(char *, int, FILE *);
extern int SendRequest(struct in_addr *, const u_char *, int,
		       u_char *, u_int, int *);
extern void SendRequest_close(void);
extern int SetDefaultServer(char *, Boolean);
extern int Finger(char *, int);
void ListHostsByType(char *, int);
void ListHosts(char *, int);
void ListHost_close(void);
int SetOption(char *);
int LookupHost(char *, Boolean);
int LookupHostWithServer(char *, Boolean);
const char * DecodeType(int);
const char * DecodeError(int);
FILE * OpenFile(char *, char *, size_t);
void  PrintHostInfo(FILE *, const char *, HostInfo *);
char * Calloc(int, int);
char * Malloc(int);
SIG_FN IntrHandler(int);
int ListSubr(int, char *, char *);
void FreeHostInfoPtr(HostInfo *);
unsigned char * res_skip(unsigned char *, int, unsigned char *);
extern Boolean IsAddr(const char *, struct in_addr *);
void PrintHelp(void);
int GetHostInfoByAddr(struct in_addr *, struct in_addr *, HostInfo *);

