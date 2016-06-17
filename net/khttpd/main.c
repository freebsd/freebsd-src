/*

kHTTPd -- the next generation

Main program


kHTTPd TNG consists of 1 thread, this main-thread handles ALL connections
simultanious. It does this by keeping queues with the requests in different
stages.

The stages are

<not accepted> 		-	TCP/IP connection is not accepted yet
WaitForHeaders		-	Connection is accepted, waiting for headers
DataSending		-	Headers decoded, sending file-data
Userspace		-	Requires userspace daemon 
Logging			-	The request is finished, cleanup and logging

A typical flow for a request would be:

<not accepted>
WaitForHeaders
DataSending
Logging

or

<not accepted>
WaitForHeaders
Userspace



*/
/****************************************************************
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2, or (at your option)
 *	any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 ****************************************************************/


static int errno;
#define __KERNEL_SYSCALLS__

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/smp_lock.h>
#include <asm/unistd.h>

#include "structure.h"
#include "prototypes.h"
#include "sysctl.h"

struct khttpd_threadinfo threadinfo[CONFIG_KHTTPD_NUMCPU];  /* The actual work-queues */


atomic_t	ConnectCount;
atomic_t	DaemonCount;

static int	ActualThreads; /* The number of actual, active threads */


static int ConnectionsPending(int CPUNR)
{
	if (threadinfo[CPUNR].DataSendingQueue!=NULL) return O_NONBLOCK;
	if (threadinfo[CPUNR].WaitForHeaderQueue!=NULL) return O_NONBLOCK;
	if (threadinfo[CPUNR].LoggingQueue!=NULL) return O_NONBLOCK;
	if (threadinfo[CPUNR].UserspaceQueue!=NULL) return O_NONBLOCK;
  return 0;
}



static wait_queue_head_t DummyWQ[CONFIG_KHTTPD_NUMCPU];
static atomic_t Running[CONFIG_KHTTPD_NUMCPU]; 

static int MainDaemon(void *cpu_pointer)
{
	int CPUNR;
	sigset_t tmpsig;
	int old_stop_count;
	
	DECLARE_WAITQUEUE(main_wait,current);
	
	MOD_INC_USE_COUNT;

	/* Remember value of stop count.  If it changes, user must have 
	 * asked us to stop.  Sensing this is much less racy than 
	 * directly sensing sysctl_khttpd_stop. - dank
	 */
	old_stop_count = atomic_read(&khttpd_stopCount);
	
	CPUNR=0;
	if (cpu_pointer!=NULL)
	CPUNR=(int)*(int*)cpu_pointer;

	sprintf(current->comm,"khttpd - %i",CPUNR);
	daemonize();
	
	init_waitqueue_head(&(DummyWQ[CPUNR]));
	

	/* Block all signals except SIGKILL, SIGSTOP and SIGHUP */
	spin_lock_irq(&current->sigmask_lock);
	tmpsig = current->blocked;
	siginitsetinv(&current->blocked, sigmask(SIGKILL) | sigmask(SIGSTOP)| sigmask(SIGHUP));
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);
	
	
	if (MainSocket->sk==NULL)
	 	return 0;
	add_wait_queue_exclusive(MainSocket->sk->sleep,&(main_wait));
	atomic_inc(&DaemonCount);
	atomic_set(&Running[CPUNR],1);
	
	while (old_stop_count == atomic_read(&khttpd_stopCount)) 
	{
		int changes = 0;
		
		changes +=AcceptConnections(CPUNR,MainSocket);
		if (ConnectionsPending(CPUNR))
		{
			changes +=WaitForHeaders(CPUNR);
			changes +=DataSending(CPUNR);
			changes +=Userspace(CPUNR);
			changes +=Logging(CPUNR);
			/* Test for incoming connections _again_, because it is possible
			   one came in during the other steps, and the wakeup doesn't happen
			   then.
			*/
			changes +=AcceptConnections(CPUNR,MainSocket);
		}
		
		if (changes==0) 
		{
			(void)interruptible_sleep_on_timeout(&(DummyWQ[CPUNR]),1);	
			if (CPUNR==0) 
				UpdateCurrentDate();
		}
			
		if (signal_pending(current)!=0)
		{
			(void)printk(KERN_NOTICE "kHTTPd: Ring Ring - signal received\n");
			break;		  
		}
	
	}
	
	remove_wait_queue(MainSocket->sk->sleep,&(main_wait));
	
	StopWaitingForHeaders(CPUNR);
	StopDataSending(CPUNR);
	StopUserspace(CPUNR);
	StopLogging(CPUNR);
	
	atomic_set(&Running[CPUNR],0);
	atomic_dec(&DaemonCount);
	(void)printk(KERN_NOTICE "kHTTPd: Daemon %i has ended\n",CPUNR);
	MOD_DEC_USE_COUNT;
	return 0;
}

static int CountBuf[CONFIG_KHTTPD_NUMCPU];



/*

The ManagementDaemon has a very simple task: Start the real daemons when the user wants us
to, and cleanup when the users wants to unload the module.

Initially, kHTTPd didn't have this thread, but it is the only way to have "delayed activation",
a feature required to prevent accidental activations resulting in unexpected backdoors.

*/
static int ManagementDaemon(void *unused)
{
	sigset_t tmpsig;
	int waitpid_result;
	
	DECLARE_WAIT_QUEUE_HEAD(WQ);
	
	sprintf(current->comm,"khttpd manager");
	daemonize();
	
	/* Block all signals except SIGKILL and SIGSTOP */
	spin_lock_irq(&current->sigmask_lock);
	tmpsig = current->blocked;
	siginitsetinv(&current->blocked, sigmask(SIGKILL) | sigmask(SIGSTOP) );
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);

	/* main loop */
	while (sysctl_khttpd_unload==0)
	{
		int I;
		int old_stop_count;
		
		/* First : wait for activation */
		while ( (sysctl_khttpd_start==0) && (!signal_pending(current)) && (sysctl_khttpd_unload==0) )
		{
			current->state = TASK_INTERRUPTIBLE;
			interruptible_sleep_on_timeout(&WQ,HZ); 
		}
		if ( (signal_pending(current)) || (sysctl_khttpd_unload!=0) )
		 	break;
		sysctl_khttpd_stop = 0;
		 	
		/* Then start listening and spawn the daemons */
		if (StartListening(sysctl_khttpd_serverport)==0)
		{
			sysctl_khttpd_start = 0;
			continue;
		}

		ActualThreads = sysctl_khttpd_threads;
		if (ActualThreads<1) 
			ActualThreads = 1;
		if (ActualThreads>CONFIG_KHTTPD_NUMCPU) 
			ActualThreads = CONFIG_KHTTPD_NUMCPU;
		/* Write back the actual value */
		sysctl_khttpd_threads = ActualThreads;
		
		InitUserspace(ActualThreads);
		
		if (InitDataSending(ActualThreads)!=0)
		{
			StopListening();
			sysctl_khttpd_start = 0;
			continue;
		}
		if (InitWaitHeaders(ActualThreads)!=0)
		{
			for (I=0; I<ActualThreads; I++) {
				StopDataSending(I);
			}
			StopListening();
			sysctl_khttpd_start = 0;
			continue;
		}
	
		/* Clean all queues */
		memset(threadinfo, 0, sizeof(struct khttpd_threadinfo));

		for (I=0; I<ActualThreads; I++) {
			atomic_set(&Running[I],1);
			(void)kernel_thread(MainDaemon,&(CountBuf[I]), CLONE_FS | CLONE_FILES | CLONE_SIGHAND);
		}
		
		/* Then wait for deactivation */
		/* Remember value of stop count.  If it changes, user must 
		 * have asked us to stop.  Sensing this is much less racy 
		 * than directly sensing sysctl_khttpd_stop. - dank
		 */
		old_stop_count = atomic_read(&khttpd_stopCount);
		while ( ( old_stop_count == atomic_read(&khttpd_stopCount)) 
			 && (!signal_pending(current)) 
			 && (sysctl_khttpd_unload==0) )
		{
			/* Used to restart dead threads here, but it was buggy*/
			interruptible_sleep_on_timeout(&WQ,HZ);
		}
		
		/* Wait for the daemons to stop, one second per iteration */
		while (atomic_read(&DaemonCount)>0)
			interruptible_sleep_on_timeout(&WQ,HZ);
		StopListening();
		sysctl_khttpd_start = 0;
		/* reap the zombie-daemons */
		do
			waitpid_result = waitpid(-1,NULL,__WCLONE|WNOHANG);
		while (waitpid_result>0);
	}
	sysctl_khttpd_start = 0;
	sysctl_khttpd_stop = 1;
	atomic_inc(&khttpd_stopCount);

	/* Wait for the daemons to stop, one second per iteration */
	while (atomic_read(&DaemonCount)>0)
 		interruptible_sleep_on_timeout(&WQ,HZ);
	StopListening();
	/* reap the zombie-daemons */
	do
		waitpid_result = waitpid(-1,NULL,__WCLONE|WNOHANG);
	while (waitpid_result>0);
	
	(void)printk(KERN_NOTICE "kHTTPd: Management daemon stopped. \n        You can unload the module now.\n");

	MOD_DEC_USE_COUNT;

	return 0;
}

int __init khttpd_init(void)
{
	int I;

	MOD_INC_USE_COUNT;
	
	for (I=0; I<CONFIG_KHTTPD_NUMCPU; I++) {
		CountBuf[I]=I;
	}
	
	atomic_set(&ConnectCount,0);
	atomic_set(&DaemonCount,0);
	atomic_set(&khttpd_stopCount,0);
	

	/* Maybe the mime-types will be set-able through sysctl in the future */	   
		
 	AddMimeType(".htm","text/html");
 	AddMimeType("html","text/html");
 	AddMimeType(".gif","image/gif");
 	AddMimeType(".jpg","image/jpeg");
 	AddMimeType(".png","image/png");
 	AddMimeType("tiff","image/tiff");
 	AddMimeType(".zip","application/zip");
	AddMimeType(".pdf","application/pdf");
 	AddMimeType("r.gz","application/x-gtar");
 	AddMimeType(".tgz","application/x-gtar");
	AddMimeType(".deb","application/x-debian-package");
	AddMimeType("lass","application/x-java");
	AddMimeType(".mp3","audio/mpeg");
	AddMimeType(".txt","text/plain");
	
	AddDynamicString("..");
	AddDynamicString("cgi-bin");

	StartSysctl();
	
	(void)kernel_thread(ManagementDaemon,NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND);
	
	return 0;
}

void khttpd_cleanup(void)
{
	EndSysctl();
}

	module_init(khttpd_init)
	module_exit(khttpd_cleanup)

	MODULE_LICENSE("GPL");
