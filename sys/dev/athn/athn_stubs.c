#include <sys/param.h>
#include <sys/sockio.h>
//#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/queue.h>
//#include <sys/timeout.h>
#include <sys/conf.h>
//#include <linux/device.h>
#include <sys/stdint.h>	/* uintptr_t */
#include <sys/endian.h>
#include <openbsd/openbsd_mbuf.h>

// #if NBPFILTER > 0
// #include <net/bpf.h>
// #endif
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <netinet/in.h>
//#include <netinet/if_ether.h>

// #include <linux/ieee80211.h>
#include <net/if_arp.h>
#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_amrr.h>
#include <net80211/ieee80211_ra.h>
#include <net80211/ieee80211_radiotap.h>

// #include <dev/ic/athnreg.h>
// #include <dev/ic/athnvar.h>
#include "athnreg.h"
#include "athnvar.h"
#include "openbsd_adapt.h"

int		ar5416_attach(struct athn_softc *sc)
{
	return 0;
}

int		ar9280_attach(struct athn_softc *sc)
{
	return 0;
}

int		ar9285_attach(struct athn_softc *sc)
{
	return 0;
}

int		ar9287_attach(struct athn_softc *sc)
{
	return 0;
}

int		ar9380_attach(struct athn_softc *sc)
{
	return 0;
}

int		ar5416_init_calib(struct athn_softc *sc,
struct ieee80211_channel *c, struct ieee80211_channel *extc)
{
	return 0;
}

int		ar9285_init_calib(struct athn_softc *sc,
struct ieee80211_channel *c, struct ieee80211_channel *extc)
{
	return 0;
}

int		ar9003_init_calib(struct athn_softc *sc)
{
	return 0;
}

void		ar9285_pa_calib(struct athn_softc *sc)
{
}

void		ar9271_pa_calib(struct athn_softc *sc)
{
}

void		ar9287_1_3_enable_async_fifo(struct athn_softc *sc)
{
}

void		ar9287_1_3_setup_async_fifo(struct athn_softc *sc)
{
}

void		ar9003_reset_txsring(struct athn_softc *sc)
{
}