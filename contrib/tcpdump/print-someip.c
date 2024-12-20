/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code
 * distributions retain the above copyright notice and this paragraph
 * in its entirety, and (2) distributions including binary code include
 * the above copyright notice and this paragraph in its entirety in
 * the documentation or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND
 * WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, WITHOUT
 * LIMITATION, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 *
 * Original code by Francesco Fondelli (francesco dot fondelli, gmail dot com)
 */

/* \summary: Autosar SOME/IP Protocol printer */

#include <config.h>

#include "netdissect-stdinc.h"
#include "netdissect.h"
#include "extract.h"

/*
 * SOMEIP Header (R19-11)
 *
 *     0                   1                   2                   3
 *     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |               Message ID (Service ID/Method ID)               |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                           Length                              |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |               Request ID (Client ID/Session ID)               |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    | Protocol Ver  | Interface Ver | Message Type  |  Return Code  |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                            Payload                            |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

static const struct tok message_type_values[] = {
    { 0x00, "REQUEST" },
    { 0x01, "REQUEST_NO_RETURN" },
    { 0x02, "NOTIFICATION" },
    { 0x80, "RESPONSE" },
    { 0x81, "ERROR" },
    { 0x20, "TP_REQUEST" },
    { 0x21, "TP_REQUEST_NO_RETURN" },
    { 0x22, "TP_NOTIFICATION" },
    { 0xa0, "TP_RESPONSE" },
    { 0xa1, "TP_ERROR" },
    { 0, NULL }
};

static const struct tok return_code_values[] = {
    { 0x00, "E_OK" },
    { 0x01, "E_NOT_OK" },
    { 0x02, "E_UNKNOWN_SERVICE" },
    { 0x03, "E_UNKNOWN_METHOD" },
    { 0x04, "E_NOT_READY" },
    { 0x05, "E_NOT_REACHABLE" },
    { 0x06, "E_TIMEOUT" },
    { 0x07, "E_WRONG_PROTOCOL_VERSION" },
    { 0x08, "E_WRONG_INTERFACE_VERSION" },
    { 0x09, "E_MALFORMED_MESSAGE" },
    { 0x0a, "E_WRONG_MESSAGE_TYPE" },
    { 0x0b, "E_E2E_REPEATED" },
    { 0x0c, "E_E2E_WRONG_SEQUENCE" },
    { 0x0d, "E_E2E" },
    { 0x0e, "E_E2E_NOT_AVAILABLE" },
    { 0x0f, "E_E2E_NO_NEW_DATA" },
    { 0, NULL }
};

void
someip_print(netdissect_options *ndo, const u_char *bp, const u_int len)
{
    uint32_t message_id;
    uint16_t service_id;
    uint16_t method_or_event_id;
    uint8_t event_flag;
    uint32_t message_len;
    uint32_t request_id;
    uint16_t client_id;
    uint16_t session_id;
    uint8_t protocol_version;
    uint8_t interface_version;
    uint8_t message_type;
    uint8_t return_code;

    ndo->ndo_protocol = "someip";
    nd_print_protocol_caps(ndo);

    if (len < 16) {
        goto invalid;
    }

    message_id = GET_BE_U_4(bp);
    service_id = message_id >> 16;
    event_flag = (message_id & 0x00008000) >> 15;
    method_or_event_id = message_id & 0x00007FFF;
    bp += 4;
    ND_PRINT(", service %u, %s %u",
             service_id, event_flag ? "event" : "method", method_or_event_id);

    message_len = GET_BE_U_4(bp);
    bp += 4;
    ND_PRINT(", len %u", message_len);

    request_id = GET_BE_U_4(bp);
    client_id = request_id >> 16;
    session_id = request_id & 0x0000FFFF;
    bp += 4;
    ND_PRINT(", client %u, session %u", client_id, session_id);

    protocol_version = GET_U_1(bp);
    bp += 1;
    ND_PRINT(", pver %u", protocol_version);

    interface_version = GET_U_1(bp);
    bp += 1;
    ND_PRINT(", iver %u", interface_version);

    message_type = GET_U_1(bp);
    bp += 1;
    ND_PRINT(", msgtype %s",
             tok2str(message_type_values, "Unknown", message_type));

    return_code = GET_U_1(bp);
    bp += 1;
    ND_PRINT(", retcode %s\n",
	     tok2str(return_code_values, "Unknown", return_code));

    return;

invalid:
    nd_print_invalid(ndo);
}
