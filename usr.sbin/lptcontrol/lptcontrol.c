/*
 * Copyright (c) 1994 Geoffrey M. Rehmet
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
 *      This product includes software developed by Geoffrey M. Rehmet
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
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

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <ctype.h>
#include <err.h>
#include <limits.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <dev/ppbus/lptio.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/types.h>


#define PATH_LPCTL	_PATH_DEV "lpctl"
#define DEFAULT_DEVICE	_PATH_DEV "lpt0"
#define IRQ_INVALID	-1
#define DO_POLL		0
#define USE_IRQ		1
#define USE_EXT_MODE	2
#define USE_STD_MODE	3

static void usage(void)
{
	fprintf(stderr, "usage: lptcontrol -i | -p | -s | -e [-d device]\n");
	exit(1);
}

static void set_interrupt_status(int irq_status, const char * file)
{
	int	fd;

	if((fd = open(file, O_WRONLY, 0660)) < 0)
		err(1, "open");
	if(ioctl(fd, LPT_IRQ, &irq_status) < 0)
		err(1, "ioctl");
	close(fd);
}

int main (int argc, char * argv[])
{
	int		opt;
	int		irq_status = IRQ_INVALID;
	const char	*device = DEFAULT_DEVICE;

	while((opt = getopt(argc, argv, "ipesd:")) != -1)
		switch(opt) {
		case 'i': irq_status = USE_IRQ; break;
		case 'p': irq_status = DO_POLL; break;
		case 'e': irq_status = USE_EXT_MODE; break;
		case 's': irq_status = USE_STD_MODE; break;
		case 'd': device = optarg; break;
		default : usage();
		}
	if(irq_status == IRQ_INVALID)
		usage();

	set_interrupt_status(irq_status, device);

	exit(0);
}
