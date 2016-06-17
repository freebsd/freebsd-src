/*
 * Diva Server 4BRI specific part of initialisation
 *
 * Copyright (C) Eicon Technology Corporation, 2000.
 *
 * Eicon File Revision :    1.7  
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
#include "constant.h"
#include "adapter.h"
#include "uxio.h"

#define TEST_INT_DIVAS_Q	0x13

#define	DIVAS_MAINT_OFFSET	0xff00	/* value for 4BRI card */
#define MQ_BOARD_DSP_OFFSET 0x00a00000
#define MQ_DSP1_ADDR_OFFSET 0x00000008
#define MQ_DSP_JUNK_OFFSET  0x00000400
#define MQ_DSP1_DATA_OFFSET 0x00000000
#define MQ_BOARD_ISAC_DSP_RESET  0x00800028
#define MQ_BREG_RISC  0x1200      /* RISC Reset */
#define MQ_ISAC_DSP_RESET 0x0028 /* ISAC and DSP reset address offset */
#define MQ_RISC_COLD_RESET_MASK         0x0001      /* RISC Cold reset                        */
#define MQ_RISC_WARM_RESET_MASK         0x0002      /* RISC Warm reset                        */
#define MQ_IRQ_REQ_ON                   0x1
#define MQ_IRQ_REQ_OFF                  0x0
#define MQ_BREG_IRQ_TEST                0x0608
#define PLX9054_INTCSR      0x69 
#define PLX9054_INT_ENA     0x09

#define DIVAS_IOBASE	0x01
#define M_PCI_RESET	0x10

byte mem_in(ADAPTER *a, void *adr);
word mem_inw(ADAPTER *a, void *adr);
void mem_in_buffer(ADAPTER *a, void *adr, void *P, word length);
void mem_look_ahead(ADAPTER *a, PBUFFER *RBuffer, ENTITY *e);
void mem_out(ADAPTER *a, void *adr, byte data);
void mem_outw(ADAPTER *a, void *adr, word data);
void mem_out_buffer(ADAPTER *a, void *adr, void *P, word length);
void mem_inc(ADAPTER *a, void *adr);

int Divas4BRIInitPCI(card_t *card, dia_card_t *cfg);
static int fourbri_ISR (card_t* card);

int FPGA_Download(word, dword, byte *, byte *, int);
extern byte FPGA_Bytes[];
extern void *get_card(int);

byte UxCardPortIoIn(ux_diva_card_t *card, byte *base, int offset);
void UxCardPortIoOut(ux_diva_card_t *card, byte *base, int offset, byte);
word GetProtFeatureValue(char *sw_id);

void memcp(byte *dst, byte *src, dword dwLen);
int memcm(byte *dst, byte *src, dword dwLen);

static int diva_server_4bri_reset(card_t *card)
{
	byte *ctl;

	DPRINTF(("divas: reset Diva Server 4BRI"));

	ctl = UxCardMemAttach(card->hw, DIVAS_CTL_MEMORY);

	/* stop RISC, DSP's and ISAC  */
   UxCardMemOut(card->hw, &ctl[MQ_BREG_RISC], 0);
   UxCardMemOut(card->hw, &ctl[MQ_ISAC_DSP_RESET], 0);

	UxCardMemDetach(card->hw, ctl);

	return 0;
}

static int diva_server_4bri_config(card_t *card, dia_config_t *config)
{
	byte *shared;
	int i, j;

	DPRINTF(("divas: configure Diva Server 4BRI"));

	shared = (byte *) UxCardMemAttach(card->hw, DIVAS_SHARED_MEMORY);
	
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

	if ((card->hw->features) && (card->hw->features & PROTCAP_V90D))
	{
		DPRINTF(("divas: Signifying V.90"));
		UxCardMemOut(card->hw, &shared[22], 4);
	}
	else
	{
		UxCardMemOut(card->hw, &shared[22], 0);
	}

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
void diva_server_4bri_reset_int(card_t *card)
{
	byte *ctl;

	ctl = UxCardMemAttach(card->hw, DIVAS_CTL_MEMORY);

	UxCardMemOut(card->hw, &ctl[MQ_BREG_IRQ_TEST], MQ_IRQ_REQ_OFF);

	UxCardMemDetach(card->hw, ctl);

	return;
}

 
static int diva_server_4bri_test_int(card_t *card)
{
	byte *ctl, i;
	byte *reg;

	DPRINTF(("divas: test interrupt for Diva Server 4BRI"));

	/* We get the last (dummy) adapter in so we need to go back to the first */

	card = get_card(card->cfg.card_id - 3);

	/* Enable interrupts on PLX chip */

	reg = UxCardMemAttach(card->hw, DIVAS_REG_MEMORY);

	UxCardPortIoOut(card->hw, reg, PLX9054_INTCSR, PLX9054_INT_ENA);

	UxCardMemDetach(card->hw, reg);

	/* Set the test interrupt flag */
	card->test_int_pend = TEST_INT_DIVAS_Q;

	/* Now to trigger the interrupt */

	ctl = UxCardMemAttach(card->hw, DIVAS_CTL_MEMORY);

	UxCardMemOut(card->hw, &ctl[MQ_BREG_IRQ_TEST], MQ_IRQ_REQ_ON);

	UxCardMemDetach(card->hw, ctl);

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

static int diva_server_4bri_load(card_t *card, dia_load_t *load)
{
	byte *pRAM=NULL;
	int download_offset=0;
	card_t *FirstCard;
	byte sw_id[80];

	DPRINTF(("divas: loading Diva Server 4BRI[%d]", load->card_id));

	switch(load->code_type)
	{
		case DIA_CPU_CODE:
			DPRINTF(("divas: RISC code"));
			print_hdr(load->code, 0x80);
			card->hw->features = GetProtFeatureValue((char *)&load->code[0x80]);
			download_offset = 0; // Protocol code written to offset 0
			pRAM = UxCardMemAttach(card->hw, DIVAS_RAM_MEMORY);
			break;

		case DIA_DSP_CODE:
			DPRINTF(("divas: DSP code"));
			print_hdr(load->code, 0x0); 
			FirstCard = get_card(load->card_id - 3);
			if ((card->hw->features) && (card->hw->features & PROTCAP_V90D))
			{
				download_offset = MQ_V90D_DSP_CODE_BASE;
			}
			else
			{
				download_offset = MQ_ORG_DSP_CODE_BASE;
			}
			pRAM = UxCardMemAttach(FirstCard->hw, DIVAS_RAM_MEMORY);
			download_offset += (((sizeof(dword) + (sizeof(t_dsp_download_desc)* DSP_MAX_DOWNLOAD_COUNT)) + 3) & 0xFFFFFFFC);

			break;

		case DIA_TABLE_CODE:
			DPRINTF(("divas: TABLE code"));
			FirstCard = get_card(load->card_id - 3);
			if ((card->hw->features) && (card->hw->features & PROTCAP_V90D))
			{
				download_offset = MQ_V90D_DSP_CODE_BASE + sizeof(dword);
			}
			else
			{
				download_offset = MQ_ORG_DSP_CODE_BASE + sizeof(dword);
			}
			pRAM = UxCardMemAttach(FirstCard->hw, DIVAS_RAM_MEMORY);
			break;

		case DIA_CONT_CODE:
			DPRINTF(("divas: continuation code"));
			break;

        case DIA_DLOAD_CNT:
			DPRINTF(("divas: COUNT code"));
			FirstCard = get_card(load->card_id - 3);
			if ((card->hw->features) && (card->hw->features & PROTCAP_V90D))
			{
				download_offset = MQ_V90D_DSP_CODE_BASE;
			}
			else
			{
				download_offset = MQ_ORG_DSP_CODE_BASE;
			}
			pRAM = UxCardMemAttach(FirstCard->hw, DIVAS_RAM_MEMORY);
			break;

		case DIA_FPGA_CODE:
			DPRINTF(("divas: 4BRI FPGA download - %d bytes", load->length));
			if (FPGA_Download(IDI_ADAPTER_MAESTRAQ,
 			card->hw->io_base,
			 sw_id,
			 load->code,
			 load->length
			) == -1)
			{
				DPRINTF(("divas: FPGA download failed"));
				return -1;
			}

			/* NOW reset the 4BRI */
			diva_server_4bri_reset(card);
			return 0; // No need for anything further loading

		default:
			DPRINTF(("divas: unknown code type"));
			return -1;
	}

   memcp(pRAM + (download_offset & 0x3FFFFF), load->code, load->length);

	{
		int mism_off;
	if ((mism_off = memcm(pRAM + (download_offset & 0x3FFFFF), load->code, load->length)))
	{
		DPRINTF(("divas: memory mismatch at offset %d", mism_off));
		UxCardMemDetach(card->hw, pRAM);
		return -1;
	}
	}

	UxCardMemDetach(card->hw, pRAM);

	return 0;
}

static int diva_server_4bri_start(card_t *card, byte *channels)
{
	byte *ctl;
	byte *shared, i;
	int adapter_num;

	DPRINTF(("divas: start Diva Server 4BRI"));
	*channels = 0;
	card->is_live = FALSE;

	ctl = UxCardMemAttach(card->hw, DIVAS_CTL_MEMORY);

	UxCardMemOutW(card->hw, &ctl[MQ_BREG_RISC], MQ_RISC_COLD_RESET_MASK);

	UxPause(2);

	UxCardMemOutW(card->hw, &ctl[MQ_BREG_RISC], MQ_RISC_WARM_RESET_MASK | MQ_RISC_COLD_RESET_MASK);

	UxPause(10);
	
	UxCardMemDetach(card->hw, ctl);

	shared = (byte *) UxCardMemAttach(card->hw, DIVAS_SHARED_MEMORY);

	for ( i = 0 ; i < 300 ; ++i )
	{
		UxPause (10) ;

		if ( UxCardMemInW(card->hw, &shared[0x1E]) == 0x4447 )
		{
			DPRINTF(("divas: Protocol startup time %d.%02d seconds",
			         (i / 100), (i % 100) ));

			break;
		}
	}

	if (i==300)
	{
		DPRINTF(("divas: Timeout starting card"));
		DPRINTF(("divas: Signature == 0x%04X", UxCardMemInW(card->hw, &shared[0x1E])));

		UxCardMemDetach(card->hw, shared);
		return -1;
	}

	UxCardMemDetach(card->hw, shared);

	for (adapter_num=3; adapter_num >= 0; adapter_num--)
	{
		card_t *qbri_card;

		qbri_card = get_card(card->cfg.card_id - adapter_num);

		if (qbri_card)
		{
			qbri_card->is_live = TRUE;
			shared = UxCardMemAttach(qbri_card->hw, DIVAS_SHARED_MEMORY);
			*channels += UxCardMemIn(qbri_card->hw, &shared[0x3F6]);
			UxCardMemDetach(qbri_card->hw, shared);
		}
		else
		{
			DPRINTF(("divas: Couldn't get card info %d", card->cfg.card_id));
		}
	}

	diva_server_4bri_test_int(card);

	return 0;
}

static
int 	diva_server_4bri_mem_get(card_t *card, mem_block_t *mem_block)

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
 * Initialise 4BRI specific entry points
 */

int Divas4BriInit(card_t *card, dia_card_t *cfg)
{
//	byte sw_id[80];
//	extern int FPGA_Done;

	DPRINTF(("divas: initialise Diva Server 4BRI"));

	if (Divas4BRIInitPCI(card, cfg) == -1)
	{
		return -1;
	}

	/* Need to download the FPGA */
/*	if (!FPGA_Done)
	{
		int retVal;

		retVal=FPGA_Download(IDI_ADAPTER_MAESTRAQ,
 			cfg->io_base,
			 sw_id,
			 FPGA_Bytes
			);
		if(retVal==-1)
		{
		
			DPRINTF(("divas: FPGA Download Failed"));
			return -1;

		}
		FPGA_Done = 1;
	} */

	card->card_reset = diva_server_4bri_reset;
	card->card_load = diva_server_4bri_load;
	card->card_config = diva_server_4bri_config;
	card->card_start = diva_server_4bri_start;
	card->reset_int = diva_server_4bri_reset_int;
	card->card_mem_get = diva_server_4bri_mem_get;

	card->xlog_offset = DIVAS_MAINT_OFFSET;

	card->out = DivasOut;
	card->test_int = DivasTestInt;
	card->dpc = DivasDpc;
	card->clear_int = DivasClearInt;
	card->card_isr = fourbri_ISR;

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

void memcp(byte *dst, byte *src, dword dwLen)
{
	while (dwLen)
	{
		*dst = *src;
		dst++; src++;
		dwLen--;
	}
}

int memcm(byte *dst, byte *src, dword dwLen)
{
	int offset = 0;

	while (offset < dwLen)
	{
		if(*dst != *src)
			return (offset+1);

		offset++;
		src++;
		dst++;
	}

	return 0;
}



/*int fourbri_ISR (card_t* card) 
{
	int served = 0;
	byte *DivasIOBase = UxCardMemAttach(card->hw, DIVAS_IOBASE);
	

	if (UxCardPortIoIn (card->hw, DivasIOBase, M_PCI_RESET) & 0x01) 
	{
		served = 1;
		card->int_pend  += 1;
		DivasDpcSchedule(); 
		UxCardPortIoOut (card->hw, DivasIOBase, M_PCI_RESET, 0x08);
	}

	UxCardMemDetach(card->hw, DivasIOBase);

	return (served != 0);
}*/


static int fourbri_ISR (card_t* card) 
{
	byte *ctl;

	card->int_pend  += 1;
	DivasDpcSchedule(); /* ISR DPC */

	ctl = UxCardMemAttach(card->hw, DIVAS_CTL_MEMORY);
	UxCardMemOut(card->hw, &ctl[MQ_BREG_IRQ_TEST], MQ_IRQ_REQ_OFF);
	UxCardMemDetach(card->hw, ctl);

	return (1);
}
