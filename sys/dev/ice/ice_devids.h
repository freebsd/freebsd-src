/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2024, Intel Corporation
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of the Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _ICE_DEVIDS_H_
#define _ICE_DEVIDS_H_

/* Device IDs */
#define ICE_DEV_ID_E822_SI_DFLT		0x1888
/* Intel(R) Ethernet Connection E823-L for backplane */
#define ICE_DEV_ID_E823L_BACKPLANE	0x124C
/* Intel(R) Ethernet Connection E823-L for SFP */
#define ICE_DEV_ID_E823L_SFP		0x124D
/* Intel(R) Ethernet Connection E823-L/X557-AT 10GBASE-T */
#define ICE_DEV_ID_E823L_10G_BASE_T	0x124E
/* Intel(R) Ethernet Connection E823-L 1GbE */
#define ICE_DEV_ID_E823L_1GBE		0x124F
/* Intel(R) Ethernet Connection E823-L for QSFP */
#define ICE_DEV_ID_E823L_QSFP		0x151D
/* Intel(R) Ethernet Controller E830-CC for backplane */
#define ICE_DEV_ID_E830_BACKPLANE	0x12D1
/* Intel(R) Ethernet Controller E830-CC for QSFP */
#define ICE_DEV_ID_E830_QSFP56		0x12D2
/* Intel(R) Ethernet Controller E830-CC for SFP */
#define ICE_DEV_ID_E830_SFP		0x12D3
/* Intel(R) Ethernet Controller E830-C for backplane */
#define ICE_DEV_ID_E830C_BACKPLANE      0x12D5
/* Intel(R) Ethernet Controller E830-XXV for backplane */
#define ICE_DEV_ID_E830_XXV_BACKPLANE   0x12DC
/* Intel(R) Ethernet Controller E830-C for QSFP */
#define ICE_DEV_ID_E830C_QSFP           0x12D8
/* Intel(R) Ethernet Controller E830-XXV for QSFP */
#define ICE_DEV_ID_E830_XXV_QSFP        0x12DD
/* Intel(R) Ethernet Controller E830-C for SFP */
#define ICE_DEV_ID_E830C_SFP            0x12DA
/* Intel(R) Ethernet Controller E830-XXV for SFP */
#define ICE_DEV_ID_E830_XXV_SFP         0x12DE
/* Intel(R) Ethernet Controller E810-C for backplane */
#define ICE_DEV_ID_E810C_BACKPLANE	0x1591
/* Intel(R) Ethernet Controller E810-C for QSFP */
#define ICE_DEV_ID_E810C_QSFP		0x1592
/* Intel(R) Ethernet Controller E810-C for SFP */
#define ICE_DEV_ID_E810C_SFP		0x1593
#define ICE_SUBDEV_ID_E810T		0x000E
#define ICE_SUBDEV_ID_E810T2		0x000F
#define ICE_SUBDEV_ID_E810T3		0x0010
#define ICE_SUBDEV_ID_E810T4		0x0011
#define ICE_SUBDEV_ID_E810T5		0x0012
#define ICE_SUBDEV_ID_E810T6		0x02E9
#define ICE_SUBDEV_ID_E810T7		0x02EA
/* Intel(R) Ethernet Controller E810-XXV for backplane */
#define ICE_DEV_ID_E810_XXV_BACKPLANE	0x1599
/* Intel(R) Ethernet Controller E810-XXV for QSFP */
#define ICE_DEV_ID_E810_XXV_QSFP	0x159A
/* Intel(R) Ethernet Controller E810-XXV for SFP */
#define ICE_DEV_ID_E810_XXV_SFP		0x159B
/* Intel(R) Ethernet Connection E823-C for backplane */
#define ICE_DEV_ID_E823C_BACKPLANE	0x188A
/* Intel(R) Ethernet Connection E823-C for QSFP */
#define ICE_DEV_ID_E823C_QSFP		0x188B
/* Intel(R) Ethernet Connection E823-C for SFP */
#define ICE_DEV_ID_E823C_SFP		0x188C
/* Intel(R) Ethernet Connection E823-C/X557-AT 10GBASE-T */
#define ICE_DEV_ID_E823C_10G_BASE_T	0x188D
/* Intel(R) Ethernet Connection E823-C 1GbE */
#define ICE_DEV_ID_E823C_SGMII		0x188E
/* Intel(R) Ethernet Connection E822-C for backplane */
#define ICE_DEV_ID_E822C_BACKPLANE	0x1890
/* Intel(R) Ethernet Connection E822-C for QSFP */
#define ICE_DEV_ID_E822C_QSFP		0x1891
/* Intel(R) Ethernet Connection E822-C for SFP */
#define ICE_DEV_ID_E822C_SFP		0x1892
/* Intel(R) Ethernet Connection E822-C/X557-AT 10GBASE-T */
#define ICE_DEV_ID_E822C_10G_BASE_T	0x1893
/* Intel(R) Ethernet Connection E822-C 1GbE */
#define ICE_DEV_ID_E822C_SGMII		0x1894
/* Intel(R) Ethernet Connection E822-L for backplane */
#define ICE_DEV_ID_E822L_BACKPLANE	0x1897
/* Intel(R) Ethernet Connection E822-L for SFP */
#define ICE_DEV_ID_E822L_SFP		0x1898
/* Intel(R) Ethernet Connection E822-L/X557-AT 10GBASE-T */
#define ICE_DEV_ID_E822L_10G_BASE_T	0x1899
/* Intel(R) Ethernet Connection E822-L 1GbE */
#define ICE_DEV_ID_E822L_SGMII		0x189A
/* Intel(R) Ethernet Connection E825-C for backplane */
#define ICE_DEV_ID_E825C_BACKPLANE	0x579C
/* Intel(R) Ethernet Connection E825-C for QSFP */
#define ICE_DEV_ID_E825C_QSFP		0x579D
/* Intel(R) Ethernet Connection E825-C for SFP */
#define ICE_DEV_ID_E825C_SFP		0x579E
/* Intel(R) Ethernet Connection E825-C 1GbE */
#define ICE_DEV_ID_E825C_SGMII		0x579F
#endif /* _ICE_DEVIDS_H_ */
