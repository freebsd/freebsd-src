#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/assym.h>
#include <sys/systm.h>
#include <sys/signal.h>

#include <compat/freebsd32/freebsd32_signal.h>
#include <compat/ia32/ia32_signal.h>

ASSYM(IA32_SIGF_HANDLER, offsetof(struct ia32_sigframe, sf_ah));
ASSYM(IA32_SIGF_UC, offsetof(struct ia32_sigframe, sf_uc));
ASSYM(IA32_UC_GS, offsetof(struct ia32_ucontext, uc_mcontext.mc_gs));
ASSYM(IA32_UC_FS, offsetof(struct ia32_ucontext, uc_mcontext.mc_fs));
ASSYM(IA32_UC_ES, offsetof(struct ia32_ucontext, uc_mcontext.mc_es));
ASSYM(IA32_UC_DS, offsetof(struct ia32_ucontext, uc_mcontext.mc_ds));
ASSYM(IA32_UC_EDI, offsetof(struct ia32_ucontext, uc_mcontext.mc_edi));
ASSYM(IA32_UC_ESI, offsetof(struct ia32_ucontext, uc_mcontext.mc_esi));
ASSYM(IA32_UC_EBP, offsetof(struct ia32_ucontext, uc_mcontext.mc_ebp));
ASSYM(IA32_UC_EBX, offsetof(struct ia32_ucontext, uc_mcontext.mc_ebx));
ASSYM(IA32_UC_EDX, offsetof(struct ia32_ucontext, uc_mcontext.mc_edx));
ASSYM(IA32_UC_ECX, offsetof(struct ia32_ucontext, uc_mcontext.mc_ecx));
ASSYM(IA32_UC_EAX, offsetof(struct ia32_ucontext, uc_mcontext.mc_eax));
ASSYM(IA32_UC_EIP, offsetof(struct ia32_ucontext, uc_mcontext.mc_eip));
ASSYM(IA32_UC_CS, offsetof(struct ia32_ucontext, uc_mcontext.mc_cs));
ASSYM(IA32_UC_EFLAGS, offsetof(struct ia32_ucontext, uc_mcontext.mc_eflags));
ASSYM(IA32_UC_ESP, offsetof(struct ia32_ucontext, uc_mcontext.mc_esp));
ASSYM(IA32_UC_SS, offsetof(struct ia32_ucontext, uc_mcontext.mc_ss));
ASSYM(IA32_UC_FSBASE, offsetof(struct ia32_ucontext, uc_mcontext.mc_fsbase));
ASSYM(IA32_UC_GSBASE, offsetof(struct ia32_ucontext, uc_mcontext.mc_gsbase));
#ifdef COMPAT_43
ASSYM(IA32_SIGF_SC, offsetof(struct ia32_osigframe, sf_siginfo.si_sc));
#endif
#ifdef COMPAT_FREEBSD4
ASSYM(IA32_SIGF_UC4, offsetof(struct ia32_freebsd4_sigframe, sf_uc));
ASSYM(IA32_UC4_GS, offsetof(struct ia32_freebsd4_ucontext, uc_mcontext.mc_gs));
ASSYM(IA32_UC4_FS, offsetof(struct ia32_freebsd4_ucontext, uc_mcontext.mc_fs));
ASSYM(IA32_UC4_ES, offsetof(struct ia32_freebsd4_ucontext, uc_mcontext.mc_es));
ASSYM(IA32_UC4_DS, offsetof(struct ia32_freebsd4_ucontext, uc_mcontext.mc_ds));
#endif
