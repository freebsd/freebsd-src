/*
 * Cellular Modem - Cellular modem AT/QMI interface management
 * BSD licensed
 */

#ifndef _MOBILE_CELLULAR_H_
#define _MOBILE_CELLULAR_H_

#include <sys/types.h>

#define CELL_MAX_IMEI_LEN       16
#define CELL_MAX_IMSI_LEN       16
#define CELL_MAX_OPERATOR_LEN   32
#define CELL_MAX_SMS_TEXT       160
#define CELL_MAX_PHONE_LEN      16
#define CELL_MAX_APN_LEN        64
#define CELL_MAX_PDP_CONTEXTS   8

typedef enum {
    CELL_REG_NONE             = 0,
    CELL_REG_HOME             = 1,
    CELL_REG_ROAMING          = 2,
    CELL_REG_DENIED           = 3,
    CELL_REG_UNKNOWN          = 4
} cell_registration_t;

typedef enum {
    CELL_NETWORK_GSM          = 0,
    CELL_NETWORK_CDMA         = 1,
    CELL_NETWORK_WCDMA        = 2,
    CELL_NETWORK_LTE          = 3,
    CELL_NETWORK_NR_SA        = 4,
    CELL_NETWORK_NR_NSA       = 5
} cell_network_t;

typedef enum {
    AT_RESP_OK                = 0,
    AT_RESP_ERROR             = -1,
    AT_RESP_CME_ERROR         = -2,
    AT_RESP_CMS_ERROR         = -3,
    AT_RESP_TIMEOUT           = -4
} at_response_t;

typedef struct {
    char            number[CELL_MAX_PHONE_LEN + 1];
    char            text[CELL_MAX_SMS_TEXT + 1];
    time_t          timestamp;
    int             index;
    int             unread;
} sms_t;

typedef struct {
    char            imei[CELL_MAX_IMEI_LEN + 1];
    char            imsi[CELL_MAX_IMSI_LEN + 1];
    char            operator[CELL_MAX_OPERATOR_LEN + 1];
    cell_network_t  network;
    int             signal_strength;
    cell_registration_t registration;
} cell_info_t;

typedef struct {
    char            apn[CELL_MAX_APN_LEN + 1];
    int             cid;
    int             active;
    struct in_addr  ip_addr;
} pdp_context_t;

typedef struct {
    int             modem_fd;
    int             qmi_fd;
    cell_info_t     info;
    pdp_context_t   contexts[CELL_MAX_PDP_CONTEXTS];
    int             context_count;
    int             sms_count;
    int             powered;
} cell_context_t;

int cell_init(const char *device_path);
void cell_shutdown(void);
int cell_get_info(cell_info_t *info);
int cell_register_network(const char *plmn);
int cell_send_sms(const char *number, const char *text);
int cell_get_sms(sms_t *messages, int *count);
int cell_delete_sms(int index);
int cell_data_connect(const char *apn);
int cell_data_disconnect(void);
int cell_get_pdp_contexts(pdp_context_t *contexts, int *count);
int cell_power_on(void);
int cell_power_off(void);
int cell_reset(void);

int at_command(const char *cmd, char *response, size_t resp_len, int timeout_ms);
int at_parse_response(const char *resp, at_response_t *code, char *error_msg, size_t err_len);
int at_set_callback(void (*cb)(const char *line, void *user), void *user_ctx);

#endif /* _MOBILE_CELLULAR_H_ */