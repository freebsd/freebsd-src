/* Added exiterr() for terminal errors to prevent SGML.MSG errors.            */
#include "sgmlincl.h"         /* #INCLUDE statements for SGML parser. */
static int iorc;              /* Return code from io* functions */
/* ENTDEF: Process an entity definition and return the pointer to it.
           The entity text must be in permanent storage.
           There is no checking to see if the entity already exists;
           the caller must have done that.
*/
#ifdef USE_PROTOTYPES
PECB entdef(UNCH *ename, UNCH estore, union etext *petx)
#else
PECB entdef(ename, estore, petx)
UNCH *ename;                  /* Entity name (with length and EOS). */
UNCH estore;                  /* Entity storage class. */
union etext *petx;            /* Ptr to entity text union. */
#endif
{
     PECB p;

     p = (PECB)hin((THASH)etab, ename, hash(ename, ENTHASH), ENTSZ);
     memcpy((UNIV)&p->etx, (UNIV)petx, ETEXTSZ);
     p->estore = estore;
     TRACEECB("ENTDEF", p);
     return(p);
}
/* ENTFIND: If an entity exists, return ptr to its ecb.
            Return NULL if it is not defined.
*/
PECB entfind(ename)
UNCH *ename;                  /* Entity name (with length and EOS). */
{
     PECB p;

     p = (PECB)hfind((THASH)etab, ename, hash(ename, ENTHASH));
     TRACEECB("ENTFIND", p);
     return p;
}
/* ENTREF: Process a general or parameter entity reference.
           If the entity is defined it returns the return code from ENTOPEN.
           It returns ENTUNDEF for undefined parameter entity references
           and for general entity references when defaulting is not allowed.
           Otherwise, it uses the default entity text.
*/
int entref(ename)
UNCH *ename;                  /* Entity name (with length and EOS). */
{
     PECB ecb;                /* Entity control block. */

     /* Get the entity control block, if the entity has been defined. */
     if ((ecb = (PECB)hfind((THASH)etab, ename, hash(ename, ENTHASH)))==0
	 || ecb->estore == 0) {
          if (ename[1] == lex.d.pero || ecbdeflt == 0) {
               sgmlerr(35, (struct parse *)0, ename+1, (UNCH *)0);
               return(ENTUNDEF);
          }
          else
	       ecb = usedef(ename);
     }
     return(entopen(ecb));
}
/* ENTOPEN: Open a newly referenced entity.
            Increment the stack pointer (es) and initialize the new entry.
            ENTDATA if entity is CDATA or SDATA, ENTPI if it is PI,
            0 if normal and all o.k.; <0 if not.
*/
int entopen(ecb)
struct entity *ecb;           /* Entity control block. */
{
     int i;                   /* Loop counter. */

     /* See if we have exceeded the entity nesting level. */
     if (es>=ENTLVL) {
          sgmlerr(34, (struct parse *)0, ecb->ename+1, ntoa(ENTLVL));
          return(ENTMAX);
     }
     if (docelsw) sgmlerr(234, (struct parse *)0, (UNCH *)0, (UNCH *)0);
     /* If entity is an etd, pi, or data, return it without creating an scb. */
     switch (ecb->estore) {
     case ESN:
          if (NEXTYPE(ecb->etx.n)!=ESNSUB) {
	       if (!NEDCNDEFINED(ecb->etx.n))
		    sgmlerr(78, (struct parse *)0, NEDCN(ecb->etx.n)+1,
			    ecb->ename+1);
	  }
	  else {
#if 0
	       if (!NEID(ecb->etx.n)) {
		    sgmlerr(149, (struct parse *)0, ecb->ename + 1, (UNCH *)0);
		    return ENTFILE;
	       }
#endif
	       if (sw.nopen >= sd.subdoc)
		    sgmlerr(188, (struct parse *)0,
			    (UNCH *)NULL, (UNCH *)NULL);
	  }
          data = (UNCH *)ecb->etx.n;
          entdatsw = NDECONT;
          return(ENTDATA);
     case ESC:
     case ESX:
	  datalen = ustrlen(ecb->etx.c);
	  /* Ignore reference to empty CDATA entity. */
	  if (datalen == 0 && ecb->estore == ESC) return(0);
          data = ecb->etx.c;
          entdatsw = (ecb->estore==ESC) ? CDECONT : SDECONT;
          return(ENTDATA);
     case ESI:
          datalen = ustrlen(ecb->etx.c);
          data = ecb->etx.c;
          entpisw = 4;
          return(ENTPI);
     }
     /* If the same entity is already open, send msg and ignore it.
        Level 0 needn't be tested, as its entity name is always *DOC.
     */
     for (i = 0; ++i<=es;) if (scbs[i].ecb.enext==ecb) {
          sgmlerr(36, (struct parse *)0, ecb->ename+1, (UNCH *)0);
          return(ENTLOOP);
     }
     /* Update SCB if entity trace is wanted in messages or entity is a file.
        (Avoid this at start when es==-1 or memory will be corrupted.)
     */
     if (es >= 0 && (sw.swenttr || FILESW)) scbset();

     /* Stack the new source control block (we know there is room). */
     ++es;                                      /* Increment scbs index. */
     RCNT = CCO = RSCC = 0;                     /* No records or chars yet. */
     COPIEDSW = 0;
     memcpy((UNIV)&ECB, (UNIV)ecb, (UNS)ENTSZ); /* Copy the ecb into the scb. */
     ECBPTR = ecb;            /* Save the ecb pointer in scb.ecb.enext. */
     TRACEECB("ENTOPEN", ECBPTR);

     /* For memory entities, the read buffer is the entity text.
        The text starts at FBUF, so FPOS should be FBUF-1
        because it is bumped before each character is read.
     */
     if (ECB.estore<ESFM) {FPOS = (FBUF = ECB.etx.c)-1; return 0;}

     /* For file entities, suspend any open file and do first read. */
     if (ECB.etx.x == 0) {
	  --es;
	  switch (ecb->estore) {
	  case ESF:
	       sgmlerr(149, (struct parse *)0, ecb->ename + 1, (UNCH *)0);
	       break;
	  case ESP:
	       sgmlerr(229, (struct parse *)0, ecb->ename + 2, (UNCH *)0);
	       break;
	  default:
	       abort();
	  }
	  return ENTFILE;
     }
     fileopen();                             /* Open new external file. */
     if (iorc<0) {                           /* If open not successful: */
          FPOS = FBUF-1;                     /* Clean CCNT for OPEN error msg.*/
          filerr(32, ecb->ename+1);
          --es;                              /* Pop the stack. */
          return(ENTFILE);
     }
     filepend(es);                           /* Suspend any open file. */
     fileread();                             /* First read of file must be ok.*/
     return 0;
}
/* ENTGET: Get next record of entity (if there is one).
           Otherwise, close the file (if entity is a file) and
           pop the entity stack.  If nothing else is on the stack,
           return -1 to advise the caller.
*/
int entget()
{
     RSCC += (CCO = FPOS-FBUF);
                                   /* Characters-in-record (ignore EOB/EOF). */
     if (es == tages)
	  tagctr += CCO;           /* Update tag length counter. */
     switch (*FPOS) {
     case EOBCHAR:                 /* End of file buffer: refill it. */
          rbufs[-2] = FPOS[-2];
	  rbufs[-1] = FPOS[-1];
          fileread();                         /* Read the file. */
          if (iorc > 0) break;
     readerr:
          filerr(31, ENTITY+1);    /* Treat error as EOF. */
     case EOFCHAR:                 /* End of file: close it. */
          fileclos();              /* Call SGMLIO to close file. */
     conterr:
          if (es==0) {             /* Report if it is primary file. */
               FPOS = FBUF-1;      /* Preserve CCNT for omitted end-tags. */
               return -1;
          }
     case EOS:                /* End of memory entity: pop the stack. */
          TRACEECB("ENTPOP", ECBPTR);
	  if (COPIEDSW) {
	       frem((UNIV)(FBUF + 1));
	       COPIEDSW = 0;
	  }
          --es;                                   /* Pop the SCB stack. */
          if (FBUF) break;                        /* Not a PEND file. */
          filecont();                             /* Resume previous file. */
          if (iorc<0) {                           /* If CONT not successful: */
               filerr(94, ENTITY+1);
               goto conterr;
          }
          fileread();                             /* Read the file. */
          if (iorc<=0) goto readerr;              /* If READ not successful: */
	  rbufs[-1] = SCB.pushback;
	  FPOS += CCO;
	  CCO = 0;
          if (delmscsw && es==0) {                /* End of DTD. */
               delmscsw = 0;
	       *rbufs = lex.d.msc;
	  }
          break;
     }
     return 0;
}
/* USEDEF: Use the default value for an entity reference.
           Returns the ECB for the defaulted entity.
*/
PECB usedef(ename)
UNCH *ename;                  /* Entity name (with length and EOS). */
{
     union etext etx;         /* Save return from entgen. */
     PECB ecb;                /* Entity control block. */
     PNE pne = 0;             /* Ptr to NDATA entity control block. */
     UNCH estore;             /* Default entity storage type. */

     if ((estore = ecbdeflt->estore)<ESFM) /* Default is an internal string. */
          etx.c = ecbdeflt->etx.c;
     else {
      /* Move entity name into fpi. */
      fpidf.fpinm = ename + 1;
      if ((etx.x = entgen(&fpidf))==0)
	  sgmlerr(150, (struct parse *)0, ename + 1, (UNCH *)0);
      if (estore==ESN) {
           memcpy((UNIV)(pne=(PNE)rmalloc((UNS)NESZ)),(UNIV)ecbdeflt->etx.n,(UNS)NESZ);
           NEID(pne) = etx.x;
           etx.n = pne;
      }
     }
     if (sw.swrefmsg) sgmlerr(45, (struct parse *)0, ename+1, (UNCH *)0);
     ++ds.ecbcnt;
     ecb = entdef(ename, estore, &etx);
     ecb->dflt = 1;
     if (pne) NEENAME(pne) = ecb->ename;
     return(ecb);
}
/* SCBSET: Set source control block to current location in the current entity.
           This routine is called by SGML when it returns to the text
           processor and by ERROR when it reports an error.
*/
VOID scbset()
{
     if (es >= 0 && FBUF) {
	  CC = *FPOS;
	  if (*FPOS == DELNONCH)
	       NEXTC = FPOS[1];
	  else
	       NEXTC = 0;
          CCO = FPOS + 1 - FBUF;
     }
}
/* FILEOPEN: Call IOOPEN to open an external entity (file).
*/
VOID fileopen()           /* Open an external entity's file. */
{
     iorc = ioopen(ECB.etx.x, &SCBFCB);
}
/* FILEREAD: Call IOREAD to read an open external entity (file).
*/
VOID fileread()           /* Read the current external entity's file. */
{
     int newfile;
     iorc = ioread(SCBFCB, rbufs, &newfile);
     FPOS = (FBUF = rbufs) - 1;            /* Actual read buffer. */
     if (newfile) RCNT = 0;
}
/* FILEPEND: Call IOPEND to close an open external entity (file) temporarily.
*/
VOID filepend(es)            /* Close the current external entity's file. */
int es;                      /* Local index to scbs. */
{
     while (--es>=0) {             /* Find last external file on stack. */
          int off;
          if (!FILESW) continue;   /* Not an external file. */
	  if (!FBUF) continue;     /* Already suspended. */
	  off = CCO;
	  assert(off >= -1);
	  if (off < 0) off = 0;
	  else CCO = 0;
	  FPOS -= CCO;
	  SCB.pushback = FPOS[-1];
          FBUF = 0;                /* Indicate pending file. */
          RSCC += off;             /* Update characters-in-record counter. */
	  if (es == tages)
	       tagctr += off;      /* Update tag length counter. */
	  iopend(SCBFCB, off, rbufs);
          return;
     }
}
/* FILECONT: Call IOCONT to reopen an external entity (file).
*/
VOID filecont()           /* Open an external entity's file. */
{
     iorc = iocont(SCBFCB);
}
/* FILECLOS: Call IOCLOSE to close an open external entity (file).
*/
VOID fileclos()           /* Close the current external entity's file. */
{
     if (!SCBFCB)
       return;
     ioclose(SCBFCB);
     /* The fcb will have been freed by sgmlio.
	Make sure we don't access it again. */
     SCBFCB = NULL;
}
/* ERROR: Interface to text processor SGML I/O services for error handling.
*/
VOID error(e)
struct error *e;
{
     scbset();                /* Update location in source control block. */
     msgprint(e);
}
/* PTRSRCH: Find a pointer in a list and return its index.
            Search key must be on list as there is no limit test.
            This routine is internal only -- not for user data.
*/
UNIV mdnmtab[] = {
     (UNIV)key[KATTLIST],
     (UNIV)key[KDOCTYPE],
     (UNIV)key[KELEMENT],
     (UNIV)key[KENTITY],
     (UNIV)key[KLINKTYPE],
     (UNIV)key[KLINK],
     (UNIV)key[KNOTATION],
     (UNIV)sgmlkey,
     (UNIV)key[KSHORTREF],
     (UNIV)key[KUSELINK],
     (UNIV)key[KUSEMAP]
};
UNIV pcbtab[] = {
     (UNIV)&pcbconc,
     (UNIV)&pcbcone,
     (UNIV)&pcbconm,
     (UNIV)&pcbconr,
     (UNIV)&pcbetag,
     (UNIV)&pcbgrcm,
     (UNIV)&pcbgrcs,
     (UNIV)&pcbgrnm,
     (UNIV)&pcbgrnt,
     (UNIV)&pcblitc,
     (UNIV)&pcblitp,
     (UNIV)&pcblitr,
     (UNIV)&pcblitt,
     (UNIV)&pcblitv,
     (UNIV)&pcbmd,
     (UNIV)&pcbmdc,
     (UNIV)&pcbmdi,
     (UNIV)&pcbmds,
     (UNIV)&pcbmsc,
     (UNIV)&pcbmsi,
     (UNIV)&pcbmsrc,
     (UNIV)&pcbpro,
     (UNIV)&pcbref,
     (UNIV)&pcbstag,
     (UNIV)&pcbval,
     (UNIV)&pcbeal,
     (UNIV)&pcbsd,
};
UNS ptrsrch(ptrtab, ptr)
UNIV ptrtab[];
UNIV ptr;
{
     UNS i;

     for (i = 0; ; ++i)
          if (ptrtab[i] == ptr)
	       break;
     return i;
}
/* MDERR: Process errors for markup declarations.
          Prepare the special parameters that only exist for
          markup declaration errors.
*/
VOID mderr(number, parm1, parm2)
UNS number;                   /* Error number. */
UNCH *parm1;                  /* Additional parameters (or NULL). */
UNCH *parm2;                  /* Additional parameters (or NULL). */
{
     struct error err;
     errorinit(&err, subdcl ? MDERR : MDERR2, number);
     err.parmno = parmno; 
     err.subdcl = subdcl;
     err.eparm[0] = (UNIV)parm1;
     err.eparm[1] = (UNIV)parm2;
     err.errsp = (sizeof(pcbtab)/sizeof(pcbtab[0])) + ptrsrch(mdnmtab,
							      (UNIV)mdname);
     error(&err);
}
/* SGMLERR: Process errors for SGML parser.
*/
VOID sgmlerr(number, pcb, parm1, parm2)
UNS number;                   /* Error number. */
struct parse *pcb;            /* Current parse control block. */
UNCH *parm1;                  /* Error message parameters. */
UNCH *parm2;                  /* Error message parameters. */
{
     struct error err;
     errorinit(&err, DOCERR, number);
     if (!pcb) pcb = prologsw ? propcb : conpcb;
     err.errsp = ptrsrch(pcbtab, (UNIV)pcb);
     err.eparm[0] = (UNIV)parm1;
     err.eparm[1] = (UNIV)parm2;
     error(&err);
}
/* SAVERR: Save an error for possible later use.
*/
UNIV saverr(number, pcb, parm1, parm2)
UNS number;                   /* Error number. */
struct parse *pcb;            /* Current parse control block. */
UNCH *parm1;                  /* Error message parameters. */
UNCH *parm2;                  /* Error message parameters. */
{
     struct error err;
     errorinit(&err, DOCERR, number);
     if (!pcb) pcb = prologsw ? propcb : conpcb;
     err.errsp = ptrsrch(pcbtab, (UNIV)pcb);
     err.eparm[0] = (UNIV)parm1;
     err.eparm[1] = (UNIV)parm2;
     scbset();
     return msgsave(&err);
}
/* SAVMDERR: Save an md error for possible later use.
*/
UNIV savmderr(number, parm1, parm2)
UNS number;                   /* Error number. */
UNCH *parm1;                  /* Additional parameters (or NULL). */
UNCH *parm2;                  /* Additional parameters (or NULL). */
{
     struct error err;
     errorinit(&err, subdcl ? MDERR : MDERR2, number);
     err.parmno = parmno; 
     err.subdcl = subdcl;
     err.eparm[0] = (UNIV)parm1;
     err.eparm[1] = (UNIV)parm2;
     err.errsp = (sizeof(pcbtab)/sizeof(pcbtab[0])) + ptrsrch(mdnmtab,
							      (UNIV)mdname);
     scbset();
     return msgsave(&err);
}
/* SVDERR: Print a saved error.
*/
VOID svderr(p)
UNIV p;
{
     msgsprint(p);
}
/* EXITERR: Process terminal errors for SGML parser.
*/
VOID exiterr(number, pcb)
UNS number;                   /* Error number. */
struct parse *pcb;            /* Current parse control block. */
{
     struct error err;
     errorinit(&err, EXITERR, number);
     if (!pcb) pcb = prologsw ? propcb : conpcb;
     err.errsp = ptrsrch(pcbtab, (UNIV)pcb);
     error(&err);
     /* The error handler should have exited. */
     abort();
}
/* SYNERR: Process syntax errors for SGML parser.
*/
VOID synerr(number, pcb)
UNS number;                   /* Error number. */
struct parse *pcb;            /* Current parse control block. */
{
     struct error err;
     errorinit(&err, DOCERR, number);
     err.errsp = ptrsrch(pcbtab, (UNIV)pcb);
     error(&err);
}
/* FILERR: Process a file access error.
*/
VOID filerr(number, parm)
UNS number;
UNCH *parm;
{
     struct error err;
     errorinit(&err, FILERR, number);
     err.eparm[0] = (UNIV)parm;
     err.sverrno = errno;
     error(&err);
}
/* ERRORINIT: Constructor for struct error.
*/
VOID errorinit(e, type, number)
struct error *e;
UNS type;
UNS number;
{
     int i;
     e->errtype = type;
     e->errnum = number;
     e->errsp = 0;
     for (i = 0; i < MAXARGS; i++)
          e->eparm[i] = 0;
     e->parmno = 0;
     e->subdcl = 0;
}
/*
Local Variables:
c-indent-level: 5
c-continued-statement-offset: 5
c-brace-offset: -5
c-argdecl-indent: 0
c-label-offset: -5
comment-column: 30
End:
*/
