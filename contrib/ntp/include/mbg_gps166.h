/*
 * /src/NTP/ntp-4/include/mbg_gps166.h,v 4.1 1998/06/12 15:07:30 kardel RELEASE_19990228_A
 *
 * $Created: Sun Jul 20 09:20:50 1997 $
 *
 * Copyright (C) 1997, 1998 by Frank Kardel
 */
#ifndef MBG_GPS166_H
#define MBG_GPS166_H


/***************************************************************************/
/*                                                                         */
/*   File:         GPSSERIO.H                    4.1          */
/*                                                                         */
/*   Project:      Common C Library                                        */
/*                                                                         */
/*   Compiler:     Borland C++                                             */
/*                                                                         */
/*   Author:       M. Burnicki,  Meinberg Funkuhren                        */
/*                                                                         */
/*                                                                         */
/*   Description:                                                          */
/*     This file defines structures and codes to be used to access GPS166  */
/*     via its serial interface COM0. COM0 should be set to a high baud    */
/*     rate, default is 19200.                                             */
/*                                                                         */
/*     Standard GPS166 serial operation is to send a time string that is   */
/*     compatible with Meinberg UA31 or PZF535 DCF77 radio remote clocks.  */
/*     That string can be transmitted automatically once per second, once  */
/*     per minute or on request per ASCII '?'.                             */
/*                                                                         */
/*     Parameter setup or parameter readout works using blocks of binary   */
/*     data which have to be isolated from the standard string. A block of */
/*     data starts with a SOH code (ASCII Start Of Header, 0x01) followed  */
/*     by a message header with constant length and a data portion with    */
/*     variable length. The first field (cmd) of the message header holds  */
/*     the command code rsp. the type of data to be transmitted. The next  */
/*     field (len) gives the number of data bytes that are transmitted     */
/*     after the header. This number ranges from 0 to sizeof( MSG_DATA ).  */
/*     The third field (data_csum) holds a checksum of all data bytes and  */
/*     the last field of the header finally holds the checksum of the.     */
/*     header.                                                             */
/*                                                                         */
/***************************************************************************/

/* the control codes defined below are to be or'ed with a command/type code */

#define GPS_REQACK    0x8000   /* to GPS166: request acknowledge */
#define GPS_ACK       0x4000   /* from GPS166: acknowledge a command */
#define GPS_NACK      0x2000   /* from GPS166: error receiving command */

#define GPS_CTRL_MSK  0xF000   /* masks control code from command */


/* The codes below specify commands/types of data to be supplied to GPS166: */

/*                            GPS166 auto-message to host            */
/*                            þ   host request, GPS166 response      */
/*                            þ   þ   host download to GPS166        */
/*                            þ   þ   þ                              */
enum {  /*                    þ   þ   þ                              */
  /* system data */
  GPS_AUTO_ON = 0x000,   /* þ   þ   þ X þ enable auto-messages from GPS166 */
  GPS_AUTO_OFF,          /* þ   þ   þ X þ disable auto-messages from GPS166 */
  GPS_SW_REV,            /* þ   þ X þ   þ request software revision */
  GPS_STAT,              /* þ   þ X þ   þ request status of buffered variables */
  GPS_TIME,              /* þ X þ   þ X þ current time or capture or init board time */
  GPS_POS_XYZ,           /* þ   þ X þ X þ current position in ECEF coords */
  GPS_POS_LLA,           /* þ   þ X þ X þ current position in geographic coords */
  GPS_TZDL,              /* þ   þ X þ X þ time zone / daylight saving */
  GPS_PORT_PARM,         /* þ   þ X þ X þ parameters of the serial ports */
  GPS_SYNTH,             /* þ   þ X þ X þ synthesizer's frequency and phase */
  GPS_ANT_INFO,          /* þ X þ X þ   þ time diff after antenna disconnect */
  GPS_UCAP,              /* þ X þ X þ   þ user capture */

  /* GPS data */
  GPS_CFGH = 0x100,      /* þ   þ X þ X þ SVs' configuration and health codes */
  GPS_ALM,               /* þ   þ X þ X þ one SV's almanac */
  GPS_EPH,               /* þ   þ X þ X þ one SV's ephemeris */
  GPS_UTC,               /* þ   þ X þ X þ UTC correction parameters */
  GPS_IONO,              /* þ   þ X þ X þ ionospheric correction parameters */
  GPS_ASCII_MSG          /* þ   þ X þ   þ the GPS ASCII message */
};

/*
 * modelled after GPSDEFS.H Revision 1.5
 */
/***************************************************************************/
/*                                                                         */
/*   File:         GPSDEFS.H                     4.1          */
/*                                                                         */
/*   Project:      Common C Library                                        */
/*                                                                         */
/*   Compiler:     Borland C++                                             */
/*                                                                         */
/*   Author:       M. Burnicki,  Meinberg Funkuhren                        */
/*                                                                         */
/*                                                                         */
/*   Description:                                                          */
/*     General definitions to be used with GPS166                          */
/*     GPS166 Rev. 1.23 or above                                           */
/*                                                                         */
/*   Modifications: see file GPSLIB.TXT                                    */
/*                                                                         */
/***************************************************************************/
#define _GPSDEFS_H
/* the type of various checksums */

#ifndef _CSUM_DEFINED
  typedef unsigned short CSUM;
#  define _CSUM_DEFINED
#endif

/* the message header */

typedef struct {
  unsigned short gps_cmd;
  unsigned short gps_len;
  unsigned short gps_data_csum;
  unsigned short gps_hdr_csum;
} GPS_MSG_HDR;

/* a struct used to hold the software revision information */

typedef struct {
  unsigned short code;       /* e.g. 0x0120 means rev. 1.20 */
  unsigned char name[17];     /* used to identify customized versions */
} SW_REV;

/* GPS ASCII message */

typedef struct {
  CSUM csum;       /* checksum of the remaining bytes */
  short valid;     /* flag data are valid */
  char s[23];      /* 22 chars GPS ASCII message plus trailing zero */
} ASCII_MSG;

#define MIN_SVNO         1                  /* min. SV number */
#define MAX_SVNO        32                  /* max. SV number */
#define N_SVNO ( MAX_SVNO - MIN_SVNO + 1)   /* number of possibly active SVs */


typedef short          SVNO;     /* the number of a SV */
typedef unsigned short HEALTH;  /* a SV's health code */
typedef unsigned short CFG;     /* a SV's configuration code */
typedef unsigned short IOD;     /* Issue-Of-Data code */

/* Date and time referred to the linear time scale defined by GPS. */
/* GPS time is defined by the number of weeks since midnight from */
/* January 5, 1980 to January 6, 1980 plus the number of seconds of */
/* the current week plus fractions of a second. GPS time differs from */
/* UTC because UTC is corrected with leap seconds while GPS time scale */
/* is continuous. */

typedef struct {
  unsigned short wn;     /* the week number since GPS has been installed */
  unsigned long sec;     /* the second of that week */
  unsigned long tick;    /* fractions of a second; scale: 1E-7 */
} T_GPS;


/* Local date and time computed from GPS time. The current number */
/* of leap seconds have to be added to get UTC from GPS time. */
/* Additional corrections could have been made according to the */
/* time zone/daylight saving parameters (TZDL, see below) defined */
/* by the user. The status field can be checked to see which corrections */
/* have been applied. */

#ifndef _TM_DEFINED
  typedef struct {
    short year;          /* 0..9999 */
    char month;          /* 1..12 */
    char mday;           /* 1..31 */
    short yday;          /* 1..366 */
    char wday;           /* 0..6 == Sun..Sat */
    char hour;           /* 0..23 */
    char minute;         /* 0..59 */
    char second;         /* 0..59 */
    long frac;           /* fractions of a second, scale 1E-7 */
    long offs_from_utc;  /* local time's offset from UTC */
    unsigned short status;       /* flags */
  } TM;

  /* status flags used with conversion from GPS time to local time */

#  define TM_UTC        0x01   /* UTC correction has been made */
#  define TM_LOCAL      0x02   /* UTC has been converted to local time */
#  define TM_DL_ANN     0x04   /* state of daylight saving is going to change */
#  define TM_DL_ENB     0x08   /* daylight saving is enabled */
#  define TM_LS_ANN     0x10   /* leap second will be inserted */
#  define TM_LS_ENB     0x20   /* current second is leap second */

#  define _TM_DEFINED
#endif


/* the status flags below are defined starting with rev. 1.32 */

#define TM_ANT_DISCONN  0x1000  /* antenna currently disconnected */
#define TM_SYN_FLAG     0x2000  /* TIME_SYN output is low */
#define TM_NO_SYNC      0x4000  /* not sync'ed after reset */
#define TM_NO_POS       0x8000  /* position not computed after reset, */
                                /*   LOCK LED off */

/* a struct used to transmit information on date and time */

typedef struct {
  short channel;        /* -1: the current time; 0, 1: capture 0, 1 */
  T_GPS t;              /* time in GPS format */
  TM tm;                /* that time converted to local time */
} TTM;



/* Two types of variables used to store a position. Type XYZ is */
/* used with a position in earth centered, earth fixed (ECEF) */
/* coordinates whereas type LLA holds such a position converted */
/* to geographic coordinates as defined by WGS84 (World Geodetic */
/* System from 1984). */

#ifndef _XYZ_DEFINED
  /* sequence and number of components of a cartesian position */
  enum { XP, YP, ZP, N_XYZ };

  /* a type of array holding a cartesian position */
  typedef l_fp XYZ[N_XYZ];      /* values are in [m] */

#  define _XYZ_DEFINED
#endif


#ifndef _LLA_DEFINED
  /* sequence and number of components of a geographic position */
  enum { LAT, LON, ALT, N_LLA };  /* latitude, longitude, altitude */

  /* a type of array holding a geographic position */
  typedef l_fp LLA[N_LLA];      /* lon, lat in [rad], alt in [m] */

#  define _LLA_DEFINED
#endif

/* Synthesizer parameters. Synthesizer frequency is expressed as a */
/* four digit decimal number (freq) to be multiplied by 0.1 Hz and an */
/* base 10 exponent (range). If the effective frequency is less than */
/* 10 kHz its phase is synchronized corresponding to the variable phase. */
/* Phase may be in a range from -360° to +360° with a resolution of 0.1°, */
/* so the resulting numbers to be stored are in a range of -3600 to +3600. */

/* Example: */
/* Assume the value of freq is 2345 (decimal) and the value of phase is 900. */
/* If range == 0 the effective frequency is 234.5 Hz with a phase of +90°. */
/* If range == 1 the synthesizer will generate a 2345 Hz output frequency */
/* and so on. */

/* Limitations: */
/* If freq == 0 the synthesizer is disabled. If range == 0 the least */
/* significant digit of freq is limited to 0, 3, 5 or 6. The resulting */
/* frequency is shown in the examples below: */
/*     freq == 1230  -->  123.0 Hz */
/*     freq == 1233  -->  123 1/3 Hz (real 1/3 Hz, NOT 123.3 Hz) */
/*     freq == 1235  -->  123.5 Hz */
/*     freq == 1236  -->  123 2/3 Hz (real 2/3 Hz, NOT 123.6 Hz) */

/* If range == MAX_RANGE the value of freq must not exceed 1200, so the */
/* output frequency is limited to 12 MHz. */

/* Phase will be ignored if the resulting frequency is greater or equal */
/* to 10 kHz. */

#define MAX_SYNTH_FREQ   1200    /* if range == MAX_SYNTH_RANGE */
#define MIN_SYNTH_RANGE     0
#define MAX_SYNTH_RANGE     5
#define MAX_SYNTH_PHASE  3600

typedef struct {
  short freq;      /* four digits used; scale: 0.1; e.g. 1234 -> 123.4 Hz */
  short range;     /* scale factor for freq; 0..MAX_SYNTH_RANGE */
  short phase;     /* -MAX_SYNTH_PHASE..+MAX_SYNTH_PHASE; >0 -> pulses later */
} SYNTH;



/* Time zone/daylight saving parameters. */

/* the name of a time zone, 5 characters plus trailing zero */
typedef char TZ_NAME[6];

typedef struct {
  long offs;         /* offset from UTC to local time [sec] */
  long offs_dl;      /* additional offset if daylight saving enabled [sec] */
  TM tm_on;          /* date/time when daylight saving starts */
  TM tm_off;         /* date/time when daylight saving ends */
  TZ_NAME name[2];   /* names without and with daylight saving enabled */
} TZDL;

/* The constant below is defined beginning with software rev. 1.29. */
/* If the year in tzdl.tmon and tzdl.tm_off is or'ed with that constant, */
/* the receiver automatically generates daylight saving year by year. */
/* See GPSLIB.TXT for more information. */

#define DL_AUTO_FLAG  0x8000

/* Example: */
/* for automatic daylight saving enable/disable in Central Europe, */
/* the variables are to be set as shown below: */
/*   offs = 3600L           one hour from UTC */
/*   offs_dl = 3600L        one additional hour if daylight saving enabled */
/*   tm_on = first Sunday from March 25, 02:00:00h ( year |= DL_AUTO_FLAG ) */
/*   tm_off = first Sunday from Sept 24, 03:00:00h ( year |= DL_AUTO_FLAG ) */
/*   name[0] == "MEZ  "     name if daylight saving not enabled */
/*   name[1] == "MESZ "     name if daylight saving is enabled */




/* the structure below was defined in rev. 1.31. It reflects the status */
/* of the antenna, the times of last disconnect/reconnect and the boards */
/* clock offset after the phase of disconnection. */

typedef struct {
  short status;    /* current status of antenna */
  TM tm_disconn;   /* time of antenna disconnect */
  TM tm_reconn;    /* time of antenna reconnect */
  long delta_t;    /* clock offset at reconnect time, units: TICKS_PER_SEC */
} ANT_INFO;


/* the status field may be set to one of the values below: */

enum {
  ANT_INVALID,   /* struct not set yet because ant. has not been disconn. */
  ANT_DISCONN,   /* ant. now disconn., tm_reconn and delta_t not set */
  ANT_RECONN     /* ant. has been disconn. and reconn., all fields valid */
};


/* Summary of configuration and health data of all SVs. */

typedef struct {
  CSUM csum;               /* checksum of the remaining bytes */
  short valid;             /* flag data are valid */

  T_GPS tot_51;            /* time of transmission, page 51 */
  T_GPS tot_63;            /* time of transmission, page 63 */
  T_GPS t0a;               /* complete reference time almanac */

  CFG cfg[N_SVNO];         /* SV configuration from page 63 */
  HEALTH health[N_SVNO];   /* SV health from pages 51, 63 */
} CFGH;



/* UTC correction parameters */

typedef struct {
  CSUM csum;       /*    checksum of the remaining bytes                  */
  short valid;     /*    flag data are valid                              */

  T_GPS t0t;       /*    Reference Time UTC Parameters              [sec] */
  l_fp A0;         /*  ± Clock Correction Coefficient 0             [sec] */
  l_fp A1;         /*  ± Clock Correction Coefficient 1         [sec/sec] */

  ushort WNlsf;    /*  week number of nearest leap second                 */
  short DNt;       /*  the day number at the end of which LS is inserted  */
  char delta_tls;  /*                                                     */
  char delta_tlsf; /*                                                     */

} UTC;

/* a struct used to hold the settings of a serial port */

#ifndef _COM_PARM_DEFINED
  typedef long BAUD_RATE;

  /* indices used to identify a parameter in the framing string */
  enum { F_DBITS, F_PRTY, F_STBITS };

  /* types of handshake */
  enum { HS_NONE, HS_XONXOFF, HS_RTSCTS };

  typedef struct {
    BAUD_RATE baud_rate;    /* e.g. 19200L */
    char framing[4];        /* e.g. "8N1" */
    short handshake;        /* a numeric value, only HS_NONE supported yet */
  } COM_PARM;

#define _COM_PARM_DEFINED
#endif



/* the codes below define what has to comes out of the serial ports */

enum { STR_ON_REQ, STR_PER_SEC,
       STR_PER_MIN, N_STR_MODE_0,      /* COM0 and COM1 */
       STR_UCAP = N_STR_MODE_0,
       STR_UCAP_REQ, N_STR_MODE_1      /* COM1 only */
     };


#define N_COM   2  /* the number of serial ports */

/* the structure used to store the modes of both serial ports */

typedef struct {
  COM_PARM com[N_COM];    /* COM0 and COM1 settings */
  u_char mode[N_COM];      /* COM0 and COM1 output mode */
} PORT_PARM;

/* Ephemeris parameters of one specific SV. Needed to compute the position */
/* of a satellite at a given time with high precision. Valid for an */
/* interval of 4 to 6 hours from start of transmission. */

typedef struct {
  CSUM csum;       /*    checksum of the remaining bytes                  */
  short valid;     /*    flag data are valid                              */

  HEALTH health;   /*    health indication of transmitting SV      [---]  */
  IOD IODC;        /*    Issue Of Data, Clock                             */
  IOD IODE2;       /*    Issue of Data, Ephemeris (Subframe 2)            */
  IOD IODE3;       /*    Issue of Data, Ephemeris (Subframe 3)            */
  T_GPS tt;        /*    time of transmission                             */
  T_GPS t0c;       /*    Reference Time Clock                      [---]  */
  T_GPS t0e;       /*    Reference Time Ephemeris                  [---]  */

  l_fp   sqrt_A;   /*    Square Root of semi-major Axis        [sqrt(m)]  */
  l_fp   e;        /*    Eccentricity                              [---]  */
  l_fp   M0;       /*  ± Mean Anomaly at Ref. Time                 [rad]  */
  l_fp   omega;    /*  ± Argument of Perigee                       [rad]  */
  l_fp   OMEGA0;   /*  ± Longit. of Asc. Node of orbit plane       [rad]  */
  l_fp   OMEGADOT; /*  ± Rate of Right Ascension               [rad/sec]  */
  l_fp   deltan;   /*  ± Mean Motion Diff. from computed value [rad/sec]  */
  l_fp   i0;       /*  ± Inclination Angle                         [rad]  */
  l_fp   idot;     /*  ± Rate of Inclination Angle             [rad/sec]  */
  l_fp   crc;      /*  ± Cosine Corr. Term to Orbit Radius           [m]  */
  l_fp   crs;      /*  ± Sine Corr. Term to Orbit Radius             [m]  */
  l_fp   cuc;      /*  ± Cosine Corr. Term to Arg. of Latitude     [rad]  */
  l_fp   cus;      /*  ± Sine Corr. Term to Arg. of Latitude       [rad]  */
  l_fp   cic;      /*  ± Cosine Corr. Term to Inclination Angle    [rad]  */
  l_fp   cis;      /*  ± Sine Corr. Term to Inclination Angle      [rad]  */

  l_fp   af0;      /*  ± Clock Correction Coefficient 0            [sec]  */
  l_fp   af1;      /*  ± Clock Correction Coefficient 1        [sec/sec]  */
  l_fp   af2;      /*  ± Clock Correction Coefficient 2       [sec/sec²]  */
  l_fp   tgd;      /*  ± estimated group delay differential        [sec]  */

  u_short URA;      /*    predicted User Range Accuracy                    */

  u_char L2code;    /*    code on L2 channel                         [---] */
  u_char L2flag;    /*    L2 P data flag                             [---] */

} EPH;

/* Almanac parameters of one specific SV. A reduced precision set of */
/* parameters used to check if a satellite is in view at a given time. */
/* Valid for an interval of more than 7 days from start of transmission. */

typedef struct {
  CSUM csum;       /*    checksum of the remaining bytes                  */
  short valid;     /*    flag data are valid                              */

  HEALTH health;   /*                                               [---] */
  T_GPS t0a;       /*    Reference Time Almanac                     [sec] */

  l_fp   sqrt_A;   /*    Square Root of semi-major Axis         [sqrt(m)] */
  l_fp   e;        /*    Eccentricity                               [---] */

  l_fp   M0;       /*  ± Mean Anomaly at Ref. Time                  [rad] */
  l_fp   omega;    /*  ± Argument of Perigee                        [rad] */
  l_fp   OMEGA0;   /*  ± Longit. of Asc. Node of orbit plane        [rad] */
  l_fp   OMEGADOT; /*  ± Rate of Right Ascension                [rad/sec] */
  l_fp   deltai;   /*  ±                                            [rad] */
  l_fp   af0;      /*  ± Clock Correction Coefficient 0             [sec] */
  l_fp   af1;      /*  ± Clock Correction Coefficient 1         [sec/sec] */
} ALM;


/* ionospheric correction parameters */

typedef struct {
  CSUM csum;       /*    checksum of the remaining bytes                  */
  short valid;     /*    flag data are valid                              */

  l_fp   alpha_0;  /*    Ionosph. Corr. Coeff. Alpha 0              [sec] */
  l_fp   alpha_1;  /*    Ionosph. Corr. Coeff. Alpha 1          [sec/deg] */
  l_fp   alpha_2;  /*    Ionosph. Corr. Coeff. Alpha 2        [sec/deg^2] */
  l_fp   alpha_3;  /*    Ionosph. Corr. Coeff. Alpha 3        [sec/deg^3] */

  l_fp   beta_0;   /*    Ionosph. Corr. Coeff. Beta 0               [sec] */
  l_fp   beta_1;   /*    Ionosph. Corr. Coeff. Beta 1           [sec/deg] */
  l_fp   beta_2;   /*    Ionosph. Corr. Coeff. Beta 2         [sec/deg^2] */
  l_fp   beta_3;   /*    Ionosph. Corr. Coeff. Beta 3         [sec/deg^3] */

} IONO;

void mbg_tm_str P((unsigned char **, TM *));
void mbg_tgps_str P((unsigned char **, T_GPS *));
void get_mbg_header P((unsigned char **, GPS_MSG_HDR *));
void put_mbg_header P((unsigned char **, GPS_MSG_HDR *));
void get_mbg_sw_rev P((unsigned char **, SW_REV *));
void get_mbg_ascii_msg P((unsigned char **, ASCII_MSG *));
void get_mbg_svno P((unsigned char **, SVNO *));
void get_mbg_health P((unsigned char **, HEALTH *));
void get_mbg_cfg P((unsigned char **, CFG *));
void get_mbg_tgps P((unsigned char **, T_GPS *));
void get_mbg_tm P((unsigned char **, TM *));
void get_mbg_ttm P((unsigned char **, TTM *));
void get_mbg_synth P((unsigned char **, SYNTH *));
void get_mbg_tzdl P((unsigned char **, TZDL *));
void get_mbg_antinfo P((unsigned char **, ANT_INFO *));
void get_mbg_cfgh P((unsigned char **, CFGH *));
void get_mbg_utc P((unsigned char **, UTC *));
void get_mbg_lla P((unsigned char **, LLA));
void get_mbg_xyz P((unsigned char **, XYZ));
void get_mbg_portparam P((unsigned char **, PORT_PARM *));
void get_mbg_eph P((unsigned char **, EPH *));
void get_mbg_alm P((unsigned char **, ALM *));
void get_mbg_iono P((unsigned char **, IONO *));

unsigned long mbg_csum P((unsigned char *, unsigned int));

#endif
/*
 * mbg_gps166.h,v
 * Revision 4.1  1998/06/12 15:07:30  kardel
 * fixed prototyping
 *
 * Revision 4.0  1998/04/10 19:50:42  kardel
 * Start 4.0 release version numbering
 *
 * Revision 1.1  1998/04/10 19:27:34  kardel
 * initial NTP VERSION 4 integration of PARSE with GPS166 binary support
 *
 * Revision 1.1  1997/10/06 20:55:38  kardel
 * new parse structure
 *
 */
