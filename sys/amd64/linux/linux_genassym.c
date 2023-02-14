#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/assym.h>

#include <amd64/linux/linux.h>
#include <compat/linux/linux_mib.h>

#include <x86/linux/linux_x86_sigframe.h>

ASSYM(LINUX_RT_SIGF_UC, offsetof(struct l_rt_sigframe, sf_uc));
ASSYM(LINUX_RT_SIGF_SC, offsetof(struct l_ucontext, uc_mcontext));
ASSYM(L_SC_R8, offsetof(struct l_sigcontext, sc_r8));
ASSYM(L_SC_R9, offsetof(struct l_sigcontext, sc_r9));
ASSYM(L_SC_R10, offsetof(struct l_sigcontext, sc_r10));
ASSYM(L_SC_R11, offsetof(struct l_sigcontext, sc_r11));
ASSYM(L_SC_R12, offsetof(struct l_sigcontext, sc_r12));
ASSYM(L_SC_R13, offsetof(struct l_sigcontext, sc_r13));
ASSYM(L_SC_R14, offsetof(struct l_sigcontext, sc_r14));
ASSYM(L_SC_R15, offsetof(struct l_sigcontext, sc_r15));
ASSYM(L_SC_RDI, offsetof(struct l_sigcontext, sc_rdi));
ASSYM(L_SC_RSI, offsetof(struct l_sigcontext, sc_rsi));
ASSYM(L_SC_RBP, offsetof(struct l_sigcontext, sc_rbp));
ASSYM(L_SC_RBX, offsetof(struct l_sigcontext, sc_rbx));
ASSYM(L_SC_RDX, offsetof(struct l_sigcontext, sc_rdx));
ASSYM(L_SC_RAX, offsetof(struct l_sigcontext, sc_rax));
ASSYM(L_SC_RCX, offsetof(struct l_sigcontext, sc_rcx));
ASSYM(L_SC_RSP, offsetof(struct l_sigcontext, sc_rsp));
ASSYM(L_SC_RIP, offsetof(struct l_sigcontext, sc_rip));
ASSYM(L_SC_RFLAGS, offsetof(struct l_sigcontext, sc_rflags));
ASSYM(L_SC_CS, offsetof(struct l_sigcontext, sc_cs));
ASSYM(LINUX_VERSION_CODE, LINUX_VERSION_CODE);
