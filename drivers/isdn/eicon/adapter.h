/*
 * Main internal include file for Diva Server driver
 *
 * Copyright (C) Eicon Technology Corporation, 2000.
 *
 * Eicon File Revision :    1.7  
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#if !defined(ADAPTER_H)
#define ADAPTER_H

#include "sys.h"
#include "idi.h"
#include "divas.h"
#undef ID_MASK
#include "pc.h"

#define XMOREC 0x1f
#define XMOREF 0x20
#define XBUSY  0x40
#define RMORE  0x80

        /* structure for all information we have to keep on a per   */
        /* adapater basis                                           */

typedef struct adapter_s ADAPTER;

struct adapter_s {
  void * io;

  byte IdTable[256];
  byte ReadyInt;

  byte (* ram_in)(ADAPTER * a, void * adr);
  word (* ram_inw)(ADAPTER * a, void * adr);
  void (* ram_in_buffer)(ADAPTER * a, void * adr, void * P, word length);
  void (* ram_look_ahead)(ADAPTER * a, PBUFFER * RBuffer, ENTITY * e);

  void (* ram_out)(ADAPTER * a, void * adr, byte data);
  void (* ram_outw)(ADAPTER * a, void * adr, word data);
  void (* ram_out_buffer)(ADAPTER * a, void * adr, void * P, word length);

  void (* ram_inc)(ADAPTER * a, void * adr);
};

typedef struct card card_t;

typedef int	card_load_fn_t(card_t *card, dia_load_t *load);
typedef int	card_config_fn_t(card_t *card, dia_config_t *config);
typedef int	card_start_fn_t(card_t *card, byte *channels);
typedef	int	card_reset_fn_t(card_t *card);
typedef int card_mem_get_fn_t(card_t *card, mem_block_t *mem_block);

#define	MAX_PENTITIES	256		/* Number of entities primary adapter */
#define MAX_ENTITIES	16		/* Number of entities standard adapter */

typedef struct e_info_s E_INFO;

struct e_info_s
{
	ENTITY		*e;				/* entity pointer */
	byte		next;			/* chaining index */
	word		assign_ref;		/* assign reference */
};

/* DIVA card info (details hidden from user) */

typedef struct	ux_diva_card_s ux_diva_card_t;

/* card info */

struct card
{
	ADAPTER				a;				/* per-adapter information */
	dia_card_t			cfg;			/* card configuration */
	int 				state;			/* State of the adapter */
	dword 				serial_no;		/* serial number */
	int 				test_int_pend;	/* set for interrupt testing */
	ux_diva_card_t		*hw;			/* O/S-specific handle */
	card_reset_fn_t		*card_reset;	/* call this to reset card */
	card_load_fn_t		*card_load;		/* call this to load card */
	card_config_fn_t	*card_config;	/* call this to config card */
	card_start_fn_t		*card_start;	/* call this to start card */
	card_mem_get_fn_t	*card_mem_get;	/* call this to get card memory */
	E_INFO				*e_tbl;			/* table of ENTITY pointers */
	byte				e_head;			/* list of active ENTITIES */
	byte				e_tail;			/* list of active ENTITIES */
	int					e_count;		/* # of active ENTITIES */
	int					e_max;			/* total # of ENTITIES */
	byte				assign;			/* assign queue entry */
	PBUFFER				RBuffer;		/* Copy of receive lookahead buffer */
	int					log_types;		/* bit-mask of active logs */
	word				xlog_offset;	/* offset to XLOG buffer on card */
	void		(*out)(ADAPTER *a);
	byte		(*dpc)(ADAPTER * a);
	byte		(*test_int)(ADAPTER * a);
	void		(*clear_int)(ADAPTER * a);
	void		(*reset_int)(card_t *c);
	int  		is_live;

	int		(*card_isr)(card_t *card);

	int 		int_pend;		/* interrupt pending */
	long		interrupt_reentered;
	long 		dpc_reentered;
	int 		set_xlog_request;

} ;

/* card information */

#define	MAX_CARDS	20		/* max number of cards on a system */

extern
card_t			DivasCards[];

extern
int				DivasCardNext;

extern
dia_config_t	DivasCardConfigs[];

extern
byte 			DivasFlavourConfig[];

/*------------------------------------------------------------------*/
/* public functions of IDI common code                              */
/*------------------------------------------------------------------*/

void DivasOut(ADAPTER * a);
byte DivasDpc(ADAPTER * a);
byte DivasTestInt(ADAPTER * a);
void DivasClearInt(ADAPTER * a);

/*------------------------------------------------------------------*/
/* public functions of configuration platform-specific code         */
/*------------------------------------------------------------------*/

int DivasConfigGet(dia_card_t *card);

/*------------------------------------------------------------------*/
/* public functions of LOG related code                             */
/*------------------------------------------------------------------*/

void	DivasXlogReq(int card_num);
int		DivasXlogRetrieve(card_t *card);
void	DivasLog(dia_log_t *log);
void	DivasLogIdi(card_t *card, ENTITY *e, int request);

/*------------------------------------------------------------------*/
/* public functions to initialise cards for each type supported     */
/*------------------------------------------------------------------*/

int		DivasPriInit(card_t *card, dia_card_t *cfg);

int		DivasBriInit(card_t *card, dia_card_t *cfg);
int		Divas4BriInit(card_t *card, dia_card_t *cfg);
void 	DivasBriPatch(card_t *card);

/*------------------------------------------------------------------*/
/* public functions of log common code                              */
/*------------------------------------------------------------------*/

extern	char	*DivasLogFifoRead(void);
extern	void	DivasLogFifoWrite(char *entry, int length);
extern	int		DivasLogFifoEmpty(void);
extern	int		DivasLogFifoFull(void);
extern	void    DivasLogAdd(void *buffer, int length);

/*------------------------------------------------------------------*/
/* public functions of misc. platform-specific code         		*/
/*------------------------------------------------------------------*/

int		DivasDpcSchedule(void);
void		DivasDoDpc(void *);
void		DivasDoRequestDpc(void *pData);
int		DivasScheduleRequestDpc(void);

/* table of IDI request functions */

extern
IDI_CALL	DivasIdiRequest[];

/*
 * intialisation entry point
 */

int		DivasInit(void);

/*
 * Get information on the number and type of cards present
 */

extern
int 	DivasCardsDiscover(void);

/*
 * initialise a new card
 */

int		DivasCardNew(dia_card_t *card);

/*
 * configure specified card
 */

int		DivasCardConfig(dia_config_t *config);

/*
 * load specified binary code onto card
 */

int		DivasCardLoad(dia_load_t *load);

/*
 * start specified card running
 */

int		DivasCardStart(int card_id);

/*
 * ISR for card
 * Returns 0 if specified card was interrupting
 */

int		DivasIsr(void *arg);

/*
 * Get number of active cards
 */

int		DivasGetNum(void);

/*
 * Get list of active cards
 */

int		DivasGetList(dia_card_list_t *card_list);

/* definitions common to several card types */

#define DIVAS_SHARED_OFFSET     (0x1000)

#endif /* ADAPTER_H */
