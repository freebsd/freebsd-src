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
 * $FreeBSD: src/release/picobsd/tinyware/sps/sps.c,v 1.2 1999/08/28 01:34:01 peter Exp $
 */

/*
 * Small replacement for ps(1) - uses only sysctl(3) to retrieve info
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <sys/proc.h>
#include <sys/user.h>

char p_stat[]="?iRSTZ";

int
main(int argc, char *argv[])
{
	int mib[3],i=0,num,len;
	struct kinfo_proc kp,*t,*u;
	char buf[20],vty[5],pst[5];
	int ma,mi;

	mib[0]=CTL_KERN;
	mib[1]=KERN_PROC;
	mib[2]=KERN_PROC_ALL;
	if(sysctl(mib,3,NULL,&len,NULL,0)) {
		perror("sysctl sizing");
		exit(1);
	}
	t=(struct kinfo_proc *)malloc(len);
	if(sysctl(mib,3,t,&len,NULL,0)) {
		perror("sysctl info");
		exit(1);
	}
	num=len / sizeof(struct kinfo_proc);
	i=0;
	printf("USERNAME  PID PPID PRI NICE TTY STAT WCHAN   COMMAND\n");
	while(i<num) {
		u=(t+num-i-1);
		ma=major(u->kp_eproc.e_tdev);
		mi=minor(u->kp_eproc.e_tdev);
		switch(ma) {
		case 255:
			strcpy(vty,"??");
			break;
		case 12:
			if(mi!=255) {
				sprintf(vty,"v%d",mi);
				break;
			}
			/* FALLTHROUGH */
		case 0:
			strcpy(vty,"con");
			break;
		case 5:
			sprintf(vty,"p%d",mi);
			break;
		}
		sprintf(pst,"%c",p_stat[u->kp_proc.p_stat]);
		printf("%8s%5d%5d %3d %4d %3s %-4s %-7s (%s)\n",
			u->kp_eproc.e_login,
			u->kp_proc.p_pid,
			u->kp_eproc.e_ppid,
			u->kp_proc.p_priority,
			u->kp_proc.p_nice,
			vty,
			pst,
			u->kp_eproc.e_wmesg,
			u->kp_proc.p_comm);
		i++;
	}
	free(t);
	exit(0);

}
