/* 8 bit data address relocations (R_CRX_NUM8).  */
d8 = 0x12;
/* 16 bit data address relocations (R_CRX_NUM16).  */
d16 = 0x1234;
/* 32 bit data address relocations (R_CRX_NUM32).  */
d32 = 0x12345678;

SECTIONS
{
/* 4 bit relocations:
   Relative address (R_CRX_REL4) : 0x6 + 2 = 0x8
*/
  .text_4 0xa :
  { 
	*(.text_4)   
	foo4 = (. + 0x6);
  }

/* 8 bit relocations:
   Relative address (R_CRX_REL8): 0x10 + 2 = 0x12
   Relative address (R_CRX_REL8_CMP) : 0x10 + 4 = 0x14
*/
  .text_8 0xe0 :
  { 
	*(.text_8)   
	foo8 = (. + 0x10);
  }

/* 12 bit relocations:
   Absolute address (R_CRX_REGREL12) : 0x0101 + 0x700 + 4 = 0x805
*/
  .text_12 0x0101 :
  { 
	*(.text_12)   
	foo12 = (. + 0x700);
  }

/* 16 bit relocations:
   Relative address (R_CRX_REL16) : 0x1000 + 4 = 0x1004
   Absolute address (R_CRX_IMM16) : 0x01010 + 0x1000 + 4 = 0x2014
*/
  .text_16 0x01010 :
  { 
	*(.text_16)   
	foo16 = (. + 0x1000);
  }

/* 22 bit relocations:
   Absolute address (R_CRX_REGREL22) : 0x0201400 + 0x100000 + 6 = 0x301406
*/
  .text_22 0x0201400 :
  { 
	*(.text_22)   
	foo22 = (. + 0x100000);
  }

/* 24 bit relocations:
   Relative address (R_CRX_REL24) : 0xe00000 + 6 = 0xe00006
*/
  .text_24 0x0f01400 :
  { 
	*(.text_24)   
	foo24 = (. + 0xe00000);
  }

/* 28 bit relocations:
   Absolute address (R_CRX_REGREL28) : 0x06201400 + 0x3100000 + 6 = 0x9301406
*/
  .text_28 0x06201400 :
  { 
	*(.text_28)   
	foo28 = (. + 0x3100000);
  }

/* 32 bit relocations:
   Absolute address (R_CRX_ABS32, R_CRX_IMM32) : 0x11014000 + 0x11000000 + 6 = 0x22014006
   Relative address (R_CRX_REL32) : 0x11000000 + 6 = 0x11000006
*/
  .text_32 0x11014000 :
  { 
	*(.text_32)   
	foo32 = (. + 0x11000000);
  }
}
