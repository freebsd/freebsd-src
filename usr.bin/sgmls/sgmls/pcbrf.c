/* PCBRF: Parse tables for reference concrete syntax.
*/
#include "config.h"
#include "entity.h"           /* Templates for entity control blocks. */
#include "action.h"           /* Action names for all parsing. */
#include "synxtrn.h"          /* Declarations for concrete syntax constants. */
#include "adl.h"              /* Definitions for attribute list processing. */
/* PCBCONM: State and action table for content parse of mixed content.
            Initial state assumes a start-tag was just processed.
*/
/* Symbols for state names (end with a number). */
#define ET0     0   /* Markup found or buffer flushed; no data. */
#define DA0     2   /* Data in buffer. */
#define DA1     4   /* Data and space in buffer. */
#define ER0     6   /* ERO found; start lookahead buffer. */
#define CR0     8   /* CRO found (ERO, RNI). */
#define RS0    10   /* RS found; possible SR 3-6. */
#define ME0    12   /* MSC found; possible SR26. */
#define ME1    14   /* MSC, MSC found. */
#define ES0    16   /* TAGO found; start lookahead buffer. */
#define EE0    18   /* End-tag start (TAGO,ETI); move to lookahead buffer. */
#define NE0    20   /* End-tag start (TAGO,NET); process NET if not end-tag. */
#define MD0    22   /* MDO found (TAGO, MDO[2]). */
#define MC0    24   /* MDO, COM found. */
#define SC0    26   /* COM found; possible SR19-20. */
#define SP0    28   /* Space found; data pending; possible SR7 or SR9. */
#define SR0    30   /* SPCR found; possible SR7 or SR9. */
#define TB0    32   /* TAB found; possible SR7 or SR9. */

int pcbcnet = ET0;            /* PCBCONM: markup found or data buffer flushed.*/
int pcbcnda = DA0;            /* PCBCONM: data in buffer. */

static UNCH
/*      free nu   nmc  nms  spc  non  ee   eob  rs   re   sep  cde  nsc  ero
        nmre com  eti  net  lit  spcr mdo  msc  mso  pio  rni  tagc tago fce   */
et0 []={DA0 ,DA0 ,DA0 ,DA0 ,SP0 ,ET0 ,ET0 ,ET0 ,RS0 ,ET0 ,TB0 ,DA0 ,ET0 ,ER0 ,
        ET0 ,SC0 ,DA0 ,ET0 ,ET0 ,SR0 ,DA0 ,ME0 ,ET0 ,DA0 ,ET0 ,DA0 ,ES0 ,ET0 },/*et0*/
et0a[]={DAS_,DAS_,DAS_,DAS_,DAS_,NON_,GET_,GET_,RSR_,SR2_,DAS_,DAS_,NSC_,LAS_,
        REF_,NOP_,DAS_,NED_,SR10,DAS_,DAS_,NOP_,SR25,DAS_,SR11,DAS_,LAS_,FCE_},

da0 []={DA0 ,DA0 ,DA0 ,DA0 ,DA1 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,DA0 ,ET0 ,ET0 ,
        ET0 ,ET0 ,DA0 ,ET0 ,ET0 ,ET0 ,DA0 ,ET0 ,ET0 ,DA0 ,DA0 ,DA0 ,ET0 ,ET0 },/*da0*/
da0a[]={NOP_,NOP_,NOP_,NOP_,NOP_,DAF_,DAF_,DAF_,DAF_,DAF_,DAF_,NOP_,DAF_,DAF_,
        DAF_,DAF_,NOP_,DAF_,DAF_,DAF_,NOP_,DAF_,DAF_,NOP_,NOP_,NOP_,DAF_,DAF_},

da1 []={DA0 ,DA0 ,DA0 ,DA0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,DA0 ,ET0 ,ET0 ,
        ET0 ,ET0 ,DA0 ,ET0 ,ET0 ,ET0 ,DA0 ,ET0 ,ET0 ,DA0 ,DA0 ,DA0 ,ET0 ,ET0 },/*da1*/
da1a[]={NOP_,NOP_,NOP_,NOP_,DAR_,DAF_,DAF_,DAR_,DAF_,DAR_,DAR_,NOP_,DAF_,DAF_,
        DAF_,DAF_,NOP_,DAF_,DAF_,DAR_,NOP_,DAF_,DAF_,NOP_,NOP_,NOP_,DAF_,DAF_},

er0 []={ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ER0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,
        ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,CR0 ,ET0 ,ET0 ,ET0 },/*er0*/
er0a[]={LAF_,LAF_,LAF_,ER_ ,LAF_,LAF_,LAF_,GET_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,
        LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAM_,LAF_,LAF_,LAF_},

/*      free nu   nmc  nms  spc  non  ee   eob  rs   re   sep  cde  nsc  ero
        nmre com  eti  net  lit  spcr mdo  msc  mso  pio  rni  tagc tago fce   */
cr0 []={ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,CR0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,
        ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 },/*cr0*/
cr0a[]={NLF_,CRN_,NLF_,CRA_,NLF_,NLF_,NLF_,GET_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,
        NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_},

rs0 []={ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,RS0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,
        ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 },/*rs0*/
rs0a[]={SR3_,SR3_,SR3_,SR3_,SR4_,SR3_,SR3_,GET_,SR3_,SR5_,SR4_,SR3_,SR3_,SR3_,
        SR3_,SR3_,SR3_,NED_,SR3_,SR4_,SR3_,SR3_,SR3_,SR3_,SR3_,SR3_,SR3_,SR3_},

me0 []={ET0, ET0, ET0, ET0, ET0 ,ET0, ET0, ME0, ET0 ,ET0 ,ET0 ,ET0, ET0, ET0,
        ET0 ,ET0 ,ET0 ,ET0, ET0, ET0, ET0, ME1 ,ET0, ET0, ET0 ,ET0, ET0, ET0 },/*me0*/
me0a[]={SR26,SR26,SR26,SR26,SR26,SR26,SR26,GET_,SR26,SR26,SR26,SR26,SR26,SR26,
        SR26,SR26,SR26,SR26,SR26,SR26,SR26,NOP_,SR26,SR26,SR26,SR26,SR26,SR26},

me1 []={ET0, ET0, ET0, ET0, ET0 ,ET0, ET0, ME1, ET0 ,ET0 ,ET0 ,ET0, ET0, ET0,
        ET0 ,ET0 ,ET0 ,ET0, ET0, ET0, ET0, ET0 ,ET0, ET0, ET0 ,ET0, ET0, ET0 },/*me1*/
me1a[]={RBR_,RBR_,RBR_,RBR_,RBR_,RBR_,RBR_,GET_,RBR_,RBR_,RBR_,RBR_,RBR_,RBR_,
        RBR_,RBR_,RBR_,RBR_,RBR_,RBR_,RBR_,RBR_,RBR_,RBR_,RBR_,MSE_,RBR_,RBR_},

/*      free nu   nmc  nms  spc  non  ee   eob  rs   re   sep  cde  nsc  ero
        nmre com  eti  net  lit  spcr mdo  msc  mso  pio  rni  tagc tago fce   */
es0 []={ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ES0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,
        ET0 ,ET0 ,EE0 ,NE0 ,ET0 ,ET0 ,MD0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 },/*es0*/
es0a[]={LAF_,LAF_,LAF_,STG_,LAF_,LAF_,LAF_,GET_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,
        LAF_,LAF_,LAM_,LAM_,LAF_,LAF_,LAM_,LAF_,LAF_,PIS_,LAF_,NST_,LAF_,LAF_},

ee0 []={ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,EE0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,
        ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 },/*ee0*/
ee0a[]={LAF_,LAF_,LAF_,ETG_,LAF_,LAF_,LAF_,GET_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,
        LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,NET_,LAF_,LAF_},

ne0 []={ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,NE0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,
        ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 },/*ne0*/
ne0a[]={NLF_,NLF_,NLF_,ETG_,NLF_,NLF_,NLF_,GET_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,
        NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NET_,NLF_,NLF_},

/*      free nu   nmc  nms  spc  non  ee   eob  rs   re   sep  cde  nsc  ero
        nmre com  eti  net  lit  spcr mdo  msc  mso  pio  rni  tagc tago fce   */
md0 []={ET0, ET0, ET0, ET0, ET0 ,ET0, ET0, MD0, ET0 ,ET0 ,ET0 ,ET0, ET0, ET0,
        ET0 ,MC0 ,ET0 ,ET0, ET0, ET0, ET0, ET0 ,ET0, ET0, ET0 ,ET0, ET0, ET0 },/*md0*/
md0a[]={LAF_,LAF_,LAF_,MD_ ,LAF_,LAF_,LAF_,GET_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,
        LAF_,LAM_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,MSS_,LAF_,LAF_,MDC_,LAF_,LAF_},

mc0 []={ET0, ET0, ET0, ET0, ET0, ET0 ,ET0, MC0, ET0 ,ET0, ET0 ,ET0, ET0, ET0,
        ET0 ,ET0 ,ET0 ,ET0, ET0, ET0, ET0, ET0 ,ET0 ,ET0 ,ET0 ,ET0, ET0, ET0 },/*mc0*/
mc0a[]={NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,GET_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,
        NLF_,MDC_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_},

sc0 []={ET0, ET0, ET0, ET0, ET0, ET0 ,ET0, SC0, ET0 ,ET0, ET0 ,ET0, ET0, ET0,
        ET0 ,ET0 ,ET0 ,ET0, ET0, ET0, ET0, ET0 ,ET0 ,ET0 ,ET0 ,ET0, ET0, ET0 },/*sc0*/
sc0a[]={SR19,SR19,SR19,SR19,SR19,SR19,SR19,GET_,SR19,SR19,SR19,SR19,SR19,SR19,
        SR19,SR20,SR19,SR19,SR19,SR19,SR19,SR19,SR19,SR19,SR19,SR19,SR19,SR19},

/*      free nu   nmc  nms  spc  non  ee   eob  rs   re   sep  cde  nsc  ero
        nmre com  eti  net  lit  spcr mdo  msc  mso  pio  rni  tagc tago fce   */
sp0 []={DA0 ,DA0 ,DA0 ,DA0 ,ET0 ,ET0 ,ET0 ,SP0 ,ET0 ,ET0 ,ET0 ,DA0 ,DA0 ,ET0 ,
        ET0 ,ET0 ,DA0 ,ET0 ,ET0 ,ET0 ,DA0 ,ET0 ,ET0 ,DA0 ,DA0 ,DA0 ,ET0 ,ET0 },/*sp0*/
sp0a[]={NOP_,NOP_,NOP_,NOP_,SR9_,DAF_,DAF_,GTR_,DAF_,SR7_,SR9_,NOP_,NOP_,DAF_,
        DAF_,DAF_,NOP_,DAF_,DAF_,SR9_,NOP_,DAF_,DAF_,NOP_,NOP_,NOP_,DAF_,DAF_},

sr0 []={ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,SR0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,
        ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 },/*sr0*/
sr0a[]={SR8_,SR8_,SR8_,SR8_,SR9_,SR8_,SR8_,GET_,SR8_,SR7_,SR9_,SR8_,SR8_,SR8_,
        SR8_,SR8_,SR8_,SR8_,SR8_,SR9_,SR8_,SR8_,SR8_,SR8_,SR8_,SR8_,SR8_,SR8_},

tb0 []={ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,TB0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,
        ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 },/*tb0*/
tb0a[]={SR1_,SR1_,SR1_,SR1_,SR9_,SR1_,SR1_,GET_,SR1_,SR7_,SR9_,SR1_,SR1_,SR1_,
        SR1_,SR1_,SR1_,SR1_,SR1_,SR9_,SR1_,SR1_,SR1_,SR1_,SR1_,SR1_,SR1_,SR1_},

/*      free nu   nmc  nms  spc  non  ee   eob  rs   re   sep  cde  nsc  ero
        nmre com  eti  net  lit  spcr mdo  msc  mso  pio  rni  tagc tago fce   */

*conmtab[] = {et0, et0a, da0, da0a, da1, da1a, er0, er0a, cr0, cr0a, rs0, rs0a,
              me0, me0a, me1, me1a, es0, es0a, ee0, ee0a, ne0, ne0a, md0, md0a,
              mc0, mc0a, sc0, sc0a, sp0, sp0a, sr0, sr0a, tb0, tb0a };
struct parse pcbconm = {"CONM", lexcnm, conmtab, 0, 0, 0, 0};
#undef ET0
#undef DA0
#undef DA1
#undef ER0
#undef CR0
#undef RS0
#undef ME0
#undef ME1
#undef ES0
#undef EE0
#undef NE0
#undef MD0
#undef MC0
#undef SC0
#undef SP0
#undef SR0
#undef TB0
/* PCBCONE: State and action table for content parse of element content.
            Initial state assumes a start-tag was just processed.
*/
/* Symbols for state names (end with a number). */
#define ET2     0   /* Markup found. */
#define ER2     2   /* ERO found; start lookahead buffer. */
#define CR2     4   /* CRO found (ERO, RNI). */
#define RS2     6   /* RS found; possible SR 3-6 if they were declared. */
#define ME2     8   /* MSC found. */
#define ME3    10   /* MSC, MSC found. */
#define ES2    12   /* TAGO found; start lookahead buffer. */
#define EE2    14   /* End-tag start (TAGO,ETI); move to lookahead buffer. */
#define NE2    16   /* End-tag start (TAGO,NET); process NET if not end-tag. */
#define MD2    18   /* MDO found (TAGO, MDO[2]). */
#define MC2    20   /* MDO, COM found. */
#define SC2    22   /* COM found; possible SR19-20 if they were mapped. */
#define SP2    24   /* Space found; possible SR7 or SR9. */
#define SR2    26   /* SPCR found; possible SR7 or SR9. */
#define TB2    28   /* TAB found; possible SR7 or SR9. */

static UNCH
/*      free nu   nmc  nms  spc  non  ee   eob  rs   re   sep  cde  nsc  ero
        nmre com  eti  net  lit  spcr mdo  msc  mso  pio  rni  tagc tago fce   */
et2 []={ET2 ,ET2 ,ET2 ,ET2 ,SP2 ,ET2 ,ET2 ,ET2 ,RS2 ,ET2 ,TB2 ,ET2 ,ET2 ,ER2 ,
        ET2 ,SC2 ,ET2 ,ET2 ,ET2 ,SR2 ,ET2 ,ME2 ,ET2 ,ET2 ,ET2 ,ET2 ,ES2 ,ET2 },/*et2*/
et2a[]={DCE_,DCE_,DCE_,DCE_,NOP_,DCE_,GET_,GET_,RS_ ,SR2_,NOP_,DCE_,DCE_,LAS_,
        NOP_,NOP_,DCE_,NED_,SR10,NOP_,DCE_,NOP_,DCE_,DCE_,SR11,DCE_,LAS_,DCE_},

er2 []={ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ER2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,
        ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,CR2 ,ET2 ,ET2 ,ET2 },/*er2*/
er2a[]={LAF_,LAF_,LAF_,ER_ ,LAF_,LAF_,LAF_,GET_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,
        LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAM_,LAF_,LAF_,LAF_},

cr2 []={ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,CR2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,
        ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 },/*cr2*/
cr2a[]={NLF_,CRN_,NLF_,CRA_,NLF_,NLF_,NLF_,GET_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,
        NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_},

rs2 []={ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,RS2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,
        ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 },/*rs2*/
rs2a[]={SR3_,SR3_,SR3_,SR3_,SR4_,SR3_,SR3_,GET_,SR3_,SR5_,SR4_,SR3_,SR3_,SR3_,
        SR3_,SR3_,SR3_,NED_,SR3_,SR4_,SR3_,SR3_,SR3_,SR3_,SR3_,SR3_,SR3_,SR3_},

/*      free nu   nmc  nms  spc  non  ee   eob  rs   re   sep  cde  nsc  ero
        nmre com  eti  net  lit  spcr mdo  msc  mso  pio  rni  tagc tago fce   */
me2 []={ET2, ET2, ET2, ET2, ET2 ,ET2, ET2, ME2, ET2 ,ET2 ,ET2 ,ET2, ET2, ET2,
        ET2 ,ET2, ET2 ,ET2, ET2, ET2, ET2, ME3 ,ET2, ET2, ET2 ,ET2, ET2, ET2 },/*me2*/
me2a[]={SR26,SR26,SR26,SR26,SR26,SR26,SR26,GET_,SR26,SR26,SR26,SR26,SR26,SR26,
        SR26,SR26,SR26,SR26,SR26,SR26,SR26,NOP_,SR26,SR26,SR26,SR26,SR26,SR26},

me3 []={ET2, ET2, ET2, ET2, ET2 ,ET2, ET2, ME3, ET2 ,ET2 ,ET2 ,ET2, ET2, ET2,
        ET2 ,ET2, ET2 ,ET2, ET2, ET2, ET2, ET2 ,ET2, ET2, ET2 ,ET2, ET2, ET2 },/*me3*/
me3a[]={RBR_,RBR_,RBR_,RBR_,RBR_,RBR_,RBR_,GET_,RBR_,RBR_,RBR_,RBR_,RBR_,RBR_,
        RBR_,RBR_,RBR_,RBR_,RBR_,RBR_,RBR_,RBR_,RBR_,RBR_,RBR_,MSE_,RBR_,RBR_},

es2 []={ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ES2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,
        ET2 ,ET2 ,EE2 ,NE2 ,ET2 ,ET2 ,MD2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 },/*es2*/
es2a[]={LAF_,LAF_,LAF_,STG_,LAF_,LAF_,LAF_,GET_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,
        LAF_,LAF_,LAM_,LAM_,LAF_,LAF_,LAM_,LAF_,LAF_,PIS_,LAF_,NST_,LAF_,LAF_},

ee2 []={ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,EE2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,
        ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 },/*ee2*/
ee2a[]={LAF_,LAF_,LAF_,ETG_,LAF_,LAF_,LAF_,GET_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,
        LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,NET_,LAF_,LAF_},

/*      free nu   nmc  nms  spc  non  ee   eob  rs   re   sep  cde  nsc  ero
        nmre com  eti  net  lit  spc  mdo  msc  mso  pio  rni  tagc tago fce   */
ne2 []={ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,NE2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,
        ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 },/*ne2*/
ne2a[]={NLF_,NLF_,NLF_,ETG_,NLF_,NLF_,NLF_,GET_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,
        NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NET_,NLF_,NLF_},

md2 []={ET2, ET2, ET2, ET2, ET2 ,ET2, ET2, MD2, ET2 ,ET2 ,ET2 ,ET2, ET2, ET2,
        ET2 ,MC2, ET2 ,ET2, ET2, ET2, ET2, ET2 ,ET2, ET2, ET2 ,ET2, ET2, ET2 },/*md2*/
md2a[]={LAF_,LAF_,LAF_,MD_ ,LAF_,LAF_,LAF_,GET_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,
        LAF_,LAM_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,MSS_,LAF_,LAF_,MDC_,LAF_,LAF_},

mc2 []={ET2, ET2, ET2, ET2, ET2, ET2 ,ET2, MC2, ET2 ,ET2, ET2 ,ET2, ET2, ET2,
        ET2 ,ET2, ET2 ,ET2, ET2, ET2, ET2, ET2 ,ET2 ,ET2 ,ET2 ,ET2, ET2, ET2 },/*mc2*/
mc2a[]={NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,GET_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,
        NLF_,MDC_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_,NLF_},

sc2 []={ET2, ET2, ET2, ET2, ET2, ET2 ,ET2, SC2, ET2 ,ET2, ET2 ,ET2, ET2, ET2,
        ET2 ,ET2 ,ET2 ,ET2, ET2, ET2, ET2, ET2 ,ET2 ,ET2 ,ET2 ,ET2, ET2, ET2 },/*sc2*/
sc2a[]={SR19,SR19,SR19,SR19,SR19,SR19,SR19,GET_,SR19,SR19,SR19,SR19,SR19,SR19,
        SR19,SR20,SR19,SR19,SR19,SR19,SR19,SR19,SR19,SR19,SR19,SR19,SR19,SR19},

/*      free nu   nmc  nms  spc  non  ee   eob  rs   re   sep  cde  nsc  ero
        nmre com  eti  net  lit  spc  mdo  msc  mso  pio  rni  tagc tago fce   */
sp2 []={ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,SP2 ,RS2 ,ET2 ,ET2 ,ET2 ,ET2 ,ER2 ,
        ET2 ,SC2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ME2 ,ET2 ,ET2 ,ET2 ,ET2 ,ES2 ,ET2 },/*sp2*/
sp2a[]={DCE_,DCE_,DCE_,DCE_,SR9_,DCE_,GET_,GET_,RS_ ,SR7_,SR9_,DCE_,DCE_,LAS_,
        NOP_,NOP_,DCE_,NED_,SR10,SR9_,DCE_,LAS_,DCE_,DCE_,SR11,DCE_,LAS_,DCE_},

sr2 []={ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,SR2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,
        ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 },/*sr2*/
sr2a[]={SR8_,SR8_,SR8_,SR8_,SR9_,SR8_,SR8_,GET_,SR8_,SR7_,SR9_,SR8_,SR8_,SR8_,
        SR8_,SR8_,SR8_,SR8_,SR8_,SR9_,SR8_,SR8_,SR8_,SR8_,SR8_,SR8_,SR8_,SR8_},

tb2 []={ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,TB2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,
        ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 },/*tb2*/
tb2a[]={SR1_,SR1_,SR1_,SR1_,SR9_,SR1_,SR1_,GET_,SR1_,SR7_,SR9_,SR1_,SR1_,SR1_,
        SR1_,SR1_,SR1_,SR1_,SR1_,SR9_,SR1_,SR1_,SR1_,SR1_,SR1_,SR1_,SR1_,SR1_},

*conetab[] = {et2, et2a, er2, er2a, cr2, cr2a, rs2, rs2a, me2, me2a, me3, me3a,
              es2, es2a, ee2, ee2a, ne2, ne2a, md2, md2a, mc2, mc2a, sc2, sc2a,
              sp2, sp2a, sr2, sr2a, tb2, tb2a  };
struct parse pcbcone = {"CONE", lexcnm, conetab, 0, 0, 0, 0};
#undef ET2
#undef ER2
#undef CR2
#undef RS2
#undef ME2
#undef ME3
#undef ES2
#undef EE2
#undef NE2
#undef MD2
#undef MC2
#undef SC2
#undef SP2
#undef SR2
#undef TB2
/* PCBCONR: State and action table for content parse of replaceable character
            data.  Initial state assumes a start-tag was just processed.
            Only entity references and character references are recognized.
*/
/* Symbols for state names (end with a number). */
#define ET4     0   /* Markup found or buffer flushed; no data. */
#define DA4     2   /* Data in buffer. */
#define ER4     4   /* ERO found; start lookahead buffer. */
#define CR4     6   /* CRO found (ER2, RNI). */
#define ES4     8   /* TAGO found; start lookahead buffer. */
#define EE4    10   /* End-tag start (TAGO,ETI); move to lookahead buffer. */
#define NE4    12   /* End-tag start (TAGO,NET); process NET if not end-tag. */

static UNCH
/*      free nu   nmc  nms  spc  non  ee   eob  rs   re   sep  cde  nsc  ero
        nmre com  eti  net  mdo  msc  mso  pero pio  rni  tagc tago  */
et4 []={DA4 ,DA4 ,DA4 ,DA4 ,DA4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,DA4 ,DA4 ,ET4 ,ER4 ,
        ET4 ,DA4 ,DA4 ,ET4 ,DA4 ,DA4 ,DA4 ,DA4 ,DA4 ,DA4 ,DA4 ,ES4 },/*et4*/
et4a[]={DAS_,DAS_,DAS_,DAS_,DAS_,NON_,EE_ ,GET_,RS_ ,REF_,DAS_,DAS_,NSC_,LAS_,
        REF_,DAS_,DAS_,NED_,DAS_,DAS_,DAS_,DAS_,DAS_,DAS_,DAS_,LAS_},

da4 []={DA4 ,DA4 ,DA4 ,DA4 ,DA4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,DA4 ,DA4 ,ET4 ,ET4 ,
        ET4 ,DA4 ,DA4 ,ET4 ,DA4 ,DA4 ,DA4 ,DA4 ,DA4 ,DA4 ,DA4 ,ET4 },/*da4*/
da4a[]={NOP_,NOP_,NOP_,NOP_,NOP_,DAF_,DAF_,DAF_,DAF_,DAF_,NOP_,NOP_,DAF_,DAF_,
        DAF_,NOP_,NOP_,DAF_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,DAF_},

er4 []={ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ER4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,
        ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,CR4 ,ET4 ,ET4 },/*er4*/
er4a[]={LAF_,LAF_,LAF_,ERX_,LAF_,LAF_,LAF_,GET_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,
        LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAM_,LAF_,LAF_},

cr4 []={ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,CR4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,
        ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 },/*cr4*/
cr4a[]={LAF_,CRN_,LAF_,CRA_,LAF_,LAF_,LAF_,GET_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,
        LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_},

/*      free nu   nmc  nms  spc  non  ee   eob  rs   re   sep  cde  nsc  ero
        nmre com  eti  net  mdo  msc  mso  pero pio  rni  tagc tago  */
es4 []={ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ES4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,
        ET4 ,ET4 ,EE4 ,NE4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 },/*es4*/
es4a[]={LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,GET_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,
        LAF_,LAF_,LAM_,LAM_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_},

ee4 []={ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,EE4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,
        ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 },/*ee4*/
ee4a[]={LAF_,LAF_,LAF_,ETC_,LAF_,LAF_,LAF_,GET_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,
        LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,NET_,LAF_},

ne4 []={EE4 ,EE4 ,EE4 ,ET4 ,EE4 ,EE4 ,EE4 ,NE4 ,EE4 ,EE4 ,EE4 ,EE4 ,EE4 ,EE4 ,
        EE4 ,EE4 ,EE4 ,EE4 ,EE4 ,EE4 ,EE4 ,EE4 ,EE4 ,EE4 ,ET4 ,EE4 },/*ne4*/
ne4a[]={RC2_,RC2_,RC2_,ETC_,RC2_,RC2_,RC2_,GET_,RC2_,RC2_,RC2_,RC2_,RC2_,RC2_,
        RC2_,RC2_,RC2_,RC2_,RC2_,RC2_,RC2_,RC2_,RC2_,RC2_,NET_,RC2_},

*conrtab[] = {et4, et4a, da4, da4a, er4, er4a, cr4, cr4a,
              es4, es4a, ee4, ee4a, ne4, ne4a};
struct parse pcbconr = {"CONR", lexcon, conrtab, 0, 0, 0, 0};
#undef ET4
#undef DA4
#undef ER4
#undef CR4
#undef ES4
#undef EE4
#undef NE4
/* PCBCONC: State and action table for content parse of character data.
            Initial state assumes a start-tag was just processed.
*/
/* Symbols for state names (end with a number). */
#define ET6     0   /* Markup found or buffer flushed; no data. */
#define DA6     2   /* Data in buffer. */
#define ES6     4   /* TAGO found; start lookahead buffer. */
#define EE6     6   /* End-tag start (TAGO,ETI); move to lookahead buffer. */
#define NE6     8   /* End-tag start (TAGO,NET); process NET if not end-tag. */

static UNCH
/*      free nu   nmc  nms  spc  non  ee   eob  rs   re   sep  cde  nsc  ero
        nmre com  eti  net  mdo  msc  mso  pero pio  rni  tagc tago  */
et6 []={DA6 ,DA6 ,DA6 ,DA6 ,DA6 ,ET6 ,ET6 ,ET6 ,ET6 ,ET6 ,DA6 ,DA6 ,ET6 ,DA6 ,
        ET6 ,DA6 ,DA6 ,ET6 ,DA6 ,DA6 ,DA6 ,DA6 ,DA6 ,DA6 ,DA6 ,ES6 },/*et6*/
et6a[]={DAS_,DAS_,DAS_,DAS_,DAS_,NON_,EOF_,GET_,RS_ ,REF_,DAS_,DAS_,NSC_,DAS_,
        REF_,DAS_,DAS_,NED_,DAS_,DAS_,DAS_,DAS_,DAS_,DAS_,DAS_,LAS_},

da6 []={DA6 ,DA6 ,DA6 ,DA6 ,DA6 ,ET6 ,ET6 ,ET6 ,ET6 ,ET6 ,DA6 ,DA6 ,ET6 ,ET6 ,
        ET6 ,DA6 ,DA6 ,ET6 ,DA6 ,DA6 ,DA6 ,DA6 ,DA6 ,DA6 ,DA6 ,ET6 },/*da6*/
da6a[]={NOP_,NOP_,NOP_,NOP_,NOP_,DAF_,DAF_,DAF_,DAF_,DAF_,NOP_,NOP_,DAF_,DAF_,
        DAF_,NOP_,NOP_,DAF_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,DAF_},

/*      free nu   nmc  nms  spc  non  ee   eob  rs   re   sep  cde  nsc  ero
        nmre com  eti  net  mdo  msc  mso  pero pio  rni  tagc tago  */
es6 []={ET6 ,ET6 ,ET6 ,ET6 ,ET6 ,ET6 ,ET6 ,ES6 ,ET6 ,ET6 ,ET6 ,ET6 ,ET6 ,ET6 ,
        ET6 ,ET6 ,EE6 ,NE6 ,ET6 ,ET6 ,ET6 ,ET6 ,ET6 ,ET6 ,ET6 ,ET6 },/*es6*/
es6a[]={LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,GET_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,
        LAF_,LAF_,LAM_,LAM_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_},

ee6 []={ET6 ,ET6 ,ET6 ,ET6 ,ET6 ,ET6 ,ET6 ,EE6 ,ET6 ,ET6 ,ET6 ,ET6 ,ET6 ,ET6 ,
        ET6 ,ET6 ,ET6 ,ET6 ,ET6 ,ET6 ,ET6 ,ET6 ,ET6 ,ET6 ,ET6 ,ET6 },/*ee6*/
ee6a[]={LAF_,LAF_,LAF_,ETC_,LAF_,LAF_,LAF_,GET_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,
        LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,NET_,LAF_},

ne6 []={EE6 ,EE6 ,EE6 ,ET6 ,EE6 ,EE6 ,EE6 ,NE6 ,EE6 ,EE6 ,EE6 ,EE6 ,EE6 ,EE6 ,
        EE6 ,EE6 ,EE6 ,EE6 ,EE6 ,EE6 ,EE6 ,EE6 ,EE6 ,EE6 ,ET6 ,EE6 },/*ne6*/
ne6a[]={RC2_,RC2_,RC2_,ETC_,RC2_,RC2_,RC2_,GET_,RC2_,RC2_,RC2_,RC2_,RC2_,RC2_,
        RC2_,RC2_,RC2_,RC2_,RC2_,RC2_,RC2_,RC2_,RC2_,RC2_,NET_,RC2_},

*conctab[] = {et6, et6a, da6, da6a, es6, es6a, ee6, ee6a, ne6, ne6a};
struct parse pcbconc = {"CONC", lexcon, conctab, 0, 0, 0, 0};
#undef ET6
#undef DA6
#undef ES6
#undef EE6
#undef NE6
/* PCBPRO: State and action table for prolog parse.
           Initial state assumes document just began.
*/
/* Symbols for state names (end with a number). */
#define ET7     0   /* Markup found. */
#define ES7     2   /* TAGO found; start lookahead buffer. */
#define MD7     4   /* MDO found (TAGO, MDO[2]). */
#define MC7     6   /* MDO, COM found. */

static UNCH
/*      free nu   nmc  nms  spc  non  ee   eob  rs   re   sep  cde  nsc  ero
        nmre com  eti  net  mdo  msc  mso  pero pio  rni  tagc tago */
et7 []={ET7 ,ET7 ,ET7 ,ET7 ,ET7 ,ET7 ,ET7 ,ET7 ,ET7 ,ET7 ,ET7 ,ET7 ,ET7 ,ET7 ,
        ET7 ,ET7 ,ET7 ,ET7 ,ET7 ,ET7 ,ET7 ,ET7 ,ET7 ,ET7 ,ET7 ,ES7 },/*et7*/
et7a[]={DCE_,DCE_,DCE_,DCE_,NOP_,DCE_,EE_ ,GET_,RS_ ,NOP_,NOP_,DCE_,DCE_,DCE_,
        DCE_,DCE_,DCE_,DCE_,DCE_,DCE_,DCE_,DCE_,DCE_,DCE_,DCE_,LAS_},

es7 []={ET7 ,ET7 ,ET7 ,ET7 ,ET7 ,ET7 ,ET7 ,ES7 ,ET7 ,ET7 ,ET7 ,ET7 ,ET7 ,ET7 ,
        ET7 ,ET7 ,ET7 ,ET7 ,MD7 ,ET7 ,ET7 ,ET7 ,ET7 ,ET7 ,ET7 ,ET7 },/*es7*/
es7a[]={PEP_,PEP_,PEP_,STE_,PEP_,PEP_,PEP_,GET_,PEP_,PEP_,PEP_,PEP_,PEP_,PEP_,
        PEP_,PEP_,PEP_,PEP_,LAM_,PEP_,PEP_,PEP_,PIS_,PEP_,STE_,PEP_},

md7 []={ET7, ET7, ET7, ET7, ET7 ,ET7, ET7, MD7, ET7 ,ET7 ,ET7 ,ET7, ET7, ET7,
        ET7, MC7, ET7, ET7, ET7, ET7 ,ET7, ET7, ET7, ET7 ,ET7, ET7 },/*md7*/
md7a[]={LAF_,LAF_,LAF_,DTD_,LAF_,LAF_,LAF_,GET_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,
        LAF_,LAM_,LAF_,LAF_,LAF_,LAF_,MSP_,LAF_,LAF_,LAF_,NOP_,LAF_},

mc7 []={ET7, ET7, ET7, ET7, ET7, ET7 ,ET7, MC7, ET7 ,ET7, ET7 ,ET7, ET7, ET7,
        ET7, ET7, ET7, ET7, ET7, ET7 ,ET7 ,ET7, ET7 ,ET7 ,ET7, ET7 },/*mc7*/
mc7a[]={LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,GET_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,
        LAF_,MDC_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_},

*protab[] = {et7, et7a, es7, es7a, md7, md7a, mc7, mc7a};
struct parse pcbpro = {"PRO", lexcon, protab, 0, 0, 0, 0};
#undef ET7
#undef ES7
#undef MD7
#undef MC7
/* PCBMDS: State and action table for parse of markup declaration subset.
           Initial state assumes subset just began (MSO found).
*/
/* Symbols for state names (end with a number). */
#define ET8     0   /* Markup found. */
#define ER8     2   /* PERO found; start lookahead buffer. */
#define ME8     4   /* MSC found. */
#define ME9     6   /* MSC, MSC found. */
#define ES8     8   /* TAGO found; start lookahead buffer. */
#define MD8    10   /* MDO found (TAGO, MDO[2]). */
#define MC8    12   /* MDO, CD found. */
#define DC8    14   /* Data characters found (erroneously). */

static UNCH
/*      free nu   nmc  nms  spc  non  ee   eob  rs   re   sep  cde  nsc  ero
        nmre com  eti  net  mdo  msc  mso  pero pio  rni  tagc tago */
et8 []={DC8 ,DC8 ,DC8 ,DC8 ,ET8 ,DC8 ,ET8 ,ET8 ,ET8 ,ET8 ,ET8 ,DC8 ,DC8 ,DC8 ,
        DC8 ,DC8 ,DC8 ,DC8 ,DC8 ,ME8 ,DC8 ,ER8 ,DC8 ,DC8 ,DC8 ,ES8 },/*et8*/
et8a[]={NOP_,NOP_,NOP_,NOP_,NOP_,SYS_,GET_,GET_,RS_ ,NOP_,NOP_,NOP_,SYS_,NOP_,
        NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_},

er8 []={DC8 ,DC8 ,DC8 ,ET8 ,DC8 ,DC8 ,DC8 ,ER8 ,DC8 ,DC8 ,DC8 ,DC8 ,DC8 ,DC8 ,
        DC8 ,DC8 ,DC8 ,DC8 ,DC8 ,DC8 ,DC8 ,DC8 ,DC8 ,DC8 ,DC8 ,DC8 },/*er8*/
er8a[]={NOP_,NOP_,NOP_,PER_,NOP_,SYS_,NOP_,GET_,NOP_,NOP_,NOP_,NOP_,SYS_,NOP_,
        NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_},

me8 []={ET8, ET8, ET8, ET8, ET8 ,ET8, ET8, ME8, ET8 ,ET8 ,ET8 ,ET8, ET8, ET8,
        ET8 ,ET8, ET8 ,ET8, ET8, ME9 ,ET8, ET8, ET8, ET8 ,ET8, ET8 },/*me8*/
me8a[]={DTE_,DTE_,DTE_,DTE_,DTE_,DTE_,DTE_,GET_,DTE_,DTE_,DTE_,DTE_,DTE_,DTE_,
        DTE_,DTE_,DTE_,DTE_,DTE_,NOP_,DTE_,DTE_,DTE_,DTE_,DTE_,DTE_},

me9 []={DC8, DC8, DC8, DC8, DC8 ,DC8, DC8, ME9, DC8 ,DC8 ,DC8 ,DC8, DC8, DC8,
        DC8 ,DC8, DC8 ,DC8, DC8, DC8 ,DC8, DC8, DC8, DC8 ,ET8, DC8 },/*me9*/
me9a[]={NOP_,NOP_,NOP_,NOP_,NOP_,SYS_,NOP_,GET_,NOP_,NOP_,NOP_,NOP_,SYS_,NOP_,
        NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,MSE_,NOP_},

/*      free nu   nmc  nms  spc  non  ee   eob  rs   re   sep  cde  nsc  ero
        nmre com  eti  net  mdo  msc  mso  pero pio  rni  tagc tago */
es8 []={DC8 ,DC8 ,DC8 ,DC8 ,DC8 ,DC8 ,DC8 ,ES8 ,DC8 ,DC8 ,DC8 ,DC8 ,DC8 ,DC8 ,
        DC8 ,DC8 ,DC8 ,DC8 ,MD8 ,DC8 ,DC8 ,DC8 ,ET8 ,DC8 ,DC8 ,DC8 },/*es8*/
es8a[]={NOP_,NOP_,NOP_,NOP_,NOP_,SYS_,NOP_,GET_,NOP_,NOP_,NOP_,NOP_,SYS_,NOP_,
        NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,PIS_,NOP_,NOP_,NOP_},

md8 []={DC8, DC8, DC8, ET8, DC8 ,DC8, DC8, MD8, DC8 ,DC8 ,DC8 ,DC8, DC8, DC8,
        DC8 ,MC8, DC8 ,DC8, DC8, DC8 ,ET8, DC8, DC8, DC8 ,ET8, DC8 },/*md8*/
md8a[]={NOP_,NOP_,NOP_,MD_ ,NOP_,SYS_,NOP_,GET_,NOP_,NOP_,NOP_,NOP_,SYS_,NOP_,
        NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,MSS_,NOP_,NOP_,NOP_,NOP_,NOP_},

mc8 []={DC8, DC8, DC8, DC8, DC8, DC8 ,DC8, MC8, DC8 ,DC8, DC8 ,DC8, DC8, DC8,
        DC8 ,ET8, DC8 ,DC8, DC8, DC8 ,DC8 ,DC8, DC8 ,DC8 ,DC8, DC8 },/*mc8*/
mc8a[]={NOP_,NOP_,NOP_,NOP_,NOP_,SYS_,NOP_,GET_,NOP_,NOP_,NOP_,NOP_,SYS_,NOP_,
        NOP_,MDC_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_},

dc8 []={DC8 ,DC8 ,DC8 ,DC8 ,DC8 ,DC8 ,DC8 ,DC8 ,DC8 ,DC8 ,DC8 ,DC8 ,DC8 ,DC8 ,
        DC8 ,DC8 ,DC8 ,DC8 ,DC8 ,ET8 ,DC8 ,ET8 ,DC8 ,DC8 ,DC8 ,ET8 },/*dc8*/
dc8a[]={NOP_,NOP_,NOP_,NOP_,NOP_,SYS_,GET_,GET_,RS_ ,NOP_,NOP_,NOP_,SYS_,NOP_,
        NOP_,NOP_,NOP_,NOP_,NOP_,CIR_,NOP_,CIR_,NOP_,NOP_,NOP_,CIR_},

*mdstab[] = {et8, et8a, er8, er8a, me8, me8a, me9, me9a,
             es8, es8a, md8, md8a, mc8, mc8a, dc8, dc8a};
struct parse pcbmds = {"MDS", lexcon, mdstab, 0, 0, 0, 0};
#undef ET8
#undef ER8
#undef ME8
#undef ME9
#undef ES8
#undef MD8
#undef MC8
#undef DC8
/* PCBGRCM: State and action table for content model group.
            Groups can nest.  Reserved names are allowed.
            Data tag token groups are allowed.
            A non-reserved name or model group can have a suffix.
            Columns are based on LEXGRP.C.
*/
/* Symbols for state names (end with a number). */
#define TK1     0   /* Token expected: name, #CHARS, data tag grp, model. */
#define CO1     2   /* Connector between tokens expected. */
#define ER1     4   /* PERO found when token was expected. */
#define SP1     6   /* Name or model: suffix or connector expected. */
#define RN1     8   /* RNI found; possible #PCDATA. */
#define DG1    10   /* Data tag: group begun; name expected. */
#define DN1    12   /* Data tag: name found; SEQ connector expected. */
#define DT1    14   /* Data tag: ignore template and pattern; MSC expected. */
#define DR1    16   /* PERO found when data tag name was expected. */
#define LI1    18   /* Literal in data tag group; search for LIT. */
#define LA1    20   /* Literal in data tag group; search for LITA. */

static UNCH
/*       bit  nmc  nms  re   spc  non  ee   eob  rs   and  grpc grpo lit  lita
         dtgc dtgo opt  or   pero plus rep  rni  seq  refc */
tk01 []={TK1 ,TK1 ,SP1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,
         TK1 ,DG1 ,TK1 ,TK1 ,ER1 ,TK1 ,TK1 ,RN1 ,TK1 ,TK1 },/*tk1*/
tk01a[]={INV_,INV_,NAS_,NOP_,NOP_,SYS_,EOF_,GET_,RS_ ,INV_,INV_,GRP_,INV_,INV_,
         INV_,GRP_,INV_,INV_,NOP_,INV_,INV_,NOP_,INV_,INV_},

co01 []={TK1 ,TK1 ,TK1 ,CO1 ,CO1 ,CO1 ,CO1 ,CO1 ,CO1 ,TK1 ,SP1 ,TK1 ,TK1 ,TK1 ,
         TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 },/*co1*/
co01a[]={INV_,INV_,INV_,NOP_,NOP_,SYS_,EE_ ,GET_,RS_ ,AND ,GRPE,INV_,INV_,INV_,
         INV_,INV_,INV_,OR  ,INV_,INV_,INV_,INV_,SEQ ,INV_},

er01 []={TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,ER1 ,TK1 ,ER1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,
         TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 },/*er1*/
er01a[]={PCI_,PCI_,PER_,PCI_,PCI_,SYS_,PCI_,GET_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,
         PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_},

sp01 []={TK1 ,TK1 ,TK1 ,CO1 ,CO1 ,SP1 ,CO1 ,SP1 ,CO1 ,TK1 ,SP1 ,TK1 ,TK1 ,TK1 ,
         TK1 ,TK1 ,CO1 ,TK1 ,TK1 ,CO1 ,CO1 ,TK1 ,TK1 ,TK1 },/*sp1*/
sp01a[]={INV_,LEN_,LEN_,NOP_,NOP_,SYS_,EE_ ,GET_,RS_ ,AND ,GRPE,INV_,INV_,INV_,
         INV_,INV_,OPT ,OR  ,INV_,REP ,OREP,INV_,SEQ ,LEN_},

/*       bit  nmc  nms  spc  spc  non  ee   eob  rs   and  grpc grpo lit  lita
         dtgc dtgo opt  or   pero plus rep  rni  seq  refc  */
rn01 []={TK1 ,TK1 ,CO1 ,TK1 ,TK1 ,RN1 ,TK1 ,RN1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,
         TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 },/*rn1*/
rn01a[]={PCI_,PCI_,RNS_,PCI_,PCI_,SYS_,PCI_,GET_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,
         PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_},

dg01 []={TK1 ,TK1 ,DN1 ,DG1 ,DG1 ,DG1 ,DG1 ,DG1 ,DG1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,
         TK1 ,TK1 ,TK1 ,TK1 ,DR1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 },/*dg1*/
dg01a[]={INV_,INV_,NAS_,NOP_,NOP_,SYS_,EOF_,GET_,RS_ ,INV_,INV_,INV_,INV_,INV_,
         INV_,INV_,INV_,INV_,NOP_,INV_,INV_,INV_,INV_,INV_},

dn01 []={TK1 ,TK1 ,TK1 ,DN1 ,DN1 ,DN1 ,DN1 ,DN1 ,DN1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,
         TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,DT1 ,TK1 },/*dn1*/
dn01a[]={INV_,INV_,INV_,NOP_,NOP_,SYS_,EE_ ,GET_,RS_ ,INV_,INV_,INV_,INV_,INV_,
         INV_,INV_,INV_,INV_,INV_,INV_,INV_,INV_,DTAG,INV_},

dt01 []={TK1 ,TK1 ,TK1 ,DT1 ,DT1 ,DT1 ,DT1 ,DT1 ,DT1 ,TK1 ,DT1 ,DT1 ,LI1 ,LA1 ,
         CO1 ,TK1 ,TK1 ,DT1 ,DT1 ,TK1 ,TK1 ,TK1 ,DT1 ,TK1 },/*dt1*/
dt01a[]={INV_,INV_,INV_,NOP_,NOP_,SYS_,EE_ ,GET_,RS_ ,INV_,NOP_,NOP_,NOP_,NOP_,
         GRPE,INV_,INV_,NOP_,NOP_,INV_,INV_,INV_,NOP_,INV_},

/*       bit  nmc  nms  spc  spc  non  ee   eob  rs   and  grpc grpo lit  lita
         dtgc dtgo opt  or   pero plus rep  rni  seq  refc  */
dr01 []={TK1 ,TK1 ,DG1 ,TK1 ,TK1 ,DR1 ,TK1 ,DR1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,
         TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 },/*dr1*/
dr01a[]={PCI_,PCI_,PER_,PCI_,PCI_,SYS_,PCI_,GET_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,
         PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_},

li01 []={LI1 ,LI1 ,LI1 ,LI1 ,LI1 ,LI1 ,LI1 ,LI1 ,LI1 ,LI1 ,LI1 ,LI1 ,DT1 ,LI1 ,
         LI1 ,LI1 ,LI1 ,LI1 ,LI1 ,LI1 ,LI1 ,LI1 ,LI1 ,LI1 },/*li1*/
li01a[]={NOP_,NOP_,NOP_,NOP_,NOP_,SYS_,EOF_,GET_,RS_ ,NOP_,NOP_,NOP_,NOP_,NOP_,
         NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_},

la01 []={LA1 ,LA1 ,LA1 ,LA1 ,LA1 ,LA1 ,LA1 ,LA1 ,LA1 ,LA1 ,LA1 ,LA1 ,LA1 ,DT1 ,
         LA1 ,LA1 ,LA1 ,LA1 ,LA1 ,LA1 ,LA1 ,LA1 ,LA1 ,LA1 },/*la1*/
la01a[]={NOP_,NOP_,NOP_,NOP_,NOP_,SYS_,EOF_,GET_,RS_ ,NOP_,NOP_,NOP_,NOP_,NOP_,
         NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_},

*grcmtab[] = {tk01, tk01a, co01, co01a, er01, er01a, sp01, sp01a,
              rn01, rn01a, dg01, dg01a, dn01, dn01a, dt01, dt01a,
              dr01, dr01a, li01, li01a, la01, la01a};
struct parse pcbgrcm = {"GRCM", lexgrp, grcmtab, 0, 0, 0, 0};
#undef TK1
#undef CO1
#undef ER1
#undef SP1
#undef RN1
#undef DG1
#undef DN1
#undef DT1
#undef DR1
#undef LI1
#undef LA1
/* PCBGRCS: State and action table for content model suffix.
            If suffix occurs, process it.  Otherwise, put character
            back for the next parse.
*/
/* Symbols for state names (end with a number). */
#define SP4     0   /* Suffix expected. */

static UNCH
/*       bit  nmc  nms  re   spc  non  ee   eob  rs   and  grpc grpo lit  lita
         dtgc dtgo opt  or   pero plus rep  rni  seq  refc */
sp04 []={SP4 ,SP4 ,SP4 ,SP4 ,SP4 ,SP4 ,SP4 ,SP4 ,SP4 ,SP4 ,SP4 ,SP4 ,SP4 ,SP4 ,
         SP4 ,SP4 ,SP4 ,SP4 ,SP4 ,SP4 ,SP4 ,SP4 ,SP4 ,SP4 },/*sp4*/
sp04a[]={RCR_,RCR_,RCR_,RCR_,RCR_,SYS_,EE_ ,GET_,RCR_,RCR_,RCR_,RCR_,RCR_,RCR_,
         RCR_,RCR_,OPT ,RCR_,RCR_,REP ,OREP,RCR_,RCR_,RCR_},

*grcstab[] = {sp04, sp04a};
struct parse pcbgrcs = {"GRCS", lexgrp, grcstab, 0, 0, 0, 0};
#undef SP4
/* PCBGRNT: State and action table for name token group parse.
            Groups cannot nest.  Reserved names are not allowed.
            No suffixes or data tag pattern groups.
*/
/* Symbols for state names (end with a number). */
#define TK1     0   /* Token expected: name, #CHARS, data tag grp, model. */
#define CO1     2   /* Connector between tokens expected. */
#define ER1     4   /* PERO found when token was expected. */

static UNCH
/*       bit  nmc  nms  re   spc  non  ee   eob  rs   and  grpc grpo lit  lita
         dtgc dtgo opt  or   pero plus rep  rni  seq  refc  */
tk02 []={TK1 ,CO1 ,CO1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,
         TK1 ,TK1 ,TK1 ,TK1 ,ER1 ,TK1 ,TK1 ,TK1 ,TK1 ,CO1 },/*tk1*/
tk02a[]={INV_,NMT_,NMT_,NOP_,NOP_,SYS_,EOF_,GET_,RS_ ,INV_,INV_,INV_,INV_,INV_,
         INV_,INV_,INV_,INV_,NOP_,INV_,INV_,INV_,INV_,NMT_},

co02 []={TK1 ,TK1 ,TK1 ,CO1 ,CO1 ,CO1 ,CO1 ,CO1 ,CO1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,
         TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 },/*co1*/
co02a[]={INV_,INV_,INV_,NOP_,NOP_,SYS_,EE_ ,GET_,RS_ ,NOP_,GRPE,INV_,INV_,INV_,
         INV_,INV_,INV_,NOP_,INV_,INV_,INV_,INV_,NOP_,INV_},

er02 []={TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,ER1 ,TK1 ,ER1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,
         TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 },/*er1*/
er02a[]={PCI_,PCI_,PER_,PCI_,PCI_,SYS_,PCI_,GET_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,
         PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_},

*grnttab[] = {tk02, tk02a, co02, co02a, er02, er02a};
struct parse pcbgrnt = {"GRNT", lexgrp, grnttab, 0, 0, 0, 0};
#undef TK1
#undef CO1
#undef ER1
/* PCBGRNM: State and action table for name group parse.
            Groups cannot nest.  Reserved names are not allowed.
            No suffixes or data tag pattern groups.
*/
/* Symbols for state names (end with a number). */
#define TK1     0   /* Token expected: name, #CHARS, data tag grp, model. */
#define CO1     2   /* Connector between tokens expected. */
#define ER1     4   /* PERO found when token was expected. */

static UNCH
/*       bit  nmc  nms  re   spc  non  ee   eob  rs   and  grpc grpo lit  lita
         dtgc dtgo opt  or   pero plus rep  rni  seq  refc  */
tk03 []={TK1 ,TK1 ,CO1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,
         TK1 ,TK1 ,TK1 ,TK1 ,ER1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 },/*tk1*/
tk03a[]={INV_,INV_,NAS_,NOP_,NOP_,SYS_,EOF_,GET_,RS_ ,INV_,INV_,INV_,INV_,INV_,
         INV_,INV_,INV_,INV_,NOP_,INV_,INV_,INV_,INV_,INV_},

co03 []={TK1 ,TK1 ,TK1 ,CO1 ,CO1 ,CO1 ,CO1 ,CO1 ,CO1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,
         TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 },/*co1*/
co03a[]={INV_,INV_,INV_,NOP_,NOP_,SYS_,EE_ ,GET_,RS_ ,NOP_,GRPE,INV_,INV_,INV_,
         INV_,INV_,INV_,NOP_,INV_,INV_,INV_,INV_,NOP_,INV_},

er03 []={TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,ER1 ,TK1 ,ER1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,
         TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 },/*er1*/
er03a[]={PCI_,PCI_,PER_,PCI_,PCI_,SYS_,PCI_,GET_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,
         PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_},

*grnmtab[] = {tk03, tk03a, co03, co03a, er03, er03a};
struct parse pcbgrnm = {"GRNM", lexgrp, grnmtab, 0, 0, 0, 0};
#undef TK1
#undef CO1
#undef ER1
/* PCBREF: State and action table to find the end of entity, parameter entity,
           and character references.  The opening delimiter and name
           have already been found; the parse determines whether the
           tokenization of the name ended normally and processes the REFC.
*/
/* Symbols for state names (end with a number). */
#define ER5     0   /* Handle REFC or other entity reference termination. */
#define ER6     2   /* Return to caller and reset state for next call. */

static UNCH
/*       bit  nmc  nms  re   spc  non  ee   eob  rs   and  grpc grpo lit  lita
         dtgc dtgo opt  or   pero plus rep  rni  seq  refc */
er05 []={ER5 ,ER6 ,ER6 ,ER6 ,ER5 ,ER5 ,ER5 ,ER5 ,ER5 ,ER5 ,ER5 ,ER5 ,ER5 ,ER5 ,
         ER5 ,ER5 ,ER5 ,ER5 ,ER5 ,ER5 ,ER5 ,ER5 ,ER5 ,ER6 },/*er5*/
er05a[]={RCR_,LEN_,LEN_,NOP_,RCR_,SYS_,RCR_,GET_,RCR_,RCR_,RCR_,RCR_,RCR_,RCR_,
         RCR_,RCR_,RCR_,RCR_,RCR_,RCR_,RCR_,RCR_,RCR_,NOP_},

er06 []={ER5 ,ER5 ,ER5 ,ER5 ,ER5 ,ER5 ,ER5 ,ER5 ,ER5 ,ER5 ,ER5 ,ER5 ,ER5 ,ER5 ,
         ER5 ,ER5 ,ER5 ,ER5 ,ER5 ,ER5 ,ER5 ,ER5 ,ER5 ,ER5 },/*er6*/
er06a[]={RCR_,RCR_,RCR_,RCR_,RCR_,RCR_,RCR_,RCR_,RCR_,RCR_,RCR_,RCR_,RCR_,RCR_,
         RCR_,RCR_,RCR_,RCR_,RCR_,RCR_,RCR_,RCR_,RCR_,RCR_},

*reftab[]={er05, er05a, er06, er06a};
struct parse pcbref = {"ENTREF", lexgrp, reftab, 0, 0, 0, 0};
#undef ER5
#undef ER6
/*
Use (typical)           Name   Ending  Chsw References RS   RE   SEP
Parameter literal       LITPC  LIT/A   OK   Parm,Char  RSM_ LAM_ LAM_
     Data tag template                 NO
System ID               LITC   LIT/A   n/a  none       RSM_ LAM_ LAM_
     Processing instruction    PIC
Attribute value         LITRV  LIT/A   NO   Gen,Char   RS_  FUN_ FUN_
Minimum literal         LITV   LIT/A   n/a  none       RS_  FUN_ MLE_
*/
/* PCBLITP: Literal parse with parameter and character references;
            no function character translation.
*/
/* Symbols for state names (end with a number). */
#define DA0     0   /* Data in buffer. */
#define ER0     2   /* ERO found. */
#define CR0     4   /* CRO found (ER0, RNI). */
#define PR0     6   /* PRO found (for PCBLITP). */

static UNCH
/*       free num  min  nms  spc  non  ee   eob  rs   re   sep  cde  nsc  ero
         mdo  msc  mso  pero rni  tagc tago litc */
da13 []={DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,ER0 ,
         DA0 ,DA0 ,DA0 ,PR0 ,DA0 ,DA0 ,DA0 ,DA0 },/*da3*/
da13a[]={MLA_,MLA_,MLA_,MLA_,MLA_,NON_,EE_ ,GET_,RSM_,MLA_,MLA_,MLA_,NSC_,NOP_,
         MLA_,MLA_,MLA_,NOP_,MLA_,MLA_,MLA_,TER_},

er13 []={DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,ER0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,
         DA0 ,DA0 ,DA0 ,DA0 ,CR0 ,DA0 ,DA0 ,DA0 },/*er3*/
er13a[]={LPR_,LPR_,LPR_,LPR_,LPR_,LPR_,LPR_,GET_,LPR_,LPR_,LPR_,LPR_,LPR_,LPR_,
         LPR_,LPR_,LPR_,LPR_,NOP_,LPR_,LPR_,LPR_},

cr13 []={DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,CR0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,
         DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 },/*cr3*/
cr13a[]={LP2_,CRN_,LP2_,CRA_,LP2_,LP2_,LP2_,GET_,LP2_,LP2_,LP2_,LP2_,LP2_,LP2_,
         LP2_,LP2_,LP2_,LP2_,LP2_,LP2_,LP2_,LP2_},

pr13 []={DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,PR0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,
         DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 },/*pr3*/
pr13a[]={LPR_,LPR_,LPR_,PEX_,LPR_,LPR_,LPR_,GET_,LPR_,LPR_,LPR_,LPR_,LPR_,LPR_,
         LPR_,LPR_,LPR_,LPR_,LPR_,LPR_,LPR_,LPR_},

*litptab[] = {da13, da13a, er13, er13a, cr13, cr13a, pr13, pr13a};
struct parse pcblitp = {"LITP", lexlms, litptab, 0, 0, 0, 0};
#undef DA0
#undef ER0
#undef CR0
#undef PR0
/* PCBLITC: Literal parse; no references; no function char translation.
            Used for character data (system data).
*/
/* Symbols for state names (end with a number). */
#define DA0     0   /* Data in buffer. */

static UNCH
/*      free num  min  nms  spc  non  ee   eob  rs   re   sep  cde  nsc  ero
        mdo  msc  mso  pero rni  tagc tago litc */
da2 []={DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,
        DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 },/*da2*/
da2a[]={MLA_,MLA_,MLA_,MLA_,MLA_,SYS_,EOF_,GET_,RSM_,MLA_,MLA_,MLA_,SYS_,MLA_,
        MLA_,MLA_,MLA_,MLA_,MLA_,MLA_,MLA_,TER_},

*litctab[] = {da2, da2a};
struct parse pcblitc = {"LITC", lexlms, litctab, 0, 0, 0, 0};
#undef DA0
/* PCBLITR: Attribute value parse; general and character references;
            function chars are translated.
*/
/* Symbols for state names (end with a number). */
#define DA0     0   /* Data in buffer. */
#define ER0     2   /* ERO found. */
#define CR0     4   /* CRO found (ER0, RNI). */

static UNCH
/*       free num  min  nms  spc  non  ee   eob  rs   re   sep  cde  nsc  ero
         mdo  msc  mso  pero rni  tagc tago litc */
da11 []={DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,ER0 ,
         DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 },/*da1*/
da11a[]={MLA_,MLA_,MLA_,MLA_,MLA_,NON_,EE_ ,GET_,RS_ ,FUN_,FUN_,MLA_,NSC_,NOP_,
         MLA_,MLA_,MLA_,MLA_,MLA_,MLA_,MLA_,TER_},

er11 []={DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,ER0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,
         DA0 ,DA0 ,DA0 ,DA0 ,CR0 ,DA0 ,DA0 ,DA0 },/*er1*/
er11a[]={LPR_,LPR_,LPR_,ERX_,LPR_,LPR_,LPR_,GET_,LPR_,LPR_,LPR_,LPR_,LPR_,LPR_,
         LPR_,LPR_,LPR_,LPR_,NOP_,LPR_,LPR_,LPR_},

cr11 []={DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,CR0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,
         DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 },/*cr1*/
cr11a[]={LP2_,CRN_,LP2_,CRA_,LP2_,LP2_,LP2_,GET_,LP2_,LP2_,LP2_,LP2_,LP2_,LP2_,
         LP2_,LP2_,LP2_,LP2_,LP2_,LP2_,LP2_,LP2_},

*litrtab[] = {da11, da11a, er11, er11a, cr11, cr11a};
struct parse pcblitr = {"LITR", lexlms, litrtab, 0, 0, 0, 0};
#undef DA0
#undef ER0
#undef CR0
/* PCBLITV: Literal parse; no references; RS ignored; RE/SPACE sequences
            become single SPACE.  Only minimum data characters allowed.
*/
/* Symbols for state names (end with a number). */
#define LS0     0   /* Leading SPACE or RE found. */
#define VA0     2   /* Valid character found. */
#define SP0     4   /* SPACE/RE sequence begun. */

static UNCH
/*       free num  min  nms  spc  non  ee   eob  rs   re   sep  cde  nsc  ero
         mdo  msc  mso  pero rni  tagc tago litc */
ls10 []={VA0 ,VA0 ,VA0 ,VA0 ,LS0 ,VA0 ,LS0 ,LS0 ,LS0 ,LS0 ,LS0 ,VA0 ,VA0 ,VA0 ,
         VA0 ,VA0 ,VA0 ,VA0 ,VA0 ,VA0 ,VA0 ,LS0 },/*ls0*/
ls10a[]={MLE_,MLA_,MLA_,MLA_,NOP_,SYS_,EOF_,GET_,RS_ ,NOP_,MLE_,SYS_,SYS_,MLE_,
         MLE_,MLE_,MLE_,MLE_,MLE_,MLE_,MLE_,TER_},
va10 []={VA0 ,VA0 ,VA0 ,VA0 ,SP0 ,VA0 ,VA0 ,VA0 ,VA0 ,SP0 ,SP0 ,VA0 ,VA0 ,VA0 ,
         VA0 ,VA0 ,VA0 ,VA0 ,VA0 ,VA0 ,VA0 ,LS0 },/*va0*/
da10a[]={MLE_,MLA_,MLA_,MLA_,MLA_,SYS_,EOF_,GET_,RS_ ,FUN_,MLE_,SYS_,SYS_,MLE_,
         MLE_,MLE_,MLE_,MLE_,MLE_,MLE_,MLE_,TER_},
sp10 []={VA0 ,VA0 ,VA0 ,VA0 ,SP0 ,VA0 ,VA0 ,SP0 ,SP0 ,SP0 ,SP0 ,VA0 ,VA0 ,VA0 ,
         VA0 ,VA0 ,VA0 ,VA0 ,VA0 ,VA0 ,VA0 ,LS0 },/*sp0*/
sp10a[]={MLE_,MLA_,MLA_,MLA_,NOP_,SYS_,EOF_,GET_,RS_ ,NOP_,MLE_,SYS_,SYS_,MLE_,
         MLE_,MLE_,MLE_,MLE_,MLE_,MLE_,MLE_,RPR_},

*litvtab[] = {ls10, ls10a, va10, da10a, sp10, sp10a};
struct parse pcblitv = {"LITV", lexlms, litvtab, 0, 0, 0, 0};
#undef LS0
#undef VA0
#undef SP0
/* PCBLITT: Tokenized attribute value parse.
*/

/* PCBLITT: Attribute value parse; general and character references;
            function chars are translated.
*/
/* Symbols for state names (end with a number). */
#define SP0	0   /* Ignore spaces */
#define DA0     2   /* Data character */
#define ER0     4   /* ERO found; ignore space */
#define ER1     6   /* ERO found; don't ignore space */
#define CR0     8   /* CRO found (ER0, RNI); ignore space */
#define CR1     10  /* CR0 found; don't ignore space */

int pcblittda = DA0;

static UNCH
/*       free num  min  nms  spc  non  ee   eob  rs   re   sep  cde  nsc  ero
         mdo  msc  mso  pero rni  tagc tago litc */

sp14 []={DA0 ,DA0 ,DA0 ,DA0 ,SP0 ,DA0 ,DA0 ,SP0 ,SP0 ,SP0 ,SP0 ,DA0 ,DA0 ,ER0 ,
         DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 },/*sp0*/
sp14a[]={MLA_,MLA_,MLA_,MLA_,NOP_,NON_,EE_ ,GET_,RS_ ,NOP_,NOP_,MLA_,NSC_,NOP_,
         MLA_,MLA_,MLA_,MLA_,MLA_,MLA_,MLA_,TER_},

da14 []={DA0 ,DA0 ,DA0 ,DA0 ,SP0 ,DA0 ,DA0 ,DA0 ,DA0 ,SP0 ,SP0 ,DA0 ,DA0 ,ER1 ,
         DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,SP0 },/*da0*/
da14a[]={MLA_,MLA_,MLA_,MLA_,MLA_,NON_,EE_ ,GET_,RS_ ,FUN_,FUN_,MLA_,NSC_,NOP_,
         MLA_,MLA_,MLA_,MLA_,MLA_,MLA_,MLA_,TER_},

er14 []={DA0 ,DA0 ,DA0 ,SP0 ,DA0 ,DA0 ,DA0 ,ER0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,
         DA0 ,DA0 ,DA0 ,DA0 ,CR0 ,DA0 ,DA0 ,DA0 },/*er0*/
er14a[]={LPR_,LPR_,LPR_,ERX_,LPR_,LPR_,LPR_,GET_,LPR_,LPR_,LPR_,LPR_,LPR_,LPR_,
         LPR_,LPR_,LPR_,LPR_,NOP_,LPR_,LPR_,LPR_},

er15 []={DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,ER1 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,
         DA0 ,DA0 ,DA0 ,DA0 ,CR1 ,DA0 ,DA0 ,DA0 },/*er1*/
er15a[]={LPR_,LPR_,LPR_,ERX_,LPR_,LPR_,LPR_,GET_,LPR_,LPR_,LPR_,LPR_,LPR_,LPR_,
         LPR_,LPR_,LPR_,LPR_,NOP_,LPR_,LPR_,LPR_},

cr14 []={DA0 ,DA0 ,DA0 ,SP0 ,DA0 ,DA0 ,DA0 ,CR0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,
         DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 },/*cr0*/
cr14a[]={LP2_,CRN_,LP2_,CRA_,LP2_,LP2_,LP2_,GET_,LP2_,LP2_,LP2_,LP2_,LP2_,LP2_,
         LP2_,LP2_,LP2_,LP2_,LP2_,LP2_,LP2_,LP2_},

cr15 []={DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,CR1 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,
         DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 },/*cr1*/
cr15a[]={LP2_,CRN_,LP2_,CRA_,LP2_,LP2_,LP2_,GET_,LP2_,LP2_,LP2_,LP2_,LP2_,LP2_,
         LP2_,LP2_,LP2_,LP2_,LP2_,LP2_,LP2_,LP2_},

*litttab[] = {sp14, sp14a, da14, da14a, er14, er14a, er15, er15a, cr14, cr14a,
	      cr15, cr15a};
struct parse pcblitt = {"LITT", lexlms, litttab, 0, 0, 0, 0};
#undef SP0
#undef DA0
#undef ER0
#undef ER1
#undef CR0
#undef CR1
/* PCBMD: State and action table for markup declaration tokenization.
          Columns are based on LEXMARK.C.
*/
/* Symbols for state names (end with a number). */
#define SP1     0   /* Separator before token expected. */
#define TK1     2   /* Token expected. */
#define CM0     4   /* COM[1] found when sep expected: possible comment, MGRP.*/
#define CM1     6   /* COM[1] found: possible comment, MGRP, or minus.*/
#define CM2     8   /* COM[2] found; in comment. */
#define CM3    10   /* Ending COM[1] found; end comment or continue it. */
#define PR1    12   /* PERO found when token was expected. */
#define PX1    14   /* PLUS found: PGRP or error. */
#define RN1    16   /* RNI found; possible reserved name start. */

int pcbmdtk = TK1;            /* PCBMD: token expected. */

static UNCH
/*       bit  nmc  num  nms  spc  non  ee   eob  rs   com  eti  grpo lit  lita
         dso  dsc  pero plus refc rni  tagc tago vi    */
sp21 []={SP1 ,SP1 ,SP1 ,SP1 ,TK1 ,SP1 ,TK1 ,SP1 ,TK1 ,CM0 ,SP1 ,TK1 ,TK1 ,TK1 ,
         TK1 ,SP1 ,PR1 ,PX1 ,SP1 ,RN1 ,SP1 ,SP1 ,SP1 },
sp21a[]={INV_,LEN_,LEN_,LEN_,NOP_,SYS_,EE_ ,GET_,RS_ ,NOP_,INV_,GRPS,LIT ,LITE,
         MDS ,INV_,NOP_,NOP_,INV_,NOP_,EMD ,INV_,INV_},

tk21 []={SP1 ,SP1 ,SP1 ,SP1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,CM1 ,SP1 ,TK1 ,TK1 ,TK1 ,
         TK1 ,SP1 ,PR1 ,PX1 ,SP1 ,RN1 ,SP1 ,SP1 ,SP1 },
tk21a[]={INV_,NMT ,NUM ,NAS ,NOP_,SYS_,EE_ ,GET_,RS_ ,NOP_,INV_,GRPS,LIT ,LITE,
         MDS ,INV_,NOP_,NOP_,INV_,NOP_,EMD ,INV_,INV_},

/*       bit  nmc  num  nms  spc  non  ee   eob  rs   com  eti  grpo lit  lita
         dso  dsc  pero plus refc rni  tagc tago vi    */
cm20 []={SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,CM0 ,SP1 ,CM0 ,SP1 ,CM2 ,SP1 ,SP1 ,SP1 ,SP1 ,
         SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,SP1 },
cm20a[]={LNR_,LNR_,LNR_,LNR_,LNR_,SYS_,LNR_,GET_,LNR_,NOP_,LNR_,LNR_,LNR_,LNR_,
         LNR_,LNR_,LNR_,LNR_,LNR_,LNR_,LNR_,LNR_,LNR_},

cm21 []={TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,CM1 ,TK1 ,CM1 ,TK1 ,CM2 ,TK1 ,TK1 ,TK1 ,TK1 ,
         TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 },
cm21a[]={CDR ,CDR ,CDR ,CDR ,CDR ,SYS_,CDR ,GET_,CDR ,NOP_,CDR ,MGRP,CDR ,CDR ,
         CDR ,CDR ,CDR ,CDR ,CDR ,CDR ,CDR ,CDR ,CDR },

cm22 []={CM2 ,CM2 ,CM2 ,CM2 ,CM2 ,CM2 ,TK1 ,CM2 ,CM2 ,CM3 ,CM2 ,CM2 ,CM2 ,CM2 ,
         CM2 ,CM2 ,CM2 ,CM2 ,CM2 ,CM2 ,CM2 ,CM2 ,CM2 },
cm22a[]={NOP_,NOP_,NOP_,NOP_,NOP_,SYS_,EOF_,GET_,RS_ ,NOP_,NOP_,NOP_,NOP_,NOP_,
         NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_},

/*       bit  nmc  num  nms  spc  non  ee   eob  rs   com  eti  grpo lit  lita
         dso  dsc  pero plus refc rni  tagc tago vi    */
cm23 []={CM2 ,CM2 ,CM2 ,CM2 ,CM2 ,CM3 ,TK1 ,CM3 ,CM2 ,TK1 ,CM2 ,CM2 ,CM2 ,CM2 ,
         CM2 ,CM2 ,CM2 ,CM2 ,CM2 ,CM2 ,CM2 ,CM2 ,CM2 },
cm23a[]={NOP_,NOP_,NOP_,NOP_,NOP_,SYS_,EOF_,GET_,RS_ ,NOP_,NOP_,NOP_,NOP_,NOP_,
         NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_},

pr21 []={SP1 ,SP1 ,SP1 ,TK1 ,TK1 ,PR1 ,SP1 ,PR1 ,TK1 ,SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,
         SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,TK1 ,SP1 ,SP1 ,SP1 },
pr21a[]={PCI_,PCI_,PCI_,PER_,PEN ,SYS_,PENR,GET_,PEN ,PENR,PCI_,PCI_,PCI_,PCI_,
         PCI_,PCI_,PENR,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_},

px21 []={SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,PX1 ,SP1 ,PX1 ,SP1 ,SP1 ,SP1 ,TK1 ,SP1 ,SP1 ,
         SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,SP1 },
px21a[]={PCI_,PCI_,PCI_,PCI_,PCI_,SYS_,PCI_,GET_,PCI_,PCI_,PCI_,PGRP,PCI_,PCI_,
         PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_},

rn21 []={TK1 ,TK1 ,TK1 ,SP1 ,TK1 ,RN1 ,TK1 ,RN1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,
         TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 },
rn21a[]={PCI_,PCI_,PCI_,RNS ,PCI_,SYS_,PCI_,GET_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,
         PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_},

*mdtab[] = {sp21, sp21a, tk21, tk21a, cm20, cm20a, cm21, cm21a, cm22, cm22a,
            cm23, cm23a, pr21, pr21a, px21, px21a, rn21, rn21a};
struct parse pcbmd = {"MD", lexmark, mdtab, 0, 0, 0, 0};
#undef SP1
#undef TK1
#undef CM0
#undef CM1
#undef CM2
#undef CM3
#undef PR1
#undef PX1
#undef RN1
/* PCBMDC: State and action table for comment declaration.
*/
/* Symbols for state names (end with a number). */
#define CD2     0   /* COM[2] found; in comment. */
#define CD3     2   /* Ending COM[1] found; end comment or continue it. */
#define EM1     4   /* Ending COM[2] found; start new comment or end. */
#define CD1     6   /* COM[1] found; new comment or error. */

static UNCH
/*      bit  nmc  num  nms  spc  non  ee   eob  rs   com  eti  grpo lit  lita
        dso  dsc  pero plus refc rni  tagc tago vi    */
cd22 []={CD2 ,CD2 ,CD2 ,CD2 ,CD2 ,CD2 ,CD2 ,CD2 ,CD2 ,CD3 ,CD2 ,CD2 ,CD2 ,CD2 ,
         CD2 ,CD2 ,CD2 ,CD2 ,CD2 ,CD2 ,CD2 ,CD2 ,CD2 },
cd22a[]={NOP_,NOP_,NOP_,NOP_,NOP_,SYS_,EOF_,GET_,RS_ ,NOP_,NOP_,NOP_,NOP_,NOP_,
         NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_},

cd23 []={CD2 ,CD2 ,CD2 ,CD2 ,CD2 ,CD3 ,CD2 ,CD3 ,CD2 ,EM1 ,CD2 ,CD2 ,CD2 ,CD2 ,
         CD2 ,CD2 ,CD2 ,CD2 ,CD2 ,CD2 ,CD2 ,CD2 ,CD2 },
cd23a[]={NOP_,NOP_,NOP_,NOP_,NOP_,SYS_,EOF_,GET_,RS_ ,NOP_,NOP_,NOP_,NOP_,NOP_,
         NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_},

em21 []={CD2 ,CD2 ,CD2 ,CD2 ,EM1 ,EM1 ,CD2 ,EM1 ,EM1 ,CD1 ,CD2 ,CD2 ,CD2 ,CD2 ,
         CD2 ,CD2 ,CD2 ,CD2 ,CD2 ,CD2 ,CD2 ,CD2 ,CD2 },
em21a[]={INV_,INV_,INV_,INV_,NOP_,SYS_,EOF_,GET_,RS_ ,NOP_,INV_,INV_,INV_,INV_,
         INV_,INV_,INV_,INV_,INV_,INV_,EMD ,INV_,INV_},

cd21 []={CD2 ,CD2 ,CD2 ,CD2 ,CD2 ,CD1 ,CD2 ,CD1 ,CD2 ,CD2 ,CD2 ,CD2 ,CD2 ,CD2 ,
         CD2 ,CD2 ,CD2 ,CD2 ,CD2 ,CD2 ,CD2 ,CD2 ,CD2 },
cd21a[]={PCI_,PCI_,PCI_,PCI_,PCI_,SYS_,EOF_,GET_,PCI_,NOP_,PCI_,PCI_,PCI_,PCI_,
         PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_,PCI_},

*mdctab[] = {cd22, cd22a, cd23, cd23a, em21, em21a, cd21, cd21a};
struct parse pcbmdc = {"MDC", lexmark, mdctab, 0, 0, 0, 0};
#undef CD2
#undef CD3
#undef EM1
#undef CD1
/* PCBMDI: State and action table for ignoring markup declarations.
           Literals are handled properly so a TAGC won't end the declaration.
           An error is noted if the entity ends within a declaration that
           is being ignored.
           TO DO: Handle nested declaration sets.
*/
/* Symbols for state names (end with a number). */
#define NC1     0   /* Not in a comment; TAGC ends declaration. */
#define IC1     2   /* COM[1] found; possible comment. */
#define IC2     4   /* COM[2] found; in comment. */
#define IC3     6   /* Ending COM[1] found; end comment or continue it. */
#define LI1     8   /* Literal parameter; search for LIT. */
#define LA1    10   /* Literal parameter; search for LITA. */

static UNCH
/*       bit  nmc  num  nms  spc  non  ee   eob  rs   com  eti  grpo lit  lita
         dso  dsc  pero plus refc rni  tagc tago vi    */
nc21 []={NC1 ,NC1 ,NC1 ,NC1 ,NC1 ,NC1 ,NC1 ,NC1 ,NC1 ,IC1 ,NC1 ,NC1 ,LI1 ,LA1 ,
         NC1 ,NC1 ,NC1 ,NC1 ,NC1 ,NC1 ,NC1 ,NC1 ,NC1 },
nc21a[]={NOP_,NOP_,NOP_,NOP_,NOP_,SYS_,EOF_,GET_,RS_ ,NOP_,NOP_,NOP_,NOP_,NOP_,
         NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,EMD ,NOP_,NOP_},

ic21 []={NC1 ,NC1 ,NC1 ,NC1 ,NC1 ,IC1 ,NC1 ,IC1 ,NC1 ,IC2 ,NC1 ,NC1 ,LI1 ,LA1 ,
         NC1 ,NC1 ,NC1 ,NC1 ,NC1 ,NC1 ,NC1 ,NC1 ,NC1 },
ic21a[]={NOP_,NOP_,NOP_,NOP_,NOP_,SYS_,EOF_,GET_,RS_ ,NOP_,NOP_,NOP_,NOP_,NOP_,
         NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,EMD ,NOP_,NOP_},

ic22 []={IC2 ,IC2 ,IC2 ,IC2 ,IC2 ,IC2 ,NC1 ,IC2 ,IC2 ,IC3 ,IC2 ,IC2 ,IC2 ,IC2 ,
         IC2 ,IC2 ,IC2 ,IC2 ,IC2 ,IC2 ,IC2 ,IC2 ,IC2 },
ic22a[]={NOP_,NOP_,NOP_,NOP_,NOP_,SYS_,EOF_,GET_,RS_ ,NOP_,NOP_,NOP_,NOP_,NOP_,
         NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_},

ic23 []={IC2 ,IC2 ,IC2 ,IC2 ,IC2 ,IC3 ,NC1 ,IC3 ,IC2 ,NC1 ,IC2 ,IC2 ,IC2 ,IC2 ,
         IC2 ,IC2 ,IC2 ,IC2 ,IC2 ,IC2 ,IC2 ,IC2 ,IC2 },/*ic3*/
ic23a[]={NOP_,NOP_,NOP_,NOP_,NOP_,SYS_,EOF_,GET_,RS_ ,NOP_,NOP_,NOP_,NOP_,NOP_,
         NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_},

/*       bit  nmc  num  nms  spc  non  ee   eob  rs   com  eti  grpo lit  lita
         dso  dsc  pero plus refc rni  tagc tago vi    */
li21 []={LI1 ,LI1 ,LI1 ,LI1 ,LI1 ,LI1 ,LI1 ,LI1 ,LI1 ,LI1 ,LI1 ,LI1 ,NC1 ,LI1 ,
         LI1 ,LI1 ,LI1 ,LI1 ,LI1 ,LI1 ,LI1 ,LI1 ,LI1 },/*li1*/
li21a[]={NOP_,NOP_,NOP_,NOP_,NOP_,SYS_,EOF_,GET_,RS_ ,NOP_,NOP_,NOP_,NOP_,NOP_,
         NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_},

la21 []={LA1 ,LA1 ,LA1 ,LA1 ,LA1 ,LA1 ,LA1 ,LA1 ,LA1 ,LA1 ,LA1 ,LA1 ,LA1 ,NC1 ,
         LA1 ,LA1 ,LA1 ,LA1 ,LA1 ,LA1 ,LA1 ,LA1 ,LA1 },/*la1*/
la21a[]={NOP_,NOP_,NOP_,NOP_,NOP_,SYS_,EOF_,GET_,RS_ ,NOP_,NOP_,NOP_,NOP_,NOP_,
         NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_},

*mditab[] = {nc21, nc21a, ic21, ic21a, ic22, ic22a,
             ic23, ic23a, li21, li21a, la21, la21a};
struct parse pcbmdi = {"MDI", lexmark, mditab, 0, 0, 0, 0};
#undef NC1
#undef IC1
#undef IC2
#undef IC3
#undef LI1
#undef LA1
/* PCBMSRC: State and action table for marked section in RCDATA mode.
            Nested marked sections are not recognized; the first MSE ends it.
            Initial state assumes an MS declaration was processed.
            Columns are based on LEXLMS.C but LITC column needn't exist.
*/
/* Symbols for state names (end with a number). */
#define ET0     0   /* MSS processed or buffer flushed; no data. */
#define DA0     2   /* Data in buffer. */
#define ER0     4   /* ERO found; start lookahead buffer. */
#define CR0     6   /* CRO found (ER0, RNI). */
#define ME0     8   /* MSC found. */
#define ME1    10   /* MSC, MSC found. */

static UNCH
/*       free nu   min  nms  spc  non  ee   eob  rs   re   sep  cde  nsc  ero
         mdo  msc  mso  pero rni  tagc tago litc */
et30 []={DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,DA0 ,DA0 ,ET0 ,ER0 ,
         DA0 ,ME0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 },/*et0*/
et30a[]={DAS_,DAS_,DAS_,DAS_,DAS_,NON_,EE_ ,GET_,RS_ ,REF_,DAS_,DAS_,NSC_,LAS_,
         DAS_,LAS_,DAS_,DAS_,DAS_,DAS_,DAS_,DAS_},

da30 []={DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,DA0 ,DA0 ,ET0 ,ET0 ,
         DA0 ,ET0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 ,DA0 },/*da0*/
da30a[]={NOP_,NOP_,NOP_,NOP_,NOP_,DAF_,DAF_,DAF_,DAF_,DAF_,NOP_,NOP_,DAF_,DAF_,
         NOP_,DAF_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_},

er30 []={ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ER0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,
         ET0 ,ET0 ,ET0 ,ET0 ,CR0 ,ET0 ,ET0 ,ET0 },/*er0*/
er30a[]={LAF_,LAF_,LAF_,ERX_,LAF_,LAF_,LAF_,GET_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,
         LAF_,LAF_,LAF_,LAF_,LAM_,LAF_,LAF_,LAF_},

/*       free nu   min  nms  spc  non  ee   eob  rs   re   sep  cde  nsc  ero
         mdo  msc  mso  pero rni  tagc tago litc */
cr30 []={ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,CR0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,
         ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 ,ET0 },/*cr0*/
cr30a[]={LAF_,CRN_,LAF_,CRA_,LAF_,LAF_,LAF_,GET_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,
         LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_},

me30 []={ET0, ET0, ET0, ET0, ET0 ,ET0, ET0, ME0, ET0 ,ET0 ,ET0 ,ET0, ET0 ,ET0 ,
         ET0, ME1, ET0 ,ET0, ET0 ,ET0, ET0 ,ET0 },/*me0*/
me30a[]={LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,GET_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,
         LAF_,LAM_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_},

me31 []={ET0, ET0, ET0, ET0, ET0 ,ET0, ET0, ME1, ET0 ,ET0 ,ET0 ,ET0, ET0 ,ET0 ,
         ET0, ET0, ET0 ,ET0, ET0 ,ET0, ET0 ,ET0,},/*me1*/
me31a[]={LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,GET_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,
         LAF_,LAF_,LAF_,LAF_,LAF_,MSE_,LAF_,LAF_},

*msrctab[]={et30, et30a, da30, da30a, er30, er30a, cr30, cr30a,
	    me30, me30a, me31, me31a};
struct parse pcbmsrc = {"MSRCDATA", lexlms, msrctab, 0, 0, 0, 0};
#undef ET0
#undef DA0
#undef ER0
#undef CR0
#undef ME0
#undef ME1
/* PCBMSC: State and action table for marked section in CDATA mode.
           Nested marked sections are not recognized; the first MSE ends it.
           Initial state assumes an MS declaration was processed.
*/
/* Symbols for state names (end with a number). */
#define ET2     0   /* MSS processed or buffer flushed; no data. */
#define DA2     2   /* Data in buffer. */
#define ME2     4   /* MSC found. */
#define ME3     6   /* MSC, MSC found. */

static UNCH
/*       free nu   min  nms  spc  non  ee   eob  rs   re   sep  cde  nsc  ero
         mdo  msc  mso  pero rni  tagc tago litc */
et32 []={DA2 ,DA2 ,DA2 ,DA2 ,DA2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,DA2 ,DA2 ,ET2 ,DA2 ,
         DA2 ,ME2 ,DA2 ,DA2 ,DA2 ,DA2 ,DA2 ,DA2 },/*et2*/
et32a[]={DAS_,DAS_,DAS_,DAS_,DAS_,NON_,EOF_,GET_,RS_ ,REF_,DAS_,DAS_,NSC_,DAS_,
         DAS_,LAS_,DAS_,DAS_,DAS_,DAS_,DAS_,DAS_},

da32 []={DA2 ,DA2 ,DA2 ,DA2 ,DA2 ,ET2 ,ET2 ,ET2 ,ET2 ,ET2 ,DA2 ,DA2 ,ET2 ,DA2 ,
         DA2 ,ET2 ,DA2 ,DA2 ,DA2 ,DA2 ,DA2 ,DA2 },/*da2*/
da32a[]={NOP_,NOP_,NOP_,NOP_,NOP_,DAF_,DAF_,DAF_,DAF_,DAF_,NOP_,NOP_,DAF_,NOP_,
         NOP_,DAF_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_},

me32 []={ET2, ET2, ET2, ET2, ET2 ,ET2, ET2, ME2, ET2 ,ET2 ,ET2 ,ET2, ET2 ,ET2 ,
         ET2, ME3, ET2 ,ET2, ET2 ,ET2, ET2, ET2,},/*me2*/
me32a[]={LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,GET_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,
         LAF_,LAM_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_},

me33 []={ET2, ET2, ET2, ET2, ET2 ,ET2, ET2, ME3, ET2 ,ET2 ,ET2 ,ET2, ET2 ,ET2 ,
         ET2, ET2, ET2 ,ET2, ET2 ,ET2, ET2, ET2,},/*me3*/
me33a[]={LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,GET_,LAF_,LAF_,LAF_,LAF_,LAF_,LAF_,
         LAF_,LAF_,LAF_,LAF_,LAF_,MSE_,LAF_,LAF_},

*msctab[]={et32, et32a, da32, da32a, me32, me32a, me33, me33a};
struct parse pcbmsc = {"MSCDATA", lexlms, msctab, 0, 0, 0, 0};
#undef ET2
#undef DA2
#undef ME2
#undef ME3
/* PCBMSI: State and action table for marked section in IGNORE mode.
           Nested marked sections are recognized; the matching MSE ends it.
           Initial state assumes an MS declaration, MSS, or MSE was processed.
*/
/* Symbols for state names (end with a number). */
#define ET4     0   /* Markup found or buffer flushed; no data. */
#define ME4     2   /* MSC found. */
#define ME5     4   /* MSC, MSC found. */
#define ES4     6   /* TAGO found. */
#define MD4     8   /* MDO found (TAGO, MDO[2]). */

static UNCH
/*       free nu   min  nms  spc  non  ee   eob  rs   re   sep  cde  nsc  ero
         mdo  msc  mso  pero rni  tagc tago litc refc */
et34 []={ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,
         ET4 ,ME4 ,ET4 ,ET4 ,ET4 ,ET4 ,ES4 ,ET4 ,ET4 },/*et4*/
et34a[]={NOP_,NOP_,NOP_,NOP_,NOP_,SYS_,EOF_,GET_,RS_ ,NOP_,NOP_,NOP_,SYS_,NOP_,
         NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_},

me34 []={ET4, ET4, ET4, ET4, ET4 ,ET4, ET4, ME4, ET4 ,ET4 ,ET4 ,ET4, ET4, ET4 ,
         ET4, ME5 ,ET4, ET4, ET4 ,ET4, ET4, ET4, ET4,},/*me4*/
me34a[]={NOP_,NOP_,NOP_,NOP_,NOP_,SYS_,EOF_,GET_,RS_ ,NOP_,NOP_,NOP_,SYS_,NOP_,
         NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_},

me35 []={ET4, ET4, ET4, ET4, ET4 ,ET4, ET4, ME5, ET4 ,ET4 ,ET4 ,ET4, ET4, ET4 ,
         ET4, ET4 ,ET4, ET4, ET4 ,ET4, ET4, ET4, ET4,},/*me5*/
me35a[]={NOP_,NOP_,NOP_,NOP_,NOP_,SYS_,EOF_,GET_,RS_ ,NOP_,NOP_,NOP_,SYS_,NOP_,
         NOP_,NOP_,NOP_,NOP_,NOP_,MSE_,NOP_,NOP_,NOP_},

/*       free nu   min  nms  spc  non  ee   eob  rs   re   sep  cde  nsc  ero
         mdo  msc  mso  pero rni  tagc tago litc */
es34 []={ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ES4 ,ET4 ,ES4 ,ET4 ,ET4 ,ET4 ,ET4 ,ES4 ,ET4 ,
         MD4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 ,ET4 },/*es4*/
es34a[]={NOP_,NOP_,NOP_,NOP_,NOP_,SYS_,EOF_,GET_,RS_ ,NOP_,NOP_,NOP_,SYS_,NOP_,
         NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_,NOP_},

md34 []={ET4, ET4, ET4, ET4, ET4 ,MD4, ET4, MD4, ET4 ,ET4 ,ET4 ,ET4, ET4, ET4 ,
         ET4, ET4 ,ET4, ET4, ET4 ,ET4, ET4, ET4,},/*md4*/
md34a[]={NOP_,NOP_,NOP_,NOP_,NOP_,SYS_,EOF_,GET_,RS_ ,NOP_,NOP_,NOP_,SYS_,NOP_,
         NOP_,NOP_,MSS_,NOP_,NOP_,NOP_,NOP_,NOP_},

*msitab[]={et34, et34a, me34, me34a, me35, me35a, es34, es34a, md34, md34a};
struct parse pcbmsi = {"MSIGNORE", lexlms, msitab, 0, 0, 0, 0};
#undef ET4
#undef ME4
#undef ME5
#undef ES4
#undef MD4
#undef NS4
/* PCBSTAG: State and action table for start-tag parse.
            Columns are based on LEXMARK.C.
*/
/* Symbols for state names (end with a number). */
#define SP1     0   /* Separator before name expected. */
#define AN1     2   /* Attribute name expected. */
#define SP2     4   /* Separator or value indicator expected. */
#define VI1     6   /* Value indicator expected. */
#define AV1     8   /* Attribute value expected. */

int pcbstan = AN1;            /* PCBSTAG: attribute name expected. */

static UNCH
/*       bit  nmc  num  nms  spc  non  ee   eob  rs   com  eti  grpo lit  lita
         dso  dsc  pero plus refc rni  tagc tago vi    */
sp41 []={SP1 ,SP1 ,SP1 ,SP1 ,AN1 ,SP1 ,SP1 ,SP1 ,AN1 ,SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,
         SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,SP1 },
sp41a[]={INV_,LEN_,LEN_,LEN_,NOP_,SYS_,EOF_,GET_,RS_ ,INV_,ETIC,INV_,INV_,INV_,
         INV_,DSC ,INV_,INV_,INV_,INV_,TAGC,TAGO,INV_},

an41 []={SP1 ,SP1 ,SP1 ,SP2 ,AN1 ,AN1 ,AN1 ,AN1 ,AN1 ,SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,
         SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,SP1 },
an41a[]={INV_,NTV ,NTV ,NVS ,NOP_,SYS_,EOF_,GET_,RS_ ,INV_,ETIC,INV_,INV_,INV_,
         INV_,DSC ,INV_,INV_,INV_,INV_,TAGC,TAGO,INV_},

sp42 []={SP1 ,SP1 ,SP1 ,SP1 ,VI1 ,SP2 ,SP2 ,SP2 ,VI1 ,SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,
         SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,AV1 },
sp42a[]={INV_,LEN_,LEN_,LEN_,NOP_,SYS_,EOF_,GET_,RS_ ,INV_,NASV,INV_,INV_,INV_,
         INV_,NASV,INV_,INV_,INV_,INV_,NASV,NASV,NOP_},

/*       bit  nmc  num  nms  spc  non  ee   eob  rs   com  eti  grpo lit  lita
         dso  dsc  pero plus refc rni  tagc tago vi    */
vi41 []={SP1 ,AN1 ,AN1 ,AN1 ,VI1 ,VI1 ,VI1 ,VI1 ,VI1 ,SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,
         SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,AV1 },
vi41a[]={INV_,NASV,NASV,NASV,NOP_,SYS_,EOF_,GET_,RS_ ,INV_,NASV,INV_,INV_,INV_,
         INV_,NASV,INV_,INV_,INV_,INV_,NASV,NASV,NOP_},

av41 []={SP1 ,SP1 ,SP1 ,SP1 ,AV1 ,AV1 ,AV1 ,AV1 ,AV1 ,SP1 ,SP1 ,SP1 ,AN1 ,AN1 ,
         SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,SP1 ,SP1 },
av41a[]={INV_,AVU ,AVU ,AVU ,NOP_,SYS_,EOF_,GET_,RS_ ,INV_,INV_,INV_,AVD ,AVDA,
         INV_,INV_,INV_,INV_,INV_,INV_,INV_,INV_,INV_},

*stagtab[] = {sp41, sp41a, an41, an41a, sp42, sp42a, vi41, vi41a, av41, av41a};
struct parse pcbstag = {"STAG", lexmark, stagtab, 0, 0, 0, 0};
#undef SP1
#undef AN1
#undef SP2
#undef VI1
#undef AV1
/* PCBETAG: State and action table for end-tag parse.
*/
#define TC1     0   /* Tag close expected (no attributes allowed). */

static UNCH
/*       bit  nmc  nu   nms  spc  non  ee   eob  rs   com  eti  grpo lit  lita
         dso  dsc  pero plus refc rni  tagc tago vi    */
tc41 []={TC1 ,TC1 ,TC1 ,TC1 ,TC1 ,TC1 ,TC1 ,TC1 ,TC1 ,TC1 ,TC1 ,TC1 ,TC1 ,TC1 ,
         TC1 ,TC1 ,TC1 ,TC1 ,TC1 ,TC1 ,TC1 ,TC1 ,TC1 },/*tc1*/
tc41a[]={INV_,INV_,INV_,INV_,NOP_,SYS_,EOF_,GET_,RS_ ,INV_,INV_,INV_,INV_,INV_,
         INV_,INV_,INV_,INV_,INV_,INV_,TAGC,TAGO,INV_},

*etagtab[] = {tc41, tc41a};
struct parse pcbetag = {"ETAG", lexmark, etagtab, 0, 0, 0, 0};
#undef TC1
/* PCBVAL: State and action table for tokenizing attribute values.
           Columns are based on lextoke (but EOB cannot occur).
*/
/* Symbols for state names (end with a number). */
#define TK1     0   /* Token expected. */
#define SP1     2   /* Separator before token expected. */

static UNCH
/*       inv  rec  sep  sp   nmc  nms  nu   eob   */
tk51 []={TK1 ,TK1 ,TK1 ,TK1 ,SP1 ,SP1 ,SP1 },/*tk1*/
tk51a[]={INVA,INVA,INVA,NOPA,NMTA,NASA,NUMA},

sp51 []={TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 },/*sp1*/
sp51a[]={INVA,INVA,INVA,NOPA,LENA,LENA,LENA},

*valtab[] = {tk51, tk51a, sp51, sp51a};
struct parse pcbval = {"VAL", lextoke, valtab, 0, 0, 0, 0};
#undef TK1
#undef SP1
/* PCBEAL: State and action table for end of attribute specification list.
           If delimiter occurs, process it.  Otherwise, put invalid character
           back for the next parse.
*/
/* Symbols for state names (end with a number). */
#define AL0     0   /* Delimiter expected. */

static UNCH
/*       bit  nmc  nms  re   spc  non  ee   eob  rs   and  grpc grpo lit  lita
         dtgc dtgo opt  or   pero plus rep  rni  seq  refc */
al00 []={AL0 ,AL0 ,AL0 ,AL0 ,AL0 ,AL0 ,AL0 ,AL0 ,AL0 ,AL0 ,AL0 ,AL0 ,AL0 ,AL0 ,
         AL0 ,AL0 ,AL0 ,AL0 ,AL0 ,AL0 ,AL0 ,AL0 ,AL0 ,AL0 },/*al0*/
al00a[]={INV_,INV_,INV_,INV_,INV_,SYS_,EE_ ,GET_,INV_,INV_,INV_,INV_,INV_,INV_,
         GRPE,INV_,INV_,INV_,INV_,INV_,INV_,INV_,INV_,INV_},

*ealtab[] = {al00, al00a};
struct parse pcbeal = {"EAL", lexgrp, ealtab, 0, 0, 0, 0};
#undef AL0

/* PCBSD: State and action tables for SGML declaration parsing. */

/* Symbols for state names. */

#define SP1     0   /* Separator before token expected. */
#define TK1     2   /* Token expected. */
#define CM0     4   /* COM[1] found when sep expected: possible comment.*/
#define CM1     6   /* COM[1] found: possible comment.*/
#define CM2     8   /* COM[2] found; in comment. */
#define CM3    10   /* Ending COM[1] found; end comment or continue it. */

static UNCH
/*       sig  dat  num  nms  spc  non  ee   eob  rs   com  lit  lita tagc  */

sp31 []={SP1 ,SP1 ,SP1 ,SP1 ,TK1 ,SP1 ,SP1 ,SP1 ,TK1 ,CM0 ,TK1 ,TK1 ,SP1 },
sp31a[]={INV_,ISIG,LEN_,LEN_,NOP_,SYS_,EOF_,GET_,RS_ ,NOP_,LIT1,LIT2,ESGD},

tk31 []={TK1 ,TK1 ,SP1 ,SP1 ,TK1 ,TK1 ,TK1 ,TK1 ,TK1 ,CM1 ,TK1 ,TK1 ,SP1 },
tk31a[]={INV_,ISIG,NUM1,NAS1,NOP_,SYS_,EOF_,GET_,RS_ ,NOP_,LIT1,LIT2,ESGD},

cm30 []={SP1 ,CM0 ,SP1 ,SP1 ,SP1 ,CM0 ,SP1 ,CM0 ,SP1 ,CM2 ,SP1 ,SP1 ,SP1 },
cm30a[]={PCI_,ISIG,PCI_,PCI_,PCI_,SYS_,PCI_,GET_,PCI_,NOP_,PCI_,PCI_,PCI_},

cm31 []={TK1 ,CM1 ,TK1 ,TK1 ,TK1 ,CM1 ,TK1 ,CM1 ,TK1 ,CM2 ,TK1 ,TK1 ,TK1 },
cm31a[]={PCI_,ISIG,PCI_,PCI_,PCI_,SYS_,PCI_,GET_,PCI_,NOP_,PCI_,PCI_,PCI_},

cm32 []={CM2 ,CM2 ,CM2 ,CM2 ,CM2 ,CM2 ,TK1 ,CM2 ,CM2 ,CM3 ,CM2 ,CM2 ,CM2 },
cm32a[]={NOP_,ISIG,NOP_,NOP_,NOP_,SYS_,EOF_,GET_,RS_ ,NOP_,NOP_,NOP_,NOP_},

cm33 []={CM2 ,CM3 ,CM2 ,CM2 ,CM2 ,CM3 ,TK1 ,CM3 ,CM2 ,TK1 ,CM2 ,CM2 ,CM2 },
cm33a[]={NOP_,ISIG,NOP_,NOP_,NOP_,SYS_,EOF_,GET_,RS_ ,NOP_,NOP_,NOP_,NOP_},

*sdtab[]={sp31, sp31a, tk31, tk31a, cm30, cm30a, cm31, cm31a, cm32, cm32a,
	  cm33, cm33a};

struct parse pcbsd = {"SD", lexsd, sdtab, 0, 0, 0, 0};

#undef SP1
#undef TK1
#undef CM0
#undef CM1
#undef CM2
#undef CM3
