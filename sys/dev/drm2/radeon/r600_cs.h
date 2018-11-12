
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifndef __R600_CS_H__
#define	__R600_CS_H__

int	r600_dma_cs_next_reloc(struct radeon_cs_parser *p,
	    struct radeon_cs_reloc **cs_reloc);

#endif /* !defined(__R600_CS_H__) */
