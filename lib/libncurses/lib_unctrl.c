#include <ctype.h>
#include <unctrl.h>

char *
 unctrl(register unsigned char uch)
{
    static char buffer[3] = "^x";

    if (isgraph(uch)) {
	/*
	 * Printable character. Simply return the character as a one-character
	 * string.
	 */
	buffer[1] = uch;
	return &buffer[1];
    }
    uch &= ~0x80;
    /*
     * It is a control character. DEL is handled specially (^?). All others
     * use ^x notation, where x is the character code for the control character
     * with 0x40 ORed in. (Control-A becomes ^A etc.).
     */ buffer[1] = (uch == 0x7F ? '?' : (uch | 0x40));

    return buffer;

}
