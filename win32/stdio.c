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
 * stdio.c Implement a whole load of i/o functions.
 *         This makes it much easier to keep track of inherited handles and
 *         also makes us reasonably vendor crt-independent.
 * -amol
 *
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#define STDIO_C
#include <ntport.h>
#include <forkdata.h>


#define __MAX_OPEN_FILES 64

#define FIOCLEX 1
#define FCONSOLE 2

typedef struct _myfile {
	HANDLE  handle;
	unsigned long flags;
} MY_FILE;

typedef unsigned long u_long;

#define INVHL (INVALID_HANDLE_VALUE)

MY_FILE __gOpenFiles[__MAX_OPEN_FILES]={0};
MY_FILE __gOpenFilesCopy[__MAX_OPEN_FILES]={0};

MY_FILE *my_stdin=0, *my_stdout=0, *my_stderr=0;

extern int didfds;
int __dup_stdin = 0;


void init_stdio(void) {

	int i;
	__gOpenFiles[0].handle = GetStdHandle(STD_INPUT_HANDLE);
	__gOpenFiles[1].handle = GetStdHandle(STD_OUTPUT_HANDLE);
	__gOpenFiles[2].handle = GetStdHandle(STD_ERROR_HANDLE);

	__gOpenFiles[0].flags = (GetFileType(ULongToPtr(STD_INPUT_HANDLE))== 
			FILE_TYPE_CHAR)?  FCONSOLE:0;
	__gOpenFiles[1].flags = (GetFileType(ULongToPtr(STD_OUTPUT_HANDLE))== 
			FILE_TYPE_CHAR)?  FCONSOLE:0;
	__gOpenFiles[2].flags = (GetFileType(ULongToPtr(STD_ERROR_HANDLE))==
			FILE_TYPE_CHAR)?  FCONSOLE:0;

	for(i=3;i<__MAX_OPEN_FILES;i++) {
		__gOpenFiles[i].handle = INVHL;
		__gOpenFilesCopy[i].handle = INVHL;
		__gOpenFiles[i].flags = 0;
	}

	my_stdin = &__gOpenFiles[0];
	my_stdout = &__gOpenFiles[1];
	my_stderr = &__gOpenFiles[2];
}

	void nt_close_on_exec(int fd, int on) {
		if(on)
			__gOpenFiles[fd].flags |= FIOCLEX;
		else
			__gOpenFiles[fd].flags &= ~FIOCLEX;
	}
void restore_fds(void ) {
	int i;
	int min=3;

	if (__forked && (didfds|| __dup_stdin))
		min =0;
	//
	// ok for tcsh. see fork.c for why
	//
	__gOpenFiles[0].handle = INVHL;
	__gOpenFiles[1].handle = INVHL;
	__gOpenFiles[2].handle = INVHL;
	my_stdin = &__gOpenFiles[0];
	my_stdout = &__gOpenFiles[1];
	my_stderr = &__gOpenFiles[2];
	for(i=min;i<__MAX_OPEN_FILES;i++) {
		if (__gOpenFilesCopy[i].handle == INVHL)
			continue;
		__gOpenFiles[i].handle = __gOpenFilesCopy[i].handle ;
		__gOpenFiles[i].flags = __gOpenFilesCopy[i].flags ;
	}
}
void close_copied_fds(void ) {
	int i;
	int min=3;
	if (didfds|| __dup_stdin)
		min =0;
	for(i=min;i<__MAX_OPEN_FILES;i++) {
		if (__gOpenFilesCopy[i].handle == INVHL)
			continue;
		CloseHandle((HANDLE)__gOpenFilesCopy[i].handle);
		__gOpenFilesCopy[i].handle = INVHL;
	}
	__dup_stdin=0;
}
void copy_fds(void ) {
	int i;
	int min=3;
	if (didfds || __dup_stdin)
		min =0;
	for(i=min;i<__MAX_OPEN_FILES;i++) {
		if (__gOpenFiles[i].handle == INVHL) {
			__gOpenFilesCopy[i].handle = INVHL;
			continue;
		}

		if(!DuplicateHandle(GetCurrentProcess(), 
					(HANDLE)__gOpenFiles[i].handle ,
					GetCurrentProcess(), 
					(HANDLE*)&__gOpenFilesCopy[i].handle,
					0, TRUE, DUPLICATE_SAME_ACCESS) )
			__gOpenFilesCopy[i].handle = INVHL;
		__gOpenFilesCopy[i].flags = __gOpenFiles[i].flags;
	}
}
intptr_t __nt_get_osfhandle(int fd) {
	return (intptr_t)(__gOpenFiles[fd].handle);
}
int __nt_open_osfhandle(intptr_t h1, int mode) {
	int i;

	UNREFERENCED_PARAMETER(mode);

	for(i=0;i<__MAX_OPEN_FILES;i++) {
		if (__gOpenFiles[i].handle == INVHL) {
			__gOpenFiles[i].handle = (HANDLE)h1;
			__gOpenFiles[i].flags = 0;
			return i;
		}
	}
	errno = EMFILE;
	return -1;
}
int nt_close(int fd) {

	if( (fd == -1) ||(__gOpenFiles[fd].handle == INVHL))
		return 0;
	CloseHandle((HANDLE)(__gOpenFiles[fd].handle));
	__gOpenFiles[fd].handle = INVHL;
	__gOpenFiles[fd].flags = 0;

	//	dprintf("closing 0x%08x\n",(__gOpenFiles[fd].handle));
	return 0;
}
int nt_access(char *filename, int mode) {

	DWORD attribs=(DWORD)-1, bintype;
	int tries=0;
	char buf[512];/*FIXBUF*/

	if (!filename) {
		errno = ENOENT;
		return -1;
	}
	(void)StringCbPrintf(buf,sizeof(buf),"%s",filename);
retry:
	attribs = GetFileAttributes(buf);
	tries++;

	if (attribs == (DWORD) -1) {
		if( (GetLastError() == ERROR_FILE_NOT_FOUND) && (mode & X_OK) ) {
			switch(tries){
				case 1:
					(void)StringCbPrintf(buf,sizeof(buf),"%s.exe",filename);
					break;
				case 2:
					(void)StringCbPrintf(buf,sizeof(buf),"%s.cmd",filename);
					break;
				case 3:
					(void)StringCbPrintf(buf,sizeof(buf),"%s.bat",filename);
					break;
				case 4:
					(void)StringCbPrintf(buf,sizeof(buf),"%s.com",filename);
					break;
				default:
					goto giveup;
					break;
			}
			goto retry;
		}
	}
giveup:
	if (attribs == (DWORD)-1 ) {
		errno = EACCES;
		return -1;
	}
	if ( (mode & W_OK) &&  (attribs & FILE_ATTRIBUTE_READONLY) ) {
		errno = EACCES;
		return -1;
	}
	if (mode & X_OK) {
		if ((mode & XD_OK) && (attribs & FILE_ATTRIBUTE_DIRECTORY) ){
			errno = EACCES;
			return -1;
		}
		if ((!(attribs & FILE_ATTRIBUTE_DIRECTORY)) && 
				!GetBinaryType(buf,&bintype) &&(tries >4) ) {
			errno = EACCES;
			return -1;
		}
	}
	return 0;
}
int nt_seek(HANDLE h1, long offset, int how) {
	DWORD dwmove;

	switch(how) {
		case SEEK_CUR:
			dwmove = FILE_CURRENT;
			break;
		case SEEK_END:
			dwmove = FILE_END;
			break;
		case SEEK_SET:
			dwmove = FILE_BEGIN;
			break;
		default:
			errno = EINVAL;
			return -1;
	}

	if (SetFilePointer(h1,offset,NULL,dwmove) == -1){
		errno = EBADF;
		return -1;
	}
	return 0;
}
int nt_lseek(int fd,long offset, int how) {
	HANDLE h1 ; 
	h1 =__gOpenFiles[fd].handle;
	return nt_seek(h1,offset,how);
}
int nt_isatty(int fd) {
	return (__gOpenFiles[fd].flags & FCONSOLE);
}
int nt_dup(int fdin) {

	HANDLE hdup;
	HANDLE horig =  __gOpenFiles[fdin].handle;
	int ret;


	if (!DuplicateHandle(GetCurrentProcess(),
				horig,
				GetCurrentProcess(),
				&hdup,
				0,
				FALSE,
				DUPLICATE_SAME_ACCESS)) {
		errno = GetLastError();
		errno = EBADF;
		return -1;
	}
	ret = __nt_open_osfhandle((intptr_t)hdup,_O_BINARY | _O_NOINHERIT);

	__gOpenFiles[ret].flags = __gOpenFiles[fdin].flags;

	return  ret;
}
int nt_dup2(int fdorig,int fdcopy) {

	HANDLE hdup;
	HANDLE horig =  __gOpenFiles[fdorig].handle;


	if (__gOpenFiles[fdcopy].handle != INVHL) {
		CloseHandle((HANDLE)__gOpenFiles[fdcopy].handle );
		__gOpenFiles[fdcopy].handle = INVHL;
		__gOpenFiles[fdcopy].flags = 0;
	}
	if (!DuplicateHandle(GetCurrentProcess(),
				horig,
				GetCurrentProcess(),
				&hdup,
				0,
				fdcopy<3?TRUE:FALSE, DUPLICATE_SAME_ACCESS)) {
		errno = GetLastError();
		errno = EBADF;
		return -1;
	}
	__gOpenFiles[fdcopy].handle = hdup;
	__gOpenFiles[fdcopy].flags = __gOpenFiles[fdorig].flags;
	switch(fdcopy) {
		case 0:
			SetStdHandle(STD_INPUT_HANDLE,hdup);
			break;
		case 1:
			SetStdHandle(STD_OUTPUT_HANDLE,hdup);
			break;
		case 2:
			SetStdHandle(STD_ERROR_HANDLE,hdup);
			break;
		default:
			break;
	}

	return  0;
}
int nt_pipe2(HANDLE hpipe[2]) {

	SECURITY_ATTRIBUTES secd;

	secd.nLength=sizeof(secd);
	secd.lpSecurityDescriptor=NULL;
	secd.bInheritHandle=FALSE;

	return (!CreatePipe(&hpipe[0],&hpipe[1],&secd,0));
}
int nt_pipe(int hpipe[2]) {
	HANDLE hpipe2[2];

	nt_pipe2(hpipe2);
	hpipe[0] = __nt_open_osfhandle((intptr_t)hpipe2[0],O_NOINHERIT);
	hpipe[1] = __nt_open_osfhandle((intptr_t)hpipe2[1],O_NOINHERIT);
	return 0;
}
/* check if name is //server. if checkifShare is set,
 * also check if //server/share
 */
int is_server(const char *name,int checkifShare) {
	const char *p1, *p2;

	if (!*name || !*(name+1))
		return 0;

	p1 = name;
	if (((p1[0] != '/') && (p1[0] != '\\') ) ||
			((p1[1] != '/') && (p1[1] != '\\') ))
		return 0;

	p2 = p1 + 2;
	while (*p2 && *p2 != '/' && *p2 != '\\')
#ifdef DSPMBYTE
		if (Ismbyte1(*p2) && *(p2 + 1))
			p2 += 2;
		else
#endif /* DSPMBYTE */
			p2++;

	/* just check for server */
	if (!checkifShare) {
		/* null terminated unc server name */
		/* terminating '/' (//server/) is also ok */
		if (!*p2 || !*(p2+1)) 
			return 1;

	}
	else {
		if (!*p2 || !*(p2+1))
			return 0;
		p2++;
		while(*p2 && *p2 != '/' && *p2 != '\\')
			p2++;
		if (!*p2 || !*(p2+1))
			return 1;
	}
	return 0;

}
__inline int is_unc(char *filename) {
	if (*filename && (*filename == '/' || *filename == '\\')
			&& *(filename+1) 
			&& (*(filename+1) == '/' || *(filename+1) == '\\')) {
		return 1;
	}
	return 0;
}
int nt_stat(const char *filename, struct stat *stbuf) {

	// stat hangs on server name 
	// Use any  directory, since the info in stat means %$!* on
	// windows anyway.
	// -amol 5/28/97
	/* is server or share */
	if (is_server(filename,0)  || is_server(filename,1) ||
			(*(filename+1) && *(filename+1) == ':' && !*(filename+2)) ) {
		return _stat("C:/",(struct _stat *)stbuf);
	}
        else  {
	    size_t len = strlen(filename);
            char *last = (char*)filename + len - 1;
            int rc;
	    /* Possible X: and X:/ strings */
	    BOOL root = (len <= 3 && *(filename + 1) == ':');
	    /* exclude X:/ strings */
	    BOOL lastslash = ((*last == '/') && !root);
            if(lastslash)
                *last = '\0';
            rc = _stat(filename,(struct _stat *)stbuf);
            if(lastslash)
                *last = '/';
            return rc;
        }
}
//
// replacement for creat that makes handle non-inheritable. 
// -amol 
//
int nt_creat(const char *filename, int mode) {
	// ignore the bloody mode

	int fd = 0,is_cons =0;
	HANDLE retval;
	SECURITY_ATTRIBUTES security;

	UNREFERENCED_PARAMETER(mode);


	security.nLength = sizeof(security);
	security.lpSecurityDescriptor = NULL;
	security.bInheritHandle = FALSE;

	if (!_stricmp(filename,"/dev/tty") ){
		filename = "CONOUT$";
		is_cons = 1;
	}
	else if (!_stricmp(filename,"/dev/null") ){
		filename = "NUL";
	}
	retval = CreateFile(filename,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			is_cons?NULL:&security,
			CREATE_ALWAYS,
			0,
			NULL);

	if (retval == INVALID_HANDLE_VALUE) {
		errno = EACCES;
		return -1;
	}
	fd = __nt_open_osfhandle((intptr_t)retval,_O_BINARY);
	if (fd <0) {
		//should never happen
		abort();
	}
	else {
		if (is_cons) {
			__gOpenFiles[fd].flags = FCONSOLE;
		}
	}
	return fd;

}
int nt_open(const char *filename, int perms,...) { 

	// ignore the bloody mode

	int fd,mode, is_cons=0;
	HANDLE retval;
	SECURITY_ATTRIBUTES security;
	DWORD dwAccess, dwFlags, dwCreateDist;
	va_list ap;

	va_start(ap,perms);
	mode = va_arg(ap,int);
	va_end(ap);

	if (!lstrcmp(filename,"/dev/tty") ){
		if (perms == O_RDONLY) //o_rdonly is 0
			filename = "CONIN$";
		else if (perms & O_WRONLY)
			filename = "CONOUT$";
		is_cons = 1;
	}
	else if (!lstrcmp(filename,"/dev/null") ){
		filename = "NUL";
	}
	security.nLength = sizeof(security);
	security.lpSecurityDescriptor = NULL;
	security.bInheritHandle = FALSE;

	switch (perms & (_O_RDONLY | _O_WRONLY | _O_RDWR) ) {
		case _O_RDONLY:
			dwAccess = GENERIC_READ;
			break;
		case _O_WRONLY:
			dwAccess = GENERIC_WRITE;
			break;
		case _O_RDWR:
			dwAccess = GENERIC_READ | GENERIC_WRITE ;
			break;
		default:
			errno = EINVAL;
			return -1;
	}
	switch (perms & (_O_CREAT | _O_TRUNC) ){
		case 0:
			dwCreateDist = OPEN_EXISTING;
			break;
		case _O_CREAT:
			dwCreateDist = CREATE_ALWAYS;
			break;
		case _O_CREAT | _O_TRUNC:
			dwCreateDist = CREATE_ALWAYS;
			break;
		case _O_TRUNC:
			dwCreateDist = TRUNCATE_EXISTING;
			break;
		default:
			errno = EINVAL;
			return -1;
	}
	dwFlags = 0;
	if (perms & O_TEMPORARY)
		dwFlags = FILE_FLAG_DELETE_ON_CLOSE;
	retval = CreateFile(filename,
			dwAccess,//GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			&security,
			dwCreateDist,//CREATE_ALWAYS,
			dwFlags,
			NULL);

	if (retval == INVALID_HANDLE_VALUE) {
		int err = GetLastError();
		if (err == ERROR_FILE_NOT_FOUND)
			errno = ENOENT;
		else
			errno = EACCES;
		return -1;
	}
	if (perms & O_APPEND) {
		SetFilePointer(retval,0,NULL,FILE_END);
	}
	fd = __nt_open_osfhandle((intptr_t)retval,_O_BINARY);
	if (fd <0) {
		//should never happen
		abort();
	}
	else {
		if (is_cons) {
			__gOpenFiles[fd].flags = FCONSOLE;
		}
	}
	return fd;

}
/*
 * This should be the LAST FUNCTION IN THIS FILE 
 *
 */
#undef fstat
#undef _open_osfhandle
#undef close
int nt_fstat(int fd, struct stat *stbuf) {
	int realfd;
	HANDLE h1;

	errno = EBADF;

	if(!DuplicateHandle(GetCurrentProcess(),
				(HANDLE)__gOpenFiles[fd].handle,
				GetCurrentProcess(),
				&h1,
				0,
				FALSE,
				DUPLICATE_SAME_ACCESS) )
		return -1;
	realfd = _open_osfhandle((intptr_t)h1,0);
	if (realfd <0 ) 
		return -1;

	if( fstat(realfd,stbuf) <0 ) {
		_close(realfd);
		return -1;
	}
	_close(realfd);
	errno =0;
	return 0;

}

