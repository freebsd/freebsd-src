

#include "asm/hp-lj/asic.h"

AsicId GetAsicId(void)
{
   static int asic = IllegalAsic;

   if (asic == IllegalAsic) {
      if (*(unsigned int *)0xbff70000 == 0x1114103c)
         asic = HarmonyAsic;
      else if (*(unsigned int *)0xbff80000 == 0x110d103c)
         asic = AndrosAsic;
      else
	 asic = UnknownAsic;
   }
   return asic;
}


const char* const GetAsicName(void)
{
   static const char* const Names[] =
        { "Illegal", "Unknown", "Andros", "Harmony" };

   return Names[(int)GetAsicId()];
}

