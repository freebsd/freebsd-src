/*-
 * Copyright (c) 2006,2010 Joseph Koshy
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: elfts.h 1337 2010-12-31 15:38:31Z jkoshy $
 */

#ifndef	_ELF_TS_H_
#define	_ELF_TS_H_ 	1

/*
 * Common definitions used by test cases.
 */

/* Invocable component requires elf_version() to be set. */
#define	IC_REQUIRES_VERSION_INIT()					\
	extern int elfts_tcinit;					\
	void	(*tet_startup)(void) = elfts_init_version

/* Test purpose needs to check for initialization success */
#define	TP_CHECK_INITIALIZATION()		do {			\
		if (elfts_tcinit != TET_PASS) {			\
			tet_infoline("unresolved: test case setup "	\
			    "failed.");					\
			tet_result(elfts_tcinit);			\
			return;						\
		}							\
	} while (0)

/* Treat a memory area as containing ELF data */
#define	TS_OPEN_MEMORY(E,M)	do	{				\
		if (((E) = elf_memory((M), sizeof((M)))) == NULL) {	\
			tet_infoline("unresolved: elf_memory() "	\
			    "failed.");					\
			tet_result(TET_UNRESOLVED);			\
			return;						\
		}							\
	} while (0)

/* Get an ELF descriptor for a file */
#define	_TS_OPEN_FILE(E,FN,CMD,FD,ACTION)	do	{		\
		if (((E) = elfts_open_file((FN),(CMD),&(FD))) == NULL)	\
			ACTION						\
	} while (0)

#define	TS_OPEN_FILE(E,FN,CMD,FD)	_TS_OPEN_FILE(E,FN,CMD,FD,return;)

#define	_TS_WRITE_FILE(FN,DATA,DSZ,ACTION)	do	{		\
		int _fd;						\
		if ((_fd = open((FN), O_CREAT|O_WRONLY, 0666)) < 0) {	\
			tet_printf("unresolved: open("FN") failed: %s.",\
			    strerror(errno));				\
			ACTION						\
		}							\
		if (write(_fd, (DATA), (DSZ)) != (DSZ)) {		\
			tet_printf("unresolved: write("FN") failed: %s.",\
			    strerror(errno));				\
			ACTION						\
		}							\
		(void) close(_fd);					\
	} while (0)

#define	_TS_READ_FILE(FN,DATA,DSZ,ACTION)	do	{		\
		int _fd;						\
		size_t _rsz, _sz;					\
		struct stat _sb;					\
		if ((_fd = open((FN), O_RDONLY, 0)) < 0) {		\
			tet_printf("unresolved: open("FN") failed: %s.", \
			    strerror(errno));				\
			ACTION						\
		}							\
		if (fstat(_fd, &_sb) < 0) {				\
			tet_printf("unresolved: fstat("FN") failed: %s.", \
			    strerror(errno));				\
			ACTION						\
		}							\
		if ((DSZ) < _sb.st_size)				\
			_sz = (DSZ);					\
		else							\
			_sz = _sb.st_size;				\
		if ((_rsz = read(_fd, (DATA), _sz)) != _sz) {		\
			tet_printf("unresolved: read("FN") failed: %s.", \
			    strerror(errno));				\
			ACTION						\
		}							\
		(void) close(_fd);					\
	} while (0)

#define	TS_NEWFILE	"new.file"

void	elfts_init_version(void);

Elf	*elfts_open_file(const char *_fn, Elf_Cmd _cmd, int *_fdp);
int	elfts_compare_files(const char *_reffn, const char *fn);
char	*elfts_copy_file(const char *_fn, int *_error);

#endif	/* _LIBELF_TS_H_ */
