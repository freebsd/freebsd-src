/*
 * Copyright (c) 1998, 1999 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#include "kuser_locl.h"
#include <kafs.h>
#include <getarg.h>

RCSID("$Id: kdestroy.c,v 1.17 1999/12/02 16:58:36 joda Exp $");

#ifdef LEGACY_KDESTROY
int ticket_flag = 1;
int unlog_flag  = 0;
#else
int ticket_flag = -1;
int unlog_flag  = -1;
#endif
int quiet_flag;
int help_flag;
int version_flag;

struct getargs args[] = {
    { "quiet", 		'q',	arg_flag, 	&quiet_flag, 
      "don't print any messages" },
    { NULL, 		'f',	arg_flag, 	&quiet_flag },
    { "tickets",	't',	arg_flag,       &ticket_flag,
    "destroy tickets" },
    { "unlog",          'u',    arg_flag,       &unlog_flag,
      "destroy AFS tokens" },
    { "version", 	0,	arg_flag,	&version_flag },
    { "help",		'h',	arg_flag,	&help_flag }
};

int num_args = sizeof(args) / sizeof(args[0]);

static void
usage(int code)
{
    arg_printusage(args, num_args, NULL, "");
    exit(code);
}

int
main(int argc, char **argv)
{
    int optind = 0;
    int ret = RET_TKFIL;

    set_progname(argv[0]);
    if(getarg(args, num_args, argc, argv, &optind))
	usage(1);

    if(help_flag)
	usage(0);

    if(version_flag) {
	print_version(NULL);
	exit(0);
    }
    
    if (unlog_flag == -1 && ticket_flag == -1)
        unlog_flag = ticket_flag = 1;

    if (ticket_flag)
        ret = dest_tkt();

    if (unlog_flag && k_hasafs())
	k_unlog();

    if (!quiet_flag) {
	if (ret == KSUCCESS)
	    printf("Tickets destroyed.\n");
	else if (ret == RET_TKFIL)
	    printf("No tickets to destroy.\n");
	else {
	    printf("Tickets NOT destroyed.\n");
	}
    }

    if (ret == KSUCCESS || ret == RET_TKFIL)
	return 0;
    else
	return 1;
}
