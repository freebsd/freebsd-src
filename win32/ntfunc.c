/*$Header: /p/tcsh/cvsroot/tcsh/win32/ntfunc.c,v 1.19 2006/08/27 01:13:28 amold Exp $*/
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
 * ntfunc.c builtins specific to NT
 * -amol
 *
 */
#pragma warning(push,3)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#pragma warning(pop)
#include <errno.h>
#include <sh.h>
#include "ed.h"

#include "nt.const.h"


extern DWORD gdwPlatform;

extern	 int StrQcmp(Char *, Char *);
extern int hashval_extern(Char*);
extern int bit_extern(int,int);
extern void bis_extern(int,int);
extern int hashname(Char*);

extern void NT_ClearScreen_WholeBuffer(void);

BOOL is_url(const char *cmd);

void error(char * ) ;
void make_err_str(unsigned int ,char *,int) ;

#define	INF INT_MAX
struct	biltins nt_bfunc[] = {
	{ "cls",		docls,		0,	0	},
#ifdef NTDBG
	{ "debugbreak",	dodebugbreak,	0,	0	},
#endif /* NTDBG */
	{ "ps",	dops,	0,	1	},
	{ "shutdown",	doshutdown,	0,	2	},
	{ "start",		dostart,	1,	INF	},
	{ "title",		dotitle,	1,	INF	},
};
int nt_nbfunc = sizeof nt_bfunc / sizeof *nt_bfunc;

char start_usage[] = { ":\n \
	Similar to cmd.exe's start  \n \
		start [-Ttitle] [-Dpath] [-min] [-max] [-separate] [-shared] \n \
		[-low|normal|realtime|high] program args \n \
		Batch/Cmd files must be started with CMD /K \n"
};

struct biltins * nt_check_additional_builtins(Char *cp) {

	register struct biltins  *bp1, *bp2;
	int i;

	for (bp1 = nt_bfunc, bp2 = nt_bfunc + nt_nbfunc; bp1 < bp2;bp1++) {

		if ((i = ((char) *cp) - *bp1->bname) == 0 &&
				(i = StrQcmp(cp, str2short(bp1->bname))) == 0)
			return bp1;
	}
	return (0);
}
void nt_print_builtins(size_t maxwidth) {

	/* would use print_by_column() in tw.parse.c but that assumes
	 * we have an array of Char * to pass.. (sg)
	 */
	extern int Tty_raw_mode;
	extern int TermH;		/* from the editor routines */
	extern int lbuffed;		/* from sh.print.c */

	register struct biltins *b;
	register size_t row, col, columns, rows;
	size_t w ,oldmax;


	/* find widest string */

	oldmax = maxwidth;

	for ( b = nt_bfunc; b < &nt_bfunc[nt_nbfunc]; ++b)
		maxwidth = max(maxwidth, (int)lstrlen(b->bname));

	if (oldmax != maxwidth)
		++maxwidth;					/* for space */

	columns = (TermH + 1) / maxwidth;	/* PWP: terminal size change */
	if (!columns)
		columns = 1;
	rows = (nt_nbfunc + (columns - 1)) / columns;

	for (b = nt_bfunc, row = 0; row < rows; row++) {
		for (col = 0; col < columns; col++) {
			if (b < &nt_bfunc[nt_nbfunc]) {
				w = (int)lstrlen(b->bname);
				xprintf("%s", b->bname);
				if (col < (columns - 1))	/* Not last column? */
					for (; w < maxwidth; w++)
						xputchar(' ');
				++b;
			}
		}
		if (Tty_raw_mode)
			xputchar('\r');
		xputchar('\n');
	}

}
/* patch from TAGA Nayuta for start . */
BOOL is_directory(const char *the_cmd) {
	DWORD attr = GetFileAttributes(the_cmd);
	return (attr != 0xFFFFFFFF &&
			(attr & FILE_ATTRIBUTE_DIRECTORY));
}
void dostart(Char ** vc, struct command *c) {

	char *cmdstr,*cmdend,*ptr;
	char argv0[256];/*FIXBUF*/
	DWORD cmdsize;
	char *currdir=NULL;
	char *savepath;
	char **v = NULL;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	DWORD dwCreationFlags=CREATE_NEW_CONSOLE;
	DWORD k,cmdlen,j,jj,ret;


	UNREFERENCED_PARAMETER(c);
	vc++;

	cmdsize = 512;
	cmdstr = heap_alloc(cmdsize);
	cmdend = cmdstr;
	cmdlen = 0;

	memset(&si,0,sizeof(si));
	si.cb = sizeof(si);

	vc = glob_all_or_error(vc);
	v = short2blk(vc);
	if(v == NULL) {
		stderror(ERR_NOMEM);
		return;
	}
	blkfree(vc);
	for (k = 0; v[k] != NULL ; k++){

		if ( v[k][0] == '-' ) {
			/* various options */
			if( (v[k][1] == 'T') || (v[k][1] == 't'))
				si.lpTitle =&( v[k][2]);
			else if ( (v[k][1] == 'D') || (v[k][1] == 'd'))
				currdir =&( v[k][2]);
			else if (!_stricmp(&v[k][1],"MIN") )
				si.wShowWindow = SW_SHOWMINIMIZED;
			else if (!_stricmp(&v[k][1],"MAX") )
				si.wShowWindow = SW_SHOWMAXIMIZED;
			else if (!_stricmp(&v[k][1],"SEPARATE") )
				dwCreationFlags |= CREATE_SEPARATE_WOW_VDM;
			else if (!_stricmp(&v[k][1],"SHARED") )
				dwCreationFlags |= CREATE_SHARED_WOW_VDM;
			else if (!_stricmp(&v[k][1],"LOW") )
				dwCreationFlags |= IDLE_PRIORITY_CLASS;
			else if (!_stricmp(&v[k][1],"NORMAL") )
				dwCreationFlags |= NORMAL_PRIORITY_CLASS;
			else if (!_stricmp(&v[k][1],"HIGH") )
				dwCreationFlags |= HIGH_PRIORITY_CLASS;
			else if (!_stricmp(&v[k][1],"REALTIME") )
				dwCreationFlags |= REALTIME_PRIORITY_CLASS;
			else{
				blkfree((Char **)v);
				stderror(ERR_SYSTEM,start_usage,"See CMD.EXE for more info");/*FIXRESET*/
			}
		}
		else{ // non-option arg
			break;
		}
	}
	/* 
	 * Stop the insanity of requiring start "tcsh -l"
	 * Option processing now stops at first non-option arg
	 * -amol 5/30/96
	 */
	for (jj=k;v[jj] != NULL; jj++) {
		j=(lstrlen(v[jj]) + 2);
		if (j + cmdlen > cmdsize) {
			ptr = cmdstr;
			cmdstr = heap_realloc(cmdstr, max(cmdsize << 1, j+cmdlen) );
			if(!cmdstr)
			{
				heap_free(ptr);
				stderror(ERR_NOMEM,"start");/*FIXRESET*/
			}
			cmdend =  cmdstr + (cmdend - ptr);
			cmdsize <<= 1;
		}
		ptr = v[jj];
		while (*ptr) {
			*cmdend++ = *ptr++;
			cmdlen++;
		}
		*cmdend++ = ' ';
		cmdlen++;
	}
	if (jj == k) {
		blkfree((Char **)v);
		stderror(ERR_SYSTEM,start_usage,"See CMD.EXE for more info");/*FIXRESET*/
		return;
	}
	*cmdend = 0;
	StringCbCopy(argv0,sizeof(argv0),v[k]);


	/* 
	 * strictly speaking, it should do no harm to set the path
	 * back to '\'-delimited even in the parent, but in the
	 * interest of consistency, we save the old value and restore it
	 * later
	 */

	savepath = fix_path_for_child();

	if (! CreateProcess(NULL,
				cmdstr,
				NULL,
				NULL,
				FALSE,
				dwCreationFlags,
				NULL,
				currdir,
				&si,
				&pi) ) {

		restore_path(savepath);

		ret = GetLastError();
		if (ret == ERROR_BAD_EXE_FORMAT || ret == ERROR_ACCESS_DENIED ||
				(ret == ERROR_FILE_NOT_FOUND && 
				 (is_url(v[k]) || is_directory(v[k]))
				) 
		   ) {

			char erbuf[MAX_PATH];

			errno = ENOEXEC;

			try_shell_ex(&v[k],0,FALSE);

			heap_free(cmdstr); /* free !! */

			if (errno) {
				strerror_s(erbuf,sizeof(erbuf),errno);
				stderror(ERR_ARCH,argv0,erbuf);/*FIXRESET*/
			}
		}
		else if (ret == ERROR_INVALID_PARAMETER) {

			errno = ENAMETOOLONG;

			heap_free(cmdstr); /* free !! */

			stderror(ERR_TOOLARGE,argv0);/*FIXRESET*/

		}
		else {
			errno = ENOENT;
			if (
					( (v[k][0] == '\\') ||(v[k][0] == '/') ) &&
					( (v[k][1] == '\\') ||(v[k][1] == '/') ) &&
					(!v[k+1])
			   )
				try_shell_ex(&v[k],0,FALSE);

			heap_free(cmdstr); /* free !! */
			if (errno) {
				stderror(ERR_NOTFOUND,argv0);/*FIXRESET*/
			}
		}
	}
	else {
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);

		heap_free(cmdstr);
		restore_path(savepath);
	}
	blkfree((Char **)v);
	return;
}
void error(char * ebuf) {

	write(2,(unsigned char*)ebuf,lstrlen(ebuf));
}
void make_err_str(unsigned int error,char *buf,int size) {

	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
			NULL,
			error, 
			MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
			buf,
			size,
			NULL);
	return;

}

// We should really use the environ array, but NT likes it to be sorted. 
// So we just let the win32 apis  take care of inheritance of the environment.
// -amol 4/7/97
//
//char nameBuf[BUFSIZ], valBuf[BUFSIZ];
char dummy;
char *nameBuf=&dummy, *valBuf=&dummy;

void nt_set_env(const Char *name, const Char *val) {
	char *cname, *cval;
	int len;

	cname = name?short2str(name):NULL;
	if(cname) {
		len = lstrlen(cname);
		nameBuf = heap_alloc(len+1);
		if (!nameBuf) {
			stderror(ERR_TOOLARGE);
		}
		StringCbCopy(nameBuf,len+1,cname);
	}
	cval = val?short2str(val):NULL;
	if(cval) {
		len = lstrlen(cval);
		valBuf = heap_alloc(len+1);
		StringCbCopy(valBuf,len+1,cval);
	}

	SetEnvironmentVariable(nameBuf,cval?valBuf:NULL);

	if (!lstrcmp(nameBuf,"TCSHONLYSTARTEXES")) 
		init_shell_dll();

	heap_free(nameBuf);
	if (cval)
		heap_free(valBuf);


}
void dotitle(Char **vc, struct command * c) {

	int k;
	char titlebuf[512];
	char errbuf[128],err2[128];
	char **v;

	UNREFERENCED_PARAMETER(c);
	vc++;
	vc = glob_all_or_error(vc);
	cleanup_push(vc, blk_cleanup);

	if ((k=GetConsoleTitle(titlebuf,512) ) != 0) {
		titlebuf[k]=0;
		setcopy(STRoldtitle,str2short(titlebuf),VAR_READWRITE);
	}

	memset(titlebuf,0,512);
	v = short2blk(vc);
	cleanup_until(vc);
	cleanup_push((Char **)v, blk_cleanup);
	for (k = 0; v[k] != NULL ; k++){
		__try {
			StringCbCat(titlebuf,sizeof(titlebuf),v[k]);
			StringCbCat(titlebuf,sizeof(titlebuf)," ");
		}
		__except(GetExceptionCode()) {
			stderror(ERR_TOOMANY);
		}
	}

	if (!SetConsoleTitle(titlebuf) ) {
		make_err_str(GetLastError(),errbuf,128);
		(void)StringCbPrintf(err2,sizeof(err2),"%s",v[k]);
		stderror(ERR_SYSTEM,err2,errbuf);
	}
	cleanup_until((Char **)v);
	return;
}
void docls(Char **vc, struct command *c) {
	UNREFERENCED_PARAMETER(vc);
	UNREFERENCED_PARAMETER(c);
	NT_ClearScreen_WholeBuffer();
}
int nt_feed_to_cmd(char *file,char **argv) {

	char *ptr, *orig;
	char cmdbuf[128];
	HANDLE htemp;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	if (!file)
		return 1;

	ptr = strrchr(file,'.');

	if(!ptr)
		return 1;

	if (lstrlen(ptr) <4)
		return 1;

	if ( _stricmp(ptr,".bat") &&  _stricmp(ptr,".cmd") )
		return 1;


	memset(&si,0,sizeof(si));
	memset(&pi,0,sizeof(pi));

	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;
	htemp= (HANDLE)_get_osfhandle(0);
	DuplicateHandle(GetCurrentProcess(),htemp,GetCurrentProcess(),
			&si.hStdInput,0,TRUE,DUPLICATE_SAME_ACCESS);
	htemp= (HANDLE)_get_osfhandle(1);
	DuplicateHandle(GetCurrentProcess(),htemp,GetCurrentProcess(),
			&si.hStdOutput,0,TRUE,DUPLICATE_SAME_ACCESS);
	htemp= (HANDLE)_get_osfhandle(2);
	DuplicateHandle(GetCurrentProcess(),htemp,GetCurrentProcess(),
			&si.hStdError,0,TRUE,DUPLICATE_SAME_ACCESS);


	ptr =file;
	while(*ptr) {
		if (*ptr == '/')
			*ptr = '\\';
		ptr++;
	}
	if (gdwPlatform == VER_PLATFORM_WIN32_WINDOWS){
		(void)StringCbPrintf(cmdbuf,sizeof(cmdbuf),
							 "command.com /c %s",file);
	}
	else
		(void)StringCbPrintf(cmdbuf,sizeof(cmdbuf),
							 "cmd /c %s",file);

	argv++;
	ptr = &cmdbuf[0] ;
	orig = ptr;
	while(*argv) {
		StringCbCat(ptr,sizeof(cmdbuf) - (orig - ptr), " ");
		StringCbCat(ptr,sizeof(cmdbuf) - (orig - ptr),*argv);
		argv++;
	}

	ptr = fix_path_for_child();

	if (!CreateProcess(NULL,
				cmdbuf,
				NULL,
				NULL,
				TRUE,
				0,//CREATE_NEW_CONSOLE |CREATE_NEW_PROCESS_GROUP,
				NULL,
				NULL,
				&si,
				&pi) ){
		restore_path(ptr);
	}
	else {

		restore_path(ptr);
		CloseHandle(pi.hThread);
		WaitForSingleObject(pi.hProcess,INFINITE);
		CloseHandle(pi.hProcess);
		ExitProcess(0);
	}

	return 1; /*NOTREACHED*/
}
static char *hb_subst_array[20] ;
void init_hb_subst(void) {
	int i= 0;
	size_t len;
	char envbuf[1024];
	char *ptr;
	char *p2;

	envbuf[0]=0;

	GetEnvironmentVariable("TCSHSUBSTHB",envbuf,1024);

	ptr = &envbuf[0];

	if (!*ptr)
		return;

	p2 = ptr;

	while (*ptr) {
		if (*ptr == ';') {
			len = ptr - p2;
			if (!len){
				ptr++;
				continue;
			}
			hb_subst_array[i] = heap_alloc(len+1);
			StringCbCopy(hb_subst_array[i],len + 1, p2);

			i++;
			p2 = ptr+1;
		}
		ptr++;
	}
}
char *hb_subst(char *orig) {
	int i, match;
	char *p1;

	for(i =0 ;i <20; i++) {
		p1 = hb_subst_array[i];
		if (!p1)
			continue;
		while(*p1 != ' ')
			p1++;

		*p1 = 0;
		match = !_stricmp(orig,hb_subst_array[i]);
		*p1 = ' ';
		if (match){
			return (p1+1);
		}
	}
	return NULL;

}
typedef BOOL (__stdcall *shell_ex_func)(LPSHELLEXECUTEINFO);

/* DO NOT initialize these here -amol */
static HMODULE hShellDll;
static shell_ex_func pShellExecuteEx;
int __nt_only_start_exes;

static char no_assoc[256]; //the environment string/*FIXBUF*/
static char *no_assoc_array[20]; // the list of extensions to NOT try /*FIXBUF*/
// explorer associations for

void init_shell_dll(void) {

	int rc,i;
	size_t len;
	char *p2, *ptr;

	if (!hShellDll) {
		hShellDll = LoadLibrary("Shell32.dll");
		if (hShellDll) {
			pShellExecuteEx = (shell_ex_func)GetProcAddress(
					hShellDll,
					"ShellExecuteEx");
		}
	}
	rc=GetEnvironmentVariable("TCSHONLYSTARTEXES",no_assoc,256) ;
	if (!rc || (rc > 255))
		return;

	if (rc == 1) {
		__nt_only_start_exes = 1;
		return;
	}

	ptr = &no_assoc[0];
	i = 0;

	if (!ptr)
		return;

	p2 = ptr;

	while (i < 20) {
		if (*ptr == ';' || (!*ptr)) {
			len = ptr - p2;
			if (!len){
				ptr++;
				continue;
			}
			no_assoc_array[i] = heap_alloc(len+1);
			StringCbCopy(no_assoc_array[i],len+1, p2);
			dprintf("no_assoc array %d inited to %s\n",i,no_assoc_array[i]);

			i++;
			p2 = ptr+1;
		}
		if (!*ptr)
			break;
		ptr++;
	}
#if NTDBG
	for(i=0;i<20,no_assoc_array[i] != NULL;i++)
		dprintf("no_assoc array %d inited remains %s\n",i,no_assoc_array[i]);
#endif NTDBG

}
// return non-zero if str is found in no_assoc_array
int find_no_assoc(char *my_str) {
	int i, match;
	char *p1;

	for(i =0 ;i <20; i++) {
		p1 = no_assoc_array[i];
		dprintf("no_assoc array %d is %s\n",i,no_assoc_array[i]);
		if (!p1)
			continue;
		match = !_stricmp(my_str,no_assoc_array[i]);
		if (match)
			return 1;
	}
	return 0;
}
void try_shell_ex(char **argv,int exitsuccess, BOOL throw_ok) {/*FIXRESET*/

	char *prog;
	char *cmdstr, *p2, *cmdend;
	char *originalPtr = NULL;
	unsigned int cmdsize,cmdlen;
	char err2[256];
	char *ptr;
	SHELLEXECUTEINFO shinfo;
	unsigned long  mask = SEE_MASK_FLAG_NO_UI;
	BOOL rc;
	char *extension;

	prog=*argv;

	dprintf("trying shellex for prog %s\n",prog);
	ptr = prog;
	if (!is_url(prog)) {

		while(*ptr) {
			if (*ptr == '/')
				*ptr = '\\';
			ptr++;
		}

		extension = ptr;

		// search back for "."
		while(extension != prog) {
			if (*extension == '.') {
				extension++;
				break;
			}
			else
				extension--;
		}
		/* check if this matches a member in the no_assoc array.
		 */
		if (extension != prog)  {
			if (find_no_assoc(extension))
				return;
		}

	}
	originalPtr = cmdstr= heap_alloc(MAX_PATH<<2);

	cmdsize = MAX_PATH<<2;

	p2 = cmdstr;

	cmdlen = 0;
	cmdend = p2;

	argv++; // the first arg is the command


	dprintf("try_shell_ex calling c_a_a_q");
	if(!concat_args_and_quote(argv,&originalPtr,&cmdstr,&cmdlen,&cmdend,&cmdsize))
	{
		errno = ENOMEM;
		heap_free(originalPtr);
		return;
	}

	*cmdend = 0;


	memset(&shinfo,0,sizeof(shinfo));
	shinfo.cbSize = sizeof(shinfo);
	shinfo.fMask = SEE_MASK_FLAG_DDEWAIT | mask;
	shinfo.hwnd = NULL;
	shinfo.lpVerb = NULL;
	shinfo.lpFile = prog;
	shinfo.lpParameters = &cmdstr[0];
	shinfo.lpDirectory = 0;
	shinfo.nShow = SW_SHOWDEFAULT;


	ptr = fix_path_for_child();

	rc = pShellExecuteEx(&shinfo);
	if (rc ) {
		if (exitsuccess) 
			ExitProcess(0);
		errno = 0;

		heap_free(originalPtr);
		return;
	}
	if (throw_ok) { 
		// if we got here, ShellExecuteEx failed, so reset() via stderror()
		// this may cause the caller to leak, but the assumption is that
		// only a child process sets exitsuccess, so it will be dead soon
		// anyway

		restore_path(ptr);

		make_err_str(GetLastError(),cmdstr,512);//don't need the full size
		(void)StringCbPrintf(err2,sizeof(err2),"%s",prog);
		stderror(ERR_SYSTEM,err2,cmdstr);/*FIXRESET*/
	}

	heap_free(originalPtr);
	restore_path(ptr);

	errno = ENOEXEC;

}
#ifdef NTDBG
void dodebugbreak(Char **vc, struct command *c) {
	UNREFERENCED_PARAMETER(vc);
	UNREFERENCED_PARAMETER(c);
	DebugBreak();
}
#endif NTDBG
int nt_texec(char *, char**) ;
static Char *epath;
static Char *abspath[] = {STRNULL,0};
int nt_try_fast_exec(struct command *t) {
	register Char  **pv, **av;
	register Char *dp,*sav;
	register char **tt;
	register char *f;
	register struct varent *v;
	register int hashval,i;
	register int slash;
	int rc = 0, gflag;
	Char *vp;
	Char   *blk[2];

	vp = varval(STRNTslowexec);
	if (vp != STRNULL)
		return 1;

	blk[0] = t->t_dcom[0];
	blk[1] = 0;

	// don't do backtick
	if(Strchr(t->t_dcom[0],'`') )
		return 1;


	gflag = tglob(blk);
	if (gflag) {
		pv = globall(blk, gflag);
		if (pv == 0) {
			return 1;
		}
	}
	else
		pv = saveblk(blk);

	trim(pv);

	epath = Strsave(pv[0]);
	v = adrof(STRpath);
	if (v == 0 && epath[0] != '/' && epath[0] != '.') {
		blkfree(pv);
		return 1;
	}
	slash = any(short2str(epath),'/');
	/*
	 * Glob the argument list, if necessary. Otherwise trim off the quote bits.
	 */
	av = &t->t_dcom[1];
	gflag = tglob(av);
	if (gflag) {
		av = globall(av, gflag);/*FIXRESET*/
		if (av == 0) {
			blkfree(pv);
			return 1;
		}
	}
	else
		av = saveblk(av);

	blkfree(t->t_dcom);
	t->t_dcom = blkspl(pv, av);
	xfree((ptr_t) pv);
	xfree((ptr_t) av);
	av = t->t_dcom;
	//trim(av);

	if (*av == NULL || **av == '\0')
		return 1;

	xechoit(av);/*FIXRESET*/		/* Echo command if -x */
	if (v == 0 || v->vec[0] == 0 || slash)
		pv = abspath;
	else
		pv = v->vec;

	sav = Strspl(STRslash,*av);
	hashval = hashval_extern(*av);

	i = 0;
	do {
#pragma warning(disable:4310)
		if (!slash && ABSOLUTEP(pv[0]) && havhash) {
#pragma warning(default:4310)
			if (!bit_extern(hashval,i)){
				pv++;i++;
				continue;
			}
		}
		if (pv[0][0] == 0 || eq(pv[0],STRdot)) {

			tt = short2blk(av);
			f = short2str(*av);

			rc = nt_texec(f, tt);

			blkfree((Char**)tt);
			if (!rc)
				break;
		}
		else {
			dp = Strspl(*pv,sav);
			tt = short2blk(av);
			f = short2str(dp);

			rc = nt_texec(f, tt);

			blkfree((Char**)tt);
			xfree((ptr_t)dp);
			if (!rc)
				break;
		}
		pv++;
		i++;
	}while(*pv);
	return rc;
}
int nt_texec(char *prog, char**args ) {

	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	HANDLE htemp;
	DWORD type=0;
	DWORD dwCreationflags;
	unsigned int priority;
	char *argv0 = NULL, *savepath = NULL;
	char *cmdstr,*cmdend ;
	char *originalPtr = NULL;
	unsigned int cmdsize,cmdlen;
	char *p2;
	char **savedargs;
	int retries=0;
	int hasdot =0;
	int is_winnt=0;
	int retval = 1;

	memset(&si,0,sizeof(si));
	savedargs = args;

	/* MUST FREE !! */
	originalPtr = cmdstr= heap_alloc(MAX_PATH<<2);
	cmdsize = MAX_PATH<<2;

	is_winnt = (gdwPlatform != VER_PLATFORM_WIN32_WINDOWS);


	p2 = cmdstr;

	cmdlen = 0;
	cmdlen += copy_quote_and_fix_slashes(prog,cmdstr,&hasdot);
	p2 += cmdlen;

	if (*cmdstr != '"') {
		// If not quoted, skip initial character we left for quote
		*cmdstr = 'A';
		cmdstr++; 
		cmdsize--;
	}
	*p2 = 0; 
	cmdend = p2;

	if (!is_winnt) {
		argv0 = NULL;
	}
	else {
		argv0= heap_alloc(MAX_PATH);
		(void)StringCbPrintf(argv0,MAX_PATH,"%s",prog);
	}

	si.cb = sizeof(STARTUPINFO);
	si.dwFlags = STARTF_USESTDHANDLES;
	htemp= (HANDLE)_get_osfhandle(SHIN);
	DuplicateHandle(GetCurrentProcess(),htemp,GetCurrentProcess(),
			&si.hStdInput,0,TRUE,DUPLICATE_SAME_ACCESS);
	htemp= (HANDLE)_get_osfhandle(SHOUT);
	DuplicateHandle(GetCurrentProcess(),htemp,GetCurrentProcess(),
			&si.hStdOutput,0,TRUE,DUPLICATE_SAME_ACCESS);
	htemp= (HANDLE)_get_osfhandle(SHDIAG);
	DuplicateHandle(GetCurrentProcess(),htemp,GetCurrentProcess(),
			&si.hStdError,0,TRUE,DUPLICATE_SAME_ACCESS);


	/* 
	   quotespace hack needed since execv() would have separated args, but
	   createproces doesnt
	   -amol 9/14/96
	 */

	args++; // the first arg is the command

	dprintf("nt_texec calling c_a_a_q");
	if(concat_args_and_quote(args,&originalPtr,&cmdstr,&cmdlen,&cmdend,&cmdsize) == NULL)
	{
		retval = 1;
		errno  = ENOMEM;
		heap_free(originalPtr);
		goto free_mem;
	}

	*cmdend = 0;

	dwCreationflags = GetPriorityClass(GetCurrentProcess());
	priority = GetThreadPriority(GetCurrentThread());

	if (is_winnt) {
		retries = 0;
		// For NT, try ShellExecuteEx first
		do {
			if (GetBinaryType(argv0,&type)) 
				break;
			if (GetLastError() == ERROR_BAD_EXE_FORMAT){
				errno = ENOEXEC;
				if (!__nt_only_start_exes)
					try_shell_ex(savedargs,0,FALSE);
				if (errno) {
					retval = 1; 
					goto free_mem;
				}
				else {
					retval = 0;
					goto free_mem;
				}
			}
			// only try shellex again after appending ".exe fails
			else if ( retries > 1 ){
				if (
						( (argv0[0] == '\\') ||(argv0[0] == '/') ) &&
						( (argv0[1] == '\\') ||(argv0[1] == '/') ) &&
						(!args[1])
				   )
					if (!__nt_only_start_exes)
						try_shell_ex(savedargs,0,FALSE);
				errno  = ENOENT;
			}
			if (retries == 0)
				(void)StringCbPrintf(argv0,MAX_PATH,"%s.exe",prog);
			else if (retries == 1) {
				(void)StringCbPrintf(argv0,MAX_PATH,"%s.EXE",prog);
			}
			retries++;
		}while(retries < 3);
	}
	savepath = fix_path_for_child();
re_cp:
	dprintf("nt_texec cmdstr %s\n",cmdstr);


	if (!CreateProcess(argv0,
				cmdstr,
				NULL,
				NULL,
				TRUE, // need this for redirecting std handles
				dwCreationflags,
				NULL,//envcrap,
				NULL,
				&si,
				&pi) ){

		if (GetLastError() == ERROR_BAD_EXE_FORMAT) {
			errno  = ENOEXEC;
		}
		else if (GetLastError() == ERROR_INVALID_PARAMETER) {
			errno = ENAMETOOLONG;
		}else {
			errno  = ENOENT;
		}
		if (!is_winnt && !hasdot) { //append '.' to the end if needed
			StringCbCat(cmdstr,cmdsize,".");
			hasdot=1;
			goto re_cp;
		}
		retval = 1;
	}
	else{
		int gui_app ;
		DWORD exitcode;
		char guivar[50];


		if (GetEnvironmentVariable("TCSH_NOASYNCGUI",guivar,50))
			gui_app=0;
		else
			gui_app= is_gui(argv0);

		if(!gui_app) {
			WaitForSingleObject(pi.hProcess,INFINITE);
			(void)GetExitCodeProcess(pi.hProcess,&exitcode);
			setv(STRstatus, putn(exitcode), VAR_READWRITE);/*FIXRESET*/
		}
		retval = 0;
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
free_mem:
	CloseHandle(si.hStdInput);
	CloseHandle(si.hStdOutput);
	CloseHandle(si.hStdError);

	if(savepath)
		restore_path(savepath);

	heap_free(originalPtr);
	if (argv0)
		heap_free(argv0);
	return retval;
}
BOOL is_url(const char *thecmd) {
	char *protocol;
	const char *c;
	HKEY hkey;
	char buf[2];
	DWORD type;
	DWORD size;

	c = strchr(thecmd, ':');
	size = (DWORD)(c - thecmd);
	if (!c || size <= 1)
		return FALSE;

	protocol = (char *)heap_alloc(size + 2);
	StringCbCopy(protocol,size+2, thecmd);
	protocol[size] = '\0';

	if (RegOpenKeyEx(HKEY_CLASSES_ROOT, protocol, 0, KEY_READ, &hkey)
			!= ERROR_SUCCESS ) {
		heap_free(protocol);
		return FALSE;
	}

	heap_free(protocol);

	type = REG_SZ;
	size = sizeof(buf);
	if ( RegQueryValueEx(hkey, "URL Protocol", NULL, &type, (BYTE*)buf, &size)
			!= ERROR_SUCCESS) {
		RegCloseKey(hkey);
		return FALSE;
	}
	RegCloseKey(hkey);
	return TRUE;
}
/*
 * patch based on work by Chun-Pong Yu (bol.pacific.net.sg)
 */
BOOL is_nt_executable(char *path,char *extension) {
	DWORD exetype;

	if (GetBinaryType(path,&exetype))
		return TRUE;
	if (*extension && find_no_assoc(extension))
		return TRUE;

	return FALSE;
}
int executable(const Char *dir, const Char *name, int dir_ok)
{
	struct stat stbuf;
	Char    path[MAXPATHLEN + 1];
	char   *strname;
	char extension[MAXPATHLEN]; 
	char *ptr, *p2 ;
	int has_ext = 0;
	extern void copyn(Char *, const Char *, size_t);
	extern void catn(Char *, const Char *, int);

	(void) memset(path, 0, sizeof(path));

	if (dir && *dir) {
		copyn(path, dir, MAXPATHLEN);
		catn(path, name, MAXPATHLEN);

		p2 = ptr = short2str(path);

		while (*ptr++)
			continue;
		--ptr;

		while(ptr > p2) { 
			if (*ptr == '/')
				break;
			if (*ptr == '.') {
				has_ext = 1;
				StringCbCopy(extension,MAXPATHLEN,ptr+1);
				break;
			}
			ptr--;
		}
		if (!has_ext && (nt_stat(p2, &stbuf) == -1))
			catn(path, STRdotEXE, MAXPATHLEN);
		strname = short2str(path);
	}
	else
		strname = short2str(name);

	return (stat(strname, &stbuf) != -1 &&
			((dir_ok && S_ISDIR(stbuf.st_mode)) ||
			 (S_ISREG(stbuf.st_mode) &&
			  (is_nt_executable(strname,extension) ||
			   (stbuf.st_mode & (S_IXOTH | S_IXGRP | S_IXUSR)))
			 )));
}
	int
nt_check_if_windir(char *path)
{
	char windir[MAX_PATH];

	(void)GetWindowsDirectory(windir, sizeof(windir));
	windir[2] = '/';

	return (strstr(path, windir) != NULL);
}

	void
nt_check_name_and_hash(int is_windir, char *file, int i)
{
	char name_only[MAX_PATH];
	char *tmp = (char *)strrchr(file, '.');
	char uptmp[5], *nameptr, *np2;
	int icount, hashval;

	if(!tmp || tmp[4]) 
		goto nodot;

	for (icount = 0; icount < 4; icount++)
		uptmp[icount] = (char)toupper(tmp[icount]);
	uptmp[4]=0;

	if (is_windir)
		if((uptmp[1] != 'E') || (uptmp[2] != 'X') || (uptmp[3] != 'E'))
			return;
	(void) memset(name_only, 0, MAX_PATH);
	nameptr = file;
	np2 = name_only;
	while(nameptr != tmp) {
		*np2++= (char)tolower(*nameptr);
		nameptr++;
	}
	hashval = hashname(str2short(name_only));
	bis_extern(hashval, i);
nodot:
	hashval = hashname(str2short(file));
	bis_extern(hashval, i);
}
