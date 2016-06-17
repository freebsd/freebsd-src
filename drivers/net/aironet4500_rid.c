/*
 *	 Aironet 4500 Pcmcia driver
 *
 *		Elmer Joandi, Januar 1999
 *	Copyright Elmer Joandi, all rights restricted
 *	
 *
 *	Revision 0.1 ,started  30.12.1998
 *
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>

#include "aironet4500.h"



#define awc_RID_gen_RidLen 				{(const struct aironet4500_rid_selector *)&aironet4500_RID_Select_General_Config,0x0000, 8,1,1,1,0, 0xffffffff,0x0000, "Length of RID" }
#define awc_RID_gen_OperatingMode_adhoc 		{&aironet4500_RID_Select_General_Config,0x0002,16,1,1,0,0, 0x00000003,0x0000,"Opmode IBSS Adhoc operation" } // Without AP
#define awc_RID_gen_OperatingMode_Infrastructure 	{&aironet4500_RID_Select_General_Config,0x0002,16,1,1,0,0, 0x00000003,0x0001,"Opmode Infrastructure Station operation" }// With AP
#define awc_RID_gen_OperatingMode_AP			{&aironet4500_RID_Select_General_Config,0x0002,16,1,1,0,0, 0x00000003,0x0002,"Opmode Access Point" } // Aironet doesn't release info on use 
#define awc_RID_gen_OperatingMode_AP_and_repeater 	{&aironet4500_RID_Select_General_Config,0x0002,16,1,1,0,0, 0x00000003,0x0003,"Opmode Access Point and Repeater" } // no info
#define awc_RID_gen_OperatingMode_No_payload_modify	{&aironet4500_RID_Select_General_Config,0x0002,16,1,1,0,0, 0x00000100,0x0100,"Opmode Payload without modify" } 
#define awc_RID_gen_OperatingMode_LLC_802_3_convert	{&aironet4500_RID_Select_General_Config,0x0002,16,1,1,0,0, 0x00000100,0x0000,"Opmode LLC -> 802.3 convert" }
#define awc_RID_gen_OperatingMode_proprietary_ext 	{&aironet4500_RID_Select_General_Config,0x0002,16,1,1,0,0, 0x00000200,0x0200,"Opmode Aironet Extentsions enabled" } // neened for 11Mbps
#define awc_RID_gen_OperatingMode_no_proprietary_ext 	{&aironet4500_RID_Select_General_Config,0x0002,16,1,1,0,0,0x00000200,0x0000,"Opmode Aironet Extentsions disabled" }
#define awc_RID_gen_OperatingMode_AP_ext 		{&aironet4500_RID_Select_General_Config,0x0002,16,1,1,0,0, 0x00000400,0x0400,"Opmode AP Extentsions enabled" }	// no info
#define awc_RID_gen_OperatingMode_no_AP_ext	 	{&aironet4500_RID_Select_General_Config,0x0002,16,1,1,0,0, 0x00000400,0x0000,"Opmode AP Extentsions disabled" }
#define awc_RID_gen_ReceiveMode 			{&aironet4500_RID_Select_General_Config,0x0004,16,1,1,0,0,0x0000ffff,0x0000,"RX Mode"}
#define awc_RID_gen_ReceiveMode_BMA 			{&aironet4500_RID_Select_General_Config,0x0004,16,1,1,0,0,0x0000000f,0x0000,"RX Mode BC MC ADDR"}
#define awc_RID_gen_ReceiveMode_BA 			{&aironet4500_RID_Select_General_Config,0x0004,16,1,1,0,0,0x0000000f,0x0001,"RX Mode BC ADDR"}
#define awc_RID_gen_ReceiveMode_A 			{&aironet4500_RID_Select_General_Config,0x0004,16,1,1,0,0,0x0000000f,0x0002,"RX Mode ADDR"}
#define awc_RID_gen_ReceiveMode_802_11_monitor		{&aironet4500_RID_Select_General_Config,0x0004,16,1,1,0,0,0x0000000f,0x0003,"RX Mode 802.11 Monitor current BSSID"}
#define awc_RID_gen_ReceiveMode_802_11_any_monitor 	{&aironet4500_RID_Select_General_Config,0x0004,16,1,1,0,0,0x0000000f,0x0004,"RX Mode 802.11 Monitor any BSSID"}
#define awc_RID_gen_ReceiveMode_LAN_monitor 		{&aironet4500_RID_Select_General_Config,0x0004,16,1,1,0,0,0x0000000f,0x0005,"RX Mode LAN Monitor current BSSID"}
#define awc_RID_gen_ReceiveMode_802_3_hdr_disable 	{&aironet4500_RID_Select_General_Config,0x0004,16,1,1,0,0,0x00000100,0x0100,"RX Mode Disable RX 802.3 Header"}
#define awc_RID_gen_ReceiveMode_802_3_hdr_enable 	{&aironet4500_RID_Select_General_Config,0x0004,16,1,1,0,0,0x00000100,0x0000,"RX Mode Enable RX 802.3 header"}
#define awc_RID_gen_Fragmentation_threshold		{&aironet4500_RID_Select_General_Config,0x0006,16,1,1,0,0,0x0000ffff,0x0000,"Fragmentation Threshold"}		// treshold of packet size starting to be fragmented
#define awc_RID_gen_RTS_threshold 			{&aironet4500_RID_Select_General_Config,0x0008,16,1,1,0,0,0xffff,0x0000,"RTS Threshold"}	// packet size, larger ones get sent with RTS/CTS
#define awc_RID_gen_Station_Mac_Id 			{&aironet4500_RID_Select_General_Config,0x000A, 8,6,1,0,0,0xff,0,"Station MAC Id"}
#define awc_RID_gen_Supported_rates 			{&aironet4500_RID_Select_General_Config,0x0010, 8,8,1,0,1,0xff,0x00,"Supported Rates"}	// Hex encoded 500kbps 
#define awc_RID_gen_Basic_Rate 				{&aironet4500_RID_Select_General_Config,0x0010, 8,1,1,0,1,0x80,0x80,"Basic Rate"}	// if 0x80 bit is set
#define awc_RID_gen_Rate_500kbps 			{&aironet4500_RID_Select_General_Config,0x0010, 8,1,1,0,1,0x7f,0x01,"Rate 500kbps"}
#define awc_RID_gen_Rate_1Mbps 				{&aironet4500_RID_Select_General_Config,0x0010, 8,1,1,0,1,0x7f,0x02,"Rate 1Mbps"}
#define awc_RID_gen_Rate_2Mbps 				{&aironet4500_RID_Select_General_Config,0x0010, 8,1,1,0,1,0x7f,0x04,"Rate 2Mbps"}
#define awc_RID_gen_Rate_4Mbps 				{&aironet4500_RID_Select_General_Config,0x0010, 8,1,1,0,1,0x7f,0x08,"Rate 4Mbps"}
#define awc_RID_gen_Rate_5Mbps 				{&aironet4500_RID_Select_General_Config,0x0010, 8,1,1,0,1,0x7f,0x0B,"Rate 5.5Mbps"}
#define awc_RID_gen_Rate_10Mbps 			{&aironet4500_RID_Select_General_Config,0x0010, 8,1,1,0,1,0x7f,0x14,"Rate 10Mbps"}
#define awc_RID_gen_Rate_11Mbps 			{&aironet4500_RID_Select_General_Config,0x0010, 8,1,1,0,1,0x7f,0x16,"Rate 11Mbps"}
#define awc_RID_gen_BasicRate_500kbps 			{&aironet4500_RID_Select_General_Config,0x0010, 8,1,1,0,1,0xff,0x81,"BasicRate 500kbps"}
#define awc_RID_gen_BasicRate_1Mbps 				{&aironet4500_RID_Select_General_Config,0x0010, 8,1,1,0,1,0xff,0x82,"BasicRate 1Mbps"}
#define awc_RID_gen_BasicRate_2Mbps 				{&aironet4500_RID_Select_General_Config,0x0010, 8,1,1,0,1,0xff,0x84,"BasicRate 2Mbps"}
#define awc_RID_gen_BasicRate_4Mbps 				{&aironet4500_RID_Select_General_Config,0x0010, 8,1,1,0,1,0xff,0x88,"BasicRate 4Mbps"}
#define awc_RID_gen_BasicRate_5Mbps 				{&aironet4500_RID_Select_General_Config,0x0010, 8,1,1,0,1,0xff,0x8B,"BasicRate 5.5Mbps"}
#define awc_RID_gen_BasicRate_10Mbps 			{&aironet4500_RID_Select_General_Config,0x0010, 8,1,1,0,1,0xff,0x94,"BasicRate 10Mbps"}
#define awc_RID_gen_BasicRate_11Mbps 			{&aironet4500_RID_Select_General_Config,0x0010, 8,1,1,0,1,0xff,0x96,"BasicRate 11Mbps"}


#define awc_RID_gen_Long_retry_limit 			{&aironet4500_RID_Select_General_Config,0x0018,16, 1,1,0,0,0xffff,0,"Short Retry Limit"}
#define awc_RID_gen_Short_retry_limit 			{&aironet4500_RID_Select_General_Config,0x001A,16, 1,1,0,0,0xffff,0,"Long Retry Limit"}
#define awc_RID_gen_Tx_MSDU_lifetime 			{&aironet4500_RID_Select_General_Config,0x001C,16, 1,1000,0,0,0xffff,0,"TX MSDU Lifetime"}
#define awc_RID_gen_Rx_MSDU_lifetime 			{&aironet4500_RID_Select_General_Config,0x001E,16, 1,1000,0,0,0xffff,0,"RX MSDU Lifetime"}
#define awc_RID_gen_Stationary 				{&aironet4500_RID_Select_General_Config,0x0020,16, 1,1,0,0,0xffff,0,"Stationary"}
#define awc_RID_gen_BC_MC_Ordering 			{&aironet4500_RID_Select_General_Config,0x0022,16, 1,1,0,0,0xffff,0,"Strictly order Bcast and Mcast"}
#define awc_RID_gen_Device_type 			{&aironet4500_RID_Select_General_Config,0x0024,16, 1,1,1,0,0xffff,0x00,"Radio Type"}
#define awc_RID_gen_Reserved_0x0026 			{&aironet4500_RID_Select_General_Config,0x0026, 8,10,1,0,0,0xff,0,"Reserved0x28"}


//SCANNING/ASSOCIATING
#define awc_RID_gen_ScanMode				awc_def_gen_RID(0x0030,"ScanMode",		16,0xf,0, NULL)
#define awc_RID_gen_ScanMode_Active 			awc_def_gen_RID(0x0030,"ScanMode Active",		16,0xf,0, "Active")
#define awc_RID_gen_ScanMode_Passive 			awc_def_gen_RID(0x0030,"ScanMode Passive",		16,0xf,1, "Passive")
#define awc_RID_gen_ScanMode_Aironet_ext		awc_def_gen_RID(0x0030,"ScanMode Aironet Ext",		16,0xf,2, "Aironet Ext")
#define awc_RID_gen_ProbeDelay 				awc_def_gen_RID(0x0032,"ProbeDelay",		16,0xffff,0," msek") 		//                 Time ms to wait after switching to a channel for clear channel assessment.
#define awc_RID_gen_ProbeEnergyTimeout 			awc_def_gen_RID(0x0034,"ProbeEnergyTimeout",	16,0xffff,0,"msek") 	//          Time to wait for energy after an active probe.
#define awc_RID_gen_ProbeResponseTimeout		awc_def_gen_RID(0x0036,"ProbeResponseTimeout",	16,0xffff,0,"msek") 	// Time to wait for a probe response after energy detected.
#define awc_RID_gen_BeaconListenTimeout 		awc_def_gen_RID(0x0038,"BeaconListenTimeout",	16,0xffff,0,"msek")	//    0 default    40          Time to listen for a beacon on each channel.
#define awc_RID_gen_IbssJoinNetTimeout 			awc_def_gen_RID(0x003A,"IbssJoinNetTimeout",	16,0xffff,0,"msek")	//       0 default    10000       IBSS: Time to scan for an IBSS before forming a
#define awc_RID_gen_AuthenticationTimeout 		awc_def_gen_RID(0x003C,"AuthenticationTimeout",16,0xffff,0,"msek")	//       0 default    2000        Time limit after which an authentication sequence will
#define awc_RID_gen_AuthenticationType 			awc_def_gen_RID(0x003E,"AuthenticationType",	16,0xffff,0,NULL)	//       0 default    1 (open) //    Selects the desired authentication and privacy methods.		 
#define awc_RID_gen_AuthenticationType_None 		awc_def_gen_RID(0x003E,"AuthenticationType None",	16,0xffff,0,"None") 	//   0x00 = None	
#define awc_RID_gen_AuthenticationType_Open		awc_def_gen_RID(0x003E,"AuthenticationType Open",	16,0xffff,1,"Open") 	//             0x01 = Open
#define awc_RID_gen_AuthenticationType_Shared		awc_def_gen_RID(0x003E,"AuthenticationType Shared-Key",	16,0xffff,2,"Shared-Key")  	//     0x02 = Shared-Key
#define awc_RID_gen_AuthenticationType_Exclude_Open 	awc_def_gen_RID(0x003E,"AuthenticationType Exclude Open",	16,0xffff,4,"Exclude Open")   	//              0x04 = Exclude Unencrypted
#define awc_RID_gen_AssociationTimeout 			awc_def_gen_RID(0x0040,"AssociationTimeout",	16,0xffff,0,"msek")	//       0 default    2000        ESS: Time limit after which an association sequence
#define awc_RID_gen_SpecifiedAPtimeout 			awc_def_gen_RID(0x0042,"SpecifiedAPtimeout",	16,0xffff,0,"msek")	//       0 default    10000       0 selects the factory default [~10 sec].
#define awc_RID_gen_OfflineScanInterval 		awc_def_gen_RID(0x0044,"OfflineScanInterval",	16,0xffff,0,"msek")	//       0            0           0 disables offline scanning.(kus)        The time period between offline scans.
#define awc_RID_gen_OfflineScanDuration 		awc_def_gen_RID(0x0046,"OfflineScanDuration",	16,0xffff,0,"msek")	//       0            0           0 disables offline scanning. //    (kus)        The duration of an offline scan.
#define awc_RID_gen_LinkLossDelay 			awc_def_gen_RID(0x0048,"LinkLossDelay",	16,0xffff,0,"msek")	//       0  0 Time to delay before reporting a loss of association
#define awc_RID_gen_MaxBeaconLostTime 			awc_def_gen_RID(0x004A,"MaxBeaconLostTime",	16,0xffff,0,"msek")	//      0 default    500        If no beacons are received for this time period, the unit
#define awc_RID_gen_RefreshInterval 			awc_def_gen_RID(0x004C,"RefreshInterval",	16,0xffff,0,"msek")		//      0 default    10000      At the specified interval, the station will send a refresh
//POWER SAVE OPERATION
#define awc_RID_gen_PowerSaveMode 			awc_def_gen_RID(0x0050,"PowerSaveMode",	16,0xffff,0,NULL) 		//      0  0Note, for IBSS there is only one PSP mode and it is only enabled if the ATIMwindow is non-zero.
#define awc_RID_gen_PowerSaveMode_CAM 		awc_def_gen_RID(0x0050,"PowerSaveMode CAM",	16,0x000f,0,"CAM") 	// 0 = CAM
#define awc_RID_gen_PowerSaveMode_PSP 		awc_def_gen_RID(0x0050,"PowerSaveMode PSP",	16,0x000f,1,"PSP") 	// 1 = PSP
#define awc_RID_gen_PowerSaveMode_Fast_PSP		awc_def_gen_RID(0x0050,"PowerSaveMode Fast PSP",	16,0x000f,2,"Fast PSP")	//2 = PSP-CAM [FASTPSP]
#define awc_RID_gen_SleepForDTIMs 			awc_def_gen_RID(0x0052,"SleepForDTIMs",	16,0xffff,0,"DTIMs")	//      0  0If non-zero, the station may sleep through DTIMs; this
#define awc_RID_gen_ListenInterval 			awc_def_gen_RID(0x0054,"ListenInterval",	16,0xffff,0,"msek")		//      0 default    200 kus    Maximum time to awaken for TIMs. 0 selects factory
#define awc_RID_gen_FastListenInterval 		awc_def_gen_RID(0x0056,"FastListenInterval",	16,0xffff,0,"msek")     // 0 default    100 kus    The listen interval to be used immediately after
#define awc_RID_gen_ListenDecay 			awc_def_gen_RID(0x0058,"ListenDecay",		16,0xffff,0,"times")	//      0 default    2Number of times to use the current listen interval
#define awc_RID_gen_FastListenDelay 		awc_def_gen_RID(0x005A,"FastListenDelay",	16,0xffff,0,"msek")	//      0 default    200 kus    Time interval to delay before going to fast listen
#define awc_RID_gen_Reserved0x005C 			awc_def_gen_RID(0x005C,"Reserved0x005C",	32,0xffffffff,0,"")	//
//ADHOC (or AP) OPERATION
#define awc_RID_gen_BeaconPeriod 			awc_def_gen_RID(0x0060,"BeaconPeriod",		16,0xffff,0,"msek")	//      0 default    100        0 selects the factory default of [~100 ms].  (kus)
#define awc_RID_gen_AtimDuration 			awc_def_gen_RID(0x0062,"AtimDuration",		16,0xffff,0,"msek")	//      0 default    5 kus      The time period reserved for ATIMs immediately after (kus)      the beacon. 0xFFFF will disable the ATIM window; power save mode will not operate.This parameter only applies to adhoc/IBSS.
#define awc_RID_gen_Reserved0x0064 			awc_def_gen_RID(0x0064,"Reserved64",		16,0xffff,0,"")	//      0  0Reserved for future use
#define awc_RID_gen_DSChannel 			awc_def_gen_RID(0x0066,"DSChannel",		16,0xffff,0,"")	//      0 default    1The desired operating channel.  ()refer to 802.11)       For North America, a Channel of 0 is 2412 MHz.
#define awc_RID_gen_Reserved0x0068 			awc_def_gen_RID(0x0068,"Reserved68",		16,0xffff,0,"")	//      0  0Reserved for future use
#define awc_RID_gen_DTIM_Period 			awc_def_gen_RID(0x006A,"DTIM Period",		16,0xffff,0,"")	//      0 default    1Selects how often a beacon is a DTIM for APs
#define awc_RID_gen_Reserved0x0006C 		awc_def_gen_RID(0x006C,"Reserved6C",		32,0xffffffff,0,"")	//    0's0's        Reserved for future use
//RADIO OPERATION
#define awc_RID_gen_RadioSpreadType 		awc_def_gen_RID(0x0070,"RadioSpreadType",	16,0xffff,0,NULL)	//      0 default    0Selects the radio operational mode. By default, this will
#define awc_RID_gen_RadioSpreadType_FH 		awc_def_gen_RID(0x0070,"RadioSpreadType FH",	16,0xffff,0,"FH")	//0 = 802.11 FH Radio (Default)
#define awc_RID_gen_RadioSpreadType_DS 		awc_def_gen_RID(0x0070,"RadioSpreadType DS",	16,0xffff,1,"DS")	//1 = 802.11 DS Radio
#define awc_RID_gen_RadioSpreadType_LM 		awc_def_gen_RID(0x0070,"RadioSpreadType LM2000",	16,0xffff,2,"LM2000")	//2 = LM2000 (Legacy) DS Radio
#define awc_RID_gen_TX_antenna_Diversity 		awc_def_gen_RID(0x0072,"TX antenna Diversity",	16,0xff00,0,NULL)	//       0 default    0x0303    This field is bit-mapped to select the operational
#define awc_RID_gen_TX_antenna_Diversity_default	awc_def_gen_RID(0x0072,"TX antenna Diversity Default",	16,0xff00,0x0000,"Default")	//  0 = Diversity as programmed at the factory
#define awc_RID_gen_TX_antenna_Diversity_1 		awc_def_gen_RID(0x0072,"TX antenna Diversity Antenna 1",	16,0xff00,0x0100,"Antenna 1")	//  1 = Antenna 1 only
#define awc_RID_gen_TX_antenna_Diversity_2 		awc_def_gen_RID(0x0072,"TX antenna Diversity Antenna 2",	16,0xff00,0x0200,"Antenna 2")	//  2 = Antenna 2 only
#define awc_RID_gen_TX_antenna_Diversity_both 	awc_def_gen_RID(0x0072,"TX antenna Diversity both antennas",	16,0xff00,0x0300,"both antennas")	//  3 = Antennas 1 and 2 are active
#define awc_RID_gen_RX_antenna_Diversity		awc_def_gen_RID(0x0072,"RX antenna Diversity",	16,0x00ff,0,NULL)	//       0 default    0x0303    This field is bit-mapped to select the operational
#define awc_RID_gen_RX_antenna_Diversity_default	awc_def_gen_RID(0x0072,"RX antenna Diversity Default",	16,0x00ff,0,"Default")	//  0 = Diversity as programmed at the factory
#define awc_RID_gen_RX_antenna_Diversity_1		awc_def_gen_RID(0x0072,"RX antenna Diversity Antenna 1",	16,0x00ff,1,"Antenna 1")	//  1 = Antenna 1 only
#define awc_RID_gen_RX_antenna_Diversity_2 		awc_def_gen_RID(0x0072,"RX antenna Diversity Antenna 2",	16,0x00ff,2,"Antenna 2")	//  2 = Antenna 2 only
#define awc_RID_gen_RX_antenna_Diversity_both	awc_def_gen_RID(0x0072,"RX antenna Diversity both antennas",	16,0x00ff,3,"both antennas")	//
#define awc_RID_gen_TransmitPower 			awc_def_gen_RID(0x0074,"TransmitPower",	16,0xffff,0,"mW (rounded up, btw)")	//       0 default    250 or    0 selects the default (maximum power allowed for the
#define awc_RID_gen_RSSIthreshold 			awc_def_gen_RID(0x0076,"RSSIthreshold",	16,0xffff,0,"units")	//       0 default    0         RSSI threshold. 0 selects factory default.
#define awc_RID_gen_Modulation 				awc_def_gen_RID(0x0078,"Modulation",	8,0xff,0,"")	//     modulation type
#define awc_RID_gen_Reserved0x0079 			awc_def_gen_RID(0x0079,"Reserved0x0079",	56,0xff,0,"")	//     0's0's       reserved for future radio specific parameters


//AIRONET EXTENSIONS
#define awc_RID_gen_NodeName 			awc_def_gen_RID(0x0080,"NodeName",		128,0,0,"")	//    0  0         Station name.
#define awc_RID_gen_ARLThreshold 			awc_def_gen_RID(0x0090,"ARLThreshold",		16,0xffff,0,"times")	//       0 default    0xFFFF    0 selects the factory defaults. (which for now is
#define awc_RID_gen_ARLDecay 			awc_def_gen_RID(0x0092,"ARLDecay",		16,0xffff,0,"times")	//       0 default    0xFFFF    0 selects the factory defaults. (which for now is
#define awc_RID_gen_ARLDelay 			awc_def_gen_RID(0x0094,"ARLDelay",		16,0xffff,0,"times")	//       0 default    0xFFFF    0 selects the factory defaults. (which for now is
#define awc_RID_gen_Unused0x0096 			awc_def_gen_RID(0x0096,"Reserved0x96",		16,0xffff,0,"")	//
#define awc_RID_gen_MagicPacketAction 		awc_def_gen_RID(0x0098,"MagicPacketAction",	8,0xff,0," hell knows what")	//        0  0         0 selects no action to be taken on a magic packet and"
#define awc_RID_gen_MagicPacketControl 		awc_def_gen_RID(0x0099,"MagicPacketControl",	8,0xff,0," hell know what")	//        0  0         0 will disable the magic packet mode command"


#define awc_RID_act_RidLen 				{&aironet4500_RID_Select_Active_Config,0x0000, 8,1,1,1,0, 0xffffffff,0x0000, "Length of RID" }
#define awc_RID_act_OperatingMode_adhoc 		{&aironet4500_RID_Select_Active_Config,0x0002,16,1,1,0,0, 0x00000003,0x0000,"Opmode IBSS Adhoc operation" }
#define awc_RID_act_OperatingMode_Infrastructure 	{&aironet4500_RID_Select_Active_Config,0x0002,16,1,1,0,0, 0x00000003,0x0001,"Opmode Infrastructure Station operation" }
#define awc_RID_act_OperatingMode_AP		{&aironet4500_RID_Select_Active_Config,0x0002,16,1,1,0,0, 0x00000003,0x0002,"Opmode Access Point" }
#define awc_RID_act_OperatingMode_AP_and_repeater 	{&aironet4500_RID_Select_Active_Config,0x0002,16,1,1,0,0, 0x00000003,0x0003,"Opmode Access Point and Repeater" }
#define awc_RID_act_OperatingMode_No_payload_modify	{&aironet4500_RID_Select_Active_Config,0x0002,16,1,1,0,0, 0x00000100,0x0100,"Opmode Payload without modify" }
#define awc_RID_act_OperatingMode_LLC_802_3_convert	{&aironet4500_RID_Select_Active_Config,0x0002,16,1,1,0,0, 0x00000100,0x0000,"Opmode LLC -> 802.3 convert" }
#define awc_RID_act_OperatingMode_proprietary_ext 	{&aironet4500_RID_Select_Active_Config,0x0002,16,1,1,0,0, 0x00000200,0x0200,"Opmode Aironet Extentsions enabled" }
#define awc_RID_act_OperatingMode_no_proprietary_ext {&aironet4500_RID_Select_Active_Config,0x0002,16,1,1,0,0,0x00000200,0x0000,"Opmode Aironet Extentsions disabled" }
#define awc_RID_act_OperatingMode_AP_ext 		{&aironet4500_RID_Select_Active_Config,0x0002,16,1,1,0,0, 0x00000400,0x0400,"Opmode AP Extentsions enabled" }
#define awc_RID_act_OperatingMode_no_AP_ext	 	{&aironet4500_RID_Select_Active_Config,0x0002,16,1,1,0,0, 0x00000400,0x0000,"Opmode AP Extentsions disabled" }
#define awc_RID_act_ReceiveMode 			{&aironet4500_RID_Select_Active_Config,0x0004,16,1,1,0,0,0xffffffff,0x0000,"RX Mode"}
#define awc_RID_act_ReceiveMode_BMA 		{&aironet4500_RID_Select_Active_Config,0x0004,16,1,1,0,0,0x0000000f,0x0000,"RX Mode BC MC ADDR"}
#define awc_RID_act_ReceiveMode_BA 			{&aironet4500_RID_Select_Active_Config,0x0004,16,1,1,0,0,0x0000000f,0x0001,"RX Mode BC ADDR"}
#define awc_RID_act_ReceiveMode_A 			{&aironet4500_RID_Select_Active_Config,0x0004,16,1,1,0,0,0x0000000f,0x0002,"RX Mode ADDR"}
#define awc_RID_act_ReceiveMode_802_11_monitor	{&aironet4500_RID_Select_Active_Config,0x0004,16,1,1,0,0,0x0000000f,0x0003,"RX Mode 802.11 Monitor current BSSID"}
#define awc_RID_act_ReceiveMode_802_11_any_monitor 	{&aironet4500_RID_Select_Active_Config,0x0004,16,1,1,0,0,0x0000000f,0x0004,"RX Mode 802.11 Monitor any BSSID"}
#define awc_RID_act_ReceiveMode_LAN_monitor 	{&aironet4500_RID_Select_Active_Config,0x0004,16,1,1,0,0,0x0000000f,0x0005,"RX Mode LAN Monitor current BSSID"}
#define awc_RID_act_ReceiveMode_802_3_hdr_disable 	{&aironet4500_RID_Select_Active_Config,0x0004,16,1,1,0,0,0x00000100,0x0100,"RX Mode Disable RX 802.3 Header"}
#define awc_RID_act_ReceiveMode_802_3_hdr_enable 	{&aironet4500_RID_Select_Active_Config,0x0004,16,1,1,0,0,0x00000100,0x0000,"RX Mode Enable RX 802.3 header"}
#define awc_RID_act_Fragmentation_threshold		{&aironet4500_RID_Select_Active_Config,0x0006,16,1,1,0,0,0x0000ffff,0x0000,"Fragmentation Threshold"}
#define awc_RID_act_RTS_threshold 			{&aironet4500_RID_Select_Active_Config,0x0008,16,1,1,0,0,0xffff,0x0000,"RTS Threshold"}
#define awc_RID_act_Station_Mac_Id 			{&aironet4500_RID_Select_Active_Config,0x000A, 8,6,1,0,0,0xff,0,"Station MAC Id"}
#define awc_RID_act_Supported_rates 			{&aironet4500_RID_Select_Active_Config,0x0010, 8,8,1,0,1,0xff,0x00,"Supported Rates"}
#define awc_RID_act_Basic_Rate 				{&aironet4500_RID_Select_Active_Config,0x0010, 8,1,1,0,1,0x80,0x80,"Basic Rate"}
#define awc_RID_act_Rate_500kbps 			{&aironet4500_RID_Select_Active_Config,0x0010, 8,1,1,0,1,0x7f,0x01,"Rate 500kbps"}
#define awc_RID_act_Rate_1Mbps 				{&aironet4500_RID_Select_Active_Config,0x0010, 8,1,1,0,1,0x7f,0x02,"Rate 1Mbps"}
#define awc_RID_act_Rate_2Mbps 				{&aironet4500_RID_Select_Active_Config,0x0010, 8,1,1,0,1,0x7f,0x04,"Rate 2Mbps"}
#define awc_RID_act_Rate_4Mbps 				{&aironet4500_RID_Select_Active_Config,0x0010, 8,1,1,0,1,0x7f,0x08,"Rate 4Mbps"}
#define awc_RID_act_Rate_5Mbps 				{&aironet4500_RID_Select_Active_Config,0x0010, 8,1,1,0,1,0x7f,0x0B,"Rate 5.5Mbps"}
#define awc_RID_act_Rate_10Mbps 			{&aironet4500_RID_Select_Active_Config,0x0010, 8,1,1,0,1,0x7f,0x14,"Rate 10Mbps"}
#define awc_RID_act_Rate_11Mbps 			{&aironet4500_RID_Select_Active_Config,0x0010, 8,1,1,0,1,0x7f,0x16,"Rate 11Mbps"}
#define awc_RID_act_BasicRate_500kbps 			{&aironet4500_RID_Select_Active_Config,0x0010, 8,1,1,0,1,0xff,0x81,"BasicRate 500kbps"}
#define awc_RID_act_BasicRate_1Mbps 				{&aironet4500_RID_Select_Active_Config,0x0010, 8,1,1,0,1,0xff,0x82,"BasicRate 1Mbps"}
#define awc_RID_act_BasicRate_2Mbps 				{&aironet4500_RID_Select_Active_Config,0x0010, 8,1,1,0,1,0xff,0x84,"BasicRate 2Mbps"}
#define awc_RID_act_BasicRate_4Mbps 				{&aironet4500_RID_Select_Active_Config,0x0010, 8,1,1,0,1,0xff,0x88,"BasicRate 4Mbps"}
#define awc_RID_act_BasicRate_5Mbps 				{&aironet4500_RID_Select_Active_Config,0x0010, 8,1,1,0,1,0xff,0x8B,"BasicRate 5.5Mbps"}
#define awc_RID_act_BasicRate_10Mbps 			{&aironet4500_RID_Select_Active_Config,0x0010, 8,1,1,0,1,0xff,0x94,"BasicRate 10Mbps"}
#define awc_RID_act_BasicRate_11Mbps 			{&aironet4500_RID_Select_Active_Config,0x0010, 8,1,1,0,1,0xff,0x96,"BasicRate 11Mbps"}


#define awc_RID_act_Long_retry_limit 		{&aironet4500_RID_Select_Active_Config,0x0018,16, 1,1,0,0,0xffff,0,"Short Retry Limit"}
#define awc_RID_act_Short_retry_limit 		{&aironet4500_RID_Select_Active_Config,0x001A,16, 1,1,0,0,0xffff,0,"Long Retry Limit"}
#define awc_RID_act_Tx_MSDU_lifetime 		{&aironet4500_RID_Select_Active_Config,0x001C,16, 1,1000,0,0,0xffff,0,"TX MSDU Lifetime"}
#define awc_RID_act_Rx_MSDU_lifetime 		{&aironet4500_RID_Select_Active_Config,0x001E,16, 1,1000,0,0,0xffff,0,"RX MSDU Lifetime"}
#define awc_RID_act_Stationary 			{&aironet4500_RID_Select_Active_Config,0x0020,16, 1,1,0,0,0xffff,0,"Stationary"}
#define awc_RID_act_BC_MC_Ordering 			{&aironet4500_RID_Select_Active_Config,0x0022,16, 1,1,0,0,0xffff,0,"Strictly order Bcast and Mcast"}
#define awc_RID_act_Device_type 			{&aironet4500_RID_Select_Active_Config,0x0024,16, 1,1,1,0,0xffff,0x0065,"Radio Type PC4500"}
#define awc_RID_act_Reserved_0x0026 			{&aironet4500_RID_Select_Active_Config,0x0026, 8,10,1,0,0,0xff,0,"Reserved0x28"}


//SCANNING/ASSOCIATING
#define awc_RID_act_ScanMode			awc_def_act_RID(0x0030,"ScanMode",		16,0xf,0, NULL)
#define awc_RID_act_ScanMode_Active 		awc_def_act_RID(0x0030,"ScanMode Active",		16,0xf,0, "Active")
#define awc_RID_act_ScanMode_Passive 		awc_def_act_RID(0x0030,"ScanMode Passive",		16,0xf,1, "Passive")
#define awc_RID_act_ScanMode_Aironet_ext		awc_def_act_RID(0x0030,"ScanMode Aironet Ext",	16,0xf,2, "Aironet Ext")
#define awc_RID_act_ProbeDelay 			awc_def_act_RID(0x0032,"ProbeDelay",		16,0xffff,0," msek") 		//                 Time ms to wait after switching to a channel for clear channel assessment.
#define awc_RID_act_ProbeEnergyTimeout 		awc_def_act_RID(0x0034,"ProbeEnergyTimeout",	16,0xffff,0,"msek") 	//          Time to wait for energy after an active probe.
#define awc_RID_act_ProbeResponseTimeout		awc_def_act_RID(0x0036,"ProbeResponseTimeout",	16,0xffff,0,"msek") 	// Time to wait for a probe response after energy detected.
#define awc_RID_act_BeaconListenTimeout 		awc_def_act_RID(0x0038,"BeaconListenTimeout",	16,0xffff,0,"msek")	//    0 default    40          Time to listen for a beacon on each channel.
#define awc_RID_act_IbssJoinNetTimeout 		awc_def_act_RID(0x003A,"IbssJoinNetTimeout",	16,0xffff,0,"msek")	//       0 default    10000       IBSS: Time to scan for an IBSS before forming a
#define awc_RID_act_AuthenticationTimeout 		awc_def_act_RID(0x003C,"AuthenticationTimeout",16,0xffff,0,"msek")	//       0 default    2000        Time limit after which an authentication sequence will
#define awc_RID_act_AuthenticationType 		awc_def_act_RID(0x003E,"AuthenticationType",	16,0xffff,0,NULL)	//       0 default    1 (open) //    Selects the desired authentication and privacy methods.		 
#define awc_RID_act_AuthenticationType_None 	awc_def_act_RID(0x003E,"AuthenticationType None",	16,0xffff,0,"None") 	//   0x00 = None	
#define awc_RID_act_AuthenticationType_Open		awc_def_act_RID(0x003E,"AuthenticationType Open",	16,0xffff,1,"Open") 	//             0x01 = Open
#define awc_RID_act_AuthenticationType_Shared	awc_def_act_RID(0x003E,"AuthenticationType Shared-Key",	16,0xffff,2,"Shared-Key")  	//     0x02 = Shared-Key
#define awc_RID_act_AuthenticationType_Exclude_Open awc_def_act_RID(0x003E,"AuthenticationType Exclude Open",	16,0xffff,4,"Exclude Open")   	//              0x04 = Exclude Unencrypted
#define awc_RID_act_AssociationTimeout 		awc_def_act_RID(0x0040,"AssociationTimeout",	16,0xffff,0,"msek")	//       0 default    2000        ESS: Time limit after which an association sequence
#define awc_RID_act_SpecifiedAPtimeout 		awc_def_act_RID(0x0042,"SpecifiedAPtimeout",	16,0xffff,0,"msek")	//       0 default    10000       0 selects the factory default [~10 sec].
#define awc_RID_act_OfflineScanInterval 		awc_def_act_RID(0x0044,"OfflineScanInterval",	16,0xffff,0,"msek")	//       0            0           0 disables offline scanning.(kus)        The time period between offline scans.
#define awc_RID_act_OfflineScanDuration 		awc_def_act_RID(0x0046,"OfflineScanDuration",	16,0xffff,0,"msek")	//       0            0           0 disables offline scanning. //    (kus)        The duration of an offline scan.
#define awc_RID_act_LinkLossDelay 			awc_def_act_RID(0x0048,"LinkLossDelay",	16,0xffff,0,"msek")	//       0  0 Time to delay before reporting a loss of association
#define awc_RID_act_MaxBeaconLostTime 		awc_def_act_RID(0x004A,"MaxBeaconLostTime",	16,0xffff,0,"msek")	//      0 default    500        If no beacons are received for this time period, the unit
#define awc_RID_act_RefreshInterval 		awc_def_act_RID(0x004C,"RefreshInterval",	16,0xffff,0,"msek")		//      0 default    10000      At the specified interval, the station will send a refresh
//POWER SAVE OPERATION
#define awc_RID_act_PowerSaveMode 			awc_def_act_RID(0x0050,"PowerSaveMode",	16,0xffff,0,NULL) 		//      0  0Note, for IBSS there is only one PSP mode and it is only enabled if the ATIMwindow is non-zero.
#define awc_RID_act_PowerSaveMode_CAM 		awc_def_act_RID(0x0050,"PowerSaveMode CAM",	16,0x000f,0,"CAM") 	// 0 = CAM
#define awc_RID_act_PowerSaveMode_PSP 		awc_def_act_RID(0x0050,"PowerSaveMode PSP",	16,0x000f,1,"PSP") 	// 1 = PSP
#define awc_RID_act_PowerSaveMode_Fast_PSP		awc_def_act_RID(0x0050,"PowerSaveMode Fast PSP",	16,0x000f,2,"Fast PSP")	//2 = PSP-CAM [FASTPSP]
#define awc_RID_act_SleepForDTIMs 			awc_def_act_RID(0x0052,"SleepForDTIMs",	16,0xffff,0,"DTIMs")	//      0  0If non-zero, the station may sleep through DTIMs; this
#define awc_RID_act_ListenInterval 			awc_def_act_RID(0x0054,"ListenInterval",	16,0xffff,0,"msek")		//      0 default    200 kus    Maximum time to awaken for TIMs. 0 selects factory
#define awc_RID_act_FastListenInterval 		awc_def_act_RID(0x0056,"FastListenInterval",	16,0xffff,0,"msek")  //    0 default    100 kus    The listen interval to be used immediately after
#define awc_RID_act_ListenDecay 			awc_def_act_RID(0x0058,"ListenDecay",		16,0xffff,0,"times")	//      0 default    2Number of times to use the current listen interval
#define awc_RID_act_FastListenDelay 		awc_def_act_RID(0x005A,"FastListenDelay",	16,0xffff,0,"msek")	//      0 default    200 kus    Time interval to delay before going to fast listen
#define awc_RID_act_Reserved0x005C 			awc_def_act_RID(0x005C,"Reserved0x005C",	32,0,0,"")	//
//ADHOC (or AP) OPERATION
#define awc_RID_act_BeaconPeriod 			awc_def_act_RID(0x0060,"BeaconPeriod",		16,0xffff,0,"msek")	//      0 default    100        0 selects the factory default of [~100 ms].  (kus)
#define awc_RID_act_AtimDuration 			awc_def_act_RID(0x0062,"AtimDuration",		16,0xffff,0,"msek")	//      0 default    5 kus      The time period reserved for ATIMs immediately after (kus)      the beacon. 0xFFFF will disable the ATIM window; power save mode will not operate.This parameter only applies to adhoc/IBSS.
#define awc_RID_act_Reserved0x0064 			awc_def_act_RID(0x0064,"Reserved64",		16,0xffff,0,"")	//      0  0Reserved for future use
#define awc_RID_act_DSChannel 			awc_def_act_RID(0x0066,"DSChannel",		16,0xffff,0,"")	//      0 default    1The desired operating channel.  ()refer to 802.11)       For North America, a Channel of 0 is 2412 MHz.
#define awc_RID_act_Reserved0x0068 			awc_def_act_RID(0x0068,"Reserved68",		16,0xffff,0,"")	//      0  0Reserved for future use
#define awc_RID_act_DTIM_Period 			awc_def_act_RID(0x006A,"DTIM Period",		16,0xffff,0,"")	//      0 default    1Selects how often a beacon is a DTIM for APs
#define awc_RID_act_Reserved0x0006C 		awc_def_act_RID(0x006C,"Reserved6C",		32,0xffffffff,0,"")	//    0's0's        Reserved for future use
//RADIO OPERATION
#define awc_RID_act_RadioSpreadType 		awc_def_act_RID(0x0070,"RadioSpreadType",	16,0xffff,0,NULL)	//      0 default    0Selects the radio operational mode. By default, this will
#define awc_RID_act_RadioSpreadType_FH 		awc_def_act_RID(0x0070,"RadioSpreadType FH",	16,0xffff,0,"FH")	//0 = 802.11 FH Radio (Default)
#define awc_RID_act_RadioSpreadType_DS 		awc_def_act_RID(0x0070,"RadioSpreadType DS",	16,0xffff,1,"DS")	//1 = 802.11 DS Radio
#define awc_RID_act_RadioSpreadType_LM 		awc_def_act_RID(0x0070,"RadioSpreadType LM2000",	16,0xffff,2,"LM2000")	//2 = LM2000 (Legacy) DS Radio
#define awc_RID_act_TX_antenna_Diversity 		awc_def_act_RID(0x0072,"TX antenna Diversity",	16,0xff00,0,NULL)	//       0 default    0x0303    This field is bit-mapped to select the operational
#define awc_RID_act_TX_antenna_Diversity_default	awc_def_act_RID(0x0072,"TX antenna Diversity Default",	16,0xff00,0x0000,"Default")	//  0 = Diversity as programmed at the factory
#define awc_RID_act_TX_antenna_Diversity_1 		awc_def_act_RID(0x0072,"TX antenna Diversity Antenna 1",	16,0xff00,0x0100,"Antenna 1")	//  1 = Antenna 1 only
#define awc_RID_act_TX_antenna_Diversity_2 		awc_def_act_RID(0x0072,"TX antenna Diversity Antenna 2",	16,0xff00,0x0200,"Antenna 2")	//  2 = Antenna 2 only
#define awc_RID_act_TX_antenna_Diversity_both 	awc_def_act_RID(0x0072,"TX antenna Diversity both antennas",	16,0xff00,0x0300,"both antennas")	//  3 = Antennas 1 and 2 are active
#define awc_RID_act_RX_antenna_Diversity		awc_def_act_RID(0x0072,"RX antenna Diversity",	16,0x00ff,0,NULL)	//       0 default    0x0303    This field is bit-mapped to select the operational
#define awc_RID_act_RX_antenna_Diversity_default	awc_def_act_RID(0x0072,"RX antenna Diversity Default",	16,0x00ff,0,"Default")	//  0 = Diversity as programmed at the factory
#define awc_RID_act_RX_antenna_Diversity_1		awc_def_act_RID(0x0072,"RX antenna Diversity Antenna 1",	16,0x00ff,1,"Antenna 1")	//  1 = Antenna 1 only
#define awc_RID_act_RX_antenna_Diversity_2 		awc_def_act_RID(0x0072,"RX antenna Diversity Antenna 2",	16,0x00ff,2,"Antenna 2")	//  2 = Antenna 2 only
#define awc_RID_act_RX_antenna_Diversity_both	awc_def_act_RID(0x0072,"RX antenna Diversity both antennas",	16,0x00ff,3,"both antennas")	//
#define awc_RID_act_TransmitPower 			awc_def_act_RID(0x0074,"TransmitPower",	16,0xffff,0,"mW (rounded up, btw)")	//       0 default    250 or    0 selects the default (maximum power allowed for the
#define awc_RID_act_RSSIthreshold 			awc_def_act_RID(0x0076,"RSSIthreshold",	16,0xffff,0,"units")	//       0 default    0         RSSI threshold. 0 selects factory default.
#define awc_RID_act_Reserved0x0078 			awc_def_act_RID(0x0078,"Reserved0x0078",	64,0,0,"")	//     0's0's       reserved for future radio specific parameters
#define awc_RID_act_Modulation 				awc_def_act_RID(0x0078,"Modulation",	8,0xff,0,"")	//     modulation type
#define awc_RID_act_Reserved0x0079 			awc_def_act_RID(0x0079,"Reserved0x0079",	56,0xff,0,"")	//     0's0's       reserved for future radio specific parameters

//AIRONET EXTENSIONS
#define awc_RID_act_NodeName 			awc_def_act_RID(0x0080,"NodeName",		128,0,0,"")	//    0  0         Station name.
#define awc_RID_act_ARLThreshold 		awc_def_act_RID(0x0090,"ARLThreshold",		16,0xffff,0,"times")	//       0 default    0xFFFF    0 selects the factory defaults. (which for now is
#define awc_RID_act_ARLDecay 			awc_def_act_RID(0x0092,"ARLDecay",		16,0xffff,0,"times")	//       0 default    0xFFFF    0 selects the factory defaults. (which for now is
#define awc_RID_act_ARLDelay 			awc_def_act_RID(0x0094,"ARLDelay",		16,0xffff,0,"times")	//       0 default    0xFFFF    0 selects the factory defaults. (which for now is
#define awc_RID_act_Unused0x0096 		awc_def_act_RID(0x0096,"Reserved0x96",		16,0xffff,0,"")	//
#define awc_RID_act_MagicPacketAction 		awc_def_act_RID(0x0098,"MagicPacketAction",	8,0xff,0," hell knows what")	//        0  0         0 selects no action to be taken on a magic packet and"
#define awc_RID_act_MagicPacketControl 		awc_def_act_RID(0x0099,"MagicPacketControl",	8,0xff,0," hell know what")	//        0  0         0 will disable the magic packet mode command"



// ***************************        SSID  RID



#define awc_RID_SSID_RidLen 				awc_def_SSID_RID(0x0000,"RidLen",		16,0xffff,0,"")	//RidLen     ",16,0xffff,,"")	//      read-only        Length of this RID including the length field 0x68
#define awc_RID_SSID_Accept_any 		awc_def_SSID_RID(0x0002,"Accept Any SSID",	16,0xffff,0,"Accept ANY SSID")	//
#define awc_RID_SSIDlen1 			awc_def_SSID_RID(0x0002,"SSIDlen1",		16,0xffff,0,"")	//      7      The length of the SSID1 byte string.
#define awc_RID_SSID1 				awc_def_SSID_RID(0x0004,"SSID1",		255,0,0,"")	//    "tsunami"        The identifier uniquely identifying the wireless system.
#define awc_RID_SSIDlen2 			awc_def_SSID_RID(0x0024,"SSIDlen2",		16,0xffff,0,"")	//      0      The length of the SSID2 byte string.
#define awc_RID_SSID2 				awc_def_SSID_RID(0x0026,"SSID2",		255,0,0,"") 	//   
#define awc_RID_SSIDlen3 			awc_def_SSID_RID(0x0046,"SSIDlen3",		16,0xffff,0,"")	//      0      The length of the SSID3 byte string.
#define awc_RID_SSID3 				awc_def_SSID_RID(0x0048,"SSID3",		255,0,0,"")	//    
#define awc_RID_SSID1hex 				awc_def_SSID_RID(0x0004,"SSID1hex",		255,0xff,0,"")	
#define awc_RID_SSID2hex 				awc_def_SSID_RID(0x0026,"SSID2hex",		255,0xff,0,"") 	
#define awc_RID_SSID3hex 				awc_def_SSID_RID(0x0048,"SSID3hex",		255,0xff,0,"")	

// AP list

#define awc_RID_AP_List_RidLen 			awc_def_AP_List_RID(0x0000,"RidLen",		16,0xffff,0,"")		//      read-only     Length of this RID including the length field
#define awc_RID_AP_List_SpecifiedAP1 		awc_def_AP_List_RID(0x0002,"SpecifiedAP1",		48,0xff,0,"")	//    0   Specifies the MAC address of an access point to attempt to associate to first, before looking for other Access Points
#define awc_RID_AP_List_SpecifiedAP2 		awc_def_AP_List_RID(0x0008,"SpecifiedAP2",		48,0xff,0,"")	//    0   Allows for a secondary AP to associate to if the radio cannot associate to the primary AP.
#define awc_RID_AP_List_SpecifiedAP3 		awc_def_AP_List_RID(0x000E,"SpecifiedAP3",		48,0xff,0,"")	//    0   Allows for a third option when specifying a list of APs.
#define awc_RID_AP_List_SpecifiedAP4 		awc_def_AP_List_RID(0x0014,"SpecifiedAP4",		48,0xff,0,"")	//    0   Allows for a fourth option when specifying a list of  APs.

//   Driver Name

#define awc_RID_Dname_RidLen 			awc_def_Dname_RID(0x0000,"RidLen",		16,0xffff,0,"")	//      read-only     Length of this RID including the length field
#define awc_RID_Dname_DriverName 		awc_def_Dname_RID(0x0002,"DriverName",		128,0,0,"")	// The driver name and version can be written here for  debugging support


//       Encapsulation Transformations RID

#define awc_RID_Enc_RidLen 			awc_def_Enc_RID(0x0000,"RidLen",	16,0xffff,0,"")	//       read-only     Length of this RID including the length field
#define awc_RID_Enc_EtherType1 			awc_def_Enc_RID(0x0002,"EtherType1",	16,0xffff,0,"")	//       0   Note, the ethertype values are in network transmission order.  So IP (0x800) is actually (0x0008). Zero ends the list and selects the default action.
#define awc_RID_Enc_Action_RX_1 		awc_def_Enc_RID(0x0004,"RX Action 1",	16,0x0001,0,NULL)	//       0   This field is bit encoded as follows:
#define awc_RID_Enc_Action_RX_1_RFC_1042 	awc_def_Enc_RID(0x0004,"RX Action 1",	16,0x0001,1,"RX RFC1042")	//  bit 0   (0x0001)  1=RFC1042 is kept for receive packets.
#define awc_RID_Enc_Action_RX_1_802_11 		awc_def_Enc_RID(0x0004,"RX Action 1",	16,0x0001,0,"RX 802.11")	//  bit 0   (0x0001)  1=RFC1042 is kept for receive packets.
#define awc_RID_Enc_Action_TX_1 		awc_def_Enc_RID(0x0004,"TX Action 1",	16,0x0002,0,NULL)	//
#define awc_RID_Enc_Action_TX_1_RFC_1042 	awc_def_Enc_RID(0x0004,"TX Action 1",	16,0x0002,1,"TX 802.11" )	//  bit 1   (0x0002)  0=RFC1042 is used for transmit encapsulation.  1=802.1H is used for transmit encapsulation.
#define awc_RID_Enc_Action_TX_1_802_11 		awc_def_Enc_RID(0x0004,"Tx Action 1",	16,0x0002,0,"TX RFC1042")	//  bit 1   (0x0002)  0=RFC1042 is used for transmit encapsulation.  1=802.1H is used for transmit encapsulation.
#define awc_RID_Enc_EtherType2 			awc_def_Enc_RID(0x0006,"EtherType2",	16,0xffff,0,"")	//       0   Note, the ethertype values are in network transmission order.  So IP (0x800) is actually (0x0008). Zero ends the list and selects the default action.
#define awc_RID_Enc_Action_RX_2 		awc_def_Enc_RID(0x0008,"RX Action 2",	16,0x0001,0,NULL)	//       0   This field is bit encoded as follows:
#define awc_RID_Enc_Action_RX_2_RFC_1042 	awc_def_Enc_RID(0x0008,"RX Action 2",	16,0x0001,1,"RX RFC1042")	//  bit 0   (0x0001)  1=RFC1042 is kept for receive packets.
#define awc_RID_Enc_Action_RX_2_802_11 		awc_def_Enc_RID(0x0008,"RX Action 2",	16,0x0001,0,"RX 802.11")	//  bit 0   (0x0001)  1=RFC1042 is kept for receive packets.
#define awc_RID_Enc_Action_TX_2 		awc_def_Enc_RID(0x0008,"TX Action 2",	16,0x0002,0,NULL)	//
#define awc_RID_Enc_Action_TX_2_RFC_1042 	awc_def_Enc_RID(0x0008,"TX Action 2",	16,0x0002,1,"TX 802.11" )	//  bit 1   (0x0002)  0=RFC1042 is used for transmit encapsulation.  1=802.1H is used for transmit encapsulation.
#define awc_RID_Enc_Action_TX_2_802_11 		awc_def_Enc_RID(0x0008,"Tx Action 2",	16,0x0002,0,"TX RFC1042")	//  bit 1   (0x0002)  0=RFC1042 is used for transmit encapsulation.  1=802.1H is used for transmit encapsulation.
#define awc_RID_Enc_EtherType3 			awc_def_Enc_RID(0x000A,"EtherType3",	16,0xffff,0,"")	//       0   Note, the ethertype values are in network transmission order.  So IP (0x800) is actually (0x0008). Zero ends the list and selects the default action.
#define awc_RID_Enc_Action_RX_3 		awc_def_Enc_RID(0x000C,"RX Action 3",	16,0x0001,0,NULL)	//       0   This field is bit encoded as follows:
#define awc_RID_Enc_Action_RX_3_RFC_1042 	awc_def_Enc_RID(0x000C,"RX Action 3",	16,0x0001,1,"RX RFC1042")	//  bit 0   (0x0001)  1=RFC1042 is kept for receive packets.
#define awc_RID_Enc_Action_RX_3_802_11 		awc_def_Enc_RID(0x000C,"RX Action 3",	16,0x0001,0,"RX 802.11")	//  bit 0   (0x0001)  1=RFC1042 is kept for receive packets.
#define awc_RID_Enc_Action_TX_3_ 		awc_def_Enc_RID(0x000C,"TX Action 3",	16,0x0002,0,NULL)	//
#define awc_RID_Enc_Action_TX_3_RFC_1042 	awc_def_Enc_RID(0x000C,"TX Action 3",	16,0x0002,1,"TX 802.11" )	//  bit 1   (0x0002)  0=RFC1042 is used for transmit encapsulation.  1=802.1H is used for transmit encapsulation.
#define awc_RID_Enc_Action_TX_3_802_11 		awc_def_Enc_RID(0x000C,"Tx Action 3",	16,0x0002,0,"TX RFC1042")	//  bit 1   (0x0002)  0=RFC1042 is used for transmit encapsulation.  1=802.1H is used for transmit encapsulation.
#define awc_RID_Enc_EtherType4			awc_def_Enc_RID(0x000E,"EtherType4",	16,0xffff,0,"")	//       0   Note, the ethertype values are in network transmission order.  So IP (0x800) is actually (0x0008). Zero ends the list and selects the default action.
#define awc_RID_Enc_Action_RX_4			awc_def_Enc_RID(0x0010,"RX Action 4",	16,0x0001,0,NULL)	//       0   This field is bit encoded as follows:
#define awc_RID_Enc_Action_RX_4_RFC_1042 	awc_def_Enc_RID(0x0010,"RX Action 4",	16,0x0001,1,"RX RFC1042")	//  bit 0   (0x0001)  1=RFC1042 is kept for receive packets.
#define awc_RID_Enc_Action_RX_4_802_11 		awc_def_Enc_RID(0x0010,"RX Action 4",	16,0x0001,0,"RX 802.11")	//  bit 0   (0x0001)  1=RFC1042 is kept for receive packets.
#define awc_RID_Enc_Action_TX_4 		awc_def_Enc_RID(0x0010,"TX Action 4",	16,0x0002,0,NULL)	//
#define awc_RID_Enc_Action_TX_4_RFC_1042 	awc_def_Enc_RID(0x0010,"TX Action 4",	16,0x0002,1,"TX 802.11" )	//  bit 1   (0x0002)  0=RFC1042 is used for transmit encapsulation.  1=802.1H is used for transmit encapsulation.
#define awc_RID_Enc_Action_TX_4_802_11 		awc_def_Enc_RID(0x0010,"Tx Action 4",	16,0x0002,0,"TX RFC1042")	//  bit 1   (0x0002)  0=RFC1042 is used for transmit encapsulation.  1=802.1H is used for transmit encapsulation.
#define awc_RID_Enc_EtherType5 			awc_def_Enc_RID(0x0012,"EtherType5",	16,0xffff,0,"")	//       0   Note, the ethertype values are in network transmission order.  So IP (0x800) is actually (0x0008). Zero ends the list and selects the default action.
#define awc_RID_Enc_Action_RX_5 		awc_def_Enc_RID(0x0014,"RX Action 5",	16,0x0001,0,NULL)	//       0   This field is bit encoded as follows:
#define awc_RID_Enc_Action_RX_5_RFC_1042 	awc_def_Enc_RID(0x0014,"RX Action 5",	16,0x0001,1,"RX RFC1042")	//  bit 0   (0x0001)  1=RFC1042 is kept for receive packets.
#define awc_RID_Enc_Action_RX_5_802_11 		awc_def_Enc_RID(0x0014,"RX Action 5",	16,0x0001,0,"RX 802.11")	//  bit 0   (0x0001)  1=RFC1042 is kept for receive packets.
#define awc_RID_Enc_Action_TX_5 		awc_def_Enc_RID(0x0014,"TX Action 5",	16,0x0002,0,NULL)	//
#define awc_RID_Enc_Action_TX_5_RFC_1042 	awc_def_Enc_RID(0x0014,"TX Action 5",	16,0x0002,1,"TX 802.11" )	//  bit 1   (0x0002)  0=RFC1042 is used for transmit encapsulation.  1=802.1H is used for transmit encapsulation.
#define awc_RID_Enc_Action_TX_5_802_11 		awc_def_Enc_RID(0x0014,"Tx Action 5",	16,0x0002,0,"TX RFC1042")	//  bit 1   (0x0002)  0=RFC1042 is used for transmit encapsulation.  1=802.1H is used for transmit encapsulation.
#define awc_RID_Enc_EtherType6 			awc_def_Enc_RID(0x0016,"EtherType6",	16,0xffff,0,"")	//       0   Note, the ethertype values are in network transmission order.  So IP (0x800) is actually (0x0008). Zero ends the list and selects the default action.
#define awc_RID_Enc_Action_RX_6 		awc_def_Enc_RID(0x0018,"RX Action 6",	16,0x0001,0,NULL)	//       0   This field is bit encoded as follows:
#define awc_RID_Enc_Action_RX_6_RFC_1042 	awc_def_Enc_RID(0x0018,"RX Action 6",	16,0x0001,1,"RX RFC1042")	//  bit 0   (0x0001)  1=RFC1042 is kept for receive packets.
#define awc_RID_Enc_Action_RX_6_802_11 		awc_def_Enc_RID(0x0018,"RX Action 6",	16,0x0001,0,"RX 802.11")	//  bit 0   (0x0001)  1=RFC1042 is kept for receive packets.
#define awc_RID_Enc_Action_TX_6 		awc_def_Enc_RID(0x0018,"TX Action 6",	16,0x0002,0,NULL)	//
#define awc_RID_Enc_Action_TX_6_RFC_1042 	awc_def_Enc_RID(0x0018,"TX Action 6",	16,0x0002,1,"TX 802.11" )	//  bit 1   (0x0002)  0=RFC1042 is used for transmit encapsulation.  1=802.1H is used for transmit encapsulation.
#define awc_RID_Enc_Action_TX_6_802_11 		awc_def_Enc_RID(0x0018,"Tx Action 6",	16,0x0002,0,"TX RFC1042")	//  bit 1   (0x0002)  0=RFC1042 is used for transmit encapsulation.  1=802.1H is used for transmit encapsulation.
#define awc_RID_Enc_EtherType7 			awc_def_Enc_RID(0x001A,"EtherType7",	16,0xffff,0,"")	//       0   Note, the ethertype values are in network transmission order.  So IP (0x800) is actually (0x0008). Zero ends the list and selects the default action.
#define awc_RID_Enc_Action_RX_7 		awc_def_Enc_RID(0x001C,"RX Action 8",	16,0x0001,0,NULL)	//       0   This field is bit encoded as follows:
#define awc_RID_Enc_Action_RX_7_RFC_1042 	awc_def_Enc_RID(0x001C,"RX Action 7",	16,0x0001,1,"RX RFC1042")	//  bit 0   (0x0001)  1=RFC1042 is kept for receive packets.
#define awc_RID_Enc_Action_RX_7_802_11 		awc_def_Enc_RID(0x001C,"RX Action 7",	16,0x0001,0,"RX 802.11")	//  bit 0   (0x0001)  1=RFC1042 is kept for receive packets.
#define awc_RID_Enc_Action_TX_7 		awc_def_Enc_RID(0x001C,"TX Action 7",	16,0x0002,0,NULL)	//
#define awc_RID_Enc_Action_TX_7_RFC_1042 	awc_def_Enc_RID(0x001C,"TX Action 7",	16,0x0002,1,"TX 802.11" )	//  bit 1   (0x0002)  0=RFC1042 is used for transmit encapsulation.  1=802.1H is used for transmit encapsulation.
#define awc_RID_Enc_Action_TX_7_802_11 		awc_def_Enc_RID(0x001C,"Tx Action 7",	16,0x0002,0,"TX RFC1042")	//  bit 1   (0x0002)  0=RFC1042 is used for transmit encapsulation.  1=802.1H is used for transmit encapsulation.
#define awc_RID_Enc_EtherType8 			awc_def_Enc_RID(0x001E,"EtherType7",	16,0xffff,0,"")	//       0   Note, the ethertype values are in network transmission order.  So IP (0x800) is actually (0x0008). Zero ends the list and selects the default action.
#define awc_RID_Enc_Action_RX_8 		awc_def_Enc_RID(0x0020,"RX Action 8",	16,0x0001,0,NULL)	//       0   This field is bit encoded as follows:
#define awc_RID_Enc_Action_RX_8_RFC_1042 	awc_def_Enc_RID(0x0020,"RX Action 8",	16,0x0001,1,"RX RFC1042")	//  bit 0   (0x0001)  1=RFC1042 is kept for receive packets.
#define awc_RID_Enc_Action_RX_8_802_11 		awc_def_Enc_RID(0x0020,"RX Action 8",	16,0x0001,0,"RX 802.11")	//  bit 0   (0x0001)  1=RFC1042 is kept for receive packets.
#define awc_RID_Enc_Action_TX_8 		awc_def_Enc_RID(0x0020,"TX Action 8",	16,0x0002,0,NULL)	//
#define awc_RID_Enc_Action_TX_8_RFC_1042 	awc_def_Enc_RID(0x0020,"TX Action 8",	16,0x0002,1,"TX 802.11" )	//  bit 1   (0x0002)  0=RFC1042 is used for transmit encapsulation.  1=802.1H is used for transmit encapsulation.
#define awc_RID_Enc_Action_TX_8_802_11 		awc_def_Enc_RID(0x0020,"Tx Action 8",	16,0x0002,0,"TX RFC1042")	//  bit 1   (0x0002)  0=RFC1042 is used for transmit encapsulation.  1=802.1H is used for transmit encapsulation.


// WEP Key volatile
#define awc_RID_WEPv_RidLen 			awc_def_WEPv_RID(0x0000,"RidLen",	16,0xffff,0,"")	//       read-only     Length of this RID including the length field
#define awc_RID_WEPv_KeyIndex 			awc_def_WEPv_RID(0x0002,"KeyIndex",	16,0xffff,0,"Index to list of keys")	
#define awc_RID_WEPv_Address 			awc_def_WEPv_RID(0x0004,"Address",	48,0xff,0,"mac address related to keys")	
#define awc_RID_WEPv_KeyLen 			awc_def_WEPv_RID(0x000A,"KeyLen",	16,0xffff,0,"Key Length (0 and 5 are valid)")	
#define awc_RID_WEPv_Key 			awc_def_WEPv_RID(0x000C,"Key",		128,0xff,0,"Key itself in hex coding")
#define awc_RID_WEPv_KeyAscii 			awc_def_WEPv_RID(0x000C,"KeyAscii",	128,0,0,"Key itself in ascii coding")

// WEP Key non-volatile
#define awc_RID_WEPnv_RidLen 			awc_def_WEPnv_RID(0x0000,"RidLen",	16,0xffff,0,"")	//       read-only     Length of this RID including the length field
#define awc_RID_WEPnv_KeyIndex 			awc_def_WEPnv_RID(0x0002,"KeyIndex",	16,0xffff,0,"Index to list of keys")	
#define awc_RID_WEPnv_Address 			awc_def_WEPnv_RID(0x0004,"Address",	48,0xff,0,"mac address related to keys")	
#define awc_RID_WEPnv_KeyLen 			awc_def_WEPnv_RID(0x000A,"KeyLen",	16,0xffff,0,"Key Length (0 and 5 are valid)")	
#define awc_RID_WEPnv_Key 			awc_def_WEPnv_RID(0x000C,"Key",		128,0xff,0,"Key itself in hex coding")
#define awc_RID_WEPnv_KeyAscii 			awc_def_WEPnv_RID(0x000C,"KeyAscii",	128,0,0,"Key itself in ascii coding")

// Modulation
#define awc_RID_Modulation_RidLen 		awc_def_Modulation_RID(0x0000,"RidLen",		16,0xffff,0,"")	//       read-only     Length of this RID including the length field
#define awc_RID_Modulation_Modulation 		awc_def_Modulation_RID(0x0002,"Modulation",	16,0xffff,0,"Modulation")	


//   Capabilities RID
#define awc_RID_Cap_RidLen 		awc_def_Cap_RID(0x0000,"RidLen",		16,0xffff,0,"")	//        read-only      Length of this RID including the length field
#define awc_RID_Cap_OUI 		awc_def_Cap_RID(0x0002,"OUI",			24,0xffff,0,"")	//      0x00 0x40      This field will give the manufacturer OUI (fourth byte   always zero).
#define awc_RID_Cap_ProductNum 		awc_def_Cap_RID(0x0006,"ProductNum",		24,0xffff,0,"")	//      0x0004         This field will give the product number.
#define awc_RID_Cap_ManufacturerName 	awc_def_Cap_RID(0x0008,"ManufacturerName",	255,0,0,"")	//      ASCIIz encoding of manufacturer name.
#define awc_RID_Cap_ProductName 	awc_def_Cap_RID(0x0028,"ProductName",		128,0,0,"")	//     PC4500         ASCIIz encoding of product name.
#define awc_RID_Cap_ProductVersion 	awc_def_Cap_RID(0x0038,"ProductVersion",	64,0,0,"")	//      .    ASCIIz encoding of product (firmware?) version.
#define awc_RID_Cap_FactoryAddress 	awc_def_Cap_RID(0x0040,"FactoryAddress",	48,0xff,0,"")	// This field will contain the OEM assigned IEEE address. If there is no OEM address assigned, the Aironet assigned  IEEE Address will be returned in this field.
#define awc_RID_Cap_AironetAddress 	awc_def_Cap_RID(0x0046,"AironetAddress",	48,0xff,0,"")	// This field will contain the Aironet factory assigned    IEEE address.
#define awc_RID_Cap_RadioSpreadType_DS 	awc_def_Cap_RID(0x004C,"RadioType_FH",		16,0x0001,1,"")	//	  0x01 = 802.11 FH
#define awc_RID_Cap_RadioSpreadType_FH 	awc_def_Cap_RID(0x004C,"RadioType_DS",		16,0x0002,2,"")	//	  0x02 = 802.11 DS
#define awc_RID_Cap_RadioSpreadType_Legacy awc_def_Cap_RID(0x004C,"RadioType_Legacy",	16,0x0004,4,"")	//	  0x04 = LM2000 (Legacy) DS //  Note, more than one bit may be set for radios     supporting multiple modes of operation.
#define awc_RID_Cap_RegDomain 		awc_def_Cap_RID(0x004E,"RegDomain",		16,0xffff,0,"")	// This field indicates the registration domain/country   The values as assigned by 802.11 will be used.
#define awc_RID_Cap_Callid 		awc_def_Cap_RID(0x0050,"Callid",		48,0xff,0,"")	// This field indicates the callid assigned to the unit (if  RegDomain is Japan) Each nibble will contain one decimal digit of the 12 digit callid. (Note, this is not the encoded format).
#define awc_RID_Cap_SupportedRates 	awc_def_Cap_RID(0x0056,"SupportedRates",	64,0xff,0,"")	//      0x02, 0x04,    This field will indicate the 802.11 supported rates as  specified in the rates.
#define awc_RID_Cap_RxDiversity 	awc_def_Cap_RID(0x005E,"RxDiversity",		8 ,0xff,0,"")	//         0x03 This field will indicate the number of antennas  supported as a bit mask.
#define awc_RID_Cap_TxDiversity 	awc_def_Cap_RID(0x005F,"TxDiversity",		8 ,0xff,0,"")	//         0x03 This field will indicate the number of antennas supported as a bit mask.
#define awc_RID_Cap_TxPowerLevels 	awc_def_Cap_RID(0x0060,"TxPowerLevels",	128,0xff,0,"")	//     250  This table indicates the supported transmit power  levels. (values are in mW)  Zero terminates the list. Note, this may be further restricted depending on   country selected.
#define awc_RID_Cap_HardwareVersion 	awc_def_Cap_RID(0x0070,"HardwareVersion",	16,0xffff,0,"")	//        0    This indicates the revision of hardware.
#define awc_RID_Cap_HardwareCapabilit 	awc_def_Cap_RID(0x0072,"HardwareCapabilit",	16,0xffff,0,"")	//        0    This is a bit-mapped field indicating harware  capabilities. No bits have been assigned yet. Initially this is zero.
#define awc_RID_Cap_TemperatureRange 	awc_def_Cap_RID(0x0074,"TemperatureRange",	16,0xffff,0,"")	//        0    This indicates the temperature range capability.
#define awc_RID_Cap_SoftwareVersion 	awc_def_Cap_RID(0x0076,"SoftwareVersion",	16,0xffff,0,"")	//        0    This indicates the revision of software.
#define awc_RID_Cap_SoftwareVersion_major 	awc_def_Cap_RID(0x0076,"SoftwareVersion major",	16,0xff00,0,"")	//  The upper byte indicates the major version and the
#define awc_RID_Cap_SoftwareVersion_minor 	awc_def_Cap_RID(0x0076,"SoftwareVersion minor",	16,0x00ff,0,"")	//  lower byte the minor version.
#define awc_RID_Cap_SoftwareSubVersion 	awc_def_Cap_RID(0x0078,"SoftwareSubVersio",	16,0xffff,0,"")	//        0    This indicates the sub-revision of software.
#define awc_RID_Cap_InterfaceVersion	awc_def_Cap_RID(0x007A,"InterfaceVersion",	16,0xffff,0,"")	//        0    This indicates the revision of the interface. This will be bumped whenever there are incompatible  modifications made to the interfac  This may be bumped on first release to ensure that  "unreleased" utilities/drivers become unusable.
#define awc_RID_Cap_SoftwareCapabilities awc_def_Cap_RID(0x007C,"SoftwareCapabiliti",	160,0xff,0,"")	//    0    This field gives a bit mapped indication of capabilities. No capability bits have yet been assigned.
#define awc_RID_Cap_BootBlockVersion 	awc_def_Cap_RID(0x007E,"BootBlockVersion ",	16,0xffff,0,"")	// This indicates the revision of bootblock software. The upper byte indicates the major version and the lower byte the minor version.  Note, BCD encoding is used. (version 2.11 would be  0x0211.)


// Status RID 

#define awc_RID_Status_RidLen 		awc_def_Stat_RID( 0x0000,"RidLen",		16,0xffff,0,"")		//    Length of this RID including the length field
#define awc_RID_Status_MacAddress 	awc_def_Stat_RID( 0x0002,"MacAddress",		48,0xff,0,"")		//  The MAC address in use by the station.
#define awc_RID_Status_OperationalMode 	awc_def_Stat_RID( 0x0008,"OperationalMode",	16,0xffff,0,NULL)	//    Bit-mapped.
#define awc_RID_Status_Configured 	awc_def_Stat_RID( 0x0008,"OperationalMode Configured",	16,0x0001,1,"Configured")	//
#define awc_RID_Status_MAC_Enabled 	awc_def_Stat_RID( 0x0008,"OperationalMode MAC Enabled",	16,0x0002,2,"MAC Enabled")	//
#define awc_RID_Status_Receive_Enabled 	awc_def_Stat_RID( 0x0008,"OperationalMode Receive Enabled",	16,0x0004,4,"Receive Enabled")	//
#define awc_RID_Status_In_Sync 		awc_def_Stat_RID( 0x0008,"OperationalMode In Sync with cell",	16,0x0010,10,"In Sync with cell")	//
#define awc_RID_Status_Associated 	awc_def_Stat_RID( 0x0008,"OperationalMode Associated",	16,0x0020,20,"Associated")	//
#define awc_RID_Status_Error 		awc_def_Stat_RID( 0x0008,"OperationalMode Error",	16,0x8000,0x8000,"Error")	//
#define awc_RID_Status_ErrorCode 	awc_def_Stat_RID( 0x000A,"ErrorCode",		16,0xffff,0,"")		//    Non-zero if an error state has been entered
#define awc_RID_Status_CurrentSignalQuality awc_def_Stat_RID( 0x000C,"CurrentSignalQuality",16,0xffff,0,"")		//    A measure of the current signal quality.
#define awc_RID_Status_SSIDlength 	awc_def_Stat_RID( 0x000E,"SSIDlength",		16,0xffff,0,"")		//    This length of the following SSID.
#define awc_RID_Status_SSID 		awc_def_Stat_RID( 0x0010,"SSID",		255,0,0,"")		// The SSID that is currently in effect.
#define awc_RID_Status_ApName 		awc_def_Stat_RID( 0x0030,"ApName",		128,0,0,"")		// The name of the current BSSID (ESS mode only)
#define awc_RID_Status_CurrentBssid 	awc_def_Stat_RID( 0x0040,"CurrentBssid",	48,0xff,0,"")		// BSSID that is currently in effect.
#define awc_RID_Status_PreviousBssid1 	awc_def_Stat_RID( 0x0046,"PreviousBssid1",	48,0xff,0,"")		// A former BSSID.
#define awc_RID_Status_PreviousBssid2 	awc_def_Stat_RID( 0x004C,"PreviousBssid2",	48,0xff,0,"")		//  A former BSSID.
#define awc_RID_Status_PreviousBssid3 	awc_def_Stat_RID( 0x0052,"PreviousBssid3",	48,0xff,0,"")		//  A former BSSID.
#define awc_RID_Status_BeaconPeriod 	awc_def_Stat_RID( 0x0058,"BeaconPeriod",	16,0xffff,0,"msek")	// (kus)        The current beacon period.
#define awc_RID_Status_DtimPeriod 	awc_def_Stat_RID( 0x005A,"DtimPeriod",		16,0xffff,0,"units")	//    The current DTIM period (number of beacons between DTIMs).
#define awc_RID_Status_AtimDuration 	awc_def_Stat_RID( 0x005C,"AtimDuration",	16,0xffff,0,"msek")	// (kus)        The current ATIM window duration. Adhoc/Ibss only
#define awc_RID_Status_HopPeriod 	awc_def_Stat_RID( 0x005E,"HopPeriod",		16,0xffff,0,"msek")	// (kus)        The current hopping period.
#define awc_RID_Status_ChannelSet 	awc_def_Stat_RID( 0x0060,"ChannelSet",		16,0xffff,0,"Set")	//    The current channel set.
#define awc_RID_Status_Channel		awc_def_Stat_RID( 0x0062,"Channel",		16,0xffff,0," ")	//    The current operating channel.
#define awc_RID_Status_HopsToBackbone 	awc_def_Stat_RID( 0x0064,"HopsToBackbone",	16,0xffff,0,"hops")	//    0 indicates a backbone association.
#define awc_RID_Status_ApTotalLoad 	awc_def_Stat_RID( 0x0066,"ApTotalLoad",	16,0xffff,0,"units")	//    Total load including broadcast/multicast from backbone.  This is the value extracted from the Aironet element.
#define awc_RID_Status_OurGeneratedLoad awc_def_Stat_RID( 0x0068,"OurGeneratedLoad",	16,0xffff,0,"units")	//   Total load generated by our station (transmitted and received). Excludes received broadcast/multicast traffic.
#define awc_RID_Status_AccumulatedArl 	awc_def_Stat_RID( 0x006A,"AccumulatedArl",	16,0xffff,0,"units")	//

// AP RID

#define awc_RID_AP_16RidLen 		awc_def_AP_RID(0x0000,"RidLen",		16,0xffff,0,"")	//        0x06, read-only Length of this RID including the length field
#define awc_RID_AP_TIM_addr 		awc_def_AP_RID(0x0002,"TIM Addr",		16,0xffff,0,"")	//        Read only       The "Traffic Indication Map" is updated by the host via
#define awc_RID_AP_Airo_addr 		awc_def_AP_RID(0x0004,"Airo Addr",		16,0xffff,0,"")	//        Read only       The "Aironet Information Element" is updated by the host via the AUX I/O ports. This is the address of the Aironet Element.


// Statistics RID

#define awc_RID_Stats_RidLen 		awc_def_Stats_RID(0x0000,0x0000,"RidLen",		"Length of the RID including the length field.")
#define awc_RID_Stats_RxOverrunErr 	awc_def_Stats_RID(0x0002,0x0004,"Stats_RxOverrunErr",	"Receive overruns -- No buffer available to handle the receive. (result is that the packet is never received)")
#define awc_RID_Stats_RxPlcpCrcErr 	awc_def_Stats_RID(0x0004,0x0008,"Stats_RxPlcpCrcErr",	"PLCP header checksum errors (CRC16).")
#define awc_RID_Stats_RxPlcpFormat 	awc_def_Stats_RID(0x0006,0x000C,"Stats_RxPlcpFormat",	"PLCP format errors.")
#define awc_RID_Stats_RxPlcpLength 	awc_def_Stats_RID(0x0008,0x0010,"Stats_RxPlcpLength",	"PLCP length is incorrect.")
#define awc_RID_Stats_RxMacCrcErr 	awc_def_Stats_RID(0x000A,0x0014,"Stats_RxMacCrcErr",	"Count of MAC CRC32 errors.")
#define awc_RID_Stats_RxMacCrcOk 	awc_def_Stats_RID(0x000C,0x0018,"Stats_RxMacCrcOk",	"Count of MAC CRC32 received correctly.")
#define awc_RID_Stats_RxWepErr 		awc_def_Stats_RID(0x000E,0x001C,"Stats_RxWepErr",	"Count of all WEP ICV checks that failed. (this value is included in Stats_RxMacCrcOk)")
#define awc_RID_Stats_RxWepOk 		awc_def_Stats_RID(0x0010,0x0020,"Stats_RxWepOk",	"Count of all WEP ICV checks that passed. (this value is  included in Stats_RxMacCrcOk)")
#define awc_RID_Stats_RetryLong 	awc_def_Stats_RID(0x0012,0x0024,"Stats_RetryLongCount",	"of all long retries. (Does not include first attempt for a packet).")
#define awc_RID_Stats_RetryShort 	awc_def_Stats_RID(0x0014,0x0028,"Stats_RetryShort",	"Count of all short retries. (Does not include first attempt for   a packet).")
#define awc_RID_Stats_MaxRetries 	awc_def_Stats_RID(0x0016,0x002C,"Stats_MaxRetries",	"Count of number of packets that max-retried -- ie were  never ACK-d.")
#define awc_RID_Stats_NoAck 		awc_def_Stats_RID(0x0018,0x0030,"Stats_NoAck",		"Count of number of times that ACK was not received.")
#define awc_RID_Stats_NoCts 		awc_def_Stats_RID(0x001A,0x0034,"Stats_NoCts",		"Count of number of timer that CTS was not received.")
#define awc_RID_Stats_RxAck 		awc_def_Stats_RID(0x001C,0x0038,"Stats_RxAck",		"Count of number of expected ACKs that were received.")
#define awc_RID_Stats_RxCts 		awc_def_Stats_RID(0x001E,0x003C,"Stats_RxCts",		"Count of number of expected CTSs that were received.")
#define awc_RID_Stats_TxAck 		awc_def_Stats_RID(0x0020,0x0040,"Stats_TxAck",		"Count of number of ACKs transmitted.")
#define awc_RID_Stats_TxRts 		awc_def_Stats_RID(0x0022,0x0044,"Stats_TxRts",		"Count of number of RTSs transmitted.")
#define awc_RID_Stats_TxCts 		awc_def_Stats_RID(0x0024,0x0048,"Stats_TxCts",		"Count of number of CTSs transmitted.")
#define awc_RID_Stats_TxMc 		awc_def_Stats_RID(0x0026,0x004C,"Stats_TxMc",		" LMAC count of multicast packets sent (uses 802.11  Address1).")
#define awc_RID_Stats_TxBc 		awc_def_Stats_RID(0x0028,0x0050,"Stats_TxBc",		" LMAC count of broadcast packets sent (uses 802.11")
#define awc_RID_Stats_TxUcFrags 	awc_def_Stats_RID(0x002A,0x0054,"Stats_TxUcFragsLMAC",	" count of ALL unicast fragments and whole packets sent (uses 802.11 Address1).")
#define awc_RID_Stats_TxUcPackets 	awc_def_Stats_RID(0x002C,0x0058,"Stats_TxUcPackets",	"LMAC count of unicast packets that were ACKd (uses   802.11 Address 1).")
#define awc_RID_Stats_TxBeacon 		awc_def_Stats_RID(0x002E,0x005C,"Stats_TxBeacon",	" Count of beacon packets transmitted.")
#define awc_RID_Stats_RxBeacon 		awc_def_Stats_RID(0x0030,0x0060,"Stats_RxBeacon",	" Count of beacon packets received matching our BSSID.")
#define awc_RID_Stats_TxSinColl 	awc_def_Stats_RID(0x0032,0x0064,"Stats_TxSinCollTransmit"," single collisions. **")
#define awc_RID_Stats_TxMulColl 	awc_def_Stats_RID(0x0034,0x0068,"Stats_TxMulCollTransmit"," multiple collisions. **")
#define awc_RID_Stats_DefersNo 		awc_def_Stats_RID(0x0036,0x006C,"Stats_DefersNo Transmit"," frames sent with no deferral. **")
#define awc_RID_Stats_DefersProt 	awc_def_Stats_RID(0x0038,0x0070,"Stats_DefersProt",	" Transmit frames deferred due to protocol.")
#define awc_RID_Stats_DefersEngy 	awc_def_Stats_RID(0x003A,0x0074,"Stats_DefersEngy",	" Transmit frames deferred due to energy detect.")
#define awc_RID_Stats_DupFram 		awc_def_Stats_RID(0x003C,0x0078,"Stats_DupFram",	"  Duplicate receive frames and fragments.")
#define awc_RID_Stats_RxFragDisc 	awc_def_Stats_RID(0x003E,0x007C,"Stats_RxFragDisc",	" Received partial frames. (each tally could indicate the  discarding of one or more fragments)")
#define awc_RID_Stats_TxAged 		awc_def_Stats_RID(0x0040,0x0080,"Stats_TxAged",		"   Transmit packets exceeding maximum transmit lifetime. **")
#define awc_RID_Stats_RxAged 		awc_def_Stats_RID(0x0042,0x0084,"Stats_RxAgedReceive",	" packets exceeding maximum receive lifetime. **")
#define awc_RID_Stats_LostSync_Max 	awc_def_Stats_RID(0x0044,0x0088,"Stats_LostSync_Max",	" Lost sync with our cell due to maximum retries occuring. Retry")
#define awc_RID_Stats_LostSync_Mis 	awc_def_Stats_RID(0x0046,0x008C,"Stats_LostSync_Mis",	"Lost sync with our cell due to missing too many beacons. sedBeacons")
#define awc_RID_Stats_LostSync_Arl 	awc_def_Stats_RID(0x0048,0x0090,"Stats_LostSync_Arl",	"Lost sync with our cell due to Average Retry Level being  Exceeded  exceeded.")
#define awc_RID_Stats_LostSync_Dea 	awc_def_Stats_RID(0x004A,0x0094,"Stats_LostSync_Dea",	"Lost sync with our cell due to being deauthenticated.,thed")
#define awc_RID_Stats_LostSync_Disa 	awc_def_Stats_RID(0x004C,0x0098,"Stats_LostSync_Disa",	" Lost sync with our cell due to being disassociated. ssoced")
#define awc_RID_Stats_LostSync_Tsf 	awc_def_Stats_RID(0x004E,0x009C,"Stats_LostSync_Tsf",	"Lost sync with our cell due to excessive change in TSF  Timingtiming.")
#define awc_RID_Stats_HostTxMc 		awc_def_Stats_RID(0x0050,0x00A0,"Stats_HostTxMc",	"Count of multicast packets sent by the host.")
#define awc_RID_Stats_HostTxBc 		awc_def_Stats_RID(0x0052,0x00A4,"Stats_HostTxBc",	"Count of broadcast packets sent by the host.")
#define awc_RID_Stats_HostTxUc 		awc_def_Stats_RID(0x0054,0x00A8,"Stats_HostTxUc",	"Count of unicast packets sent by the host.")
#define awc_RID_Stats_HostTxFail 	awc_def_Stats_RID(0x0056,0x00AC,"Stats_HostTxFail",	"  Count of host transmitted packets which failed.")
#define awc_RID_Stats_HostRxMc 		awc_def_Stats_RID(0x0058,0x00B0,"Stats_HostRxMc",	"Count of host received multicast packets.")
#define awc_RID_Stats_HostRxBc 		awc_def_Stats_RID(0x005A,0x00B4,"Stats_HostRxBc",	"Count of host received broadcast packets.")
#define awc_RID_Stats_HostRxUc 		awc_def_Stats_RID(0x005C,0x00B8,"Stats_HostRxUc",	"Count of host received unicast packets.")
#define awc_RID_Stats_HostRxDiscar 	awc_def_Stats_RID(0x005E,0x00BC,"Stats_HostRxDiscar",	"Count of host received packets discarded due to:\n  Host not enabling receive.\n  Host failing to dequeue receive packets quickly.\n Packets being discarded due to magic packet mode.")
#define awc_RID_Stats_HmacTxMc 		awc_def_Stats_RID(0x0060,0x00C0,"Stats_HmacTxMc",	"Count of internally generated multicast (DA) packets.")
#define awc_RID_Stats_HmacTxBc 		awc_def_Stats_RID(0x0062,0x00C4,"Stats_HmacTxBc",	"Count of internally generated broadcast (DA) packets.")
#define awc_RID_Stats_HmacTxUc 		awc_def_Stats_RID(0x0064,0x00C8,"Stats_HmacTxUc",	"Count of internally generated unicast (DA) packets.")
#define awc_RID_Stats_HmacTxFail 	awc_def_Stats_RID(0x0066,0x00CC,"Stats_HmacTxFail",	"  Count of internally generated transmit packets that failed.")
#define awc_RID_Stats_HmacRxMc 		awc_def_Stats_RID(0x0068,0x00D0,"Stats_HmacRxMc",	"Count of internally received multicast (DA) packets.")
#define awc_RID_Stats_HmacRxBc 		awc_def_Stats_RID(0x006A,0x00D4,"Stats_HmacRxBc",	"Count of internally received broadcast (DA) packets.")
#define awc_RID_Stats_HmacRxUc 		awc_def_Stats_RID(0x006C,0x00D8,"Stats_HmacRxUc",	"Count of internally received multicast (DA) packets.")
#define awc_RID_Stats_HmacRxDisca 	awc_def_Stats_RID(0x006E,0x00DC,"Stats_HmacRxDisca",	" Count of internally received packets that were discarded  (usually because the destination address is not for the host).")
#define awc_RID_Stats_HmacRxAcce 	awc_def_Stats_RID(0x0070,0x00E0,"Stats_HmacRxAcce",	"  Count of internally received packets that were accepted")
#define awc_RID_Stats_SsidMismatch 	awc_def_Stats_RID(0x0072,0x00E4,"Stats_SsidMismatch",	" Count of SSID mismatches.")
#define awc_RID_Stats_ApMismatch 	awc_def_Stats_RID(0x0074,0x00E8,"Stats_ApMismatch",	"  Count of specified AP mismatches.")
#define awc_RID_Stats_RatesMismatc 	awc_def_Stats_RID(0x0076,0x00EC,"Stats_RatesMismatc",	" Count of rate mismatches.")
#define awc_RID_Stats_AuthReject 	awc_def_Stats_RID(0x0078,0x00F0,"Stats_AuthReject",	"  Count of authentication rejections.")
#define awc_RID_Stats_AuthTimeout 	awc_def_Stats_RID(0x007A,0x00F4,"Stats_AuthTimeout",	" Count of authentication timeouts.")
#define awc_RID_Stats_AssocReject 	awc_def_Stats_RID(0x007C,0x00F8,"Stats_AssocReject",	" Count of association rejections.")
#define awc_RID_Stats_AssocTimeout 	awc_def_Stats_RID(0x007E,0x00FC,"Stats_AssocTimeout",	" Count of association timeouts.")
#define awc_RID_Stats_NewReason 	awc_def_Stats_RID(0x0080,0x0100,"Stats_NewReason",	"Count of reason/status codes of greater than 19.  (Values of 0 = successful are not counted)")
#define awc_RID_Stats_AuthFail_1 	awc_def_Stats_RID(0x0082,0x0104,"Stats_AuthFail_1",	"Unspecified reason.")
#define awc_RID_Stats_AuthFail_2 	awc_def_Stats_RID(0x0084,0x0108,"Stats_AuthFail_2",	"Previous authentication no longer valid.")
#define awc_RID_Stats_AuthFail_3 	awc_def_Stats_RID(0x0086,0x010C,"Stats_AuthFail_3",	"Deauthenticated because sending station is leaving (has left) IBSS or ESS.")
#define awc_RID_Stats_AuthFail_4 	awc_def_Stats_RID(0x0088,0x0110,"Stats_AuthFail_4",	"Disassociated due to inactivity")
#define awc_RID_Stats_AuthFail_5 	awc_def_Stats_RID(0x008A,0x0114,"Stats_AuthFail_5",	"Disassociated because AP is unable to handle all currently  associated stations.")
#define awc_RID_Stats_AuthFail_6 	awc_def_Stats_RID(0x008C,0x0118,"Stats_AuthFail_6",	"Class 2 Frame received from non-Authenticated station.")
#define awc_RID_Stats_AuthFail_7 	awc_def_Stats_RID(0x008E,0x011C,"Stats_AuthFail_7",	"Class 3 Frame received from non-Associated station.")
#define awc_RID_Stats_AuthFail_8 	awc_def_Stats_RID(0x0090,0x0120,"Stats_AuthFail_8",	"Disassociated because sending station is leaving (has left)")
#define awc_RID_Stats_AuthFail_9 	awc_def_Stats_RID(0x0092,0x0124,"Stats_AuthFail_9",	"Station requesting (Re)Association is not Authenticated")
#define awc_RID_Stats_AuthFail_10 	awc_def_Stats_RID(0x0094,0x0128,"Stats_AuthFail_10",	"Cannot support all requested capabilities in the Capability")
#define awc_RID_Stats_AuthFail_11 	awc_def_Stats_RID(0x0096,0x012C,"Stats_AuthFail_11",	"Reassociation denied due to inability to confirm")
#define awc_RID_Stats_AuthFail_12 	awc_def_Stats_RID(0x0098,0x0130,"Stats_AuthFail_12",	"Association denied due to reason outside the scope of the 802.11")
#define awc_RID_Stats_AuthFail_13 	awc_def_Stats_RID(0x009A,0x0134,"Stats_AuthFail_13",	"Responding station does not support the specified Auth Alogorithm")
#define awc_RID_Stats_AuthFail_14 	awc_def_Stats_RID(0x009C,0x0138,"Stats_AuthFail_14",	"Received an out of sequence Authentication Frame.")
#define awc_RID_Stats_AuthFail_15 	awc_def_Stats_RID(0x009E,0x013C,"Stats_AuthFail_15",	"Authentication rejected due to challenge failure.")
#define awc_RID_Stats_AuthFail_16 	awc_def_Stats_RID(0x00A0,0x0140,"Stats_AuthFail_16",	"Authentication rejected due to timeout waiting for next  frame in sequence.")
#define awc_RID_Stats_AuthFail_17 	awc_def_Stats_RID(0x00A2,0x0144,"Stats_AuthFail_17",	"Association denied because AP is unable to handle  additional associated stations.")
#define awc_RID_Stats_AuthFail_18 	awc_def_Stats_RID(0x00A4,0x0148,"Stats_AuthFail_18",	"Association denied due to requesting station not supportingall basic rates.")
#define awc_RID_Stats_AuthFail_19 	awc_def_Stats_RID(0x00A6,0x014C,"Stats_AuthFail_19",	"Reserved")
#define awc_RID_Stats_RxMan 		awc_def_Stats_RID(0x00A8,0x0150,"Stats_RxMan",		" Count of management packets received and handled.")
#define awc_RID_Stats_TxMan 		awc_def_Stats_RID(0x00AA,0x0154,"Stats_TxMan",		" Count of management packets transmitted.")
#define awc_RID_Stats_RxRefresh 	awc_def_Stats_RID(0x00AC,0x0158,"Stats_RxRefresh",	" Count of null data packets received.")
#define awc_RID_Stats_TxRefresh 	awc_def_Stats_RID(0x00AE,0x015C,"Stats_TxRefresh",	" Count of null data packets transmitted.")
#define awc_RID_Stats_RxPoll 		awc_def_Stats_RID(0x00B0,0x0160,"Stats_RxPoll",		"Count of PS-Poll packets received.")
#define awc_RID_Stats_TxPoll 		awc_def_Stats_RID(0x00B2,0x0164,"Stats_TxPoll",		"Count of PS-Poll packets transmitted.")
#define awc_RID_Stats_HostRetries 	awc_def_Stats_RID(0x00B4,0x0168,"Stats_HostRetries",	" Count of long and short retries used to transmit host packets  (does not include first attempt).")
#define awc_RID_Stats_LostSync_HostReq 	awc_def_Stats_RID(0x00B6,0x016C,"Stats_LostSync_HostReq","Lost sync with our cell due to host request.")
#define awc_RID_Stats_HostTxBytes 	awc_def_Stats_RID(0x00B8,0x0170,"Stats_HostTxBytes",	" Count of bytes transferred from the host.")
#define awc_RID_Stats_HostRxBytes 	awc_def_Stats_RID(0x00BA,0x0174,"Stats_HostRxBytes",	" Count of bytes transferred to the host.")
#define awc_RID_Stats_ElapsedUsec 	awc_def_Stats_RID(0x00BC,0x0178,"Stats_ElapsedUsec",	" Total time since power up (or clear) in microseconds.")
#define awc_RID_Stats_ElapsedSec 	awc_def_Stats_RID(0x00BE,0x017C,"Stats_ElapsedSec",	" Total time since power up (or clear) in seconds.")
#define awc_RID_Stats_LostSyncBett 	awc_def_Stats_RID(0x00C0,0x0180,"Stats_LostSyncBett",	"Lost Sync to switch to a better access point")



#define awc_RID_Stats_delta_RidLen 		awc_def_Stats_delta_RID(0x0000,0x0000,"RidLen",		"Length of the RID including the length field.")
#define awc_RID_Stats_delta_RxOverrunErr 	awc_def_Stats_delta_RID(0x0002,0x0004,"Stats_RxOverrunErr",	"Receive overruns -- No buffer available to handle the receive. (result is that the packet is never received)")
#define awc_RID_Stats_delta_RxPlcpCrcErr 	awc_def_Stats_delta_RID(0x0004,0x0008,"Stats_RxPlcpCrcErr",	"PLCP header checksum errors (CRC16).")
#define awc_RID_Stats_delta_RxPlcpFormat 	awc_def_Stats_delta_RID(0x0006,0x000C,"Stats_RxPlcpFormat",	"PLCP format errors.")
#define awc_RID_Stats_delta_RxPlcpLength 	awc_def_Stats_delta_RID(0x0008,0x0010,"Stats_RxPlcpLength",	"PLCP length is incorrect.")
#define awc_RID_Stats_delta_RxMacCrcErr 	awc_def_Stats_delta_RID(0x000A,0x0014,"Stats_RxMacCrcErr",	"Count of MAC CRC32 errors.")
#define awc_RID_Stats_delta_RxMacCrcOk 		awc_def_Stats_delta_RID(0x000C,0x0018,"Stats_RxMacCrcOk",	"Count of MAC CRC32 received correctly.")
#define awc_RID_Stats_delta_RxWepErr 		awc_def_Stats_delta_RID(0x000E,0x001C,"Stats_RxWepErr",	"Count of all WEP ICV checks that failed. (this value is included in Stats_RxMacCrcOk)")
#define awc_RID_Stats_delta_RxWepOk 		awc_def_Stats_delta_RID(0x0010,0x0020,"Stats_RxWepOk",	"Count of all WEP ICV checks that passed. (this value is  included in Stats_RxMacCrcOk)")
#define awc_RID_Stats_delta_RetryLong 		awc_def_Stats_delta_RID(0x0012,0x0024,"Stats_RetryLongCount",	"of all long retries. (Does not include first attempt for a packet).")
#define awc_RID_Stats_delta_RetryShort 		awc_def_Stats_delta_RID(0x0014,0x0028,"Stats_RetryShort",	"Count of all short retries. (Does not include first attempt for   a packet).")
#define awc_RID_Stats_delta_MaxRetries 		awc_def_Stats_delta_RID(0x0016,0x002C,"Stats_MaxRetries",	"Count of number of packets that max-retried -- ie were  never ACKd.")
#define awc_RID_Stats_delta_NoAck 		awc_def_Stats_delta_RID(0x0018,0x0030,"Stats_NoAck",		"Count of number of times that ACK was not received.")
#define awc_RID_Stats_delta_NoCts 		awc_def_Stats_delta_RID(0x001A,0x0034,"Stats_NoCts",		"Count of number of timer that CTS was not received.")
#define awc_RID_Stats_delta_RxAck 		awc_def_Stats_delta_RID(0x001C,0x0038,"Stats_RxAck",		"Count of number of expected ACKs that were received.")
#define awc_RID_Stats_delta_RxCts 		awc_def_Stats_delta_RID(0x001E,0x003C,"Stats_RxCts",		"Count of number of expected CTSs that were received.")
#define awc_RID_Stats_delta_TxAck 		awc_def_Stats_delta_RID(0x0020,0x0040,"Stats_TxAck",		"Count of number of ACKs transmitted.")
#define awc_RID_Stats_delta_TxRts 		awc_def_Stats_delta_RID(0x0022,0x0044,"Stats_TxRts",		"Count of number of RTSs transmitted.")
#define awc_RID_Stats_delta_TxCts 		awc_def_Stats_delta_RID(0x0024,0x0048,"Stats_TxCts",		"Count of number of CTSs transmitted.")
#define awc_RID_Stats_delta_TxMc 		awc_def_Stats_delta_RID(0x0026,0x004C,"Stats_TxMc",		" LMAC count of multicast packets sent (uses 802.11  Address1).")
#define awc_RID_Stats_delta_TxBc 		awc_def_Stats_delta_RID(0x0028,0x0050,"Stats_TxBc",		" LMAC count of broadcast packets sent (uses 802.11")
#define awc_RID_Stats_delta_TxUcFrags 		awc_def_Stats_delta_RID(0x002A,0x0054,"Stats_TxUcFragsLMAC",	" count of ALL unicast fragments and whole packets sent (uses 802.11 Address1).")
#define awc_RID_Stats_delta_TxUcPackets 	awc_def_Stats_delta_RID(0x002C,0x0058,"Stats_TxUcPackets",	"LMAC count of unicast packets that were ACKd (uses   802.11 Address 1).")
#define awc_RID_Stats_delta_TxBeacon 		awc_def_Stats_delta_RID(0x002E,0x005C,"Stats_TxBeacon",	" Count of beacon packets transmitted.")
#define awc_RID_Stats_delta_RxBeacon 		awc_def_Stats_delta_RID(0x0030,0x0060,"Stats_RxBeacon",	" Count of beacon packets received matching our BSSID.")
#define awc_RID_Stats_delta_TxSinColl 		awc_def_Stats_delta_RID(0x0032,0x0064,"Stats_TxSinCollTransmit"," single collisions. **")
#define awc_RID_Stats_delta_TxMulColl 		awc_def_Stats_delta_RID(0x0034,0x0068,"Stats_TxMulCollTransmit"," multiple collisions. **")
#define awc_RID_Stats_delta_DefersNo 		awc_def_Stats_delta_RID(0x0036,0x006C,"Stats_DefersNo Transmit"," frames sent with no deferral. **")
#define awc_RID_Stats_delta_DefersProt 		awc_def_Stats_delta_RID(0x0038,0x0070,"Stats_DefersProt",	" Transmit frames deferred due to protocol.")
#define awc_RID_Stats_delta_DefersEngy 		awc_def_Stats_delta_RID(0x003A,0x0074,"Stats_DefersEngy",	" Transmit frames deferred due to energy detect.")
#define awc_RID_Stats_delta_DupFram 		awc_def_Stats_delta_RID(0x003C,0x0078,"Stats_DupFram",	"  Duplicate receive frames and fragments.")
#define awc_RID_Stats_delta_RxFragDisc 		awc_def_Stats_delta_RID(0x003E,0x007C,"Stats_RxFragDisc",	" Received partial frames. (each tally could indicate the  discarding of one or more fragments)")
#define awc_RID_Stats_delta_TxAged 		awc_def_Stats_delta_RID(0x0040,0x0080,"Stats_TxAged",		"   Transmit packets exceeding maximum transmit lifetime. **")
#define awc_RID_Stats_delta_RxAged 		awc_def_Stats_delta_RID(0x0042,0x0084,"Stats_RxAgedReceive",	" packets exceeding maximum receive lifetime. **")
#define awc_RID_Stats_delta_LostSync_Max 	awc_def_Stats_delta_RID(0x0044,0x0088,"Stats_LostSync_Max",	" Lost sync with our cell due to maximum retries occuring. Retry")
#define awc_RID_Stats_delta_LostSync_Mis 	awc_def_Stats_delta_RID(0x0046,0x008C,"Stats_LostSync_Mis",	"Lost sync with our cell due to missing too many beacons. sedBeacons")
#define awc_RID_Stats_delta_LostSync_Arl 	awc_def_Stats_delta_RID(0x0048,0x0090,"Stats_LostSync_Arl",	"Lost sync with our cell due to Average Retry Level being  Exceeded  exceeded.")
#define awc_RID_Stats_delta_LostSync_Dea 	awc_def_Stats_delta_RID(0x004A,0x0094,"Stats_LostSync_Dea",	"Lost sync with our cell due to being deauthenticated.,thed")
#define awc_RID_Stats_delta_LostSync_Disa 	awc_def_Stats_delta_RID(0x004C,0x0098,"Stats_LostSync_Disa",	" Lost sync with our cell due to being disassociated. ssoced")
#define awc_RID_Stats_delta_LostSync_Tsf 	awc_def_Stats_delta_RID(0x004E,0x009C,"Stats_LostSync_Tsf",	"Lost sync with our cell due to excessive change in TSF  Timingtiming.")
#define awc_RID_Stats_delta_HostTxMc 		awc_def_Stats_delta_RID(0x0050,0x00A0,"Stats_HostTxMc",	"Count of multicast packets sent by the host.")
#define awc_RID_Stats_delta_HostTxBc 		awc_def_Stats_delta_RID(0x0052,0x00A4,"Stats_HostTxBc",	"Count of broadcast packets sent by the host.")
#define awc_RID_Stats_delta_HostTxUc 		awc_def_Stats_delta_RID(0x0054,0x00A8,"Stats_HostTxUc",	"Count of unicast packets sent by the host.")
#define awc_RID_Stats_delta_HostTxFail 		awc_def_Stats_delta_RID(0x0056,0x00AC,"Stats_HostTxFail",	"  Count of host transmitted packets which failed.")
#define awc_RID_Stats_delta_HostRxMc 		awc_def_Stats_delta_RID(0x0058,0x00B0,"Stats_HostRxMc",	"Count of host received multicast packets.")
#define awc_RID_Stats_delta_HostRxBc 		awc_def_Stats_delta_RID(0x005A,0x00B4,"Stats_HostRxBc",	"Count of host received broadcast packets.")
#define awc_RID_Stats_delta_HostRxUc 		awc_def_Stats_delta_RID(0x005C,0x00B8,"Stats_HostRxUc",	"Count of host received unicast packets.")
#define awc_RID_Stats_delta_HostRxDiscar 	awc_def_Stats_delta_RID(0x005E,0x00BC,"Stats_HostRxDiscar",	"Count of host received packets discarded due to:\n  Host not enabling receive.\n  Host failing to dequeue receive packets quickly.\n Packets being discarded due to magic packet mode.")
#define awc_RID_Stats_delta_HmacTxMc 		awc_def_Stats_delta_RID(0x0060,0x00C0,"Stats_HmacTxMc",	"Count of internally generated multicast (DA) packets.")
#define awc_RID_Stats_delta_HmacTxBc 		awc_def_Stats_delta_RID(0x0062,0x00C4,"Stats_HmacTxBc",	"Count of internally generated broadcast (DA) packets.")
#define awc_RID_Stats_delta_HmacTxUc 		awc_def_Stats_delta_RID(0x0064,0x00C8,"Stats_HmacTxUc",	"Count of internally generated unicast (DA) packets.")
#define awc_RID_Stats_delta_HmacTxFail 		awc_def_Stats_delta_RID(0x0066,0x00CC,"Stats_HmacTxFail",	"  Count of internally generated transmit packets that failed.")
#define awc_RID_Stats_delta_HmacRxMc 		awc_def_Stats_delta_RID(0x0068,0x00D0,"Stats_HmacRxMc",	"Count of internally received multicast (DA) packets.")
#define awc_RID_Stats_delta_HmacRxBc 		awc_def_Stats_delta_RID(0x006A,0x00D4,"Stats_HmacRxBc",	"Count of internally received broadcast (DA) packets.")
#define awc_RID_Stats_delta_HmacRxUc 		awc_def_Stats_delta_RID(0x006C,0x00D8,"Stats_HmacRxUc",	"Count of internally received multicast (DA) packets.")
#define awc_RID_Stats_delta_HmacRxDisca 	awc_def_Stats_delta_RID(0x006E,0x00DC,"Stats_HmacRxDisca",	" Count of internally received packets that were discarded  (usually because the destination address is not for the host).")
#define awc_RID_Stats_delta_HmacRxAcce 		awc_def_Stats_delta_RID(0x0070,0x00E0,"Stats_HmacRxAcce",	"  Count of internally received packets that were accepted")
#define awc_RID_Stats_delta_SsidMismatch 	awc_def_Stats_delta_RID(0x0072,0x00E4,"Stats_SsidMismatch",	" Count of SSID mismatches.")
#define awc_RID_Stats_delta_ApMismatch 		awc_def_Stats_delta_RID(0x0074,0x00E8,"Stats_ApMismatch",	"  Count of specified AP mismatches.")
#define awc_RID_Stats_delta_RatesMismatc 	awc_def_Stats_delta_RID(0x0076,0x00EC,"Stats_RatesMismatc",	" Count of rate mismatches.")
#define awc_RID_Stats_delta_AuthReject 		awc_def_Stats_delta_RID(0x0078,0x00F0,"Stats_AuthReject",	"  Count of authentication rejections.")
#define awc_RID_Stats_delta_AuthTimeout 	awc_def_Stats_delta_RID(0x007A,0x00F4,"Stats_AuthTimeout",	" Count of authentication timeouts.")
#define awc_RID_Stats_delta_AssocReject 	awc_def_Stats_delta_RID(0x007C,0x00F8,"Stats_AssocReject",	" Count of association rejections.")
#define awc_RID_Stats_delta_AssocTimeout 	awc_def_Stats_delta_RID(0x007E,0x00FC,"Stats_AssocTimeout",	" Count of association timeouts.")
#define awc_RID_Stats_delta_NewReason 		awc_def_Stats_delta_RID(0x0080,0x0100,"Stats_NewReason",	"Count of reason/status codes of greater than 19.  (Values of 0 = successful are not counted)")
#define awc_RID_Stats_delta_AuthFail_1 		awc_def_Stats_delta_RID(0x0082,0x0104,"Stats_AuthFail_1",	"Unspecified reason.")
#define awc_RID_Stats_delta_AuthFail_2 		awc_def_Stats_delta_RID(0x0084,0x0108,"Stats_AuthFail_2",	"Previous authentication no longer valid.")
#define awc_RID_Stats_delta_AuthFail_3 		awc_def_Stats_delta_RID(0x0086,0x010C,"Stats_AuthFail_3",	"Deauthenticated because sending station is leaving (has left) IBSS or ESS.")
#define awc_RID_Stats_delta_AuthFail_4 		awc_def_Stats_delta_RID(0x0088,0x0110,"Stats_AuthFail_4",	"Disassociated due to inactivity")
#define awc_RID_Stats_delta_AuthFail_5 		awc_def_Stats_delta_RID(0x008A,0x0114,"Stats_AuthFail_5",	"Disassociated because AP is unable to handle all currently  associated stations.")
#define awc_RID_Stats_delta_AuthFail_6 		awc_def_Stats_delta_RID(0x008C,0x0118,"Stats_AuthFail_6",	"Class 2 Frame received from non-Authenticated station.")
#define awc_RID_Stats_delta_AuthFail_7 		awc_def_Stats_delta_RID(0x008E,0x011C,"Stats_AuthFail_7",	"Class 3 Frame received from non-Associated station.")
#define awc_RID_Stats_delta_AuthFail_8 		awc_def_Stats_delta_RID(0x0090,0x0120,"Stats_AuthFail_8",	"Disassociated because sending station is leaving (has left)")
#define awc_RID_Stats_delta_AuthFail_9 		awc_def_Stats_delta_RID(0x0092,0x0124,"Stats_AuthFail_9",	"Station requesting (Re)Association is not Authenticated")
#define awc_RID_Stats_delta_AuthFail_10 	awc_def_Stats_delta_RID(0x0094,0x0128,"Stats_AuthFail_10",	"Cannot support all requested capabilities in the Capability")
#define awc_RID_Stats_delta_AuthFail_11 	awc_def_Stats_delta_RID(0x0096,0x012C,"Stats_AuthFail_11",	"Reassociation denied due to inability to confirm")
#define awc_RID_Stats_delta_AuthFail_12 	awc_def_Stats_delta_RID(0x0098,0x0130,"Stats_AuthFail_12",	"Association denied due to reason outside the scope of the 802.11")
#define awc_RID_Stats_delta_AuthFail_13 	awc_def_Stats_delta_RID(0x009A,0x0134,"Stats_AuthFail_13",	"Responding station does not support the specified Auth Alogorithm")
#define awc_RID_Stats_delta_AuthFail_14 	awc_def_Stats_delta_RID(0x009C,0x0138,"Stats_AuthFail_14",	"Received an out of sequence Authentication Frame.")
#define awc_RID_Stats_delta_AuthFail_15 	awc_def_Stats_delta_RID(0x009E,0x013C,"Stats_AuthFail_15",	"Authentication rejected due to challenge failure.")
#define awc_RID_Stats_delta_AuthFail_16 	awc_def_Stats_delta_RID(0x00A0,0x0140,"Stats_AuthFail_16",	"Authentication rejected due to timeout waiting for next  frame in sequence.")
#define awc_RID_Stats_delta_AuthFail_17 	awc_def_Stats_delta_RID(0x00A2,0x0144,"Stats_AuthFail_17",	"Association denied because AP is unable to handle  additional associated stations.")
#define awc_RID_Stats_delta_AuthFail_18 	awc_def_Stats_delta_RID(0x00A4,0x0148,"Stats_AuthFail_18",	"Association denied due to requesting station not supportingall basic rates.")
#define awc_RID_Stats_delta_AuthFail_19 	awc_def_Stats_delta_RID(0x00A6,0x014C,"Stats_AuthFail_19",	"Reserved")
#define awc_RID_Stats_delta_RxMan 		awc_def_Stats_delta_RID(0x00A8,0x0150,"Stats_RxMan",		" Count of management packets received and handled.")
#define awc_RID_Stats_delta_TxMan 		awc_def_Stats_delta_RID(0x00AA,0x0154,"Stats_TxMan",		" Count of management packets transmitted.")
#define awc_RID_Stats_delta_RxRefresh 		awc_def_Stats_delta_RID(0x00AC,0x0158,"Stats_RxRefresh",	" Count of null data packets received.")
#define awc_RID_Stats_delta_TxRefresh 		awc_def_Stats_delta_RID(0x00AE,0x015C,"Stats_TxRefresh",	" Count of null data packets transmitted.")
#define awc_RID_Stats_delta_RxPoll 		awc_def_Stats_delta_RID(0x00B0,0x0160,"Stats_RxPoll",		"Count of PS-Poll packets received.")
#define awc_RID_Stats_delta_TxPoll 		awc_def_Stats_delta_RID(0x00B2,0x0164,"Stats_TxPoll",		"Count of PS-Poll packets transmitted.")
#define awc_RID_Stats_delta_HostRetries 	awc_def_Stats_delta_RID(0x00B4,0x0168,"Stats_HostRetries",	" Count of long and short retries used to transmit host packets  (does not include first attempt).")
#define awc_RID_Stats_delta_LostSync_HostReq 	awc_def_Stats_delta_RID(0x00B6,0x016C,"Stats_LostSync_HostReq","Lost sync with our cell due to host request.")
#define awc_RID_Stats_delta_HostTxBytes 	awc_def_Stats_delta_RID(0x00B8,0x0170,"Stats_HostTxBytes",	" Count of bytes transferred from the host.")
#define awc_RID_Stats_delta_HostRxBytes 	awc_def_Stats_delta_RID(0x00BA,0x0174,"Stats_HostRxBytes",	" Count of bytes transferred to the host.")
#define awc_RID_Stats_delta_ElapsedUsec 	awc_def_Stats_delta_RID(0x00BC,0x0178,"Stats_ElapsedUsec",	" Total time since power up (or clear) in microseconds.")
#define awc_RID_Stats_delta_ElapsedSec 		awc_def_Stats_delta_RID(0x00BE,0x017C,"Stats_ElapsedSec",	" Total time since power up (or clear) in seconds.")
#define awc_RID_Stats_delta_LostSyncBett 	awc_def_Stats_delta_RID(0x00C0,0x0180,"Stats_LostSyncBett",	"Lost Sync to switch to a better access point")



#define awc_RID_Stats_clear_RidLen 		awc_def_Stats_clear_RID(0x0000,0x0000,"RidLen",		"Length of the RID including the length field.")
#define awc_RID_Stats_clear_RxOverrunErr 	awc_def_Stats_clear_RID(0x0002,0x0004,"Stats_RxOverrunErr",	"Receive overruns -- No buffer available to handle the receive. (result is that the packet is never received)")
#define awc_RID_Stats_clear_RxPlcpCrcErr 	awc_def_Stats_clear_RID(0x0004,0x0008,"Stats_RxPlcpCrcErr",	"PLCP header checksum errors (CRC16).")
#define awc_RID_Stats_clear_RxPlcpFormat 	awc_def_Stats_clear_RID(0x0006,0x000C,"Stats_RxPlcpFormat",	"PLCP format errors.")
#define awc_RID_Stats_clear_RxPlcpLength 	awc_def_Stats_clear_RID(0x0008,0x0010,"Stats_RxPlcpLength",	"PLCP length is incorrect.")
#define awc_RID_Stats_clear_RxMacCrcErr 	awc_def_Stats_clear_RID(0x000A,0x0014,"Stats_RxMacCrcErr",	"Count of MAC CRC32 errors.")
#define awc_RID_Stats_clear_RxMacCrcOk 		awc_def_Stats_clear_RID(0x000C,0x0018,"Stats_RxMacCrcOk",	"Count of MAC CRC32 received correctly.")
#define awc_RID_Stats_clear_RxWepErr 		awc_def_Stats_clear_RID(0x000E,0x001C,"Stats_RxWepErr",	"Count of all WEP ICV checks that failed. (this value is included in Stats_RxMacCrcOk)")
#define awc_RID_Stats_clear_RxWepOk 		awc_def_Stats_clear_RID(0x0010,0x0020,"Stats_RxWepOk",	"Count of all WEP ICV checks that passed. (this value is  included in Stats_RxMacCrcOk)")
#define awc_RID_Stats_clear_RetryLong 		awc_def_Stats_clear_RID(0x0012,0x0024,"Stats_RetryLongCount",	"of all long retries. (Does not include first attempt for a packet).")
#define awc_RID_Stats_clear_RetryShort 		awc_def_Stats_clear_RID(0x0014,0x0028,"Stats_RetryShort",	"Count of all short retries. (Does not include first attempt for   a packet).")
#define awc_RID_Stats_clear_MaxRetries 		awc_def_Stats_clear_RID(0x0016,0x002C,"Stats_MaxRetries",	"Count of number of packets that max-retried -- ie were  never ACKd.")
#define awc_RID_Stats_clear_NoAck 		awc_def_Stats_clear_RID(0x0018,0x0030,"Stats_NoAck",		"Count of number of times that ACK was not received.")
#define awc_RID_Stats_clear_NoCts 		awc_def_Stats_clear_RID(0x001A,0x0034,"Stats_NoCts",		"Count of number of timer that CTS was not received.")
#define awc_RID_Stats_clear_RxAck 		awc_def_Stats_clear_RID(0x001C,0x0038,"Stats_RxAck",		"Count of number of expected ACKs that were received.")
#define awc_RID_Stats_clear_RxCts 		awc_def_Stats_clear_RID(0x001E,0x003C,"Stats_RxCts",		"Count of number of expected CTSs that were received.")
#define awc_RID_Stats_clear_TxAck 		awc_def_Stats_clear_RID(0x0020,0x0040,"Stats_TxAck",		"Count of number of ACKs transmitted.")
#define awc_RID_Stats_clear_TxRts 		awc_def_Stats_clear_RID(0x0022,0x0044,"Stats_TxRts",		"Count of number of RTSs transmitted.")
#define awc_RID_Stats_clear_TxCts 		awc_def_Stats_clear_RID(0x0024,0x0048,"Stats_TxCts",		"Count of number of CTSs transmitted.")
#define awc_RID_Stats_clear_TxMc 		awc_def_Stats_clear_RID(0x0026,0x004C,"Stats_TxMc",		" LMAC count of multicast packets sent (uses 802.11  Address1).")
#define awc_RID_Stats_clear_TxBc 		awc_def_Stats_clear_RID(0x0028,0x0050,"Stats_TxBc",		" LMAC count of broadcast packets sent (uses 802.11")
#define awc_RID_Stats_clear_TxUcFrags 		awc_def_Stats_clear_RID(0x002A,0x0054,"Stats_TxUcFragsLMAC",	" count of ALL unicast fragments and whole packets sent (uses 802.11 Address1).")
#define awc_RID_Stats_clear_TxUcPackets 	awc_def_Stats_clear_RID(0x002C,0x0058,"Stats_TxUcPackets",	"LMAC count of unicast packets that were ACKd (uses   802.11 Address 1).")
#define awc_RID_Stats_clear_TxBeacon 		awc_def_Stats_clear_RID(0x002E,0x005C,"Stats_TxBeacon",	" Count of beacon packets transmitted.")
#define awc_RID_Stats_clear_RxBeacon 		awc_def_Stats_clear_RID(0x0030,0x0060,"Stats_RxBeacon",	" Count of beacon packets received matching our BSSID.")
#define awc_RID_Stats_clear_TxSinColl 		awc_def_Stats_clear_RID(0x0032,0x0064,"Stats_TxSinCollTransmit"," single collisions. **")
#define awc_RID_Stats_clear_TxMulColl 		awc_def_Stats_clear_RID(0x0034,0x0068,"Stats_TxMulCollTransmit"," multiple collisions. **")
#define awc_RID_Stats_clear_DefersNo 		awc_def_Stats_clear_RID(0x0036,0x006C,"Stats_DefersNo Transmit"," frames sent with no deferral. **")
#define awc_RID_Stats_clear_DefersProt 		awc_def_Stats_clear_RID(0x0038,0x0070,"Stats_DefersProt",	" Transmit frames deferred due to protocol.")
#define awc_RID_Stats_clear_DefersEngy 		awc_def_Stats_clear_RID(0x003A,0x0074,"Stats_DefersEngy",	" Transmit frames deferred due to energy detect.")
#define awc_RID_Stats_clear_DupFram 		awc_def_Stats_clear_RID(0x003C,0x0078,"Stats_DupFram",	"  Duplicate receive frames and fragments.")
#define awc_RID_Stats_clear_RxFragDisc 		awc_def_Stats_clear_RID(0x003E,0x007C,"Stats_RxFragDisc",	" Received partial frames. (each tally could indicate the  discarding of one or more fragments)")
#define awc_RID_Stats_clear_TxAged 		awc_def_Stats_clear_RID(0x0040,0x0080,"Stats_TxAged",		"   Transmit packets exceeding maximum transmit lifetime. **")
#define awc_RID_Stats_clear_RxAged 		awc_def_Stats_clear_RID(0x0042,0x0084,"Stats_RxAgedReceive",	" packets exceeding maximum receive lifetime. **")
#define awc_RID_Stats_clear_LostSync_Max 	awc_def_Stats_clear_RID(0x0044,0x0088,"Stats_LostSync_Max",	" Lost sync with our cell due to maximum retries occuring. Retry")
#define awc_RID_Stats_clear_LostSync_Mis 	awc_def_Stats_clear_RID(0x0046,0x008C,"Stats_LostSync_Mis",	"Lost sync with our cell due to missing too many beacons. sedBeacons")
#define awc_RID_Stats_clear_LostSync_Arl 	awc_def_Stats_clear_RID(0x0048,0x0090,"Stats_LostSync_Arl",	"Lost sync with our cell due to Average Retry Level being  Exceeded  exceeded.")
#define awc_RID_Stats_clear_LostSync_Dea 	awc_def_Stats_clear_RID(0x004A,0x0094,"Stats_LostSync_Dea",	"Lost sync with our cell due to being deauthenticated.,thed")
#define awc_RID_Stats_clear_LostSync_Disa 	awc_def_Stats_clear_RID(0x004C,0x0098,"Stats_LostSync_Disa",	" Lost sync with our cell due to being disassociated. ssoced")
#define awc_RID_Stats_clear_LostSync_Tsf 	awc_def_Stats_clear_RID(0x004E,0x009C,"Stats_LostSync_Tsf",	"Lost sync with our cell due to excessive change in TSF  Timingtiming.")
#define awc_RID_Stats_clear_HostTxMc 		awc_def_Stats_clear_RID(0x0050,0x00A0,"Stats_HostTxMc",	"Count of multicast packets sent by the host.")
#define awc_RID_Stats_clear_HostTxBc 		awc_def_Stats_clear_RID(0x0052,0x00A4,"Stats_HostTxBc",	"Count of broadcast packets sent by the host.")
#define awc_RID_Stats_clear_HostTxUc 		awc_def_Stats_clear_RID(0x0054,0x00A8,"Stats_HostTxUc",	"Count of unicast packets sent by the host.")
#define awc_RID_Stats_clear_HostTxFail 		awc_def_Stats_clear_RID(0x0056,0x00AC,"Stats_HostTxFail",	"  Count of host transmitted packets which failed.")
#define awc_RID_Stats_clear_HostRxMc 		awc_def_Stats_clear_RID(0x0058,0x00B0,"Stats_HostRxMc",	"Count of host received multicast packets.")
#define awc_RID_Stats_clear_HostRxBc 		awc_def_Stats_clear_RID(0x005A,0x00B4,"Stats_HostRxBc",	"Count of host received broadcast packets.")
#define awc_RID_Stats_clear_HostRxUc 		awc_def_Stats_clear_RID(0x005C,0x00B8,"Stats_HostRxUc",	"Count of host received unicast packets.")
#define awc_RID_Stats_clear_HostRxDiscar 	awc_def_Stats_clear_RID(0x005E,0x00BC,"Stats_HostRxDiscar",	"Count of host received packets discarded due to:\n  Host not enabling receive.\n  Host failing to dequeue receive packets quickly.\n Packets being discarded due to magic packet mode.")
#define awc_RID_Stats_clear_HmacTxMc 		awc_def_Stats_clear_RID(0x0060,0x00C0,"Stats_HmacTxMc",	"Count of internally generated multicast (DA) packets.")
#define awc_RID_Stats_clear_HmacTxBc 		awc_def_Stats_clear_RID(0x0062,0x00C4,"Stats_HmacTxBc",	"Count of internally generated broadcast (DA) packets.")
#define awc_RID_Stats_clear_HmacTxUc 		awc_def_Stats_clear_RID(0x0064,0x00C8,"Stats_HmacTxUc",	"Count of internally generated unicast (DA) packets.")
#define awc_RID_Stats_clear_HmacTxFail 		awc_def_Stats_clear_RID(0x0066,0x00CC,"Stats_HmacTxFail",	"  Count of internally generated transmit packets that failed.")
#define awc_RID_Stats_clear_HmacRxMc 		awc_def_Stats_clear_RID(0x0068,0x00D0,"Stats_HmacRxMc",	"Count of internally received multicast (DA) packets.")
#define awc_RID_Stats_clear_HmacRxBc 		awc_def_Stats_clear_RID(0x006A,0x00D4,"Stats_HmacRxBc",	"Count of internally received broadcast (DA) packets.")
#define awc_RID_Stats_clear_HmacRxUc 		awc_def_Stats_clear_RID(0x006C,0x00D8,"Stats_HmacRxUc",	"Count of internally received multicast (DA) packets.")
#define awc_RID_Stats_clear_HmacRxDisca 	awc_def_Stats_clear_RID(0x006E,0x00DC,"Stats_HmacRxDisca",	" Count of internally received packets that were discarded  (usually because the destination address is not for the host).")
#define awc_RID_Stats_clear_HmacRxAcce 		awc_def_Stats_clear_RID(0x0070,0x00E0,"Stats_HmacRxAcce",	"  Count of internally received packets that were accepted")
#define awc_RID_Stats_clear_SsidMismatch 	awc_def_Stats_clear_RID(0x0072,0x00E4,"Stats_SsidMismatch",	" Count of SSID mismatches.")
#define awc_RID_Stats_clear_ApMismatch 		awc_def_Stats_clear_RID(0x0074,0x00E8,"Stats_ApMismatch",	"  Count of specified AP mismatches.")
#define awc_RID_Stats_clear_RatesMismatc 	awc_def_Stats_clear_RID(0x0076,0x00EC,"Stats_RatesMismatc",	" Count of rate mismatches.")
#define awc_RID_Stats_clear_AuthReject 		awc_def_Stats_clear_RID(0x0078,0x00F0,"Stats_AuthReject",	"  Count of authentication rejections.")
#define awc_RID_Stats_clear_AuthTimeout 	awc_def_Stats_clear_RID(0x007A,0x00F4,"Stats_AuthTimeout",	" Count of authentication timeouts.")
#define awc_RID_Stats_clear_AssocReject 	awc_def_Stats_clear_RID(0x007C,0x00F8,"Stats_AssocReject",	" Count of association rejections.")
#define awc_RID_Stats_clear_AssocTimeout 	awc_def_Stats_clear_RID(0x007E,0x00FC,"Stats_AssocTimeout",	" Count of association timeouts.")
#define awc_RID_Stats_clear_NewReason 		awc_def_Stats_clear_RID(0x0080,0x0100,"Stats_NewReason",	"Count of reason/status codes of greater than 19.  (Values of 0 = successful are not counted)")
#define awc_RID_Stats_clear_AuthFail_1 		awc_def_Stats_clear_RID(0x0082,0x0104,"Stats_AuthFail_1",	"Unspecified reason.")
#define awc_RID_Stats_clear_AuthFail_2 		awc_def_Stats_clear_RID(0x0084,0x0108,"Stats_AuthFail_2",	"Previous authentication no longer valid.")
#define awc_RID_Stats_clear_AuthFail_3 		awc_def_Stats_clear_RID(0x0086,0x010C,"Stats_AuthFail_3",	"Deauthenticated because sending station is leaving (has left) IBSS or ESS.")
#define awc_RID_Stats_clear_AuthFail_4 		awc_def_Stats_clear_RID(0x0088,0x0110,"Stats_AuthFail_4",	"Disassociated due to inactivity")
#define awc_RID_Stats_clear_AuthFail_5 		awc_def_Stats_clear_RID(0x008A,0x0114,"Stats_AuthFail_5",	"Disassociated because AP is unable to handle all currently  associated stations.")
#define awc_RID_Stats_clear_AuthFail_6 		awc_def_Stats_clear_RID(0x008C,0x0118,"Stats_AuthFail_6",	"Class 2 Frame received from non-Authenticated station.")
#define awc_RID_Stats_clear_AuthFail_7 		awc_def_Stats_clear_RID(0x008E,0x011C,"Stats_AuthFail_7",	"Class 3 Frame received from non-Associated station.")
#define awc_RID_Stats_clear_AuthFail_8 		awc_def_Stats_clear_RID(0x0090,0x0120,"Stats_AuthFail_8",	"Disassociated because sending station is leaving (has left)")
#define awc_RID_Stats_clear_AuthFail_9 		awc_def_Stats_clear_RID(0x0092,0x0124,"Stats_AuthFail_9",	"Station requesting (Re)Association is not Authenticated")
#define awc_RID_Stats_clear_AuthFail_10 	awc_def_Stats_clear_RID(0x0094,0x0128,"Stats_AuthFail_10",	"Cannot support all requested capabilities in the Capability")
#define awc_RID_Stats_clear_AuthFail_11 	awc_def_Stats_clear_RID(0x0096,0x012C,"Stats_AuthFail_11",	"Reassociation denied due to inability to confirm")
#define awc_RID_Stats_clear_AuthFail_12 	awc_def_Stats_clear_RID(0x0098,0x0130,"Stats_AuthFail_12",	"Association denied due to reason outside the scope of the 802.11")
#define awc_RID_Stats_clear_AuthFail_13 	awc_def_Stats_clear_RID(0x009A,0x0134,"Stats_AuthFail_13",	"Responding station does not support the specified Auth Alogorithm")
#define awc_RID_Stats_clear_AuthFail_14 	awc_def_Stats_clear_RID(0x009C,0x0138,"Stats_AuthFail_14",	"Received an out of sequence Authentication Frame.")
#define awc_RID_Stats_clear_AuthFail_15 	awc_def_Stats_clear_RID(0x009E,0x013C,"Stats_AuthFail_15",	"Authentication rejected due to challenge failure.")
#define awc_RID_Stats_clear_AuthFail_16 	awc_def_Stats_clear_RID(0x00A0,0x0140,"Stats_AuthFail_16",	"Authentication rejected due to timeout waiting for next  frame in sequence.")
#define awc_RID_Stats_clear_AuthFail_17 	awc_def_Stats_clear_RID(0x00A2,0x0144,"Stats_AuthFail_17",	"Association denied because AP is unable to handle  additional associated stations.")
#define awc_RID_Stats_clear_AuthFail_18 	awc_def_Stats_clear_RID(0x00A4,0x0148,"Stats_AuthFail_18",	"Association denied due to requesting station not supportingall basic rates.")
#define awc_RID_Stats_clear_AuthFail_19 	awc_def_Stats_clear_RID(0x00A6,0x014C,"Stats_AuthFail_19",	"Reserved")
#define awc_RID_Stats_clear_RxMan 		awc_def_Stats_clear_RID(0x00A8,0x0150,"Stats_RxMan",		" Count of management packets received and handled.")
#define awc_RID_Stats_clear_TxMan 		awc_def_Stats_clear_RID(0x00AA,0x0154,"Stats_TxMan",		" Count of management packets transmitted.")
#define awc_RID_Stats_clear_RxRefresh 		awc_def_Stats_clear_RID(0x00AC,0x0158,"Stats_RxRefresh",	" Count of null data packets received.")
#define awc_RID_Stats_clear_TxRefresh 		awc_def_Stats_clear_RID(0x00AE,0x015C,"Stats_TxRefresh",	" Count of null data packets transmitted.")
#define awc_RID_Stats_clear_RxPoll 		awc_def_Stats_clear_RID(0x00B0,0x0160,"Stats_RxPoll",		"Count of PS-Poll packets received.")
#define awc_RID_Stats_clear_TxPoll 		awc_def_Stats_clear_RID(0x00B2,0x0164,"Stats_TxPoll",		"Count of PS-Poll packets transmitted.")
#define awc_RID_Stats_clear_HostRetries 	awc_def_Stats_clear_RID(0x00B4,0x0168,"Stats_HostRetries",	" Count of long and short retries used to transmit host packets  (does not include first attempt).")
#define awc_RID_Stats_clear_LostSync_HostReq 	awc_def_Stats_clear_RID(0x00B6,0x016C,"Stats_LostSync_HostReq","Lost sync with our cell due to host request.")
#define awc_RID_Stats_clear_HostTxBytes 	awc_def_Stats_clear_RID(0x00B8,0x0170,"Stats_HostTxBytes",	" Count of bytes transferred from the host.")
#define awc_RID_Stats_clear_HostRxBytes 	awc_def_Stats_clear_RID(0x00BA,0x0174,"Stats_HostRxBytes",	" Count of bytes transferred to the host.")
#define awc_RID_Stats_clear_ElapsedUsec 	awc_def_Stats_clear_RID(0x00BC,0x0178,"Stats_ElapsedUsec",	" Total time since power up (or clear) in microseconds.")
#define awc_RID_Stats_clear_ElapsedSec 		awc_def_Stats_clear_RID(0x00BE,0x017C,"Stats_ElapsedSec",	" Total time since power up (or clear) in seconds.")
#define awc_RID_Stats_clear_LostSyncBett 	awc_def_Stats_clear_RID(0x00C0,0x0180,"Stats_LostSyncBett",	"Lost Sync to switch to a better access point")



#define awc_RID_Stats16_RidLen 		awc_def_Stats16_RID(0x0000,0x0000,"RidLen",		"Length of the RID including the length field.")
#define awc_RID_Stats16_RxOverrunErr 	awc_def_Stats16_RID(0x0002,0x0004,"Stats_RxOverrunErr",	"Receive overruns -- No buffer available to handle the receive. (result is that the packet is never received)")
#define awc_RID_Stats16_RxPlcpCrcErr 	awc_def_Stats16_RID(0x0004,0x0008,"Stats_RxPlcpCrcErr",	"PLCP header checksum errors (CRC16).")
#define awc_RID_Stats16_RxPlcpFormat 	awc_def_Stats16_RID(0x0006,0x000C,"Stats_RxPlcpFormat",	"PLCP format errors.")
#define awc_RID_Stats16_RxPlcpLength 	awc_def_Stats16_RID(0x0008,0x0010,"Stats_RxPlcpLength",	"PLCP length is incorrect.")
#define awc_RID_Stats16_RxMacCrcErr 	awc_def_Stats16_RID(0x000A,0x0014,"Stats_RxMacCrcErr",	"Count of MAC CRC32 errors.")
#define awc_RID_Stats16_RxMacCrcOk 	awc_def_Stats16_RID(0x000C,0x0018,"Stats_RxMacCrcOk",	"Count of MAC CRC32 received correctly.")
#define awc_RID_Stats16_RxWepErr 	awc_def_Stats16_RID(0x000E,0x001C,"Stats_RxWepErr",	"Count of all WEP ICV checks that failed. (this value is included in Stats_RxMacCrcOk)")
#define awc_RID_Stats16_RxWepOk 	awc_def_Stats16_RID(0x0010,0x0020,"Stats_RxWepOk",	"Count of all WEP ICV checks that passed. (this value is  included in Stats_RxMacCrcOk)")
#define awc_RID_Stats16_RetryLong 	awc_def_Stats16_RID(0x0012,0x0024,"Stats_RetryLongCount",	"of all long retries. (Does not include first attempt for a packet).")
#define awc_RID_Stats16_RetryShort 	awc_def_Stats16_RID(0x0014,0x0028,"Stats_RetryShort",	"Count of all short retries. (Does not include first attempt for   a packet).")
#define awc_RID_Stats16_MaxRetries 	awc_def_Stats16_RID(0x0016,0x002C,"Stats_MaxRetries",	"Count of number of packets that max-retried -- ie were  never ACKd.")
#define awc_RID_Stats16_NoAck 		awc_def_Stats16_RID(0x0018,0x0030,"Stats_NoAck",		"Count of number of times that ACK was not received.")
#define awc_RID_Stats16_NoCts 		awc_def_Stats16_RID(0x001A,0x0034,"Stats_NoCts",		"Count of number of timer that CTS was not received.")
#define awc_RID_Stats16_RxAck 		awc_def_Stats16_RID(0x001C,0x0038,"Stats_RxAck",		"Count of number of expected ACKs that were received.")
#define awc_RID_Stats16_RxCts 		awc_def_Stats16_RID(0x001E,0x003C,"Stats_RxCts",		"Count of number of expected CTSs that were received.")
#define awc_RID_Stats16_TxAck 		awc_def_Stats16_RID(0x0020,0x0040,"Stats_TxAck",		"Count of number of ACKs transmitted.")
#define awc_RID_Stats16_TxRts 		awc_def_Stats16_RID(0x0022,0x0044,"Stats_TxRts",		"Count of number of RTSs transmitted.")
#define awc_RID_Stats16_TxCts 		awc_def_Stats16_RID(0x0024,0x0048,"Stats_TxCts",		"Count of number of CTSs transmitted.")
#define awc_RID_Stats16_TxMc 		awc_def_Stats16_RID(0x0026,0x004C,"Stats_TxMc",		" LMAC count of multicast packets sent (uses 802.11  Address1).")
#define awc_RID_Stats16_TxBc 		awc_def_Stats16_RID(0x0028,0x0050,"Stats_TxBc",		" LMAC count of broadcast packets sent (uses 802.11")
#define awc_RID_Stats16_TxUcFrags 	awc_def_Stats16_RID(0x002A,0x0054,"Stats_TxUcFragsLMAC",	" count of ALL unicast fragments and whole packets sent (uses 802.11 Address1).")
#define awc_RID_Stats16_TxUcPackets 	awc_def_Stats16_RID(0x002C,0x0058,"Stats_TxUcPackets",	"LMAC count of unicast packets that were ACKd (uses   802.11 Address 1).")
#define awc_RID_Stats16_TxBeacon 	awc_def_Stats16_RID(0x002E,0x005C,"Stats_TxBeacon",	" Count of beacon packets transmitted.")
#define awc_RID_Stats16_RxBeacon 	awc_def_Stats16_RID(0x0030,0x0060,"Stats_RxBeacon",	" Count of beacon packets received matching our BSSID.")
#define awc_RID_Stats16_TxSinColl 	awc_def_Stats16_RID(0x0032,0x0064,"Stats_TxSinCollTransmit"," single collisions. **")
#define awc_RID_Stats16_TxMulColl 	awc_def_Stats16_RID(0x0034,0x0068,"Stats_TxMulCollTransmit"," multiple collisions. **")
#define awc_RID_Stats16_DefersNo 	awc_def_Stats16_RID(0x0036,0x006C,"Stats_DefersNo Transmit"," frames sent with no deferral. **")
#define awc_RID_Stats16_DefersProt 	awc_def_Stats16_RID(0x0038,0x0070,"Stats_DefersProt",	" Transmit frames deferred due to protocol.")
#define awc_RID_Stats16_DefersEngy 	awc_def_Stats16_RID(0x003A,0x0074,"Stats_DefersEngy",	" Transmit frames deferred due to energy detect.")
#define awc_RID_Stats16_DupFram 	awc_def_Stats16_RID(0x003C,0x0078,"Stats_DupFram",	"  Duplicate receive frames and fragments.")
#define awc_RID_Stats16_RxFragDisc 	awc_def_Stats16_RID(0x003E,0x007C,"Stats_RxFragDisc",	" Received partial frames. (each tally could indicate the  discarding of one or more fragments)")
#define awc_RID_Stats16_TxAged 		awc_def_Stats16_RID(0x0040,0x0080,"Stats_TxAged",		"   Transmit packets exceeding maximum transmit lifetime. **")
#define awc_RID_Stats16_RxAged 		awc_def_Stats16_RID(0x0042,0x0084,"Stats_RxAgedReceive",	" packets exceeding maximum receive lifetime. **")
#define awc_RID_Stats16_LostSync_Max 	awc_def_Stats16_RID(0x0044,0x0088,"Stats_LostSync_Max",	" Lost sync with our cell due to maximum retries occuring. Retry")
#define awc_RID_Stats16_LostSync_Mis 	awc_def_Stats16_RID(0x0046,0x008C,"Stats_LostSync_Mis",	"Lost sync with our cell due to missing too many beacons. sedBeacons")
#define awc_RID_Stats16_LostSync_Arl 	awc_def_Stats16_RID(0x0048,0x0090,"Stats_LostSync_Arl",	"Lost sync with our cell due to Average Retry Level being  Exceeded  exceeded.")
#define awc_RID_Stats16_LostSync_Dea 	awc_def_Stats16_RID(0x004A,0x0094,"Stats_LostSync_Dea",	"Lost sync with our cell due to being deauthenticated.,thed")
#define awc_RID_Stats16_LostSync_Disa 	awc_def_Stats16_RID(0x004C,0x0098,"Stats_LostSync_Disa",	" Lost sync with our cell due to being disassociated. ssoced")
#define awc_RID_Stats16_LostSync_Tsf 	awc_def_Stats16_RID(0x004E,0x009C,"Stats_LostSync_Tsf",	"Lost sync with our cell due to excessive change in TSF  Timingtiming.")
#define awc_RID_Stats16_HostTxMc 	awc_def_Stats16_RID(0x0050,0x00A0,"Stats_HostTxMc",	"Count of multicast packets sent by the host.")
#define awc_RID_Stats16_HostTxBc 	awc_def_Stats16_RID(0x0052,0x00A4,"Stats_HostTxBc",	"Count of broadcast packets sent by the host.")
#define awc_RID_Stats16_HostTxUc 	awc_def_Stats16_RID(0x0054,0x00A8,"Stats_HostTxUc",	"Count of unicast packets sent by the host.")
#define awc_RID_Stats16_HostTxFail 	awc_def_Stats16_RID(0x0056,0x00AC,"Stats_HostTxFail",	"  Count of host transmitted packets which failed.")
#define awc_RID_Stats16_HostRxMc 	awc_def_Stats16_RID(0x0058,0x00B0,"Stats_HostRxMc",	"Count of host received multicast packets.")
#define awc_RID_Stats16_HostRxBc 	awc_def_Stats16_RID(0x005A,0x00B4,"Stats_HostRxBc",	"Count of host received broadcast packets.")
#define awc_RID_Stats16_HostRxUc 	awc_def_Stats16_RID(0x005C,0x00B8,"Stats_HostRxUc",	"Count of host received unicast packets.")
#define awc_RID_Stats16_HostRxDiscar 	awc_def_Stats16_RID(0x005E,0x00BC,"Stats_HostRxDiscar",	"Count of host received packets discarded due to:\n  Host not enabling receive.\n  Host failing to dequeue receive packets quickly.\n Packets being discarded due to magic packet mode.")
#define awc_RID_Stats16_HmacTxMc 	awc_def_Stats16_RID(0x0060,0x00C0,"Stats_HmacTxMc",	"Count of internally generated multicast (DA) packets.")
#define awc_RID_Stats16_HmacTxBc 	awc_def_Stats16_RID(0x0062,0x00C4,"Stats_HmacTxBc",	"Count of internally generated broadcast (DA) packets.")
#define awc_RID_Stats16_HmacTxUc 	awc_def_Stats16_RID(0x0064,0x00C8,"Stats_HmacTxUc",	"Count of internally generated unicast (DA) packets.")
#define awc_RID_Stats16_HmacTxFail 	awc_def_Stats16_RID(0x0066,0x00CC,"Stats_HmacTxFail",	"  Count of internally generated transmit packets that failed.")
#define awc_RID_Stats16_HmacRxMc 	awc_def_Stats16_RID(0x0068,0x00D0,"Stats_HmacRxMc",	"Count of internally received multicast (DA) packets.")
#define awc_RID_Stats16_HmacRxBc 	awc_def_Stats16_RID(0x006A,0x00D4,"Stats_HmacRxBc",	"Count of internally received broadcast (DA) packets.")
#define awc_RID_Stats16_HmacRxUc 	awc_def_Stats16_RID(0x006C,0x00D8,"Stats_HmacRxUc",	"Count of internally received multicast (DA) packets.")
#define awc_RID_Stats16_HmacRxDisca 	awc_def_Stats16_RID(0x006E,0x00DC,"Stats_HmacRxDisca",	" Count of internally received packets that were discarded  (usually because the destination address is not for the host).")
#define awc_RID_Stats16_HmacRxAcce 	awc_def_Stats16_RID(0x0070,0x00E0,"Stats_HmacRxAcce",	"  Count of internally received packets that were accepted")
#define awc_RID_Stats16_SsidMismatch 	awc_def_Stats16_RID(0x0072,0x00E4,"Stats_SsidMismatch",	" Count of SSID mismatches.")
#define awc_RID_Stats16_ApMismatch 	awc_def_Stats16_RID(0x0074,0x00E8,"Stats_ApMismatch",	"  Count of specified AP mismatches.")
#define awc_RID_Stats16_RatesMismatc 	awc_def_Stats16_RID(0x0076,0x00EC,"Stats_RatesMismatc",	" Count of rate mismatches.")
#define awc_RID_Stats16_AuthReject 	awc_def_Stats16_RID(0x0078,0x00F0,"Stats_AuthReject",	"  Count of authentication rejections.")
#define awc_RID_Stats16_AuthTimeout 	awc_def_Stats16_RID(0x007A,0x00F4,"Stats_AuthTimeout",	" Count of authentication timeouts.")
#define awc_RID_Stats16_AssocReject 	awc_def_Stats16_RID(0x007C,0x00F8,"Stats_AssocReject",	" Count of association rejections.")
#define awc_RID_Stats16_AssocTimeout 	awc_def_Stats16_RID(0x007E,0x00FC,"Stats_AssocTimeout",	" Count of association timeouts.")
#define awc_RID_Stats16_NewReason 	awc_def_Stats16_RID(0x0080,0x0100,"Stats_NewReason",	"Count of reason/status codes of greater than 19.  (Values of 0 = successful are not counted)")
#define awc_RID_Stats16_AuthFail_1 	awc_def_Stats16_RID(0x0082,0x0104,"Stats_AuthFail_1",	"Unspecified reason.")
#define awc_RID_Stats16_AuthFail_2 	awc_def_Stats16_RID(0x0084,0x0108,"Stats_AuthFail_2",	"Previous authentication no longer valid.")
#define awc_RID_Stats16_AuthFail_3 	awc_def_Stats16_RID(0x0086,0x010C,"Stats_AuthFail_3",	"Deauthenticated because sending station is leaving (has left) IBSS or ESS.")
#define awc_RID_Stats16_AuthFail_4 	awc_def_Stats16_RID(0x0088,0x0110,"Stats_AuthFail_4",	"Disassociated due to inactivity")
#define awc_RID_Stats16_AuthFail_5 	awc_def_Stats16_RID(0x008A,0x0114,"Stats_AuthFail_5",	"Disassociated because AP is unable to handle all currently  associated stations.")
#define awc_RID_Stats16_AuthFail_6 	awc_def_Stats16_RID(0x008C,0x0118,"Stats_AuthFail_6",	"Class 2 Frame received from non-Authenticated station.")
#define awc_RID_Stats16_AuthFail_7 	awc_def_Stats16_RID(0x008E,0x011C,"Stats_AuthFail_7",	"Class 3 Frame received from non-Associated station.")
#define awc_RID_Stats16_AuthFail_8 	awc_def_Stats16_RID(0x0090,0x0120,"Stats_AuthFail_8",	"Disassociated because sending station is leaving (has left)")
#define awc_RID_Stats16_AuthFail_9 	awc_def_Stats16_RID(0x0092,0x0124,"Stats_AuthFail_9",	"Station requesting (Re)Association is not Authenticated")
#define awc_RID_Stats16_AuthFail_10 	awc_def_Stats16_RID(0x0094,0x0128,"Stats_AuthFail_10",	"Cannot support all requested capabilities in the Capability")
#define awc_RID_Stats16_AuthFail_11 	awc_def_Stats16_RID(0x0096,0x012C,"Stats_AuthFail_11",	"Reassociation denied due to inability to confirm")
#define awc_RID_Stats16_AuthFail_12 	awc_def_Stats16_RID(0x0098,0x0130,"Stats_AuthFail_12",	"Association denied due to reason outside the scope of the 802.11")
#define awc_RID_Stats16_AuthFail_13 	awc_def_Stats16_RID(0x009A,0x0134,"Stats_AuthFail_13",	"Responding station does not support the specified Auth Alogorithm")
#define awc_RID_Stats16_AuthFail_14 	awc_def_Stats16_RID(0x009C,0x0138,"Stats_AuthFail_14",	"Received an out of sequence Authentication Frame.")
#define awc_RID_Stats16_AuthFail_15 	awc_def_Stats16_RID(0x009E,0x013C,"Stats_AuthFail_15",	"Authentication rejected due to challenge failure.")
#define awc_RID_Stats16_AuthFail_16 	awc_def_Stats16_RID(0x00A0,0x0140,"Stats_AuthFail_16",	"Authentication rejected due to timeout waiting for next  frame in sequence.")
#define awc_RID_Stats16_AuthFail_17 	awc_def_Stats16_RID(0x00A2,0x0144,"Stats_AuthFail_17",	"Association denied because AP is unable to handle  additional associated stations.")
#define awc_RID_Stats16_AuthFail_18 	awc_def_Stats16_RID(0x00A4,0x0148,"Stats_AuthFail_18",	"Association denied due to requesting station not supportingall basic rates.")
#define awc_RID_Stats16_AuthFail_19 	awc_def_Stats16_RID(0x00A6,0x014C,"Stats_AuthFail_19",	"Reserved")
#define awc_RID_Stats16_RxMan 		awc_def_Stats16_RID(0x00A8,0x0150,"Stats_RxMan",		" Count of management packets received and handled.")
#define awc_RID_Stats16_TxMan 		awc_def_Stats16_RID(0x00AA,0x0154,"Stats_TxMan",		" Count of management packets transmitted.")
#define awc_RID_Stats16_RxRefresh 	awc_def_Stats16_RID(0x00AC,0x0158,"Stats_RxRefresh",	" Count of null data packets received.")
#define awc_RID_Stats16_TxRefresh 	awc_def_Stats16_RID(0x00AE,0x015C,"Stats_TxRefresh",	" Count of null data packets transmitted.")
#define awc_RID_Stats16_RxPoll 		awc_def_Stats16_RID(0x00B0,0x0160,"Stats_RxPoll",		"Count of PS-Poll packets received.")
#define awc_RID_Stats16_TxPoll 		awc_def_Stats16_RID(0x00B2,0x0164,"Stats_TxPoll",		"Count of PS-Poll packets transmitted.")
#define awc_RID_Stats16_HostRetries 	awc_def_Stats16_RID(0x00B4,0x0168,"Stats_HostRetries",	" Count of long and short retries used to transmit host packets  (does not include first attempt).")
#define awc_RID_Stats16_LostSync_HostReq awc_def_Stats16_RID(0x00B6,0x016C,"Stats_LostSync_HostReq","Lost sync with our cell due to host request.")
#define awc_RID_Stats16_HostTxBytes 	awc_def_Stats16_RID(0x00B8,0x0170,"Stats_HostTxBytes",	" Count of bytes transferred from the host.")
#define awc_RID_Stats16_HostRxBytes 	awc_def_Stats16_RID(0x00BA,0x0174,"Stats_HostRxBytes",	" Count of bytes transferred to the host.")
#define awc_RID_Stats16_ElapsedUsec 	awc_def_Stats16_RID(0x00BC,0x0178,"Stats_ElapsedUsec",	" Total time since power up (or clear) in microseconds.")
#define awc_RID_Stats16_ElapsedSec 	awc_def_Stats16_RID(0x00BE,0x017C,"Stats_ElapsedSec",	" Total time since power up (or clear) in seconds.")
#define awc_RID_Stats16_LostSyncBett 	awc_def_Stats16_RID(0x00C0,0x0180,"Stats_LostSyncBett",	"Lost Sync to switch to a better access point")



#define awc_RID_Stats16_delta_RidLen 		awc_def_Stats16_delta_RID(0x0000,0x0000,"RidLen",		"Length of the RID including the length field.")
#define awc_RID_Stats16_delta_RxOverrunErr 	awc_def_Stats16_delta_RID(0x0002,0x0004,"Stats_RxOverrunErr",	"Receive overruns -- No buffer available to handle the receive. (result is that the packet is never received)")
#define awc_RID_Stats16_delta_RxPlcpCrcErr 	awc_def_Stats16_delta_RID(0x0004,0x0008,"Stats_RxPlcpCrcErr",	"PLCP header checksum errors (CRC16).")
#define awc_RID_Stats16_delta_RxPlcpFormat 	awc_def_Stats16_delta_RID(0x0006,0x000C,"Stats_RxPlcpFormat",	"PLCP format errors.")
#define awc_RID_Stats16_delta_RxPlcpLength 	awc_def_Stats16_delta_RID(0x0008,0x0010,"Stats_RxPlcpLength",	"PLCP length is incorrect.")
#define awc_RID_Stats16_delta_RxMacCrcErr 	awc_def_Stats16_delta_RID(0x000A,0x0014,"Stats_RxMacCrcErr",	"Count of MAC CRC32 errors.")
#define awc_RID_Stats16_delta_RxMacCrcOk 	awc_def_Stats16_delta_RID(0x000C,0x0018,"Stats_RxMacCrcOk",	"Count of MAC CRC32 received correctly.")
#define awc_RID_Stats16_delta_RxWepErr 		awc_def_Stats16_delta_RID(0x000E,0x001C,"Stats_RxWepErr",	"Count of all WEP ICV checks that failed. (this value is included in Stats_RxMacCrcOk)")
#define awc_RID_Stats16_delta_RxWepOk 		awc_def_Stats16_delta_RID(0x0010,0x0020,"Stats_RxWepOk",	"Count of all WEP ICV checks that passed. (this value is  included in Stats_RxMacCrcOk)")
#define awc_RID_Stats16_delta_RetryLong 	awc_def_Stats16_delta_RID(0x0012,0x0024,"Stats_RetryLongCount",	"of all long retries. (Does not include first attempt for a packet).")
#define awc_RID_Stats16_delta_RetryShort 	awc_def_Stats16_delta_RID(0x0014,0x0028,"Stats_RetryShort",	"Count of all short retries. (Does not include first attempt for   a packet).")
#define awc_RID_Stats16_delta_MaxRetries 	awc_def_Stats16_delta_RID(0x0016,0x002C,"Stats_MaxRetries",	"Count of number of packets that max-retried -- ie were  never ACKd.")
#define awc_RID_Stats16_delta_NoAck 		awc_def_Stats16_delta_RID(0x0018,0x0030,"Stats_NoAck",		"Count of number of times that ACK was not received.")
#define awc_RID_Stats16_delta_NoCts 		awc_def_Stats16_delta_RID(0x001A,0x0034,"Stats_NoCts",		"Count of number of timer that CTS was not received.")
#define awc_RID_Stats16_delta_RxAck 		awc_def_Stats16_delta_RID(0x001C,0x0038,"Stats_RxAck",		"Count of number of expected ACKs that were received.")
#define awc_RID_Stats16_delta_RxCts 		awc_def_Stats16_delta_RID(0x001E,0x003C,"Stats_RxCts",		"Count of number of expected CTSs that were received.")
#define awc_RID_Stats16_delta_TxAck 		awc_def_Stats16_delta_RID(0x0020,0x0040,"Stats_TxAck",		"Count of number of ACKs transmitted.")
#define awc_RID_Stats16_delta_TxRts 		awc_def_Stats16_delta_RID(0x0022,0x0044,"Stats_TxRts",		"Count of number of RTSs transmitted.")
#define awc_RID_Stats16_delta_TxCts 		awc_def_Stats16_delta_RID(0x0024,0x0048,"Stats_TxCts",		"Count of number of CTSs transmitted.")
#define awc_RID_Stats16_delta_TxMc 		awc_def_Stats16_delta_RID(0x0026,0x004C,"Stats_TxMc",		" LMAC count of multicast packets sent (uses 802.11  Address1).")
#define awc_RID_Stats16_delta_TxBc 		awc_def_Stats16_delta_RID(0x0028,0x0050,"Stats_TxBc",		" LMAC count of broadcast packets sent (uses 802.11")
#define awc_RID_Stats16_delta_TxUcFrags 	awc_def_Stats16_delta_RID(0x002A,0x0054,"Stats_TxUcFragsLMAC",	" count of ALL unicast fragments and whole packets sent (uses 802.11 Address1).")
#define awc_RID_Stats16_delta_TxUcPackets 	awc_def_Stats16_delta_RID(0x002C,0x0058,"Stats_TxUcPackets",	"LMAC count of unicast packets that were ACKd (uses   802.11 Address 1).")
#define awc_RID_Stats16_delta_TxBeacon 		awc_def_Stats16_delta_RID(0x002E,0x005C,"Stats_TxBeacon",	" Count of beacon packets transmitted.")
#define awc_RID_Stats16_delta_RxBeacon 		awc_def_Stats16_delta_RID(0x0030,0x0060,"Stats_RxBeacon",	" Count of beacon packets received matching our BSSID.")
#define awc_RID_Stats16_delta_TxSinColl 	awc_def_Stats16_delta_RID(0x0032,0x0064,"Stats_TxSinCollTransmit"," single collisions. **")
#define awc_RID_Stats16_delta_TxMulColl 	awc_def_Stats16_delta_RID(0x0034,0x0068,"Stats_TxMulCollTransmit"," multiple collisions. **")
#define awc_RID_Stats16_delta_DefersNo 		awc_def_Stats16_delta_RID(0x0036,0x006C,"Stats_DefersNo Transmit"," frames sent with no deferral. **")
#define awc_RID_Stats16_delta_DefersProt 	awc_def_Stats16_delta_RID(0x0038,0x0070,"Stats_DefersProt",	" Transmit frames deferred due to protocol.")
#define awc_RID_Stats16_delta_DefersEngy 	awc_def_Stats16_delta_RID(0x003A,0x0074,"Stats_DefersEngy",	" Transmit frames deferred due to energy detect.")
#define awc_RID_Stats16_delta_DupFram 		awc_def_Stats16_delta_RID(0x003C,0x0078,"Stats_DupFram",	"  Duplicate receive frames and fragments.")
#define awc_RID_Stats16_delta_RxFragDisc 	awc_def_Stats16_delta_RID(0x003E,0x007C,"Stats_RxFragDisc",	" Received partial frames. (each tally could indicate the  discarding of one or more fragments)")
#define awc_RID_Stats16_delta_TxAged 		awc_def_Stats16_delta_RID(0x0040,0x0080,"Stats_TxAged",		"   Transmit packets exceeding maximum transmit lifetime. **")
#define awc_RID_Stats16_delta_RxAged 		awc_def_Stats16_delta_RID(0x0042,0x0084,"Stats_RxAgedReceive",	" packets exceeding maximum receive lifetime. **")
#define awc_RID_Stats16_delta_LostSync_Max 	awc_def_Stats16_delta_RID(0x0044,0x0088,"Stats_LostSync_Max",	" Lost sync with our cell due to maximum retries occuring. Retry")
#define awc_RID_Stats16_delta_LostSync_Mis 	awc_def_Stats16_delta_RID(0x0046,0x008C,"Stats_LostSync_Mis",	"Lost sync with our cell due to missing too many beacons. sedBeacons")
#define awc_RID_Stats16_delta_LostSync_Arl 	awc_def_Stats16_delta_RID(0x0048,0x0090,"Stats_LostSync_Arl",	"Lost sync with our cell due to Average Retry Level being  Exceeded  exceeded.")
#define awc_RID_Stats16_delta_LostSync_Dea 	awc_def_Stats16_delta_RID(0x004A,0x0094,"Stats_LostSync_Dea",	"Lost sync with our cell due to being deauthenticated.,thed")
#define awc_RID_Stats16_delta_LostSync_Disa 	awc_def_Stats16_delta_RID(0x004C,0x0098,"Stats_LostSync_Disa",	" Lost sync with our cell due to being disassociated. ssoced")
#define awc_RID_Stats16_delta_LostSync_Tsf 	awc_def_Stats16_delta_RID(0x004E,0x009C,"Stats_LostSync_Tsf",	"Lost sync with our cell due to excessive change in TSF  Timingtiming.")
#define awc_RID_Stats16_delta_HostTxMc 		awc_def_Stats16_delta_RID(0x0050,0x00A0,"Stats_HostTxMc",	"Count of multicast packets sent by the host.")
#define awc_RID_Stats16_delta_HostTxBc 		awc_def_Stats16_delta_RID(0x0052,0x00A4,"Stats_HostTxBc",	"Count of broadcast packets sent by the host.")
#define awc_RID_Stats16_delta_HostTxUc 		awc_def_Stats16_delta_RID(0x0054,0x00A8,"Stats_HostTxUc",	"Count of unicast packets sent by the host.")
#define awc_RID_Stats16_delta_HostTxFail 	awc_def_Stats16_delta_RID(0x0056,0x00AC,"Stats_HostTxFail",	"  Count of host transmitted packets which failed.")
#define awc_RID_Stats16_delta_HostRxMc 		awc_def_Stats16_delta_RID(0x0058,0x00B0,"Stats_HostRxMc",	"Count of host received multicast packets.")
#define awc_RID_Stats16_delta_HostRxBc 		awc_def_Stats16_delta_RID(0x005A,0x00B4,"Stats_HostRxBc",	"Count of host received broadcast packets.")
#define awc_RID_Stats16_delta_HostRxUc 		awc_def_Stats16_delta_RID(0x005C,0x00B8,"Stats_HostRxUc",	"Count of host received unicast packets.")
#define awc_RID_Stats16_delta_HostRxDiscar 	awc_def_Stats16_delta_RID(0x005E,0x00BC,"Stats_HostRxDiscar",	"Count of host received packets discarded due to:\n  Host not enabling receive.\n  Host failing to dequeue receive packets quickly.\n Packets being discarded due to magic packet mode.")
#define awc_RID_Stats16_delta_HmacTxMc 		awc_def_Stats16_delta_RID(0x0060,0x00C0,"Stats_HmacTxMc",	"Count of internally generated multicast (DA) packets.")
#define awc_RID_Stats16_delta_HmacTxBc 		awc_def_Stats16_delta_RID(0x0062,0x00C4,"Stats_HmacTxBc",	"Count of internally generated broadcast (DA) packets.")
#define awc_RID_Stats16_delta_HmacTxUc 		awc_def_Stats16_delta_RID(0x0064,0x00C8,"Stats_HmacTxUc",	"Count of internally generated unicast (DA) packets.")
#define awc_RID_Stats16_delta_HmacTxFail 	awc_def_Stats16_delta_RID(0x0066,0x00CC,"Stats_HmacTxFail",	"  Count of internally generated transmit packets that failed.")
#define awc_RID_Stats16_delta_HmacRxMc 		awc_def_Stats16_delta_RID(0x0068,0x00D0,"Stats_HmacRxMc",	"Count of internally received multicast (DA) packets.")
#define awc_RID_Stats16_delta_HmacRxBc 		awc_def_Stats16_delta_RID(0x006A,0x00D4,"Stats_HmacRxBc",	"Count of internally received broadcast (DA) packets.")
#define awc_RID_Stats16_delta_HmacRxUc 		awc_def_Stats16_delta_RID(0x006C,0x00D8,"Stats_HmacRxUc",	"Count of internally received multicast (DA) packets.")
#define awc_RID_Stats16_delta_HmacRxDisca 	awc_def_Stats16_delta_RID(0x006E,0x00DC,"Stats_HmacRxDisca",	" Count of internally received packets that were discarded  (usually because the destination address is not for the host).")
#define awc_RID_Stats16_delta_HmacRxAcce 	awc_def_Stats16_delta_RID(0x0070,0x00E0,"Stats_HmacRxAcce",	"  Count of internally received packets that were accepted")
#define awc_RID_Stats16_delta_SsidMismatch 	awc_def_Stats16_delta_RID(0x0072,0x00E4,"Stats_SsidMismatch",	" Count of SSID mismatches.")
#define awc_RID_Stats16_delta_ApMismatch 	awc_def_Stats16_delta_RID(0x0074,0x00E8,"Stats_ApMismatch",	"  Count of specified AP mismatches.")
#define awc_RID_Stats16_delta_RatesMismatc 	awc_def_Stats16_delta_RID(0x0076,0x00EC,"Stats_RatesMismatc",	" Count of rate mismatches.")
#define awc_RID_Stats16_delta_AuthReject 	awc_def_Stats16_delta_RID(0x0078,0x00F0,"Stats_AuthReject",	"  Count of authentication rejections.")
#define awc_RID_Stats16_delta_AuthTimeout 	awc_def_Stats16_delta_RID(0x007A,0x00F4,"Stats_AuthTimeout",	" Count of authentication timeouts.")
#define awc_RID_Stats16_delta_AssocReject 	awc_def_Stats16_delta_RID(0x007C,0x00F8,"Stats_AssocReject",	" Count of association rejections.")
#define awc_RID_Stats16_delta_AssocTimeout 	awc_def_Stats16_delta_RID(0x007E,0x00FC,"Stats_AssocTimeout",	" Count of association timeouts.")
#define awc_RID_Stats16_delta_NewReason 	awc_def_Stats16_delta_RID(0x0080,0x0100,"Stats_NewReason",	"Count of reason/status codes of greater than 19.  (Values of 0 = successful are not counted)")
#define awc_RID_Stats16_delta_AuthFail_1 	awc_def_Stats16_delta_RID(0x0082,0x0104,"Stats_AuthFail_1",	"Unspecified reason.")
#define awc_RID_Stats16_delta_AuthFail_2 	awc_def_Stats16_delta_RID(0x0084,0x0108,"Stats_AuthFail_2",	"Previous authentication no longer valid.")
#define awc_RID_Stats16_delta_AuthFail_3 	awc_def_Stats16_delta_RID(0x0086,0x010C,"Stats_AuthFail_3",	"Deauthenticated because sending station is leaving (has left) IBSS or ESS.")
#define awc_RID_Stats16_delta_AuthFail_4 	awc_def_Stats16_delta_RID(0x0088,0x0110,"Stats_AuthFail_4",	"Disassociated due to inactivity")
#define awc_RID_Stats16_delta_AuthFail_5 	awc_def_Stats16_delta_RID(0x008A,0x0114,"Stats_AuthFail_5",	"Disassociated because AP is unable to handle all currently  associated stations.")
#define awc_RID_Stats16_delta_AuthFail_6 	awc_def_Stats16_delta_RID(0x008C,0x0118,"Stats_AuthFail_6",	"Class 2 Frame received from non-Authenticated station.")
#define awc_RID_Stats16_delta_AuthFail_7 	awc_def_Stats16_delta_RID(0x008E,0x011C,"Stats_AuthFail_7",	"Class 3 Frame received from non-Associated station.")
#define awc_RID_Stats16_delta_AuthFail_8 	awc_def_Stats16_delta_RID(0x0090,0x0120,"Stats_AuthFail_8",	"Disassociated because sending station is leaving (has left)")
#define awc_RID_Stats16_delta_AuthFail_9 	awc_def_Stats16_delta_RID(0x0092,0x0124,"Stats_AuthFail_9",	"Station requesting (Re)Association is not Authenticated")
#define awc_RID_Stats16_delta_AuthFail_10 	awc_def_Stats16_delta_RID(0x0094,0x0128,"Stats_AuthFail_10",	"Cannot support all requested capabilities in the Capability")
#define awc_RID_Stats16_delta_AuthFail_11 	awc_def_Stats16_delta_RID(0x0096,0x012C,"Stats_AuthFail_11",	"Reassociation denied due to inability to confirm")
#define awc_RID_Stats16_delta_AuthFail_12 	awc_def_Stats16_delta_RID(0x0098,0x0130,"Stats_AuthFail_12",	"Association denied due to reason outside the scope of the 802.11")
#define awc_RID_Stats16_delta_AuthFail_13 	awc_def_Stats16_delta_RID(0x009A,0x0134,"Stats_AuthFail_13",	"Responding station does not support the specified Auth Alogorithm")
#define awc_RID_Stats16_delta_AuthFail_14 	awc_def_Stats16_delta_RID(0x009C,0x0138,"Stats_AuthFail_14",	"Received an out of sequence Authentication Frame.")
#define awc_RID_Stats16_delta_AuthFail_15 	awc_def_Stats16_delta_RID(0x009E,0x013C,"Stats_AuthFail_15",	"Authentication rejected due to challenge failure.")
#define awc_RID_Stats16_delta_AuthFail_16 	awc_def_Stats16_delta_RID(0x00A0,0x0140,"Stats_AuthFail_16",	"Authentication rejected due to timeout waiting for next  frame in sequence.")
#define awc_RID_Stats16_delta_AuthFail_17 	awc_def_Stats16_delta_RID(0x00A2,0x0144,"Stats_AuthFail_17",	"Association denied because AP is unable to handle  additional associated stations.")
#define awc_RID_Stats16_delta_AuthFail_18 	awc_def_Stats16_delta_RID(0x00A4,0x0148,"Stats_AuthFail_18",	"Association denied due to requesting station not supportingall basic rates.")
#define awc_RID_Stats16_delta_AuthFail_19 	awc_def_Stats16_delta_RID(0x00A6,0x014C,"Stats_AuthFail_19",	"Reserved")
#define awc_RID_Stats16_delta_RxMan 		awc_def_Stats16_delta_RID(0x00A8,0x0150,"Stats_RxMan",		" Count of management packets received and handled.")
#define awc_RID_Stats16_delta_TxMan 		awc_def_Stats16_delta_RID(0x00AA,0x0154,"Stats_TxMan",		" Count of management packets transmitted.")
#define awc_RID_Stats16_delta_RxRefresh 	awc_def_Stats16_delta_RID(0x00AC,0x0158,"Stats_RxRefresh",	" Count of null data packets received.")
#define awc_RID_Stats16_delta_TxRefresh 	awc_def_Stats16_delta_RID(0x00AE,0x015C,"Stats_TxRefresh",	" Count of null data packets transmitted.")
#define awc_RID_Stats16_delta_RxPoll 		awc_def_Stats16_delta_RID(0x00B0,0x0160,"Stats_RxPoll",		"Count of PS-Poll packets received.")
#define awc_RID_Stats16_delta_TxPoll 		awc_def_Stats16_delta_RID(0x00B2,0x0164,"Stats_TxPoll",		"Count of PS-Poll packets transmitted.")
#define awc_RID_Stats16_delta_HostRetries 	awc_def_Stats16_delta_RID(0x00B4,0x0168,"Stats_HostRetries",	" Count of long and short retries used to transmit host packets  (does not include first attempt).")
#define awc_RID_Stats16_delta_LostSync_HostReq 	awc_def_Stats16_delta_RID(0x00B6,0x016C,"Stats_LostSync_HostReq","Lost sync with our cell due to host request.")
#define awc_RID_Stats16_delta_HostTxBytes 	awc_def_Stats16_delta_RID(0x00B8,0x0170,"Stats_HostTxBytes",	" Count of bytes transferred from the host.")
#define awc_RID_Stats16_delta_HostRxBytes 	awc_def_Stats16_delta_RID(0x00BA,0x0174,"Stats_HostRxBytes",	" Count of bytes transferred to the host.")
#define awc_RID_Stats16_delta_ElapsedUsec 	awc_def_Stats16_delta_RID(0x00BC,0x0178,"Stats_ElapsedUsec",	" Total time since power up (or clear) in microseconds.")
#define awc_RID_Stats16_delta_ElapsedSec 	awc_def_Stats16_delta_RID(0x00BE,0x017C,"Stats_ElapsedSec",	" Total time since power up (or clear) in seconds.")
#define awc_RID_Stats16_delta_LostSyncBett 	awc_def_Stats16_delta_RID(0x00C0,0x0180,"Stats_LostSyncBett",	"Lost Sync to switch to a better access point")


#define awc_RID_Stats16_clear_RidLen 		awc_def_Stats16_clear_RID(0x0000,0x0000,"RidLen",		"Length of the RID including the length field.")
#define awc_RID_Stats16_clear_RxOverrunErr 	awc_def_Stats16_clear_RID(0x0002,0x0004,"Stats_RxOverrunErr",	"Receive overruns -- No buffer available to handle the receive. (result is that the packet is never received)")
#define awc_RID_Stats16_clear_RxPlcpCrcErr 	awc_def_Stats16_clear_RID(0x0004,0x0008,"Stats_RxPlcpCrcErr",	"PLCP header checksum errors (CRC16).")
#define awc_RID_Stats16_clear_RxPlcpFormat 	awc_def_Stats16_clear_RID(0x0006,0x000C,"Stats_RxPlcpFormat",	"PLCP format errors.")
#define awc_RID_Stats16_clear_RxPlcpLength 	awc_def_Stats16_clear_RID(0x0008,0x0010,"Stats_RxPlcpLength",	"PLCP length is incorrect.")
#define awc_RID_Stats16_clear_RxMacCrcErr 	awc_def_Stats16_clear_RID(0x000A,0x0014,"Stats_RxMacCrcErr",	"Count of MAC CRC32 errors.")
#define awc_RID_Stats16_clear_RxMacCrcOk 	awc_def_Stats16_clear_RID(0x000C,0x0018,"Stats_RxMacCrcOk",	"Count of MAC CRC32 received correctly.")
#define awc_RID_Stats16_clear_RxWepErr 		awc_def_Stats16_clear_RID(0x000E,0x001C,"Stats_RxWepErr",	"Count of all WEP ICV checks that failed. (this value is included in Stats_RxMacCrcOk)")
#define awc_RID_Stats16_clear_RxWepOk 		awc_def_Stats16_clear_RID(0x0010,0x0020,"Stats_RxWepOk",	"Count of all WEP ICV checks that passed. (this value is  included in Stats_RxMacCrcOk)")
#define awc_RID_Stats16_clear_RetryLong 	awc_def_Stats16_clear_RID(0x0012,0x0024,"Stats_RetryLongCount",	"of all long retries. (Does not include first attempt for a packet).")
#define awc_RID_Stats16_clear_RetryShort 	awc_def_Stats16_clear_RID(0x0014,0x0028,"Stats_RetryShort",	"Count of all short retries. (Does not include first attempt for   a packet).")
#define awc_RID_Stats16_clear_MaxRetries 	awc_def_Stats16_clear_RID(0x0016,0x002C,"Stats_MaxRetries",	"Count of number of packets that max-retried -- ie were  never ACKd.")
#define awc_RID_Stats16_clear_NoAck 		awc_def_Stats16_clear_RID(0x0018,0x0030,"Stats_NoAck",		"Count of number of times that ACK was not received.")
#define awc_RID_Stats16_clear_NoCts 		awc_def_Stats16_clear_RID(0x001A,0x0034,"Stats_NoCts",		"Count of number of timer that CTS was not received.")
#define awc_RID_Stats16_clear_RxAck 		awc_def_Stats16_clear_RID(0x001C,0x0038,"Stats_RxAck",		"Count of number of expected ACKs that were received.")
#define awc_RID_Stats16_clear_RxCts 		awc_def_Stats16_clear_RID(0x001E,0x003C,"Stats_RxCts",		"Count of number of expected CTSs that were received.")
#define awc_RID_Stats16_clear_TxAck 		awc_def_Stats16_clear_RID(0x0020,0x0040,"Stats_TxAck",		"Count of number of ACKs transmitted.")
#define awc_RID_Stats16_clear_TxRts 		awc_def_Stats16_clear_RID(0x0022,0x0044,"Stats_TxRts",		"Count of number of RTSs transmitted.")
#define awc_RID_Stats16_clear_TxCts 		awc_def_Stats16_clear_RID(0x0024,0x0048,"Stats_TxCts",		"Count of number of CTSs transmitted.")
#define awc_RID_Stats16_clear_TxMc 		awc_def_Stats16_clear_RID(0x0026,0x004C,"Stats_TxMc",		" LMAC count of multicast packets sent (uses 802.11  Address1).")
#define awc_RID_Stats16_clear_TxBc 		awc_def_Stats16_clear_RID(0x0028,0x0050,"Stats_TxBc",		" LMAC count of broadcast packets sent (uses 802.11")
#define awc_RID_Stats16_clear_TxUcFrags 	awc_def_Stats16_clear_RID(0x002A,0x0054,"Stats_TxUcFragsLMAC",	" count of ALL unicast fragments and whole packets sent (uses 802.11 Address1).")
#define awc_RID_Stats16_clear_TxUcPackets 	awc_def_Stats16_clear_RID(0x002C,0x0058,"Stats_TxUcPackets",	"LMAC count of unicast packets that were ACKd (uses   802.11 Address 1).")
#define awc_RID_Stats16_clear_TxBeacon 		awc_def_Stats16_clear_RID(0x002E,0x005C,"Stats_TxBeacon",	" Count of beacon packets transmitted.")
#define awc_RID_Stats16_clear_RxBeacon 		awc_def_Stats16_clear_RID(0x0030,0x0060,"Stats_RxBeacon",	" Count of beacon packets received matching our BSSID.")
#define awc_RID_Stats16_clear_TxSinColl 	awc_def_Stats16_clear_RID(0x0032,0x0064,"Stats_TxSinCollTransmit"," single collisions. **")
#define awc_RID_Stats16_clear_TxMulColl 	awc_def_Stats16_clear_RID(0x0034,0x0068,"Stats_TxMulCollTransmit"," multiple collisions. **")
#define awc_RID_Stats16_clear_DefersNo 		awc_def_Stats16_clear_RID(0x0036,0x006C,"Stats_DefersNo Transmit"," frames sent with no deferral. **")
#define awc_RID_Stats16_clear_DefersProt 	awc_def_Stats16_clear_RID(0x0038,0x0070,"Stats_DefersProt",	" Transmit frames deferred due to protocol.")
#define awc_RID_Stats16_clear_DefersEngy 	awc_def_Stats16_clear_RID(0x003A,0x0074,"Stats_DefersEngy",	" Transmit frames deferred due to energy detect.")
#define awc_RID_Stats16_clear_DupFram 		awc_def_Stats16_clear_RID(0x003C,0x0078,"Stats_DupFram",	"  Duplicate receive frames and fragments.")
#define awc_RID_Stats16_clear_RxFragDisc 	awc_def_Stats16_clear_RID(0x003E,0x007C,"Stats_RxFragDisc",	" Received partial frames. (each tally could indicate the  discarding of one or more fragments)")
#define awc_RID_Stats16_clear_TxAged 		awc_def_Stats16_clear_RID(0x0040,0x0080,"Stats_TxAged",		"   Transmit packets exceeding maximum transmit lifetime. **")
#define awc_RID_Stats16_clear_RxAged 		awc_def_Stats16_clear_RID(0x0042,0x0084,"Stats_RxAgedReceive",	" packets exceeding maximum receive lifetime. **")
#define awc_RID_Stats16_clear_LostSync_Max 	awc_def_Stats16_clear_RID(0x0044,0x0088,"Stats_LostSync_Max",	" Lost sync with our cell due to maximum retries occuring. Retry")
#define awc_RID_Stats16_clear_LostSync_Mis 	awc_def_Stats16_clear_RID(0x0046,0x008C,"Stats_LostSync_Mis",	"Lost sync with our cell due to missing too many beacons. sedBeacons")
#define awc_RID_Stats16_clear_LostSync_Arl 	awc_def_Stats16_clear_RID(0x0048,0x0090,"Stats_LostSync_Arl",	"Lost sync with our cell due to Average Retry Level being  Exceeded  exceeded.")
#define awc_RID_Stats16_clear_LostSync_Dea 	awc_def_Stats16_clear_RID(0x004A,0x0094,"Stats_LostSync_Dea",	"Lost sync with our cell due to being deauthenticated.,thed")
#define awc_RID_Stats16_clear_LostSync_Disa 	awc_def_Stats16_clear_RID(0x004C,0x0098,"Stats_LostSync_Disa",	" Lost sync with our cell due to being disassociated. ssoced")
#define awc_RID_Stats16_clear_LostSync_Tsf 	awc_def_Stats16_clear_RID(0x004E,0x009C,"Stats_LostSync_Tsf",	"Lost sync with our cell due to excessive change in TSF  Timingtiming.")
#define awc_RID_Stats16_clear_HostTxMc 		awc_def_Stats16_clear_RID(0x0050,0x00A0,"Stats_HostTxMc",	"Count of multicast packets sent by the host.")
#define awc_RID_Stats16_clear_HostTxBc 		awc_def_Stats16_clear_RID(0x0052,0x00A4,"Stats_HostTxBc",	"Count of broadcast packets sent by the host.")
#define awc_RID_Stats16_clear_HostTxUc 		awc_def_Stats16_clear_RID(0x0054,0x00A8,"Stats_HostTxUc",	"Count of unicast packets sent by the host.")
#define awc_RID_Stats16_clear_HostTxFail 	awc_def_Stats16_clear_RID(0x0056,0x00AC,"Stats_HostTxFail",	"  Count of host transmitted packets which failed.")
#define awc_RID_Stats16_clear_HostRxMc 		awc_def_Stats16_clear_RID(0x0058,0x00B0,"Stats_HostRxMc",	"Count of host received multicast packets.")
#define awc_RID_Stats16_clear_HostRxBc 		awc_def_Stats16_clear_RID(0x005A,0x00B4,"Stats_HostRxBc",	"Count of host received broadcast packets.")
#define awc_RID_Stats16_clear_HostRxUc 		awc_def_Stats16_clear_RID(0x005C,0x00B8,"Stats_HostRxUc",	"Count of host received unicast packets.")
#define awc_RID_Stats16_clear_HostRxDiscar 	awc_def_Stats16_clear_RID(0x005E,0x00BC,"Stats_HostRxDiscar",	"Count of host received packets discarded due to:\n  Host not enabling receive.\n  Host failing to dequeue receive packets quickly.\n Packets being discarded due to magic packet mode.")
#define awc_RID_Stats16_clear_HmacTxMc 		awc_def_Stats16_clear_RID(0x0060,0x00C0,"Stats_HmacTxMc",	"Count of internally generated multicast (DA) packets.")
#define awc_RID_Stats16_clear_HmacTxBc 		awc_def_Stats16_clear_RID(0x0062,0x00C4,"Stats_HmacTxBc",	"Count of internally generated broadcast (DA) packets.")
#define awc_RID_Stats16_clear_HmacTxUc 		awc_def_Stats16_clear_RID(0x0064,0x00C8,"Stats_HmacTxUc",	"Count of internally generated unicast (DA) packets.")
#define awc_RID_Stats16_clear_HmacTxFail 	awc_def_Stats16_clear_RID(0x0066,0x00CC,"Stats_HmacTxFail",	"  Count of internally generated transmit packets that failed.")
#define awc_RID_Stats16_clear_HmacRxMc 		awc_def_Stats16_clear_RID(0x0068,0x00D0,"Stats_HmacRxMc",	"Count of internally received multicast (DA) packets.")
#define awc_RID_Stats16_clear_HmacRxBc 		awc_def_Stats16_clear_RID(0x006A,0x00D4,"Stats_HmacRxBc",	"Count of internally received broadcast (DA) packets.")
#define awc_RID_Stats16_clear_HmacRxUc 		awc_def_Stats16_clear_RID(0x006C,0x00D8,"Stats_HmacRxUc",	"Count of internally received multicast (DA) packets.")
#define awc_RID_Stats16_clear_HmacRxDisca 	awc_def_Stats16_clear_RID(0x006E,0x00DC,"Stats_HmacRxDisca",	" Count of internally received packets that were discarded  (usually because the destination address is not for the host).")
#define awc_RID_Stats16_clear_HmacRxAcce 	awc_def_Stats16_clear_RID(0x0070,0x00E0,"Stats_HmacRxAcce",	"  Count of internally received packets that were accepted")
#define awc_RID_Stats16_clear_SsidMismatch 	awc_def_Stats16_clear_RID(0x0072,0x00E4,"Stats_SsidMismatch",	" Count of SSID mismatches.")
#define awc_RID_Stats16_clear_ApMismatch 	awc_def_Stats16_clear_RID(0x0074,0x00E8,"Stats_ApMismatch",	"  Count of specified AP mismatches.")
#define awc_RID_Stats16_clear_RatesMismatc 	awc_def_Stats16_clear_RID(0x0076,0x00EC,"Stats_RatesMismatc",	" Count of rate mismatches.")
#define awc_RID_Stats16_clear_AuthReject 	awc_def_Stats16_clear_RID(0x0078,0x00F0,"Stats_AuthReject",	"  Count of authentication rejections.")
#define awc_RID_Stats16_clear_AuthTimeout 	awc_def_Stats16_clear_RID(0x007A,0x00F4,"Stats_AuthTimeout",	" Count of authentication timeouts.")
#define awc_RID_Stats16_clear_AssocReject 	awc_def_Stats16_clear_RID(0x007C,0x00F8,"Stats_AssocReject",	" Count of association rejections.")
#define awc_RID_Stats16_clear_AssocTimeout 	awc_def_Stats16_clear_RID(0x007E,0x00FC,"Stats_AssocTimeout",	" Count of association timeouts.")
#define awc_RID_Stats16_clear_NewReason 	awc_def_Stats16_clear_RID(0x0080,0x0100,"Stats_NewReason",	"Count of reason/status codes of greater than 19.  (Values of 0 = successful are not counted)")
#define awc_RID_Stats16_clear_AuthFail_1 	awc_def_Stats16_clear_RID(0x0082,0x0104,"Stats_AuthFail_1",	"Unspecified reason.")
#define awc_RID_Stats16_clear_AuthFail_2 	awc_def_Stats16_clear_RID(0x0084,0x0108,"Stats_AuthFail_2",	"Previous authentication no longer valid.")
#define awc_RID_Stats16_clear_AuthFail_3 	awc_def_Stats16_clear_RID(0x0086,0x010C,"Stats_AuthFail_3",	"Deauthenticated because sending station is leaving (has left) IBSS or ESS.")
#define awc_RID_Stats16_clear_AuthFail_4 	awc_def_Stats16_clear_RID(0x0088,0x0110,"Stats_AuthFail_4",	"Disassociated due to inactivity")
#define awc_RID_Stats16_clear_AuthFail_5 	awc_def_Stats16_clear_RID(0x008A,0x0114,"Stats_AuthFail_5",	"Disassociated because AP is unable to handle all currently  associated stations.")
#define awc_RID_Stats16_clear_AuthFail_6 	awc_def_Stats16_clear_RID(0x008C,0x0118,"Stats_AuthFail_6",	"Class 2 Frame received from non-Authenticated station.")
#define awc_RID_Stats16_clear_AuthFail_7 	awc_def_Stats16_clear_RID(0x008E,0x011C,"Stats_AuthFail_7",	"Class 3 Frame received from non-Associated station.")
#define awc_RID_Stats16_clear_AuthFail_8 	awc_def_Stats16_clear_RID(0x0090,0x0120,"Stats_AuthFail_8",	"Disassociated because sending station is leaving (has left) " )
#define awc_RID_Stats16_clear_AuthFail_9 	awc_def_Stats16_clear_RID(0x0092,0x0124,"Stats_AuthFail_9",	"Station requesting (Re)Association is not Authenticated")
#define awc_RID_Stats16_clear_AuthFail_10 	awc_def_Stats16_clear_RID(0x0094,0x0128,"Stats_AuthFail_10",	"Cannot support all requested capabilities in the Capability")
#define awc_RID_Stats16_clear_AuthFail_11 	awc_def_Stats16_clear_RID(0x0096,0x012C,"Stats_AuthFail_11",	"Reassociation denied due to inability to confirm")
#define awc_RID_Stats16_clear_AuthFail_12 	awc_def_Stats16_clear_RID(0x0098,0x0130,"Stats_AuthFail_12",	"Association denied due to reason outside the scope of the 802.11")
#define awc_RID_Stats16_clear_AuthFail_13 	awc_def_Stats16_clear_RID(0x009A,0x0134,"Stats_AuthFail_13",	"Responding station does not support the specified Auth Alogorithm")
#define awc_RID_Stats16_clear_AuthFail_14 	awc_def_Stats16_clear_RID(0x009C,0x0138,"Stats_AuthFail_14",	"Received an out of sequence Authentication Frame.")
#define awc_RID_Stats16_clear_AuthFail_15 	awc_def_Stats16_clear_RID(0x009E,0x013C,"Stats_AuthFail_15",	"Authentication rejected due to challenge failure.")
#define awc_RID_Stats16_clear_AuthFail_16 	awc_def_Stats16_clear_RID(0x00A0,0x0140,"Stats_AuthFail_16",	"Authentication rejected due to timeout waiting for next  frame in sequence.")
#define awc_RID_Stats16_clear_AuthFail_17 	awc_def_Stats16_clear_RID(0x00A2,0x0144,"Stats_AuthFail_17",	"Association denied because AP is unable to handle  additional associated stations.")
#define awc_RID_Stats16_clear_AuthFail_18 	awc_def_Stats16_clear_RID(0x00A4,0x0148,"Stats_AuthFail_18",	"Association denied due to requesting station not supportingall basic rates.")
#define awc_RID_Stats16_clear_AuthFail_19 	awc_def_Stats16_clear_RID(0x00A6,0x014C,"Stats_AuthFail_19",	"Reserved")
#define awc_RID_Stats16_clear_RxMan 		awc_def_Stats16_clear_RID(0x00A8,0x0150,"Stats_RxMan",		" Count of management packets received and handled.")
#define awc_RID_Stats16_clear_TxMan 		awc_def_Stats16_clear_RID(0x00AA,0x0154,"Stats_TxMan",		" Count of management packets transmitted.")
#define awc_RID_Stats16_clear_RxRefresh 	awc_def_Stats16_clear_RID(0x00AC,0x0158,"Stats_RxRefresh",	" Count of null data packets received.")
#define awc_RID_Stats16_clear_TxRefresh 	awc_def_Stats16_clear_RID(0x00AE,0x015C,"Stats_TxRefresh",	" Count of null data packets transmitted.")
#define awc_RID_Stats16_clear_RxPoll 		awc_def_Stats16_clear_RID(0x00B0,0x0160,"Stats_RxPoll",		"Count of PS-Poll packets received.")
#define awc_RID_Stats16_clear_TxPoll 		awc_def_Stats16_clear_RID(0x00B2,0x0164,"Stats_TxPoll",		"Count of PS-Poll packets transmitted.")
#define awc_RID_Stats16_clear_HostRetries 	awc_def_Stats16_clear_RID(0x00B4,0x0168,"Stats_HostRetries",	" Count of long and short retries used to transmit host packets  (does not include first attempt).")
#define awc_RID_Stats16_clear_LostSync_HostReq 	awc_def_Stats16_clear_RID(0x00B6,0x016C,"Stats_LostSync_HostReq","Lost sync with our cell due to host request.")
#define awc_RID_Stats16_clear_HostTxBytes 	awc_def_Stats16_clear_RID(0x00B8,0x0170,"Stats_HostTxBytes",	" Count of bytes transferred from the host.")
#define awc_RID_Stats16_clear_HostRxBytes 	awc_def_Stats16_clear_RID(0x00BA,0x0174,"Stats_HostRxBytes",	" Count of bytes transferred to the host.")
#define awc_RID_Stats16_clear_ElapsedUsec 	awc_def_Stats16_clear_RID(0x00BC,0x0178,"Stats_ElapsedUsec",	" Total time since power up (or clear) in microseconds.")
#define awc_RID_Stats16_clear_ElapsedSec 	awc_def_Stats16_clear_RID(0x00BE,0x017C,"Stats_ElapsedSec",	" Total time since power up (or clear) in seconds.")
#define awc_RID_Stats16_clear_LostSyncBett 	awc_def_Stats16_clear_RID(0x00C0,0x0180,"Stats_LostSyncBett",	"Lost Sync to switch to a better access point")
/*
const struct aironet4500_rid_selector  aironet4500_RID_Select_General_Config	=(const struct aironet4500_rid_selector){ 0xFF10, 1,0,0, "General Configuration" }; //        See notes General Configuration        Many configuration items.
const struct aironet4500_rid_selector  aironet4500_RID_Select_SSID_list		=(const struct aironet4500_rid_selector){ 0xFF11, 1,0,0, "Valid SSID list" }; //          See notes Valid SSID list              List of SSIDs which the station may associate to.
const struct aironet4500_rid_selector  aironet4500_RID_Select_AP_list		=(const struct aironet4500_rid_selector){ 0xFF12, 1,0,0, "Valid AP list" }; //          See notes Valid AP list                List of APs which the station may associate to.
const struct aironet4500_rid_selector  aironet4500_RID_Select_Driver_name	=(const struct aironet4500_rid_selector){ 0xFF13, 1,0,0, "Driver name" }; //          See notes Driver name                  The name and version of the driver (for debugging)
const struct aironet4500_rid_selector  aironet4500_RID_Select_Encapsulation	=(const struct aironet4500_rid_selector){ 0xFF14, 1,0,0, "Ethernet Protocol" }; //          See notes Ethernet Protocol            Rules for encapsulating ethernet payloads onto 802.11.
const struct aironet4500_rid_selector  aironet4500_RID_Select_WEP_volatile	=(const struct aironet4500_rid_selector){ 0xFF15, 1,0,0, "WEP key volatile" }; //          
const struct aironet4500_rid_selector  aironet4500_RID_Select_WEP_nonvolatile	=(const struct aironet4500_rid_selector){ 0xFF16, 1,0,0, "WEP key non-volatile" }; //
const struct aironet4500_rid_selector  aironet4500_RID_Select_Modulation	=(const struct aironet4500_rid_selector){ 0xFF17, 1,0,0, "Modulation" }; //
const struct aironet4500_rid_selector  aironet4500_RID_Select_Active_Config	=(const struct aironet4500_rid_selector){ 0xFF20, 0,1,1, "Actual Configuration" }; //          Read only      Actual Configuration    This has the same format as the General Configuration.
const struct aironet4500_rid_selector  aironet4500_RID_Select_Capabilities	=(const struct aironet4500_rid_selector){ 0xFF00, 0,1,0, "Capabilities" }; //          Read Only      Capabilities            PC4500 Information
const struct aironet4500_rid_selector  aironet4500_RID_Select_AP_Info		=(const struct aironet4500_rid_selector){ 0xFF01, 0,1,1, "AP Info" }; //          Read Only      AP Info                 Access Point Information
const struct aironet4500_rid_selector  aironet4500_RID_Select_Radio_Info	=(const struct aironet4500_rid_selector){ 0xFF02, 0,1,1, "Radio Info" }; //          Read Only      Radio Info              Radio Information -- note radio specific
const struct aironet4500_rid_selector  aironet4500_RID_Select_Status		=(const struct aironet4500_rid_selector){ 0xFF50, 0,1,1, "Status" }; //          Read Only      Status                  PC4500 Current Status Information
const struct aironet4500_rid_selector  aironet4500_RID_Select_16_stats		=(const struct aironet4500_rid_selector){ 0xFF60, 0,1,1, "Cumulative 16-bit Statistics" }; //          Read Only      16-bit Statistics       Cumulative 16-bit Statistics
const struct aironet4500_rid_selector  aironet4500_RID_Select_16_stats_delta	=(const struct aironet4500_rid_selector){ 0xFF61, 0,1,1, "Delta 16-bit Statistics" }; //          Read Only      16-bit Statistics       Delta 16-bit Statistics (since last clear)
const struct aironet4500_rid_selector  aironet4500_RID_Select_16_stats_clear	=(const struct aironet4500_rid_selector){ 0xFF62, 0,1,1, "Delta 16-bit Statistics and Clear" }; //          Read Only /    16-bit Statistics       Delta 16-bit Statistics and Clear
const struct aironet4500_rid_selector  aironet4500_RID_Select_32_stats      	=(const struct aironet4500_rid_selector){ 0xFF68, 0,1,1, "Cumulative 32-bit Statistics" }; //          Read Only      32-bit Statistics       Cumulative 32-bit Statistics
const struct aironet4500_rid_selector  aironet4500_RID_Select_32_stats_delta	=(const struct aironet4500_rid_selector){ 0xFF69, 0,1,1, "Delta 32-bit Statistics"  }; //          Read Only      32-bit Statistics       Delta 32-bit Statistics (since last clear)
const struct aironet4500_rid_selector  aironet4500_RID_Select_32_stats_clear	=(const struct aironet4500_rid_selector){ 0xFF6A, 0,1,1, "Delta 32-bit Statistics and Clear" }; //          Read Only /    32-bit Statistics       Delta 32-bit Statistics and Clear
*/

struct aironet4500_RID awc_gen_RID[] ={
	awc_RID_gen_RidLen,
	awc_RID_gen_OperatingMode_adhoc,
	awc_RID_gen_OperatingMode_Infrastructure,
	awc_RID_gen_OperatingMode_AP,
	awc_RID_gen_OperatingMode_AP_and_repeater,
	awc_RID_gen_OperatingMode_No_payload_modify,
	awc_RID_gen_OperatingMode_LLC_802_3_convert,
	awc_RID_gen_OperatingMode_proprietary_ext,
	awc_RID_gen_OperatingMode_no_proprietary_ext,
	awc_RID_gen_OperatingMode_AP_ext,
	awc_RID_gen_OperatingMode_no_AP_ext,
	awc_RID_gen_ReceiveMode,
	awc_RID_gen_ReceiveMode_BMA,
	awc_RID_gen_ReceiveMode_BA,
	awc_RID_gen_ReceiveMode_A,
	awc_RID_gen_ReceiveMode_802_11_monitor,
	awc_RID_gen_ReceiveMode_802_11_any_monitor,
	awc_RID_gen_ReceiveMode_LAN_monitor,
	awc_RID_gen_ReceiveMode_802_3_hdr_disable,
	awc_RID_gen_ReceiveMode_802_3_hdr_enable,
	awc_RID_gen_Fragmentation_threshold,
	awc_RID_gen_RTS_threshold,
	awc_RID_gen_Station_Mac_Id,
	awc_RID_gen_Supported_rates,
	awc_RID_gen_Basic_Rate,
	awc_RID_gen_Rate_500kbps,
	awc_RID_gen_Rate_1Mbps,
	awc_RID_gen_Rate_2Mbps,
	awc_RID_gen_Rate_4Mbps,
	awc_RID_gen_Rate_5Mbps,
	awc_RID_gen_Rate_10Mbps,
	awc_RID_gen_Rate_11Mbps,
	awc_RID_gen_BasicRate_500kbps,
	awc_RID_gen_BasicRate_1Mbps,
	awc_RID_gen_BasicRate_2Mbps,
	awc_RID_gen_BasicRate_4Mbps,
	awc_RID_gen_BasicRate_5Mbps,
	awc_RID_gen_BasicRate_10Mbps,
	awc_RID_gen_BasicRate_11Mbps,
	awc_RID_gen_Long_retry_limit,
	awc_RID_gen_Short_retry_limit,
	awc_RID_gen_Tx_MSDU_lifetime,
	awc_RID_gen_Rx_MSDU_lifetime,
	awc_RID_gen_Stationary,
	awc_RID_gen_BC_MC_Ordering,
	awc_RID_gen_Device_type,
	awc_RID_gen_Reserved_0x0026,
	awc_RID_gen_ScanMode,
	awc_RID_gen_ScanMode_Active,
	awc_RID_gen_ScanMode_Passive,
	awc_RID_gen_ScanMode_Aironet_ext,
	awc_RID_gen_ProbeDelay,
	awc_RID_gen_ProbeEnergyTimeout,
	awc_RID_gen_ProbeResponseTimeout,
	awc_RID_gen_BeaconListenTimeout,
	awc_RID_gen_IbssJoinNetTimeout,
	awc_RID_gen_AuthenticationTimeout,
	awc_RID_gen_AuthenticationType,
	awc_RID_gen_AuthenticationType_None,
	awc_RID_gen_AuthenticationType_Open,
	awc_RID_gen_AuthenticationType_Shared,
	awc_RID_gen_AuthenticationType_Exclude_Open,
	awc_RID_gen_AssociationTimeout,
	awc_RID_gen_SpecifiedAPtimeout,
	awc_RID_gen_OfflineScanInterval,
	awc_RID_gen_OfflineScanDuration,
	awc_RID_gen_LinkLossDelay,
	awc_RID_gen_MaxBeaconLostTime,
	awc_RID_gen_RefreshInterval,
	awc_RID_gen_PowerSaveMode,
	awc_RID_gen_PowerSaveMode_CAM,
	awc_RID_gen_PowerSaveMode_PSP,
	awc_RID_gen_PowerSaveMode_Fast_PSP,
	awc_RID_gen_SleepForDTIMs,
	awc_RID_gen_ListenInterval,
	awc_RID_gen_FastListenInterval,
	awc_RID_gen_ListenDecay,
	awc_RID_gen_FastListenDelay,
	awc_RID_gen_Reserved0x005C,
	awc_RID_gen_BeaconPeriod,
	awc_RID_gen_AtimDuration,
	awc_RID_gen_Reserved0x0064,
	awc_RID_gen_DSChannel,
	awc_RID_gen_Reserved0x0068,
	awc_RID_gen_DTIM_Period,
	awc_RID_gen_Reserved0x0006C,
	awc_RID_gen_RadioSpreadType,
	awc_RID_gen_RadioSpreadType_FH,
	awc_RID_gen_RadioSpreadType_DS,
	awc_RID_gen_RadioSpreadType_LM,
	awc_RID_gen_TX_antenna_Diversity,
	awc_RID_gen_TX_antenna_Diversity_default,
	awc_RID_gen_TX_antenna_Diversity_1,
	awc_RID_gen_TX_antenna_Diversity_2,
	awc_RID_gen_TX_antenna_Diversity_both,
	awc_RID_gen_RX_antenna_Diversity,
	awc_RID_gen_RX_antenna_Diversity_default,
	awc_RID_gen_RX_antenna_Diversity_1,
	awc_RID_gen_RX_antenna_Diversity_2,
	awc_RID_gen_RX_antenna_Diversity_both,
	awc_RID_gen_TransmitPower,
	awc_RID_gen_RSSIthreshold,
	awc_RID_gen_Modulation,
	awc_RID_gen_Reserved0x0079,
	awc_RID_gen_NodeName,
	awc_RID_gen_ARLThreshold,
	awc_RID_gen_ARLDecay,
	awc_RID_gen_ARLDelay,
	awc_RID_gen_Unused0x0096,
	awc_RID_gen_MagicPacketAction,
	awc_RID_gen_MagicPacketControl,
	{0}
};

struct aironet4500_RID awc_act_RID[]={
	awc_RID_act_RidLen,
	awc_RID_act_OperatingMode_adhoc,
	awc_RID_act_OperatingMode_Infrastructure,
	awc_RID_act_OperatingMode_AP,
	awc_RID_act_OperatingMode_AP_and_repeater,
	awc_RID_act_OperatingMode_No_payload_modify,
	awc_RID_act_OperatingMode_LLC_802_3_convert,
	awc_RID_act_OperatingMode_proprietary_ext,
	awc_RID_act_OperatingMode_no_proprietary_ext,
	awc_RID_act_OperatingMode_AP_ext,
	awc_RID_act_OperatingMode_no_AP_ext,
	awc_RID_act_ReceiveMode,
	awc_RID_act_ReceiveMode_BMA,
	awc_RID_act_ReceiveMode_BA,
	awc_RID_act_ReceiveMode_A,
	awc_RID_act_ReceiveMode_802_11_monitor,
	awc_RID_act_ReceiveMode_802_11_any_monitor,
	awc_RID_act_ReceiveMode_LAN_monitor,
	awc_RID_act_ReceiveMode_802_3_hdr_disable,
	awc_RID_act_ReceiveMode_802_3_hdr_enable,
	awc_RID_act_Fragmentation_threshold,
	awc_RID_act_RTS_threshold,
	awc_RID_act_Station_Mac_Id,
	awc_RID_act_Supported_rates,
	awc_RID_act_Basic_Rate,
	awc_RID_act_Rate_500kbps,
	awc_RID_act_Rate_1Mbps,
	awc_RID_act_Rate_2Mbps,
	awc_RID_act_Rate_4Mbps,
	awc_RID_act_Rate_5Mbps,
	awc_RID_act_Rate_10Mbps,
	awc_RID_act_Rate_11Mbps,
	awc_RID_act_BasicRate_500kbps,
	awc_RID_act_BasicRate_1Mbps,
	awc_RID_act_BasicRate_2Mbps,
	awc_RID_act_BasicRate_4Mbps,
	awc_RID_act_BasicRate_5Mbps,
	awc_RID_act_BasicRate_10Mbps,
	awc_RID_act_BasicRate_11Mbps,
	awc_RID_act_Long_retry_limit,
	awc_RID_act_Short_retry_limit,
	awc_RID_act_Tx_MSDU_lifetime,
	awc_RID_act_Rx_MSDU_lifetime,
	awc_RID_act_Stationary,
	awc_RID_act_BC_MC_Ordering,
	awc_RID_act_Device_type,
	awc_RID_act_Reserved_0x0026,
	awc_RID_act_ScanMode,
	awc_RID_act_ScanMode_Active,
	awc_RID_act_ScanMode_Passive,
	awc_RID_act_ScanMode_Aironet_ext,
	awc_RID_act_ProbeDelay,
	awc_RID_act_ProbeEnergyTimeout,
	awc_RID_act_ProbeResponseTimeout,
	awc_RID_act_BeaconListenTimeout,
	awc_RID_act_IbssJoinNetTimeout,
	awc_RID_act_AuthenticationTimeout,
	awc_RID_act_AuthenticationType,
	awc_RID_act_AuthenticationType_None,
	awc_RID_act_AuthenticationType_Open,
	awc_RID_act_AuthenticationType_Shared,
	awc_RID_act_AuthenticationType_Exclude_Open,
	awc_RID_act_AssociationTimeout,
	awc_RID_act_SpecifiedAPtimeout,
	awc_RID_act_OfflineScanInterval,
	awc_RID_act_OfflineScanDuration,
	awc_RID_act_LinkLossDelay,
	awc_RID_act_MaxBeaconLostTime,
	awc_RID_act_RefreshInterval,
	awc_RID_act_PowerSaveMode,
	awc_RID_act_PowerSaveMode_CAM,
	awc_RID_act_PowerSaveMode_PSP,
	awc_RID_act_PowerSaveMode_Fast_PSP,
	awc_RID_act_SleepForDTIMs,
	awc_RID_act_ListenInterval,
	awc_RID_act_FastListenInterval,
	awc_RID_act_ListenDecay,
	awc_RID_act_FastListenDelay,
	awc_RID_act_Reserved0x005C,
	awc_RID_act_BeaconPeriod,
	awc_RID_act_AtimDuration,
	awc_RID_act_Reserved0x0064,
	awc_RID_act_DSChannel,
	awc_RID_act_Reserved0x0068,
	awc_RID_act_DTIM_Period,
	awc_RID_act_Reserved0x0006C,
	awc_RID_act_RadioSpreadType,
	awc_RID_act_RadioSpreadType_FH,
	awc_RID_act_RadioSpreadType_DS,
	awc_RID_act_RadioSpreadType_LM,
	awc_RID_act_TX_antenna_Diversity,
	awc_RID_act_TX_antenna_Diversity_default,
	awc_RID_act_TX_antenna_Diversity_1,
	awc_RID_act_TX_antenna_Diversity_2,
	awc_RID_act_TX_antenna_Diversity_both,
	awc_RID_act_RX_antenna_Diversity,
	awc_RID_act_RX_antenna_Diversity_default,
	awc_RID_act_RX_antenna_Diversity_1,
	awc_RID_act_RX_antenna_Diversity_2,
	awc_RID_act_RX_antenna_Diversity_both,
	awc_RID_act_TransmitPower,
	awc_RID_act_RSSIthreshold,
	awc_RID_act_Modulation,
	awc_RID_act_Reserved0x0079,
	awc_RID_act_NodeName,
	awc_RID_act_ARLThreshold,
	awc_RID_act_ARLDecay,
	awc_RID_act_ARLDelay,
	awc_RID_act_Unused0x0096,
	awc_RID_act_MagicPacketAction,
	awc_RID_act_MagicPacketControl,
	{0}
};



struct aironet4500_RID awc_SSID_RID[]={
	awc_RID_SSID_RidLen,
	awc_RID_SSID_Accept_any,
	awc_RID_SSIDlen1,
	awc_RID_SSID1,
	awc_RID_SSIDlen2,
	awc_RID_SSID2,
	awc_RID_SSIDlen3,
	awc_RID_SSID3,
	awc_RID_SSID1hex,
	awc_RID_SSID2hex,
	awc_RID_SSID3hex,
	{0}
};


struct aironet4500_RID awc_AP_List_RID[]={
	awc_RID_AP_List_RidLen,
	awc_RID_AP_List_SpecifiedAP1,
	awc_RID_AP_List_SpecifiedAP2,
	awc_RID_AP_List_SpecifiedAP3,
	awc_RID_AP_List_SpecifiedAP4,
	{0}
};


struct aironet4500_RID awc_Dname_RID[]={
	awc_RID_Dname_RidLen,
	awc_RID_Dname_DriverName,
	{0}
};




struct aironet4500_RID awc_enc_RID[]={
	awc_RID_Enc_RidLen,
	awc_RID_Enc_EtherType1,
	awc_RID_Enc_Action_RX_1,
	awc_RID_Enc_Action_RX_1_RFC_1042,
	awc_RID_Enc_Action_RX_1_802_11,
	awc_RID_Enc_Action_TX_1,
	awc_RID_Enc_Action_TX_1_RFC_1042,
	awc_RID_Enc_Action_TX_1_802_11,
	awc_RID_Enc_EtherType2,
	awc_RID_Enc_Action_RX_2,
	awc_RID_Enc_Action_RX_2_RFC_1042,
	awc_RID_Enc_Action_RX_2_802_11,
	awc_RID_Enc_Action_TX_2,
	awc_RID_Enc_Action_TX_2_RFC_1042,
	awc_RID_Enc_Action_TX_2_802_11,
	awc_RID_Enc_EtherType3,
	awc_RID_Enc_Action_RX_3,
	awc_RID_Enc_Action_RX_3_RFC_1042,
	awc_RID_Enc_Action_RX_3_802_11,
	awc_RID_Enc_Action_TX_3_,
	awc_RID_Enc_Action_TX_3_RFC_1042,
	awc_RID_Enc_Action_TX_3_802_11,
	awc_RID_Enc_EtherType4,
	awc_RID_Enc_Action_RX_4,
	awc_RID_Enc_Action_RX_4_RFC_1042,
	awc_RID_Enc_Action_RX_4_802_11,
	awc_RID_Enc_Action_TX_4,
	awc_RID_Enc_Action_TX_4_RFC_1042,
	awc_RID_Enc_Action_TX_4_802_11,
	awc_RID_Enc_EtherType5,
	awc_RID_Enc_Action_RX_5,
	awc_RID_Enc_Action_RX_5_RFC_1042,
	awc_RID_Enc_Action_RX_5_802_11,
	awc_RID_Enc_Action_TX_5,
	awc_RID_Enc_Action_TX_5_RFC_1042,
	awc_RID_Enc_Action_TX_5_802_11,
	awc_RID_Enc_EtherType6,
	awc_RID_Enc_Action_RX_6,
	awc_RID_Enc_Action_RX_6_RFC_1042,
	awc_RID_Enc_Action_RX_6_802_11,
	awc_RID_Enc_Action_TX_6,
	awc_RID_Enc_Action_TX_6_RFC_1042,
	awc_RID_Enc_Action_TX_6_802_11,
	awc_RID_Enc_EtherType7,
	awc_RID_Enc_Action_RX_7,
	awc_RID_Enc_Action_RX_7_RFC_1042,
	awc_RID_Enc_Action_RX_7_802_11,
	awc_RID_Enc_Action_TX_7,
	awc_RID_Enc_Action_TX_7_RFC_1042,
	awc_RID_Enc_Action_TX_7_802_11,
	awc_RID_Enc_EtherType8,
	awc_RID_Enc_Action_RX_8,
	awc_RID_Enc_Action_RX_8_RFC_1042,
	awc_RID_Enc_Action_RX_8_802_11,
	awc_RID_Enc_Action_TX_8,
	awc_RID_Enc_Action_TX_8_RFC_1042,
	awc_RID_Enc_Action_TX_8_802_11,
	{0}
};

struct aironet4500_RID awc_WEPv_RID[]={
	awc_RID_WEPv_RidLen,
	awc_RID_WEPv_KeyIndex,
	awc_RID_WEPv_Address,
	awc_RID_WEPv_KeyLen,
	awc_RID_WEPv_Key,
	awc_RID_WEPv_KeyAscii,
	{0}
};

struct aironet4500_RID awc_WEPnv_RID[]={
	awc_RID_WEPnv_RidLen,
	awc_RID_WEPnv_KeyIndex,
	awc_RID_WEPnv_Address,
	awc_RID_WEPnv_KeyLen,
	awc_RID_WEPnv_Key,
	awc_RID_WEPnv_KeyAscii,
	{0}
};

struct aironet4500_RID awc_Modulation_RID[]={
	awc_RID_Modulation_RidLen,
	awc_RID_Modulation_Modulation,
	{0}
};



struct aironet4500_RID awc_Cap_RID[]={
	awc_RID_Cap_RidLen,
	awc_RID_Cap_OUI,
	awc_RID_Cap_ProductNum,
	awc_RID_Cap_ManufacturerName,
	awc_RID_Cap_ProductName,
	awc_RID_Cap_ProductVersion,
	awc_RID_Cap_FactoryAddress,
	awc_RID_Cap_AironetAddress,
	awc_RID_Cap_RadioSpreadType_DS,
	awc_RID_Cap_RadioSpreadType_FH,
	awc_RID_Cap_RadioSpreadType_Legacy,
	awc_RID_Cap_RegDomain,
	awc_RID_Cap_Callid,
	awc_RID_Cap_SupportedRates,
	awc_RID_Cap_RxDiversity,
	awc_RID_Cap_TxDiversity,
	awc_RID_Cap_TxPowerLevels,
	awc_RID_Cap_HardwareVersion,
	awc_RID_Cap_HardwareCapabilit,
	awc_RID_Cap_TemperatureRange,
	awc_RID_Cap_SoftwareVersion,
	awc_RID_Cap_SoftwareVersion_major,
	awc_RID_Cap_SoftwareVersion_minor,
	awc_RID_Cap_SoftwareSubVersion,
	awc_RID_Cap_InterfaceVersion,
	awc_RID_Cap_SoftwareCapabilities,
	awc_RID_Cap_BootBlockVersion,
	{0}
};


struct aironet4500_RID awc_Status_RID[]={
	awc_RID_Status_RidLen,
	awc_RID_Status_MacAddress,
	awc_RID_Status_OperationalMode,
	awc_RID_Status_Configured,
	awc_RID_Status_MAC_Enabled,
	awc_RID_Status_Receive_Enabled,
	awc_RID_Status_In_Sync,
	awc_RID_Status_Associated,
	awc_RID_Status_Error,
	awc_RID_Status_ErrorCode,
	awc_RID_Status_CurrentSignalQuality,
	awc_RID_Status_SSIDlength,
	awc_RID_Status_SSID,
	awc_RID_Status_ApName,
	awc_RID_Status_CurrentBssid,
	awc_RID_Status_PreviousBssid1,
	awc_RID_Status_PreviousBssid2,
	awc_RID_Status_PreviousBssid3,
	awc_RID_Status_BeaconPeriod,
	awc_RID_Status_DtimPeriod,
	awc_RID_Status_AtimDuration,
	awc_RID_Status_HopPeriod,
	awc_RID_Status_ChannelSet,
	awc_RID_Status_Channel,
	awc_RID_Status_HopsToBackbone,
	awc_RID_Status_ApTotalLoad,
	awc_RID_Status_OurGeneratedLoad,
	awc_RID_Status_AccumulatedArl,
	{0}
};


struct aironet4500_RID awc_AP_RID[]={
	awc_RID_AP_16RidLen,
	awc_RID_AP_TIM_addr,
	awc_RID_AP_Airo_addr,
	{0}
};


struct aironet4500_RID awc_Stats_RID[]={
	awc_RID_Stats_RidLen,
	awc_RID_Stats_RxOverrunErr,
	awc_RID_Stats_RxPlcpCrcErr,
	awc_RID_Stats_RxPlcpFormat,
	awc_RID_Stats_RxPlcpLength,
	awc_RID_Stats_RxMacCrcErr,
	awc_RID_Stats_RxMacCrcOk,
	awc_RID_Stats_RxWepErr,
	awc_RID_Stats_RxWepOk,
	awc_RID_Stats_RetryLong,
	awc_RID_Stats_RetryShort,
	awc_RID_Stats_MaxRetries,
	awc_RID_Stats_NoAck,
	awc_RID_Stats_NoCts,
	awc_RID_Stats_RxAck,
	awc_RID_Stats_RxCts,
	awc_RID_Stats_TxAck,
	awc_RID_Stats_TxRts,
	awc_RID_Stats_TxCts,
	awc_RID_Stats_TxMc,
	awc_RID_Stats_TxBc,
	awc_RID_Stats_TxUcFrags,
	awc_RID_Stats_TxUcPackets,
	awc_RID_Stats_TxBeacon,
	awc_RID_Stats_RxBeacon,
	awc_RID_Stats_TxSinColl,
	awc_RID_Stats_TxMulColl,
	awc_RID_Stats_DefersNo,
	awc_RID_Stats_DefersProt,
	awc_RID_Stats_DefersEngy,
	awc_RID_Stats_DupFram,
	awc_RID_Stats_RxFragDisc,
	awc_RID_Stats_TxAged,
	awc_RID_Stats_RxAged,
	awc_RID_Stats_LostSync_Max,
	awc_RID_Stats_LostSync_Mis,
	awc_RID_Stats_LostSync_Arl,
	awc_RID_Stats_LostSync_Dea,
	awc_RID_Stats_LostSync_Disa,
	awc_RID_Stats_LostSync_Tsf,
	awc_RID_Stats_HostTxMc,
	awc_RID_Stats_HostTxBc,
	awc_RID_Stats_HostTxUc,
	awc_RID_Stats_HostTxFail,
	awc_RID_Stats_HostRxMc,
	awc_RID_Stats_HostRxBc,
	awc_RID_Stats_HostRxUc,
	awc_RID_Stats_HostRxDiscar,
	awc_RID_Stats_HmacTxMc,
	awc_RID_Stats_HmacTxBc,
	awc_RID_Stats_HmacTxUc,
	awc_RID_Stats_HmacTxFail,
	awc_RID_Stats_HmacRxMc,
	awc_RID_Stats_HmacRxBc,
	awc_RID_Stats_HmacRxUc,
	awc_RID_Stats_HmacRxDisca,
	awc_RID_Stats_HmacRxAcce,
	awc_RID_Stats_SsidMismatch,
	awc_RID_Stats_ApMismatch,
	awc_RID_Stats_RatesMismatc,
	awc_RID_Stats_AuthReject,
	awc_RID_Stats_AuthTimeout,
	awc_RID_Stats_AssocReject,
	awc_RID_Stats_AssocTimeout,
	awc_RID_Stats_NewReason,
	awc_RID_Stats_AuthFail_1,
	awc_RID_Stats_AuthFail_2,
	awc_RID_Stats_AuthFail_3,
	awc_RID_Stats_AuthFail_4,
	awc_RID_Stats_AuthFail_5,
	awc_RID_Stats_AuthFail_6,
	awc_RID_Stats_AuthFail_7,
	awc_RID_Stats_AuthFail_8,
	awc_RID_Stats_AuthFail_9,
	awc_RID_Stats_AuthFail_10,
	awc_RID_Stats_AuthFail_11,
	awc_RID_Stats_AuthFail_12,
	awc_RID_Stats_AuthFail_13,
	awc_RID_Stats_AuthFail_14,
	awc_RID_Stats_AuthFail_15,
	awc_RID_Stats_AuthFail_16,
	awc_RID_Stats_AuthFail_17,
	awc_RID_Stats_AuthFail_18,
	awc_RID_Stats_AuthFail_19,
	awc_RID_Stats_RxMan,
	awc_RID_Stats_TxMan,
	awc_RID_Stats_RxRefresh,
	awc_RID_Stats_TxRefresh,
	awc_RID_Stats_RxPoll,
	awc_RID_Stats_TxPoll,
	awc_RID_Stats_HostRetries,
	awc_RID_Stats_LostSync_HostReq,
	awc_RID_Stats_HostTxBytes,
	awc_RID_Stats_HostRxBytes,
	awc_RID_Stats_ElapsedUsec,
	awc_RID_Stats_ElapsedSec,
	awc_RID_Stats_LostSyncBett,
	{0}
};



struct aironet4500_RID awc_Stats_delta_RID[]={
	awc_RID_Stats_delta_RidLen,
	awc_RID_Stats_delta_RxOverrunErr,
	awc_RID_Stats_delta_RxPlcpCrcErr,
	awc_RID_Stats_delta_RxPlcpFormat,
	awc_RID_Stats_delta_RxPlcpLength,
	awc_RID_Stats_delta_RxMacCrcErr,
	awc_RID_Stats_delta_RxMacCrcOk,
	awc_RID_Stats_delta_RxWepErr,
	awc_RID_Stats_delta_RxWepOk,
	awc_RID_Stats_delta_RetryLong,
	awc_RID_Stats_delta_RetryShort,
	awc_RID_Stats_delta_MaxRetries,
	awc_RID_Stats_delta_NoAck,
	awc_RID_Stats_delta_NoCts,
	awc_RID_Stats_delta_RxAck,
	awc_RID_Stats_delta_RxCts,
	awc_RID_Stats_delta_TxAck,
	awc_RID_Stats_delta_TxRts,
	awc_RID_Stats_delta_TxCts,
	awc_RID_Stats_delta_TxMc,
	awc_RID_Stats_delta_TxBc,
	awc_RID_Stats_delta_TxUcFrags,
	awc_RID_Stats_delta_TxUcPackets,
	awc_RID_Stats_delta_TxBeacon,
	awc_RID_Stats_delta_RxBeacon,
	awc_RID_Stats_delta_TxSinColl,
	awc_RID_Stats_delta_TxMulColl,
	awc_RID_Stats_delta_DefersNo,
	awc_RID_Stats_delta_DefersProt,
	awc_RID_Stats_delta_DefersEngy,
	awc_RID_Stats_delta_DupFram,
	awc_RID_Stats_delta_RxFragDisc,
	awc_RID_Stats_delta_TxAged,
	awc_RID_Stats_delta_RxAged,
	awc_RID_Stats_delta_LostSync_Max,
	awc_RID_Stats_delta_LostSync_Mis,
	awc_RID_Stats_delta_LostSync_Arl,
	awc_RID_Stats_delta_LostSync_Dea,
	awc_RID_Stats_delta_LostSync_Disa,
	awc_RID_Stats_delta_LostSync_Tsf,
	awc_RID_Stats_delta_HostTxMc,
	awc_RID_Stats_delta_HostTxBc,
	awc_RID_Stats_delta_HostTxUc,
	awc_RID_Stats_delta_HostTxFail,
	awc_RID_Stats_delta_HostRxMc,
	awc_RID_Stats_delta_HostRxBc,
	awc_RID_Stats_delta_HostRxUc,
	awc_RID_Stats_delta_HostRxDiscar,
	awc_RID_Stats_delta_HmacTxMc,
	awc_RID_Stats_delta_HmacTxBc,
	awc_RID_Stats_delta_HmacTxUc,
	awc_RID_Stats_delta_HmacTxFail,
	awc_RID_Stats_delta_HmacRxMc,
	awc_RID_Stats_delta_HmacRxBc,
	awc_RID_Stats_delta_HmacRxUc,
	awc_RID_Stats_delta_HmacRxDisca,
	awc_RID_Stats_delta_HmacRxAcce,
	awc_RID_Stats_delta_SsidMismatch,
	awc_RID_Stats_delta_ApMismatch,
	awc_RID_Stats_delta_RatesMismatc,
	awc_RID_Stats_delta_AuthReject,
	awc_RID_Stats_delta_AuthTimeout,
	awc_RID_Stats_delta_AssocReject,
	awc_RID_Stats_delta_AssocTimeout,
	awc_RID_Stats_delta_NewReason,
	awc_RID_Stats_delta_AuthFail_1,
	awc_RID_Stats_delta_AuthFail_2,
	awc_RID_Stats_delta_AuthFail_3,
	awc_RID_Stats_delta_AuthFail_4,
	awc_RID_Stats_delta_AuthFail_5,
	awc_RID_Stats_delta_AuthFail_6,
	awc_RID_Stats_delta_AuthFail_7,
	awc_RID_Stats_delta_AuthFail_8,
	awc_RID_Stats_delta_AuthFail_9,
	awc_RID_Stats_delta_AuthFail_10,
	awc_RID_Stats_delta_AuthFail_11,
	awc_RID_Stats_delta_AuthFail_12,
	awc_RID_Stats_delta_AuthFail_13,
	awc_RID_Stats_delta_AuthFail_14,
	awc_RID_Stats_delta_AuthFail_15,
	awc_RID_Stats_delta_AuthFail_16,
	awc_RID_Stats_delta_AuthFail_17,
	awc_RID_Stats_delta_AuthFail_18,
	awc_RID_Stats_delta_AuthFail_19,
	awc_RID_Stats_delta_RxMan,
	awc_RID_Stats_delta_TxMan,
	awc_RID_Stats_delta_RxRefresh,
	awc_RID_Stats_delta_TxRefresh,
	awc_RID_Stats_delta_RxPoll,
	awc_RID_Stats_delta_TxPoll,
	awc_RID_Stats_delta_HostRetries,
	awc_RID_Stats_delta_LostSync_HostReq,
	awc_RID_Stats_delta_HostTxBytes,
	awc_RID_Stats_delta_HostRxBytes,
	awc_RID_Stats_delta_ElapsedUsec,
	awc_RID_Stats_delta_ElapsedSec,
	awc_RID_Stats_delta_LostSyncBett,
	{0}
};

struct aironet4500_RID awc_Stats_clear_RID[]={
	awc_RID_Stats_clear_RidLen,
	awc_RID_Stats_clear_RxOverrunErr,
	awc_RID_Stats_clear_RxPlcpCrcErr,
	awc_RID_Stats_clear_RxPlcpFormat,
	awc_RID_Stats_clear_RxPlcpLength,
	awc_RID_Stats_clear_RxMacCrcErr,
	awc_RID_Stats_clear_RxMacCrcOk,
	awc_RID_Stats_clear_RxWepErr,
	awc_RID_Stats_clear_RxWepOk,
	awc_RID_Stats_clear_RetryLong,
	awc_RID_Stats_clear_RetryShort,
	awc_RID_Stats_clear_MaxRetries,
	awc_RID_Stats_clear_NoAck,
	awc_RID_Stats_clear_NoCts,
	awc_RID_Stats_clear_RxAck,
	awc_RID_Stats_clear_RxCts,
	awc_RID_Stats_clear_TxAck,
	awc_RID_Stats_clear_TxRts,
	awc_RID_Stats_clear_TxCts,
	awc_RID_Stats_clear_TxMc,
	awc_RID_Stats_clear_TxBc,
	awc_RID_Stats_clear_TxUcFrags,
	awc_RID_Stats_clear_TxUcPackets,
	awc_RID_Stats_clear_TxBeacon,
	awc_RID_Stats_clear_RxBeacon,
	awc_RID_Stats_clear_TxSinColl,
	awc_RID_Stats_clear_TxMulColl,
	awc_RID_Stats_clear_DefersNo,
	awc_RID_Stats_clear_DefersProt,
	awc_RID_Stats_clear_DefersEngy,
	awc_RID_Stats_clear_DupFram,
	awc_RID_Stats_clear_RxFragDisc,
	awc_RID_Stats_clear_TxAged,
	awc_RID_Stats_clear_RxAged,
	awc_RID_Stats_clear_LostSync_Max,
	awc_RID_Stats_clear_LostSync_Mis,
	awc_RID_Stats_clear_LostSync_Arl,
	awc_RID_Stats_clear_LostSync_Dea,
	awc_RID_Stats_clear_LostSync_Disa,
	awc_RID_Stats_clear_LostSync_Tsf,
	awc_RID_Stats_clear_HostTxMc,
	awc_RID_Stats_clear_HostTxBc,
	awc_RID_Stats_clear_HostTxUc,
	awc_RID_Stats_clear_HostTxFail,
	awc_RID_Stats_clear_HostRxMc,
	awc_RID_Stats_clear_HostRxBc,
	awc_RID_Stats_clear_HostRxUc,
	awc_RID_Stats_clear_HostRxDiscar,
	awc_RID_Stats_clear_HmacTxMc,
	awc_RID_Stats_clear_HmacTxBc,
	awc_RID_Stats_clear_HmacTxUc,
	awc_RID_Stats_clear_HmacTxFail,
	awc_RID_Stats_clear_HmacRxMc,
	awc_RID_Stats_clear_HmacRxBc,
	awc_RID_Stats_clear_HmacRxUc,
	awc_RID_Stats_clear_HmacRxDisca,
	awc_RID_Stats_clear_HmacRxAcce,
	awc_RID_Stats_clear_SsidMismatch,
	awc_RID_Stats_clear_ApMismatch,
	awc_RID_Stats_clear_RatesMismatc,
	awc_RID_Stats_clear_AuthReject,
	awc_RID_Stats_clear_AuthTimeout,
	awc_RID_Stats_clear_AssocReject,
	awc_RID_Stats_clear_AssocTimeout,
	awc_RID_Stats_clear_NewReason,
	awc_RID_Stats_clear_AuthFail_1,
	awc_RID_Stats_clear_AuthFail_2,
	awc_RID_Stats_clear_AuthFail_3,
	awc_RID_Stats_clear_AuthFail_4,
	awc_RID_Stats_clear_AuthFail_5,
	awc_RID_Stats_clear_AuthFail_6,
	awc_RID_Stats_clear_AuthFail_7,
	awc_RID_Stats_clear_AuthFail_8,
	awc_RID_Stats_clear_AuthFail_9,
	awc_RID_Stats_clear_AuthFail_10,
	awc_RID_Stats_clear_AuthFail_11,
	awc_RID_Stats_clear_AuthFail_12,
	awc_RID_Stats_clear_AuthFail_13,
	awc_RID_Stats_clear_AuthFail_14,
	awc_RID_Stats_clear_AuthFail_15,
	awc_RID_Stats_clear_AuthFail_16,
	awc_RID_Stats_clear_AuthFail_17,
	awc_RID_Stats_clear_AuthFail_18,
	awc_RID_Stats_clear_AuthFail_19,
	awc_RID_Stats_clear_RxMan,
	awc_RID_Stats_clear_TxMan,
	awc_RID_Stats_clear_RxRefresh,
	awc_RID_Stats_clear_TxRefresh,
	awc_RID_Stats_clear_RxPoll,
	awc_RID_Stats_clear_TxPoll,
	awc_RID_Stats_clear_HostRetries,
	awc_RID_Stats_clear_LostSync_HostReq,
	awc_RID_Stats_clear_HostTxBytes,
	awc_RID_Stats_clear_HostRxBytes,
	awc_RID_Stats_clear_ElapsedUsec,
	awc_RID_Stats_clear_ElapsedSec,
	awc_RID_Stats_clear_LostSyncBett,
	{0}
};
#ifdef AWC_USE_16BIT_STATS
struct aironet4500_RID awc_Stats16_RID[]={
	awc_RID_Stats16_RidLen,
	awc_RID_Stats16_RxOverrunErr,
	awc_RID_Stats16_RxPlcpCrcErr,
	awc_RID_Stats16_RxPlcpFormat,
	awc_RID_Stats16_RxPlcpLength,
	awc_RID_Stats16_RxMacCrcErr,
	awc_RID_Stats16_RxMacCrcOk,
	awc_RID_Stats16_RxWepErr,
	awc_RID_Stats16_RxWepOk,
	awc_RID_Stats16_RetryLong,
	awc_RID_Stats16_RetryShort,
	awc_RID_Stats16_MaxRetries,
	awc_RID_Stats16_NoAck,
	awc_RID_Stats16_NoCts,
	awc_RID_Stats16_RxAck,
	awc_RID_Stats16_RxCts,
	awc_RID_Stats16_TxAck,
	awc_RID_Stats16_TxRts,
	awc_RID_Stats16_TxCts,
	awc_RID_Stats16_TxMc,
	awc_RID_Stats16_TxBc,
	awc_RID_Stats16_TxUcFrags,
	awc_RID_Stats16_TxUcPackets,
	awc_RID_Stats16_TxBeacon,
	awc_RID_Stats16_RxBeacon,
	awc_RID_Stats16_TxSinColl,
	awc_RID_Stats16_TxMulColl,
	awc_RID_Stats16_DefersNo,
	awc_RID_Stats16_DefersProt,
	awc_RID_Stats16_DefersEngy,
	awc_RID_Stats16_DupFram,
	awc_RID_Stats16_RxFragDisc,
	awc_RID_Stats16_TxAged,
	awc_RID_Stats16_RxAged,
	awc_RID_Stats16_LostSync_Max,
	awc_RID_Stats16_LostSync_Mis,
	awc_RID_Stats16_LostSync_Arl,
	awc_RID_Stats16_LostSync_Dea,
	awc_RID_Stats16_LostSync_Disa,
	awc_RID_Stats16_LostSync_Tsf,
	awc_RID_Stats16_HostTxMc,
	awc_RID_Stats16_HostTxBc,
	awc_RID_Stats16_HostTxUc,
	awc_RID_Stats16_HostTxFail,
	awc_RID_Stats16_HostRxMc,
	awc_RID_Stats16_HostRxBc,
	awc_RID_Stats16_HostRxUc,
	awc_RID_Stats16_HostRxDiscar,
	awc_RID_Stats16_HmacTxMc,
	awc_RID_Stats16_HmacTxBc,
	awc_RID_Stats16_HmacTxUc,
	awc_RID_Stats16_HmacTxFail,
	awc_RID_Stats16_HmacRxMc,
	awc_RID_Stats16_HmacRxBc,
	awc_RID_Stats16_HmacRxUc,
	awc_RID_Stats16_HmacRxDisca,
	awc_RID_Stats16_HmacRxAcce,
	awc_RID_Stats16_SsidMismatch,
	awc_RID_Stats16_ApMismatch,
	awc_RID_Stats16_RatesMismatc,
	awc_RID_Stats16_AuthReject,
	awc_RID_Stats16_AuthTimeout,
	awc_RID_Stats16_AssocReject,
	awc_RID_Stats16_AssocTimeout,
	awc_RID_Stats16_NewReason,
	awc_RID_Stats16_AuthFail_1,
	awc_RID_Stats16_AuthFail_2,
	awc_RID_Stats16_AuthFail_3,
	awc_RID_Stats16_AuthFail_4,
	awc_RID_Stats16_AuthFail_5,
	awc_RID_Stats16_AuthFail_6,
	awc_RID_Stats16_AuthFail_7,
	awc_RID_Stats16_AuthFail_8,
	awc_RID_Stats16_AuthFail_9,
	awc_RID_Stats16_AuthFail_10,
	awc_RID_Stats16_AuthFail_11,
	awc_RID_Stats16_AuthFail_12,
	awc_RID_Stats16_AuthFail_13,
	awc_RID_Stats16_AuthFail_14,
	awc_RID_Stats16_AuthFail_15,
	awc_RID_Stats16_AuthFail_16,
	awc_RID_Stats16_AuthFail_17,
	awc_RID_Stats16_AuthFail_18,
	awc_RID_Stats16_AuthFail_19,
	awc_RID_Stats16_RxMan,
	awc_RID_Stats16_TxMan,
	awc_RID_Stats16_RxRefresh,
	awc_RID_Stats16_TxRefresh,
	awc_RID_Stats16_RxPoll,
	awc_RID_Stats16_TxPoll,
	awc_RID_Stats16_HostRetries,
	awc_RID_Stats16_LostSync_HostReq,
	awc_RID_Stats16_HostTxBytes,
	awc_RID_Stats16_HostRxBytes,
	awc_RID_Stats16_ElapsedUsec,
	awc_RID_Stats16_ElapsedSec,
	awc_RID_Stats16_LostSyncBett,
	{0}
};

struct aironet4500_RID awc_Stats16_delta_RID[]={
	awc_RID_Stats16_delta_RidLen,
	awc_RID_Stats16_delta_RxOverrunErr,
	awc_RID_Stats16_delta_RxPlcpCrcErr,
	awc_RID_Stats16_delta_RxPlcpFormat,
	awc_RID_Stats16_delta_RxPlcpLength,
	awc_RID_Stats16_delta_RxMacCrcErr,
	awc_RID_Stats16_delta_RxMacCrcOk,
	awc_RID_Stats16_delta_RxWepErr,
	awc_RID_Stats16_delta_RxWepOk,
	awc_RID_Stats16_delta_RetryLong,
	awc_RID_Stats16_delta_RetryShort,
	awc_RID_Stats16_delta_MaxRetries,
	awc_RID_Stats16_delta_NoAck,
	awc_RID_Stats16_delta_NoCts,
	awc_RID_Stats16_delta_RxAck,
	awc_RID_Stats16_delta_RxCts,
	awc_RID_Stats16_delta_TxAck,
	awc_RID_Stats16_delta_TxRts,
	awc_RID_Stats16_delta_TxCts,
	awc_RID_Stats16_delta_TxMc,
	awc_RID_Stats16_delta_TxBc,
	awc_RID_Stats16_delta_TxUcFrags,
	awc_RID_Stats16_delta_TxUcPackets,
	awc_RID_Stats16_delta_TxBeacon,
	awc_RID_Stats16_delta_RxBeacon,
	awc_RID_Stats16_delta_TxSinColl,
	awc_RID_Stats16_delta_TxMulColl,
	awc_RID_Stats16_delta_DefersNo,
	awc_RID_Stats16_delta_DefersProt,
	awc_RID_Stats16_delta_DefersEngy,
	awc_RID_Stats16_delta_DupFram,
	awc_RID_Stats16_delta_RxFragDisc,
	awc_RID_Stats16_delta_TxAged,
	awc_RID_Stats16_delta_RxAged,
	awc_RID_Stats16_delta_LostSync_Max,
	awc_RID_Stats16_delta_LostSync_Mis,
	awc_RID_Stats16_delta_LostSync_Arl,
	awc_RID_Stats16_delta_LostSync_Dea,
	awc_RID_Stats16_delta_LostSync_Disa,
	awc_RID_Stats16_delta_LostSync_Tsf,
	awc_RID_Stats16_delta_HostTxMc,
	awc_RID_Stats16_delta_HostTxBc,
	awc_RID_Stats16_delta_HostTxUc,
	awc_RID_Stats16_delta_HostTxFail,
	awc_RID_Stats16_delta_HostRxMc,
	awc_RID_Stats16_delta_HostRxBc,
	awc_RID_Stats16_delta_HostRxUc,
	awc_RID_Stats16_delta_HostRxDiscar,
	awc_RID_Stats16_delta_HmacTxMc,
	awc_RID_Stats16_delta_HmacTxBc,
	awc_RID_Stats16_delta_HmacTxUc,
	awc_RID_Stats16_delta_HmacTxFail,
	awc_RID_Stats16_delta_HmacRxMc,
	awc_RID_Stats16_delta_HmacRxBc,
	awc_RID_Stats16_delta_HmacRxUc,
	awc_RID_Stats16_delta_HmacRxDisca,
	awc_RID_Stats16_delta_HmacRxAcce,
	awc_RID_Stats16_delta_SsidMismatch,
	awc_RID_Stats16_delta_ApMismatch,
	awc_RID_Stats16_delta_RatesMismatc,
	awc_RID_Stats16_delta_AuthReject,
	awc_RID_Stats16_delta_AuthTimeout,
	awc_RID_Stats16_delta_AssocReject,
	awc_RID_Stats16_delta_AssocTimeout,
	awc_RID_Stats16_delta_NewReason,
	awc_RID_Stats16_delta_AuthFail_1,
	awc_RID_Stats16_delta_AuthFail_2,
	awc_RID_Stats16_delta_AuthFail_3,
	awc_RID_Stats16_delta_AuthFail_4,
	awc_RID_Stats16_delta_AuthFail_5,
	awc_RID_Stats16_delta_AuthFail_6,
	awc_RID_Stats16_delta_AuthFail_7,
	awc_RID_Stats16_delta_AuthFail_8,
	awc_RID_Stats16_delta_AuthFail_9,
	awc_RID_Stats16_delta_AuthFail_10,
	awc_RID_Stats16_delta_AuthFail_11,
	awc_RID_Stats16_delta_AuthFail_12,
	awc_RID_Stats16_delta_AuthFail_13,
	awc_RID_Stats16_delta_AuthFail_14,
	awc_RID_Stats16_delta_AuthFail_15,
	awc_RID_Stats16_delta_AuthFail_16,
	awc_RID_Stats16_delta_AuthFail_17,
	awc_RID_Stats16_delta_AuthFail_18,
	awc_RID_Stats16_delta_AuthFail_19,
	awc_RID_Stats16_delta_RxMan,
	awc_RID_Stats16_delta_TxMan,
	awc_RID_Stats16_delta_RxRefresh,
	awc_RID_Stats16_delta_TxRefresh,
	awc_RID_Stats16_delta_RxPoll,
	awc_RID_Stats16_delta_TxPoll,
	awc_RID_Stats16_delta_HostRetries,
	awc_RID_Stats16_delta_LostSync_HostReq,
	awc_RID_Stats16_delta_HostTxBytes,
	awc_RID_Stats16_delta_HostRxBytes,
	awc_RID_Stats16_delta_ElapsedUsec,
	awc_RID_Stats16_delta_ElapsedSec,
	awc_RID_Stats16_delta_LostSyncBett,
	{0}
};

struct aironet4500_RID awc_Stats16_clear_RID[]={
	awc_RID_Stats16_clear_RidLen,
	awc_RID_Stats16_clear_RxOverrunErr,
	awc_RID_Stats16_clear_RxPlcpCrcErr,
	awc_RID_Stats16_clear_RxPlcpFormat,
	awc_RID_Stats16_clear_RxPlcpLength,
	awc_RID_Stats16_clear_RxMacCrcErr,
	awc_RID_Stats16_clear_RxMacCrcOk,
	awc_RID_Stats16_clear_RxWepErr,
	awc_RID_Stats16_clear_RxWepOk,
	awc_RID_Stats16_clear_RetryLong,
	awc_RID_Stats16_clear_RetryShort,
	awc_RID_Stats16_clear_MaxRetries,
	awc_RID_Stats16_clear_NoAck,
	awc_RID_Stats16_clear_NoCts,
	awc_RID_Stats16_clear_RxAck,
	awc_RID_Stats16_clear_RxCts,
	awc_RID_Stats16_clear_TxAck,
	awc_RID_Stats16_clear_TxRts,
	awc_RID_Stats16_clear_TxCts,
	awc_RID_Stats16_clear_TxMc,
	awc_RID_Stats16_clear_TxBc,
	awc_RID_Stats16_clear_TxUcFrags,
	awc_RID_Stats16_clear_TxUcPackets,
	awc_RID_Stats16_clear_TxBeacon,
	awc_RID_Stats16_clear_RxBeacon,
	awc_RID_Stats16_clear_TxSinColl,
	awc_RID_Stats16_clear_TxMulColl,
	awc_RID_Stats16_clear_DefersNo,
	awc_RID_Stats16_clear_DefersProt,
	awc_RID_Stats16_clear_DefersEngy,
	awc_RID_Stats16_clear_DupFram,
	awc_RID_Stats16_clear_RxFragDisc,
	awc_RID_Stats16_clear_TxAged,
	awc_RID_Stats16_clear_RxAged,
	awc_RID_Stats16_clear_LostSync_Max,
	awc_RID_Stats16_clear_LostSync_Mis,
	awc_RID_Stats16_clear_LostSync_Arl,
	awc_RID_Stats16_clear_LostSync_Dea,
	awc_RID_Stats16_clear_LostSync_Disa,
	awc_RID_Stats16_clear_LostSync_Tsf,
	awc_RID_Stats16_clear_HostTxMc,
	awc_RID_Stats16_clear_HostTxBc,
	awc_RID_Stats16_clear_HostTxUc,
	awc_RID_Stats16_clear_HostTxFail,
	awc_RID_Stats16_clear_HostRxMc,
	awc_RID_Stats16_clear_HostRxBc,
	awc_RID_Stats16_clear_HostRxUc,
	awc_RID_Stats16_clear_HostRxDiscar,
	awc_RID_Stats16_clear_HmacTxMc,
	awc_RID_Stats16_clear_HmacTxBc,
	awc_RID_Stats16_clear_HmacTxUc,
	awc_RID_Stats16_clear_HmacTxFail,
	awc_RID_Stats16_clear_HmacRxMc,
	awc_RID_Stats16_clear_HmacRxBc,
	awc_RID_Stats16_clear_HmacRxUc,
	awc_RID_Stats16_clear_HmacRxDisca,
	awc_RID_Stats16_clear_HmacRxAcce,
	awc_RID_Stats16_clear_SsidMismatch,
	awc_RID_Stats16_clear_ApMismatch,
	awc_RID_Stats16_clear_RatesMismatc,
	awc_RID_Stats16_clear_AuthReject,
	awc_RID_Stats16_clear_AuthTimeout,
	awc_RID_Stats16_clear_AssocReject,
	awc_RID_Stats16_clear_AssocTimeout,
	awc_RID_Stats16_clear_NewReason,
	awc_RID_Stats16_clear_AuthFail_1,
	awc_RID_Stats16_clear_AuthFail_2,
	awc_RID_Stats16_clear_AuthFail_3,
	awc_RID_Stats16_clear_AuthFail_4,
	awc_RID_Stats16_clear_AuthFail_5,
	awc_RID_Stats16_clear_AuthFail_6,
	awc_RID_Stats16_clear_AuthFail_7,
	awc_RID_Stats16_clear_AuthFail_8,
	awc_RID_Stats16_clear_AuthFail_9,
	awc_RID_Stats16_clear_AuthFail_10,
	awc_RID_Stats16_clear_AuthFail_11,
	awc_RID_Stats16_clear_AuthFail_12,
	awc_RID_Stats16_clear_AuthFail_13,
	awc_RID_Stats16_clear_AuthFail_14,
	awc_RID_Stats16_clear_AuthFail_15,
	awc_RID_Stats16_clear_AuthFail_16,
	awc_RID_Stats16_clear_AuthFail_17,
	awc_RID_Stats16_clear_AuthFail_18,
	awc_RID_Stats16_clear_AuthFail_19,
	awc_RID_Stats16_clear_RxMan,
	awc_RID_Stats16_clear_TxMan,
	awc_RID_Stats16_clear_RxRefresh,
	awc_RID_Stats16_clear_TxRefresh,
	awc_RID_Stats16_clear_RxPoll,
	awc_RID_Stats16_clear_TxPoll,
	awc_RID_Stats16_clear_HostRetries,
	awc_RID_Stats16_clear_LostSync_HostReq,
	awc_RID_Stats16_clear_HostTxBytes,
	awc_RID_Stats16_clear_HostRxBytes,
	awc_RID_Stats16_clear_ElapsedUsec,
	awc_RID_Stats16_clear_ElapsedSec,
	awc_RID_Stats16_clear_LostSyncBett,
	{0}
};

#endif

struct awc_rid_dir awc_rids[]={
	// following MUST be consistent with awc_rids_setup !!!
   {&aironet4500_RID_Select_General_Config,sizeof(awc_gen_RID) / sizeof(struct aironet4500_RID)  ,awc_gen_RID , NULL, NULL,0 },
   {&aironet4500_RID_Select_SSID_list, sizeof(awc_SSID_RID) / sizeof(struct aironet4500_RID) , awc_SSID_RID , NULL, NULL,0 },
   {&aironet4500_RID_Select_AP_list, sizeof(awc_AP_List_RID) / sizeof(struct aironet4500_RID) , awc_AP_List_RID , NULL, NULL,0 },
   {&aironet4500_RID_Select_Driver_name, sizeof(awc_Dname_RID) / sizeof(struct aironet4500_RID) , awc_Dname_RID , NULL, NULL,0 },
   {&aironet4500_RID_Select_Encapsulation, sizeof(awc_enc_RID) / sizeof(struct aironet4500_RID) , awc_enc_RID , NULL, NULL,0 },
   {&aironet4500_RID_Select_Active_Config, sizeof(awc_act_RID) / sizeof(struct aironet4500_RID) , awc_act_RID , NULL, NULL,0 },
   {&aironet4500_RID_Select_Capabilities, sizeof(awc_Cap_RID) / sizeof(struct aironet4500_RID) , awc_Cap_RID , NULL, NULL,0 },
   {&aironet4500_RID_Select_Status, sizeof(awc_Status_RID) / sizeof(struct aironet4500_RID) , awc_Status_RID , NULL, NULL,0 },
   {&aironet4500_RID_Select_AP_Info, sizeof(awc_AP_RID) / sizeof(struct aironet4500_RID) , awc_AP_RID , NULL, NULL,0 },
   {&aironet4500_RID_Select_32_stats, sizeof(awc_Stats_RID) / sizeof(struct aironet4500_RID) , awc_Stats_RID , NULL, NULL,0 },
   {&aironet4500_RID_Select_32_stats_delta, sizeof(awc_Stats_delta_RID) / sizeof(struct aironet4500_RID) , awc_Stats_delta_RID , NULL, NULL,0 },
   {&aironet4500_RID_Select_32_stats_clear, sizeof(awc_Stats_clear_RID) / sizeof(struct aironet4500_RID) , awc_Stats_clear_RID , NULL, NULL,0 },
   {&aironet4500_RID_Select_WEP_volatile, sizeof(awc_WEPv_RID) / sizeof(struct aironet4500_RID) , awc_WEPv_RID , NULL, NULL,0 },
   {&aironet4500_RID_Select_WEP_nonvolatile, sizeof(awc_WEPnv_RID) / sizeof(struct aironet4500_RID) , awc_WEPnv_RID , NULL, NULL,0 },
   {&aironet4500_RID_Select_Modulation, sizeof(awc_Modulation_RID) / sizeof(struct aironet4500_RID) , awc_Modulation_RID , NULL, NULL,0 },

#ifdef AWC_USE_16BIT_STATS
   {&aironet4500_RID_Select_16_stats, sizeof(awc_Stats16_RID) / sizeof(struct aironet4500_RID) , awc_Stats16_RID , NULL, NULL,0 },
   {&aironet4500_RID_Select_16_stats_delta, sizeof(awc_Stats16_delta_RID) / sizeof(struct aironet4500_RID) , awc_Stats16_delta_RID , NULL, NULL,0 },
   {&aironet4500_RID_Select_16_stats_clear, sizeof(awc_Stats16_clear_RID) / sizeof(struct aironet4500_RID) , awc_Stats16_clear_RID , NULL, NULL,0 },
#else 
   {NULL},{NULL},{NULL},
#endif	
 
   {0} 


};


int awc_nof_rids = (sizeof(awc_rids) / sizeof(struct awc_rid_dir)) -1;


int awc_rids_setup(struct net_device * dev){

	struct awc_private * priv = (struct awc_private *) dev->priv;
	int i=0;
	while ( i < AWC_NOF_RIDS){
		if (awc_rids[i].selector)
			memcpy(&priv->rid_dir[i],&awc_rids[i],sizeof(priv->rid_dir[0]) );
		else priv->rid_dir[i].selector = NULL;
		i++;
	}
	for (i=0; i< AWC_NOF_RIDS && i < awc_nof_rids; i++){
		priv->rid_dir[i].dev = dev;
	};
	
	// following MUST be consistent with awc_rids !!!
 	priv->rid_dir[0].buff = &priv->config; // card RID mirrors
	priv->rid_dir[1].buff = &priv->SSIDs;
	priv->rid_dir[2].buff = &priv->fixed_APs;
     	priv->rid_dir[3].buff = &priv->driver_name;
      	priv->rid_dir[4].buff = &priv->enc_trans;
	priv->rid_dir[5].buff = &priv->general_config; //      	
	priv->rid_dir[6].buff = &priv->capabilities;
 	priv->rid_dir[7].buff = &priv->status;
  	priv->rid_dir[8].buff = &priv->AP;
   	priv->rid_dir[9].buff = &priv->statistics;
    	priv->rid_dir[10].buff = &priv->statistics_delta;
     	priv->rid_dir[11].buff = &priv->statistics_delta_clear;
	priv->rid_dir[12].buff = &priv->wep_volatile;
	priv->rid_dir[13].buff = &priv->wep_nonvolatile;
	priv->rid_dir[14].buff = &priv->modulation;

      	priv->rid_dir[15].buff = &priv->statistics16;
	priv->rid_dir[16].buff = &priv->statistics16_delta;
 	priv->rid_dir[17].buff = &priv->statistics16_delta_clear;
                       	
 	priv->rid_dir[0].bufflen = sizeof(priv->config); // card RID mirrors
	priv->rid_dir[1].bufflen = sizeof(priv->SSIDs);
	priv->rid_dir[2].bufflen = sizeof(priv->fixed_APs);
     	priv->rid_dir[3].bufflen = sizeof(priv->driver_name);
      	priv->rid_dir[4].bufflen = sizeof(priv->enc_trans);
	priv->rid_dir[5].bufflen = sizeof(priv->general_config); //
	priv->rid_dir[6].bufflen = sizeof(priv->capabilities);
 	priv->rid_dir[7].bufflen = sizeof(priv->status);
  	priv->rid_dir[8].bufflen = sizeof(priv->AP);
   	priv->rid_dir[9].bufflen = sizeof(priv->statistics);
    	priv->rid_dir[10].bufflen = sizeof(priv->statistics_delta);
     	priv->rid_dir[11].bufflen = sizeof(priv->statistics_delta_clear);
	priv->rid_dir[12].bufflen = sizeof(priv->wep_volatile);
	priv->rid_dir[13].bufflen = sizeof(priv->wep_nonvolatile);
	priv->rid_dir[14].bufflen = sizeof(priv->modulation);

      	priv->rid_dir[15].bufflen = sizeof(priv->statistics16);
	priv->rid_dir[16].bufflen = sizeof(priv->statistics16_delta);
 	priv->rid_dir[17].bufflen = sizeof(priv->statistics16_delta_clear);

	return 0;

};





