/*
 * Copyright (c) 1997, 1999 Hellmuth Michaelis. All rights reserved.
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
 *---------------------------------------------------------------------------
 *
 *	exec.h - supplemental program/script execution
 *	----------------------------------------------
 *
 *	$Id: exec.c,v 1.13 1999/12/13 21:25:24 hm Exp $ 
 *
 * $FreeBSD$
 *
 *      last edit-date: [Mon Dec 13 21:45:59 1999]
 *
 *---------------------------------------------------------------------------*/

#include "isdnd.h"

#include <sys/wait.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_PIDS 32

static struct pid_tab {
	pid_t	pid;
	cfg_entry_t *cep;
} pid_tab[MAX_PIDS];

/*---------------------------------------------------------------------------*
 *	SIGCHLD signal handler
 *---------------------------------------------------------------------------*/
void
sigchild_handler(int sig)
{
	int retstat;
	register int i;
	pid_t pid;
	
	if((pid = waitpid(-1, &retstat, WNOHANG)) <= 0)
	{
		log(LL_ERR, "ERROR, sigchild_handler, waitpid: %s", strerror(errno));
		error_exit(1, "ERROR, sigchild_handler, waitpid: %s", strerror(errno));
	}
	else
	{
		if(WIFEXITED(retstat))
		{
			DBGL(DL_PROC, (log(LL_DBG, "normal child (pid=%d) termination, exitstat = %d",
				pid, WEXITSTATUS(retstat))));
		}
		else if(WIFSIGNALED(retstat))
		{
			if(WCOREDUMP(retstat))
				log(LL_WRN, "child (pid=%d) termination due to signal %d (coredump)",
					pid, WTERMSIG(retstat));
			else
				log(LL_WRN, "child (pid=%d) termination due to signal %d",
					pid, WTERMSIG(retstat));
		}
	}

	/* check if hangup required */
	
	for(i=0; i < MAX_PIDS; i++)
	{
		if(pid_tab[i].pid == pid)
		{
			if(pid_tab[i].cep->cdid != CDID_UNUSED)
			{
				DBGL(DL_PROC, (log(LL_DBG, "sigchild_handler: scheduling hangup for cdid %d, pid %d",
					pid_tab[i].cep->cdid, pid_tab[i].pid)));
				pid_tab[i].cep->hangup = 1;
			}
			pid_tab[i].pid = 0;
			break;
		}
	}
}

/*---------------------------------------------------------------------------*
 *	execute prog as a subprocess and pass an argumentlist
 *---------------------------------------------------------------------------*/
pid_t
exec_prog(char *prog, char **arglist)
{
	char tmp[MAXPATHLEN];
	char path[MAXPATHLEN+1];
	pid_t pid;
	int a;

	snprintf(path, sizeof(path), "%s/%s", ETCPATH, prog);

	arglist[0] = path;

	tmp[0] = '\0';

	for(a=1; arglist[a] != NULL; ++a )
	{
		strcat(tmp, " " );
		strcat(tmp, arglist[a]);
	}

	DBGL(DL_PROC, (log(LL_DBG, "exec_prog: %s, args:%s", path, tmp)));
	
	switch(pid = fork())
	{
		case -1:		/* error */
			log(LL_ERR, "ERROR, exec_prog/fork: %s", strerror(errno));
			error_exit(1, "ERROR, exec_prog/fork: %s", strerror(errno));
		case 0:			/* child */
			break;
		default:		/* parent */
			return(pid);
	}

	/* this is the child now */

	if(execvp(path,arglist) < 0 )
		_exit(127);

	return(-1);
}

/*---------------------------------------------------------------------------*
 *	run interface up/down script
 *---------------------------------------------------------------------------*/
int
exec_connect_prog(cfg_entry_t *cep, const char *prog, int link_down)
{
	char *argv[32], **av = argv;
	char devicename[MAXPATHLEN], addr[100];
	char *device;
	int s;
	struct ifreq ifr;

	/* the obvious things */
	device = bdrivername(cep->usrdevicename);
	snprintf(devicename, sizeof(devicename), "%s%d", device, cep->usrdeviceunit);
	*av++ = (char*)prog;
	*av++ = "-d";
	*av++ = devicename;
	*av++ = "-f";
	*av++ = link_down ? "down" : "up";

	/* try to figure AF_INET address of interface */
	addr[0] = '\0';
	memset(&ifr, 0, sizeof ifr);
	ifr.ifr_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name, devicename, sizeof(ifr.ifr_name));
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s >= 0) {
		if (ioctl(s, SIOCGIFADDR, (caddr_t)&ifr) >= 0) {
			struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr;
			strcpy(addr, inet_ntoa(sin->sin_addr));
			*av++ = "-a";
			*av++ = addr;
		}
		close(s);
	}

	/* terminate argv */
	*av++ = NULL;

	return exec_prog((char*)prog, argv);
}

/*---------------------------------------------------------------------------*
 *	run answeringmachine application
 *---------------------------------------------------------------------------*/
int
exec_answer(cfg_entry_t *cep)
{
	char *argv[32];
	u_char devicename[MAXPATHLEN];	
	int pid;
	char *device;
	
	device = bdrivername(cep->usrdevicename);

	snprintf(devicename, sizeof(devicename), "/dev/i4b%s%d", device, cep->usrdeviceunit);

	argv[0] = cep->answerprog;
	argv[1] = "-D";
	argv[2] = devicename;
	argv[3] = "-d";
	argv[4] = "unknown";
	argv[5] = "-s";
	argv[6] = "unknown";
	argv[7] = NULL;

	/* if destination telephone number avail, add it as argument */
	
	if(*cep->local_phone_incoming)
		argv[4] = cep->local_phone_incoming;

	/* if source telephone number avail, add it as argument */
	
	if(*cep->real_phone_incoming)
		argv[6] = cep->real_phone_incoming;

	if(*cep->display)
	{
		argv[7] = "-t";
		argv[8] = cep->display;
		argv[9] = NULL;
	}

	/* exec program */
	
	DBGL(DL_PROC, (log(LL_DBG, "exec_answer: prog=[%s]", cep->answerprog)));
	
	pid = exec_prog(cep->answerprog, argv);
		
	/* enter pid and conf ptr entry addr into table */
	
	if(pid != -1)
	{
		int i;
		
		for(i=0; i < MAX_PIDS; i++)
		{
			if(pid_tab[i].pid == 0)
			{
				pid_tab[i].pid = pid;
				pid_tab[i].cep = cep;
				break;
			}
		}
		return(GOOD);
	}
	return(ERROR);
}

/*---------------------------------------------------------------------------*
 *	check if a connection has an outstanding process, if yes, kill it
 *---------------------------------------------------------------------------*/
void
check_and_kill(cfg_entry_t *cep)
{
	int i;
	
	for(i=0; i < MAX_PIDS; i++)
	{
		if(pid_tab[i].cep == cep)
		{
			pid_t kp;

			DBGL(DL_PROC, (log(LL_DBG, "check_and_kill: killing pid %d", pid_tab[i].pid)));

			kp = pid_tab[i].pid;
			pid_tab[i].pid = 0;			
			kill(kp, SIGHUP);
			break;
		}
	}
}

/* EOF */
