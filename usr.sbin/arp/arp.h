#ifndef _USR_SBIN_ARP_ARP_H_
#define _USR_SBIN_ARP_ARP_H_

int valid_type(int type);
struct sockaddr_in *getaddr(char *host);
int print_entries_nl(uint32_t ifindex, struct in_addr addr);

struct arp_opts {
	bool aflag;
	bool nflag;
	time_t expire_time;
	int flags;
	char *rifname;
	unsigned int rifindex;
};
extern struct arp_opts opts;

int print_entries_nl(uint32_t ifindex, struct in_addr addr);
int delete_nl(uint32_t ifindex, char *host);
int set_nl(uint32_t ifindex, struct sockaddr_in *dst, struct sockaddr_dl *sdl,
    char *host);

#endif
