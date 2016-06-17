#include <asm/contregs.h>
#include <asm/sun3mmu.h>
#include <asm/io.h>
#include <asm/movs.h>

#define FC_CONTROL 3    /* This should go somewhere else... */

void sun3_leds(unsigned char byte)
{
	unsigned char dfc;
	
	GET_DFC(dfc);
        SET_DFC(FC_CONTROL);
       	SET_CONTROL_BYTE(AC_LEDS,byte);
	SET_DFC(dfc);
}
