#include "opt_defunct.h"

#ifdef CHILD_MAX
#warning "obsolete option CHILD_MAX - rename it to PROC0_RLIMIT_NPROC"
#endif

#ifdef GATEWAY
#warning "obsolete option EXTRAVNODES - use `sysctl -w kern.maxvnodes=value'"
#endif

#ifdef GATEWAY
#warning "obsolete option GATEWAY - use `sysctl -w net.inet.ip_forwarding=1'"
#endif

#ifdef OPEN_MAX
#warning "obsolete option OPEN_MAX - rename it to PROC0_RLIMIT_NOFILE"
#endif
