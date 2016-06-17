/*
 * Copyright (C) Eicon Technology Corporation, 2000.
 *
 * Eicon File Revision :    1.2  
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#include "sys.h"
#include "idi.h"
#include "uxio.h"

#define FPGA_PORT		0x6E
#define FPGA_DLOAD_BUFLEN   	256
#define NAME_OFFSET         	0x10
#define NAME_MAXLEN         	12
#define DATE_OFFSET         	0x2c
#define DATE_MAXLEN         	10

word UxCardPortIoInW(ux_diva_card_t *card, byte *base, int offset);
void UxCardPortIoOutW(ux_diva_card_t *card, byte *base, int offset, word);
void UxPause(long int);

/*-------------------------------------------------------------------------*/
/* Loads the FPGA configuration file onto the hardware.                    */
/* Function returns 0 on success, else an error number.                    */
/* On success, an identifier string is returned in the buffer              */
/*                                                                         */
/* A buffer of FPGA_BUFSIZE, a handle to the already opened bitstream      */
/* file and a file read function has to be provided by the operating       */
/* system part.                                                            */
/* ----------------------------------------------------------------------- */
int FPGA_Download( word      cardtype,
                        dword     RegBase,
                        byte *strbuf,
                        byte FPGA_SRC[],
			int FPGA_LEN
                      )
{
  word        i, j, k;
  word        baseval, Mask_PROGRAM, Mask_DONE, Mask_CCLK, Mask_DIN;
  dword       addr;
  byte        *pFPGA;

  //--- check for legal cardtype
  switch (cardtype)
  {
    case IDI_ADAPTER_MAESTRAQ:
      addr          = RegBase ; // address where to access FPGA
      Mask_PROGRAM  = 0x0001;         // FPGA pins at address
      Mask_DONE     = 0x0002;
      Mask_CCLK     = 0x0100;
      Mask_DIN      = 0x0400;
      baseval       = 0x000d;         // PROGRAM hi, CCLK lo, DIN lo by default
    break;
      
    default:
	
  	DPRINTF(("divas: FPGA Download ,Illegal Card"));
      	return -1; // illegal card 
  }

  //--- generate id string from file content
  for (j=NAME_OFFSET, k=0; j<(NAME_OFFSET+NAME_MAXLEN); j++, k++) //name
  {
    if (!FPGA_SRC[j]) break;
    strbuf[k] = FPGA_SRC[j];
  } 
  strbuf[k++] = ' ';
  for (j=DATE_OFFSET; j<(DATE_OFFSET+DATE_MAXLEN); j++, k++) // date
  {
    if (!FPGA_SRC[j]) break;
    strbuf[k] = FPGA_SRC[j];
  } 
  strbuf[k] = 0;

  DPRINTF(("divas: FPGA Download - %s", strbuf));

  //--- prepare download, Pulse PROGRAM pin down.
  UxCardPortIoOutW(NULL, (byte *) addr, FPGA_PORT, baseval &~Mask_PROGRAM);  // PROGRAM low pulse
  UxCardPortIoOutW(NULL, (byte *) addr, FPGA_PORT, baseval);                 // release
  UxPause(50);  // wait until FPGA finised internal memory clear
  
  //--- check done pin, must be low
  if (UxCardPortIoInW(NULL, (byte *) addr, FPGA_PORT) &Mask_DONE) 
  {
    DPRINTF(("divas: FPGA_ERR_DONE_WRONG_LEVEL"));
    return -1;
  }

  pFPGA = FPGA_SRC;

  i = 0; 
  /* Move past the header */
  while ((FPGA_SRC[i] != 0xFF) && (i < FPGA_LEN)) 
  {
    i++;
  }

  // We've hit the 0xFF so move on to the next byte
  // i++;
  DPRINTF(("divas: FPGA Code starts at offset %d", i));

  //--- put data onto the FPGA
  for (;i<FPGA_LEN; i++)
  {
    //--- put byte onto FPGA
    for (j=0; j<8; j++)
    {
      if (FPGA_SRC[i] &(0x80>>j)) baseval |= Mask_DIN; // write a hi
      else                      baseval &=~Mask_DIN; // write a lo
      UxCardPortIoOutW(NULL, (byte *) addr, FPGA_PORT, baseval);
      UxCardPortIoOutW(NULL, (byte *) addr, FPGA_PORT, baseval | Mask_CCLK);     // set CCLK hi
      UxCardPortIoOutW(NULL, (byte *) addr, FPGA_PORT, baseval);                 // set CCLK lo
    }
  }

  //--- add some additional startup clock cycles and check done pin
  for (i=0; i<5; i++) 
  {
    UxCardPortIoOutW(NULL, (byte *) addr, FPGA_PORT, baseval | Mask_CCLK);     // set CCLK hi
    UxCardPortIoOutW(NULL, (byte *) addr, FPGA_PORT, baseval);                 // set CCLK lo
  }

  UxPause(100);

  if (UxCardPortIoInW(NULL, (byte *) addr, FPGA_PORT) &Mask_DONE) 
  {
    DPRINTF(("divas: FPGA download successful"));
  }
  else
  {
    DPRINTF(("divas: FPGA download failed - 0x%x", UxCardPortIoInW(NULL, (byte *) addr, FPGA_PORT)));
	return -1;
  }

return 0;
}

