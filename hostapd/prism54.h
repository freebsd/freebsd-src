#ifndef PRISM54_H
#define PRISM54_H

struct ieee802_3_hdr_s {
	unsigned char da[6];
	unsigned char sa[6];
	unsigned short type;
} __attribute__ ((packed));

typedef struct ieee802_3_hdr_s ieee802_3_hdr;

#define PIMOP_GET	0
#define PIMOP_SET	1
#define PIMOP_RESPONSE	2
#define PIMOP_ERROR	3
#define PIMOP_TRAP	4

struct pimdev_hdr_s {
	int op;
	unsigned long oid;
} __attribute__ ((packed));

typedef struct pimdev_hdr_s pimdev_hdr;

#define DOT11_OID_ATTACHMENT	0x19000003

/* really need to check */
#define DOT11_PKT_BEACON	0x80
#define DOT11_PKT_ASSOC_RESP	0x10
#define DOT11_PKT_REASSOC_RESP	0x30
#define DOT11_PKT_PROBE_RESP	0x50

struct obj_attachment_hdr {
	char type;
	char reserved;
	short id;
	short size;
} __attribute__ ((packed));

struct obj_attachment {
	char type;
	char reserved;
	short id;
	short size;
	char data[1];
} __attribute__ ((packed));

#define DOT11_OID_MLMEAUTOLEVEL		0x19000001
#define DOT11_MLME_AUTO			0
#define DOT11_MLME_INTERMEDIATE		0x01000000
#define DOT11_MLME_EXTENDED		0x02000000

#define DOT11_OID_DEAUTHENTICATE	0x18000000
#define DOT11_OID_AUTHENTICATE		0x18000001
#define DOT11_OID_DISASSOCIATE		0x18000002
#define DOT11_OID_ASSOCIATE		0x18000003
#define DOT11_OID_BEACON		0x18000005
#define DOT11_OID_PROBE			0x18000006
#define DOT11_OID_REASSOCIATE		0x1800000b

struct obj_mlme {
	char address[6];
	short id;
	short state;
	short code;
} __attribute__ ((packed));

#define DOT11_OID_DEAUTHENTICATEEX	0x18000007
#define DOT11_OID_AUTHENTICATEEX	0x18000008
#define DOT11_OID_DISASSOCIATEEX	0x18000009
#define DOT11_OID_ASSOCIATEEX		0x1800000a
#define DOT11_OID_REASSOCIATEEX		0x1800000c

struct obj_mlmeex {
	char address[6];
	short id;
	short state;
	short code;
	short size;
	char data[1];
} __attribute__ ((packed));

#define DOT11_OID_STAKEY        0x12000008

#define DOT11_PRIV_WEP  0
#define DOT11_PRIV_TKIP 1

/* endian reversed to bigger endian */
#define DOT11_STAKEY_OPTION_DEFAULTKEY	0x100

struct obj_stakey {
	char address[6];
	char keyid;
	char reserved;
	short options;
	char type;
	char length;
	char key[32];
} __attribute__ ((packed));

#define DOT11_OID_DEFKEYID	0x12000003
#define DOT11_OID_DEFKEY1	0x12000004
#define DOT11_OID_DEFKEY2	0x12000005
#define DOT11_OID_DEFKEY3       0x12000006
#define DOT11_OID_DEFKEY4       0x12000007

struct obj_key {
	char type;
	char length;
	char key[32];
} __attribute__ ((packed));

#define DOT11_OID_STASC		0x1200000a

struct obj_stasc {
	char address[6];
	char keyid;
	char tx_sc;
	unsigned long sc_high;
	unsigned short sc_low;
} __attribute__ ((packed));

#define DOT11_OID_CLIENTS	0x15000001
#define DOT11_OID_CLIENTSASSOCIATED	0x15000002
#define DOT11_OID_CLIENTST	0x15000003
#define DOT11_OID_CLIENTEND	0x150007d9
#define DOT11_OID_CLIENTFIND	0x150007db

#define DOT11_NODE_UNKNOWN
#define DOT11_NODE_CLIENT
#define DOT11_NODE_AP

/* endian reversed to bigger endian */
#define DOT11_STATE_NONE	0
#define DOT11_STATE_AUTHING	0x100
#define DOT11_STATE_AUTH	0x200
#define DOT11_STATE_ASSOCING	0x300
#define DOT11_STATE_REASSOCING	0x400
#define DOT11_STATE_ASSOC	0x500
#define DOT11_STATE_WDS		0x600

struct obj_sta {
	char address[6];
	char pad[2];
	char state;
	char node;
	short age;
	char reserved1;
	char rssi;
	char rate;
	char reserved2;
} __attribute__ ((packed));

#define DOT11_OID_SSID		0x10000002
#define DOT11_OID_SSIDOVERRIDE	0x10000006

struct obj_ssid {
	char length;
	char octets[33];
} __attribute__ ((packed));

#define DOT11_OID_EAPAUTHSTA		0x150007de
#define DOT11_OID_EAPUNAUTHSTA		0x150007df
/* not in 38801 datasheet??? */
#define DOT11_OID_DOT1XENABLE		0x150007e0
#define DOT11_OID_MICFAILURE		0x150007e1
#define DOT11_OID_AUTHENABLE		0x12000000
#define DOT11_OID_PRIVACYINVOKED	0x12000001
#define DOT11_OID_EXUNENCRYPTED		0x12000002

#define DOT11_AUTH_OS			0x01000000
#define DOT11_AUTH_SK			0x02000000
#define DOT11_AUTH_BOTH			0x03000000

#define DOT11_BOOL_TRUE			0x01000000

#endif /* PRISM54_H */
