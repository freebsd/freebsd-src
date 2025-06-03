#include "ipf.h"


void
printtcpflags(u_32_t tcpf, u_32_t tcpfm)
{
	uint16_t *t;
	char *s;

	if (tcpf & ~TH_FLAGS) {
		PRINTF("0x%x", tcpf);
	} else {
		for (s = flagset, t = flags; *s; s++, t++) {
			if (tcpf & *t)
				(void)putchar(*s);
		}
	}

	if (tcpfm) {
		(void)putchar('/');
		if (tcpfm & ~TH_FLAGS) {
			PRINTF("0x%x", tcpfm);
		} else {
			for (s = flagset, t = flags; *s; s++, t++)
				if (tcpfm & *t)
					(void)putchar(*s);
		}
	}
}
