#!/bin/sh

# Test scenario from:
# Bug 272585 - calling mprotect in an mmap-ed stack can affect non-target pages 
# Test scenario by: John F. Carr <jfc mit edu>

. ../default.cfg
set -u
prog=$(basename "$0" .sh)
cat > /tmp/$prog.c <<EOF
/* Test program from:
   Bug 272585 - calling mprotect in an mmap-ed stack can affect non-target pages
 */
#include <err.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sysexits.h>
#include <unistd.h>

#ifndef MAP_GROWSDOWN
#define MAP_GROWSDOWN 0
#endif
#ifndef MAP_STACK
#define MAP_STACK 0
#endif

int main(void)
{
  long pagesize;
  char *addr, *guard;
  size_t alloc_size;

  pagesize = sysconf(_SC_PAGESIZE);
  if (pagesize < 0)
    err(EX_OSERR, "getPAGESIZE");

  alloc_size = 0x200000 + pagesize;

  addr = mmap(0, alloc_size, PROT_READ|PROT_WRITE,
              MAP_GROWSDOWN|MAP_STACK|MAP_PRIVATE|MAP_ANONYMOUS,
              -1, 0);
  if (addr == MAP_FAILED) {
    err(EX_OSERR, "mmap");
  }

  /* Only 0x20 causes a failure. */
  guard = addr + alloc_size - 0x20 * pagesize;

  if (mprotect(guard, pagesize, PROT_NONE)) {
    err(EX_OSERR, "mprotect");
  }

  printf("mapped %p..%p, guard at %p\n", addr, addr + alloc_size, guard);
  fflush(stdout);

  ((volatile char *)guard)[-1];

  return 0;
}
EOF
mycc -o /tmp/$prog -Wall -Wextra -O0 /tmp/$prog.c || exit 0

cd /tmp
./$prog; s=$?
cd -

rm -f /tmp/$prog /tmp/$prog.c /tmp/$prog.core
exit $s
