#ifndef AR9285
#define AR9285

int	ar9285_attach(struct athn_softc *);
void	ar9285_setup(struct athn_softc *);
void	ar9285_swap_rom(struct athn_softc *);
const	struct ar_spur_chan *ar9285_get_spur_chans(struct athn_softc *, int);
void	ar9285_init_from_rom(struct athn_softc *, struct ieee80211_channel *,
	    struct ieee80211_channel *);
void	ar9285_pa_calib(struct athn_softc *);
void	ar9271_pa_calib(struct athn_softc *);
int	ar9285_cl_cal(struct athn_softc *, struct ieee80211_channel *,
	    struct ieee80211_channel *);
void	ar9271_load_ani(struct athn_softc *);
int	ar9285_init_calib(struct athn_softc *, struct ieee80211_channel *,
	    struct ieee80211_channel *);
void	ar9285_get_pdadcs(struct athn_softc *, struct ieee80211_channel *,
	    int, uint8_t, uint8_t *, uint8_t *);
void	ar9285_set_power_calib(struct athn_softc *,
	    struct ieee80211_channel *);
void	ar9285_set_txpower(struct athn_softc *, struct ieee80211_channel *,
	    struct ieee80211_channel *);

#endif	/* AR9285 */