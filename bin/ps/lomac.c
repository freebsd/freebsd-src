/*-
 * Copyright (c) 2001 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by NAI Labs, the
 * Security Research Division of Network Associates, Inc. under
 * DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA
 * CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 * $Id: lomac.c,v 1.3 2001/11/26 21:04:04 bfeldman Exp $
 */

/*
 * This file encapsulates ls's use of LOMAC's ioctl interface.  ls uses
 * this interface to determine the LOMAC attributes of files.
 */

#include <sys/cdefs.h>

__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/lomacio.h>

#include <err.h>
#include <fts.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "lomac.h"

#define LOMAC_DEVICE "/dev/lomac"

static int devlomac = -1;		/* file descriptor for LOMAC_DEVICE */

/* lomac_start()
 *
 * in:     nothing
 * out:    nothing
 * return: nothing
 *
 * Makes `devlomac' a fd to LOMAC_DEVICE
 */

static void 
lomac_start(void)
{
	if ((devlomac = open(LOMAC_DEVICE, O_RDWR)) == -1)
		err(1, "cannot open %s", LOMAC_DEVICE);
}

/* lomac_stop()
 *
 * in:     nothing
 * out:    nothing
 * return: nothing
 *
 * Closes `devlomac', the fd to LOMAC_DEVICE.
 */

void 
lomac_stop(void)
{

	if (devlomac != -1 && close(devlomac) == -1)
		err(1, "cannot close %s", LOMAC_DEVICE);
}

/* get_lattr()
 *
 * in:     pid - pid of process whose level we want to know
 * out:    nothing
 * return: level of proces `pid'
 *
 * This function uses LOMAC's ioctl interface to determine the LOMAC
 * attributes of the process with pid `pid'.
 *
 * This function presently reports only levels.  When LOMAC's ioctl
 * interface is expanded to report levels and flags, this function
 * will also need expansion.
 */

int
get_lattr(int pid)
{

	if (devlomac == -1)
		lomac_start();
	if (ioctl(devlomac, LIOGETPLEVEL, &pid) == -1)
		err(1, NULL);
	return (pid);
}
