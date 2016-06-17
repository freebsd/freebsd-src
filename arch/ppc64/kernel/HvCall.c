/*
 * HvCall.c
 * Copyright (C) 2001  Mike Corrigan IBM Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/stddef.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <asm/system.h>
#include <asm/page.h>
#include <asm/iSeries/HvCall.h>
#ifndef  _HVCALLSC_H
#include <asm/iSeries/HvCallSc.h>
#endif
#include <asm/iSeries/LparData.h>

#ifndef  _HVTYPES_H
#include <asm/iSeries/HvTypes.h>
#endif


/*=====================================================================
 * Note that this call takes at MOST one page worth of data
 */
int HvCall_readLogBuffer(HvLpIndex lpIndex, void *buffer, u64 bufLen)
{
	struct HvLpBufferList *bufList;
	u64 bytesLeft = bufLen;
	u64 leftThisPage;
	u64 curPtr = virt_to_absolute( (unsigned long) buffer );
	u64 retVal;
	int npages;
	int i;

	npages = 0;
	while (bytesLeft) {
		npages++;
		leftThisPage = ((curPtr & PAGE_MASK) + PAGE_SIZE) - curPtr;

		if (leftThisPage > bytesLeft)
			bytesLeft = 0;
		else
			bytesLeft -= leftThisPage;

		curPtr = (curPtr & PAGE_MASK) + PAGE_SIZE;
	}

	if (npages == 0)
		return 0;

	bufList = (struct HvLpBufferList *)
		kmalloc(npages * sizeof(struct HvLpBufferList), GFP_ATOMIC);
	bytesLeft = bufLen;
	curPtr = virt_to_absolute( (unsigned long) buffer );
	for(i=0; i<npages; i++) {
		bufList[i].addr = curPtr;

		leftThisPage = ((curPtr & PAGE_MASK) + PAGE_SIZE) - curPtr;

		if (leftThisPage > bytesLeft) {
			bufList[i].len = bytesLeft;
			bytesLeft = 0;
		} else {
			bufList[i].len = leftThisPage;
			bytesLeft -= leftThisPage;
		}

		curPtr = (curPtr & PAGE_MASK) + PAGE_SIZE;
	}


	retVal = HvCall3(HvCallBaseReadLogBuffer, lpIndex,
			 virt_to_absolute((unsigned long)bufList), bufLen);

	kfree(bufList);

	return (int)retVal;
}

/*=====================================================================
 */
void HvCall_writeLogBuffer(const void *buffer, u64 bufLen)
{
	struct HvLpBufferList bufList;
	u64 bytesLeft = bufLen;
	u64 leftThisPage;
	u64 curPtr = virt_to_absolute((unsigned long) buffer);

	while (bytesLeft) {
		bufList.addr = curPtr;

		leftThisPage = ((curPtr & PAGE_MASK) + PAGE_SIZE) - curPtr;

		if (leftThisPage > bytesLeft) {
			bufList.len = bytesLeft;
			bytesLeft = 0;
		} else {
			bufList.len = leftThisPage;
			bytesLeft -= leftThisPage;
		}


		HvCall2(HvCallBaseWriteLogBuffer,
			virt_to_absolute((unsigned long) &bufList),
			bufList.len);

		curPtr = (curPtr & PAGE_MASK) + PAGE_SIZE;
	}
}
