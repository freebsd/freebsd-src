/*
 * Copyright (c) 1999, Boris Popov
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * calls that don't fit to any other category
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include <netncp/ncp_lib.h>

static time_t
ncp_nw_to_ctime(struct nw_time_buffer *source) {
	struct tm u_time;

	bzero(&u_time,sizeof(struct tm));
	/*
	 * XXX: NW 4.x tracks daylight automatically
	 */
	u_time.tm_isdst = -1;
	u_time.tm_sec = source->second;
	u_time.tm_min = source->minute;
	u_time.tm_hour = source->hour;
	u_time.tm_mday = source->day;
	u_time.tm_mon = source->month - 1;
	u_time.tm_year = source->year;

	if (u_time.tm_year < 80) {
		u_time.tm_year += 100;
	}
	return mktime(&u_time);
}

int
ncp_get_file_server_information(NWCONN_HANDLE connid,
	struct ncp_file_server_info *target)
{
	int error;
	DECLARE_RQ;

	ncp_init_request_s(conn, 17);
	if ((error = ncp_request(connid, 23, conn)) != 0) 
		return error;
	memcpy(target, ncp_reply_data(conn, 0), sizeof(*target));
	target->MaximumServiceConnections
	    = htons(target->MaximumServiceConnections);
	target->ConnectionsInUse
	    = htons(target->ConnectionsInUse);
	target->MaxConnectionsEverUsed
	    = htons(target->MaxConnectionsEverUsed);
	target->NumberMountedVolumes
	    = htons(target->NumberMountedVolumes);
	return 0;
}

int
ncp_get_stations_logged_info(NWCONN_HANDLE connid, u_int32_t connection,
	struct ncp_bindery_object *target, time_t *login_time)
{
	int error;
	DECLARE_RQ;

	ncp_init_request_s(conn, 28);
	ncp_add_dword_lh(conn, connection);

	if ((error = ncp_request(connid, 23, conn)) != 0)
		return error;
	bzero(target, sizeof(*target));
	target->object_id = ncp_reply_dword_hl(conn, 0);
	target->object_type = ncp_reply_word_hl(conn, 4);
	memcpy(target->object_name, ncp_reply_data(conn, 6),
	       sizeof(target->object_name));
	*login_time = ncp_nw_to_ctime((struct nw_time_buffer *)ncp_reply_data(conn, 54));
	return 0;
}

int
ncp_get_internet_address(NWCONN_HANDLE connid, u_int32_t connection,
	struct ipx_addr *target, u_int8_t * conn_type)
{
	int error;
	DECLARE_RQ;

	ncp_init_request_s(conn, 26);
	ncp_add_dword_lh(conn, connection);
	error = ncp_request(connid, 23, conn);
	if (error) return error;
	bzero(target, sizeof(*target));
	ipx_netlong(*target) = ncp_reply_dword_lh(conn, 0);
	memcpy(&(target->x_host), ncp_reply_data(conn, 4), 6);
	target->x_port = ncp_reply_word_lh(conn, 10);
	*conn_type = ncp_reply_byte(conn, 12);
	return 0;
}

NWCCODE
NWGetObjectConnectionNumbers(NWCONN_HANDLE connHandle,
		pnstr8 pObjName, nuint16 objType,
		pnuint16 pNumConns, pnuint16 pConnHandleList,
		nuint16 maxConns) 
{
	int error, i, n;
	nuint32 lastconn;
	DECLARE_RQ;

	lastconn = 0;
	ncp_init_request_s(conn, 27);
	ncp_add_dword_lh(conn, lastconn);
	ncp_add_word_hl(conn, objType);
	ncp_add_pstring(conn, pObjName);
	if ((error = ncp_request(connHandle, 23, conn)) != 0) return error;
	n = min(ncp_reply_byte(conn, 0), maxConns);
	*pNumConns = n;
	for (i = 0; i < n ; i++) {
		*pConnHandleList++ = ncp_reply_dword_lh(conn, i * 4 + 1);
	}
	return 0;
}

void
NWUnpackDateTime(nuint32 dateTime, NW_DATE *sDate, NW_TIME *sTime) {
	NWUnpackDate(dateTime >> 16, sDate);
	NWUnpackTime(dateTime & 0xffff, sTime);
}

void
NWUnpackDate(nuint16 date, NW_DATE *sDate) {
	sDate->day = date & 0x1f;
	sDate->month = (date >> 5) & 0xf;
	sDate->year = ((date >> 9) & 0x7f) + 1980;
}

void
NWUnpackTime(nuint16 time, NW_TIME *sTime) {
	sTime->seconds = time & 0x1f;
	sTime->minutes = (time >> 5) & 0x3f;
	sTime->hours = (time >> 11) & 0x1f;
}

nuint32
NWPackDateTime(NW_DATE *sDate, NW_TIME *sTime) {
	return 0;
}

nuint16
NWPackDate(NW_DATE *sDate) {
	return 0;
}

nuint16
NWPackTime(NW_TIME *sTime) {
	return 0;
}

time_t
ncp_UnpackDateTime(nuint32 dateTime) {
	struct tm u_time;
	NW_DATE d;
	NW_TIME t;

	NWUnpackDateTime(dateTime, &d, &t);
	bzero(&u_time,sizeof(struct tm));
	u_time.tm_isdst = -1;
	u_time.tm_sec = t.seconds;
	u_time.tm_min = t.minutes;
	u_time.tm_hour = t.hours;
	u_time.tm_mday = d.day;
	u_time.tm_mon = d.month - 1;
	u_time.tm_year = d.year - 1900;

	return mktime(&u_time);
}

int
ncp_GetFileServerDateAndTime(NWCONN_HANDLE cH, time_t *target) {
	int error;
	DECLARE_RQ;

	ncp_init_request(conn);
	if ((error = ncp_request(cH, 20, conn)) != 0)
		return error;
	*target = ncp_nw_to_ctime((struct nw_time_buffer *) ncp_reply_data(conn, 0));
	return 0;
}

int
ncp_SetFileServerDateAndTime(NWCONN_HANDLE cH, time_t * source) {
	int year;
	struct tm *utime = localtime(source);
	DECLARE_RQ;

	year = utime->tm_year;
	if (year > 99) {
		year -= 100;
	}
	ncp_init_request_s(conn, 202);
	ncp_add_byte(conn, year);
	ncp_add_byte(conn, utime->tm_mon + 1);
	ncp_add_byte(conn, utime->tm_mday);
	ncp_add_byte(conn, utime->tm_hour);
	ncp_add_byte(conn, utime->tm_min);
	ncp_add_byte(conn, utime->tm_sec);
	return ncp_request(cH, 23, conn);
}

NWCCODE
NWDownFileServer(NWCONN_HANDLE cH, int force) {
	DECLARE_RQ;

	ncp_init_request_s(conn, 211);
	ncp_add_byte(conn, force ? 0 : 0xff);
	return ncp_request(cH, 23, conn);
}

NWCCODE
NWCloseBindery(NWCONN_HANDLE cH) {
	DECLARE_RQ;

	ncp_init_request_s(conn, 68);
	return ncp_request(cH, 23, conn);
}

NWCCODE
NWOpenBindery(NWCONN_HANDLE cH) {
	DECLARE_RQ;

	ncp_init_request_s(conn, 69);
	return ncp_request(cH, 23, conn);
}

NWCCODE
NWDisableTTS(NWCONN_HANDLE cH) {
	DECLARE_RQ;

	ncp_init_request_s(conn, 207);
	return ncp_request(cH, 23, conn);
}

NWCCODE
NWEnableTTS(NWCONN_HANDLE cH) {
	DECLARE_RQ;

	ncp_init_request_s(conn, 208);
	return ncp_request(cH, 23, conn);
}

NWCCODE
NWDisableFileServerLogin(NWCONN_HANDLE cH) {
	DECLARE_RQ;

	ncp_init_request_s(conn, 203);
	return ncp_request(cH, 23, conn);
}

NWCCODE
NWEnableFileServerLogin(NWCONN_HANDLE cH) {
	DECLARE_RQ;

	ncp_init_request_s(conn, 204);
	return ncp_request(cH, 23, conn);
}
