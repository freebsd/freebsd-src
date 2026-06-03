/*
 * NFC Framework - Near Field Communication management
 * BSD licensed
 */

#ifndef _MOBILE_NFC_H_
#define _MOBILE_NFC_H_

#include <sys/types.h>

#define NFC_MAX_TAGS            8
#define NFC_MAX_NDEF_RECORDS    16
#define NFC_MAX_NDEF_TYPE       32
#define NFC_MAX_NDEF_PAYLOAD    1024
#define NFC_MAX_AID_LEN         16

typedef enum {
    NFC_TECH_NFC_A           = 0x01,
    NFC_TECH_NFC_B           = 0x02,
    NFC_TECH_NFC_F           = 0x04,
    NFC_TECH_NFC_V           = 0x08,
    NFC_TECH_ISO_14443_A     = 0x10,
    NFC_TECH_ISO_14443_B     = 0x20,
    NFC_TECH_ISO_15693       = 0x40,
    NFC_TECH_ISO_DEP         = 0x80,
    NFC_TECH_NFC_DEP         = 0x100
} nfc_tech_t;

typedef enum {
    NFC_POLL_MODE_NORMAL       = 0,
    NFC_POLL_MODE_EASY        = 1,
    NFC_POLL_MODE_ALL         = 2
} nfc_poll_mode_t;

typedef enum {
    NFC_TAG_STATE_DISCOVERED   = 0,
    NFC_TAG_STATE_CONNECTED    = 1,
    NFC_TAG_STATE_DISCONNECTED = 2,
    NFC_TAG_STATE_ERROR        = 3
} nfc_tag_state_t;

typedef enum {
    NFC_RECORD_WELL_KNOWN     = 0x01,
    NFC_RECORD_MIME_MEDIA     = 0x02,
    NFC_RECORD_URI            = 0x03,
    NFC_RECORD_EXTERNAL         = 0x04,
    NFC_RECORD_UNKNOWN        = 0xFF
} nfc_record_type_t;

typedef struct {
    char            type[NFC_MAX_NDEF_TYPE + 1];
    uint8_t         payload[NFC_MAX_NDEF_PAYLOAD];
    size_t          payload_len;
    char            uri[NFC_MAX_NDEF_PAYLOAD];
} ndef_record_t;

typedef struct {
    ndef_record_t   records[NFC_MAX_NDEF_RECORDS];
    int             record_count;
    uint8_t         raw_data[NFC_MAX_NDEF_PAYLOAD * 4];
    size_t          raw_len;
} ndef_message_t;

typedef struct {
    nfc_tech_t      technologies;
    nfc_tag_state_t state;
    uint8_t         uid[10];
    size_t          uid_len;
    char            tag_type[32];
    ndef_message_t  ndef;
} nfc_tag_t;

typedef struct {
    char            name[128];
    int             enabled;
    int             discoverable;
    int             polling;
    int             p2p_enabled;
    int             hce_enabled;
    uint8_t         hce_aid[NFC_MAX_AID_LEN];
    size_t          hce_aid_len;
} nfc_adapter_info_t;

typedef struct {
    int             device_fd;
    nfc_adapter_info_t adapter;
    nfc_tag_t       tags[NFC_MAX_TAGS];
    int             tag_count;
} nfc_context_t;

int nfc_init(void);
void nfc_shutdown(void);
int nfc_enable(void);
int nfc_disable(void);
int nfc_start_discovery(nfc_poll_mode_t mode);
int nfc_stop_discovery(void);
int nfc_get_tags(nfc_tag_t *tags, int *count);
int nfc_connect(nfc_tech_t tech);
int nfc_disconnect(void);
int nfc_read_ndef(ndef_message_t *ndef);
int nfc_write_ndef(const ndef_message_t *ndef);
int nfc_format_tag(void);
int nfc_p2p_send(const uint8_t *data, size_t len);
int nfc_p2p_receive(uint8_t *data, size_t len);
int nfc_hce_enable(int enabled);
int nfc_hce_register_aid(const uint8_t aid[NFC_MAX_AID_LEN], size_t aid_len);
int nfc_hce_send_apdu(const uint8_t *apdu, size_t len);
int nfc_set_adapter_name(const char *name);

#endif /* _MOBILE_NFC_H_ */