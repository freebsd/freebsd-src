/*********************************************************************
 *                
 * Filename:      via-ircc.h
 * Version:       1.0
 * Description:   Driver for the VIA VT8231/VT8233 IrDA chipsets
 * Author:        VIA Technologies, inc
 * Date  :	  08/06/2003

Copyright (c) 1998-2003 VIA Technologies, Inc.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTIES OR REPRESENTATIONS; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

 * Comment:
 * jul/08/2002 : Rx buffer length should use Rx ring ptr.	
 * Oct/28/2002 : Add SB id for 3147 and 3177.	
 * jul/09/2002 : only implement two kind of dongle currently.
 * Oct/02/2002 : work on VT8231 and VT8233 .
 * Aug/06/2003 : change driver format to pci driver .
 ********************************************************************/
#ifndef via_IRCC_H
#define via_IRCC_H
#include <linux/time.h>
#include <linux/spinlock.h>
#include <linux/pm.h>
#include <asm/io.h>

#define MAX_TX_WINDOW 7
#define MAX_RX_WINDOW 7

struct st_fifo_entry {
	int status;
	int len;
};

struct st_fifo {
	struct st_fifo_entry entries[MAX_RX_WINDOW + 2];
	int pending_bytes;
	int head;
	int tail;
	int len;
};

struct frame_cb {
	void *start;		/* Start of frame in DMA mem */
	int len;		/* Lenght of frame in DMA mem */
};

struct tx_fifo {
	struct frame_cb queue[MAX_TX_WINDOW + 2];	/* Info about frames in queue */
	int ptr;		/* Currently being sent */
	int len;		/* Lenght of queue */
	int free;		/* Next free slot */
	void *tail;		/* Next free start in DMA mem */
};


struct eventflag		// for keeping track of Interrupt Events
{
	//--------tx part
	unsigned char TxFIFOUnderRun;
	unsigned char EOMessage;
	unsigned char TxFIFOReady;
	unsigned char EarlyEOM;
	//--------rx part
	unsigned char PHYErr;
	unsigned char CRCErr;
	unsigned char RxFIFOOverRun;
	unsigned char EOPacket;
	unsigned char RxAvail;
	unsigned char TooLargePacket;
	unsigned char SIRBad;
	//--------unknown
	unsigned char Unknown;
	//----------
	unsigned char TimeOut;
	unsigned char RxDMATC;
	unsigned char TxDMATC;
};

/* Private data for each instance */
struct via_ircc_cb {
	struct st_fifo st_fifo;	/* Info about received frames */
	struct tx_fifo tx_fifo;	/* Info about frames to be transmitted */

	struct net_device *netdev;	/* Yes! we are some kind of netdevice */
	struct net_device_stats stats;

	struct irlap_cb *irlap;	/* The link layer we are binded to */
	struct qos_info qos;	/* QoS capabilities for this device */

	chipio_t io;		/* IrDA controller information */
	iobuff_t tx_buff;	/* Transmit buffer */
	iobuff_t rx_buff;	/* Receive buffer */

	__u8 ier;		/* Interrupt enable register */

	struct timeval stamp;
	struct timeval now;

	spinlock_t lock;	/* For serializing operations */

	__u32 flags;		/* Interface flags */
	__u32 new_speed;
	int index;		/* Instance index */

	struct eventflag EventFlag;
	struct pm_dev *dev;
	unsigned int chip_id;	/* to remember chip id */
	unsigned int RetryCount;
	unsigned int RxDataReady;
	unsigned int RxLastCount;
};


//---------I=Infrared,  H=Host, M=Misc, T=Tx, R=Rx, ST=Status,
//         CF=Config, CT=Control, L=Low, H=High, C=Count
#define  I_CF_L_0  		0x10
#define  I_CF_H_0		0x11
#define  I_SIR_BOF		0x12
#define  I_SIR_EOF		0x13
#define  I_ST_CT_0		0x15
#define  I_ST_L_1		0x16
#define  I_ST_H_1		0x17
#define  I_CF_L_1		0x18
#define  I_CF_H_1		0x19
#define  I_CF_L_2		0x1a
#define  I_CF_H_2		0x1b
#define  I_CF_3		0x1e
#define  H_CT			0x20
#define  H_ST			0x21
#define  M_CT			0x22
#define  TX_CT_1		0x23
#define  TX_CT_2		0x24
#define  TX_ST			0x25
#define  RX_CT			0x26
#define  RX_ST			0x27
#define  RESET			0x28
#define  P_ADDR		0x29
#define  RX_C_L		0x2a
#define  RX_C_H		0x2b
#define  RX_P_L		0x2c
#define  RX_P_H		0x2d
#define  TX_C_L		0x2e
#define  TX_C_H		0x2f
#define  TIMER         	0x32
#define  I_CF_4         	0x33
#define  I_T_C_L		0x34
#define  I_T_C_H		0x35
#define  VERSION		0x3f
//-------------------------------
#define StartAddr 	0x10	// the first register address
#define EndAddr 	0x3f	// the last register address
#define GetBit(val,bit)  val = (unsigned char) ((val>>bit) & 0x1)
			// Returns the bit
#define SetBit(val,bit)  val= (unsigned char ) (val | (0x1 << bit))
			// Sets bit to 1
#define ResetBit(val,bit) val= (unsigned char ) (val & ~(0x1 << bit))
			// Sets bit to 0
#define PCI_CONFIG_ADDRESS 0xcf8
#define PCI_CONFIG_DATA    0xcfc

#define VenderID    0x1106
#define DeviceID1   0x8231
#define DeviceID2   0x3109
#define DeviceID3   0x3074
//F01_S
#define DeviceID4   0x3147
#define DeviceID5   0x3177
//F01_E

#define OFF   0
#define ON   1
#define DMA_TX_MODE   0x08
#define DMA_RX_MODE   0x04

#define DMA1   0
#define DMA2   0xc0
#define MASK1   DMA1+0x0a
#define MASK2   DMA2+0x14

#define Clk_bit 0x40
#define Tx_bit 0x01
#define Rd_Valid 0x08
#define RxBit 0x08

__u8 ReadPCIByte(__u8, __u8, __u8, __u8);
__u32 ReadPCI(__u8, __u8, __u8, __u8);
void WritePCI(__u8, __u8, __u8, __u8, __u32);
void WritePCIByte(__u8, __u8, __u8, __u8, __u8);
int mySearchPCI(__u8 *, __u16, __u16);


void DisableDmaChannel(unsigned int channel)
{
	switch (channel) {	// 8 Bit DMA channels DMAC1
	case 0:
		outb(4, MASK1);	//mask channel 0
		break;
	case 1:
		outb(5, MASK1);	//Mask channel 1
		break;
	case 2:
		outb(6, MASK1);	//Mask channel 2
		break;
	case 3:
		outb(7, MASK1);	//Mask channel 3
		break;
	case 5:
		outb(5, MASK2);	//Mask channel 5
		break;
	case 6:
		outb(6, MASK2);	//Mask channel 6
		break;
	case 7:
		outb(7, MASK2);	//Mask channel 7
		break;
	default:
		break;
	};			//Switch
}

unsigned char ReadLPCReg(int iRegNum)
{
	unsigned char iVal;

	outb(0x87, 0x2e);
	outb(0x87, 0x2e);
	outb(iRegNum, 0x2e);
	iVal = inb(0x2f);
	outb(0xaa, 0x2e);

	return iVal;
}

void WriteLPCReg(int iRegNum, unsigned char iVal)
{

	outb(0x87, 0x2e);
	outb(0x87, 0x2e);
	outb(iRegNum, 0x2e);
	outb(iVal, 0x2f);
	outb(0xAA, 0x2e);
}

__u8 ReadReg(unsigned int BaseAddr, int iRegNum)
{
	return ((__u8) inb(BaseAddr + iRegNum));
}

void WriteReg(unsigned int BaseAddr, int iRegNum, unsigned char iVal)
{
	outb(iVal, BaseAddr + iRegNum);
}

int WriteRegBit(unsigned int BaseAddr, unsigned char RegNum,
		unsigned char BitPos, unsigned char value)
{
	__u8 Rtemp, Wtemp;

	if (BitPos > 7) {
		return -1;
	}
	if ((RegNum < StartAddr) || (RegNum > EndAddr))
		return -1;
	Rtemp = ReadReg(BaseAddr, RegNum);
	if (value == 0)
		Wtemp = ResetBit(Rtemp, BitPos);
	else {
		if (value == 1)
			Wtemp = SetBit(Rtemp, BitPos);
		else
			return -1;
	}
	WriteReg(BaseAddr, RegNum, Wtemp);
	return 0;
}

__u8 CheckRegBit(unsigned int BaseAddr, unsigned char RegNum,
		 unsigned char BitPos)
{
	__u8 temp;

	if (BitPos > 7)
		return 0xff;
	if ((RegNum < StartAddr) || (RegNum > EndAddr)) {
//     printf("what is the register %x!\n",RegNum);
	}
	temp = ReadReg(BaseAddr, RegNum);
	return GetBit(temp, BitPos);
}

__u8 ReadPCIByte(__u8 bus, __u8 device, __u8 fun, __u8 reg)
{
	__u32 dTmp;
	__u8 bData, bTmp;

	bTmp = reg & ~0x03;
	dTmp = ReadPCI(bus, device, fun, bTmp);
	bTmp = reg & 0x03;
	bData = (__u8) (dTmp >> bTmp);
	return bData;
}

__u32 ReadPCI(__u8 bus, __u8 device, __u8 fun, __u8 reg)
{
	__u32 CONFIG_ADDR, temp, data;

	if ((bus == 0xff) || (device == 0xff) || (fun == 0xff))
		return 0xffffffff;
	CONFIG_ADDR = 0x80000000;
	temp = (__u32) reg << 2;
	CONFIG_ADDR = CONFIG_ADDR | temp;
	temp = (__u32) fun << 8;
	CONFIG_ADDR = CONFIG_ADDR | temp;
	temp = (__u32) device << 11;
	CONFIG_ADDR = CONFIG_ADDR | temp;
	temp = (__u32) bus << 16;
	CONFIG_ADDR = CONFIG_ADDR | temp;

	outl(PCI_CONFIG_ADDRESS, CONFIG_ADDR);
	data = inl(PCI_CONFIG_DATA);
	return data;
}


void WritePCIByte(__u8 bus, __u8 device, __u8 fun, __u8 reg,
		  __u8 CONFIG_DATA)
{
	__u32 dTmp, dTmp1 = 0;
	__u8 bTmp;

	bTmp = reg & ~0x03;
	dTmp = ReadPCI(bus, device, fun, bTmp);
	switch (reg & 0x03) {
	case 0:
		dTmp1 = (dTmp & ~0xff) | CONFIG_DATA;
		break;
	case 1:
		dTmp = (dTmp & ~0x00ff00);
		dTmp1 = CONFIG_DATA;
		dTmp1 = dTmp1 << 8;
		dTmp1 = dTmp1 | dTmp;
		break;
	case 2:
		dTmp = (dTmp & ~0xff0000);
		dTmp1 = CONFIG_DATA;
		dTmp1 = dTmp1 << 16;
		dTmp1 = dTmp1 | dTmp;
		break;
	case 3:
		dTmp = (dTmp & ~0xff000000);
		dTmp1 = CONFIG_DATA;
		dTmp1 = dTmp1 << 24;
		dTmp1 = dTmp1 | dTmp;
		break;
	}
	WritePCI(bus, device, fun, bTmp, dTmp1);
}

//------------------
void WritePCI(__u8 bus, __u8 device, __u8 fun, __u8 reg, __u32 CONFIG_DATA)
{
	__u32 CONFIG_ADDR, temp;

	if ((bus == 0xff) || (device == 0xff) || (fun == 0xff))
		return;
	CONFIG_ADDR = 0x80000000;
	temp = (__u32) reg << 2;
	CONFIG_ADDR = CONFIG_ADDR | temp;
	temp = (__u32) fun << 8;
	CONFIG_ADDR = CONFIG_ADDR | temp;
	temp = (__u32) device << 11;
	CONFIG_ADDR = CONFIG_ADDR | temp;
	temp = (__u32) bus << 16;
	CONFIG_ADDR = CONFIG_ADDR | temp;

	outl(PCI_CONFIG_ADDRESS, CONFIG_ADDR);
	outl(PCI_CONFIG_DATA, CONFIG_DATA);

}

											// find device with DeviceID and VenderID                                       // if match return three byte buffer (bus,device,function)                      // no found, address={99,99,99} 
int mySearchPCI(__u8 * SBridpos, __u16 VID, __u16 DID)
{
	__u8 i, j, k;
	__u16 FindDeviceID, FindVenderID;

	for (k = 0; k < 8; k++) {	//scan function
		i = 0;
		j = 0x11;
		k = 0;
		if (ReadPCI(i, j, k, 0) < 0xffffffff) {	// not empty
			FindDeviceID = (__u16) (ReadPCI(i, j, k, 0) >> 16);
			FindVenderID =
			    (__u16) (ReadPCI(i, j, k, 0) & 0x0000ffff);
			if ((VID == FindVenderID) && (DID == FindDeviceID)) {
				SBridpos[0] = i;	// bus
				SBridpos[1] = j;	//device
				SBridpos[2] = k;	//func
				return 1;
			}
		}
	}
	return 0;
}

void SetMaxRxPacketSize(__u16 iobase, __u16 size)
{
	__u16 low, high;
	if ((size & 0xe000) == 0) {
		low = size & 0x00ff;
		high = (size & 0x1f00) >> 8;
		WriteReg(iobase, I_CF_L_2, low);
		WriteReg(iobase, I_CF_H_2, high);

	}

}

//for both Rx and Tx

void SetFIFO(__u16 iobase, __u16 value)
{
	switch (value) {
	case 128:
		WriteRegBit(iobase, 0x11, 0, 0);
		WriteRegBit(iobase, 0x11, 7, 1);
		break;
	case 64:
		WriteRegBit(iobase, 0x11, 0, 0);
		WriteRegBit(iobase, 0x11, 7, 0);
		break;
	case 32:
		WriteRegBit(iobase, 0x11, 0, 1);
		WriteRegBit(iobase, 0x11, 7, 0);
		break;
	default:
		WriteRegBit(iobase, 0x11, 0, 0);
		WriteRegBit(iobase, 0x11, 7, 0);
	}

}

#define CRC16(BaseAddr,val)         WriteRegBit(BaseAddr,I_CF_L_0,7,val)	//0 for 32 CRC
/*
#define SetVFIR(BaseAddr,val)       WriteRegBit(BaseAddr,I_CF_H_0,5,val)
#define SetFIR(BaseAddr,val)        WriteRegBit(BaseAddr,I_CF_L_0,6,val)
#define SetMIR(BaseAddr,val)        WriteRegBit(BaseAddr,I_CF_L_0,5,val)
#define SetSIR(BaseAddr,val)        WriteRegBit(BaseAddr,I_CF_L_0,4,val)
*/
#define SIRFilter(BaseAddr,val)     WriteRegBit(BaseAddr,I_CF_L_0,3,val)
#define Filter(BaseAddr,val)        WriteRegBit(BaseAddr,I_CF_L_0,2,val)
#define InvertTX(BaseAddr,val)      WriteRegBit(BaseAddr,I_CF_L_0,1,val)
#define InvertRX(BaseAddr,val)      WriteRegBit(BaseAddr,I_CF_L_0,0,val)
//****************************I_CF_H_0
#define EnableTX(BaseAddr,val)      WriteRegBit(BaseAddr,I_CF_H_0,4,val)
#define EnableRX(BaseAddr,val)      WriteRegBit(BaseAddr,I_CF_H_0,3,val)
#define EnableDMA(BaseAddr,val)     WriteRegBit(BaseAddr,I_CF_H_0,2,val)
#define SIRRecvAny(BaseAddr,val)    WriteRegBit(BaseAddr,I_CF_H_0,1,val)
#define DiableTrans(BaseAddr,val)   WriteRegBit(BaseAddr,I_CF_H_0,0,val)
//***************************I_SIR_BOF,I_SIR_EOF
#define SetSIRBOF(BaseAddr,val)     WriteReg(BaseAddr,I_SIR_BOF,val)
#define SetSIREOF(BaseAddr,val)     WriteReg(BaseAddr,I_SIR_EOF,val)
#define GetSIRBOF(BaseAddr)        ReadReg(BaseAddr,I_SIR_BOF)
#define GetSIREOF(BaseAddr)        ReadReg(BaseAddr,I_SIR_EOF)
//*******************I_ST_CT_0
#define EnPhys(BaseAddr,val)   WriteRegBit(BaseAddr,I_ST_CT_0,7,val)
#define IsModeError(BaseAddr) CheckRegBit(BaseAddr,I_ST_CT_0,6)	//RO
#define IsVFIROn(BaseAddr)     CheckRegBit(BaseAddr,0x14,0)	//RO for VT1211 only
#define IsFIROn(BaseAddr)     CheckRegBit(BaseAddr,I_ST_CT_0,5)	//RO
#define IsMIROn(BaseAddr)     CheckRegBit(BaseAddr,I_ST_CT_0,4)	//RO
#define IsSIROn(BaseAddr)     CheckRegBit(BaseAddr,I_ST_CT_0,3)	//RO
#define IsEnableTX(BaseAddr)  CheckRegBit(BaseAddr,I_ST_CT_0,2)	//RO
#define IsEnableRX(BaseAddr)  CheckRegBit(BaseAddr,I_ST_CT_0,1)	//RO
#define Is16CRC(BaseAddr)     CheckRegBit(BaseAddr,I_ST_CT_0,0)	//RO
//***************************I_CF_3
#define DisableAdjacentPulseWidth(BaseAddr,val) WriteRegBit(BaseAddr,I_CF_3,5,val)	//1 disable
#define DisablePulseWidthAdjust(BaseAddr,val)   WriteRegBit(BaseAddr,I_CF_3,4,val)	//1 disable
#define UseOneRX(BaseAddr,val)                  WriteRegBit(BaseAddr,I_CF_3,1,val)	//0 use two RX
#define SlowIRRXLowActive(BaseAddr,val)         WriteRegBit(BaseAddr,I_CF_3,0,val)	//0 show RX high=1 in SIR
//***************************H_CT
#define EnAllInt(BaseAddr,val)   WriteRegBit(BaseAddr,H_CT,7,val)
#define TXStart(BaseAddr,val)    WriteRegBit(BaseAddr,H_CT,6,val)
#define RXStart(BaseAddr,val)    WriteRegBit(BaseAddr,H_CT,5,val)
#define ClearRXInt(BaseAddr,val)   WriteRegBit(BaseAddr,H_CT,4,val)	// 1 clear
//*****************H_ST
#define IsRXInt(BaseAddr)           CheckRegBit(BaseAddr,H_ST,4)
#define GetIntIndentify(BaseAddr)   ((ReadReg(BaseAddr,H_ST)&0xf1) >>1)
#define IsHostBusy(BaseAddr)        CheckRegBit(BaseAddr,H_ST,0)
#define GetHostStatus(BaseAddr)     ReadReg(BaseAddr,H_ST)	//RO
//**************************M_CT
#define EnTXDMA(BaseAddr,val)         WriteRegBit(BaseAddr,M_CT,7,val)
#define EnRXDMA(BaseAddr,val)         WriteRegBit(BaseAddr,M_CT,6,val)
#define SwapDMA(BaseAddr,val)         WriteRegBit(BaseAddr,M_CT,5,val)
#define EnInternalLoop(BaseAddr,val)  WriteRegBit(BaseAddr,M_CT,4,val)
#define EnExternalLoop(BaseAddr,val)  WriteRegBit(BaseAddr,M_CT,3,val)
//**************************TX_CT_1
#define EnTXFIFOHalfLevelInt(BaseAddr,val)   WriteRegBit(BaseAddr,TX_CT_1,4,val)	//half empty int (1 half)
#define EnTXFIFOUnderrunEOMInt(BaseAddr,val) WriteRegBit(BaseAddr,TX_CT_1,5,val)
#define EnTXFIFOReadyInt(BaseAddr,val)       WriteRegBit(BaseAddr,TX_CT_1,6,val)	//int when reach it threshold (setting by bit 4)
//**************************TX_CT_2
#define ForceUnderrun(BaseAddr,val)   WriteRegBit(BaseAddr,TX_CT_2,7,val)	// force an underrun int
#define EnTXCRC(BaseAddr,val)         WriteRegBit(BaseAddr,TX_CT_2,6,val)	//1 for FIR,MIR...0 (not SIR)
#define ForceBADCRC(BaseAddr,val)     WriteRegBit(BaseAddr,TX_CT_2,5,val)	//force an bad CRC
#define SendSIP(BaseAddr,val)         WriteRegBit(BaseAddr,TX_CT_2,4,val)	//send indication pulse for prevent SIR disturb
#define ClearEnTX(BaseAddr,val)       WriteRegBit(BaseAddr,TX_CT_2,3,val)	// opposite to EnTX
//*****************TX_ST
#define GetTXStatus(BaseAddr) 	ReadReg(BaseAddr,TX_ST)	//RO
//**************************RX_CT
#define EnRXSpecInt(BaseAddr,val)           WriteRegBit(BaseAddr,RX_CT,0,val)
#define EnRXFIFOReadyInt(BaseAddr,val)      WriteRegBit(BaseAddr,RX_CT,1,val)	//enable int when reach it threshold (setting by bit 7)
#define EnRXFIFOHalfLevelInt(BaseAddr,val)  WriteRegBit(BaseAddr,RX_CT,7,val)	//enable int when (1) half full...or (0) just not full
//*****************RX_ST
#define GetRXStatus(BaseAddr) 	ReadReg(BaseAddr,RX_ST)	//RO
//***********************P_ADDR
#define SetPacketAddr(BaseAddr,addr)        WriteReg(BaseAddr,P_ADDR,addr)
//***********************I_CF_4
#define EnGPIOtoRX2(BaseAddr,val)	WriteRegBit(BaseAddr,I_CF_4,7,val)
#define EnTimerInt(BaseAddr,val)		WriteRegBit(BaseAddr,I_CF_4,1,val)
#define ClearTimerInt(BaseAddr,val)	WriteRegBit(BaseAddr,I_CF_4,0,val)
//***********************I_T_C_L
#define WriteGIO(BaseAddr,val)	    WriteRegBit(BaseAddr,I_T_C_L,7,val)
#define ReadGIO(BaseAddr)		    CheckRegBit(BaseAddr,I_T_C_L,7)
#define ReadRX(BaseAddr)		    CheckRegBit(BaseAddr,I_T_C_L,3)	//RO
#define WriteTX(BaseAddr,val)		WriteRegBit(BaseAddr,I_T_C_L,0,val)
//***********************I_T_C_H
#define EnRX2(BaseAddr,val)		    WriteRegBit(BaseAddr,I_T_C_H,7,val)
#define ReadRX2(BaseAddr)           CheckRegBit(BaseAddr,I_T_C_H,7)
//**********************Version
#define GetFIRVersion(BaseAddr)		ReadReg(BaseAddr,VERSION)


void SetTimer(__u16 iobase, __u8 count)
{
	EnTimerInt(iobase, OFF);
	WriteReg(iobase, TIMER, count);
	EnTimerInt(iobase, ON);
}


void SetSendByte(__u16 iobase, __u32 count)
{
	__u32 low, high;

	if ((count & 0xf000) == 0) {
		low = count & 0x00ff;
		high = (count & 0x0f00) >> 8;
		WriteReg(iobase, TX_C_L, low);
		WriteReg(iobase, TX_C_H, high);
	}
}

void ResetChip(__u16 iobase, __u8 type)
{
	__u8 value;

	value = (type + 2) << 4;
	WriteReg(iobase, RESET, type);
}

void SetAddrMode(__u16 iobase, __u8 mode)
{
	__u8 bTmp = 0;
	if (mode < 3) {
		bTmp = (ReadReg(iobase, RX_CT) & 0xcf) | (mode << 4);
		WriteReg(iobase, RX_CT, bTmp);
	}
}

int CkRxRecv(__u16 iobase, struct via_ircc_cb *self)
{
	__u8 low, high;
	__u16 wTmp = 0, wTmp1 = 0, wTmp_new = 0;

	low = ReadReg(iobase, RX_C_L);
	high = ReadReg(iobase, RX_C_H);
	wTmp1 = high;
	wTmp = (wTmp1 << 8) | low;
	udelay(10);
	low = ReadReg(iobase, RX_C_L);
	high = ReadReg(iobase, RX_C_H);
	wTmp1 = high;
	wTmp_new = (wTmp1 << 8) | low;
	if (wTmp_new != wTmp)
		return 1;
	else
		return 0;

}

__u16 RxCurCount(__u16 iobase, struct via_ircc_cb * self)
{
	__u8 low, high;
	__u16 wTmp = 0, wTmp1 = 0;

	low = ReadReg(iobase, RX_P_L);
	high = ReadReg(iobase, RX_P_H);
	wTmp1 = high;
	wTmp = (wTmp1 << 8) | low;
	return wTmp;
}

/* This Routine can only use in recevie_complete
 * for it will update last count.
 */

__u16 GetRecvByte(__u16 iobase, struct via_ircc_cb * self)
{
	__u8 low, high;
	__u16 wTmp, wTmp1, ret;

	low = ReadReg(iobase, RX_P_L);
	high = ReadReg(iobase, RX_P_H);
	wTmp1 = high;
	wTmp = (wTmp1 << 8) | low;


	if (wTmp >= self->RxLastCount)
		ret = wTmp - self->RxLastCount;
	else
		ret = (0x8000 - self->RxLastCount) + wTmp;
	self->RxLastCount = wTmp;

/* RX_P is more actually the RX_C
 low=ReadReg(iobase,RX_C_L);
 high=ReadReg(iobase,RX_C_H);

 if(!(high&0xe000)) {
	 temp=(high<<8)+low;
	 return temp;
 }
 else return 0;
*/
	return ret;
}


__u16 GetRecvLen(__u16 iobase)
{
	__u8 low, high;
	__u16 temp;

	low = ReadReg(iobase, RX_P_L);
	high = ReadReg(iobase, RX_P_H);

	if (!(high & 0xe000)) {
		temp = (high << 8) + low;
		return temp;
	} else
		return 0;
}

void Sdelay(__u16 scale)
{
	__u8 bTmp;
	int i, j;

	for (j = 0; j < scale; j++) {
		for (i = 0; i < 0x20; i++) {
			bTmp = inb(0xeb);
			outb(bTmp, 0xeb);
		}
	}
}

void Tdelay(__u16 scale)
{
	__u8 bTmp;
	int i, j;

	for (j = 0; j < scale; j++) {
		for (i = 0; i < 0x50; i++) {
			bTmp = inb(0xeb);
			outb(bTmp, 0xeb);
		}
	}
}


void ActClk(__u16 iobase, __u8 value)
{
	__u8 bTmp;
	bTmp = ReadReg(iobase, 0x34);
	if (value)
		WriteReg(iobase, 0x34, bTmp | Clk_bit);
	else
		WriteReg(iobase, 0x34, bTmp & ~Clk_bit);
}

void ActTx(__u16 iobase, __u8 value)
{
	__u8 bTmp;

	bTmp = ReadReg(iobase, 0x34);
	if (value)
		WriteReg(iobase, 0x34, bTmp | Tx_bit);
	else
		WriteReg(iobase, 0x34, bTmp & ~Tx_bit);
}

void ClkTx(__u16 iobase, __u8 Clk, __u8 Tx)
{
	__u8 bTmp;

	bTmp = ReadReg(iobase, 0x34);
	if (Clk == 0)
		bTmp &= ~Clk_bit;
	else {
		if (Clk == 1)
			bTmp |= Clk_bit;
	}
	WriteReg(iobase, 0x34, bTmp);
	Sdelay(1);
	if (Tx == 0)
		bTmp &= ~Tx_bit;
	else {
		if (Tx == 1)
			bTmp |= Tx_bit;
	}
	WriteReg(iobase, 0x34, bTmp);
}

void Wr_Byte(__u16 iobase, __u8 data)
{
	__u8 bData = data;
//      __u8 btmp;
	int i;

	ClkTx(iobase, 0, 1);

	Tdelay(2);
	ActClk(iobase, 1);
	Tdelay(1);

	for (i = 0; i < 8; i++) {	//LDN

		if ((bData >> i) & 0x01) {
			ClkTx(iobase, 0, 1);	//bit data = 1;
		} else {
			ClkTx(iobase, 0, 0);	//bit data = 1;
		}
		Tdelay(2);
		Sdelay(1);
		ActClk(iobase, 1);	//clk hi
		Tdelay(1);
	}
}

__u8 Rd_Indx(__u16 iobase, __u8 addr, __u8 index)
{
	__u8 data = 0, bTmp, data_bit;
	int i;

	bTmp = addr | (index << 1) | 0;
	ClkTx(iobase, 0, 0);
	Tdelay(2);
	ActClk(iobase, 1);
	udelay(1);
	Wr_Byte(iobase, bTmp);
	Sdelay(1);
	ClkTx(iobase, 0, 0);
	Tdelay(2);
	for (i = 0; i < 10; i++) {
		ActClk(iobase, 1);
		Tdelay(1);
		ActClk(iobase, 0);
		Tdelay(1);
		ClkTx(iobase, 0, 1);
		Tdelay(1);
		bTmp = ReadReg(iobase, 0x34);
		if (!(bTmp & Rd_Valid))
			break;
	}
	if (!(bTmp & Rd_Valid)) {
		for (i = 0; i < 8; i++) {
			ActClk(iobase, 1);
			Tdelay(1);
			ActClk(iobase, 0);
			bTmp = ReadReg(iobase, 0x34);
			data_bit = 1 << i;
			if (bTmp & RxBit)
				data |= data_bit;
			else
				data &= ~data_bit;
			Tdelay(2);
		}
	} else {
		for (i = 0; i < 2; i++) {
			ActClk(iobase, 1);
			Tdelay(1);
			ActClk(iobase, 0);
			Tdelay(2);
		}
		bTmp = ReadReg(iobase, 0x34);
	}
	for (i = 0; i < 1; i++) {
		ActClk(iobase, 1);
		Tdelay(1);
		ActClk(iobase, 0);
		Tdelay(2);
	}
	ClkTx(iobase, 0, 0);
	Tdelay(1);
	for (i = 0; i < 3; i++) {
		ActClk(iobase, 1);
		Tdelay(1);
		ActClk(iobase, 0);
		Tdelay(2);
	}
	return data;
}

void Wr_Indx(__u16 iobase, __u8 addr, __u8 index, __u8 data)
{
	int i;
	__u8 bTmp;

	ClkTx(iobase, 0, 0);
	udelay(2);
	ActClk(iobase, 1);
	udelay(1);
	bTmp = addr | (index << 1) | 1;
	Wr_Byte(iobase, bTmp);
	Wr_Byte(iobase, data);
	for (i = 0; i < 2; i++) {
		ClkTx(iobase, 0, 0);
		Tdelay(2);
		ActClk(iobase, 1);
		Tdelay(1);
	}
	ActClk(iobase, 0);
}

void ResetDongle(__u16 iobase)
{
	int i;
	ClkTx(iobase, 0, 0);
	Tdelay(1);
	for (i = 0; i < 30; i++) {
		ActClk(iobase, 1);
		Tdelay(1);
		ActClk(iobase, 0);
		Tdelay(1);
	}
	ActClk(iobase, 0);
}

void SetSITmode(__u16 iobase)
{

	__u8 bTmp;

	bTmp = ReadLPCReg(0x28);
	WriteLPCReg(0x28, bTmp | 0x10);	//select ITMOFF
	bTmp = ReadReg(iobase, 0x35);
	WriteReg(iobase, 0x35, bTmp | 0x40);	// Driver ITMOFF
	WriteReg(iobase, 0x28, bTmp | 0x80);	// enable All interrupt
}

void SI_SetMode(__u16 iobase, int mode)
{
	//__u32 dTmp;
	__u8 bTmp;

	WriteLPCReg(0x28, 0x70);	// S/W Reset
	SetSITmode(iobase);
	ResetDongle(iobase);
	udelay(10);
	Wr_Indx(iobase, 0x40, 0x0, 0x17);	//RX ,APEN enable,Normal power
	Wr_Indx(iobase, 0x40, 0x1, mode);	//Set Mode
	Wr_Indx(iobase, 0x40, 0x2, 0xff);	//Set power to FIR VFIR > 1m
	bTmp = Rd_Indx(iobase, 0x40, 1);
}

void InitCard(__u16 iobase)
{
	ResetChip(iobase, 5);
	WriteReg(iobase, I_ST_CT_0, 0x00);	// open CHIP on
	SetSIRBOF(iobase, 0xc0);	// hardware default value
	SetSIREOF(iobase, 0xc1);
}

void CommonShutDown(__u16 iobase, __u8 TxDMA)
{
	DisableDmaChannel(TxDMA);
}

void CommonInit(__u16 iobase)
{
//  EnTXCRC(iobase,0);
	SwapDMA(iobase, OFF);
	SetMaxRxPacketSize(iobase, 0x0fff);	//set to max:4095
	EnRXFIFOReadyInt(iobase, OFF);
	EnRXFIFOHalfLevelInt(iobase, OFF);
	EnTXFIFOHalfLevelInt(iobase, OFF);
	EnTXFIFOUnderrunEOMInt(iobase, ON);
//  EnTXFIFOReadyInt(iobase,ON);
	InvertTX(iobase, OFF);
	InvertRX(iobase, OFF);
//  WriteLPCReg(0xF0,0); //(if VT1211 then do this)
	if (IsSIROn(iobase)) {
		SIRFilter(iobase, ON);
		SIRRecvAny(iobase, ON);
	} else {
		SIRFilter(iobase, OFF);
		SIRRecvAny(iobase, OFF);
	}
	EnRXSpecInt(iobase, ON);
	WriteReg(iobase, I_ST_CT_0, 0x80);
	EnableDMA(iobase, ON);
}

void SetBaudRate(__u16 iobase, __u32 rate)
{
	__u8 value = 11, temp;

	if (IsSIROn(iobase)) {
		switch (rate) {
		case (__u32) (2400L):
			value = 47;
			break;
		case (__u32) (9600L):
			value = 11;
			break;
		case (__u32) (19200L):
			value = 5;
			break;
		case (__u32) (38400L):
			value = 2;
			break;
		case (__u32) (57600L):
			value = 1;
			break;
		case (__u32) (115200L):
			value = 0;
			break;
		default:
			break;
		};
	} else if (IsMIROn(iobase)) {
		value = 0;	// will automatically be fixed in 1.152M
	} else if (IsFIROn(iobase)) {
		value = 0;	// will automatically be fixed in 4M
	}
	temp = (ReadReg(iobase, I_CF_H_1) & 0x03);
	temp = temp | (value << 2);
	WriteReg(iobase, I_CF_H_1, temp);
}

void SetPulseWidth(__u16 iobase, __u8 width)
{
	__u8 temp, temp1, temp2;

	temp = (ReadReg(iobase, I_CF_L_1) & 0x1f);
	temp1 = (ReadReg(iobase, I_CF_H_1) & 0xfc);
	temp2 = (width & 0x07) << 5;
	temp = temp | temp2;
	temp2 = (width & 0x18) >> 3;
	temp1 = temp1 | temp2;
	WriteReg(iobase, I_CF_L_1, temp);
	WriteReg(iobase, I_CF_H_1, temp1);
}

void SetSendPreambleCount(__u16 iobase, __u8 count)
{
	__u8 temp;

	temp = ReadReg(iobase, I_CF_L_1) & 0xe0;
	temp = temp | count;
	WriteReg(iobase, I_CF_L_1, temp);

}

void SetVFIR(__u16 BaseAddr, __u8 val)
{
	__u8 tmp;

	tmp = ReadReg(BaseAddr, I_CF_L_0);
	WriteReg(BaseAddr, I_CF_L_0, tmp & 0x8f);
	WriteRegBit(BaseAddr, I_CF_H_0, 5, val);
}

void SetFIR(__u16 BaseAddr, __u8 val)
{
	__u8 tmp;

	WriteRegBit(BaseAddr, I_CF_H_0, 5, 0);
	tmp = ReadReg(BaseAddr, I_CF_L_0);
	WriteReg(BaseAddr, I_CF_L_0, tmp & 0x8f);
	WriteRegBit(BaseAddr, I_CF_L_0, 6, val);
}

void SetMIR(__u16 BaseAddr, __u8 val)
{
	__u8 tmp;

	WriteRegBit(BaseAddr, I_CF_H_0, 5, 0);
	tmp = ReadReg(BaseAddr, I_CF_L_0);
	WriteReg(BaseAddr, I_CF_L_0, tmp & 0x8f);
	WriteRegBit(BaseAddr, I_CF_L_0, 5, val);
}

void SetSIR(__u16 BaseAddr, __u8 val)
{
	__u8 tmp;

	WriteRegBit(BaseAddr, I_CF_H_0, 5, 0);
	tmp = ReadReg(BaseAddr, I_CF_L_0);
	WriteReg(BaseAddr, I_CF_L_0, tmp & 0x8f);
	WriteRegBit(BaseAddr, I_CF_L_0, 4, val);
}

void ClrHBusy(__u16 iobase)
{

	EnableDMA(iobase, OFF);
	EnableDMA(iobase, ON);
	RXStart(iobase, OFF);
	RXStart(iobase, ON);
	RXStart(iobase, OFF);
	EnableDMA(iobase, OFF);
	EnableDMA(iobase, ON);
}

void SetFifo64(__u16 iobase)
{

	WriteRegBit(iobase, I_CF_H_0, 0, 0);
	WriteRegBit(iobase, I_CF_H_0, 7, 0);
}


#endif				/* via_IRCC_H */
