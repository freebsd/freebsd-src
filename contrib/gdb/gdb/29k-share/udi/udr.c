/* This module supports sending and receiving data objects over a
   socket conection.

   Copyright 1993 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

static char udr_c[]="@(#)udr.c	2.8  Daniel Mann";
static char udr_c_AMD[]="@(#)udr.c	2.3, AMD";
/* 
*	All data is serialised into a character stream,
*	and de-serialised back into the approproiate objects.
********************************************************************** HISTORY
*/
/* This is all unneeded on DOS machines.  */
#ifndef __GO32__

#include <stdio.h>
#include <sys/types.h>

/* This used to say sys/fcntl.h, but the only systems I know of that
   require that are old (pre-4.3, at least) BSD systems, which we
   probably don't need to worry about.  */
#include <fcntl.h>

#include <sys/socket.h>
#include "udiproc.h"
#include "udisoc.h"

extern	int	errno;
extern	char*	malloc();

/* local type decs. and macro defs. not in a .h  file ************* MACRO/TYPE
*/

/* global dec/defs. which are not in a .h   file ************* EXPORT DEC/DEFS
*/
int		udr_errno;		/* error occurs during UDR service */

/* local dec/defs. which are not in a .h   file *************** LOCAL DEC/DEFS
*/

/****************************************************************** UDR_CREATE
* Build UDR structure for character stream processing.
*/
int udr_create(udrs, sd, size)
UDR*	udrs;
int	sd;
int	size;
{
    udrs->sd = sd;
    if(!udrs->buff) udrs->buff = malloc(size);
    udrs->getbytes = udrs->buff;	/* set the buffer to the start */
    udrs->putbytes = udrs->buff;
    udrs->putend = udrs->buff;
    udrs->udr_op = -1;			/* don't know the direction */
    udrs->previous_op = -1;		/* don't know the direction */
    udrs->bufsize = size;
    return 0;
}

/******************************************************************** UDR_FREE
* Free USR structure and close socket.
*/
int udr_free(udrs)
UDR*	udrs;
{
    close(udrs->sd);
    free(udrs->buff);
    return 0;
}

/****************************************************************** UDR_SIGNAL
* Send a signal to the process at the other end of the socket,
* indicating that it should expect to recieve a new message shortly.
*/
int udr_signal(udrs)
UDR*	udrs;
{
    if(send(udrs->sd, "I", 1, MSG_OOB) == -1)
    {	perror("ERROR, udr_signal(), send(...MSG_OOB)");
	udr_errno =  UDIErrorIPCInternal;
    	return -1;		/* return error code */
    }
    return 0;
}

/***************************************************************** UDR_SENDNOW
* used to flush the current character stream buffer to
* the associated socket.  */
int udr_sendnow(udrs)
UDR*	udrs;
{
    int size = (UDIUInt32)(udrs->putend) - (UDIUInt32)(udrs->buff);
    if(udrs->previous_op == 0)
    {   udr_errno =  UDIErrorIPCInternal;
	return -1;
    }
    udrs->putbytes = udrs->buff;
    udrs->putend = udrs->buff;
    if (write(udrs->sd, udrs->buff, size) == -1)
    {	perror("ERROR, udr_sendnow(), write() call: ");
	udr_errno =  UDIErrorIPCInternal;
    	return -1;		/* return error code */
    }
    return 0;
}

/******************************************************************** UDR_WORK
* Function to send or recieve data from the buffers supporting
* socket communication. The buffer contains serialised objects
* sent/recieved over a socket connection.
*/
int udr_work(udrs, object_p, size)
UDR*	udrs;
void*	object_p;
int	size;
{
    int	    cnt, remain;

    if(udrs->udr_op != udrs->previous_op)
    {	if(udrs->previous_op == 0)
	{   udr_errno =  UDIErrorIPCInternal;
	    return -1;
        }
	udrs->previous_op= udrs->udr_op;
	udrs->putbytes = udrs->buff;
	udrs->getbytes = udrs->buff;
    }

    if(udrs->udr_op == UDR_ENCODE)
    {			/* write data into character stream buffer */
    	if( (UDIUInt32)(udrs->putbytes) + size >
	    (UDIUInt32)(udrs->buff) + (UDIUInt32)(udrs->bufsize) )
	{   udr_errno =  UDIErrorIPCInternal;
	    return -1;
        }
	memcpy(udrs->putbytes, (char*)object_p, size);
    	udrs->putbytes += size;
    	if(udrs->putbytes > udrs->putend) udrs->putend = udrs->putbytes;
    }
    else if(udrs->udr_op == UDR_DECODE)
    {
    	if( (UDIUInt32)(udrs->putbytes)-(UDIUInt32)(udrs->getbytes) < size )
	{		 /* need more data in character stream buffer */
       	    remain = (UDIUInt32)(udrs->bufsize) -
		( (UDIUInt32)(udrs->putbytes)-(UDIUInt32)(udrs->buff) );
       	    if( ((UDIUInt32)(udrs->bufsize) + (UDIUInt32)(udrs->buff)
		- (UDIUInt32)(udrs->getbytes)) < size)
	    {   udr_errno =  UDIErrorIPCInternal;
	        return -1;
            }
    	    cnt = read(udrs->sd, (char*)udrs->putbytes, remain);
	    if(cnt == -1) perror("ERROR udr_work(),  read() failure: ");
	    udrs->putbytes += cnt;
    	    if( (UDIUInt32)(udrs->putbytes)-(UDIUInt32)(udrs->getbytes) < size )
	    {	udr_errno =  UDIErrorIPCInternal;
	        return -1;		/* return error code */
    	    }
    	}		/* read data from character stream buffer */
	memcpy((char*)object_p,  udrs->getbytes, size);
    	udrs->getbytes += size;
    }
    else
    {	udr_errno =  UDIErrorIPCInternal;
	return -1;
    }
    return 0;
}

/************************************************************* UDR_UDIResource
*/
int udr_UDIResource(udrs, object_p)
UDR*	udrs;
UDIResource*	object_p;
{
    int	retval;

    retval = udr_CPUSpace(udrs, &object_p->Space);
    retval = retval |  udr_CPUOffset(udrs, &object_p->Offset);
    return retval;
}

/**************************************************************** UDR_UDIRange
*/
int udr_UDIRange(udrs, object_p)
UDR*		udrs;
UDIRange*	object_p;
{
    int	retval;

    retval = udr_CPUOffset(udrs, &object_p->Low);
    retval = retval | udr_CPUOffset(udrs, &object_p->High);
    return retval;
}

/********************************************************** UDR_UDIMemoryRange
*/
int udr_UDIMemoryRange(udrs, object_p)
UDR*		udrs;
UDIMemoryRange*	object_p;
{
    int	retval;

    retval = udr_CPUSpace(udrs, &object_p->Space);
    retval = retval | udr_CPUOffset(udrs, &object_p->Offset);
    retval = retval | udr_CPUSizeT(udrs, &object_p->Size);
    return retval;
}

/****************************************************************** UDR_string
*/
int udr_string(udrs, sp)
UDR*	udrs;
char*	sp;
{
    int	len, retval;

    if(udrs->udr_op == UDR_ENCODE)
    {
	if(sp)
    	{   len = strlen(sp) + 1;
    	    retval = udr_UDIInt32(udrs, &len);
    	    retval = retval | udr_work(udrs, sp, len);
	}
	else	/* deal with NULL pointer */
	{   len = 0;
    	    retval = udr_UDIInt32(udrs, &len);
	}
    }
    else if(udrs->udr_op == UDR_DECODE)
    {
    	retval = udr_UDIInt32(udrs, &len);
	if(len)
    	    retval = retval | udr_work(udrs, sp, len);
	else	*sp = '\0';			/* terminate string */
    }
    else
    {	udr_errno =  UDIErrorIPCInternal;
	return -1;
    }
    return retval;
}

/******************************************************************* UDR_BYTES
*/
int udr_bytes(udrs, ptr, len)
UDR*	udrs;
char*	ptr;
int	len;
{
    return udr_work(udrs, ptr, len);
}

/********************************************************************* UDR_INT
*/
int udr_int(udrs, int_p)
UDR*	udrs;
int*	int_p;
{
    int ret_val;
    UDIInt32  udr_obj;			/* object of know size */

    if(udrs->udr_op == UDR_ENCODE)
    {
        udr_obj = *int_p;		/* copy into know object size */
        return udr_UDIInt32(udrs, &udr_obj);
    }
    else if(udrs->udr_op == UDR_DECODE)
    {
        ret_val = udr_UDIInt32(udrs, &udr_obj);	/* get object of known size */
	*int_p = udr_obj;
	return ret_val;
    }
    else
    {	udr_errno =  UDIErrorIPCInternal;
	return -1;
    }
}

/****************************************************************** UDR_INLINE
*/
char* udr_inline(udrs, size)
UDR*	udrs;
int	size;
{
    if(udrs->udr_op != udrs->previous_op)
    {	if(udrs->previous_op == 0)
	{   udr_errno =  UDIErrorIPCInternal;
	    return 0;
        }
    	udrs->previous_op= udrs->udr_op;
	udrs->putbytes = udrs->buff;
	udrs->getbytes = udrs->buff;
    }
    if(udrs->udr_op == UDR_ENCODE)
    {
    	if(udrs->putbytes + size > udrs->bufsize + udrs->buff)
	   return 0;
    	udrs->putbytes += size;
	return udrs->putbytes - size;
    }
    else if(udrs->udr_op == UDR_DECODE)
    {
    	if(udrs->getbytes + size > udrs->bufsize + udrs->buff)
	   return 0;
    	udrs->getbytes += size;
	return udrs->getbytes - size;
    }
    else
    {	udr_errno =  UDIErrorIPCInternal;
	return 0;
    }
}

/****************************************************************** UDR_GETPOS
*/
char*	udr_getpos(udrs)
UDR*	udrs;
{
    if(udrs->udr_op == UDR_ENCODE)
    {
	return udrs->putbytes;
    }
    else if(udrs->udr_op == UDR_DECODE)
    {
	return udrs->getbytes;
    }
    else
    {	udr_errno =  UDIErrorIPCInternal;
	return 0;
    }
}

/****************************************************************** UDR_SETPOS
*/
int	udr_setpos(udrs, pos)
UDR*	udrs;
char*	pos;
{
    if( ((UDIUInt32)pos > (UDIUInt32)(udrs->buff) + (UDIUInt32)(udrs->bufsize))
     || ((UDIUInt32)pos < (UDIUInt32)(udrs->buff) ) )
    {	udr_errno =  UDIErrorIPCInternal;
	return 0;
    }
    if(udrs->udr_op == UDR_ENCODE)
    {
	udrs->putbytes = pos;
	return 1;
    }
    else if(udrs->udr_op == UDR_DECODE)
    {
	udrs->getbytes = pos;
	return 1;
    }
    else
    {	udr_errno =  UDIErrorIPCInternal;
	return 0;
    }
}

/***************************************************************** UDR_READNOW
* Try and ensure "size" bytes are available in the
* receive buffer character stream.
*/
int	udr_readnow(udrs, size)
UDR*	udrs;
int	size;
{
    int	cnt, remain;

    if(udrs->udr_op == UDR_ENCODE)
    {
	udr_errno =  UDIErrorIPCInternal;
	return -1;
    }
    else if(udrs->udr_op == UDR_DECODE)
    {
    	if( (UDIUInt32)(udrs->putbytes)-(UDIUInt32)(udrs->getbytes) < size )
	{		 /* need more data in character stream buffer */
       	    remain = (UDIUInt32)(udrs->bufsize) -
		( (UDIUInt32)(udrs->putbytes)-(UDIUInt32)(udrs->buff) );
    	    cnt = read(udrs->sd, (char*)udrs->putbytes, remain);
	    if(cnt == -1) perror("ERROR udr_work(),  read() failure: ");
	    udrs->putbytes += cnt;
    	    if( (UDIUInt32)(udrs->putbytes)-(UDIUInt32)(udrs->getbytes) < size )
    	    {  fprintf(stderr,"ERROR, udr_readnow() too few bytes in stream\n");
	       return -1;		/* return error code */
    	    }
    	}		
    }
    else
    {	udr_errno =  UDIErrorIPCInternal;
	return -1;
    }
    return 0;
}

/******************************************************************* UDR_ALIGN
*/
int udr_align(udrs, size)
UDR*	udrs;
int	size;
{
    char*   align;
    int	    offset;	

    align = udr_getpos(udrs);
    offset = size - ((int)align & (size -1));
    offset = offset & (size -1);
    if(offset) udr_setpos(udrs, align + offset);
}
#endif /* __GO32__ */
