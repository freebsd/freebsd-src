/*
 * Copyright by UWM -- comments to soft-eng@cs.uwm.edu
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
 *
 */
#define ALL_EXTERNAL_TO_ME
#include "sound_config.h"

#ifdef CONFIGURE_SOUNDCARD

#include "pas.h"
#define ESUCCESS 0

#if !defined(EXCLUDE_PRO_MIDI) && !defined(EXCLUDE_CHIP_MIDI)


/** Structure for handling operations **/


static struct generic_midi_operations pro_midi_operations = {

	{"Pro_Audio_Spectrum 16 MV101", 0},
 	pro_midi_open,
	pro_midi_close,
	pro_midi_write,
	pro_midi_read
};

/*
 * Note! Note! Note!
 * Follow the same model for any other attach function you
 * may write
 */

long pro_midi_attach( long mem_start)
{
  pro_midi_dev = num_generic_midis;
  generic_midi_devs[num_generic_midis++] = &pro_midi_operations;
  return mem_start;
} 

int pro_midi_open(int dev, int mode)
{

   int intr_mask, s;


   s = splhigh();


   /* Reset the input and output FIFO pointers */


  outb(MIDI_CONTROL,M_C_RESET_INPUT_FIFO | M_C_RESET_OUTPUT_FIFO);

   /* Get the interrupt status */

   intr_mask = inb(INTERRUPT_MASK);


   /* Enable MIDI IRQ */

   intr_mask |= I_M_MIDI_IRQ_ENABLE;
   outb(INTERRUPT_MASK, intr_mask);


  /* Enable READ/WRITE on MIDI port. This part is quite unsure though */

    outb(MIDI_CONTROL,M_C_ENA_OUTPUT_IRQ | M_C_ENA_INPUT_IRQ);

  /* Acknowledge pending interrupts */

    outb(MIDI_STATUS,0xff);


     splx(s);

    return(ESUCCESS);


}


void pro_midi_close(int dev)
{

    int intr_mask;

	/* Clean up */

   outb(MIDI_CONTROL,M_C_RESET_INPUT_FIFO | M_C_RESET_OUTPUT_FIFO);
   intr_mask = inb(INTERRUPT_MASK);
   intr_mask &= ~I_M_MIDI_IRQ_ENABLE;
   outb(INTERRUPT_MASK,intr_mask);

   return;
}

int pro_midi_write(int dev, struct uio *uio)
{

   int s;
   unsigned char data;

   /* printf("midi: Going to do write routine..\n"); */
   while(uio->uio_resid) {

      if ( uiomove(&data,1,uio) ) return(ENOTTY);

      s = splhigh();

      DELAY(30);
      outb(MIDI_DATA,data);
      DELAY(70);		/* Ze best pause.. find a better one if
				 * you can :) 
				 */
      splx(s);
                         }

   return(ESUCCESS);

}


int pro_midi_read(int dev, struct uio *uio)
{

    int s;
    unsigned char data;

   s = splhigh();

   /* For each uio_iov[] entry .... */

  while (uio->uio_resid) {

     if((( inb(MIDI_STATUS) & M_S_INPUT_AVAIL)  == 0 ) &&
                ((inb(MIDI_FIFO_STATUS) & MIDI_INPUT_AVAILABLE) == 0 ) )

                data = 0xfe;
     else
        data = inb(MIDI_DATA);

        if ( uiomove(&data, 1 , uio)) {

                        printf("midi: Bad copyout()!\n");
                        return(ENOTTY);

                                       }

                         }
      splx(s);
      return(ESUCCESS);

}

#endif

#endif
