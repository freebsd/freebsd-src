/*-
 * Copyright (c) 2002, 2003 Sam Leffler, Errno Consulting, Atheros
 * Communications, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the following conditions are met:
 * 1. The materials contained herein are unmodified and are used
 *    unmodified.
 * 2. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following NO
 *    ''WARRANTY'' disclaimer below (''Disclaimer''), without
 *    modification.
 * 3. Redistributions in binary form must reproduce at minimum a
 *    disclaimer similar to the Disclaimer below and any redistribution
 *    must be conditioned upon including a substantially similar
 *    Disclaimer requirement for further binary redistribution.
 * 4. Neither the names of the above-listed copyright holders nor the
 *    names of any contributors may be used to endorse or promote
 *    product derived from this software without specific prior written
 *    permission.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT,
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
 * FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES.
 *
 * $Id: ah_devid.h,v 1.6 2003/06/25 04:50:22 sam Exp $
 */

#ifndef _DEV_ATH_DEVID_H_
#define _DEV_ATH_DEVID_H_

#define ATHEROS_VENDOR_ID	0x168c		/* Atheros PCI vendor ID */

/* AR5210 (for reference) */
#define AR5210_DEFAULT          0x1107          /* No eeprom HW default */
#define AR5210_PROD             0x0007          /* Final device ID */
#define AR5210_AP               0x0207          /* Early AP11s */

/* AR5211 */
#define AR5211_DEFAULT          0x1112          /* No eeprom HW default */
#define AR5311_DEVID            0x0011          /* Final ar5311 devid */
#define AR5211_DEVID            0x0012          /* Final ar5211 devid */
#define AR5211_LEGACY           0xff12          /* Original emulation board */
#define AR5211_FPGA11B          0xf11b          /* 11b emulation board */

/* AR5212 */
#define AR5212_DEFAULT          0x1113          /* No eeprom HW default */
#define AR5212_DEVID            0x0013          /* Final ar5212 devid */
#define AR5212_FPGA             0xf013          /* Emulation board */

#define	AR_SUBVENDOR_ID_NOG	0x0e11		/* No 11G subvendor ID */
#endif /* _DEV_ATH_DEVID_H */
