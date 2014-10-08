/*-
 * Copyright (c) 2014 Robert N. M. Watson
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

#if !__has_feature(capabilities)
#error "This code requires a CHERI-aware compiler"
#endif

#include <sys/types.h>

#include <machine/cheri.h>
#include <machine/cheric.h>

#include <cheri/cheri_fd.h>
#include <cheri/sandbox.h>

#include <cheri_helloworld-helper.h>
#include <err.h>
#include <stdio.h>
#include <sysexits.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

static struct sandbox_object *objectp;

#define REPS         100
#define REPREPS        20

typedef uint64_t memcpy_t(__capability char *, __capability char *, size_t);
int benchmark(memcpy_t *,  __capability char *, __capability char *, size_t);

uint64_t do_memcpy (__capability char *dataout, __capability char *datain, size_t len)
{
  uint64_t start_count, end_count;
  start_count = cheri_get_cyclecount();
  memcpy_c(dataout, datain, len);
  end_count = cheri_get_cyclecount();
  return end_count - start_count;
}

uint64_t invoke_memcpy(__capability char *dataout, __capability char *datain, size_t len)
{
  int ret;
  uint64_t start_count, end_count;
  start_count = cheri_get_cyclecount();
  ret = sandbox_object_cinvoke(objectp, CHERI_HELLOWORLD_HELPER_OP_MEMCPY, len, 0, 0, 0, 0, 0, 0,
			     (__capability void *) dataout,
			     (__capability void *) datain,
			     cheri_zerocap(),
			     cheri_zerocap(),
			     cheri_zerocap(),
			     cheri_zerocap(),
			     cheri_zerocap(),
			     cheri_zerocap());
  end_count = cheri_get_cyclecount();
  assert(ret == 0);
  return end_count - start_count;
}

int benchmark(memcpy_t *memcpy_func, __capability char *dataout, __capability char *datain, size_t size)
{
      uint64_t total_cycles = 0;

      // Initialise arrays
      for (uint i=0; i < size; i++) 
	{
	  datain[i]  = (char) i;
	  dataout[i] = 0;
	}
      
      for (uint rep = 0; rep < REPS; rep++) 
	{

	  total_cycles += memcpy_func(dataout, datain, size);
	  for (uint i=0; i < size; i++)
	    {
	      assert(dataout[i] == (char) i);
	      dataout[i] = 0;
	    }
	}

      printf("%lu,", total_cycles);
  return 0;
}

int
main(int argc __unused, char *argv[] __unused)
{
	struct sandbox_class *classp;
	__capability char *datain;
	__capability char *dataout;
	size_t sizes[] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000};

	if (sandbox_class_new("/usr/libexec/cheri_helloworld-helper.bin",
	    4*1024*1024, &classp) < 0)
		err(EX_OSFILE, "sandbox_class_new");
	if (sandbox_object_new(classp, &objectp) < 0)
		err(EX_OSFILE, "sandbox_object_new");


	/*
	 * Ideally, this information would be sucked out of ELF.
	 */
	(void)sandbox_class_method_declare(classp,
	    CHERI_HELLOWORLD_HELPER_OP_MEMCPY, "memcpy");

	for (uint s = 0; s < sizeof(sizes)/sizeof(sizes[0]); s++)
	  {
	    size_t size = sizes[s];
	    datain  = (__capability char*) malloc(size);
	    assert(datain != NULL);
	    dataout = (__capability char*) malloc(size);
	    assert(dataout != NULL);
	    printf("func %zu,", size);
	    for (uint rep = 0; rep < REPREPS; rep++)
	      {
		benchmark(do_memcpy, dataout, datain, s);
	      }
	    printf("\ninvoke %zu,", size);
	    for (uint rep = 0; rep < REPREPS; rep++)
	      {
		benchmark(invoke_memcpy, dataout, datain, s);
	      }
	    printf("\n");

	    free((void *)datain);
	    free((void *)dataout);
	  }

	sandbox_object_destroy(objectp);
	sandbox_class_destroy(classp);

	return (0);
}
