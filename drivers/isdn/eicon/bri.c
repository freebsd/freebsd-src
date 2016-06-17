/*
 * Copyright (C) Eicon Technology Corporation, 2000.
 *
 * Eicon File Revision :    1.8  
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#include "eicon.h"
#include "sys.h"
#include "idi.h"
#include "divas.h"
#include "pc.h"
#include "pr_pc.h"
#include "dsp_defs.h"

#include "adapter.h"
#include "uxio.h"

#define PCI_BADDR0	0x10
#define PCI_BADDR1	0x14
#define PCI_BADDR2	0x18

#define DIVAS_SIGNATURE 0x4447

/* offset to start of MAINT area (used by xlog) */

#define	DIVAS_MAINT_OFFSET	0xff00		/* value for BRI card */

#define PROTCAP_TELINDUS	0x1
#define PROTCAP_V90D		0x8

word GetProtFeatureValue(char *sw_id);
byte io_in(ADAPTER *a, void *adr);
word io_inw(ADAPTER *a, void *adr);
void io_in_buffer(ADAPTER *a, void *adr, void *P, word length);
void io_look_ahead(ADAPTER *a, PBUFFER *RBuffer, ENTITY *e);
void io_out(ADAPTER *a, void *adr, byte data);
void io_outw(ADAPTER *a, void *adr, word data);
void io_out_buffer(ADAPTER *a, void *adr, void *P, word length);
void io_inc(ADAPTER *a, void *adr);

static int diva_server_bri_test_int(card_t *card);
static int bri_ISR (card_t* card);

#define PLX_IOBASE		0
#define	DIVAS_IOBASE	1

#define	REG_DATA		0x00
#define	REG_ADDRLO		0x04
#define REG_ADDRHI		0x0C
#define REG_IOCTRL		0x10

#define M_PCI_RESET	0x10

byte UxCardPortIoIn(ux_diva_card_t *card, byte *base, int offset);
word UxCardPortIoInW(ux_diva_card_t *card, byte *base, int offset);
void UxCardPortIoOut(ux_diva_card_t *card, byte *base, int offset, byte);
void UxCardPortIoOutW(ux_diva_card_t *card, byte *base, int offset, word);

int DivasBRIInitPCI(card_t *card, dia_card_t *cfg);

static
int	diva_server_bri_reset(card_t *card)
{
	byte *DivasIOBase;
	word i;
	dword dwWait;

	UxCardLog(0);

	DPRINTF(("divas: resetting BRI adapter"));

	DivasIOBase = UxCardMemAttach(card->hw, DIVAS_IOBASE);

	UxCardPortIoOut(card->hw, DivasIOBase, REG_IOCTRL, 0);

	for (i=0; i < 50000; i++)
		;

	UxCardPortIoOut(card->hw, DivasIOBase, REG_ADDRHI, 0);
	UxCardPortIoOutW(card->hw, DivasIOBase, REG_ADDRLO, 0);
	UxCardPortIoOutW(card->hw, DivasIOBase, REG_DATA  , 0);

	UxCardPortIoOut(card->hw, DivasIOBase, REG_ADDRHI, 0xFF);
	UxCardPortIoOutW(card->hw, DivasIOBase, REG_ADDRLO, 0x0000);

	for (i=0; i<0x8000; i++)
	{
		UxCardPortIoOutW(card->hw, DivasIOBase, REG_DATA , 0);
	}

	for (dwWait=0; dwWait < 0x00FFFFFF; dwWait++)
		;

	UxCardMemDetach(card->hw, DivasIOBase);

	return 0;
}

static
void diva_server_bri_reset_int(card_t *card)
{
	byte *DivasIOBase = NULL;

	DivasIOBase = UxCardMemAttach(card->hw, DIVAS_IOBASE);

	UxCardPortIoOut(card->hw, DivasIOBase, REG_IOCTRL, 0x08);

	UxCardMemDetach(card->hw, DivasIOBase);

	return;
}

static
int diva_server_bri_start(card_t *card, byte *channels)
{
	byte *DivasIOBase, *PLXIOBase;
	word wSig = 0;
	word i;
	dword dwSerialNum;
	byte bPLX9060 = FALSE;

	DPRINTF(("divas: starting Diva Server BRI card"));

	card->is_live = FALSE;

	DivasIOBase = UxCardMemAttach(card->hw, DIVAS_IOBASE);

	UxCardPortIoOut(card->hw, DivasIOBase, REG_ADDRHI, 0xFF);
	UxCardPortIoOutW(card->hw, DivasIOBase, REG_ADDRLO, 0x1E);

	UxCardPortIoOut(card->hw, DivasIOBase, REG_DATA  , 0);
	UxCardPortIoOut(card->hw, DivasIOBase, REG_DATA  , 0);
	
	UxCardPortIoOut(card->hw, DivasIOBase, REG_IOCTRL, 0x08);

	/* wait for signature to indicate card has started */
	for (i = 0; i < 300; i++)
	{
		UxCardPortIoOut(card->hw, DivasIOBase, REG_ADDRHI, 0xFF);
		UxCardPortIoOutW(card->hw, DivasIOBase, REG_ADDRLO, 0x1E);
		wSig = UxCardPortIoInW(card->hw, DivasIOBase, REG_DATA);

		if (wSig == DIVAS_SIGNATURE)
		{
			DPRINTF(("divas: card started after %d ms", i * 10));
			break;
		}
		UxPause(10);
	}

	if (wSig != DIVAS_SIGNATURE)
	{
		DPRINTF(("divas: card failed to start (Sig=0x%x)", wSig));
		UxCardMemDetach(card->hw, DivasIOBase);
		return -1;
	}

	card->is_live = TRUE;

	UxCardPortIoOut(card->hw, DivasIOBase, REG_ADDRHI, 0xFF);
	UxCardPortIoOutW(card->hw, DivasIOBase, REG_ADDRLO, 0x3F6);
	*channels = UxCardPortIoInW(card->hw, DivasIOBase, REG_DATA);

	UxCardMemDetach(card->hw, DivasIOBase);

	PLXIOBase = UxCardMemAttach(card->hw, PLX_IOBASE);

	bPLX9060 = UxCardPortIoInW(card->hw, PLXIOBase, 0x6C) | UxCardPortIoInW(card->hw, PLXIOBase, 0x6E);

	if (bPLX9060)
	{ 
		dwSerialNum = (UxCardPortIoInW(card->hw, PLXIOBase, 0x1E) << 16) | 
					(UxCardPortIoInW(card->hw, PLXIOBase, 0x22));
		DPRINTF(("divas: PLX9060 in use. Serial number 0x%04X", dwSerialNum));
	}
	else
	{
		dwSerialNum = (UxCardPortIoInW(card->hw, PLXIOBase, 0x22) << 16) | 
					(UxCardPortIoInW(card->hw, PLXIOBase, 0x26));
		DPRINTF(("divas: PLX9050 in use. Serial number 0x%04X", dwSerialNum));
	}

	UxCardMemDetach(card->hw, PLXIOBase);

	card->serial_no = dwSerialNum;

	diva_server_bri_test_int(card);
	
	return 0;
}

static
int diva_server_bri_load(card_t *card, dia_load_t *load)
{
	byte *DivasIOBase;
	dword r3000_base;
	dword dwAddr, dwLength, i;
	word wTest, aWord;

	DPRINTF(("divas: loading Diva Server BRI card"));

	switch (load->code_type)
	{
		case DIA_CPU_CODE:
		DPRINTF(("divas: loading RISC %s", &load->code[0x80]));

		card->hw->features = GetProtFeatureValue((char *)&load->code[0x80]);
		DPRINTF(("divas: features 0x%x", card->hw->features));
		if (card->hw->features == 0xFFFF)
		{
			DPRINTF(("divas: invalid feature string failed load\n"));
			return -1;
		}

		r3000_base = 0;
		break;

		case DIA_DSP_CODE:
		DPRINTF(("divas: DSP code \"%s\"", load->code));

		if ((card->hw->features) && (!(card->hw->features & PROTCAP_TELINDUS)))
		{
			DPRINTF(("divas: only Telindus style binaries supported"));
			return -1;
		}

		if ((card->hw->features) && (card->hw->features & PROTCAP_V90D))
		{
			DPRINTF(("divas: V.90 DSP binary"));
			r3000_base = (0xBF790000 + (((sizeof(dword) + (sizeof(t_dsp_download_desc)* DSP_MAX_DOWNLOAD_COUNT)) + 3) & 0xFFFFFFFC));
		}
		else
		{
			DPRINTF(("divas: non-V.90 DSP binary"));
			r3000_base = (0xBF7A0000 + (((sizeof(dword) + (sizeof(t_dsp_download_desc)* DSP_MAX_DOWNLOAD_COUNT)) + 3) & 0xFFFFFFFC));
		}
		DPRINTF(("divas: loading at 0x%x", r3000_base));
		break;

		case DIA_TABLE_CODE:
		DPRINTF(("divas: TABLE code"));
		if ((card->hw->features) && (card->hw->features & PROTCAP_V90D))
		{
			r3000_base = 0xBF790000 + sizeof(dword);
		}
		else
		{
			r3000_base = 0xBF7A0000 + sizeof(dword);
		}

		break;

		case DIA_DLOAD_CNT:
		DPRINTF(("divas: COUNT code"));
		if ((card->hw->features) && (card->hw->features & PROTCAP_V90D))
		{
			r3000_base = 0xBF790000;
		}
		else
		{
			r3000_base = 0xBF7A0000;
		}
		break;

		default:
		DPRINTF(("divas: unknown code type %d", load->code_type));
		return -1;
		break;
	}

	DPRINTF(("divas: Writing %d bytes to adapter, address 0x%x", load->length, r3000_base));

	DivasIOBase = UxCardMemAttach(card->hw, DIVAS_IOBASE);

	DPRINTF(("divas: Attached to 0x%04X", DivasIOBase));

	dwLength = load->length;

	for (i=0; i < dwLength; i++)
	{
		dwAddr = r3000_base + i;

		UxCardPortIoOut(card->hw, DivasIOBase, REG_ADDRHI, dwAddr >> 16);
		UxCardPortIoOutW(card->hw, DivasIOBase, REG_ADDRLO, dwAddr);

		UxCardPortIoOut(card->hw, DivasIOBase, REG_DATA, load->code[i]);
	}

	DPRINTF(("divas: Verifying"));

	for (i=0; i<dwLength; i++)
	{
		dwAddr = r3000_base + i;

		UxCardPortIoOut(card->hw, DivasIOBase, REG_ADDRHI, dwAddr >> 16);
		UxCardPortIoOutW(card->hw, DivasIOBase, REG_ADDRLO, dwAddr);

		wTest = UxCardPortIoIn(card->hw, DivasIOBase, REG_DATA);

		aWord = load->code[i];

		if (wTest != aWord)
		{
			DPRINTF(("divas: load verify failed on byte %d", i));
			DPRINTF(("divas: RAM 0x%x   File 0x%x",wTest,aWord));
			
			UxCardMemDetach(card->hw, DivasIOBase);

			return -1;
		}
	}

	DPRINTF(("divas: Loaded and verified. Detaching from adapter"));

	UxCardMemDetach(card->hw, DivasIOBase);

	UxCardLog(0);

	return 0;
}

static
int diva_server_bri_config(card_t *card, dia_config_t *config)
{
	byte *DivasIOBase, i;

	DPRINTF(("divas: configuring Diva Server BRI card"));

	DivasIOBase = UxCardMemAttach(card->hw, DIVAS_IOBASE);

	UxCardPortIoOut(card->hw, DivasIOBase, REG_ADDRHI, 0xFF);

	UxCardPortIoOutW(card->hw, DivasIOBase, REG_ADDRLO, 8);
	UxCardPortIoOut(card->hw, DivasIOBase, REG_DATA, config->tei);

	UxCardPortIoOutW(card->hw, DivasIOBase, REG_ADDRLO, 9);
	UxCardPortIoOut(card->hw, DivasIOBase, REG_DATA, config->nt2);

	UxCardPortIoOutW(card->hw, DivasIOBase, REG_ADDRLO, 10);
	UxCardPortIoOut(card->hw, DivasIOBase, REG_DATA, config->sig_flags);

	UxCardPortIoOutW(card->hw, DivasIOBase, REG_ADDRLO, 11);
	UxCardPortIoOut(card->hw, DivasIOBase, REG_DATA, config->watchdog);

	UxCardPortIoOutW(card->hw, DivasIOBase, REG_ADDRLO, 12);
	UxCardPortIoOut(card->hw, DivasIOBase, REG_DATA, config->permanent);

	UxCardPortIoOutW(card->hw, DivasIOBase, REG_ADDRLO, 13);
	UxCardPortIoOut(card->hw, DivasIOBase, REG_DATA, 0);

	UxCardPortIoOutW(card->hw, DivasIOBase, REG_ADDRLO, 14);
	UxCardPortIoOut(card->hw, DivasIOBase, REG_DATA, config->stable_l2);

	UxCardPortIoOutW(card->hw, DivasIOBase, REG_ADDRLO, 15);
	UxCardPortIoOut(card->hw, DivasIOBase, REG_DATA, config->no_order_check);

	UxCardPortIoOutW(card->hw, DivasIOBase, REG_ADDRLO, 16);
	UxCardPortIoOut(card->hw, DivasIOBase, REG_DATA, 0);

	UxCardPortIoOutW(card->hw, DivasIOBase, REG_ADDRLO, 17);
	UxCardPortIoOut(card->hw, DivasIOBase, REG_DATA, 0);

	UxCardPortIoOutW(card->hw, DivasIOBase, REG_ADDRLO, 18);
	UxCardPortIoOut(card->hw, DivasIOBase, REG_DATA, config->low_channel);

	UxCardPortIoOutW(card->hw, DivasIOBase, REG_ADDRLO, 19);
	UxCardPortIoOut(card->hw, DivasIOBase, REG_DATA, config->prot_version);

	UxCardPortIoOutW(card->hw, DivasIOBase, REG_ADDRLO, 20);
	UxCardPortIoOut(card->hw, DivasIOBase, REG_DATA, config->crc4);

	UxCardPortIoOutW(card->hw, DivasIOBase, REG_ADDRLO, 21);
	UxCardPortIoOut(card->hw, DivasIOBase, REG_DATA, 0);

	if ((card->hw->features) && (card->hw->features & PROTCAP_V90D))
	{
		DPRINTF(("divas: Signifying V.90"));
		UxCardPortIoOutW(card->hw, DivasIOBase, REG_ADDRLO, 22);
		UxCardPortIoOut(card->hw, DivasIOBase, REG_DATA, 4);
	}
	else
	{
		UxCardPortIoOutW(card->hw, DivasIOBase, REG_ADDRLO, 22);
		UxCardPortIoOut(card->hw, DivasIOBase, REG_DATA, 0);
	}

	UxCardPortIoOutW(card->hw, DivasIOBase, REG_ADDRLO, 23);
	UxCardPortIoOut(card->hw, DivasIOBase, REG_DATA, card->serial_no & 0xFF);

	UxCardPortIoOutW(card->hw, DivasIOBase, REG_ADDRLO, 24);
	UxCardPortIoOut(card->hw, DivasIOBase, REG_DATA, (card->serial_no >> 8) & 0xFF);

	UxCardPortIoOutW(card->hw, DivasIOBase, REG_ADDRLO, 25);
	UxCardPortIoOut(card->hw, DivasIOBase, REG_DATA, (card->serial_no >> 16) & 0xFF);

	UxCardPortIoOutW(card->hw, DivasIOBase, REG_ADDRLO, 26);
	UxCardPortIoOut(card->hw, DivasIOBase, REG_DATA, 21);

	for (i=0; i<32; i++)
	{
		UxCardPortIoOutW(card->hw, DivasIOBase, REG_ADDRLO, 32+i);
		UxCardPortIoOut(card->hw, DivasIOBase, REG_DATA, config->terminal[0].oad[i]);

		UxCardPortIoOutW(card->hw, DivasIOBase, REG_ADDRLO, 64+i);
		UxCardPortIoOut(card->hw, DivasIOBase, REG_DATA, config->terminal[0].osa[i]);

		UxCardPortIoOutW(card->hw, DivasIOBase, REG_ADDRLO, 96+i);
		UxCardPortIoOut(card->hw, DivasIOBase, REG_DATA, config->terminal[0].spid[i]);


		UxCardPortIoOutW(card->hw, DivasIOBase, REG_ADDRLO, 128+i);
		UxCardPortIoOut(card->hw, DivasIOBase, REG_DATA, config->terminal[1].oad[i]);

		UxCardPortIoOutW(card->hw, DivasIOBase, REG_ADDRLO, 160+i);
		UxCardPortIoOut(card->hw, DivasIOBase, REG_DATA, config->terminal[1].osa[i]);

		UxCardPortIoOutW(card->hw, DivasIOBase, REG_ADDRLO, 192+i);
		UxCardPortIoOut(card->hw, DivasIOBase, REG_DATA, config->terminal[1].spid[i]);
	}

	UxCardMemDetach(card->hw, DivasIOBase);

	return 0;
}

void DivasBriPatch(card_t *card)
{
	dword	PLXIOBase = 0;
	dword	DivasIOBase = 0;

	PLXIOBase = card->cfg.reset_base;
	DivasIOBase = card->cfg.io_base;

	if(card->hw == NULL)
	{
		DPRINTF(("Divas: BRI PATCH (PLX chip) card->hw is null"));
		return;
	}

	if (PLXIOBase == 0)
	{
		DPRINTF(("Divas: BRI (PLX chip) cannot be patched. The BRI adapter may"));
		DPRINTF(("Divas:   not function properly. If you do encounter problems,"));
		DPRINTF(("Divas:   ensure that your machine is using the latest BIOS."));
		return;
	}

	DPRINTF(("Divas: PLX I/O Base 0x%x", PLXIOBase));
	DPRINTF(("Divas: Divas I/O Base 0x%x", DivasIOBase));

	if (PLXIOBase & 0x80)
	{
		dword dwSize, dwSerialNum, dwCmd;
		boolean_t bPLX9060;
		word wSerHi, wSerLo;

		DPRINTF(("Divas: Patch required"));
		dwCmd = 0;
		UxPciConfigWrite(card->hw, 4, PCI_COMMAND, &dwCmd);

		PLXIOBase &= ~0x80;
		UxPciConfigWrite(card->hw, 4, PCI_BADDR1, &PLXIOBase);

		dwSize = 0xFFFFFFFF;
		UxPciConfigWrite(card->hw, 4, PCI_BADDR1, &dwSize);
		UxPciConfigRead(card->hw, 4, PCI_BADDR1, &dwSize);
		
		dwSize = (~ (dwSize & ~7)) + 1;

		DivasIOBase = PLXIOBase + dwSize;

		card->cfg.reset_base = PLXIOBase;
		card->cfg.io_base = DivasIOBase;
		UxPciConfigWrite(card->hw, 4, PCI_BADDR1, &card->cfg.reset_base);
		UxPciConfigWrite(card->hw, 4, PCI_BADDR2, &card->cfg.io_base);

		dwCmd = 5;
		UxPciConfigWrite(card->hw, 4, PCI_COMMAND, &dwCmd);

		bPLX9060 = UxCardPortIoInW(card->hw, (void *) card->cfg.reset_base, 0x6C) | 
			   UxCardPortIoInW(card->hw, (void *) card->cfg.reset_base, 0x6E);

		if (bPLX9060)
		{
			wSerHi = UxCardPortIoInW(card->hw, (void *) card->cfg.reset_base, 0x1E);
			wSerLo = UxCardPortIoInW(card->hw, (void *) card->cfg.reset_base, 0x22);
			dwSerialNum = (wSerHi << 16) | wSerLo;
			UxCardLog(0);
		}
		else
		{
			wSerHi = UxCardPortIoInW(card->hw, (void *) card->cfg.reset_base, 0x22);
			wSerLo = UxCardPortIoInW(card->hw, (void *) card->cfg.reset_base, 0x26);
			dwSerialNum = (wSerHi << 16) | wSerLo;
			UxCardLog(0);
		}
	}
	else
	{
		word wSerHi, wSerLo;
		boolean_t bPLX9060;
		dword dwSerialNum;

		DPRINTF(("divas: No patch required"));

		bPLX9060 = UxCardPortIoInW(card->hw, (void *) card->cfg.reset_base, 0x6C) | 
			   UxCardPortIoInW(card->hw, (void *) card->cfg.reset_base, 0x6E);

		if (bPLX9060)
		{
			wSerHi = UxCardPortIoInW(card->hw, (void *) card->cfg.reset_base, 0x1E);
			wSerLo = UxCardPortIoInW(card->hw, (void *) card->cfg.reset_base, 0x22);
			dwSerialNum = (wSerHi << 16) | wSerLo;
		}
		else
		{
			wSerHi = UxCardPortIoInW(card->hw, (void *) card->cfg.reset_base, 0x22);
			wSerLo = UxCardPortIoInW(card->hw, (void *) card->cfg.reset_base, 0x26);
			dwSerialNum = (wSerHi << 16) | wSerLo;
		}
	}
	DPRINTF(("Divas: After patching:"));
	DPRINTF(("Divas: PLX I/O Base 0x%x", PLXIOBase));
	DPRINTF(("Divas: Divas I/O Base 0x%x", DivasIOBase));

}

#define TEST_INT_DIVAS_BRI	0x12
static
int	diva_server_bri_test_int(card_t *card)
{
	boolean_t bPLX9060 = FALSE;
	byte *PLXIOBase = NULL, *DivasIOBase = NULL;

	DPRINTF(("divas: test interrupt for Diva Server BRI card"));

	PLXIOBase = UxCardMemAttach(card->hw, PLX_IOBASE);

	bPLX9060 = UxCardPortIoInW(card->hw, PLXIOBase, 0x6C) || UxCardPortIoInW(card->hw, PLXIOBase, 0x6E);

	if (bPLX9060)
	{ /* PLX9060 */
		UxCardPortIoOut(card->hw, PLXIOBase, 0x69, 0x09);
	}
	else
	{ /* PLX9050 */
		UxCardPortIoOut(card->hw, PLXIOBase, 0x4C, 0x41);
	}

	card->test_int_pend = TEST_INT_DIVAS_BRI;

	UxCardMemDetach(card->hw, PLXIOBase);

	DivasIOBase = UxCardMemAttach(card->hw, DIVAS_IOBASE);

	UxCardPortIoOut(card->hw, DivasIOBase, REG_IOCTRL, 0x89);

	UxCardMemDetach(card->hw, DivasIOBase);
	
	return 0;
}

static
int diva_server_bri_mem_get(card_t *card, mem_block_t *mem_block)
{
	dword user_addr = mem_block->addr;
	word	length = 0;
	dword	addr;
	word	i;
	byte *DivasIOBase;

	DPRINTF(("divas: Retrieving memory from 0x%x", user_addr));

	DivasIOBase = UxCardMemAttach(card->hw, DIVAS_IOBASE);

	addr = user_addr;

	for (i=0; i < (16 * 8); i++)
	{
		addr = user_addr + i;

		UxCardPortIoOut(card->hw, DivasIOBase, REG_ADDRHI, addr >> 16);
		UxCardPortIoOutW(card->hw, DivasIOBase, REG_ADDRLO, (word) addr);

		mem_block->data[i] = UxCardPortIoIn(card->hw, DivasIOBase, REG_DATA);
		length++;
	}

	UxCardMemDetach(card->hw, DivasIOBase);

	return length;
}

int DivasBriInit(card_t *card, dia_card_t *cfg)
{
	DPRINTF(("divas: initialise Diva Server BRI card"));

	if (DivasBRIInitPCI(card, cfg) == -1)
	{
		return -1;
	}

	card->card_reset 		= diva_server_bri_reset;
	card->card_start 		= diva_server_bri_start;
	card->card_load  		= diva_server_bri_load;
	card->card_config		= diva_server_bri_config;
	card->reset_int 		= diva_server_bri_reset_int;
	card->card_mem_get 		= diva_server_bri_mem_get;

	card->xlog_offset 		= DIVAS_MAINT_OFFSET;

	card->out 			= DivasOut;
	card->test_int 			= DivasTestInt;
	card->dpc 			= DivasDpc;
	card->clear_int 		= DivasClearInt;
	card->card_isr 			= bri_ISR;

	card->a.ram_out 		= io_out;
	card->a.ram_outw 		= io_outw;
	card->a.ram_out_buffer 	= io_out_buffer;
	card->a.ram_inc 		= io_inc;

	card->a.ram_in 			= io_in;
	card->a.ram_inw 		= io_inw;
	card->a.ram_in_buffer 	= io_in_buffer;
	card->a.ram_look_ahead	= io_look_ahead;

	return 0;
}

word GetProtFeatureValue(char *sw_id)
{
	word features = 0;

	while ((*sw_id) && (sw_id[0] != '['))
		sw_id++;

	if (sw_id == NULL)
	{
		DPRINTF(("divas: no feature string present"));
		features = -1;
	}
	else
	{
		byte i, shifter;

		sw_id += 3;

		for (i=0, shifter=12; i<4; i++, shifter-=4)
		{
			if ((sw_id[i] >= '0') && (sw_id[i] <= '9'))
			{
				features |= (sw_id[i] - '0') << shifter;
			}
			else if ((sw_id[i] >= 'a') && (sw_id[i] <= 'f'))
			{
				features |= (sw_id[i] - 'a' + 10) << shifter;
			}
			else if ((sw_id[i] >= 'A') && (sw_id[i] <= 'F'))
			{
				features |= (sw_id[i] - 'A' + 10) << shifter;
			}
			else
			{
				DPRINTF(("divas: invalid feature string"));
				return -1;
			}
		}
	}

	return features;
}


int bri_ISR (card_t* card) 
{
	int served = 0;
	byte *DivasIOBase = UxCardMemAttach(card->hw, DIVAS_IOBASE);

	if (UxCardPortIoIn (card->hw, DivasIOBase, M_PCI_RESET) & 0x01) 
	{
		served = 1;
		card->int_pend  += 1;
		DivasDpcSchedule(); /* ISR DPC */
		UxCardPortIoOut (card->hw, DivasIOBase, M_PCI_RESET, 0x08);
	}

	UxCardMemDetach(card->hw, DivasIOBase);

	return (served != 0);
}


