#ifndef ACCOUNTING_H
#define ACCOUNTING_H


void accounting_sta_start(hostapd *hapd, struct sta_info *sta);
void accounting_sta_interim(hostapd *hapd, struct sta_info *sta);
void accounting_sta_stop(hostapd *hapd, struct sta_info *sta);
void accounting_sta_get_id(struct hostapd_data *hapd, struct sta_info *sta);
int accounting_init(hostapd *hapd);
void accounting_deinit(hostapd *hapd);


#endif /* ACCOUNTING_H */
