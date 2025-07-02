/*-
 * Copyright © 2025 ARJANEN Loïc Jean David
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef ARCHIVE_TIME_PRIVATE_H_INCLUDED
#define ARCHIVE_TIME_PRIVATE_H_INCLUDED

#ifndef __LIBARCHIVE_BUILD
#error This header is only to be used internally to libarchive.
#endif
#include <stdint.h>

/* NTFS time to Unix sec/nsec. */
void ntfs_to_unix(uint64_t ntfs, int64_t* secs, uint32_t* nsecs);
/* DOS time to Unix sec. */
int64_t dos_to_unix(uint32_t dos);
/* Unix sec/nsec to NTFS time. */
uint64_t unix_to_ntfs(int64_t secs, uint32_t nsecs);
/* Unix sec to DOS time. */
uint32_t unix_to_dos(int64_t secs);
#if defined(_WIN32) && !defined(__CYGWIN__)
#include <windef.h>
#include <winbase.h>
/* Windows FILETIME to NTFS time. */
uint64_t FILETIME_to_ntfs(const FILETIME* filetime);
#endif
#endif /* ARCHIVE_TIME_PRIVATE_H_INCLUDED */
