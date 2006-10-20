#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>

#include "emac.h"
#include "lib.h"
#include "ee.h"

extern unsigned char mac[];

static void
MacFromEE()
{	
	uint32_t sig;
	sig = 0;
	EERead(12 * 1024, (uint8_t *)&sig, sizeof(sig));
	if (sig != 0xaa55aa55)
		return;
	EERead(12 * 1024 + 4, mac, 6);
	printf("MAC %x:%x:%x:%x:%x:%x\n", mac[0],
	  mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void
Update(void)
{
}

void
board_init(void)
{
    EEInit();
    MacFromEE();
}
