SECTIONS
{
  . = 0x1000 + SIZEOF_HEADERS;
  .text ALIGN (0x20) :
   {
     LONG (label - .)
     label = .;
     LONG (ADDR (.other))
   }
   .other 0x2000 : {}
}
