/*
 * $Id$
 */

struct tun_data {
#ifdef __OpenBSD__
  struct tunnel_header head;
#endif
  u_char data[MAX_MRU];
};

#ifdef __OpenBSD__
#define tun_fill_header(f,proto) do { (f).head.tun_af = (proto); } while (0)
#define tun_check_header(f,proto) ((f).head.tun_af == (proto))
#else
#define tun_fill_header(f,proto) do { } while (0)
#define tun_check_header(f,proto) (1)
#endif

extern void tun_configure(int, int);
