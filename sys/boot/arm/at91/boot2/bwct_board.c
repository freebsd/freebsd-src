#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>

#include "emac.h"
#include "lib.h"
#include "ee.h"
#include "board.h"
#include "sd-card.h"

unsigned char mac[6];

static void
MacFromEE()
{	
#if 0
	uint32_t sig;

	sig = 0;
	EERead(0, (uint8_t *)&sig, sizeof(sig));
	if (sig != 0xaa55aa55)
		return;
	EERead(48, mac, 3);
	EERead(48+5, mac+3, 3);
#else
	mac[0] = 0x00;
	mac[1] = 0x0e;
	mac[2] = 0x42;
	mac[3] = 0x02;
	mac[4] = 0x00;
	mac[5] = 0x21;
#endif
	printf("MAC %x:%x:%x:%x:%x:%x\n", mac[0],
	  mac[1], mac[2], mac[3], mac[4], mac[5]);
}

#ifdef XMODEM_DL
#define FLASH_OFFSET (0 * FLASH_PAGE_SIZE)
#define KERNEL_OFFSET (220 * FLASH_PAGE_SIZE)
#define KERNEL_LEN (6 * 1024 * FLASH_PAGE_SIZE)

static void
UpdateFlash(int offset)
{
	char *addr = (char *)0x20000000 + (1 << 20); /* Load to base + 1MB */
	int len, i, off;

	while ((len = xmodem_rx(addr)) == -1)
		continue;
	printf("\nDownloaded %u bytes.\n", len);
	for (i = 0; i < len; i+= FLASH_PAGE_SIZE) {
		off = i + offset;
		SPI_WriteFlash(off, addr + i, FLASH_PAGE_SIZE);
	}
}
void
Update(void)
{
	UpdateFlash(FLASH_OFFSET);
}

#else
void
Update(void)
{
}
#endif

void
board_init(void)
{
    EEInit();
    MacFromEE();
    EMAC_Init();
    sdcard_init();
    EMAC_SetMACAddress(mac);

}

#include "../bootspi/ee.c"

int
drvread(void *buf, unsigned lba, unsigned nblk)
{
    return (MCI_read((char *)buf, lba << 9, nblk << 9));
}
