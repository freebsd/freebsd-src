/*
 * $RISS: if_arl/dev/arl/if_arl.c,v 1.7 2004/03/16 04:43:27 count Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"

#ifdef INET
#define ARLCACHE
#endif

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/proc.h>
#include <sys/conf.h>

#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_media.h>
#include <net/ethernet.h>

#include <net/if_ieee80211.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif

#include <net/bpf.h>

#include <machine/clock.h>

#include <dev/arl/if_arlreg.h>

/*#define DEBUG */
#ifdef DEBUG
#define D(x)	{printf("arl%d: ", sc->arl_unit); printf x; }
#else
#define D(x)
#endif

/*
 * channel attention
 */
#define ARL_CHANNEL(sc) \
	{ \
		D(("channel ctrl %x reg %x\n", sc->arl_control, ar->controlRegister)); \
		ar->controlRegister = (sc->arl_control ^= ARL_CHANNEL_ATTENTION); \
	}

/*
 * Check registration
 */
#define ARL_CHECKREG(sc) (ar->registrationMode && ar->registrationStatus == 0)

#define GET_ARL_PARAM(name) (arcfg.name = ar->name)
#define SET_ARL_PARAM(name) (ar->name = arcfg.name)

#ifndef BPF_MTAP
#define BPF_MTAP(_ifp,_m)					\
	do {							\
		if ((_ifp)->if_bpf)				\
			bpf_mtap((_ifp), (_m));			\
	} while (0)
#endif

#if __FreeBSD_version < 500100
#define BROADCASTADDR	(etherbroadcastaddr)
#define _ARL_CURPROC	(curproc)
#else
#define BROADCASTADDR	(sc->arpcom.ac_if.if_broadcastaddr)
#define _ARL_CURPROC	(curthread)
#endif

static void	arl_hwreset	(struct arl_softc *);
static void	arl_reset	(struct arl_softc *);
static int	arl_ioctl	(struct ifnet *, u_long, caddr_t);
static void	arl_init	(void *);
static void	arl_start	(struct ifnet *);

static void	arl_watchdog	(struct ifnet *);
static void	arl_waitreg	(struct ifnet *);

static void	arl_enable	(struct arl_softc *);
static void	arl_config	(struct arl_softc *);
static int	arl_command	(struct arl_softc *);
static void	arl_put		(struct arl_softc *);

static void	arl_read	(struct arl_softc *, caddr_t, int);
static void	arl_recv	(struct arl_softc *);
static struct mbuf* arl_get	(caddr_t, int, int, struct ifnet *);

#ifdef ARLCACHE
static void	arl_cache_store	(struct arl_softc *, struct ether_header *,
					u_int8_t, u_int8_t, int);
#endif

static int  arl_media_change	(struct ifnet *);
static void arl_media_status	(struct ifnet *, struct ifmediareq *);
static void arl_read_config	(struct arl_softc *);

devclass_t	arl_devclass;

u_int8_t rate2media[4] = {
	IFM_IEEE80211_DS354k,
	IFM_IEEE80211_DS512k,
	IFM_IEEE80211_DS1,
	IFM_IEEE80211_DS2
};

/*
 * Copy config values to local cache
 */
static void
arl_read_config(sc)
	struct arl_softc *sc;
{
	bzero(&arcfg, sizeof(arcfg));

	bcopy(ar->lanCardNodeId, arcfg.lanCardNodeId,
		sizeof(ar->lanCardNodeId));
	bcopy(ar->specifiedRouter, arcfg.specifiedRouter,
		sizeof(ar->specifiedRouter));
	GET_ARL_PARAM(hardwareType);
	GET_ARL_PARAM(majorHardwareVersion);
	GET_ARL_PARAM(minorHardwareVersion);
	GET_ARL_PARAM(radioModule);
	GET_ARL_PARAM(channelSet);
	if (!arcfg.channelSet)
		arcfg.channelSet = ar->defaultChannelSet;
	GET_ARL_PARAM(channelNumber);
	GET_ARL_PARAM(spreadingCode);
	GET_ARL_PARAM(priority);
	GET_ARL_PARAM(receiveMode);
	arcfg.registrationMode = 1;	/* set default TMA mode */
	arcfg.txRetry = 0;

	bcopy(ar->name, arcfg.name, ARLAN_NAME_SIZE);
	bcopy(ar->systemId, arcfg.sid, 4 * sizeof(arcfg.sid[0]));
}

/*
 * Attach device
 */
int
arl_attach(dev)
	device_t	dev;
{
	struct arl_softc* sc = device_get_softc(dev);
	struct ifnet *ifp = &sc->arpcom.ac_if;
	int		attached, configured = 0;

	D(("attach\n"));

	configured = ar->configuredStatusFlag;
	attached = (ifp->if_softc != 0);

	if (!configured && bootverbose)
		device_printf(dev, "card is not configured\n");
	else
		arl_read_config(sc);

	arl_reset(sc);

	/* Read config for default values if card was not configured */
	if (!configured)
		arl_read_config(sc);

	/* Initialize ifnet structure. */
	ifp->if_softc = sc;
#if __FreeBSD_version < 502000
	ifp->if_unit = sc->arl_unit;
	ifp->if_name = "arl";
#else
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
#endif
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_start = arl_start;
	ifp->if_ioctl = arl_ioctl;
	ifp->if_watchdog = arl_watchdog;
	ifp->if_init = arl_init;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;
	ifp->if_baudrate = 2000000;
	ifp->if_timer = 0;

	ifmedia_init(&sc->arl_ifmedia, 0, arl_media_change, arl_media_status);
#define ADD(s, o)	ifmedia_add(&sc->arl_ifmedia, \
	IFM_MAKEWORD(IFM_IEEE80211, (s), (o), 0), 0, NULL)
	ADD(IFM_IEEE80211_DS354k, 0);
	ADD(IFM_IEEE80211_DS354k, IFM_IEEE80211_ADHOC);
	ADD(IFM_IEEE80211_DS512k, 0);
	ADD(IFM_IEEE80211_DS512k, IFM_IEEE80211_ADHOC);
	ADD(IFM_IEEE80211_DS1, 0);
	ADD(IFM_IEEE80211_DS1, IFM_IEEE80211_ADHOC);
	ADD(IFM_IEEE80211_DS2, 0);
	ADD(IFM_IEEE80211_DS2, IFM_IEEE80211_ADHOC);
	ifmedia_set(&sc->arl_ifmedia, IFM_MAKEWORD(IFM_IEEE80211,
		rate2media[arcfg.spreadingCode - 1], 0, 0));
#undef ADD

	/*
	 * Attach the interface
	 */
	if (!attached) {
#if __FreeBSD_version < 500100
		ether_ifattach(ifp, ETHER_BPF_SUPPORTED);
#else
		ether_ifattach(ifp, sc->arpcom.ac_enaddr);
#endif
	}

	return (0);
}

/*
 * Hardware reset
 * reset all setting to default (setted ARLANDGS)
 */
static void
arl_hwreset(sc)
	struct arl_softc *sc;
{
	D(("hw reset\n"));

	ar->controlRegister = 1;
	DELAY(ARDELAY1);

	if (arl_wait_reset(sc, 0x24, ARDELAY1))
		arl_stop(sc);

	ar->controlRegister = (sc->arl_control = 1);
	DELAY(ARDELAY1);
}


/*
 * wait arlan command
 */
static int
arl_command(sc)
	struct arl_softc *sc;
{
	int i;       /* long stuppid delay ??? */

	D(("commandByte %x\n", ar->commandByte));

	for (i = 100000; ar->commandByte && i; i--)
		;

	if (i == 0)
		ar->commandByte = 0;

	return (i == 0);
}

/*
 * Enable for recieveng
 */
static void
arl_enable(sc)
	struct arl_softc *sc;
{
	D(("enable\n"));
	sc->arl_control = (ARL_INTERRUPT_ENABLE | ARL_CLEAR_INTERRUPT);
	ar->controlRegister = sc->arl_control;
	arl_command(sc);

	ar->rxStatusVector = 0;
	ar->commandByte = 0x83;
	ar->commandParameter[0] = 1;
	ARL_CHANNEL(sc);
	arl_command(sc);
}

/*
 * reset and set user parameters
 */
static void
arl_reset(sc)
	struct arl_softc *sc;
{
	D(("reset\n"));
	arl_hwreset(sc);

	ar->resetFlag1 = 1;
	bzero((ar), 0x1FF0);              /* fill memory board with 0 */
	ar->resetFlag1 = 0;

	sc->arl_control = 0;

/*	if (arl_wait_reset(sc, 0x168, ARDELAY1))
		return;
 */
#if 1
	{
		int cnt = 0x168;
		int delay = ARDELAY1;

		ar->resetFlag = 0xFF;		/* wish reset */
		ar->controlRegister = 0;	/* unreeze - do it */

		while (ar->resetFlag && cnt--)
			DELAY(delay);

		if (cnt == 0) {
			printf("arl%d: reset timeout\n", sc->arl_unit);
			return;
		}

		D(("reset wait %d\n", 0x168 - cnt));
	}
#endif

	if (ar->diagnosticInfo != 0xff) {
		printf("arl%d: reset error\n", sc->arl_unit);
		return;
	}
	arl_config(sc);
}

/*
 * configure radio parameters
 */
static void
arl_config(sc)
	struct arl_softc *sc;
{
	int i;

	D(("config\n"));

	SET_ARL_PARAM(spreadingCode);
	SET_ARL_PARAM(channelNumber);
	SET_ARL_PARAM(channelSet);
	SET_ARL_PARAM(registrationMode);
	SET_ARL_PARAM(priority);
	SET_ARL_PARAM(receiveMode);

	bcopy(arcfg.sid, ar->systemId, 4 * sizeof(ar->systemId[0]));
	bcopy(arcfg.specifiedRouter, ar->specifiedRouter, ETHER_ADDR_LEN);
	bcopy(arcfg.lanCardNodeId, ar->lanCardNodeId, ETHER_ADDR_LEN);

	bzero(ar->name, ARLAN_NAME_SIZE); /* clear name */
	strncpy(ar->name, arcfg.name, ARLAN_NAME_SIZE);

	ar->diagnosticInfo = 0;
	ar->commandByte = 1;
	ARL_CHANNEL(sc);
	DELAY(ARDELAY1);

	if (arl_command(sc)) {
		D(("config failed\n"));
		return;
	}

	for (i = 0x168; ar->diagnosticInfo == 0 && i; i--)
		DELAY(ARDELAY1);

	if (i == 0) {
		D(("config timeout\n"));
		return;
	}

	if (ar->diagnosticInfo != 0xff) {
		D(("config error\n"));
		return;
	}

	D(("config lanCardNodeId %6D\n", ar->lanCardNodeId, ":"));
	D(("config channel set %d, frequency %d, spread %d, mode %d\n",
	    ar->channelSet,
	    ar->channelNumber,
	    ar->spreadingCode,
	    ar->registrationMode));
	/* clear quality stat */
	bzero(sc->arl_sigcache, MAXARLCACHE * sizeof(struct arl_sigcache));
}

/*
 * Socket Ioctl's.
 */
static int
arl_ioctl(ifp, cmd, data)
	register struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct arl_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	struct ieee80211req *ireq = (struct ieee80211req *)data;
	struct proc *p = _ARL_CURPROC;
	struct arl_req arlan_io;
	int count, s, error = 0;
	u_int8_t tmpstr[IEEE80211_NWID_LEN*2];
	u_int8_t *tmpptr;
	u_int32_t newsid;
	caddr_t user;

	D(("ioctl %lx\n", cmd));
	s = splimp();

	switch (cmd) {
	case SIOCSIFADDR:
	case SIOCGIFADDR:
	case SIOCSIFMTU:
		error = ether_ioctl(ifp, cmd, data);
		break;

	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_flags & IFF_RUNNING))
				arl_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				arl_stop(sc);
		}
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->arl_ifmedia, cmd);
		break;

	case SIOCG80211:
		switch (ireq->i_type) {
		case IEEE80211_IOC_SSID:
			if (ireq->i_val != -1) {
				error = EINVAL;
				break;
			}
			bzero(tmpstr, IEEE80211_NWID_LEN);
			snprintf(tmpstr, IEEE80211_NWID_LEN - 1, "0x%08x",
					*(int *)arcfg.sid);
			ireq->i_len = IEEE80211_NWID_LEN;
			error = copyout(tmpstr, ireq->i_data,
				IEEE80211_NWID_LEN);
			break;
		case IEEE80211_IOC_STATIONNAME:
			ireq->i_len = sizeof(arcfg.name);
			tmpptr = arcfg.name;
			bzero(tmpstr, IEEE80211_NWID_LEN);
			bcopy(tmpptr, tmpstr, ireq->i_len);
			error = copyout(tmpstr, ireq->i_data,
				IEEE80211_NWID_LEN);
			break;
		case IEEE80211_IOC_CHANNEL:
			ireq->i_val = arcfg.channelNumber;
			break;
		case IEEE80211_IOC_POWERSAVE:
			ireq->i_val = (arcfg.registrationMode == 2 ?
				IEEE80211_POWERSAVE_PSP :
				IEEE80211_POWERSAVE_OFF);
			break;
		default:
			error = EINVAL;
			break;
		}
		break;

	case SIOCS80211:
		if ((error = suser(p)))
			break;
		switch (ireq->i_type) {
		case IEEE80211_IOC_SSID:
			if (ireq->i_len > 4) {
				error = EINVAL;
				break;
			}
			bzero(&newsid, sizeof(newsid));
			error = copyin(ireq->i_data,
				(u_char *)(&newsid) + 4 - ireq->i_len,
				ireq->i_len);

			if (error)
				break;

			newsid = htonl(newsid);
			if (newsid < 0  || newsid % 2) {
				error = EINVAL;
				break;
			}

			bcopy(&newsid, arcfg.sid, sizeof(arcfg.sid));
			break;
		case IEEE80211_IOC_STATIONNAME:
			if (ireq->i_len > ARLAN_NAME_SIZE) {
				error = EINVAL;
				break;
			}
			bzero(arcfg.name, ARLAN_NAME_SIZE);
			error = copyin(ireq->i_data, arcfg.name, ireq->i_len);
			break;
		case IEEE80211_IOC_CHANNEL:
			if (ireq->i_val < 0 || ireq->i_val > 5) {
				error = EINVAL;
				break;
			}
			arcfg.channelNumber = ireq->i_val;
			break;
		case IEEE80211_IOC_POWERSAVE:
			switch (ireq->i_val) {
			case IEEE80211_POWERSAVE_OFF:
				if (arcfg.registrationMode == 2)
					arcfg.registrationMode = 1;
				break;
			case IEEE80211_POWERSAVE_ON:
			case IEEE80211_POWERSAVE_PSP:
				arcfg.registrationMode = 2;
				break;
			default:
				error = EINVAL;
				break;
			}
			break;
		default:
			error = EINVAL;
			break;
		}

		if (!error)
			arl_config(sc);

		break;

#define GET_PARAM(name) (arlan_io.cfg.name = arcfg.name)

#define GET_COPY_PARAM(name)						\
	{								\
		bzero(arlan_io.cfg.name, sizeof(arlan_io.cfg.name));	\
		bcopy(arcfg.name, arlan_io.cfg.name, sizeof(arlan_io.cfg.name)); \
	}
	case SIOCGARLALL:
		bzero(&arlan_io, sizeof(arlan_io));
		if (!suser(p)) {
			bcopy(ar->systemId, arlan_io.cfg.sid, 4);
		}

		GET_COPY_PARAM(name);
		GET_COPY_PARAM(lanCardNodeId);
		GET_COPY_PARAM(specifiedRouter);
		GET_PARAM(channelNumber);
		GET_PARAM(channelSet);
		GET_PARAM(spreadingCode);
		GET_PARAM(registrationMode);
		GET_PARAM(hardwareType);
		GET_PARAM(majorHardwareVersion);
		GET_PARAM(minorHardwareVersion);
		GET_PARAM(radioModule);
		GET_PARAM(priority);
		GET_PARAM(receiveMode);
		GET_PARAM(txRetry);

		user = (void *)ifr->ifr_data;
		for (count = 0; count < sizeof(arlan_io); count++)
			if (subyte(user + count, ((char *)&arlan_io)[count]))
				return (EFAULT);
		break;

#define SET_PARAM(name)							\
	do {								\
		if (arlan_io.what_set & ARLAN_SET_##name)		\
			arcfg.name = arlan_io.cfg.name;			\
	} while (0)
#define SET_COPY_PARAM(name)						\
	do {								\
		if (arlan_io.what_set & ARLAN_SET_##name) {		\
			bzero(arcfg.name, sizeof(arcfg.name));		\
			bcopy(arlan_io.cfg.name, arcfg.name, sizeof(arcfg.name)); \
		}							\
	} while (0)

	case SIOCSARLALL:
		if (suser(p))
			break;

		user = (void *)ifr->ifr_data;
		for (count = 0; count < sizeof(arlan_io); count++) {
			if (fubyte(user + count) < 0)
				return (EFAULT);
		}

		bcopy(user, (char *)&arlan_io, sizeof(arlan_io));

		D(("need set 0x%04x\n", arlan_io.what_set));

		if (arlan_io.what_set) {
			SET_COPY_PARAM(name);
			SET_COPY_PARAM(sid);
			SET_COPY_PARAM(specifiedRouter);
			SET_COPY_PARAM(lanCardNodeId);
			SET_PARAM(channelSet);
			SET_PARAM(channelNumber);
			SET_PARAM(spreadingCode);
			SET_PARAM(registrationMode);
			SET_PARAM(priority);
			SET_PARAM(receiveMode);
			SET_PARAM(txRetry);

			arl_config(sc);
		}
#undef SET_COPY_PARAM
#undef SET_PARAM
#undef GET_COPY_PARAM
#undef GET_PARAM
		break;
#ifdef ARLCACHE
	case SIOCGARLQLT:
		user = (void *)ifr->ifr_data;
		for (count = 0; count < sizeof(sc->arl_sigcache); count++) {
			if (fubyte(user + count) < 0)
				return (EFAULT);
		}
		while (ar->interruptInProgress) ; /* wait */
		bcopy(&(sc->arl_sigcache), (void *)ifr->ifr_data, sizeof(sc->arl_sigcache));
		break;
#endif
	case SIOCGARLSTB:
		user = (void *)ifr->ifr_data;
		for (count = 0; count < sizeof(struct arl_stats); count++) {
			if (fubyte(user + count) < 0)
				return (EFAULT);
		}

		while (ar->lancpuLock) ;
		ar->hostcpuLock = 1;
		bcopy(&(ar->stat), (void *)ifr->ifr_data, sizeof(struct arl_stats));
		ar->hostcpuLock  = 0;
		break;

	default:
		error = EINVAL;
	}

	splx(s);

	return (error);
}

/*
 * Wait registration
 */
static void
arl_waitreg(ifp)
	struct ifnet		*ifp;
{
	struct arl_softc	*sc = ifp->if_softc;

	D(("wait reg\n"));

	if (ifp->if_flags & IFF_RUNNING) {
		if (ARL_CHECKREG(sc)) {
			/* wait registration */
			D(("wait registration\n"));
			ifp->if_watchdog = arl_waitreg;
			ifp->if_timer = 2;
		} else {
			/* registration restored */
			D(("registration restored\n"));
			ifp->if_timer = 0;
			arl_init(sc);
		}
	}
}

/*
 * Handle transmit timeouts.
 */
static void
arl_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct arl_softc        *sc = ifp->if_softc;

	if (!(ifp->if_flags & IFF_RUNNING))
		return;

	D(("device timeout\n"));

	if (ARL_CHECKREG(sc)) {
		/* Lost registratoin */
		D(("timeout lost registration\n"));
		ifp->if_watchdog = arl_waitreg;
		ifp->if_timer = 2;
	}
}

/*
 * Initialize
 */
static void
arl_init(xsc)
	void *xsc;
{
	struct arl_softc *sc = xsc;
	struct ifnet *ifp = &sc->arpcom.ac_if;
	int s;

	D(("init\n"));

	s = splimp();

	if (ARL_CHECKREG(sc))
		arl_reset(sc);

	arl_enable(sc);

	/* set flags */

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	arl_start(ifp);

	splx(s);
	D(("init done\n"));
}

/*
 * Put buffer into arlan buffer and start transmit
 */
static void
arl_put(sc)
	struct arl_softc *sc;
{
	struct arl_tx_param txp;
	int i;

	if (ARL_CHECKREG(sc))
		sc->arpcom.ac_if.if_oerrors++;

	/* copy dst adr */
	for(i = 0; i < 6; i++)
		txp.dest[i] = sc->arl_tx[i];
	txp.clear	= 0;
	txp.retries	= arcfg.txRetry;      /* use default value */
	txp.routing	= 1;
	txp.scrambled	= 0;
	txp.offset	= (intptr_t)ar->txBuffer - (intptr_t)(ar);
	txp.length	= sc->tx_len - ARLAN_HEADER_SIZE;

#ifdef SEND_ARLAN_HEADER
	if (ar->registrationMode != 1)
		txp.length = sc->tx_len;
#endif

	/* copy from internal buffer to adapter memory */
#ifdef SEND_ARLAN_HEADER
	if (ar->registrationMode)
#endif
		bcopy(sc->arl_tx + ARLAN_HEADER_SIZE,
		      ar->txBuffer,
		      sc->tx_len - ARLAN_HEADER_SIZE);
#ifdef SEND_ARLAN_MODE
	else
		bcopy(sc->arl_tx, ar->txBuffer, sc->tx_len);
#endif

	/* copy command parametr */
	bcopy(&txp, ar->commandParameter, 14);
	ar->commandByte = 0x85;       /* send command */
	ARL_CHANNEL(sc);
	if (arl_command(sc))
		sc->arpcom.ac_if.if_oerrors++;
}

/*
 * start output
 */
static void
arl_start(ifp)
	struct ifnet		*ifp;
{
	struct arl_softc	*sc;
	struct mbuf		*m;
	struct mbuf		*m0 = NULL;

	sc = ifp->if_softc;

	D(("start\n"));

	/* Don't do anything if output is active */
	if (ifp->if_flags & IFF_OACTIVE)
		return;

	/* Dequeue the next datagram */
	IF_DEQUEUE(&ifp->if_snd, m0);

	/* If there's nothing to send, return. */
	if (m0 != NULL) {
		ifp->if_flags |= IFF_OACTIVE;

		/* Copy the datagram to the buffer. */
		sc->tx_len = 0;
		for(m = m0; m != NULL; m = m->m_next) {
			if (m->m_len == 0)
				continue;
			bcopy(mtod(m, caddr_t),
			      sc->arl_tx + sc->tx_len, m->m_len);
			sc->tx_len += m->m_len;
		}

		/* if packet size is less than minimum ethernet frame size,
		 * pad it with zeroes to that size */
		if (sc->tx_len < ETHER_MIN_LEN) {
			bzero(sc->arl_tx + sc->tx_len, ETHER_MIN_LEN - sc->tx_len);
			sc->tx_len = ETHER_MIN_LEN;
		}

		/* Give the packet to the bpf, if any */
		BPF_MTAP(ifp, m0);

		m_freem(m0);

		/* Now transmit the datagram */
		ifp->if_timer = 1;        /* wait 1 sec */

		ifp->if_watchdog = arl_watchdog;
		arl_put(sc);
	}
}

/*
 * stop interface
 */
void
arl_stop(sc)
	struct arl_softc *sc;
{
	struct ifnet	*ifp;
	int		s;

	s = splimp();

	ifp = &sc->arpcom.ac_if;

	ifp->if_timer = 0;        /* disable timer */
	ifp->if_flags &= ~(IFF_RUNNING|IFF_OACTIVE);
	/*  arl_hwreset(unit);  */
	sc->rx_len = 0;
	sc->tx_len = 0;
	/* disable interrupt */
	ar->controlRegister = 0;

	splx(s);
}

/*
 * Pull read data off a interface.
 * Len is length of data, with local net header stripped.
 */
static struct mbuf*
arl_get(buf, totlen, off0, ifp)
	caddr_t		buf;
	int		totlen;
	int		off0;
	struct ifnet *	ifp;
{
	struct mbuf *top, **mp, *m;
	int off = off0, len;
	caddr_t cp = buf;
	char *epkt;

	cp = buf;
	epkt = cp + totlen;

	if (off) {
		cp += off + 2 * sizeof(u_short);
		totlen -= 2 * sizeof(u_short);
	}

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == 0)
		return (0);
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = totlen;
	m->m_len = MHLEN;
	top = 0;
	mp = &top;
	while (totlen > 0) {
		if (top) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == 0) {
				m_freem(top);
				return (0);
			}
			m->m_len = MLEN;
		}
		len = min(totlen, epkt - cp);
		if (len >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if (m->m_flags & M_EXT)
				m->m_len = len = min(len, MCLBYTES);
			else
				len = m->m_len;
		} else {
			/*
			 *			  * Place initial small packet/header at end of mbuf.
			 *						   */
			if (len < m->m_len) {
				if (top == 0 && len + max_linkhdr <= m->m_len)
					m->m_data += max_linkhdr;
				m->m_len = len;
			} else
				len = m->m_len;
		}
		bcopy(cp, mtod(m, caddr_t), (unsigned)len);
		cp += len;
		*mp = m;
		mp = &m->m_next;
		totlen -= len;
		if (cp == epkt)
			cp = buf;
	}

	return (top);
}

/* ------------------------------------------------------------------
 *  * Pass a packet up to the higher levels.
 *   */
static void
arl_read(sc, buf, len)
	struct arl_softc	*sc;
	caddr_t			buf;
	int			len;
{
	register struct ether_header *eh;
	struct ifnet *ifp = &sc->arpcom.ac_if;
	struct mbuf *m;

	eh = (struct ether_header *)buf;
	/*
	 * Check if there's a bpf filter listening on this interface.
	 * If so, hand off the raw packet to bpf.
	 */
	if (ifp->if_bpf) {
		/*
		 * Note that the interface cannot be in promiscuous mode if
		 * there are no bpf listeners.  And if el are in promiscuous
		 * mode, el have to check if this packet is really ours.
		 *
		 * This test does not support multicasts.
		 */
		if ((ifp->if_flags & IFF_PROMISC)
		   && bcmp(eh->ether_dhost, sc->arpcom.ac_enaddr,
			   sizeof(eh->ether_dhost)) != 0
		   && bcmp(eh->ether_dhost, BROADCASTADDR,
			   sizeof(eh->ether_dhost)) != 0)
			return;
	}
	/*
	 * Pull packet off interface.
	 */
#if __FreeBSD_version < 500100
	buf += sizeof(struct ether_header);
	len -= sizeof(struct ether_header);
#endif
	m = arl_get(buf, len, 0, ifp);
	if (m == 0)
		return;

#ifdef ARLCACHE
	arl_cache_store(sc, eh, ar->rxQuality & 0x0f,
			(ar->rxQuality & 0xf0) >> 4, ARLCACHE_RX);
#endif

#if __FreeBSD_version < 500100
	ether_input(ifp, eh, m);
#else
	(*ifp->if_input)(ifp, m);
#endif
}

/*
 * get packet from adapter
 */
static void
arl_recv(sc)
	struct arl_softc *sc;
{
	sc->rx_len = ar->rxLength;

	if (sc->rx_len) {
#ifdef SEND_ARLAN_HEADER
		if (ar->registrationMode == 1) {
#endif
			bcopy(ar->ultimateDestAddress, sc->arl_rx, 6);
			bcopy(ar->rxSrc, (char*)sc->arl_rx + 6, 6);
			bcopy((char *)(ar) + ar->rxOffset,
			      (char *)sc->arl_rx + 12,
			      sc->rx_len);
			sc->rx_len += ARLAN_HEADER_SIZE;
#ifdef SEND_ARLAN_HEADER
		} else {
			bcopy((char *)(ar) + ar->rxOffset,
			      (char *)sc->arl_rx, sc->rx_len);
		}
#endif
	}
}

/*
 * Ethernet interface interrupt processor
 */
void
arl_intr(arg)
	void *arg;
{
	register struct arl_softc *sc = (struct arl_softc *) arg;
	struct ifnet *ifp = &sc->arpcom.ac_if;

	/* enable interrupt */
	ar->controlRegister = (sc->arl_control & ~ARL_CLEAR_INTERRUPT);
	ar->controlRegister = (sc->arl_control | ARL_CLEAR_INTERRUPT);

	if (ar->txStatusVector) {
		if (ar->txStatusVector != 1)
			sc->arpcom.ac_if.if_collisions++;
		ifp->if_timer = 0;     /* disable timer */
		ifp->if_flags &= ~IFF_OACTIVE;
		arl_start(ifp);
		ar->txStatusVector = 0;
#ifdef ARLCACHE
		arl_cache_store(sc,
			(struct ether_header *)(sc->arl_tx),
			ar->txAckQuality & 0x0f,
			(ar->txAckQuality & 0xf0) >> 4, ARLCACHE_TX);
#endif
	}

	if (ar->rxStatusVector) {
		if (ar->rxStatusVector == 1) {   /* it is data frame */
			arl_recv(sc);
			arl_read(sc, sc->arl_rx, sc->rx_len);
			ifp->if_opackets++;
		}
		ar->rxStatusVector = 0;

		ar->commandByte = 0x83;
		ar->commandParameter[0] = 1;
		ARL_CHANNEL(sc);
		if (arl_command(sc))
			ifp->if_ierrors++;
	}

	return;
}

/*
 * waiting for resetFlag dropped
 */
int
arl_wait_reset(sc, cnt, delay)
	struct arl_softc *sc;
	int cnt;
	int delay;
{
	D(("wait_reset cnt=%d delay=%d\n", cnt, delay));

	ar->resetFlag = 1;		/* wish reset */
	ar->controlRegister = 0;	/* unreeze - do it */

	while (ar->resetFlag && cnt--)
		DELAY(delay);

	D(("reset done. %d cycles left\n", cnt));

	if (cnt == 0)
		printf("arl%d: reset failed\n", sc->arl_unit);

	return (cnt == 0);
}

/*
 * Allocate an irq resource with the given resource id
 */
int
arl_alloc_irq(dev, rid, flags)
	device_t dev;
	int rid;
	int flags;
{
	struct arl_softc *sc = device_get_softc(dev);
	struct resource *res;

	res = bus_alloc_resource_any(
	    dev, SYS_RES_IRQ, &rid, (RF_ACTIVE | flags));
	if (res) {
		sc->irq_rid = rid;
		sc->irq_res = res;
		return (0);
	} else
		return (ENOENT);
}

/*
 * Allocate an memory resource with the given resource id
 */
int
arl_alloc_memory(dev, rid, size)
	device_t dev;
	int rid;
	int size;
{
	struct arl_softc *sc = device_get_softc(dev);
	struct resource *res;

	res = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid,
				 0ul, ~0ul, size, RF_ACTIVE);
	if (res) {
		sc->mem_rid = rid;
		sc->mem_res = res;
		return (0);
	} else
		return (ENOENT);
}

/*
 * Release all resources
 */
void
arl_release_resources(dev)
	device_t dev;
{
	struct arl_softc *sc = device_get_softc(dev);

	if (sc->mem_res) {
		bus_release_resource(dev, SYS_RES_MEMORY,
				     sc->mem_rid, sc->mem_res);
		sc->mem_res = 0;
	}
	if (sc->irq_res) {
		bus_release_resource(dev, SYS_RES_IRQ,
				     sc->irq_rid, sc->irq_res);
		sc->irq_res = 0;
	}
}

#ifdef ARLCACHE
static void
arl_cache_store(sc, eh, level, quality, dir)
	struct arl_softc *sc;
	struct ether_header *eh;
	u_int8_t level;
	u_int8_t quality;
	int	dir;
{
	int i;
	static int cache_slot = 0;
	static int wrapindex = 0;
	u_int8_t zero[6] = {0, 0, 0, 0, 0, 0};
	u_char *mac;

	if ((ntohs(eh->ether_type) != ETHERTYPE_IP)) {
		return;
	}

	mac = (dir == ARLCACHE_RX ? eh->ether_shost : eh->ether_dhost);

	for (i = 0; i < MAXARLCACHE; i++) {
		if (!bcmp(zero, sc->arl_sigcache[i].macsrc, 6) ||
		    !bcmp(mac, sc->arl_sigcache[i].macsrc, 6))
			break;
	}

	if (i < MAXARLCACHE)
		cache_slot = i;
	else {
		if (wrapindex == MAXARLCACHE)
			wrapindex = 0;
		cache_slot = wrapindex++;
	}

	bcopy(dir == ARLCACHE_RX ? eh->ether_shost : eh->ether_dhost,
	    sc->arl_sigcache[cache_slot].macsrc, 6);

	sc->arl_sigcache[cache_slot].level[dir] = level;
	sc->arl_sigcache[cache_slot].quality[dir] = quality;
}
#endif

static int
arl_media_change(ifp)
	struct ifnet	*ifp;
{
	struct arl_softc *sc = ifp->if_softc;
	int otype = arcfg.registrationMode;
	int orate = arcfg.spreadingCode;
	int nrate, i;

	nrate = IFM_SUBTYPE(sc->arl_ifmedia.ifm_cur->ifm_media);

	for(i = 1; i <= 4; i++) {
		if (rate2media[i - 1] == nrate)
			break;
	}

	if (i == 5)
		return (EINVAL);

	arcfg.spreadingCode = i;

	/* XXX Need fix for PSP mode */
	if ((sc->arl_ifmedia.ifm_cur->ifm_media & IFM_IEEE80211_ADHOC) != 0)
		arcfg.registrationMode = 0;
	else
		arcfg.registrationMode = 1;

	if (otype != arcfg.registrationMode ||
	    orate != arcfg.spreadingCode)
		arl_config(sc);

	return (0);
}

static void
arl_media_status(ifp, imr)
	struct ifnet		*ifp;
	struct ifmediareq	*imr;
{
	struct arl_softc	*sc = ifp->if_softc;

	imr->ifm_active = IFM_IEEE80211;

	if (arcfg.registrationMode == 0)
		imr->ifm_active |= IFM_IEEE80211_ADHOC;

	imr->ifm_active |= IFM_MAKEWORD(IFM_IEEE80211,
				rate2media[arcfg.spreadingCode - 1], 0, 0);
	imr->ifm_status = IFM_AVALID;
	if (!ARL_CHECKREG(sc))
		imr->ifm_status |= IFM_ACTIVE;
}
