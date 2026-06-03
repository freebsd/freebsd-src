/*
 * NFC Framework - Near Field Communication management implementation
 * BSD licensed
 */

#include "nfc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>

static nfc_context_t nfc_ctx;

int nfc_init(void)
{
    memset(&nfc_ctx, 0, sizeof(nfc_ctx));
    
    nfc_ctx.device_fd = open("/dev/nfcd", O_RDWR);
    if (nfc_ctx.device_fd < 0) {
        nfc_ctx.device_fd = open("/dev/nfc0", O_RDWR);
    }
    
    return nfc_ctx.device_fd >= 0 ? 0 : -1;
}

void nfc_shutdown(void)
{
    nfc_stop_discovery();
    nfc_hce_enable(0);
    
    if (nfc_ctx.device_fd >= 0) {
        close(nfc_ctx.device_fd);
        nfc_ctx.device_fd = -1;
    }
}

int nfc_enable(void)
{
    nfc_ctx.adapter.enabled = 1;
    
    if (nfc_ctx.device_fd >= 0) {
        return ioctl(nfc_ctx.device_fd, 1, 1);
    }
    
    return 0;
}

int nfc_disable(void)
{
    nfc_ctx.adapter.enabled = 0;
    nfc_stop_discovery();
    
    if (nfc_ctx.device_fd >= 0) {
        return ioctl(nfc_ctx.device_fd, 1, 0);
    }
    
    return 0;
}

int nfc_start_discovery(nfc_poll_mode_t mode)
{
    char cmd[64];
    
    if (!nfc_ctx.adapter.enabled)
        return -1;
    
    nfc_ctx.adapter.polling = 1;
    
    if (nfc_ctx.device_fd >= 0) {
        return ioctl(nfc_ctx.device_fd, 2, mode);
    }
    
    snprintf(cmd, sizeof(cmd), "echo %d > /sys/class/nfc/poll_mode", mode);
    return system(cmd);
}

int nfc_stop_discovery(void)
{
    nfc_ctx.adapter.polling = 0;
    
    if (nfc_ctx.device_fd >= 0) {
        return ioctl(nfc_ctx.device_fd, 2, 0);
    }
    
    return system("echo 0 > /sys/class/nfc/poll_mode");
}

int nfc_get_tags(nfc_tag_t *tags, int *count)
{
    uint8_t buf[4096];
    int ret;
    
    if (!tags || !count)
        return -1;
    
    if (nfc_ctx.device_fd < 0) {
        *count = 0;
        return 0;
    }
    
    ret = read(nfc_ctx.device_fd, buf, sizeof(buf));
    if (ret < 0)
        return -1;
    
    *count = 0;
    
    nfc_tag_t *tag = tags;
    for (int i = 0; i < ret && *count < NFC_MAX_TAGS; i += 32) {
        if (buf[i] == 0xAA) {
            tag->state = NFC_TAG_STATE_DISCOVERED;
            tag->technologies = buf[i + 1];
            tag->uid_len = buf[i + 2];
            memcpy(tag->uid, &buf[i + 3], tag->uid_len);
            
            if (tag->technologies & NFC_TECH_ISO_14443_A)
                strlcpy(tag->tag_type, "ISO 14443-A", sizeof(tag->tag_type));
            else if (tag->technologies & NFC_TECH_ISO_14443_B)
                strlcpy(tag->tag_type, "ISO 14443-B", sizeof(tag->tag_type));
            else if (tag->technologies & NFC_TECH_NFC_A)
                strlcpy(tag->tag_type, "NFC-A", sizeof(tag->tag_type));
            
            (*count)++;
            tag++;
        }
    }
    
    return 0;
}

int nfc_connect(nfc_tech_t tech)
{
    if (!nfc_ctx.adapter.enabled)
        return -1;
    
    for (int i = 0; i < NFC_MAX_TAGS; i++) {
        if (nfc_ctx.tags[i].state == NFC_TAG_STATE_DISCOVERED) {
            if (nfc_ctx.tags[i].technologies & tech) {
                nfc_ctx.tags[i].state = NFC_TAG_STATE_CONNECTED;
                return 0;
            }
        }
    }
    
    return -1;
}

int nfc_disconnect(void)
{
    for (int i = 0; i < NFC_MAX_TAGS; i++) {
        nfc_ctx.tags[i].state = NFC_TAG_STATE_DISCONNECTED;
    }
    
    return 0;
}

static int ndef_parse_record(uint8_t *data, size_t len, ndef_record_t *record)
{
    if (len < 4)
        return -1;
    
    uint8_t header = data[0];
    record->type[0] = '\0';
    
    if ((header & 0x06) == 0) {
        record->type[0] = '\0';
    } else {
        size_t type_len = data[1];
        if (len < 4 + type_len)
            return -1;
        
        memcpy(record->type, &data[3], type_len);
        record->type[type_len] = '\0';
    }
    
    size_t payload_len = data[2];
    if (payload_len == 0xFF) {
        payload_len = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
    }
    
    if (len < 3 + payload_len)
        return -1;
    
    record->payload_len = payload_len;
    memcpy(record->payload, &data[3 + (len >= 0xFF ? 4 : 0)], payload_len);
    
    if (strcmp(record->type, "U") == 0) {
        record->payload[0] = 'h';
        record->payload[1] = 't';
        record->payload[2] = 't';
        record->payload[3] = 'p';
        record->payload[4] = ':';
        record->payload[5] = '/';
        record->payload[6] = '/';
        strlcpy(record->uri, (char *)record->payload, NFC_MAX_NDEF_PAYLOAD);
    }
    
    return 0;
}

int nfc_read_ndef(ndef_message_t *ndef)
{
    uint8_t buf[4096];
    int ret;
    
    if (!ndef)
        return -1;
    
    memset(ndef, 0, sizeof(*ndef));
    
    ret = read(nfc_ctx.device_fd, buf, sizeof(buf));
    if (ret < 0)
        return -1;
    
    ndef->raw_len = ret;
    memcpy(ndef->raw_data, buf, ret);
    
    for (int i = 0; i < ret && ndef->record_count < NFC_MAX_NDEF_RECORDS; ) {
        if (ndef_parse_record(&buf[i], ret - i, &ndef->records[ndef->record_count]) == 0) {
            size_t rec_len = 3 + ndef->records[ndef->record_count].payload_len;
            i += rec_len;
            ndef->record_count++;
        } else {
            break;
        }
    }
    
    return 0;
}

int nfc_write_ndef(const ndef_message_t *ndef)
{
    uint8_t buf[4096];
    int len = 0;
    
    if (!ndef || nfc_ctx.device_fd < 0)
        return -1;
    
    buf[len++] = 0xD1;
    
    buf[len++] = 1;
    
    for (int i = 0; i < ndef->record_count; i++) {
        const ndef_record_t *rec = &ndef->records[i];
        buf[len++] = 0x55;
        
        size_t type_len = strlen(rec->type);
        buf[len++] = type_len;
        
        memcpy(&buf[len], rec->type, type_len);
        len += type_len;
        
        memcpy(&buf[len], rec->payload, rec->payload_len);
        len += rec->payload_len;
    }
    
    buf[len++] = 0xFE;
    
    return write(nfc_ctx.device_fd, buf, len);
}

int nfc_format_tag(void)
{
    if (nfc_ctx.device_fd < 0)
        return -1;
    
    uint8_t format_cmd[] = { 0xD1, 0x01, 0x0E, 0x55, 0x01, 'T', 0x02, 0x65, 0x6E };
    
    return write(nfc_ctx.device_fd, format_cmd, sizeof(format_cmd));
}

int nfc_p2p_send(const uint8_t *data, size_t len)
{
    if (!nfc_ctx.adapter.p2p_enabled || nfc_ctx.device_fd < 0)
        return -1;
    
    return write(nfc_ctx.device_fd, data, len);
}

int nfc_p2p_receive(uint8_t *data, size_t len)
{
    if (!nfc_ctx.adapter.p2p_enabled || nfc_ctx.device_fd < 0)
        return -1;
    
    struct pollfd pfd = { .fd = nfc_ctx.device_fd, .events = POLLIN };
    
    if (poll(&pfd, 1, 5000) <= 0)
        return -1;
    
    return read(nfc_ctx.device_fd, data, len);
}

int nfc_hce_enable(int enabled)
{
    nfc_ctx.adapter.hce_enabled = enabled;
    
    if (nfc_ctx.device_fd >= 0) {
        return ioctl(nfc_ctx.device_fd, 3, enabled);
    }
    
    return 0;
}

int nfc_hce_register_aid(const uint8_t aid[NFC_MAX_AID_LEN], size_t aid_len)
{
    if (nfc_ctx.device_fd < 0)
        return -1;
    
    if (aid_len > NFC_MAX_AID_LEN)
        return -1;
    
    memcpy(nfc_ctx.adapter.hce_aid, aid, aid_len);
    nfc_ctx.adapter.hce_aid_len = aid_len;
    
    return ioctl(nfc_ctx.device_fd, 4, aid);
}

int nfc_hce_send_apdu(const uint8_t *apdu, size_t len)
{
    if (nfc_ctx.device_fd < 0)
        return -1;
    
    return write(nfc_ctx.device_fd, apdu, len);
}

int nfc_set_adapter_name(const char *name)
{
    if (!name)
        return -1;
    
    strlcpy(nfc_ctx.adapter.name, name, sizeof(nfc_ctx.adapter.name));
    
    return 0;
}