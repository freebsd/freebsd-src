/* lextaba.c: lexical tables for ASCII. */

/* These tables are munged by setnonsgml(). */

#include "config.h"
#include "entity.h"
#include "lexcode.h"
#include "sgmldecl.h"

/* LEXCNM: Lexical table for mixed content (PCBCONM) parse.
*/
/* Symbols for SGML character set divisions and function characters. */
#define NU      1   /* NUMERAL            Numerals */
#define NMC     2   /* LC/UCNMCHAR . -    Period and hyphen */
#define NMS     3   /* LC/UCNMSTRT        Lower and uppercase letters */
#define SPC     4   /* SPACE     32       Space */
#define NON     5   /* NONSGML   0-31 127 255 Unused, except for: */
#define EE      6   /* NONSGML   00 26    Entity end (end of file) */
#define EOB     7   /* NONSGML   28       End disk buffer */
#define RS      8   /* Function  10       Line feed */
#define RE      9   /* Function  13       Carrier return */
#define SEP    10   /* SEPCHAR   09       TAB: horizontal tab */
#define NSC    12   /* NONSGML   delnonch Non-SGML character prefix */

/* Symbols for SGML delimiter roles in CON and CXT.
   ETI and NET must be the same in LEXCNM and LEXCON.
   FRE characters are changed to FCE if an FCE entity is declared.
   They are changed back to FRE when the entity is canceled.
*/
#define ERO    13   /* &    Also CRO[1] */
#define NMRE   14   /* 08   Generated non-markup RE */
#define COM    15   /* -    For MDO context; also SR19 and SR20. */
#undef LIT1
#define LIT1   18   /* "    SR10 */
#define MDO    20   /* !    Actually MDO[2] */
#define MSC1   21   /* ]    Both MSC[1] and MSC[2]; also SR26. */
#define MSO    22   /* [    For MDO context; also SR25. */
#define PIO    23   /* ?    Actually PIO[2] */
#define RNI    24   /* #    For CRO[2]; also SR11. */
#define TGC1   25   /* >    For TAGO and MSC context; also MDC, PIC */
#define TGO1   26   /* <    TAGO; also MDO[1], PIO[1] */

UNCH    lexcnm[256] = { /*
000  001                          bs   tab  lf   home ff   cr   so   si   */
EE,  NON, NON, NON, NON, NON, NON, NON, NMRE,SEP, RS,  NON, NON, RE,  NON, NON, /*
                                        eof  esc  rt   left up   down */
NON, NON, NON, NON, NON, NON, NON, NON, NON, NON, EE,  NON, EOB, NON, NON, NSC, /*
032  !    "    #    $    %    &    '    (    )    *    +    ,    -    .    /    */
SPC, MDO, LIT1,RNI, FRE, FRE ,ERO, FRE, FRE, FRE, FRE, FRE, FRE, COM, NMC, ETI, /*
0    1    2    3    4    5    6    7    8    9    :    ;    <    =    >    ?    */
NU , NU , NU , NU , NU , NU , NU , NU , NU , NU , FRE, FRE, TGO1,FRE, TGC1,PIO, /*
@    A    B    C    D    E    F    G    H    I    J    K    L    M    N    O    */
FRE, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, /*
P    Q    R    S    T    U    V    W    X    Y    Z    [    \    ]    ^    _    */
NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, MSO, FRE, MSC1,FRE, FRE, /*
`    a    b    c    d    e    f    g    h    i    j    k    l    m    n    o    */
FRE, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, /*
p    q    r    s    t    u    v    w    x    y    z    {    |    }    ~    127  */
NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, FRE, FRE, FRE, FRE, NON,
FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE,
FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE,
FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE,
FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE,
FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE,
FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE,
FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE,
FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, NON
};
/*      free nu   nmc  nms  spc  non  ee   eob  rs   re   sep  cde  nsc  ero
        nmre com  eti  lit  spcr mdo  msc  mso  net  pio  rni  tagc tago fce   */
#undef ERO
#undef NMRE
#undef COM
#undef LIT1
/* def SPCR*/
#undef MDO
#undef MSC1
#undef MSO
#undef PIO
#undef RNI
#undef TGC1
/* def TGO1*/
/* def FCE*/
/* LEXCON: Lexical table for RCDATA and CDATA content (PCBCON?),
           prolog (PCBPRO), and nested declaration set (PCBMDS) parses.
   Note: NMC is same as FRE; kept for consistency with LEXCNM and LEXLMS.
*/
/* Symbols for SGML character set divisions and function characters. */
/* Same as for LEXCNM. */

/* Symbols for SGML delimiter roles in CON, CXT, and DS.
   ETI and NET must be the same in LEXCNM and LEXCON.
   FRE characters are changed to FCE if an FCE entity is declared.
   They are changed back to FRE when the entity is canceled.
*/
#define ERO    13   /* &    Also CRO[1] */
#define NMRE   14   /* 08   Generated non-markup RE */
#define COM    15   /* -    For MDO context. */
/*#define ETI    16    /    Actually ETAGO[2] */
/*#define NET    17    /    When enabled. */
#define MDO    18   /* !    Actually MDO[2] */
#define MSC2   19   /* ]    Both MSC[1] and MSC[2]. */
#define MSO    20   /* [    For MDO context. */
#define PERO   21   /* %    For prolog */
#define PIO    22   /* ?    Actually PIO[2] */
#define RNI    23   /* #    For CRO[2]. */
#define TGC2   24   /* >    For TAGO and MSC context; also MDC, PIC */

UNCH    lexcon[256] = { /*
000  001                          bs   tab  lf   home ff   cr   so   si   */
EE,  NON, NON, NON, NON, NON, NON, NON, NMRE,SEP, RS,  NON, NON, RE,  NON, NON, /*
                                        eof  esc  rt   left up   down */
NON, NON, NON, NON, NON, NON, NON, NON, NON, NON, EE,  NON, EOB, NON, NON, NSC, /*
032  !    "    #    $    %    &    '    (    )    *    +    ,    -    .    /    */
SPC, MDO, FRE, RNI, FRE, PERO,ERO, FRE, FRE, FRE, FRE, FRE, FRE, COM, NMC, ETI, /*
0    1    2    3    4    5    6    7    8    9    :    ;    <    =    >    ?    */
NU , NU , NU , NU , NU , NU , NU , NU , NU , NU , FRE, FRE, TGO2,FRE, TGC2,PIO, /*
@    A    B    C    D    E    F    G    H    I    J    K    L    M    N    O    */
FRE, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, /*
P    Q    R    S    T    U    V    W    X    Y    Z    [    \    ]    ^    _    */
NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, MSO, FRE, MSC2,FRE, FRE, /*
`    a    b    c    d    e    f    g    h    i    j    k    l    m    n    o    */
FRE, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, /*
p    q    r    s    t    u    v    w    x    y    z    {    |    }    ~    127  */
NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, FRE, FRE, FRE, FRE, NON,
FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE,
FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE,
FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE,
FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE,
FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE,
FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE,
FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE,
FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, NON
};
/*      free nu   nmc  nms  spc  non  ee   eob  rs   re   sep  cde  nsc  ero
        nmre com  eti  net  mdo  msc  mso  pero pio  rni  tagc tago  */
#undef FRE
#undef NU
#undef NMC
#undef NMS
#undef SPC
#undef NON
#undef EE
#undef EOB
#undef RS
#undef RE
#undef SEP
#undef NSC
#undef ERO
#undef NMRE
#undef COM
/* def ETI*/
/* def NET*/
#undef MDO
#undef MSC2
#undef MSO
#undef PERO
#undef PIO
#undef RNI
#undef TGC2
/* LEXGRP: Lexical table for group parses, including PCBREF.
*/
/* Symbols for SGML character set divisions. */
#define BIT     0   /* Bit combinations (not NONCHAR) not allowed in a group. */
#define NMC     1   /* NAMECHAR  . -      Period, underscore, and numerals */
#define NMS     2   /* NAMESTRT           Lower and uppercase letters */
#define RE      3   /* Function  13       Carrier return */
#define SPC     4   /* SPACE     32 09    Space; includes TAB */
#define NON     5   /* NONCHAR   0-31 127 255 Unused, except for: */
#define EE      6   /* Function  26 00    EE: entity end (end of file) */
#define EOB     7   /* NONCHAR   28       End disk buffer. */
#define RS      8   /* Function  10       RS: record start (line feed) */

/* Symbols for SGML delimiter roles in GRP. */
#define AND1    9   /* &    */
#define GRPC   10   /* )    */
#define GRPO   11   /* (    */
#undef LIT2
#define LIT2   12   /* "    For datatags. */
#define LITA   13   /* '    For datatags. */
#define DTGC   14   /* ]    For datatags. */
#define DTGO   15   /* [    For datatags. */
#define OPT1   16   /* ?    */
#define OR1    17   /* |    */
#define PERO   18   /* %    */
#define PLUS   19   /* +    */
#define REP1   20   /* *    */
#define RNI    21   /* #    For #CHARS */
#define SEQ1   22   /* ,    */
#define REFC   23   /* ;    For references */

UNCH lexgrp[256] = { /*
000  001                          bs   tab  lf   home ff   cr   so   si   */
EE , NON, NON, NON, NON, NON, NON, NON, NON, SPC, RS,  NON, NON, RE,  NON, NON, /*
                                        eof  esc  rt   left up   down */
NON, NON, NON, NON, NON, NON, NON, NON, NON, NON, EE , NON, EOB, NON, NON, NON, /*
032  !    "    #    $    %    &    '    (    )    *    +    ,    -    .    /    */
SPC, BIT, LIT2,RNI, BIT, PERO,AND1,LITA,GRPO,GRPC,REP1,PLUS,SEQ1,NMC, NMC, BIT, /*
0    1    2    3    4    5    6    7    8    9    :    ;    <    =    >    ?    */
NMC, NMC, NMC, NMC, NMC, NMC, NMC, NMC, NMC, NMC, BIT, REFC,BIT, BIT, BIT, OPT1,/*
@    A    B    C    D    E    F    G    H    I    J    K    L    M    N    O    */
BIT, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, /*
P    Q    R    S    T    U    V    W    X    Y    Z    [    \    ]    ^    _    */
NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, DTGO,BIT, DTGC,BIT, BIT, /*
`    a    b    c    d    e    f    g    h    i    j    k    l    m    n    o    */
BIT, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, /*
p    q    r    s    t    u    v    w    x    y    z    {    |    }    ~    127  */
NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, BIT, OR1, BIT, BIT, NON,
BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT,
BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT,
BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT,
BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT,
BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT,
BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT,
BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT,
BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, NON
};
/*      bit  nmc  nms  re   spc  non  ee   eob  rs   and  grpc grpo lit  lita
        dtgc dtgo opt  or   pero plus rep  rni  seq  refc */
#undef BIT
#undef NMC
#undef NMS
#undef RE
#undef SPC
#undef NON
#undef EE
#undef EOB
#undef RS
#undef AND1
#undef GRPC
#undef GRPO
#undef LIT2
#undef LITA
#undef DTGC
#undef DTGO
#undef OPT1
#undef OR1
#undef PERO
#undef PLUS
#undef REP1
#undef RNI
#undef SEQ1
#undef REFC
/* LEXLMS: Lexical table for literal parses and marked sections.
*/
/* Symbols for SGML character set divisions and function characters.
*/
#define FRE     0   /* Free char: not in a delimiter or minimum literal. */
#define NU      1   /* Numeral            Numerals */
#undef MIN
#define MIN     2   /* Minimum literal    '()+,-./:?= */
#define NMS     3   /* LC/UCNMSTRT        Lower and uppercase letters */
#define SPC     4   /* SPACE     32       Space */
#define NON     5   /* NONSGML   0-31 127 255 Unused, except for: */
#define EE      6   /* NONSGML   00 26    Entity end (end of file) */
#define EOB     7   /* NONSGML   28       End disk buffer */
#define RS      8   /* Function  10       Line feed */
#define RE      9   /* Function  13       Carrier return */
#define SEP    10   /* SEPCHAR   09       TAB: horizontal tab */
/*#define CDE    11    NONSGML   delcdata CDATA/SDATA delimiter */
#define NSC    12   /* NONSGML   delnonch Non-SGML character prefix */
/* Symbols for SGML delimiter roles in LIT, PI, and marked sections.
   Either LIT, LITA, PIC, or EE, is changed to LITC when a literal is begun.
   It is changed back when the LITC occurs (i.e., when the literal ends).
*/
#define ERO    13   /* &    */
#define MDO    14   /* !    Actually MDO[2] */
#define MSO    16   /* [    For MDO context. */
#define PERO   17   /* %    For prolog. */
#define RNI    18   /* #    For CRO[2] */
#define TGC3   19   /* >    Also MDC for MSC context. */
#define TGO3   20   /* <    TAGO; also MDO[1] */

/* Room has been left in the parse tables in case re-parsing of text
   is eventually supported (i.e., saved parsed text is used by the
   application to create a new SGML document, but CDATA and SDATA
   entities in literals, and non-SGML characters, are left in their
   parsed state to avoid the overhead of reconstituting the original
   markup).  In such a case, the two non-SGML characters DELCDATA and
   DELSDATA are changed to CDE.
   NOTE: The idea is a bad one, because the generated document would
   be non-conforming, as it would contain non-SGML characters.
*/
UNCH    lexlms[256] = { /*
000  001                          bs   tab  lf   home ff   cr   so   si   */
EE,  NON, NON, NON, NON, NON, NON, NON, NON ,SEP, RS,  NON, NON, RE,  NON, NON, /*
                                        eof  esc  rt   left up   down */
NON, NON, NON, NON, NON, NON, NON, NON, NON, NON, EE,  NON, EOB, NON, NON, NSC, /*
032  !    "    #    $    %    &    '    (    )    *    +    ,    -    .    /    */
SPC, MDO, FRE, RNI, FRE, PERO,ERO, MIN, MIN, MIN, FRE, MIN, MIN, MIN, MIN, MIN, /*
0    1    2    3    4    5    6    7    8    9    :    ;    <    =    >    ?    */
NU , NU , NU , NU , NU , NU , NU , NU , NU , NU , MIN, FRE, TGO3,MIN, TGC3,MIN, /*
@    A    B    C    D    E    F    G    H    I    J    K    L    M    N    O    */
FRE, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, /*
P    Q    R    S    T    U    V    W    X    Y    Z    [    \    ]    ^    _    */
NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, MSO, FRE, MSC3,FRE, FRE, /*
`    a    b    c    d    e    f    g    h    i    j    k    l    m    n    o    */
FRE, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, /*
p    q    r    s    t    u    v    w    x    y    z    {    |    }    ~    127  */
NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, FRE, FRE, FRE, FRE, NON,
FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE,
FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE,
FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE,
FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE,
FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE,
FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE,
FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE,
FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, NON
};
/*      free nu   min  nms  spc  non  ee   eob  rs   re   sep  cde  nsc  ero
        mdo  msc  mso  pero rni  tago tagc litc */
/* def FRE*/
#undef NU
#undef MIN
#undef NMS
#undef SPC
#undef NON
#undef EE
#undef EOB
#undef RS
#undef RE
#undef SEP
/* def CDE*/
/* def NSC*/
#undef ERO
#undef MDO
/* def MSC3*/
#undef MSO
#undef PERO
#undef RNI
#undef TGC3
#undef TGO3
/* def LITC*/
/* LEXMIN: Lexical table for minimum data literals.
*/
/* Symbols for SGML character set divisions and function characters.
*/
#define FRE     0   /* Free char: not in a delimiter or minimum literal. */
#define NU      1   /* Numeral            Numerals */
#undef MIN
#define MIN     2   /* Minimum literal    '()+,-./:?= */
#define NMS     3   /* LC/UCNMSTRT        Lower and uppercase letters */
#define SPC     4   /* SPACE     32       Space */
#define NON     5   /* NONSGML   0-31 127 255 Unused, except for: */
#define EE      6   /* NONSGML   00 26    Entity end (end of file) */
#define EOB     7   /* NONSGML   28       End disk buffer */
#define RS      8   /* Function  10       Line feed */
#define RE      9   /* Function  13       Carrier return */
#define SEP    10   /* SEPCHAR   09       TAB: horizontal tab */
/*#define CDE    11    NONSGML   delcdata CDATA/SDATA delimiter */
#define NSC    12   /* NONSGML   delnonch Non-SGML character prefix */
/* Either LIT or LITA changed to LITC when a literal is begun.
   It is changed back when the LITC occurs (i.e., when the literal ends).
*/
UNCH    lexmin[256] = { /*
000  001                          bs   tab  lf   home ff   cr   so   si   */
EE,  NON, NON, NON, NON, NON, NON, NON, NON ,SEP, RS,  NON, NON, RE,  NON, NON, /*
                                        eof  esc  rt   left up   down */
NON, NON, NON, NON, NON, NON, NON, NON, NON, NON, EE,  NON, EOB, NON, NON, NSC, /*
032  !    "    #    $    %    &    '    (    )    *    +    ,    -    .    /    */
SPC, FRE, FRE, FRE, FRE, FRE, FRE, MIN, MIN, MIN, FRE, MIN, MIN, MIN, MIN, MIN, /*
0    1    2    3    4    5    6    7    8    9    :    ;    <    =    >    ?    */
NU , NU , NU , NU , NU , NU , NU , NU , NU , NU , MIN, FRE, FRE, MIN, FRE, MIN, /*
@    A    B    C    D    E    F    G    H    I    J    K    L    M    N    O    */
FRE, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, /*
P    Q    R    S    T    U    V    W    X    Y    Z    [    \    ]    ^    _    */
NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, FRE, FRE, FRE, FRE, FRE, /*
`    a    b    c    d    e    f    g    h    i    j    k    l    m    n    o    */
FRE, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, /*
p    q    r    s    t    u    v    w    x    y    z    {    |    }    ~    127  */
NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, FRE, FRE, FRE, FRE, NON,
FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE,
FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE,
FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE,
FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE,
FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE,
FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE,
FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE,
FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, FRE, NON
};
/*      free nu   min  nms  spc  non  ee   eob  rs   re   sep  cde  nsc  ero
        mdo  msc  mso  pero rni  tago tagc litc */
/* def FRE*/
#undef NU
#undef MIN
#undef NMS
#undef SPC
#undef NON
#undef EE
#undef EOB
#undef RS
#undef RE
#undef SEP
/* def CDE*/
/* def NSC*/
/* def LITC*/
/* LEXMARK: Lexical scan table for markup: PCBMD? and PCB?TAG.
*/
/* Symbols for SGML character set divisions. */
#define BIT     0   /* Bit combinations not allowed; includes ESC SO SI */
#define NMC     1   /* NAMECHAR  . _      Period and underscore */
#define NU      2   /* NUMERAL            Numerals */
#define NMS     3   /* NAMESTRT           Lower and uppercase letters */
#define SPC     4   /* SPACE     32 13 09 Space; includes RE TAB */
#define NON     5   /* NONCHAR   0-31 127 255 Unused, except for: */
#define EE      6   /* Function  26 00    EE: entity end (end of file) */
#define EOB     7   /* NONCHAR   28       End disk buffer. */
#define RS      8   /* Function  10       RS: record start (line feed) */

/* Symbols for SGML delimiter roles in MD and TAG. */
#define COM1    9   /* -    Actually COM[1]; also COM[2], MINUS. */
#define ETIB   10   /* /    ETI; actually ETAGO[2]. */
#define GRPO   11   /* (    */
#define LIT3   12   /* "    */
#define LITA   13   /* '    */
#define DSO    14   /* [    */
#define DSC1   15   /* ]    For data attribute specifications */
#define PERO   16   /* %    */
#define PLUS   17   /* +    */
#define REFC   18   /* ;    For references */
#define RNI    19   /* #    Also CRO[2] */
#define TGC4   20   /* >    Also MDC, PIC */
#define TGO4   21   /* <    TAGO; also MDO[1] */
#define VI     22   /* =    */

UNCH lexmark[256] = { /*
000  001                          bs   tab  lf   home ff   cr   so   si   */
EE , NON, NON, NON, NON, NON, NON, NON, NON, SPC, RS,  NON, NON, SPC, NON, NON, /*
                                        eof  esc  rt   left up   down */
NON, NON, NON, NON, NON, NON, NON, NON, NON, NON, EE , NON, EOB, NON, NON, NON, /*
032  !    "    #    $    %    &    '    (    )    *    +    ,    -    .    /    */
SPC, BIT, LIT3,RNI, BIT, PERO,BIT, LITA,GRPO,BIT, BIT, PLUS,BIT, COM1,NMC ,ETIB,/*
0    1    2    3    4    5    6    7    8    9    :    ;    <    =    >    ?    */
NU,  NU,  NU,  NU,  NU,  NU,  NU,  NU,  NU,  NU,  BIT, REFC,TGO4,VI,  TGC4,BIT, /*
@    A    B    C    D    E    F    G    H    I    J    K    L    M    N    O    */
BIT, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, /*
P    Q    R    S    T    U    V    W    X    Y    Z    [    \    ]     ^    _    */
NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, DSO, BIT, DSC1, BIT, BIT, /*
`    a    b    c    d    e    f    g    h    i    j    k    l    m    n    o    */
BIT, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, /*
p    q    r    s    t    u    v    w    x    y    z    {    |    }    ~    127  */
NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, BIT, BIT, BIT, BIT, NON,
BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT,
BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT,
BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT,
BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT,
BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT,
BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT,
BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT,
BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, BIT, NON
};
/*      bit  nmc  nu   nms  spc  non  ee   eob  rs   com  eti  grpo lit  lita
        dso  pero plus refc rni  tagc tago vi   */
#undef BIT
#undef NMC
#undef NU
#undef NMS
#undef SPC
#undef NON
#undef EE
#undef EOB
#undef RS
#undef COM1
#undef ETIB
#undef GRPO
#undef LIT3
#undef LITA
#undef DSO
#undef DSC
#undef PERO
#undef PLUS
#undef REFC
#undef RNI
#undef TGC4
#undef TGO4
#undef VI
/* LEXSD: Lexical scan table for SGML declaration.
*/

/* Symbols for SGML character set divisions. */
#define SIG     0   /* Significant SGML characters. */
#define DAT     1   /* DATACHAR  Not significant, and not non-sgml. */
#define NU      2   /* NUMERAL            Numerals */
#define NMS     3   /* NAMESTRT           Lower and uppercase letters */
#define SPC     4   /* SPACE     32 13 09 Space; includes RE TAB */
#define NON     5   /* NONCHAR   NONSGML */
#define EE      6   /* Function  26 00    EE: entity end (end of file) */
#define EOB     7   /* NONCHAR   28       End disk buffer. */
#define RS      8   /* Function  10       RS: record start (line feed) */
/* Symbols for SGML delimiter roles in SGML declaration. */
#define COM1    9   /* -    Actually COM[1]; also COM[2]. */
#define LIT3   10   /* "    */
#define LITA   11   /* '    */
#define TGC4   12   /* >    Also MDC, PIC */

UNCH lexsd[256] = { /*
000  001                          bs   tab  lf   home ff   cr   so   si   */
EE , NON, NON, NON, NON, NON, NON, NON, NON, SPC, RS,  NON, NON, SPC, NON, NON, /*
                                        eof  esc  rt   left up   down */
NON, NON, NON, NON, NON, NON, NON, NON, NON, NON, EE , NON, EOB, NON, NON, NON, /*
032  !    "    #    $    %    &    '    (    )    *    +    ,    -    .    /    */
SPC, SIG, LIT3,SIG, DAT, SIG ,SIG, LITA,SIG, SIG, SIG, SIG, SIG, COM1,SIG ,SIG,/*
0    1    2    3    4    5    6    7    8    9    :    ;    <    =    >    ?    */
NU,  NU,  NU,  NU,  NU,  NU,  NU,  NU,  NU,  NU,  SIG, SIG, SIG, SIG, TGC4,SIG, /*
@    A    B    C    D    E    F    G    H    I    J    K    L    M    N    O    */
SIG, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, /*
P    Q    R    S    T    U    V    W    X    Y    Z    [    \    ]     ^    _    */
NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, SIG, DAT, SIG, SIG, SIG, /*
`    a    b    c    d    e    f    g    h    i    j    k    l    m    n    o    */
DAT, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, /*
p    q    r    s    t    u    v    w    x    y    z    {    |    }    ~    127  */
NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, SIG, SIG, SIG, SIG, NON,
DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT,
DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT,
DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT,
DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT,
DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT,
DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT,
DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT,
DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, DAT, NON
};

#undef SIG 
#undef DAT 
#undef NON 
#undef NU  
#undef NMS 
#undef SPC 
#undef EE  
#undef EOB 
#undef RS  
#undef COM1
#undef LIT3
#undef LITA
#undef TGC4

/* LEXTRAN: Translation table for SGML names.
*/
UNCH lextran[256] = { /*
000  001                          bs   tab  lf  home  ff   cr   so   si   */
0  , 1  , 2  , 3  , 4  , 5  , 6  , 7  , 8  , 9  , 10 , 11 , 12 , 13 , 14 , 15 , /*
                                        eof  esc  rt   left up   down */
16 , 17 , 18 , 19 , 20 , 21 , 22 , 23 , 24 , 25 , 26 , 27 , 28 , 29 , 30 , 31 , /*
space!    "    #    $    %    &    '    (    )    *    +    ,    -    .    /    */
32 , 33 , 34 , 35 , 36 , 37 , 38 , 39 , 40 , 41 , 42 , 43 , 44 , 45 , 46 , 47 , /*
0    1    2    3    4    5    6    7    8    9    :    ;    <    =    >    ?    */
48 , 49 , 50 , 51 , 52 , 53 , 54 , 55 , 56 , 57 , 58 , 59 , 60 , 61 , 62 , 63 , /*
@    A    B    C    D    E    F    G    H    I    J    K    L    M    N    O    */
64 , 65 , 66 , 67 , 68 , 69 , 70 , 71 , 72 , 73 , 74 , 75 , 76 , 77 , 78 , 79 , /*
P    Q    R    S    T    U    V    W    X    Y    Z    [    \    ]    ^    _    */
80 , 81 , 82 , 83 , 84 , 85 , 86 , 87 , 88 , 89 , 90 , 91 , 92 , 93 , 94 , 95 , /*
`    a    b    c    d    e    f    g    h    i    j    k    l    m    n    o    */
96 , 65 , 66 , 67 , 68 , 69 , 70 , 71 , 72 , 73 , 74 , 75 , 76 , 77 , 78 , 79 , /*
p    q    r    s    t    u    v    w    x    y    z    {    |    }    ~    127  */
80 , 81 , 82 , 83 , 84 , 85 , 86 , 87 , 88 , 89 , 90 , 123, 124, 125, 126, 127,
128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,
176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191,
192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223,
224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255
};
/* LEXTOKE: Lexical class table for tokenization scan.
*/
#include "lextoke.h"          /* Symbols for tokenization lexical classes. */
UNCH lextoke[256] = { /*

000  001                     bs  tab  lf  home  ff   cr         */
INV, INV, INV, INV, INV, INV, INV, INV, INV, SEP, REC, INV, INV, REC, INV, INV, /*
                              eof  esc  rt   left up   down */
INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, EOB, INV, INV, INV, /*
space!    "    #    $    %    &    '    (    )    *    +    ,    -    .    /    */
SP , INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, NMC, NMC, INV, /*
0    1    2    3    4    5    6    7    8    9    :    ;    <    =    >    ?    */
NU , NU , NU , NU , NU , NU , NU , NU , NU , NU , INV, INV, INV, INV, INV, INV, /*
@    A    B    C    D    E    F    G    H    I    J    K    L    M    N    O    */
INV, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, /*
P    Q    R    S    T    U    V    W    X    Y    Z    [    \    ]    ^    _    */
NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, INV, INV, INV, INV, INV, /*
`    a    b    c    d    e    f    g    h    i    j    k    l    m    n    o    */
INV, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, /*
p    q    r    s    t    u    v    w    x    y    z    {    |    }    ~    127  */
NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, NMS, INV, INV, INV, INV, INV,
INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV
};

/* This table maps ASCII to the system character set. */
int iso646charset[] = {
0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
};

/* This table maps the C0 part of ISO646 to the system character set. */
/* We through in 32 and 127 for free, since ISO 2022 maps them in
automatically. */
int iso646C0charset[] = {
0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
32, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, 127,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
};

/* This table maps the G0 part of ISO646 to the system character set. */
int iso646G0charset[] = {
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
};

int iso8859_1charset[] = {
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,
176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191,
192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223,
224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
};

int iso6429C1charset[] = {
128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED, UNUSED,
};
