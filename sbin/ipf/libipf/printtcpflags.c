#include "ipf.h"


void
printtcpflags(u_32_t tcpf, u_32_t tcpfm)
{
	u_char *t;
	char *s;

	if (tcpf & ~TCPF_ALL) {
		PRINTF("0x%x", tcpf);
	} else {
		for (s = flagset, t = flags; *s; s++, t++) {
			if (tcpf & *t)
				(void)putchar(*s);
		}
	}

	if (tcpfm) {
		(void)putchar('/');
		if (tcpfm & ~TCPF_ALL) {
			PRINTF("0x%x", tcpfm);
		} else {
			for (s = flagset, t = flags; *s; s++, t++)
				if (tcpfm & *t)
					(void)putchar(*s);
		}
	}
}
