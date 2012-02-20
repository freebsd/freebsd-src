/*$Header: /p/tcsh/cvsroot/tcsh/win32/nt.who.c,v 1.6 2006/03/05 08:59:36 amold Exp $*/
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
 * nt.who.c: Support for who-like functions, using NETBIOS
 * -amol
 *
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <nb30.h>
#include <stdio.h>
#include "sh.h"


typedef struct _ASTAT_ {
	ADAPTER_STATUS adapt;
	NAME_BUFFER    NameBuff [10];
} ASTAT;

typedef struct _n_ctx {
	NCB ncb;
	u_char usr_name[NCBNAMSZ];
	u_char mach_name[NCBNAMSZ];
} ncb_ctx;

typedef UCHAR (APIENTRY *netbios_func)(NCB *);


static netbios_func p_Netbios =0;
static int ginited = 0;

static CRITICAL_SECTION nb_critter;
static HMODULE hnetapi;


extern int add_to_who_list(u_char *,u_char*);

void init_netbios(void ) {


	if (!ginited) {
		hnetapi = LoadLibrary("NETAPI32.DLL");
		if (!hnetapi)
			return ;

		p_Netbios = (netbios_func)GetProcAddress(hnetapi,"Netbios");

		if (!p_Netbios )
			return ;
		ginited = 1;
	}
	InitializeCriticalSection(&nb_critter);
}
void cleanup_netbios(void) {
	if (hnetapi){
		DeleteCriticalSection(&nb_critter);
		FreeLibrary(hnetapi);
	}
}
void CALLBACK complete_ncb( NCB * p_ncb) {

	int count,i;
	ADAPTER_STATUS *p_ad;
	ASTAT *pas;
	char *p1;
	ncb_ctx *ctx = (ncb_ctx *)p_ncb;

	if (p_ncb->ncb_retcode)
		goto end;

	__try {

		EnterCriticalSection(&nb_critter);
		pas = ((ASTAT*) p_ncb->ncb_buffer);
		p_ad = &pas->adapt;

		count = p_ad->name_count;

		if (count <=0 )
			__leave;

		if (ctx->usr_name[0] == 0) { //any user on given machine
			for(i=0; i<count;i++) {
				if (pas->NameBuff[i].name[15] == 03) { // unique name
					if (!strncmp((char*)(pas->NameBuff[i].name), 
							 	 (char*)(p_ncb->ncb_callname),
								 NCBNAMSZ)) {
						continue;
					}
					else {
						p1 = strchr((char*)(pas->NameBuff[i].name),' ');
						if (p1)
							*p1 = 0;
						else
							pas->NameBuff[i].name[15]= 0;
						add_to_who_list(pas->NameBuff[i].name,
								ctx->mach_name);
						break;
					}
				}
			}
		}
		else if (ctx->mach_name[0] == 0)  { // given user on any machine
			for(i=0; i<count;i++) {
				if (pas->NameBuff[i].name[15] == 03) { // unique name
					if (!strncmp((char*)(pas->NameBuff[i].name),
								 (char*)(p_ncb->ncb_callname),
								 NCBNAMSZ))
						continue;
					else {
						p1 = strchr((char*)(pas->NameBuff[i].name),' ');
						if (p1)
							*p1 = 0;
						else
							pas->NameBuff[i].name[15]= 0;

						add_to_who_list(ctx->usr_name, pas->NameBuff[i].name);

						break;
					}
				}
			}
		}
		else { // specific user on specific machine
			for(i=0; i<count;i++) {
				if (pas->NameBuff[i].name[15] == 03) { // unique name
					// skip computer name
					if (!strncmp((char*)(pas->NameBuff[i].name),
								 (char*)(p_ncb->ncb_callname),
								 NCBNAMSZ)) {
						continue;
					}
					else if (!strncmp((char*)(pas->NameBuff[i].name),
									  (char*)(ctx->usr_name),
									  lstrlen((char*)ctx->usr_name))) {
						p1 = strchr((char*)pas->NameBuff[i].name,' ');
						if (p1)
							*p1 = 0;
						else
							pas->NameBuff[i].name[15]= 0;
						add_to_who_list(pas->NameBuff[i].name,ctx->mach_name);
						break;
					}
				}
			}
		}
	}
	__except(GetExceptionCode()) {
		;
	}
	LeaveCriticalSection(&nb_critter);
end:
	heap_free(p_ncb->ncb_buffer);
	heap_free(p_ncb);
	return;
}
void start_ncbs (Char **vp) {

	ncb_ctx * p_ctx;
	NCB *Ncb;
	Char **namevec = vp;
	char *p1,*p2,*nb_name;
	UCHAR uRetCode;
	ASTAT *Adapter;

	if (!ginited) {
		init_netbios();
	}
	if (!ginited)
		return;

	for (namevec = vp;*namevec != NULL;namevec +=2) {

		p_ctx = heap_alloc(sizeof(ncb_ctx));
		Adapter = heap_alloc(sizeof(ASTAT));

		Ncb = (NCB*)p_ctx;

		memset( Ncb, 0, sizeof(NCB) );

		Ncb->ncb_command = NCBRESET;
		Ncb->ncb_lana_num = 0;

		uRetCode = p_Netbios( Ncb );

		if(uRetCode)
			goto cleanup;

		if  ((**namevec == '\0' ) || ( *(namevec +1) ==  NULL) ||
				(**(namevec +1) == '\0') )
			break;


		p1 = short2str(*namevec);
		if (!_stricmp(p1,"any") ) {
			p_ctx->usr_name[0] = 0;
		}
		else {
			StringCbCopy((char*)p_ctx->usr_name,sizeof(p_ctx->usr_name),p1);
		}
		p1 = (char*)&(p_ctx->usr_name[0]);

		p2 = short2str(*(namevec+1));
		//
		// If machine is not "any", make it the callname
		//
		if (!_stricmp(p2,"any") ) {
			p_ctx->mach_name[0] = 0;
			nb_name = p1;
		}
		else {
			StringCbCopy((char*)p_ctx->mach_name,sizeof(p_ctx->mach_name),p2);
			nb_name = p2;
		}

		// do not permit any any
		//
		if( (p_ctx->mach_name[0] == 0) && (p_ctx->usr_name[0] == 0) )
			goto cleanup;



		memset( Ncb, 0, sizeof (NCB) );

		Ncb->ncb_command = NCBASTAT | ASYNCH;
		Ncb->ncb_lana_num = 0;

		memset(Ncb->ncb_callname,' ',sizeof(Ncb->ncb_callname));

		Ncb->ncb_callname[15]=03;

		memcpy(Ncb->ncb_callname,nb_name,lstrlen(nb_name));

		Ncb->ncb_buffer = (u_char *) Adapter;
		Ncb->ncb_length = sizeof(*Adapter);

		Ncb->ncb_post = complete_ncb;

		uRetCode = p_Netbios( Ncb );
	}
	return;

cleanup:
	heap_free(Adapter);
	heap_free(p_ctx);
	return;
}
