/*
* AUTOMATICALLY GENERATED FILE, DO NOT EDIT!
*
* $FreeBSD$
*/
struct pci_device_information
{
	int		id;
	char	*desc;
};
struct pci_vendor_information 
{
	int				id;
	char				*desc;
	struct pci_device_information	*devices;
};
static struct pci_device_information pci_device_0675[] = {
	{0x1700,"IS64PH ISDN Adapter"},
	{0x1702,"IS64PH ISDN Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_0e11[] = {
	{0x0001,"PCI to EISA Bridge"},
	{0x0002,"ISA Bridge"},
	{0x0508,"Neteligent 4/16 TR PCI UTP/STP Controller"},
	{0x1000,"Model 1000 Triflex/PCI CPU Bridge"},
	{0x2000,"Model 2000 Triflex/PCI CPU Bridge"},
	{0x3032,"QVision 1280/p v0 GUI Accelerator"},
	{0x3033,"QVision 1280/p v1 GUI Accelerator"},
	{0x3034,"QVision 1280/p v2 GUI Accelerator"},
	{0x4000,"4000 Triflex/PCI CPU Bridge"},
	{0x6010,"Model 6010 HotPlug PCI Bridge"},
	{0x7020,"USB Controller"},
	{0xa0ec,"Fibre Channel Host Controller"},
	{0xa0f0,"Advanced System Management Controller"},
	{0xa0f3,"Triflex PCI to ISA PnP Bridge"},
	{0xa0f7,"PCI Hotplug Controller"},
	{0xa0f8,"USB Open Host Controller"},
	{0xae10,"Smart-2 Array Controller"},
	{0xae29,"MIS-L PCI to ISA Bridge"},
	{0xae2a,"MPC CPU to PCI Bridge"},
	{0xae2b,"MIS-E PCI to ISA PnP Bridge"},
	{0xae31,"System Management Controller"},
	{0xae32,"Netelligent 10/100 TX"},
	{0xae33,"Triflex Dual EIDE Controller"},
	{0xae34,"Netelligent 10 T"},
	{0xae35,"Integrated NetFlex 3/P"},
	{0xae40,"Dual Port Netelligent 10/100 TX"},
	{0xae43,"ProLiant Integrated Netelligent 10/100 TX"},
	{0xae69,"CETUS-L PCI to ISA Bridge"},
	{0xae6c,"DRACO PCI Bridge"},
	{0xae6d,"NorthStar CPU to PCI Bridge"},
	{0xb011,"Dual Port Netelligent 10/100 TX"},
	{0xb012,"Netelligent 10 T/2"},
	{0xb030,"Netelligent 10/100TX"},
	{0xb04a,"10/100TX WOL UTP Controller"},
	{0xb0c6,"10/100TX Embedded WOL UTP Controller"},
	{0xb0d7,"NC3121 rev. A & B"},
	{0xb196,"Conexant SoftK56 Modem"},
	{0xf130,"ThunderLAN / NetFlex-3/P"},
	{0xf150,"NetFlex 3/P w/ BNC"},
	{0, 0}
};
static struct pci_device_information pci_device_1000[] = {
	{0x0001,"53C810 Fast/Narrow SCSI I/O Cntrlr"},
	{0x0002,"53C820 Fast-wide SCSI"},
	{0x0003,"53C825 Fast-wide SCSI"},
	{0x0004,"53C815 Fast SCSI"},
	{0x0005,"53C810AP Fast SCSI"},
	{0x0006,"53C860 Ultra SCSI/Narrow"},
	{0x000a,"53C1510"},
	{0x000b,"53C896 dual-channel Ultra-2 Wide SCSI"},
	{0x000c,"SYM53C895 Ultra-2 Wide SCSI"},
	{0x000d,"53C885 Ultra Wide SCSI, Ethernet"},
	{0x000f,"53C875, 53C876 Ultra-Wide SCSI (53C876 is dual-channel)"},
	{0x0012,"53C895A Ultra-2 Wide SCSI"},
	{0x0020,"53C1010-33 PCI to Dual Channel Ultra3 SCSI Ctrlr"},
	{0x008f,"53C875J Ultra Wide SCSI"},
	{0x0701,"53C885 10/100 MBit Ethernet"},
	{0x0702,"Gigabit Ethernet Controller"},
	{0x0901,"61C102 USB Controller"},
	{0x1000,"63C815 Fast SCSI Controller"},
	{0, 0}
};
static struct pci_device_information pci_device_1001[] = {
	{0x0010,"ispLSI1032E PCI 1616, 16 TTL-IN, 16 TTL-OUT"},
	{0x0011,"ispLSI1032E OPTO-PCI, 16 IN / 16 OUT 24 VDC"},
	{0x0012,"ispLSI1032E PCI-AD, PCI-ADDA analog I/O-card"},
	{0x0013,"ispLSI1032E PCI-OptoRel, PCI-Relais 16 Relais & Opto"},
	{0x0014,"ispLSI1032E Timer, Pulse & Counter-card 16..32 bit"},
	{0x0015,"ispLSI1032E PCI-DAC416, 4 channel D/A16bit precision"},
	{0x0016,"ispLSI1032E PCI-MFB high-speed analog I/O"},
	{0x0017,"ispLSI1032E PROTO-3 PCI, digital I/O with chipselect"},
	{0, 0}
};
static struct pci_device_information pci_device_1002[] = {
	{0x4158,"68800AX Mach 32"},
	{0x4354,"215CT222 Mach 64 CT"},
	{0x4358,"210888CX Mach64 CX"},
	{0x4554,"Mach64 ET"},
	{0x4654,"Mach64 VT"},
	{0x4742,"Rage 3D Pro AGP 2x"},
	{0x4744,"Rage 3D Pro AGP 1x"},
	{0x4747,"Rage 3D Pro"},
	{0x4749,"3D RAGE PRO 2X AGP ATI ALL IN WONDER PRO (8MB)"},
	{0x474c,"Rage XC PCI-66"},
	{0x474d,"Rage XL AGP 2x"},
	{0x474e,"Rage XC AGP 2x"},
	{0x474f,"Rage XL PCI-66"},
	{0x4750,"Rage 3D Pro PCI Graphics Accelerator"},
	{0x4751,"Rage 3D Pro PCI"},
	{0x4753,"Rage XC PCI"},
	{0x4754,"Mach 64 GT Rage 3D II Graphics Accelerator"},
	{0x4755,"Rage 3D II+"},
	{0x4756,"Rage 3D IIC PCI Graphics Accelerator"},
	{0x4757,"Rage 3D IIC AGP"},
	{0x4758,"210888GX Mach 64 GX (WinTurbo)"},
	{0x4759,"Rage 3D IIC"},
	{0x475a,"Rage 3D IIC AGP"},
	{0x4c42,"Rage 3D LT Pro AGP 133 MHz"},
	{0x4c44,"Rage 3D LT Pro AGP 66 MHz"},
	{0x4c46,"Mobility M3 AGP 2x"},
	{0x4c47,"Rage 3D LT-G"},
	{0x4c49,"Rage 3D LT Pro PCI"},
	{0x4c4d,"Rage P/M Mobility AGP 2x"},
	{0x4c4e,"Rage L Mobility AGP 2x"},
	{0x4c50,"Rage 3D LT Pro PCI"},
	{0x4c51,"Rage 3D LT Pro PCI"},
	{0x4c52,"Rage P/M Mobility PCI"},
	{0x4c53,"Rage L Mobility PCI"},
	{0x4c54,"Mach 64 LT"},
	{0x5041,"Rage 128 Pro PCI"},
	{0x5042,"Rage 128 Pro AGP 2x"},
	{0x5043,"Rage 128 Pro AGP 4x"},
	{0x5044,"Rage 128 Pro PCI (TMDS)"},
	{0x5045,"Rage 128 Pro AGP 2x (TMDS)"},
	{0x5046,"Rage Fury MAXX AGP4x"},
	{0x5047,"Rage 128 Pro PCI"},
	{0x5048,"Rage 128 Pro AGP 2x"},
	{0x5049,"Rage 128 Pro AGP 4x"},
	{0x504a,"Rage 128 Pro PCI"},
	{0x5245,"Rage 128 GL PCI"},
	{0x5246,"Rage 128 GL AGP 2x"},
	{0x524b,"Rage 128 VR PCI"},
	{0x524c,"Rage 128 VR AGP 2x"},
	{0x5345,"Rage 128 4x PCI"},
	{0x5346,"Rage 128 4x AGP 2x"},
	{0x5347,"Rage 128 4x AGP 4x"},
	{0x5348,"Rage 128 4x"},
	{0x5354,"Mach 64 ST"},
	{0x5654,"215VT222 Mach 64 VT VIDEO XPRESSION"},
	{0x5655,"Mach 64 VT3"},
	{0x5656,"Mach 64 VT4 PCI"},
	{0, 0}
};
static struct pci_device_information pci_device_1003[] = {
	{0x0201,"US201 Graphics Cntrlr"},
	{0, 0}
};
static struct pci_device_information pci_device_1004[] = {
	{0x0005,"82C591/2-FC1 CPU Bridge"},
	{0x0006,"82C593 ISA Bridge"},
	{0x0007,"82C594 Wildcat System Controller"},
	{0x0008,"82C596/597 Wildcat ISA Bridge"},
	{0x000c,"82C541"},
	{0x000d,"82C543"},
	{0x0100,"CPU to PCI Bridge for notebook"},
	{0x0101,"82C532 Peripheral Controller"},
	{0x0102,"82C534 PCI to PCI Bridge"},
	{0x0103,"82C538 PCI to ISA Bridge"},
	{0x0104,"82C535 Host Bridge"},
	{0x0105,"82C147 IrDA Controller"},
	{0x0200,"82C975 RISC GUI Accelerator"},
	{0x0280,"82C925 RISC GUI Accelerator"},
	{0x0304,"ThunderBird QSound PCI Audio"},
	{0x0305,"ThunderBird Gameport device"},
	{0x0306,"ThunderBird PCI Audio Support Registers"},
	{0x0702,"VAS96011 Golden Gate II"},
	{0, 0}
};
static struct pci_device_information pci_device_1005[] = {
	{0x2064,"ALG2032/2064"},
	{0x2128,"ALG2364A"},
	{0x2301,"ALG2301 GUI Accelerator"},
	{0x2302,"ALG2302 GUI Accelerator"},
	{0x2364,"AL2364 GUI Accelerator"},
	{0x2464,"ALG2364A"},
	{0x2501,"ALG2564A/25128A"},
	{0, 0}
};
static struct pci_device_information pci_device_100b[] = {
	{0x0001,"DP83810 10/100 Ethernet MAC"},
	{0x0002,"PC87415 PCI-IDE DMA Master Mode Interface Ctrlr"},
	{0x000f,"OHCI Compliant FireWire Controller"},
	{0x0011,"PCI System I/O"},
	{0x0012,"USB Controller"},
	{0xd001,"PC87410 PCI-IDE Interface"},
	{0, 0}
};
static struct pci_device_information pci_device_100c[] = {
	{0x3202,"ET4000W32P-A GUI Accelerator"},
	{0x3205,"ET4000W32P-B GUI Accelerator"},
	{0x3206,"ET4000W32P-C GUI Accelerator"},
	{0x3207,"ET4000W32P-D GUI Accelerator"},
	{0x3208,"ET6000 Graphics/Multimedia Engine"},
	{0x4702,"ET6300"},
	{0, 0}
};
static struct pci_device_information pci_device_100e[] = {
	{0x9000,"P9000 WeitekPower GUI Accelerator"},
	{0x9001,"P9000 GUI Accelerator"},
	{0x9100,"P9100 GUI Accelerator"},
	{0, 0}
};
static struct pci_device_information pci_device_1011[] = {
	{0x0001,"DC21050 PCI-PCI Bridge"},
	{0x0002,"DC21040 Tulip Ethernet Adapter"},
	{0x0004,"DC21030 PCI Graphics Accelerator"},
	{0x0007,"Zephyr NV-RAM"},
	{0x0008,"KZPSA SCSI to SCSI Adapter"},
	{0x0009,"DC21140 Fast Ethernet Ctrlr"},
	{0x000a,"DC21230 Video Codec"},
	{0x000c,"DC21130 PCI Integrated Graphics & Video Accel"},
	{0x000d,"TGA2"},
	{0x000f,"DEFPA FDDI"},
	{0x0014,"DC21041 Tulip Plus Ethernet Adapter"},
	{0x0016,"DGLPB ATM"},
	{0x0019,"DC21142/3 PCI/CardBus 10/100 Mbit Ethernet Ctlr"},
	{0x0021,"21052 PCI-PCI Bridge"},
	{0x0022,"DC21150-AA PCI-PCI Bridge"},
	{0x0023,"DC21150 PCI to PCI Bridge"},
	{0x0024,"DC21151/2 PCI-PCI Bridge"},
	{0x0025,"21153 PCI-PCI Bridge"},
	{0x0026,"21154 PCI-PCI Bridge"},
	{0x0045,"DC21553 PCI to PCI Bridge"},
	{0x0046,"21554 PCI-to-PCI Bridge"},
	{0x1065,"RAID Controller"},
	{0, 0}
};
static struct pci_device_information pci_device_1013[] = {
	{0x0038,"CL-GD7548 GUI-Accelerated XGA/SVGA LCD Controller"},
	{0x0040,"CL-GD7555 Flat Panel GUI Accelerator"},
	{0x004c,"CL-GD7556 64-bit Accelerated LCD/CRT Controller"},
	{0x00a0,"CL-GD5340 GUI Accelerator"},
	{0x00a2,"CL-GD5432 Alpine GUI Accelerator"},
	{0x00a4,"CL-GD5434 Alpine GUI Accelerator"},
	{0x00a8,"CL-GD5434 Alpine GUI Accelerator"},
	{0x00ac,"CL-GD5436 Alpine GUI Accelerator"},
	{0x00b8,"CL-GD5446 64-bit VisualMedia Accelerator"},
	{0x00bc,"CL-GD5480 64-bit SGRAM GUI accelerator"},
	{0x00d0,"CL-GD5462 Laguna VisualMedia graphics accelerator"},
	{0x00d4,"CL-GD5464 Laguna 3D VisualMedia Graphics Accel"},
	{0x00d6,"CL-GD5465 Laguna 3D VisualMedia Graphics Accel"},
	{0x1100,"CL-PD6729 PCI-to-PC Card host adapter"},
	{0x1110,"CL-PD6832 PCMCIA/CardBus Controller"},
	{0x1112,"CL-PD6834 PCMCIA/CardBus Controller"},
	{0x1113,"CL-PD6833 PCI-to-CardBus Host Adapter"},
	{0x1200,"CL-GD7542 Nordic GUI Accelerator"},
	{0x1202,"CL-GD7543 Viking GUI Accelerator"},
	{0x1204,"CL-GD7541 Nordic-lite VGA Cntrlr"},
	{0x4400,"CL-CD4400 Communications Controller"},
	{0x6001,"CS4610 CrystalClear SoundFusion PCI Audio Accel"},
	{0x6003,"CS4614/22/24 CrystalClear SoundFusion PCI Audio Accel"},
	{0x6005,"CS4281 CrystalClear PCI Audio Interface"},
	{0, 0}
};
static struct pci_device_information pci_device_1014[] = {
	{0x0002,"MCA Bridge MCA Bridge"},
	{0x0005,"Alta Lite CPU Bridge"},
	{0x0007,"Alta MP CPU Bridge"},
	{0x000a,"ISA Bridge w/PnP ISA Bridge w/PnP"},
	{0x0017,"CPU Bridge CPU Bridge"},
	{0x0018,"Auto LANStreamer"},
	{0x001b,"GXT-150P Graphics Adapter"},
	{0x001d,"82G2675"},
	{0x0020,"MCA Bridge"},
	{0x0022,"82351/2 PCI to PCI Bridge"},
	{0x002d,"Python"},
	{0x002e,"ServeRAID RAID SCSI Adapter"},
	{0x0036,"Miami/PCI 32-bit LocalBus Bridge"},
	{0x003e,"85H9533 16/4 Token Ring PCI IBM UTP/STP Ctrlr"},
	{0x0046,"MPIC Interrupt Controller"},
	{0x0047,"PCI to PCI Bridge"},
	{0x0048,"PCI to PCI Bridge"},
	{0x0053,"25 MBit ATM controller"},
	{0x0057,"MPEG PCI Bridge"},
	{0x005c,"i82557B 10/100 PCI Ethernet Adapter"},
	{0x005d,"05J3506 TCP/IP networking device"},
	{0x007d,"MPEG-2 Decoder"},
	{0x0095,"20H2999 PCI Docking Bridge"},
	{0x00b7,"256-bit Graphics Rasterizer"},
	{0x00ce,"02li537 Adapter 2 Token Ring Card"},
	{0, 0}
};
static struct pci_device_information pci_device_1017[] = {
	{0x5343,"SPEA 3D Accelerator"},
	{0, 0}
};
static struct pci_device_information pci_device_101a[] = {
	{0x0005,"8156 100VG/AnyLAN Adapter"},
	{0x0009,"Altera FLEX ??? Raid Controller ???"},
	{0, 0}
};
static struct pci_device_information pci_device_101c[] = {
	{0x0193,"WD33C193A 8-bit SCSI Cntrlr"},
	{0x0196,"WD33C196A PCI-SCSI Bridge"},
	{0x0197,"WD33C197A 16-bit SCSI Cntrlr"},
	{0x0296,"WD33C296A high perf 16-bit SCSI Cntrlr"},
	{0x3193,"WD7193 Fast SCSI-II"},
	{0x3197,"WD7197 Fast-wide SCSI-II"},
	{0x3296,"WD33C296A Fast Wide SCSI bridge"},
	{0x4296,"WD34C296 Wide Fast-20 Bridge"},
	{0x9710,"Pipeline 9710"},
	{0x9712,"Pipeline 9712"},
	{0xc24a,"90C"},
	{0, 0}
};
static struct pci_device_information pci_device_101e[] = {
	{0x9010,"MegaRAID Fast-wide SCSI/RAID"},
	{0x9030,"IDE Cntrlr"},
	{0x9031,"IDE Cntrlr"},
	{0x9032,"IDE and SCSI Cntrlr"},
	{0x9033,"SCSI Cntrlr"},
	{0x9040,"Multimedia card"},
	{0x9060,"MegaRAID RAID Controller"},
	{0, 0}
};
static struct pci_device_information pci_device_1022[] = {
	{0x2000,"79C970 Ethernet Ctrlr"},
	{0x2001,"Am79C978 PCnet-Home Networking Ctrlr (1/10 Mbps)"},
	{0x2020,"53C974 SCSI Ctrlr"},
	{0x2040,"79C974 Ethernet & SCSI Ctrlr"},
	{0x7006,"AMD-751 Processor-to-PCI Bridge / Memory Ctrlr"},
	{0x7007,"AMD-751 AGP and PCI-to-PCI Bridge"},
	{0x7400,"AMD-755 PCI to ISA Bridge"},
	{0x7401,"AMD-755 Bus Master IDE Controller"},
	{0x7403,"AMD-755 Power Management Controller"},
	{0x7404,"AMD-755 PCI to USB Open Host Controller"},
	{0x7408,"AMD-756 PCI-ISA Bridge"},
	{0x7409,"AMD-756 EIDE Controller"},
	{0x740b,"AMD-756 Power Management"},
	{0x740c,"AMD-756 USB Controller"},
	{0, 0}
};
static struct pci_device_information pci_device_1023[] = {
	{0x0194,"82C194 CardBus Controller"},
	{0x2000,"4DWAVE-DX advanced PCI DirectSound accelerator"},
	{0x2001,"4DWAVE-NX PCI Audio"},
	{0x8400,"CyberBlade i7"},
	{0x8420,"CyberBlade i7 AGP"},
	{0x8500,"CyberBlade i1"},
	{0x8520,"CyberBlade i1 AGP"},
	{0x9320,"TGUI9320 32-bit GUI Accelerator"},
	{0x9350,"32-bit GUI Accelerator"},
	{0x9360,"Flat panel Cntrlr"},
	{0x9382,"Cyber9382"},
	{0x9383,"Cyber9383"},
	{0x9385,"Cyber9385"},
	{0x9386,"Cyber9386 Video Accelerator"},
	{0x9388,"Cyber9388 Video Accelerator"},
	{0x9397,"Cyber9397 Video Accelerator"},
	{0x939a,"Cyber9397DVD Video Accelerator"},
	{0x9420,"TGUI9420 DGi GUI Accelerator"},
	{0x9430,"TGUI9430 GUI Accelerator"},
	{0x9440,"TGUI9440 DGi GUI Acclerator"},
	{0x9460,"TGUI9460 32-bit GUI Accelerator"},
	{0x9470,"TGUI9470"},
	{0x9520,"Cyber9520 Video Accelerator"},
	{0x9525,"Cyber9525 Video Accelerator"},
	{0x9660,"TGUI9660XGi GUI Accelerator"},
	{0x9680,"TGUI9680 GUI Accelerator"},
	{0x9682,"TGUI9682 Multimedia Accelerator"},
	{0x9683,"TGUI9683 GUI Accelerator"},
	{0x9685,"ProVIDIA 9685"},
	{0x9750,"3DImage 9750 PCI/AGP"},
	{0x9753,"TGUI9753 Video Accelerator"},
	{0x9754,"TGUI9753 Wave Video Accelerator"},
	{0x9759,"TGUI975? Image GUI Accelerator"},
	{0x9783,"TGUI9783"},
	{0x9785,"TGUI9785"},
	{0x9850,"3D Image 9850 AGP"},
	{0x9880,"Blade 3D PCI/AGP"},
	{0, 0}
};
static struct pci_device_information pci_device_1025[] = {
	{0x1435,"M1435 VL Bridge"},
	{0x1445,"M1445 VL Bridge & EIDE"},
	{0x1449,"M1449 ISA Bridge"},
	{0x1451,"M1451 Pentium Chipset"},
	{0x1461,"M1461 P54C Chipset"},
	{0x1489,"M1489"},
	{0x1511,"M1511"},
	{0x1512,"M1512"},
	{0x1513,"M1513"},
	{0x1521,"M1521 CPU Bridge"},
	{0x1523,"M1523 ISA Bridge"},
	{0x1531,"M1531 North Bridge"},
	{0x1533,"M1533 ISA South Bridge"},
	{0x1535,"M1535 PCI South Bridge"},
	{0x1541,"M1541 AGP PCI North Bridge Aladdin V/V+"},
	{0x1542,"M1542 AGP+PCI North Bridge"},
	{0x1543,"M1543C PCi South Bridge Aladdin IV+/V"},
	{0x1561,"M1561 Northbridge"},
	{0x1621,"M1621 PCI North Bridge Aladdin Pro II"},
	{0x1631,"M1631 PCI North Bridge Aladdin Pro III"},
	{0x1641,"M1641 PCI North Bridge Aladdin Pro IV"},
	{0x3141,"M3141 GUI Accelerator"},
	{0x3143,"M3143 GUI Accelerator"},
	{0x3145,"M3145 GUI Accelerator"},
	{0x3147,"M3147 GUI Accelerator"},
	{0x3149,"M3149 GUI Accelerator"},
	{0x3151,"M3151 GUI Accelerator"},
	{0x3307,"M3307 MPEG-1 Decoder"},
	{0x3309,"M3309 MPEG Decoder"},
	{0x5212,"M4803"},
	{0x5215,"M5217 EIDE Controller"},
	{0x5217,"M5217 I/O Controller"},
	{0x5219,"M5219 I/O Controller"},
	{0x5225,"M5225 EIDE Controller"},
	{0x5229,"M5229 EIDE Controller"},
	{0x5235,"M5235 I/O Controller"},
	{0x5237,"M5237 PCI USB Host Controller"},
	{0x5240,"EIDE Controller"},
	{0x5241,"PCMCIA Bridge"},
	{0x5242,"General Purpose Controller"},
	{0x5243,"PCI to PCI Bridge"},
	{0x5244,"Floppy Disk Controller"},
	{0x5247,"M1541 PCI-PCI Bridge"},
	{0x5427,"PCI to AGP Bridge"},
	{0x5451,"M5451 PCI AC-Link Controller Audio Device"},
	{0x5453,"M5453 M5453 AC-Link Controller Modem Device"},
	{0x7101,"M7101 PCI PMU Power Management Controller"},
	{0, 0}
};
static struct pci_device_information pci_device_1028[] = {
	{0x0001,"PowerEdge 2 /Si Expandable RAID Controller"},
	{0x0002,"PowerEdge 3/Di Expandable RAID Controller"},
	{0x0003,"PowerEdge 3/Si Expandable RAID Controller"},
	{0, 0}
};
static struct pci_device_information pci_device_102a[] = {
	{0x0000,"HYDRA P5 Chipset"},
	{0x0010,"ASPEN i486 Chipset"},
	{0, 0}
};
static struct pci_device_information pci_device_102b[] = {
	{0x0010,"MGA-I Impression?"},
	{0x0518,"MGA-PX2085 Ultima/Atlas GUI Accelerator"},
	{0x0519,"MGA-2064W Millenium GUI Accelerator"},
	{0x051a,"MGA 1064SG 64-bit graphics chip"},
	{0x051b,"MGA-21164W Millenium II"},
	{0x051e,"MGA-1164SG Mystique 220 (AGP)"},
	{0x051f,"MGA2164WA-B Matrox Millenium II AGP"},
	{0x0520,"MGA-G200B Millennium/Mystique G200 AGP"},
	{0x0521,"MGA-G200 Millennium/Mystique G200 AGP"},
	{0x0d10,"MGA-I Ultima/Impression GUI accelerator"},
	{0x1000,"MGA-G100"},
	{0x1001,"MGA-G100"},
	{0x2007,"Mistral GUI+3D Accelerator"},
	{0x4536,"Meteor 2/MC Video Capture Card"},
	{0x6573,"Shark 10/100 Multiport Switch NIC"},
	{0, 0}
};
static struct pci_device_information pci_device_102c[] = {
	{0x00b8,"64310 Wingine DGX - DRAM Graphics Accelerator"},
	{0x00c0,"69000 Video Accelerator with Integrated Memory"},
	{0x00d0,"65545 Flat panel/crt VGA Cntrlr"},
	{0x00d8,"65540 Flat Panel/CRT VGA Controller"},
	{0x00dc,"65548 GUI Accelerator"},
	{0x00e0,"65550 LCD/CRT controller"},
	{0x00e4,"65554 Flat Panel/LCD CRT GUI Accelerator"},
	{0x00e5,"65555 VGA GUI Accelerator"},
	{0x00f0,"68554 GUI Controller"},
	{0x00f4,"68554 HiQVision Flat Panel/CRT GUI Controller"},
	{0x00f5,"68555 GUI Controller"},
	{0x03c0,"69030 AGP Video Accelerator"},
	{0, 0}
};
static struct pci_device_information pci_device_102d[] = {
	{0x50dc,"3328 Audio"},
	{0, 0}
};
static struct pci_device_information pci_device_102f[] = {
	{0x0009,"r4x00 CPU Bridge"},
	{0x0020,"Meteor 155 ATM PCI Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_1031[] = {
	{0x5601,"MiroVIDEO DC20 I/O & JPEG"},
	{0x5607,"video in and out with motion jpeg compression and deco"},
	{0x5631,"Media 3D"},
	{0x6057,"MiroVIDEO DC10/DC30"},
	{0, 0}
};
static struct pci_device_information pci_device_1033[] = {
	{0x0001,"PCI to 486 like bus Bridge"},
	{0x0002,"PCI to vl98 Bridge"},
	{0x0003,"atm lan Cntrlr"},
	{0x0004,"R4000 PCI bus Bridge"},
	{0x0005,"PCI to 486 like peripheral bus Bridge"},
	{0x0006,"GUI Accelerator"},
	{0x0007,"PCI to ux-bus Bridge"},
	{0x0008,"GUI Accelerator (vga equivalent)"},
	{0x0009,"graphic Cntrlr for 98"},
	{0x001a,"Nile II"},
	{0x0021,"Vrc4373 Nile I"},
	{0x0029,"PoverVR PCX1 3D Accelerator"},
	{0x002a,"PoverVR 3D Accelerator"},
	{0x0035,"uPD9210FGC-7EA USB Host Controller"},
	{0x003e,"NAPCCARD CardBus Controller"},
	{0x0046,"PoverVR PCX2 3D Accelerator"},
	{0x005a,"Vrc5074 Nile 4"},
	{0x0063,"uPD72862 Firewarden IEEE1394 OHCI Host Controller"},
	{0x0067,"PoverVR Neon 250 3D Accelerator"},
	{0x0074,"56k Voice Modem"},
	{0x009b,"Vrc5476"},
	{0x00cd,"uPD72870 IEEE1394 1-Chip OHCI Host Controller"},
	{0x00ce,"uPD72871 IEEE1394 1-Chip OHCI Host Controller"},
	{0, 0}
};
static struct pci_device_information pci_device_1036[] = {
	{0x0000,"TMC-18C30 Fast SCSI"},
	{0, 0}
};
static struct pci_device_information pci_device_1039[] = {
	{0x0001,"SiS 530 Virtual PCI-to-PCI bridge (AGP)"},
	{0x0002,"SiS 6202 PCI True-Color Graphics Accelerator"},
	{0x0005,"Pentium chipset"},
	{0x0006,"SiS 85C501 PCI/ISA Cache Memory Controller (PCMC)"},
	{0x0008,"SiS 85C503 PCI System I/O (PSIO)"},
	{0x0009,"SiS 5595 Power Management Unit (PMU)"},
	{0x0018,"SiS 85C503 PCI to ISA Bridge (LPC Bridge)"},
	{0x0200,"SiS5597 5597 Onboard Graphics Controller"},
	{0x0204,"SiS 6215 PCI Graphics & Video Accelerator"},
	{0x0205,"SiS 6205 PCI Graphics & Video Accelerator"},
	{0x0300,"SiS300/305 GUI Accelerator+3D"},
	{0x0305,"SiS305 2D/3D/Video/DVD Accelerator"},
	{0x0406,"85C501 PCI/ISA Cache Memory Controller (PCMC)"},
	{0x0496,"85C496 VL Bridge"},
	{0x0530,"SiS530 Host-to-PCI bridge"},
	{0x0540,"SiS540 Host-to-PCI Bridge"},
	{0x0596,"p5 chipset w/DE"},
	{0x0597,"SiS5513 EIDE Controller (step C)"},
	{0x0601,"SiS83C601 PCI EIDE Controller"},
	{0x0620,"SiS620 Host-to-PCI Bridge"},
	{0x0630,"SiS630 Host-to-PCI Bridge"},
	{0x0900,"SiS630 Fast Ethernet/Home Networking Ctrlr"},
	{0x3602,"SiS83C602 IDE Controller"},
	{0x5107,"SiS5107 Hot Docking Controller"},
	{0x5300,"SiS540 AGP"},
	{0x5401,"486 chipset"},
	{0x5511,"SiS5511/5512 PCI/ISA System Memory Controller"},
	{0x5513,"SiS5513 PCI IDE Controller"},
	{0x5517,"SiS5517 CPU to PCI Bridge"},
	{0x5571,"SiS5571 Memory/PCI bridge"},
	{0x5581,"p5 chipset"},
	{0x5582,"ISA Bridge"},
	{0x5591,"SiS 5591/5592 PCI AGP & CPU Memory Controller"},
	{0x5596,"SiS5596 PCI, Memory & VGA Controller"},
	{0x5597,"SiS5597 Host to PCI bridge"},
	{0x5600,"SiS600 Host-to-PCI Bridge"},
	{0x6204,"video decoder/mpeg interface"},
	{0x6205,"PCI vga Cntrlr"},
	{0x6225,"SiS 6225 PCI Graphics & Video Accelerator"},
	{0x6300,"SiS630 AGP"},
	{0x6306,"SiS530 Integrated 3D VGA Controller"},
	{0x6326,"SiS 86C326 AGP/PCI Graphics & Video Accelerator"},
	{0x7001,"SiS5571 USB Host Controller"},
	{0x7007,"OHCI Compliant FireWire Controller"},
	{0x7016,"SiS7016 10/100 Ethernet Adapter"},
	{0x7018,"SiS630 Audio Accelerator"},
	{0, 0}
};
static struct pci_device_information pci_device_103c[] = {
	{0x1030,"J2585A DeskDirect 10/100VG LAN Adapter"},
	{0x1031,"DeskDirect 10/100 NIC"},
	{0x1064,"79C970 PCnet Ethernet Controller"},
	{0x10c1,"NetServer Smart IRQ Router"},
	{0x10ed,"TopTools Remote Control"},
	{0x1200,"82557B 10/100 NIC"},
	{0x1219,"NetServer PCI Hot-Plug Controller"},
	{0x121a,"NetServer SMIC Controller"},
	{0x121b,"NetServer Legacy COM Port Decoder"},
	{0x121c,"NetServer PCI COM Port Decoder"},
	{0x2910,"E2910A PCI Bus Exerciser"},
	{0x2920,"Fast Host Interface"},
	{0x2924,"E2924A PCI Host Interface Adapter"},
	{0x2925,"E2925A 32 bit PCI Bus Exerciser and Analyzer"},
	{0x2926,"E2926A 64 bit PCI Bus Exerciser and Analyzer"},
	{0x2927,"E2927A 64 Bit, 66/50MHz PCI Analyzer & Exerciser"},
	{0x2940,"E2940A 64 bit, 66/50MHz CompactPCI Analyzer&Exerciser"},
	{0, 0}
};
static struct pci_device_information pci_device_1042[] = {
	{0x1000,"RZ1000 IDE Ctrlr"},
	{0x1000,"FDC 37C665 EIDE"},
	{0x1001,"37C922"},
	{0x3000,"Samurai 0 CPU to PCI Bridge"},
	{0x3010,"Samurai 1 CPU to PCI Bridge"},
	{0x3020,"Samurai IDE Controller"},
	{0x3030,"MT82P664 Samurai 64M2"},
	{0, 0}
};
static struct pci_device_information pci_device_1043[] = {
	{0x0200,"AGP-V3400 Asus RivaTNT Video Board"},
	{0x401d,"GeForce2 MX"},
	{0, 0}
};
static struct pci_device_information pci_device_1044[] = {
	{0x1012,"Domino RAID Engine"},
	{0xa400,"2124A/9X SmartCache III/RAID SCSI Controller"},
	{0xa500,"PCI Bridge"},
	{0xa501,"SmartRAID V Controller"},
	{0, 0}
};
static struct pci_device_information pci_device_1045[] = {
	{0xa0f8,"82C750 PCI USB Controller"},
	{0xc101,"82C264 GUI Accelerator"},
	{0xc178,"82C178 LCD GUI Accelerator"},
	{0xc556,"82C556 Viper"},
	{0xc557,"82C557 CPU Bridge (Viper)"},
	{0xc558,"82C558 ISA Bridge w/PnP"},
	{0xc567,"82C750 Vendetta chipset: host bridge"},
	{0xc568,"82C750 Vendetta chipset: ISA bridge"},
	{0xc569,"82C579 Pentium to PCI Bridge"},
	{0xc621,"82C621 PCI IDE Controller (PIC)"},
	{0xc700,"82C700 FireStar chipset, PCI-ISA bridge???"},
	{0xc701,"82C700 FireStar mobile chipset: host bridge"},
	{0xc814,"82C814 FireBridge II Docking Station Controller"},
	{0xc822,"82C822 EIDE Ctrlr"},
	{0xc824,"82C824 FireFox 32-Bit PC Card Controller"},
	{0xc825,"82C825 function 0 PCI-to-ISA Bridge"},
	{0xc832,"82C832 CPU-to-PCI and PCI-to-ISA Bridge"},
	{0xc861,"82C861 FireLink PCI-to-USB Bridge"},
	{0xc895,"82C895"},
	{0xc935,"82C935 MachOne integrated PCI audio processor"},
	{0xd568,"82C825 PCI bus master IDE controller"},
	{0xd768,"82C750 Ultra DMA IDE controller"},
	{0, 0}
};
static struct pci_device_information pci_device_1048[] = {
	{0x1000,"PCI to SCSI Bridge"},
	{0x3000,"QuickStep 3000"},
	{0, 0}
};
static struct pci_device_information pci_device_104a[] = {
	{0x0008,"STG 2000X"},
	{0x0009,"STG 1764X"},
	{0x1746,"STG 1746X"},
	{0x3520,"MPEG-II Video Decoder"},
	{0, 0}
};
static struct pci_device_information pci_device_104b[] = {
	{0x0140,"BT-946C Multimaster NC (SCSI-2)"},
	{0x1040,"BA80C30 Multimaster"},
	{0x8130,"Flashpoint LT Ultra SCSI"},
	{0, 0}
};
static struct pci_device_information pci_device_104c[] = {
	{0x0500,"100 MBit LAN Cntrlr"},
	{0x0508,"TMS380C2X Compressor interface"},
	{0x1000,"TI PCI Eagle i/f AS"},
	{0x3d04,"TVP4010 Permedia"},
	{0x3d07,"TVP4020 AGP Permedia 2"},
	{0x8000,"LYNX FireWire Host Controller"},
	{0x8009,"OHCI Compliant FireWire Controller"},
	{0x8019,"TSB12LV23 OHCI Compliant IEEE-1394 Controller"},
	{0xa001,"TDC1570 64-bit PCI ATM sar"},
	{0xa100,"TDC1561 32-bit PCI ATM sar"},
	{0xac10,"PCI1050 pc card Cntrlr"},
	{0xac11,"PCI1030/1053 PC Card Controller"},
	{0xac12,"PCI1130 PC card CardBus Controller"},
	{0xac13,"PCI1031 PCI-TO-PC CARD16 CONTROLLER UNIT"},
	{0xac16,"PCI1250 pc card Cardbus Cntrlr"},
	{0xac17,"PCI1220 CardBus Controller"},
	{0xac18,"PCI1260 PC card CardBus Controller"},
	{0xac1a,"PCI1210 PC card CardBus Controller"},
	{0xac1b,"PCI1450 PC card CardBus Controller"},
	{0xac1f,"PCI1251B PC card CardBus Controller"},
	{0xac20,"PCI2030 PCI to PCI Bridge"},
	{0xac30,"PCI1260 PC card CardBus Controller"},
	{0xac40,"PCI4450 PC card CardBus Controller"},
	{0xac41,"PCI4410 PC card CardBus Controller"},
	{0xac42,"PCI4451 PC card CardBus Controller"},
	{0xac50,"PCI1410 PC card cardBus Controller"},
	{0xac52,"PCI1451 PC card CardBus Controller"},
	{0xac53,"PCI1421 PC card CardBus Controller"},
	{0xfe00,"FireWire Host Controller"},
	{0xfe03,"12C01A FireWire Host Controller"},
	{0, 0}
};
static struct pci_device_information pci_device_104d[] = {
	{0x8009,"CXD1947A IEEE1394 link layer / PCI bridge"},
	{0x8039,"CXD3222 OHCI i.LINK (IEEE 1394) PCI Host Ctrlr"},
	{0x8056,"Rockwell HCF 56K Modem"},
	{0, 0}
};
static struct pci_device_information pci_device_104e[] = {
	{0x0017,"OTI-64017"},
	{0x0107,"OTI107 Spitfire VGA Accelerator"},
	{0x0109,"Video Adapter"},
	{0x0111,"OTI-64111 Spitfire"},
	{0x0217,"OTI-64217"},
	{0x0317,"OTI-64317"},
	{0, 0}
};
static struct pci_device_information pci_device_1050[] = {
	{0x0000,"Ethernet Cntrlr"},
	{0x0001,"W83769F Ethernet Adapter"},
	{0x0105,"W82C105 Ethernet Adapter"},
	{0x0840,"W89C840F 100/10Mbps Ethernet Controller"},
	{0x0940,"w89c940f winbond pci ethernet"},
	{0x5a5a,"W89C940F NE2000-compatible Ethernet Adapter"},
	{0x6692,"W6692CF ISDN"},
	{0x9970,"W9970CF"},
	{0, 0}
};
static struct pci_device_information pci_device_1054[] = {
	{0x0001,"PCI Bridge"},
	{0x0002,"PCI bus Cntrlr"},
	{0, 0}
};
static struct pci_device_information pci_device_1055[] = {
	{0x0810,"486 host Bridge"},
	{0x0922,"Pentium/p54c host Bridge"},
	{0x0926,"ISA Bridge"},
	{0, 0}
};
static struct pci_device_information pci_device_1057[] = {
	{0x0001,"MPC105 Eagle PowerPC Chipset"},
	{0x0002,"MPC106 Grackle PowerPC Chipset"},
	{0x0100,"MC145575 HCF-PCI"},
	{0x0431,"KTI829c 100VG Ethernet Controller"},
	{0x1801,"56301 Audio I/O Controller (MIDI)"},
	{0x4801,"Raven PowerPC Chipset"},
	{0x4802,"Falcon"},
	{0x4803,"Hawk"},
	{0x4806,"CPX8216"},
	{0x5600,"SM56 PCI Speakerphone Modem"},
	{0, 0}
};
static struct pci_device_information pci_device_105a[] = {
	{0x4d33,"PDC20246 Ultra ATA controller"},
	{0x4d38,"PDC20262 UltraDMA66 EIDE Controller"},
	{0x5300,"DC5300 EIDE Controller"},
	{0, 0}
};
static struct pci_device_information pci_device_105d[] = {
	{0x2309,"Imagine 128 GUI Accelerator"},
	{0x2339,"I128s2 Imagine 128 Series 2"},
	{0x493d,"T2R Revolution 3D"},
	{0x5348,"Revolution IV Revolution IV"},
	{0, 0}
};
static struct pci_device_information pci_device_1060[] = {
	{0x0001,"UM82C881 486 Chipset"},
	{0x0002,"UM82C886 ISA Bridge"},
	{0x0101,"UM8673F EIDE Controller"},
	{0x0881,"UM8881 HB4 486 PCI Chipset"},
	{0x0881,"UM8881"},
	{0x0886,"UM8886F ISA Bridge"},
	{0x0891,"UM82C891 Pentium Chipset"},
	{0x1001,"UM886A IDE Cntrlr (dual function)"},
	{0x673a,"UM8886 Funktion 1: EIDE Controller"},
	{0x673b,"EIDE Master/DMA"},
	{0x8710,"UM8710 VGA Cntrlr"},
	{0x8821,"CPU/PCI Bridge"},
	{0x8822,"PCI/ISA Bridge"},
	{0x8851,"Pentium CPU/PCI Bridge"},
	{0x8852,"Pentium CPU/ISA Bridge"},
	{0x886a,"UM8886 ISA Bridge with EIDE"},
	{0x8881,"UM8881F HB4 486 PCI Chipset"},
	{0x8886,"UM8886 ISA Bridge"},
	{0x888a,"UM8886A"},
	{0x8891,"UM8891 586 Chipset"},
	{0x9017,"UM9017F Ethernet"},
	{0x9018,"UM9018 Ethernet"},
	{0x9026,"UM9026 Fast Ethernet"},
	{0xe881,"UM8881 486 Chipset"},
	{0xe886,"UM8886 ISA Bridge w/EIDE"},
	{0xe88a,"UM8886N"},
	{0xe891,"UM8891 Pentium Chipset"},
	{0, 0}
};
static struct pci_device_information pci_device_1061[] = {
	{0x0001,"AGX013/016 GUI Accelerator"},
	{0x0002,"IIT3204/3501 MPEG Decoder"},
	{0, 0}
};
static struct pci_device_information pci_device_1066[] = {
	{0x0000,"PT80C826 VL Bridge"},
	{0x0001,"PT86C521 Vesuvius V1-LS System Controller"},
	{0x0002,"PT86C523 Vesuvius V3-LS ISA Bridge"},
	{0x0004,"ISA Bridge"},
	{0x0005,"PC87550 System Controller"},
	{0x8002,"PT86C523 ISA Bridge"},
	{0, 0}
};
static struct pci_device_information pci_device_1067[] = {
	{0x1002,"VG500 VolumePro Volume Rendering Accelerator"},
	{0, 0}
};
static struct pci_device_information pci_device_1069[] = {
	{0x0001,"DAC960P DAC960P 3 ch SCSI RAID Controller"},
	{0x0002,"DAC960PD DAC960PD 3 ch SCSI RAID Controller"},
	{0x0010,"DAC960PJ DAC960PJ 3 ch SCSI RAID Controller"},
	{0x0050,"i960 AcceleRAID 170"},
	{0xba55,"1100 eXtremeRAID Controller"},
	{0, 0}
};
static struct pci_device_information pci_device_106b[] = {
	{0x0001,"Bandit"},
	{0x0002,"Grand Central"},
	{0x0003,"Control Video"},
	{0x0004,"PlanB Video-in"},
	{0x0007,"OHare I/O"},
	{0x000e,"Hydra"},
	{0x0010,"Heathrow Mac I/O"},
	{0x0017,"Paddington Mac I/O"},
	{0, 0}
};
static struct pci_device_information pci_device_106c[] = {
	{0x8801,"Dual Pentium ISA/PCI Motherboard"},
	{0x8802,"PowerPC ISA/PCI Motherboard"},
	{0x8803,"Dual Window Graphics Accelerator"},
	{0x8804,"PCI LAN Controller"},
	{0x8805,"100-BaseT LAN Controller"},
	{0, 0}
};
static struct pci_device_information pci_device_1073[] = {
	{0x0001,"3D graphics Cntrlr"},
	{0x0002,"YGV615 RPA3 3D-Graphics Controller"},
	{0x0003,"YMF740"},
	{0x0004,"YMF724"},
	{0x0005,"DS1 DS1 Audio"},
	{0x0006,"DS1 DS1 Audio"},
	{0x0008,"DS1 DS1 Audio"},
	{0x000a,"DS1L DS1L Audio"},
	{0x000c,"YMF740C DS-1L PCI audio controller"},
	{0x000d,"YMF724F DS-1 PCI audio controller"},
	{0x0010,"YMF744B DS-1S PCI audio controller"},
	{0x0012,"YMF754B DS-1S Audio"},
	{0x0020,"DS-1 Audio"},
	{0, 0}
};
static struct pci_device_information pci_device_1074[] = {
	{0x4e78,"82C500/1 Nx586 Chipset"},
	{0, 0}
};
static struct pci_device_information pci_device_1077[] = {
	{0x1020,"ISP1020A Fast-wide SCSI"},
	{0x1022,"ISP1022A Fast-wide SCSI"},
	{0x1080,"ISP1080 SCSI Host Adapter"},
	{0x1240,"ISP1240 SCSI Host Adapter"},
	{0x1280,"ISP1280"},
	{0x2020,"ISP2020 Fast!SCSI Basic Adapter"},
	{0x2100,"QLA2100 64-bit Fibre Channel Adapter"},
	{0x2200,"ISP2200"},
	{0, 0}
};
static struct pci_device_information pci_device_1078[] = {
	{0x0000,"Cx5520 ISA Bridge"},
	{0x0001,"MediaGXm MMX Cyrix Integrated CPU"},
	{0x0002,"Cx5520 ISA Bridge"},
	{0x0100,"Cx5530 Legacy device"},
	{0x0101,"Cx5530 SMI"},
	{0x0102,"Cx5530 IDE"},
	{0x0103,"Cx5530 Audio"},
	{0x0104,"Cx5530 Video"},
	{0, 0}
};
static struct pci_device_information pci_device_107d[] = {
	{0x0000,"P86C850 Graphic GLU-Logic"},
	{0, 0}
};
static struct pci_device_information pci_device_107e[] = {
	{0x0001,"ATM interface card"},
	{0x0002,"100 vg anylan Cntrlr"},
	{0x0004,"5526"},
	{0x0005,"55x6"},
	{0x0008,"155 MBit ATM controller"},
	{0, 0}
};
static struct pci_device_information pci_device_107f[] = {
	{0x0802,"SL82C105 EIDE Ctrlr"},
	{0x0803,"EIDE Bus Master Controller"},
	{0x0806,"EIDE Controller"},
	{0x2015,"EIDE Controller"},
	{0, 0}
};
static struct pci_device_information pci_device_1080[] = {
	{0x0600,"82C596/9 PCI to VLB Bridge"},
	{0xc691,"Cypress CY82C691"},
	{0xc693,"82C693 PCI to ISA Bridge"},
	{0, 0}
};
static struct pci_device_information pci_device_1081[] = {
	{0x0d47,"PCi to NuBUS Bridge"},
	{0, 0}
};
static struct pci_device_information pci_device_1083[] = {
	{0x0001,"FR710 EIDE Ctrlr"},
	{0x0613,"Host Bridge"},
	{0, 0}
};
static struct pci_device_information pci_device_108a[] = {
	{0x0001,"Model 617 PCI-VME Bus Adapter"},
	{0x0010,"Model 618 VME Bridge"},
	{0x3000,"Model 2106 VME Bridge"},
	{0, 0}
};
static struct pci_device_information pci_device_108d[] = {
	{0x0001,"OC-3136/37 16/4 PCI Ethernet Adapter"},
	{0x0002,"OC-3139f Fastload 16/4 PCI/III Token Ring Adapter"},
	{0x0004,"OC-3139/40 RapidFire Token Ring 16/4 Adapter"},
	{0x0005,"OC-3250 GoCard Token Ring 16/4 Adapter"},
	{0x0006,"OC-3530 RapidFire Token Ring 100 Adapter"},
	{0x0007,"OC-3141 RapidFire Token Ring 16/4 Adapter"},
	{0x0008,"OC-3540 RapidFire HSTR 100/16/4 Adapter"},
	{0x0011,"OC-2805 Ethernet Controller"},
	{0x0012,"OC-2325 Ethernet PCI/II 10/100 Controller"},
	{0x0013,"OC-2183/85 PCI/II Ethernet Controller"},
	{0x0014,"OC-2326 Ethernet PCI/II 10/100 Controller"},
	{0x0019,"OC-2327/50 10/100 Ethernet Controller"},
	{0x0021,"OC-6151/52 ATM Adapter"},
	{0x0022,"ATM Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_108e[] = {
	{0x0001,"SPARC EBUS"},
	{0x1000,"EBUS? HME bridge device"},
	{0x1001,"HME Happy Meal Ethernet"},
	{0x5000,"Simba PCI Bridge"},
	{0x5043,"SunPCI Co-processor"},
	{0x8000,"PCI Bus Module"},
	{0xa000,"UltraSPARC III PCI"},
	{0, 0}
};
static struct pci_device_information pci_device_1091[] = {
	{0x0020,"3D Graphics Processor"},
	{0x0021,"3D graphics processor w/texturing"},
	{0x0040,"3D graphics frame buffer"},
	{0x0041,"3D graphics frame buffer"},
	{0x0060,"Proprietary bus Bridge"},
	{0x00e4,"Powerstorm 4D50T"},
	{0x0720,"Motion JPEG Codec"},
	{0, 0}
};
static struct pci_device_information pci_device_1092[] = {
	{0x00a0,"SpeedStar Pro SE GUI Accelerator"},
	{0x00a8,"SpeedStar 64 GUI Accelerator"},
	{0x08d4,"Supra 2260 Modem"},
	{0x1092,"Viper V330"},
	{0x6120,"Maximum DVD"},
	{0x8810,"Stealth SE GUI Accelerator"},
	{0x8811,"Stealth 64/SE GUI Accelerator"},
	{0x8880,"Stealth Video"},
	{0x8881,"Stealth Video GUI Accelerator"},
	{0x88b0,"Stealth 64 Video GUI Accelerator"},
	{0x88b1,"Stealth 64 Video GUI Accelerator"},
	{0x88c0,"Stealth 64 GUI Accelerator"},
	{0x88c1,"Stealth 64 GUI Accelerator"},
	{0x88d0,"Stealth 64 GUI Accelerator"},
	{0x88d1,"Stealth 64 GUI Accelerator"},
	{0x88f0,"Stealth 64 Video GUI Accelerator"},
	{0x88f1,"Stealth 64 Video GUI Accelerator"},
	{0x9999,"Monster Sound"},
	{0, 0}
};
static struct pci_device_information pci_device_1093[] = {
	{0x0160,"PCI-DIO-96"},
	{0x0162,"PCI-MIO-16XE-50"},
	{0x1170,"PCI-MIO-16XE-10"},
	{0x1180,"PCI-MIO-16E-1"},
	{0x1190,"PCI-MIO-16E-4"},
	{0x1330,"PCI-6031E"},
	{0x1350,"PCI-6071E"},
	{0x2a60,"PCI-6923E"},
	{0xb001,"IMAQ-PCI-1408"},
	{0xb011,"IMAQ-PXI-1408"},
	{0xb021,"IMAQ-PCI-1424"},
	{0xb031,"IMAQ-PCI-1413"},
	{0xb041,"IMAQ-PCI-1407"},
	{0xb051,"IMAQ-PXI-1407"},
	{0xb061,"IMAQ-PCI-1411"},
	{0xb071,"IMAQ-PCI-1422"},
	{0xb081,"IMAQ-PXI-1422"},
	{0xb091,"IMAQ-PXI-1411"},
	{0xc801,"PCI-GPIB GPIB Controller Interface Board"},
	{0, 0}
};
static struct pci_device_information pci_device_1095[] = {
	{0x0640,"PCI0640A EIDE Ctrlr"},
	{0x0641,"PCI0640 PCI EIDE Adapter with RAID 1"},
	{0x0642,"IDE Cntrlr w/RAID 1"},
	{0x0643,"PCI0643 PCI EIDE controller"},
	{0x0646,"PCI0646 bus master IDE"},
	{0x0647,"PCI0647"},
	{0x0648,"PCI-648 Bus Master Ultra DMA PCI-IDE/ATA Chip"},
	{0x0650,"PBC0650A Fast SCSI-II Ctrlr"},
	{0x0670,"USB0670 PCI-USB ASIC"},
	{0x0673,"USB0673 PCI-USB ASIC"},
	{0, 0}
};
static struct pci_device_information pci_device_1097[] = {
	{0x0038,"EIDE Controller (single FIFO)"},
	{0, 0}
};
static struct pci_device_information pci_device_1098[] = {
	{0x0001,"QD8500 EIDE Controller"},
	{0x0002,"QD8580 EIDE Controller"},
	{0, 0}
};
static struct pci_device_information pci_device_109e[] = {
	{0x0350,"BT848 TV/PCI with DMA Push"},
	{0x0351,"Bt849 Video Capture"},
	{0x036c,"Bt879? Video Capture"},
	{0x036e,"Bt878 MediaStream Controller"},
	{0x036f,"Bt879 Video Capture"},
	{0x0370,"Bt880 Video Capture"},
	{0x0878,"Bt878 Video Capture (Audio Section)"},
	{0x0879,"Bt879 Video Capture (Audio Section)"},
	{0x0880,"Bt880 Video Capture (Audio Section)"},
	{0x2115,"BtV 2115 BtV Mediastream Controller"},
	{0x2125,"BtV 2125 BtV Mediastream Controller"},
	{0x2164,"BtV 2164 Display Adapter"},
	{0x2165,"BtV 2165 MediaStream Controller"},
	{0x8230,"BtV 8230 ATM Segment/Reassembly Controller (SRC)"},
	{0x8472,"Bt8472"},
	{0x8474,"Bt8474"},
	{0, 0}
};
static struct pci_device_information pci_device_10a8[] = {
	{0x0000,"? 64-bit GUI Accelerator"},
	{0, 0}
};
static struct pci_device_information pci_device_10a9[] = {
	{0x0004,"O2 MACE"},
	{0x0005,"RAD Audio"},
	{0x0006,"HPCEX"},
	{0x0007,"RPCEX"},
	{0x0008,"DiVO VIP"},
	{0x0009,"Alteon Gigabit Ethernet"},
	{0x0010,"AMP Video I/O"},
	{0x0011,"GRIP"},
	{0x0012,"SGH PSHAC GSN"},
	{0x1001,"Magic Carpet"},
	{0x1002,"Lithium"},
	{0x1003,"Dual JPEG 1"},
	{0x1004,"Dual JPEG 2"},
	{0x1005,"Dual JPEG 3"},
	{0x1006,"Dual JPEG 4"},
	{0x1007,"Dual JPEG 5"},
	{0x1008,"Cesium"},
	{0x2001,"Fibre Channel"},
	{0x2002,"ASDE"},
	{0x8001,"O2 1394"},
	{0x8002,"G-net NT"},
	{0, 0}
};
static struct pci_device_information pci_device_10aa[] = {
	{0x0000,"ACC 2056/2188 CPU to PCI Bridge (Pentium)"},
	{0x2051,"Laptop Chipset CPU Bridge"},
	{0x5842,"Laptop Chipset ISA Bridge"},
	{0, 0}
};
static struct pci_device_information pci_device_10ad[] = {
	{0x0001,"W83769F EIDE Ctrlr"},
	{0x0103,"sl82c103 PCI-ide mode 4.5 Cntrlr"},
	{0x0105,"sl82c105 - bus master PCI-ide mode 4.5 Cntrlr"},
	{0, 0}
};
static struct pci_device_information pci_device_10b5[] = {
	{0x0480,"IOP 480 Integrated PowerPC I/O Processor"},
	{0x0960,"PCI 9080RDK-960 PCI Reference Design Kit for PCI 9080"},
	{0x9030,"PCI 9030 PCI SMARTarget I/O Accelerator"},
	{0x9036,"PCI9036 Interface chip"},
	{0x9050,"PCI 9050 Target PCI Interface Chip"},
	{0x9052,"PCI 9052 PCI 9052 Target PCI Interface Chip"},
	{0x9054,"PCI 9054 PCI I/O Accelerator"},
	{0x9060,"PCI9060xx PCI Bus Master Interface Chip"},
	{0x906d,"PCI 9060SD PCI Bus Master Interface Chip"},
	{0x906e,"PCI 9060ES PCI Bus Master Interface Chip"},
	{0x9080,"PCI 9080 High performance PCI to Local Bus chip"},
	{0, 0}
};
static struct pci_device_information pci_device_10b6[] = {
	{0x0001,"Smart 16/4 Ringnode (PCI1b)"},
	{0x0002,"Smart 16/4 Ringnode (PCIBM2/CardBus)"},
	{0x0003,"Smart 16/4 Ringnode"},
	{0x0004,"Smart 16/4 Ringnode Mk1 (PCIBM1)"},
	{0x0006,"16/4 CardBus Adapter (Eric 2)"},
	{0x0007,"Presto PCI"},
	{0x0009,"Smart 100/16/4 PCi-HS Ringnode"},
	{0x000a,"Smart 100/16/4 PCI Ringnode"},
	{0x000b,"16/4 CardBus  Adapter Mk2"},
	{0x1000,"Collage 25 ATM adapter"},
	{0x1001,"Collage 155 ATM adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_10b7[] = {
	{0x0001,"3C985 1000BaseSX Gigabit Etherlink"},
	{0x3390,"Velocity Token Link Velocity"},
	{0x3590,"3C359 TokenLink Velocity XL Adapter"},
	{0x5057,"3C575 Megahertz 10/100 LAN CardBus PC Card"},
	{0x5157,"3C575 Megahertz 10/100 LAN CardBus PC Card"},
	{0x5900,"3C590 Ethernet 10bT"},
	{0x5950,"3C595 Ethernet 100bTX"},
	{0x5951,"3C595 Ethernet 100bT4"},
	{0x5952,"3C595 Ethernet 100b-MII"},
	{0x8811,"Token Ring"},
	{0x9000,"3C900-TPO Fast Etherlink XL PCI 10"},
	{0x9001,"3C900-COMBO Fast Etherlink XL PCI 10"},
	{0x9004,"3C900B-TPO EtherLink XL TPO 10Mb"},
	{0x9005,"3C900B-COMBO Fast Etherlink XL 10Mb"},
	{0x9006,"3C900B-TPC EtherLink XL TPC"},
	{0x900a,"3C900B-FL EtherLink XL FL"},
	{0x9050,"3C905-TX Fast Etherlink XL PCI 10/100"},
	{0x9051,"3C905-T4 Fast Etherlink XL 10/100"},
	{0x9055,"3C905B Fast Etherlink XL 10/100"},
	{0x9058,"3C905B-COMBO Deluxe EtherLink XL 10/100"},
	{0x905a,"3C905B-FX Fast EtherLink XL FX 10/100"},
	{0x9200,"3C905C-TX Fast EtherLink for PC Management NIC"},
	{0x9800,"3C980-TX Fast EtherLink XL Server Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_10b8[] = {
	{0x0005,"83C170QF Fast Ethernet Adapter 100bTX"},
	{0x0006,"Fast Ethernet Adapter"},
	{0x1000,"37C665 FDC"},
	{0x1001,"37C922 FDC"},
	{0xa011,"83C170QF Fast ethernet controller"},
	{0xb106,"SMC34C90 CardBus Controller"},
	{0, 0}
};
static struct pci_device_information pci_device_10b9[] = {
	{0x0111,"CMI8738/C3DX C-Media Audio Device (OEM)"},
	{0x1435,"M1435 VL Bridge"},
	{0x1445,"M1445 VL Bridge w/EIDE"},
	{0x1449,"M1449 ISA Bridge"},
	{0x1451,"M1451 Pentium Chipset"},
	{0x1461,"M1461 P54C Chipset"},
	{0x1489,"M1489 486 PCI Chipset"},
	{0x1511,"M1511 Aladdin 2 Host Bridge"},
	{0x1513,"M1513 Aladdin 2 South Bridge"},
	{0x1521,"M1521 Alladin III CPU to PCI Bridge"},
	{0x1523,"M1523 ISA Bridge"},
	{0x1533,"M1533 PCI South Bridge"},
	{0x1541,"M1541 Aladdin V AGPset Host Bridge"},
	{0x1543,"M1543 Aladdin V AGPset South Bridge"},
	{0x3141,"M3141 GUI Accelerator"},
	{0x3143,"M3143 GUI Accelerator"},
	{0x3145,"M3145 GUI Accelerator"},
	{0x3147,"M3147 GUI Accelerator"},
	{0x3149,"M3149 GUI Accelerator"},
	{0x3151,"M3151 GUI Accelerator"},
	{0x3307,"M3307 MPEG-1 Decoder"},
	{0x3309,"M3309 MPEG Decoder"},
	{0x5212,"M4803"},
	{0x5215,"MS4803 EIDE Ctrlr"},
	{0x5217,"m5217h I/O (?)"},
	{0x5219,"m5219 I/O (?)"},
	{0x5225,"M5225 IDE Controller"},
	{0x5229,"M1543 Southbridge EIDE Controller"},
	{0x5235,"M5235 I/O Controller"},
	{0x7101,"M7101 Power Management Controller"},
	{0, 0}
};
static struct pci_device_information pci_device_10bd[] = {
	{0x0e34,"NE34 NE2000 PCI clone"},
	{0x5240,"IDE Cntrlr"},
	{0x5241,"PCMCIA Bridge"},
	{0x5242,"General Purpose Cntrlr"},
	{0x5243,"Bus Cntrlr"},
	{0x5244,"FCD Cntrlr"},
	{0, 0}
};
static struct pci_device_information pci_device_10c8[] = {
	{0x0000,"Graphics Cntrlr"},
	{0x0003,"NM2093 MagicGraph 128ZV Video Controller"},
	{0x0004,"NM2160 MagicGraph 128XD"},
	{0x0005,"NM2200 MagicMedia 256AV"},
	{0x0006,"NM2360 MagicMedia 256ZX/256M6D"},
	{0x0016,"NM2380 MagicMedia 256XL+"},
	{0x0025,"NM2230 MagicMedia 256AV+"},
	{0x0083,"NM2097 Graphic Controller NeoMagic MagicGraph128ZV+"},
	{0x8005,"NM2200 MagicMedia 256AV Audio Device"},
	{0x8006,"NM2360 MagicMedia 256ZX Audio Device"},
	{0x8016,"NM2380 MagicMedia 256XL+ Audio Device"},
	{0, 0}
};
static struct pci_device_information pci_device_10cd[] = {
	{0x1100,"ASC1100 PCI SCSI Host Adapter"},
	{0x1200,"ASC1200 Fast SCSI-II"},
	{0x1300,"ASC-3050 ASC-3150"},
	{0x2300,"ASC2300 PCI Ultra Wide SCSI-2 Host Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_10cf[] = {
	{0x2001,"mb86605 Wide SCSI-2"},
	{0, 0}
};
static struct pci_device_information pci_device_10d9[] = {
	{0x0066,"MX86101P"},
	{0x0512,"MX98713 Fast Ethernet Adapter"},
	{0x0531,"MX98715/725 Single Chip Fast Ethernet NIC Controller"},
	{0x0532,"MX98723/727 PCI/CardBus Fast Ethernet Controller"},
	{0x8625,"MX86250"},
	{0x8626,"MX86251"},
	{0x8627,"MX86251"},
	{0x8888,"MX86200"},
	{0, 0}
};
static struct pci_device_information pci_device_10da[] = {
	{0x0508,"TC4048 Token Ring"},
	{0x3390,"Tl3c3x9 Token Ring"},
	{0, 0}
};
static struct pci_device_information pci_device_10dc[] = {
	{0x0001,"STAR/RD24 SCSI (PMC)"},
	{0x0002,"ATT 2C15-3 (FPGA) SCI bridge  on PCI 5 Volt card"},
	{0x0010,"680-1110-150/400 Simple PMC/PCI to S-LINK interface"},
	{0x0011,"680-1110-200/450 Simple S-LINK to PMC/PCI interface"},
	{0x0021,"HIPPI destination"},
	{0x0022,"HIPPI source"},
	{0x10dc,"ATT 2C15-3 (FPGA)"},
	{0, 0}
};
static struct pci_device_information pci_device_10dd[] = {
	{0x0001,"3D graphics processor"},
	{0, 0}
};
static struct pci_device_information pci_device_10de[] = {
	{0x0008,"NV1 EDGE 3D Accelerator"},
	{0x0009,"NV1 EDGE 3D Multimedia"},
	{0x0010,"Mutara V08 (NV2)"},
	{0x0018,"Riva 128 Riva 128 accelerator"},
	{0x0020,"Riva TNT AGP"},
	{0x0028,"Riva TNT2 Riva TNT2"},
	{0x0029,"Riva TNT2 Ultra"},
	{0x002a,"Riva TNT2 (NV5)"},
	{0x002b,"Riva TNT2 (NV5)"},
	{0x002c,"VANTA"},
	{0x002d,"Riva TNT2 M64 Riva TNT2 Model 64"},
	{0x002e,"VANTA (NV6)"},
	{0x002f,"VANTA (NV6)"},
	{0x00a0,"RIVA TNT2 Aladdin"},
	{0x0100,"GeForce 256"},
	{0x0101,"GeForce 256 DDR"},
	{0x0103,"GeForce 256 GL Quadro"},
	{0x0110,"NV11 GeForce 2 MX"},
	{0x0111,"NV11 DDR"},
	{0x0113,"NV11 GL"},
	{0x0150,"NV15 GeForce2 GTS"},
	{0x0151,"NV15 DDR GeForce2 GTS"},
	{0x0152,"NV15 Bladerunner GeForce2 GTS"},
	{0x0153,"NV15 GL Quadro2"},
	{0, 0}
};
static struct pci_device_information pci_device_10df[] = {
	{0x10df,"Light Pulse Fibre Channel Adapter"},
	{0x1ae5,"LP6000 Fibre Channel Host Adapter"},
	{0xf700,"LP7000 Fibre Channel Host Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_10e0[] = {
	{0x5026,"IMS5026/27/28 VL Bridge"},
	{0x5027,"IMS5027"},
	{0x5028,"IMS5028 ISA Bridge"},
	{0x8849,"IMS8849/48 VL Bridge"},
	{0x8853,"IMS8853 ATM Network Adapter"},
	{0x9128,"IMS9129 GUI Accelerator"},
	{0, 0}
};
static struct pci_device_information pci_device_10e1[] = {
	{0x0391,"TRM-S1040"},
	{0x690c,"DC-690c"},
	{0xdc20,"DC-290 EIDE Controller"},
	{0, 0}
};
static struct pci_device_information pci_device_10e3[] = {
	{0x0000,"CA91C042 VMEbus Bridge"},
	{0x0860,"CA91C860 Motorola Processor Bridge"},
	{0x0862,"CA91L826A PCI to Motorola Processor Bridge"},
	{0, 0}
};
static struct pci_device_information pci_device_10e8[] = {
	{0x2011,"Q-Motion Video Capture/Edit board"},
	{0x4750,"S5933 PCI Ctrlr"},
	{0x5920,"S5920 32-Bit PCI Bus Target Interface"},
	{0x8033,"BBK-PCI light Transputer Link Interface"},
	{0x8043,"LANai4.x Myrinet LANai interface chip"},
	{0x8062,"S5933 Parastation"},
	{0x807d,"S5933 PCI44"},
	{0x8088,"FS Kingsberg Spacetec Format Synchronizer"},
	{0x8089,"SOB Kingsberg Spacetec Serial Output Board"},
	{0x809c,"S5933 Traquair HEPC3"},
	{0x80d7,"PCI-9112"},
	{0x80d9,"PCI-9118"},
	{0x811a,"PCI-DSlink PCI-IEEE1355-DS-DE interface"},
	{0x8170,"S5933 Matchmaker PCI Chipset Development Tool"},
	{0, 0}
};
static struct pci_device_information pci_device_10ea[] = {
	{0x1680,"IGA-1680"},
	{0x1682,"IGA-1682"},
	{0x1683,"IGA-1683"},
	{0x2000,"CyberPro 2000"},
	{0x2010,"CyperPro 2000A"},
	{0x5000,"CyberPro 5000"},
	{0x5050,"CyberPro 5050"},
	{0, 0}
};
static struct pci_device_information pci_device_10eb[] = {
	{0x0101,"3GA 64 bit graphics processor"},
	{0x8111,"Twist3 Frame Grabber"},
	{0, 0}
};
static struct pci_device_information pci_device_10ec[] = {
	{0x8029,"RTL8029 NE2000 compatible Ethernet"},
	{0x8129,"RTL8129 10/100 Fast Ethernet Controller"},
	{0x8138,"RT8139B/C CardBus Fast Ethernet Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_10ed[] = {
	{0x7310,"V7310 VGA Video Overlay Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_10ee[] = {
	{0x3fc0,"RME Digi96"},
	{0x3fc1,"RME Digi96/8"},
	{0x3fc2,"RME Digi 96/8 Pro"},
	{0x3fc3,"RME Digi96/8 Pad"},
	{0, 0}
};
static struct pci_device_information pci_device_10ef[] = {
	{0x8154,"M815x Token Ring Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_10f0[] = {
	{0xa800,"VCL-P Graphics board"},
	{0xb300,"VCL-M graphics board"},
	{0, 0}
};
static struct pci_device_information pci_device_10f1[] = {
	{0x1566,"IDE/SCSI"},
	{0x1677,"Multimedia"},
	{0, 0}
};
static struct pci_device_information pci_device_10f5[] = {
	{0xa001,"NDR4000 NR4600 Bridge"},
	{0, 0}
};
static struct pci_device_information pci_device_10fa[] = {
	{0x0000,"GUI Accelerator"},
	{0x0001,"GUI Accelerator"},
	{0x0002,"GUI Accelerator"},
	{0x0003,"GUI Accelerator"},
	{0x0004,"GUI Accelerator"},
	{0x0005,"GUI Accelerator"},
	{0x0006,"GUI Accelerator"},
	{0x0007,"GUI Accelerator"},
	{0x0008,"GUI Accelerator"},
	{0x0009,"GUI Accelerator"},
	{0x000a,"GUI Accelerator"},
	{0x000b,"GUI Accelerator"},
	{0x000c,"Targa 1000 Video Capture & Editing card"},
	{0x000d,"GUI Accelerator"},
	{0x000e,"GUI Accelerator"},
	{0x000f,"GUI Accelerator"},
	{0x0010,"GUI Accelerator"},
	{0x0011,"GUI Accelerator"},
	{0x0012,"GUI Accelerator"},
	{0x0013,"GUI Accelerator"},
	{0x0014,"GUI Accelerator"},
	{0x0015,"GUI Accelerator"},
	{0, 0}
};
static struct pci_device_information pci_device_1101[] = {
	{0x0002,"Ultra SCSI Adapter"},
	{0x1060,"INI-A100U2W Ultra-2 SCSI Controller"},
	{0x134a,"Ultra SCSI Adapter"},
	{0x9100,"INI-9010/9010W Fast Wide SCSI Controller"},
	{0x9400,"INI-940 Fast Wide SCSI Controller"},
	{0x9401,"INI-950 Fast Wide SCSI Controller"},
	{0x9500,"INI-9100U/UW SCSI Controller"},
	{0x9700,"Fast Wide SCSI"},
	{0, 0}
};
static struct pci_device_information pci_device_1102[] = {
	{0x0002,"EMU10000 Sound Blaster Live!"},
	{0x1017,"Banshee 3D Blaster Banshee PCI CT6760"},
	{0x1047,"3D Blaster Annihilator 2"},
	{0x7002,"EMU10000 Game Port"},
	{0, 0}
};
static struct pci_device_information pci_device_1103[] = {
	{0x0003,"HPT343/345 UDMA EIDE Controller"},
	{0x0004,"HPT366 UDMA66 EIDE Controller"},
	{0, 0}
};
static struct pci_device_information pci_device_1105[] = {
	{0x5000,"Multimedia"},
	{0x8300,"VM491 DVD/MPEG-2 accelerator"},
	{0x8400,"EM8400 MPEG-2 Decoder"},
	{0, 0}
};
static struct pci_device_information pci_device_1106[] = {
	{0x0305,"VT8363 Host Bus-PCI Bridge"},
	{0x0391,"VT8371 KX133 Athlon Chipset Host Bridge"},
	{0x0501,"VT8501 MVP4 System Controller"},
	{0x0505,"82C505 VL Bridge"},
	{0x0561,"82C561 IDE"},
	{0x0571,"VT82C586/686 PCI IDE Controller"},
	{0x0576,"82C576 P54 Ctrlr"},
	{0x0585,"VT82C585VP Host Bus-PCI Bridge"},
	{0x0586,"VT82C586VP PCI-to-ISA Bridge"},
	{0x0595,"VT82C595 Apollo VP2 PCI North Bridge"},
	{0x0596,"VT82C596 PCI ISA Bridge"},
	{0x0597,"VT82C597 Host Bridge (Apollo VP3)"},
	{0x0598,"VT82C598 Apollo MVP3/Pro133A Host Bridge"},
	{0x0601,"VT8601"},
	{0x0680,"VT82C680 Apollo P6"},
	{0x0686,"VT82C686 PCI-to-ISA bridge"},
	{0x0691,"VT82C691/693A/694 Apollo Pro/133/133A System Controller"},
	{0x0693,"VT82C693 Apollo Pro+ Host Bridge"},
	{0x0926,"VT86C926 Amazon PCI Ethernet Controller"},
	{0x1000,"82C570MV P54 Ctrlr"},
	{0x1106,"82C570MV ISA Bridge w/IDE"},
	{0x1571,"VT82C416 IDE Controller"},
	{0x1595,"VT82C595 VP2, VP2/97 System Controller"},
	{0x3038,"VT83C572 PCI USB Controller"},
	{0x3040,"VT83C572 Power Management Controller"},
	{0x3043,"VT86C100A Rhine 10/100 Ethernet Adapter"},
	{0x3044,"OHCI Compliant IEEE 1394 Host Ctrlr"},
	{0x3050,"VT82C596 Power Management Controller"},
	{0x3051,"Power Management Controller"},
	{0x3057,"VT82C686A ACPI Power Management Controller"},
	{0x3058,"VT82C686 Audio Codec 97"},
	{0x3068,"VT82C686 Modem Codec 97"},
	{0x3086,"VT82C686 Power management"},
	{0x5030,"VT82C596 Apollo Pro ACPI Power Management Ctrlr"},
	{0x6100,"VT86C100A PCI Fast Ethernet Controller"},
	{0x8231,"VT8231 PCI to ISA Bridge"},
	{0x8305,"VT8363 PCI to AGP Bridge"},
	{0x8391,"VT8371 PCI to AGP Bridge"},
	{0x8501,"CPU to AGP Controller?"},
	{0x8596,"VT82C596 PCI to AGP Bridge"},
	{0x8597,"VT82C597 PCI-to-PCI Bridge (AGP)"},
	{0x8598,"VT82C598/686A Apollo MVP3 PCI-to-PCI Bridge"},
	{0x8601,"PCI to AGP Controller?"},
	{0x8691,"VT82C691 Apollo Pro PCI-to-PCI Bridge"},
	{0x8693,"VT82C693 Apollo Pro+ PCI-to-PCI Bridge"},
	{0, 0}
};
static struct pci_device_information pci_device_1107[] = {
	{0x8576,"PCI Host Bridge"},
	{0, 0}
};
static struct pci_device_information pci_device_1108[] = {
	{0x0100,"p1690plus-AA Token Ring Adapter"},
	{0x0101,"p1690plus-AB 2-Port Token Ring Adapter"},
	{0x0105,"P1690Plus Token Ring Adapter"},
	{0x0108,"P1690Plus Token Ring Adapter"},
	{0x0138,"P1690Plus Token Ring Adapter"},
	{0x0139,"P1690Plus Token Ring Adapter"},
	{0x013c,"P1690Plus Token Ring Adapter"},
	{0x013d,"P1690Plus Token Ring Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_1109[] = {
	{0x1400,"EM110TX EX110TX PCI Fast Ethernet Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_110a[] = {
	{0x0002,"Piranha PCI-EIDE Adapter (2-port)"},
	{0x0005,"Tulip-Ctrlr, Power-Mgmt, Switch Extender"},
	{0x113c,"FPGA-CPTR Hardware Tracer for CP113C / CP113D"},
	{0x113e,"FPGA-CPTRE Hardware Tracer for CP113E"},
	{0x2102,"PEB/PEF 20534 DSCC4 Multiprotocol HDLC Controller"},
	{0x3160,"MCCA Pentium-PCI Host Bridge Core ASIC"},
	{0x4942,"FPGA-IBTR I-Bus Tracer for MBD"},
	{0x6120,"SZB6120 Multimedia Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_110b[] = {
	{0x0001,"Mpact Media Processor"},
	{0, 0}
};
static struct pci_device_information pci_device_1110[] = {
	{0x6037,"Firepower Powerized SMP I/O ASIC"},
	{0x6073,"Firepower Powerized SMP I/O ASIC"},
	{0, 0}
};
static struct pci_device_information pci_device_1112[] = {
	{0x2200,"FDDI adapter"},
	{0x2300,"fast ethernet adapter"},
	{0x2340,"4 Port FEN Adapter 4 10/100 UTP Fast Ethernet Adapter"},
	{0x2400,"ATM adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_1113[] = {
	{0x1211,"EN-1207D Fast Ethernet Adapter"},
	{0x1217,"EN-1217 Ethernet Adapter"},
	{0x9211,"EN-1207D Fast Ethernet Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_1116[] = {
	{0x0022,"DT3001"},
	{0x0023,"DT3002"},
	{0x0024,"DT3003"},
	{0x0025,"DT3004"},
	{0x0026,"Dt3005"},
	{0x0027,"DT3001-PGL"},
	{0x0028,"DT3003-PGL"},
	{0, 0}
};
static struct pci_device_information pci_device_1117[] = {
	{0x9500,"max-lc SVGA card"},
	{0x9501,"MaxPCI image processing board"},
	{0, 0}
};
static struct pci_device_information pci_device_1119[] = {
	{0x0000,"GDT6000/6020/6050 SCSI RAID"},
	{0x0001,"GDT6000/6010 SCSI RAID"},
	{0x0002,"GDT6110/6510 SCSI RAID"},
	{0x0003,"GDT6120/6520 2-chan SCSI RAID"},
	{0x0004,"GDT6530 3-chan SCSI RAID"},
	{0x0005,"GDT6550 5-chan SCSI RAID"},
	{0x0006,"GDT6117/6517"},
	{0x0007,"GDT6127/6527"},
	{0x0008,"GDT6537"},
	{0x0009,"GDT6557"},
	{0x000a,"GDT6115/6515"},
	{0x000b,"GDT6125/6525"},
	{0x000c,"GDT6535"},
	{0x000d,"GDT6555"},
	{0x0100,"GDT6117RP/6517 2 Channel SCSI"},
	{0x0101,"GDT6127RP/6527RP"},
	{0x0102,"GDT6537RP"},
	{0x0103,"GDT6557RP"},
	{0x0104,"GDT6111RP/6511RP"},
	{0x0105,"GDT6127RP/6527RP"},
	{0x0110,"GDT6117RP1/6517RP1"},
	{0x0111,"GDT6127RP1/6527RP1"},
	{0x0112,"GDT6537RP1"},
	{0x0113,"GDT6557RP1"},
	{0x0114,"GDT6111RP1/6511RP1"},
	{0x0115,"GDT6127RP1/6527RP1"},
	{0x0118,"GDT 6x18RD"},
	{0x0119,"GDT 6x28RD"},
	{0x011a,"GDT 6x38RD"},
	{0x011b,"GDT 6x58RD"},
	{0x0120,"GDT6117RP2/6517RP2"},
	{0x0121,"GDT6127RP2/6527RP2"},
	{0x0122,"GDT6537RP2"},
	{0x0123,"GDT6557RP2"},
	{0x0124,"GDT6111RP2/6511RP2"},
	{0x0125,"GDT6127RP2/6527RP2"},
	{0x0168,"GDT 7x18RN"},
	{0x0169,"GDT 7x28RN"},
	{0x016a,"GST 7x38RN"},
	{0x016b,"GDT 7x58RN"},
	{0x0210,"GDT 6x19RD"},
	{0x0211,"GDT 6x29RD"},
	{0x0260,"GDT 7x19RN"},
	{0x0261,"GDT 7x29RN"},
	{0, 0}
};
static struct pci_device_information pci_device_111a[] = {
	{0x0000,"155P-MF1"},
	{0x0002,"166P-MF1"},
	{0x0003,"ENI-25P ATM Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_111c[] = {
	{0x0001,"Powerbus Bridge"},
	{0, 0}
};
static struct pci_device_information pci_device_111d[] = {
	{0x0001,"IDT77211 ATM Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_111f[] = {
	{0x4a47,"Precision MX Video engine interface"},
	{0x5243,"Frame Capture Bus Interface"},
	{0, 0}
};
static struct pci_device_information pci_device_1127[] = {
	{0x0200,"FireRunner PCA-200 ATM"},
	{0x0210,"PCA-200PC ATM"},
	{0x0250,"ATM"},
	{0x0300,"PCA-200EPC ATM"},
	{0x0310,"ATM"},
	{0x0400,"ForeRunner HE ATM Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_112e[] = {
	{0x0000,"EIDE/hdd and IDE/cd-rom Ctrlr"},
	{0x000b,"EIDE/hdd and IDE/cd-rom Ctrlr"},
	{0, 0}
};
static struct pci_device_information pci_device_112f[] = {
	{0x0000,"ICPCI"},
	{0x0001,"video frame grabber/processor"},
	{0, 0}
};
static struct pci_device_information pci_device_1131[] = {
	{0x2780,"tda 2780 AQ Tv deflection"},
	{0x5400,"TM100 TriMedia"},
	{0x7145,"SAA7145"},
	{0x7146,"SAA7146 Multi Media Bridge Scaler"},
	{0, 0}
};
static struct pci_device_information pci_device_1133[] = {
	{0x7901,"EiconCard S90"},
	{0xe001,"DIVA Pro 2.0 S/T"},
	{0xe002,"DIVA 2.0 S/T"},
	{0xe003,"DIVA Pro 2.0 U"},
	{0xe004,"DIVA 2.0 U"},
	{0xe005,"DIVA 2.01 S/T Eicon ISDN card using Siemens IPAC chip"},
	{0xe010,"DIVA Server BRI-2M"},
	{0xe014,"DIVA Server PRI-30M"},
	{0, 0}
};
static struct pci_device_information pci_device_1134[] = {
	{0x0001,"Raceway Bridge"},
	{0, 0}
};
static struct pci_device_information pci_device_1135[] = {
	{0x0001,"Printer Cntrlr"},
	{0, 0}
};
static struct pci_device_information pci_device_1138[] = {
	{0x8905,"8905 STD 32 Bridge"},
	{0, 0}
};
static struct pci_device_information pci_device_113c[] = {
	{0x0000,"PCI9060 i960 Bridge"},
	{0x0001,"PCI9060 i960 Bridge / Evaluation Platform"},
	{0x0911,"PCI911 i960Jx I/O Controller"},
	{0x0912,"PCI912 i960Cx I/O Controller"},
	{0x0913,"PCI913 i960Hx I/O Controller"},
	{0x0914,"PCI914 I/O Controller with secondary PCI bus"},
	{0, 0}
};
static struct pci_device_information pci_device_113f[] = {
	{0x0808,"SST-64P Adapter"},
	{0x1010,"SST-128P Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_1141[] = {
	{0x0001,"EIDE/ATAPI super adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_1142[] = {
	{0x3210,"ProMotion 3210 VGA/AVI Playback Accelerator"},
	{0x6410,"GUI Accelerator"},
	{0x6412,"GUI Accelerator"},
	{0x6420,"GUI Accelerator"},
	{0x6422,"Provideo 6422"},
	{0x6424,"ProVideo 6424 GUI Accelerator"},
	{0x6425,"ProMotion AT25"},
	{0x6426,"GUI Accelerator"},
	{0x643d,"AT25 ProMotion-AT3D"},
	{0, 0}
};
static struct pci_device_information pci_device_1144[] = {
	{0x0001,"Noservo Cntrlr"},
	{0, 0}
};
static struct pci_device_information pci_device_1148[] = {
	{0x4000,"FDDI adapter"},
	{0x4200,"Token Ring"},
	{0x4300,"SK-984x SK-NET Gigabit Ethernet Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_114a[] = {
	{0x7587,"VMIVME-7587"},
	{0, 0}
};
static struct pci_device_information pci_device_114f[] = {
	{0x0002,"AccelePort EPC"},
	{0x0003,"RightSwitch SE-6"},
	{0x0004,"AccelePort Xem"},
	{0x0005,"AccelePort Xr"},
	{0x0006,"AccelePort C/X"},
	{0x0009,"AccelePort Xr/J"},
	{0x000a,"AccelePort EPC/J"},
	{0x000c,"DataFirePRIme T1"},
	{0x000d,"SyncPort X.25/FR 2-port"},
	{0x0011,"AccelePort8r EIA-232"},
	{0x0012,"AccelePort8r EIA-422"},
	{0x0013,"AccelePort Xr"},
	{0x0014,"AccelePort8r EIA-422"},
	{0x0015,"AccelePort Xem"},
	{0x0016,"AccelePort EPC/X"},
	{0x0017,"AccelePort C/X"},
	{0x001a,"DataFirePRIme E1"},
	{0x001b,"AccelePort C/X (IBM)"},
	{0x001d,"DataFire RAS T1/E1/PRI"},
	{0x0023,"AccelePort RAS"},
	{0x0024,"DataFire RAS B4 ST/U"},
	{0x0026,"AccelePort 4r 920"},
	{0x0027,"AccelePort 8r 920"},
	{0x0034,"AccelePort 2r 920"},
	{0x0035,"DataFire DSP T1/E1/PRI, Compact PCI"},
	{0x6001,"Avanstar"},
	{0, 0}
};
static struct pci_device_information pci_device_1155[] = {
	{0x0810,"486 CPU/PCI Bridge"},
	{0x0922,"Pentium CPU/PCI Bridge"},
	{0x0926,"PCI/ISA Bridge"},
	{0, 0}
};
static struct pci_device_information pci_device_1158[] = {
	{0x3011,"Tokenet/vg 1001/10m anylan"},
	{0x9050,"Lanfleet/Truevalue"},
	{0x9051,"Lanfleet/Truevalue"},
	{0, 0}
};
static struct pci_device_information pci_device_1159[] = {
	{0x0001,"MV1000"},
	{0x0002,"MV-1500 Frame Grabber"},
	{0, 0}
};
static struct pci_device_information pci_device_115d[] = {
	{0x0003,"CardBus Ethernet 10/100"},
	{0x0005,"CardBus Ethernet 10/100"},
	{0x0007,"CardBus Ethernet 10/100"},
	{0x000b,"CardBus Ethernet 10/100"},
	{0x000f,"CardBus Ethernet 10/100"},
	{0x0101,"CardBus 56k Modem"},
	{0x0103,"CardBus Ehternet + 56k Modem"},
	{0, 0}
};
static struct pci_device_information pci_device_1161[] = {
	{0x0001,"Host Bridge"},
	{0, 0}
};
static struct pci_device_information pci_device_1163[] = {
	{0x0001,"Verite 1000 3D Blaster"},
	{0x2000,"Verite 2000"},
	{0, 0}
};
static struct pci_device_information pci_device_1165[] = {
	{0x0001,"Motion JPEG rec/play w/audio"},
	{0, 0}
};
static struct pci_device_information pci_device_1166[] = {
	{0x0005,"PCI Host Bridge (Southbridge copy)"},
	{0x0007,"CNB20-LE CPU to PCI Bridge"},
	{0x0008,"CNB20HE"},
	{0, 0}
};
static struct pci_device_information pci_device_116a[] = {
	{0x6100,"Bus/Tag Channel"},
	{0x6800,"Escon Channel"},
	{0x7100,"Bus/Tag Channel"},
	{0x7800,"Escon Channel"},
	{0, 0}
};
static struct pci_device_information pci_device_1178[] = {
	{0xafa1,"Fast Ethernet"},
	{0, 0}
};
static struct pci_device_information pci_device_1179[] = {
	{0x0406,"Tecra Video Capture device"},
	{0x0601,"Toshiba CPU to PCI bridge"},
	{0x0602,"PCI to ISA Bridge for Notebooks"},
	{0x0603,"ToPIC95 PCI to CardBus Bridge for Notebooks"},
	{0x0604,"PCI to PCI Bridge for Notebooks"},
	{0x0605,"PCI to ISA Bridge for Notebooks"},
	{0x0606,"PCI to ISA Bridge for Notebooks"},
	{0x0609,"PCI to PCI Bridge for Notebooks"},
	{0x060a,"Toshiba ToPIC95 CardBus Controller"},
	{0x060f,"ToPIC97 CardBus Controller"},
	{0x0611,"PCI to ISA Bridge"},
	{0x0617,"ToPIC95 PCI to CardBus Bridge with ZV support"},
	{0x0618,"CPU to PCI and PCI to ISA Bridge"},
	{0x0701,"PCI Communication Device"},
	{0x0d01,"FIR Port Type-DO"},
	{0, 0}
};
static struct pci_device_information pci_device_117e[] = {
	{0x0001,"Printer Host"},
	{0, 0}
};
static struct pci_device_information pci_device_1180[] = {
	{0x0465,"RL5C465 CardBus controller"},
	{0x0466,"RL5C466 CardBus controller"},
	{0x0475,"RL5C475 CardBus controller"},
	{0x0476,"RL5C476 CardBus controller"},
	{0x0477,"RLc477 CardBus Controller"},
	{0x0478,"RLc478 CardBus Controller"},
	{0, 0}
};
static struct pci_device_information pci_device_1185[] = {
	{0x8929,"EIDE Controller"},
	{0, 0}
};
static struct pci_device_information pci_device_1186[] = {
	{0x0100,"DC21041 Ethernet Adapter"},
	{0x1100,"Fast Ethernet Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_1189[] = {
	{0x1592,"VL/PCI Bridge"},
	{0, 0}
};
static struct pci_device_information pci_device_118c[] = {
	{0x0014,"PCIB C-bus II to PCI bus host bridge chip"},
	{0x1117,"MAC-94C201B3 Corollary/Intel Memory Controller Chip"},
	{0, 0}
};
static struct pci_device_information pci_device_118d[] = {
	{0x0001,"n/a Raptor-PCI framegrabber"},
	{0x0012,"Model 12 Road Runner Frame Grabber"},
	{0x0014,"Model 14 Road Runner Frame Grabber"},
	{0x0024,"Model 24 Road Runner Frame Grabber"},
	{0x0044,"Model 44 Road Runner Frame Grabber"},
	{0x0112,"Model 12 Road Runner Frame Grabber"},
	{0x0114,"Model 14 Road Runner Frame Grabber"},
	{0x0124,"Model 24 Road Runner Frame Grabber"},
	{0x0144,"Model 44 Road Runner Frame Grabber"},
	{0x0212,"Model 12 Road Runner Frame Grabber"},
	{0x0214,"Model 14 Road Runner Frame Grabber"},
	{0x0224,"Model 24 Road Runner Frame Grabber"},
	{0x0244,"Model 44 Road Runner Frame Grabber"},
	{0x0312,"Model 12 Road Runner Frame Grabber"},
	{0x0314,"Model 14 Road Runner Frame Grabber"},
	{0x0324,"Model 24 Road Runner Frame Grabber"},
	{0x0344,"Model 44 Road Runner Frame Grabber"},
	{0, 0}
};
static struct pci_device_information pci_device_1190[] = {
	{0x2550,"TC-2550 Single Chip Ultra (Wide) SCSI Processor"},
	{0xc721,"EIDE"},
	{0xc731,"TP-910/920/940 PCI Ultra (Wide) SCSI Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_1191[] = {
	{0x0001,"IDE Ctrlr"},
	{0x0002,"ATP850UF UltraDMA33 EIDE Controller (AEC6210UF)"},
	{0x0003,"SCSI-2 cache Cntrlr"},
	{0x0004,"atp8400a ASIC cache controller"},
	{0x0005,"ATP850UF UltraDMA33 EIDE Controller (AEC6210UF)"},
	{0x0006,"ATP860A UltraDMA66 EDIE Controller (AEC6260)"},
	{0x0007,"ATP860A UltraDMA66 EIDE Controller (AEC6260)"},
	{0x8001,"ATP8600 SCSI-2 RAID (cache?) Adapter (AEC6820U)"},
	{0x8002,"ATP850S SCSI-2 Host Adapter (AEC6710L/F)"},
	{0x8010,"ATP870 Ultra Wide SCSI Controller"},
	{0x8020,"ATP870 Ultra SCSI Controller"},
	{0x8030,"ATP870 Ultra SCSI Controller"},
	{0x8040,"ATP870 SCSI Controller"},
	{0x8050,"Ultra Wide SCSI Controller"},
	{0, 0}
};
static struct pci_device_information pci_device_1193[] = {
	{0x0001,"1221"},
	{0x0002,"1225"},
	{0, 0}
};
static struct pci_device_information pci_device_1199[] = {
	{0x0001,"IRMA 3270 PCI Adapter"},
	{0x0002,"Advanced ISCA PCI Adapter"},
	{0x0201,"SDLC PCI Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_119b[] = {
	{0x1221,"82C092G"},
	{0, 0}
};
static struct pci_device_information pci_device_11a9[] = {
	{0x4240,"AMCC S5933Q Intelligent Serial Card"},
	{0, 0}
};
static struct pci_device_information pci_device_11ab[] = {
	{0x0146,"GT-64010 System ctrlr w/PCI for R46xx CPU"},
	{0x4801,"GT-48001 8 port switched ethernet ctrlr"},
	{0x4809,"EV-48300 Evaluation board for the GT-48300"},
	{0xf003,"GT-64010 Primary Image Piranha Image Generator"},
	{0xf004,"GT64120 Primary Image Barracuda Image Generator"},
	{0, 0}
};
static struct pci_device_information pci_device_11ad[] = {
	{0x0002,"NGMC169B 10/100 Ethernet (NetGear FA310TX)"},
	{0xc115,"LC82C115 PNIC II 10/100 PCI MAC/PHY"},
	{0, 0}
};
static struct pci_device_information pci_device_11ae[] = {
	{0x4153,"Bridge Controller"},
	{0x5842,"Bridge Controller"},
	{0, 0}
};
static struct pci_device_information pci_device_11b0[] = {
	{0x0292,"V292PBC Am29030/40 Bridge"},
	{0x0960,"V96xPBC i960 Bridges for i960 Processors"},
	{0xc960,"V96DPC i960 Dual PCI Bridge"},
	{0, 0}
};
static struct pci_device_information pci_device_11b8[] = {
	{0x0001,"Quad PeerMaster"},
	{0, 0}
};
static struct pci_device_information pci_device_11b9[] = {
	{0xc0ed,"SSA Ctrlr"},
	{0, 0}
};
static struct pci_device_information pci_device_11bc[] = {
	{0x0001,"NPI NuCard PCI FDDI"},
	{0, 0}
};
static struct pci_device_information pci_device_11c1[] = {
	{0x0440,"LT Winmodem 56k Data+Fax+Voice+DSVD"},
	{0x0441,"LT Winmodem 56k Data+Fax"},
	{0x0442,"1646T00 V.90 Lucent Modem"},
	{0x0443,"LT Winmodem"},
	{0x0444,"LT Winmodem"},
	{0x0445,"LT Winmodem"},
	{0x0446,"LT Winmodem"},
	{0x0447,"LT Winmodem"},
	{0x0448,"LT Winmodem 56k"},
	{0x0449,"LT Winmodem 56k"},
	{0x044a,"LT Winmodem 56k"},
	{0x044b,"LT Winmodem"},
	{0x044c,"LT Winmodem"},
	{0x044d,"LT Winmodem"},
	{0x044e,"LT Winmodem"},
	{0x0450,"LT Winmodem"},
	{0x0451,"LT Winmodem"},
	{0x0452,"LT Winmodem"},
	{0x0453,"LT Winmodem"},
	{0x0454,"LT Winmodem"},
	{0x0455,"LT Winmodem"},
	{0x0456,"LT Winmodem"},
	{0x0457,"LT Winmodem"},
	{0x0458,"LT Winmodem"},
	{0x0459,"LT Winmodem"},
	{0x045a,"LT Winmodem"},
	{0x0480,"Venus Winmodem"},
	{0x5801,"USB Open Host Controller"},
	{0, 0}
};
static struct pci_device_information pci_device_11c8[] = {
	{0x0658,"PSB PCI-SCI Bridge"},
	{0xd665,"PSB64"},
	{0xd667,"PSB66"},
	{0, 0}
};
static struct pci_device_information pci_device_11c9[] = {
	{0x0010,"16-line serial port w/DMA"},
	{0x0011,"4-line serial port w/DMA"},
	{0, 0}
};
static struct pci_device_information pci_device_11cb[] = {
	{0x2000,"PCI-9050 Target Interface"},
	{0x4000,"SUPI-1 XIO/SIO Host"},
	{0x8000,"T225 Bridge RIO Host"},
	{0, 0}
};
static struct pci_device_information pci_device_11d1[] = {
	{0x01f7,"VxP524 PCI Video Processor"},
	{0, 0}
};
static struct pci_device_information pci_device_11d4[] = {
	{0x2f44,"ADSP-2141 SafeNet Crypto Accelerator chip"},
	{0, 0}
};
static struct pci_device_information pci_device_11d5[] = {
	{0x0115,"10115 Greensheet"},
	{0x0117,"10117 Greensheet"},
	{0, 0}
};
static struct pci_device_information pci_device_11de[] = {
	{0x6057,"ZR36057 MotionJPEG/TV Card"},
	{0, 0}
};
static struct pci_device_information pci_device_11f0[] = {
	{0x4232,"FASTline UTP Quattro"},
	{0x4233,"FASTline FO"},
	{0x4234,"FASTline UTP"},
	{0x4235,"FASTline-II UTP"},
	{0x4236,"FASTline-II FO"},
	{0x4731,"GIGAline"},
	{0, 0}
};
static struct pci_device_information pci_device_11f4[] = {
	{0x2915,"2915"},
	{0, 0}
};
static struct pci_device_information pci_device_11f6[] = {
	{0x0112,"ReadyLink ENET100-VG4"},
	{0x1401,"ReadyLink 2000 (Winbod W89C940)"},
	{0x2011,"RL100-ATX 10/100Ethernet Adapter"},
	{0x2201,"ReadyLink 100TX (Winbond W89C840)"},
	{0x9881,"RL100TX Fast Ethernet Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_11f8[] = {
	{0x7375,"PM7375 LASAR-155 ATM SAR"},
	{0, 0}
};
static struct pci_device_information pci_device_11fe[] = {
	{0x0001,"RocketPort"},
	{0x0002,"RocketPort"},
	{0x0003,"RocketPort"},
	{0x0004,"RocketPort"},
	{0x0005,"RocketPort"},
	{0x0006,"RocketPort"},
	{0x0008,"RocketPort"},
	{0x0009,"RocketPort"},
	{0x000a,"RocketPort"},
	{0x000b,"RocketPort"},
	{0x000c,"RocketPort"},
	{0, 0}
};
static struct pci_device_information pci_device_1208[] = {
	{0x4853,"HS-Link Device"},
	{0, 0}
};
static struct pci_device_information pci_device_120e[] = {
	{0x0100,"Cyclom-Y Multiport Serial Card"},
	{0x0101,"Cyclom-Y Multiport Serial Card"},
	{0x0102,"Cyclom-4Y Multiport Serial Card"},
	{0x0103,"Cyclom-4Y Multiport Serial Card"},
	{0x0104,"Cyclom-8Y Multiport Serial Card"},
	{0x0105,"Cyclom-8Y Multiport Serial Card"},
	{0x0200,"Cyclom-Z Intelligent Multiport Serial"},
	{0x0201,"Cyclom-Z Intelligent Serial Card"},
	{0, 0}
};
static struct pci_device_information pci_device_120f[] = {
	{0x0001,"Roadrunner"},
	{0, 0}
};
static struct pci_device_information pci_device_1217[] = {
	{0x673a,"OZ6730 PCI to PCMCIA Bridge"},
	{0x6792,"OZ6729 PCI to PCMCIA Bridge"},
	{0x6832,"OZ6832/3 CardBus Controller"},
	{0x6836,"OZ6836/6860 CardBus Controller"},
	{0x6872,"OZ6812 CardBus Controller"},
	{0x6933,"OZ6933 CardBus Controller"},
	{0, 0}
};
static struct pci_device_information pci_device_121a[] = {
	{0x0001,"Voodoo Voodoo 3D Acceleration Chip"},
	{0x0002,"Voodoo2 Voodoo 2 3D Accelerator"},
	{0x0003,"Voodoo Banshee Voodoo Banshee"},
	{0x0005,"Voodoo3 All Voodoo3 chips, 3000"},
	{0, 0}
};
static struct pci_device_information pci_device_1220[] = {
	{0x1220,"AMCC 5933 TMS320C80 DSP/Imaging Board"},
	{0, 0}
};
static struct pci_device_information pci_device_122d[] = {
	{0x1206,"368DSP"},
	{0x50dc,"3328 Audio"},
	{0x80da,"3328 Audio"},
	{0, 0}
};
static struct pci_device_information pci_device_1236[] = {
	{0x0000,"RealMagic64/GX"},
	{0x6401,"REALmagic64/GX GUI Accelerator"},
	{0, 0}
};
static struct pci_device_information pci_device_123d[] = {
	{0x0000,"EasyConnect 8/32"},
	{0x0002,"EasyConnect 8/64"},
	{0x0003,"EasyIO"},
	{0, 0}
};
static struct pci_device_information pci_device_123f[] = {
	{0x00e4,"MPEG"},
	{0x8120,"176 E4?"},
	{0x8888,"Cinemaster C 3.0 DVD Decoder"},
	{0, 0}
};
static struct pci_device_information pci_device_1242[] = {
	{0x4643,"FCI-1063 Fibre Channel Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_1244[] = {
	{0x0700,"B1 ISDN controller"},
	{0x0a00,"A1 AVM Fritz!Card"},
	{0x0a00,"A1 AVM Fritz!Card"},
	{0, 0}
};
static struct pci_device_information pci_device_124d[] = {
	{0x0000,"EasyConnect 8/32"},
	{0x0002,"EasyConnect 8/64"},
	{0x0003,"EasyIO PCI"},
	{0, 0}
};
static struct pci_device_information pci_device_124f[] = {
	{0x0041,"IFT-2000 PCI RAID Controller"},
	{0, 0}
};
static struct pci_device_information pci_device_1255[] = {
	{0x1110,"MPEG Forge"},
	{0x1210,"MPEG Fusion"},
	{0x2110,"VideoPlex"},
	{0x2120,"VideoPlex CC"},
	{0x2130,"VideoQuest"},
	{0, 0}
};
static struct pci_device_information pci_device_1256[] = {
	{0x4401,"PCI-2220i Dale EIDE Adapter"},
	{0x5201,"PCI-2000 IntelliCache SCSI Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_1259[] = {
	{0x2560,"AT-2560 Fast Ethernet Adapter (i82557B)"},
	{0, 0}
};
static struct pci_device_information pci_device_125d[] = {
	{0x0000,"ESS336H PCI Fax Modem (early model)"},
	{0x1968,"ES1968 Maestro-2 PCI audio accelerator"},
	{0x1969,"ES1938/41/46 Solo-1 PCI AudioDrive family"},
	{0x199b,"ES1938 Maestro-3.COMM PCI Voice+Fax Modem"},
	{0x2808,"ES336H PCI Fax Modem (later model)"},
	{0x2898,"ES2898 ES56-PI Family V.90 PCI Modem"},
	{0, 0}
};
static struct pci_device_information pci_device_1260[] = {
	{0x8130,"HMP8130 NTSC/PAL Video Decoder"},
	{0x8131,"HMP8131 NTSC/PAL Video Decoder"},
	{0, 0}
};
static struct pci_device_information pci_device_1266[] = {
	{0x0001,"NE10/100 Adapter (i82557B)"},
	{0x1910,"NE2000Plus (RT8029) Ethernet Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_1267[] = {
	{0x5352,"PCR2101"},
	{0x5a4b,"telsatturbo"},
	{0, 0}
};
static struct pci_device_information pci_device_126f[] = {
	{0x0710,"SM710 LynxEM"},
	{0x0712,"SM712 LynxEM+"},
	{0x0720,"SM720 Lynx3DM"},
	{0x0810,"SM810 LynxE"},
	{0x0811,"SM811 LynxE"},
	{0x0820,"SM820 Lynx3D"},
	{0x0910,"SM910 Lynx"},
	{0, 0}
};
static struct pci_device_information pci_device_1273[] = {
	{0x0002,"DirecPC"},
	{0, 0}
};
static struct pci_device_information pci_device_1274[] = {
	{0x1371,"ES1371 AudioPCI"},
	{0x5000,"ES1370 AudioPCI"},
	{0x5880,"5880 AudioPCI"},
	{0, 0}
};
static struct pci_device_information pci_device_1279[] = {
	{0x0295,"Virtual Northbridge"},
	{0, 0}
};
static struct pci_device_information pci_device_127a[] = {
	{0x1002,"RC56HCFPCI Modem enumerator"},
	{0x1003,"HCF 56k V.90 Modem"},
	{0x1004,"HCF 56k V.90 Modem"},
	{0x1005,"HCF 56k V.90 Speakerphone Modem"},
	{0x1025,"HCF 56k PCI Modem"},
	{0x1026,"HCF 56k V.90 Speakerphone Modem"},
	{0x1035,"HCF 56k Speakerphone Modem"},
	{0x1085,"Volcano HCF 56k PCI Modem"},
	{0x2004,"SoftK56VB2.1V2.08.02 K56 modem"},
	{0x2013,"Soft 56K PCI modem"},
	{0x2014,"PCI modem"},
	{0x2015,"Conexant SoftK56 Speakerphone Modem"},
	{0x4320,"Riptide PCI Audio Controller"},
	{0x4321,"Riptide HCF 56k PCI Modem"},
	{0x4322,"Riptide PCI Game Controller"},
	{0x8234,"RapidFire 616X ATM155 Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_1282[] = {
	{0x9102,"DM9102/A GFast Ethernet Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_1283[] = {
	{0x673a,"IT8330G IDE Controller"},
	{0x8330,"IT8330G Host Bridge"},
	{0x8888,"IT8888F PCI to ISA Bridge with SMB"},
	{0x8889,"IT8889F PCI to ISA Bridge"},
	{0xe886,"IT8330G PCI to ISA Bridge"},
	{0, 0}
};
static struct pci_device_information pci_device_1285[] = {
	{0x0100,"ES1849 Maestro-1 AudioDrive"},
	{0, 0}
};
static struct pci_device_information pci_device_1287[] = {
	{0x001e,"LS220D DVD Decoder"},
	{0x001f,"LS220C DVD Decoder"},
	{0, 0}
};
static struct pci_device_information pci_device_128d[] = {
	{0x0021,"ATM Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_128e[] = {
	{0x0008,"ST128 WSS/SB"},
	{0x0009,"ST128 SAM9407"},
	{0x000a,"ST128 Game Port"},
	{0x000b,"ST128 MPU Port"},
	{0x000c,"ST128 Control Port"},
	{0, 0}
};
static struct pci_device_information pci_device_12ab[] = {
	{0x3000,"TUN-200/MPG-200C PCI TV (and DVD Decoder?) Card"},
	{0, 0}
};
static struct pci_device_information pci_device_12ae[] = {
	{0x0001,"ACENIC"},
	{0, 0}
};
static struct pci_device_information pci_device_12b9[] = {
	{0x1006,"USR 56k Internal WinModem"},
	{0x1007,"USR 56k Internal WinModem"},
	{0x1008,"USR 56k Internal Modem"},
	{0, 0}
};
static struct pci_device_information pci_device_12be[] = {
	{0x3041,"AN3041Q CO-MEM PCI Bus Interface and Cache"},
	{0, 0}
};
static struct pci_device_information pci_device_12c5[] = {
	{0x007f,"ISE PEI Imaging Subsystem Engine"},
	{0x0081,"PCIVST PCI Thresholding Engine"},
	{0x0085,"Video Simulator/Sender"},
	{0x0086,"THR2 Multi-scale Thresholder"},
	{0, 0}
};
static struct pci_device_information pci_device_12d2[] = {
	{0x0008,"NV1"},
	{0x0009,"DAC64"},
	{0x0018,"RIVA 128 Riva 128 2D/3D GUI Accelerator"},
	{0x0019,"RIVA 128ZX 2D/3D GUI Accelerator"},
	{0x0020,"TNT"},
	{0x0028,"TNT2"},
	{0x0029,"UTNT2"},
	{0x002c,"VTNT2"},
	{0x00a0,"ITNT2"},
	{0, 0}
};
static struct pci_device_information pci_device_12db[] = {
	{0x0003,"FoxFire II"},
	{0, 0}
};
static struct pci_device_information pci_device_12de[] = {
	{0x0200,"Cryptoswift 200"},
	{0, 0}
};
static struct pci_device_information pci_device_12e0[] = {
	{0x0010,"ST16C654 Quad UART"},
	{0x0020,"ST16C654 Quad UART"},
	{0x0030,"ST16C654 Quad UART"},
	{0, 0}
};
static struct pci_device_information pci_device_12e4[] = {
	{0x1140,"ISDN Controller"},
	{0, 0}
};
static struct pci_device_information pci_device_12eb[] = {
	{0x0001,"AU8820 Vortex Digital Audio Processor"},
	{0, 0}
};
static struct pci_device_information pci_device_12f8[] = {
	{0x0002,"VideoMaker"},
	{0, 0}
};
static struct pci_device_information pci_device_1307[] = {
	{0x0001,"PCI-DAS1602/16"},
	{0x0006,"PCI-GPIB"},
	{0x000b,"PCI-DIO48H"},
	{0x000c,"PCI-PDISO8"},
	{0x000d,"PCI-PDISO16"},
	{0x000f,"PCI-DAS1200"},
	{0x0010,"PCI-DAS1602/12"},
	{0x0014,"PCI-DIO24H"},
	{0x0015,"PCI-DIO24H/CTR3"},
	{0x0016,"PCI-DIO24H/CTR16"},
	{0x0017,"PCI-DIO96H"},
	{0x0018,"PCI-CTR05"},
	{0x0019,"PCI-DAS1200/JR"},
	{0x001a,"PCI-DAS1001"},
	{0x001b,"PCI-DAS1002"},
	{0x001c,"PCI-DAS1602JR/16"},
	{0x001d,"PCI-DAS6402/16"},
	{0x001e,"PCI-DAS6402/12"},
	{0x001f,"PCI-DAS16/M1"},
	{0x0020,"PCI-DDA02/12"},
	{0x0021,"PCI-DDA04/12"},
	{0x0022,"PCI-DDA08/12"},
	{0x0023,"PCI-DDA02/16"},
	{0x0024,"PCI-DDA04/16"},
	{0x0025,"PCI-DDA08/16"},
	{0x0026,"PCI-DAC04/12-HS"},
	{0x0027,"PCI-DAC04/16-HS"},
	{0x0028,"CIO-DIO24 24 Bit Digital Input/Output Board"},
	{0x0029,"PCI-DAS08"},
	{0x002c,"PCI-INT32"},
	{0x0033,"PCI-DUAL-AC5"},
	{0x0034,"PCI-DAS-TC"},
	{0x0035,"PCI-DAS64/M1/16"},
	{0x0036,"PCI-DAS64/M2/16"},
	{0x0037,"PCI-DAS64/M3/16"},
	{0x004c,"PCI-DAS1000"},
	{0, 0}
};
static struct pci_device_information pci_device_1308[] = {
	{0x0001,"NetCelerator Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_1318[] = {
	{0x0911,"G-NIC II 1000BT Network Interface Card"},
	{0, 0}
};
static struct pci_device_information pci_device_1319[] = {
	{0x0801,"FM801 PCI audio controller"},
	{0x0802,"FM801 PCI Joystick"},
	{0x1000,"FM801 PCI Audio"},
	{0x1001,"FM801 PCI Joystick"},
	{0, 0}
};
static struct pci_device_information pci_device_134a[] = {
	{0x0001,"Domex DMX 3191 PCI SCSI Controller"},
	{0x0002,"Domex DMX3194UP SCSI Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_134d[] = {
	{0x7890,"HSP MicroModem 56"},
	{0x7891,"HSP MicroModem 56"},
	{0x7892,"HSP MicroModem 56"},
	{0x7893,"HSP MicroModem 56"},
	{0x7894,"HSP MicroModem 56"},
	{0x7895,"HSP MicroModem 56"},
	{0x7896,"HSP MicroModem 56"},
	{0x7897,"97860963 HSP MicroModem 56"},
	{0, 0}
};
static struct pci_device_information pci_device_135e[] = {
	{0x7101,"Single Port RS-232/422/485/520"},
	{0x7201,"Dual Port RS-232/422/485 Interface"},
	{0x7202,"Dual Port RS-232 Interface"},
	{0x7401,"Four Port RS-232 Interface"},
	{0x7402,"Four Port RS-422/485 Interface"},
	{0x7801,"Eight Port RS-232 Interface"},
	{0x8001,"8001 Digital I/O Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_1385[] = {
	{0x620a,"GA620"},
	{0, 0}
};
static struct pci_device_information pci_device_1389[] = {
	{0x0001,"PCI1500PFB Intelligent fieldbus Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_1397[] = {
	{0x2bd0,"BIPAC-PCI Billion ISDN Card"},
	{0, 0}
};
static struct pci_device_information pci_device_13c0[] = {
	{0x0010,"SyncLink PCI WAN Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_13c1[] = {
	{0x1000,"ATA-RAID"},
	{0, 0}
};
static struct pci_device_information pci_device_13df[] = {
	{0x0001,"PCI56RVP Modem"},
	{0, 0}
};
static struct pci_device_information pci_device_13f6[] = {
	{0x0100,"CMI8338-031 PCI Audio Device"},
	{0x0101,"CMI8338-031 PCI Audio Device"},
	{0x0111,"CMI8738/C3DX PCI Audio Device"},
	{0x0211,"HSP56 Audiomodem Riser"},
	{0, 0}
};
static struct pci_device_information pci_device_1407[] = {
	{0x8000,"Lava Parallel"},
	{0x8001,"Lava Dual Parallel port A"},
	{0x8002,"Lava Dual Parallel port A"},
	{0x8003,"Lava Dual Parallel port B"},
	{0x8800,"BOCA Research IOPPAR"},
	{0, 0}
};
static struct pci_device_information pci_device_1412[] = {
	{0x1712,"ICE1712 Envy24 PCI Multi-Channel I/O Ctrlr"},
	{0, 0}
};
static struct pci_device_information pci_device_1448[] = {
	{0x0001,"ADAT/EDIT Audio Editing"},
	{0, 0}
};
static struct pci_device_information pci_device_144a[] = {
	{0x7296,"PCI-7296"},
	{0x7432,"PCI-7432"},
	{0x7433,"PCI-7433"},
	{0x7434,"PCI-7434"},
	{0x7841,"PCI-7841"},
	{0x8133,"PCI-8133"},
	{0x8554,"PCI-8554"},
	{0x9111,"PCI-9111"},
	{0x9113,"PCI-9113"},
	{0x9114,"PCI-9114"},
	{0, 0}
};
static struct pci_device_information pci_device_145f[] = {
	{0x0001,"NextMove PCI"},
	{0, 0}
};
static struct pci_device_information pci_device_148d[] = {
	{0x1003,"Creative ModemBlaster V.90 PCI DI5635"},
	{0, 0}
};
static struct pci_device_information pci_device_14b3[] = {
	{0x0000,"DSL NIC"},
	{0, 0}
};
static struct pci_device_information pci_device_14b7[] = {
	{0x0001,"Symphony 4110"},
	{0, 0}
};
static struct pci_device_information pci_device_14b9[] = {
	{0x0001,"PC4800"},
	{0, 0}
};
static struct pci_device_information pci_device_14d4[] = {
	{0x0400,"Panacom 7 Interface chip"},
	{0, 0}
};
static struct pci_device_information pci_device_14db[] = {
	{0x2120,"TK9902"},
	{0, 0}
};
static struct pci_device_information pci_device_14dc[] = {
	{0x0000,"PCI230"},
	{0x0001,"PCI242"},
	{0x0002,"PCI244"},
	{0x0003,"PCI247"},
	{0x0004,"PCI248"},
	{0x0005,"PCI249"},
	{0x0006,"PCI260"},
	{0x0007,"PCI224"},
	{0x0008,"PCI234"},
	{0x0009,"PCI236"},
	{0, 0}
};
static struct pci_device_information pci_device_14f2[] = {
	{0x0120,"Mobility Split Bridge"},
	{0, 0}
};
static struct pci_device_information pci_device_1500[] = {
	{0x1300,"SIS900 10/100M PCI Fast Ethernet Controller"},
	{0x1320,"VT86C100A 10/100M PCI Fast Ethernet Controler"},
	{0x1360,"RTL8139A 10/100 Mbps PCI Fast Ethernet Controller"},
	{0x1380,"DEC21143PD 10/100M PCI Fast Ethernet Controller"},
	{0, 0}
};
static struct pci_device_information pci_device_1507[] = {
	{0x0001,"MPC105 Eagle"},
	{0x0002,"MPC106 Grackle"},
	{0x0003,"MPC8240 Kahlua"},
	{0x0100,"MPC145575 HFC-PCI"},
	{0x0431,"KTI829c 100VG"},
	{0x4801,"Raven"},
	{0x4802,"Falcon"},
	{0x4803,"Hawk"},
	{0x4806,"CPX8216"},
	{0, 0}
};
static struct pci_device_information pci_device_151a[] = {
	{0x1002,"PCI-1002"},
	{0x1004,"PCI-1004"},
	{0x1008,"PCI-1008"},
	{0, 0}
};
static struct pci_device_information pci_device_157c[] = {
	{0x8001,"Fix2000 PCI Y2K Compliance Card"},
	{0, 0}
};
static struct pci_device_information pci_device_1592[] = {
	{0x0781,"Multi-IO Card"},
	{0x0782,"Dual Parallel Port Card (EPP)"},
	{0x0783,"Multi-IO Card"},
	{0x0785,"Multi-IO Card"},
	{0x0786,"Multi-IO Card"},
	{0x0787,"Multi-IO Card 2 series"},
	{0x0788,"Multi-IO Card"},
	{0x078a,"Multi-IO Card"},
	{0, 0}
};
static struct pci_device_information pci_device_15ad[] = {
	{0x0710,"Virtual SVGA"},
	{0, 0}
};
static struct pci_device_information pci_device_15dc[] = {
	{0x0001,"Argus 300 PCI Cryptography Module"},
	{0, 0}
};
static struct pci_device_information pci_device_1619[] = {
	{0x0400,"FarSync T2P Two Port Intelligent Sync Comms Card"},
	{0x0440,"FarSync T4P Four Port Intelligent Sync Comms Card"},
	{0, 0}
};
static struct pci_device_information pci_device_1a08[] = {
	{0x0000,"SC15064"},
	{0, 0}
};
static struct pci_device_information pci_device_1c1c[] = {
	{0x0001,"FR710 EIDE Ctrlr"},
	{0x0001,"82C101 IDE Ctrlr"},
	{0, 0}
};
static struct pci_device_information pci_device_1d44[] = {
	{0xa400,"PM2x24/3224 SCSI Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_1de1[] = {
	{0x0391,"TRM-S1040 SCSI ASIC"},
	{0x2020,"DC-390 SCSI Controller"},
	{0x690c,"DC-690C IDE Cache Controller"},
	{0xdc29,"DC290M Bus Master IDE PCI 2 controllers"},
	{0, 0}
};
static struct pci_device_information pci_device_2348[] = {
	{0x2010,"8142 100VG/AnyLAN"},
	{0, 0}
};
static struct pci_device_information pci_device_3388[] = {
	{0x8011,"CPU to PCI Bridge"},
	{0x8012,"PCI to ISA Bridge"},
	{0x8013,"EIDE Controller"},
	{0, 0}
};
static struct pci_device_information pci_device_3d3d[] = {
	{0x0001,"GLint 300SX 3D Accelerator"},
	{0x0002,"GLint 500TX Sapphire 3D Accelerator"},
	{0x0003,"GLint Delta Geometry processor"},
	{0x0004,"3C0SX 2D+3D Accelerator"},
	{0x0005,"Permedia 2D+3D Accelerator"},
	{0x0006,"GLint MX 3D Accelerator"},
	{0x0007,"3D Extreme Permedia II 2D+3D Accelerator"},
	{0x0008,"GLint Gamma G1"},
	{0x0009,"Permedia 3 2D+3D Accelerator"},
	{0x000a,"GLint R3"},
	{0x0100,"Permedia II 2D+3D Accelerator"},
	{0x1004,"Permedia 3D+3D Accelerator"},
	{0x3d04,"Permedia 2D+3D Accelerator"},
	{0xffff,"GLint VGA"},
	{0, 0}
};
static struct pci_device_information pci_device_4005[] = {
	{0x0300,"ALS300 PCI Audio Device"},
	{0x0308,"ALS300+ PCI Audio Device"},
	{0x0309,"ALS300+ PCI Input Controller"},
	{0x1064,"ALG2064 GUI Accelerator"},
	{0x2064,"ALG2032/2064i GUI Accelerator"},
	{0x2128,"ALG2364A GUI Accelerator"},
	{0x2301,"ALG2301 GUI Accelerator"},
	{0x2302,"ALG2302 GUI Accelerator"},
	{0x2303,"AVG2302 GUI Accelerator"},
	{0x2364,"ALG2364 GUI Accelerator"},
	{0x2464,"ALG2464 GUI Accelerator"},
	{0x2501,"ALG2564A/25128A GUI Accelerator"},
	{0x4000,"ALS4000 Audio Chipset"},
	{0, 0}
};
static struct pci_device_information pci_device_4033[] = {
	{0x1300,"SIS900 10/100Mbps Fast Ethernet Controller"},
	{0x1320,"VT86C100A 10/100M PCI Fast Ethernet Controller"},
	{0x1360,"RTL8139A 10/100 Mbps PCI Fast Ethernet Controller"},
	{0x1380,"DEC 21143PD 10/100M PCI Fast Ethernet Controller"},
	{0, 0}
};
static struct pci_device_information pci_device_4a14[] = {
	{0x5000,"NV5000 RT8029-based Ethernet Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_4b10[] = {
	{0x3080,"SCSI Host Adapter"},
	{0x4010,"Fast/wide SCSI-2"},
	{0, 0}
};
static struct pci_device_information pci_device_4d51[] = {
	{0x0200,"MQ-200"},
	{0, 0}
};
static struct pci_device_information pci_device_5053[] = {
	{0x2010,"Daytona Audio Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_5145[] = {
	{0x3031,"Concert AudioPCI"},
	{0, 0}
};
static struct pci_device_information pci_device_5301[] = {
	{0x0001,"ProMotion aT3D"},
	{0, 0}
};
static struct pci_device_information pci_device_5333[] = {
	{0x0551,"86C551 Plato/PX"},
	{0x5631,"86C325 Virge 3D GUI Accelerator"},
	{0x8800,"Vision 866 GUI Accelerator"},
	{0x8801,"Vision 964 GUI Accelerator"},
	{0x8810,"86C732 Trio 32 GUI Accelerator rev. 0"},
	{0x8811,"86C764/765 Trio 64/64V+ GUI Accelerator"},
	{0x8812,"86CM65? Aurora 64V+"},
	{0x8813,"86C764 Trio 32/64 GUI Accelerator v3"},
	{0x8814,"86C767 Trio 64UV+ GUI Accelerator"},
	{0x8815,"86CM66 Aurora128"},
	{0x883d,"86C988 ViRGE/VX 3D GUI Accelerator"},
	{0x8870,"Fire GL"},
	{0x8880,"86C868 Vision 868 GUI Accelerator VRAM rev. 0"},
	{0x8881,"86C868 Vision 868 GUI Accelerator VRAM rev. 1"},
	{0x8882,"86C868 Vision 868 GUI Accelerator VRAM rev. 2"},
	{0x8883,"86C868 Vision 868 GUI Accelerator VRAM rev. 3"},
	{0x88b0,"86C928 Vision 928 GUI Accelerator VRAM rev. 0"},
	{0x88b1,"86C928 Vision 928 GUI Accelerator VRAM rev. 1"},
	{0x88b2,"86C928 Vision 928 GUI Accelerator VRAM rev. 2"},
	{0x88b3,"86C928 Vision 928 GUI Accelerator VRAM rev. 3"},
	{0x88c0,"86C864 Vision 864 GUI Accelerator DRAM rev. 0"},
	{0x88c1,"86C864 Vision 864 GUI Accelerator DRAM rev. 1"},
	{0x88c2,"86C864 Vision 864 GUI Accelerator DRAM rev. 2"},
	{0x88c3,"86C864 Vision 864 GUI Accelerator DRAM rev. 3"},
	{0x88d0,"86C964 Vision 964 GUI Accelerator VRAM rev. 0"},
	{0x88d1,"86C964 Vision 964-P GUI Accelerator VRAM rev. 1"},
	{0x88d2,"86C964 Vision 964-P GUI Accelerator DRAM rev 2"},
	{0x88d3,"86C964 Vision 964-P GUI Accelerator VRAM rev. 3"},
	{0x88f0,"86C968 Vision 968 GUI Accelerator VRAM rev. 0"},
	{0x88f1,"86C968 Vision 968 GUI Accelerator VRAM rev. 1"},
	{0x88f2,"86C968 Vision 968 GUI Accelerator VRAM rev. 2"},
	{0x88f3,"86C968 Vision 968 GUI Accelerator VRAM rev. 3"},
	{0x8900,"86C775 Trio64V2/DX"},
	{0x8901,"86C775/86C785 Trio 64V2 DX/GX"},
	{0x8902,"86C551 SMA Family"},
	{0x8903,"TrioV Family"},
	{0x8904,"86C365/366 Trio3D"},
	{0x8905,"86C765 Trio64V+ comatible"},
	{0x8906,"86C765 Trio64V+ comatible"},
	{0x8907,"86C765 Trio64V+ comatible"},
	{0x8908,"86C765 Trio64V+ comatible"},
	{0x8909,"86C765 Trio64V+ comatible"},
	{0x890a,"86C765 Trio64V+ comatible"},
	{0x890b,"86C765 Trio64V+ compatible"},
	{0x890c,"86C765 Trio64V+ compatible"},
	{0x890d,"86C765 Trio64V+ compatible"},
	{0x890e,"86C765 Trio64V+ compatible"},
	{0x890f,"86C765 Trio64V+ compatible"},
	{0x8a01,"82C375/86C385 ViRGE /DX & /GX"},
	{0x8a10,"86C357 ViRGE /GX2"},
	{0x8a11,"86C359 ViRGE /GX2+ Macrovision"},
	{0x8a12,"86C359 ViRGE /GX2+"},
	{0x8a13,"86C362 Trio3DX2 AGP"},
	{0x8a20,"86C390/391 Savage3D"},
	{0x8a21,"86C390 Savage3D/MV"},
	{0x8a22,"86C394-397 Savage 4"},
	{0x8a23,"86C394-397 Savage 4"},
	{0x8c00,"85C260 ViRGE/M3 (ViRGE/MX)"},
	{0x8c01,"86C260 ViRGE/M5 (ViRGE/MX)"},
	{0x8c02,"86C240 ViRGE/MXC"},
	{0x8c03,"86C280 ViRGE /MX+ Macrovision"},
	{0x8c10,"86C270/274/290/294 Savage MX/IX/MX+MV/IX+MV"},
	{0x8c12,"86C270/274/290/294 Savage MX/IX/MX+MV/IX+MV"},
	{0x9102,"86C410 Savage 2000"},
	{0xca00,"86C617 SonicVibes PCI Audio Accelerator"},
	{0, 0}
};
static struct pci_device_information pci_device_5455[] = {
	{0x4458,"S5933 PCI-MyBus-Bridge"},
	{0, 0}
};
static struct pci_device_information pci_device_5555[] = {
	{0x0003,"TURBOstor HFP-832 HiPPI NIC"},
	{0, 0}
};
static struct pci_device_information pci_device_6356[] = {
	{0x4002,"ULTRA24 SCSI Host"},
	{0x4102,"ULTRA24 SCSI Host"},
	{0x4202,"ULTRA24 SCSI Host"},
	{0x4302,"ULTRA24 SCSI Host"},
	{0x4402,"ULTRA24 SCSI Host"},
	{0x4502,"ULTRA24 SCSI Host"},
	{0x4602,"ULTRA24 SCSI Host"},
	{0x4702,"ULTRA24 SCSI Host"},
	{0x4802,"ULTRA24 SCSI Host"},
	{0x4902,"ULTRA24 SCSI Host"},
	{0x4a02,"ULTRA24 SCSI Host"},
	{0x4b02,"ULTRA24 SCSI Host"},
	{0x4c02,"ULTRA24 SCSI Host"},
	{0x4d02,"ULTRA24 SCSI Host"},
	{0x4e02,"ULTRA24 SCSI Host"},
	{0x4f02,"ULTRA24 SCSI Host"},
	{0, 0}
};
static struct pci_device_information pci_device_6374[] = {
	{0x6773,"GPPCI PCI Interface"},
	{0, 0}
};
static struct pci_device_information pci_device_6666[] = {
	{0x0001,"PCCOM4"},
	{0x0002,"PCCOM8"},
	{0, 0}
};
static struct pci_device_information pci_device_8001[] = {
	{0x0010,"ispLSI1032E PCI-decoder"},
	{0, 0}
};
static struct pci_device_information pci_device_8008[] = {
	{0x0010,"PWDOG1/2 PCI-Watchdog 1"},
	{0x0011,"PWDOG1/2 Watchdog2/PCI"},
	{0x0016,"PROTO2"},
	{0x0100,"PREL8"},
	{0x0102,"PREL16"},
	{0x0103,"POPTOREL16"},
	{0x0105,"POPTO16IN"},
	{0x0106,"PTTL24IO"},
	{0x0107,"PUNIREL"},
	{0x1000,"PDAC4"},
	{0x1001,"PAD12DAC4"},
	{0x1002,"PAD16DAC4"},
	{0x1005,"PAD12"},
	{0x1006,"PAD16"},
	{0x3000,"POPTOLCA"},
	{0, 0}
};
static struct pci_device_information pci_device_8086[] = {
	{0x0008,"Extended Express System Support Ctrlr"},
	{0x0482,"82375EB PCI-EISA Bridge (PCEB)"},
	{0x0483,"82424TX/ZX CPU (i486) Bridge (Saturn)"},
	{0x0484,"82378ZB/IB SIO ISA Bridge"},
	{0x0486,"82425EX PCI System Controller (PSC) for i486 (Aries)"},
	{0x04a3,"82434LX CPU (Pentium) Bridge (Mercury)"},
	{0x0960,"80960RP i960 RP Microprocessor/Bridge"},
	{0x1000,"82542 Gigabit Ethernet Controller"},
	{0x1001,"82543GC 10/100/1000 Ethernet Controller"},
	{0x1029,"PRO/100 PCI Ethernet Adapter"},
	{0x1030,"82559 PCI Networking device"},
	{0x1100,"82815 Host-Hub Interface Bridge / DRAM Ctrlr"},
	{0x1101,"82815 AGP Bridge"},
	{0x1102,"82815 Internal Graphics Device"},
	{0x1110,"8x815 Host-Hub Interface Bridge / DRAM Ctrlr"},
	{0x1112,"82815 Internal Graphics Device"},
	{0x1120,"82815 Host-Hub Interface Bridge / DRAM Ctrlr"},
	{0x1121,"82815 AGP Bridge"},
	{0x1130,"82815/82815EM Host-Hub Interface Bridge / DRAM Ctrlr"},
	{0x1131,"82815/82815EM AGP Bridge"},
	{0x1132,"82815 Internal Graphics Device"},
	{0x1161,"82806AA I/O APIC Device"},
	{0x1209,"82559ER"},
	{0x1221,"82092AA PCMCIA Bridge"},
	{0x1222,"82092AA IDE Ctrlr"},
	{0x1223,"SAA7116 Video Controller"},
	{0x1225,"82452KX/GX Orion Extended Express CPU to PCI Bridge"},
	{0x1226,"82596 EtherExpress PRO/10"},
	{0x1227,"82865 EtherExpress PRO100"},
	{0x1228,"EE PRO/100 Smart Intelligent 10/100 Fast Ethernet Adapter"},
	{0x1229,"82557/8/9 Fast Ethernet LAN Controller"},
	{0x122d,"82437FX System Controller (TSC)"},
	{0x122e,"82371FB PCI to ISA Bridge (Triton)"},
	{0x1230,"82371FB IDE Interface (Triton)"},
	{0x1231,"DSVD Modem"},
	{0x1234,"82371MX Mobile PCI I/O IDE Xcelerator (MPIIX)"},
	{0x1235,"82437MX Mobile System Controller (MTSC)"},
	{0x1237,"82441FX PCI & Memory Controller (PMC)"},
	{0x1239,"82371FB IDE Interface (Triton)"},
	{0x123c,"82380AB Mobile PCI-to-ISA Bridge (MISA)"},
	{0x123d,"683053 Programmable Interrupt Device"},
	{0x1240,"82752 AGP Graphics Accelerator"},
	{0x124b,"82380FB 82380FB(MPCI2)"},
	{0x1250,"82439HX System Controller (TXC)"},
	{0x1360,"82806AA Hub Interface to PCI Bridge"},
	{0x1361,"82806AA Advanced Interrupt Controller"},
	{0x1960,"80960RP i960RP Microprocessor"},
	{0x1a21,"82840 Host-Hub Interface A Bridge / DRAM Ctrlr"},
	{0x1a23,"82840 AGP Bridge"},
	{0x1a24,"82840 Hub Interface B Bridge"},
	{0x2125,"82801AB AC97 Audio Controller"},
	{0x2410,"82801AA LPC Interface"},
	{0x2411,"82801AA IDE Controller"},
	{0x2412,"82801AA USB Controller"},
	{0x2413,"82801AA SMBus Controller"},
	{0x2415,"82801AA AC97 Audio Controller"},
	{0x2416,"82801AA AC97 Modem Controller"},
	{0x2418,"82801AA Hub Interface-to-PCI Bridge"},
	{0x2420,"82801AB LPC Interface"},
	{0x2421,"82801AB IDE Controller"},
	{0x2422,"82801AB USB Controller"},
	{0x2423,"82801AB SMBus Controller"},
	{0x2425,"82801AB AC97 Audio Controller"},
	{0x2426,"82801AB AC97 Modem Controller"},
	{0x2428,"82801AB Hub Interface-to-PCI Bridge"},
	{0x2440,"82801BA LPC Interface Bridge"},
	{0x2442,"82801BA USB Controller"},
	{0x2443,"82801BA SMBus Controller"},
	{0x2444,"82801BA USB Controller"},
	{0x2445,"82801BA AC97 Audio Controller"},
	{0x2446,"82801BA AC97 Modem Controller"},
	{0x2449,"82801BA LAN Controller"},
	{0x244b,"82801BA IDE Controller"},
	{0x244e,"82801BA Hub Interface to PCI Bridge"},
	{0x2500,"82820 Host-Hub Interface Bridge / DRAM Ctrlr"},
	{0x2501,"82820 Host Bridge (MCH)"},
	{0x250b,"82820 Host Bridge (MCH)"},
	{0x250f,"82820 AGP Bridge"},
	{0x2520,"82805AA Memory Translator Hub (MTH)"},
	{0x2521,"82804AA Memory Repeater Hub for SDRAM (MRH-S)"},
	{0x5200,"PCI to PCI Bridge"},
	{0x5201,"Network Controller"},
	{0x7000,"82371SB PIIX3 PCI-to-ISA Bridge (Triton II)"},
	{0x7010,"82371SB PIIX3 IDE Interface (Triton II)"},
	{0x7020,"82371SB PIIX3 USB Host Controller (Triton II)"},
	{0x7030,"82437VX System Controller"},
	{0x7051,"PB 642365-003 Intel Business Video Conferencing Card"},
	{0x7100,"82439TX System Controller (MTXC), part of 430TX chipset"},
	{0x7110,"82371AB PIIX4 ISA Bridge"},
	{0x7111,"82371AB PIIX4 IDE Controller"},
	{0x7112,"82371AB PIIX4 USB Interface"},
	{0x7113,"82371AB PIIX4 Power Management Controller"},
	{0x7120,"82810 Host-Hub Interface Bridge / DRAM Ctrlr"},
	{0x7121,"82810 Graphics Device"},
	{0x7122,"82810-DC100 Host-Hub Interface Bridge / DRAM Ctrlr"},
	{0x7123,"82810-DC100 Graphics Device"},
	{0x7124,"82810E Host-Hub Interface Bridge / DRAM Ctrlr"},
	{0x7125,"82810E Graphics Device"},
	{0x7180,"82443LX/EX (PAC) Host/PCI bridge in 440LX/EX AGP chipset"},
	{0x7181,"AGP device in 440LX/EX AGP chipset"},
	{0x7190,"82443BX/ZX 440BX/ZX AGPset Host Bridge"},
	{0x7191,"82443BX/ZX 440BX/ZX AGPset PCI-to-PCI bridge"},
	{0x7192,"82443BX/ZX 440BX/ZX chipset Host-to-PCI Bridge"},
	{0x7194,"82443MX I/O Controller?"},
	{0x7195,"82443MX? AC97 Audio Controller"},
	{0x7198,"82443MX PCI to ISA Bridge"},
	{0x7199,"82443MX EIDE Controller"},
	{0x719a,"82443MX USB Universal Host Controller"},
	{0x719b,"82443MX Power Management Controller"},
	{0x71a0,"82443GX Host-to-PCI Bridge"},
	{0x71a1,"82443GX PCI-to-PCI Bridge (AGP)"},
	{0x71a2,"82443GX Host-to-PCI Bridge"},
	{0x7600,"82372FB PCI to ISA Bridge"},
	{0x7601,"82372FB EIDE Controller"},
	{0x7602,"82372FB PCI to USB Universal Host Controller"},
	{0x7800,"82740 AGP Graphics Accelerator"},
	{0x84c4,"82450KX/GX 450KX/GX PCI Bridge (Orion)"},
	{0x84c5,"82453KX/GX 450KX/GX Memory Controller (Orion)"},
	{0x84ca,"82451NX 450NX PCIset Memory & I/O Controller"},
	{0x84cb,"82454NX 450NX PCIset PCI Expander Bridge"},
	{0, 0}
};
static struct pci_device_information pci_device_8800[] = {
	{0x2008,"video assistant component"},
	{0, 0}
};
static struct pci_device_information pci_device_8e2e[] = {
	{0x3000,"Et32/Px Ethernet Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_9004[] = {
	{0x1078,"AIC-7810C RAID Coprocessor"},
	{0x1160,"AIC-1160 Fibre Channel Adapter"},
	{0x2178,"AIC-7821 SCSI Controller"},
	{0x3860,"AIC-2930U Ultra SCSI Ctrlr"},
	{0x3b78,"AHA-4944W/4944UW QuadChannel Fast-Wide/Ultra-Wide Diff. SCSI Ctrlr"},
	{0x5075,"AIC-755x SCSI Ctrlr"},
	{0x5078,"AIC-7850P Fast/Wide SCSI Controller"},
	{0x5175,"AIC-755x SCSI Ctrlr"},
	{0x5178,"AIC-7850 FAST-SCSI Ctrlr"},
	{0x5275,"AIC-755x SCSI Ctrlr"},
	{0x5278,"AIC-7850 Fast SCSI Ctrlr"},
	{0x5375,"AIC-755x SCSI Ctrlr"},
	{0x5378,"AIC-7850 Fast SCSI Ctrlr"},
	{0x5475,"AIC-755x SCSI Ctrlr"},
	{0x5478,"AIC-7850 Fast SCSI Ctrlr"},
	{0x5575,"AVA-2930 SCSI Ctrlr"},
	{0x5578,"AIC-7855 Fast SCSI Ctrlr"},
	{0x5675,"AIC-755x SCSI Ctrlr"},
	{0x5678,"AIC-7856 Fast SCSI Ctrlr"},
	{0x5775,"AIC-755x SCSI Ctrlr"},
	{0x5778,"AIC-7850 Fast SCSI Ctrlr"},
	{0x5800,"AIC-5800 PCI-to-1394 Ctrlr"},
	{0x5900,"ANA-5910/30/40 ATM155 & 25 LAN Controller"},
	{0x5905,"ANA-5910A/30A/40A ATM Adpater"},
	{0x6038,"AIC-3860 SCSI Host Adpater"},
	{0x6075,"AIC-7560? CardBus Ultra SCSI Controller"},
	{0x6078,"AIC-7860 SCSI Ctrlr"},
	{0x6178,"AIC-7861 SCSI Controller"},
	{0x6278,"AIC-7860 SCSI Ctrlr"},
	{0x6378,"AIC-7860 SCSI Ctrlr"},
	{0x6478,"AIC-786x SCSI Ctrlr"},
	{0x6578,"AIC-786x SCSI Ctrlr"},
	{0x6678,"AIC-786x SCSI Ctrlr"},
	{0x6778,"AIC-786x SCSI Ctrlr"},
	{0x6915,"ANA620xx/69011A Fast Ethernet"},
	{0x7078,"AIC-7870 Fast and Wide SCSI Ctrlr"},
	{0x7178,"AHA-2940/2940W Fast/Fast-Wide SCSI Ctrlr"},
	{0x7278,"AHA-3940/3940W Multichannel Fast/Fast-Wide SCSI Ctrlr"},
	{0x7378,"AHA-3985 4-chan RAID SCSI Ctrlr"},
	{0x7478,"AHA-2944 SCSI Ctrlr"},
	{0x7578,"AHA-3944/3944W Multichannel Fast/Fast-Wide Diff. SCSI Ctrlr"},
	{0x7678,"AHA-4944W/4944UW QuadChannel Fast-Wide/Ultra-Wide Diff. SCSI Ctrlr"},
	{0x7778,"AIC-787x SCSI Ctrlr"},
	{0x7810,"aic 7810 Memory control IC"},
	{0x7815,"AIC-7515 RAID + Memory Controller IC"},
	{0x7850,"aic-7850 Fast/Wide SCSI-2 Controller"},
	{0x7855,"aha 2930 Single SCSI channel"},
	{0x7860,"AIC-7860 SCSI Ctrlr"},
	{0x7870,"aic-7870 SCSI IC"},
	{0x7871,"aha 2940 SCSI"},
	{0x7872,"aha 3940 Multiple SCSI channels"},
	{0x7873,"aha 3980 Multiple SCSI channels"},
	{0x7874,"aha 2944 Differential SCSI"},
	{0x7880,"aic7880 Fast 20 SCSI"},
	{0x7890,"AIC-7890 SCSI controller"},
	{0x7891,"AIC-789x SCSI controller"},
	{0x7892,"AIC-789x SCSI controller"},
	{0x7893,"AIC-789x SCSI controller"},
	{0x7894,"AIC-789x SCSI controller"},
	{0x7895,"AIC-7895 Ultra-Wide SCSI Ctrlr on AHA-2940 AHA-394x"},
	{0x7896,"AIC-789x SCSI controller"},
	{0x7897,"AIC-789x SCSI controller"},
	{0x8078,"AIC-7880 Ultra Wide SCSI"},
	{0x8178,"AHA-2940U/2940UW Ultra/Ultra-Wide SCSI Ctrlr"},
	{0x8278,"AHA-3940Uxx AHA-3940U/3940UW/3940UWD SCSI Ctrlr"},
	{0x8378,"AIC-7883U SCSI Controller"},
	{0x8478,"AHA-2944UW Ultra-Wide Diff. SCSI Ctrlr"},
	{0x8578,"AHA-3944U/3944UWD Fast-Wide/Ultra-Wide Diff. SCSI Ctrlr"},
	{0x8678,"AHA-4944UW QuadChannel Ultra-Wide Diff. SCSI Ctrlr"},
	{0x8778,"AIC-788x Ultra-Wide SCSI Ctrlr"},
	{0x8878,"AIC-7888? Ultra Wide SCSI Controller"},
	{0x8b78,"ABA-1030"},
	{0xec78,"AHA-4944W/4944UW QuadChannel Fast-Wide/Ultra-Wide Diff. SCSI Ctrlr"},
	{0, 0}
};
static struct pci_device_information pci_device_9005[] = {
	{0x0010,"AHA-2940U2W/U2B,2950U2W Ultra2 SCSI"},
	{0x0011,"AHA-2930U2 Ultra2 SCSI Host Adapter"},
	{0x0013,"AIC-7890/1 SCSI Controller"},
	{0x001f,"AIC-7890/1 Ultra2-Wide SCSI controller"},
	{0x0020,"AIC-789x SCSI Controller"},
	{0x002f,"AIC-789x SCSI Controller"},
	{0x0030,"AIC-789x SCSI Controller"},
	{0x003f,"AIC-789x SCSI Controller"},
	{0x0050,"AHA-3940U2x/3950U2x Ultra2 SCSI Adapter"},
	{0x0051,"AHA-3950U2x Ultra2 SCSI Adapter"},
	{0x0053,"AIC-7896 SCSI Controller"},
	{0x005f,"AIC-7896/7 Ultra2 SCSI Controller"},
	{0x0080,"AIC-7892A Ultra160/m PCI SCSI Controller"},
	{0x0081,"AIC-7892B Ultra160 SCSI Controller"},
	{0x0083,"AIC-7892D Ultra160 SCSI Controller"},
	{0x008f,"AIC-7892 Ultra160 SCSI Controller"},
	{0x00c0,"AIC-7899A Ultra160 SCSI Controller"},
	{0x00c1,"AIC-7899B Ultra160 SCSI Controller"},
	{0x00c3,"AIC-7899D Ultra160 SCSI Controller"},
	{0x00cf,"AIC-7899 Ultra160 SCSI Controller"},
	{0, 0}
};
static struct pci_device_information pci_device_907f[] = {
	{0x2015,"IDE-2015PL EIDE Ctrlr"},
	{0, 0}
};
static struct pci_device_information pci_device_9412[] = {
	{0x6565,"HT6565 IDE Controller?"},
	{0, 0}
};
static struct pci_device_information pci_device_e000[] = {
	{0xe000,"W89C940 Ethernet Adapter"},
	{0, 0}
};
static struct pci_device_information pci_device_e159[] = {
	{0x0001,"Tiger 300/320 PCI interface"},
	{0x0600,"Tiger 600 PCI-to-PCI Bridge"},
	{0, 0}
};
static struct pci_device_information pci_device_edd8[] = {
	{0xa091,"ARK1000PV Stingray GUI Accelerator"},
	{0xa099,"ARK2000PV Stingray GUI Accelerator"},
	{0xa0a1,"ARK2000MT Stingray 64"},
	{0xa0a9,"ARK2000MI Quadro645"},
	{0, 0}
};
static struct pci_device_information pci_device_fffe[] = {
	{0x0710,"Virtual SVGA"},
	{0, 0}
};
static struct pci_device_information pci_device_ffff[] = {
	{0x0140,"BAD ! BAD Buslogic BT-946C SCSI?"},
	{0, 0}
};
static struct pci_vendor_information pci_vendor_information[] = {
	{0x0000,"Gammagraphx, Inc.",0},
	{0x001a,"Ascend Communications, Inc.",0},
	{0x0033,"Paradyne Corp.",0},
	{0x003d,"Lockheed Martin Corp",0},
	{0x0070,"Hauppauge Computer Works Inc.",0},
	{0x0100,"Ncipher Corp. Ltd",0},
	{0x0123,"General Dynamics",0},
	{0x0675,"Dynalink",pci_device_0675},
	{0x0a89,"BREA Technologies Inc.",0},
	{0x0e11,"Compaq Computer Corp.",pci_device_0e11},
	{0x1000,"LSI Logic",pci_device_1000},
	{0x1001,"Kolter Electronic - Germany",pci_device_1001},
	{0x1002,"ATI Technologies",pci_device_1002},
	{0x1003,"ULSI",pci_device_1003},
	{0x1004,"VLSI Technology",pci_device_1004},
	{0x1005,"Avance Logic Inc.",pci_device_1005},
	{0x1006,"Reply Group",0},
	{0x1007,"Netframe Systems",0},
	{0x1008,"Epson",0},
	{0x100a,"Phoenix Technologies Ltd.",0},
	{0x100b,"National Semiconductor",pci_device_100b},
	{0x100c,"Tseng Labs",pci_device_100c},
	{0x100d,"AST Research",0},
	{0x100e,"Weitek",pci_device_100e},
	{0x1010,"Video Logic Ltd.",0},
	{0x1011,"Digital Equipment Corporation",pci_device_1011},
	{0x1012,"Micronics Computers Inc.",0},
	{0x1013,"Cirrus Logic",pci_device_1013},
	{0x1014,"IBM",pci_device_1014},
	{0x1015,"LSI Logic Corp of Canada",0},
	{0x1016,"Fujitsu ICL Computers",0},
	{0x1017,"Spea Software AG",pci_device_1017},
	{0x1018,"Unisys Systems",0},
	{0x1019,"Elitegroup Computer Sys",0},
	{0x101a,"NCR/AT&T GIS",pci_device_101a},
	{0x101b,"Vitesse Semiconductor",0},
	{0x101c,"Western Digital",pci_device_101c},
	{0x101e,"American Megatrends Inc.",pci_device_101e},
	{0x101f,"PictureTel Corp.",0},
	{0x1020,"Hitachi Computer Electronics",0},
	{0x1021,"Oki Electric Industry",0},
	{0x1022,"Advanced Micro Devices",pci_device_1022},
	{0x1023,"Trident Microsystems",pci_device_1023},
	{0x1024,"Zenith Data Systems",0},
	{0x1025,"Acer Incorporated",pci_device_1025},
	{0x1028,"Dell Computer Corporation",pci_device_1028},
	{0x1029,"Siemens Nixdorf AG",0},
	{0x102a,"LSI Logic Headland Div",pci_device_102a},
	{0x102b,"Matrox",pci_device_102b},
	{0x102c,"Chips And Technologies",pci_device_102c},
	{0x102d,"Wyse Technologies",pci_device_102d},
	{0x102e,"Olivetti Advanced Technology",0},
	{0x102f,"Toshiba America",pci_device_102f},
	{0x1030,"TMC Research",0},
	{0x1031,"miro Computer Products AG",pci_device_1031},
	{0x1032,"Compaq",0},
	{0x1033,"NEC Corporation",pci_device_1033},
	{0x1034,"Burndy Corporation",0},
	{0x1035,"Computer&Communication Research Lab",0},
	{0x1036,"Future Domain",pci_device_1036},
	{0x1037,"Hitachi Micro Systems Inc",0},
	{0x1038,"AMP Incorporated",0},
	{0x1039,"Silicon Integrated System",pci_device_1039},
	{0x103a,"Seiko Epson Corporation",0},
	{0x103b,"Tatung Corp. Of America",0},
	{0x103c,"Hewlett-Packard Company",pci_device_103c},
	{0x103e,"Solliday Engineering",0},
	{0x103f,"Logic Modeling",0},
	{0x1040,"Kubota Pacific Computer Inc.",0},
	{0x1041,"Computrend",0},
	{0x1042,"PC Technology",pci_device_1042},
	{0x1043,"Asustek Computer Inc.",pci_device_1043},
	{0x1044,"Distributed Processing Tech",pci_device_1044},
	{0x1045,"OPTi Inc.",pci_device_1045},
	{0x1046,"IPC Corporation LTD",0},
	{0x1047,"Genoa Systems Corp.",0},
	{0x1048,"ELSA AG",pci_device_1048},
	{0x1049,"Fountain Technology",0},
	{0x104a,"ST Microelectronics",pci_device_104a},
	{0x104b,"Mylex Corporation",pci_device_104b},
	{0x104c,"Texas Instruments",pci_device_104c},
	{0x104d,"Sony Corporation",pci_device_104d},
	{0x104e,"Oak Technology",pci_device_104e},
	{0x104f,"Co-Time Computer Ltd.",0},
	{0x1050,"Winbond Electronics Corp.",pci_device_1050},
	{0x1051,"Anigma Corp.",0},
	{0x1052,"Young Micro Systems",0},
	{0x1054,"Hitachi LTD",pci_device_1054},
	{0x1055,"EFAR Microsystems",pci_device_1055},
	{0x1056,"ICL",0},
	{0x1057,"Motorola",pci_device_1057},
	{0x1058,"Electronics & Telecommunication Res",0},
	{0x1059,"Teknor Microsystems",0},
	{0x105a,"Promise Technology",pci_device_105a},
	{0x105b,"Foxconn International",0},
	{0x105c,"Wipro Infotech Limited",0},
	{0x105d,"Number Nine Visual Technology",pci_device_105d},
	{0x105e,"Vtech Engineering Canada Ltd.",0},
	{0x105f,"Infotronic America Inc.",0},
	{0x1060,"United Microelectronics",pci_device_1060},
	{0x1061,"8x8 Inc.",pci_device_1061},
	{0x1062,"Maspar Computer Corp.",0},
	{0x1063,"Ocean Office Automation",0},
	{0x1064,"Alcatel Cit",0},
	{0x1065,"Texas Microsystems",0},
	{0x1066,"Picopower Technology",pci_device_1066},
	{0x1067,"Mitsubishi Electronics",pci_device_1067},
	{0x1068,"Diversified Technology",0},
	{0x1069,"Mylex Corporation",pci_device_1069},
	{0x106a,"Aten Research Inc.",0},
	{0x106b,"Apple Computer Inc.",pci_device_106b},
	{0x106c,"Hyundai Electronics America",pci_device_106c},
	{0x106d,"Sequent",0},
	{0x106e,"DFI Inc.",0},
	{0x106f,"City Gate Development LTD",0},
	{0x1070,"Daewoo Telecom Ltd.",0},
	{0x1071,"Mitac",0},
	{0x1072,"GIT Co. Ltd.",0},
	{0x1073,"Yamaha Corporation",pci_device_1073},
	{0x1074,"Nexgen Microsysteme",pci_device_1074},
	{0x1075,"Advanced Integration Research",0},
	{0x1076,"Chaintech Computer Co. Ltd.",0},
	{0x1077,"Q Logic",pci_device_1077},
	{0x1078,"Cyrix Corporation",pci_device_1078},
	{0x1079,"I-Bus",0},
	{0x107a,"Networth",0},
	{0x107b,"Gateway 2000",0},
	{0x107c,"Goldstar Co. Ltd.",0},
	{0x107d,"Leadtek Research",pci_device_107d},
	{0x107e,"Interphase Corporation",pci_device_107e},
	{0x107f,"Data Technology Corporation",pci_device_107f},
	{0x1080,"Contaq Microsystems",pci_device_1080},
	{0x1081,"Supermac Technology Inc.",pci_device_1081},
	{0x1082,"EFA Corporation Of America",0},
	{0x1083,"Forex Computer Corporation",pci_device_1083},
	{0x1084,"Parador",0},
	{0x1085,"Tulip Computers Int'l BV",0},
	{0x1086,"J. Bond Computer Systems",0},
	{0x1087,"Cache Computer",0},
	{0x1088,"Microcomputer Systems (M) Son",0},
	{0x1089,"Data General Corporation",0},
	{0x108a,"Bit3 Computer",pci_device_108a},
	{0x108c,"Elonex PLC c/o Oakleigh Systems Inc.",0},
	{0x108d,"Olicom",pci_device_108d},
	{0x108e,"Sun Microsystems",pci_device_108e},
	{0x108f,"Systemsoft Corporation",0},
	{0x1090,"Encore Computer Corporation",0},
	{0x1091,"Intergraph Corporation",pci_device_1091},
	{0x1092,"Diamond Computer Systems",pci_device_1092},
	{0x1093,"National Instruments",pci_device_1093},
	{0x1094,"First Int'l Computers",0},
	{0x1095,"CMD Technology Inc.",pci_device_1095},
	{0x1096,"Alacron",0},
	{0x1097,"Appian Graphics",pci_device_1097},
	{0x1098,"Quantum Designs Ltd.",pci_device_1098},
	{0x1099,"Samsung Electronics Co. Ltd.",0},
	{0x109a,"Packard Bell",0},
	{0x109b,"Gemlight Computer Ltd.",0},
	{0x109c,"Megachips Corporation",0},
	{0x109d,"Zida Technologies Ltd.",0},
	{0x109e,"Brooktree Corporation",pci_device_109e},
	{0x109f,"Trigem Computer Inc.",0},
	{0x10a0,"Meidensha Corporation",0},
	{0x10a1,"Juko Electronics Inc. Ltd.",0},
	{0x10a2,"Quantum Corporation",0},
	{0x10a3,"Everex Systems Inc.",0},
	{0x10a4,"Globe Manufacturing Sales",0},
	{0x10a5,"Racal Interlan",0},
	{0x10a6,"Informtech Industrial Ltd.",0},
	{0x10a7,"Benchmarq Microelectronics",0},
	{0x10a8,"Sierra Semiconductor",pci_device_10a8},
	{0x10a9,"Silicon Graphics",pci_device_10a9},
	{0x10aa,"ACC Microelectronics",pci_device_10aa},
	{0x10ab,"Digicom",0},
	{0x10ac,"Honeywell IASD",0},
	{0x10ad,"Symphony Labs",pci_device_10ad},
	{0x10ae,"Cornerstone Technology",0},
	{0x10af,"Micro Computer Systems Inc.",0},
	{0x10b0,"Cardexpert Technology",0},
	{0x10b1,"Cabletron Systems Inc.",0},
	{0x10b2,"Raytheon Company",0},
	{0x10b3,"Databook Inc.",0},
	{0x10b4,"STB Systems",0},
	{0x10b5,"PLX Technology",pci_device_10b5},
	{0x10b6,"Madge Networks",pci_device_10b6},
	{0x10b7,"3Com Corporation",pci_device_10b7},
	{0x10b8,"Standard Microsystems Corporation",pci_device_10b8},
	{0x10b9,"Acer Labs Inc.",pci_device_10b9},
	{0x10ba,"Mitsubishi Electronics Corp.",0},
	{0x10bb,"Dapha Electronics Corporation",0},
	{0x10bc,"Advanced Logic Research Inc.",0},
	{0x10bd,"Surecom Technology",pci_device_10bd},
	{0x10be,"Tsenglabs International Corp.",0},
	{0x10bf,"MOST Corp.",0},
	{0x10c0,"Boca Research Inc.",0},
	{0x10c1,"ICM Corp. Ltd.",0},
	{0x10c2,"Auspex Systems Inc.",0},
	{0x10c3,"Samsung Semiconductors",0},
	{0x10c4,"Award Software Int'l Inc.",0},
	{0x10c5,"Xerox Corporation",0},
	{0x10c6,"Rambus Inc.",0},
	{0x10c7,"Media Vision",0},
	{0x10c8,"Neomagic Corporation",pci_device_10c8},
	{0x10c9,"Dataexpert Corporation",0},
	{0x10ca,"Fujitsu",0},
	{0x10cb,"Omron Corporation",0},
	{0x10cc,"Mentor Arc Inc.",0},
	{0x10cd,"Advanced System Products",pci_device_10cd},
	{0x10ce,"Radius Inc.",0},
	{0x10cf,"Citicorp TTI",pci_device_10cf},
	{0x10d0,"Fujitsu Limited",0},
	{0x10d1,"Future+ Systems",0},
	{0x10d2,"Molex Incorporated",0},
	{0x10d3,"Jabil Circuit Inc.",0},
	{0x10d4,"Hualon Microelectronics",0},
	{0x10d5,"Autologic Inc.",0},
	{0x10d6,"Cetia",0},
	{0x10d7,"BCM Advanced Research",0},
	{0x10d8,"Advanced Peripherals Labs",0},
	{0x10d9,"Macronix International Co. Ltd.",pci_device_10d9},
	{0x10da,"Thomas-Conrad Corporation",pci_device_10da},
	{0x10db,"Rohm Research",0},
	{0x10dc,"CERN-European Lab. for Particle Physics",pci_device_10dc},
	{0x10dd,"Evans & Sutherland",pci_device_10dd},
	{0x10de,"Nvidia Corporation",pci_device_10de},
	{0x10df,"Emulex Corporation",pci_device_10df},
	{0x10e0,"Integrated Micro Solutions",pci_device_10e0},
	{0x10e1,"Tekram Technology Corp. Ltd.",pci_device_10e1},
	{0x10e2,"Aptix Corporation",0},
	{0x10e3,"Tundra Semiconductor Corp.",pci_device_10e3},
	{0x10e4,"Tandem Computers",0},
	{0x10e5,"Micro Industries Corporation",0},
	{0x10e6,"Gainbery Computer Products Inc.",0},
	{0x10e7,"Vadem",0},
	{0x10e8,"Applied Micro Circuits Corp.",pci_device_10e8},
	{0x10e9,"Alps Electronic Corp. Ltd.",0},
	{0x10ea,"Integraphics Systems",pci_device_10ea},
	{0x10eb,"Artist Graphics",pci_device_10eb},
	{0x10ec,"Realtek Semiconductor",pci_device_10ec},
	{0x10ed,"Ascii Corporation",pci_device_10ed},
	{0x10ee,"Xilinx Corporation",pci_device_10ee},
	{0x10ef,"Racore Computer Products",pci_device_10ef},
	{0x10f0,"Peritek Corporation",pci_device_10f0},
	{0x10f1,"Tyan Computer",pci_device_10f1},
	{0x10f2,"Achme Computer Inc.",0},
	{0x10f3,"Alaris Inc.",0},
	{0x10f4,"S-Mos Systems",0},
	{0x10f5,"NKK Corporation",pci_device_10f5},
	{0x10f6,"Creative Electronic Systems SA",0},
	{0x10f7,"Matsushita Electric Industrial Corp.",0},
	{0x10f8,"Altos India Ltd.",0},
	{0x10f9,"PC Direct",0},
	{0x10fa,"Truevision",pci_device_10fa},
	{0x10fb,"Thesys Microelectronic's",0},
	{0x10fc,"I-O Data Device Inc.",0},
	{0x10fd,"Soyo Technology Corp. Ltd.",0},
	{0x10fe,"Fast Electronic GmbH",0},
	{0x10ff,"Ncube",0},
	{0x1100,"Jazz Multimedia",0},
	{0x1101,"Initio Corporation",pci_device_1101},
	{0x1102,"Creative Labs",pci_device_1102},
	{0x1103,"HighPoint Technologies Inc.",pci_device_1103},
	{0x1104,"Rasterops",0},
	{0x1105,"Sigma Designs Inc.",pci_device_1105},
	{0x1106,"VIA Technologies Inc",pci_device_1106},
	{0x1107,"Stratus Computer",pci_device_1107},
	{0x1108,"Proteon Inc.",pci_device_1108},
	{0x1109,"Cogent Data Technologies",pci_device_1109},
	{0x110a,"Infineon Technologies",pci_device_110a},
	{0x110b,"Chromatic Research Inc",pci_device_110b},
	{0x110c,"Mini-Max Technology Inc.",0},
	{0x110d,"ZNYX Corporation",0},
	{0x110e,"CPU Technology",0},
	{0x110f,"Ross Technology",0},
	{0x1110,"Powerhouse Systems",pci_device_1110},
	{0x1111,"Santa Cruz Operation",0},
	{0x1112,"Osicom Technologies Inc.",pci_device_1112},
	{0x1113,"Accton Technology Corporation",pci_device_1113},
	{0x1114,"Atmel Corp.",0},
	{0x1115,"Dupont Pixel Systems Ltd.",0},
	{0x1116,"Data Translation",pci_device_1116},
	{0x1117,"Datacube Inc.",pci_device_1117},
	{0x1118,"Berg Electronics",0},
	{0x1119,"Vortex Computersysteme GmbH",pci_device_1119},
	{0x111a,"Efficent Networks",pci_device_111a},
	{0x111b,"Teledyne Electronic Systems",0},
	{0x111c,"Tricord Systems Inc.",pci_device_111c},
	{0x111d,"Integrated Device Technology Inc.",pci_device_111d},
	{0x111e,"Eldec Corp.",0},
	{0x111f,"Precision Digital Images",pci_device_111f},
	{0x1120,"EMC Corp.",0},
	{0x1121,"Zilog",0},
	{0x1122,"Multi-Tech Systems Inc.",0},
	{0x1123,"Excellent Design Inc.",0},
	{0x1124,"Leutron Vision AG",0},
	{0x1125,"Eurocore/Vigra",0},
	{0x1126,"Vigra",0},
	{0x1127,"FORE Systems",pci_device_1127},
	{0x1129,"Firmworks",0},
	{0x112a,"Hermes Electronics Co. Ltd.",0},
	{0x112b,"Linotype - Hell AG",0},
	{0x112c,"Zenith Data Systems",0},
	{0x112d,"Ravicad",0},
	{0x112e,"Infomedia",pci_device_112e},
	{0x112f,"Imaging Technology",pci_device_112f},
	{0x1130,"Computervision",0},
	{0x1131,"Philips Semiconductors",pci_device_1131},
	{0x1132,"Mitel Corp.",0},
	{0x1133,"Eicon Technology Corporation",pci_device_1133},
	{0x1134,"Mercury Computer Systems Inc.",pci_device_1134},
	{0x1135,"Fuji Xerox Co Ltd",pci_device_1135},
	{0x1136,"Momentum Data Systems",0},
	{0x1137,"Cisco Systems Inc",0},
	{0x1138,"Ziatech Corporation",pci_device_1138},
	{0x1139,"Dynamic Pictures Inc",0},
	{0x113a,"FWB  Inc",0},
	{0x113b,"Network Computing Devices",0},
	{0x113c,"Cyclone Microsystems",pci_device_113c},
	{0x113d,"Leading Edge Products Inc",0},
	{0x113e,"Sanyo Electric Co",0},
	{0x113f,"Equinox Systems",pci_device_113f},
	{0x1140,"Intervoice Inc",0},
	{0x1141,"Crest Microsystem Inc",pci_device_1141},
	{0x1142,"Alliance Semiconductor CA - USA",pci_device_1142},
	{0x1143,"Netpower Inc",0},
	{0x1144,"Cincinnati Milacron",pci_device_1144},
	{0x1145,"Workbit Corp",0},
	{0x1146,"Force Computers",0},
	{0x1147,"Interface Corp",0},
	{0x1148,"Schneider & Koch",pci_device_1148},
	{0x1149,"Win System Corporation",0},
	{0x114a,"VMIC",pci_device_114a},
	{0x114b,"Canopus Co. Ltd",0},
	{0x114c,"Annabooks",0},
	{0x114d,"IC Corporation",0},
	{0x114e,"Nikon Systems Inc",0},
	{0x114f,"Digi International",pci_device_114f},
	{0x1150,"Thinking Machines Corporation",0},
	{0x1151,"JAE Electronics Inc.",0},
	{0x1152,"Megatek",0},
	{0x1153,"Land Win Electronic Corp",0},
	{0x1154,"Melco Inc",0},
	{0x1155,"Pine Technology Ltd",pci_device_1155},
	{0x1156,"Periscope Engineering",0},
	{0x1157,"Avsys Corporation",0},
	{0x1158,"Voarx R&D Inc",pci_device_1158},
	{0x1159,"Mutech",pci_device_1159},
	{0x115a,"Harlequin Ltd",0},
	{0x115b,"Parallax Graphics",0},
	{0x115c,"Photron Ltd.",0},
	{0x115d,"Xircom",pci_device_115d},
	{0x115e,"Peer Protocols Inc",0},
	{0x115f,"Maxtor Corporation",0},
	{0x1160,"Megasoft Inc",0},
	{0x1161,"PFU Ltd",pci_device_1161},
	{0x1162,"OA Laboratory Co Ltd",0},
	{0x1163,"Rendition Inc",pci_device_1163},
	{0x1164,"Advanced Peripherals Tech",0},
	{0x1165,"Imagraph Corporation",pci_device_1165},
	{0x1166,"Pequr Technology / Ross Computer Corp",pci_device_1166},
	{0x1167,"Mutoh Industries Inc",0},
	{0x1168,"Thine Electronics Inc",0},
	{0x1169,"Centre f/Dev. of Adv. Computing",0},
	{0x116a,"Polaris Communications",pci_device_116a},
	{0x116b,"Connectware Inc",0},
	{0x116c,"Intelligent Resources",0},
	{0x116e,"Electronics for Imaging",0},
	{0x116f,"Workstation Technology",0},
	{0x1170,"Inventec Corporation",0},
	{0x1171,"Loughborough Sound Images",0},
	{0x1172,"Altera Corporation",0},
	{0x1173,"Adobe Systems",0},
	{0x1174,"Bridgeport Machines",0},
	{0x1175,"Mitron Computer Inc.",0},
	{0x1176,"SBE",0},
	{0x1177,"Silicon Engineering",0},
	{0x1178,"Alfa Inc",pci_device_1178},
	{0x1179,"Toshiba America Info Systems",pci_device_1179},
	{0x117a,"A-Trend Technology",0},
	{0x117b,"LG Electronics Inc.",0},
	{0x117c,"Atto Technology",0},
	{0x117d,"Becton & Dickinson",0},
	{0x117e,"T/R Systems",pci_device_117e},
	{0x117f,"Integrated Circuit Systems",0},
	{0x1180,"Ricoh Co Ltd",pci_device_1180},
	{0x1181,"Telmatics International",0},
	{0x1183,"Fujikura Ltd",0},
	{0x1184,"Forks Inc",0},
	{0x1185,"Dataworld",pci_device_1185},
	{0x1186,"D-Link System Inc",pci_device_1186},
	{0x1187,"Advanced Technology Laboratories",0},
	{0x1188,"Shima Seiki Manufacturing Ltd.",0},
	{0x1189,"Matsushita Electronics",pci_device_1189},
	{0x118a,"Hilevel Technology",0},
	{0x118b,"Hypertec Pty Ltd",0},
	{0x118c,"Corollary Inc",pci_device_118c},
	{0x118d,"BitFlow Inc",pci_device_118d},
	{0x118e,"Hermstedt GmbH",0},
	{0x118f,"Green Logic",0},
	{0x1190,"Tripace",pci_device_1190},
	{0x1191,"ACARD Technology",pci_device_1191},
	{0x1192,"Densan Co. Ltd",0},
	{0x1193,"Zeitnet Inc.",pci_device_1193},
	{0x1194,"Toucan Technology",0},
	{0x1195,"Ratoc System Inc",0},
	{0x1196,"Hytec Electronics Ltd",0},
	{0x1197,"Gage Applied Sciences Inc.",0},
	{0x1198,"Lambda Systems Inc",0},
	{0x1199,"Attachmate Corp.",pci_device_1199},
	{0x119a,"Mind Share Inc.",0},
	{0x119b,"Omega Micro Inc.",pci_device_119b},
	{0x119c,"Information Technology Inst.",0},
	{0x119d,"Bug Sapporo Japan",0},
	{0x119e,"Fujitsu",0},
	{0x119f,"Bull Hn Information Systems",0},
	{0x11a0,"Convex Computer Corporation",0},
	{0x11a1,"Hamamatsu Photonics K.K.",0},
	{0x11a2,"Sierra Research and Technology",0},
	{0x11a3,"Deuretzbacher GmbH & Co. Eng. KG",0},
	{0x11a4,"Barco",0},
	{0x11a5,"MicroUnity Systems Engineering Inc.",0},
	{0x11a6,"Pure Data",0},
	{0x11a7,"Power Computing Corp.",0},
	{0x11a8,"Systech Corp.",0},
	{0x11a9,"InnoSys Inc.",pci_device_11a9},
	{0x11aa,"Actel",0},
	{0x11ab,"Galileo Technology Ltd.",pci_device_11ab},
	{0x11ac,"Canon Information Systems",0},
	{0x11ad,"Lite-On Communications Inc",pci_device_11ad},
	{0x11ae,"Scitex Corporation Ltd",pci_device_11ae},
	{0x11af,"Avid Technology Inc.",0},
	{0x11b0,"V3 Semiconductor Inc.",pci_device_11b0},
	{0x11b1,"Apricot Computers",0},
	{0x11b2,"Eastman Kodak",0},
	{0x11b3,"Barr Systems Inc.",0},
	{0x11b4,"Leitch Technology International",0},
	{0x11b5,"Radstone Technology Plc",0},
	{0x11b6,"United Video Corp",0},
	{0x11b7,"Motorola",0},
	{0x11b8,"Xpoint Technologies Inc",pci_device_11b8},
	{0x11b9,"Pathlight Technology Inc.",pci_device_11b9},
	{0x11ba,"Videotron Corp",0},
	{0x11bb,"Pyramid Technology",0},
	{0x11bc,"Network Peripherals Inc",pci_device_11bc},
	{0x11bd,"Pinnacle Systems Inc.",0},
	{0x11be,"International Microcircuits Inc",0},
	{0x11bf,"Astrodesign Inc.",0},
	{0x11c0,"Hewlett-Packard",0},
	{0x11c1,"AT&T Microelectronics",pci_device_11c1},
	{0x11c2,"Sand Microelectronics",0},
	{0x11c3,"NEC Corporation",0},
	{0x11c4,"Document Technologies Ind.",0},
	{0x11c5,"Shiva Corporatin",0},
	{0x11c6,"Dainippon Screen Mfg. Co",0},
	{0x11c7,"D.C.M. Data Systems",0},
	{0x11c8,"Dolphin Interconnect Solutions",pci_device_11c8},
	{0x11c9,"MAGMA",pci_device_11c9},
	{0x11ca,"LSI Systems Inc",0},
	{0x11cb,"Specialix International Ltd.",pci_device_11cb},
	{0x11cc,"Michels & Kleberhoff Computer GmbH",0},
	{0x11cd,"HAL Computer Systems Inc.",0},
	{0x11ce,"Primary Rate Inc",0},
	{0x11cf,"Pioneer Electronic Corporation",0},
	{0x11d0,"Loral Frederal Systems - Manassas",0},
	{0x11d1,"AuraVision Corporation",pci_device_11d1},
	{0x11d2,"Intercom Inc.",0},
	{0x11d3,"Trancell Systems Inc",0},
	{0x11d4,"Analog Devices",pci_device_11d4},
	{0x11d5,"Ikon Corp",pci_device_11d5},
	{0x11d6,"Tekelec Technologies",0},
	{0x11d7,"Trenton Terminals Inc",0},
	{0x11d8,"Image Technologies Development",0},
	{0x11d9,"Tec Corporation",0},
	{0x11da,"Novell",0},
	{0x11db,"Sega Enterprises Ltd",0},
	{0x11dc,"Questra Corp",0},
	{0x11dd,"Crosfield Electronics Ltd",0},
	{0x11de,"Zoran Corporation",pci_device_11de},
	{0x11df,"New Wave Pdg",0},
	{0x11e0,"Cray Communications A/S",0},
	{0x11e1,"Gec Plessey Semi Inc",0},
	{0x11e2,"Samsung Information Systems America",0},
	{0x11e3,"Quicklogic Corp",0},
	{0x11e4,"Second Wave Inc",0},
	{0x11e5,"IIX Consulting",0},
	{0x11e6,"Mitsui-Zosen System Research",0},
	{0x11e7,"Toshiba America Elec. Co",0},
	{0x11e8,"Digital Processing Systems Inc",0},
	{0x11e9,"Highwater Designs Ltd",0},
	{0x11ea,"Elsag Bailey",0},
	{0x11eb,"Formation Inc",0},
	{0x11ec,"Coreco Inc",0},
	{0x11ed,"Mediamatics",0},
	{0x11ee,"Dome Imaging Systems Inc",0},
	{0x11ef,"Nicolet Technologies BV",0},
	{0x11f0,"Compu-Shack GmbH",pci_device_11f0},
	{0x11f1,"Symbios Logic Inc",0},
	{0x11f2,"Picture Tel Japan KK",0},
	{0x11f3,"Keithley Metrabyte",0},
	{0x11f4,"Kinetic Systems Corporation",pci_device_11f4},
	{0x11f5,"Computing Devices Intl",0},
	{0x11f6,"Powermatic Data Systems Ltd",pci_device_11f6},
	{0x11f7,"Scientific Atlanta",0},
	{0x11f8,"PMC-Sierra Inc.",pci_device_11f8},
	{0x11f9,"I-Cube Inc",0},
	{0x11fa,"Kasan Electronics Co Ltd",0},
	{0x11fb,"Datel Inc",0},
	{0x11fc,"Silicon Magic",0},
	{0x11fd,"High Street Consultants",0},
	{0x11fe,"Comtrol Corp",pci_device_11fe},
	{0x11ff,"Scion Corp",0},
	{0x1200,"CSS Corp",0},
	{0x1201,"Vista Controls Corp",0},
	{0x1202,"Network General Corp",0},
	{0x1203,"Bayer Corporation Agfa Div",0},
	{0x1204,"Lattice Semiconductor Corp",0},
	{0x1205,"Array Corp",0},
	{0x1206,"Amdahl Corp",0},
	{0x1208,"Parsytec GmbH",pci_device_1208},
	{0x1209,"Sci Systems Inc",0},
	{0x120a,"Synaptel",0},
	{0x120b,"Adaptive Solutions",0},
	{0x120d,"Compression Labs Inc.",0},
	{0x120e,"Cyclades Corporation",pci_device_120e},
	{0x120f,"Essential Communications",pci_device_120f},
	{0x1210,"Hyperparallel Technologies",0},
	{0x1211,"Braintech Inc",0},
	{0x1212,"Kingston Technology Corp",0},
	{0x1213,"Applied Intelligent Systems Inc",0},
	{0x1214,"Performance Technologies Inc",0},
	{0x1215,"Interware Co Ltd",0},
	{0x1216,"Purup-Eskofot A/S",0},
	{0x1217,"O2Micro Inc",pci_device_1217},
	{0x1218,"Hybricon Corp",0},
	{0x1219,"First Virtual Corp",0},
	{0x121a,"3dfx Interactive Inc",pci_device_121a},
	{0x121b,"Advanced Telecommunications Modules",0},
	{0x121c,"Nippon Texa Co Ltd",0},
	{0x121d,"Lippert Automationstechnik GmbH",0},
	{0x121e,"CSPI",0},
	{0x121f,"Arcus Technology Inc",0},
	{0x1220,"Ariel Corporation",pci_device_1220},
	{0x1221,"Contec Co Ltd",0},
	{0x1222,"Ancor Communications Inc",0},
	{0x1223,"Heurikon/Computer Products",0},
	{0x1224,"Interactive Images",0},
	{0x1225,"Power I/O Inc.",0},
	{0x1227,"Tech-Source",0},
	{0x1228,"Norsk Elektro Optikk A/S",0},
	{0x1229,"Data Kinesis Inc.",0},
	{0x122a,"Integrated Telecom",0},
	{0x122b,"LG Industrial Systems Co. Ltd.",0},
	{0x122c,"Sican GmbH",0},
	{0x122d,"Aztech System Ltd",pci_device_122d},
	{0x122e,"Xyratex",0},
	{0x122f,"Andrew Corp.",0},
	{0x1230,"Fishcamp Engineering",0},
	{0x1231,"Woodward McCoach Inc.",0},
	{0x1232,"GPT Ltd.",0},
	{0x1233,"Bus-Tech Inc.",0},
	{0x1234,"Technical Corp",0},
	{0x1235,"Risq Modular Systems Inc.",0},
	{0x1236,"Sigma Designs Corp.",pci_device_1236},
	{0x1237,"Alta Technology Corp.",0},
	{0x1238,"Adtran",0},
	{0x1239,"The 3DO Company",0},
	{0x123a,"Visicom Laboratories Inc.",0},
	{0x123b,"Seeq Technology Inc.",0},
	{0x123c,"Century Systems Inc.",0},
	{0x123d,"Engineering Design Team Inc.",pci_device_123d},
	{0x123f,"C-Cube Microsystems",pci_device_123f},
	{0x1240,"Marathon Technologies Corp.",0},
	{0x1241,"DSC Communications",0},
	{0x1242,"Jaycor Network Inc.",pci_device_1242},
	{0x1243,"Delphax",0},
	{0x1244,"AVM AUDIOVISUELLES MKTG & Computer GmbH",pci_device_1244},
	{0x1245,"APD S.A.",0},
	{0x1246,"Dipix Technologies Inc",0},
	{0x1247,"Xylon Research Inc.",0},
	{0x1248,"Central Data Corp.",0},
	{0x1249,"Samsung Electronics Co. Ltd.",0},
	{0x124a,"AEG Electrocom GmbH",0},
	{0x124b,"GreenSpring Computers",0},
	{0x124c,"Solitron Technologies Inc.",0},
	{0x124d,"Stallion Technologies",pci_device_124d},
	{0x124e,"Cylink",0},
	{0x124f,"Infortrend Technology Inc",pci_device_124f},
	{0x1250,"Hitachi Microcomputer System Ltd.",0},
	{0x1251,"VLSI Solution OY",0},
	{0x1253,"Guzik Technical Enterprises",0},
	{0x1254,"Linear Systems Ltd.",0},
	{0x1255,"Optibase Ltd.",pci_device_1255},
	{0x1256,"Perceptive Solutions Inc.",pci_device_1256},
	{0x1257,"Vertex Networks Inc.",0},
	{0x1258,"Gilbarco Inc.",0},
	{0x1259,"Allied Telesyn International",pci_device_1259},
	{0x125a,"ABB Power Systems",0},
	{0x125b,"Asix Electronics Corp.",0},
	{0x125c,"Aurora Technologies Inc.",0},
	{0x125d,"ESS Technology",pci_device_125d},
	{0x125e,"Specialvideo Engineering SRL",0},
	{0x125f,"Concurrent Technologies Inc.",0},
	{0x1260,"Harris Semiconductor",pci_device_1260},
	{0x1261,"Matsushita-Kotobuki Electronics Indu",0},
	{0x1262,"ES Computer Co. Ltd.",0},
	{0x1263,"Sonic Solutions",0},
	{0x1264,"Aval Nagasaki Corp.",0},
	{0x1265,"Casio Computer Co. Ltd.",0},
	{0x1266,"Microdyne Corp.",pci_device_1266},
	{0x1267,"S.A. Telecommunications",pci_device_1267},
	{0x1268,"Tektronix",0},
	{0x1269,"Thomson-CSF/TTM",0},
	{0x126a,"Lexmark International Inc.",0},
	{0x126b,"Adax Inc.",0},
	{0x126c,"Northern Telecom",0},
	{0x126d,"Splash Technology Inc.",0},
	{0x126e,"Sumitomo Metal Industries Ltd.",0},
	{0x126f,"Silicon Motion",pci_device_126f},
	{0x1270,"Olympus Optical Co. Ltd.",0},
	{0x1271,"GW Instruments",0},
	{0x1272,"Telematics International",0},
	{0x1273,"Hughes Network Systems",pci_device_1273},
	{0x1274,"Ensoniq",pci_device_1274},
	{0x1275,"Network Appliance",0},
	{0x1276,"Switched Network Technologies Inc.",0},
	{0x1277,"Comstream",0},
	{0x1278,"Transtech Parallel Systems",0},
	{0x1279,"Transmeta Corp.",pci_device_1279},
	{0x127a,"Conexant Systems",pci_device_127a},
	{0x127b,"Pixera Corp",0},
	{0x127c,"Crosspoint Solutions Inc.",0},
	{0x127d,"Vela Research",0},
	{0x127e,"Winnov L.P.",0},
	{0x127f,"Fujifilm",0},
	{0x1280,"Photoscript Group Ltd.",0},
	{0x1281,"Yokogawa Electronic Corp.",0},
	{0x1282,"Davicom Semiconductor Inc.",pci_device_1282},
	{0x1283,"Integrated Technology Express Inc.",pci_device_1283},
	{0x1284,"Sahara Networks Inc.",0},
	{0x1285,"Platform Technologies Inc.",pci_device_1285},
	{0x1286,"Mazet GmbH",0},
	{0x1287,"LuxSonor Inc.",pci_device_1287},
	{0x1288,"Timestep Corp.",0},
	{0x1289,"AVC Technology Inc.",0},
	{0x128a,"Asante Technologies Inc.",0},
	{0x128b,"Transwitch Corp.",0},
	{0x128c,"Retix Corp.",0},
	{0x128d,"G2 Networks Inc.",pci_device_128d},
	{0x128e,"Samho Multi Tech Ltd.",pci_device_128e},
	{0x128f,"Tateno Dennou Inc.",0},
	{0x1290,"Sord Computer Corp.",0},
	{0x1291,"NCS Computer Italia",0},
	{0x1292,"Tritech Microelectronics Intl PTE",0},
	{0x1293,"Media Reality Technology",0},
	{0x1294,"Rhetorex Inc.",0},
	{0x1295,"Imagenation Corp.",0},
	{0x1296,"Kofax Image Products",0},
	{0x1297,"Holco Enterprise",0},
	{0x1298,"Spellcaster Telecommunications Inc.",0},
	{0x1299,"Knowledge Technology Laboratories",0},
	{0x129a,"VMETRO",0},
	{0x129b,"Image Access",0},
	{0x129d,"CompCore Multimedia Inc.",0},
	{0x129e,"Victor Co. of Japan Ltd.",0},
	{0x129f,"OEC Medical Systems Inc.",0},
	{0x12a0,"Allen Bradley Co.",0},
	{0x12a1,"Simpact Inc",0},
	{0x12a2,"NewGen Systems Corp.",0},
	{0x12a3,"Lucent Technologies",0},
	{0x12a4,"NTT Electronics Technology Co.",0},
	{0x12a5,"Vision Dynamics Ltd.",0},
	{0x12a6,"Scalable Networks Inc.",0},
	{0x12a7,"AMO GmbH",0},
	{0x12a8,"News Datacom",0},
	{0x12a9,"Xiotech Corp.",0},
	{0x12aa,"SDL Communications Inc.",0},
	{0x12ab,"Yuan Yuan Enterprise Co. Ltd.",pci_device_12ab},
	{0x12ac,"MeasureX Corp.",0},
	{0x12ad,"Multidata GmbH",0},
	{0x12ae,"Alteon Networks Inc.",pci_device_12ae},
	{0x12af,"TDK USA Corp.",0},
	{0x12b0,"Jorge Scientific Corp.",0},
	{0x12b1,"GammaLink",0},
	{0x12b2,"General Signal Networks",0},
	{0x12b3,"Inter-Face Co. Ltd.",0},
	{0x12b4,"Future Tel Inc.",0},
	{0x12b5,"Granite Systems Inc.",0},
	{0x12b6,"Natural Microsystems",0},
	{0x12b7,"Acumen",0},
	{0x12b8,"Korg",0},
	{0x12b9,"US Robotics",pci_device_12b9},
	{0x12ba,"Bittware Research Systems Inc",0},
	{0x12bb,"Nippon Unisoft Corp.",0},
	{0x12bc,"Array Microsystems",0},
	{0x12bd,"Computerm Corp.",0},
	{0x12be,"Anchor Chips Inc.",pci_device_12be},
	{0x12bf,"Fujifilm Microdevices",0},
	{0x12c0,"Infimed",0},
	{0x12c1,"GMM Research Corp.",0},
	{0x12c2,"Mentec Ltd.",0},
	{0x12c3,"Holtek Microelectronics Inc.",0},
	{0x12c4,"Connect Tech Inc.",0},
	{0x12c5,"Picture Elements Inc.",pci_device_12c5},
	{0x12c6,"Mitani Corp.",0},
	{0x12c7,"Dialogic Corp.",0},
	{0x12c8,"G Force Co. Ltd.",0},
	{0x12c9,"Gigi Operations",0},
	{0x12ca,"Integrated Computing Engines, Inc.",0},
	{0x12cb,"Antex Electronics Corp.",0},
	{0x12cc,"Pluto Technologies International",0},
	{0x12cd,"Aims Lab",0},
	{0x12ce,"Netspeed Inc.",0},
	{0x12cf,"Prophet Systems Inc.",0},
	{0x12d0,"GDE Systems Inc.",0},
	{0x12d1,"PsiTech",0},
	{0x12d2,"NVidia / SGS Thomson",pci_device_12d2},
	{0x12d3,"Vingmed Sound A/S",0},
	{0x12d4,"DGM & S",0},
	{0x12d5,"Equator Technologies",0},
	{0x12d6,"Analogic Corp.",0},
	{0x12d7,"Biotronic SRL",0},
	{0x12d8,"Pericom Semiconductor",0},
	{0x12d9,"Aculab Plc.",0},
	{0x12da,"TrueTime",0},
	{0x12db,"Annapolis Micro Systems Inc.",pci_device_12db},
	{0x12dc,"Symicron Computer Communication Ltd.",0},
	{0x12dd,"Management Graphics Inc.",0},
	{0x12de,"Rainbow Technologies",pci_device_12de},
	{0x12df,"SBS Technologies Inc.",0},
	{0x12e0,"Chase Research PLC",pci_device_12e0},
	{0x12e1,"Nintendo Co. Ltd.",0},
	{0x12e2,"Datum Inc. Bancomm-Timing Division",0},
	{0x12e3,"Imation Corp. - Medical Imaging Syst",0},
	{0x12e4,"Brooktrout Technology Inc.",pci_device_12e4},
	{0x12e6,"Cirel Systems",0},
	{0x12e7,"Sebring Systems Inc",0},
	{0x12e8,"CRISC Corp.",0},
	{0x12e9,"GE Spacenet",0},
	{0x12ea,"Zuken",0},
	{0x12eb,"Aureal Semiconductor",pci_device_12eb},
	{0x12ec,"3A International Inc.",0},
	{0x12ed,"Optivision Inc.",0},
	{0x12ee,"Orange Micro, Inc.",0},
	{0x12ef,"Vienna Systems",0},
	{0x12f0,"Pentek",0},
	{0x12f1,"Sorenson Vision Inc.",0},
	{0x12f2,"Gammagraphx Inc.",0},
	{0x12f4,"Megatel",0},
	{0x12f5,"Forks",0},
	{0x12f6,"Dawson France",0},
	{0x12f7,"Cognex",0},
	{0x12f8,"Electronic-Design GmbH",pci_device_12f8},
	{0x12f9,"FourFold Technologies",0},
	{0x12fb,"Spectrum Signal Processing",0},
	{0x12fc,"Capital Equipment Corp",0},
	{0x12fe,"ESD Electronic System Design GmbH",0},
	{0x1304,"Juniper Networks Inc.",0},
	{0x1307,"ComputerBoards",pci_device_1307},
	{0x1308,"Jato Technologies Inc.",pci_device_1308},
	{0x130a,"Mitsubishi Electric Microcomputer",0},
	{0x130b,"Colorgraphic Communications Corp",0},
	{0x130f,"Advanet Inc.",0},
	{0x1310,"Gespac",0},
	{0x1312,"Robotic Vision Systems Incorporated",0},
	{0x1313,"Yaskawa Electric Co.",0},
	{0x1316,"Teradyne Inc.",0},
	{0x1317,"Admtek Inc",0},
	{0x1318,"Packet Engines, Inc.",pci_device_1318},
	{0x1319,"Forte Media, Inc.",pci_device_1319},
	{0x131f,"SIIG",0},
	{0x1325,"Salix Technologies Inc",0},
	{0x1326,"Seachange International",0},
	{0x1331,"RadiSys Corporation",0},
	{0x1335,"Videomail Inc.",0},
	{0x133d,"Prisa Networks",0},
	{0x133f,"SCM Microsystems",0},
	{0x1342,"Promax Systems Inc",0},
	{0x1344,"Micron Technology Inc",0},
	{0x1347,"Odetics",0},
	{0x134a,"DTC Technology Corp.",pci_device_134a},
	{0x134b,"ARK Research Corp.",0},
	{0x134c,"Chori Joho System Co. Ltd",0},
	{0x134d,"PCTEL Inc.",pci_device_134d},
	{0x135a,"Brain Boxes Limited",0},
	{0x135b,"Giganet Inc.",0},
	{0x135c,"Quatech Inc",0},
	{0x135d,"ABB Network Partner AB",0},
	{0x135e,"Sealevel Systems Inc.",pci_device_135e},
	{0x135f,"I-Data International A-S",0},
	{0x1360,"Meinberg Funkuhren",0},
	{0x1361,"Soliton Systems K.K.",0},
	{0x1363,"Phoenix Technologies Ltd",0},
	{0x1367,"Hitachi Zosen Corporation",0},
	{0x1368,"Skyware Corporation",0},
	{0x1369,"Digigram",0},
	{0x136b,"Kawasaki Steel Corporation",0},
	{0x136c,"Adtek System Science Co Ltd",0},
	{0x1375,"Boeing - Sunnyvale",0},
	{0x1377,"GMBH",0},
	{0x137a,"Mark Of The Unicorn Inc",0},
	{0x137b,"PPT Vision",0},
	{0x137c,"Iwatsu Electric Co Ltd",0},
	{0x137d,"Dynachip Corporation",0},
	{0x137e,"Patriot Scientific Corp.",0},
	{0x1380,"Sanritz Automation Co LTC",0},
	{0x1381,"Brains Co. Ltd",0},
	{0x1384,"Stellar Semiconductor Inc",0},
	{0x1385,"Netgear",pci_device_1385},
	{0x1387,"Systran Corp",0},
	{0x1388,"Hitachi Information Technology Co Ltd",0},
	{0x1389,"Applicom International",pci_device_1389},
	{0x138b,"Tokimec Inc",0},
	{0x138e,"Basler GMBH",0},
	{0x138f,"Patapsco Designs Inc",0},
	{0x1390,"Concept Development Inc.",0},
	{0x1393,"Moxa Technologies Co Ltd",0},
	{0x1395,"Ambicom Inc",0},
	{0x1396,"Cipher Systems Inc",0},
	{0x1397,"Cologne Chip Designs GmbH",pci_device_1397},
	{0x1398,"Clarion Co. Ltd",0},
	{0x139a,"Alacritech Inc",0},
	{0x139d,"Xstreams PLC/ EPL Limited",0},
	{0x139e,"Echostar Data Networks",0},
	{0x13a0,"Crystal Group Inc",0},
	{0x13a1,"Kawasaki Heavy Industries Ltd",0},
	{0x13a4,"Rascom Inc",0},
	{0x13a7,"Teles AG",0},
	{0x13a8,"Exar Corp.",0},
	{0x13a9,"Siemens Medical Systems Ultrasound Group",0},
	{0x13aa,"Nortel Networks - BWA Division",0},
	{0x13af,"T.Sqware",0},
	{0x13b1,"Tamura Corporation",0},
	{0x13b4,"Wellbean Co Inc",0},
	{0x13b5,"ARM Ltd",0},
	{0x13b6,"DLoG GMBH",0},
	{0x13b8,"Nokia Telecommunications OY",0},
	{0x13bf,"Sharewave Inc",0},
	{0x13c0,"Microgate Corp.",pci_device_13c0},
	{0x13c1,"3ware Inc.",pci_device_13c1},
	{0x13c2,"Technotrend Systemtechnik GMBH",0},
	{0x13c3,"Janz Computer AG",0},
	{0x13c7,"Blue Chip Technology Ltd",0},
	{0x13cc,"Metheus Corporation",0},
	{0x13cf,"Studio Audio & Video Ltd",0},
	{0x13d0,"B2C2 Inc",0},
	{0x13d1,"Abocom Systems Inc",0},
	{0x13d4,"Graphics Microsystems Inc",0},
	{0x13d6,"K.I. Technology Co Ltd",0},
	{0x13d7,"Toshiba Engineering Corporation",0},
	{0x13d8,"Phobos Corporation",0},
	{0x13d9,"Apex Inc",0},
	{0x13dc,"Netboost Corporation",0},
	{0x13de,"ABB Robotics Products AB",0},
	{0x13df,"E-Tech Inc.",pci_device_13df},
	{0x13e0,"GVC Corporation",0},
	{0x13e3,"Nest Inc",0},
	{0x13e4,"Calculex Inc",0},
	{0x13e5,"Telesoft Design Ltd",0},
	{0x13e9,"Intraserver Technology Inc",0},
	{0x13ea,"Dallas Semiconductor",0},
	{0x13f0,"Sundance Technology Inc",0},
	{0x13f1,"OCE - Industries S.A.",0},
	{0x13f4,"Troika Networks Inc",0},
	{0x13f6,"C-Media Electronics Inc.",pci_device_13f6},
	{0x13f9,"NTT Advanced Technology Corp.",0},
	{0x13fa,"Pentland Systems Ltd.",0},
	{0x13fb,"Aydin Corp",0},
	{0x13fd,"Micro Science Inc",0},
	{0x1400,"ARTX Inc",0},
	{0x1402,"Meilhaus Electronic GmbH Germany",0},
	{0x1404,"Fundamental Software Inc",0},
	{0x1406,"Océ Printing Systems",0},
	{0x1407,"Lava Computer MFG Inc.",pci_device_1407},
	{0x1408,"Aloka Co. Ltd",0},
	{0x1409,"eTIMedia Technology Co Ltd",0},
	{0x140a,"DSP Research Inc",0},
	{0x140b,"Ramix Inc",0},
	{0x140d,"Matsushita Electric Works Ltd",0},
	{0x140f,"Salient Systems Corp",0},
	{0x1412,"IC Ensemble, Inc.",pci_device_1412},
	{0x1413,"Addonics",0},
	{0x1415,"Oxford Semiconductor Ltd",0},
	{0x1418,"Kyushu Electronics Systems Inc",0},
	{0x1419,"Excel Switching Corp",0},
	{0x141b,"Zoom Telephonics Inc",0},
	{0x141e,"Fanuc Co. Ltd",0},
	{0x141f,"Visiontech Ltd",0},
	{0x1420,"Psion Dacom PLC",0},
	{0x1425,"ASIC Designers Inc",0},
	{0x1428,"Edec Co Ltd",0},
	{0x1429,"Unex Technology Corp.",0},
	{0x142a,"Kingmax Technology Inc",0},
	{0x142b,"Radiolan",0},
	{0x142c,"Minton Optic Industry Co Ltd",0},
	{0x142d,"Pixstream Inc",0},
	{0x1430,"ITT Aerospace/Communications Division",0},
	{0x1433,"Eltec Elektronik AG",0},
	{0x1436,"CIS Technology Inc",0},
	{0x1437,"Nissin Inc Co",0},
	{0x1438,"Atmel-Dream",0},
	{0x143f,"Lightwell Co Ltd - Zax Division",0},
	{0x1441,"Agie SA.",0},
	{0x1445,"Logical Co Ltd",0},
	{0x1446,"Graphin Co. Ltd",0},
	{0x1447,"Aim GMBH",0},
	{0x1448,"Alesis Studio",pci_device_1448},
	{0x144a,"Adlink Technology",pci_device_144a},
	{0x144b,"Loronix Information Systems, Inc.",0},
	{0x144d,"Samsung Electronics Co Ltd",0},
	{0x1450,"Octave Communications Ind.",0},
	{0x1451,"SP3D Chip Design GMBH",0},
	{0x1453,"Mycom Inc",0},
	{0x1455,"Logic Plus PLUS Inc",0},
	{0x1458,"Giga-Byte Technologies",0},
	{0x145c,"Cryptek",0},
	{0x145f,"Baldor Electric Company",pci_device_145f},
	{0x1460,"Dynarc Inc",0},
	{0x1462,"Micro-Star International Co Ltd",0},
	{0x1463,"Fast Corporation",0},
	{0x1464,"Interactive Circuits & Systems Ltd",0},
	{0x1465,"GN Nettest Telecom Div.",0},
	{0x1468,"Ambit Microsystems Corp.",0},
	{0x1469,"Cleveland Motion Controls",0},
	{0x146c,"Ruby Tech Corp.",0},
	{0x146d,"Tachyon Inc.",0},
	{0x146e,"WMS Gaming",0},
	{0x1471,"Integrated Telecom Express Inc",0},
	{0x1473,"Zapex Technologies Inc",0},
	{0x1474,"Doug Carson & Associates",0},
	{0x1477,"Net Insight",0},
	{0x1478,"Diatrend Corporation",0},
	{0x147b,"Abit Computer Corp.",0},
	{0x147f,"Nihon Unisys Ltd.",0},
	{0x1482,"Isytec - Integrierte Systemtechnik Gmbh",0},
	{0x1483,"Labway Coporation",0},
	{0x1485,"Erma - Electronic GMBH",0},
	{0x1489,"KYE Systems Corporation",0},
	{0x148a,"Opto 22",0},
	{0x148b,"Innomedialogic Inc.",0},
	{0x148d,"Digicom Systems Inc.",pci_device_148d},
	{0x148e,"OSI Plus Corporation",0},
	{0x148f,"Plant Equipment Inc.",0},
	{0x1490,"TC Labs Pty Ltd.",0},
	{0x1493,"Maker Communications",0},
	{0x1495,"Tokai Communications Industry Co. Ltd",0},
	{0x1496,"Joytech Computer Co. Ltd.",0},
	{0x1497,"SMA Regelsysteme GMBH",0},
	{0x1499,"Micro-Technology Co Ltd",0},
	{0x149b,"Seiko Instruments Inc",0},
	{0x149e,"Mapletree Networks Inc.",0},
	{0x149f,"Lectron Co Ltd",0},
	{0x14a0,"Softing GMBH",0},
	{0x14a2,"Millennium Engineering Inc",0},
	{0x14a4,"GVC/BCM Advanced Research",0},
	{0x14a5,"Xionics Document Technologies Inc.",0},
	{0x14a9,"Hivertec Inc.",0},
	{0x14ab,"Mentor Graphics Corp.",0},
	{0x14b1,"Nextcom K.K.",0},
	{0x14b3,"Xpeed Inc.",pci_device_14b3},
	{0x14b4,"Philips Business Electronics B.V.",0},
	{0x14b6,"Quantum Data Corp.",0},
	{0x14b7,"Proxim Inc.",pci_device_14b7},
	{0x14b9,"Aironet Wireless Communication",pci_device_14b9},
	{0x14ba,"Internix Inc.",0},
	{0x14bb,"Semtech Corporation",0},
	{0x14be,"L3 Communications",0},
	{0x14c1,"Myricom Inc.",0},
	{0x14c2,"DTK Computer",0},
	{0x14c4,"Iwasaki Information Systems Co Ltd",0},
	{0x14c5,"ABB Automation Products AB",0},
	{0x14c6,"Data Race Inc",0},
	{0x14c9,"Odin Telesystems Inc",0},
	{0x14cb,"Billionton Systems Inc./Cadmus Micro Inc",0},
	{0x14cd,"Universal Scientific Ind.",0},
	{0x14cf,"TEK Microsystems Inc.",0},
	{0x14d2,"Oxford Semiconductor",0},
	{0x14d4,"Panacom Technology Corporation",pci_device_14d4},
	{0x14d5,"Nitsuko Corporation",0},
	{0x14d6,"Accusys Inc",0},
	{0x14d7,"Hirakawa Hewtech Corp",0},
	{0x14d8,"Hopf Elektronik GMBH",0},
	{0x14d9,"Alpha Processor Inc",0},
	{0x14db,"Avlab Technology Inc.",pci_device_14db},
	{0x14dc,"Amplicon Liveline Inc.",pci_device_14dc},
	{0x14dd,"Imodl Inc.",0},
	{0x14de,"Applied Integration Corporation",0},
	{0x14e3,"Amtelco",0},
	{0x14e4,"Broadcom Corporation",0},
	{0x14eb,"Seiko Epson Corporation",0},
	{0x14ec,"Acqiris",0},
	{0x14ed,"Datakinetics Ltd",0},
	{0x14ef,"Carry Computer Eng. Co Ltd",0},
	{0x14f1,"Conexant Systems, Inc.",0},
	{0x14f2,"Mobility Electronics, Inc.",pci_device_14f2},
	{0x14f4,"Tokyo Electronic Industry Co. Ltd.",0},
	{0x14f5,"Sopac Ltd",0},
	{0x14f6,"Coyote Technologies LLC",0},
	{0x14f7,"Wolf Technology Inc",0},
	{0x14f8,"Audiocodes Inc",0},
	{0x14f9,"AG Communications",0},
	{0x14fb,"Transas Marine (UK) Ltd",0},
	{0x14fc,"Quadrics Supercomputers World",0},
	{0x14fd,"Japan Computer Industry Inc.",0},
	{0x14fe,"Archtek Telecom Corp.",0},
	{0x14ff,"Twinhead International Corp.",0},
	{0x1500,"DELTA Electronics, Inc.",pci_device_1500},
	{0x1501,"Banksoft Canada Ltd",0},
	{0x1502,"Mitsubishi Electric Logistics Support Co",0},
	{0x1503,"Kawasaki LSI USA Inc",0},
	{0x1504,"Kaiser Electronics",0},
	{0x1506,"Chameleon Systems Inc",0},
	{0x1507,"Htec Ltd.",pci_device_1507},
	{0x1509,"First International Computer Inc",0},
	{0x150b,"Yamashita Systems Corp",0},
	{0x150c,"Kyopal Co Ltd",0},
	{0x150d,"Warpspped Inc",0},
	{0x150e,"C-Port Corporation",0},
	{0x150f,"Intec GMBH",0},
	{0x1510,"Behavior Tech Computer Corp",0},
	{0x1511,"Centillium Technology Corp",0},
	{0x1512,"Rosun Technologies Inc",0},
	{0x1513,"Raychem",0},
	{0x1514,"TFL LAN Inc",0},
	{0x1515,"ICS Advent",0},
	{0x1516,"Myson Technology Inc",0},
	{0x1517,"Echotek Corporation",0},
	{0x1518,"PEP Modular Computers GMBH",0},
	{0x1519,"Telefon Aktiebolaget LM Ericsson",0},
	{0x151a,"Globetek Inc.",pci_device_151a},
	{0x151b,"Combox Ltd",0},
	{0x151c,"Digital Audio Labs Inc",0},
	{0x151d,"Fujitsu Computer Products Of America",0},
	{0x151e,"Matrix Corp.",0},
	{0x151f,"Topic Semiconductor Corp",0},
	{0x1520,"Chaplet System Inc",0},
	{0x1521,"Bell Corporation",0},
	{0x1522,"Mainpine Limited",0},
	{0x1523,"Music Semiconductors",0},
	{0x1524,"ENE Technology Inc",0},
	{0x1525,"Impact Technologies",0},
	{0x1526,"ISS Inc",0},
	{0x1527,"Solectron",0},
	{0x1528,"Acksys",0},
	{0x1529,"American Microsystems Inc",0},
	{0x152a,"Quickturn Design Systems",0},
	{0x152b,"Flytech Technology Co Ltd",0},
	{0x152c,"Macraigor Systems LLC",0},
	{0x152d,"Quanta Computer Inc",0},
	{0x152e,"Melec Inc",0},
	{0x152f,"Philips - Crypto",0},
	{0x1532,"Echelon Corporation",0},
	{0x1533,"Baltimore",0},
	{0x1534,"Road Corporation",0},
	{0x1535,"Evergreen Technologies Inc",0},
	{0x1537,"Datalex Communcations",0},
	{0x1538,"Aralion Inc.",0},
	{0x1539,"Atelier Informatiques et Electronique Et",0},
	{0x153a,"ONO Sokki",0},
	{0x153b,"Terratec Electronic GMBH",0},
	{0x153c,"Antal Electronic",0},
	{0x153d,"Filanet Corporation",0},
	{0x153e,"Techwell Inc",0},
	{0x153f,"MIPS Denmark",0},
	{0x1540,"Provideo Multimedia Co Ltd",0},
	{0x1541,"Telocity Inc.",0},
	{0x1542,"Vivid Technology Inc",0},
	{0x1543,"Silicon Laboratories",0},
	{0x1544,"DCM Data Systems",0},
	{0x1545,"Visiontek",0},
	{0x1546,"IOI Technology Corp.",0},
	{0x1547,"Mitutoyo Corporation",0},
	{0x1548,"Jet Propulsion Laboratory",0},
	{0x1549,"Interconnect Systems Solutions",0},
	{0x154a,"Max Technologies Inc.",0},
	{0x154b,"Computex Co Ltd",0},
	{0x154c,"Visual Technology Inc.",0},
	{0x154d,"PAN International Industrial Corp",0},
	{0x154e,"Servotest Ltd",0},
	{0x154f,"Stratabeam Technology",0},
	{0x1550,"Open Network Co Ltd",0},
	{0x1551,"Smart Electronic Development GMBH",0},
	{0x1552,"Racal Airtech Ltd",0},
	{0x1553,"Chicony Electronics Co Ltd",0},
	{0x1554,"Prolink Microsystems Corp.",0},
	{0x1556,"PLD Applications",0},
	{0x1557,"Mediastar Co. Ltd",0},
	{0x1558,"Clevo/Kapok Computer",0},
	{0x1559,"SI Logic Ltd",0},
	{0x155a,"Innomedia Inc",0},
	{0x155b,"Protac International Corp",0},
	{0x155c,"Cemax-Icon Inc",0},
	{0x155d,"MAC System Co Ltd",0},
	{0x155e,"LP Elektronik GMBH",0},
	{0x155f,"Perle Systems Limited",0},
	{0x1560,"Terayon Communications Systems",0},
	{0x1561,"Viewgraphics Inc",0},
	{0x1562,"Symbol Technologies",0},
	{0x1563,"A-Trend Technology Co Ltd",0},
	{0x1564,"Yamakatsu Electronics Industry Co Ltd",0},
	{0x1565,"Biostar Microtech Intl Corp",0},
	{0x1566,"Ardent Technologies Inc",0},
	{0x1567,"Jungsoft",0},
	{0x1568,"DDK Electronics Inc",0},
	{0x1569,"Palit Microsystems Inc",0},
	{0x156a,"Avtec Systems",0},
	{0x156b,"2wire Inc",0},
	{0x156c,"Vidac Electronics GMBH",0},
	{0x156d,"Alpha-Top Corp",0},
	{0x156e,"Alfa Inc.",0},
	{0x156f,"M-Systems Flash Disk Pioneers Ltd",0},
	{0x1570,"Lecroy Corporation",0},
	{0x1571,"Contemporary Controls",0},
	{0x1572,"Otis Elevator Company",0},
	{0x1573,"Lattice - Vantis",0},
	{0x1574,"Fairchild Semiconductor",0},
	{0x1575,"Voltaire Advanced Data Security Ltd",0},
	{0x1576,"Viewcast Com",0},
	{0x1578,"Hitt",0},
	{0x1579,"Dual Technology Corporation",0},
	{0x157a,"Japan Elecronics Ind. Inc",0},
	{0x157b,"Star Multimedia Corp.",0},
	{0x157c,"Eurosoft (UK)",pci_device_157c},
	{0x157d,"Gemflex Networks",0},
	{0x157e,"Transition Networks",0},
	{0x157f,"PX Instruments Technology Ltd",0},
	{0x1580,"Primex Aerospace Co.",0},
	{0x1581,"SEH Computertechnik GMBH",0},
	{0x1582,"Cytec Corporation",0},
	{0x1583,"Inet Technologies Inc",0},
	{0x1584,"Uniwill Computer Corp.",0},
	{0x1585,"Marconi Commerce Systems SRL",0},
	{0x1586,"Lancast Inc",0},
	{0x1587,"Konica Corporation",0},
	{0x1588,"Solidum Systems Corp",0},
	{0x1589,"Atlantek Microsystems Pty Ltd",0},
	{0x158a,"Digalog Systems Inc",0},
	{0x158b,"Allied Data Technologies",0},
	{0x158c,"Hitachi Semiconductor & Devices Sales Co",0},
	{0x158d,"Point Multimedia Systems",0},
	{0x158e,"Lara Technology Inc",0},
	{0x158f,"Ditect Coop",0},
	{0x1590,"3pardata Inc.",0},
	{0x1591,"ARN",0},
	{0x1592,"Syba Tech Ltd.",pci_device_1592},
	{0x1593,"Bops Inc",0},
	{0x1594,"Netgame Ltd",0},
	{0x1595,"Diva Systems Corp.",0},
	{0x1596,"Folsom Research Inc",0},
	{0x1597,"Memec Design Services",0},
	{0x1598,"Granite Microsystems",0},
	{0x1599,"Delta Electronics Inc",0},
	{0x159a,"General Instrument",0},
	{0x159b,"Faraday Technology Corp",0},
	{0x159c,"Stratus Computer Systems",0},
	{0x159d,"Ningbo Harrison Electronics Co Ltd",0},
	{0x159e,"A-Max Technology Co Ltd",0},
	{0x159f,"Galea Network Security",0},
	{0x15a0,"Compumaster SRL",0},
	{0x15a1,"Geocast Network Systems Inc",0},
	{0x15a2,"Catalyst Enterprises Inc",0},
	{0x15a3,"Italtel",0},
	{0x15a4,"X-Net OY",0},
	{0x15a5,"Toyota MACS Inc",0},
	{0x15a6,"Sunlight Ultrasound Technologies Ltd",0},
	{0x15a7,"SSE Telecom Inc",0},
	{0x15a8,"Shanghai Communications Technologies Cen",0},
	{0x15aa,"Moreton Bay",0},
	{0x15ab,"Bluesteel Networks Inc",0},
	{0x15ac,"North Atlantic Instruments",0},
	{0x15ad,"VMware Inc.",pci_device_15ad},
	{0x15ae,"Amersham Pharmacia Biotech",0},
	{0x15b0,"Zoltrix International Limited",0},
	{0x15b1,"Source Technology Inc",0},
	{0x15b2,"Mosaid Technologies Inc.",0},
	{0x15b3,"Mellanox Technology",0},
	{0x15b4,"CCI/Triad",0},
	{0x15b5,"Cimetrics Inc",0},
	{0x15b6,"Texas Memory Systems Inc",0},
	{0x15b7,"Sandisk Corp.",0},
	{0x15b8,"Addi-Data GMBH",0},
	{0x15b9,"Maestro Digital Communications",0},
	{0x15ba,"Impacct Technology Corp",0},
	{0x15bb,"Portwell Inc",0},
	{0x15bc,"Agilent Technologies",0},
	{0x15bd,"DFI Inc.",0},
	{0x15be,"Sola Electronics",0},
	{0x15bf,"High Tech Computer Corp (HTC)",0},
	{0x15c0,"BVM Limited",0},
	{0x15c1,"Quantel",0},
	{0x15c2,"Newer Technology Inc",0},
	{0x15c3,"Taiwan Mycomp Co Ltd",0},
	{0x15c4,"EVSX Inc",0},
	{0x15c5,"Procomp Informatics Ltd",0},
	{0x15c6,"Technical University Of Budapest",0},
	{0x15c7,"Tateyama System Laboratory Co Ltd",0},
	{0x15c8,"Penta Media Co. Ltd",0},
	{0x15c9,"Serome Technology Inc",0},
	{0x15ca,"Bitboys OY",0},
	{0x15cb,"AG Electronics Ltd",0},
	{0x15cc,"Hotrail Inc.",0},
	{0x15cd,"Dreamtech Co Ltd",0},
	{0x15ce,"Genrad Inc.",0},
	{0x15cf,"Hilscher GMBH",0},
	{0x15d1,"Infineon Technologies AG",0},
	{0x15d2,"FIC (First International Computer Inc)",0},
	{0x15d3,"NDS Technologies Israel Ltd",0},
	{0x15d4,"Iwill Corporation",0},
	{0x15d5,"Tatung Co.",0},
	{0x15d6,"Entridia Corporation",0},
	{0x15d7,"Rockwell-Collins Inc",0},
	{0x15d8,"Cybernetics Technology Co Ltd",0},
	{0x15d9,"Super Micro Computer Inc",0},
	{0x15da,"Cyberfirm Inc.",0},
	{0x15db,"Applied Computing Systems Inc.",0},
	{0x15dc,"Litronic Inc.",pci_device_15dc},
	{0x15dd,"Sigmatel Inc.",0},
	{0x15de,"Malleable Technologies Inc",0},
	{0x15df,"Infinilink Corp.",0},
	{0x15e0,"Cacheflow Inc",0},
	{0x15e1,"Voice Technologies Group",0},
	{0x15e2,"Quicknet Technologies Inc",0},
	{0x15e3,"Networth Technologies Inc",0},
	{0x15e4,"VSN Systemen BV",0},
	{0x15e5,"Valley Technologies Inc",0},
	{0x15e6,"Agere Inc.",0},
	{0x15e7,"GET Engineering Corp.",0},
	{0x15e8,"National Datacomm Corp.",0},
	{0x15e9,"Pacific Digital Corp.",0},
	{0x15ea,"Tokyo Denshi Sekei K.K.",0},
	{0x15eb,"Drsearch GMBH",0},
	{0x15ec,"Beckhoff GMBH",0},
	{0x15ed,"Macrolink Inc",0},
	{0x15ee,"IN Win Development Inc.",0},
	{0x15ef,"Intelligent Paradigm Inc",0},
	{0x15f0,"B-Tree Systems Inc",0},
	{0x15f1,"Times N Systems Inc",0},
	{0x15f2,"Diagnostic Instruments Inc",0},
	{0x15f3,"Digitmedia Corp.",0},
	{0x15f4,"Valuesoft",0},
	{0x15f5,"Power Micro Research",0},
	{0x15f6,"Extreme Packet Device Inc",0},
	{0x15f7,"Banctec",0},
	{0x15f8,"Koga Electronics Co",0},
	{0x15f9,"Zenith Electronics Corporation",0},
	{0x15fa,"J.P. Axzam Corporation",0},
	{0x15fb,"Zilog Inc.",0},
	{0x15fc,"Techsan Electronics Co Ltd",0},
	{0x15fd,"N-Cubed.Net",0},
	{0x15fe,"Kinpo Electronics Inc",0},
	{0x15ff,"Fastpoint Technologies Inc.",0},
	{0x1600,"Northrop Grumman - Canada Ltd",0},
	{0x1601,"Tenta Technology",0},
	{0x1602,"Prosys-TEC Inc.",0},
	{0x1603,"Nokia Wireless Business Communications",0},
	{0x1604,"Central System Research Co Ltd",0},
	{0x1605,"Pairgain Technologies",0},
	{0x1606,"Europop AG",0},
	{0x1607,"Lava Semiconductor Manufacturing Inc.",0},
	{0x1608,"Automated Wagering International",0},
	{0x1609,"Sciemetric Instruments Inc",0},
	{0x160a,"Kollmorgen Servotronix",0},
	{0x160b,"Onkyo Corp.",0},
	{0x160c,"Oregon Micro Systems Inc.",0},
	{0x160d,"Aaeon Electronics Inc",0},
	{0x160e,"CML Emergency Services",0},
	{0x160f,"ITEC Co Ltd",0},
	{0x1610,"Tottori Sanyo Electric Co Ltd",0},
	{0x1611,"Bel Fuse Inc.",0},
	{0x1612,"Telesynergy Research Inc.",0},
	{0x1613,"System Craft Inc.",0},
	{0x1614,"Jace Tech Inc.",0},
	{0x1615,"Equus Computer Systems Inc",0},
	{0x1616,"Iotech Inc.",0},
	{0x1617,"Rapidstream Inc",0},
	{0x1618,"Esec SA",0},
	{0x1619,"FarSite Communications Limited",pci_device_1619},
	{0x161a,"Wvinten Ltd",0},
	{0x161b,"Mobilian Israel Ltd",0},
	{0x161c,"Berkshire Products",0},
	{0x161d,"Gatec",0},
	{0x161e,"Kyoei Sangyo Co Ltd",0},
	{0x161f,"Arima Computer Co",0},
	{0x1620,"Sigmacom Co Ltd",0},
	{0x1621,"Lynx Studio Technology Inc",0},
	{0x1622,"Nokia Home Communications",0},
	{0x1623,"KRF Tech Ltd",0},
	{0x1624,"CE Infosys GMBH",0},
	{0x1625,"Warp Nine Engineering",0},
	{0x1626,"TDK Semiconductor Corp.",0},
	{0x1627,"BCom Electronics Inc",0},
	{0x1629,"Kongsberg Spacetec a.s.",0},
	{0x162a,"Sejin Computerland Co Ltd",0},
	{0x162b,"Shanghai Bell Company Limited",0},
	{0x162c,"C&H Technologies Inc",0},
	{0x162d,"Reprosoft Co Ltd",0},
	{0x162e,"Margi Systems Inc",0},
	{0x162f,"Rohde & Schwarz GMBH & Co KG",0},
	{0x1630,"Sky Computers Inc",0},
	{0x1631,"NEC Computer International",0},
	{0x1632,"Verisys Inc",0},
	{0x1633,"Adac Corporation",0},
	{0x1634,"Visionglobal Network Corp.",0},
	{0x1635,"Decros",0},
	{0x1636,"Jean Company Ltd",0},
	{0x1637,"NSI",0},
	{0x1638,"Eumitcom Technology Inc",0},
	{0x163a,"Air Prime Inc",0},
	{0x163b,"Glotrex Co Ltd",0},
	{0x163c,"Smart Link",0},
	{0x163d,"Heidelberg Digital LLC",0},
	{0x163e,"3dpower",0},
	{0x163f,"Renishaw PLC",0},
	{0x1640,"Intelliworxx Inc",0},
	{0x1641,"MKNet Corporation",0},
	{0x1642,"Bitland",0},
	{0x1643,"Hajime Industries Ltd",0},
	{0x1644,"Western Avionics Ltd",0},
	{0x1645,"Quick-Serv. Computer Co. Ltd",0},
	{0x1646,"Nippon Systemware Co Ltd",0},
	{0x1647,"Hertz Systemtechnik GMBH",0},
	{0x1648,"MeltDown Systems LLC",0},
	{0x1649,"Jupiter Systems",0},
	{0x164a,"Aiwa Co. Ltd",0},
	{0x164c,"Department Of Defense",0},
	{0x164d,"Ishoni Networks",0},
	{0x164e,"Micrel Inc.",0},
	{0x164f,"Datavoice (Pty) Ltd.",0},
	{0x1650,"Admore Technology Inc.",0},
	{0x1651,"Chaparral Network Storage",0},
	{0x1652,"Spectrum Digital Inc.",0},
	{0x1653,"Nature Worldwide Technology Corp",0},
	{0x1654,"Sonicwall Inc",0},
	{0x1655,"Dazzle Multimedia Inc.",0},
	{0x1656,"Insyde Software Corp",0},
	{0x1657,"Brocade Communications Systems",0},
	{0x1658,"Med Associates Inc.",0},
	{0x1659,"Shiba Denshi Systems Inc.",0},
	{0x165a,"Epix Inc.",0},
	{0x165b,"Real-Time Digital Inc.",0},
	{0x165c,"Gidel Ltd.",0},
	{0x165d,"Hsing Tech. Enterprise Co. Ltd.",0},
	{0x165e,"Hyunju Computer Co. Ltd.",0},
	{0x165f,"Add One Company",0},
	{0x1660,"Network Security Technologies Inc. (Net ",0},
	{0x1661,"Worldspace Corp.",0},
	{0x1662,"Int Labs",0},
	{0x1663,"Elmec Inc. Ltd.",0},
	{0x1664,"Fastfame Technology Co. Ltd.",0},
	{0x1665,"Edax Inc.",0},
	{0x1666,"Norpak Corporation",0},
	{0x1667,"CoSystems Inc.",0},
	{0x166a,"Komatsu Ltd.",0},
	{0x166b,"Supernet Inc.",0},
	{0x166c,"Shade Ltd.",0},
	{0x166d,"Sibyte Inc.",0},
	{0x166e,"Schneider Automation Inc.",0},
	{0x166f,"Televox Software Inc.",0},
	{0x1670,"Rearden Steel",0},
	{0x1671,"Atan Technology Inc.",0},
	{0x1672,"Unitec Co. Ltd.",0},
	{0x1673,"Connex",0},
	{0x1675,"Square Wave Technology",0},
	{0x1676,"Emachines Inc.",0},
	{0x1677,"Bernecker + Rainer",0},
	{0x1678,"INH Semiconductor",0},
	{0x1679,"Tokyo Electron Device Ltd.",0},
	{0x1813,"Ambient Technologies Inc",0},
	{0x1a08,"Sierra Semiconductor",pci_device_1a08},
	{0x1b13,"Jaton Corporation",0},
	{0x1c1c,"Symphony",pci_device_1c1c},
	{0x1d44,"Distributed Processing Technology",pci_device_1d44},
	{0x1de1,"Tekram",pci_device_1de1},
	{0x2001,"Temporal Research Ltd",0},
	{0x2348,"Racore",pci_device_2348},
	{0x2646,"Kingston Technology Co.",0},
	{0x270f,"ChainTech Computer Co. Ltd.",0},
	{0x2ec1,"Zenic Inc",0},
	{0x3000,"Hansol Electronics Inc.",0},
	{0x3142,"Post Impressions Systems",0},
	{0x3388,"Hint Corp.",pci_device_3388},
	{0x3411,"Quantum Designs (H.K.) Inc.",0},
	{0x3513,"ARCOM Control Systems Ltd.",0},
	{0x38ef,"4links",0},
	{0x3d3d,"3Dlabs, Inc. Ltd",pci_device_3d3d},
	{0x4005,"Avance Logic Inc.",pci_device_4005},
	{0x4033,"Addtron Technology Co., Inc.",pci_device_4033},
	{0x4143,"Digital Equipment Corp.",0},
	{0x416c,"Aladdin Knowledge Systems",0},
	{0x4444,"ICompression Inc.",0},
	{0x4468,"Bridgeport Machines",0},
	{0x4594,"Cogetec Informatique Inc.",0},
	{0x45fb,"Baldor Electric Company",0},
	{0x4680,"UMAX Computer Corp.",0},
	{0x4843,"Hercules Computer Technology",0},
	{0x4943,"Growth Networks",0},
	{0x4954,"Integral Technologies",0},
	{0x4978,"Axil Computer Inc.",0},
	{0x4a14,"NetVin",pci_device_4a14},
	{0x4b10,"Buslogic Inc",pci_device_4b10},
	{0x4c48,"Lung Hwa Electronics",0},
	{0x4c53,"SBS-OR Industrial Computers",0},
	{0x4ca1,"Seanix Technology Inc",0},
	{0x4d51,"Mediaq Inc.",pci_device_4d51},
	{0x4d54,"Microtechnica Co Ltd",0},
	{0x4ddc,"ILC Data Device Corp.",0},
	{0x5053,"TBS/Voyetra Technologies",pci_device_5053},
	{0x5136,"S S Technologies",0},
	{0x5143,"Qualcomm Inc.",0},
	{0x5145,"ENSONIQ",pci_device_5145},
	{0x5301,"Alliance Semicondutor Corp.",pci_device_5301},
	{0x5333,"S3 Incorporated",pci_device_5333},
	{0x544c,"Teralogic Inc",0},
	{0x5455,"Technische Universtiaet Berlin",pci_device_5455},
	{0x5519,"Cnet Technoliges, Inc.",0},
	{0x5555,"Genroco Inc.",pci_device_5555},
	{0x5700,"Netpower",0},
	{0x6356,"UltraStor",pci_device_6356},
	{0x6374,"c't Magazin f_r Computertechnik",pci_device_6374},
	{0x6409,"Logitec Corp.",0},
	{0x6666,"Decision Computer International Co.",pci_device_6666},
	{0x7604,"O.N. Electric Co. Ltd.",0},
	{0x7747,"DaoGuo Technology Co.,Ltd",0},
	{0x7bde,"MIDAC Corporation",0},
	{0x7fed,"PowerTV",0},
	{0x8001,"Beyertone AG - Germany",pci_device_8001},
	{0x8008,"QUANCOM Informationssysteme GmbH",pci_device_8008},
	{0x8086,"Intel Corporation",pci_device_8086},
	{0x8800,"Trigem Computer",pci_device_8800},
	{0x8866,"T-Square Design Inc.",0},
	{0x8888,"Silicon Magic",0},
	{0x8e0e,"Computone Corporation",0},
	{0x8e2e,"KTI",pci_device_8e2e},
	{0x9004,"Adaptec",pci_device_9004},
	{0x9005,"Adaptec",pci_device_9005},
	{0x907f,"Atronics",pci_device_907f},
	{0x919a,"Gigapixel Corp",0},
	{0x9412,"Holtek",pci_device_9412},
	{0x9699,"Omni Media Technology Inc.",0},
	{0x9902,"Starbridge Technologies Inc",0},
	{0xa0a0,"Aopen Inc.",0},
	{0xa0f1,"Unisys Corporation",0},
	{0xa200,"NEC Corp.",0},
	{0xa259,"Hewlett Packard",0},
	{0xa25b,"Hewlett Packard GmbH PL24-MKT",0},
	{0xa304,"Sony",0},
	{0xa727,"3com Corporation",0},
	{0xaa42,"Scitex Digital Video",0},
	{0xac1e,"Digital Receiver Technology Inc",0},
	{0xb1b3,"Shiva Europe Ltd.",0},
	{0xb894,"Brown & Sharpe Mfg. Co.",0},
	{0xc001,"TSI Telsys",0},
	{0xc0a9,"Micron/Crucial Technology",0},
	{0xc0de,"Motorola",0},
	{0xc0fe,"Motion Engineering Inc.",0},
	{0xc622,"Hudson Soft Co Ltd",0},
	{0xca50,"Varian Australia Pty. Ltd.",0},
	{0xcafe,"Chrysalis-ITS",0},
	{0xcccc,"Catapult Communications",0},
	{0xd4d4,"DY4 Systems Inc.",0},
	{0xd84d,"Exsys",0},
	{0xdc93,"Dawicontrol",0},
	{0xdead,"Indigita Corporation",0},
	{0xe000,"Winbond",pci_device_e000},
	{0xe159,"Tiger Jet Network Inc",pci_device_e159},
	{0xe4bf,"EKF Elektronik GMBH",0},
	{0xea01,"Eagle Technology",0},
	{0xecc0,"Echo Corporation",0},
	{0xedd8,"ARK Logic, Inc",pci_device_edd8},
	{0xf5f5,"F5 Networks Inc.",0},
	{0xfa57,"Fast Search & Transfer ASA",0},
	{0xfeda,"Epigram Inc                             ",0},
	{0xfffe,"VMware Inc.",pci_device_fffe},
	{0xffff,"ILLEGITIMATE VENDOR ID",pci_device_ffff},
	{0x00c7,"Modular Technology Ltd.",0},
	{0, 0, 0}
};
