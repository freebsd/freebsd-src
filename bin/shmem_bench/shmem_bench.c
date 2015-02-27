/*-
 * Copyright (c) 2014 Robert N. M. Watson
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

static inline uint32_t
get_cyclecount(void)
{
#if 0
  //uint64_t _time;
  //__asm __volatile("rdhwr %0, $2" : "=r" (_time));
  //__asm __volatile(".word 0x7c10103b\n\tmove %0, $16" : "=r" (_time) :: "$16"); // rdhwr $16, $2 manually assembled due to gas issuesa
  //return (_time & 0xffffffff);
#endif
  __asm __volatile("dsra $zero, $1, $2"); // we can't do rdhwr on beri kernels because it is not enabled, so use this as a placeholder
  return 0;
}

static int fd_socket_pair[2];
static int shmem_socket_pair[2];
static  char *shmin, *shmout;
//static uint   offset = 0;
static size_t max_size = 0;

typedef uint64_t memcpy_t( char *,  char *, size_t);

static uint64_t do_memcpy ( char *dataout,  char *datain, size_t len)
{
  uint32_t start_count, end_count;
  start_count = get_cyclecount();
  memcpy(dataout, datain, len);
  end_count = get_cyclecount();
  return end_count - start_count;
}

static uint64_t socket_memcpy( char *dataout,  char *datain, size_t len)
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


static void socket_sandbox_func(int fd)
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

static uint64_t shmem_memcpy( char *dataout __unused,  char *datain __unused, size_t len)
{
  ssize_t  io_len;
  uint32_t start_count = get_cyclecount();
  io_len = send(shmem_socket_pair[0], &len, sizeof(len), MSG_WAITALL);
  assert(io_len == sizeof(len)); // XXX rmn30 lazy
  io_len = recv(shmem_socket_pair[0], &len, sizeof(len), MSG_WAITALL);
  assert(io_len == sizeof(len));
  return get_cyclecount() - start_count;
}

static void shmem_sandbox_func(int fd)
{
  size_t len;
  ssize_t io_len;
  while(1)
    {
      io_len = recv(fd, &len, sizeof(len), MSG_WAITALL);
      if (io_len != sizeof(len))
        exit(0); // XXX don't complain when parent dies err(1, "shm child recv");
      memcpy(shmout, shmin, len);
      io_len = send(fd, &len, sizeof(len), MSG_WAITALL);
      if (io_len != sizeof(len))
        err(1, "shm child send");
    }
}

int benchmark(memcpy_t *memcpy_func,  char *dataout,  char *datain, size_t size, uint reps);
int benchmark(memcpy_t *memcpy_func,  char *dataout,  char *datain, size_t size, uint reps)
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
#define ARGS 3
static void usage(void)
{
  errx(1,  "usage: cheri_bench [-fps] -t <trials> -r <reps> -o <in offset> -O <out offset> <size>...\n");
}

int
main(int argc, char *argv[])
{
	 char *datain;
	 char *dataout;
	int arg;
	long trials = 100;
	uint reps = 100;
	pid_t child_pid;
	size_t size;
	char *endp;
	char *shmem_p;
	int ch;
	int func = 0, socket_p = 0, shared = 0;
	int inOffset = 0, outOffset = 0;

	// use unbuffered output to avoid dropped characters on uart
	setbuf(stdout, NULL);

	while ((ch = getopt(argc, argv, "fipst:r:o:O:")) != -1) {
	  switch (ch) {
	  case 'f':
	    func = 1;
	    break;
	  case 'p':
	    socket_p = 1;
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
	if (socket_p)
	  {
	    if (socketpair(PF_LOCAL, SOCK_STREAM, 0, fd_socket_pair) < 0)
	      err(1, NULL);
	    child_pid = fork();
	    if (child_pid < 0)
	      err(1, "fork socket");
	    if (!child_pid)
	      {
		close(fd_socket_pair[0]);
		socket_sandbox_func(fd_socket_pair[1]);
	      }
	    close(fd_socket_pair[1]);
	  }

	if (shared)
	  {
	    shmem_p = mmap(NULL, max_size + inOffset, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0); // rmn30 XXX attach read only in child?
	    if (shmem_p == MAP_FAILED)
	      err(1, "mmap in");
	    shmin = ( void*) (shmem_p + inOffset);
	    shmem_p = mmap(NULL, max_size+outOffset, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0); // rmn30 XXX attach read only in parent?
	    if (shmem_p == MAP_FAILED)
	      err(1, "mmap out");
	    shmout = ( void*) (shmem_p + outOffset);
	    if (socketpair(PF_LOCAL, SOCK_STREAM, 0, shmem_socket_pair) < 0)
	      err(1, NULL);
	    child_pid = fork();
	    if (child_pid < 0)
	      err(1, "fork shared");
	    if (!child_pid)
	      {
		close(shmem_socket_pair[0]);
		shmem_sandbox_func(shmem_socket_pair[1]);
	      }
	    close(shmem_socket_pair[1]);
	  }

	datain  = (( char*) malloc(max_size + inOffset) + inOffset);
	dataout = (( char*) malloc(max_size + outOffset) + outOffset);
	if (datain == NULL || dataout == NULL) err(1, "malloc");

	printf("#trials=%lu reps=%u inOffset=%u outOffset=%u datain=%p dataout=%p\n", trials, reps, inOffset, outOffset, (void *) datain, (void*) dataout);

	for(arg = 0; arg < argc; arg++)
	  {

	    size = strtol(argv[arg], &endp, 0);
	    if(func)
	      {
		printf("\n#func %zu\n%zu,%u", size, size,reps);
		for (uint rep = 0; rep < trials; rep++)
		  {
		    benchmark(do_memcpy, dataout, datain, size, reps);
		  }
	      }
	    if(shared)
	      {
		printf("\n#shmem %zu\n%zu,%u", size, size, reps);
		for (uint rep = 0; rep < trials; rep++)
		  {
		    benchmark(shmem_memcpy, ( char*)shmout, ( char*)shmin, size, reps);
		  }
	      }
	    if(socket_p)
	      {
		printf("\n#socket %zu\n%zu,%u", size, size, reps);
		for (uint rep = 0; rep < trials; rep++)
		  {
		    benchmark(socket_memcpy, dataout, datain, size, reps);
		  }
	      }
	  }
	putchar('\n');
	//free((void *)datain);
	//free((void *)dataout);
	return (0);
}
