/*
 * iperlsys.h - Perl's interface to the system
 *
 * This file defines the system level functionality that perl needs.
 *
 * When using C, this definition is in the form of a set of macros
 * that can be #defined to the system-level function (or a wrapper
 * provided elsewhere).
 *
 * When using C++ with -DPERL_OBJECT, this definition is in the
 * form of a set of virtual base classes which must be subclassed to
 * provide a real implementation.  The Perl Object will use instances
 * of this implementation to use the system-level functionality.
 *
 * GSAR 21-JUN-98
 */

#ifndef __Inc__IPerl___
#define __Inc__IPerl___

/*
 *	PerlXXX_YYY explained - DickH and DougL @ ActiveState.com
 *
 * XXX := functional group
 * YYY := stdlib/OS function name
 *
 * Continuing with the theme of PerlIO, all OS functionality was
 * encapsulated into one of several interfaces.
 *
 * PerlIO - stdio
 * PerlLIO - low level I/O
 * PerlMem - malloc, realloc, free
 * PerlDir - directory related
 * PerlEnv - process environment handling
 * PerlProc - process control
 * PerlSock - socket functions
 *
 *
 * The features of this are:
 * 1. All OS dependant code is in the Perl Host and not the Perl Core.
 *    (At least this is the holy grail goal of this work)
 * 2. The Perl Host (see perl.h for description) can provide a new and
 *    improved interface to OS functionality if required.
 * 3. Developers can easily hook into the OS calls for instrumentation
 *    or diagnostic purposes.
 *
 * What was changed to do this:
 * 1. All calls to OS functions were replaced with PerlXXX_YYY
 *
 */


/*
    Interface for perl stdio functions
*/


/* Clean up (or at least document) the various possible #defines.
   This section attempts to match the 5.003_03 Configure variables
   onto the 5.003_02 header file values.
   I can't figure out where USE_STDIO was supposed to be set.
   --AD
*/
#ifndef USE_PERLIO
# define PERLIO_IS_STDIO
#endif

/* Below is the 5.003_02 stuff. */
#ifdef USE_STDIO
#  ifndef PERLIO_IS_STDIO
#      define PERLIO_IS_STDIO
#  endif
#else
extern void PerlIO_init _((void));
#endif

#ifdef PERL_OBJECT

#ifndef PerlIO
typedef struct _PerlIO PerlIO;
#endif

class IPerlStdIO
{
public:
    virtual PerlIO *	Stdin(void) = 0;
    virtual PerlIO *	Stdout(void) = 0;
    virtual PerlIO *	Stderr(void) = 0;
    virtual PerlIO *	Open(const char *, const char *, int &err) = 0;
    virtual int		Close(PerlIO*, int &err) = 0;
    virtual int		Eof(PerlIO*, int &err) = 0;
    virtual int		Error(PerlIO*, int &err) = 0;
    virtual void	Clearerr(PerlIO*, int &err) = 0;
    virtual int		Getc(PerlIO*, int &err) = 0;
    virtual char *	GetBase(PerlIO *, int &err) = 0;
    virtual int		GetBufsiz(PerlIO *, int &err) = 0;
    virtual int		GetCnt(PerlIO *, int &err) = 0;
    virtual char *	GetPtr(PerlIO *, int &err) = 0;
    virtual char *	Gets(PerlIO*, char*, int, int& err) = 0;
    virtual int		Putc(PerlIO*, int, int &err) = 0;
    virtual int		Puts(PerlIO*, const char *, int &err) = 0;
    virtual int		Flush(PerlIO*, int &err) = 0;
    virtual int		Ungetc(PerlIO*,int, int &err) = 0;
    virtual int		Fileno(PerlIO*, int &err) = 0;
    virtual PerlIO *	Fdopen(int, const char *, int &err) = 0;
    virtual PerlIO *	Reopen(const char*, const char*, PerlIO*, int &err) = 0;
    virtual SSize_t	Read(PerlIO*,void *,Size_t, int &err) = 0;
    virtual SSize_t	Write(PerlIO*,const void *,Size_t, int &err) = 0;
    virtual void	SetBuf(PerlIO *, char*, int &err) = 0;
    virtual int		SetVBuf(PerlIO *, char*, int, Size_t, int &err) = 0;
    virtual void	SetCnt(PerlIO *, int, int &err) = 0;
    virtual void	SetPtrCnt(PerlIO *, char *, int, int& err) = 0;
    virtual void	Setlinebuf(PerlIO*, int &err) = 0;
    virtual int		Printf(PerlIO*, int &err, const char *,...) = 0;
    virtual int		Vprintf(PerlIO*, int &err, const char *, va_list) = 0;
    virtual long	Tell(PerlIO*, int &err) = 0;
    virtual int		Seek(PerlIO*, Off_t, int, int &err) = 0;
    virtual void	Rewind(PerlIO*, int &err) = 0;
    virtual PerlIO *	Tmpfile(int &err) = 0;
    virtual int		Getpos(PerlIO*, Fpos_t *, int &err) = 0;
    virtual int		Setpos(PerlIO*, const Fpos_t *, int &err) = 0;
    virtual void	Init(int &err) = 0;
    virtual void	InitOSExtras(void* p) = 0;
#ifdef WIN32
    virtual int		OpenOSfhandle(long osfhandle, int flags) = 0;
    virtual int		GetOSfhandle(int filenum) = 0;
#endif
};



#ifdef USE_STDIO_PTR
#  define PerlIO_has_cntptr(f)		1       
#  ifdef STDIO_CNT_LVALUE
#    define PerlIO_canset_cnt(f)	1      
#    ifdef STDIO_PTR_LVALUE
#      define PerlIO_fast_gets(f)	1        
#    endif
#  else
#    define PerlIO_canset_cnt(f)	0      
#  endif
#else  /* USE_STDIO_PTR */
#  define PerlIO_has_cntptr(f)		0
#  define PerlIO_canset_cnt(f)		0
#endif /* USE_STDIO_PTR */

#ifndef PerlIO_fast_gets
#define PerlIO_fast_gets(f)		0        
#endif

#ifdef FILE_base
#define PerlIO_has_base(f)		1
#else
#define PerlIO_has_base(f)		0
#endif

#define PerlIO_stdin()		PL_piStdIO->Stdin()
#define PerlIO_stdout()		PL_piStdIO->Stdout()
#define PerlIO_stderr()		PL_piStdIO->Stderr()
#define PerlIO_open(x,y)	PL_piStdIO->Open((x),(y), ErrorNo())
#define PerlIO_close(f)		PL_piStdIO->Close((f), ErrorNo())
#define PerlIO_eof(f)		PL_piStdIO->Eof((f), ErrorNo())
#define PerlIO_error(f)		PL_piStdIO->Error((f), ErrorNo())
#define PerlIO_clearerr(f)	PL_piStdIO->Clearerr((f), ErrorNo())
#define PerlIO_getc(f)		PL_piStdIO->Getc((f), ErrorNo())
#define PerlIO_get_base(f)	PL_piStdIO->GetBase((f), ErrorNo())
#define PerlIO_get_bufsiz(f)	PL_piStdIO->GetBufsiz((f), ErrorNo())
#define PerlIO_get_cnt(f)	PL_piStdIO->GetCnt((f), ErrorNo())
#define PerlIO_get_ptr(f)	PL_piStdIO->GetPtr((f), ErrorNo())
#define PerlIO_putc(f,c)	PL_piStdIO->Putc((f),(c), ErrorNo())
#define PerlIO_puts(f,s)	PL_piStdIO->Puts((f),(s), ErrorNo())
#define PerlIO_flush(f)		PL_piStdIO->Flush((f), ErrorNo())
#define PerlIO_gets(s, n, fp)   PL_piStdIO->Gets((fp), s, n, ErrorNo())
#define PerlIO_ungetc(f,c)	PL_piStdIO->Ungetc((f),(c), ErrorNo())
#define PerlIO_fileno(f)	PL_piStdIO->Fileno((f), ErrorNo())
#define PerlIO_fdopen(f, s)	PL_piStdIO->Fdopen((f),(s), ErrorNo())
#define PerlIO_reopen(p, m, f)  PL_piStdIO->Reopen((p), (m), (f), ErrorNo())
#define PerlIO_read(f,buf,count)					\
	(SSize_t)PL_piStdIO->Read((f), (buf), (count), ErrorNo())
#define PerlIO_write(f,buf,count)					\
	PL_piStdIO->Write((f), (buf), (count), ErrorNo())
#define PerlIO_setbuf(f,b)	PL_piStdIO->SetBuf((f), (b), ErrorNo())
#define PerlIO_setvbuf(f,b,t,s)	PL_piStdIO->SetVBuf((f), (b), (t), (s), ErrorNo())
#define PerlIO_set_cnt(f,c)	PL_piStdIO->SetCnt((f), (c), ErrorNo())
#define PerlIO_set_ptrcnt(f,p,c)					\
	PL_piStdIO->SetPtrCnt((f), (p), (c), ErrorNo())
#define PerlIO_setlinebuf(f)	PL_piStdIO->Setlinebuf((f), ErrorNo())
#define PerlIO_printf		fprintf
#define PerlIO_stdoutf		PL_piStdIO->Printf
#define PerlIO_vprintf(f,fmt,a)	PL_piStdIO->Vprintf((f), ErrorNo(), (fmt),a)          
#define PerlIO_tell(f)		PL_piStdIO->Tell((f), ErrorNo())
#define PerlIO_seek(f,o,w)	PL_piStdIO->Seek((f),(o),(w), ErrorNo())
#define PerlIO_getpos(f,p)	PL_piStdIO->Getpos((f),(p), ErrorNo())
#define PerlIO_setpos(f,p)	PL_piStdIO->Setpos((f),(p), ErrorNo())
#define PerlIO_rewind(f)	PL_piStdIO->Rewind((f), ErrorNo())
#define PerlIO_tmpfile()	PL_piStdIO->Tmpfile(ErrorNo())
#define PerlIO_init()		PL_piStdIO->Init(ErrorNo())
#undef 	init_os_extras
#define init_os_extras()	PL_piStdIO->InitOSExtras(this)

#else	/* PERL_OBJECT */

#include "perlsdio.h"

#endif	/* PERL_OBJECT */

#ifndef PERLIO_IS_STDIO
#ifdef USE_SFIO
#include "perlsfio.h"
#endif /* USE_SFIO */
#endif /* PERLIO_IS_STDIO */

#ifndef EOF
#define EOF (-1)
#endif

/* This is to catch case with no stdio */
#ifndef BUFSIZ
#define BUFSIZ 1024
#endif

#ifndef SEEK_SET
#define SEEK_SET 0
#endif

#ifndef SEEK_CUR
#define SEEK_CUR 1
#endif

#ifndef SEEK_END
#define SEEK_END 2
#endif

#ifndef PerlIO
struct _PerlIO;
#define PerlIO struct _PerlIO
#endif /* No PerlIO */

#ifndef Fpos_t
#define Fpos_t long
#endif

#ifndef NEXT30_NO_ATTRIBUTE
#ifndef HASATTRIBUTE       /* disable GNU-cc attribute checking? */
#ifdef  __attribute__      /* Avoid possible redefinition errors */
#undef  __attribute__
#endif
#define __attribute__(attr)
#endif
#endif

#ifndef PerlIO_stdoutf
extern int	PerlIO_stdoutf		_((const char *,...))
					__attribute__((format (printf, 1, 2)));
#endif
#ifndef PerlIO_puts
extern int	PerlIO_puts		_((PerlIO *,const char *));
#endif
#ifndef PerlIO_open
extern PerlIO *	PerlIO_open		_((const char *,const char *));
#endif
#ifndef PerlIO_close
extern int	PerlIO_close		_((PerlIO *));
#endif
#ifndef PerlIO_eof
extern int	PerlIO_eof		_((PerlIO *));
#endif
#ifndef PerlIO_error
extern int	PerlIO_error		_((PerlIO *));
#endif
#ifndef PerlIO_clearerr
extern void	PerlIO_clearerr		_((PerlIO *));
#endif
#ifndef PerlIO_getc
extern int	PerlIO_getc		_((PerlIO *));
#endif
#ifndef PerlIO_putc
extern int	PerlIO_putc		_((PerlIO *,int));
#endif
#ifndef PerlIO_flush
extern int	PerlIO_flush		_((PerlIO *));
#endif
#ifndef PerlIO_ungetc
extern int	PerlIO_ungetc		_((PerlIO *,int));
#endif
#ifndef PerlIO_fileno
extern int	PerlIO_fileno		_((PerlIO *));
#endif
#ifndef PerlIO_fdopen
extern PerlIO *	PerlIO_fdopen		_((int, const char *));
#endif
#ifndef PerlIO_importFILE
extern PerlIO *	PerlIO_importFILE	_((FILE *,int));
#endif
#ifndef PerlIO_exportFILE
extern FILE *	PerlIO_exportFILE	_((PerlIO *,int));
#endif
#ifndef PerlIO_findFILE
extern FILE *	PerlIO_findFILE		_((PerlIO *));
#endif
#ifndef PerlIO_releaseFILE
extern void	PerlIO_releaseFILE	_((PerlIO *,FILE *));
#endif
#ifndef PerlIO_read
extern SSize_t	PerlIO_read		_((PerlIO *,void *,Size_t));
#endif
#ifndef PerlIO_write
extern SSize_t	PerlIO_write		_((PerlIO *,const void *,Size_t));
#endif
#ifndef PerlIO_setlinebuf
extern void	PerlIO_setlinebuf	_((PerlIO *));
#endif
#ifndef PerlIO_printf
extern int	PerlIO_printf		_((PerlIO *, const char *,...))
					__attribute__((format (printf, 2, 3)));
#endif
#ifndef PerlIO_sprintf
extern int	PerlIO_sprintf		_((char *, int, const char *,...))
					__attribute__((format (printf, 3, 4)));
#endif
#ifndef PerlIO_vprintf
extern int	PerlIO_vprintf		_((PerlIO *, const char *, va_list));
#endif
#ifndef PerlIO_tell
extern Off_t	PerlIO_tell		_((PerlIO *));
#endif
#ifndef PerlIO_seek
extern int	PerlIO_seek		_((PerlIO *, Off_t, int));
#endif
#ifndef PerlIO_rewind
extern void	PerlIO_rewind		_((PerlIO *));
#endif
#ifndef PerlIO_has_base
extern int	PerlIO_has_base		_((PerlIO *));
#endif
#ifndef PerlIO_has_cntptr
extern int	PerlIO_has_cntptr	_((PerlIO *));
#endif
#ifndef PerlIO_fast_gets
extern int	PerlIO_fast_gets	_((PerlIO *));
#endif
#ifndef PerlIO_canset_cnt
extern int	PerlIO_canset_cnt	_((PerlIO *));
#endif
#ifndef PerlIO_get_ptr
extern STDCHAR * PerlIO_get_ptr		_((PerlIO *));
#endif
#ifndef PerlIO_get_cnt
extern int	PerlIO_get_cnt		_((PerlIO *));
#endif
#ifndef PerlIO_set_cnt
extern void	PerlIO_set_cnt		_((PerlIO *,int));
#endif
#ifndef PerlIO_set_ptrcnt
extern void	PerlIO_set_ptrcnt	_((PerlIO *,STDCHAR *,int));
#endif
#ifndef PerlIO_get_base
extern STDCHAR * PerlIO_get_base	_((PerlIO *));
#endif
#ifndef PerlIO_get_bufsiz
extern int	PerlIO_get_bufsiz	_((PerlIO *));
#endif
#ifndef PerlIO_tmpfile
extern PerlIO *	PerlIO_tmpfile		_((void));
#endif
#ifndef PerlIO_stdin
extern PerlIO *	PerlIO_stdin	_((void));
#endif
#ifndef PerlIO_stdout
extern PerlIO *	PerlIO_stdout	_((void));
#endif
#ifndef PerlIO_stderr
extern PerlIO *	PerlIO_stderr	_((void));
#endif
#ifndef PerlIO_getpos
extern int	PerlIO_getpos		_((PerlIO *,Fpos_t *));
#endif
#ifndef PerlIO_setpos
extern int	PerlIO_setpos		_((PerlIO *,const Fpos_t *));
#endif


/*
 *   Interface for directory functions
 */

#ifdef PERL_OBJECT

class IPerlDir
{
public:
    virtual int		Makedir(const char *dirname, int mode, int &err) = 0;
    virtual int		Chdir(const char *dirname, int &err) = 0;
    virtual int		Rmdir(const char *dirname, int &err) = 0;
    virtual int		Close(DIR *dirp, int &err) = 0;
    virtual DIR *	Open(char *filename, int &err) = 0;
    virtual struct direct *Read(DIR *dirp, int &err) = 0;
    virtual void	Rewind(DIR *dirp, int &err) = 0;
    virtual void	Seek(DIR *dirp, long loc, int &err) = 0;
    virtual long	Tell(DIR *dirp, int &err) = 0;
};

#define PerlDir_mkdir(name, mode)				\
	PL_piDir->Makedir((name), (mode), ErrorNo())
#define PerlDir_chdir(name)					\
	PL_piDir->Chdir((name), ErrorNo())
#define PerlDir_rmdir(name)					\
	PL_piDir->Rmdir((name), ErrorNo())
#define PerlDir_close(dir)					\
	PL_piDir->Close((dir), ErrorNo())
#define PerlDir_open(name)					\
	PL_piDir->Open((name), ErrorNo())
#define PerlDir_read(dir)					\
	PL_piDir->Read((dir), ErrorNo())
#define PerlDir_rewind(dir)					\
	PL_piDir->Rewind((dir), ErrorNo())
#define PerlDir_seek(dir, loc)					\
	PL_piDir->Seek((dir), (loc), ErrorNo())
#define PerlDir_tell(dir)					\
	PL_piDir->Tell((dir), ErrorNo())

#else	/* PERL_OBJECT */

#define PerlDir_mkdir(name, mode)	Mkdir((name), (mode))
#ifdef VMS
#  define PerlDir_chdir(n)		chdir(((n) && *(n)) ? (n) : "SYS$LOGIN")
#else 
#  define PerlDir_chdir(name)		chdir((name))
#endif
#define PerlDir_rmdir(name)		rmdir((name))
#define PerlDir_close(dir)		closedir((dir))
#define PerlDir_open(name)		opendir((name))
#define PerlDir_read(dir)		readdir((dir))
#define PerlDir_rewind(dir)		rewinddir((dir))
#define PerlDir_seek(dir, loc)		seekdir((dir), (loc))
#define PerlDir_tell(dir)		telldir((dir))

#endif	/* PERL_OBJECT */

/*
    Interface for perl environment functions
*/

#ifdef PERL_OBJECT

class IPerlEnv
{
public:
    virtual char *	Getenv(const char *varname, int &err) = 0;
    virtual int		Putenv(const char *envstring, int &err) = 0;
    virtual char *	LibPath(char *patchlevel) =0;
    virtual char *	SiteLibPath(char *patchlevel) =0;
};

#define PerlEnv_putenv(str)		PL_piENV->Putenv((str), ErrorNo())
#define PerlEnv_getenv(str)		PL_piENV->Getenv((str), ErrorNo())
#ifdef WIN32
#define PerlEnv_lib_path(str)		PL_piENV->LibPath((str))
#define PerlEnv_sitelib_path(str)	PL_piENV->SiteLibPath((str))
#endif

#else	/* PERL_OBJECT */

#define PerlEnv_putenv(str)		putenv((str))
#define PerlEnv_getenv(str)		getenv((str))

#endif	/* PERL_OBJECT */

/*
    Interface for perl low-level IO functions
*/

#ifdef PERL_OBJECT

class IPerlLIO
{
public:
    virtual int		Access(const char *path, int mode, int &err) = 0;
    virtual int		Chmod(const char *filename, int pmode, int &err) = 0;
    virtual int		Chown(const char *filename, uid_t owner,
			      gid_t group, int &err) = 0;
    virtual int		Chsize(int handle, long size, int &err) = 0;
    virtual int		Close(int handle, int &err) = 0;
    virtual int		Dup(int handle, int &err) = 0;
    virtual int		Dup2(int handle1, int handle2, int &err) = 0;
    virtual int		Flock(int fd, int oper, int &err) = 0;
    virtual int		FileStat(int handle, struct stat *buffer, int &err) = 0;
    virtual int		IOCtl(int i, unsigned int u, char *data, int &err) = 0;
    virtual int		Isatty(int handle, int &err) = 0;
    virtual long	Lseek(int handle, long offset, int origin, int &err) = 0;
    virtual int		Lstat(const char *path, struct stat *buffer, int &err) = 0;
    virtual char *	Mktemp(char *Template, int &err) = 0;
    virtual int		Open(const char *filename, int oflag, int &err) = 0;	
    virtual int		Open(const char *filename, int oflag,
			     int pmode, int &err) = 0;	
    virtual int		Read(int handle, void *buffer,
			     unsigned int count, int &err) = 0;
    virtual int		Rename(const char *oname,
			       const char *newname, int &err) = 0;
    virtual int		Setmode(int handle, int mode, int &err) = 0;
    virtual int		NameStat(const char *path,
				 struct stat *buffer, int &err) = 0;
    virtual char *	Tmpnam(char *string, int &err) = 0;
    virtual int		Umask(int pmode, int &err) = 0;
    virtual int		Unlink(const char *filename, int &err) = 0;
    virtual int		Utime(char *filename, struct utimbuf *times, int &err) = 0;
    virtual int		Write(int handle, const void *buffer,
			      unsigned int count, int &err) = 0;
};

#define PerlLIO_access(file, mode)					\
	PL_piLIO->Access((file), (mode), ErrorNo())
#define PerlLIO_chmod(file, mode)					\
	PL_piLIO->Chmod((file), (mode), ErrorNo())
#define PerlLIO_chown(file, owner, group)				\
	PL_piLIO->Chown((file), (owner), (group), ErrorNo())
#define PerlLIO_chsize(fd, size)					\
	PL_piLIO->Chsize((fd), (size), ErrorNo())
#define PerlLIO_close(fd)						\
	PL_piLIO->Close((fd), ErrorNo())
#define PerlLIO_dup(fd)							\
	PL_piLIO->Dup((fd), ErrorNo())
#define PerlLIO_dup2(fd1, fd2)						\
	PL_piLIO->Dup2((fd1), (fd2), ErrorNo())
#define PerlLIO_flock(fd, op)						\
	PL_piLIO->Flock((fd), (op), ErrorNo())
#define PerlLIO_fstat(fd, buf)						\
	PL_piLIO->FileStat((fd), (buf), ErrorNo())
#define PerlLIO_ioctl(fd, u, buf)					\
	PL_piLIO->IOCtl((fd), (u), (buf), ErrorNo())
#define PerlLIO_isatty(fd)						\
	PL_piLIO->Isatty((fd), ErrorNo())
#define PerlLIO_lseek(fd, offset, mode)					\
	PL_piLIO->Lseek((fd), (offset), (mode), ErrorNo())
#define PerlLIO_lstat(name, buf)					\
	PL_piLIO->Lstat((name), (buf), ErrorNo())
#define PerlLIO_mktemp(file)						\
	PL_piLIO->Mktemp((file), ErrorNo())
#define PerlLIO_open(file, flag)					\
	PL_piLIO->Open((file), (flag), ErrorNo())
#define PerlLIO_open3(file, flag, perm)					\
	PL_piLIO->Open((file), (flag), (perm), ErrorNo())
#define PerlLIO_read(fd, buf, count)					\
	PL_piLIO->Read((fd), (buf), (count), ErrorNo())
#define PerlLIO_rename(oname, newname)					\
	PL_piLIO->Rename((oname), (newname), ErrorNo())
#define PerlLIO_setmode(fd, mode)					\
	PL_piLIO->Setmode((fd), (mode), ErrorNo())
#define PerlLIO_stat(name, buf)						\
	PL_piLIO->NameStat((name), (buf), ErrorNo())
#define PerlLIO_tmpnam(str)						\
	PL_piLIO->Tmpnam((str), ErrorNo())
#define PerlLIO_umask(mode)						\
	PL_piLIO->Umask((mode), ErrorNo())
#define PerlLIO_unlink(file)						\
	PL_piLIO->Unlink((file), ErrorNo())
#define PerlLIO_utime(file, time)					\
	PL_piLIO->Utime((file), (time), ErrorNo())
#define PerlLIO_write(fd, buf, count)					\
	PL_piLIO->Write((fd), (buf), (count), ErrorNo())

#else	/* PERL_OBJECT */

#define PerlLIO_access(file, mode)	access((file), (mode))
#define PerlLIO_chmod(file, mode)	chmod((file), (mode))
#define PerlLIO_chown(file, owner, grp)	chown((file), (owner), (grp))
#define PerlLIO_chsize(fd, size)	chsize((fd), (size))
#define PerlLIO_close(fd)		close((fd))
#define PerlLIO_dup(fd)			dup((fd))
#define PerlLIO_dup2(fd1, fd2)		dup2((fd1), (fd2))
#define PerlLIO_flock(fd, op)		FLOCK((fd), (op))
#define PerlLIO_fstat(fd, buf)		Fstat((fd), (buf))
#define PerlLIO_ioctl(fd, u, buf)	ioctl((fd), (u), (buf))
#define PerlLIO_isatty(fd)		isatty((fd))
#define PerlLIO_lseek(fd, offset, mode)	lseek((fd), (offset), (mode))
#define PerlLIO_lstat(name, buf)	lstat((name), (buf))
#define PerlLIO_mktemp(file)		mktemp((file))
#define PerlLIO_mkstemp(file)		mkstemp((file))
#define PerlLIO_open(file, flag)	open((file), (flag))
#define PerlLIO_open3(file, flag, perm)	open((file), (flag), (perm))
#define PerlLIO_read(fd, buf, count)	read((fd), (buf), (count))
#define PerlLIO_rename(old, new)	rename((old), (new))
#define PerlLIO_setmode(fd, mode)	setmode((fd), (mode))
#define PerlLIO_stat(name, buf)		Stat((name), (buf))
#define PerlLIO_tmpnam(str)		tmpnam((str))
#define PerlLIO_umask(mode)		umask((mode))
#define PerlLIO_unlink(file)		unlink((file))
#define PerlLIO_utime(file, time)	utime((file), (time))
#define PerlLIO_write(fd, buf, count)	write((fd), (buf), (count))

#endif	/* PERL_OBJECT */

/*
    Interface for perl memory allocation
*/

#ifdef PERL_OBJECT

class IPerlMem
{
public:
    virtual void *	Malloc(size_t) = 0;
    virtual void *	Realloc(void*, size_t) = 0;
    virtual void	Free(void*) = 0;
};

#define PerlMem_malloc(size)		PL_piMem->Malloc((size))
#define PerlMem_realloc(buf, size)	PL_piMem->Realloc((buf), (size))
#define PerlMem_free(buf)		PL_piMem->Free((buf))

#else	/* PERL_OBJECT */

#define PerlMem_malloc(size)		malloc((size))
#define PerlMem_realloc(buf, size)	realloc((buf), (size))
#define PerlMem_free(buf)		free((buf))

#endif	/* PERL_OBJECT */

/*
    Interface for perl process functions
*/


#ifdef PERL_OBJECT

#ifndef Sighandler_t
typedef Signal_t (*Sighandler_t) _((int));
#endif
#ifndef jmp_buf
#include <setjmp.h>
#endif

class IPerlProc
{
public:
    virtual void	Abort(void) = 0;
    virtual char *	Crypt(const char* clear, const char* salt) = 0;
    virtual void	Exit(int status) = 0;
    virtual void	_Exit(int status) = 0;
    virtual int		Execl(const char *cmdname, const char *arg0,
			      const char *arg1, const char *arg2,
			      const char *arg3) = 0;
    virtual int		Execv(const char *cmdname, const char *const *argv) = 0;
    virtual int		Execvp(const char *cmdname, const char *const *argv) = 0;
    virtual uid_t	Getuid(void) = 0;
    virtual uid_t	Geteuid(void) = 0;
    virtual gid_t	Getgid(void) = 0;
    virtual gid_t	Getegid(void) = 0;
    virtual char *	Getlogin(void) = 0;
    virtual int		Kill(int pid, int sig) = 0;
    virtual int		Killpg(int pid, int sig) = 0;
    virtual int		PauseProc(void) = 0;
    virtual PerlIO *	Popen(const char *command, const char *mode) = 0;
    virtual int		Pclose(PerlIO *stream) = 0;
    virtual int		Pipe(int *phandles) = 0;
    virtual int		Setuid(uid_t uid) = 0;
    virtual int		Setgid(gid_t gid) = 0;
    virtual int		Sleep(unsigned int) = 0;
    virtual int		Times(struct tms *timebuf) = 0;
    virtual int		Wait(int *status) = 0;
    virtual int		Waitpid(int pid, int *status, int flags) = 0;
    virtual Sighandler_t	Signal(int sig, Sighandler_t subcode) = 0;
#ifdef WIN32
    virtual void	GetSysMsg(char*& msg, DWORD& dwLen, DWORD dwErr) = 0;
    virtual void	FreeBuf(char* msg) = 0;
    virtual BOOL	DoCmd(char *cmd) = 0;
    virtual int		Spawn(char*cmds) = 0;
    virtual int		Spawnvp(int mode, const char *cmdname,
				const char *const *argv) = 0;
    virtual int		ASpawn(void *vreally, void **vmark, void **vsp) = 0;
#endif
};

#define PerlProc_abort()	PL_piProc->Abort()
#define PerlProc_crypt(c,s)	PL_piProc->Crypt((c), (s))
#define PerlProc_exit(s)	PL_piProc->Exit((s))
#define PerlProc__exit(s)	PL_piProc->_Exit((s))
#define PerlProc_execl(c, w, x, y, z)					\
	PL_piProc->Execl((c), (w), (x), (y), (z))

#define PerlProc_execv(c, a)	PL_piProc->Execv((c), (a))
#define PerlProc_execvp(c, a)	PL_piProc->Execvp((c), (a))
#define PerlProc_getuid()	PL_piProc->Getuid()
#define PerlProc_geteuid()	PL_piProc->Geteuid()
#define PerlProc_getgid()	PL_piProc->Getgid()
#define PerlProc_getegid()	PL_piProc->Getegid()
#define PerlProc_getlogin()	PL_piProc->Getlogin()
#define PerlProc_kill(i, a)	PL_piProc->Kill((i), (a))
#define PerlProc_killpg(i, a)	PL_piProc->Killpg((i), (a))
#define PerlProc_pause()	PL_piProc->PauseProc()
#define PerlProc_popen(c, m)	PL_piProc->Popen((c), (m))
#define PerlProc_pclose(f)	PL_piProc->Pclose((f))
#define PerlProc_pipe(fd)	PL_piProc->Pipe((fd))
#define PerlProc_setuid(u)	PL_piProc->Setuid((u))
#define PerlProc_setgid(g)	PL_piProc->Setgid((g))
#define PerlProc_sleep(t)	PL_piProc->Sleep((t))
#define PerlProc_times(t)	PL_piProc->Times((t))
#define PerlProc_wait(t)	PL_piProc->Wait((t))
#define PerlProc_waitpid(p,s,f)	PL_piProc->Waitpid((p), (s), (f))
#define PerlProc_setjmp(b, n)	Sigsetjmp((b), (n))
#define PerlProc_longjmp(b, n)	Siglongjmp((b), (n))
#define PerlProc_signal(n, h)	PL_piProc->Signal((n), (h))

#ifdef WIN32
#define PerlProc_GetSysMsg(s,l,e)					\
	PL_piProc->GetSysMsg((s), (l), (e))

#define PerlProc_FreeBuf(s)	PL_piProc->FreeBuf((s))
#define PerlProc_Cmd(s)		PL_piProc->DoCmd((s))
#define do_spawn(s)		PL_piProc->Spawn((s))
#define do_spawnvp(m, c, a)	PL_piProc->Spawnvp((m), (c), (a))
#define PerlProc_aspawn(m,c,a)	PL_piProc->ASpawn((m), (c), (a))
#endif

#else	/* PERL_OBJECT */

#define PerlProc_abort()	abort()
#define PerlProc_crypt(c,s)	crypt((c), (s))
#define PerlProc_exit(s)	exit((s))
#define PerlProc__exit(s)	_exit((s))
#define PerlProc_execl(c,w,x,y,z)					\
	execl((c), (w), (x), (y), (z))
#define PerlProc_execv(c, a)	execv((c), (a))
#define PerlProc_execvp(c, a)	execvp((c), (a))
#define PerlProc_getuid()	getuid()
#define PerlProc_geteuid()	geteuid()
#define PerlProc_getgid()	getgid()
#define PerlProc_getegid()	getegid()
#define PerlProc_getlogin()	getlogin()
#define PerlProc_kill(i, a)	kill((i), (a))
#define PerlProc_killpg(i, a)	killpg((i), (a))
#define PerlProc_pause()	Pause()
#define PerlProc_popen(c, m)	my_popen((c), (m))
#define PerlProc_pclose(f)	my_pclose((f))
#define PerlProc_pipe(fd)	pipe((fd))
#define PerlProc_setuid(u)	setuid((u))
#define PerlProc_setgid(g)	setgid((g))
#define PerlProc_sleep(t)	sleep((t))
#define PerlProc_times(t)	times((t))
#define PerlProc_wait(t)	wait((t))
#define PerlProc_waitpid(p,s,f)	waitpid((p), (s), (f))
#define PerlProc_setjmp(b, n)	Sigsetjmp((b), (n))
#define PerlProc_longjmp(b, n)	Siglongjmp((b), (n))
#define PerlProc_signal(n, h)	signal((n), (h))


#endif	/* PERL_OBJECT */

/*
    Interface for perl socket functions
*/

#ifdef PERL_OBJECT

class IPerlSock
{
public:
    virtual u_long	Htonl(u_long hostlong) = 0;
    virtual u_short	Htons(u_short hostshort) = 0;
    virtual u_long	Ntohl(u_long netlong) = 0;
    virtual u_short	Ntohs(u_short netshort) = 0;
    virtual SOCKET	Accept(SOCKET s, struct sockaddr* addr,
			       int* addrlen, int &err) = 0;
    virtual int		Bind(SOCKET s, const struct sockaddr* name,
			     int namelen, int &err) = 0;
    virtual int		Connect(SOCKET s, const struct sockaddr* name,
				int namelen, int &err) = 0;
    virtual void	Endhostent(int &err) = 0;
    virtual void	Endnetent(int &err) = 0;
    virtual void	Endprotoent(int &err) = 0;
    virtual void	Endservent(int &err) = 0;
    virtual int		Gethostname(char* name, int namelen, int &err) = 0;
    virtual int		Getpeername(SOCKET s, struct sockaddr* name,
				    int* namelen, int &err) = 0;
    virtual struct hostent *	Gethostbyaddr(const char* addr, int len,
					      int type, int &err) = 0;
    virtual struct hostent *	Gethostbyname(const char* name, int &err) = 0;
    virtual struct hostent *	Gethostent(int &err) = 0;
    virtual struct netent *	Getnetbyaddr(long net, int type, int &err) = 0;
    virtual struct netent *	Getnetbyname(const char *, int &err) = 0;
    virtual struct netent *	Getnetent(int &err) = 0;
    virtual struct protoent *	Getprotobyname(const char* name, int &err) = 0;
    virtual struct protoent *	Getprotobynumber(int number, int &err) = 0;
    virtual struct protoent *	Getprotoent(int &err) = 0;
    virtual struct servent *	Getservbyname(const char* name,
					      const char* proto, int &err) = 0;
    virtual struct servent *	Getservbyport(int port, const char* proto,
					      int &err) = 0;
    virtual struct servent *	Getservent(int &err) = 0;
    virtual int		Getsockname(SOCKET s, struct sockaddr* name,
				    int* namelen, int &err) = 0;
    virtual int		Getsockopt(SOCKET s, int level, int optname,
				   char* optval, int* optlen, int &err) = 0;
    virtual unsigned long	InetAddr(const char* cp, int &err) = 0;
    virtual char *	InetNtoa(struct in_addr in, int &err) = 0;
    virtual int		Listen(SOCKET s, int backlog, int &err) = 0;
    virtual int		Recv(SOCKET s, char* buf, int len,
			     int flags, int &err) = 0;
    virtual int		Recvfrom(SOCKET s, char* buf, int len, int flags,
				 struct sockaddr* from, int* fromlen, int &err) = 0;
    virtual int		Select(int nfds, char* readfds, char* writefds,
			       char* exceptfds, const struct timeval* timeout,
			       int &err) = 0;
    virtual int		Send(SOCKET s, const char* buf, int len,
			     int flags, int &err) = 0; 
    virtual int		Sendto(SOCKET s, const char* buf, int len, int flags,
			       const struct sockaddr* to, int tolen, int &err) = 0;
    virtual void	Sethostent(int stayopen, int &err) = 0;
    virtual void	Setnetent(int stayopen, int &err) = 0;
    virtual void	Setprotoent(int stayopen, int &err) = 0;
    virtual void	Setservent(int stayopen, int &err) = 0;
    virtual int		Setsockopt(SOCKET s, int level, int optname,
				   const char* optval, int optlen, int &err) = 0;
    virtual int		Shutdown(SOCKET s, int how, int &err) = 0;
    virtual SOCKET	Socket(int af, int type, int protocol, int &err) = 0;
    virtual int		Socketpair(int domain, int type, int protocol,
				   int* fds, int &err) = 0;
#ifdef WIN32
    virtual int		Closesocket(SOCKET s, int& err) = 0;
    virtual int		Ioctlsocket(SOCKET s, long cmd, u_long *argp,
				    int& err) = 0;
#endif
};

#define PerlSock_htonl(x)		PL_piSock->Htonl(x)
#define PerlSock_htons(x)		PL_piSock->Htons(x)
#define PerlSock_ntohl(x)		PL_piSock->Ntohl(x)
#define PerlSock_ntohs(x)		PL_piSock->Ntohs(x)
#define PerlSock_accept(s, a, l)	PL_piSock->Accept(s, a, l, ErrorNo())
#define PerlSock_bind(s, n, l)		PL_piSock->Bind(s, n, l, ErrorNo())
#define PerlSock_connect(s, n, l)	PL_piSock->Connect(s, n, l, ErrorNo())
#define PerlSock_endhostent()		PL_piSock->Endhostent(ErrorNo())
#define PerlSock_endnetent()		PL_piSock->Endnetent(ErrorNo())
#define PerlSock_endprotoent()		PL_piSock->Endprotoent(ErrorNo())
#define PerlSock_endservent()		PL_piSock->Endservent(ErrorNo())
#define PerlSock_gethostbyaddr(a, l, t)	PL_piSock->Gethostbyaddr(a, l, t, ErrorNo())
#define PerlSock_gethostbyname(n)	PL_piSock->Gethostbyname(n, ErrorNo())
#define PerlSock_gethostent()		PL_piSock->Gethostent(ErrorNo())
#define PerlSock_gethostname(n, l)	PL_piSock->Gethostname(n, l, ErrorNo())
#define PerlSock_getnetbyaddr(n, t)	PL_piSock->Getnetbyaddr(n, t, ErrorNo())
#define PerlSock_getnetbyname(c)	PL_piSock->Getnetbyname(c, ErrorNo())
#define PerlSock_getnetent()		PL_piSock->Getnetent(ErrorNo())
#define PerlSock_getpeername(s, n, l)	PL_piSock->Getpeername(s, n, l, ErrorNo())
#define PerlSock_getprotobyname(n)	PL_piSock->Getprotobyname(n, ErrorNo())
#define PerlSock_getprotobynumber(n)	PL_piSock->Getprotobynumber(n, ErrorNo())
#define PerlSock_getprotoent()		PL_piSock->Getprotoent(ErrorNo())
#define PerlSock_getservbyname(n, p)	PL_piSock->Getservbyname(n, p, ErrorNo())
#define PerlSock_getservbyport(port, p)	PL_piSock->Getservbyport(port, p, ErrorNo())
#define PerlSock_getservent()		PL_piSock->Getservent(ErrorNo())
#define PerlSock_getsockname(s, n, l)	PL_piSock->Getsockname(s, n, l, ErrorNo())
#define PerlSock_getsockopt(s,l,n,v,i)	PL_piSock->Getsockopt(s, l, n, v, i, ErrorNo())
#define PerlSock_inet_addr(c)		PL_piSock->InetAddr(c, ErrorNo())
#define PerlSock_inet_ntoa(i)		PL_piSock->InetNtoa(i, ErrorNo())
#define PerlSock_listen(s, b)		PL_piSock->Listen(s, b, ErrorNo())
#define PerlSock_recv(s, b, l, f)	PL_piSock->Recv(s, b, l, f, ErrorNo())
#define PerlSock_recvfrom(s,b,l,f,from,fromlen)				\
	PL_piSock->Recvfrom(s, b, l, f, from, fromlen, ErrorNo())
#define PerlSock_select(n, r, w, e, t)					\
	PL_piSock->Select(n, (char*)r, (char*)w, (char*)e, t, ErrorNo())
#define PerlSock_send(s, b, l, f)	PL_piSock->Send(s, b, l, f, ErrorNo())
#define PerlSock_sendto(s, b, l, f, t, tlen)				\
	PL_piSock->Sendto(s, b, l, f, t, tlen, ErrorNo())
#define PerlSock_sethostent(f)		PL_piSock->Sethostent(f, ErrorNo())
#define PerlSock_setnetent(f)		PL_piSock->Setnetent(f, ErrorNo())
#define PerlSock_setprotoent(f)		PL_piSock->Setprotoent(f, ErrorNo())
#define PerlSock_setservent(f)		PL_piSock->Setservent(f, ErrorNo())
#define PerlSock_setsockopt(s, l, n, v, len)				\
	PL_piSock->Setsockopt(s, l, n, v, len, ErrorNo())
#define PerlSock_shutdown(s, h)		PL_piSock->Shutdown(s, h, ErrorNo())
#define PerlSock_socket(a, t, p)	PL_piSock->Socket(a, t, p, ErrorNo())
#define PerlSock_socketpair(a, t, p, f)	PL_piSock->Socketpair(a, t, p, f, ErrorNo())

#else	/* PERL_OBJECT */

#define PerlSock_htonl(x)		htonl(x)
#define PerlSock_htons(x)		htons(x)
#define PerlSock_ntohl(x)		ntohl(x)
#define PerlSock_ntohs(x)		ntohs(x)
#define PerlSock_accept(s, a, l)	accept(s, a, l)
#define PerlSock_bind(s, n, l)		bind(s, n, l)
#define PerlSock_connect(s, n, l)	connect(s, n, l)

#define PerlSock_gethostbyaddr(a, l, t)	gethostbyaddr(a, l, t)
#define PerlSock_gethostbyname(n)	gethostbyname(n)
#define PerlSock_gethostent		gethostent
#define PerlSock_endhostent		endhostent
#define PerlSock_gethostname(n, l)	gethostname(n, l)

#define PerlSock_getnetbyaddr(n, t)	getnetbyaddr(n, t)
#define PerlSock_getnetbyname(n)	getnetbyname(n)
#define PerlSock_getnetent		getnetent
#define PerlSock_endnetent		endnetent
#define PerlSock_getpeername(s, n, l)	getpeername(s, n, l)

#define PerlSock_getprotobyname(n)	getprotobyname(n)
#define PerlSock_getprotobynumber(n)	getprotobynumber(n)
#define PerlSock_getprotoent		getprotoent
#define PerlSock_endprotoent		endprotoent

#define PerlSock_getservbyname(n, p)	getservbyname(n, p)
#define PerlSock_getservbyport(port, p)	getservbyport(port, p)
#define PerlSock_getservent		getservent
#define PerlSock_endservent		endservent

#define PerlSock_getsockname(s, n, l)	getsockname(s, n, l)
#define PerlSock_getsockopt(s,l,n,v,i)	getsockopt(s, l, n, v, i)
#define PerlSock_inet_addr(c)		inet_addr(c)
#define PerlSock_inet_ntoa(i)		inet_ntoa(i)
#define PerlSock_listen(s, b)		listen(s, b)
#define PerlSock_recv(s, b, l, f)	recv(s, b, l, f)
#define PerlSock_recvfrom(s, b, l, f, from, fromlen)			\
	recvfrom(s, b, l, f, from, fromlen)
#define PerlSock_select(n, r, w, e, t)	select(n, r, w, e, t)
#define PerlSock_send(s, b, l, f)	send(s, b, l, f)
#define PerlSock_sendto(s, b, l, f, t, tlen)				\
	sendto(s, b, l, f, t, tlen)
#define PerlSock_sethostent(f)		sethostent(f)
#define PerlSock_setnetent(f)		setnetent(f)
#define PerlSock_setprotoent(f)		setprotoent(f)
#define PerlSock_setservent(f)		setservent(f)
#define PerlSock_setsockopt(s, l, n, v, len)				\
	setsockopt(s, l, n, v, len)
#define PerlSock_shutdown(s, h)		shutdown(s, h)
#define PerlSock_socket(a, t, p)	socket(a, t, p)
#define PerlSock_socketpair(a, t, p, f)	socketpair(a, t, p, f)


#endif	/* PERL_OBJECT */

#endif	/* __Inc__IPerl___ */

