f/*
 * Copyright (C) 2000 Benno Rice.
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
 * THIS SOFTWARE IS PROVIDED BY Benno Rice ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Disk I/O routines using Open Firmware
 */

#include <sys/param.h>
#include <sys/disklabel.h>
#include <sys/disklabel_mbr.h>

#include <netinet/in.h>

#include <stand.h>

#include "bootstrap.h"
#include "libofw.h"
#include "openfirm.h"

static int	ofwd_init(void);
static int	ofwd_strategy(void *devdata, int flag, daddr_t dblk, 
				size_t size, char *buf, size_t *rsize);
static int	ofwd_open(struct open_file *f, ...);
static int	ofwd_close(struct open_file *f);
static void	ofwd_print(int verbose);

struct devsw ofwdisk = {
	"disk",
	DEVT_DISK,
	ofwd_init,
	ofwd_strategy,
	ofwd_open,
	ofwd_close,
	noioctl,
	ofwd_print
};

static struct ofwdinfo {
	int	ofwd_unit;
	char	ofwd_path[255];
} ofwdinfo[MAXDEV];
static int nofwdinfo = 0;

static int
ofwd_init(void)
{
	int ret;
	char devpath[255];
	ihandle_t instance;

	ofw_devsearch_init();
	while((ret = ofw_devsearch("block", devpath)) != 0) {
		if (ret == -1)
			return (1);

		instance = OF_open(devpath);
		if (instance != -1) {
			ofwdinfo[nofwdinfo].ofwd_unit = nofwdinfo;
			strncpy(ofwdinfo[nofwdinfo].ofwd_path, devpath, 255);
			printf("disk%d is %s\n", nofwdinfo, devpath);
			nofwdinfo++;
			OF_close(instance);
		}

		if (nofwdinfo > MAXDEV) {
			printf("Hit MAXDEV probing disks.\n");
			return (1);
		}
	}

	return (0);
}

static int
ofwd_strategy(void *devdata, int flag, daddr_t dblk, size_t size, char *buf,
    size_t *rsize)
{
	return (0);
}

static int
ofwd_open(struct open_file *f, ...)
{
	return (0);
}

static int
ofwd_close(struct open_file *f)
{
	return (0);
}

static void
ofwd_print(int verbose)
{
	printf("ofwd_print called.\n");
	return;
}
