/*-
 * Copyright (c) 2014-2016 Robert N. M. Watson
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
#include <sys/param.h>
#include <sys/cpuset.h>
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
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/wait.h>

#if __has_feature(capabilities)

#include <cheri/cheri.h>
#include <cheri/cheric.h>
#include <cheri/cheri_fd.h>
#include <cheri/sandbox.h>
#include <cheri/cheri_invoke.h>
#include <cheri_bench-helper.h>

#define CAP
struct cheri_object cheri_bench;

#define DEFINE_RDHWR_COUNTER_GETTER(name,regno)      \
  static inline int32_t get_##name##_count (void) \
  {					       \
  int32_t ret;				       \
  __asm __volatile("rdhwr %0, $"#regno : "=r" (ret)); \
  return ret;						\
  }


#else

#define __capability
#define memcpy_c memcpy
#define cheri_ptr(c,l) c

// Manually assembled due to gcc/gas refusing to recognise custom rdhwr registers:
// rdhwr $12, $rdhwrreg
// move  $ret, $12
// Move is needed because we can't get a raw register number for ret in the assembler
// template without it being prefixed by $. Note that $12 == $t0.
#define DEFINE_RDHWR_COUNTER_GETTER(name,regno)				\
  static inline int32_t get_##name##_count (void)			\
  {									\
    int32_t ret;							\
    __asm __volatile(".word (0x1f << 26) | (12 << 16) | (" #regno  " << 11) | 0x3b\n\tmove %0,$12" : "=r" (ret) :: "$12"); \
    return ret;								\
  }

#endif

#ifndef CHERI_START_TRACE
#define CHERI_START_TRACE
#endif
#ifndef CHERI_STOP_TRACE
#define CHERI_STOP_TRACE
#endif

static useconds_t console_usleep = 100000;

DEFINE_RDHWR_COUNTER_GETTER(cycle,2)
DEFINE_RDHWR_COUNTER_GETTER(inst,4)
DEFINE_RDHWR_COUNTER_GETTER(tlb_inst,5)
DEFINE_RDHWR_COUNTER_GETTER(tlb_data,6)

typedef void memcpy_t(__capability char *, __capability char *, size_t, void *data);

struct semaphore_shared_data {
  sem_t sem_request;
  sem_t sem_response;
  __capability char * volatile datain;
  __capability char * volatile dataout;
  volatile size_t len;
  int core;
};

struct counters {
  // NB we use signed 32-bit because that is what rdhwr returns (annoyingly) and casting
  // causes compiler to emit a lot of extra code which can be avoided if we just keep
  // everything int32 then cast to uint32 just before displaying. Roll-over is fine
  // so long as test is shorter than 2*32 cycles (42 seconds @ 100MHz).
  int32_t insts;
  int32_t cycles;
  int32_t instTLB;
  int32_t dataTLB;
};

/*
 * Set the affinity of current thread to given cpu
 */
static void set_my_affinity(int cpunum)
{
  cpuset_t cpu_mask;
  CPU_ZERO(&cpu_mask);
  CPU_SET(cpunum, &cpu_mask);
  if(cpuset_setaffinity(CPU_LEVEL_WHICH ,CPU_WHICH_TID, (id_t) -1, sizeof(cpu_mask), &cpu_mask))
    err(1, "set affinity cpunum=%d", cpunum);

}

static void do_memcpy (__capability char *dataout, __capability char *datain, size_t len, void * __unused data)
{
  memcpy_c(dataout, datain, len);
}

#ifdef CAP
static void invoke_memcpy(__capability char *dataout, __capability char *datain, size_t len, void * __unused data)
{
  int ret;
  ret = cheri_bench_memcpy_cap(
			cheri_bench,
		     (__capability void *) dataout,
		     (__capability void *) datain,
		     len);
  if (ret != 0) err(1, "Invoke failed.");
}
#endif

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

static void socket_memcpy(__capability char *dataout, __capability char *datain, size_t len, void *data)
{
  int fd = *((int *) data);
  ssize_t  io_len;

  struct iovec iovs[2];
  iovs[0].iov_base = &len;
  iovs[0].iov_len  = sizeof(len);
  iovs[1].iov_base = (void *) datain;
  iovs[1].iov_len  = len;
  
  io_len = writev(fd, iovs, 2);
  if(io_len != (ssize_t)(sizeof(len) + len))
    err(1, "socket parent write");

  read_retry(fd, (char *) dataout, len);
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

static void shmem_memcpy(__capability char *dataout __unused, __capability char *datain __unused, size_t len, void *data)
{
  ssize_t  io_len;
  int fd = *((int *) data);
  io_len = write(fd, &len, sizeof(len));
  assert(io_len == sizeof(len)); // XXX rmn30 lazy
  io_len = read(fd, &len, sizeof(len));
  assert(io_len == sizeof(len));
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

static void semaphore_memcpy(__capability char *dataout, __capability char *datain, size_t len, void *data)
{
  struct semaphore_shared_data *sdata = (struct semaphore_shared_data *) data;
  sdata->dataout = dataout;
  sdata->datain  = datain;
  sdata->len     = len;
  // XXX rmn30 need sync?
  sem_post(&(sdata->sem_request));
  sem_wait(&(sdata->sem_response));
}

static void *semaphore_sandbox_func(void *arg)
{
  struct semaphore_shared_data *data = (struct semaphore_shared_data *) arg;
  set_my_affinity(data->core);
  while(1)
    {
      sem_wait(&(data->sem_request));
      memcpy_c(data->dataout, data->datain, data->len);
      sem_post(&(data->sem_response));
    }
}

int benchmark(memcpy_t *memcpy_func, __capability char *dataout, __capability char *datain, size_t size, const char* name, uint reps, struct counters *samples, void *data) __attribute__((__noinline__));
int benchmark(memcpy_t *memcpy_func, __capability char *dataout, __capability char *datain, size_t size, const char* name, uint reps, struct counters *samples, void *data)
{
      int32_t cycles, cycles2, insts, insts2, instTLB, instTLB2, dataTLB, dataTLB2;
      // Initialise arrays
      for (uint i=0; i < size; i++) 
	{
	  datain[i]  = (char) i;
	  dataout[i] = 0;
	}

      for (uint rep = 0; rep < reps; rep++) 
	{
	  cycles  = get_cycle_count();
	  insts   = get_inst_count();
	  instTLB = get_tlb_inst_count();
	  dataTLB = get_tlb_data_count();
	  CHERI_START_TRACE;
	  memcpy_func(dataout, datain, size, data);
	  CHERI_STOP_TRACE;
	  cycles2  = get_cycle_count();
	  insts2   = get_inst_count();
	  instTLB2 = get_tlb_inst_count();
	  dataTLB2 = get_tlb_data_count();
	  samples[rep].cycles = cycles2 - cycles;
	  samples[rep].insts  = insts2 - insts;
	  samples[rep].instTLB = instTLB2 - instTLB;
	  samples[rep].dataTLB = dataTLB2 - dataTLB;
	  for (uint i=0; i < size; i+=8)
	    {
	      assert(dataout[i] == (char) i);
	      //datain[i]  = (char) i;
	      //dataout[i] = 0;
	    }
	}

#define flushit() do { fflush(stdout); usleep(console_usleep); } while (0)
#define dump_metric(metric) \
      do {							\
	flushit();						\
	printf("\n%zu,%s,"#metric, size, name);			\
	for (uint rep = 0; rep < reps; rep++) {			\
	  if ((rep & 0x7) == 0) flushit();			\
	  printf(",%u", (uint32_t) samples[rep].metric);	\
	}							\
	flushit();						\
      } while(0)
      dump_metric(cycles);
      dump_metric(insts);
      dump_metric(instTLB);
      dump_metric(dataTLB);
      return 0;
}

static void usage(void)
{
  errx(1,  "usage: cheri_bench [-afipsSt] -r <reps> -o <in offset> -O <out offset> <size>...\n");
}

static void reap(pid_t pid)
{
  if ((pid != 0) && (kill(pid, SIGKILL) < 0))
    err(1, "failed to reap pid=%d", pid);
}
  

int
main(int argc, char *argv[])
{
#ifdef CAP
	struct sandbox_class *classp;
	struct sandbox_object *sandboxp;
#endif
	char *datain, *dataout;
	__capability char *datain_cap, *dataout_cap;
	int socket_pair[2], pipe_pair[2], shmem_socket_pair[2];
	int arg;
	uint rep, reps = 100;
	pid_t socket_child = 0, pipe_child = 0, shared_child = 0, mutex_child = 0;
	size_t size, max_size = 0;
	char *endp;
	int ch;
	int func = 0, invoke = 0, do_socket = 0, do_pipe=0, shared = 0, threads = 0, mutex = 0;
	int inOffset = 0, outOffset = 0;
	struct counters *samples;
	pthread_t sb_thread;
	struct semaphore_shared_data *mutex_shared;
	struct semaphore_shared_data *pthread_shared;
	int core = 0; // core affinity for sandbox thread
	// use unbuffered output to avoid dropped characters on uart
	setbuf(stdout, NULL);

	while ((ch = getopt(argc, argv, "afipsStmr:o:O:c:u:")) != -1) {
	  switch (ch) {
	  case 'a':
	    func = 1;
	    invoke = 1;
	    do_pipe = 1;
	    do_socket = 1;
	    shared = 1;
	    threads = 1;
	    mutex = 1;
	    break;
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
	    do_socket = 1;
	    break;
	  case 'S':
	    shared = 1;
	    break;
	  case 't':
	    threads = 1;
	    break;
	  case 'm':
	    mutex = 1;
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
	  case 'c':
	    core = (int)strtol(optarg, &endp, 0);
	    if (*endp != '\0')
	      printf("Invalid core: %s\n", optarg);
	    break;
	  case 'u':
	    console_usleep = (useconds_t)strtol(optarg, &endp, 0);
	    if (*endp != '\0')
	      printf("Invalid console_usleep: %s\n", optarg);
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
	// parent always runs on core 0 (XXX rdhwr counters not enabled on APs)
	set_my_affinity(0); 
	datain  = mmap(NULL, max_size + inOffset, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);
	dataout = mmap(NULL, max_size + outOffset, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);
	if (datain == NULL || dataout == NULL) err(1, "malloc");

	datain_cap  = cheri_ptr(datain + inOffset, max_size);
	dataout_cap = cheri_ptr(dataout + outOffset, max_size);


#ifdef CAP
	if (invoke)
	  {
	    if (sandbox_class_new("/usr/libexec/cheri_bench-helper",
				  4*1024*1024, &classp) < 0)
	      err(EX_OSFILE, "sandbox_class_new");
	    if (sandbox_object_new(classp, 2*1024*1024, &sandboxp) < 0)
	      err(EX_OSFILE, "sandbox_object_new");
	    cheri_bench = sandbox_object_getobject(sandboxp);
	  }
#endif

	if (do_socket)
	  {
	    if (socketpair(PF_LOCAL, SOCK_STREAM, 0, socket_pair) < 0)
	      err(1, NULL);
	    socket_child = fork();
	    if (socket_child < 0)
	      err(1, "fork socket");
	    if (!socket_child)
	      {
  		set_my_affinity(core);
		close(socket_pair[0]);
		socket_sandbox_func(socket_pair[1], max_size);
	      }
	    close(socket_pair[1]);
	  }

	if (do_pipe)
	  {
	    if (pipe(pipe_pair))
	      err(1, NULL);
	    pipe_child = fork();
	    if (pipe_child < 0)
	      err(1, "fork pipe");
	    if (!pipe_child)
	      {
  		set_my_affinity(core);
		close(pipe_pair[0]);
		socket_sandbox_func(pipe_pair[1], max_size);
	      }
	    close(pipe_pair[1]);
	  }
	
	if (shared)
	  {
	    if (pipe(shmem_socket_pair) < 0)
	      err(1, NULL);
	    shared_child = fork();
	    if (shared_child < 0)
	      err(1, "fork shared");
	    // XXX remap regions read/write only?
	    if (!shared_child)
	      {
    		set_my_affinity(core);
		close(shmem_socket_pair[0]);
		shmem_sandbox_func(shmem_socket_pair[1], datain_cap, dataout_cap);
	      }
	    close(shmem_socket_pair[1]);
	  }

	if (threads)
	  {
	    pthread_shared = malloc(sizeof(*pthread_shared));
	    pthread_shared->core = core;
	    if (pthread_shared == NULL)
	      err(1, "malloc pthread shared");
	    if(sem_init(&pthread_shared->sem_request, 0, 0))
	      err(1, "sem_init sem_request");
	    if(sem_init(&pthread_shared->sem_response, 0, 0))
	      err(1, "sem_init sem_request");
	    if(pthread_create(&sb_thread, NULL, semaphore_sandbox_func, pthread_shared))
	      err(1, "pthread create");
	  }

	if (mutex)
	  {
	    mutex_shared = mmap(NULL, sizeof(*mutex_shared), PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);
	    mutex_shared->core = core;
	    if (mutex_shared == NULL)
	      err(1, "mutex alloc shared");
	    if(sem_init(&(mutex_shared->sem_request), 1, 0))
	      err(1, "sem_init mutex sem_request");
	    if(sem_init(&(mutex_shared->sem_response), 1, 0))
	      err(1, "sem_init mutex sem_response");
	    mutex_child = fork();
	    if (mutex_child < 0)
	      err(1, "fork mutex");
	    // XXX remap regions read/write only?
	    if (!mutex_child)
	      {
		semaphore_sandbox_func(mutex_shared);
	      }
	  }

	printf("#reps=%u inOffset=%u outOffset=%u datain=%p dataout=%p\n", reps, inOffset, outOffset, (void *) (datain + inOffset), (void*) (dataout + outOffset));
	printf("size,method,metric");
	for(rep=0;rep<reps;rep++)
	  {
	    printf(",r%d",rep);
	  }
	samples = malloc(sizeof(struct counters) * reps);
	if (samples == NULL)
	  err(1, "malloc samples");
	for(arg = 0; arg < argc; arg++)
	  {
	    size = strtol(argv[arg], &endp, 0);
	    if(do_socket)
	      {
		benchmark(socket_memcpy, dataout_cap, datain_cap, size,  "0-socket", reps, samples, & socket_pair[0]);
	      }
	    if(do_pipe)
	      {
		benchmark(socket_memcpy, dataout_cap, datain_cap, size, "1-pipe", reps, samples, & pipe_pair[0]);
	      }
	    if(shared)
	      {
		benchmark(shmem_memcpy, dataout_cap, datain_cap, size, "2-shmem+pipe", reps, samples, & shmem_socket_pair[0]);
	      }
	    if(mutex)
	      {
		benchmark(semaphore_memcpy, dataout_cap, datain_cap, size, "3-shmem+sem", reps, samples, mutex_shared);
	      }
	    if(threads)
	      {
		benchmark(semaphore_memcpy, dataout_cap, datain_cap, size, "4-pthread+sem", reps, samples, pthread_shared);
	      }
#ifdef CAP
	    if (invoke)
	      {
		benchmark(invoke_memcpy, dataout_cap, datain_cap, size, "5-invoke", reps, samples, 0);
	      }
#endif
	    if(func)
	      {
		benchmark(do_memcpy, dataout_cap, datain_cap, size, "6-func", reps, samples, 0);
	      }
	  }
	if (samples != NULL)
	  free(samples);
	putchar('\n');

	reap(socket_child);
	reap(pipe_child);
	reap(shared_child);
	reap(mutex_child);

	munmap(datain, max_size + inOffset);
	munmap(dataout, max_size + outOffset);

#ifdef CAP
	if (invoke)
	  {
	    sandbox_object_destroy(sandboxp);
	    sandbox_class_destroy(classp);
	  }
#endif

	return (0);
}
