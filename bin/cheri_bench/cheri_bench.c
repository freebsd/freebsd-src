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

#include <machine/cheri.h>
#include <machine/cheric.h>
#include <cheri/cheri_fd.h>
#include <cheri/sandbox.h>
#include <cheri_bench-helper.h>

#define get_cyclecount cheri_get_cyclecount

static struct sandbox_object *objectp;
static int fd_socket_pair[2];
static int shmem_socket_pair[2];

typedef uint64_t memcpy_t(__capability char *, __capability char *, size_t);

static uint64_t do_memcpy (__capability char *dataout, __capability char *datain, size_t len)
{
  uint32_t start_count, end_count;
  start_count = get_cyclecount();
  memcpy_c(dataout, datain, len);
  end_count = get_cyclecount();
  return end_count - start_count;
}

static uint64_t invoke_memcpy(__capability char *dataout, __capability char *datain, size_t len)
{
  int ret;
  uint32_t start_count, end_count;
  start_count = get_cyclecount();
  ret = sandbox_object_cinvoke(objectp,
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


static uint64_t socket_memcpy(__capability char *dataout, __capability char *datain, size_t len)
{
  ssize_t  io_len;
  uint32_t start_count = get_cyclecount();

  io_len = send(fd_socket_pair[0], &len, sizeof(len), 0);
  if(io_len != sizeof(len))
    err(1, "socket parent send len");

  io_len = send(fd_socket_pair[0], (char *) datain, len, 0);
  if(io_len != (ssize_t) len)
    err(1, "socket parent send data");
  io_len = recv(fd_socket_pair[0], (char *) dataout, len, MSG_WAITALL);
  if (io_len != (ssize_t) len)
    err(1, "parent receive");
  return get_cyclecount() - start_count;
}


static void socket_sandbox_func(int fd, size_t max_size)
{
  char*   buf = malloc(max_size);
  ssize_t io_len;
  ssize_t msg_len;
  while(1)
    {
      /* Read length of message*/
      io_len = recv(fd, &msg_len, sizeof(msg_len), 0);
      if (io_len != sizeof(msg_len))
	exit(0); // XXX don't complain when parent dies err(1, "socket recv child len");
      /* read message from stream */
      io_len = recv(fd, buf, msg_len, MSG_WAITALL);
      if (io_len != msg_len)
	err(1, "socket recv child msg");
      /* echo message back */
      io_len = send(fd, buf, msg_len, 0);
      if (io_len != msg_len)
	err(1, "socket send child msg");
    }
}

static uint64_t shmem_memcpy(__capability char *dataout __unused, __capability char *datain __unused, size_t len)
{
  ssize_t  io_len;
  uint32_t start_count = get_cyclecount();
  io_len = send(shmem_socket_pair[0], &len, sizeof(len), MSG_WAITALL);
  assert(io_len == sizeof(len)); // XXX rmn30 lazy
  io_len = recv(shmem_socket_pair[0], &len, sizeof(len), MSG_WAITALL);
  assert(io_len == sizeof(len));
  return get_cyclecount() - start_count;
}

static void shmem_sandbox_func(int fd, __capability char *datain, __capability char *dataout)
{
  size_t len;
  ssize_t io_len;
  while(1)
    {
      io_len = recv(fd, &len, sizeof(len), MSG_WAITALL);
      if (io_len != sizeof(len))
        exit(0); // XXX don't complain when parent dies err(1, "shm child recv");
      memcpy_c(dataout, datain, len);
      io_len = send(fd, &len, sizeof(len), MSG_WAITALL);
      if (io_len != sizeof(len))
        err(1, "shm child send");
    }
}

int benchmark(memcpy_t *memcpy_func, __capability char *dataout, __capability char *datain, size_t size, uint reps) __attribute__((__noinline__))
{
      uint64_t total_cycles = 0;

      // Initialise arrays
      for (uint i=0; i < size; i++) 
	{
	  datain[i]  = (char) i;
	  dataout[i] = 0;
	}

      reps += 1;      
      for (uint rep = 0; rep < reps; rep++) 
	{
	  total_cycles += memcpy_func(dataout, datain, size);
	  if (rep == 0)
	    total_cycles = 0; // throw away the first rep because of startup costs.
	  for (uint i=0; i < size; i++)
	    {
	      assert(dataout[i] == (char) i);
	      datain[i]  = (char) i;
	      dataout[i] = 0;
	    }
	}

      printf(",%lu", total_cycles);
  return 0;
}

#define USAGE
static void usage(void)
{
  errx(1,  "usage: cheri_bench [-fips] -t <trials> -r <reps> -o <in offset> -O <out offset> <size>...\n");
}

int
main(int argc, char *argv[])
{
	struct sandbox_class *classp;
	char *datain, *dataout;
	__capability char *datain_cap, *dataout_cap;
	int arg;
	long trials = 100;
	uint reps = 100;
	pid_t child_pid;
	size_t size, max_size = 0;
	char *endp;
	void * shmem_p;
	int ch;
	int func = 0, invoke = 0, socket = 0, shared = 0;
	int inOffset = 0, outOffset = 0;

	// use unbuffered output to avoid dropped characters on uart
	setbuf(stdout, NULL);

	while ((ch = getopt(argc, argv, "fipst:r:o:O:")) != -1) {
	  switch (ch) {
	  case 'f':
	    func = 1;
	    break;
	  case 'i':
	    invoke = 1;
	    break;
	  case 'p':
	    socket = 1;
	    break;
	  case 's':
	    shared = 1;
	    break;
	  case 'r':
	    reps = strtol(optarg, &endp, 0);
	    if (*endp != '\0')
		printf("Invalid rep count: %s\n", optarg);
	    break;
	  case 't':
	    trials = strtol(optarg, &endp, 0);
	    if (*endp != '\0')
		printf("Invalid trial count: %s\n", optarg);
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
	    if (sandbox_object_new(classp, &objectp) < 0)
	      err(EX_OSFILE, "sandbox_object_new");

	    /*
	     * Ideally, this information would be sucked out of ELF.
	     */
	    (void)sandbox_class_method_declare(classp,
	       CHERI_BENCH_HELPER_OP_MEMCPY, "memcpy");
	  }

	if (socket)
	  {
	    if (socketpair(PF_LOCAL, SOCK_STREAM, 0, fd_socket_pair) < 0)
	      err(1, NULL);
	    child_pid = fork();
	    if (child_pid < 0)
	      err(1, "fork socket");
	    if (!child_pid)
	      {
		close(fd_socket_pair[0]);
		socket_sandbox_func(fd_socket_pair[1], max_size);
	      }
	    close(fd_socket_pair[1]);
	  }

	if (shared)
	  {
	    if (socketpair(PF_LOCAL, SOCK_STREAM, 0, shmem_socket_pair) < 0)
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

	printf("#trials=%lu reps=%u inOffset=%u outOffset=%u datain=%p dataout=%p\n", trials, reps, inOffset, outOffset, (void *) (datain + inOffset), (void*) (dataout + outOffset));

	//for (uint s = 0; s < sizeof(sizes)/sizeof(sizes[0]); s++)
	//  {
	for(arg = 0; arg < argc; arg++)
	  {
	    size = strtol(argv[arg], &endp, 0);
	    if(func)
	      {
		printf("\n#func %zu\n%zu,%u", size, size,reps);
		for (uint rep = 0; rep < trials; rep++)
		  {
		    benchmark(do_memcpy, dataout_cap, datain_cap, size, reps);
		  }
	      }
	    if (invoke)
	      {
		printf("\n#invoke %zu\n%zu,%u", size, size, reps);
		for (uint rep = 0; rep < trials; rep++)
		  {
		    benchmark(invoke_memcpy, dataout_cap, datain_cap, size, reps);
		  }
	      }
	    if(shared)
	      {
		printf("\n#shmem %zu\n%zu,%u", size, size, reps);
		for (uint rep = 0; rep < trials; rep++)
		  {
		    benchmark(shmem_memcpy, dataout_cap, datain_cap, size, reps);
		  }
	      }
	    if(socket)
	      {
		printf("\n#socket %zu\n%zu,%u", size, size, reps);
		for (uint rep = 0; rep < trials; rep++)
		  {
		    benchmark(socket_memcpy, dataout_cap, datain_cap, size, reps);
		  }
	      }
	  }
	putchar('\n');
	
	munmap(datain, max_size + inOffset);
	munmap(dataout, max_size + outOffset);

	if (invoke)
	  {
	    sandbox_object_destroy(objectp);
	    sandbox_class_destroy(classp);
	  }

	return (0);
}
