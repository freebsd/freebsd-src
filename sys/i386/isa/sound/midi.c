/* UWM - comments to soft-eng@cs.uwm.edu */
#define _MIDI_TABLE_C_
#include "sound_config.h"

#ifdef CONFIGURE_SOUNDCARD

#ifndef EXCLUDE_CHIP_MIDI


static int generic_midi_busy[MAX_MIDI_DEV];

long CMIDI_init (long mem_start)
{
 
  int i; 
  int n  = num_midi_drivers;
 /* int  n = sizeof (midi_supported) / sizeof( struct generic_midi_info ); 
  */ 
 for (i = 0; i < n; i++)
 {
    if ( midi_supported[i].attach (mem_start) ) 
    {
	printk("MIDI: Successfully attached %s\n",midi_supported[i].name);
    }

  } 
   return (mem_start);
}


int
CMIDI_open (int dev, struct fileinfo *file)
{

   int mode, err, retval;

   dev = dev >> 4;

   mode = file->mode & O_ACCMODE;


   if (generic_midi_busy[dev])
      return (RET_ERROR(EBUSY));

  
   if (dev >= num_generic_midis)
   {
	printk(" MIDI device %d not installed.\n", dev);
	return (ENXIO);
   }

   if (!generic_midi_devs[dev])
   {
      printk(" MIDI device %d not initialized\n",dev);
      return (ENXIO);
   } 

   /* If all good and healthy, go ahead and issue call! */

    
    retval =  generic_midi_devs[dev]->open (dev, mode) ;

	/* If everything ok, set device as busy */

    if ( retval >= 0 )
		generic_midi_busy[dev] = 1;
    
   return ( retval );

}

int 
CMIDI_write  (int dev, struct fileinfo *file, snd_rw_buf * buf, int count)
{

    int retval;
    
    dev = dev >> 4;

   if (dev >= num_generic_midis)
   {
        printk(" MIDI device %d not installed.\n", dev);
        return (ENXIO);
   }

	/* Make double sure of healthiness -- doubt
	 * Need we check this again??
	 *
	 */

   if (!generic_midi_devs[dev])
   {
      printk(" MIDI device %d not initialized\n",dev);
      return (ENXIO);
   } 

   /* If all good and healthy, go ahead and issue call! */

    
     retval = generic_midi_devs[dev]->write (dev, buf);

     return ( retval );

} 

int
CMIDI_read (int dev, struct fileinfo *file, snd_rw_buf *buf, int count)
{
     int retval;
    
    dev = dev >> 4;

   if (dev >= num_generic_midis)
   {
        printk(" MIDI device %d not installed.\n", dev);
        return (ENXIO);
   }

        /* Make double sure of healthiness -- doubt
         * Need we check this again??
         *
         */
 
   if (!generic_midi_devs[dev])
   {
      printk(" MIDI device %d not initialized\n",dev);
      return (ENXIO);
   } 

   /* If all good and healthy, go ahead and issue call! */

 
     retval = generic_midi_devs[dev]->read(dev,buf);

     return (retval);

} 
   
int
CMIDI_close (int dev, struct fileinfo *file)
{

   int retval;
   dev = dev >> 4;

   if (dev >= num_generic_midis)
   {
        printk(" MIDI device %d not installed.\n", dev);
        return (ENXIO);
   }

        /* Make double sure of healthiness -- doubt
         * Need we check this again??
         *
         */
 
   if (!generic_midi_devs[dev])
   {
      printk(" MIDI device %d not initialized\n",dev);
      return (ENXIO);
   } 

   /* If all good and healthy, go ahead and issue call! */


       generic_midi_devs[dev]->close(dev);     

	  generic_midi_busy[dev] = 0;	/* Free the device */

    return (0) ;

}

#endif

#endif
