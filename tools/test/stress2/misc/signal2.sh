#!/bin/sh

# Test scenario from:
# Bug 265889 - sys.kern.basic_signal.trap_signal_test crashes bhyve in i386 VM 
# Test scenario by: Li-Wen Hsu <lwhsu@FreeBSD.org>

cat > /tmp/signal2.c <<EOF
#include <stdio.h>
#include <signal.h>

#include <machine/psl.h>
#define    SET_TRACE_FLAG(ucp)    (ucp)->uc_mcontext.mc_eflags |= PSL_T
#define    CLR_TRACE_FLAG(ucp)    (ucp)->uc_mcontext.mc_eflags &= ~PSL_T

static volatile sig_atomic_t trap_signal_fired = 0;

static void
trap_sig_handler(int signo __unused, siginfo_t *info __unused, void *_ucp)
{
	ucontext_t *ucp = _ucp;

	if (trap_signal_fired < 9) {
		SET_TRACE_FLAG(ucp);
	} else {
		CLR_TRACE_FLAG(ucp);
	}
	trap_signal_fired++;
}

int main() {
	struct sigaction sa = {
		.sa_sigaction = trap_sig_handler,
		.sa_flags = SA_SIGINFO,
	};

	sigemptyset(&sa.sa_mask);
	sigaction(SIGTRAP, &sa, NULL);

	raise(SIGTRAP);

	printf("test\n");
}
EOF
cc -o /tmp/signal2 -Wall -Wextra -O0 -m32 /tmp/signal2.c || exit 1

/tmp/signal2; s=$?
for i in `jot 30`; do
	/tmp/signal2 &
done > /dev/null
wait

rm -f /tmp/signal2 /tmp/signal2.c
exit $s
