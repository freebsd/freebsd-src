/*
 * Driver for ST5481 USB ISDN modem
 *
 * Author       Frode Isaksen
 * Copyright    2001 by Frode Isaksen      <fisaksen@bewan.com>
 *              2001 by Kai Germaschewski  <kai.germaschewski@gmx.de>
 * 
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#include "st5481_hdlc.h"

static const unsigned short int crc16_tab[] = {
	0x0000,0x1189,0x2312,0x329b,0x4624,0x57ad,0x6536,0x74bf,
	0x8c48,0x9dc1,0xaf5a,0xbed3,0xca6c,0xdbe5,0xe97e,0xf8f7,
	0x1081,0x0108,0x3393,0x221a,0x56a5,0x472c,0x75b7,0x643e,
	0x9cc9,0x8d40,0xbfdb,0xae52,0xdaed,0xcb64,0xf9ff,0xe876,
	0x2102,0x308b,0x0210,0x1399,0x6726,0x76af,0x4434,0x55bd,
	0xad4a,0xbcc3,0x8e58,0x9fd1,0xeb6e,0xfae7,0xc87c,0xd9f5,
	0x3183,0x200a,0x1291,0x0318,0x77a7,0x662e,0x54b5,0x453c,
	0xbdcb,0xac42,0x9ed9,0x8f50,0xfbef,0xea66,0xd8fd,0xc974,
	0x4204,0x538d,0x6116,0x709f,0x0420,0x15a9,0x2732,0x36bb,
	0xce4c,0xdfc5,0xed5e,0xfcd7,0x8868,0x99e1,0xab7a,0xbaf3,
	0x5285,0x430c,0x7197,0x601e,0x14a1,0x0528,0x37b3,0x263a,
	0xdecd,0xcf44,0xfddf,0xec56,0x98e9,0x8960,0xbbfb,0xaa72,
	0x6306,0x728f,0x4014,0x519d,0x2522,0x34ab,0x0630,0x17b9,
	0xef4e,0xfec7,0xcc5c,0xddd5,0xa96a,0xb8e3,0x8a78,0x9bf1,
	0x7387,0x620e,0x5095,0x411c,0x35a3,0x242a,0x16b1,0x0738,
	0xffcf,0xee46,0xdcdd,0xcd54,0xb9eb,0xa862,0x9af9,0x8b70,
	0x8408,0x9581,0xa71a,0xb693,0xc22c,0xd3a5,0xe13e,0xf0b7,
	0x0840,0x19c9,0x2b52,0x3adb,0x4e64,0x5fed,0x6d76,0x7cff,
	0x9489,0x8500,0xb79b,0xa612,0xd2ad,0xc324,0xf1bf,0xe036,
	0x18c1,0x0948,0x3bd3,0x2a5a,0x5ee5,0x4f6c,0x7df7,0x6c7e,
	0xa50a,0xb483,0x8618,0x9791,0xe32e,0xf2a7,0xc03c,0xd1b5,
	0x2942,0x38cb,0x0a50,0x1bd9,0x6f66,0x7eef,0x4c74,0x5dfd,
	0xb58b,0xa402,0x9699,0x8710,0xf3af,0xe226,0xd0bd,0xc134,
	0x39c3,0x284a,0x1ad1,0x0b58,0x7fe7,0x6e6e,0x5cf5,0x4d7c,
	0xc60c,0xd785,0xe51e,0xf497,0x8028,0x91a1,0xa33a,0xb2b3,
	0x4a44,0x5bcd,0x6956,0x78df,0x0c60,0x1de9,0x2f72,0x3efb,
	0xd68d,0xc704,0xf59f,0xe416,0x90a9,0x8120,0xb3bb,0xa232,
	0x5ac5,0x4b4c,0x79d7,0x685e,0x1ce1,0x0d68,0x3ff3,0x2e7a,
	0xe70e,0xf687,0xc41c,0xd595,0xa12a,0xb0a3,0x8238,0x93b1,
	0x6b46,0x7acf,0x4854,0x59dd,0x2d62,0x3ceb,0x0e70,0x1ff9,
	0xf78f,0xe606,0xd49d,0xc514,0xb1ab,0xa022,0x92b9,0x8330,
	0x7bc7,0x6a4e,0x58d5,0x495c,0x3de3,0x2c6a,0x1ef1,0x0f78
};



enum {
	HDLC_FAST_IDLE,HDLC_GET_FLAG_B0,HDLC_GETFLAG_B1A6,HDLC_GETFLAG_B7,
	HDLC_GET_DATA,HDLC_FAST_FLAG
};

enum {
	HDLC_SEND_DATA,HDLC_SEND_CRC1,HDLC_SEND_FAST_FLAG,
	HDLC_SEND_FIRST_FLAG,HDLC_SEND_CRC2,HDLC_SEND_CLOSING_FLAG,
	HDLC_SEND_IDLE1,HDLC_SEND_FAST_IDLE,HDLC_SENDFLAG_B0,
	HDLC_SENDFLAG_B1A6,HDLC_SENDFLAG_B7,STOPPED
};

void 
hdlc_rcv_init(struct hdlc_vars *hdlc, int do_adapt56)
{
   	hdlc->bit_shift = 0;
	hdlc->hdlc_bits1 = 0;
	hdlc->data_bits = 0;
	hdlc->ffbit_shift = 0;
	hdlc->data_received = 0;
	hdlc->state = HDLC_GET_DATA;
	hdlc->do_adapt56 = do_adapt56;
	hdlc->dchannel = 0;
	hdlc->crc = 0;
	hdlc->cbin = 0;
	hdlc->shift_reg = 0;
	hdlc->ffvalue = 0;
	hdlc->dstpos = 0;
}

void 
hdlc_out_init(struct hdlc_vars *hdlc, int is_d_channel, int do_adapt56)
{
   	hdlc->bit_shift = 0;
	hdlc->hdlc_bits1 = 0;
	hdlc->data_bits = 0;
	hdlc->ffbit_shift = 0;
	hdlc->data_received = 0;
	hdlc->do_closing = 0;
	hdlc->ffvalue = 0;
	if (is_d_channel) {
		hdlc->dchannel = 1;
		hdlc->state = HDLC_SEND_FIRST_FLAG;
	} else {
		hdlc->dchannel = 0;
		hdlc->state = HDLC_SEND_FAST_FLAG;
		hdlc->ffvalue = 0x7e;
	} 
	hdlc->cbin = 0x7e;
	hdlc->bit_shift = 0;
	if(do_adapt56){
		hdlc->do_adapt56 = 1;		
		hdlc->data_bits = 0;
		hdlc->state = HDLC_SENDFLAG_B0;
	} else {
		hdlc->do_adapt56 = 0;		
		hdlc->data_bits = 8;
	}
	hdlc->shift_reg = 0;
}

/*
  hdlc_decode - decodes HDLC frames from a transparent bit stream.

  The source buffer is scanned for valid HDLC frames looking for
  flags (01111110) to indicate the start of a frame. If the start of
  the frame is found, the bit stuffing is removed (0 after 5 1's).
  When a new flag is found, the complete frame has been received
  and the CRC is checked.
  If a valid frame is found, the function returns the frame length 
  excluding the CRC with the bit HDLC_END_OF_FRAME set.
  If the beginning of a valid frame is found, the function returns
  the length. 
  If a framing error is found (too many 1s and not a flag) the function 
  returns the length with the bit HDLC_FRAMING_ERROR set.
  If a CRC error is found the function returns the length with the
  bit HDLC_CRC_ERROR set.
  If the frame length exceeds the destination buffer size, the function
  returns the length with the bit HDLC_LENGTH_ERROR set.

  src - source buffer
  slen - source buffer length
  count - number of bytes removed (decoded) from the source buffer
  dst _ destination buffer
  dsize - destination buffer size
  returns - number of decoded bytes in the destination buffer and status
  flag.
 */
int hdlc_decode(struct hdlc_vars *hdlc, const unsigned char *src,
		int slen, int *count, unsigned char *dst, int dsize)
{
	int status=0;

	static const unsigned char fast_flag[]={
		0x00,0x00,0x00,0x20,0x30,0x38,0x3c,0x3e,0x3f
	};

	static const unsigned char fast_flag_value[]={
		0x00,0x7e,0xfc,0xf9,0xf3,0xe7,0xcf,0x9f,0x3f
	};

	static const unsigned char fast_abort[]={
		0x00,0x00,0x80,0xc0,0xe0,0xf0,0xf8,0xfc,0xfe,0xff
	};

	*count = slen;

	while(slen > 0){
		if(hdlc->bit_shift==0){
			hdlc->cbin = *src++;
			slen--;
			hdlc->bit_shift = 8;
			if(hdlc->do_adapt56){
				hdlc->bit_shift --;
			}
		}

		switch(hdlc->state){
		case STOPPED:
			return 0;
		case HDLC_FAST_IDLE:
			if(hdlc->cbin == 0xff){
				hdlc->bit_shift = 0;
				break;
			}
			hdlc->state = HDLC_GET_FLAG_B0;
			hdlc->hdlc_bits1 = 0;
			hdlc->bit_shift = 8;
			break;
		case HDLC_GET_FLAG_B0:
			if(!(hdlc->cbin & 0x80)) {
				hdlc->state = HDLC_GETFLAG_B1A6;
				hdlc->hdlc_bits1 = 0;
			} else {
				if(!hdlc->do_adapt56){
					if(++hdlc->hdlc_bits1 >=8 ) if(hdlc->bit_shift==1)
						hdlc->state = HDLC_FAST_IDLE;
				}
			}
			hdlc->cbin<<=1;
			hdlc->bit_shift --;
			break;
		case HDLC_GETFLAG_B1A6:
			if(hdlc->cbin & 0x80){
				hdlc->hdlc_bits1++;
				if(hdlc->hdlc_bits1==6){
					hdlc->state = HDLC_GETFLAG_B7;
				}
			} else {
				hdlc->hdlc_bits1 = 0;
			}
			hdlc->cbin<<=1;
			hdlc->bit_shift --;
			break;
		case HDLC_GETFLAG_B7:
			if(hdlc->cbin & 0x80) {
				hdlc->state = HDLC_GET_FLAG_B0;
			} else {
				hdlc->state = HDLC_GET_DATA;
				hdlc->crc = 0xffff;
				hdlc->shift_reg = 0;
				hdlc->hdlc_bits1 = 0;
				hdlc->data_bits = 0;
				hdlc->data_received = 0;
			}
			hdlc->cbin<<=1;
			hdlc->bit_shift --;
			break;
		case HDLC_GET_DATA:
			if(hdlc->cbin & 0x80){
				hdlc->hdlc_bits1++;
				switch(hdlc->hdlc_bits1){
				case 6:
					break;
				case 7:
					if(hdlc->data_received) {
						// bad frame
						status = -HDLC_FRAMING_ERROR;
					}
					if(!hdlc->do_adapt56){
						if(hdlc->cbin==fast_abort[hdlc->bit_shift+1]){
							hdlc->state = HDLC_FAST_IDLE;
							hdlc->bit_shift=1;
							break;
						}
					} else {
						hdlc->state = HDLC_GET_FLAG_B0;
					}
					break;
				default:
					hdlc->shift_reg>>=1;
					hdlc->shift_reg |= 0x80;
					hdlc->data_bits++;
					break;
				}
			} else {
				switch(hdlc->hdlc_bits1){
				case 5:
					break;
				case 6:
					if(hdlc->data_received){
						if (hdlc->dstpos < 2) {
							status = -HDLC_FRAMING_ERROR;
						} else if (hdlc->crc != 0xf0b8){
							// crc error
							status = -HDLC_CRC_ERROR;
						} else {
							// remove CRC
							hdlc->dstpos -= 2;
							// good frame
							status = hdlc->dstpos;
						}
					}
					hdlc->crc = 0xffff;
					hdlc->shift_reg = 0;
					hdlc->data_bits = 0;
					if(!hdlc->do_adapt56){
						if(hdlc->cbin==fast_flag[hdlc->bit_shift]){
							hdlc->ffvalue = fast_flag_value[hdlc->bit_shift];
							hdlc->state = HDLC_FAST_FLAG;
							hdlc->ffbit_shift = hdlc->bit_shift;
							hdlc->bit_shift = 1;
						} else {
							hdlc->state = HDLC_GET_DATA;
							hdlc->data_received = 0;
						}
					} else {
						hdlc->state = HDLC_GET_DATA;
						hdlc->data_received = 0;
					}
					break;
				default:
					hdlc->shift_reg>>=1;
					hdlc->data_bits++;
					break;
				}
				hdlc->hdlc_bits1 = 0;
			}
			if (status) {
				hdlc->dstpos = 0;
				*count -= slen;
				hdlc->cbin <<= 1;
				hdlc->bit_shift--;
				return status;
			}
			if(hdlc->data_bits==8){
				unsigned cval;
				
				hdlc->data_bits = 0;
				hdlc->data_received = 1;
				cval = (hdlc->crc^hdlc->shift_reg) & 0xff;
				hdlc->crc = (hdlc->crc>>8)^crc16_tab[cval];
				// good byte received
				if (dsize--) {
					dst[hdlc->dstpos++] = hdlc->shift_reg;
				} else {
					// frame too long
					status = -HDLC_LENGTH_ERROR;
					hdlc->dstpos = 0;
				}
			}
			hdlc->cbin <<= 1;
			hdlc->bit_shift--;
			break;
		case HDLC_FAST_FLAG:
			if(hdlc->cbin==hdlc->ffvalue){
				hdlc->bit_shift = 0;
				break;
			} else {
				if(hdlc->cbin == 0xff){
					hdlc->state = HDLC_FAST_IDLE;
					hdlc->bit_shift=0;
				} else if(hdlc->ffbit_shift==8){
					hdlc->state = HDLC_GETFLAG_B7;
					break;
				} else {
					hdlc->shift_reg = fast_abort[hdlc->ffbit_shift-1];
					hdlc->hdlc_bits1 = hdlc->ffbit_shift-2;
					if(hdlc->hdlc_bits1<0)hdlc->hdlc_bits1 = 0;
					hdlc->data_bits = hdlc->ffbit_shift-1;
					hdlc->state = HDLC_GET_DATA;
					hdlc->data_received = 0;
				}
			}
			break;
		default:
			break;
		}
	}
	*count -= slen;
	return 0;
}

/*
  hdlc_encode - encodes HDLC frames to a transparent bit stream.

  The bit stream starts with a beginning flag (01111110). After
  that each byte is added to the bit stream with bit stuffing added
  (0 after 5 1's).
  When the last byte has been removed from the source buffer, the
  CRC (2 bytes is added) and the frame terminates with the ending flag.
  For the dchannel, the idle character (all 1's) is also added at the end.
  If this function is called with empty source buffer (slen=0), flags or
  idle character will be generated.
 
  src - source buffer
  slen - source buffer length
  count - number of bytes removed (encoded) from source buffer
  dst _ destination buffer
  dsize - destination buffer size
  returns - number of encoded bytes in the destination buffer
*/
int hdlc_encode(struct hdlc_vars *hdlc, const unsigned char *src, 
		unsigned short slen, int *count,
		unsigned char *dst, int dsize)
{
	static const unsigned char xfast_flag_value[] = {
		0x7e,0x3f,0x9f,0xcf,0xe7,0xf3,0xf9,0xfc,0x7e
	};

	int len = 0;

	*count = slen;

	while (dsize > 0) {
		if(hdlc->bit_shift==0){	
			if(slen && !hdlc->do_closing){
				hdlc->shift_reg = *src++;
				slen--;
				if (slen == 0) 
					hdlc->do_closing = 1;  /* closing sequence, CRC + flag(s) */
				hdlc->bit_shift = 8;
			} else {
				if(hdlc->state == HDLC_SEND_DATA){
					if(hdlc->data_received){
						hdlc->state = HDLC_SEND_CRC1;
						hdlc->crc ^= 0xffff;
						hdlc->bit_shift = 8;
						hdlc->shift_reg = hdlc->crc & 0xff;
					} else if(!hdlc->do_adapt56){
						hdlc->state = HDLC_SEND_FAST_FLAG;
					} else {
						hdlc->state = HDLC_SENDFLAG_B0;
					}
				}
			  
			}
		}

		switch(hdlc->state){
		case STOPPED:
			while (dsize--)
				*dst++ = 0xff;
		  
			return dsize;
		case HDLC_SEND_FAST_FLAG:
			hdlc->do_closing = 0;
			if(slen == 0){
				*dst++ = hdlc->ffvalue;
				len++;
				dsize--;
				break;
			}
			if(hdlc->bit_shift==8){
				hdlc->cbin = hdlc->ffvalue>>(8-hdlc->data_bits);
				hdlc->state = HDLC_SEND_DATA;
				hdlc->crc = 0xffff;
				hdlc->hdlc_bits1 = 0;
				hdlc->data_received = 1;
			}
			break;
		case HDLC_SENDFLAG_B0:
			hdlc->do_closing = 0;
			hdlc->cbin <<= 1;
			hdlc->data_bits++;
			hdlc->hdlc_bits1 = 0;
			hdlc->state = HDLC_SENDFLAG_B1A6;
			break;
		case HDLC_SENDFLAG_B1A6:
			hdlc->cbin <<= 1;
			hdlc->data_bits++;
			hdlc->cbin++;
			if(++hdlc->hdlc_bits1 == 6)
				hdlc->state = HDLC_SENDFLAG_B7;
			break;
		case HDLC_SENDFLAG_B7:
			hdlc->cbin <<= 1;
			hdlc->data_bits++;
			if(slen == 0){
				hdlc->state = HDLC_SENDFLAG_B0;
				break;
			}
			if(hdlc->bit_shift==8){
				hdlc->state = HDLC_SEND_DATA;
				hdlc->crc = 0xffff;
				hdlc->hdlc_bits1 = 0;
				hdlc->data_received = 1;
			}
			break;
		case HDLC_SEND_FIRST_FLAG:
			hdlc->data_received = 1;
			if(hdlc->data_bits==8){
				hdlc->state = HDLC_SEND_DATA;
				hdlc->crc = 0xffff;
				hdlc->hdlc_bits1 = 0;
				break;
			}
			hdlc->cbin <<= 1;
			hdlc->data_bits++;
			if(hdlc->shift_reg & 0x01)
				hdlc->cbin++;
			hdlc->shift_reg >>= 1;
			hdlc->bit_shift--;
			if(hdlc->bit_shift==0){
				hdlc->state = HDLC_SEND_DATA;
				hdlc->crc = 0xffff;
				hdlc->hdlc_bits1 = 0;
			}
			break;
		case HDLC_SEND_DATA:
			hdlc->cbin <<= 1;
			hdlc->data_bits++;
			if(hdlc->hdlc_bits1 == 5){
				hdlc->hdlc_bits1 = 0;
				break;
			}
			if(hdlc->bit_shift==8){
				unsigned cval;

				cval = (hdlc->crc^hdlc->shift_reg) & 0xff;
				hdlc->crc = (hdlc->crc>>8)^crc16_tab[cval];
			}
			if(hdlc->shift_reg & 0x01){
				hdlc->hdlc_bits1++;
				hdlc->cbin++;
				hdlc->shift_reg >>= 1;
				hdlc->bit_shift--;
			} else {
				hdlc->hdlc_bits1 = 0;
				hdlc->shift_reg >>= 1;
				hdlc->bit_shift--;
			}
			break;
		case HDLC_SEND_CRC1:
			hdlc->cbin <<= 1;
			hdlc->data_bits++;
			if(hdlc->hdlc_bits1 == 5){
				hdlc->hdlc_bits1 = 0;
				break;
			}
			if(hdlc->shift_reg & 0x01){
				hdlc->hdlc_bits1++;
				hdlc->cbin++;
				hdlc->shift_reg >>= 1;
				hdlc->bit_shift--;
			} else {
				hdlc->hdlc_bits1 = 0;
				hdlc->shift_reg >>= 1;
				hdlc->bit_shift--;
			}
			if(hdlc->bit_shift==0){
				hdlc->shift_reg = (hdlc->crc >> 8);
				hdlc->state = HDLC_SEND_CRC2;
				hdlc->bit_shift = 8;
			}
			break;
		case HDLC_SEND_CRC2:
			hdlc->cbin <<= 1;
			hdlc->data_bits++;
			if(hdlc->hdlc_bits1 == 5){
				hdlc->hdlc_bits1 = 0;
				break;
			}
			if(hdlc->shift_reg & 0x01){
				hdlc->hdlc_bits1++;
				hdlc->cbin++;
				hdlc->shift_reg >>= 1;
				hdlc->bit_shift--;
			} else {
				hdlc->hdlc_bits1 = 0;
				hdlc->shift_reg >>= 1;
				hdlc->bit_shift--;
			}
			if(hdlc->bit_shift==0){
				hdlc->shift_reg = 0x7e;
				hdlc->state = HDLC_SEND_CLOSING_FLAG;
				hdlc->bit_shift = 8;
			}
			break;
		case HDLC_SEND_CLOSING_FLAG:
			hdlc->cbin <<= 1;
			hdlc->data_bits++;
			if(hdlc->hdlc_bits1 == 5){
				hdlc->hdlc_bits1 = 0;
				break;
			}
			if(hdlc->shift_reg & 0x01){
				hdlc->cbin++;
			}
			hdlc->shift_reg >>= 1;
			hdlc->bit_shift--;
			if(hdlc->bit_shift==0){
				hdlc->ffvalue = xfast_flag_value[hdlc->data_bits];
				if(hdlc->dchannel){
					hdlc->ffvalue = 0x7e;
					hdlc->state = HDLC_SEND_IDLE1;
					hdlc->bit_shift = 8-hdlc->data_bits;
					if(hdlc->bit_shift==0)
						hdlc->state = HDLC_SEND_FAST_IDLE;
				} else {
					if(!hdlc->do_adapt56){
						hdlc->state = HDLC_SEND_FAST_FLAG;
						hdlc->data_received = 0;
					} else {
						hdlc->state = HDLC_SENDFLAG_B0;
						hdlc->data_received = 0;
					}
					// Finished with this frame, send flags
					if (dsize > 1) dsize = 1; 
				}
			}
			break;
		case HDLC_SEND_IDLE1:
			hdlc->do_closing = 0;
			hdlc->cbin <<= 1;
			hdlc->cbin++;
			hdlc->data_bits++;
			hdlc->bit_shift--;
			if(hdlc->bit_shift==0){
				hdlc->state = HDLC_SEND_FAST_IDLE;
				hdlc->bit_shift = 0;
			}
			break;
		case HDLC_SEND_FAST_IDLE:
			hdlc->do_closing = 0;
			hdlc->cbin = 0xff;
			hdlc->data_bits = 8;
			if(hdlc->bit_shift == 8){
				hdlc->cbin = 0x7e;
				hdlc->state = HDLC_SEND_FIRST_FLAG;
			} else {
				*dst++ = hdlc->cbin;
				hdlc->bit_shift = hdlc->data_bits = 0;
				len++;
				dsize = 0;
			}
			break;
		default:
			break;
		}
		if(hdlc->do_adapt56){
			if(hdlc->data_bits==7){
				hdlc->cbin <<= 1;
				hdlc->cbin++;
				hdlc->data_bits++;
			}
		}
		if(hdlc->data_bits==8){
			*dst++ = hdlc->cbin;
			hdlc->data_bits = 0;
			len++;
			dsize--;
		}
	}
	*count -= slen;

	return len;
}

