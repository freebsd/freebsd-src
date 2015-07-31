// Copyright (c) 2015 Nuxi, https://nuxi.nl/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.

#ifndef COMMON_SYSCALLDEFS_MI_H
#define COMMON_SYSCALLDEFS_MI_H

// Machine independent definitions.

// Socket address families.
#define CLOUDABI_AF_UNSPEC 0
#define CLOUDABI_AF_INET 1
#define CLOUDABI_AF_INET6 2
#define CLOUDABI_AF_UNIX 3

// File and memory I/O advice.
#define CLOUDABI_ADVICE_DONTNEED 1
#define CLOUDABI_ADVICE_NOREUSE 2
#define CLOUDABI_ADVICE_NORMAL 3
#define CLOUDABI_ADVICE_RANDOM 4
#define CLOUDABI_ADVICE_SEQUENTIAL 5
#define CLOUDABI_ADVICE_WILLNEED 6

// Auxiliary vector entries. All entries that are also part of the
// x86-64 ABI use the same number. All extensions start at 256.
#define CLOUDABI_AT_ARGDATA 256
#define CLOUDABI_AT_ARGDATALEN 257
#define CLOUDABI_AT_CANARY 258
#define CLOUDABI_AT_CANARYLEN 259
#define CLOUDABI_AT_NCPUS 260
#define CLOUDABI_AT_NULL 0
#define CLOUDABI_AT_PAGESZ 6
#define CLOUDABI_AT_PHDR 3
#define CLOUDABI_AT_PHNUM 4
#define CLOUDABI_AT_TID 261

// Clocks.
#define CLOUDABI_CLOCK_MONOTONIC 1
#define CLOUDABI_CLOCK_PROCESS_CPUTIME_ID 2
#define CLOUDABI_CLOCK_REALTIME 3
#define CLOUDABI_CLOCK_THREAD_CPUTIME_ID 4

// Condition variables.
#define CLOUDABI_CONDVAR_HAS_NO_WAITERS 0

// The start of a directory, to be passed to readdir().
#define CLOUDABI_DIRCOOKIE_START 0

// POSIX standard error numbers.
#define CLOUDABI_E2BIG 1
#define CLOUDABI_EACCES 2
#define CLOUDABI_EADDRINUSE 3
#define CLOUDABI_EADDRNOTAVAIL 4
#define CLOUDABI_EAFNOSUPPORT 5
#define CLOUDABI_EAGAIN 6
#define CLOUDABI_EALREADY 7
#define CLOUDABI_EBADF 8
#define CLOUDABI_EBADMSG 9
#define CLOUDABI_EBUSY 10
#define CLOUDABI_ECANCELED 11
#define CLOUDABI_ECHILD 12
#define CLOUDABI_ECONNABORTED 13
#define CLOUDABI_ECONNREFUSED 14
#define CLOUDABI_ECONNRESET 15
#define CLOUDABI_EDEADLK 16
#define CLOUDABI_EDESTADDRREQ 17
#define CLOUDABI_EDOM 18
#define CLOUDABI_EDQUOT 19
#define CLOUDABI_EEXIST 20
#define CLOUDABI_EFAULT 21
#define CLOUDABI_EFBIG 22
#define CLOUDABI_EHOSTUNREACH 23
#define CLOUDABI_EIDRM 24
#define CLOUDABI_EILSEQ 25
#define CLOUDABI_EINPROGRESS 26
#define CLOUDABI_EINTR 27
#define CLOUDABI_EINVAL 28
#define CLOUDABI_EIO 29
#define CLOUDABI_EISCONN 30
#define CLOUDABI_EISDIR 31
#define CLOUDABI_ELOOP 32
#define CLOUDABI_EMFILE 33
#define CLOUDABI_EMLINK 34
#define CLOUDABI_EMSGSIZE 35
#define CLOUDABI_EMULTIHOP 36
#define CLOUDABI_ENAMETOOLONG 37
#define CLOUDABI_ENETDOWN 38
#define CLOUDABI_ENETRESET 39
#define CLOUDABI_ENETUNREACH 40
#define CLOUDABI_ENFILE 41
#define CLOUDABI_ENOBUFS 42
#define CLOUDABI_ENODEV 43
#define CLOUDABI_ENOENT 44
#define CLOUDABI_ENOEXEC 45
#define CLOUDABI_ENOLCK 46
#define CLOUDABI_ENOLINK 47
#define CLOUDABI_ENOMEM 48
#define CLOUDABI_ENOMSG 49
#define CLOUDABI_ENOPROTOOPT 50
#define CLOUDABI_ENOSPC 51
#define CLOUDABI_ENOSYS 52
#define CLOUDABI_ENOTCONN 53
#define CLOUDABI_ENOTDIR 54
#define CLOUDABI_ENOTEMPTY 55
#define CLOUDABI_ENOTRECOVERABLE 56
#define CLOUDABI_ENOTSOCK 57
#define CLOUDABI_ENOTSUP 58
#define CLOUDABI_ENOTTY 59
#define CLOUDABI_ENXIO 60
#define CLOUDABI_EOVERFLOW 61
#define CLOUDABI_EOWNERDEAD 62
#define CLOUDABI_EPERM 63
#define CLOUDABI_EPIPE 64
#define CLOUDABI_EPROTO 65
#define CLOUDABI_EPROTONOSUPPORT 66
#define CLOUDABI_EPROTOTYPE 67
#define CLOUDABI_ERANGE 68
#define CLOUDABI_EROFS 69
#define CLOUDABI_ESPIPE 70
#define CLOUDABI_ESRCH 71
#define CLOUDABI_ESTALE 72
#define CLOUDABI_ETIMEDOUT 73
#define CLOUDABI_ETXTBSY 74
#define CLOUDABI_EXDEV 75

// Non-standard error numbers.
#define CLOUDABI_ENOTCAPABLE 76

#define CLOUDABI_EVENT_FD_READWRITE_HANGUP 0x1

// Filter types for cloudabi_eventtype_t.
#define CLOUDABI_EVENTTYPE_CLOCK 1
#define CLOUDABI_EVENTTYPE_CONDVAR 2
#define CLOUDABI_EVENTTYPE_FD_READ 3
#define CLOUDABI_EVENTTYPE_FD_WRITE 4
#define CLOUDABI_EVENTTYPE_LOCK_RDLOCK 5
#define CLOUDABI_EVENTTYPE_LOCK_WRLOCK 6
#define CLOUDABI_EVENTTYPE_PROC_TERMINATE 7

// File descriptor behavior flags.
#define CLOUDABI_FDFLAG_APPEND 0x1
#define CLOUDABI_FDFLAG_DSYNC 0x2
#define CLOUDABI_FDFLAG_NONBLOCK 0x4
#define CLOUDABI_FDFLAG_RSYNC 0x8
#define CLOUDABI_FDFLAG_SYNC 0x10

// fdstat_put() flags.
#define CLOUDABI_FDSTAT_FLAGS 0x1
#define CLOUDABI_FDSTAT_RIGHTS 0x2

// filestat_put() flags.
#define CLOUDABI_FILESTAT_ATIM 0x1
#define CLOUDABI_FILESTAT_ATIM_NOW 0x2
#define CLOUDABI_FILESTAT_MTIM 0x4
#define CLOUDABI_FILESTAT_MTIM_NOW 0x8
#define CLOUDABI_FILESTAT_SIZE 0x10

// File types returned through struct stat::st_mode.
#define CLOUDABI_FILETYPE_UNKNOWN 0
#define CLOUDABI_FILETYPE_BLOCK_DEVICE 0x10
#define CLOUDABI_FILETYPE_CHARACTER_DEVICE 0x11
#define CLOUDABI_FILETYPE_DIRECTORY 0x20
#define CLOUDABI_FILETYPE_FIFO 0x30
#define CLOUDABI_FILETYPE_POLL 0x40
#define CLOUDABI_FILETYPE_PROCESS 0x50
#define CLOUDABI_FILETYPE_REGULAR_FILE 0x60
#define CLOUDABI_FILETYPE_SHARED_MEMORY 0x70
#define CLOUDABI_FILETYPE_SOCKET_DGRAM 0x80
#define CLOUDABI_FILETYPE_SOCKET_SEQPACKET 0x81
#define CLOUDABI_FILETYPE_SOCKET_STREAM 0x82
#define CLOUDABI_FILETYPE_SYMBOLIC_LINK 0x90

// Read-write lock related constants.
#define CLOUDABI_LOCK_UNLOCKED 0                 // Lock is unlocked.
#define CLOUDABI_LOCK_WRLOCKED 0x40000000        // Lock is write locked.
#define CLOUDABI_LOCK_KERNEL_MANAGED 0x80000000  // Lock has waiters.
#define CLOUDABI_LOCK_BOGUS 0x80000000           // Lock is broken.

// Lookup properties for *at() functions.
#define CLOUDABI_LOOKUP_SYMLINK_FOLLOW (UINT64_C(0x1) << 32)

// Open flags for openat(), etc.
#define CLOUDABI_O_CREAT 0x1
#define CLOUDABI_O_DIRECTORY 0x2
#define CLOUDABI_O_EXCL 0x4
#define CLOUDABI_O_TRUNC 0x8

// File descriptor passed to poll() to poll just once.
#define CLOUDABI_POLL_ONCE 0xffffffff

// File descriptor returned to pdfork()'s child process.
#define CLOUDABI_PROCESS_CHILD 0xffffffff

// mmap() map flags.
#define CLOUDABI_MAP_ANON 0x1
#define CLOUDABI_MAP_FIXED 0x2
#define CLOUDABI_MAP_PRIVATE 0x4
#define CLOUDABI_MAP_SHARED 0x8

// File descriptor that must be passed in when using CLOUDABI_MAP_ANON.
#define CLOUDABI_MAP_ANON_FD 0xffffffff

// msync() flags.
#define CLOUDABI_MS_ASYNC 0x1
#define CLOUDABI_MS_INVALIDATE 0x2
#define CLOUDABI_MS_SYNC 0x4

// send() and recv() flags.
#define CLOUDABI_MSG_CTRUNC 0x1    // Control data truncated.
#define CLOUDABI_MSG_EOR 0x2       // Terminates a record.
#define CLOUDABI_MSG_PEEK 0x4      // Leave received data in queue.
#define CLOUDABI_MSG_TRUNC 0x8     // Normal data truncated.
#define CLOUDABI_MSG_WAITALL 0x10  // Attempt to fill the read buffer.

// mmap()/mprotect() protection flags.
#define CLOUDABI_PROT_EXEC 0x1
#define CLOUDABI_PROT_WRITE 0x2
#define CLOUDABI_PROT_READ 0x4

// File descriptor capabilities/rights.
#define CLOUDABI_RIGHT_BIT(bit) (UINT64_C(1) << (bit))
#define CLOUDABI_RIGHT_FD_DATASYNC CLOUDABI_RIGHT_BIT(0)
#define CLOUDABI_RIGHT_FD_READ CLOUDABI_RIGHT_BIT(1)
#define CLOUDABI_RIGHT_FD_SEEK CLOUDABI_RIGHT_BIT(2)
#define CLOUDABI_RIGHT_FD_STAT_PUT_FLAGS CLOUDABI_RIGHT_BIT(3)
#define CLOUDABI_RIGHT_FD_SYNC CLOUDABI_RIGHT_BIT(4)
#define CLOUDABI_RIGHT_FD_TELL CLOUDABI_RIGHT_BIT(5)
#define CLOUDABI_RIGHT_FD_WRITE CLOUDABI_RIGHT_BIT(6)
#define CLOUDABI_RIGHT_FILE_ADVISE CLOUDABI_RIGHT_BIT(7)
#define CLOUDABI_RIGHT_FILE_ALLOCATE CLOUDABI_RIGHT_BIT(8)
#define CLOUDABI_RIGHT_FILE_CREATE_DIRECTORY CLOUDABI_RIGHT_BIT(9)
#define CLOUDABI_RIGHT_FILE_CREATE_FILE CLOUDABI_RIGHT_BIT(10)
#define CLOUDABI_RIGHT_FILE_CREATE_FIFO CLOUDABI_RIGHT_BIT(11)
#define CLOUDABI_RIGHT_FILE_LINK_SOURCE CLOUDABI_RIGHT_BIT(12)
#define CLOUDABI_RIGHT_FILE_LINK_TARGET CLOUDABI_RIGHT_BIT(13)
#define CLOUDABI_RIGHT_FILE_OPEN CLOUDABI_RIGHT_BIT(14)
#define CLOUDABI_RIGHT_FILE_READDIR CLOUDABI_RIGHT_BIT(15)
#define CLOUDABI_RIGHT_FILE_READLINK CLOUDABI_RIGHT_BIT(16)
#define CLOUDABI_RIGHT_FILE_RENAME_SOURCE CLOUDABI_RIGHT_BIT(17)
#define CLOUDABI_RIGHT_FILE_RENAME_TARGET CLOUDABI_RIGHT_BIT(18)
#define CLOUDABI_RIGHT_FILE_STAT_FGET CLOUDABI_RIGHT_BIT(19)
#define CLOUDABI_RIGHT_FILE_STAT_FPUT_SIZE CLOUDABI_RIGHT_BIT(20)
#define CLOUDABI_RIGHT_FILE_STAT_FPUT_TIMES CLOUDABI_RIGHT_BIT(21)
#define CLOUDABI_RIGHT_FILE_STAT_GET CLOUDABI_RIGHT_BIT(22)
#define CLOUDABI_RIGHT_FILE_STAT_PUT_TIMES CLOUDABI_RIGHT_BIT(23)
#define CLOUDABI_RIGHT_FILE_SYMLINK CLOUDABI_RIGHT_BIT(24)
#define CLOUDABI_RIGHT_FILE_UNLINK CLOUDABI_RIGHT_BIT(25)
#define CLOUDABI_RIGHT_MEM_MAP CLOUDABI_RIGHT_BIT(26)
#define CLOUDABI_RIGHT_MEM_MAP_EXEC CLOUDABI_RIGHT_BIT(27)
#define CLOUDABI_RIGHT_POLL_FD_READWRITE CLOUDABI_RIGHT_BIT(28)
#define CLOUDABI_RIGHT_POLL_MODIFY CLOUDABI_RIGHT_BIT(29)
#define CLOUDABI_RIGHT_POLL_PROC_TERMINATE CLOUDABI_RIGHT_BIT(30)
#define CLOUDABI_RIGHT_POLL_WAIT CLOUDABI_RIGHT_BIT(31)
#define CLOUDABI_RIGHT_PROC_EXEC CLOUDABI_RIGHT_BIT(32)
#define CLOUDABI_RIGHT_SOCK_ACCEPT CLOUDABI_RIGHT_BIT(33)
#define CLOUDABI_RIGHT_SOCK_BIND_DIRECTORY CLOUDABI_RIGHT_BIT(34)
#define CLOUDABI_RIGHT_SOCK_BIND_SOCKET CLOUDABI_RIGHT_BIT(35)
#define CLOUDABI_RIGHT_SOCK_CONNECT_DIRECTORY CLOUDABI_RIGHT_BIT(36)
#define CLOUDABI_RIGHT_SOCK_CONNECT_SOCKET CLOUDABI_RIGHT_BIT(37)
#define CLOUDABI_RIGHT_SOCK_LISTEN CLOUDABI_RIGHT_BIT(38)
#define CLOUDABI_RIGHT_SOCK_SHUTDOWN CLOUDABI_RIGHT_BIT(39)
#define CLOUDABI_RIGHT_SOCK_STAT_GET CLOUDABI_RIGHT_BIT(40)

// Socket shutdown flags.
#define CLOUDABI_SHUT_RD 0x1
#define CLOUDABI_SHUT_WR 0x2

// Signals.
#define CLOUDABI_SIGABRT 1
#define CLOUDABI_SIGALRM 2
#define CLOUDABI_SIGBUS 3
#define CLOUDABI_SIGCHLD 4
#define CLOUDABI_SIGCONT 5
#define CLOUDABI_SIGFPE 6
#define CLOUDABI_SIGHUP 7
#define CLOUDABI_SIGILL 8
#define CLOUDABI_SIGINT 9
#define CLOUDABI_SIGKILL 10
#define CLOUDABI_SIGPIPE 11
#define CLOUDABI_SIGQUIT 12
#define CLOUDABI_SIGSEGV 13
#define CLOUDABI_SIGSTOP 14
#define CLOUDABI_SIGSYS 15
#define CLOUDABI_SIGTERM 16
#define CLOUDABI_SIGTRAP 17
#define CLOUDABI_SIGTSTP 18
#define CLOUDABI_SIGTTIN 19
#define CLOUDABI_SIGTTOU 20
#define CLOUDABI_SIGURG 21
#define CLOUDABI_SIGUSR1 22
#define CLOUDABI_SIGUSR2 23
#define CLOUDABI_SIGVTALRM 24
#define CLOUDABI_SIGXCPU 25
#define CLOUDABI_SIGXFSZ 26

// sockstat() flags.
#define CLOUDABI_SOCKSTAT_CLEAR_ERROR 0x1

// sockstat() state.
#define CLOUDABI_SOCKSTAT_ACCEPTCONN 0x1

// cloudabi_subscription_t flags.
#define CLOUDABI_SUBSCRIPTION_ADD 0x1
#define CLOUDABI_SUBSCRIPTION_CLEAR 0x2
#define CLOUDABI_SUBSCRIPTION_DELETE 0x4
#define CLOUDABI_SUBSCRIPTION_DISABLE 0x8
#define CLOUDABI_SUBSCRIPTION_ENABLE 0x10
#define CLOUDABI_SUBSCRIPTION_ONESHOT 0x20

// unlinkat().
#define CLOUDABI_UNLINK_REMOVEDIR 0x1

// Seeking.
#define CLOUDABI_WHENCE_CUR 1
#define CLOUDABI_WHENCE_END 2
#define CLOUDABI_WHENCE_SET 3

typedef uint8_t cloudabi_advice_t;      // posix_fadvise() and posix_madvise().
typedef uint32_t cloudabi_backlog_t;    // listen().
typedef uint32_t cloudabi_clockid_t;    // clock_*().
typedef uint32_t cloudabi_condvar_t;    // pthread_cond_*().
typedef uint64_t cloudabi_device_t;     // struct stat::st_dev.
typedef uint64_t cloudabi_dircookie_t;  // readdir().
typedef uint16_t cloudabi_errno_t;      // errno.
typedef uint8_t cloudabi_eventtype_t;   // poll().
typedef uint32_t cloudabi_exitcode_t;   // _exit() and _Exit().
typedef uint32_t cloudabi_fd_t;         // File descriptors.
typedef uint16_t cloudabi_fdflags_t;    // cloudabi_fdstat_t.
typedef uint16_t cloudabi_fdsflags_t;   // fd_stat_put().
typedef int64_t cloudabi_filedelta_t;   // lseek().
typedef uint64_t cloudabi_filesize_t;   // ftruncate(), struct stat::st_size.
typedef uint8_t cloudabi_filetype_t;    // struct stat::st_mode.
typedef uint16_t cloudabi_fsflags_t;    // file_stat_put().
typedef uint64_t cloudabi_inode_t;      // struct stat::st_ino.
typedef uint32_t cloudabi_linkcount_t;  // struct stat::st_nlink.
typedef uint32_t cloudabi_lock_t;       // pthread_{mutex,rwlock}_*().
typedef uint64_t cloudabi_lookup_t;     // openat(), linkat(), etc.
typedef uint8_t cloudabi_mflags_t;      // mmap().
typedef uint8_t cloudabi_mprot_t;       // mmap().
typedef uint8_t cloudabi_msflags_t;     // msync().
typedef uint16_t cloudabi_msgflags_t;   // send() and recv().
typedef uint32_t cloudabi_nthreads_t;   // pthread_cond_*().
typedef uint16_t cloudabi_oflags_t;     // openat(), etc.
typedef uint64_t cloudabi_rights_t;     // File descriptor rights.
typedef uint8_t cloudabi_sa_family_t;   // Socket address family.
typedef uint8_t cloudabi_sdflags_t;     // shutdown().
typedef uint8_t cloudabi_ssflags_t;     // sockstat().
typedef uint8_t cloudabi_signal_t;      // raise().
typedef uint32_t cloudabi_tid_t;        // Thread ID.
typedef uint64_t cloudabi_timestamp_t;  // clock_*(), struct stat::st_*tim.
typedef uint8_t cloudabi_ulflags_t;     // unlinkat().
typedef uint64_t cloudabi_userdata_t;   // User-supplied data for callbacks.
typedef uint8_t cloudabi_whence_t;      // lseek().

// Macro to force sane alignment rules.
//
// On x86-32 it is the case that 64-bit integers are 4-byte aligned when
// embedded in structs, even though they are 8-byte aligned when not
// embedded. Force 8-byte alignment explicitly.
#define MEMBER(type) alignas(alignof(type)) type
#define ASSERT_OFFSET(type, field, offset)                    \
  static_assert(offsetof(cloudabi_##type, field) == (offset), \
                "Offset incorrect")
#define ASSERT_SIZE(type, size) \
  static_assert(sizeof(cloudabi_##type) == (size), "Size incorrect")

// Directory entries.
typedef struct {
  MEMBER(cloudabi_dircookie_t) d_next;  // Cookie of the next entry.
  MEMBER(cloudabi_inode_t) d_ino;       // Inode number of the current entry.
  MEMBER(uint32_t) d_namlen;  // Length of the name of the current entry.
  MEMBER(cloudabi_filetype_t) d_type;  // File type of the current entry.
} cloudabi_dirent_t;
ASSERT_OFFSET(dirent_t, d_next, 0);
ASSERT_OFFSET(dirent_t, d_ino, 8);
ASSERT_OFFSET(dirent_t, d_namlen, 16);
ASSERT_OFFSET(dirent_t, d_type, 20);
ASSERT_SIZE(dirent_t, 24);

// File descriptor status.
typedef struct {
  MEMBER(cloudabi_filetype_t) fs_filetype;         // File descriptor type.
  MEMBER(cloudabi_fdflags_t) fs_flags;             // Non-blocking mode, etc.
  MEMBER(cloudabi_rights_t) fs_rights_base;        // Base rights.
  MEMBER(cloudabi_rights_t) fs_rights_inheriting;  // Inheriting rights.
} cloudabi_fdstat_t;
ASSERT_OFFSET(fdstat_t, fs_filetype, 0);
ASSERT_OFFSET(fdstat_t, fs_flags, 2);
ASSERT_OFFSET(fdstat_t, fs_rights_base, 8);
ASSERT_OFFSET(fdstat_t, fs_rights_inheriting, 16);
ASSERT_SIZE(fdstat_t, 24);

// File status.
typedef struct {
  MEMBER(cloudabi_device_t) st_dev;         // Device storing the file.
  MEMBER(cloudabi_inode_t) st_ino;          // Inode of the file.
  MEMBER(cloudabi_filetype_t) st_filetype;  // File type.
  MEMBER(cloudabi_linkcount_t) st_nlink;    // Number of hardlinks.
  MEMBER(cloudabi_filesize_t) st_size;      // Size of the file.
  MEMBER(cloudabi_timestamp_t) st_atim;     // Access time.
  MEMBER(cloudabi_timestamp_t) st_mtim;     // Modification time.
  MEMBER(cloudabi_timestamp_t) st_ctim;     // Change time.
} cloudabi_filestat_t;
ASSERT_OFFSET(filestat_t, st_dev, 0);
ASSERT_OFFSET(filestat_t, st_ino, 8);
ASSERT_OFFSET(filestat_t, st_filetype, 16);
ASSERT_OFFSET(filestat_t, st_nlink, 20);
ASSERT_OFFSET(filestat_t, st_size, 24);
ASSERT_OFFSET(filestat_t, st_atim, 32);
ASSERT_OFFSET(filestat_t, st_mtim, 40);
ASSERT_OFFSET(filestat_t, st_ctim, 48);
ASSERT_SIZE(filestat_t, 56);

typedef struct {
  MEMBER(cloudabi_sa_family_t) sa_family;
  union {
    struct {
      // IPv4 address and port number.
      MEMBER(uint8_t) addr[4];
      MEMBER(uint16_t) port;
    } sa_inet;
    struct {
      // IPv6 address and port number.
      // TODO(ed): What about the flow info and scope ID?
      MEMBER(uint8_t) addr[16];
      MEMBER(uint16_t) port;
    } sa_inet6;
  };
} cloudabi_sockaddr_t;
ASSERT_OFFSET(sockaddr_t, sa_family, 0);
ASSERT_OFFSET(sockaddr_t, sa_inet.addr, 2);
ASSERT_OFFSET(sockaddr_t, sa_inet.port, 6);
ASSERT_OFFSET(sockaddr_t, sa_inet6.addr, 2);
ASSERT_OFFSET(sockaddr_t, sa_inet6.port, 18);
ASSERT_SIZE(sockaddr_t, 20);

// Socket status.
typedef struct {
  MEMBER(cloudabi_sockaddr_t) ss_sockname;  // Socket address.
  MEMBER(cloudabi_sockaddr_t) ss_peername;  // Peer address.
  MEMBER(cloudabi_errno_t) ss_error;        // Current error state.
  MEMBER(uint32_t) ss_state;                // State flags.
} cloudabi_sockstat_t;
ASSERT_OFFSET(sockstat_t, ss_sockname, 0);
ASSERT_OFFSET(sockstat_t, ss_peername, 20);
ASSERT_OFFSET(sockstat_t, ss_error, 40);
ASSERT_OFFSET(sockstat_t, ss_state, 44);
ASSERT_SIZE(sockstat_t, 48);

#undef MEMBER
#undef ASSERT_OFFSET
#undef ASSERT_SIZE

#endif
