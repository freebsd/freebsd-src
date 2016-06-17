#ifndef _INCLUDE_GUARD_STRUCTURE_H_
#define _INCLUDE_GUARD_STRUCTURE_H_

#include <linux/time.h>
#include <linux/wait.h>


struct http_request;

struct http_request
{
	/* Linked list */
	struct http_request *Next;
	
	/* Network and File data */
	struct socket	*sock;		
	struct file	*filp;		

	/* Raw data about the file */
	
	int		FileLength;	/* File length in bytes */
	int		Time;		/* mtime of the file, unix format */
	int		BytesSent;	/* The number of bytes already sent */
	int		IsForUserspace;	/* 1 means let Userspace handle this one */
	
	/* Wait queue */
	
	wait_queue_t sleep;		/* For putting in the socket's waitqueue */
	
	/* HTTP request information */
	char		FileName[256];	/* The requested filename */
	int		FileNameLength; /* The length of the string representing the filename */
	char		Agent[128];	/* The agent-string of the remote browser */
	char		IMS[128];	/* If-modified-since time, rfc string format */
	char		Host[128];	/* Value given by the Host: header */
	int		HTTPVER;        /* HTTP-version; 9 for 0.9,   10 for 1.0 and above */


	/* Derived date from the above fields */	
	int		IMS_Time;	/* if-modified-since time, unix format */
	char		TimeS[64];	/* File mtime, rfc string representation */
	char		LengthS[14];	/* File length, string representation */
	char		*MimeType;	/* Pointer to a string with the mime-type 
					   based on the filename */
	__kernel_size_t	MimeLength;	/* The length of this string */
	
};



/*

struct khttpd_threadinfo represents the four queues that 1 thread has to deal with.
It is padded to occupy 1 (Intel) cache-line, to avoid "cacheline-pingpong".

*/
struct khttpd_threadinfo
{
	struct http_request* WaitForHeaderQueue;
	struct http_request* DataSendingQueue;
	struct http_request* LoggingQueue;
	struct http_request* UserspaceQueue;
	char  dummy[16];  /* Padding for cache-lines */
};



#endif
