/*-
 * Copyright (c) 1998 Andrzej Bialecki
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
 *	$Id: vm.c,v 1.1.1.1 1998/07/14 07:30:54 abial Exp $
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>
#include <vm/vm_param.h>

int
main(int argc, char *argv[])
{
	int mib[2],i=0,len;
	struct vmtotal v;

	len=sizeof(struct vmtotal);
	mib[0]=CTL_VM;
	mib[1]=VM_METER;
	for(;;) {
		sysctl(mib,2,&v,&len,NULL,0);
		if(i==0) {
			printf("  procs    kB virt mem      real mem     shared vm   shared real    free\n");
			printf(" r d p s    tot    act    tot    act    tot    act    tot    act\n");
		}
		printf("%2hu%2hu%2hu%2hu",v.t_rq,v.t_dw,v.t_pw,v.t_sl);
		printf("%7u%7u%7u%7u",
		v.t_vm<<2,v.t_avm<<2,v.t_rm<<2,v.t_arm<<2);
		printf("%7u%7u%7u%7u%7u\n",
		v.t_vmshr<<2,v.t_avmshr<<2,v.t_rmshr<<2,v.t_armshr<<2,v.t_free<<2);
		sleep(5);
		i++;
		if(i>22) i=0;
	}
	exit(0);

}
