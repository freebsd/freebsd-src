/* ACTION.H: Symbols for all PCB action codes. */
/* CONACT.H: Symbols for content parse action names (end with '_').
             There must be no conflict with PARSEACT.H, which
             uses 0 through 19, or SGMLACT.H, which uses 20 through 32
             (except that 31 - 32 can be defined here because they are
             used only by PARSEPRO and do not conflict with SGML.C).
*/
#define CIR_   31   /* Invalid character(s) ignored in MDS; restarting parse. */
#define DTD_   32   /* Process DOCTYPE declaration. */
#define DTE_   33   /* End of DOCTYPE declaration. */
#define PEP_   34   /* TEMP: Previous character ended prolog. */
#define DAS_   35   /* Current character begins data. */
#define FCE_   36   /* Process free character (SR12-18, 21-30). */
#define DCE_   37   /* Data character in element text; change PCB. */
#define LAS_   38   /* Start lookahead buffer with current character. */
#define LAM_   39   /* Move character to lookahead buffer. */
#define LAF_   40   /* Flush the lookahead buffer; REPEATCC. */
#define NED_   41   /* Process null end-tag delimiter. */
#define NET_   42   /* Process null end-tag. */
#define NST_   43   /* Process null start-tag. */
#define NLF_   44   /* Flush lookahead buffer except for trailing NET or SR. */
#define ETC_   45   /* End-tag in CDATA or RCDATA; treat as data if invalid. */
#define SRMIN  46   /* Dummy for SHORT REFERENCES: srn = SRn - SRMIN. */
#define SR1_   47   /* TAB */
#define SR2_   48   /* RE */
#define SR3_   49   /* RS */
#define SR4_   50   /* Leading blanks */
#define SR5_   51   /* Null record */
#define DAR_   52   /* Flush data buffer after repeating current character. */
#define SR7_   53   /* Trailing blanks */
#define SR8_   54   /* Space */
#define SR9_   55   /* Two or more blanks */
#define SR10   56   /* Quotation mark (first data character) */
#define SR11   57   /* Number sign */
#define SR12   58   /* FCE CHARACTERS start here */
/*         _   59   */
#define BSQ_   60   /* Blank sequence begun; find its end. */
/*             61      In use by PARSEACT.H */
/*             62      In use by PARSEACT.H */
/*             63      In use by PARSEACT.H */
/*             64      In use by PARSEACT.H */
#define SR19   65   /* Hyphen */
#define SR20   66   /* Two hyphens */
#define SR25   71   /* Left bracket */
#define SR26   72   /* Right bracket */
#define RBR_   73   /* Two right brackets. */
#define GTR_   74   /* EOB with pending data character */
#define MSP_   75   /* Marked section start in prolog outside DTD */
#define APP_   76   /* APPINFO (other than NONE) */
#define STE_   77   /* Start tag ended prolog */

/* GRPACT.H: Symbols for group tokenization action names (all alpha).
             There must be no conflict with PARSEACT.H, which
             uses 0 - 19.
*/
#define AND    20   /* AND connector found. */
#define DTAG   21   /* Data tag token group occurred (treat as #CHARS). */
#define GRPE   22   /* Group ended. */
#define GRP_   23   /* Group started. */
#define NAS_   24   /* Name started in content model or name group. */
#define NMT_   25   /* Name or name token started in name token group. */
#define OPT    26   /* OPT occurrence indicator for previous token. */
#define OR     27   /* OR connector found. */
#define OREP   28   /* OREP occurrence indicator for previous token. */
#define REP    29   /* REP occurrence indicator for previous token. */
#define RNS_   30   /* Reserved name started (#PCDATA). */
#define SEQ    31   /* SEQ connector found. */
/* LITACT.H: Symbols for content parse action names (end with '_').
             There must be no conflict with PARSEACT.H, which
             uses 0 through 19.
*/
#define MLA_   20   /* Move character to look-aside data buffer. */
#define LPR_   21   /* Move previous character to data buffer. */
#define RSM_   22   /* Process record start and move it to data buffer. */
#define FUN_   23   /* Replace function character with a space. */
#define LP2_   24   /* Move previous two characters to data buffer. */
#define MLE_   25   /* Minimum literal error: invalid character ignored. */
#define RPR_   26   /* Remove previous character from data buffer; terminate. */
#define TER_   27   /* Terminate the parse. */
/* MDACT.H: Symbols for markup declaration parse action names (all alpha).
            There must be no conflict with PARSEACT.H, which
            uses 0 - 19.
*/
#define CDR    20   /* CD[1] (MINUS) occurred previously. */
#define EMD    21   /* End of markup declaration. */
#define GRPS   22   /* Group started. */
#define LIT    23   /* Literal started: character data. */
#define LITE   24   /* Literal started: character data; LITA is delimiter. */
#define MGRP   25   /* Minus exception group (MINUS,GRPO). */
#define NAS    26   /* Name started. */
#define NMT    27   /* Name token started. */
#define NUM    28   /* Number or number token started. */
#define PEN    29   /* Parameter entity name being defined (PERO found). */
#define PGRP   30   /* Plus exception group (PLUS,GRPO). */
#define RNS    31   /* Reserved name started. */
#define MDS    32   /* Markup declaration subset start. */
#define PENR   33   /* REPEATCC; PERO found. */
/* PARSEACT.H: Symbols for common parse action names (end with '_').
               There must be no conflict with other action name
               files, which use numbers greater than 19.
*/
#define CRA_    1   /* Character reference: alphabetic. */
#define CRN_    2   /* Character reference: numeric; non-char refs o.k.. */
#define NON_    3   /* Single byte of non-character data found. */
#define EOF_    4   /* Error: illegal entity end; resume old input; return. */
#define ER_     5   /* Entity reference; start new input source; continue. */
#define GET_    6   /* EOB, EOS, or EE: resume old input source; continue. */
#define INV_    7   /* Error: invalid char terminated markup; repeat char. */
#define LEN_    8   /* Error: length limit exceeded; end markup; repeat char. */
#define NOP_    9   /* No action necessary. */
#define PCI_   10   /* Previous character was invalid. */
#define PER_   11   /* Parameter reference; start new input source; continue. */
#define RC2_   12   /* Back up two characters. */
#define RCC_   13   /* Repeat current character. */
#define RCR_   14   /* Repeat current character and return to caller. */
#define EE_    15   /* EOS or EE: resume old input source; return to caller. */
#define RS_    16   /* Record start: ccnt=0; ++rcnt. */
#define ERX_   17   /* Entity reference; start new input source; return. */
#define SYS_   18   /* Error allowed: SYSCHAR in input stream; replace it. */
#define EOD_   19   /* End of document. */
/* Number way out of order to avoid recompilation. */
#define NSC_   58   /* Handle DELNONCH/DELXNONCH when NON_ is allowed */
#define PEX_   61   /* Parameter entity ref; start new input source; return. */
#define DEF_   62   /* Data entity found. */
#define PIE_   63   /* PI entity found (needed in markup). */
#define LNR_   64   /* LEN_ error with extra REPEATCC. */
/* SGMLACT.H: Symbols for content parse action names (end with '_')
              that are returned to SGML.C for processing.
              There must be no conflict with PARSEACT.H, which
              uses 0 through 19, or CONACT.H, which uses 34 and above.
              (Note: 31 is also used in CONACT.H, but no conflict
              is created because they are tested only in PARSEPRO.C, which
              completes before SGML.C starts to examine those codes.
              Also, when EOD_ is returned from PARSECON, it is changed
              to LOP_.)
*/
#define CON_   20   /* Normal content action (one of the following). */
#define DAF_   21   /* Data found. */
#define ETG_   22   /* Process end-tag. */
#define MD_    23   /* Process markup declaration (NAMESTRT found). */
#define MDC_   24   /* Process markup declaration comment (CD found). */
#define MSS_   25   /* Process marked section start. */
#define MSE_   26   /* Process marked section end. */
#define PIS_   27   /* Processing instruction (string). */
#define REF_   28   /* Record end found. */
#define STG_   29   /* Process start-tag. */
#define RSR_   30   /* Return RS to effect SGML state transition. */
#define LOP_   31   /* Loop for new content without returning anything. */
/* TAGACT.H: Symbols for tag parse action names (all alpha).
             There must be no conflict with PARSEACT.H, which
             uses 0 - 19.
*/
#define AVD    20   /* Delimited attribute value started: normal delimiter. */
#define AVU    21   /* Undelimited value started. */
#define ETIC   22   /* Tag closed with ETI. */
#define NVS    23   /* Name of attribute or value started. */
#define NASV   24   /* Saved NAS was actually an NTV. */
#define NTV    25   /* Name token value started; get name and full value. */
#define TAGC   26   /* Tag closed normally. */
#define TAGO   27   /* Tag closed implicitly by TAGO character. */
#define AVDA   28   /* Delimited attribute value started: alternative delim. */
#define DSC    29   /* Closed by DSC character. */
/* VALACT.H: Symbols for attribute value tokenization action names (all alpha).
*/
#define NOPA    0   /* No action necessary. */
#define INVA    1   /* Invalid character; terminate parse. */
#define LENA    2   /* Length limit of token exceeded; terminate parse. */
#define NASA    3   /* Name started. */
#define NMTA    4   /* Name token started. */
#define NUMA    5   /* Number or number token started. */

/* SGML declaration parsing actions. */

#define ESGD 20			/* End of SGML declaration. */
#define LIT1 21			/* Literal started. */
#define LIT2 22			/* Literal started with LITA delimiter. */
#define NUM1 23			/* Number started. */
#define NAS1 24			/* Name started. */
#define ISIG 25			/* Insignificant character occurred. */
