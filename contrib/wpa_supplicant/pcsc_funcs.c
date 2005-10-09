/*
 * WPA Supplicant / PC/SC smartcard interface for USIM, GSM SIM
 * Copyright (c) 2004-2005, Jouni Malinen <jkmaline@cc.hut.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <winscard.h>

#include "common.h"
#include "wpa_supplicant.h"
#include "pcsc_funcs.h"


/* See ETSI GSM 11.11 and ETSI TS 102 221 for details.
 * SIM commands:
 * Command APDU: CLA INS P1 P2 P3 Data
 *   CLA (class of instruction): A0 for GSM, 00 for USIM
 *   INS (instruction)
 *   P1 P2 P3 (parameters, P3 = length of Data)
 * Response APDU: Data SW1 SW2
 *   SW1 SW2 (Status words)
 * Commands (INS P1 P2 P3):
 *   SELECT: A4 00 00 02 <file_id, 2 bytes>
 *   GET RESPONSE: C0 00 00 <len>
 *   RUN GSM ALG: 88 00 00 00 <RAND len = 10>
 *   RUN UMTS ALG: 88 00 81 <len=0x22> data: 0x10 | RAND | 0x10 | AUTN
 *	P1 = ID of alg in card
 *	P2 = ID of secret key
 *   READ BINARY: B0 <offset high> <offset low> <len>
 *   VERIFY CHV: 20 00 <CHV number> 08
 *   CHANGE CHV: 24 00 <CHV number> 10
 *   DISABLE CHV: 26 00 01 08
 *   ENABLE CHV: 28 00 01 08
 *   UNBLOCK CHV: 2C 00 <00=CHV1, 02=CHV2> 10
 *   SLEEP: FA 00 00 00
 */

/* GSM SIM commands */
#define SIM_CMD_SELECT			0xa0, 0xa4, 0x00, 0x00, 0x02
#define SIM_CMD_RUN_GSM_ALG		0xa0, 0x88, 0x00, 0x00, 0x10
#define SIM_CMD_GET_RESPONSE		0xa0, 0xc0, 0x00, 0x00
#define SIM_CMD_READ_BIN		0xa0, 0xb0, 0x00, 0x00
#define SIM_CMD_VERIFY_CHV1		0xa0, 0x20, 0x00, 0x01, 0x08

/* USIM commands */
#define USIM_CLA			0x00
#define USIM_CMD_RUN_UMTS_ALG		0x00, 0x88, 0x00, 0x81, 0x22
#define USIM_CMD_GET_RESPONSE		0x00, 0xc0, 0x00, 0x00

#define USIM_FSP_TEMPL_TAG		0x62

#define USIM_TLV_FILE_DESC		0x82
#define USIM_TLV_FILE_ID		0x83
#define USIM_TLV_DF_NAME		0x84
#define USIM_TLV_PROPR_INFO		0xA5
#define USIM_TLV_LIFE_CYCLE_STATUS	0x8A
#define USIM_TLV_FILE_SIZE		0x80
#define USIM_TLV_TOTAL_FILE_SIZE	0x81
#define USIM_TLV_PIN_STATUS_TEMPLATE	0xC6
#define USIM_TLV_SHORT_FILE_ID		0x88

#define USIM_PS_DO_TAG			0x90

#define AKA_RAND_LEN 16
#define AKA_AUTN_LEN 16
#define AKA_AUTS_LEN 14
#define RES_MAX_LEN 16
#define IK_LEN 16
#define CK_LEN 16


typedef enum { SCARD_GSM_SIM, SCARD_USIM } sim_types;

struct scard_data {
	long ctx;
	long card;
	unsigned long protocol;
	SCARD_IO_REQUEST recv_pci;
	sim_types sim_type;
};


static int _scard_select_file(struct scard_data *scard, unsigned short file_id,
			      unsigned char *buf, size_t *buf_len,
			      sim_types sim_type, unsigned char *aid);
static int scard_select_file(struct scard_data *scard, unsigned short file_id,
			     unsigned char *buf, size_t *buf_len);
static int scard_verify_pin(struct scard_data *scard, char *pin);


static int scard_parse_fsp_templ(unsigned char *buf, size_t buf_len,
				 int *ps_do, int *file_len)
{
		unsigned char *pos, *end;

		if (ps_do)
			*ps_do = -1;
		if (file_len)
			*file_len = -1;

		pos = buf;
		end = pos + buf_len;
		if (*pos != USIM_FSP_TEMPL_TAG) {
			wpa_printf(MSG_DEBUG, "SCARD: file header did not "
				   "start with FSP template tag");
			return -1;
		}
		pos++;
		if (pos >= end)
			return -1;
		if ((pos + pos[0]) < end)
			end = pos + 1 + pos[0];
		pos++;
		wpa_hexdump(MSG_DEBUG, "SCARD: file header FSP template",
			    pos, end - pos);

		while (pos + 1 < end) {
			wpa_printf(MSG_MSGDUMP, "SCARD: file header TLV "
				   "0x%02x len=%d", pos[0], pos[1]);
			if (pos + 2 + pos[1] > end)
				break;

			if (pos[0] == USIM_TLV_FILE_SIZE &&
			    (pos[1] == 1 || pos[1] == 2) && file_len) {
				if (pos[1] == 1)
					*file_len = (int) pos[2];
				else
					*file_len = ((int) pos[2] << 8) |
						(int) pos[3];
				wpa_printf(MSG_DEBUG, "SCARD: file_size=%d",
					   *file_len);
			}

			if (pos[0] == USIM_TLV_PIN_STATUS_TEMPLATE &&
			    pos[1] >= 2 && pos[2] == USIM_PS_DO_TAG &&
			    pos[3] >= 1 && ps_do) {
				wpa_printf(MSG_DEBUG, "SCARD: PS_DO=0x%02x",
					   pos[4]);
				*ps_do = (int) pos[4];
			}

			pos += 2 + pos[1];

			if (pos == end)
				return 0;
		}
		return -1;
}


static int scard_pin_needed(struct scard_data *scard,
			    unsigned char *hdr, size_t hlen)
{
	if (scard->sim_type == SCARD_GSM_SIM) {
		if (hlen > SCARD_CHV1_OFFSET &&
		    !(hdr[SCARD_CHV1_OFFSET] & SCARD_CHV1_FLAG))
			return 1;
		return 0;
	}

	if (scard->sim_type == SCARD_USIM) {
		int ps_do;
		if (scard_parse_fsp_templ(hdr, hlen, &ps_do, NULL))
			return -1;
		/* TODO: there could be more than one PS_DO entry because of
		 * multiple PINs in key reference.. */
		if (ps_do)
			return 1;
	}

	return -1;
}


struct scard_data * scard_init(scard_sim_type sim_type, char *pin)
{
	long ret, len;
	struct scard_data *scard;
	char *readers = NULL;
	char buf[100];
	size_t blen;

	wpa_printf(MSG_DEBUG, "SCARD: initializing smart card interface");
	scard = malloc(sizeof(*scard));
	if (scard == NULL)
		return NULL;
	memset(scard, 0, sizeof(*scard));

	ret = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL,
				    &scard->ctx);
	if (ret != SCARD_S_SUCCESS) {
		wpa_printf(MSG_DEBUG, "SCARD: Could not establish smart card "
			   "context (err=%ld)", ret);
		goto failed;
	}

	ret = SCardListReaders(scard->ctx, NULL, NULL, &len);
	if (ret != SCARD_S_SUCCESS) {
		wpa_printf(MSG_DEBUG, "SCARD: SCardListReaders failed "
			   "(err=%ld)", ret);
		goto failed;
	}

	readers = malloc(len);
	if (readers == NULL) {
		printf("malloc failed\n");
		goto failed;
	}

	ret = SCardListReaders(scard->ctx, NULL, readers, &len);
	if (ret != SCARD_S_SUCCESS) {
		wpa_printf(MSG_DEBUG, "SCARD: SCardListReaders failed(2) "
			   "(err=%ld)", ret);
		goto failed;
	}
	if (len < 3) {
		wpa_printf(MSG_WARNING, "SCARD: No smart card readers "
			   "available.");
		goto failed;
	}
	/* readers is a list of available reader. Last entry is terminated with
	 * double NUL.
	 * TODO: add support for selecting the reader; now just use the first
	 * one.. */
	wpa_printf(MSG_DEBUG, "SCARD: Selected reader='%s'", readers);

	ret = SCardConnect(scard->ctx, readers, SCARD_SHARE_SHARED,
			   SCARD_PROTOCOL_T0, &scard->card, &scard->protocol);
	if (ret != SCARD_S_SUCCESS) {
		if (ret == SCARD_E_NO_SMARTCARD)
			wpa_printf(MSG_INFO, "No smart card inserted.");
		else
			wpa_printf(MSG_WARNING, "SCardConnect err=%lx", ret);
		goto failed;
	}

	free(readers);
	readers = NULL;

	wpa_printf(MSG_DEBUG, "SCARD: card=%ld active_protocol=%lu",
		   scard->card, scard->protocol);

	blen = sizeof(buf);

	scard->sim_type = SCARD_GSM_SIM;
	if (sim_type == SCARD_USIM_ONLY || sim_type == SCARD_TRY_BOTH) {
		wpa_printf(MSG_DEBUG, "SCARD: verifying USIM support");
		if (_scard_select_file(scard, SCARD_FILE_MF, buf, &blen,
				       SCARD_USIM, NULL)) {
			wpa_printf(MSG_DEBUG, "SCARD: USIM is not supported");
			if (sim_type == SCARD_USIM_ONLY)
				goto failed;
			wpa_printf(MSG_DEBUG, "SCARD: Trying to use GSM SIM");
			scard->sim_type = SCARD_GSM_SIM;
		} else {
			wpa_printf(MSG_DEBUG, "SCARD: USIM is supported");
			scard->sim_type = SCARD_USIM;
		}
	}

	if (scard->sim_type == SCARD_GSM_SIM) {
		blen = sizeof(buf);
		if (scard_select_file(scard, SCARD_FILE_MF, buf, &blen)) {
			wpa_printf(MSG_DEBUG, "SCARD: Failed to read MF");
			goto failed;
		}

		blen = sizeof(buf);
		if (scard_select_file(scard, SCARD_FILE_GSM_DF, buf, &blen)) {
			wpa_printf(MSG_DEBUG, "SCARD: Failed to read GSM DF");
			goto failed;
		}
	} else {
		/* Select based on AID = 3G RID */
		blen = sizeof(buf);
		if (_scard_select_file(scard, 0, buf, &blen, scard->sim_type,
				       "\xA0\x00\x00\x00\x87")) {
			wpa_printf(MSG_DEBUG, "SCARD: Failed to read 3G RID "
				   "AID");
			goto failed;
		}
	}

	/* Verify whether CHV1 (PIN1) is needed to access the card. */
	if (scard_pin_needed(scard, buf, blen)) {
		wpa_printf(MSG_DEBUG, "PIN1 needed for SIM access");
		if (pin == NULL) {
			wpa_printf(MSG_INFO, "No PIN configured for SIM "
				   "access");
			/* TODO: ask PIN from user through a frontend (e.g.,
			 * wpa_cli) */
			goto failed;
		}
		if (scard_verify_pin(scard, pin)) {
			wpa_printf(MSG_INFO, "PIN verification failed for "
				"SIM access");
			/* TODO: what to do? */
			goto failed;
		}
	}

	return scard;

failed:
	free(readers);
	scard_deinit(scard);
	return NULL;
}


void scard_deinit(struct scard_data *scard)
{
	long ret;

	if (scard == NULL)
		return;

	wpa_printf(MSG_DEBUG, "SCARD: deinitializing smart card interface");
	if (scard->card) {
		ret = SCardDisconnect(scard->card, SCARD_UNPOWER_CARD);
		if (ret != SCARD_S_SUCCESS) {
			wpa_printf(MSG_DEBUG, "SCARD: Failed to disconnect "
				   "smart card (err=%ld)", ret);
		}
	}

	if (scard->ctx) {
		ret = SCardReleaseContext(scard->ctx);
		if (ret != SCARD_S_SUCCESS) {
			wpa_printf(MSG_DEBUG, "Failed to release smart card "
				   "context (err=%ld)", ret);
		}
	}
	free(scard);
}


static long scard_transmit(struct scard_data *scard,
			   unsigned char *send, size_t send_len,
			   unsigned char *recv, size_t *recv_len)
{
	long ret;
	unsigned long rlen;

	wpa_hexdump_key(MSG_DEBUG, "SCARD: scard_transmit: send",
			send, send_len);
	rlen = *recv_len;
	ret = SCardTransmit(scard->card,
			    scard->protocol == SCARD_PROTOCOL_T1 ?
			    SCARD_PCI_T1 : SCARD_PCI_T0,
			    send, (unsigned long) send_len,
			    &scard->recv_pci, recv, &rlen);
	*recv_len = rlen;
	if (ret == SCARD_S_SUCCESS) {
		wpa_hexdump(MSG_DEBUG, "SCARD: scard_transmit: recv",
			    recv, rlen);
	} else {
		wpa_printf(MSG_WARNING, "SCARD: SCardTransmit failed "
			   "(err=0x%lx)", ret);
	}
	return ret;
}


static int _scard_select_file(struct scard_data *scard, unsigned short file_id,
			      unsigned char *buf, size_t *buf_len,
			      sim_types sim_type, unsigned char *aid)
{
	long ret;
	unsigned char resp[3];
	unsigned char cmd[10] = { SIM_CMD_SELECT };
	int cmdlen;
	unsigned char get_resp[5] = { SIM_CMD_GET_RESPONSE };
	size_t len, rlen;

	if (sim_type == SCARD_USIM) {
		cmd[0] = USIM_CLA;
		cmd[3] = 0x04;
		get_resp[0] = USIM_CLA;
	}

	wpa_printf(MSG_DEBUG, "SCARD: select file %04x", file_id);
	if (aid) {
		cmd[2] = 0x04; /* Select by AID */
		cmd[4] = 5; /* len */
		memcpy(cmd + 5, aid, 5);
		cmdlen = 10;
	} else {
		cmd[5] = file_id >> 8;
		cmd[6] = file_id & 0xff;
		cmdlen = 7;
	}
	len = sizeof(resp);
	ret = scard_transmit(scard, cmd, cmdlen, resp, &len);
	if (ret != SCARD_S_SUCCESS) {
		wpa_printf(MSG_WARNING, "SCARD: SCardTransmit failed "
			   "(err=0x%lx)", ret);
		return -1;
	}

	if (len != 2) {
		wpa_printf(MSG_WARNING, "SCARD: unexpected resp len "
			   "%d (expected 2)", (int) len);
		return -1;
	}

	if (resp[0] == 0x98 && resp[1] == 0x04) {
		/* Security status not satisfied (PIN_WLAN) */
		wpa_printf(MSG_WARNING, "SCARD: Security status not satisfied "
			   "(PIN_WLAN)");
		return -1;
	}

	if (resp[0] == 0x6e) {
		wpa_printf(MSG_DEBUG, "SCARD: used CLA not supported");
		return -1;
	}

	if (resp[0] != 0x6c && resp[0] != 0x9f && resp[0] != 0x61) {
		wpa_printf(MSG_WARNING, "SCARD: unexpected response 0x%02x "
			   "(expected 0x61, 0x6c, or 0x9f)", resp[0]);
		return -1;
	}
	/* Normal ending of command; resp[1] bytes available */
	get_resp[4] = resp[1];
	wpa_printf(MSG_DEBUG, "SCARD: trying to get response (%d bytes)",
		   resp[1]);

	rlen = *buf_len;
	ret = scard_transmit(scard, get_resp, sizeof(get_resp), buf, &rlen);
	if (ret == SCARD_S_SUCCESS) {
		*buf_len = resp[1] < rlen ? resp[1] : rlen;
		return 0;
	}

	wpa_printf(MSG_WARNING, "SCARD: SCardTransmit err=0x%lx\n", ret);
	return -1;
}


static int scard_select_file(struct scard_data *scard, unsigned short file_id,
			     unsigned char *buf, size_t *buf_len)
{
	return _scard_select_file(scard, file_id, buf, buf_len,
				  scard->sim_type, NULL);
}


static int scard_read_file(struct scard_data *scard,
			   unsigned char *data, size_t len)
{
	char cmd[5] = { SIM_CMD_READ_BIN, len };
	size_t blen = len + 3;
	unsigned char *buf;
	long ret;

	buf = malloc(blen);
	if (buf == NULL)
		return -1;

	if (scard->sim_type == SCARD_USIM)
		cmd[0] = USIM_CLA;
	ret = scard_transmit(scard, cmd, sizeof(cmd), buf, &blen);
	if (ret != SCARD_S_SUCCESS) {
		free(buf);
		return -2;
	}
	if (blen != len + 2) {
		wpa_printf(MSG_DEBUG, "SCARD: file read returned unexpected "
			   "length %d (expected %d)", blen, len + 2);
		free(buf);
		return -3;
	}

	if (buf[len] != 0x90 || buf[len + 1] != 0x00) {
		wpa_printf(MSG_DEBUG, "SCARD: file read returned unexpected "
			   "status %02x %02x (expected 90 00)",
			   buf[len], buf[len + 1]);
		free(buf);
		return -4;
	}

	memcpy(data, buf, len);
	free(buf);

	return 0;
}


static int scard_verify_pin(struct scard_data *scard, char *pin)
{
	long ret;
	unsigned char resp[3];
	char cmd[5 + 8] = { SIM_CMD_VERIFY_CHV1 };
	size_t len;

	wpa_printf(MSG_DEBUG, "SCARD: verifying PIN");

	if (pin == NULL || strlen(pin) > 8)
		return -1;

	if (scard->sim_type == SCARD_USIM)
		cmd[0] = USIM_CLA;
	memcpy(cmd + 5, pin, strlen(pin));
	memset(cmd + 5 + strlen(pin), 0xff, 8 - strlen(pin));

	len = sizeof(resp);
	ret = scard_transmit(scard, cmd, sizeof(cmd), resp, &len);
	if (ret != SCARD_S_SUCCESS)
		return -2;

	if (len != 2 || resp[0] != 0x90 || resp[1] != 0x00) {
		wpa_printf(MSG_WARNING, "SCARD: PIN verification failed");
		return -1;
	}

	wpa_printf(MSG_DEBUG, "SCARD: PIN verified successfully");
	return 0;
}


int scard_get_imsi(struct scard_data *scard, char *imsi, size_t *len)
{
	char buf[100];
	size_t blen, imsilen;
	char *pos;
	int i;

	wpa_printf(MSG_DEBUG, "SCARD: reading IMSI from (GSM) EF-IMSI");
	blen = sizeof(buf);
	if (scard_select_file(scard, SCARD_FILE_GSM_EF_IMSI, buf, &blen))
		return -1;
	if (blen < 4) {
		wpa_printf(MSG_WARNING, "SCARD: too short (GSM) EF-IMSI "
			   "header (len=%d)", blen);
		return -2;
	}

	if (scard->sim_type == SCARD_GSM_SIM) {
		blen = (buf[2] << 8) | buf[3];
	} else {
		int file_size;
		if (scard_parse_fsp_templ(buf, blen, NULL, &file_size))
			return -3;
		blen = file_size;
	}
	if (blen < 2 || blen > sizeof(buf)) {
		wpa_printf(MSG_DEBUG, "SCARD: invalid IMSI file length=%d",
			   blen);
		return -3;
	}

	imsilen = (blen - 2) * 2 + 1;
	wpa_printf(MSG_DEBUG, "SCARD: IMSI file length=%d imsilen=%d",
		   blen, imsilen);
	if (blen < 2 || imsilen > *len) {
		*len = imsilen;
		return -4;
	}

	if (scard_read_file(scard, buf, blen))
		return -5;

	pos = imsi;
	*pos++ = '0' + (buf[1] >> 4 & 0x0f);
	for (i = 2; i < blen; i++) {
		unsigned char digit;

		digit = buf[i] & 0x0f;
		if (digit < 10)
			*pos++ = '0' + digit;
		else
			imsilen--;

		digit = buf[i] >> 4 & 0x0f;
		if (digit < 10)
			*pos++ = '0' + digit;
		else
			imsilen--;
	}
	*len = imsilen;

	return 0;
}


int scard_gsm_auth(struct scard_data *scard, unsigned char *rand,
		   unsigned char *sres, unsigned char *kc)
{
	unsigned char cmd[5 + 1 + 16] = { SIM_CMD_RUN_GSM_ALG };
	int cmdlen;
	unsigned char get_resp[5] = { SIM_CMD_GET_RESPONSE };
	unsigned char resp[3], buf[12 + 3 + 2];
	size_t len;
	long ret;

	if (scard == NULL)
		return -1;

	wpa_hexdump(MSG_DEBUG, "SCARD: GSM auth - RAND", rand, 16);
	if (scard->sim_type == SCARD_GSM_SIM) {
		cmdlen = 5 + 16;
		memcpy(cmd + 5, rand, 16);
	} else {
		cmdlen = 5 + 1 + 16;
		cmd[0] = USIM_CLA;
		cmd[3] = 0x80;
		cmd[4] = 17;
		cmd[5] = 16;
		memcpy(cmd + 6, rand, 16);
	}
	len = sizeof(resp);
	ret = scard_transmit(scard, cmd, sizeof(cmd), resp, &len);
	if (ret != SCARD_S_SUCCESS)
		return -2;

	if ((scard->sim_type == SCARD_GSM_SIM &&
	     (len != 2 || resp[0] != 0x9f || resp[1] != 0x0c)) ||
	    (scard->sim_type == SCARD_USIM &&
	     (len != 2 || resp[0] != 0x61 || resp[1] != 0x0e))) {
		wpa_printf(MSG_WARNING, "SCARD: unexpected response for GSM "
			   "auth request (len=%d resp=%02x %02x)",
			   len, resp[0], resp[1]);
		return -3;
	}
	get_resp[4] = resp[1];

	len = sizeof(buf);
	ret = scard_transmit(scard, get_resp, sizeof(get_resp), buf, &len);
	if (ret != SCARD_S_SUCCESS)
		return -4;

	if (scard->sim_type == SCARD_GSM_SIM) {
		if (len != 4 + 8 + 2) {
			wpa_printf(MSG_WARNING, "SCARD: unexpected data "
				   "length for GSM auth (len=%d, expected 14)",
				   len);
			return -5;
		}
		memcpy(sres, buf, 4);
		memcpy(kc, buf + 4, 8);
	} else {
		if (len != 1 + 4 + 1 + 8 + 2) {
			wpa_printf(MSG_WARNING, "SCARD: unexpected data "
				   "length for USIM auth (len=%d, "
				   "expected 16)", len);
			return -5;
		}
		if (buf[0] != 4 || buf[5] != 8) {
			wpa_printf(MSG_WARNING, "SCARD: unexpected SREC/Kc "
				   "length (%d %d, expected 4 8)",
				   buf[0], buf[5]);
		}
		memcpy(sres, buf + 1, 4);
		memcpy(kc, buf + 6, 8);
	}

	wpa_hexdump(MSG_DEBUG, "SCARD: GSM auth - SRES", sres, 4);
	wpa_hexdump(MSG_DEBUG, "SCARD: GSM auth - Kc", kc, 8);

	return 0;
}


int scard_umts_auth(struct scard_data *scard, unsigned char *rand,
		    unsigned char *autn, unsigned char *res, size_t *res_len,
		    unsigned char *ik, unsigned char *ck, unsigned char *auts)
{
	unsigned char cmd[5 + 1 + AKA_RAND_LEN + 1 + AKA_AUTN_LEN] =
		{ USIM_CMD_RUN_UMTS_ALG };
	int cmdlen;
	unsigned char get_resp[5] = { USIM_CMD_GET_RESPONSE };
	unsigned char resp[3], buf[64], *pos, *end;
	size_t len;
	long ret;

	if (scard == NULL)
		return -1;

	if (scard->sim_type == SCARD_GSM_SIM) {
		wpa_printf(MSG_ERROR, "SCARD: Non-USIM card - cannot do UMTS "
			   "auth");
		return -1;
	}

	wpa_hexdump(MSG_DEBUG, "SCARD: UMTS auth - RAND", rand, AKA_RAND_LEN);
	wpa_hexdump(MSG_DEBUG, "SCARD: UMTS auth - AUTN", autn, AKA_AUTN_LEN);
	cmdlen = 5 + 1 + AKA_RAND_LEN + 1 + AKA_AUTN_LEN;
	cmd[5] = AKA_RAND_LEN;
	memcpy(cmd + 6, rand, AKA_RAND_LEN);
	cmd[6 + AKA_RAND_LEN] = AKA_AUTN_LEN;
	memcpy(cmd + 6 + AKA_RAND_LEN + 1, autn, AKA_AUTN_LEN);

	len = sizeof(resp);
	ret = scard_transmit(scard, cmd, sizeof(cmd), resp, &len);
	if (ret != SCARD_S_SUCCESS)
		return -1;

	if (len >= 0 && len <= sizeof(resp))
		wpa_hexdump(MSG_DEBUG, "SCARD: UMTS alg response", resp, len);

	if (len == 2 && resp[0] == 0x98 && resp[1] == 0x62) {
		wpa_printf(MSG_WARNING, "SCARD: UMTS auth failed - "
			   "MAC != XMAC");
		return -1;
	} else if (len != 2 || resp[0] != 0x61) {
		wpa_printf(MSG_WARNING, "SCARD: unexpected response for UMTS "
			   "auth request (len=%d resp=%02x %02x)",
			   len, resp[0], resp[1]);
		return -1;
	}
	get_resp[4] = resp[1];

	len = sizeof(buf);
	ret = scard_transmit(scard, get_resp, sizeof(get_resp), buf, &len);
	if (ret != SCARD_S_SUCCESS || len < 0 || len > sizeof(buf))
		return -1;

	wpa_hexdump(MSG_DEBUG, "SCARD: UMTS get response result", buf, len);
	if (len >= 2 + AKA_AUTS_LEN && buf[0] == 0xdc &&
	    buf[1] == AKA_AUTS_LEN) {
		wpa_printf(MSG_DEBUG, "SCARD: UMTS Synchronization-Failure");
		memcpy(auts, buf + 2, AKA_AUTS_LEN);
		wpa_hexdump(MSG_DEBUG, "SCARD: AUTS", auts, AKA_AUTS_LEN);
		return -2;
	} else if (len >= 6 + IK_LEN + CK_LEN && buf[0] == 0xdb) {
		pos = buf + 1;
		end = buf + len;

		/* RES */
		if (pos[0] > RES_MAX_LEN || pos + pos[0] > end) {
			wpa_printf(MSG_DEBUG, "SCARD: Invalid RES");
			return -1;
		}
		*res_len = *pos++;
		memcpy(res, pos, *res_len);
		pos += *res_len;
		wpa_hexdump(MSG_DEBUG, "SCARD: RES", res, *res_len);

		/* CK */
		if (pos[0] != CK_LEN || pos + CK_LEN > end) {
			wpa_printf(MSG_DEBUG, "SCARD: Invalid CK");
			return -1;
		}
		pos++;
		memcpy(ck, pos, CK_LEN);
		pos += CK_LEN;
		wpa_hexdump(MSG_DEBUG, "SCARD: CK", ck, CK_LEN);

		/* IK */
		if (pos[0] != IK_LEN || pos + IK_LEN > end) {
			wpa_printf(MSG_DEBUG, "SCARD: Invalid IK");
			return -1;
		}
		pos++;
		memcpy(ik, pos, IK_LEN);
		pos += IK_LEN;
		wpa_hexdump(MSG_DEBUG, "SCARD: IK", ik, IK_LEN);

		return 0;
	}

	wpa_printf(MSG_DEBUG, "SCARD: Unrecognized response");
	return -1;
}
