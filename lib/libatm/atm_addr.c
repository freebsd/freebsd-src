/*
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 *	@(#) $FreeBSD: src/lib/libatm/atm_addr.c,v 1.4 1999/08/27 23:58:04 peter Exp $
 *
 */

/*
 * User Space Library Functions
 * ----------------------------
 *
 * ATM address utility functions
 *
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netatm/port.h>
#include <netatm/atm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_ioctl.h>

#include <stdio.h>
#include <string.h>

#include "libatm.h"

#ifndef lint
__RCSID("@(#) $FreeBSD: src/lib/libatm/atm_addr.c,v 1.4 1999/08/27 23:58:04 peter Exp $");
#endif


extern char	*prog;


/*
 * Get NSAP, NSAP prefix or MAC address
 *
 * Arguments:
 *	in	pointer to an address in ASCII
 *	out	pointer to a buffer for the converted address
 *	len	the length of the output buffer
 *
 * Returns:
 *	0	error in format
 *	len	the length of the data in the output buffer
 *
 */
int
get_hex_atm_addr(in, out, len)
	char	*in;
	u_char	*out;
	int	len;
{
	int	c_type, c_value, i, out_len, state, val = 0;

	/*
	 * Character table
	 */
	static struct {
		char	c;
		int	type;
		int	value;
	} char_table[] = {
		{'.',	0,	0},	/* Type 0 -- period */
		{':',	0,	0},	/* Type 0 -- colon */
		{'0',	1,	0},	/* Type 1 -- hex digit */
		{'1',	1,	1},
		{'2',	1,	2},
		{'3',	1,	3},
		{'4',	1,	4},
		{'5',	1,	5},
		{'6',	1,	6},
		{'7',	1,	7},
		{'8',	1,	8},
		{'9',	1,	9},
		{'a',	1,	10},
		{'b',	1,	11},
		{'c',	1,	12},
		{'d',	1,	13},
		{'e',	1,	14},
		{'f',	1,	15},
		{'A',	1,	10},
		{'B',	1,	11},
		{'C',	1,	12},
		{'D',	1,	13},
		{'E',	1,	14},
		{'F',	1,	15},
		{'\0',	2,	0},	/* Type 2 -- end of input */
	};

	/*
	 * State table
	 */
	static struct {
		int	action;
		int	state;
	} state_table[3][3] = {
		/* Period     Hex       End			*/
		{ { 0, 0 }, { 1, 1 }, { 2, 0} },	/* Init	*/
		{ { 4, 0 }, { 3, 2 }, { 4, 0} },	/* C1	*/
		{ { 0, 2 }, { 1, 1 }, { 2, 0} },	/* C2	*/
	};

	/*
	 * Initialize
	 */
	state = 0;
	out_len = 0;
	if (!strncasecmp(in, "0x", 2)) {
		in += 2;
	}

	/*
	 * Loop through input until state table says to return
	 */
	while (1) {
		/*
		 * Get the character type and value
		 */
		for (i=0; char_table[i].c; i++)
			if (char_table[i].c == *in)
				break;
		if (char_table[i].c != *in)
			return(0);
		c_type = char_table[i].type;
		c_value = char_table[i].value;

		/*
		 * Process next character based on state and type
		 */
		switch(state_table[state][c_type].action) {
		case 0:
			/*
			 * Ignore the character
			 */
			break;

		case 1:
			/*
			 * Save the character's value
			 */
			val = c_value;
			break;

		case 2:
			/*
			 * Return the assembled NSAP
			 */
			return(out_len);

		case 3:
			/*
			 * Assemble and save the output byte
			 */
			val = val << 4;
			val += c_value;
			out[out_len] = (u_char) val;
			out_len++;
			break;

		case 4:
			/*
			 * Invalid input sequence
			 */
			return(0);

		default:
			return(0);
		}

		/*
		 * Set the next state and go on to the next character
		 */
		state = state_table[state][c_type].state;
		in++;
	}
}


/*
 * Format an ATM address into a string
 * 
 * Arguments:
 *	addr	pointer to an atm address
 *
 * Returns:
 *	none
 *
 */
char *
format_atm_addr(addr)
	Atm_addr *addr;
{
	int		i;
	char		*nsap_format;
	Atm_addr_nsap	*atm_nsap;
	Atm_addr_e164	*atm_e164;
	Atm_addr_spans	*atm_spans;
	Atm_addr_pvc	*atm_pvc;
	static char	str[256];
	union {
		int	w;
		char	c[4];
	} u1, u2;

	static char	nsap_format_DCC[] = "0x%02x.%02x%02x.%02x.%02x%02x%02x.%02x%02x.%02x%02x.%02x%02x.%02x%02x%02x%02x%02x%02x.%02x";
	static char	nsap_format_ICD[] = "0x%02x.%02x%02x.%02x.%02x%02x%02x.%02x%02x.%02x%02x.%02x%02x.%02x%02x%02x%02x%02x%02x.%02x";
	static char	nsap_format_E164[] = "0x%02x.%02x%02x%02x%02x%02x%02x%02x%02x.%02x%02x.%02x%02x.%02x%02x%02x%02x%02x%02x.%02x";

	/*
	 * Clear the returned string
	 */
	UM_ZERO(str, sizeof(str));
	strcpy(str, "-");

	/*
	 * Print format is determined by address type
	 */
	switch (addr->address_format) {
	case T_ATM_ENDSYS_ADDR:
		atm_nsap = (Atm_addr_nsap *)addr->address;
		switch(atm_nsap->aan_afi) {
		default:
		case AFI_DCC:
			nsap_format = nsap_format_DCC;
			break;
		case AFI_ICD:
			nsap_format = nsap_format_ICD;
			break;
		case AFI_E164:
			nsap_format = nsap_format_E164;
			break;
		}
		sprintf(str, nsap_format, 
				atm_nsap->aan_afi,
				atm_nsap->aan_afspec[0],
				atm_nsap->aan_afspec[1],
				atm_nsap->aan_afspec[2],
				atm_nsap->aan_afspec[3],
				atm_nsap->aan_afspec[4],
				atm_nsap->aan_afspec[5],
				atm_nsap->aan_afspec[6],
				atm_nsap->aan_afspec[7],
				atm_nsap->aan_afspec[8],
				atm_nsap->aan_afspec[9],
				atm_nsap->aan_afspec[10],
				atm_nsap->aan_afspec[11],
				atm_nsap->aan_esi[0],
				atm_nsap->aan_esi[1],
				atm_nsap->aan_esi[2],
				atm_nsap->aan_esi[3],
				atm_nsap->aan_esi[4],
				atm_nsap->aan_esi[5],
				atm_nsap->aan_sel);
		break;

	case T_ATM_E164_ADDR:
		atm_e164 = (Atm_addr_e164 *)addr->address;
		for(i=0; i<addr->address_length; i++) {
			sprintf(&str[strlen(str)], "%c",
					atm_e164->aae_addr[i]);
		}
		break;

	case T_ATM_SPANS_ADDR:
		/*
		 * Print SPANS address as two words, xxxx.yyyy
		 */
		atm_spans = (Atm_addr_spans *)addr->address;
		u1.c[0] = atm_spans->aas_addr[0];
		u1.c[1] = atm_spans->aas_addr[1];
		u1.c[2] = atm_spans->aas_addr[2];
		u1.c[3] = atm_spans->aas_addr[3];

		u2.c[0] = atm_spans->aas_addr[4];
		u2.c[1] = atm_spans->aas_addr[5];
		u2.c[2] = atm_spans->aas_addr[6];
		u2.c[3] = atm_spans->aas_addr[7];

		if (!(u1.w == 0 && u2.w == 0))
			sprintf(str, "0x%08lx.%08lx", ntohl(u1.w), ntohl(u2.w));
		break;

	case T_ATM_PVC_ADDR:
		/*
		 * Print PVC as VPI, VCI
		 */
		atm_pvc = (Atm_addr_pvc *)addr->address;
		sprintf(str, "%d, %d",
				ATM_PVC_GET_VPI(atm_pvc),
				ATM_PVC_GET_VCI(atm_pvc));
		break;

	case T_ATM_ABSENT:
	default:
		break;
	}

	return(str);
}
