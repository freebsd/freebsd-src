#include <stdio.h>
#include <malloc.h>

void heapdump( void )
{
   _HEAPINFO hinfo;
   int heapstatus;
   hinfo._pentry = NULL;
   while( ( heapstatus = _heapwalk( &hinfo ) ) == _HEAPOK )
   { printf( "%6s block at %Fp of size %4.4X\n",
        ( hinfo._useflag == _USEDENTRY ? "USED" : "FREE" ),
          hinfo._pentry, hinfo._size );
   }

   switch( heapstatus )
   {
   case _HEAPEMPTY:
      printf( "OK - empty heap\n" );
      break;
   case _HEAPEND:
      printf( "OK - end of heap\n" );
      break;
   case _HEAPBADPTR:
      printf( "ERROR - bad pointer to heap\n" );
      break;
   case _HEAPBADBEGIN:
      printf( "ERROR - bad start of heap\n" );
      break;
   case _HEAPBADNODE:
      printf( "ERROR - bad node in heap\n" );
      break;
   }
}
