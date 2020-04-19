/*-
 * util.c
 *
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2001 Maksim Yevmenkin <m_evmenkin@yahoo.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: util.c,v 1.2 2003/05/19 17:29:29 max Exp $
 * $FreeBSD$
 */
 
#include <sys/param.h>
#define L2CAP_SOCKET_CHECKED
#include <bluetooth.h>
#include <stdio.h>
#include <string.h>

#define SIZE(x) (sizeof((x))/sizeof((x)[0]))

char const *
hci_link2str(int link_type)
{
	static char const * const	t[] = {
		/* NG_HCI_LINK_SCO */ "SCO",
		/* NG_HCI_LINK_ACL */ "ACL"
	};

	return (link_type >= SIZE(t)? "?" : t[link_type]);
} /* hci_link2str */

char const *
hci_pin2str(int type)
{
	static char const * const	t[] = {
		/* 0x00 */ "Variable PIN",
		/* 0x01 */ "Fixed PIN"
	};

	return (type >= SIZE(t)? "?" : t[type]);
} /* hci_pin2str */

char const *
hci_scan2str(int scan)
{
	static char const * const	t[] = {
		/* 0x00 */ "No Scan enabled",
		/* 0x01 */ "Inquiry Scan enabled. Page Scan disabled",
		/* 0x02 */ "Inquiry Scan disabled. Page Scan enabled",
		/* 0x03 */ "Inquiry Scan enabled. Page Scan enabled"
	};

	return (scan >= SIZE(t)? "?" : t[scan]);
} /* hci_scan2str */

char const *
hci_encrypt2str(int encrypt, int brief)
{
	static char const * const	t[] = {
		/* 0x00 */ "Disabled",
		/* 0x01 */ "Only for point-to-point packets",
		/* 0x02 */ "Both point-to-point and broadcast packets"
	};

	static char const * const	t1[] = {
		/* NG_HCI_ENCRYPTION_MODE_NONE */ "NONE",
		/* NG_HCI_ENCRYPTION_MODE_P2P */  "P2P",
		/* NG_HCI_ENCRYPTION_MODE_ALL */  "ALL",
	};

	if (brief)
		return (encrypt >= SIZE(t1)? "?" : t1[encrypt]);

	return (encrypt >= SIZE(t)? "?" : t[encrypt]);
} /* hci_encrypt2str */

char const *
hci_coding2str(int coding)
{
	static char const * const	t[] = {
		/* 0x00 */ "Linear",
		/* 0x01 */ "u-law",
		/* 0x02 */ "A-law",
		/* 0x03 */ "Reserved"
	};

	return (coding >= SIZE(t)? "?" : t[coding]);
} /* hci_coding2str */

char const *
hci_vdata2str(int data)
{
	static char const * const	t[] = {
		/* 0x00 */ "1's complement",
		/* 0x01 */ "2's complement",
		/* 0x02 */ "Sign-Magnitude",
		/* 0x03 */ "Reserved"
	};

	return (data >= SIZE(t)? "?" : t[data]);
} /* hci_vdata2str */

char const *
hci_hmode2str(int mode, char *buffer, int size)
{
	static char const * const	t[] = {
		/* 0x01 */ "Suspend Page Scan ",
		/* 0x02 */ "Suspend Inquiry Scan ",
		/* 0x04 */ "Suspend Periodic Inquiries "
        };

	if (buffer != NULL && size > 0) {
		int	n;

		memset(buffer, 0, size);
		size--;
		for (n = 0; n < SIZE(t); n++) {
			int	len = strlen(buffer);

			if (len >= size)
				break;
			if (mode & (1 << n))
				strncat(buffer, t[n], size - len);
		}
	}

	return (buffer);
} /* hci_hmode2str */

char const *
hci_ver2str(int ver)
{
	static char const * const	t[] = {
		/* 0x00 */ "Bluetooth HCI Specification 1.0B",
		/* 0x01 */ "Bluetooth HCI Specification 1.1",
		/* 0x02 */ "Bluetooth HCI Specification 1.2",
		/* 0x03 */ "Bluetooth HCI Specification 2.0",
		/* 0x04 */ "Bluetooth HCI Specification 2.1",
		/* 0x05 */ "Bluetooth HCI Specification 3.0",
		/* 0x06 */ "Bluetooth HCI Specification 4.0",
		/* 0x07 */ "Bluetooth HCI Specification 4.1",
		/* 0x08 */ "Bluetooth HCI Specification 4.2"		
	};

	return (ver >= SIZE(t)? "?" : t[ver]);
} /* hci_ver2str */

char const *
hci_lmpver2str(int ver)
{
	static char const * const	t[] = {
		/* 0x00 */ "Bluetooth LMP 1.0",
		/* 0x01 */ "Bluetooth LMP 1.1",
		/* 0x02 */ "Bluetooth LMP 1.2",
		/* 0x03 */ "Bluetooth LMP 2.0",
		/* 0x04 */ "Bluetooth LMP 2.1",
		/* 0x04 */ "Bluetooth LMP 3.0",
		/* 0x04 */ "Bluetooth LMP 4.0",
		/* 0x04 */ "Bluetooth LMP 4.1",
		/* 0x04 */ "Bluetooth LMP 4.2"		
	};

	return (ver >= SIZE(t)? "?" : t[ver]);
} /* hci_lmpver2str */

char const *
hci_manufacturer2str(int m)
{
	static char const * const	t[] = {
		/* 0000 */ "Ericsson Technology Licensing",
		/* 0001 */ "Nokia Mobile Phones",
		/* 0002 */ "Intel Corp.",
		/* 0003 */ "IBM Corp.",
		/* 0004 */ "Toshiba Corp.",
		/* 0005 */ "3Com",
		/* 0006 */ "Microsoft",
		/* 0007 */ "Lucent",
		/* 0008 */ "Motorola",
		/* 0009 */ "Infineon Technologies AG",
		/* 0010 */ "Cambridge Silicon Radio",
		/* 0011 */ "Silicon Wave",
		/* 0012 */ "Digianswer A/S",
		/* 0013 */ "Texas Instruments Inc.",
		/* 0014 */ "Parthus Technologies Inc.",
		/* 0015 */ "Broadcom Corporation",
		/* 0016 */ "Mitel Semiconductor",
		/* 0017 */ "Widcomm, Inc.",
		/* 0018 */ "Zeevo, Inc.",
		/* 0019 */ "Atmel Corporation",
		/* 0020 */ "Mitsubishi Electric Corporation",
		/* 0021 */ "RTX Telecom A/S",
		/* 0022 */ "KC Technology Inc.",
		/* 0023 */ "Newlogic",
		/* 0024 */ "Transilica, Inc.",
		/* 0025 */ "Rohde & Schwartz GmbH & Co. KG",
		/* 0026 */ "TTPCom Limited",
		/* 0027 */ "Signia Technologies, Inc.",
		/* 0028 */ "Conexant Systems Inc.",
		/* 0029 */ "Qualcomm",
		/* 0030 */ "Inventel",
		/* 0031 */ "AVM Berlin",
		/* 0032 */ "BandSpeed, Inc.",
		/* 0033 */ "Mansella Ltd",
		/* 0034 */ "NEC Corporation",
		/* 0035 */ "WavePlus Technology Co., Ltd.",
		/* 0036 */ "Alcatel",
		/* 0037 */ "Philips Semiconductors",
		/* 0038 */ "C Technologies",
		/* 0039 */ "Open Interface",
		/* 0040 */ "R F Micro Devices",
		/* 0041 */ "Hitachi Ltd",
		/* 0042 */ "Symbol Technologies, Inc.",
		/* 0043 */ "Tenovis",
		/* 0044 */ "Macronix International Co. Ltd.",
		/* 0045 */ "GCT Semiconductor",
		/* 0046 */ "Norwood Systems",
		/* 0047 */ "MewTel Technology Inc.",
		/* 0048 */ "ST Microelectronics",
		/* 0049 */ "Synopsys",
		/* 0050 */ "Red-M (Communications) Ltd",
		/* 0051 */ "Commil Ltd",
		/* 0052 */ "Computer Access Technology Corporation (CATC)",
		/* 0053 */ "Eclipse (HQ Espana) S.L.",
		/* 0054 */ "Renesas Technology Corp.",
		/* 0055 */ "Mobilian Corporation",
		/* 0056 */ "Terax",
		/* 0057 */ "Integrated System Solution Corp.",
		/* 0058 */ "Matsushita Electric Industrial Co., Ltd.",
		/* 0059 */ "Gennum Corporation",
		/* 0060 */ "Research In Motion",
		/* 0061 */ "IPextreme, Inc.",
		/* 0062 */ "Systems and Chips, Inc",
		/* 0063 */ "Bluetooth SIG, Inc",
		/* 0064 */ "Seiko Epson Corporation"
        };

	return (m >= SIZE(t)? "?" : t[m]);
} /* hci_manufacturer2str */

char const *
hci_features2str(uint8_t *features, char *buffer, int size)
{
	static char const * const	t[][8] = {
	{ /* byte 0 */
		/* 0 */ "<3-Slot> ",
		/* 1 */ "<5-Slot> ",
		/* 2 */ "<Encryption> ",
		/* 3 */ "<Slot offset> ",
		/* 4 */ "<Timing accuracy> ",
		/* 5 */ "<Switch> ",
		/* 6 */ "<Hold mode> ",
		/* 7 */ "<Sniff mode> "
	},
	{ /* byte 1 */
		/* 0 */ "<Park mode> ",
		/* 1 */ "<RSSI> ",
		/* 2 */ "<Channel quality> ",
		/* 3 */ "<SCO link> ",
		/* 4 */ "<HV2 packets> ",
		/* 5 */ "<HV3 packets> ",
		/* 6 */ "<u-law log> ",
		/* 7 */ "<A-law log> "
	},
	{ /* byte 2 */
		/* 0 */ "<CVSD> ",
		/* 1 */ "<Paging scheme> ",
		/* 2 */ "<Power control> ",
		/* 3 */ "<Transparent SCO data> ",
		/* 4 */ "<Flow control lag (bit0)> ",
		/* 5 */ "<Flow control lag (bit1)> ",
		/* 6 */ "<Flow control lag (bit2)> ",
		/* 7 */ "<Broadcast Encryption> "
	},
	{ /* byte 3 */
		/* 0 */ "<Unknown 3.0> ",
		/* 1 */ "<EDR ACL 2 Mb/s> ",
		/* 2 */ "<EDR ACL 3 Mb/s> ",
		/* 3 */ "<Enhanced inquiry scan> ",
		/* 4 */ "<Interlaced inquiry scan> ",
		/* 5 */ "<Interlaced page scan> ",
		/* 6 */ "<RSSI with inquiry results> ",
		/* 7 */ "<Extended SCO link (EV3 packets)> "
	},
	{ /* byte 4 */
		/* 0 */ "<EV4 packets> ",
		/* 1 */ "<EV5 packets> ",
		/* 2 */ "<Unknown 4.2> ",
		/* 3 */ "<AFH capable slave> ",
		/* 4 */ "<AFH classification slave> ",
		/* 5 */ "<BR/EDR Not Supported> ",
		/* 6 */ "<LE Supported (Controller)> ",
		/* 7 */ "<3-Slot EDR ACL packets> "
	},
	{ /* byte 5 */
		/* 0 */ "<5-Slot EDR ACL packets> ",
		/* 1 */ "<Sniff subrating> ",
		/* 2 */ "<Pause encryption> ",
		/* 3 */ "<AFH capable master> ",
		/* 4 */ "<AFH classification master> ",
		/* 5 */ "<EDR eSCO 2 Mb/s mode> ",
		/* 6 */ "<EDR eSCO 3 Mb/s mode> ",
		/* 7 */ "<3-Slot EDR eSCO packets> "
	},
	{ /* byte 6 */
		/* 0 */ "<Enhanced Inquiry Response> ",
		/* 1 */ "<Simultaneous LE and BR/EDR (Controller)> ",
		/* 2 */ "<Unknown 6.2> ",
		/* 3 */ "<Secure Simple Pairing (Controller Support)> ",
		/* 4 */ "<Encapsulated PDU> ",
		/* 5 */ "<Erroneous Data Reporting> ",
		/* 6 */ "<Non-flushable Packed Boundary Flag> ",
		/* 7 */ "<Unknown 6.7> "
	},
	{ /* byte 7 */
		/* 0 */ "<HCI_Link_Supervision_Timeout_Changed event> ",
		/* 1 */ "<Variable Inquiry TX Power Level> ",
		/* 2 */ "<Enhanced Power Control> ",
		/* 3 */ "<Unknown 7.3> ",
		/* 4 */ "<Unknown 7.4> ",
		/* 5 */ "<Unknown 7.5> ",
		/* 6 */ "<Unknown 7.6> ",
		/* 7 */ "<Extended features> "
	}};

	if (buffer != NULL && size > 0) {
		int	n, i, len0, len1;

		memset(buffer, 0, size);
		len1 = 0;
		size--;

		for (n = 0; n < SIZE(t); n++) {
			for (i = 0; i < SIZE(t[n]); i++) {
				len0 = strlen(buffer);
				if (len0 >= size)
					goto done;

				if (features[n] & (1 << i)) {
					if (len1 + strlen(t[n][i]) > 60) {
						len1 = 0;
						buffer[len0 - 1] = '\n';
					}

					len1 += strlen(t[n][i]);
					strncat(buffer, t[n][i], size - len0);
				}
			}
		}
	}
done:
	return (buffer);
} /* hci_features2str */

char const *
hci_le_features2str(uint8_t *features, char *buffer, int size)
{
	static char const * const t[][8] = {
	{ /* byte 0 */
		/* 0 */ "<LE Encryption> ",
		/* 1 */ "<Connection Parameters Request Procedure> ",
		/* 2 */ "<Extended Reject Indication> ",
		/* 3 */ "<Slave-initiated Features Exchange> ",
		/* 4 */ "<LE Ping> ",
		/* 5 */ "<LE Data Packet Length Extension> ",
		/* 6 */ "<LL Privacy> ",
		/* 7 */ "<Extended Scanner Filter Policies> "
	},
	{ /* byte 1 */
		/* 0 */ "<LE 2M PHY> ",
		/* 1 */ "<Stable Modulation Index - Transmitter> ",
		/* 2 */ "<Stable Modulation Index - Receiver> ",
		/* 3 */ "<LE Coded PHY> ",
		/* 4 */ "<LE Extended Advertising> ",
		/* 5 */ "<LE Periodic Advertising> ",
		/* 6 */ "<Channel Selection Algorithm #2> ",
		/* 7 */ "<LE Power Class 1> "
	},
	{ /* byte 2 */
		/* 0 */ "<Minimum Number of Used Channels Procedure> ",
		/* 1 */ "<Connection CTE Request> ",
		/* 2 */ "<Connection CTE Response> ",
		/* 3 */ "<Connectionless CTE Transmitter> ",
		/* 4 */ "<Connectionless CTE Receiver> ",
		/* 5 */ "<Antenna Switching During CTE Transmission (AoD)> ",
		/* 6 */ "<Antenna Switching During CTE Reception (AoA)> ",
		/* 7 */ "<Receiving Constant Tone Extensions> "
	},
	{ /* byte 3 */
		/* 0 */ "<Periodic Advertising Sync Transfer - Sender> ",
		/* 1 */ "<Periodic Advertising Sync Transfer - Recipient> ",
		/* 2 */ "<Sleep Clock Accuracy Updates> ",
		/* 3 */ "<Remote Public Key Validation> ",
		/* 4 */ "<Connected Isochronous Stream - Master> ",
		/* 5 */ "<Connected Isochronous Stream - Slave> ",
		/* 6 */ "<Isochronous Broadcaster> ",
		/* 7 */ "<Synchronized Receiver> "
	},
	{ /* byte 4 */
		/* 0 */ "<Isochronous Channels (Host Support)> ",
		/* 1 */ "<LE Power Control Request> ",
		/* 2 */ "<LE Power Change Indication> ",
		/* 3 */ "<LE Path Loss Monitoring> ",
		/* 4 */ "<Reserved for future use> ",
		/* 5 */ "<Unknown 4.5> ",
		/* 6 */ "<Unknown 4.6> ",
		/* 7 */ "<Unknown 4.7> "
	},
	{ /* byte 5 */
		/* 0 */ "<Unknown 5.0> ",
		/* 1 */ "<Unknown 5.1> ",
		/* 2 */ "<Unknown 5.2> ",
		/* 3 */ "<Unknown 5.3> ",
		/* 4 */ "<Unknown 5.4> ",
		/* 5 */ "<Unknown 5.5> ",
		/* 6 */ "<Unknown 5.6> ",
		/* 7 */ "<Unknown 5.7> "
	},
	{ /* byte 6 */
		/* 0 */ "<Unknown 6.0> ",
		/* 1 */ "<Unknown 6.1> ",
		/* 2 */ "<Unknown 6.2> ",
		/* 3 */ "<Unknown 6.3> ",
		/* 4 */ "<Unknown 6.4> ",
		/* 5 */ "<Unknown 6.5> ",
		/* 6 */ "<Unknown 6.6> ",
		/* 7 */ "<Unknown 6.7> "
	},
	{ /* byte 7 */
		/* 0 */ "<Unknown 7.0> ",
		/* 1 */ "<Unknown 7.1> ",
		/* 2 */ "<Unknown 7.2> ",
		/* 3 */ "<Unknown 7.3> ",
		/* 4 */ "<Unknown 7.4> ",
		/* 5 */ "<Unknown 7.5> ",
		/* 6 */ "<Unknown 7.6> ",
		/* 7 */ "<Unknown 7.7> "
	}};

	if (buffer != NULL && size > 0) {
		int n, i, len0, len1;

		memset(buffer, 0, size);
		len1 = 0;
		size--;

		for (n = 0; n < SIZE(t); n++) {
			for (i = 0; i < SIZE(t[n]); i++) {
				len0 = strlen(buffer);
				if (len0 >= size)
					goto done;

				if (features[n] & (1 << i)) {
					if (len1 + strlen(t[n][i]) > 60) {
						len1 = 0;
						buffer[len0 - 1] = '\n';
					}

					len1 += strlen(t[n][i]);
					strncat(buffer, t[n][i], size - len0);
				}
			}
		}
	}
done:
	return (buffer);
}

char const *
hci_cc2str(int cc)
{
	static char const * const	t[] = {
		/* 0x00 */ "North America, Europe, Japan",
		/* 0x01 */ "France"
	};

	return (cc >= SIZE(t)? "?" : t[cc]);
} /* hci_cc2str */

char const *
hci_con_state2str(int state)
{
	static char const * const	t[] = {
		/* NG_HCI_CON_CLOSED */           "CLOSED",
		/* NG_HCI_CON_W4_LP_CON_RSP */    "W4_LP_CON_RSP",
		/* NG_HCI_CON_W4_CONN_COMPLETE */ "W4_CONN_COMPLETE",
		/* NG_HCI_CON_OPEN */             "OPEN"
        };

	return (state >= SIZE(t)? "UNKNOWN" : t[state]);
} /* hci_con_state2str */

char const *
hci_status2str(int status)
{
	static char const * const       t[] = {
		/* 0x00 */ "No error",
		/* 0x01 */ "Unknown HCI command",
		/* 0x02 */ "No connection",
		/* 0x03 */ "Hardware failure",
		/* 0x04 */ "Page timeout",
		/* 0x05 */ "Authentication failure",
		/* 0x06 */ "Key missing",
		/* 0x07 */ "Memory full",
		/* 0x08 */ "Connection timeout",
		/* 0x09 */ "Max number of connections",
		/* 0x0a */ "Max number of SCO connections to a unit",
		/* 0x0b */ "ACL connection already exists",
		/* 0x0c */ "Command disallowed",
		/* 0x0d */ "Host rejected due to limited resources",
		/* 0x0e */ "Host rejected due to security reasons",
		/* 0x0f */ "Host rejected due to remote unit is a personal unit",
		/* 0x10 */ "Host timeout",
		/* 0x11 */ "Unsupported feature or parameter value",
		/* 0x12 */ "Invalid HCI command parameter",
		/* 0x13 */ "Other end terminated connection: User ended connection",
		/* 0x14 */ "Other end terminated connection: Low resources",
		/* 0x15 */ "Other end terminated connection: About to power off",
		/* 0x16 */ "Connection terminated by local host",
		/* 0x17 */ "Repeated attempts",
		/* 0x18 */ "Pairing not allowed",
		/* 0x19 */ "Unknown LMP PDU",
		/* 0x1a */ "Unsupported remote feature",
		/* 0x1b */ "SCO offset rejected",
		/* 0x1c */ "SCO interval rejected",
		/* 0x1d */ "SCO air mode rejected",
		/* 0x1e */ "Invalid LMP parameters",
		/* 0x1f */ "Unspecified error",
		/* 0x20 */ "Unsupported LMP parameter value",
		/* 0x21 */ "Role change not allowed",
		/* 0x22 */ "LMP response timeout",
		/* 0x23 */ "LMP error transaction collision",
		/* 0x24 */ "LMP PSU not allowed",
		/* 0x25 */ "Encryption mode not acceptable",
		/* 0x26 */ "Unit key used",
		/* 0x27 */ "QoS is not supported",
		/* 0x28 */ "Instant passed",
		/* 0x29 */ "Pairing with unit key not supported",
		/* 0x2a */ "Different Transaction Collision",
		/* 0x2b */ "Unknown error (Reserved for future use)",
		/* 0x2c */ "QoS Unacceptable Parameter",
		/* 0x2d */ "QoS Rejected",
		/* 0x2e */ "Channel Classification Not Supported",
		/* 0x2f */ "Insufficient Security",
		/* 0x30 */ "Parameter Out Of Mandatory Range",
		/* 0x31 */ "Unknown error (Reserved for future use)",
		/* 0x32 */ "Role Switch Pending",
		/* 0x33 */ "Unknown error (Reserved for future use)",
		/* 0x34 */ "Reserved Slot Violation",
		/* 0x35 */ "Role Switch Failed",
		/* 0x36 */ "Extended Inquiry Response Too Large",
		/* 0x37 */ "Secure Simple Pairing Not Supported By Host",
		/* 0x38 */ "Host Busy - Pairing",
		/* 0x39 */ "Connection Rejected due to No Suitable Channel Found",
		/* 0x3a */ "Controller Busy",
		/* 0x3b */ "Unacceptable Connection Parameters",
		/* 0x3c */ "Advertising Timeout",
		/* 0x3d */ "Connection Terminated due to MIC Failure",
		/* 0x3e */ "Connection Failed to be Established / Synchronization Timeout",
		/* 0x3f */ "MAC Connection Failed",
		/* 0x40 */ "Coarse Clock Adjustment Rejected but Will Try to Adjust Using Clock Dragging",
		/* 0x41 */ "Type0 Submap Not Defined",
		/* 0x42 */ "Unknown Advertising Identifier",
		/* 0x43 */ "Limit Reached",
		/* 0x44 */ "Operation Cancelled by Host",
		/* 0x45 */ "Packet Too Long"
	};

	return (status >= SIZE(t)? "Unknown error" : t[status]);
} /* hci_status2str */

char const *
hci_bdaddr2str(bdaddr_t const *ba)
{
	extern int	 numeric_bdaddr;
	static char	 buffer[MAXHOSTNAMELEN];
	struct hostent	*he = NULL;

	if (memcmp(ba, NG_HCI_BDADDR_ANY, sizeof(*ba)) == 0) {
		buffer[0] = '*';
		buffer[1] = 0;

		return (buffer);
	}

	if (!numeric_bdaddr &&
	    (he = bt_gethostbyaddr((char *)ba, sizeof(*ba), AF_BLUETOOTH)) != NULL) {
		strlcpy(buffer, he->h_name, sizeof(buffer));

		return (buffer);
	}

	bt_ntoa(ba, buffer);

	return (buffer);
} /* hci_bdaddr2str */

