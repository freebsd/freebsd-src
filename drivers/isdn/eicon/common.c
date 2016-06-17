/*
 * Copyright (C) Eicon Technology Corporation, 2000.
 *
 * Eicon File Revision :    1.15  
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#include "eicon.h"
#include "sys.h"
#include "idi.h"
#include "constant.h"
#include "divas.h"
#include "pc.h"
#include "pr_pc.h"

#include "uxio.h"

#define DIVAS_LOAD_CMD		0x02
#define DIVAS_START_CMD		0x03
#define DIVAS_IRQ_RESET		0xC18
#define DIVAS_IRQ_RESET_VAL	0xFE

#define TEST_INT_DIVAS		0x11
#define TEST_INT_DIVAS_BRI	0x12
#define TEST_INT_DIVAS_Q	0x13

#define DIVAS_RESET	0x81
#define DIVAS_LED1	0x04
#define DIVAS_LED2	0x08
#define DIVAS_LED3	0x20
#define DIVAS_LED4	0x40

#define DIVAS_SIGNATURE 0x4447

#define MP_PROTOCOL_ADDR 0xA0011000

#define PLX_IOBASE	0
#define	DIVAS_IOBASE	1

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

int		DivasCardNext;
card_t	DivasCards[MAX_CARDS];

dia_config_t *DivasConfig(card_t *, dia_config_t *);

static
DESCRIPTOR DIDD_Table[32];

void    DIVA_DIDD_Read( DESCRIPTOR *table, int tablelength )
{
        memset(table, 0, tablelength);

        if (tablelength > sizeof(DIDD_Table))
          tablelength = sizeof(DIDD_Table);

        if(tablelength % sizeof(DESCRIPTOR)) {
          tablelength /= sizeof(DESCRIPTOR);
          tablelength *= sizeof(DESCRIPTOR);
        }

        if (tablelength > 0)
          memcpy((void *)table, (void *)DIDD_Table, tablelength);

	return;
}

void 	DIVA_DIDD_Write(DESCRIPTOR *table, int tablelength)
{
        if (tablelength > sizeof(DIDD_Table))
          tablelength = sizeof(DIDD_Table);

	memcpy((void *)DIDD_Table, (void *)table, tablelength);

	return;
}

static
void    init_idi_tab(void)
{
    DESCRIPTOR d[32];

    memset(d, 0, sizeof(d));

    d[0].type = IDI_DIMAINT;  /* identify the DIMAINT entry */
    d[0].channels = 0; /* zero channels associated with dimaint*/
    d[0].features = 0; /* no features associated with dimaint */
    d[0].request = (IDI_CALL) DivasPrintf;
    
    DIVA_DIDD_Write(d, sizeof(d));

    return;
}

/*
 * I/O routines for memory mapped cards
 */

byte mem_in(ADAPTER *a, void *adr)
{
	card_t			*card = a->io;
	unsigned char	*b, *m;
	byte			value;

	m = b = UxCardMemAttach(card->hw, DIVAS_SHARED_MEMORY);

	m += (unsigned int) adr;

	value = UxCardMemIn(card->hw, m);

	UxCardMemDetach(card->hw, b);

	return value;
}

word mem_inw(ADAPTER *a, void *adr)
{
	card_t			*card = a->io;
	unsigned char	*b, *m;
	word			value;

	m = b = UxCardMemAttach(card->hw, DIVAS_SHARED_MEMORY);

	m += (unsigned int) adr;

	value = UxCardMemInW(card->hw, m);

	UxCardMemDetach(card->hw, b);

	return value;
}

void mem_in_buffer(ADAPTER *a, void *adr, void *P, word length)
{
	card_t			*card = a->io;
	unsigned char	*b, *m;

	m = b = UxCardMemAttach(card->hw, DIVAS_SHARED_MEMORY);

	m += (unsigned int) adr;

	UxCardMemInBuffer(card->hw, m, P, length);

	UxCardMemDetach(card->hw, b);

	return;
}

void mem_look_ahead(ADAPTER *a, PBUFFER *RBuffer, ENTITY *e)
{
	card_t			*card = a->io;
	unsigned char	*b, *m;

	m = b = UxCardMemAttach(card->hw, DIVAS_SHARED_MEMORY);

	m += (dword) &RBuffer->length;
	card->RBuffer.length = UxCardMemInW(card->hw, m);

	m = b;
	m += (dword) &RBuffer->P;
	UxCardMemInBuffer(card->hw, m, card->RBuffer.P, card->RBuffer.length);

	e->RBuffer = (DBUFFER *) &card->RBuffer;

	UxCardMemDetach(card->hw, b);

	return;
}

void mem_out(ADAPTER *a, void *adr, byte data)
{
	card_t			*card = a->io;
	unsigned char	*b, *m;

	m = b = UxCardMemAttach(card->hw, DIVAS_SHARED_MEMORY);

	m += (unsigned int) adr;

	UxCardMemOut(card->hw, m, data);

	UxCardMemDetach(card->hw, b);

	return;
}

void mem_outw(ADAPTER *a, void *adr, word data)
{
	card_t			*card = a->io;
	unsigned char	*b, *m;

	m = b = UxCardMemAttach(card->hw, DIVAS_SHARED_MEMORY);

	m += (unsigned int) adr;

	UxCardMemOutW(card->hw, m, data);

	UxCardMemDetach(card->hw, b);

	return;
}

void mem_out_buffer(ADAPTER *a, void *adr, void *P, word length)
{
	card_t			*card = a->io;
	unsigned char	*b, *m;

	m = b = UxCardMemAttach(card->hw, DIVAS_SHARED_MEMORY);

	m += (unsigned int) adr;

	UxCardMemOutBuffer(card->hw, m, P, length);

	UxCardMemDetach(card->hw, b);

	return;
}

void mem_inc(ADAPTER *a, void *adr)
{
	word			value;
	card_t			*card = a->io;
	unsigned char	*b, *m;

	m = b = UxCardMemAttach(card->hw, DIVAS_SHARED_MEMORY);

	m += (unsigned int) adr;

	value = UxCardMemInW(card->hw, m);
	value++;
	UxCardMemOutW(card->hw, m, value);

	UxCardMemDetach(card->hw, b);

	return;
}

/*
 * I/O routines for I/O mapped cards
 */

byte io_in(ADAPTER *a, void *adr)
{
	card_t		    *card = a->io;
	byte		    value;
	byte	*DivasIOBase = NULL;

	DivasIOBase = UxCardMemAttach(card->hw, DIVAS_IOBASE);

	value = UxCardIoIn(card->hw, DivasIOBase, adr);

	UxCardMemDetach(card->hw, DivasIOBase);

    return value;
}

word io_inw(ADAPTER *a, void *adr)
{
	card_t		*card = a->io;
	word		value;
	byte	*DivasIOBase = NULL;

	DivasIOBase = UxCardMemAttach(card->hw, DIVAS_IOBASE);

	value = UxCardIoInW(card->hw, DivasIOBase, adr);

	UxCardMemDetach(card->hw, DivasIOBase);

	return value;
}

void io_in_buffer(ADAPTER *a, void *adr, void *P, word length)
{
	card_t *card = a->io;
	byte *DivasIOBase = NULL;

	DivasIOBase = UxCardMemAttach(card->hw, DIVAS_IOBASE);

	UxCardIoInBuffer(card->hw, DivasIOBase, adr, P,length);

	UxCardMemDetach(card->hw, DivasIOBase);

    return;
}

void io_look_ahead(ADAPTER *a, PBUFFER *RBuffer, ENTITY *e)
{
	card_t *card = a->io;
	byte *DivasIOBase = NULL;

	DivasIOBase = UxCardMemAttach(card->hw, DIVAS_IOBASE);

	card->RBuffer.length = UxCardIoInW(card->hw, DivasIOBase, (byte *) RBuffer);

	UxCardIoInBuffer(card->hw, DivasIOBase, &RBuffer->P, card->RBuffer.P, card->RBuffer.length);

	UxCardMemDetach(card->hw, DivasIOBase);

	e->RBuffer = (DBUFFER *) &card->RBuffer;

    return;
}

void io_out(ADAPTER *a, void *adr, byte data)
{
	card_t		*card = a->io;
	byte	*DivasIOBase = NULL;

	DivasIOBase = UxCardMemAttach(card->hw, DIVAS_IOBASE);

	UxCardIoOut(card->hw, DivasIOBase, adr, data);

	UxCardMemDetach(card->hw, DivasIOBase);

    return;
}

void io_outw(ADAPTER *a, void *adr, word data)
{
	card_t		*card = a->io;
	byte	*DivasIOBase = NULL;

	DivasIOBase = UxCardMemAttach(card->hw, DIVAS_IOBASE);

	UxCardIoOutW(card->hw, DivasIOBase, adr, data);

	UxCardMemDetach(card->hw, DivasIOBase);

    return;
}

void io_out_buffer(ADAPTER *a, void *adr, void *P, word length)
{
	card_t		*card = a->io;
	byte *DivasIOBase = NULL;

	DivasIOBase = UxCardMemAttach(card->hw, DIVAS_IOBASE);

	UxCardIoOutBuffer(card->hw, DivasIOBase, adr, P, length);

	UxCardMemDetach(card->hw, DivasIOBase);

    return;
}

void io_inc(ADAPTER *a, void *adr)
{
	word		value;
	card_t		*card = a->io;
	byte *DivasIOBase;

	DivasIOBase = UxCardMemAttach(card->hw, DIVAS_IOBASE);

	value = UxCardIoInW(card->hw, DivasIOBase, adr);
	
	value++;

	UxCardIoOutW(card->hw, DivasIOBase, adr, value);

	UxCardMemDetach(card->hw, DivasIOBase);

    return;
}

static
void test_int(card_t *card)

{
	byte *shared, *DivasIOBase;

	switch (card->test_int_pend)
	{
		case TEST_INT_DIVAS:
			DPRINTF(("divas: test interrupt pending"));
			shared = UxCardMemAttach(card->hw, DIVAS_SHARED_MEMORY);

			if (UxCardMemIn(card->hw, &shared[0x3FE]))
			{
				UxCardMemOut(card->hw, 
								&(((struct pr_ram *)shared)->RcOutput), 0);
				UxCardMemDetach(card->hw, shared);
            	(*card->reset_int)(card);
				shared = UxCardMemAttach(card->hw, DIVAS_SHARED_MEMORY);
				UxCardMemOut(card->hw, &shared[0x3FE], 0);
				DPRINTF(("divas: test interrupt cleared"));
			}

			UxCardMemDetach(card->hw, shared);

			card->test_int_pend = 0;
			break;

		case TEST_INT_DIVAS_BRI:
			DPRINTF(("divas: BRI test interrupt pending"));
			(*card->reset_int)(card);
			DivasIOBase = UxCardMemAttach(card->hw, DIVAS_IOBASE);
			UxCardIoOutW(card->hw, DivasIOBase, (void *) 0x3FE, 0);
			UxCardMemDetach(card->hw, DivasIOBase);
			DPRINTF(("divas: test interrupt cleared"));
			card->test_int_pend = 0;
			break;

		case TEST_INT_DIVAS_Q:
			DPRINTF(("divas: 4BRI test interrupt pending"));
			(*card->reset_int)(card);
			card->test_int_pend = 0;
			break;

		default:
			DPRINTF(("divas: unknown test interrupt pending"));
			return;
	}
	return;
}

void card_isr (void *dev_id)
{
	card_t *card = (card_t *) dev_id;
	ADAPTER *a = &card->a;
	int ipl;

	if (card->test_int_pend)
	{
		ipl = UxCardLock(card->hw);
		card->int_pend=0;
		test_int(card);
		UxCardUnlock(card->hw,ipl);
		return;
	}
	
	if(card->card_isr)
	{
		(*(card->card_isr))(card);
	}
	else
	{
		ipl = UxCardLock(card->hw);
	
		if ((card->test_int)(a))
		{
			(card->reset_int)(card);
		}
		
		UxCardUnlock(card->hw,ipl);
		
	}

}

int DivasCardNew(dia_card_t *card_info)
{
	card_t *card;
	static boolean_t first_call = TRUE;
	boolean_t NeedISRandReset = FALSE;

	DPRINTF(("divas: new card "));

	if (first_call)
	{
		first_call = FALSE;
		init_idi_tab();
	}

	DivasConfigGet(card_info);
	
	if (DivasCardNext == DIM(DivasCards))
	{
		KDPRINTF((KERN_WARNING "Divas: no space available for new card"));
		return -1;
	}

	card = &DivasCards[DivasCardNext];

	card->state = DIA_UNKNOWN;

	card->cfg = *card_info;

	card->a.io = card;

	if (UxCardHandleGet(&card->hw, card_info))
	{
		KDPRINTF((KERN_WARNING "Divas: cannot get OS specific handle for card"));
		return -1;
	}

	if (card_info->card_type == DIA_CARD_TYPE_DIVA_SERVER_B)
	{
		DivasBriPatch(card);
		card_info->io_base = card->cfg.io_base;
	}

	switch (card_info->card_type)
	{
		case DIA_CARD_TYPE_DIVA_SERVER:
			if (DivasPriInit(card, card_info))
			{
				return -1;
			}
			NeedISRandReset = TRUE;
			break;

		case DIA_CARD_TYPE_DIVA_SERVER_B:
			if (DivasBriInit(card, card_info))
			{
				return -1;
			}
			NeedISRandReset = TRUE;
			break;

 		case DIA_CARD_TYPE_DIVA_SERVER_Q:
			if (Divas4BriInit(card, card_info))
			{
				return -1;
			}

			if (card_info->name[6] == '0')
			{
				NeedISRandReset = TRUE;
			}
			else // Need to set paramater for ISR anyway
			{
				card->hw->user_isr_arg = card;
				card->hw->user_isr = card_isr;
			}
			break;   

		default:
			KDPRINTF((KERN_WARNING "Divas: unsupported card type (%d)", card_info->card_type));
			return -1;
	}

	if (NeedISRandReset)
	{
		if (UxIsrInstall(card->hw, card_isr, card))
		{
			KDPRINTF((KERN_WARNING "Divas: Install ISR failed (IRQ %d)", card->cfg.irq));
			UxCardHandleFree(card->hw);
			return -1;
		}

		if (card_info->card_type != DIA_CARD_TYPE_DIVA_SERVER_Q)
		{
			if ((*card->card_reset)(card))
			{
				KDPRINTF((KERN_WARNING "Divas: Adapter reset failed"));
				return -1;
			}
			card->state = DIA_RESET;
		}

		NeedISRandReset = FALSE;
	}

	DivasCardNext++;

	return 0;
}

void	*get_card(int card_id)
{
	int i;

	for (i=0; i < DivasCardNext; i++)
	{
		if (DivasCards[i].cfg.card_id == card_id)
		{
			return(&DivasCards[i]);
		}
	}

	DPRINTF(("divas: get_card() : no such card id (%d)", card_id));

	return NULL;
}

int DivasCardConfig(dia_config_t *config)
{
	card_t *card;
	int status;

	DPRINTF(("divas: configuring card"));

	card = get_card(config->card_id);
	if (!card)
	{
		return -1;
	}

	config = DivasConfig(card, config);

	status = (*card->card_config)(card, config);

	if (!status)
	{
		card->state = DIA_CONFIGURED;
	}
	return status;
}

int DivasCardLoad(dia_load_t *load)
{
	card_t *card;
	int	status;

	card = get_card(load->card_id);
	if (!card)
	{
		return -1;
	}

	if (card->state == DIA_RUNNING)
	{
		(*card->card_reset)(card);
	}

	status = (*card->card_load)(card, load);
	if (!status)
	{
		card->state = DIA_LOADED;
	}
	return status;
}

static int idi_register(card_t *card, byte channels)
{
    DESCRIPTOR d[32];
    int length, num_entities;

	DPRINTF(("divas: registering card with IDI"));

	num_entities = (channels > 2) ? MAX_PENTITIES : MAX_ENTITIES;
	card->e_tbl = UxAlloc(sizeof(E_INFO) * num_entities);

	if (!card->e_tbl)
	{
		KDPRINTF((KERN_WARNING "Divas: IDI register failed - no memory available"));
		return -1;
	}

	memset(card->e_tbl, 0, sizeof(E_INFO) * num_entities);
	card->e_max = num_entities;

    DIVA_DIDD_Read(d, sizeof(d));

        for(length=0; length < DIM(d); length++)
          if (d[length].type == 0) break;

	if (length >= DIM(d))
	{
		KDPRINTF((KERN_WARNING "Divas: IDI register failed - table full"));
		return -1;
	}

	switch (card->cfg.card_type)
	{
		case DIA_CARD_TYPE_DIVA_SERVER:
		d[length].type = IDI_ADAPTER_PR;
		/* d[length].serial = card->serial_no; */
		break;

		case DIA_CARD_TYPE_DIVA_SERVER_B:
		d[length].type = IDI_ADAPTER_MAESTRA;
		/* d[length].serial = card->serial_no; */
		break;

		// 4BRI is treated as 4 BRI adapters
		case DIA_CARD_TYPE_DIVA_SERVER_Q:
		d[length].type = IDI_ADAPTER_MAESTRA;
		/* d[length].serial = card->cfg.serial; */
	}

	d[length].features = 0;
	d[length].features |= DI_FAX3|DI_MODEM|DI_POST|DI_V110|DI_V120;

	if ( card->hw->features & PROTCAP_MANIF )
	{
		d[length].features |= DI_MANAGE ;
	}
	if ( card->hw->features & PROTCAP_V_42 )
	{
		d[length].features |= DI_V_42 ;
	}
	if ( card->hw->features & PROTCAP_EXTD_FAX )
	{
		d[length].features |= DI_EXTD_FAX ;
	}

	d[length].channels = channels;
	d[length].request = DivasIdiRequest[card - DivasCards];

	length++;

	DIVA_DIDD_Write(d, sizeof(d));

    return 0;
}

int DivasCardStart(int card_id)
{
	card_t *card;
	byte channels;
	int status;

	DPRINTF(("divas: starting card"));

	card = get_card(card_id);
	if (!card)
	{
		return -1;
	}

	status = (*card->card_start)(card, &channels);
	if (status)
	{
		return status;
	}

	/* 4BRI == 4 x BRI so call idi_register 4 times each with 2 channels */
	if (card->cfg.card_type == DIA_CARD_TYPE_DIVA_SERVER_Q)
	{
		int i;
		card_t *FourBRISlave;

		for (i=3; i >= 0; i--)
		{
			FourBRISlave = get_card(card_id - i); /* 0, 1, 2, 3 */
			if (FourBRISlave)
			{
				idi_register(FourBRISlave, 2);
				FourBRISlave->state = DIA_RUNNING;
			}
		}
		card->serial_no = card->cfg.serial;

		DPRINTF(("divas: card id %d (4BRI), serial no. 0x%x ready with %d channels", 
				card_id - 3, card->serial_no, (int) channels));
	}
	else
	{
		status = idi_register(card, channels);
		if (!status)
		{
			card->state = DIA_RUNNING;
			DPRINTF(("divas: card id %d, serial no. 0x%x ready with %d channels", 
						card_id, card->serial_no, (int) channels));
		}
	}

	return status;
}

int DivasGetMem(mem_block_t *mem_block)
{
	card_t *card;
	word	card_id = mem_block->card_id;

	card = get_card(card_id);
	if (!card)
	{
		return 0;
	}

	return (*card->card_mem_get)(card, mem_block);
}


/*
 * Deleyed Procedure Call for handling interrupts from card
 */

void	DivaDoCardDpc(card_t *card)
{
	ADAPTER	*a;

	a = &card->a;

	if(UxInterlockedIncrement(card->hw, &card->dpc_reentered) > 1)
	{
		return;
	}

	do{
		if((*(card->test_int))(a))
		{
			(*(card->dpc))(a);
			(*(card->clear_int))(a);
		}
			(*(card->out))(a);
	}while(UxInterlockedDecrement(card->hw, &card->dpc_reentered));
			
}

void	DivasDoDpc(void *pData)
{
	card_t	*card = DivasCards;
	int 	i = DivasCardNext;
	
	while(i--)
	{
            if (card->state == DIA_RUNNING)
		DivaDoCardDpc(card);
            card++;
	}
}

void	DivasDoRequestDpc(void *pData)
{
	DivasDoDpc(pData);
}

/*
 * DivasGetNum
 * Returns the number of active adapters
 */

int DivasGetNum(void)
{
	return(DivasCardNext);
}

/*
 * DivasGetList
 * Returns a list of active adapters
 */
int DivasGetList(dia_card_list_t *card_list)
{
	int i;

	memset(card_list, 0, sizeof(dia_card_list_t));

	for(i = 0; i < DivasCardNext; i++)
	{
		card_list->card_type = DivasCards[i].cfg.card_type;
		card_list->card_slot = DivasCards[i].cfg.slot;
		card_list->state     = DivasCards[i].state;
		card_list++;
	}

	return 0;

}

/*
 * control logging for specified card
 */

void	DivasLog(dia_log_t *log)
{
	card_t *card;

	card = get_card(log->card_id);
	if (!card)
	{
		return;
	}

	card->log_types = log->log_types;

	return;
}

