#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/conf.h>
#include <sys/stdint.h>	/* uintptr_t */
#include <sys/endian.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/ethernet.h>
#include <net/if_media.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_amrr.h>
#include <net80211/ieee80211_radiotap.h>

#include "athnreg.h"
#include "athnvar.h"


int		ar9285_attach(struct athn_softc *sc)
{
	printf("%s is stub", __FUNCTION__);
	return 0;
}

int		ar9285_init_calib(struct athn_softc *sc,
struct ieee80211_channel *c, struct ieee80211_channel *extc)
{
	printf("%s is stub", __FUNCTION__);
	return 0;
}

void		ar9271_pa_calib(struct athn_softc *sc)
{
	printf("%s is stub", __FUNCTION__);
}
