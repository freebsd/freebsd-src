/* Panic handler. MUST BE PIC. BE CAREFUL WHAT YOU DO,
 * AS THERE ARE NO STATICS AVAILABLE, AND NOWHERE TO GO 
 * MMU IS OFF!!!!!!!!
 */

#include <linux/kernel.h>
#include <asm/io.h>
#include <asm/registers.h>

/* THIS IS A PHYSICAL ADDRESS */
#define HDSP2534_ADDR (0x04002100)

void
panic_handler(unsigned long panicPC, unsigned long panicSSR,
	      unsigned long panicEXPEVT)
{
	int i;
	unsigned nibble;

#ifdef CONFIG_SH_CAYMAN

	while (1) {

		/* This piece of code displays the PC on the LED display */
		for (i = 0; i < 8; i++) {
			nibble = ((panicPC >> (i * 4)) & 0xf);

			ctrl_outb(nibble + ((nibble > 9) ? 55 : 48),
				  HDSP2534_ADDR + 0xe0 + ((7 - i) << 2));
		}

		for (i = 0; i < 2500000; i++) {
		}		/* poor man's delay */

		for (i = 0; i < 8; i++) {
			nibble = ((panicSSR >> (i * 4)) & 0xf);

			ctrl_outb(nibble + ((nibble > 9) ? 55 : 48),
				  HDSP2534_ADDR + 0xe0 + ((7 - i) << 2));
		}

		for (i = 0; i < 2500000; i++) {
		}		/* poor man's delay */

		for (i = 0; i < 8; i++) {
			nibble = ((panicEXPEVT >> (i * 4)) & 0xf);

			ctrl_outb(nibble + ((nibble > 9) ? 55 : 48),
				  HDSP2534_ADDR + 0xe0 + ((7 - i) << 2));
		}

		for (i = 0; i < 2500000; i++) {
		}		/* poor man's delay */
	}
#endif

	/* Never return from the panic handler */
	for (;;) ;

}
