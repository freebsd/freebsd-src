/*
 * Copyright (c) 2019, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "ptunit.h"

#include "pt_packet_decoder.h"

#include "intel-pt.h"


/* A test fixture providing a decoder operating on a small buffer. */
struct test_fixture {
	/* The packet_decoder. */
	struct pt_packet_decoder decoder;

	/* The configuration. */
	struct pt_config config;

	/* The buffer it operates on. */
	uint8_t buffer[24];

	/* The test fixture initialization and finalization functions. */
	struct ptunit_result (*init)(struct test_fixture *tfix);
	struct ptunit_result (*fini)(struct test_fixture *tfix);
};

static struct ptunit_result tfix_init(struct test_fixture *tfix)
{
	struct pt_config *config;
	uint8_t *buffer;
	int errcode;

	config = &tfix->config;
	buffer = tfix->buffer;

	memset(buffer, 0, sizeof(tfix->buffer));

	pt_config_init(config);
	config->begin = buffer;
	config->end = buffer + sizeof(tfix->buffer);

	errcode = pt_pkt_decoder_init(&tfix->decoder, config);
	ptu_int_eq(errcode, 0);

	return ptu_passed();
}

static struct ptunit_result decoder_init_null(void)
{
	struct pt_packet_decoder decoder;
	struct pt_config config;
	int errcode;

	errcode = pt_pkt_decoder_init(NULL, &config);
	ptu_int_eq(errcode, -pte_invalid);

	errcode = pt_pkt_decoder_init(&decoder, NULL);
	ptu_int_eq(errcode, -pte_invalid);

	return ptu_passed();
}

static struct ptunit_result decoder_fini_null(void)
{
	pt_pkt_decoder_fini(NULL);

	return ptu_passed();
}

static struct ptunit_result alloc_decoder_null(void)
{
	struct pt_packet_decoder *decoder;

	decoder = pt_pkt_alloc_decoder(NULL);
	ptu_null(decoder);

	return ptu_passed();
}

static struct ptunit_result free_decoder_null(void)
{
	pt_pkt_free_decoder(NULL);

	return ptu_passed();
}

static struct ptunit_result sync_forward_null(void)
{
	int errcode;

	errcode = pt_pkt_sync_forward(NULL);
	ptu_int_eq(errcode, -pte_invalid);

	return ptu_passed();
}

static struct ptunit_result sync_backward_null(void)
{
	int errcode;

	errcode = pt_pkt_sync_backward(NULL);
	ptu_int_eq(errcode, -pte_invalid);

	return ptu_passed();
}

static struct ptunit_result sync_set_null(void)
{
	int errcode;

	errcode = pt_pkt_sync_set(NULL, 0ull);
	ptu_int_eq(errcode, -pte_invalid);

	return ptu_passed();
}

static struct ptunit_result sync_set_eos(struct test_fixture *tfix)
{
	int errcode;

	errcode = pt_pkt_sync_set(&tfix->decoder, sizeof(tfix->buffer) + 1);
	ptu_int_eq(errcode, -pte_eos);

	return ptu_passed();
}

static struct ptunit_result get_offset_null(void)
{
	struct pt_packet_decoder decoder;
	uint64_t offset;
	int errcode;

	errcode = pt_pkt_get_offset(NULL, &offset);
	ptu_int_eq(errcode, -pte_invalid);

	errcode = pt_pkt_get_offset(&decoder, NULL);
	ptu_int_eq(errcode, -pte_invalid);

	return ptu_passed();
}

static struct ptunit_result get_offset_init(struct test_fixture *tfix)
{
	uint64_t offset;
	int errcode;

	errcode = pt_pkt_get_offset(&tfix->decoder, &offset);
	ptu_int_eq(errcode, -pte_nosync);

	return ptu_passed();
}

static struct ptunit_result sync_set_get_offset(struct test_fixture *tfix)
{
	uint64_t offset;
	int errcode;

	errcode = pt_pkt_sync_set(&tfix->decoder, 1ull);
	ptu_int_eq(errcode, 0);

	errcode = pt_pkt_get_offset(&tfix->decoder, &offset);
	ptu_int_eq(errcode, 0);
	ptu_uint_eq(offset, 1ull);

	return ptu_passed();
}

static struct ptunit_result get_sync_offset_null(void)
{
	struct pt_packet_decoder decoder;
	uint64_t offset;
	int errcode;

	errcode = pt_pkt_get_sync_offset(NULL, &offset);
	ptu_int_eq(errcode, -pte_invalid);

	errcode = pt_pkt_get_sync_offset(&decoder, NULL);
	ptu_int_eq(errcode, -pte_invalid);

	return ptu_passed();
}

static struct ptunit_result get_config_null(void)
{
	const struct pt_config *config;

	config = pt_pkt_get_config(NULL);
	ptu_null(config);

	return ptu_passed();
}

static struct ptunit_result get_config(struct test_fixture *tfix)
{
	const struct pt_config *config;

	config = pt_pkt_get_config(&tfix->decoder);
	ptu_ptr(config);

	return ptu_passed();
}

static struct ptunit_result next_null(void)
{
	struct pt_packet_decoder decoder;
	struct pt_packet packet;
	int errcode;

	errcode = pt_pkt_next(NULL, &packet, sizeof(packet));
	ptu_int_eq(errcode, -pte_invalid);

	errcode = pt_pkt_next(&decoder, NULL, sizeof(packet));
	ptu_int_eq(errcode, -pte_invalid);

	return ptu_passed();
}

int main(int argc, char **argv)
{
	struct test_fixture tfix;
	struct ptunit_suite suite;

	tfix.init = tfix_init;
	tfix.fini = NULL;

	suite = ptunit_mk_suite(argc, argv);

	ptu_run(suite, decoder_init_null);
	ptu_run(suite, decoder_fini_null);
	ptu_run(suite, alloc_decoder_null);
	ptu_run(suite, free_decoder_null);

	ptu_run(suite, sync_forward_null);
	ptu_run(suite, sync_backward_null);
	ptu_run(suite, sync_set_null);
	ptu_run_f(suite, sync_set_eos, tfix);

	ptu_run(suite, get_offset_null);
	ptu_run_f(suite, get_offset_init, tfix);
	ptu_run_f(suite, sync_set_get_offset, tfix);
	ptu_run(suite, get_sync_offset_null);

	ptu_run(suite, get_config_null);
	ptu_run_f(suite, get_config, tfix);

	ptu_run(suite, next_null);

	return ptunit_report(&suite);
}
