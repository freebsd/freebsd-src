/*
 * Copyright (C) Eicon Technology Corporation, 2000.
 *
 * Eicon File Revision :    1.0  
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

/*------------------------------------------------------------------*/
/* Q.931 information elements maximum length                        */
/* excluding the identifier, including the length field             */
/*------------------------------------------------------------------*/

#define MAX_LEN_BC      13
#define MAX_LEN_LLC     19 /* ctr3 */
#define MAX_LEN_HLC     6  /* ctr3 */
#define MAX_LEN_UUI     200 /* Hicom USBS req */
#define MAX_LEN_NUM     24
#define MAX_LEN_DSP     83 /* ctr3 */
#define MAX_LEN_NI      4
#define MAX_LEN_PI      5
#define MAX_LEN_SIN     3
#define MAX_LEN_CST     4
#define MAX_LEN_SIG     2
#define MAX_LEN_SPID    32
#define MAX_LEN_EID     3
#define MAX_LEN_CHI     35  /* ctr3 */
#define MAX_LEN_CAU     33
#define MAX_LEN_FTY     130
#define MAX_LEN_KEY     83  /* ctr3 */
#define MAX_LEN_RSI     4
#define MAX_LEN_CAI     11
#define MAX_NUM_SPID    4
#define MAX_LEN_USERID  9
#define MAX_LEN_APPLID  5
#define MAX_LEN_NTTCIF  15

/*------------------------------------------------------------------*/
/* decision return values                                           */
/*------------------------------------------------------------------*/

#define YES             1
#define NO              0


/*-------------------------------------------------------------------*/
/* w element coding                                                  */
/*-------------------------------------------------------------------*/

#define NTTCIF          0x01
#define BC              0x04
#define CAU             0x08
#define CAD             0x0c
#define CAI             0x10
#define CST             0x14
#define CHI             0x18
#define LLI             0x19
#define CHA             0x1a
#define FTY             0x1c
#define PI              0x1e
#define NFAC            0x20
#define TC              0x24
#define ATT_EID         0x26
#define NI              0x27
#define DSP             0x28
#define DT              0x29
#define KEY             0x2c
#define KP              0x2c
#define UID             0x2d
#define SIG             0x34
#define FI              0x39
#define SPID            0x3a
#define EID             0x3b
#define DSPF            0x3c
#define ECAD            0x4c
#define OAD             0x6c
#define OSA             0x6d
#define DAD             0x70
#define CPN             0x70
#define DSA             0x71
#define RDX             0x73
#define RAD             0x74
#define RDN             0x74
#define RSI             0x79
#define SCR             0x7A   /* internal unscreened CPN          */
#define MIE             0x7a   /* internal management info element */
#define LLC             0x7c
#define HLC             0x7d
#define UUI             0x7e
#define ESC             0x7f

#define SHIFT           0x90
#define MORE            0xa0
#define CL              0xb0

/* information elements used on the spid interface */
#define SPID_CMD        0xc0
#define SPID_LINK       0x10
#define SPID_DN         0x70
#define SPID_BC         0x04
#define SPID_SWITCH     0x11

/*------------------------------------------------------------------*/
/* global configuration parameters, defined in exec.c               */
/* these parameters are configured with program loading             */
/*------------------------------------------------------------------*/

#define PROT_1TR6       0
#define PROT_ETSI       1
#define PROT_FRANC      2
#define PROT_BELG       3
#define PROT_SWED       4
#define PROT_NI         5
#define PROT_5ESS       6
#define PROT_JAPAN      7
#define PROT_ATEL       8
#define PROT_US         9
#define PROT_ITALY      10
#define PROT_TWAN       11
#define PROT_AUSTRAL    12

#define INIT_PROT_1TR6    0x80|PROT_1TR6
#define INIT_PROT_ETSI    0x80|PROT_ETSI
#define INIT_PROT_FRANC   0x80|PROT_FRANC
#define INIT_PROT_BELG    0x80|PROT_BELG
#define INIT_PROT_SWED    0x80|PROT_SWED
#define INIT_PROT_NI      0x80|PROT_NI
#define INIT_PROT_5ESS    0x80|PROT_5ESS
#define INIT_PROT_JAPAN   0x80|PROT_JAPAN
#define INIT_PROT_ATEL    0x80|PROT_ATEL
#define INIT_PROT_ITALY   0x80|PROT_ITALY
#define INIT_PROT_TWAN    0x80|PROT_TWAN
#define INIT_PROT_AUSTRAL 0x80|PROT_AUSTRAL


/* -----------------------------------------------------------**
** The PROTOCOL_FEATURE_STRING in feature.h (included         **
** in prstart.sx and astart.sx) defines capabilities and      **
** features of the actual protocol code. It's used as a bit   **
** mask.                                                      **
** The following Bits are defined:                            **
** -----------------------------------------------------------*/
                                           
#define PROTCAP_TELINDUS  0x0001  /* Telindus Variant of protocol code   */
#define PROTCAP_MANIF     0x0002  /* Management interface implemented    */
#define PROTCAP_V_42      0x0004  /* V42 implemented                     */
#define PROTCAP_V90D      0x0008  /* V.90D (implies up to 384k DSP code) */
#define PROTCAP_EXTD_FAX  0x0010  /* Extended FAX (ECM, 2D, T6, Polling) */
#define PROTCAP_FREE4     0x0020  /* not used                            */
#define PROTCAP_FREE5     0x0040  /* not used                            */
#define PROTCAP_FREE6     0x0080  /* not used                            */
#define PROTCAP_FREE7     0x0100  /* not used                            */
#define PROTCAP_FREE8     0x0200  /* not used                            */
#define PROTCAP_FREE9     0x0400  /* not used                            */
#define PROTCAP_FREE10    0x0800  /* not used                            */
#define PROTCAP_FREE11    0x1000  /* not used                            */
#define PROTCAP_FREE12    0x2000  /* not used                            */
#define PROTCAP_FREE13    0x4000  /* not used                            */
#define PROTCAP_EXTENSION 0x8000  /* used for future extentions          */
