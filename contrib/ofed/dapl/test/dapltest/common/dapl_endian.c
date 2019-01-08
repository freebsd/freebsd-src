/*
 * Copyright (c) 2002-2005, Network Appliance, Inc. All rights reserved.
 *
 * This Software is licensed under one of the following licenses:
 *
 * 1) under the terms of the "Common Public License 1.0" a copy of which is
 *    in the file LICENSE.txt in the root directory. The license is also
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/cpl.php.
 *
 * 2) under the terms of the "The BSD License" a copy of which is in the file
 *    LICENSE2.txt in the root directory. The license is also available from
 *    the Open Source Initiative, see
 *    http://www.opensource.org/licenses/bsd-license.php.
 *
 * 3) under the terms of the "GNU General Public License (GPL) Version 2" a 
 *    copy of which is in the file LICENSE3.txt in the root directory. The 
 *    license is also available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/gpl-license.php.
 *
 * Licensee has the right to choose one of the above licenses.
 *
 * Redistributions of source code must retain the above copyright
 * notice and one of the license notices.
 *
 * Redistributions in binary form must reproduce both the above copyright
 * notice, one of the license notices in the documentation
 * and/or other materials provided with the distribution.
 */

#include "dapl_proto.h"

void DT_Endian_Init(void)
{
	int endian;
	endian = 1;
	DT_local_is_little_endian = *((unsigned char *)(&endian)) == 1;
}

/*
 * Big/Little Endian conversion functions
 */

#define c1a32 ((DAT_UINT32)0x00FF00FF)
#define c1b32 ((DAT_UINT32)0xFF00FF00)
#define c2a32 ((DAT_UINT32)0x0000FFFF)
#define c2b32 ((DAT_UINT32)0xFFFF0000)
#define c164 ((DAT_UINT64)0x00FF00FF)
#define c1a64 (c164 | (c164 << 32))
#define c1b64 (c1a64 << 8)
#define c264 ((DAT_UINT64)0x0000FFFF)
#define c2a64 (c264 | (c264 << 32))
#define c2b64 (c2a64 << 16)
#define c3a64 ((DAT_UINT64)0xFFFFFFFF)
#define c3b64 (c3a64 << 32)

DAT_UINT32 DT_Endian32(DAT_UINT32 val)
{
	if (DT_local_is_little_endian) {
		return val;
	}
	val = ((val & c1a32) << 8) | ((val & c1b32) >> 8);
	val = ((val & c2a32) << 16) | ((val & c2b32) >> 16);
	return (val);
}

DAT_UINT64 DT_Endian64(DAT_UINT64 val)
{
	if (DT_local_is_little_endian) {
		return val;
	}
	val = ((val & c1a64) << 8) | ((val & c1b64) >> 8);
	val = ((val & c2a64) << 16) | ((val & c2b64) >> 16);
	val = ((val & c3a64) << 32) | ((val & c3b64) >> 32);
	return (val);
}

DAT_UINT32 DT_EndianMemHandle(DAT_UINT32 val)
{
	if (DT_local_is_little_endian)
		return val;
	val = ((val & c1a32) << 8) | ((val & c1b32) >> 8);
	val = ((val & c2a32) << 16) | ((val & c2b32) >> 16);
	return (val);
}

DAT_UINT64 DT_EndianMemAddress(DAT_UINT64 val)
{
	DAT_UINT64 val64;

	if (DT_local_is_little_endian)
		return val;
	val64 = val;
	val64 = ((val64 & c1a64) << 8) | ((val64 & c1b64) >> 8);
	val64 = ((val64 & c2a64) << 16) | ((val64 & c2b64) >> 16);
	val64 = ((val64 & c3a64) << 32) | ((val64 & c3b64) >> 32);
	return val64;
}
