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

// Machine dependent definitions.

// Macro to force sane alignment rules.
//
// On x86-32 it is the case that 64-bit integers are 4-byte aligned when
// embedded in structs, even though they are 8-byte aligned when not
// embedded. Force 8-byte alignment explicitly.
#define MEMBER(type) alignas(alignof(type)) type
#define ASSERT_OFFSET(type, field, offset32, offset64)            \
  static_assert((sizeof(PTR(void)) == 4 &&                        \
                 offsetof(IDENT(type), field) == (offset32)) ||   \
                    (sizeof(PTR(void)) == 8 &&                    \
                     offsetof(IDENT(type), field) == (offset64)), \
                "Offset incorrect")
#define ASSERT_SIZE(type, size32, size64)                              \
  static_assert(                                                       \
      (sizeof(PTR(void)) == 4 && sizeof(IDENT(type)) == (size32)) ||   \
          (sizeof(PTR(void)) == 8 && sizeof(IDENT(type)) == (size64)), \
      "Size incorrect")

typedef void IDENT(threadentry_t)(cloudabi_tid_t, PTR(void));

// Auxiliary vector entry, used to provide paramters on startup.
typedef struct {
  uint32_t a_type;
  union {
    MEMBER(IDENT(size_t)) a_val;
    MEMBER(PTR(void)) a_ptr;
  };
} IDENT(auxv_t);
ASSERT_OFFSET(auxv_t, a_type, 0, 0);
ASSERT_OFFSET(auxv_t, a_val, 4, 8);
ASSERT_OFFSET(auxv_t, a_ptr, 4, 8);
ASSERT_SIZE(auxv_t, 8, 16);

typedef struct {
  MEMBER(PTR(const void)) iov_base;
  MEMBER(IDENT(size_t)) iov_len;
} IDENT(ciovec_t);
ASSERT_OFFSET(ciovec_t, iov_base, 0, 0);
ASSERT_OFFSET(ciovec_t, iov_len, 4, 8);
ASSERT_SIZE(ciovec_t, 8, 16);

typedef struct {
  MEMBER(cloudabi_userdata_t) userdata;
  MEMBER(cloudabi_errno_t) error;
  MEMBER(cloudabi_eventtype_t) type;
  union {
    // CLOUDABI_EVENTTYPE_CLOCK: Wait until the value of a clock
    // exceeds a value.
    struct {
      MEMBER(cloudabi_userdata_t) identifier;
    } clock;

    // CLOUDABI_EVENTTYPE_CONDVAR: Release a lock and wait on a
    // condition variable.
    struct {
      MEMBER(PTR(_Atomic(cloudabi_condvar_t))) condvar;
    } condvar;

    // CLOUDABI_EVENTTYPE_FD_READ and CLOUDABI_EVENTTYPE_FD_WRITE:
    // Wait for a file descriptor to allow read() and write() to be
    // called without blocking.
    struct {
      MEMBER(cloudabi_filesize_t) nbytes;
      MEMBER(cloudabi_fd_t) fd;
      MEMBER(uint16_t) flags;
    } fd_readwrite;

    // CLOUDABI_EVENT_LOCK_RDLOCK and CLOUDABI_EVENT_LOCK_WRLOCK: Wait
    // and acquire a read or write lock.
    struct {
      MEMBER(PTR(_Atomic(cloudabi_lock_t))) lock;
    } lock;

    // CLOUDABI_EVENTTYPE_PROC_TERMINATE: Wait for a process to terminate.
    struct {
      MEMBER(cloudabi_fd_t) fd;
      MEMBER(cloudabi_signal_t) signal;      // Non-zero if process got killed.
      MEMBER(cloudabi_exitcode_t) exitcode;  // Exit code.
    } proc_terminate;
  };
} IDENT(event_t);
ASSERT_OFFSET(event_t, userdata, 0, 0);
ASSERT_OFFSET(event_t, error, 8, 8);
ASSERT_OFFSET(event_t, type, 10, 10);
ASSERT_OFFSET(event_t, clock.identifier, 16, 16);
ASSERT_OFFSET(event_t, condvar.condvar, 16, 16);
ASSERT_OFFSET(event_t, fd_readwrite.nbytes, 16, 16);
ASSERT_OFFSET(event_t, fd_readwrite.fd, 24, 24);
ASSERT_OFFSET(event_t, fd_readwrite.flags, 28, 28);
ASSERT_OFFSET(event_t, lock.lock, 16, 16);
ASSERT_OFFSET(event_t, proc_terminate.fd, 16, 16);
ASSERT_OFFSET(event_t, proc_terminate.signal, 20, 20);
ASSERT_OFFSET(event_t, proc_terminate.exitcode, 24, 24);
ASSERT_SIZE(event_t, 32, 32);

typedef struct {
  MEMBER(PTR(void)) iov_base;
  MEMBER(IDENT(size_t)) iov_len;
} IDENT(iovec_t);
ASSERT_OFFSET(iovec_t, iov_base, 0, 0);
ASSERT_OFFSET(iovec_t, iov_len, 4, 8);
ASSERT_SIZE(iovec_t, 8, 16);

typedef struct {
  MEMBER(PTR(const IDENT(iovec_t))) ri_data;  // Data I/O vectors.
  MEMBER(IDENT(size_t)) ri_datalen;           // Number of data I/O vectors.
  MEMBER(PTR(cloudabi_fd_t)) ri_fds;          // File descriptors.
  MEMBER(IDENT(size_t)) ri_fdslen;            // Number of file descriptors.
  MEMBER(cloudabi_msgflags_t) ri_flags;       // Input flags.
} IDENT(recv_in_t);
ASSERT_OFFSET(recv_in_t, ri_data, 0, 0);
ASSERT_OFFSET(recv_in_t, ri_datalen, 4, 8);
ASSERT_OFFSET(recv_in_t, ri_fds, 8, 16);
ASSERT_OFFSET(recv_in_t, ri_fdslen, 12, 24);
ASSERT_OFFSET(recv_in_t, ri_flags, 16, 32);
ASSERT_SIZE(recv_in_t, 20, 40);

typedef struct {
  MEMBER(IDENT(size_t)) ro_datalen;  // Bytes of data received.
  MEMBER(IDENT(size_t)) ro_fdslen;   // Number of file descriptors received.
  MEMBER(cloudabi_sockaddr_t) ro_sockname;  // Address of receiver.
  MEMBER(cloudabi_sockaddr_t) ro_peername;  // Address of sender.
  MEMBER(cloudabi_msgflags_t) ro_flags;     // Output flags.
} IDENT(recv_out_t);
ASSERT_OFFSET(recv_out_t, ro_datalen, 0, 0);
ASSERT_OFFSET(recv_out_t, ro_fdslen, 4, 8);
ASSERT_OFFSET(recv_out_t, ro_sockname, 8, 16);
ASSERT_OFFSET(recv_out_t, ro_peername, 28, 36);
ASSERT_OFFSET(recv_out_t, ro_flags, 48, 56);
ASSERT_SIZE(recv_out_t, 52, 64);

typedef struct {
  MEMBER(PTR(const IDENT(ciovec_t))) si_data;  // Data I/O vectors.
  MEMBER(IDENT(size_t)) si_datalen;            // Number of data I/O vectors.
  MEMBER(PTR(const cloudabi_fd_t)) si_fds;     // File descriptors.
  MEMBER(IDENT(size_t)) si_fdslen;             // Number of file descriptors.
  MEMBER(cloudabi_msgflags_t) si_flags;        // Input flags.
} IDENT(send_in_t);
ASSERT_OFFSET(send_in_t, si_data, 0, 0);
ASSERT_OFFSET(send_in_t, si_datalen, 4, 8);
ASSERT_OFFSET(send_in_t, si_fds, 8, 16);
ASSERT_OFFSET(send_in_t, si_fdslen, 12, 24);
ASSERT_OFFSET(send_in_t, si_flags, 16, 32);
ASSERT_SIZE(send_in_t, 20, 40);

typedef struct {
  MEMBER(IDENT(size_t)) so_datalen;  // Bytes of data sent.
} IDENT(send_out_t);
ASSERT_OFFSET(send_out_t, so_datalen, 0, 0);
ASSERT_SIZE(send_out_t, 4, 8);

typedef struct {
  MEMBER(cloudabi_userdata_t) userdata;
  MEMBER(uint16_t) flags;
  MEMBER(cloudabi_eventtype_t) type;
  union {
    // CLOUDABI_EVENTTYPE_CLOCK: Wait until the value of a clock
    // exceeds a value.
    struct {
      MEMBER(cloudabi_userdata_t) identifier;
      MEMBER(cloudabi_clockid_t) clock_id;
      MEMBER(cloudabi_timestamp_t) timeout;
      MEMBER(cloudabi_timestamp_t) precision;
    } clock;

    // CLOUDABI_EVENTTYPE_CONDVAR: Release a lock and wait on a
    // condition variable.
    struct {
      MEMBER(PTR(_Atomic(cloudabi_condvar_t))) condvar;
      MEMBER(PTR(_Atomic(cloudabi_lock_t))) lock;
    } condvar;

    // CLOUDABI_EVENTTYPE_FD_READ and CLOUDABI_EVENTTYPE_FD_WRITE:
    // Wait for a file descriptor to allow read() and write() to be
    // called without blocking.
    struct {
      MEMBER(cloudabi_fd_t) fd;
    } fd_readwrite;

    // CLOUDABI_EVENT_LOCK_RDLOCK and CLOUDABI_EVENT_LOCK_WRLOCK: Wait
    // and acquire a read or write lock.
    struct {
      MEMBER(PTR(_Atomic(cloudabi_lock_t))) lock;
    } lock;

    // CLOUDABI_EVENTTYPE_PROC_TERMINATE: Wait for a process to terminate.
    struct {
      MEMBER(cloudabi_fd_t) fd;
    } proc_terminate;
  };
} IDENT(subscription_t);
ASSERT_OFFSET(subscription_t, userdata, 0, 0);
ASSERT_OFFSET(subscription_t, flags, 8, 8);
ASSERT_OFFSET(subscription_t, type, 10, 10);
ASSERT_OFFSET(subscription_t, clock.identifier, 16, 16);
ASSERT_OFFSET(subscription_t, clock.clock_id, 24, 24);
ASSERT_OFFSET(subscription_t, clock.timeout, 32, 32);
ASSERT_OFFSET(subscription_t, clock.precision, 40, 40);
ASSERT_OFFSET(subscription_t, condvar.condvar, 16, 16);
ASSERT_OFFSET(subscription_t, condvar.lock, 20, 24);
ASSERT_OFFSET(subscription_t, fd_readwrite.fd, 16, 16);
ASSERT_OFFSET(subscription_t, lock.lock, 16, 16);
ASSERT_OFFSET(subscription_t, proc_terminate.fd, 16, 16);
ASSERT_SIZE(subscription_t, 48, 48);

typedef struct {
  MEMBER(PTR(IDENT(threadentry_t))) entry_point;  // Entry point.
  MEMBER(PTR(void)) stack;                        // Pointer to stack buffer.
  MEMBER(IDENT(size_t)) stack_size;               // Size of stack buffer.
  MEMBER(PTR(void)) argument;  // Argument to be passed to entry point.
} IDENT(threadattr_t);
ASSERT_OFFSET(threadattr_t, entry_point, 0, 0);
ASSERT_OFFSET(threadattr_t, stack, 4, 8);
ASSERT_OFFSET(threadattr_t, stack_size, 8, 16);
ASSERT_OFFSET(threadattr_t, argument, 12, 24);
ASSERT_SIZE(threadattr_t, 16, 32);

#undef MEMBER
#undef ASSERT_OFFSET
#undef ASSERT_SIZE
