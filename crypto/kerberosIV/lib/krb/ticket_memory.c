/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* ticket_memory.c - Storage for tickets in memory
 * Author: d93-jka@nada.kth.se - June 1996
 */

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "krb_locl.h"
#include "ticket_memory.h"

RCSID("$Id: ticket_memory.c,v 1.9 1997/04/20 18:07:36 assar Exp $");

void msg(char *text, int error);

/* Global variables for memory mapping. */
HANDLE	SharedMemoryHandle;
tktmem	*SharedMemory;

static int CredIndex = -1;

int
newTktMem(const char *tf_name)
{
    if(!SharedMemory)
    {
	unsigned int MemorySize = sizeof(tktmem);
	unsigned int MemorySizeHi = sizeof(tktmem)>>16;
	unsigned int MemorySizeLo = MemorySize&0xFFFF;
	SharedMemoryHandle = CreateFileMapping((HANDLE)(int)-1, 0,
					       PAGE_READWRITE,
					       MemorySizeHi, MemorySizeLo,
					       "krb_memory");

	if(!SharedMemoryHandle)
	{
	    msg("Could not create shared memory.", GetLastError());
	    return KFAILURE;
	}
		
	SharedMemory = MapViewOfFile(SharedMemoryHandle,
				     FILE_MAP_WRITE, 0, 0, 0);
	if(!SharedMemory)
	{
	    msg("Unable to alloc shared memory.", GetLastError());
	    return KFAILURE;
	}
	if(GetLastError() != ERROR_ALREADY_EXISTS)
	{
	    if(tf_name)
		strcpy(SharedMemory->tmname, tf_name);
	    SharedMemory->last_cred_no = 0;
	}
    }
	
    CredIndex = 0;
    return KSUCCESS;
}

int
freeTktMem(const char *tf_name)
{
    if(SharedMemory)
    {
	UnmapViewOfFile(SharedMemory);
	CloseHandle(SharedMemoryHandle);
    }
    return KSUCCESS;
}



tktmem *
getTktMem(const char *tf_name)
{
    return SharedMemory;
}

void
firstCred(void)
{
    if(getTktMem(0)->last_cred_no > 0)
	CredIndex = 0;
    else
	CredIndex = -1;
}
	
int
nextCredIndex(void)
{
    const tktmem *mem;
    int last;
    mem = getTktMem(0);
    last = mem->last_cred_no;
    if(CredIndex >= 0 && CredIndex < last )
	return CredIndex++;
    else
	return CredIndex = -1;
}

int
currCredIndex(void)
{
    const tktmem *mem;
    int last;
    mem = getTktMem(0);
    last = mem->last_cred_no;
    if(CredIndex >= 0 && CredIndex < last)
	return CredIndex;
    else
	return CredIndex = -1;
}

int
nextFreeIndex(void)
{
    tktmem *mem = getTktMem(0);
    if(mem->last_cred_no > CRED_VEC_SZ)
	return -1;
    else
	return mem->last_cred_no++;
}

/*
 * in_tkt() is used to initialize the ticket store.  It creates the
 * file to contain the tickets and writes the given user's name "pname"
 * and instance "pinst" in the file.  in_tkt() returns KSUCCESS on
 * success, or KFAILURE if something goes wrong.
 */

int
in_tkt(char *pname, char *pinst)
{
    /* Here goes code to initialize shared memory, to store tickets in. */
    /* Implemented somewhere else. */
    return KFAILURE;
}

/*
 * dest_tkt() is used to destroy the ticket store upon logout.
 * If the ticket file does not exist, dest_tkt() returns RET_TKFIL.
 * Otherwise the function returns RET_OK on success, KFAILURE on
 * failure.
 *
 * The ticket file (TKT_FILE) is defined in "krb.h".
 */

int
dest_tkt(void)
{
    /* Here goes code to destroy tickets in shared memory. */
    /* Not implemented yet. */
    return KFAILURE;
}

/* Short description of routines:
 *
 * tf_init() opens the ticket file and locks it.
 *
 * tf_get_pname() returns the principal's name.
 *
 * tf_put_pname() writes the principal's name to the ticket file.
 *
 * tf_get_pinst() returns the principal's instance (may be null).
 *
 * tf_put_pinst() writes the instance.
 *
 * tf_get_cred() returns the next CREDENTIALS record.
 *
 * tf_save_cred() appends a new CREDENTIAL record to the ticket file.
 *
 * tf_close() closes the ticket file and releases the lock.
 *
 * tf_gets() returns the next null-terminated string.  It's an internal
 * routine used by tf_get_pname(), tf_get_pinst(), and tf_get_cred().
 *
 * tf_read() reads a given number of bytes.  It's an internal routine
 * used by tf_get_cred().
 */

/*
 * tf_init() should be called before the other ticket file routines.
 * It takes the name of the ticket file to use, "tf_name", and a
 * read/write flag "rw" as arguments. 
 *
 * Returns KSUCCESS if all went well, otherwise one of the following: 
 *
 * NO_TKT_FIL   - file wasn't there
 * TKT_FIL_ACC  - file was in wrong mode, etc.
 * TKT_FIL_LCK  - couldn't lock the file, even after a retry
 */

int
tf_init(char *tf_name, int rw)
{
    if(!getTktMem(tf_name))
	return NO_TKT_FIL;
    firstCred();
    return KSUCCESS;
}

/*
 * tf_create() should be called when creating a new ticket file.
 * The only argument is the name of the ticket file.
 * After calling this, it should be possible to use other tf_* functions.
 */

int
tf_create(char *tf_name)
{
    if(newTktMem(tf_name) != KSUCCESS)
	return NO_TKT_FIL;
    return KSUCCESS;
}

/*
 * tf_get_pname() reads the principal's name from the ticket file. It
 * should only be called after tf_init() has been called.  The
 * principal's name is filled into the "p" parameter.  If all goes well,
 * KSUCCESS is returned.  If tf_init() wasn't called, TKT_FIL_INI is
 * returned.  If the name was null, or EOF was encountered, or the name
 * was longer than ANAME_SZ, TKT_FIL_FMT is returned. 
 */

int
tf_get_pname(char *p)
{
    tktmem *TktStore;

    if(!(TktStore =  getTktMem(0)))
	return KFAILURE;
    if(!TktStore->pname)
	return KFAILURE;
    strcpy(p, TktStore->pname);
    return KSUCCESS;
}

/*
 * tf_put_pname() sets the principal's name in the ticket file. Call
 * after tf_create().
 */

int
tf_put_pname(char *p)
{
    tktmem *TktStore;

    if(!(TktStore =  getTktMem(0)))
	return KFAILURE;
    if(!TktStore->pname)
	return KFAILURE;
    strcpy(TktStore->pname, p);
    return KSUCCESS;
}

/*
 * tf_get_pinst() reads the principal's instance from a ticket file.
 * It should only be called after tf_init() and tf_get_pname() have been
 * called.  The instance is filled into the "inst" parameter.  If all
 * goes well, KSUCCESS is returned.  If tf_init() wasn't called,
 * TKT_FIL_INI is returned.  If EOF was encountered, or the instance
 * was longer than ANAME_SZ, TKT_FIL_FMT is returned.  Note that the
 * instance may be null. 
 */

int
tf_get_pinst(char *inst)
{
    tktmem *TktStore;

    if(!(TktStore =  getTktMem(0)))
	return KFAILURE;
    if(!TktStore->pinst)
	return KFAILURE;
    strcpy(inst, TktStore->pinst);
    return KSUCCESS;
}

/*
 * tf_put_pinst writes the principal's instance to the ticket file.
 * Call after tf_create.
 */

int
tf_put_pinst(char *inst)
{
    tktmem *TktStore;

    if(!(TktStore =  getTktMem(0)))
	return KFAILURE;
    if(!TktStore->pinst)
	return KFAILURE;
    strcpy(TktStore->pinst, inst);
    return KSUCCESS;
}

/*
 * tf_get_cred() reads a CREDENTIALS record from a ticket file and fills
 * in the given structure "c".  It should only be called after tf_init(),
 * tf_get_pname(), and tf_get_pinst() have been called. If all goes well,
 * KSUCCESS is returned.  Possible error codes are: 
 *
 * TKT_FIL_INI  - tf_init wasn't called first
 * TKT_FIL_FMT  - bad format
 * EOF          - end of file encountered
 */

int
tf_get_cred(CREDENTIALS *c)
{
    int index;
    CREDENTIALS *cred;
    tktmem *TktStore;

    if(!(TktStore =  getTktMem(0)))
	return KFAILURE;
    if((index = nextCredIndex()) == -1)
	return EOF;
    if(!(cred = TktStore->cred_vec+index))
       return KFAILURE;
    if(!c)
	return KFAILURE;
    memcpy(c, cred, sizeof(*c));
    return KSUCCESS;
}

/*
 * tf_close() closes the ticket file and sets "fd" to -1. If "fd" is
 * not a valid file descriptor, it just returns.  It also clears the
 * buffer used to read tickets.
 */

void
tf_close(void)
{
}

/*
 * tf_save_cred() appends an incoming ticket to the end of the ticket
 * file.  You must call tf_init() before calling tf_save_cred().
 *
 * The "service", "instance", and "realm" arguments specify the
 * server's name; "session" contains the session key to be used with
 * the ticket; "kvno" is the server key version number in which the
 * ticket is encrypted, "ticket" contains the actual ticket, and
 * "issue_date" is the time the ticket was requested (local host's time).
 *
 * Returns KSUCCESS if all goes well, TKT_FIL_INI if tf_init() wasn't
 * called previously, and KFAILURE for anything else that went wrong.
 */
 
int
tf_save_cred(char *service,	/* Service name */
	     char *instance,	/* Instance */
	     char *realm,	/* Auth domain */
	     unsigned char *session, /* Session key */
	     int lifetime,	/* Lifetime */
	     int kvno,		/* Key version number */
	     KTEXT ticket,	/* The ticket itself */
	     u_int32_t issue_date) /* The issue time */
{
    CREDENTIALS *cred;
    tktmem *mem =  getTktMem(0);
    int last = nextFreeIndex();

    if(last == -1)
	return KFAILURE;
    cred = mem->cred_vec+last;
    strcpy(cred->service, service);
    strcpy(cred->instance, instance);
    strcpy(cred->realm, realm);
    strcpy(cred->session, session);
    cred->lifetime = lifetime;
    cred->kvno = kvno;
    memcpy(&(cred->ticket_st), ticket, sizeof(*ticket));
    cred->issue_date = issue_date;
    strcpy(cred->pname, mem->pname);
    strcpy(cred->pinst, mem->pinst);
    return KSUCCESS;
}


int
tf_setup(CREDENTIALS *cred, char *pname, char *pinst)
{
    int ret;
    ret = tf_create(tkt_string());
    if (ret != KSUCCESS)
	return ret;

    if (tf_put_pname(pname) != KSUCCESS ||
	tf_put_pinst(pinst) != KSUCCESS) {
	tf_close();
	return INTK_ERR;
    }

    ret = tf_save_cred(cred->service, cred->instance, cred->realm, 
		       cred->session, cred->lifetime, cred->kvno,
		       &cred->ticket_st, cred->issue_date);
    tf_close();
    return ret;
}
