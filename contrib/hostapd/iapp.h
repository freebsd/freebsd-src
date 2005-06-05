#ifndef IAPP_H
#define IAPP_H

struct iapp_data;

#ifdef CONFIG_IAPP

void iapp_new_station(struct iapp_data *iapp, struct sta_info *sta);
struct iapp_data * iapp_init(struct hostapd_data *hapd, const char *iface);
void iapp_deinit(struct iapp_data *iapp);

#else /* CONFIG_IAPP */

static inline void iapp_new_station(struct iapp_data *iapp,
				    struct sta_info *sta)
{
}

static inline struct iapp_data * iapp_init(struct hostapd_data *hapd,
					   const char *iface)
{
	return NULL;
}

static inline void iapp_deinit(struct iapp_data *iapp)
{
}

#endif /* CONFIG_IAPP */

#endif /* IAPP_H */
