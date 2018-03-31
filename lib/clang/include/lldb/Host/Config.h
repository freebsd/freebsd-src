// $FreeBSD$
//===-- Config.h -----------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_HOST_CONFIG_H
#define LLDB_HOST_CONFIG_H

#define LLDB_CONFIG_TERMIOS_SUPPORTED

/* #undef LLDB_DISABLE_POSIX */

#define HAVE_SYS_EVENT_H 1

#define HAVE_PPOLL 1

#define HAVE_SIGACTION 1

#define HAVE_PROCESS_VM_READV 0

#define HAVE_NR_PROCESS_VM_READV 0

/* #undef HAVE_LIBCOMPRESSION */

#endif // #ifndef LLDB_HOST_CONFIG_H
