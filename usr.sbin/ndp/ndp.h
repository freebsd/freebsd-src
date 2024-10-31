#ifndef _USR_SBIN_NDP_NDP_H_
#define _USR_SBIN_NDP_NDP_H_

#define W_ADDR	36
#define W_LL	17
#define W_IF	6

struct ndp_opts {
	bool nflag;
	bool tflag;
	int flags;
	time_t expire_time;
	int repeat;
};

extern struct ndp_opts opts;

bool valid_type(int if_type);
void ts_print(const struct timeval *tvp);
char *ether_str(struct sockaddr_dl *sdl);
char *sec2str(time_t total);
int getaddr(char *host, struct sockaddr_in6 *sin6);
int print_entries_nl(uint32_t ifindex, struct sockaddr_in6 *addr, bool cflag);
int delete_nl(uint32_t ifindex, char *host, bool warn);
int set_nl(uint32_t ifindex, struct sockaddr_in6 *dst, struct sockaddr_dl *sdl,
    char *host);

#endif
