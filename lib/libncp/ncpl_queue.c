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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
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
 * $FreeBSD: src/lib/libncp/ncpl_queue.c,v 1.2 1999/10/31 03:39:03 bp Exp $
 *
 * NetWare queue interface
 *
 */
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <netncp/ncp_lib.h>

int
ncp_create_queue_job_and_file(NWCONN_HANDLE connid, u_int32_t queue_id, 
	struct queue_job *job)
{
	int error;
	DECLARE_RQ;

	ncp_init_request_s(conn, 121);
	ncp_add_dword_hl(conn, queue_id);
	ncp_add_mem(conn, &(job->j), sizeof(job->j));

	if ((error = ncp_request(connid, 23, conn)) != 0)
		return error;
	memcpy(&(job->j), ncp_reply_data(conn, 0), 78);
	ConvertToNWfromDWORD(job->j.JobFileHandle, &job->file_handle);
	return 0;
}

int
ncp_close_file_and_start_job(NWCONN_HANDLE connid, u_int32_t queue_id,
	struct queue_job *job)
{
	int error;
	DECLARE_RQ;

	ncp_init_request_s(conn, 127);
	ncp_add_dword_hl(conn, queue_id);
	ncp_add_dword_lh(conn, job->j.JobNumber);
	error = ncp_request(connid, 23, conn);
	return error;
}

int
ncp_attach_to_queue(NWCONN_HANDLE connid, u_int32_t queue_id) {
	int error;
	DECLARE_RQ;

	ncp_init_request_s(conn, 111);
	ncp_add_dword_hl(conn, queue_id);
	error = ncp_request(connid, 23, conn);
	return error;
}

int
ncp_detach_from_queue(NWCONN_HANDLE connid, u_int32_t queue_id) {
	int error;
	DECLARE_RQ;

	ncp_init_request_s(conn, 112);
	ncp_add_dword_hl(conn, queue_id);
	error= ncp_request(connid, 23, conn);
	return error;
}

int
ncp_service_queue_job(NWCONN_HANDLE connid, u_int32_t queue_id,
	u_int16_t job_type, struct queue_job *job)
{
	int error;
	DECLARE_RQ;

	ncp_init_request_s(conn, 124);
	ncp_add_dword_hl(conn, queue_id);
	ncp_add_word_hl(conn, job_type);
	if ((error = ncp_request(connid, 23, conn)) != 0) {
		return error;
	}
	memcpy(&(job->j), ncp_reply_data(conn, 0), 78);
	ConvertToNWfromDWORD(job->j.JobFileHandle, &job->file_handle);
	return error;
}

int
ncp_finish_servicing_job(NWCONN_HANDLE connid, u_int32_t queue_id,
	u_int32_t job_number, u_int32_t charge_info)
{
	int error;
	DECLARE_RQ;

	ncp_init_request_s(conn, 131);
	ncp_add_dword_hl(conn, queue_id);
	ncp_add_dword_lh(conn, job_number);
	ncp_add_dword_hl(conn, charge_info);

	error = ncp_request(connid, 23, conn);
	return error;
}

int
ncp_abort_servicing_job(NWCONN_HANDLE connid, u_int32_t queue_id,
	u_int32_t job_number)
{
	int error;
	DECLARE_RQ;

	ncp_init_request_s(conn, 132);
	ncp_add_dword_hl(conn, queue_id);
	ncp_add_dword_lh(conn, job_number);
	error = ncp_request(connid, 23, conn);
	return error;
}

int
ncp_get_queue_length(NWCONN_HANDLE connid, u_int32_t queue_id,
	u_int32_t *queue_length)
{
	int error;
	DECLARE_RQ;

        ncp_init_request_s(conn, 125);
        ncp_add_dword_hl(conn, queue_id);

	if ((error = ncp_request(connid, 23, conn)) != 0) 
		return error;
    	if (conn->rpsize < 12) {
        	ncp_printf("ncp_reply_size %d < 12\n", conn->rpsize);
        	return EINVAL;
	}
	if (ncp_reply_dword_hl(conn,0) != queue_id) {
        	printf("Ouch! Server didn't reply with same queue id in ncp_get_queue_length!\n");
        	return EINVAL;
	}
        *queue_length = ncp_reply_dword_lh(conn,8);
        return error;
}

int 
ncp_get_queue_job_ids(NWCONN_HANDLE connid, u_int32_t queue_id,
	u_int32_t queue_section, u_int32_t *length1, u_int32_t *length2,
	u_int32_t ids[])
{
	int error;
	DECLARE_RQ;

        ncp_init_request_s(conn,129);
        ncp_add_dword_hl(conn, queue_id);
        ncp_add_dword_lh(conn, queue_section);
        
        if ((error = ncp_request(connid, 23, conn)) != 0)
                return error;
        if (conn->rpsize < 8) {
                ncp_printf("ncp_reply_size %d < 8\n", conn->rpsize);
                return EINVAL;
        }
        *length2 = ncp_reply_dword_lh(conn,4);
        if (conn->rpsize < 8 + 4*(*length2)) {
                ncp_printf("ncp_reply_size %d < %d\n", conn->rpsize, 8+4*(*length2));
                return EINVAL;
        }
        if (ids) {
		int count = min(*length1, *length2)*sizeof(u_int32_t);
		int pos;

		for (pos=0; pos<count; pos+=sizeof(u_int32_t)) {
			*ids++ = ncp_reply_dword_lh(conn, 8+pos);
		}
	}
        *length1 = ncp_reply_dword_lh(conn,0);
        return error;
}

int
ncp_get_queue_job_info(NWCONN_HANDLE connid, u_int32_t queue_id,
	u_int32_t job_id, struct nw_queue_job_entry *jobdata)
{
        int error;
	DECLARE_RQ;

        ncp_init_request_s(conn,122);
        ncp_add_dword_hl(conn, queue_id);
        ncp_add_dword_lh(conn, job_id);

        if ((error = ncp_request(connid, 23, conn)) != 0)
                return error;

        if (conn->rpsize < sizeof(struct nw_queue_job_entry)) {
                ncp_printf("ncp_reply_size %d < %d\n", conn->rpsize,sizeof(struct nw_queue_job_entry));
                return EINVAL;
        }    
	memcpy(jobdata,ncp_reply_data(conn,0), sizeof(struct nw_queue_job_entry));
	return error;
}
