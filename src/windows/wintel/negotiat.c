/*
 *   negotiat.c
 *
 *   Telnet option negotiation functions
 *

/*
 * Includes
 */

/* #define USETEK */
/* #define USERAS */

#ifdef DEBUG   /* define this to print the raw network data to debuging monitor */
#define NEGOTIATEDEBUG
#endif

#if 0
#define PRINT_EVERYTHING /* talk a lot */
#endif

#include <time.h>
#include "telnet.h"
#include "telnet_arpa.h"
#include "auth.h"
#include "encrypt.h"

#define STNORM 0
#define NEGOTIATE 1
#define ESCFOUND 5
#define IACFOUND 6

unsigned char parsedat[256];

/* Local functions */
static void parse_subnegotiat(kstream ks,int end_sub);

/* Local variables */
static char *telstates[]={
  "EOF",
  "Suspend Process",
  "Abort Process",
  "Unknown (239)",
  "Subnegotiation End",
  "NOP",
  "Data Mark",
  "Break",
  "Interrupt Process",
  "Abort Output",
  "Are You There",
  "Erase Character",
  "Erase Line",
  "Go Ahead",
  "Subnegotiate",
  "Will",
  "Won't",
  "Do",
  "Don't"
};

static char *teloptions[256]={      /* ascii strings for Telnet options */
  "Binary",                               /* 0 */
  "Echo",
  "Reconnection",
  "Supress Go Ahead",
  "Message Size Negotiation",
  "Status",                               /* 5 */
  "Timing Mark",
  "Remote Controlled Trans and Echo",
  "Output Line Width",
  "Output Page Size",
  "Output Carriage-Return Disposition",   /* 10 */
  "Output Horizontal Tab Stops",
  "Output Horizontal Tab Disposition",
  "Output Formfeed Disposition",
  "Output Vertical Tabstops",
  "Output Vertical Tab Disposition",      /* 15 */
  "Output Linefeed Disposition",
  "Extended ASCII",
  "Logout",
  "Byte Macro",
  "Data Entry Terminal",                  /* 20 */
  "SUPDUP",
  "SUPDUP Output",
  "Send Location",
  "Terminal Type",
  "End of Record",                        /* 25 */
  "TACACS User Identification",
  "Output Marking",
  "Terminal Location Number",
  "3270 Regime",
  "X.3 PAD",                              /* 30 */
  "Negotiate About Window Size",
  "Terminal Speed",
  "Toggle Flow Control",
  "Linemode",
  "X Display Location",                   /* 35 */
  "Environment",
  "Authentication",
  "Data Encryption",
  "39",
  "40","41","42","43","44","45","46","47","48","49",
  "50","51","52","53","54","55","56","57","58","59",
  "60","61","62","63","64","65","66","67","68","69",
  "70","71","72","73","74","75","76","77","78","79",
  "80","81","82","83","84","85","86","87","88","89",
  "90","91","92","93","94","95","96","97","98","99",
  "100","101","102","103","104","105","106","107","108","109",
  "110","111","112","113","114","115","116","117","118","119",
  "120","121","122","123","124","125","126","127","128","129",
  "130","131","132","133","134","135","136","137","138","139",
  "140","141","142","143","144","145","146","147","148","149",
  "150","151","152","153","154","155","156","157","158","159",
  "160","161","162","163","164","165","166","167","168","169",
  "170","171","172","173","174","175","176","177","178","179",
  "180","181","182","183","184","185","186","187","188","189",
  "190","191","192","193","194","195","196","197","198","199",
  "200","201","202","203","204","205","206","207","208","209",
  "210","211","212","213","214","215","216","217","218","219",
  "220","221","222","223","224","225","226","227","228","229",
  "230","231","232","233","234","235","236","237","238","239",
  "240","241","242","243","244","245","246","247","248","249",
  "250","251","252","253","254",
  "Extended Options List"     /* 255 */
};

static char *LMoptions[]={      /* ascii strings for Linemode sub-options */
  "None",     "MODE",     "FORWARDMASK",     "SLC"
};

static char *ModeOptions[]={      /* ascii strings for Linemode edit options */
  "None",     "EDIT",     "TRAPSIG",      "ACK",     "SOFT TAB",    "LIT ECHO"
};

static char *SLCoptions[]={     /* ascii strings for Linemode SLC characters */
  "None",     "SYNCH",    "BREAK",    "IP",       "ABORT OUTPUT",
  "AYT",      "EOR",      "ABORT",    "EOF",      "SUSP",
  "EC",       "EL",       "EW",       "RP",       "LNEXT",
  "XON",      "XOFF",     "FORW1",    "FORW2",    "MCL",
  "MCR",      "MCWL",     "MCWR",     "MCBOL",    "MCEOL",
  "INSRT",    "OVER",     "ECR",      "EWR",      "EBOL",
  "EEOL"
};

static char *SLCflags[]={      /* ascii strings for Linemode SLC flags */
  "SLC_NOSUPPORT",    "SLC_CANTCHANGE",   "SLC_VALUE",    "SLC_DEFAULT"
};

/* Linemode default character for each function */
static unsigned char LMdefaults[NTELOPTS + 1]={
  (unsigned char)-1,          /* zero isn't used */
  (unsigned char)-1,          /* we don't support SYNCH */
  3,                          /* ^C is default for BRK */
  3,                          /* ^C is default for IP */
  15,                         /* ^O is default for AO */
  25,                         /* ^Y is default for AYT */         /* 5 */
  (unsigned char)-1,          /* we don't support EOR */
  3,                          /* ^C is default for ABORT */
  4,                          /* ^D is default for EOF */
  26,                         /* ^Z is default for SUSP */
  8,                          /* ^H is default for EC */          /* 10 */
  21,                         /* ^U is default for EL */
  23,                         /* ^W is default for EW */
  18,                         /* ^R is default for RP */
  22,                         /* ^V is default for LNEXT */
  17,                         /* ^Q is default for XON */         /* 15 */
  19,                         /* ^S is default for XOFF */
  22,                         /* ^V is default for FORW1 */
  5,                          /* ^E is default for FORW2 */
  (unsigned char)-1,          /* we don't support MCL */
  (unsigned char)-1,          /* we don't support MCR */          /* 20 */
  (unsigned char)-1,          /* we don't support MCWL */
  (unsigned char)-1,          /* we don't support MCWR */
  (unsigned char)-1,          /* we don't support MCBOL */
  (unsigned char)-1,          /* we don't support MCEOL */
  (unsigned char)-1,          /* we don't support INSRT */        /* 25 */
  (unsigned char)-1,          /* we don't support OVER */
  (unsigned char)-1,          /* we don't support ECR */
  (unsigned char)-1,          /* we don't support EWR */
  (unsigned char)-1,          /* we don't support EBOL */
  (unsigned char)-1           /* we don't support EEOL */         /* 30 */
};


/*
 *  Function :   start_negotiation()
 *  Purpose  :   Send the initial negotiations on the network and print
 *               the negotitations to the console screen.
 *  Parameters   :
 *           dat - the port number to write to
 *           cvs - the console's virtual screen
 *  Returns  :   none
 *  Calls    :   tprintf(), netprintf()
 *  Called by    :   dosessions()
 */
void
start_negotiation(kstream ks)
{
  char buf[128];

  /* Send the initial telnet negotiations */
#ifdef ENCRYPTION /* XXX */
  if (encrypt_flag)
    wsprintf(buf,"%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c",
	     IAC, WILL, TELOPT_AUTHENTICATION,
	     IAC, WILL, TELOPT_ENCRYPT,
	     IAC,   DO, TELOPT_SGA,
	     IAC,   DO, TELOPT_ECHO,
	     IAC, WILL, TELOPT_NAWS
	     );
  else
#endif
    wsprintf(buf,"%c%c%c%c%c%c%c%c%c%c%c%c",
	     IAC, WILL, TELOPT_AUTHENTICATION,
	     IAC,   DO, TELOPT_SGA,
	     IAC,   DO, TELOPT_ECHO,
	     IAC, WILL, TELOPT_NAWS
	     );
  TelnetSend(ks,buf,lstrlen(buf),0);

#ifdef NOT
  /* check whether we are going to be output mapping */
  if(tw->mapoutput) {
    netprintf(tw->pnum,"%c%c%c",IAC,DO,TELOPT_BINARY);
    /* set the flag indicating we wanted server to start transmitting binary */
    tw->uwantbinary=1;
    netprintf(tw->pnum,"%c%c%c",IAC,WILL,TELOPT_BINARY);
    /* set the flag indicating we want to start transmitting binary */
    tw->iwantbinary=1;
  } /* end if */
#endif

  /* Print to the console what we just did */
#ifdef NEGOTIATEDEBUG
  wsprintf(strTmp,"SEND: %s %s\r\n",telstates[DO - TELCMD_FIRST],
	   teloptions[TELOPT_ECHO]);
  OutputDebugString(strTmp);
  wsprintf(strTmp,"SEND: %s %s\r\n",telstates[DO - TELCMD_FIRST],
	   teloptions[TELOPT_SGA]);
  OutputDebugString(strTmp);
  wsprintf(strTmp,"SEND: %s %s\r\n",telstates[WILL - TELCMD_FIRST],
	   teloptions[TELOPT_NAWS]);
  OutputDebugString(strTmp);

#ifdef NOT
  tprintf(cvs,"SEND: %s %s\r\n",telstates[DO - TELCMD_FIRST],
	  teloptions[BINARY]);
  tprintf(cvs,"SEND: %s %s\r\n",telstates[WILL - TELCMD_FIRST],
	  teloptions[BINARY]);
#endif
#endif
}   /* end start_negotiation() */

/*
 *  parse
 *   Do the telnet negotiation parsing.
 *
 *   look at the string which has just come in from outside and
 *   check for special sequences that we are interested in.
 *
 *   Tries to pass through routine strings immediately, waiting for special
 *   characters ESC and IAC to change modes.
 */
void
parse(CONNECTION *con,unsigned char *st,int cnt)
{
  static int sub_pos; /* the position we are in the subnegotiation parsing */
  static int end_sub; /* index of last byte in parsedat in a subnegotiation */
  unsigned char *mark, *orig;
  char buf[256];
  kstream ks;

  ks = con->ks;

#ifdef PRINT_EVERYTHING
  hexdump("Options to process:", st, cnt);
#endif /* PRINT_EVERYTHING */

  orig = st;                /* remember beginning point */
  mark = st + cnt;            /* set to end of input string */

#ifdef HUH
  netpush(tw->pnum);
#endif

  /*
   *  traverse string, looking for any special characters which indicate that
   *  we need to change modes.
   */
  while(st < mark) {

    while(con->telstate != STNORM && st < mark) {
      switch(con->telstate) {
      case IACFOUND:              /* telnet option negotiation */
	if(*st == IAC) {          /* real data=255 */
	  st++;                   /* real 255 will get sent */
	  con->telstate = STNORM;
	  break;
	} /* end if */

	if(*st > 239) {
	  con->telstate = *st++; /* by what the option is */
	  break;
	} /* end if */

#ifdef NEGOTIATEDEBUG
	wsprintf(buf, "\r\n strange telnet option");
	OutputDebugString(buf);
#endif
	orig=++st;
	con->telstate=STNORM;
	break;

      case EL:     /* received a telnet erase line command */
      case EC:     /* received a telnet erase character command */
      case AYT:    /* received a telnet Are-You-There command */
      case AO:     /* received a telnet Abort Output command */
      case IP:     /* received a telnet Interrupt Process command */
      case BREAK:  /* received a telnet Break command */
      case DM:     /* received a telnet Data Mark command */
      case NOP:    /* received a telnet No Operation command */
      case SE:     /* received a telnet Subnegotiation End command */
      case ABORT:  /* received a telnet Abort Process command */
      case SUSP:   /* received a telnet Suspend Process command */
      case xEOF:   /* received a telnet EOF command */
#ifdef NEGOTIATEDEBUG
	wsprintf(buf,"RECV: %s\r\n",
		 telstates[con->telstate-TELCMD_FIRST]);
	OutputDebugString(buf);
#endif
	con->telstate=STNORM;
	orig=++st;
	break;

      case GA:       /* telnet go ahead option*/
#ifdef NEGOTIATEDEBUG
	wsprintf(buf,"RECV: %s\r\n",
		 telstates[con->telstate-TELCMD_FIRST]);
	OutputDebugString(buf);
#endif
	con->telstate=STNORM;
	orig=++st;
	break;

      case DO:     /* received a telnet DO negotiation */
#ifdef NEGOTIATEDEBUG
	wsprintf(buf,"RECV: %s %s\r\n",
		 telstates[con->telstate-TELCMD_FIRST],teloptions[*st]);
	OutputDebugString(buf);
#endif
	switch(*st) {
#ifdef NOT
	case TELOPT_BINARY:       /* DO: binary transmission */
	  if(!tw->ibinary) { /* binary */
	    if(!tw->iwantbinary) {
	      netprintf(tw->pnum,"%c%c%c",
			IAC,WILL,BINARY);
	      if(tw->condebug>0)
		tprintf(cv,"SEND: %s %s\r\n",
			telstates[WILL - TELCMD_FIRST],
			teloptions[BINARY]);
	    } /* end if */
	    else
	      tw->iwantbinary=0;  /* turn off this now */
	    tw->ibinary=1;
	  } /* end if */
	  else {
	    if(tw->condebug>0)
	      tprintf(cv,"NO REPLY NEEDED: %s %s\r\n",
		      telstates[WILL - TELCMD_FIRST],
		      teloptions[BINARY]);
	  } /* end else */
	  break;
#endif

	case TELOPT_SGA:       /* DO: Suppress go-ahead */
	  if(!con->igoahead) { /* suppress go-ahead */
	    wsprintf(buf,"%c%c%c",IAC,WILL,TELOPT_SGA);
	    TelnetSend(ks,buf,lstrlen(buf),0);
#ifdef NEGOTIATEDEBUG
	    wsprintf(strTmp,"SEND: %s %s\r\n",
		     telstates[WILL - TELCMD_FIRST],
		     teloptions[TELOPT_SGA]);
	    OutputDebugString(strTmp);
	    OutputDebugString("igoahead");
#endif
	    con->igoahead=1;
	  } /* end if */
	  else {
#ifdef NEGOTIATEDEBUG
	    wsprintf(strTmp,
		     "NO REPLY NEEDED: %s %s\r\n",
		     telstates[WILL - TELCMD_FIRST],
		     teloptions[TELOPT_SGA]);
	    OutputDebugString(strTmp);
#endif
	  } /* end else */
	  break;

	case TELOPT_TTYPE:      /* DO: terminal type negotiation */
	  if(!con->termsent) {
	    con->termsent=TRUE;
	    wsprintf(buf,"%c%c%c",IAC,WILL,TELOPT_TTYPE);
	    TelnetSend(ks,buf,lstrlen(buf),0);
#ifdef NEGOTIATEDEBUG
	    wsprintf(strTmp,"SEND: %s %s\r\n",
		     telstates[WILL - TELCMD_FIRST],
		     teloptions[TELOPT_TTYPE]);
	    OutputDebugString(strTmp);
#endif
	  } /* end if */
	  else {
#ifdef NEGOTIATEDEBUG
	    wsprintf(strTmp,"NO REPLY NEEDED: %s %s\r\n",
		     telstates[WILL - TELCMD_FIRST],
		     teloptions[TELOPT_TTYPE]);
	    OutputDebugString(strTmp);
#endif
	  } /* end else */
	  break;

#ifdef LATER
	case TELOPT_LINEMODE:      /* DO: linemode negotiation */
	  tw->lmflag=1;   /* set the linemode flag */
	  netprintf(tw->pnum,"%c%c%c",IAC,WILL,TELOPT_LINEMODE);
	  /*
	   * Tell the other side to send us
	   * its default character set
	   */
	  netprintf(tw->pnum,"%c%c%c%c",
		    IAC,SB,TELOPT_LINEMODE,SLC,0,SLC_DEFAULT,0,IAC,SE);
	  if(tw->condebug>0) {
	    tprintf(cv,"SEND: %s %s\r\n",
		    telstates[WILL - TELCMD_FIRST],
		    teloptions[TELOPT_LINEMODE]);
	    tprintf(cv,
		    "SEND: SB LINEMODE SLC 0 SLC_DEFAULT 0 IAC SE\r\n");
	  } /* end if */
	  break;
#endif
	case TELOPT_NAWS:      /* DO: Negotiate About Window Size */
	  con->bResizeable=TRUE;
	  send_naws(con);
	  break;

	case TELOPT_AUTHENTICATION: /* DO: Authentication requested */
	  wsprintf(buf, "%c%c%c", IAC, WILL, TELOPT_AUTHENTICATION);
	  TelnetSend(ks, buf, lstrlen(buf), 0);
#ifdef NEGOTIATEDEBUG
	  wsprintf(strTmp,"SEND: %s %s\r\n",
		   telstates[WILL - TELCMD_FIRST],
		   teloptions[TELOPT_AUTHENTICATION]);
	  OutputDebugString(strTmp);
#endif
	  break;

#ifdef ENCRYPTION
	case TELOPT_ENCRYPT: /* DO: Remote is willing to receive encrypted */
	  wsprintf(buf, "%c%c%c", IAC,
		   (encrypt_flag ? WILL : WONT), TELOPT_ENCRYPT);
	  TelnetSend(ks, buf, lstrlen(buf), 0);
#ifdef NEGOTIATEDEBUG
	  wsprintf(strTmp,"SEND: %s %s\r\n",
		   telstates[(encrypt_flag ? WILL : WONT)
			    - TELCMD_FIRST],
		   teloptions[TELOPT_ENCRYPT]);
	  OutputDebugString(strTmp);
#endif
	  break;
#endif /* ENCRYPTION */

	default:        /* DO: */
	  wsprintf(buf, "%c%c%c", IAC, WONT, *st);
	  TelnetSend(ks, buf, lstrlen(buf), 0);
#ifdef NEGOTIATEDEBUG
	  wsprintf(strTmp,"SEND: %s %s\r\n",
		   telstates[WONT - TELCMD_FIRST], teloptions[*st]);
	  OutputDebugString(strTmp);
#endif
	  break;

	} /* end switch */
	con->telstate = STNORM;
	orig = ++st;
	break;

      case DONT:       /* Received a telnet DONT option */
	switch (*st) {
	case TELOPT_NAWS:
	  con->bResizeable=FALSE;
#ifdef NEGOTIATEDEBUG
	  wsprintf(strTmp,"RECV: %s %s\r\n",
		   telstates[con->telstate-TELCMD_FIRST],teloptions[*st]);
	  OutputDebugString(strTmp);
#endif
	  break;

#ifdef NOT
	case BINARY: /* DONT: check for binary neg. */
	  if(tw->ibinary) {   /* binary */
	    if(!tw->iwantbinary) {
	      netprintf(tw->pnum,"%c%c%c",IAC,WONT,BINARY);
	      if(tw->condebug>0)
		tprintf(cv,"SEND: %s %s\r\n",
			telstates[WONT-TELCMD_FIRST],
			teloptions[BINARY]);
	    } /* end if */
	    else
	      tw->iwantbinary=0;  /* turn off this now */
	    tw->ibinary=0;
	    tw->mapoutput=0;    /* turn output mapping off */
	  } /* end if */
#ifdef NEGOTIATEDEBUG
	  wsprintf(strTmp,"NO REPLY NEEDED: %s %s\r\n",
		   telstates[WONT-TELCMD_FIRST],
		   teloptions[BINARY]);
	  OutputDebugString(strTmp);
#endif
	  break;
#endif
#ifdef ENCRYPTION
	case ENCRYPTION:
	  break;
#endif
	}

	/* all these just fall through to here... */

	con->telstate=STNORM;
	orig=++st;
	break;

      case WILL:       /* received a telnet WILL option */
#ifdef NEGOTIATEDEBUG
	wsprintf(strTmp,"RECV: %s %s\r\n",
		 telstates[con->telstate-TELCMD_FIRST],
		 teloptions[*st]);
	OutputDebugString(strTmp);
#endif
	switch(*st) {
#ifdef NOT
	case TELOPT_BINARY:            /* WILL: binary */
	  if(!tw->ubinary) {   /* binary */
	    if(!tw->uwantbinary) {
	      netprintf(tw->pnum,"%c%c%c",
			IAC,DO,TELOPT_BINARY);
	      if(tw->condebug>0)
		tprintf(cv,"SEND: %s %s\r\n",
			telstates[DO - TELCMD_FIRST],
			teloptions[TELOPT_BINARY]);
	    } /* end if */
	    else
	      tw->uwantbinary=0;  /* turn off this now */
	    tw->ubinary=1;
	  } /* end if */
	  else {
	    if(tw->condebug>0)
	      tprintf(cv,"NO REPLY NEEDED: %s %s\r\n",
		      telstates[DO - TELCMD_FIRST],
		      teloptions[TELOPT_BINARY]);
	  } /* end else */
	  break;
#endif

	case TELOPT_SGA:               /* WILL: suppress go-ahead */
	  if(!con->ugoahead) {
	    con->ugoahead=1;
	    wsprintf(buf,"%c%c%c",IAC,DO,TELOPT_SGA); /* ack */
	    TelnetSend(ks,buf,lstrlen(buf),0);
#ifdef NEGOTIATEDEBUG
	    wsprintf(strTmp,"SEND: %s %s\r\n",
		     telstates[DO - TELCMD_FIRST],
		     teloptions[TELOPT_SGA]);
	    OutputDebugString(strTmp);
#endif
	  } /* end if */
	  break;

	case TELOPT_ECHO:              /* WILL: echo */
	  if(!con->echo) {
	    con->echo = 1;
	    wsprintf(buf, "%c%c%c", IAC, DO, TELOPT_ECHO); /* ack */
	    TelnetSend(ks, buf, lstrlen(buf), 0);
#ifdef NEGOTIATEDEBUG
	    wsprintf(strTmp,"SEND: %s %s\r\n",
		     telstates[DO - TELCMD_FIRST],
		     teloptions[TELOPT_ECHO]);
	    OutputDebugString(strTmp);
#endif
	  } /* end if */
	  break;

	case TELOPT_TM:        /* WILL: Timing mark */
	  con->timing=0;
	  break;
#ifdef ENCRYPTION
	case TELOPT_ENCRYPT:   /* WILL: decrypt our input */
	  wsprintf(buf, "%c%c%c", IAC,
		   (encrypt_flag ? DO : DONT), TELOPT_ENCRYPT);
	  TelnetSend(ks, buf, lstrlen(buf), 0);
	  if (encrypt_flag)
	    encrypt_send_support();

#ifdef NEGOTIATEDEBUG
	  wsprintf(strTmp,"SEND: %s %s\r\n",
		   telstates[(encrypt_flag ? DO : DONT) - TELCMD_FIRST],
		   teloptions[TELOPT_ENCRYPT]);
	  OutputDebugString(strTmp);
#endif
	  break;
#endif

	default:
	  wsprintf(buf,"%c%c%c",IAC,DONT,*st);
	  TelnetSend(ks,buf,lstrlen(buf),0);
#ifdef NEGOTIATEDEBUG
	  wsprintf(strTmp,"SEND: %s %s\r\n",
		   telstates[DONT-TELCMD_FIRST],teloptions[*st]);
	  OutputDebugString(strTmp);
#endif
	  break;
	} /* end switch */
	con->telstate=STNORM;
	orig=++st;
	break;

      case WONT:       /* Received a telnet WONT option */
#ifdef NEGOTIATEDEBUG
	wsprintf(strTmp,"RECV: %s %s\r\n",
		 telstates[con->telstate-TELCMD_FIRST],teloptions[*st]);
	OutputDebugString((LPSTR)strTmp);
#endif
	con->telstate=STNORM;
	switch(*st++) {     /* which option? */
#ifdef NOT
	case BINARY:            /* WONT: binary */
	  if(tw->ubinary) {  /* binary */
	    if(!tw->uwantbinary) {
	      netprintf(tw->pnum,"%c%c%c",
			IAC,DONT,BINARY);
	      if(tw->condebug>0)
		tprintf(cv,"SEND: %s %s\r\n",
			telstates[DONT-TELCMD_FIRST],
			teloptions[BINARY]);
	    } /* end if */
	    else
	      tw->uwantbinary=0;  /* turn off this now */
	    tw->ubinary=0;
	    tw->mapoutput=0; /* turn output mapping off */
	  } /* end if */
	  else {
	    if(tw->condebug>0)
	      tprintf(cv,"NO REPLY NEEDED: %s %s\r\n",
		      telstates[DONT-TELCMD_FIRST],
		      teloptions[BINARY]);
	  } /* end else */
	  break;

#endif
	case TELOPT_ECHO:              /* WONT: echo */
	  if(con->echo) {
	    con->echo=0;
	    wsprintf(buf,"%c%c%c",IAC,DONT,TELOPT_ECHO);
	    TelnetSend(ks,buf,lstrlen(buf),0);
#ifdef NEGOTIATEDEBUG
	    wsprintf(strTmp,"SEND: %s %s\r\n",
		     telstates[DONT-TELCMD_FIRST],
		     teloptions[TELOPT_ECHO]);
	    OutputDebugString(strTmp);
	    OutputDebugString("Other side won't echo!");
#endif
	  } /* end if */
	  break;

	case TELOPT_TM:    /* WONT: Telnet timing mark option */
	  con->timing=0;
	  break;

#ifdef ENCRYPTION
	case TELOPT_ENCRYPT:   /* WONT: don't encrypt our input */
	  break;
#endif

	default:
	  break;
	} /* end switch */
	orig=st;
	break;

      case SB:        /* telnet sub-options negotiation */
	con->telstate=NEGOTIATE;
	orig=st;
	end_sub=0;
	sub_pos=con->substate=0;     /* Defined for each */
#ifdef OLD_WAY
	break;
#endif

      case NEGOTIATE:
        /* until we change sub-negotiation states, accumulate bytes */
	if(con->substate==0) {
	  if(*st==IAC) {  /* check if we found an IAC byte */
	    if(*(st+1)==IAC) {  /* skip over double IAC's */
	      st++;
	      parsedat[sub_pos++]=*st++;
	    } /* end if */
	    else {
	      end_sub=sub_pos;
	      con->substate=*st++;
	    } /* end else */
	  } /* end if */
	  else     /* otherwise, just stash the byte */
	    parsedat[sub_pos++]=*st++;
	} /* end if */
	else {
	  con->substate=*st++;
	  /* check if we've really ended the sub-negotiations */
	  if(con->substate==SE)
	    parse_subnegotiat(ks,end_sub);

	  orig=st;
	  /*
	   * XXX hack to decrypt the rest of the buffer
	   */
	  if (encrypt_flag == 2) {
	    decrypt_ks_hack(orig, mark - orig);
	    encrypt_flag = 1;
	  }

	  con->telstate=STNORM;
	} /* end else */
	break;

      default:
	con->telstate=STNORM;
	break;
      } /* end switch */
    } /* end while */

    /*
     * quick scan of the remaining string, skip chars while they are
     * uninteresting
     */
    if(con->telstate==STNORM && st<mark) {
      /*
       *  skip along as fast as possible until an interesting character is found
       */
      while(st<mark && *st!=27 && *st!=IAC) {
#if 0
	if(!tw->ubinary)
	  *st&=127;                 /* mask off high bit */
#endif
	st++;
      } /* end while */
#if 0
      if(!tw->timing)
	parsewrite(tw,orig,st-orig);
#endif
      orig=st;                /* forget what we have sent already */
      if(st<mark)
	switch(*st) {
	case IAC:           /* telnet IAC */
	  con->telstate=IACFOUND;
	  st++;
	  break;

	default:
#ifdef NEGOTIATEDEBUG
	  wsprintf(buf," strange char>128 0x%x\r\n", *st);
	  OutputDebugString(buf);
#endif
	  st++;
	  break;
	} /* end switch */
    } /* end if */
  } /* end while */
}   /* end parse() */

/*
 *  Function :   parse_subnegotiat()
 *  Purpose  :   Parse the telnet sub-negotiations read into the parsedat
 *               array.
 *  Parameters   :
 *           end_sub - index of the character in the 'parsedat' array which
 *                           is the last byte in a sub-negotiation
 *  Returns  :   none
 *  Calls    :
 *  Called by    :   parse()
 */
static void
parse_subnegotiat(kstream ks, int end_sub)
{
  char buf[128];

  switch(parsedat[0]) {
  case TELOPT_TTYPE:
    if(parsedat[1]==1) {
      /* QAK!!! */    wsprintf(buf,"%c%c%c%cvt100%c%c",IAC,SB,TELOPT_TTYPE,
			       0,IAC,SE);
      TelnetSend(ks,(LPSTR)buf,11,0);
#ifdef NEGOTIATEDEBUG
      wsprintf(strTmp,"SB TERMINAL-TYPE SEND\r\n"
	       "SEND: SB TERMINAL-TYPE IS vt100 \r\n len=%d \r\n",
	       lstrlen((LPSTR)buf));
      OutputDebugString(strTmp);
#endif
    }
    break;

  case TELOPT_AUTHENTICATION:
    auth_parse(ks, parsedat, end_sub);
    break;
#ifdef ENCRYPTION
  case TELOPT_ENCRYPT:
    if (encrypt_flag)
      encrypt_parse(ks, parsedat, end_sub);
    break;
#endif
  default:
    break;
  } /* end switch */
}   /* parse_subnegotiat */


/*
 *  Function :   send_naws
 *  Purpose  :   Send a window size sub-negotiation.
 *  Parameters   :
 *           ks - the kstream to send to.
 *  Returns  :   none
 */
void
send_naws(CONNECTION *con)
{
  unsigned char buf[40];
  int len;

  wsprintf(buf, "%c%c%c", IAC, SB, TELOPT_NAWS);
  len = 3;

  buf[len++] = HIBYTE(con->width);
  if (buf[len-1] == IAC) buf[len++] = IAC;

  buf[len++] = LOBYTE(con->width);
  if (buf[len-1] == IAC) buf[len++] = IAC;

  buf[len++] = HIBYTE(con->height);
  if (buf[len-1] == IAC) buf[len++] = IAC;

  buf[len++] = LOBYTE(con->height);
  if (buf[len-1] == IAC) buf[len++] = IAC;

  buf[len++] = IAC;
  buf[len++] = SE;

  TelnetSend(con->ks, buf, len, 0);

#ifdef NEGOTIATEDEBUG
  wsprintf(buf, "SEND: SB NAWS %d %d %d %d IAC SE\r\n",
	   HIBYTE(con->width), LOBYTE(con->width),
	   HIBYTE(con->height), LOBYTE(con->height));
  OutputDebugString(buf);
#endif

} /* send_naws */
