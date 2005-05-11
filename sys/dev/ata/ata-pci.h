/*-
 * Copyright (c) 2003 - 2005 Søren Schmidt <sos@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/* structure holding chipset config info */
struct ata_chip_id {
    u_int32_t           chipid;
    u_int8_t            chiprev;
    int                 cfg1;
    int                 cfg2;
    u_int8_t            max_dma;
    char                *text;
};

/* structure describing a PCI ATA controller */
struct ata_pci_controller {
    device_t            dev;
    int                 r_type1;
    int                 r_rid1;
    struct resource     *r_res1;
    int                 r_type2;
    int                 r_rid2;
    struct resource     *r_res2;
    struct resource     *r_irq;
    void                *handle;
    struct ata_chip_id  *chip;
    int                 channels;
    int                 (*chipinit)(device_t);
    int                 (*allocate)(device_t);
    int                 (*locking)(device_t, int);
    void                (*reset)(device_t);
    void                (*dmainit)(device_t);
    void                (*setmode)(device_t, int);
    struct {
    void                (*function)(void *);
    void                *argument;
    } interrupt[8];     /* SOS max ch# for now XXX */
};

/* structure for SATA connection update hotplug/hotswap support */
struct ata_connect_task {
    struct task task;
    device_t    dev;  
    int         action;
#define ATA_C_ATTACH    1
#define ATA_C_DETACH    2
};

/* defines for known chipset PCI id's */
#define ATA_ACARD_ID            0x1191
#define ATA_ATP850              0x00021191
#define ATA_ATP850A             0x00041191
#define ATA_ATP850R             0x00051191
#define ATA_ATP860A             0x00061191
#define ATA_ATP860R             0x00071191
#define ATA_ATP865A             0x00081191
#define ATA_ATP865R             0x00091191

#define ATA_AMD_ID              0x1022
#define ATA_AMD755              0x74011022
#define ATA_AMD756              0x74091022
#define ATA_AMD766              0x74111022
#define ATA_AMD768              0x74411022
#define ATA_AMD8111             0x74691022

#define ATA_ACER_LABS_ID        0x10b9
#define ATA_ALI_5229            0x522910b9
#define ATA_ALI_5281            0x528110b9
#define ATA_ALI_5287            0x528710b9
#define ATA_ALI_5288            0x528810b9
#define ATA_ALI_5289            0x528910b9

#define ATA_CENATEK_ID          0x16ca
#define ATA_CENATEK_ROCKET      0x000116ca

#define ATA_CYRIX_ID            0x1078
#define ATA_CYRIX_5530          0x01021078

#define ATA_CYPRESS_ID          0x1080
#define ATA_CYPRESS_82C693      0xc6931080

#define ATA_DEC_21150           0x00221011
#define ATA_DEC_21150_1         0x00231011

#define ATA_HIGHPOINT_ID        0x1103
#define ATA_HPT366              0x00041103
#define ATA_HPT372              0x00051103
#define ATA_HPT302              0x00061103
#define ATA_HPT371              0x00071103
#define ATA_HPT374              0x00081103

#define ATA_INTEL_ID            0x8086
#define ATA_I960RM              0x09628086
#define ATA_I82371FB            0x12308086
#define ATA_I82371SB            0x70108086
#define ATA_I82371AB            0x71118086
#define ATA_I82443MX            0x71998086
#define ATA_I82451NX            0x84ca8086
#define ATA_I82372FB            0x76018086
#define ATA_I82801AB            0x24218086
#define ATA_I82801AA            0x24118086
#define ATA_I82801BA            0x244a8086
#define ATA_I82801BA_1          0x244b8086
#define ATA_I82801CA            0x248a8086
#define ATA_I82801CA_1          0x248b8086
#define ATA_I82801DB            0x24cb8086
#define ATA_I82801DB_1          0x24ca8086
#define ATA_I82801EB            0x24db8086
#define ATA_I82801EB_S1         0x24d18086
#define ATA_I82801EB_R1         0x24df8086
#define ATA_I6300ESB            0x25a28086
#define ATA_I6300ESB_S1         0x25a38086
#define ATA_I6300ESB_R1         0x25b08086
#define ATA_I82801FB            0x266f8086
#define ATA_I82801FB_S1         0x26518086
#define ATA_I82801FB_R1         0x26528086
#define ATA_I82801FB_M          0x26538086
#define ATA_I82801GB		0x27df8086
#define ATA_I82801GB_S1		0x27c08086
#define ATA_I82801GB_R1		0x27c38086
#define ATA_I82801GB_AH		0x27c18086
#define ATA_I82801GB_M		0x27c58086

#define ATA_ITE_ID              0x1283
#define ATA_IT8212F             0x82121283

#define ATA_MICRON_ID           0x1042
#define ATA_MICRON_RZ1000       0x10001042
#define ATA_MICRON_RZ1001       0x10011042

#define ATA_NATIONAL_ID         0x100b
#define ATA_SC1100              0x0502100b

#define ATA_NVIDIA_ID           0x10de
#define ATA_NFORCE1             0x01bc10de
#define ATA_NFORCE2             0x006510de
#define ATA_NFORCE2_MCP         0x008510de
#define ATA_NFORCE2_MCP_S1      0x008e10de
#define ATA_NFORCE3             0x00d510de
#define ATA_NFORCE3_PRO         0x00e510de
#define ATA_NFORCE3_PRO_S1      0x00e310de
#define ATA_NFORCE3_PRO_S2      0x00ee10de
#define ATA_NFORCE3_MCP         0x003510de
#define ATA_NFORCE3_MCP_S1      0x003610de
#define ATA_NFORCE3_MCP_S2      0x003e10de
#define ATA_NFORCE4             0x005310de
#define ATA_NFORCE4_S1          0x005410de
#define ATA_NFORCE4_S2          0x005510de


#define ATA_PROMISE_ID          0x105a
#define ATA_PDC20246            0x4d33105a
#define ATA_PDC20262            0x4d38105a
#define ATA_PDC20263            0x0d38105a
#define ATA_PDC20265            0x0d30105a
#define ATA_PDC20267            0x4d30105a
#define ATA_PDC20268            0x4d68105a
#define ATA_PDC20269            0x4d69105a
#define ATA_PDC20270            0x6268105a
#define ATA_PDC20271            0x6269105a
#define ATA_PDC20275            0x1275105a
#define ATA_PDC20276            0x5275105a
#define ATA_PDC20277            0x7275105a
#define ATA_PDC20318            0x3318105a
#define ATA_PDC20319            0x3319105a
#define ATA_PDC20371            0x3371105a
#define ATA_PDC20375            0x3375105a
#define ATA_PDC20376            0x3376105a
#define ATA_PDC20377            0x3377105a
#define ATA_PDC20378            0x3373105a
#define ATA_PDC20379            0x3372105a
#define ATA_PDC20571            0x3571105a
#define ATA_PDC20575            0x3d75105a
#define ATA_PDC20579            0x3574105a
#define ATA_PDC20580            0x3570105a
#define ATA_PDC40518            0x3d18105a
#define ATA_PDC40519            0x3519105a
#define ATA_PDC20617            0x6617105a
#define ATA_PDC20618            0x6626105a
#define ATA_PDC20619            0x6629105a
#define ATA_PDC20620            0x6620105a
#define ATA_PDC20621            0x6621105a
#define ATA_PDC20622            0x6622105a

#define ATA_SERVERWORKS_ID      0x1166
#define ATA_ROSB4_ISA           0x02001166
#define ATA_ROSB4               0x02111166
#define ATA_CSB5                0x02121166
#define ATA_CSB6                0x02131166
#define ATA_CSB6_1              0x02171166

#define ATA_SILICON_IMAGE_ID    0x1095
#define ATA_SII3114             0x31141095
#define ATA_SII3512             0x35121095
#define ATA_SII3112             0x31121095
#define ATA_SII3112_1           0x02401095
#define ATA_SII0680             0x06801095
#define ATA_CMD646              0x06461095
#define ATA_CMD648              0x06481095
#define ATA_CMD649              0x06491095

#define ATA_SIS_ID              0x1039
#define ATA_SISSOUTH            0x00081039
#define ATA_SIS5511             0x55111039
#define ATA_SIS5513             0x55131039
#define ATA_SIS5517             0x55171039
#define ATA_SIS5518             0x55181039
#define ATA_SIS5571             0x55711039
#define ATA_SIS5591             0x55911039
#define ATA_SIS5596             0x55961039
#define ATA_SIS5597             0x55971039
#define ATA_SIS5598             0x55981039
#define ATA_SIS5600             0x56001039
#define ATA_SIS530              0x05301039
#define ATA_SIS540              0x05401039
#define ATA_SIS550              0x05501039
#define ATA_SIS620              0x06201039
#define ATA_SIS630              0x06301039
#define ATA_SIS635              0x06351039
#define ATA_SIS633              0x06331039
#define ATA_SIS640              0x06401039
#define ATA_SIS645              0x06451039
#define ATA_SIS646              0x06461039
#define ATA_SIS648              0x06481039
#define ATA_SIS650              0x06501039
#define ATA_SIS651              0x06511039
#define ATA_SIS652              0x06521039
#define ATA_SIS655              0x06551039
#define ATA_SIS658              0x06581039
#define ATA_SIS661              0x06611039
#define ATA_SIS730              0x07301039
#define ATA_SIS733              0x07331039
#define ATA_SIS735              0x07351039
#define ATA_SIS740              0x07401039
#define ATA_SIS745              0x07451039
#define ATA_SIS746              0x07461039
#define ATA_SIS748              0x07481039
#define ATA_SIS750              0x07501039
#define ATA_SIS751              0x07511039
#define ATA_SIS752              0x07521039
#define ATA_SIS755              0x07551039
#define ATA_SIS961              0x09611039
#define ATA_SIS962              0x09621039
#define ATA_SIS963              0x09631039
#define ATA_SIS964              0x09641039
#define ATA_SIS965              0x09651039
#define ATA_SIS180              0x01801039
#define ATA_SIS181              0x01811039
#define ATA_SIS182              0x01821039

#define ATA_VIA_ID              0x1106
#define ATA_VIA82C571           0x05711106
#define ATA_VIA82C586           0x05861106
#define ATA_VIA82C596           0x05961106
#define ATA_VIA82C686           0x06861106
#define ATA_VIA8231             0x82311106
#define ATA_VIA8233             0x30741106
#define ATA_VIA8233A            0x31471106
#define ATA_VIA8233C            0x31091106
#define ATA_VIA8235             0x31771106
#define ATA_VIA8237             0x32271106
#define ATA_VIA8361             0x31121106
#define ATA_VIA8363             0x03051106
#define ATA_VIA8371             0x03911106
#define ATA_VIA8662             0x31021106
#define ATA_VIA6410             0x31641106
#define ATA_VIA6420             0x31491106
#define ATA_VIA6421             0x32491106

/* chipset setup related defines */
#define ATPOLD          1

#define ALIOLD          0x01
#define ALINEW          0x02
#define ALISATA         0x04

#define HPT366          0
#define HPT370          1
#define HPT372          2
#define HPT374          3
#define HPTOLD          0x01

#define PROLD           0
#define PRNEW           1
#define PRTX            2
#define PRMIO           3
#define PRTX4           0x01
#define PRSX4X          0x02
#define PRSX6K          0x04
#define PRPATA          0x08
#define PRCMBO          0x10
#define PRCMBO2         0x20
#define PRSATA          0x40
#define PRSATA2         0x80

#define SWKS33          0
#define SWKS66          1
#define SWKS100         2

#define SIIMEMIO        1
#define SIIINTR         0x01
#define SIISETCLK       0x02
#define SIIBUG          0x04
#define SII4CH          0x08

#define SIS_SOUTH       1
#define SISSATA         2
#define SIS133NEW       3
#define SIS133OLD       4
#define SIS100NEW       5
#define SIS100OLD       6
#define SIS66           7
#define SIS33           8

#define VIA33           0
#define VIA66           1
#define VIA100          2
#define VIA133          3
#define AMDNVIDIA       4

#define AMDCABLE        0x01
#define AMDBUG          0x02
#define NVIDIA          0x04
#define NV4OFF          0x08
#define VIACLK          0x10
#define VIABUG          0x20


/* global prototypes ata-pci.c */
int ata_pci_probe(device_t dev);
int ata_pci_attach(device_t dev);
int ata_pci_detach(device_t dev);
struct resource * ata_pci_alloc_resource(device_t dev, device_t child, int type, int *rid, u_long start, u_long end, u_long count, u_int flags);
int ata_pci_release_resource(device_t dev, device_t child, int type, int rid, struct resource *r);
int ata_pci_setup_intr(device_t dev, device_t child, struct resource *irq, int flags, driver_intr_t *function, void *argument, void **cookiep);
 int ata_pci_teardown_intr(device_t dev, device_t child, struct resource *irq, void *cookie);
int ata_pci_allocate(device_t dev);

/* global prototypes ata-chipset.c */
int ata_generic_ident(device_t);
int ata_acard_ident(device_t);
int ata_ali_ident(device_t);
int ata_amd_ident(device_t);
int ata_cyrix_ident(device_t);
int ata_cypress_ident(device_t);
int ata_highpoint_ident(device_t);
int ata_intel_ident(device_t);
int ata_ite_ident(device_t);
int ata_national_ident(device_t);
int ata_nvidia_ident(device_t);
int ata_promise_ident(device_t);
int ata_serverworks_ident(device_t);
int ata_sii_ident(device_t);
int ata_sis_ident(device_t);
int ata_via_ident(device_t);
int ata_legacy(device_t);
struct ata_chip_id *ata_match_chip(device_t, struct ata_chip_id *);

/* global prototypes ata-dma.c */
void ata_dmainit(device_t);
