/* Define if compiling on Solaris 7. */
#undef _MSE_INT_H

/* Define if pstatus_t type is available */
#undef HAVE_PSTATUS_T

/* Define if prrun_t type is available */
#undef HAVE_PRRUN_T

/* Define if fpregset_t type is available. */
#undef HAVE_FPREGSET_T

/* Define if gregset_t type is available. */
#undef HAVE_GREGSET_T

/* Define if <sys/procfs.h> has prgregset_t. */
#undef HAVE_PRGREGSET_T

/* Define if <sys/procfs.h> has prfpregset_t. */
#undef HAVE_PRFPREGSET_T

/* Define if <sys/procfs.h> has lwpid_t. */
#undef HAVE_LWPID_T

/* Define if <sys/procfs.h> has psaddr_t. */
#undef HAVE_PSADDR_T

/* Define if the prfpregset_t type is broken. */
#undef PRFPREGSET_T_BROKEN

/* Define if you want to use new multi-fd /proc interface
   (replaces HAVE_MULTIPLE_PROC_FDS as well as other macros). */
#undef NEW_PROC_API

/* Define if ioctl argument PIOCSET is available. */
#undef HAVE_PROCFS_PIOCSET

/* Define if the `long long' type works.  */
#undef CC_HAS_LONG_LONG

/* Define if the "ll" format works to print long long ints. */
#undef PRINTF_HAS_LONG_LONG

/* Define if the "%Lg" format works to print long doubles. */
#undef PRINTF_HAS_LONG_DOUBLE

/* Define if the "%Lg" format works to scan long doubles. */
#undef SCANF_HAS_LONG_DOUBLE

/* Define if using Solaris thread debugging.  */
#undef HAVE_THREAD_DB_LIB

/* Define on a GNU/Linux system to work around problems in sys/procfs.h.  */
#undef START_INFERIOR_TRAPS_EXPECTED
#undef sys_quotactl

/* Define if you have HPUX threads */
#undef HAVE_HPUX_THREAD_SUPPORT

/* Define if you want to use the memory mapped malloc package (mmalloc). */
#undef USE_MMALLOC

/* Define if the runtime uses a routine from mmalloc before gdb has a chance
   to initialize mmalloc, and we want to force checking to be used anyway.
   This may cause spurious memory corruption messages if the runtime tries
   to explicitly deallocate that memory when gdb calls exit. */
#undef MMCHECK_FORCE

/* Define to 1 if NLS is requested.  */
#undef ENABLE_NLS

/* Define as 1 if you have catgets and don't want to use GNU gettext.  */
#undef HAVE_CATGETS

/* Define as 1 if you have gettext and don't want to use GNU gettext.  */
#undef HAVE_GETTEXT

/* Define as 1 if you have the stpcpy function.  */
#undef HAVE_STPCPY

/* Define if your locale.h file contains LC_MESSAGES.  */
#undef HAVE_LC_MESSAGES

/* Define if you want to use the full-screen terminal user interface.  */
#undef TUI

/* Define if <proc_service.h> on solaris uses int instead of
   size_t, and assorted other type changes. */
#undef PROC_SERVICE_IS_OLD

/* If you want to specify a default CPU variant, define this to be its
   name, as a C string.  */
#undef TARGET_CPU_DEFAULT

/* Define if the simulator is being linked in.  */
#undef WITH_SIM

/* Set to true if the save_state_t structure is present */
#undef HAVE_STRUCT_SAVE_STATE_T

/* Set to true if the save_state_t structure has the ss_wide member */
#undef HAVE_STRUCT_MEMBER_SS_WIDE

/* Define if <sys/ptrace.h> defines the PTRACE_GETREGS request.  */
#undef HAVE_PTRACE_GETREGS

/* Define if <sys/ptrace.h> defines the PTRACE_GETXFPREGS request.  */
#undef HAVE_PTRACE_GETXFPREGS
