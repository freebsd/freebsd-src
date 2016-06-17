/*

kHTTPd -- the next generation

Permissions/Security functions

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

#include "sysctl.h"
#include "security.h"
#include "prototypes.h"

/*

The basic security function answers "Userspace" when any one of the following 
conditions is met:

1) The filename contains a "?" (this is before % decoding, all others are 
                                after % decoding)
2) The filename doesn't start with a "/"                                
3) The file does not exist
4) The file does not have enough permissions 
   (sysctl-configurable, default = worldreadble)
5) The file has any of the "forbidden" permissions 
   (sysctl-configurable, default = execute, directory and sticky)
6) The filename contains a string as defined in the "Dynamic" list.

*/	


/* Prototypes */

static void DecodeHexChars(char *URL);
static struct DynamicString *DynamicList=NULL;

	

/*

The function "OpenFileForSecurity" returns either the "struct file" pointer
of the file, or NULL. NULL means "let userspace handle it". 

*/
struct file *OpenFileForSecurity(char *Filename)
{
	struct file *filp = NULL;
	struct DynamicString *List;
	umode_t permission;
	
	EnterFunction("OpenFileForSecurity");
	if (Filename==NULL)
		goto out_error;
	
	if (strlen(Filename)>=256 )
		goto out_error;  /* Sanity check */
	
	/* Rule no. 1  -- No "?" characters */
#ifndef BENCHMARK	
	if (strchr(Filename,'?')!=NULL)
		goto out_error;

	/* Intermediate step: decode all %hex sequences */
	
	DecodeHexChars(Filename);

	/* Rule no. 2  -- Must start with a "/" */
	
	if (Filename[0]!='/')
		goto out_error;
		
#endif
	/* Rule no. 3 -- Does the file exist ? */

	filp = filp_open(Filename, O_RDONLY, 0);
	
	if (IS_ERR(filp))
		goto out_error;

#ifndef BENCHMARK		
	permission = filp->f_dentry->d_inode->i_mode;
	
	/* Rule no. 4 : must have enough permissions */
	
	if ((permission & sysctl_khttpd_permreq)==0)
		goto out_error_put;	

	/* Rule no. 5 : cannot have "forbidden" permission */
	
	if ((permission & sysctl_khttpd_permforbid)!=0)
		goto out_error_put;	
		
	/* Rule no. 6 : No string in DynamicList can be a
			substring of the filename */
	
	List = DynamicList;
	while (List!=NULL)
	{
		if (strstr(Filename,List->value)!=NULL)
			goto out_error_put;	

		List = List->Next;
	}
	
#endif	
	LeaveFunction("OpenFileForSecurity - success");
out:
	return filp;

out_error_put:
	fput(filp);
out_error:
	filp=NULL;
	LeaveFunction("OpenFileForSecurity - fail");
	goto out;
}

/* 

DecodeHexChars does the actual %HEX decoding, in place. 
In place is possible because strings only get shorter by this.

*/
static void DecodeHexChars(char *URL)
{
	char *Source,*Dest;
	int val,val2;
	
	EnterFunction("DecodeHexChars");
	
	Source = strchr(URL,'%');
	
	if (Source==NULL) 
		return;
		
	Dest = Source;
	
	while (*Source!=0)
	{
		if (*Source=='%')
		{
			Source++;
			val = *Source;
			
			if (val>'Z') val-=0x20;
			val = val - '0';
			if (val<0) val=0; 
			if (val>9) val-=7;
			if (val>15) val=15;
			
			Source++;

			val2 = *Source;
			
			if (val2>'Z') val2-=0x20;
			val2 = val2 - '0';
			if (val2<0) val2=0; 
			if (val2>9) val2-=7;
			if (val2>15) val2=15;

			*Dest=val*16+val2;
		} else *Dest = *Source;
		Dest++;
		Source++;
	}
	*Dest=0;	
	
	LeaveFunction("DecodeHexChars");
}


void AddDynamicString(const char *String)
{
	struct DynamicString *Temp;
	
	EnterFunction("AddDynamicString");
	
	Temp = (struct DynamicString*)kmalloc(sizeof(struct DynamicString),(int)GFP_KERNEL);
	
	if (Temp==NULL) 
		return;
		
	memset(Temp->value,0,sizeof(Temp->value));
	strncpy(Temp->value,String,sizeof(Temp->value)-1);
	
	Temp->Next = DynamicList;
	DynamicList = Temp;
	
	LeaveFunction("AddDynamicString");
}

void GetSecureString(char *String)
{
	struct DynamicString *Temp;
	int max;
	
	EnterFunction("GetSecureString");
	
	*String = 0;
	
	memset(String,0,255);
	
	strncpy(String,"Dynamic strings are : -",255);
	Temp = DynamicList;
	while (Temp!=NULL)
	{
		max=253 - strlen(String) - strlen(Temp->value);
		strncat(String,Temp->value,max);
		max=253 - strlen(String) - 3;
		strncat(String,"- -",max);
		Temp = Temp->Next;
	}	
	
	LeaveFunction("GetSecureString");
}
