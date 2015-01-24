#
#  Copyright (c) 2012 Spectra Logic Corporation
#  All rights reserved.
# 
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions, and the following disclaimer,
#     without modification.
#  2. Redistributions in binary form must reproduce at minimum a disclaimer
#     substantially similar to the "NO WARRANTY" disclaimer below
#     ("Disclaimer") and any redistribution must be conditioned upon
#     including a substantially similar Disclaimer requirement for further
#     binary redistribution.
# 
#  NO WARRANTY
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
#  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#  HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
#  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
#  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
#  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
#  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
#  IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGES.
# 
#  Authors: Alan Somers         (Spectra Logic Corporation)
#
# $FreeBSD$


#
# Test Case: Repeatedly load and unload the fasttrap module in an environment
# with frequent forks and execs and verify that the kernel does not panic.
# Regresses SpectraLogic BUG25389 and BUG25382
#
atf_test_case fasttrap_load_unload
fasttrap_load_unload_head()
{
	atf_set "descr" "Rapidly load and unload the fasttrap module"
	atf_set "require.files" "/boot/kernel/fasttrap.ko"
	atf_set "require.user" "root"
}

fasttrap_load_unload_body()
{
	fork_bomb_pids=""

	# Explicitly load these modules (dependencies of fasttrap) so they
	# won't be implicitly unloaded every time we unload fasttrap.
	kldload cyclic
	kldload dtrace

	# Verify that we can load and unload fasttrap
	{ kldload fasttrap && kldunload fasttrap ; } || \
		atf_skip "Cannot load and unload the fasttrap module"

	# The number of fork_bomb threads should be comfortably greater than
	# the number of cpus.  That way we maximize the chance that a process
	# gets preempted during the critical time window
	N_FORK_THREADS=$(( 6 * $(sysctl kern.smp.cpus | awk '{print $2}') ))

	i=0
	while [ $i -lt $N_FORK_THREADS ]; do
		fork_bomb &
		fork_bomb_pids="$fork_bomb_pids $!"
		i=$(($i+1))
	done

	i=0
	while [ $i -lt "$RELOAD_COUNT" ]; do
		kldload fasttrap
		kldunload fasttrap
		i=$(($i+1))
	done

	for p in $fork_bomb_pids; do 
		kill "$p"
	done

	# If we didn't panic, then we passed
	atf_pass
}


atf_init_test_cases()
{
  atf_add_test_case fasttrap_load_unload
}

export RELOAD_COUNT=500

fork_bomb()
{
	while true; do
		uname > /dev/null
	done
}
