/* Intel 386 running any FreeBSD Unix */

#include <machine/param.h>
#include <machine/vmparam.h>

#define NBPG			PAGE_SIZE
#define	HOST_PAGE_SIZE		NBPG
#define	HOST_MACHINE_ARCH	bfd_arch_i386
#define	HOST_TEXT_START_ADDR		USRTEXT

/* Jolitz suggested defining HOST_STACK_END_ADDR to
   (u.u_kproc.kp_eproc.e_vm.vm_maxsaddr + MAXSSIZ), which should work on
   both BSDI and 386BSD, but that is believed not to work for BSD 4.4.  */

/* This seems to be the right thing for FreeBSD and BSDI */
#define	HOST_STACK_END_ADDR	USRSTACK

/* Leave HOST_DATA_START_ADDR undefined, since the default when it is not
   defined sort of works, and FreeBSD-2.0 through FreeBSD-2.2.2 do not
   set u.u_kproc.kp_eproc.e_vm.vm_daddr.  */

#define TRAD_UNIX_CORE_FILE_FAILING_SIGNAL(core_bfd) \
  ((core_bfd)->tdata.trad_core_data->u.u_sig)
#define u_comm u_kproc.kp_proc.p_comm
