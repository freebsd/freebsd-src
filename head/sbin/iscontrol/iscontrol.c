/*-
 * Copyright (c) 2005-2010 Daniel Braniss <danny@cs.huji.ac.il>
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
 */
/*
 | $Id: iscontrol.c,v 2.2 2006/12/01 09:11:56 danny Exp danny $
 */
/*
 | the user level initiator (client)
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <camlib.h>

#include <dev/iscsi/initiator/iscsi.h>
#include "iscontrol.h"

#define USAGE "[-v] [-d] [-c config] [-n name] [-t target] [-p pidfile]"
#define OPTIONS	"vdc:t:n:p:"

token_t AuthMethods[] = {
     {"None",	NONE},
     {"KRB5",	KRB5},
     {"SPKM1",	SPKM1},
     {"SPKM2",	SPKM2},
     {"SRP",	SRP},
     {"CHAP",	CHAP},
     {0, 0}
};

token_t	DigestMethods[] = {
     {"None",	0},
     {"CRC32",	1},
     {"CRC32C",	1},
     {0, 0}
};

u_char	isid[6 + 6];
/*
 | Default values
 */
isc_opt_t opvals = {
     .port			= 3260,
     .sockbufsize		= 128,
     .iqn			= "iqn.2005-01.il.ac.huji.cs:",

     .sessionType		= "Normal",
     .targetAddress		= 0,
     .targetName		= 0,
     .initiatorName		= 0,
     .authMethod		= "None",
     .headerDigest		= "None,CRC32C",
     .dataDigest		= "None,CRC32C",
     .maxConnections		= 1,
     .maxRecvDataSegmentLength	= 64 * 1024,
     .maxXmitDataSegmentLength	= 8 * 1024, // 64 * 1024,
     .maxBurstLength		= 128 * 1024,
     .firstBurstLength		= 64 * 1024, // must be less than maxBurstLength
     .defaultTime2Wait		= 0,
     .defaultTime2Retain	= 0,
     .maxOutstandingR2T		= 1,
     .errorRecoveryLevel	= 0,

     .dataPDUInOrder		= TRUE,
     .dataSequenceInOrder	= TRUE,

     .initialR2T		= TRUE,
     .immediateData		= TRUE,
};

int
lookup(token_t *tbl, char *m)
{
     token_t	*tp;

     for(tp = tbl; tp->name != NULL; tp++)
	  if(strcasecmp(tp->name, m) == 0)
	       return tp->val;
     return 0;
}

int
main(int cc, char **vv)
{
     int	ch, disco;
     char	*pname, *pidfile, *p, *q, *ta, *kw;
     isc_opt_t	*op;
     FILE	*fd;

     op = &opvals;
     iscsidev = "/dev/"ISCSIDEV;
     fd = NULL;
     pname = vv[0];
     if((p = strrchr(pname, '/')) != NULL)
	  pname = p + 1;

     kw = ta = 0;
     disco = 0;
     pidfile = NULL;

     while((ch = getopt(cc, vv, OPTIONS)) != -1) {
	  switch(ch) {
	  case 'v':
	       vflag++;
	       break;
	  case 'c':
	       fd = fopen(optarg, "r");
	       if(fd == NULL) {
		    perror(optarg);
		    exit(1);
	       }
	       break;
	  case 'd':
	       disco = 1;
	       break;
	  case 't':
	       ta = optarg;
	       break;
	  case 'n':
	       kw = optarg;
	       break;
	  case 'p':
	       pidfile = optarg;
	       break;
	  default:
	  badu:
	       fprintf(stderr, "Usage: %s %s\n", pname, USAGE);
	       exit(1);
	  }
     }
     if(fd == NULL)
	  fd = fopen("/etc/iscsi.conf", "r");

     if(fd != NULL) {
	  parseConfig(fd, kw, op);
	  fclose(fd);
     }
     cc -= optind;
     vv += optind;
     if(cc > 0) {
	  if(vflag)
	       printf("adding '%s'\n", *vv);
	  parseArgs(cc, vv, op);
     }
     if(ta)
	  op->targetAddress = ta;

     if(op->targetAddress == NULL) {
	  fprintf(stderr, "No target!\n");
	  goto badu;
     }
     q = op->targetAddress;
     if(*q == '[' && (q = strchr(q, ']')) != NULL) {
	  *q++ = '\0';
	  op->targetAddress++;
     } else
	  q = op->targetAddress;
     if((p = strchr(q, ':')) != NULL) {
	  *p++ = 0;
	  op->port = atoi(p);
	  p = strchr(p, ',');
     }
     if(p || ((p = strchr(q, ',')) != NULL)) {
	  *p++ = 0;
	  op->targetPortalGroupTag = atoi(p);
     }
     if(op->initiatorName == 0) {
	  char	hostname[256];

	  if(op->iqn) {
	       if(gethostname(hostname, sizeof(hostname)) == 0)
		    asprintf(&op->initiatorName, "%s:%s", op->iqn, hostname);
	       else
		    asprintf(&op->initiatorName, "%s:%d", op->iqn, (int)time(0) & 0xff); // XXX:
	  }
	  else {
	       if(gethostname(hostname, sizeof(hostname)) == 0)
		    asprintf(&op->initiatorName, "%s", hostname);
	       else
		    asprintf(&op->initiatorName, "%d", (int)time(0) & 0xff); // XXX:
	  }
     }
     if(disco) {
	  op->sessionType = "Discovery";
	  op->targetName = 0;
     }
     op->pidfile = pidfile;
     fsm(op);

     exit(0);
}
