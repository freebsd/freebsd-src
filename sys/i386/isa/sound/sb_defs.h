#ifdef PC98
#define DSP_RESET	(sbc_base + 0x600)
#define DSP_READ	(sbc_base + 0xA00)
#define DSP_WRITE	(sbc_base + 0xC00)
#define DSP_COMMAND	(sbc_base + 0xC00)
#define DSP_STATUS	(sbc_base + 0xC00)
#define DSP_DATA_AVAIL	(sbc_base + 0xE00)
#define DSP_DATA_AVL16	(sbc_base + 0xF00)
#define MIXER_ADDR	(sbc_base + 0x400)
#define MIXER_DATA	(sbc_base + 0x500)
#define OPL3_LEFT	(sbc_base + 0x000)
#define OPL3_RIGHT	(sbc_base + 0x200)
#define OPL3_BOTH	(sbc_base + 0x800)
#else
#define DSP_RESET	(sbc_base + 0x6)
#define DSP_READ	(sbc_base + 0xA)
#define DSP_WRITE	(sbc_base + 0xC)
#define DSP_COMMAND	(sbc_base + 0xC)
#define DSP_STATUS	(sbc_base + 0xC)
#define DSP_DATA_AVAIL	(sbc_base + 0xE)
#define DSP_DATA_AVL16	(sbc_base + 0xF)
#define MIXER_ADDR	(sbc_base + 0x4)
#define MIXER_DATA	(sbc_base + 0x5)
#define OPL3_LEFT	(sbc_base + 0x0)
#define OPL3_RIGHT	(sbc_base + 0x2)
#define OPL3_BOTH	(sbc_base + 0x8)
#endif
/* DSP Commands */

#define DSP_CMD_SPKON		0xD1
#define DSP_CMD_SPKOFF		0xD3
#define DSP_CMD_DMAON		0xD0
#define DSP_CMD_DMAOFF		0xD4

#define IMODE_NONE		0
#define IMODE_OUTPUT		1
#define IMODE_INPUT		2
#define IMODE_INIT		3
#define IMODE_MIDI		4

#define NORMAL_MIDI	0
#define UART_MIDI	1

