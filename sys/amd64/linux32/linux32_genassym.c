#include <sys/param.h>
#include <sys/systm.h>
#include <sys/assym.h>
#include <sys/resource.h>

#include <amd64/linux32/linux.h>
#include <compat/linux/linux_mib.h>

#include <x86/linux/linux_x86_sigframe.h>

ASSYM(LINUX_SIGF_SC, offsetof(struct l_sigframe, sf_sc));
ASSYM(LINUX_RT_SIGF_UC, offsetof(struct l_rt_sigframe, sf_uc));
ASSYM(LINUX_RT_SIGF_SC, offsetof(struct l_ucontext, uc_mcontext));
ASSYM(L_SC_GS, offsetof(struct l_sigcontext, sc_gs));
ASSYM(L_SC_FS, offsetof(struct l_sigcontext, sc_fs));
ASSYM(L_SC_ES, offsetof(struct l_sigcontext, sc_es));
ASSYM(L_SC_DS, offsetof(struct l_sigcontext, sc_ds));
ASSYM(L_SC_CS, offsetof(struct l_sigcontext, sc_cs));
ASSYM(L_SC_SS, offsetof(struct l_sigcontext, sc_ss));
ASSYM(L_SC_EFLAGS, offsetof(struct l_sigcontext, sc_eflags));
ASSYM(L_SC_EDI, offsetof(struct l_sigcontext, sc_edi));
ASSYM(L_SC_ESI, offsetof(struct l_sigcontext, sc_esi));
ASSYM(L_SC_EBP, offsetof(struct l_sigcontext, sc_ebp));
ASSYM(L_SC_EBX, offsetof(struct l_sigcontext, sc_ebx));
ASSYM(L_SC_EDX, offsetof(struct l_sigcontext, sc_edx));
ASSYM(L_SC_ECX, offsetof(struct l_sigcontext, sc_ecx));
ASSYM(L_SC_EAX, offsetof(struct l_sigcontext, sc_eax));
ASSYM(L_SC_EIP, offsetof(struct l_sigcontext, sc_eip));
ASSYM(L_SC_ESP, offsetof(struct l_sigcontext, sc_esp_at_signal));
ASSYM(LINUX_VERSION_CODE, LINUX_VERSION_CODE);
