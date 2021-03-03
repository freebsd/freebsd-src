#!/bin/sh

#
# Copyright (c) 2015 EMC Corp.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

# Bug 198163 - Kernel panic in vm_reserv_populate()
# Test scenario by: ikosarev@accesssoftek.com
# http://people.freebsd.org/~pho/stress/log/kostik771.txt
# Fixed by r280238

. ../default.cfg

uname -a | egrep -q "i386|amd64" || exit 0
odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > vm_reserv_populate.cc
rm -f /tmp/vm_reserv_populate
mycc -o vm_reserv_populate -Wall -Wextra -g -O2 vm_reserv_populate.cc ||
    exit 1
rm -f vm_reserv_populate.cc

(cd $odir/../testcases/swap; ./swap -t 5m -i 20 -h -l 100 > /dev/null) &
./vm_reserv_populate
while pgrep -q swap; do
	pkill -9 swap
done
rm vm_reserv_populate
exit 0
EOF
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define INLINE inline
#define NOINLINE __attribute__((noinline))
#define SYSCALL(name) SYS_ ## name
#define internal_syscall __syscall

typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef unsigned long uptr;

struct atomic_uint32_t {
  typedef u32 Type;
  volatile Type val_dont_use;
};

struct atomic_uintptr_t {
  typedef uptr Type;
  volatile Type val_dont_use;
};

uptr internal_sched_yield() {
  return internal_syscall(SYSCALL(sched_yield));
}

enum memory_order {
  memory_order_relaxed = 1 << 0,
  memory_order_consume = 1 << 1,
  memory_order_acquire = 1 << 2,
  memory_order_release = 1 << 3,
  memory_order_acq_rel = 1 << 4,
  memory_order_seq_cst = 1 << 5
};

INLINE void proc_yield(int cnt) {
  __asm__ __volatile__("" ::: "memory");
  for (int i = 0; i < cnt; i++)
    __asm__ __volatile__("pause");
  __asm__ __volatile__("" ::: "memory");
}

template<typename T>
NOINLINE typename T::Type atomic_load(
    const volatile T *a, memory_order mo) {
  assert(mo & (memory_order_relaxed | memory_order_consume
      | memory_order_acquire | memory_order_seq_cst));
  assert(!((uptr)a % sizeof(*a)));
  typename T::Type v;

  if (sizeof(*a) < 8 || sizeof(void*) == 8) {
    // Assume that aligned loads are atomic.
    if (mo == memory_order_relaxed) {
      v = a->val_dont_use;
    } else if (mo == memory_order_consume) {
      // Assume that processor respects data dependencies
      // (and that compiler won't break them).
      __asm__ __volatile__("" ::: "memory");
      v = a->val_dont_use;
      __asm__ __volatile__("" ::: "memory");
    } else if (mo == memory_order_acquire) {
      __asm__ __volatile__("" ::: "memory");
      v = a->val_dont_use;
      // On x86 loads are implicitly acquire.
      __asm__ __volatile__("" ::: "memory");
    } else {  // seq_cst
      // On x86 plain MOV is enough for seq_cst store.
      __asm__ __volatile__("" ::: "memory");
      v = a->val_dont_use;
      __asm__ __volatile__("" ::: "memory");
    }
  } else {
    // 64-bit load on 32-bit platform.
    __asm__ __volatile__(
        "movq %1, %%mm0;"  // Use mmx reg for 64-bit atomic moves
        "movq %%mm0, %0;"  // (ptr could be read-only)
        "emms;"            // Empty mmx state/Reset FP regs
        : "=m" (v)
        : "m" (a->val_dont_use)
        : // mark the FP stack and mmx registers as clobbered
          "st", "st(1)", "st(2)", "st(3)", "st(4)", "st(5)", "st(6)", "st(7)",
#ifdef __MMX__
          "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7",
#endif  // #ifdef __MMX__
          "memory");
  }
  return v;
}

template<typename T>
INLINE void atomic_store(volatile T *a, typename T::Type v, memory_order mo) {
  assert(mo & (memory_order_relaxed | memory_order_release
      | memory_order_seq_cst));
  assert(!((uptr)a % sizeof(*a)));

  if (sizeof(*a) < 8 || sizeof(void*) == 8) {
    // Assume that aligned loads are atomic.
    if (mo == memory_order_relaxed) {
      a->val_dont_use = v;
    } else if (mo == memory_order_release) {
      // On x86 stores are implicitly release.
      __asm__ __volatile__("" ::: "memory");
      a->val_dont_use = v;
      __asm__ __volatile__("" ::: "memory");
    } else {  // seq_cst
      // On x86 stores are implicitly release.
      __asm__ __volatile__("" ::: "memory");
      a->val_dont_use = v;
      __sync_synchronize();
    }
  } else {
    // 64-bit store on 32-bit platform.
    __asm__ __volatile__(
        "movq %1, %%mm0;"  // Use mmx reg for 64-bit atomic moves
        "movq %%mm0, %0;"
        "emms;"            // Empty mmx state/Reset FP regs
        : "=m" (a->val_dont_use)
        : "m" (v)
        : // mark the FP stack and mmx registers as clobbered
          "st", "st(1)", "st(2)", "st(3)", "st(4)", "st(5)", "st(6)", "st(7)",
#ifdef __MMX__
          "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7",
#endif  // #ifdef __MMX__
          "memory");
    if (mo == memory_order_seq_cst)
      __sync_synchronize();
  }
}

template<typename T>
INLINE bool atomic_compare_exchange_strong(volatile T *a,
                                           typename T::Type *cmp,
                                           typename T::Type xchg,
                                           memory_order mo __unused) {
  typedef typename T::Type Type;
  Type cmpv = *cmp;
  Type prev = __sync_val_compare_and_swap(&a->val_dont_use, cmpv, xchg);
  if (prev == cmpv)
    return true;
  *cmp = prev;
  return false;
}

template<typename T>
INLINE bool atomic_compare_exchange_weak(volatile T *a,
                                         typename T::Type *cmp,
                                         typename T::Type xchg,
                                         memory_order mo) {
  return atomic_compare_exchange_strong(a, cmp, xchg, mo);
}

const u32 kTabSizeLog = 20;
const int kTabSize = 1 << kTabSizeLog;

static atomic_uintptr_t tab[kTabSize];

int x_fork(void) {
  for (int i = 0; i < kTabSize; ++i) {
    atomic_uintptr_t *p = &tab[i];
    for (int j = 0;; j++) {
      uptr cmp = atomic_load(p, memory_order_relaxed);
      if ((cmp & 1) == 0 &&
          atomic_compare_exchange_weak(p, &cmp, cmp | 1, memory_order_acquire))
        break;
      if (j < 10)
        proc_yield(10);
      else
        internal_sched_yield();
    }
  }

  int pid = fork();

  for (int i = 0; i < kTabSize; ++i) {
    atomic_uintptr_t *p = &tab[i];
    uptr s = atomic_load(p, memory_order_relaxed);
    atomic_store(p, (s & ~1UL), memory_order_release);
  }

  return pid;
}

void test() {
  pid_t pid = x_fork();
  if (pid) {
    pid_t p;
    while ((p = wait(NULL)) == -1) { }
  }
}

int main() {
  const int kChildren = 1000;
  for (int i = 0; i < kChildren; ++i) {
    pid_t pid = x_fork();
    if (!pid) {
      test();
      return 0;
    }
  }

  sleep(5);

  for (int i = 0; i < kChildren; ++i) {
    pid_t p;
    while ((p = wait(NULL)) == -1) {  }
  }

  return 0;
}
