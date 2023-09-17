/*-
 * adv_data.c
 *
 * SPDX-License-Identifier: BSD-2-Clause

 * Copyright (c) 2020 Marc Veldman <marc@bumblingdork.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id$
 */

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <uuid.h>
#define L2CAP_SOCKET_CHECKED
#include <bluetooth.h>
#include "hccontrol.h"

static char* const adv_data2str(int len, uint8_t* data, char* buffer,
	int size);
static char* const adv_name2str(int len, uint8_t* advdata, char* buffer,
	int size);
static char* const adv_uuid2str(int datalen, uint8_t* data, char* buffer,
	int size);

void dump_adv_data(int len, uint8_t* advdata)
{
	int n=0;
	fprintf(stdout, "\tADV Data: ");
	for (n = 0; n < len+1; n++) {
		fprintf(stdout, "%02x ", advdata[n]);
	}
	fprintf(stdout, "\n");
}

void print_adv_data(int len, uint8_t* advdata)
{
	int n=0;
	while(n < len)
	{
		char buffer[2048];
		uint8_t datalen = advdata[n];
		uint8_t datatype = advdata[++n];
		/* Skip type */ 
		++n;
		datalen--;
		switch (datatype) {
			case 0x01:
				fprintf(stdout,
					"\tFlags: %s\n",
					adv_data2str(
						datalen,
						&advdata[n],
						buffer,
						sizeof(buffer)));
				break;
			case 0x02:
				fprintf(stdout,
					"\tIncomplete list of service"
					" class UUIDs (16-bit): %s\n",
					adv_data2str(
						datalen,
						&advdata[n],
						buffer,
						sizeof(buffer)));
				break;
			case 0x03:
				fprintf(stdout,
					"\tComplete list of service "
					"class UUIDs (16-bit): %s\n",
					adv_data2str(
						datalen,
						&advdata[n],
						buffer,
						sizeof(buffer)));
				break;
			case 0x07:
				fprintf(stdout,
					"\tComplete list of service "
					"class UUIDs (128 bit): %s\n",
					adv_uuid2str(
						datalen,
						&advdata[n],
						buffer,
						sizeof(buffer)));
				break;
			case 0x08:
				fprintf(stdout,
					"\tShortened local name: %s\n",
					adv_name2str(
						datalen,
						&advdata[n],
						buffer,
						sizeof(buffer)));
				break;
			case 0x09:
				fprintf(stdout,
					"\tComplete local name: %s\n",
					adv_name2str(
						datalen,
						&advdata[n],
						buffer,
						sizeof(buffer)));
				break;
			case 0x0a:
				fprintf(stdout,
					"\tTx Power level: %d dBm\n",
						(int8_t)advdata[n]);
				break;
			case 0x0d:
				fprintf(stdout,
					"\tClass of device: %s\n",
					adv_data2str(
						datalen,
						&advdata[n],
						buffer,
						sizeof(buffer)));
				break;
			case 0x16:
				fprintf(stdout,
					"\tService data: %s\n",
					adv_data2str(
						datalen,
						&advdata[n],
						buffer,
						sizeof(buffer)));
				break;
			case 0x19:
				fprintf(stdout,
					"\tAppearance: %s\n",
					adv_data2str(
						datalen,
						&advdata[n],
						buffer,
						sizeof(buffer)));
				break;
			case 0xff:
				fprintf(stdout,
					"\tManufacturer: %s\n",
			       		hci_manufacturer2str(
						advdata[n]|advdata[n+1]<<8));
				fprintf(stdout,
					"\tManufacturer specific data: %s\n",
					adv_data2str(
						datalen-2,
						&advdata[n+2],
						buffer,
						sizeof(buffer)));
				break;
			default:
				fprintf(stdout,
					"\tUNKNOWN datatype: %02x data %s\n",
					datatype,
					adv_data2str(
						datalen,
						&advdata[n],
						buffer,
						sizeof(buffer)));
		}
		n += datalen;
	}
}

static char* const adv_data2str(int datalen, uint8_t* data, char* buffer,
	int size)
{
        int i = 0;
	char tmpbuf[5];

	if (buffer == NULL)
		return NULL;

	memset(buffer, 0, size);

	while(i < datalen) {
		(void)snprintf(tmpbuf, sizeof(tmpbuf), "%02x ", data[i]);
		/* Check if buffer is full */
		if (strlcat(buffer, tmpbuf, size) > size)
			break;
		i++;
	}
	return buffer;
}

static char* const adv_name2str(int datalen, uint8_t* data, char* buffer,
	int size)
{
	if (buffer == NULL)
		return NULL;

	memset(buffer, 0, size);

	(void)strlcpy(buffer, (char*)data, datalen+1);
	return buffer;
}

static char* const adv_uuid2str(int datalen, uint8_t* data, char* buffer,
	int size)
{
	int i;
	uuid_t uuid;
	uint32_t ustatus;
	char* tmpstr;

	if (buffer == NULL)
		return NULL;

	memset(buffer, 0, size);
	if (datalen < 16)
		return buffer;
	uuid.time_low = le32dec(data+12);
	uuid.time_mid = le16dec(data+10);
	uuid.time_hi_and_version = le16dec(data+8);
	uuid.clock_seq_hi_and_reserved = data[7];
	uuid.clock_seq_low = data[6];
	for(i = 0; i < _UUID_NODE_LEN; i++){
		uuid.node[i] = data[5 - i];
	}
	uuid_to_string(&uuid, &tmpstr, &ustatus);
	if(ustatus == uuid_s_ok) {
		strlcpy(buffer, tmpstr, size);
	}
	free(tmpstr);
			
	return buffer;
}
