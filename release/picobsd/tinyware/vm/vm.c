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
 * $FreeBSD$
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>
#include <vm/vm_param.h>

#define pgtok(a) ((a) * (u_int) pagesize >> 10)

int
main(int argc, char *argv[])
{
	int mib[2],i=0,len;
	int pagesize, pagesize_len;
	struct vmtotal v;

	pagesize_len = sizeof(int);
	sysctlbyname("vm.stats.vm.v_page_size",&pagesize,&pagesize_len,NULL,0);

	len=sizeof(struct vmtotal);
	mib[0]=CTL_VM;
	mib[1]=VM_METER;
	for(;;) {
		sysctl(mib,2,&v,&len,NULL,0);
		if(i==0) {
			printf("  procs    kB virt mem       real mem     shared vm   shared real    free\n");
			printf(" r w l s    tot     act    tot    act    tot    act    tot    act\n");
		}
		printf("%2hu%2hu%2hu%2hu",v.t_rq-1,v.t_dw+v.t_pw,v.t_sl,v.t_sw);
		printf("%7ld %7ld %7ld%7ld",
			(long)pgtok(v.t_vm),(long)pgtok(v.t_avm),
			(long)pgtok(v.t_rm),(long)pgtok(v.t_arm));
		printf("%7ld%7ld%7ld%7ld%7ld\n",
			(long)pgtok(v.t_vmshr),(long)pgtok(v.t_avmshr),
			(long)pgtok(v.t_rmshr),(long)pgtok(v.t_armshr),
			(long)pgtok(v.t_free));
		sleep(5);
		i++;
		if(i>22) i=0;
	}
	exit(0);

}
