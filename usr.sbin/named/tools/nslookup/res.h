/*
 * Copyright (c) 1985,1989 Regents of the University of California.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *
 *	@(#)res.h	5.10 (Berkeley) 6/1/90
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
 *  External routines:
 */

extern Boolean IsAddr();
extern int  Print_query();
extern char *Print_cdname();
extern char *Print_cdname2();	/* fixed width */
extern char *Print_rr();
extern char *DecodeType();	/* descriptive version of p_type */
extern char *DecodeError();
extern char *Calloc();
extern char *Malloc();
extern void NsError();
extern void PrintServer();
extern void PrintHostInfo();
extern void ShowOptions();
extern void FreeHostInfoPtr();
extern FILE *OpenFile();
extern char *res_skip();
