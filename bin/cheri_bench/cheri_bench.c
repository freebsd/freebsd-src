/*-
 * Copyright (c) 2014-2015 Robert N. M. Watson
 * Copyright (c) 2014 Robert M. Norton
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 */

#include <sys/cdefs.h>
#include <sys/types.h>
#include <err.h>
#include <stdio.h>
#include <sysexits.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <assert.h>
#include <sys/uio.h>

#include <machine/cheri.h>
#include <machine/cheric.h>
#include <cheri/cheri_fd.h>
#include <cheri/sandbox.h>
#include <cheri/cheri_invoke.h>
#include <cheri_bench-helper.h>

#define get_cyclecount cheri_get_cyclecount

static struct cheri_object objectp;

typedef uint64_t memcpy_t(__capability char *, __capability char *, size_t, int fd);

static uint64_t do_memcpy (__capability char *dataout, __capability char *datain, size_t len, int fd)
{
  uint32_t start_count, end_count;
  start_count = get_cyclecount();
  memcpy_c(dataout, datain, len);
  end_count = get_cyclecount();
  return end_count - start_count;
}

static uint64_t invoke_memcpy(__capability char *dataout, __capability char *datain, size_t len, int fd)
{
  int ret;
  uint32_t start_count, end_count;
  start_count = get_cyclecount();
  ret = cheri_invoke(objectp,
		     CHERI_BENCH_HELPER_OP_MEMCPY,
		     len, 0, 0, 0, 0, 0, 0, 0,
		     (__capability void *) dataout,
		     (__capability void *) datain,
		     cheri_zerocap(),
		     cheri_zerocap(),
		     cheri_zerocap(),
		     cheri_zerocap(),
		     cheri_zerocap(),
		     cheri_zerocap());
  end_count = get_cyclecount();
  if (ret != 0) err(1, "Invoke failed.");
  return end_count - start_count;
}

static inline void read_retry(int fd, char *buf, size_t len)
{
  ssize_t io_len;
  while(len > 0)
    {
      io_len = read(fd, buf, len);
      if (io_len < 0)
	err(1, "read_retry");
      len -= io_len;
      buf += io_len;
    }
}

static uint64_t socket_memcpy(__capability char *dataout, __capability char *datain, size_t len, int fd)
{
  ssize_t  io_len;
  uint32_t start_count = get_cyclecount();

  struct iovec iovs[2];
  iovs[0].iov_base = &len;
  iovs[0].iov_len  = sizeof(len);
  iovs[1].iov_base = (void *) datain;
  iovs[1].iov_len  = len;
  
  io_len = writev(fd, iovs, 2);
  if(io_len != (ssize_t)(sizeof(len) + len))
    err(1, "socket parent write");

  read_retry(fd, (char *) dataout, len);
  
  return get_cyclecount() - start_count;
}

static void socket_sandbox_func(int fd, size_t max_size)
{
  char*   buf = malloc(max_size);
  ssize_t io_len, msg_len;
  while(1)
    {
      /* Read length of message*/
      io_len = read(fd, &msg_len, sizeof(msg_len));
      if (io_len != sizeof(msg_len))
	exit(0); // XXX don't complain when parent dies err(1, "socket recv child len");
      read_retry(fd, buf, msg_len);
      io_len = write(fd, buf, msg_len);
      if (io_len < 0)
	err(1, "socket child write msg");
    }
}

static uint64_t shmem_memcpy(__capability char *dataout __unused, __capability char *datain __unused, size_t len, int fd)
{
  ssize_t  io_len;
  uint32_t start_count = get_cyclecount();
  io_len = write(fd, &len, sizeof(len));
  assert(io_len == sizeof(len)); // XXX rmn30 lazy
  io_len = read(fd, &len, sizeof(len));
  assert(io_len == sizeof(len));
  return get_cyclecount() - start_count;
}

static void shmem_sandbox_func(int fd, __capability char *datain, __capability char *dataout)
{
  size_t len;
  ssize_t io_len;
  while(1)
    {
      io_len = read(fd, &len, sizeof(len));
      if (io_len != sizeof(len))
        exit(0); // XXX don't complain when parent dies err(1, "shm child recv");
      memcpy_c(dataout, datain, len);
      io_len = write(fd, &len, sizeof(len));
      if (io_len != sizeof(len))
        err(1, "shm child write");
    }
}

int benchmark(memcpy_t *memcpy_func, __capability char *dataout, __capability char *datain, size_t size, uint reps, uint64_t *samples, int fd) __attribute__((__noinline__));
int benchmark(memcpy_t *memcpy_func, __capability char *dataout, __capability char *datain, size_t size, uint reps, uint64_t *samples, int fd)
{
      // Initialise arrays
      for (uint i=0; i < size; i++) 
	{
	  datain[i]  = (char) i;
	  dataout[i] = 0;
	}

      for (uint rep = 0; rep < reps; rep++) 
	{
	  samples[rep] = memcpy_func(dataout, datain, size, fd);
	  for (uint i=0; i < size; i+=8)
	    {
	      assert(dataout[i] == (char) i);
	      //datain[i]  = (char) i;
	      //dataout[i] = 0;
	    }
	}

      for (uint rep = 0; rep < reps; rep++)
	printf(",%lu", samples[rep]);
      return 0;
}

static void usage(void)
{
  errx(1,  "usage: cheri_bench [-fipsS] -r <reps> -o <in offset> -O <out offset> <size>...\n");
}

int
main(int argc, char *argv[])
{
	struct sandbox_class *classp;
	struct sandbox_object *sandboxp;
	char *datain, *dataout;
	__capability char *datain_cap, *dataout_cap;
	int socket_pair[2], pipe_pair[2], shmem_socket_pair[2];
	int arg;
	uint reps = 100;
	pid_t child_pid;
	size_t size, max_size = 0;
	char *endp;
	int ch;
	int func = 0, invoke = 0, socket = 0, do_pipe=0, shared = 0;
	int inOffset = 0, outOffset = 0;
	uint64_t *samples;
	
	// use unbuffered output to avoid dropped characters on uart
	setbuf(stdout, NULL);

	while ((ch = getopt(argc, argv, "fipsSr:o:O:")) != -1) {
	  switch (ch) {
	  case 'f':
	    func = 1;
	    break;
	  case 'i':
	    invoke = 1;
	    break;
	  case 'p':
	    do_pipe = 1;
	    break;
	  case 's':
	    socket = 1;
	    break;
	  case 'S':
	    shared = 1;
	    break;
	  case 'r':
	    reps = strtol(optarg, &endp, 0);
	    if (*endp != '\0')
		printf("Invalid rep count: %s\n", optarg);
	    break;
	  case 'o':
	    inOffset = strtol(optarg, &endp, 0);
	    if (*endp != '\0')
		printf("Invalid offset: %s\n", optarg);
	    break;
	  case 'O':
	    outOffset = strtol(optarg, &endp, 0);
	    if (*endp != '\0')
		printf("Invalid offset: %s\n", optarg);
	    break;	    
	  case '?':
	  default:
	    usage();
	  }
	}
	argc -= optind;
	argv += optind;

	if (argc <= 0)
	  {
	    printf("must give at least one size.\n");
	    usage();
	  }

	for(arg = 0; arg < argc; arg++)
	  {
	    size = strtol(argv[arg], &endp, 0);
	    if (*endp != '\0')
		printf("Invalid argument: %s\n", argv[arg]);
	    max_size = size > max_size ? size : max_size;
	  }
	
	datain  = mmap(NULL, max_size + inOffset, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);
	dataout = mmap(NULL, max_size + outOffset, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);
	if (datain == NULL || dataout == NULL) err(1, "malloc");
	datain_cap  = __builtin_cheri_set_cap_length((__capability char *) (datain + inOffset), max_size);
	dataout_cap = __builtin_cheri_set_cap_length((__capability char *) (dataout + outOffset), max_size);

	if (invoke)
	  {
	    if (sandbox_class_new("/usr/libexec/cheri_bench-helper",
				  4*1024*1024, &classp) < 0)
	      err(EX_OSFILE, "sandbox_class_new");
	    if (sandbox_object_new(classp, 2*1024*1024, &sandboxp) < 0)
	      err(EX_OSFILE, "sandbox_object_new");

	    /*
	     * Ideally, this information would be sucked out of ELF.
	     */
	    (void)sandbox_class_method_declare(classp,
	       CHERI_BENCH_HELPER_OP_MEMCPY, "memcpy");
	    objectp = sandbox_object_getobject(sandboxp);
	  }

	if (socket)
	  {
	    if (socketpair(PF_LOCAL, SOCK_STREAM, 0, socket_pair) < 0)
	      err(1, NULL);
	    child_pid = fork();
	    if (child_pid < 0)
	      err(1, "fork socket");
	    if (!child_pid)
	      {
		close(socket_pair[0]);
		socket_sandbox_func(socket_pair[1], max_size);
	      }
	    close(socket_pair[1]);
	  }

	if (do_pipe)
	  {
	    if (pipe(pipe_pair))
	      err(1, NULL);
	    child_pid = fork();
	    if (child_pid < 0)
	      err(1, "fork pipe");
	    if (!child_pid)
	      {
		close(pipe_pair[0]);
		socket_sandbox_func(pipe_pair[1], max_size);
	      }
	    close(pipe_pair[1]);
	  }
	
	if (shared)
	  {
	    if (pipe(shmem_socket_pair) < 0)
	      err(1, NULL);
	    child_pid = fork();
	    if (child_pid < 0)
	      err(1, "fork shared");
	    // XXX remap regions read/write only?
	    if (!child_pid)
	      {
		close(shmem_socket_pair[0]);
		shmem_sandbox_func(shmem_socket_pair[1], datain_cap, dataout_cap);
	      }
	    close(shmem_socket_pair[1]);
	  }

	printf("#reps=%u inOffset=%u outOffset=%u datain=%p dataout=%p\n", reps, inOffset, outOffset, (void *) (datain + inOffset), (void*) (dataout + outOffset));

	samples = malloc(sizeof(uint64_t) * reps);
	if (samples == NULL)
	  err(1, "malloc samples");
	for(arg = 0; arg < argc; arg++)
	  {
	    size = strtol(argv[arg], &endp, 0);
	    if(func)
	      {
		printf("\n#func %zu\n%zu", size, size);
		benchmark(do_memcpy, dataout_cap, datain_cap, size, reps, samples, 0);
	      }
	    if (invoke)
	      {
		printf("\n#invoke %zu\n%zu", size, size);
		benchmark(invoke_memcpy, dataout_cap, datain_cap, size, reps, samples, 0);
	      }
	    if(shared)
	      {
		printf("\n#shmem %zu\n%zu", size, size);
		benchmark(shmem_memcpy, dataout_cap, datain_cap, size, reps, samples, shmem_socket_pair[0]);
	      }
	    if(socket)
	      {
		printf("\n#socket %zu\n%zu", size, size);
		benchmark(socket_memcpy, dataout_cap, datain_cap, size, reps, samples, socket_pair[0]);
	      }
	    if(do_pipe)
	      {
		printf("\n#pipe %zu\n%zu", size, size);
		benchmark(socket_memcpy, dataout_cap, datain_cap, size, reps, samples, pipe_pair[0]);
	      }
	  }
	if (samples != NULL)
	  free(samples);
	putchar('\n');
	
	munmap(datain, max_size + inOffset);
	munmap(dataout, max_size + outOffset);

	if (invoke)
	  {
	    sandbox_object_destroy(sandboxp);
	    sandbox_class_destroy(classp);
	  }

	return (0);
}
