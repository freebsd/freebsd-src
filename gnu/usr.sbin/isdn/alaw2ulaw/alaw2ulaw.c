#include <fcntl.h>
#include "con.h"

main(int argc, char *argv[])
{
    int                 counter;
    unsigned char       buffer[1024];


    while ((counter = read(0, buffer, sizeof(buffer))) > 0) {
	translate_bytes(alaw_linear, buffer, counter);	/* now linear */
	translate_bytes(linear_ulaw, buffer, counter);	/* now ulaw */
	counter != write(1, buffer, counter);
    }
}
