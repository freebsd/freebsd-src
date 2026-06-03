/*
 * Cellular Modem - Cellular modem AT/QMI interface management implementation
 * BSD licensed
 */

#include "cellular.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <poll.h>

static cell_context_t cell_ctx;

static int cell_configure_port(int fd, int baudrate)
{
    struct termios tio;
    
    memset(&tio, 0, sizeof(tio));
    tio.c_cflag = baudrate | CS8 | CREAD | HUPCL | CLOCAL;
    tio.c_cflag |= PARENB;
    tio.c_cflag &= ~PARODD;
    tio.c_cflag &= ~CSTOPB;
    tio.c_iflag = IGNPAR;
    tio.c_oflag = 0;
    tio.c_lflag = 0;
    
    return tcsetattr(fd, TCSANOW, &tio);
}

int cell_init(const char *device_path)
{
    int fd;
    
    memset(&cell_ctx, 0, sizeof(cell_ctx));
    
    fd = open(device_path, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd < 0)
        return -1;
    
    if (cell_configure_port(fd, B115200) < 0) {
        close(fd);
        return -1;
    }
    
    fcntl(fd, F_SETFL, 0);
    cell_ctx.modem_fd = fd;
    
    char resp[256];
    if (at_command("ATE0", resp, sizeof(resp), 1000) < 0) {
        close(fd);
        return -1;
    }
    
    if (at_command("AT+CGMI", resp, sizeof(resp), 1000) < 0) {
        close(fd);
        return -1;
    }
    
    return 0;
}

void cell_shutdown(void)
{
    if (cell_ctx.modem_fd >= 0) {
        close(cell_ctx.modem_fd);
        cell_ctx.modem_fd = -1;
    }
    if (cell_ctx.qmi_fd >= 0) {
        close(cell_ctx.qmi_fd);
        cell_ctx.qmi_fd = -1;
    }
}

int cell_get_info(cell_info_t *info)
{
    char resp[256];
    
    if (!info)
        return -1;
    
    if (at_command("AT+CGSN", resp, sizeof(resp), 1000) >= 0) {
        char *p = resp;
        while (*p && (*p < '0' || *p > '9')) p++;
        strlcpy(info->imei, p, CELL_MAX_IMEI_LEN + 1);
    }
    
    if (at_command("AT+CIMI", resp, sizeof(resp), 1000) >= 0) {
        strlcpy(info->imsi, resp, CELL_MAX_IMSI_LEN + 1);
    }
    
    at_command("AT+COPS?", resp, sizeof(resp), 1000);
    if (sscanf(resp, "%*[^']'%32[^']'", info->operator, CELL_MAX_OPERATOR_LEN) == 1) {
        info->operator[CELL_MAX_OPERATOR_LEN] = '\0';
    }
    
    at_command("AT+CSQ", resp, sizeof(resp), 1000);
    int rssi, ber;
    if (sscanf(resp, "+CSQ: %d,%d", &rssi, &ber) == 2) {
        info->signal_strength = rssi * 2 - 110;
        if (rssi == 99)
            info->signal_strength = 0;
    }
    
    at_command("AT+CREG?", resp, sizeof(resp), 1000);
    int reg_status;
    if (sscanf(resp, "+CREG: %*d,%d", &reg_status) == 1) {
        info->registration = (cell_registration_t)reg_status;
    }
    
    cell_ctx.info = *info;
    return 0;
}

int cell_register_network(const char *plmn)
{
    char cmd[64], resp[256];
    int ret;
    
    if (!plmn)
        return at_command("AT+COPS=0", resp, sizeof(resp), 30000);
    
    snprintf(cmd, sizeof(cmd), "AT+COPS=1,2,\"%s\"", plmn);
    ret = at_command(cmd, resp, sizeof(resp), 30000);
    
    return ret;
}

int cell_send_sms(const char *number, const char *text)
{
    char cmd[256], resp[256];
    int ret;
    
    if (!number || !text)
        return -1;
    
    ret = at_command("AT+CMGF=1", resp, sizeof(resp), 1000);
    if (ret < 0)
        return -1;
    
    snprintf(cmd, sizeof(cmd), "AT+CMGS=\"%s\"", number);
    ret = at_command(cmd, resp, sizeof(resp), 1000);
    if (ret < 0)
        return -1;
    
    write(cell_ctx.modem_fd, text, strlen(text));
    write(cell_ctx.modem_fd, "\x1A", 1);
    
    return 0;
}

int cell_get_sms(sms_t *messages, int *count)
{
    char cmd[64], resp[2048];
    int ret, idx = 1;
    char *line, *saveptr;
    
    if (!messages || !count)
        return -1;
    
    ret = at_command("AT+CMGF=1", resp, sizeof(resp), 1000);
    if (ret < 0)
        return -1;
    
    *count = 0;
    
    for (line = strtok_r(resp, "\n", &saveptr); line; line = strtok_r(NULL, "\n", &saveptr)) {
        char phone[17], date[20];
        
        if (sscanf(line, "+CMGL: %d,\"%*[^,]\",\"%16[^\"]\",\"%*[^"]\",\"%19[^\"]\"",
                   &idx, phone, date) == 3) {
            strlcpy(messages[*count].number, phone, CELL_MAX_PHONE_LEN + 1);
            messages[*count].index = idx;
            (*count)++;
            
            if (*count >= CELL_MAX_PDP_CONTEXTS)
                break;
        }
    }
    
    return 0;
}

int cell_delete_sms(int index)
{
    char cmd[64], resp[256];
    snprintf(cmd, sizeof(cmd), "AT+CMGD=%d", index);
    return at_command(cmd, resp, sizeof(resp), 1000);
}

int cell_data_connect(const char *apn)
{
    char cmd[128], resp[256];
    int ret, cid = 1;
    
    if (!apn)
        return -1;
    
    snprintf(cmd, sizeof(cmd), "AT+CGDCONT=1,\"IP\",\"%s\"", apn);
    ret = at_command(cmd, resp, sizeof(resp), 1000);
    if (ret < 0)
        return -1;
    
    snprintf(cmd, sizeof(cmd), "AT+CGACT=1,%d", cid);
    ret = at_command(cmd, resp, sizeof(resp), 15000);
    
    if (ret >= 0) {
        strlcpy(cell_ctx.contexts[0].apn, apn, CELL_MAX_APN_LEN + 1);
        cell_ctx.contexts[0].cid = cid;
        cell_ctx.contexts[0].active = 1;
        cell_ctx.context_count = 1;
    }
    
    return ret;
}

int cell_data_disconnect(void)
{
    char cmd[64], resp[256];
    
    if (cell_ctx.context_count == 0)
        return 0;
    
    snprintf(cmd, sizeof(cmd), "AT+CGACT=0,%d", cell_ctx.contexts[0].cid);
    int ret = at_command(cmd, resp, sizeof(resp), 10000);
    
    if (ret >= 0) {
        cell_ctx.context_count = 0;
    }
    
    return ret;
}

int cell_get_pdp_contexts(pdp_context_t *contexts, int *count)
{
    char resp[256];
    
    if (!contexts || !count)
        return -1;
    
    int ret = at_command("AT+CGDCONT?", resp, sizeof(resp), 1000);
    if (ret < 0)
        return -1;
    
    *count = 0;
    char *line, *saveptr;
    
    for (line = strtok_r(resp, "\n", &saveptr); line; line = strtok_r(NULL, "\n", &saveptr)) {
        int cid;
        char apn_type[16];
        
        if (sscanf(line, "+CGDCONT: %d,\"%16[^\"]\",\"%64[^\"]\"",
                   &cid, apn_type, contexts[*count].apn) == 3) {
            contexts[*count].cid = cid;
            (*count)++;
            
            if (*count >= CELL_MAX_PDP_CONTEXTS)
                break;
        }
    }
    
    return 0;
}

int cell_power_on(void)
{
    char resp[256];
    cell_ctx.powered = 1;
    return at_command("AT", resp, sizeof(resp), 1000);
}

int cell_power_off(void)
{
    char resp[256];
    cell_ctx.powered = 0;
    return at_command("AT+CPOWD=1", resp, sizeof(resp), 5000);
}

int cell_reset(void)
{
    char resp[256];
    at_command("AT+CFUN=1,1", resp, sizeof(resp), 10000);
    return 0;
}

int at_command(const char *cmd, char *response, size_t resp_len, int timeout_ms)
{
    struct pollfd pfd;
    char buf[256];
    ssize_t n;
    int ret;
    at_response_t code;
    
    if (write(cell_ctx.modem_fd, cmd, strlen(cmd)) < 0)
        return -1;
    
    if (write(cell_ctx.modem_fd, "\r\n", 2) < 0)
        return -1;
    
    memset(response, 0, resp_len);
    
    pfd.fd = cell_ctx.modem_fd;
    pfd.events = POLLIN;
    
    ret = poll(&pfd, 1, timeout_ms);
    if (ret <= 0)
        return AT_RESP_TIMEOUT;
    
    n = read(cell_ctx.modem_fd, buf, sizeof(buf) - 1);
    if (n < 0)
        return AT_RESP_ERROR;
    
    buf[n] = '\0';
    strlcpy(response, buf, resp_len);
    
    at_parse_response(buf, &code, NULL, 0);
    
    return (code == AT_RESP_OK) ? 0 : -1;
}

int at_parse_response(const char *resp, at_response_t *code, char *error_msg, size_t err_len)
{
    if (!resp || !code)
        return -1;
    
    if (strstr(resp, "OK")) {
        *code = AT_RESP_OK;
    } else if (strstr(resp, "ERROR")) {
        *code = AT_RESP_ERROR;
    } else if (strstr(resp, "+CME ERROR")) {
        *code = AT_RESP_CME_ERROR;
        if (error_msg && err_len)
            strlcpy(error_msg, "Modem error", err_len);
    } else if (strstr(resp, "+CMS ERROR")) {
        *code = AT_RESP_CMS_ERROR;
        if (error_msg && err_len)
            strlcpy(error_msg, "SMS error", err_len);
    } else {
        *code = AT_RESP_TIMEOUT;
    }
    
    return 0;
}

int at_set_callback(void (*cb)(const char *line, void *user), void *user_ctx)
{
    return 0;
}