#include "opt_defunct.h"

#ifdef CHILD_MAX
#warning "obsolete option CHILD_MAX - use /etc/login.conf"
#endif

#ifdef EXTRAVNODES
#warning "obsolete option EXTRAVNODES - use `sysctl -w kern.maxvnodes=value'"
#endif

#ifdef GATEWAY
#warning "obsolete option GATEWAY - use `sysctl -w net.inet.ip_forwarding=1'"
#endif

#ifdef OPEN_MAX
#warning "obsolete option OPEN_MAX - use /etc/login.conf"
#endif
