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
 *
 * $Id: lptcontrol.c,v 1.2 1994/04/08 22:23:39 csgr Exp $
 */

#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <machine/lpt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/errno.h>


#define DEFAULT_LPT	"/dev/lpt0"
#define IRQ_INVALID	-1
#define DO_POLL		0
#define USE_IRQ		1

static char	default_printer[] = DEFAULT_LPT;

static void usage(const char * progname)
{
	fprintf(stderr, "usage: %s -i | -p  [-f <file name>]\n", progname);
	exit(1);
}

static void set_interrupt_status(int irq_status, const char * file)
{
	int	fd;

	if((fd = open(file, O_WRONLY, 0660)) < 0) {
		perror("open");
		exit(1);
	}
	if(ioctl(fd, LPT_IRQ, &irq_status) < 0) {
		perror("ioctl");
		exit(1);
	}
	close(fd);
}


int main (int argc, char * argv[])
{
	int		opt;
	int		irq_status = -1;
	char		* file = default_printer;
	
	while((opt = getopt(argc, argv, "pif:")) != -1)
		switch(opt) {
		case 'i': irq_status = USE_IRQ; break;
		case 'p': irq_status = DO_POLL; break;
		case 'f': file = optarg; break;
		default : usage(argv[0]); 
		}
	if(irq_status == IRQ_INVALID) usage(argv[0]);

	set_interrupt_status(irq_status, file);

	exit(0);
}


