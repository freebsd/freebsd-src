/*-
 * Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2026 Broadcom Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BNXT_LOG_H
#define BNXT_LOG_H

#define BNXT_LOGGER_L2         1
#define BNXT_LOGGER_ROCE       2

#define BNXT_LOG_MSG_SIZE	256
#define BNXT_LOG_NUM_BUFFERS(x)	((x) / BNXT_LOG_MSG_SIZE)

struct bnxt_logger {
	TAILQ_ENTRY(bnxt_logger) list;
	uint16_t logger_id;
	uint32_t buffer_size;
	uint16_t head;
	uint16_t tail;
	bool valid;
	void *msgs;
	uint32_t live_max_size;
	void *live_msgs;
	uint32_t max_live_buff_size;
	uint32_t live_msgs_len;
	void (*log_live_op)(void *dev);
};

struct bnxt_coredump_segment_hdr {
	uint8_t  signature[4];
	uint32_t component_id;
	uint32_t segment_id;
	uint32_t flags;
	uint8_t  low_version;
	uint8_t  high_version;
	uint16_t function_id;
	uint32_t offset;
	uint32_t length;
	uint32_t status;
	uint32_t duration;
	uint32_t data_offset;
	uint32_t instance;
	uint32_t rsvd[5];
};

struct bnxt_coredump_record {
	uint8_t  signature[4];
	uint32_t flags;
	uint8_t  low_version;
	uint8_t  high_version;
	uint8_t  asic_state;
	uint8_t  rsvd0[5];
	char     system_name[32];
	uint16_t year;
	uint16_t month;
	uint16_t day;
	uint16_t hour;
	uint16_t minute;
	uint16_t second;
	uint16_t utc_bias;
	uint16_t rsvd1;
	char     commandline[256];
	uint32_t total_segments;
	uint32_t os_ver_major;
	uint32_t os_ver_minor;
	uint32_t rsvd2;
	char     os_name[32];
	uint16_t end_year;
	uint16_t end_month;
	uint16_t end_day;
	uint16_t end_hour;
	uint16_t end_minute;
	uint16_t end_second;
	uint16_t end_utc_bias;
	uint32_t asic_id1;
	uint32_t asic_id2;
	uint32_t coredump_status;
	uint8_t  ioctl_low_version;
	uint8_t  ioctl_high_version;
	uint16_t rsvd3[313];
};

int bnxt_register_logger(struct bnxt_softc *bp, uint16_t logger_id,
				uint32_t num_buffers, void (*log_live)(void *),
			       	uint32_t live_size);
void bnxt_unregister_logger(struct bnxt_softc *bp, int logger_id);
void bnxt_log_add_msg(struct bnxt_softc *bp, uint16_t logger_id,
				const char *format, ...);
void bnxt_log_live(struct bnxt_softc *bp, uint16_t logger_id,
				const char *format, ...);
void bnxt_reset_loggers(struct bnxt_softc *bp);
size_t bnxt_get_loggers_coredump_size(struct bnxt_softc *bp);
int  bnxt_start_logging_driver_coredump(struct bnxt_softc *bp, char *dest_buf);
void bnxt_fill_coredump_seg_hdr(struct bnxt_softc *bp,
				struct bnxt_coredump_segment_hdr *seg_hdr,
				struct coredump_segment_record *seg_rec,
				uint32_t seg_len, int status,
			       	uint32_t duration, uint32_t instance,
			       	uint32_t comp_id, uint32_t seg_id);
#endif
