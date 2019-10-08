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

// ntport.h
// the main header.
// -amol
//
//
#ifndef NTPORT_H
#define NTPORT_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>
#include <direct.h>
#include "dirent.h"
#include "version.h"

#ifndef WINDOWS_ONLY
#define STRSAFE_NO_DEPRECATE
#endif /* WINDOWS_ONLY*/
#define STRSAFE_LIB
#define STR_NO_CCH_FUNCTIONS
#include <strsafe.h>

// These needed for fork(), which controls the heap management.
#pragma data_seg(".fusrdata")
#define INIT_ZERO =0
#define INIT_ZERO_STRUCT ={0}
#define malloc fmalloc
#define calloc fcalloc
#define realloc frealloc
#define free ffree
#undef min
#undef max

#define heap_alloc(s) HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,(s))
#define heap_free(p) HeapFree(GetProcessHeap(),0,(p))
#define heap_realloc(p,s) HeapReAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,(p),(s))

#pragma warning(disable:4018)  //signed-unsigned mismatch
#define HAVENOLIMIT

/* os-dependent stuff. belongs in tc.os.h, but I never said this would be
pretty */

#define lstat stat
#ifdef S_IFMT
# if !defined(S_ISDIR) && defined(S_IFDIR)
#  define S_ISDIR(a)	(((a) & S_IFMT) == S_IFDIR)
# endif	/* ! S_ISDIR && S_IFDIR */
# if !defined(S_ISCHR) && defined(S_IFCHR)
#  define S_ISCHR(a)	(((a) & S_IFMT) == S_IFCHR)
# endif /* ! S_ISCHR && S_IFCHR */
# if !defined(S_ISBLK) && defined(S_IFBLK)
#  define S_ISBLK(a)	(((a) & S_IFMT) == S_IFBLK)
# endif	/* ! S_ISBLK && S_IFBLK */
# if !defined(S_ISREG) && defined(S_IFREG)
#  define S_ISREG(a)	(((a) & S_IFMT) == S_IFREG)
# endif	/* ! S_ISREG && S_IFREG */
# if !defined(S_ISFIFO) && defined(S_IFIFO)
#  define S_ISFIFO(a)	(((a) & S_IFMT) == S_IFIFO)
# endif	/* ! S_ISFIFO && S_IFIFO */
# if !defined(S_ISNAM) && defined(S_IFNAM)
#  define S_ISNAM(a)	(((a) & S_IFMT) == S_IFNAM)
# endif	/* ! S_ISNAM && S_IFNAM */
# if !defined(S_ISLNK) && defined(S_IFLNK)
#  define S_ISLNK(a)	(((a) & S_IFMT) == S_IFLNK)
# endif	/* ! S_ISLNK && S_IFLNK */
# if !defined(S_ISSOCK) && defined(S_IFSOCK)
#  define S_ISSOCK(a)	(((a) & S_IFMT) == S_IFSOCK)
# endif	/* ! S_ISSOCK && S_IFSOCK */
#endif /* S_IFMT */

/* port defines */
#define getpid                GetCurrentProcessId
#define getpgrp               GetCurrentProcessId
#define tcgetpgrp(a)          GetCurrentProcessId()
#define tcsetpgrp(a,b)        0
#define setpgid(a,b)          0


#define close(a)              nt_close(a)
#define execv(a,b)            nt_exec((a),(b))
#define execve(a,b,c)         nt_execve((a),(b),(c))

#define open                  nt_open
#define read(f,b,n)           nt_read((f),(b),(n))
#define write(f,b,n)          nt_write((f),(b),(n))
#define creat(f,m)            nt_creat((f),(m))
#define _exit(a)              ExitProcess((a))

#define chdir(a)              nt_chdir(a)

#undef putc
#undef putchar
#define fgetc(a)              nt_fgetc(a)
#define fputs(a,b)            nt_fputs((a),(b))
#define putc(a,b)             nt_putc((char)(a),(b))
#define fflush(a)             nt_fflush((a))
#define fputc(a,b)            nt_fputc((char)(a),(b))
#define fprintf               nt_fprintf
#define puts(a)               nt_puts(a)
#define putchar(a)            nt_putchar((char)(a))
#define fclose(p)             nt_fclose(p)
#define _get_osfhandle        __nt_get_osfhandle
#define _open_osfhandle       __nt_open_osfhandle
#define clearerr              nt_clearerr
#define dup2                  nt_dup2
#define fdopen                nt_fdopen
#define fgets                 nt_fgets
#define fileno                nt_fileno
#define fopen                 nt_fopen
#define fread                 nt_fread
#define fseek                 nt_fseek
#define ftell                 nt_ftell
#define fwrite                nt_fwrite
#define isatty                nt_isatty
#define lseek                 nt_lseek
#define printf                nt_printf
#define access                nt_access
#define fstat(a,b)            nt_fstat((a),(b))
#define stat(a,b)             nt_stat((a),(b))

#define setvbuf(a,b,c,d) 
#define setpgrp(a,b) (-1)
#define tcsetattr(a,b,c) 0

#define inline __inline

#undef stdin
#undef stdout
#undef stderr
#define stdin                 ((FILE*)my_stdin)
#define stdout                ((FILE*)my_stdout)
#define stderr                ((FILE*)my_stderr)

#define dup(f)                nt_dup((f))
#define sleep(a)              Sleep((a)*1000)

#define getcwd(a,b)           forward_slash_get_cwd((a),(b))


#define L_SET                 SEEK_SET
#define L_XTND                SEEK_END
#define L_INCR                SEEK_CUR
#define S_IXUSR               S_IEXEC
#define S_IXGRP               S_IEXEC
#define S_IXOTH               S_IEXEC

#define NOFILE                64
#define ARG_MAX               1024
#define MAXSIG                NSIG

/*
mode Value	Checks File For

00	Existence only
02 	Write permission
04	Read permission
06	Read and write permission
*/
#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4
#define XD_OK 9 //executable and not directory

/* base key mappings + ctrl-key mappings + alt-key mappings */
/* see nt.bind.c  to figure these out */
/*  256 + 
	4*24 (fkeys) + 
	4*4 (arrow) + 
	4*2 (pgup/dn) +
	4*2 (home/end) + 
	4*2 (ins/del) 
*/
#define NT_NUM_KEYS	               392

#define NT_SPECIFIC_BINDING_OFFSET 256 /* where our bindings start */

#define KEYPAD_MAPPING_BEGIN       24 /* offset from NT_SPECIFIC 
										  where keypad mappings begin */
#define INS_DEL_MAPPING_BEGIN      32 

#define SINGLE_KEY_OFFSET          0  /*if no ctrl or alt pressed */
#define CTRL_KEY_OFFSET            34
#define ALT_KEY_OFFSET             (34*2)
#define SHIFT_KEY_OFFSET           (34*3)

typedef int pid_t;
typedef int speed_t;
typedef unsigned char u_char;
typedef size_t caddr_t;
typedef int sig_atomic_t;
typedef int mode_t;
typedef UINT32 uint32_t;
typedef unsigned char uint8_t;

struct timeval{
	long tv_sec;
	long tv_usec;
};
struct termios;
/*
struct timezone{
	int tz_minuteswest;
	int dsttime;
};
*/
struct rusage {

	 struct timeval ru_utime; /* user time used */
	 struct timeval ru_stime; /* system time used */
	 long ru_maxrss;          /* maximum resident set size */
	 long ru_ixrss;      /* integral shared memory size */
	 long ru_idrss;      /* integral unshared data size */
	 long ru_isrss;      /* integral unshared stack size */
	 long ru_minflt;          /* page reclaims */
	 long ru_majflt;          /* page faults */
	 long ru_nswap;      /* swaps */
	 long ru_inblock;         /* block input operations */
	 long ru_oublock;         /* block output operations */
	 long ru_msgsnd;          /* messages sent */
	 long ru_msgrcv;          /* messages received */
	 long ru_nsignals;        /* signals received */
	 long ru_nvcsw;      /* voluntary context switches */
	 long ru_nivcsw;          /* involuntary context switches */
};
typedef int uid_t;
typedef int gid_t;
typedef long ssize_t;

struct passwd {
	  char    *pw_name;       /* user name */
	  char    *pw_passwd;     /* user password */
	  uid_t   pw_uid;         /* user id */
	  gid_t   pw_gid;         /* group id */
	  char    *pw_gecos;      /* real name */
	  char    *pw_dir;        /* home directory */
	  char    *pw_shell;      /* shell program */
};  
struct group {
	  char    *gr_name;        /* group name */
	  char    *gr_passwd;      /* group password */
	  gid_t   gr_gid;          /* group id */
	  char    **gr_mem;        /* group members */
};

#ifndef _INTPTR_T_DEFINED
#ifdef  _WIN64
typedef __int64             intptr_t;
#else
typedef int                 intptr_t;
#endif
#define _INTPTR_T_DEFINED
#endif
/* ntport.c */
extern char *			 ttyname(int);
extern struct passwd*    getpwuid(uid_t ) ;
extern struct group *    getgrgid(gid_t ) ;
extern struct passwd*    getpwnam(const char* ) ;
extern struct group*     getgrnam(char* ) ;
extern gid_t 			 getuid(void) ;
extern gid_t 			 getgid(void) ;
extern gid_t 			 geteuid(void) ;
extern gid_t 			 getegid(void) ;

#ifdef NTDBG
extern void dprintf(char *,...);
#define DBreak() __asm {int 3}
#else
#define dprintf (void)
#endif NTDBG

#define pipe(a) nt_pipe(a)


/* support.c */
extern void nt_init(void);
extern int gethostname(char*,int);
extern char* forward_slash_get_cwd(char *,size_t len );
extern int  nt_chdir(char*);
extern void  nt_execve(char *,char**,char**);
extern void  nt_exec(char *,char**);
extern int quoteProtect(char *, char *,unsigned long) ;
extern char* fix_path_for_child(void) ;
extern void restore_path(char *) ;
extern int copy_quote_and_fix_slashes(char *,char *, int * );
extern char* concat_args_and_quote(char **,char**,char **,unsigned int *, char **, 
	unsigned int *) ;


extern int is_nt_executable(char*,char*);
/* io.c */
extern int  force_read(int, unsigned char*,size_t);
extern int  nt_read(int, unsigned char*,size_t);
extern int  nt_write(int, const unsigned char*,size_t);
extern int stringtable_read(int,char*,size_t);

/* tparse.c */
extern int  tc_putc(char,FILE*);


void nt_cleanup(void);

/* stdio.c */
extern int  nt_creat(const char*,int);
extern int  nt_close(int);
extern int  nt_open(const char*,int ,...);
extern int  nt_pipe(int*);
extern void restore_fds(void ) ;
extern void copy_fds(void);
extern void close_copied_fds(void ) ;
extern int  nt_fgetc(FILE*);
extern int	 nt_dup(int);
extern int  nt_fputs(char*,FILE*);
extern int  nt_putc(char,FILE*);
extern int  nt_fflush(FILE*);
extern int  nt_fputc(char, FILE*);
extern int  nt_fprintf(FILE*,char*,...);
extern int  nt_puts(char*);
extern int  nt_putchar(char);
extern int  nt_fclose(FILE*);
extern int  nt_fputs(char *, FILE*);
extern intptr_t  __nt_get_osfhandle(int);
extern int __nt_open_osfhandle(intptr_t, int);
extern int nt_clearerr(FILE*);
extern int  nt_dup2(int,int );
extern FILE* nt_fdopen(int,char*);
extern char *  nt_fgets(char *,int, FILE*);
extern int nt_fileno(FILE*);
extern FILE *nt_fopen(char *,char*);
extern int nt_fread(void *,size_t,size_t,FILE*);
extern int nt_fwrite(void*,size_t,size_t,FILE*);
extern int nt_fseek(FILE*,long,int);
extern long nt_ftell(FILE*);
extern int  nt_isatty(int);
extern int  nt_lseek(int,long,int);
extern int nt_printf(char*,...);
extern int nt_access(char*,int);
extern int nt_fstat(int, struct stat *) ;
extern int nt_stat(const char *, struct stat *) ;
extern void nt_close_on_exec(int , int);
extern void init_stdio(void) ;
extern int is_resource_file(int);
#ifndef STDIO_C
extern void *my_stdin,*my_stdout,*my_stderr;
#endif STDIO_C


/* nt.char.c */
extern unsigned char oem_it(unsigned char );
extern char *nt_cgets(int,int,char*);
extern void nls_dll_init(void);
extern void nls_dll_unload(void);
extern void nt_autoset_dspmbyte(void);

/* fork.c */
extern int fork_init(void);
extern int fork(void);
extern void *sbrk(int);
extern void *fmalloc(size_t);
extern void ffree(void *);
extern void *frealloc(void*,size_t);
extern void *fcalloc(size_t,size_t);
extern void set_stackbase(void*);

/* console.c */
extern void do_nt_cooked_mode(void );
extern void do_nt_raw_mode(void ) ;
extern int do_nt_check_cooked_mode(void);
extern void set_cons_attr (char *);
extern void NT_MoveToLineOrChar(int ,int ) ;
extern void nt_term_init(void);
extern void nt_term_cleanup(void);
extern void nt_set_size(int,int);
//extern DWORD set_cooked_mode(HANDLE);
//extern void set_raw_mode(HANDLE);
//extern void set_arbitrary_mode(HANDLE,DWORD);
extern void set_attributes(const unsigned char *color);

/* ../sh.exec.c */
extern int nt_check_if_windir(char *);
extern void nt_check_name_and_hash(int ,char *,int);


/* clip.c */
extern void cut_clip(void);
extern int paste_clip(void);
extern void init_clipboard(void);
extern HANDLE create_clip_writer_thread(void) ;
extern HANDLE create_clip_reader_thread(void) ;

/* signal.c */
extern int kill(int,int);
extern int nice(int);
extern void nt_init_signals(void) ;
extern void nt_cleanup_signals(void) ;
extern void start_sigchild_thread(HANDLE , DWORD ) ;

/* nt.who.c */
extern void start_ncbs(short **);
extern void cleanup_netbios(void);

/* ntfunc.c */
struct command;
extern void dostart(short **,struct command *);
extern void docls(short **,struct command *);
extern void dotitle(short **, struct command * ) ;
extern void dostacksize(short**,struct command *);
extern void dosourceresource(short **, struct command * ) ;
extern void doprintresource(short **, struct command * ) ;
#ifdef NTDBG
extern void dodebugbreak(short **, struct command * ) ;
#endif NTDBG
extern void nt_set_env(const short *, const short*);
extern char *hb_subst(char *) ;
extern void init_hb_subst() ;
extern void init_shell_dll(void) ;
extern void try_shell_ex(char**,int,BOOL);
extern int nt_try_fast_exec(struct command *);
extern int nt_feed_to_cmd(char*,char**);
extern short nt_translate_bindkey(const short*);

extern struct biltins *nt_check_additional_builtins(short *);
extern void nt_print_builtins(size_t);

/* ps.c */
extern void init_plister(void);
extern void dops(short **,struct command *);
extern void doshutdown(short **,struct command *);
extern int kill_by_wm_close(int ) ;

/* globals.c */
extern int  is_gui(char*);
extern int  is_9x_gui(char*);


/* Global variables */
extern unsigned short __nt_want_vcode,__nt_vcode,__nt_really_exec;
extern int __dup_stdin;
extern int __nt_only_start_exes;
extern unsigned short __nt_child_nohupped;
extern DWORD gdwPlatform,gdwVersion;
extern int is_dev_clipboard_active;
extern HANDLE ghdevclipthread;
extern DWORD gdwStackSize;

// bogus
#define getppid() 0

struct tms {
	clock_t tms_utime;
	clock_t tms_stime;
	clock_t tms_cutime;
	clock_t tms_cstime;
};
#define UT_UNKNOWN 0
#define DEAD_PROCESS 7
#define USER_PROCESS 8
#define UT_LINESIZE  16
#define UT_NAMESIZE  8
#define UT_HOSTSIZE  16

struct utmp {
	short ut_type; /* type of login */
	pid_t ut_pid;
	char ut_line[UT_LINESIZE]; /* device pref'/dev/' */
	char ut_id[2]; /*abbrev tty name */
	time_t ut_time; /* login time */
	char ut_user[UT_NAMESIZE]; /* user name */
	char ut_host[UT_HOSTSIZE]; /* hostname for rlogin */
	long ut_addr;  /*ipaddr of remote host */
};


#define ut_name ut_user
#define killpg kill

#endif NTPORT_H
