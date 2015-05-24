/*-
 * Copyright (c) 2007 Roman Divacky
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
 * $FreeBSD$
 */

#ifndef _LINUX_FILE_H_
#define	_LINUX_FILE_H_

#define	LINUX_AT_FDCWD			-100
#define	LINUX_AT_SYMLINK_NOFOLLOW	0x100
#define	LINUX_AT_EACCESS		0x200
#define	LINUX_AT_REMOVEDIR		0x200
#define	LINUX_AT_SYMLINK_FOLLOW		0x400

/*
 * posix_fadvise advice
 */
#define	LINUX_POSIX_FADV_NORMAL		0
#define	LINUX_POSIX_FADV_RANDOM		1
#define	LINUX_POSIX_FADV_SEQUENTIAL    	2
#define	LINUX_POSIX_FADV_WILLNEED      	3
#define	LINUX_POSIX_FADV_DONTNEED      	4
#define	LINUX_POSIX_FADV_NOREUSE       	5

/*
 * mount flags
 */
#define	LINUX_MS_RDONLY		0x0001
#define	LINUX_MS_NOSUID		0x0002
#define	LINUX_MS_NODEV		0x0004
#define	LINUX_MS_NOEXEC		0x0008
#define	LINUX_MS_REMOUNT	0x0020

/*
 * common open/fcntl flags
 */
#define	LINUX_O_RDONLY		00000000
#define	LINUX_O_WRONLY		00000001
#define	LINUX_O_RDWR		00000002
#define	LINUX_O_ACCMODE		00000003
#define	LINUX_O_CREAT		00000100
#define	LINUX_O_EXCL		00000200
#define	LINUX_O_NOCTTY		00000400
#define	LINUX_O_TRUNC		00001000
#define	LINUX_O_APPEND		00002000
#define	LINUX_O_NONBLOCK	00004000
#define	LINUX_O_NDELAY		LINUX_O_NONBLOCK
#define	LINUX_O_SYNC		00010000
#define	LINUX_FASYNC		00020000
#define	LINUX_O_DIRECT		00040000	/* Direct disk access hint */
#define	LINUX_O_LARGEFILE	00100000
#define	LINUX_O_DIRECTORY	00200000	/* Must be a directory */
#define	LINUX_O_NOFOLLOW	00400000	/* Do not follow links */
#define	LINUX_O_NOATIME		01000000
#define	LINUX_O_CLOEXEC		02000000

#define	LINUX_F_DUPFD		0
#define	LINUX_F_GETFD		1
#define	LINUX_F_SETFD		2
#define	LINUX_F_GETFL		3
#define	LINUX_F_SETFL		4
#ifndef LINUX_F_GETLK
#define	LINUX_F_GETLK		5
#define	LINUX_F_SETLK		6
#define	LINUX_F_SETLKW		7
#endif
#ifndef LINUX_F_SETOWN
#define	LINUX_F_SETOWN		8
#define	LINUX_F_GETOWN		9
#endif

#ifndef LINUX_F_RDLCK
#define	LINUX_F_RDLCK		0
#define	LINUX_F_WRLCK		1
#define	LINUX_F_UNLCK		2
#endif

#endif	/* !_LINUX_FILE_H_ */
