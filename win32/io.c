/*$Header: /p/tcsh/cvsroot/tcsh/win32/io.c,v 1.9 2006/04/13 00:59:02 amold Exp $*/
/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
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
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * io.c
 * wrapper functions for some i/o routines.
 * -amol
 *
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <fcntl.h>
#include <memory.h>
#include <errno.h>
#include "sh.h"
#include "ntport.h"
#include "signal.h"


#pragma warning(disable:4127) //conditional expr is constant

#define CR 0x0d


extern void make_err_str(unsigned int ,char *,int ) ;
extern void generic_handler(int);
extern int console_write(HANDLE,unsigned char*,int);

int consoleread(HANDLE , unsigned char * ,size_t ) ;

INPUT_RECORD girec[2048];

unsigned short __nt_want_vcode=0,__nt_vcode=0;
HANDLE __h_con_alarm=0;
HANDLE __h_con_int=0;
HANDLE __h_con_hup=0;

extern int NoNLSRebind;

extern int OLDSTD, SHIN;
/* 
 * force_read: Forces a ReadFile, instead of ReadConsole 
 *
 */
int force_read(int fd, unsigned char * buf, size_t howmany) {
    DWORD numread=0,err=0;
    HANDLE hRead ;


    hRead= (HANDLE)__nt_get_osfhandle(fd);
    if (hRead == INVALID_HANDLE_VALUE) {
	return 0;
    }
again:
    if (!ReadFile(hRead, buf,(DWORD)howmany,&numread, NULL ) ){
	err = GetLastError();
	switch(err) {
	    case ERROR_IO_PENDING:
		break;
	    case ERROR_ACCESS_DENIED:
	    case ERROR_INVALID_HANDLE:
		errno = EBADF;
		return -1;
		break;
	    case ERROR_HANDLE_EOF:
	    case ERROR_BROKEN_PIPE:
		errno = 0;
		return 0;
	    default:
		errno = EBADF;
		return 0;
	}
    }
    if (numread == 1 && buf[0] == CR)
	goto again;
    return numread;
}
int nt_read(int fd, unsigned char * buf, size_t howmany) {

    DWORD numread=0,err=0;
    HANDLE hRead ;
    DWORD ftype;
    //

    hRead= (HANDLE)__nt_get_osfhandle(fd);
    if (hRead == INVALID_HANDLE_VALUE) {
	return 0;
    }

    ftype = GetFileType(hRead);


    if ((ftype == FILE_TYPE_CHAR) /*&& (fd != OLDSTD) && (fd != SHIN)*/)
	return consoleread(hRead,buf,howmany);
again:
    if (!ReadFile(hRead, buf,(DWORD)howmany,&numread, NULL ) ){
	err = GetLastError();
	switch(err) {
	    case ERROR_IO_PENDING:
		break;
	    case ERROR_ACCESS_DENIED:
	    case ERROR_INVALID_HANDLE:
		errno = EBADF;
		return -1;
		break;
	    case ERROR_HANDLE_EOF:
	    case ERROR_BROKEN_PIPE:
		errno = 0;
		return 0;
	    default:
		errno = EBADF;
		return 0;
	}
    }
    if (numread) {
	if (buf[numread-1] == CR) 
	    numread--;
	if (numread == 0)
	    goto again;
    }
    return numread;
}

/* color-ls patches from TAGA nayuta (nayuta@is.s.u-tokyo.ac.jp) */
#ifdef COLOR_LS_F

int nt_write_(int , const unsigned char * , size_t );
int nt_write(int fd, const unsigned char * buf, size_t howmany) {
    static unsigned char color_buf[256];
    static char len = 0;

    ssize_t i;
    ssize_t start = 0;
    int rc,wrote = 0;

    if (!isatty(fd) || (varval(STRcolor) == NULL))
	return nt_write_(fd, buf, howmany);

    for (i = 0; i < howmany; i++) {
	switch (len) {
	    case 0:
		if (buf[i] == '\x1b') {
		    color_buf[len++] = buf[i];
		    if (0 < i - start){
			if ((rc=nt_write_(fd, &(buf[start]), i - start)) <0)
			    return -1;
			else
			    wrote += rc;
		    }
		    start = -1;
		}
		break;

	    case 1:
		if (buf[i] != '[')
		    goto set_color;
		color_buf[len++] = buf[i];
		break;

	    default:
		if (buf[i] == 'm' || (!isdigit(buf[i]) && buf[i] != ';'))
		    goto set_color;
		color_buf[len++] = buf[i];
		break;

	    case sizeof(color_buf) - 1:
set_color:
		color_buf[len] = '\0';
		set_attributes(color_buf);
		len = 0;
		start = i + 1;
		break;
	}
    }

    if (0 < i - start && 0 <= start) {
	if ((rc=nt_write_(fd, &(buf[start]), i - start)) < 0)
	    return -1;
	else
	    wrote += rc;
    }
    return wrote;
}
int nt_write_(int fd, const unsigned char * buf, size_t howmany)
#else /* if !COLOR_LS_F */
int nt_write(int fd, const unsigned char * buf, size_t howmany)
#endif /* COLOR_LS_F */
{
    int bytes_rtn,err;
    HANDLE hout;


    hout = (HANDLE)__nt_get_osfhandle(fd);
    /*
       if (isatty(fd)) 
       ;//		return console_write(hout,buf,howmany);
     */

    if(!WriteFile(hout, buf,(DWORD)howmany,(ULONG*)&bytes_rtn,
		NULL)){
	err = GetLastError();
	switch(err) {
	    case ERROR_ACCESS_DENIED:
	    case ERROR_INVALID_HANDLE:
		errno = EBADF;
		return -1;
		break;
	    case ERROR_BROKEN_PIPE:
		errno = EPIPE;
		return -1;
	    default:
		errno = EBADF;
		return -1;
	}

    }
    return bytes_rtn?bytes_rtn:-1;

}

#define IS_CTRL_COMBO(a) ( (a) & ( RIGHT_CTRL_PRESSED |  LEFT_CTRL_PRESSED) ) 
#define IS_ALT_COMBO(a) ( /*(a) &*/ alt_pressed ) 
#define IS_SHIFT_COMBO(a) ( (a) & SHIFT_PRESSED)

int consoleread(HANDLE hInput, unsigned char * buf,size_t howmany) {

	INPUT_RECORD *irec = NULL;
	DWORD numread,controlkey,i;
	WORD vcode;
	unsigned char ch;
	int rc;
	size_t where=0;
	int alt_pressed = 0,memfree=0;
	HANDLE hevents[4];
	static int pre_ch = -1;

	if (0 <= pre_ch) {
		buf[0] = (unsigned char)pre_ch;
		pre_ch = -1;
		return 1;
	}

	howmany /= 2; // [ALT + KEY] is expanded ESC KEY, so we need more buffer
	if (howmany == 0)
		howmany = 1;

	if (howmany > 2048){
		irec = heap_alloc(howmany*sizeof(INPUT_RECORD));
		memfree=1;
	}
	else
		irec = &(girec[0]);
	if (!irec){
		errno = ENOMEM;
		return -1;
	}
	while(1) {
		hevents[0] = __h_con_alarm;
		hevents[1] = __h_con_int;
		hevents[2] = __h_con_hup;
		hevents[3] = hInput;
		rc = WaitForMultipleObjects(sizeof(hevents)/sizeof(hevents[0]),
					    hevents,FALSE,INFINITE);
		if (rc == WAIT_OBJECT_0) {
			generic_handler(SIGALRM);
		}
		if (rc == (WAIT_OBJECT_0 +1) ) {
			errno = EINTR;
			generic_handler(SIGINT);
			break;
		}
		if (rc == (WAIT_OBJECT_0 +2) ) {
			errno = EINTR;
			generic_handler(SIGHUP);
			break;
		}
		rc = ReadConsoleInput(hInput,irec,(DWORD)howmany,&numread);
		if (!rc) {
			rc = GetLastError();
			switch (rc) {
				case ERROR_INVALID_HANDLE:
				case ERROR_ACCESS_DENIED:
					errno = EBADF;
					break;
			}
			if (memfree)
				heap_free(irec);
			return -1;
		}
		__nt_vcode=0;
		for(i=0;i<numread;i++) {
			switch(irec[i].EventType) {
				case KEY_EVENT:
					if (irec[i].Event.KeyEvent.bKeyDown) {
						vcode=(irec[i].Event.KeyEvent.wVirtualKeyCode);
						ch=(irec[i].Event.KeyEvent.uChar.AsciiChar);
						controlkey=(irec[i].Event.KeyEvent.dwControlKeyState);
						if (controlkey & LEFT_ALT_PRESSED)
							alt_pressed=1;
						else if (controlkey & RIGHT_ALT_PRESSED){
							if (NoNLSRebind)
								alt_pressed=1;
						}

						if (__nt_want_vcode != 1)
							goto skippy;

						if (vcode >= VK_F1 && vcode <= VK_F24) {

							__nt_vcode=NT_SPECIFIC_BINDING_OFFSET ;
							__nt_vcode += (vcode- VK_F1) + SINGLE_KEY_OFFSET;

							if (IS_CTRL_COMBO(controlkey))
								__nt_vcode += CTRL_KEY_OFFSET;

							else if (IS_ALT_COMBO(controlkey))
								__nt_vcode += ALT_KEY_OFFSET;
							else if (IS_SHIFT_COMBO(controlkey))
								__nt_vcode += SHIFT_KEY_OFFSET;

							__nt_want_vcode=2;

							return 1;
						}
						else if (vcode>= VK_PRIOR && vcode <= VK_DOWN) {

							__nt_vcode  = NT_SPECIFIC_BINDING_OFFSET ;
							__nt_vcode += KEYPAD_MAPPING_BEGIN;
							__nt_vcode += (vcode -VK_PRIOR);	

							__nt_vcode += SINGLE_KEY_OFFSET ;

							if (IS_CTRL_COMBO(controlkey))
								__nt_vcode += CTRL_KEY_OFFSET;

							else if (IS_ALT_COMBO(controlkey))
								__nt_vcode += ALT_KEY_OFFSET;
							else if (IS_SHIFT_COMBO(controlkey))
								__nt_vcode += SHIFT_KEY_OFFSET;

							__nt_want_vcode=2;
							return 1;
						}
						else if (vcode == VK_INSERT) {
							__nt_vcode  = NT_SPECIFIC_BINDING_OFFSET ;
							__nt_vcode += INS_DEL_MAPPING_BEGIN;

							if (IS_CTRL_COMBO(controlkey))
								__nt_vcode += CTRL_KEY_OFFSET;

							else if (IS_ALT_COMBO(controlkey))
								__nt_vcode += ALT_KEY_OFFSET;

							else if (IS_SHIFT_COMBO(controlkey))
								__nt_vcode += SHIFT_KEY_OFFSET;

							__nt_want_vcode=2;
							return 1;
						}
						else if (vcode == VK_DELETE) {
							__nt_vcode  = NT_SPECIFIC_BINDING_OFFSET ;
							__nt_vcode += INS_DEL_MAPPING_BEGIN + 1;

							if (IS_CTRL_COMBO(controlkey))
								__nt_vcode += CTRL_KEY_OFFSET;

							else if (IS_ALT_COMBO(controlkey))
								__nt_vcode += ALT_KEY_OFFSET;

							else if (IS_SHIFT_COMBO(controlkey))
								__nt_vcode += SHIFT_KEY_OFFSET;

							__nt_want_vcode=2;

							return 1;
						}
skippy:
						switch(vcode) {
							case VK_ESCAPE:
								buf[where++]='\033';
								break;
							default:
								if(ch ){
									/* 
									 * Looks like win95 has a spurious
									 * newline left over
									 */
									if (gdwPlatform ==
											VER_PLATFORM_WIN32_WINDOWS && 
											ch == '\r'){
										DWORD bread;
										(void)ReadFile(hInput,&ch,1,&bread,NULL);
									}
									/* patch from TAGA nayuta */
									if ( NoNLSRebind  &&
											(ch == ' ' || ch == '@') &&
											IS_CTRL_COMBO(controlkey)
											/*(controlkey & LEFT_CTRL_PRESSED ||
											  controlkey & RIGHT_CTRL_PRESSED)*/
									   )
										ch = 0;
									if (alt_pressed) {
#ifdef DSPMBYTE
										buf[where++] = '\033';
										if (howmany == 1)
											pre_ch = ch;
										else
											buf[where++] = ch;
#else /* !DSPMBYTE */
										buf[where++] = ch | 0200;
#endif /* !DSPMBYTE */
									}
									else
										buf[where++] = ch;
								}
								break;
						}

						alt_pressed=0;
					}
					break;
				default:
					break;
			}
		}
		if (where == 0)
			continue;
		if (howmany < where) // avoid trashing memory. -amol 4/16/97
			buf[where]=0;
		break;
	}
	if (memfree)
		heap_free(irec);
	if (!where)
		return -1;
	return (int)(where );
}
int console_write(HANDLE hout, unsigned char * buf,int howmany) {
    int bytes,rc;

    bytes = -1;

    rc = WriteConsole(hout,buf,howmany,(DWORD*)&bytes,NULL);
    if (!rc) {
	errno = EBADF;
	bytes = -1;
	rc = GetLastError();
    }

    return bytes;
}
