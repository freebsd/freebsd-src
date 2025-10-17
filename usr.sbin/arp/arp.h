#ifndef _USR_SBIN_ARP_ARP_H_
#define _USR_SBIN_ARP_ARP_H_

int valid_type(int type);
int get_ifinfo(in_addr_t ipaddr, struct ether_addr *hwaddr, uint32_t *pifindex);
struct sockaddr_in *getaddr(char *host);

struct arp_opts {
	bool aflag;
	bool nflag;
	time_t expire_time;
	int flags;
	char *rifname;
	uint32_t rifindex;
};
extern struct arp_opts opts;

int print_entries_nl(uint32_t ifindex, struct in_addr addr);
int delete_nl(char *host);
int set_nl(struct sockaddr_in *dst, struct sockaddr_dl *sdl, char *host);

#endif
