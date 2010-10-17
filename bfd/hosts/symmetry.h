/* Symmetry running either dynix 3.1 (bsd) or ptx (sysv).  */

#define NBPG 4096
#define UPAGES 1

#ifdef _SEQUENT_
/* ptx */
#define HOST_TEXT_START_ADDR 0
#define HOST_STACK_END_ADDR 0x3fffe000
#define TRAD_CORE_USER_OFFSET ((UPAGES * NBPG) - sizeof (struct user))
#else
/* dynix */
#define HOST_TEXT_START_ADDR 0x1000
#define HOST_DATA_START_ADDR (NBPG * u.u_tsize)
#define HOST_STACK_END_ADDR 0x3ffff000
#define TRAD_UNIX_CORE_FILE_FAILING_SIGNAL(core_bfd) \
  ((core_bfd)->tdata.trad_core_data->u.u_arg[0])
#endif

#define TRAD_CORE_DSIZE_INCLUDES_TSIZE
