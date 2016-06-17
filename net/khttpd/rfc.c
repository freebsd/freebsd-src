/*

kHTTPd -- the next generation

RFC related functions (headers and stuff)

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

#include <linux/ctype.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/net.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/unistd.h>
#include <linux/file.h>
#include <linux/smp_lock.h>

#include <net/ip.h>
#include <net/sock.h>

#include <asm/atomic.h>
#include <asm/semaphore.h>
#include <asm/processor.h>
#include <asm/uaccess.h>


#include "prototypes.h"
#include "structure.h"
#include "sysctl.h"


#define KHTTPD_NUMMIMETYPES 	40

static atomic_t	MimeCount;

struct MimeType
{
	__u32 	identifier;
	char	type[64-sizeof(__u32)-sizeof(__kernel_size_t)];  
	__kernel_size_t	len;
};

static struct MimeType	MimeTypes[KHTTPD_NUMMIMETYPES];


void AddMimeType(const char *Ident,const char *Type)
{	
	__u32	*I;
	
	EnterFunction("AddMimeType");
	
	if (strlen(Ident)!=4) 
   	{	
   		(void)printk(KERN_ERR "httpd: Only 4-byte mime-identifiers are accepted\n");
   		return;
   	}

	if (strlen(Type)>(64-sizeof(__u32)-sizeof(__kernel_size_t) ) )  
   	{	
   		(void)printk(KERN_ERR "httpd: Mime-string too long.\n");
   		return;
   	}
   	
   	I=(__u32*)Ident;
   	
   	/* FIXME: Need to lock-down all access to the mime-structure here */
   	/*        For now, just don't add mime-types after initialisation */
   	
   	
   	MimeTypes[atomic_read(&MimeCount)].identifier=*I;
   	strncpy(MimeTypes[atomic_read(&MimeCount)].type,Type,(64-sizeof(__u32)-sizeof(__kernel_size_t)));
   	MimeTypes[atomic_read(&MimeCount)].len = strlen(Type);
   	
   	atomic_inc(&MimeCount);
   	LeaveFunction("AddMimeType");
}


char *ResolveMimeType(const char *File,__kernel_size_t *Len)
/*

	The returned string is for READ ONLY, ownership of the memory is NOT
	transferred.

*/
{	
	__u32	*I;
	int pos,lc,filelen;
	
	EnterFunction("ResolveMimeType");
	
	*Len = 0;
	
	if (File==NULL)
		return NULL;
	
	filelen = (int)strlen(File);
	
	if (filelen<4) 
   	{	
   		return NULL;
   	}
   	
   	/* The Merced-people are NOT going to like this! So this has to be fixed
   	   in a later stage. */

	pos = filelen-4;
   	I=(__u32*)(File+pos);
   	
   	lc=0;
   	
   	while (lc<atomic_read(&MimeCount))
 	{
   		if (MimeTypes[lc].identifier == *I)
   		{
   			*Len = MimeTypes[lc].len;
   			LeaveFunction("ResolveMimeType - success");
   	  		return MimeTypes[lc].type;
   	  	}
   	  	lc++;
   	} 	
   	
	if (sysctl_khttpd_sloppymime)
	{
		*Len = MimeTypes[0].len;  
		LeaveFunction("ResolveMimeType - unknown");
	   	return MimeTypes[0].type;
	}
	else
	{
		LeaveFunction("ResolveMimeType - failure");
	   	return NULL;
	}
}


static char HeaderPart1[] = "HTTP/1.0 200 OK\r\nServer: kHTTPd/0.1.6\r\nDate: ";
#ifdef BENCHMARK
static char HeaderPart1b[] ="HTTP/1.0 200 OK";
#endif
static char HeaderPart3[] = "\r\nContent-type: ";
static char HeaderPart5[] = "\r\nLast-modified: ";
static char HeaderPart7[] = "\r\nContent-length: ";
static char HeaderPart9[] = "\r\n\r\n";

#ifdef BENCHMARK
/* In BENCHMARK-mode, just send the bare essentials */
void SendHTTPHeader(struct http_request *Request)
{
	struct msghdr	msg;
	mm_segment_t	oldfs;
	struct iovec	iov[9];
	int 		len,len2;
	
	
	EnterFunction("SendHTTPHeader");
		
	msg.msg_name     = 0;
	msg.msg_namelen  = 0;
	msg.msg_iov	 = &iov[0];
	msg.msg_iovlen   = 6;
	msg.msg_control  = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags    = 0;  /* Synchronous for now */
	
	iov[0].iov_base = HeaderPart1b;
	iov[0].iov_len  = 15;
	iov[1].iov_base = HeaderPart3;
	iov[1].iov_len  = 16;
	iov[2].iov_base = Request->MimeType;
	iov[2].iov_len  = Request->MimeLength;
	
	iov[3].iov_base = HeaderPart7;
	iov[3].iov_len  = 18;
	
	
	sprintf(Request->LengthS,"%i",Request->FileLength);
	iov[4].iov_base = Request->LengthS;
	iov[4].iov_len  = strlen(Request->LengthS);
	iov[5].iov_base = HeaderPart9;
	iov[5].iov_len  = 4;
	
	len2=15+16+18+iov[2].iov_len+iov[4].iov_len+4;
	
	
	len = 0;
	

	oldfs = get_fs(); set_fs(KERNEL_DS);
	len = sock_sendmsg(Request->sock,&msg,len2);
	set_fs(oldfs);

	
	return;	
}
#else
void SendHTTPHeader(struct http_request *Request)
{
	struct msghdr	msg;
	mm_segment_t	oldfs;
	struct iovec	iov[9];
	int 		len,len2;
	__kernel_size_t	slen;
	
	EnterFunction("SendHTTPHeader");
	
	msg.msg_name     = 0;
	msg.msg_namelen  = 0;
	msg.msg_iov	 = &(iov[0]);
	msg.msg_iovlen   = 9;
	msg.msg_control  = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags    = 0;  /* Synchronous for now */
	
	iov[0].iov_base = HeaderPart1;
	iov[0].iov_len  = 45;
	iov[1].iov_base = CurrentTime;
	iov[1].iov_len  = 29;
	iov[2].iov_base = HeaderPart3;
	iov[2].iov_len  = 16;
	
	iov[3].iov_base = Request->MimeType;
	iov[3].iov_len  = Request->MimeLength;
	
	iov[4].iov_base = HeaderPart5;
	iov[4].iov_len  = 17;
	iov[5].iov_base = &(Request->TimeS[0]);
	iov[5].iov_len  = 29;
	iov[6].iov_base = HeaderPart7;
	iov[6].iov_len  = 18;
	iov[7].iov_base = &(Request->LengthS[0]);
	slen = strlen(Request->LengthS); 
	iov[7].iov_len  = slen;
	iov[8].iov_base = HeaderPart9;
	iov[8].iov_len  = 4;
	
	len2=45+2*29+16+17+18+slen+4+iov[3].iov_len;
	
	len = 0;

	oldfs = get_fs(); set_fs(KERNEL_DS);
	len = sock_sendmsg(Request->sock,&msg,len2);
	set_fs(oldfs);
	LeaveFunction("SendHTTPHeader");
	

	return;	
}
#endif



/* 

Parse a HTTP-header. Be careful for buffer-overflows here, this is the most important
place for this, since the remote-user controls the data.

*/
void ParseHeader(char *Buffer,const int length, struct http_request *Head)
{
	char *Endval,*EOL,*tmp;
	
	EnterFunction("ParseHeader");
	Endval = Buffer + length;
	
	/* We want to parse only the first header if multiple headers are present */
	tmp = strstr(Buffer,"\r\n\r\n"); 
	if (tmp!=NULL)
	    Endval = tmp;
	
	
	while (Buffer<Endval)
	{
		if (isspace(Buffer[0]))
		{
			Buffer++;
			continue;
		}
			
		
		EOL=strchr(Buffer,'\n');
		
		if (EOL==NULL) EOL=Endval;
		
		if (EOL-Buffer<4) 
		{
			Buffer++;
			continue;
		}
		
		if (strncmp("GET ",Buffer,4)==0)
		{
			int PrefixLen;
			Buffer+=4;
			
			tmp=strchr(Buffer,' ');
			if (tmp==0) 
			{
				tmp=EOL-1;
				Head->HTTPVER = 9;
			} else
				Head->HTTPVER = 10;
			
			if (tmp>Endval) continue;
			
			strncpy(Head->FileName,sysctl_khttpd_docroot,sizeof(Head->FileName));
			PrefixLen = strlen(sysctl_khttpd_docroot);
			Head->FileNameLength = min_t(unsigned int, 255, tmp - Buffer + PrefixLen);		
			
			strncat(Head->FileName,Buffer,min_t(unsigned int, 255 - PrefixLen, tmp - Buffer));
					
			Buffer=EOL+1;	
#ifdef BENCHMARK
			break;
#endif						
			continue;
		}
#ifndef BENCHMARK		
		if (strncmp("If-Modified-Since: ",Buffer,19)==0)
		{
			Buffer+=19;
			
			strncpy(Head->IMS,Buffer,min_t(unsigned int, 127,EOL-Buffer-1));
					
			Buffer=EOL+1;	
			continue;
		}

		if (strncmp("User-Agent: ",Buffer,12)==0)
		{
			Buffer+=12;
			
			strncpy(Head->Agent,Buffer,min_t(unsigned int, 127,EOL-Buffer-1));
					
			Buffer=EOL+1;	
			continue;
		}
		

		if (strncmp("Host: ",Buffer,6)==0)
		{
			Buffer+=6;
			
			strncpy(Head->Host,Buffer,min_t(unsigned int, 127,EOL-Buffer-1));
					
			Buffer=EOL+1;	
			continue;
		}
#endif		
		Buffer = EOL+1;  /* Skip line */
	}
	LeaveFunction("ParseHeader");
}
