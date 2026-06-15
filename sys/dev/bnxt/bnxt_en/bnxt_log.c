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

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/systm.h>

#include "bnxt.h"
#include "bnxt_log.h"

MALLOC_DEFINE(M_BNXT_LOG, "bnxt_log_buffer", "buffer for bnxt logging module");

void
bnxt_fill_coredump_seg_hdr(struct bnxt_softc *bp,
			   struct bnxt_coredump_segment_hdr *seg_hdr,
			   struct coredump_segment_record *seg_rec,
			   uint32_t seg_len, int status, uint32_t duration,
			   uint32_t instance, uint32_t comp_id, uint32_t seg_id)
{
	memset(seg_hdr, 0, sizeof(*seg_hdr));
	memcpy(seg_hdr->signature, "sEgM", 4);
	if (seg_rec) {
		seg_hdr->component_id =
		    htole32((uint32_t)seg_rec->component_id);
		seg_hdr->segment_id = htole32((uint32_t)seg_rec->segment_id);
		seg_hdr->low_version = seg_rec->version_low;
		seg_hdr->high_version = seg_rec->version_hi;
		seg_hdr->flags = seg_rec->compress_flags;
	} else {
		seg_hdr->component_id = htole32(comp_id);
		seg_hdr->segment_id = htole32(seg_id);
	}
	seg_hdr->function_id =  htole16(bp->func.fw_fid);
	seg_hdr->length = htole32(seg_len);
	seg_hdr->status = htole32((uint32_t)status);
	seg_hdr->duration = htole32(duration);
	seg_hdr->data_offset = htole32(sizeof(*seg_hdr));
	seg_hdr->instance = htole32(instance);
}

int
bnxt_register_logger(struct bnxt_softc *bp, uint16_t logger_id,
		     uint32_t num_buffs, void (*log_live)(void *),
		     uint32_t live_max_size)
{
	struct bnxt_logger *logger;
	void *data;

	if (!log_live || !live_max_size)
		return (EINVAL);

	if (num_buffs == 0 || (num_buffs & (num_buffs - 1)) != 0)
		return (EINVAL);

	logger = malloc(sizeof(*logger), M_BNXT_LOG, M_WAITOK | M_ZERO);

	logger->logger_id = logger_id;
	logger->buffer_size = num_buffs * BNXT_LOG_MSG_SIZE;
	logger->log_live_op = log_live;
	logger->max_live_buff_size = live_max_size;

	data = malloc(logger->buffer_size, M_BNXT_LOG, M_WAITOK);
	logger->msgs = data;

	mtx_lock(&bp->log_lock);
	TAILQ_INSERT_TAIL(&bp->loggers_list, logger, list);
	mtx_unlock(&bp->log_lock);
	return (0);
}

void
bnxt_unregister_logger(struct bnxt_softc *bp, int logger_id)
{
	struct bnxt_logger *l = NULL, *tmp;

	mtx_lock(&bp->log_lock);
	TAILQ_FOREACH_SAFE(l, &bp->loggers_list, list, tmp) {
		if (l->logger_id == logger_id) {
			TAILQ_REMOVE(&bp->loggers_list, l, list);
			break;
		}
	}
	mtx_unlock(&bp->log_lock);

	if (!l) {
		device_printf(bp->dev, "logger id %d not registered\n",
			      logger_id);
		return;
	}

	free(l->msgs, M_BNXT_LOG);
	free(l, M_BNXT_LOG);
}

static int
bnxt_log_info(char *buf, size_t max_len, const char *format, va_list args)
{
	static char textbuf[BNXT_LOG_MSG_SIZE];
	char *text = textbuf;
	size_t text_len;
	char *next;

	text_len = vsnprintf(text, sizeof(textbuf), format, args);
	if (text_len >= sizeof(textbuf))
		text_len = sizeof(textbuf) - 1;

	next = memchr(text, '\n', text_len);
	if (next)
		text_len = next - text;
	else if (text[text_len] == '\0')
		text[text_len] = '\n';

	if (text_len > max_len) {
		/* Truncate */
		text_len = max_len;
		text[text_len] = '\n';
	}

	memcpy(buf, text, text_len + 1);

	return (text_len + 1);
}

void
bnxt_log_add_msg(struct bnxt_softc *bp, uint16_t logger_id,
    const char *format, ...)
{
	struct bnxt_logger *logger = NULL, *tmp;
	uint16_t start, tail;
	va_list args;
	void *buf;
	uint32_t mask;

	mtx_lock(&bp->log_lock);
	TAILQ_FOREACH_SAFE(logger, &bp->loggers_list, list, tmp) {
		if (logger->logger_id == logger_id)
			break;
	}

	if (!logger) {
		mtx_unlock(&bp->log_lock);
		return;
	}

	mask = BNXT_LOG_NUM_BUFFERS(logger->buffer_size) - 1;
	tail = logger->tail;
	start = logger->head;

	if (logger->valid && start == tail)
		logger->head = ++start & mask;

	buf = (uint8_t *)logger->msgs + BNXT_LOG_MSG_SIZE * logger->tail;
	logger->tail = ++tail & mask;

	if (!logger->valid)
		logger->valid = true;

	va_start(args, format);
	bnxt_log_info(buf, BNXT_LOG_MSG_SIZE, format, args);
	va_end(args);
	mtx_unlock(&bp->log_lock);
}

void
bnxt_log_live(struct bnxt_softc *bp, uint16_t logger_id,
    const char *format, ...)
{
	struct bnxt_logger *logger = NULL, *tmp;
	va_list args;
	int len;

	TAILQ_FOREACH_SAFE(logger, &bp->loggers_list, list, tmp) {
		if (logger->logger_id == logger_id)
			break;
	}

	if (!logger || !logger->live_msgs)
		return;

	va_start(args, format);
	len = bnxt_log_info(
	    (uint8_t *)logger->live_msgs + logger->live_msgs_len,
	    logger->max_live_buff_size - logger->live_msgs_len,
	    format, args);
	va_end(args);

	logger->live_msgs_len += len;
}

static size_t
bnxt_get_data_len(char *buf)
{
	size_t count = 0;

	while (*buf++ != '\n')
		count++;
	return (count + 1);
}

static size_t
bnxt_collect_logs_buffer(struct bnxt_logger *logger, char *dest)
{
	uint32_t mask = BNXT_LOG_NUM_BUFFERS(logger->buffer_size) - 1;
	uint16_t head = logger->head;
	uint16_t tail = logger->tail;
	size_t total_len = 0;
	int count;

	if (!logger->valid)
		return (0);

	count = (tail > head) ? (tail - head) : (tail - head + mask + 1);
	while (count--) {
		void *src = (uint8_t *)logger->msgs +
		    BNXT_LOG_MSG_SIZE * (head & mask);
		size_t len;

		len = bnxt_get_data_len(src);
		memcpy(dest + total_len, src, len);
		total_len += len;
		head++;
	}

	return (total_len);
}

size_t
bnxt_get_loggers_coredump_size(struct bnxt_softc *bp)
{
	struct bnxt_logger *logger, *tmp;
	size_t len = 0;

	mtx_lock(&bp->log_lock);
	TAILQ_FOREACH_SAFE(logger, &bp->loggers_list, list, tmp) {
		len += sizeof(struct bnxt_coredump_segment_hdr) +
		       logger->max_live_buff_size + logger->buffer_size;
	}
	mtx_unlock(&bp->log_lock);
	return (len);
}

int
bnxt_start_logging_driver_coredump(struct bnxt_softc *bp, char *dest_buf)
{
	struct bnxt_logger *logger, *tmp;
	size_t offset = 0;
	uint32_t seg_id = 0;

	if (!dest_buf)
		return (0);

	mtx_lock(&bp->log_lock);
	TAILQ_FOREACH_SAFE(logger, &bp->loggers_list, list, tmp) {
		struct bnxt_coredump_segment_hdr seg_hdr;
		void *seg_hdr_dest = dest_buf + offset;
		size_t len;

		offset += sizeof(seg_hdr);
		/* First collect logs from buffer */
		len = bnxt_collect_logs_buffer(logger, dest_buf + offset);
		offset += len;
		/* Let logger to collect live messages */
		logger->live_msgs = dest_buf + offset;
		logger->live_msgs_len = 0;
		logger->log_live_op(bp);

		len += logger->buffer_size;
		offset += logger->buffer_size;

		bnxt_fill_coredump_seg_hdr(bp, &seg_hdr, NULL, len,
					   0, 0, 0, 13, seg_id);
		memcpy(seg_hdr_dest, &seg_hdr, sizeof(seg_hdr));
		seg_id++;
	}
	mtx_unlock(&bp->log_lock);
	return (offset);
}

void
bnxt_reset_loggers(struct bnxt_softc *bp)
{
	struct bnxt_logger *logger, *tmp;

	mtx_lock(&bp->log_lock);
	TAILQ_FOREACH_SAFE(logger, &bp->loggers_list, list, tmp) {
		logger->head = 0;
		logger->tail = 0;
		logger->valid = false;
	}
	mtx_unlock(&bp->log_lock);
}
