/*-
 * Copyright (c) 2025 Martin Filla
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/libkern.h>
#include <sys/kernel.h>
#include <sys/cdefs.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include <dev/fdt/fdt_common.h>
#include <dev/fdt/fdt_pinctrl.h>
#include <dev/fdt/simplebus.h>
#include <dev/gpio/gpiobusvar.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

static struct ofw_compat_data compat_data[] =
        {{"mediatek,mt7622-pinctrl", 1},
         {NULL,                      0}};

struct mt7622_pinmux_desc {
    const char *modes[8];
    bus_size_t reg_offset;
    int shift;
};

struct mt7622_functions_desc {
    const char *group;
    const char *function;
    const uint32_t pin;
    const char *mode;
};

struct mt7622_pinctrl_softc {
    device_t dev;
    struct resource *mem_res;
    int mem_rid;
    const struct mt7622_pinmux_desc *pinmux;
    const struct mt7622_functions_desc *functions;
};

/* for mt7622 from pinctrl-mt7622.txt */
static const struct mt7622_functions_desc functions[] = {
        /* group, function, pin, mode */
        /* emmc functions */
        {"emmc",                 "emmc",     40, NULL},
        {"emmc",                 "emmc",     41, NULL},
        {"emmc",                 "emmc",     42, NULL},
        {"emmc",                 "emmc",     43, NULL},
        {"emmc",                 "emmc",     44, NULL},
        {"emmc",                 "emmc",     45, NULL},
        {"emmc",                 "emmc",     47, NULL},
        {"emmc",                 "emmc",     48, NULL},
        {"emmc",                 "emmc",     49, NULL},
        {"emmc",                 "emmc",     50, NULL},
        /* emmc_rst */
        {"emmc_rst",             "emmc",     37, "EMMC"},

        /* eth functions*/
        /* eth_pins: eth-pins {
            mux {
                function = "eth";
                groups = "mdc_mdio", "rgmii_via_gmac2";
            };
        }; */

        /* rgmii_via_gmac1 */
        {"rgmii_via_gmac1",      "eth",      59, "G1_TXD"},
        {"rgmii_via_gmac1",      "eth",      60, "G1_TXD"},
        {"rgmii_via_gmac1",      "eth",      61, "G1_TXD"},
        {"rgmii_via_gmac1",      "eth",      62, "G1_TXD"},
        {"rgmii_via_gmac1",      "eth",      63, "G1_TXEN"},
        {"rgmii_via_gmac1",      "eth",      64, "G1_TXC"},
        {"rgmii_via_gmac1",      "eth",      65, "G1_RXD"},
        {"rgmii_via_gmac1",      "eth",      66, "G1_RXD"},
        {"rgmii_via_gmac1",      "eth",      67, "G1_RXD"},
        {"rgmii_via_gmac1",      "eth",      68, "G1_RXD"},
        {"rgmii_via_gmac1",      "eth",      69, "G1_RXDV"},
        {"rgmii_via_gmac1",      "eth",      70, "G1_RXCV"},
        /* rgmii_via_gmac2 */
        {"rgmii_via_gmac2",      "eth",      25, "RGMII"},
        {"rgmii_via_gmac2",      "eth",      26, NULL},
        {"rgmii_via_gmac2",      "eth",      27, NULL},
        {"rgmii_via_gmac2",      "eth",      28, NULL},
        {"rgmii_via_gmac2",      "eth",      29, NULL},
        {"rgmii_via_gmac2",      "eth",      30, NULL},
        {"rgmii_via_gmac2",      "eth",      31, NULL},
        {"rgmii_via_gmac2",      "eth",      32, NULL},
        {"rgmii_via_gmac2",      "eth",      33, NULL},
        {"rgmii_via_gmac2",      "eth",      34, NULL},
        {"rgmii_via_gmac2",      "eth",      35, NULL},
        {"rgmii_via_gmac2",      "eth",      36, NULL},
        /* mdc_mdio */
        {"mdc_mdio",             "eth",      23, "MDIO"},
        {"mdc_mdio",             "eth",      24, NULL},

        /* i2c1_0 */
        {"i2c1_0",               "i2c",      55, "I2C1_SCL"},
        {"i2c1_0",               "i2c",      56, "I2C1_SDA"},
        /* i2c2_0 */
        {"i2c2_0",               "i2c",      57, "I2C2_SCL"},
        {"i2c2_0",               "i2c",      58, "I2C2_SDA"},
        /* i2s1_in_data */
        {"i2s1_in_data",         "i2c",      1,  "I2S1_IN"},
        /* i2s_out_mclk_bclk_ws */
        {"i2s_out_mclk_bclk_ws", "i2c",      3,  "I2S_BCLK_OUT"},
        {"i2s_out_mclk_bclk_ws", "i2c",      4,  "I2S_WS_OUT"},
        {"i2s_out_mclk_bclk_ws", "i2c",      5,  "I2S_MCLK"},
        /* i2s1_out_data */
        {"i2s1_out_data",        "i2c",      2,  "I2S1_OUT"},

        /* ir_1_tx */
        {"ir_1_tx",              "ir",       59, "IR_T"},
        /* ir_1_rx */
        {"ir_1_rx",              "ir",       60, "IR_R"},

        /* wled */
        {"wled",                 "led",      85, "WLED_N"},

        /* par_nand */
        {"par_nand",             "flash",    37, "Parallel NAND Flash"},
        {"par_nand",             "flash",    38, NULL},
        {"par_nand",             "flash",    39, NULL},
        {"par_nand",             "flash",    40, NULL},
        {"par_nand",             "flash",    41, NULL},
        {"par_nand",             "flash",    42, NULL},
        {"par_nand",             "flash",    43, NULL},
        {"par_nand",             "flash",    44, NULL},
        {"par_nand",             "flash",    45, NULL},
        {"par_nand",             "flash",    46, NULL},
        {"par_nand",             "flash",    47, NULL},
        {"par_nand",             "flash",    48, NULL},
        {"par_nand",             "flash",    49, NULL},
        {"par_nand",             "flash",    50, NULL},
        /* snfi */
        {"snfi",                 "flash",    8,  "SNFI_WP"},
        {"snfi",                 "flash",    9,  "SNFI_HOLD"},
        {"snfi",                 "flash",    10, NULL},
        {"snfi",                 "flash",    11, NULL},
        {"snfi",                 "flash",    12, NULL},
        {"snfi",                 "flash",    13, NULL},
        /* spi_nor */
        {"spi_nor",              "flash",    8,  "SPI_WP"},
        {"spi_nor",              "flash",    9,  "SPI_HOLD"},
        {"spi_nor",              "flash",    10, "SPI NOR Flash"},
        {"spi_nor",              "flash",    11, NULL},
        {"spi_nor",              "flash",    12, NULL},
        {"spi_nor",              "flash",    13, NULL},

        /* pcie0_1_waken */
        {"pcie0_1_waken",        "pcie",     79, "PCIE0_PAD_WAKE"},
        /* pcie1_0_waken */
        {"pcie1_0_waken",        "pcie",     14, NULL},
        /* pcie0_1_clkreq */
        {"pcie0_1_clkreq",       "pcie",     80, "CPIE0_PAD_CLKREQ"},
        /* pcie1_0_clkreq */
        {"pcie1_0_clkreq",       "pcie",     15, NULL},
        /* pcie0_pad_perst */
        {"pcie0_pad_perst",      "pcie",     83, "PCIE0_PAD_PERST"},
        /* pcie1_pad_perst */
        {"pcie1_pad_perst",      "pcie",     84, "PCIE1_PAD_PERST"},

        /* pmic_bus */
        {"pmic_bus",             "pmic",     71, "PMIC I2C"},
        {"pmic_bus",             "pmic",     72, NULL},

        /* pwm_ch1_0 */
        {"pwm_ch1_0",            "pwm",      51, "PWM_CH1"},
        /* pwm_ch2_0 */
        {"pwm_ch2_0",            "pwm",      52, "PWM_CH2"},
        /* pwm_ch3_2 */
        {"pwm_ch3_2",            "pwm",      97, "PWM_CH3"},
        /* pwm_ch4_1 */
        {"pwm_ch4_1",            "pwm",      67, "PWM_CH4"},
        /* pwm_ch5_0 */
        {"pwm_ch5_0",            "pwm",      68, "PWM_CH5"},
        /* pwm_ch6_0 */
        {"pwm_ch6_0",            "pwm",      69, "PWM_CH6"},

        /* sd_0 */
        {"sd_0",                 "sd",       16, "SD_D2"},
        {"sd_0",                 "sd",       17, "SD_D2"},
        {"sd_0",                 "sd",       18, "SD_D1"},
        {"sd_0",                 "sd",       19, "SD_D0"},
        {"sd_0",                 "sd",       20, "SD_CLK"},
        {"sd_0",                 "sd",       21, "SD_CMD"},

        /* spic0_0 */
        {"spic0_0",              "spi",      63, "SPIC0_CLK"},
        {"spic0_0",              "spi",      64, "SPIC0_MOSI"},
        {"spic0_0",              "spi",      65, "SPIC0_MISO"},
        {"spic0_0",              "spi",      66, "SPIC0_CS"},
        /* spic1_0 */
        {"spic1_0",              "spi",      67, "SPIC1_CLK"},
        {"spic1_0",              "spi",      68, "SPIC1_MOSI"},
        {"spic1_0",              "spi",      69, "SPIC1_MISO"},
        {"spic1_0",              "spi",      70, "SPIC1_CS"},

        /* uart0_0_tx_rx */
        {"uart0_0_tx_rx",        "uart",     6,  "UART0"},
        {"uart0_0_tx_rx",        "uart",     7,  NULL},
        /* uart2_1_tx_rx */
        {"uart2_1_tx_rx",        "uart",     51, "UART_TXD2"},
        {"uart2_1_tx_rx",        "uart",     52, "UART_RXD2"},

        /* watchdog */
        {"watchdog",             "watchdog", 78, "WATCHDOG"},
};

static const struct mt7622_pinmux_desc pinmux[] = {
        /* from MT7622_Reference_Manual_for_Develope_Board(BPi) */
        /* number of pin , mode 0..7, register, shift */
        [23] = {{"MDIO", "GPIO", NULL, NULL, NULL, NULL, NULL, NULL}, 0x300, 24},
        [24] = {{NULL, "GPIO", NULL, NULL, NULL, NULL, NULL, NULL}, 0x300, 24},
        [37] = {{"Parallel NAND Flash", "GPIO", "EMMC", NULL, NULL, NULL, NULL, NULL}, 0x300, 20},
        [50] = {{NULL, "GPIO", NULL, NULL, NULL, NULL, NULL, NULL}, 0x300, 20},
        [71] = {{"PMIC I2C", "GPIO", NULL, NULL, NULL, NULL, NULL, NULL}, 0x300, 16},
        [72] = {{NULL, "GPIO", NULL, NULL, NULL, NULL, NULL, NULL}, 0x300, 16},
        [25] = {{"RGMII", "GPIO", "SDXC", NULL, NULL, NULL, NULL, NULL}, 0x300, 12},
        [36] = {{NULL, "GPIO", NULL, NULL, NULL, NULL, NULL, NULL}, 0x300, 12},
        [10] = {{"SPI NOR Flash", "GPIO", "SPI NAND Flash", NULL, NULL, NULL, NULL, NULL}, 0x300, 8},
        [13] = {{NULL, "GPIO", NULL, NULL, NULL, NULL, NULL, NULL}, 0x300, 8},
        [6] = {{"UART0", "GPIO", NULL, NULL, NULL, NULL, NULL, NULL}, 0x300, 4},
        [7] = {{NULL, "GPIO", NULL, NULL, NULL, NULL, NULL, NULL}, 0x300, 4},

        [21] = {{"I2S4_OUT", "GPIO21", "SD_CMD", NULL, NULL, "ANTSEL", "BT_SPXT_C0", "DBG_UTIF"}, 0x310, 28},
        [20] = {{"I2S3_OUT", "GPIO20", "SD_CLK", NULL, NULL, "ANTSEL", "BT_SPXT_C1", "DBG_UTIF"}, 0x310, 24},
        [19] = {{"I2S2_OUT", "GPIO19", "SD_D0", NULL, NULL, "ANTSEL", "BT_IPATH_EN", "DBG_UTIF"}, 0x310, 20},
        [18] = {{"I2S4_IN", "GPIO18", "SD_D1", NULL, NULL, "ANTSEL", "BT_ERX_EN", "DBG_UTIF"}, 0x310, 16},
        [76] = {{"SPIC1_CS", "GPIO76", "UART CTS1", "I2C2_SDA", "PWM_CH4", "ANTSEL", NULL, "DBG_UTIF"}, 0x310, 12},
        [75] = {{"SPIC1_MISO", "GPIO75", "UART RTS1", "I2C2_SCL", "PWM_CH3", "ANTSEL", NULL, "DBG_UTIF"}, 0x310, 8},
        [74] = {{"SPIC1_MOSI", "GPIO74", "UART RDX1", "I2C2_SDA", "PWM_CH4", "ANTSEL", NULL, "DBG_UTIF"}, 0x310, 4},
        [73] = {{"SPIC1_CLK", "GPIO73", "UART TXD1", "I2C1_SCL", "PWM_CH1", "ANTSEL", NULL, "DBG_UTIF"}, 0x310, 0},

        [77] = {{"GPIO_D/GPIO77", "GPIO77", NULL, NULL, "PWM_CH5", "ANTSEL", NULL, "DBG_UTIF"}, 0x320, 28},
        [17] = {{"I2S3_IN", "GPIO17", "SD_D2", NULL, "IR_R", "ANTSEL", "BT_ELNA_EN", "DBG_UTIF"}, 0x320, 24},
        [16] = {{"I2S2_IN", "GPIO17", "SD_D2", NULL, "IR_R", "ANTSEL", "BT_ELNA_EN", "DBG_UTIF"}, 0x320, 20},
        [0] = {{"GPIO_A/GPIO0", "GPIO0", NULL, NULL, NULL, NULL, NULL, NULL}, 0x320, 16},
        [78] = {{"WATCHDOG", "GPIO78", NULL, NULL, "PWM_CH6", NULL, NULL, "DBG_UTIF"}, 0x320, 12},
        [35] = {{"GPIO_A/GPIO0", "GPIO35", "PCIE0_PAD_CLKREQ", "PCIE1_PAD_CLKREQ", NULL, "ANTSEL", NULL, NULL}, 0x320, 8},
        [34] = {{"GPIO_A/GPIO0", "GPIO34", "PCIE0_PAD_WAKE", "PCIE1_PAD_WAKE", NULL, "ANTSEL", NULL, "EXT_BGCK"}, 0x320, 4},
        [5] = {{"I2S_MCLK", "GPIO5", NULL, NULL, NULL, NULL, NULL, "DBG_UTIF"}, 0x320, 0},

        [57] = {{"I2C2_SCL", "GPIO57", "UART_RTS1", "TDM_OUT_MCLK", NULL, NULL, NULL, NULL}, 0x330, 28},
        [56] = {{"I2C1_SDA", "GPIO56", "UART_RXD1", "TDM_IN_DATA", NULL, NULL, NULL, NULL}, 0x330, 24},
        [55] = {{"I2C1_SCL", "GPIO55", "UART_TXD1", "TDM_OUT_DATA", NULL, NULL, NULL, NULL}, 0x330, 20},
        [54] = {{"UART_CTS2", "GPIO54", NULL, "PWM_CH4", NULL, NULL, NULL, NULL}, 0x330, 16},
        [53] = {{"UART_RTS2", "GPIO53", NULL, "PWM_CH3", NULL, NULL, NULL, NULL}, 0x330, 12},
        [52] = {{"UART_RXD2", "GPIO52", NULL, "PWM_CH2", NULL, NULL, NULL, NULL}, 0x330, 8},
        [51] = {{"UART_TXD2", "GPIO51", NULL, "PWM_CH1", NULL, NULL, NULL, NULL}, 0x330, 4},
        [84] = {{"PCIE1_PAD_PERST", "GPIO84", NULL, NULL, NULL, NULL, NULL, NULL}, 0x330, 0},

        [65] = {{"ESW_RXD", "GPIO65", "G1_RXD", NULL, "SPIC0_MISO", NULL, NULL, NULL}, 0x340, 28},
        [64] = {{"ESW_TXC", "GPIO64", "G1_TXC", NULL, "SPIC0_MOSI", NULL, NULL, NULL}, 0x340, 24},
        [63] = {{"ESW_TXEN", "GPIO63", "G1_TXEN", NULL, "SPIC0_CLK", NULL, NULL, NULL}, 0x340, 20},
        [62] = {{"ESW_TXD", "GPIO62", "G1_TXD", "TDM_IN_WS", "UART_CTS2_N", "UART_RXD4", NULL, NULL}, 0x340, 16},
        [61] = {{"ESW_TXD", "GPIO61", "G1_TXD", "TDM_IN_BCLK", "UART_RTS2_N", "UART_TXD4", NULL, NULL}, 0x340, 12},
        [60] = {{"ESW_TXD", "GPIO60", "G1_TXD", "TDM_IN_MCLK", "UART_RXD2", "IR_R", NULL, NULL}, 0x340, 8},
        [59] = {{"ESW_TXD", "GPIO59", "G1_TXD", "TMD_OUT_WS", "UART_TXD2", "IR_T", NULL, NULL}, 0x340, 4},
        [58] = {{"I2C2_SDA", "GPIO58", "UART_CTS1", "TDM_OUT_BCLK", NULL, NULL, NULL, NULL}, 0x340, 0},

        [83] = {{"PCIE0_PAD_PERST", "GPIO83", NULL, NULL, NULL, NULL, NULL, NULL}, 0x350, 28},
        [9] = {{"SPI_HOLD", "GPIO9", "SNFI_HOLD", "TDM_OUT_BCLK", NULL, NULL, "FPC_DL_STS", NULL}, 0x350, 24},
        [8] = {{"SPI_WP", "GPIO8", "SNFI_WP", "TDM_OUT_MCLK", NULL, NULL, "FPC_DAT_STS", NULL}, 0x350, 20},
        [70] = {{"ESW_RXC", "GPIO70", "G1_RXCV", "SPIC1_CS", NULL, NULL, NULL, NULL}, 0x350, 16},
        [69] = {{"ESW_RXDV", "GPIO69", "G1_RXDV", "PWM_CH6", "SPIC1_MISO", NULL, NULL, NULL}, 0x350, 12},
        [68] = {{"ESW_RXD", "GPIO68", "G1_RXD", "PWM_CH5", "SPIC1_MOSI", NULL, NULL, NULL}, 0x350, 8},
        [67] = {{"ESW_RXD", "GPIO67", "G1_RXD", "PWM_CH4", "SPIC1_CLK", NULL, NULL, NULL}, 0x350, 4},
        [66] = {{"ESW_RXD", "GPIO66", "G1_RXD", NULL, "SPIC0_CS", NULL, NULL, NULL}, 0x350, 0},

        [90] = {{"I2C2_SDA", "GPIO90", NULL, NULL, NULL, NULL, NULL, NULL}, 0x360, 24},
        [89] = {{"I2C2_SCL", "GPIO89", NULL, NULL, NULL, NULL, NULL, NULL}, 0x360, 20},
        [88] = {{"I2C1_SDA", "GPIO88", NULL, NULL, NULL, NULL, NULL, NULL}, 0x360, 16},
        [87] = {{"I2C1_SCL", "GPIO87", NULL, NULL, NULL, NULL, NULL, NULL}, 0x360, 12},
        [86] = {{"EPHY_LED0_N", "GPIO86", NULL, NULL, "CPUM_HW_SEL", NULL, "FPC_CTL", "JTRST_N"}, 0x360, 8},
        [85] = {{"WLED_N", "GPIO85", NULL, NULL, NULL, NULL, NULL, NULL}, 0x360, 4},
        [102] = {{"GPIO_E/GPIO102", "GPIO102", NULL, NULL, NULL, NULL, "ANTSEL", "FPC_DATA"}, 0x360, 0},

        [97] = {{"PWM_CH3", "GPIO97", "UART_TXD4", NULL, "AICE_TCKC", "ANTSEL", "FPC_DATA[", "W_JTCLK"}, 0x380, 28},
        [96] = {{"PWM_CH2", "GPIO96", "UART_CTS4", "UART_RXD2", "CPUM_CK_XI", "ANTSEL", "FPC_DATA", "W_DBGACK"}, 0x380, 24},
        [95] = {{"I2C1_SDA", "GPIO88", NULL, NULL, NULL, NULL, NULL, NULL}, 0x380, 20},
        [22] = {{"GPIO_B/GPIO22", "GPIO22", NULL, "TSF_INTR", NULL, NULL, "ANTSEL", "DBG_UTIF"}, 0x380, 16},

        [94] = {{"UART_CTS4", "GPIO94", "EPHY_LED4_N", "DFD_TMS", "CPUM", "ANTSEL", "FPC_CTL", "JTMS"}, 0x390, 28},
        [93] = {{"UART_RTS4", "GPIO93", "EPHY_LED3_N", "DFD_TCK", "CPUM", "ANTSEL", "FPC_CTL", "JTCLK"}, 0x390, 24},
        [92] = {{"UART_RXD4", "GPIO92", "EPHY_LED2_N", "DFD_TDO", "CPUM", "ANTSEL", "FPC_CTL", "JTDO"}, 0x390, 20},
        [91] = {{"UART_TXD4", "GPIO91", "EPHY_LED1_N", "DFD_TDI", "CPUM_2B_SEL", "ANTSEL", "FPC_CK_XI", "JTDI"}, 0x390, 16},
        [101] = {{"PWM_CH7", "GPIO101", NULL, NULL, NULL, "ANTSEL", "FPC_DATA", "DBG_UART_TXD"}, 0x390, 12},
        [100] = {{"PWM_CH6", "GPIO100", NULL, "IR_R", NULL, "ANTSEL", "FPC_DATA", "W_JTRST_N"}, 0x390, 8},
        [99] = {{"PWM_CH5", "GPIO99", NULL, "IR_T", "AICE_TMSC", "ANTSEL", "FPC_DATA", "W_JTMS"}, 0x390, 4},
        [98] = {{"PWM_CH4", "GPIO98", "UART_RXD4", NULL, NULL, "ANTSEL", "FPC_DATA", "W_JTDI"}, 0x390, 0},

        [4] = {{"I2S_WS_OUT", "GPIO4", "UART_RXD2", "i2S_WS_IN", NULL, NULL, NULL, NULL}, 0x3A0, 28},
        [3] = {{"I2S_BCLK_OUT", "GPIO3", "UART_TXD2", "i2S_BCLK_IN", NULL, NULL, NULL, NULL}, 0x3A0, 24},
        [2] = {{"I2S1_OUT", "GPIO2", "UART_CTS2_N", NULL, NULL, NULL, NULL, NULL}, 0x3A0, 20},
        [1] = {{"I2S1_IN", "GPIO1", "UART_RTS2_N", NULL, NULL, NULL, NULL, NULL}, 0x3A0, 16},
        [82] = {{"UART_RXD3", "GPIO82", "SPDIF_R", "SPIC0_MOSI", "PWM_CH7", "ANTSEL", NULL, "DBG_UTIF"}, 0x3A0, 12},
        [81] = {{"UART_TXD3", "GPIO81", "SPDIF_T", "SPIC0_CLK", "PWM_CH6", "ANTSEL", NULL, "DBG_UTIF"}, 0x3A0, 8},
        [80] = {{"UART_CTS3", "GPIO80", NULL, "SPIC0_CS", "CPIE0_PAD_CLKREQ", "ANTSEL", NULL, "DBG_UTIF"}, 0x3A0, 4},
        [79] = {{"UART_RTS3", "GPIO79", NULL, "SPIC0_MISO", "PCIE0_PAD_WAKE", "ANTSEL", NULL, "DBG_UTIF"}, 0x3A0, 0}
};

static void
mt7622_pinctrl_process_entry(struct mt7622_pinctrl_softc *sc, const char *group, char *function) {
    for (int i = 0; i < nitems(functions); i++) {
        if ((strcmp(functions[i].group, group) == 0) &&
            (strcmp(functions[i].function, function) == 0)) {

            uint32_t pin = functions[i].pin;
            const char *mode = functions[i].mode;
            const struct mt7622_pinmux_desc *pinmux = &sc->pinmux[pin];
            if (mode != NULL && pinmux != NULL) {
                for (int j = 0; j < nitems(pinmux->modes); j++) {
                    if(pinmux->modes[j] != NULL) {
                        if (strcmp(pinmux->modes[j], mode) == 0) {
                            uint32_t val = bus_read_4(sc->mem_res, pinmux->reg_offset);
                            val &= ~(0xF << pinmux->shift);
                            val |= (j << pinmux->shift);
                            device_printf(sc->dev,
                                          "Pin %d: reg 0x%lX (shift %d): mode %d, mask 0x%X, reg val 0x%08X\n",
                                          pin,
                                          pinmux->reg_offset,
                                          pinmux->shift,
                                          j,
                                          0xF << pinmux->shift,
                                          val
                            );
                            bus_write_4(sc->mem_res, pinmux->reg_offset, val);

                            uint32_t check_val = bus_read_4(sc->mem_res, pinmux->reg_offset);
                            uint32_t set_bits = (check_val >> pinmux->shift) & 0xF;
                            if (set_bits != (j & 0xF)) {
                                device_printf(sc->dev,
                                              "Warning: Pin %d: mode not set correctly! Expected %d, got %d in reg 0x%X\n",
                                              pin, j & 0xF, set_bits, check_val);
                            } else {
                                device_printf(sc->dev,
                                              "Pin %d successfully set to mode %d, reg 0x%lX = 0x%08X\n",
                                              pin, set_bits, pinmux->reg_offset, check_val);
                            }

                        }
                    }
                }
            }
        }
    }
}

static int
mt7622_pinctrl_process_node(struct mt7622_pinctrl_softc *sc, phandle_t child) {
    char mux[64];
    const char **groups = NULL;
    char *function = NULL;
    int num_groups = 0;

    if (OF_getprop(child, "name", &mux, sizeof(mux)) > 0) {
        //device_printf(sc->dev, "mux: %s\n", mux);
        if (strncmp(mux, "mux", 3) == 0) {
            num_groups = ofw_bus_string_list_to_array(child, "groups", &groups);
            if (num_groups <= 0) {
                return (ENOENT);
            }

            if (OF_getprop_alloc(child, "function", (void **) &function) == -1) {
                OF_prop_free(groups);
                return (ENOENT);
            }

            for (int i = 0; i < num_groups; i++) {
                mt7622_pinctrl_process_entry(sc, groups[i], function);
            }
        }
    }
    OF_prop_free(groups);
    OF_prop_free(function);
    return 0;
}

static int
mt7622_pinctrl_configure(device_t dev, phandle_t cfgxref) {
    struct mt7622_pinctrl_softc *sc;
    phandle_t child, node;
    sc = device_get_softc(dev);
    node = OF_node_from_xref(cfgxref);

    for (child = OF_child(node); child != 0 && child != -1; child = OF_peer(child)) {
        mt7622_pinctrl_process_node(sc, child);
    }

    return 0;
}

static int
mt7622_pinctrl_probe(device_t dev) {
    if (!ofw_bus_status_okay(dev))
        return (ENXIO);

    if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data)
        return (ENXIO);

    device_set_desc(dev, "Mediatek 7622 pinctrl configuration");
    return (BUS_PROBE_DEFAULT);
}

static int
mt7622_pinctrl_attach(device_t dev) {
    struct mt7622_pinctrl_softc *sc = device_get_softc(dev);

    sc->dev = dev;
    /* Map memory resource */
    sc->mem_rid = 0;
    sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->mem_rid, RF_ACTIVE);
    if (sc->mem_res == NULL) {
        device_printf(dev, "Could not map memory resource\n");
        return (ENXIO);
    }

    sc->pinmux = pinmux;
    sc->functions = functions;

    fdt_pinctrl_register(dev, NULL);
    fdt_pinctrl_configure_tree(dev);

    return (0);
}

static int mt7622_pinctrl_detach(device_t dev) {
    struct mt7622_pinctrl_softc *sc = device_get_softc(dev);
    if (sc->mem_res) {
        bus_release_resource(dev, SYS_RES_MEMORY, sc->mem_rid, sc->mem_res);
        sc->mem_res = NULL;
    }

    return (0);
}

static device_method_t mt7622_pinctrl_methods[] = {
        DEVMETHOD(device_probe, mt7622_pinctrl_probe),
        DEVMETHOD(device_attach, mt7622_pinctrl_attach),
        DEVMETHOD(device_detach, mt7622_pinctrl_detach),
        DEVMETHOD(fdt_pinctrl_configure, mt7622_pinctrl_configure),
        DEVMETHOD_END
};

static DEFINE_CLASS_0(mt7622_pinctrl, mt7622_pinctrl_driver, mt7622_pinctrl_methods,
sizeof(struct mt7622_pinctrl_softc));
EARLY_DRIVER_MODULE(mt7622_pinctrl, simplebus, mt7622_pinctrl_driver, NULL, NULL,
71);
