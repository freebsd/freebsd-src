/*$Header: /p/tcsh/cvsroot/tcsh/win32/dirent.c,v 1.9 2006/04/07 00:57:59 amold Exp $*/
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

/* dirent.c
 * directory interface functions. Sort of like dirent functions on unix.
 * Also allow browsing network shares as if they were directories
 *
 * -amol
 *
 */
#define WIN32_LEAN_AND_MEAN
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <direct.h>
#include "dirent.h"
#include <winnetwk.h>

#ifndef WINDOWS_ONLY
#define STRSAFE_NO_DEPRECATE
#endif /* WINDOWS_ONLY*/
#define STRSAFE_LIB
#define STRSAFE_NO_CCH_FUNCTIONS
#include <strsafe.h>

#pragma intrinsic("memset")

static HANDLE open_enum(char *,WIN32_FIND_DATA*);
static void close_enum(DIR*) ;
static int enum_next_share(DIR*);

typedef struct _enum_h {
	unsigned char *netres;
	HANDLE henum;
} nethandle_t;

static int inode= 1; // useless piece that some unix programs need
DIR * opendir(const char *inbuf) {

    DIR *dptr;
    WIN32_FIND_DATA fdata = {0};
    char *tmp  = NULL;
    char *buf = NULL;
    int is_net=0;
    int had_error = 0;
    size_t buflen;

    buflen = lstrlen(inbuf) + 1;
    buf= (char *)heap_alloc(buflen); 
    (void)StringCbCopy(buf,buflen,inbuf);

    if (!buf)
	buf = "." ;
    tmp = buf;
    while(*tmp) {
#ifdef DSPMBYTE
	if (Ismbyte1(*tmp) && *(tmp + 1))
	    tmp ++;
	else
#endif DSPMBYTE
	    if (*tmp == '\\')
		*tmp = '/';
	tmp++;
    }
    /*
     * paths like / confuse NT because it looks like a UNC name
     * when we append "\*" -amol
     */
    if (*(tmp -1) == '/')
	*(tmp -1) = 0;

    buflen = lstrlen(buf) + 4;
    tmp= (char *)heap_alloc(buflen); 

    if ( (buf[0] == '/') && (buf[1] != '/') ) {
	(void)StringCbPrintf(tmp,buflen, "%c:%s*",
			     'A' + (_getdrive()-1),buf);
    }
    else if ( (buf[0] == '/')  &&  (buf[1] == '/')  ){
	is_net = 1;
	(void)StringCbPrintf(tmp,buflen,"%s",buf);
    }
    else { 
	(void)StringCbPrintf(tmp,buflen,"%s/*",buf);
    }

    dptr = (DIR *)heap_alloc(sizeof(DIR));
    dptr->dd_fd = INVALID_HANDLE_VALUE;
    if (!dptr){
	errno = ENOMEM;
	had_error =1;
	goto done;
    }

    if (is_net){
	dptr->dd_fd = open_enum(tmp,&fdata);
	dptr->flags = IS_NET;
    }
    if (dptr->dd_fd == INVALID_HANDLE_VALUE){
	(void)StringCbPrintf(tmp,buflen,"%s/*",buf);
	dptr->flags = 0;
	dptr->dd_fd = FindFirstFile(tmp,&fdata);
    }
    if (dptr->dd_fd == INVALID_HANDLE_VALUE){
	if (GetLastError() == ERROR_DIRECTORY)
	    errno = ENOTDIR;
	else
	    errno = ENOENT;	

	had_error =1;
	goto done;
    }
    memset(dptr->orig_dir_name,0,sizeof(dptr->orig_dir_name));
    memcpy(dptr->orig_dir_name,tmp,lstrlen(tmp));

    dptr->dd_loc = 0;
    dptr->dd_size = fdata.nFileSizeLow;
    dptr->dd_buf = (struct dirent *)heap_alloc(sizeof(struct dirent));
    if (!dptr->dd_buf){
	errno = ENOMEM;
	had_error=1;
	goto done;
    }
    (dptr->dd_buf)->d_ino = inode++;
    (dptr->dd_buf)->d_off = 0;
    (dptr->dd_buf)->d_reclen = 0;
    if (lstrcmpi(fdata.cFileName,".") ){
	//dptr->dd_buf->d_name[0] = '.';
	memcpy((dptr->dd_buf)->d_name,".",2);
	dptr->flags |= IS_ROOT;
    }
    else
	memcpy((dptr->dd_buf)->d_name,fdata.cFileName,MAX_PATH);

done:
    if(tmp)
	heap_free(tmp);
    if(had_error) {
	heap_free(dptr);
	dptr = NULL;
    }

    return dptr;
}
int closedir(DIR *dptr){

	if (!dptr)
		return 0;
	if (dptr->flags & IS_NET) {
		close_enum(dptr);
	}
	else
		FindClose(dptr->dd_fd);
	heap_free(dptr->dd_buf);
	heap_free(dptr);
	return 0;
}
void rewinddir(DIR *dptr) {

	HANDLE hfind;
	WIN32_FIND_DATA fdata;
	char *tmp = dptr->orig_dir_name;

	if (!dptr) return;

	if (dptr->flags & IS_NET) {
		hfind = open_enum(tmp,&fdata);
		close_enum(dptr);
		dptr->dd_fd = hfind;
	}
	else {
		hfind = FindFirstFile(tmp,&fdata);
		assert(hfind != INVALID_HANDLE_VALUE);
		FindClose(dptr->dd_fd);
		dptr->dd_fd = hfind;
	}
	dptr->dd_size = fdata.nFileSizeLow;
	(dptr->dd_buf)->d_ino = inode++;
	(dptr->dd_buf)->d_off = 0;
	(dptr->dd_buf)->d_reclen = 0;
	memcpy((dptr->dd_buf)->d_name,fdata.cFileName,MAX_PATH);
	return;
}
struct dirent *readdir(DIR *dir) {

	WIN32_FIND_DATA fdata = {0};
	HANDLE hfind;
	char *tmp ;

	if (!dir)
		return NULL;

	if (dir->flags & IS_NET) {
		if(enum_next_share(dir)<0)
			return NULL;
	}
	// special hack for root (which does not have . or ..)
	else if (dir->flags & IS_ROOT) {
		tmp= dir->orig_dir_name;
		hfind = FindFirstFile(tmp,&fdata);
		FindClose(dir->dd_fd);
		dir->dd_fd = hfind;
		dir->dd_size = fdata.nFileSizeLow;
		(dir->dd_buf)->d_ino = inode++;
		(dir->dd_buf)->d_off = 0;
		(dir->dd_buf)->d_reclen = 0;
		memcpy((dir->dd_buf)->d_name,fdata.cFileName,MAX_PATH);
		dir->flags &= ~IS_ROOT;
		return dir->dd_buf;

	}
	if(!(dir->flags & IS_NET) && !FindNextFile(dir->dd_fd,&fdata) ){
		return NULL;
	}
	(dir->dd_buf)->d_ino = inode++;
	(dir->dd_buf)->d_off = 0;
	(dir->dd_buf)->d_reclen = 0;
	if (! (dir->flags & IS_NET))
		memcpy((dir->dd_buf)->d_name,fdata.cFileName,MAX_PATH);

	return dir->dd_buf;

}

// Support for treating share names as directories
// -amol 5/28/97
static int ginited = 0;
static HMODULE hmpr;

typedef DWORD (__stdcall *open_fn)(DWORD,DWORD,DWORD,NETRESOURCE *, HANDLE*);
typedef DWORD (__stdcall *close_fn)( HANDLE);
typedef DWORD (__stdcall *enum_fn)( HANDLE,DWORD * ,void *,DWORD*);


static open_fn p_WNetOpenEnum;
static close_fn p_WNetCloseEnum;
static enum_fn  p_WNetEnumResource;

HANDLE open_enum(char *server, WIN32_FIND_DATA *fdata) {

	NETRESOURCE netres;
	HANDLE henum;
	unsigned long ret;
	char *ptr;
	int slashes;

	nethandle_t *hnet;

	ptr = server;
	slashes = 0;

	while(*ptr) {
		if (*ptr == '/') {
			*ptr = '\\';
			slashes++;
		}
		ptr++;
	}

	if (!ginited) {
		hmpr = LoadLibrary("MPR.DLL");
		if (!hmpr)
			return INVALID_HANDLE_VALUE;

		p_WNetOpenEnum = (open_fn)GetProcAddress(hmpr,"WNetOpenEnumA");
		p_WNetCloseEnum = (close_fn)GetProcAddress(hmpr,"WNetCloseEnum");
		p_WNetEnumResource = (enum_fn)GetProcAddress(hmpr,"WNetEnumResourceA");

		if (!p_WNetOpenEnum || !p_WNetCloseEnum || !p_WNetEnumResource)
			return INVALID_HANDLE_VALUE;
		ginited = 1;
	}
	if (slashes > 2)
		return INVALID_HANDLE_VALUE;

	memset(fdata,0,sizeof(WIN32_FIND_DATA));
	fdata->cFileName[0] = '.';

	netres.dwScope = RESOURCE_GLOBALNET;
	netres.dwType = RESOURCETYPE_ANY;
	netres.lpRemoteName = server;
	netres.lpProvider = NULL;
	netres.dwUsage = 0;

	ret = p_WNetOpenEnum(RESOURCE_GLOBALNET,RESOURCETYPE_ANY,0,
							&netres,&henum);
	if (ret != NO_ERROR)
		return INVALID_HANDLE_VALUE;
	
	hnet = heap_alloc(sizeof(nethandle_t));
	hnet->netres = heap_alloc(1024);/*FIXBUF*/
	hnet->henum = henum;


	return (HANDLE)hnet;

}
void close_enum(DIR*dptr) {
	nethandle_t *hnet;

	hnet = (nethandle_t*)(dptr->dd_fd);

	heap_free(hnet->netres);
	p_WNetCloseEnum(hnet->henum);
	heap_free(hnet);
}
int enum_next_share(DIR *dir) {
	nethandle_t *hnet;
	char *tmp,*p1;
	HANDLE henum;
	DWORD count, breq,ret;

	hnet = (nethandle_t*)(dir->dd_fd);
	henum = hnet->henum;
	count =  1;
	breq = 1024;

	ret = p_WNetEnumResource(henum, &count,hnet->netres,&breq);
	if (ret != NO_ERROR)
		return -1;
	
	tmp = ((NETRESOURCE*)hnet->netres)->lpRemoteName;
	p1 = &tmp[2];
#ifdef DSPMBYTE
	for (; *p1 != '\\'; p1 ++)
		if (Ismbyte1(*p1) && *(p1 + 1))
			p1 ++;
#else /* DSPMBYTE */
	while(*p1++ != '\\');
#endif /* DSPMBYTE */

	memcpy( (dir->dd_buf)->d_name, p1, lstrlen(p1)+1);

	dir->dd_size = 0;

	return 0;
}
