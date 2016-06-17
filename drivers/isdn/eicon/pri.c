/*
 * Diva Server PRI specific part of initialisation
 *
 * Copyright (C) Eicon Technology Corporation, 2000.
 *
 * Eicon File Revision :    1.5  
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#include "sys.h"
#include "idi.h"
#include "divas.h"
#include "pc.h"
#include "pr_pc.h"
#include "dsp_defs.h"

#include "adapter.h"
#include "uxio.h"

#define	DIVAS_LOAD_CMD		0x02
#define	DIVAS_START_CMD		0x03
#define	DIVAS_IRQ_RESET		0xC18
#define DIVAS_IRQ_RESET_VAL	0xFE

#define	TEST_INT_DIVAS		0x11
#define TEST_INT_DIVAS_BRI	0x12

#define DIVAS_RESET	0x81
#define DIVAS_LED1	0x04
#define DIVAS_LED2	0x08
#define DIVAS_LED3	0x20
#define DIVAS_LED4	0x40

#define	DIVAS_RESET_REG		0x20

#define	DIVAS_SIGNATURE	0x4447

/* offset to start of MAINT area (used by xlog) */

#define	DIVAS_MAINT_OFFSET	0xef00	/* value for PRI card */

#define MP_PROTOCOL_ADDR		0xA0011000
#define MP_DSP_CODE_BASE		0xa03a0000  

typedef struct {
		dword cmd;
		dword addr;
		dword len;
		dword err;
		dword live;
		dword reserved[(0x1020>>2)-6];
		dword signature;
		byte  data[1];
} diva_server_boot_t;

byte mem_in(ADAPTER *a, void *adr);
word mem_inw(ADAPTER *a, void *adr);
void mem_in_buffer(ADAPTER *a, void *adr, void *P, word length);
void mem_look_ahead(ADAPTER *a, PBUFFER *RBuffer, ENTITY *e);
void mem_out(ADAPTER *a, void *adr, byte data);
void mem_outw(ADAPTER *a, void *adr, word data);
void mem_out_buffer(ADAPTER *a, void *adr, void *P, word length);
void mem_inc(ADAPTER *a, void *adr);

int DivasPRIInitPCI(card_t *card, dia_card_t *cfg);
static int pri_ISR (card_t* card);

static int diva_server_reset(card_t *card)
{
	byte *reg;
	diva_server_boot_t *boot = NULL;
	dword live = 0;
	int	i = 0;
	dword	dwWait;

	DPRINTF(("divas: reset Diva Server PRI"));

	reg = UxCardMemAttach(card->hw, DIVAS_REG_MEMORY);

	UxCardMemOut(card->hw, &reg[DIVAS_RESET_REG], DIVAS_RESET | 
						DIVAS_LED1 | DIVAS_LED2 | DIVAS_LED3 | DIVAS_LED4);

	for (dwWait = 0x000fffff; dwWait; dwWait--)
		;

	UxCardMemOut(card->hw, &reg[DIVAS_RESET_REG], 0x00);

	for (dwWait = 0x000fffff; dwWait; dwWait--)
		;

	UxCardMemDetach(card->hw, reg);

	boot = UxCardMemAttach(card->hw, DIVAS_RAM_MEMORY);

	UxCardMemOutD(card->hw, boot->reserved, 0);

	live = UxCardMemInD(card->hw, &boot->live);

	for (i=0; i<5; i++)
	{
		if (live != UxCardMemInD(card->hw, &boot->live))
		{
			break;
		}
		UxPause(10);
	}

	if (i == 5)
	{
		UxCardMemDetach(card->hw, boot);

		DPRINTF(("divas: card is reset but CPU not running"));
		return -1;
	}

	UxCardMemDetach(card->hw, boot);

	DPRINTF(("divas: card reset after %d ms", i * 10));

	return 0;
}

static int diva_server_config(card_t *card, dia_config_t *config)
{
	byte *shared;
	int i, j;

	DPRINTF(("divas: configure Diva Server PRI"));

	shared = UxCardMemAttach(card->hw, DIVAS_SHARED_MEMORY);

	UxCardLog(0);
	for (i=0; i<256; i++)
	{
		UxCardMemOut(card->hw, &shared[i], 0);
	}

	UxCardMemOut(card->hw, &shared[ 8], config->tei);
	UxCardMemOut(card->hw, &shared[ 9], config->nt2);
	UxCardMemOut(card->hw, &shared[10], config->sig_flags);
	UxCardMemOut(card->hw, &shared[11], config->watchdog);
	UxCardMemOut(card->hw, &shared[12], config->permanent);
	UxCardMemOut(card->hw, &shared[13], config->x_interface);
	UxCardMemOut(card->hw, &shared[14], config->stable_l2);
	UxCardMemOut(card->hw, &shared[15], config->no_order_check);
	UxCardMemOut(card->hw, &shared[16], config->handset_type);
	UxCardMemOut(card->hw, &shared[17], 0);
	UxCardMemOut(card->hw, &shared[18], config->low_channel);
	UxCardMemOut(card->hw, &shared[19], config->prot_version);
	UxCardMemOut(card->hw, &shared[20], config->crc4);

	for (i=0; i<2; i++)
	{
		for (j=0; j<32; j++)
		{
			UxCardMemOut(card->hw, &shared[32+(i*96)+j],config->terminal[i].oad[j]);
		}

		for (j=0; j<32; j++)
		{
			UxCardMemOut(card->hw, &shared[64+(i*96)+j],config->terminal[i].osa[j]);
		}

		for (j=0; j<32; j++)
		{
			UxCardMemOut(card->hw, &shared[96+(i*96)+j],config->terminal[i].spid[j]);
		}
	}

	UxCardMemDetach(card->hw, shared);

	return 0;
}

static
void diva_server_reset_int(card_t *card)
{
	byte *cfg;

	cfg = UxCardMemAttach(card->hw, DIVAS_CFG_MEMORY);

	UxCardMemOutW(card->hw, &cfg[DIVAS_IRQ_RESET], DIVAS_IRQ_RESET_VAL);
	UxCardMemOutW(card->hw, &cfg[DIVAS_IRQ_RESET + 2], 0);
	UxCardMemDetach(card->hw, cfg);

	return;
}

 
static int diva_server_test_int(card_t *card)
{
	int i;
	byte *shared;
	byte req_int;

	DPRINTF(("divas: test interrupt for Diva Server PRI"));

	shared = UxCardMemAttach(card->hw, DIVAS_SHARED_MEMORY);

	UxCardMemIn(card->hw, &shared[0x3FE]);
	UxCardMemOut(card->hw, &shared[0x3FE], 0);
	UxCardMemIn(card->hw, &shared[0x3FE]);

	UxCardMemDetach(card->hw, shared);

	diva_server_reset_int(card);

	shared = UxCardMemAttach(card->hw, DIVAS_SHARED_MEMORY);

	card->test_int_pend = TEST_INT_DIVAS;

	req_int = UxCardMemIn(card->hw, &(((struct pr_ram *)shared)->ReadyInt));

	req_int++;

	UxCardMemOut(card->hw, &(((struct pr_ram *)shared)->ReadyInt), req_int);

	UxCardMemDetach(card->hw, shared);

	UxCardLog(0);
	for (i = 0; i < 50; i++)
	{
		if (!card->test_int_pend)
		{
			break;
		}
		UxPause(10);
	}


	if (card->test_int_pend)
	{

		DPRINTF(("active: timeout waiting for card to interrupt"));
		return (-1);
	
	}
	
	return 0;
}


static void print_hdr(unsigned char *code, int offset)
{
	unsigned char hdr[80];
		int i;

	i = 0;

	while ((i < (DIM(hdr) -1)) && 
		(code[offset + i] != '\0') &&
		(code[offset + i] != '\r') &&
		(code[offset + i] != '\n'))
	{
		hdr[i] = code[offset + i];
		i++;
	}

	hdr[i] = '\0';

	DPRINTF(("divas: loading %s", hdr));
}

static int diva_server_load(card_t *card, dia_load_t *load)
{
	diva_server_boot_t *boot;
	int i, offset, length;
	dword cmd = 0;

	DPRINTF(("divas: loading Diva Server PRI"));

	boot = UxCardMemAttach(card->hw, DIVAS_RAM_MEMORY);

	switch(load->code_type)
	{
		case DIA_CPU_CODE:
			DPRINTF(("divas: RISC code"));
			print_hdr(load->code, 0x80);

			UxCardMemOutD(card->hw, &boot->addr, MP_PROTOCOL_ADDR);
			break;

		case DIA_DSP_CODE:
			DPRINTF(("divas: DSP code"));
			print_hdr(load->code, 0x0);

			UxCardMemOutD(card->hw, &boot->addr,  
				(MP_DSP_CODE_BASE + (((sizeof(dword) +
				(sizeof(t_dsp_download_desc) * DSP_MAX_DOWNLOAD_COUNT))
				+ ~ALIGNMENT_MASK_MAESTRA) & ALIGNMENT_MASK_MAESTRA)));
			break;

		case DIA_TABLE_CODE:
			DPRINTF(("divas: TABLE code"));
			UxCardMemOutD(card->hw, &boot->addr,
				(MP_DSP_CODE_BASE + sizeof(dword)));
			break;

		case DIA_CONT_CODE:
			DPRINTF(("divas: continuation code"));
			break;

        case DIA_DLOAD_CNT:
			DPRINTF(("divas: COUNT code"));
			UxCardMemOutD(card->hw, &boot->addr, MP_DSP_CODE_BASE);
			break;

		default:
			DPRINTF(("divas: unknown code type"));
			UxCardMemDetach(card->hw, boot);
			return -1;
	}

	UxCardLog(0);
	offset = 0;

	do
	{
		length = (load->length - offset >= 400) ? 400 : load->length - offset;

		for (i=0; i<length; i++)
		{
			UxCardMemOut(card->hw, &boot->data[i], load->code[offset+i]);
		}

        for (i=0; i<length; i++)
		{
			if (load->code[offset + i] != UxCardMemIn(card->hw, &boot->data[i]))
			{
				UxCardMemDetach(card->hw, boot);

				DPRINTF(("divas: card code block verify failed"));
				return -1;
			}
		}
	
		UxCardMemOutD(card->hw, &boot->len, (length + 3) / 4);
		UxCardMemOutD(card->hw, &boot->cmd, DIVAS_LOAD_CMD);

		for (i=0; i<50000; i++)
		{
			cmd = UxCardMemInD(card->hw, &boot->cmd);
			if (!cmd)
			{
				break;
			}
			/*UxPause(1);*/
		}

		if (cmd)
		{
			DPRINTF(("divas: timeout waiting for card to ACK load (offset = %d)", offset));
			UxCardMemDetach(card->hw, boot);
			return -1;
		}

		offset += length;

	} while (offset < load->length);

	UxCardMemDetach(card->hw, boot);

	DPRINTF(("divas: DIVA Server card loaded"));

	return 0;
}

static int diva_server_start(card_t *card, byte *channels)
{
	diva_server_boot_t *boot;
	byte *ram;
	int	i;
	dword signature = 0;

	DPRINTF(("divas: start Diva Server PRI"));

	card->is_live = FALSE;

	boot = UxCardMemAttach(card->hw, DIVAS_RAM_MEMORY);

	UxCardMemOutD(card->hw, &boot->addr, MP_PROTOCOL_ADDR);
	UxCardMemOutD(card->hw, &boot->cmd, DIVAS_START_CMD);

	UxCardLog(0);

	for (i = 0; i < 300; i++)
	{
		signature = UxCardMemInD(card->hw, &boot->signature);
		if ((signature >> 16) == DIVAS_SIGNATURE)
		{
			DPRINTF(("divas: started card after %d ms", i * 10));
			break;
		}
		UxPause(10);
	}

	if ((signature >> 16) != DIVAS_SIGNATURE)
	{
		UxCardMemDetach(card->hw, boot);
		DPRINTF(("divas: timeout waiting for card to run protocol code (sig = 0x%x)", signature));
		return -1;
	}

	card->is_live = TRUE;

	ram = (byte *) boot;
	ram += DIVAS_SHARED_OFFSET;

	*channels = UxCardMemIn(card->hw, &ram[0x3F6]);
	card->serial_no = UxCardMemInD(card->hw, &ram[0x3F0]);

	UxCardMemDetach(card->hw, boot);

	if (diva_server_test_int(card))
	{
		DPRINTF(("divas: interrupt test failed"));
		return -1;	
	}

	DPRINTF(("divas: DIVA Server card started"));

	return 0;
}

static
int 	diva_server_mem_get(card_t *card, mem_block_t *mem_block)

{
	byte	*a;
	byte	*card_addr;
	word	length = 0;
	int		i;

	a = UxCardMemAttach(card->hw, DIVAS_RAM_MEMORY);

	card_addr = a;
	card_addr += mem_block->addr;

	for (i=0; i < sizeof(mem_block->data); i++)
	{
		mem_block->data[i] = UxCardMemIn(card->hw, card_addr);
		card_addr++;
		length++;
	}

	UxCardMemDetach(card->hw, a);

	return length;
}

/*
 * Initialise PRI specific entry points
 */

int DivasPriInit(card_t *card, dia_card_t *cfg)
{
	DPRINTF(("divas: initialise Diva Server PRI"));

	if (DivasPRIInitPCI(card, cfg) == -1)
	{
		return -1;
	}

	card->card_reset = diva_server_reset;
	card->card_load = diva_server_load;
	card->card_config = diva_server_config;
	card->card_start = diva_server_start;
	card->reset_int = diva_server_reset_int;
	card->card_mem_get = diva_server_mem_get;

	card->xlog_offset = DIVAS_MAINT_OFFSET;

	card->out = DivasOut;
	card->test_int = DivasTestInt;
	card->dpc = DivasDpc;
	card->clear_int = DivasClearInt;
	card->card_isr  = pri_ISR;

	card->a.ram_out = mem_out;
	card->a.ram_outw = mem_outw;
	card->a.ram_out_buffer = mem_out_buffer;
	card->a.ram_inc = mem_inc;

	card->a.ram_in = mem_in;
	card->a.ram_inw = mem_inw;
	card->a.ram_in_buffer = mem_in_buffer;
	card->a.ram_look_ahead = mem_look_ahead;

	return 0;
}


static int pri_ISR (card_t* card) 
{
	int served = 0;
	byte* cfg = UxCardMemAttach(card->hw, DIVAS_CFG_MEMORY);
	volatile unsigned long* isr = (unsigned long*)&cfg[DIVAS_IRQ_RESET];
	register unsigned long val = *isr;
	
	if (val & 0x80000000)  /* our card had caused interrupt ??? */
	{
		served = 1;
		card->int_pend  += 1;
		DivasDpcSchedule(); /* ISR DPC */

		*isr = (unsigned long)~0x03E00000; /* Clear interrupt line */
	}

	UxCardMemDetach(card->hw, cfg);

	return (served != 0);
}


