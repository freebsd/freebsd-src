#include "opt_defunct.h"

#ifdef CHILD_MAX
#warning "obsolete option CHILD_MAX - use /etc/login.conf"
#endif

#ifdef EXTRAVNODES
#warning "obsolete option EXTRAVNODES - use `sysctl -w kern.maxvnodes=value'"
#endif

#ifdef GATEWAY
#warning "obsolete option GATEWAY - use `sysctl -w net.inet.ip.forwarding=1'"
#endif

#ifdef ARP_PROXYALL
#warning "obsolete option ARP_PROXYALL - use `sysctl -w net.link.ether.inet.proxyall=1'"
#endif

#ifdef OPEN_MAX
#warning "obsolete option OPEN_MAX - use /etc/login.conf"
#endif
