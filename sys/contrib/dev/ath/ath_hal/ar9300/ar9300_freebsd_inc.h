#ifndef	__AR9300_FREEBSD_INC_H__
#define	__AR9300_FREEBSD_INC_H__

/*
 * Define some configuration entries for the AR9300 HAL, so #if entries
 * don't have to be removed.
 */
#define ATH_DRIVER_SIM          0       /* SIM */
#define ATH_WOW                 0       /* Wake on Wireless */
#define ATH_SUPPORT_MCI         1       /* MCI btcoex */
#define ATH_SUPPORT_AIC         0       /* XXX to do with btcoex? */
#define AH_NEED_TX_DATA_SWAP    0       /* TX descriptor swap? */
#define AH_NEED_RX_DATA_SWAP    0       /* TX descriptor swap? */
#define ATH_SUPPORT_WIRESHARK   0       /* Radiotap HAL code */
#define AH_SUPPORT_WRITE_EEPROM 0       /* EEPROM write support */
#define ATH_SUPPORT_WAPI        0       /* China WAPI support */
#define ATH_ANT_DIV_COMB        1       /* Antenna combining */
#define ATH_SUPPORT_RAW_ADC_CAPTURE     0       /* Raw ADC capture support */
#define ATH_TRAFFIC_FAST_RECOVER        0       /* XXX not sure yet */
#define ATH_SUPPORT_SPECTRAL    0       /* Spectral scan support */
#define ATH_BT_COEX             1       /* Enable BT Coex code */
#define ATH_PCIE_ERROR_MONITOR  0       /* ??? */
#define ATH_SUPPORT_CRDC        0       /* ??? */
#define ATH_LOW_POWER_ENABLE    0       /* ??? */
#define ATH_SUPPORT_VOW_DCS     0       /* Video over wireless dynamic channel select */
#define REMOVE_PKT_LOG          1
#define ATH_VC_MODE_PROXY_STA   0       /* Azimuth + proxysta? */
#define ATH_GEN_RANDOMNESS      0
#define __PKT_SERIOUS_ERRORS__  0
#define HAL_INTR_REFCOUNT_DISABLE       1       /* XXX wha? And atomics in the HAL!? */
#define UMAC_SUPPORT_SMARTANTENNA       0       /* sigh.. */
#define ATH_SMARTANTENNA_DISABLE_JTAG   0
#define ATH_SUPPORT_WIRESHARK           0
#define ATH_SUPPORT_WIFIPOS     0
#define ATH_SUPPORT_PAPRD       1
#define ATH_SUPPORT_TxBF        0

/* XXX need to reverify these; they came in with qcamain */
#define ATH_SUPPORT_FAST_CC 0
#define ATH_SUPPORT_RADIO_RETENTION 0
#define ATH_SUPPORT_CAL_REUSE 0

#define ATH_WOW_OFFLOAD 0

#define HAL_NO_INTERSPERSED_READS

/* Required or things will probe/attach, but not work right */
#define	AH_SUPPORT_OSPREY		1
#define	AH_SUPPORT_POSEIDON		1
#define	AH_SUPPORT_AR9300		1

/* These are the embedded boards; we don't currently support these */
//#define AH_SUPPORT_HORNET               1
//#define AH_SUPPORT_WASP                 1
//#define AH_SUPPORT_SCORPION             1

#define FIX_NOISE_FLOOR                 1

/* XXX this needs to be removed! No atomics in the HAL! */
typedef int os_atomic_t;                /* XXX shouldn't do atomics here! */
#define OS_ATOMIC_INC(a)        (*a)++
#define OS_ATOMIC_DEC(a)        (*a)--

/*
 * HAL definitions which aren't necessarily for public consumption (yet).
 */

enum {
	HAL_TRUE_CHIP = 1,
	HAL_MAC_TO_MAC_EMU,
	HAL_MAC_BB_EMU,
};

/* HAL_KEY_TYPE */
enum {
	HAL_KEY_PROXY_STA_MASK = 0x10,
};

typedef enum {
	HAL_SMPS_DEFAULT = 0,
	HAL_SMPS_SW_CTRL_LOW_PWR,       /* Software control, low power setting */
	HAL_SMPS_SW_CTRL_HIGH_PWR,      /* Software control, high power setting */
	HAL_SMPS_HW_CTRL                /* Hardware Control */
} HAL_SMPS_MODE;

/*
 * Green Tx, Based on different RSSI of Received Beacon thresholds,
 * using different tx power by modified register tx power related values.
 * The thresholds are decided by system team.
 */
#define	GreenTX_thres1	56	/* in dB */
#define	GreenTX_thres2	36	/* in dB */

typedef enum {
	HAL_RSSI_TX_POWER_NONE		= 0,
	HAL_RSSI_TX_POWER_SHORT		= 1,	/* short range, reduce OB/DB bias current and disable PAL */
	HAL_RSSI_TX_POWER_MIDDLE	= 2,	/* middle range, reduce OB/DB bias current and PAL is enabled */
	HAL_RSSI_TX_POWER_LONG		= 3,	/* long range, orig. OB/DB bias current and PAL is enabled */
} HAL_RSSI_TX_POWER;

struct  dfs_pulse {
	u_int32_t	rp_numpulses    ;       /* Num of pulses in radar burst */
	u_int32_t	rp_pulsedur;            /* Duration of each pulse in usecs */
	u_int32_t	rp_pulsefreq;           /* Frequency of pulses in burst */
	u_int32_t	rp_max_pulsefreq;       /* Frequency of pulses in burst */
	u_int32_t	rp_patterntype;         /* fixed or variable pattern type*/
	u_int32_t	rp_pulsevar;            /* Time variation of pulse duration for
							  matched filter (single-sided) in usecs */
	u_int32_t	rp_threshold;           /* Threshold for MF output to indicate
							  radar match */
	u_int32_t	rp_mindur;              /* Min pulse duration to be considered for
							  this pulse type */
	u_int32_t	rp_maxdur;              /* Max pusle duration to be considered for
							  this pulse type */
	u_int32_t	rp_rssithresh;          /* Minimum rssi to be considered a radar pulse */
	u_int32_t	rp_meanoffset;          /* Offset for timing adjustment */
	int32_t		rp_rssimargin;          /* rssi threshold margin. In Turbo Mode HW reports rssi 3dBm */
						       /* lower than in non TURBO mode.
							  This will be used to offset that diff.*/
	u_int32_t	rp_ignore_pri_window;
	u_int32_t	rp_pulseid;             /* Unique ID for identifying filter */
};

struct  dfs_staggered_pulse {
       u_int32_t       rp_numpulses;           /* Num of pulses in radar burst */
       u_int32_t       rp_pulsedur;            /* Duration of each pulse in usecs */
       u_int32_t       rp_min_pulsefreq;       /* Frequency of pulses in burst */
       u_int32_t       rp_max_pulsefreq;       /* Frequency of pulses in burst */
       u_int32_t       rp_patterntype;         /* fixed or variable pattern type*/
       u_int32_t       rp_pulsevar;            /* Time variation of pulse duration for
                                                   matched filter (single-sided) in usecs */
       u_int32_t       rp_threshold;           /* Thershold for MF output to indicateC
                                                  radar match */
       u_int32_t       rp_mindur;              /* Min pulse duration to be considered for
                                                  this pulse type */
       u_int32_t       rp_maxdur;              /* Max pusle duration to be considered for
                                                  this pulse type */
       u_int32_t       rp_rssithresh;          /* Minimum rssi to be considered a radar pulse */
       u_int32_t       rp_meanoffset;          /* Offset for timing adjustment */
       int32_t         rp_rssimargin;          /* rssi threshold margin. In Turbo Mode HW reports rssi 3dBm */
                                               /* lower than in non TURBO mode. This will be used to offset that diff.*/
       u_int32_t       rp_pulseid;             /* Unique ID for identifying filter */
       };

struct dfs_bin5pulse {
        u_int32_t       b5_threshold;          /* Number of bin5 pulses to indicate detection */
        u_int32_t       b5_mindur;             /* Min duration for a bin5 pulse */
        u_int32_t       b5_maxdur;             /* Max duration for a bin5 pulse */
        u_int32_t       b5_timewindow;         /* Window over which to count bin5 pulses */
        u_int32_t       b5_rssithresh;         /* Min rssi to be considered a pulse */
        u_int32_t       b5_rssimargin;         /* rssi threshold margin. In Turbo Mode HW reports rssi 3dB */
};

#if 0
/* SPECTRAL SCAN defines begin */
typedef struct {
        u_int16_t       ss_fft_period;  /* Skip interval for FFT reports */
        u_int16_t       ss_period;      /* Spectral scan period */
        u_int16_t       ss_count;       /* # of reports to return from ss_active */
        u_int16_t       ss_short_report;/* Set to report ony 1 set of FFT results */
        u_int8_t        radar_bin_thresh_sel;
        u_int16_t       ss_spectral_pri;                /* are we doing a noise power cal ? */
        int8_t          ss_nf_cal[AH_MAX_CHAINS*2];     /* nf calibrated values for ctl+ext from eeprom */
        int8_t          ss_nf_pwr[AH_MAX_CHAINS*2];     /* nf pwr values for ctl+ext from eeprom */
        int32_t         ss_nf_temp_data;                /* temperature data taken during nf scan */
} HAL_SPECTRAL_PARAM;
#define HAL_SPECTRAL_PARAM_NOVAL        0xFFFF
#define HAL_SPECTRAL_PARAM_ENABLE       0x8000  /* Enable/Disable if applicable */
#endif

/*
 * Noise power data definitions
 * units are: 4 x dBm - NOISE_PWR_DATA_OFFSET (e.g. -25 = (-25/4 - 90) = -96.25 dBm)
 * range (for 6 signed bits) is (-32 to 31) + offset => -122dBm to -59dBm
 * resolution (2 bits) is 0.25dBm
 */
#define NOISE_PWR_DATA_OFFSET           -90 /* dbm - all pwr report data is represented offset by this */
#define INT_2_NOISE_PWR_DBM(_p)         (((_p) - NOISE_PWR_DATA_OFFSET) << 2)
#define NOISE_PWR_DBM_2_INT(_p)         ((((_p) + 3) >> 2) + NOISE_PWR_DATA_OFFSET)
#define NOISE_PWR_DBM_2_DEC(_p)         (((-(_p)) & 3) * 25)
#define N2DBM(_x,_y)                    ((((_x) - NOISE_PWR_DATA_OFFSET) << 2) - (_y)/25)
/* SPECTRAL SCAN defines end */

typedef struct halvowstats {
    u_int32_t   tx_frame_count;
    u_int32_t   rx_frame_count;
    u_int32_t   rx_clear_count;
    u_int32_t   cycle_count;
    u_int32_t   ext_cycle_count;
} HAL_VOWSTATS;

#define HAL_BT_COEX_FLAG_LOW_ACK_PWR        0x00000001
#define HAL_BT_COEX_FLAG_LOWER_TX_PWR       0x00000002
#define HAL_BT_COEX_FLAG_ANT_DIV_ALLOW      0x00000004    /* Check Rx Diversity is allowed */
#define HAL_BT_COEX_FLAG_ANT_DIV_ENABLE     0x00000008    /* Check Diversity is on or off */
#define HAL_BT_COEX_FLAG_MCI_MAX_TX_PWR     0x00000010
#define HAL_BT_COEX_FLAG_MCI_FTP_STOMP_RX   0x00000020

/*
 * Weight table configurations.
 */
#define AR9300_BT_WGHT                     0xcccc4444
#define AR9300_STOMP_ALL_WLAN_WGHT0        0xfffffff0
#define AR9300_STOMP_ALL_WLAN_WGHT1        0xfffffff0
#define AR9300_STOMP_LOW_WLAN_WGHT0        0x88888880
#define AR9300_STOMP_LOW_WLAN_WGHT1        0x88888880
#define AR9300_STOMP_NONE_WLAN_WGHT0       0x00000000
#define AR9300_STOMP_NONE_WLAN_WGHT1       0x00000000
#define AR9300_STOMP_ALL_FORCE_WLAN_WGHT0  0xffffffff   // Stomp BT even when WLAN is idle
#define AR9300_STOMP_ALL_FORCE_WLAN_WGHT1  0xffffffff
#define AR9300_STOMP_LOW_FORCE_WLAN_WGHT0  0x88888888   // Stomp BT even when WLAN is idle
#define AR9300_STOMP_LOW_FORCE_WLAN_WGHT1  0x88888888

#define JUPITER_STOMP_ALL_WLAN_WGHT0       0x01017d01
#define JUPITER_STOMP_ALL_WLAN_WGHT1       0x41414101
#define JUPITER_STOMP_ALL_WLAN_WGHT2       0x41414101
#define JUPITER_STOMP_ALL_WLAN_WGHT3       0x41414141
#define JUPITER_STOMP_LOW_WLAN_WGHT0       0x01017d01
#define JUPITER_STOMP_LOW_WLAN_WGHT1       0x3b3b3b01
#define JUPITER_STOMP_LOW_WLAN_WGHT2       0x3b3b3b01
#define JUPITER_STOMP_LOW_WLAN_WGHT3       0x3b3b3b3b
#define JUPITER_STOMP_LOW_FTP_WLAN_WGHT0   0x01017d01
#define JUPITER_STOMP_LOW_FTP_WLAN_WGHT1   0x013b0101
#define JUPITER_STOMP_LOW_FTP_WLAN_WGHT2   0x3b3b0101
#define JUPITER_STOMP_LOW_FTP_WLAN_WGHT3   0x3b3b013b
#define JUPITER_STOMP_NONE_WLAN_WGHT0      0x01017d01
#define JUPITER_STOMP_NONE_WLAN_WGHT1      0x01010101
#define JUPITER_STOMP_NONE_WLAN_WGHT2      0x01010101
#define JUPITER_STOMP_NONE_WLAN_WGHT3      0x01010101
#define JUPITER_STOMP_ALL_FORCE_WLAN_WGHT0 0x01017d7d
#define JUPITER_STOMP_ALL_FORCE_WLAN_WGHT1 0x7d7d7d01
#define JUPITER_STOMP_ALL_FORCE_WLAN_WGHT2 0x7d7d7d7d
#define JUPITER_STOMP_ALL_FORCE_WLAN_WGHT3 0x7d7d7d7d
#define JUPITER_STOMP_LOW_FORCE_WLAN_WGHT0 0x01013b3b
#define JUPITER_STOMP_LOW_FORCE_WLAN_WGHT1 0x3b3b3b01
#define JUPITER_STOMP_LOW_FORCE_WLAN_WGHT2 0x3b3b3b3b
#define JUPITER_STOMP_LOW_FORCE_WLAN_WGHT3 0x3b3b3b3b

#define MCI_CONCUR_TX_WLAN_WGHT1_MASK      0xff000000
#define MCI_CONCUR_TX_WLAN_WGHT1_MASK_S    24
#define MCI_CONCUR_TX_WLAN_WGHT2_MASK      0x00ff0000
#define MCI_CONCUR_TX_WLAN_WGHT2_MASK_S    16
#define MCI_CONCUR_TX_WLAN_WGHT3_MASK      0x000000ff
#define MCI_CONCUR_TX_WLAN_WGHT3_MASK_S    0
#define MCI_CONCUR_TX_WLAN_WGHT3_MASK2     0x00ff0000
#define MCI_CONCUR_TX_WLAN_WGHT3_MASK2_S   16

#define MCI_QUERY_BT_VERSION_VERBOSE            0
#define MCI_LINKID_INDEX_MGMT_PENDING           1

#define HAL_MCI_FLAG_DISABLE_TIMESTAMP      0x00000001      /* Disable time stamp */

typedef enum mci_message_header {
    MCI_LNA_CTRL     = 0x10,        /* len = 0 */
    MCI_CONT_NACK    = 0x20,        /* len = 0 */
    MCI_CONT_INFO    = 0x30,        /* len = 4 */
    MCI_CONT_RST     = 0x40,        /* len = 0 */
    MCI_SCHD_INFO    = 0x50,        /* len = 16 */
    MCI_CPU_INT      = 0x60,        /* len = 4 */
    MCI_SYS_WAKING   = 0x70,        /* len = 0 */
    MCI_GPM          = 0x80,        /* len = 16 */
    MCI_LNA_INFO     = 0x90,        /* len = 1 */
    MCI_LNA_STATE    = 0x94,
    MCI_LNA_TAKE     = 0x98,
    MCI_LNA_TRANS    = 0x9c,
    MCI_SYS_SLEEPING = 0xa0,        /* len = 0 */
    MCI_REQ_WAKE     = 0xc0,        /* len = 0 */
    MCI_DEBUG_16     = 0xfe,        /* len = 2 */
    MCI_REMOTE_RESET = 0xff         /* len = 16 */
} MCI_MESSAGE_HEADER;

/* Default remote BT device MCI COEX version */
#define MCI_GPM_COEX_MAJOR_VERSION_DEFAULT  3
#define MCI_GPM_COEX_MINOR_VERSION_DEFAULT  0
/* Local WLAN MCI COEX version */
#define MCI_GPM_COEX_MAJOR_VERSION_WLAN     3
#define MCI_GPM_COEX_MINOR_VERSION_WLAN     0

typedef enum mci_gpm_subtype {
    MCI_GPM_BT_CAL_REQ      = 0,
    MCI_GPM_BT_CAL_GRANT    = 1,
    MCI_GPM_BT_CAL_DONE     = 2,
    MCI_GPM_WLAN_CAL_REQ    = 3,
    MCI_GPM_WLAN_CAL_GRANT  = 4,
    MCI_GPM_WLAN_CAL_DONE   = 5,
    MCI_GPM_COEX_AGENT      = 0x0C,
    MCI_GPM_RSVD_PATTERN    = 0xFE,
    MCI_GPM_RSVD_PATTERN32  = 0xFEFEFEFE,
    MCI_GPM_BT_DEBUG        = 0xFF
} MCI_GPM_SUBTYPE_T;

typedef enum mci_gpm_coex_opcode {
    MCI_GPM_COEX_VERSION_QUERY      = 0,
    MCI_GPM_COEX_VERSION_RESPONSE   = 1,
    MCI_GPM_COEX_STATUS_QUERY       = 2,
    MCI_GPM_COEX_HALT_BT_GPM        = 3,
    MCI_GPM_COEX_WLAN_CHANNELS      = 4,
    MCI_GPM_COEX_BT_PROFILE_INFO    = 5,
    MCI_GPM_COEX_BT_STATUS_UPDATE   = 6,
    MCI_GPM_COEX_BT_UPDATE_FLAGS    = 7
} MCI_GPM_COEX_OPCODE_T;

typedef enum mci_gpm_coex_query_type {
    /* WLAN information */
    MCI_GPM_COEX_QUERY_WLAN_ALL_INFO    = 0x01,
    /* BT information */
    MCI_GPM_COEX_QUERY_BT_ALL_INFO      = 0x01,
    MCI_GPM_COEX_QUERY_BT_TOPOLOGY      = 0x02,
    MCI_GPM_COEX_QUERY_BT_DEBUG         = 0x04
} MCI_GPM_COEX_QUERY_TYPE_T;

typedef enum mci_gpm_coex_halt_bt_gpm {
    MCI_GPM_COEX_BT_GPM_UNHALT      = 0,
    MCI_GPM_COEX_BT_GPM_HALT        = 1
} MCI_GPM_COEX_HALT_BT_GPM_T;

typedef enum mci_gpm_coex_profile_type {
    MCI_GPM_COEX_PROFILE_UNKNOWN    = 0,
    MCI_GPM_COEX_PROFILE_RFCOMM     = 1,
    MCI_GPM_COEX_PROFILE_A2DP       = 2,
    MCI_GPM_COEX_PROFILE_HID        = 3,
    MCI_GPM_COEX_PROFILE_BNEP       = 4,
    MCI_GPM_COEX_PROFILE_VOICE      = 5,
    MCI_GPM_COEX_PROFILE_MAX
} MCI_GPM_COEX_PROFILE_TYPE_T;

typedef enum mci_gpm_coex_profile_state {
    MCI_GPM_COEX_PROFILE_STATE_END      = 0,
    MCI_GPM_COEX_PROFILE_STATE_START    = 1
} MCI_GPM_COEX_PROFILE_STATE_T;

typedef enum mci_gpm_coex_profile_role {
    MCI_GPM_COEX_PROFILE_SLAVE      = 0,
    MCI_GPM_COEX_PROFILE_MASTER     = 1
} MCI_GPM_COEX_PROFILE_ROLE_T;

typedef enum mci_gpm_coex_bt_status_type {
    MCI_GPM_COEX_BT_NONLINK_STATUS  = 0,
    MCI_GPM_COEX_BT_LINK_STATUS     = 1
} MCI_GPM_COEX_BT_STATUS_TYPE_T;

typedef enum mci_gpm_coex_bt_status_state {
    MCI_GPM_COEX_BT_NORMAL_STATUS   = 0,
    MCI_GPM_COEX_BT_CRITICAL_STATUS = 1
} MCI_GPM_COEX_BT_STATUS_STATE_T;

#define MCI_GPM_INVALID_PROFILE_HANDLE  0xff

typedef enum mci_gpm_coex_bt_updata_flags_op {
    MCI_GPM_COEX_BT_FLAGS_READ          = 0x00,
    MCI_GPM_COEX_BT_FLAGS_SET           = 0x01,
    MCI_GPM_COEX_BT_FLAGS_CLEAR         = 0x02
} MCI_GPM_COEX_BT_FLAGS_OP_T;

/* MCI GPM/Coex opcode/type definitions */
enum {
    MCI_GPM_COEX_W_GPM_PAYLOAD      = 1,
    MCI_GPM_COEX_B_GPM_TYPE         = 4,
    MCI_GPM_COEX_B_GPM_OPCODE       = 5,
    /* MCI_GPM_WLAN_CAL_REQ, MCI_GPM_WLAN_CAL_DONE */
    MCI_GPM_WLAN_CAL_W_SEQUENCE     = 2,
    /* MCI_GPM_COEX_VERSION_QUERY */
    /* MCI_GPM_COEX_VERSION_RESPONSE */
    MCI_GPM_COEX_B_MAJOR_VERSION    = 6,
    MCI_GPM_COEX_B_MINOR_VERSION    = 7,
    /* MCI_GPM_COEX_STATUS_QUERY */
    MCI_GPM_COEX_B_BT_BITMAP        = 6,
    MCI_GPM_COEX_B_WLAN_BITMAP      = 7,
    /* MCI_GPM_COEX_HALT_BT_GPM */
    MCI_GPM_COEX_B_HALT_STATE       = 6,
    /* MCI_GPM_COEX_WLAN_CHANNELS */
    MCI_GPM_COEX_B_CHANNEL_MAP      = 6,
    /* MCI_GPM_COEX_BT_PROFILE_INFO */
    MCI_GPM_COEX_B_PROFILE_TYPE     = 6,
    MCI_GPM_COEX_B_PROFILE_LINKID   = 7,
    MCI_GPM_COEX_B_PROFILE_STATE    = 8,
    MCI_GPM_COEX_B_PROFILE_ROLE     = 9,
    MCI_GPM_COEX_B_PROFILE_RATE     = 10,
    MCI_GPM_COEX_B_PROFILE_VOTYPE   = 11,
    MCI_GPM_COEX_H_PROFILE_T        = 12,
    MCI_GPM_COEX_B_PROFILE_W        = 14,
    MCI_GPM_COEX_B_PROFILE_A        = 15,
    /* MCI_GPM_COEX_BT_STATUS_UPDATE */
    MCI_GPM_COEX_B_STATUS_TYPE      = 6,
    MCI_GPM_COEX_B_STATUS_LINKID    = 7,
    MCI_GPM_COEX_B_STATUS_STATE     = 8,
    /* MCI_GPM_COEX_BT_UPDATE_FLAGS */
    MCI_GPM_COEX_B_BT_FLAGS_OP      = 10,
    MCI_GPM_COEX_W_BT_FLAGS         = 6
};

#define MCI_GPM_RECYCLE(_p_gpm) \
    {                           \
        *(((u_int32_t *)(_p_gpm)) + MCI_GPM_COEX_W_GPM_PAYLOAD) = MCI_GPM_RSVD_PATTERN32; \
    }
#define MCI_GPM_TYPE(_p_gpm)    \
    (*(((u_int8_t *)(_p_gpm)) + MCI_GPM_COEX_B_GPM_TYPE) & 0xff)
#define MCI_GPM_OPCODE(_p_gpm)  \
    (*(((u_int8_t *)(_p_gpm)) + MCI_GPM_COEX_B_GPM_OPCODE) & 0xff)

#define MCI_GPM_SET_CAL_TYPE(_p_gpm, _cal_type)             \
    {                                                       \
        *(((u_int8_t *)(_p_gpm)) + MCI_GPM_COEX_B_GPM_TYPE) = (_cal_type) & 0xff; \
    }
#define MCI_GPM_SET_TYPE_OPCODE(_p_gpm, _type, _opcode)     \
    {                                                       \
        *(((u_int8_t *)(_p_gpm)) + MCI_GPM_COEX_B_GPM_TYPE) = (_type) & 0xff;     \
        *(((u_int8_t *)(_p_gpm)) + MCI_GPM_COEX_B_GPM_OPCODE) = (_opcode) & 0xff;   \
    }
#define MCI_GPM_IS_CAL_TYPE(_type) ((_type) <= MCI_GPM_WLAN_CAL_DONE)

#define MCI_NUM_BT_CHANNELS     79

#define MCI_GPM_SET_CHANNEL_BIT(_p_gpm, _bt_chan)                   \
    {                                                               \
        if (_bt_chan < MCI_NUM_BT_CHANNELS) {                       \
            *(((u_int8_t *)(_p_gpm)) + MCI_GPM_COEX_B_CHANNEL_MAP + \
                (_bt_chan / 8)) |= 1 << (_bt_chan & 7);             \
        }                                                           \
    }

#define MCI_GPM_CLR_CHANNEL_BIT(_p_gpm, _bt_chan)                   \
    {                                                               \
        if (_bt_chan < MCI_NUM_BT_CHANNELS) {                       \
            *(((u_int8_t *)(_p_gpm)) + MCI_GPM_COEX_B_CHANNEL_MAP + \
                (_bt_chan / 8)) &= ~(1 << (_bt_chan & 7));          \
        }                                                           \
    }

#define HAL_MCI_INTERRUPT_SW_MSG_DONE            0x00000001
#define HAL_MCI_INTERRUPT_CPU_INT_MSG            0x00000002
#define HAL_MCI_INTERRUPT_RX_CHKSUM_FAIL         0x00000004
#define HAL_MCI_INTERRUPT_RX_INVALID_HDR         0x00000008
#define HAL_MCI_INTERRUPT_RX_HW_MSG_FAIL         0x00000010
#define HAL_MCI_INTERRUPT_RX_SW_MSG_FAIL         0x00000020
#define HAL_MCI_INTERRUPT_TX_HW_MSG_FAIL         0x00000080
#define HAL_MCI_INTERRUPT_TX_SW_MSG_FAIL         0x00000100
#define HAL_MCI_INTERRUPT_RX_MSG                 0x00000200
#define HAL_MCI_INTERRUPT_REMOTE_SLEEP_UPDATE    0x00000400
#define HAL_MCI_INTERRUPT_CONT_INFO_TIMEOUT      0x80000000
#define HAL_MCI_INTERRUPT_MSG_FAIL_MASK ( HAL_MCI_INTERRUPT_RX_HW_MSG_FAIL | \
                                          HAL_MCI_INTERRUPT_RX_SW_MSG_FAIL | \
                                          HAL_MCI_INTERRUPT_TX_HW_MSG_FAIL | \
                                          HAL_MCI_INTERRUPT_TX_SW_MSG_FAIL )

#define HAL_MCI_INTERRUPT_RX_MSG_REMOTE_RESET    0x00000001
#define HAL_MCI_INTERRUPT_RX_MSG_LNA_CONTROL     0x00000002
#define HAL_MCI_INTERRUPT_RX_MSG_CONT_NACK       0x00000004
#define HAL_MCI_INTERRUPT_RX_MSG_CONT_INFO       0x00000008
#define HAL_MCI_INTERRUPT_RX_MSG_CONT_RST        0x00000010
#define HAL_MCI_INTERRUPT_RX_MSG_SCHD_INFO       0x00000020
#define HAL_MCI_INTERRUPT_RX_MSG_CPU_INT         0x00000040
#define HAL_MCI_INTERRUPT_RX_MSG_GPM             0x00000100
#define HAL_MCI_INTERRUPT_RX_MSG_LNA_INFO        0x00000200
#define HAL_MCI_INTERRUPT_RX_MSG_SYS_SLEEPING    0x00000400
#define HAL_MCI_INTERRUPT_RX_MSG_SYS_WAKING      0x00000800
#define HAL_MCI_INTERRUPT_RX_MSG_REQ_WAKE        0x00001000
#define HAL_MCI_INTERRUPT_RX_MSG_MONITOR         (HAL_MCI_INTERRUPT_RX_MSG_LNA_CONTROL | \
                                                  HAL_MCI_INTERRUPT_RX_MSG_LNA_INFO    | \
                                                  HAL_MCI_INTERRUPT_RX_MSG_CONT_NACK   | \
                                                  HAL_MCI_INTERRUPT_RX_MSG_CONT_INFO   | \
                                                  HAL_MCI_INTERRUPT_RX_MSG_CONT_RST)

typedef enum mci_bt_state {
    MCI_BT_SLEEP,
    MCI_BT_AWAKE,
    MCI_BT_CAL_START,
    MCI_BT_CAL
} MCI_BT_STATE_T;

/* Type of state query */
typedef enum mci_state_type {
    HAL_MCI_STATE_ENABLE,
    HAL_MCI_STATE_INIT_GPM_OFFSET,
    HAL_MCI_STATE_NEXT_GPM_OFFSET,
    HAL_MCI_STATE_LAST_GPM_OFFSET,
    HAL_MCI_STATE_BT,
    HAL_MCI_STATE_SET_BT_SLEEP,
    HAL_MCI_STATE_SET_BT_AWAKE,
    HAL_MCI_STATE_SET_BT_CAL_START,
    HAL_MCI_STATE_SET_BT_CAL,
    HAL_MCI_STATE_LAST_SCHD_MSG_OFFSET,
    HAL_MCI_STATE_REMOTE_SLEEP,
    HAL_MCI_STATE_CONT_RSSI_POWER,
    HAL_MCI_STATE_CONT_PRIORITY,
    HAL_MCI_STATE_CONT_TXRX,
    HAL_MCI_STATE_RESET_REQ_WAKE,
    HAL_MCI_STATE_SEND_WLAN_COEX_VERSION,
    HAL_MCI_STATE_SET_BT_COEX_VERSION,
    HAL_MCI_STATE_SEND_WLAN_CHANNELS,
    HAL_MCI_STATE_SEND_VERSION_QUERY,
    HAL_MCI_STATE_SEND_STATUS_QUERY,
    HAL_MCI_STATE_NEED_FLUSH_BT_INFO,
    HAL_MCI_STATE_SET_CONCUR_TX_PRI,
    HAL_MCI_STATE_RECOVER_RX,
    HAL_MCI_STATE_NEED_FTP_STOMP,
    HAL_MCI_STATE_NEED_TUNING,
    HAL_MCI_STATE_SHARED_CHAIN_CONCUR_TX,
    HAL_MCI_STATE_DEBUG,
    HAL_MCI_STATE_MAX
} HAL_MCI_STATE_TYPE;

#define HAL_MCI_STATE_DEBUG_REQ_BT_DEBUG    1

#define HAL_MCI_BT_MCI_FLAGS_UPDATE_CORR          0x00000002
#define HAL_MCI_BT_MCI_FLAGS_UPDATE_HDR           0x00000004
#define HAL_MCI_BT_MCI_FLAGS_UPDATE_PLD           0x00000008
#define HAL_MCI_BT_MCI_FLAGS_LNA_CTRL             0x00000010
#define HAL_MCI_BT_MCI_FLAGS_DEBUG                0x00000020
#define HAL_MCI_BT_MCI_FLAGS_SCHED_MSG            0x00000040
#define HAL_MCI_BT_MCI_FLAGS_CONT_MSG             0x00000080
#define HAL_MCI_BT_MCI_FLAGS_COEX_GPM             0x00000100
#define HAL_MCI_BT_MCI_FLAGS_CPU_INT_MSG          0x00000200
#define HAL_MCI_BT_MCI_FLAGS_MCI_MODE             0x00000400
#define HAL_MCI_BT_MCI_FLAGS_EGRET_MODE           0x00000800
#define HAL_MCI_BT_MCI_FLAGS_JUPITER_MODE         0x00001000
#define HAL_MCI_BT_MCI_FLAGS_OTHER                0x00010000

#define HAL_MCI_DEFAULT_BT_MCI_FLAGS        0x00011dde
/*
    HAL_MCI_BT_MCI_FLAGS_UPDATE_CORR  = 1
    HAL_MCI_BT_MCI_FLAGS_UPDATE_HDR   = 1
    HAL_MCI_BT_MCI_FLAGS_UPDATE_PLD   = 1
    HAL_MCI_BT_MCI_FLAGS_LNA_CTRL     = 1
    HAL_MCI_BT_MCI_FLAGS_DEBUG        = 0
    HAL_MCI_BT_MCI_FLAGS_SCHED_MSG    = 1
    HAL_MCI_BT_MCI_FLAGS_CONT_MSG     = 1
    HAL_MCI_BT_MCI_FLAGS_COEX_GPM     = 1
    HAL_MCI_BT_MCI_FLAGS_CPU_INT_MSG  = 0
    HAL_MCI_BT_MCI_FLAGS_MCI_MODE     = 1
    HAL_MCI_BT_MCI_FLAGS_EGRET_MODE   = 1
    HAL_MCI_BT_MCI_FLAGS_JUPITER_MODE = 1
    HAL_MCI_BT_MCI_FLAGS_OTHER        = 1
*/

#define HAL_MCI_TOGGLE_BT_MCI_FLAGS \
    (   HAL_MCI_BT_MCI_FLAGS_UPDATE_CORR    |   \
        HAL_MCI_BT_MCI_FLAGS_UPDATE_HDR     |   \
        HAL_MCI_BT_MCI_FLAGS_UPDATE_PLD     |   \
        HAL_MCI_BT_MCI_FLAGS_MCI_MODE   )

#define HAL_MCI_2G_FLAGS_CLEAR_MASK         0x00000000
#define HAL_MCI_2G_FLAGS_SET_MASK           HAL_MCI_TOGGLE_BT_MCI_FLAGS
#define HAL_MCI_2G_FLAGS                    HAL_MCI_DEFAULT_BT_MCI_FLAGS

#define HAL_MCI_5G_FLAGS_CLEAR_MASK         HAL_MCI_TOGGLE_BT_MCI_FLAGS
#define HAL_MCI_5G_FLAGS_SET_MASK           0x00000000
#define HAL_MCI_5G_FLAGS                    (HAL_MCI_DEFAULT_BT_MCI_FLAGS & \
                                            ~HAL_MCI_TOGGLE_BT_MCI_FLAGS)
    
#define HAL_MCI_GPM_NOMORE  0
#define HAL_MCI_GPM_MORE    1
#define HAL_MCI_GPM_INVALID 0xffffffff    

#define ATH_AIC_MAX_BT_CHANNEL          79

/*
 * Default value for Jupiter   is 0x00002201
 * Default value for Aphrodite is 0x00002282
 */
#define ATH_MCI_CONFIG_CONCUR_TX            0x00000003
#define ATH_MCI_CONFIG_MCI_OBS_MCI          0x00000004
#define ATH_MCI_CONFIG_MCI_OBS_TXRX         0x00000008
#define ATH_MCI_CONFIG_MCI_OBS_BT           0x00000010
#define ATH_MCI_CONFIG_DISABLE_MCI_CAL      0x00000020
#define ATH_MCI_CONFIG_DISABLE_OSLA         0x00000040
#define ATH_MCI_CONFIG_DISABLE_FTP_STOMP    0x00000080
#define ATH_MCI_CONFIG_AGGR_THRESH          0x00000700
#define ATH_MCI_CONFIG_AGGR_THRESH_S        8
#define ATH_MCI_CONFIG_DISABLE_AGGR_THRESH  0x00000800
#define ATH_MCI_CONFIG_CLK_DIV              0x00003000
#define ATH_MCI_CONFIG_CLK_DIV_S            12
#define ATH_MCI_CONFIG_DISABLE_TUNING       0x00004000
#define ATH_MCI_CONFIG_MCI_WEIGHT_DBG       0x40000000
#define ATH_MCI_CONFIG_DISABLE_MCI          0x80000000

#define ATH_MCI_CONFIG_MCI_OBS_MASK     ( ATH_MCI_CONFIG_MCI_OBS_MCI | \
                                          ATH_MCI_CONFIG_MCI_OBS_TXRX | \
                                          ATH_MCI_CONFIG_MCI_OBS_BT )
#define ATH_MCI_CONFIG_MCI_OBS_GPIO     0x0000002F

#define ATH_MCI_CONCUR_TX_SHARED_CHN    0x01
#define ATH_MCI_CONCUR_TX_UNSHARED_CHN  0x02
#define ATH_MCI_CONCUR_TX_DEBUG         0x03

/* 
 * The values below come from the system team test result.
 * For Jupiter, BT tx power level is from 0(-20dBm) to 6(4dBm).
 * Lowest WLAN tx power would be in bit[23:16] of dword 1.
 */
static const u_int32_t mci_concur_tx_max_pwr[4][8] =
    { /* No limit */
      {0x7f7f7f7f, 0x7f7f7f7f, 0x7f7f7f7f, 0x7f7f7f7f,
       0x7f7f7f7f, 0x7f7f7f7f, 0x7f7f7f7f, 0x7f7f7f7f},
      /* 11G */
      {0x16161616, 0x12121516, 0x12121212, 0x12121212,
       0x12121212, 0x12121212, 0x12121212, 0x7f121212},
      /* HT20 */
      {0x15151515, 0x14141515, 0x14141414, 0x14141414,
       0x14141414, 0x14141414, 0x14141414, 0x7f141414},
      /* HT40 */
      {0x10101010, 0x10101010, 0x10101010, 0x10101010,
       0x10101010, 0x10101010, 0x10101010, 0x7f101010}};
#define ATH_MCI_CONCUR_TX_LOWEST_PWR_MASK     0x00ff0000
#define ATH_MCI_CONCUR_TX_LOWEST_PWR_MASK_S   16

#endif	/* __AR9300_FREEBSD_INC_H__ */
