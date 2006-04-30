#ifndef HOSTAPD_CTRL_H
#define HOSTAPD_CTRL_H

struct hostapd_ctrl;

struct hostapd_ctrl * hostapd_ctrl_open(const char *ctrl_path);
void hostapd_ctrl_close(struct hostapd_ctrl *ctrl);
int hostapd_ctrl_request(struct hostapd_ctrl *ctrl, char *cmd, size_t cmd_len,
			 char *reply, size_t *reply_len,
			 void (*msg_cb)(char *msg, size_t len));
int hostapd_ctrl_attach(struct hostapd_ctrl *ctrl);
int hostapd_ctrl_detach(struct hostapd_ctrl *ctrl);
int hostapd_ctrl_recv(struct hostapd_ctrl *ctrl, char *reply,
		      size_t *reply_len);
int hostapd_ctrl_pending(struct hostapd_ctrl *ctrl);
int hostapd_ctrl_get_fd(struct hostapd_ctrl *ctrl);

#endif /* HOSTAPD_CTRL_H */
