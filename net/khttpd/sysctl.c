/*

kHTTPd -- the next generation

Sysctl interface

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


#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/net.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/smp_lock.h>
#include <linux/sysctl.h>
#include <linux/un.h>
#include <linux/unistd.h>

#include <net/ip.h>
#include <net/sock.h>
#include <net/tcp.h>

#include <asm/atomic.h>
#include <asm/semaphore.h>
#include <asm/processor.h>
#include <asm/uaccess.h>

#include <linux/file.h>
#include "prototypes.h"



char 	sysctl_khttpd_docroot[200] = "/var/www";
int 	sysctl_khttpd_stop 	= 0;
int 	sysctl_khttpd_start 	= 0;
int 	sysctl_khttpd_unload 	= 0;
int 	sysctl_khttpd_clientport = 80;
int 	sysctl_khttpd_permreq    = S_IROTH; /* "other" read-access is required by default*/
int 	sysctl_khttpd_permforbid = S_IFDIR | S_ISVTX | S_IXOTH | S_IXGRP | S_IXUSR;
 				/* forbidden is execute, directory and sticky*/
int 	sysctl_khttpd_logging 	= 0;
int 	sysctl_khttpd_serverport= 8080;

char	sysctl_khttpd_dynamicstring[200];
int 	sysctl_khttpd_sloppymime= 0;
int	sysctl_khttpd_threads	= 2;
int	sysctl_khttpd_maxconnect = 1000;

atomic_t        khttpd_stopCount;

static struct ctl_table_header *khttpd_table_header;

static int sysctl_SecureString(ctl_table *table, int *name, int nlen,
		  void *oldval, size_t *oldlenp,
		  void *newval, size_t newlen, void **context);
static int proc_dosecurestring(ctl_table *table, int write, struct file *filp,
		  void *buffer, size_t *lenp);
static int khttpd_stop_wrap_proc_dointvec(ctl_table *table, int write, struct file *filp,
		  void *buffer, size_t *lenp);


static ctl_table khttpd_table[] = {
	{	NET_KHTTPD_DOCROOT,
		"documentroot",
		&sysctl_khttpd_docroot,
		sizeof(sysctl_khttpd_docroot),
		0644,
		NULL,
		proc_dostring,
		&sysctl_string,
		NULL,
		NULL,
		NULL
	},
	{	NET_KHTTPD_STOP,
		"stop",
		&sysctl_khttpd_stop,
		sizeof(int),
		0644,
		NULL,
		khttpd_stop_wrap_proc_dointvec,
		&sysctl_intvec,
		NULL,
		NULL,
		NULL
	},
	{	NET_KHTTPD_START,
		"start",
		&sysctl_khttpd_start,
		sizeof(int),
		0644,
		NULL,
		proc_dointvec,
		&sysctl_intvec,
		NULL,
		NULL,
		NULL
	},
	{	NET_KHTTPD_UNLOAD,
		"unload",
		&sysctl_khttpd_unload,
		sizeof(int),
		0644,
		NULL,
		proc_dointvec,
		&sysctl_intvec,
		NULL,
		NULL,
		NULL
	},
	{	NET_KHTTPD_THREADS,
		"threads",
		&sysctl_khttpd_threads,
		sizeof(int),
		0644,
		NULL,
		proc_dointvec,
		&sysctl_intvec,
		NULL,
		NULL,
		NULL
	},
	{	NET_KHTTPD_MAXCONNECT,
		"maxconnect",
		&sysctl_khttpd_maxconnect,
		sizeof(int),
		0644,
		NULL,
		proc_dointvec,
		&sysctl_intvec,
		NULL,
		NULL,
		NULL
	},
	{	NET_KHTTPD_SLOPPYMIME,
		"sloppymime",
		&sysctl_khttpd_sloppymime,
		sizeof(int),
		0644,
		NULL,
		proc_dointvec,
		&sysctl_intvec,
		NULL,
		NULL,
		NULL
	},
	{	NET_KHTTPD_CLIENTPORT,
		"clientport",
		&sysctl_khttpd_clientport,
		sizeof(int),
		0644,
		NULL,
		proc_dointvec,
		&sysctl_intvec,
		NULL,
		NULL,
		NULL
	},
	{	NET_KHTTPD_PERMREQ,
		"perm_required",
		&sysctl_khttpd_permreq,
		sizeof(int),
		0644,
		NULL,
		proc_dointvec,
		&sysctl_intvec,
		NULL,
		NULL,
		NULL
	},
	{	NET_KHTTPD_PERMFORBID,
		"perm_forbid",
		&sysctl_khttpd_permforbid,
		sizeof(int),
		0644,
		NULL,
		proc_dointvec,
		&sysctl_intvec,
		NULL,
		NULL,
		NULL
	},
	{	NET_KHTTPD_LOGGING,
		"logging",
		&sysctl_khttpd_logging,
		sizeof(int),
		0644,
		NULL,
		proc_dointvec,
		&sysctl_intvec,
		NULL,
		NULL,
		NULL
	},
	{	NET_KHTTPD_SERVERPORT,
		"serverport",
		&sysctl_khttpd_serverport,
		sizeof(int),
		0644,
		NULL,
		proc_dointvec,
		&sysctl_intvec,
		NULL,
		NULL,
		NULL
	},
	{	NET_KHTTPD_DYNAMICSTRING,
		"dynamic",
		&sysctl_khttpd_dynamicstring,
		sizeof(sysctl_khttpd_dynamicstring),
		0644,
		NULL,
		proc_dosecurestring,
		&sysctl_SecureString,
		NULL,
		NULL,
		NULL
	},
	{0,0,0,0,0,0,0,0,0,0,0}	};
	
	
static ctl_table khttpd_dir_table[] = {
	{NET_KHTTPD, "khttpd", NULL, 0, 0555, khttpd_table,0,0,0,0,0},
	{0,0,0,0,0,0,0,0,0,0,0}
};

static ctl_table khttpd_root_table[] = {
	{CTL_NET, "net", NULL, 0, 0555, khttpd_dir_table,0,0,0,0,0},
	{0,0,0,0,0,0,0,0,0,0,0}
};
	

void StartSysctl(void)
{
	khttpd_table_header = register_sysctl_table(khttpd_root_table,1);
}


void EndSysctl(void)
{
	unregister_sysctl_table(khttpd_table_header);
}

static int proc_dosecurestring(ctl_table *table, int write, struct file *filp,
		  void *buffer, size_t *lenp)
{
	size_t len;
	char *p, c=0;
	char String[256];
	
	if ((table->data==0) || (table->maxlen==0) || (*lenp==0) ||
	    ((filp->f_pos!=0) && (write==0))) {
		*lenp = 0;
		return 0;
	}
	
	if (write!=0) {
		len = 0;
		p = buffer;
		while (len < *lenp) {
			if(get_user(c, p++))
				return -EFAULT;
			if (c == 0 || c == '\n')
				break;
			len++;
		}
		if (len >= table->maxlen)
			len = table->maxlen-1;
		if(copy_from_user(String, buffer,(unsigned long)len))
			return -EFAULT;
		((char *) String)[len] = 0;
		filp->f_pos += *lenp;
		AddDynamicString(String);
	} else {
		GetSecureString(String);
		len = strlen(String);
		if (len > table->maxlen)
			len = table->maxlen;
		if (len > *lenp)
			len = *lenp;
		if (len!=0)
			if(copy_to_user(buffer, String,(unsigned long)len))
				return -EFAULT;
		if (len < *lenp) {
			if(put_user('\n', ((char *) buffer) + len))
				return -EFAULT;
			len++;
		}
		*lenp = len;
		filp->f_pos += len;
	}
	return 0;
}

/* A wrapper around proc_dointvec that computes
 * khttpd_stopCount = # of times sysctl_khttpd_stop has gone true
 * Sensing sysctl_khttpd_stop in other threads is racy;
 * sensing khttpd_stopCount in other threads is not.
 */
static int khttpd_stop_wrap_proc_dointvec(ctl_table *table, int write, struct file *filp,
		  void *buffer, size_t *lenp)
{
	int rv;
	int oldstop = sysctl_khttpd_stop;
	rv = proc_dointvec(table, write, filp, buffer, lenp);
	if (sysctl_khttpd_stop && !oldstop)
		atomic_inc(&khttpd_stopCount);

	return rv;
}
		

static int sysctl_SecureString (/*@unused@*/ctl_table *table, 
				/*@unused@*/int *name, 
				/*@unused@*/int nlen,
		  		/*@unused@*/void *oldval, 
		  		/*@unused@*/size_t *oldlenp,
		  		/*@unused@*/void *newval, 
		  		/*@unused@*/size_t newlen, 
		  		/*@unused@*/void **context)
{
	return -ENOSYS;
}
