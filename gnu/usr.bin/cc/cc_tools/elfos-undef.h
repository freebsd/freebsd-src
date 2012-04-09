/* $FreeBSD: src/gnu/usr.bin/cc/cc_tools/elfos-undef.h,v 1.1.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $ */

/* This header exists to avoid editing contrib/gcc/config/elfos.h - which
   isn't coded to be defensive as it should... */

#undef  ASM_DECLARE_OBJECT_NAME
#undef  ASM_OUTPUT_IDENT
#undef  IDENT_ASM_OP
#undef  READONLY_DATA_SECTION_ASM_OP
