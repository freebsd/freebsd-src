#!/usr/libexec/flua
-- ex: sw=4 et:
--[[
/*-
 * Copyright (c) 2014, Alexander V. Chernikov
 * Copyright (c) 2020, Ryan Moeller <freqlabs@FreeBSD.org>
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
]]

-- Try to put the template.lua library in the package search path.
package.path = (os.getenv("SRCTOP") or "/usr/src").."/tools/lua/?.lua"

-- Render the template named by the first argument to this script.
require("template").render(arg[1], { -- This table is the template's context.

-- The table `enums' is accessible in the template.  It is a list of strings
-- and tables that describe the various enum types we are generating and the
-- ancillary metadata for generating other related code.
enums = {

    -- Strings at this level are rendered as block comments for convenience.
    "SFF-8024 Rev. 4.6 Table 4-1: Indentifier Values",

    -- This table describes an enum type, in this case enum sfp_id:
    {
        name = "id", -- The template prepends the sfp_ prefix to our name.
        description = "Transceiver identifier",

        -- What width int is needed to store this type:
        bits = 8, -- This could be inferred by the values below...

        -- The values, symbols, display names, and descriptions of this enum:
        values =  {
            -- The prefix SFP_ID_ is prepended to the symbolic names.
            -- Only this enum has shortened names for the values, though they
            -- could be added to the other enums.

            -- value, symbolic name, description, shortened name
            {0x00, "UNKNOWN",   "Unknown or unspecified", "Unknown"},
            {0x01, "GBIC",      "GBIC",               "GBIC"},
            {0x02, "SFF",       "Module soldered to motherboard (ex: SFF)",
                "SFF"},
            {0x03, "SFP",       "SFP or SFP+",        "SFP/SFP+/SFP28"},
            {0x04, "XBI",       "300 pin XBI",        "XBI"},
            {0x05, "XENPAK",    "Xenpak",             "Xenpak"},
            {0x06, "XFP",       "XFP",                "XFP"},
            {0x07, "XFF",       "XFF",                "XFF"},
            {0x08, "XFPE",      "XFP-E",              "XFP-E"},
            {0x09, "XPAK",      "XPAK",               "XPAK"},
            {0x0A, "X2",        "X2",                 "X2"},
            {0x0B, "DWDM_SFP",  "DWDM-SFP/SFP+",      "DWDM-SFP/SFP+"},
            {0x0C, "QSFP",      "QSFP",               "QSFP"},
            {0x0D, "QSFPPLUS",  "QSFP+ or later",     "QSFP+"},
            {0x0E, "CXP",       "CXP",                "CXP"},
            {0x0F, "HD4X",      "Shielded Mini Multilane HD 4X", "HD4X"},
            {0x10, "HD8X",      "Shielded Mini Multilane HD 8X", "HD8X"},
            {0x11, "QSFP28",    "QSFP28 or later",    "QSFP28"},
            {0x12, "CXP2",      "CXP2 (aka CXP28)",   "CXP2"},
            {0x13, "CDFP",      "CDFP (Style 1/Style 2)", "CDFP"},
            {0x14, "SMM4",      "Shielded Mini Multilane HD 4X fanout",
                "SMM4"},
            {0x15, "SMM8",      "Shielded Mini Multilane HD 8X fanout",
                "SMM8"},
            {0x16, "CDFP3",     "CDFP (Style 3)",     "CDFP3"},
            {0x17, "MICROQSFP", "microQSFP",          "microQSFP"},
            {0x18, "QSFP_DD",   "QSFP-DD 8X pluggable transceiver", "QSFP-DD"},
            {0x19, "QSFP8X",    "QSFP 8X pluggable transceiver", "QSFP8X"},
            {0x1A, "SFP_DD",    "SFP-DD 2X pluggable transceiver", "SFP-DD"},
            {0x1B, "DSFP",      "DSFP Dual SFP pluggable transceiver", "DSFP"},
            {0x1C, "X4ML",      "x4 MiniLink/OcuLink", "x4MiniLink/OcuLink"},
            {0x1D, "X8ML",      "x8 MiniLink", "x8MiniLink"},
            {0x1E, "QSFP_CMIS",
                "QSFP+ or later w/Common Management Interface Specification",
                "QSFP+(CMIS)"},
        },
    },

    "SFF-8024 Rev. 4.6 Table 4-3: Connector Types",
    {
        name = "conn",
        description = "Connector type",
        bits = 8,
        values = {
            {0x00, "UNKNOWN",         "Unknown"},
            {0x01, "SC",              "SC"},
            {0x02, "FC_1_COPPER",     "Fibre Channel Style 1 copper"},
            {0x03, "FC_2_COPPER",     "Fibre Channel Style 2 copper"},
            {0x04, "BNC_TNC",         "BNC/TNC"},
            {0x05, "FC_COAX",         "Fibre Channel coaxial"},
            {0x06, "FIBER_JACK",      "Fiber Jack"},
            {0x07, "LC",              "LC"},
            {0x08, "MT_RJ",           "MT-RJ"},
            {0x09, "MU",              "MU"},
            {0x0A, "SG",              "SG"},
            {0x0B, "OPTICAL_PIGTAIL", "Optical pigtail"},
            {0x0C, "MPO_1X12_POPTIC", "MPO 1x12 Parallel Optic"},
            {0x0D, "MPO_2X16_POPTIC", "MPO 2x16 Parallel Optic"},
            {0x20, "HSSDC_II",        "HSSDC II"},
            {0x21, "COPPER_PIGTAIL",  "Copper pigtail"},
            {0x22, "RJ45",            "RJ45"},
            {0x23, "NONE",            "No separable connector"},
            {0x24, "MXC_2X16",        "MXC 2x16"},
            {0x25, "CS_OPTICAL",      "CS optical connector"},
            {0x26, "MINI_CS_OPTICAL", "Mini CS optical connector"},
            {0x27, "MPO_2X12_POPTIC", "MPO 2x12 Parallel Optic"},
            {0x28, "MPO_1X16_POPTIC", "MPO 1x16 Parallel Optic"},
        },
    },
    "SFF-8472 Rev. 11.4 table 3.5: Transceiver codes",
    "10G Ethernet/IB compliance codes, byte 3",
    {
        name = "eth_10g",
        description = "10G Ethernet/IB compliance",
        bits = 8,
        values = {
            {0x80, "10G_BASE_ER",       "10G Base-ER"},
            {0x40, "10G_BASE_LRM",      "10G Base-LRM"},
            {0x20, "10G_BASE_LR",       "10G Base-LR"},
            {0x10, "10G_BASE_SR",       "10G Base-SR"},
            {0x08, "1X_SX",             "1X SX"},
            {0x04, "1X_LX",             "1X LX"},
            {0x02, "1X_COPPER_ACTIVE",  "1X Copper Active"},
            {0x01, "1X_COPPER_PASSIVE", "1X Copper Passive"},
        },
    },
    "Ethernet compliance codes, byte 6",
    {
        name = "eth",
        description = "Ethernet compliance",
        bits = 8,
        values = {
            {0x80, "BASE_PX",         "BASE-PX"},
            {0x40, "BASE_BX10",       "BASE-BX10"},
            {0x20, "100BASE_FX",      "100BASE-FX"},
            {0x10, "100BASE_LX_LX10", "100BASE-LX/LX10"},
            {0x08, "1000BASE_T",      "1000BASE-T"},
            {0x04, "1000BASE_CX",     "1000BASE-CX"},
            {0x02, "1000BASE_LX",     "1000BASE-LX"},
            {0x01, "1000BASE_SX",     "1000BASE-SX"},
        },
    },
    "FC link length, byte 7",
    {
        name = "fc_len",
        description = "Fibre Channel link length",
        bits = 8,
        values = {
            {0x80, "VERY_LONG",    "very long distance"},
            {0x40, "SHORT",        "short distance"},
            {0x20, "INTERMEDIATE", "intermediate distance"},
            {0x10, "LONG",         "long distance"},
            {0x08, "MEDIUM",       "medium distance"},
        },
    },
    "Channel/Cable technology, byte 7-8",
    {
        name = "cab_tech",
        description = "Channel/cable technology",
        bits = 16,
        values = {
            {0x0400, "SA",       "Shortwave laser (SA)"},
            {0x0200, "LC",       "Longwave laser (LC)"},
            {0x0100, "EL_INTER", "Electrical inter-enclosure (EL)"},
            {0x0080, "EL_INTRA", "Electrical intra-enclosure (EL)"},
            {0x0040, "SN",       "Shortwave laser (SN)"},
            {0x0020, "SL",       "Shortwave laser (SL)"},
            {0x0010, "LL",       "Longwave laser (LL)"},
            {0x0008, "ACTIVE",   "Active Cable"},
            {0x0004, "PASSIVE",  "Passive Cable"},
        },
    },
    "FC Transmission media, byte 9",
    {
        name = "fc_media",
        description = "Fibre Channel transmission media",
        bits = 8,
        values = {
            {0x80, "TW",       "Twin Axial Pair (TW)"},
            {0x40, "TP",       "Twisted Pair (TP)"},
            {0x20, "MI",       "Miniature Coax (MI)"},
            {0x10, "TV",       "Video Coax (TV)"},
            {0x08, "M6",       "Miltimode 62.5um (M6)"},
            {0x04, "M5",       "Multimode 50um (M5)"},
            {0x02, "RESERVED", "Reserved"},
            {0x01, "SM",       "Single Mode (SM)"},
        },
    },
    "FC Speed, byte 10",
    {
        name = "fc_speed",
        description = "Fibre Channel speed",
        bits = 8,
        values = {
            {0x80, "1200", "1200 MBytes/sec"},
            {0x40, "800",  "800 MBytes/sec"},
            {0x20, "1600", "1600 MBytes/sec"},
            {0x10, "400",  "400 MBytes/sec"},
            {0x08, "3200", "3200 MBytes/sec"},
            {0x04, "200",  "200 MBytes/sec"},
            {0x01, "100",  "100 MBytes/sec"},
        },
    },
    "SFF-8436 Rev. 4.8 table 33: Specification compliance",
    "10/40G Ethernet compliance codes, byte 128 + 3",
    {
        name = "eth_1040g",
        description = "10/40G Ethernet compliance",
        bits = 8,
        values = {
            {0x80, "EXTENDED",    "Extended"},
            {0x40, "10GBASE_LRM", "10GBASE-LRM"},
            {0x20, "10GBASE_LR",  "10GBASE-LR"},
            {0x10, "10GBASE_SR",  "10GBASE-SR"},
            {0x08, "40GBASE_CR4", "40GBASE-CR4"},
            {0x04, "40GBASE_SR4", "40GBASE-SR4"},
            {0x02, "40GBASE_LR4", "40GBASE-LR4"},
            {0x01, "40G_ACTIVE",  "40G Active Cable"},
        },
    },
    "SFF-8024 Rev. 4.6 table 4-4: Extended Specification Compliance",
    {
        name = "eth_ext",
        description = "Extended specification compliance",
        bits = 8,
        values = {
            {0xFF, "RESERVED_FF",             "Reserved"},
            {0x55, "128GFC_LW",               "128GFC LW"},
            {0x54, "128GFC_SW",               "128GFC SW"},
            {0x53, "128GFC_EA",               "128GFC EA"},
            {0x52, "64GFC_LW",                "64GFC LW"},
            {0x51, "64GFC_SW",                "64GFC SW"},
            {0x50, "64GFC_EA",                "64GFC EA"},
            {0x4F, "RESERVED_4F",             "Reserved"},
            {0x4E, "RESERVED_4E",             "Reserved"},
            {0x4D, "RESERVED_4D",             "Reserved"},
            {0x4C, "RESERVED_4C",             "Reserved"},
            {0x4B, "RESERVED_4B",             "Reserved"},
            {0x4A, "RESERVED_4A",             "Reserved"},
            {0x49, "RESERVED_49",             "Reserved"},
            {0x48, "RESERVED_48",             "Reserved"},
            {0x47, "RESERVED_47",             "Reserved"},
            {0x46, "200GBASE_LR4",            "200GBASE-LR4"},
            {0x45, "50GBASE_LR",              "50GBASE-LR"},
            {0x44, "200G_1550NM_PSM4",        "200G 1550nm PSM4"},
            {0x43, "200GBASE_FR4",            "200GBASE-FR4"},
            {0x42, "50GBASE_FR_200GBASE_DR4", "50GBASE-FR or 200GBASE-DR4"},
            {0x41, "50GBASE_SR_100GBASE_SR2_200GBASE_SR4",
                   "50GBASE-SR/100GBASE-SR2/200GBASE-SR4"},
            {0x40, "50GBASE_CR_100GBASE_CR2_200GBASE_CR4",
                   "50GBASE-CR/100GBASE-CR2/200GBASE-CR4"},
            {0x3F, "RESERVED_3F",             "Reserved"},
            {0x3E, "RESERVED_3E",             "Reserved"},
            {0x3D, "RESERVED_3D",             "Reserved"},
            {0x3C, "RESERVED_3C",             "Reserved"},
            {0x3B, "RESERVED_3B",             "Reserved"},
            {0x3A, "RESERVED_3A",             "Reserved"},
            {0x39, "RESERVED_39",             "Reserved"},
            {0x38, "RESERVED_38",             "Reserved"},
            {0x37, "RESERVED_37",             "Reserved"},
            {0x36, "RESERVED_36",             "Reserved"},
            {0x35, "RESERVED_35",             "Reserved"},
            {0x34, "RESERVED_34",             "Reserved"},
            {0x33, "50_100_200GAUI_AOC_HI_BER",
                   "50GAUI/100GAUI-2/200GAUI-4 AOC (BER <2.6e-4)"},
            {0x32, "50_100_200GAUI_ACC_HI_BER",
                   "50GAUI/100GAUI-2/200GAUI-4 ACC (BER <2.6e-4)"},
            {0x31, "50_100_200GAUI_AOC_LO_BER",
                   "50GAUI/100GAUI-2/200GAUI-4 AOC (BER <1e-6)"},
            {0x30, "50_100_200GAUI_ACC_LO_BER",
                   "50GAUI/100GAUI-2/200GAUI-4 ACC (BER <1e-6)"},
            {0x2F, "RESERVED_2F",             "Reserved"},
            {0x2E, "RESERVED_2E",             "Reserved"},
            {0x2D, "RESERVED_2D",             "Reserved"},
            {0x2C, "RESERVED_2C",             "Reserved"},
            {0x2B, "RESERVED_2B",             "Reserved"},
            {0x2A, "RESERVED_2A",             "Reserved"},
            {0x29, "RESERVED_29",             "Reserved"},
            {0x28, "RESERVED_28",             "Reserved"},
            {0x27, "100G_LR",                 "100G-LR"},
            {0x26, "100G_FR",                 "100G-FR"},
            {0x25, "100GBASE_DR",             "100GBASE-DR"},
            {0x24, "4WDM_40_MSA",             "4WDM-40 MSA"},
            {0x23, "4WDM_20_MSA",             "4WDM-20 MSA"},
            {0x22, "4WDM_10_MSA",             "4WDM-10 MSA"},
            {0x21, "100G_PAM4_BIDI",          "100G PAM4 BiDi"},
            {0x20, "100G_SWDM4",              "100G SWDM4"},
            {0x1F, "40G_SWDM4",               "40G SWDM4"},
            {0x1E, "2_5GBASE_T",              "2.5GBASE-T"},
            {0x1D, "5GBASE_T",                "5GBASE-T"},
            {0x1C, "10GBASE_T_SR",            "10GBASE-T Short Reach"},
            {0x1B, "100G_1550NM_WDM",         "100G 1550nm WDM"},
            {0x1A, "100GE_DWDM2",             "100GE-DWDM2"},
            {0x19, "100G_25GAUI_C2M_ACC",     "100G ACC or 25GAUI C2M ACC"},
            {0x18, "100G_25GAUI_C2M_AOC",     "100G AOC or 25GAUI C2M AOC"},
            {0x17, "100G_CLR4",               "100G CLR4"},
            {0x16, "10GBASE_T_SFI",
                   "10GBASE-T with SFI electrical interface"},
            {0x15, "G959_1_P1L1_2D2",         "G959.1 profile P1L1-2D2"},
            {0x14, "G959_1_P1S1_2D2",         "G959.1 profile P1S1-2D2"},
            {0x13, "G959_1_P1I1_2D1",         "G959.1 profile P1I1-2D1"},
            {0x12, "40G_PSM4",                "40G PSM4 Parallel SMF"},
            {0x11, "4X_10GBASE_SR",           "4 x 10GBASE-SR"},
            {0x10, "40GBASE_ER4",             "40GBASE-ER4"},
            {0x0F, "RESERVED_0F",             "Reserved"},
            {0x0E, "RESERVED_0E",             "Reserved"},
            {0x0D, "CA_25G_N",                "25GBASE-CR CA-25G-N"},
            {0x0C, "CA_25G_S",                "25GBASE-CR CA-25G-S"},
            {0x0B, "CA_L",                    "100GBASE-CR4 or 25GBASE-CR CA-L"},
            {0x0A, "RESERVED_0A",             "Reserved"},
            {0x09, "OBSOLETE",                "Obsolete"},
            {0x08, "100G_25GAUI_C2M_ACC_1",
                   "100G ACC (Active Copper Cable)"},
            {0x07, "100G_PSM4_P_SMF",         "100G PSM4 Parallel SMF"},
            {0x06, "100G_CWDM4",              "100G CWDM4"},
            {0x05, "100GBASE_SR10",           "100GBASE-SR10"},
            {0x04, "100GBASE_ER4_25GBASE_ER", "100GBASE-ER4 or 25GBASE-ER"},
            {0x03, "100GBASE_LR4_25GBASE_LR", "100GBASE-LR4 or 25GBASE-LR"},
            {0x02, "100GBASE_SR4_25GBASE_SR", "100GBASE-SR4 or 25GBASE-SR"},
            {0x01, "100G_25GAUI_C2M_AOC_1",
                   "100G AOC (Active Optical Cable)"},
            {0x00, "UNSPECIFIED",             "Unspecified"},
        },
    },
    "SFF-8636 Rev. 2.9 table 6.3: Revision compliance",
    {
        name = "rev",
        description = "Revision compliance",
        bits = 8,
        values = {
            {0x1, "SFF_8436_REV_LE_4_8",     "SFF-8436 rev <=4.8"},
            {0x2, "SFF_8436_REV_LE_4_8_ALT", "SFF-8436 rev <=4.8"},
            {0x3, "SFF_8636_REV_LE_1_3",     "SFF-8636 rev <=1.3"},
            {0x4, "SFF_8636_REV_LE_1_4",     "SFF-8636 rev <=1.4"},
            {0x5, "SFF_8636_REV_LE_1_5",     "SFF-8636 rev <=1.5"},
            {0x6, "SFF_8636_REV_LE_2_0",     "SFF-8636 rev <=2.0"},
            {0x7, "SFF_8636_REV_LE_2_7",     "SFF-8636 rev <=2.7"},
            {0x8, "SFF_8363_REV_GE_2_8",     "SFF-8636 rev >=2.8"},
            {0x0, "UNSPECIFIED",             "Unspecified"},
        },
    },
}

-- Nothing else in this context.
})
