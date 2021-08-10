#!/bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2018-2021 Gavin D. Howard and contributors.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

# This script is only really useful for running on Linux. It tests the code to
# free temps in order to make an allocation work. In order to see it work, I
# suggest adding code after the following line in src/vm.c:
#
# if (BC_ERR(ptr == NULL)) bc_vm_fatalError(BC_ERR_FATAL_ALLOC_ERR);
#
# The code you should add is the following:
#
# bc_file_printf(&vm.ferr, "If you see this, the code worked.\n");
# bc_file_flush(&vm.ferr, bc_flush_none);
#
# If you do not see the that message printed, the code did not work. Or, in the
# case of some allocators, like jemalloc, the allocator just isn't great with
# turning a bunch of small allocations into a bigger allocation,

script="$0"
scriptdir=$(dirname "$script")

export LANG=C

virtlimit=1000000

ulimit -v $virtlimit

# This script is designed to allocate lots of memory with a lot of caching of
# numbers (the function f() specifically). Then, it's designed allocate one
# large number and grow it until allocation failure (the function g()).
"$scriptdir/../bin/bc" <<*EOF

define f(i, n) {
	if (n == 0) return i;
	return f(i + 1, n - 1)
}

define g(n) {
	t = (10^9)^(2^24)
	while (n) {
		n *= t
		print "success\n"
	}
}

iterations=2000000

for (l=0; l < 100; l++) {
    iterations
    j = f(0, iterations$)
    iterations += 100000
    print "here\n"
    n=10^235929600
    g(n)
    print "success\n"
    n=0
}
*EOF
