#include "header.h"

println(s) char *s; {
  printf("%s\n", s);
}

esc(s) char *s; {
  printf("%c%s", 27, s);
}

esc2(s1, s2) char s1, s2; {
  printf("%c%s%s", 27, s1, s2);
}

brcstr(ps, c) char *ps, c; {
  printf("%c[%s%c", 27, ps, c);
}

brc(pn,c) int pn; char c; {
  printf("%c[%d%c", 27, pn, c);
}

brc2(pn1, pn2 ,c) int pn1, pn2; char c; {
  printf("%c[%d;%d%c", 27, pn1, pn2, c);
}

cub(pn) int pn; {  /* Cursor Backward */
  brc(pn,'D');
}
cud(pn) int pn; {  /* Cursor Down */
  brc(pn,'B');
}
cuf(pn) int pn; {  /* Cursor Forward */
  brc(pn,'C');
}
cup(pn1, pn2) int pn1, pn2; {  /* Cursor Position */
  brc2(pn1, pn2, 'H');
}
cuu(pn) int pn; {  /* Cursor Up */
  brc(pn,'A');
}
da() {  /* Device Attributes */
  brc(0,'c');
}
decaln() {  /* Screen Alignment Display */
  esc("#8");
}
decdhl(lower) int lower; {  /* Double Height Line (also double width) */
  if (lower) esc("#4");
  else       esc("#3");
}
decdwl() {  /* Double Wide Line */
  esc("#6");
}
deckpam() {  /* Keypad Application Mode */
  esc("=");
}
deckpnm() {  /* Keypad Numeric Mode */
  esc(">");
}
decll(ps) char *ps; {  /* Load LEDs */
  brcstr(ps, 'q');
}
decrc() {  /* Restore Cursor */
  esc("8");
}
decreqtparm(pn) int pn; {  /* Request Terminal Parameters */
  brc(pn,'x');
}
decsc() {  /* Save Cursor */
  esc("7");
}
decstbm(pn1, pn2) int pn1, pn2; {  /* Set Top and Bottom Margins */
  if (pn1 || pn2) brc2(pn1, pn2, 'r');
  else            esc("[r");
  /* Good for >24-line terminals */
}
decswl() {  /* Single With Line */
  esc("#5");
}
dectst(pn) int pn; {  /* Invoke Confidence Test */
  brc2(2, pn, 'y');
}
dsr(pn) int pn; {  /* Device Status Report */
  brc(pn, 'n');
}
ed(pn) int pn; {  /* Erase in Display */
  brc(pn, 'J');
}
el(pn) int pn; {  /* Erase in Line */
  brc(pn,'K');
}
hts() {  /* Horizontal Tabulation Set */
  esc("H");
}
hvp(pn1, pn2) int pn1, pn2; {  /* Horizontal and Vertical Position */
  brc2(pn1, pn2, 'f');
}
ind() {  /* Index */
  esc("D");
}
nel() {  /* Next Line */
  esc("E");
}
ri() {  /* Reverse Index */
  esc("M");
}
ris() { /*  Reset to Initial State */
  esc("c");
}
rm(ps) char *ps; {  /* Reset Mode */
  brcstr(ps, 'l');
}
scs(g,c) int g; char c; {  /* Select character Set */
  printf("%c%c%c%c%c%c%c", 27, g ? ')' : '(', c,
                           27, g ? '(' : ')', 'B',
			   g ? 14 : 15);
}
sgr(ps) char *ps; {  /* Select Graphic Rendition */
  brcstr(ps, 'm');
}
sm(ps) char *ps; {  /* Set Mode */
  brcstr(ps, 'h');
}
tbc(pn) int pn; {  /* Tabulation Clear */
  brc(pn, 'g');
}

vt52cup(l,c) int l,c; {
  printf("%cY%c%c", 27, l + 31, c + 31);
}

char inchar() {

  /*
   *   Wait until a character is typed on the terminal
   *   then read it, without waiting for CR.
   */

#ifdef UNIX
  int lval, waittime, getpid(); static int val; char ch;

  fflush(stdout);
  lval = val;
  brkrd = 0;
  reading = 1;
  read(0,&ch,1);
  reading = 0;
  if (brkrd)
    val = 0177;
  else
    val = ch;
  if ((val==0177) && (val==lval))
    kill(getpid(), (int) SIGTERM);
#endif
#ifdef SARG10
  int val, waittime;

  waittime = 0;
  while(!uuo(051,2,&val)) {		/* TTCALL 2, (INCHRS)	*/
    zleep(100);				/* Wait 0.1 seconds	*/
    if ((waittime += ttymode) > 600)	/* Time-out, in case	*/
      return('\177');			/* of hung in ttybin(1)	*/
  }
#endif
#ifdef SARG20	/* try to fix a time-out function */
  int val, waittime;

  waittime = 0;
  while(jsys(SIBE,2,_PRIIN) == 0) {	/* Is input empty? */
    zleep(100);
    if ((waittime += ttymode) > 600)
      return('\177');
  }
  ejsys(BIN,_PRIIN);
  val = jsac[2];
#endif
  return(val);
}

char *instr() {

  /*
   *   Get an unfinished string from the terminal:
   *   wait until a character is typed on the terminal,
   *   then read it, and all other available characters.
   *   Return a pointer to that string.
   */


  int i, val, crflag; long l1; char ch;
  static char result[80];

  i = 0;
  result[i++] = inchar();
/* Wait 0.1 seconds (1 second in vanilla UNIX) */
#ifdef SARG10
  if (trmop(01031,0) < 5) zleep(500); /* wait longer if low speed */
  else                    zleep(100);
#else
  zleep(100);
#endif
#ifdef UNIX
  fflush(stdout);
#ifdef XENIX
  while(rdchk(0)) {
    read(0,result+i,1);
    if (i++ == 78) break;
  }
#else
#ifdef SIII
  while(read(2,result+i,1) == 1)
    if (i++ == 78) break;
#else
  while(ioctl(0,FIONREAD,&l1), l1 > 0L) {
    while(l1-- > 0L) {
      read(0,result+i,1);
      if (i++ == 78) goto out1;
    }
  }
out1:
#endif
#endif
#endif
#ifdef SARG10
  while(uuo(051,2,&val)) {	/* TTCALL 2, (INCHRS)  */
    if (!(val == '\012' && crflag))	/* TOPS-10 adds LF to CR */
      result[i++] = val;
    crflag = val == '\015';
    if (i == 79) break;
    zleep(50);          /* Wait 0.05 seconds */
  }
#endif
#ifdef SARG20
  while(jsys(SIBE,2,_PRIIN) != 0) {	/* read input until buffer is empty */
    ejsys(BIN,_PRIIN);
    result[i++] = jsac[2];
    if (i == 79) break;
    zleep(50);		/* Wait 0.05 seconds */
  }
#endif
  result[i] = '\0';
  return(result);
}

ttybin(bin) int bin; {
#ifdef SARG10
  #define OPEN 050
  #define IO_MOD 0000017
  #define _IOPIM 2
  #define _IOASC 0
  #define _TOPAG 01021
  #define _TOSET 01000

  int v;
  static int arglst[] = {
    _IOPIM,
    `TTY`,
    0
  };
  arglst[0] = bin ? _IOPIM : _IOASC;
  v = uuo(OPEN, 1, &arglst[0]);
  if (!v) { printf("OPEN failed"); exit(); }
  trmop(_TOPAG + _TOSET, bin ? 0 : 1);
  ttymode = bin;
#endif
#ifdef SARG20
  /*	TTYBIN will set the line in BINARY/ASCII mode
   *	BINARY mode is needed to send control characters
   *	Bit 28 must be 0 (we don't flip it).
   *	Bit 29 is used for the mode change.
   */

  #define _TTASC 0000100
  #define _MOXOF 0000043

  int v;

  ejsys(RFMOD,_CTTRM);
  v = ejsys(SFMOD,_CTTRM, bin ? (~_TTASC & jsac[2]) : (_TTASC | jsac[2]));
  if (v) { printf("SFMOD failed"); exit(); }
  v = ejsys(MTOPR,_CTTRM,_MOXOF,0);
  if (v) { printf("MTOPR failed"); exit(); }
#endif
}

#ifdef SARG20
/*
 *	SUPERBIN turns off/on all input character interrupts
 *	This affects ^C, ^O, ^T
 *	Beware where and how you use it !!!!!!!
 */

superbin(bin) int bin; {
  int v;

  v = ejsys(STIW,(0//-5), bin ? 0 : -1);
  if (v) { printf("STIW superbinary setting failed"); exit(); }
  ttymode = bin;
}

/*
 *	PAGE affects the ^S/^Q handshake.
 *	Set bit 34 to turn it on. Clear it for off.
 */

page(bin) int bin; {
  int v;

  #define TT_PGM 0000002

  ejsys(RFMOD,_CTTRM);	/* Get the current terminal status */
  v = ejsys(STPAR,_CTTRM, bin ? (TT_PGM | jsac[2]) : (~TT_PGM & jsac[2]));
  if (v) { printf("STPAR failed"); exit(); }
}
#endif

trmop(fc,arg) int fc, arg; {
#ifdef SARG10
  int retvalp;
  int arglst[3];

  /* TRMOP is a TOPS-10 monitor call that does things to the terminal. */

  /* Find out TTY nbr (PA1050 barfs if TRMOP get -1 instead of udx)    */
  /* A TRMNO monitor call returns the udx (Universal Device Index)     */

  arglst[0] = fc;		/* function code	*/
  arglst[1] = calli(0115, -1);	/* udx, TRMNO. UUO	*/
  arglst[2] = arg;		/* Optional argument	*/

  if (calli(0116, 3 // &arglst[0], &retvalp))           /* TRMOP. UUO */
  return (retvalp);
  else {
    printf("?Error return in TRMOP.");
    exit();
  }
#endif
}

inputline(s) char *s; {
  scanf("%s",s);
#ifdef SARG10
  readnl();
#endif
#ifdef SARG20
  readnl();
#endif
}

inflush() {

  /*
   *   Flush input buffer, make sure no pending input character
   */

  int val;

#ifdef UNIX
#ifdef XENIX
  while(rdchk(0)) read(0,&val,1);
#else
#ifdef SIII
  while(read(2,&val,1));
#else
  long l1;
  ioctl (0, FIONREAD, &l1);
  while(l1-- > 0L) read(0,&val,1);
#endif
#endif
#endif
#ifdef SARG10
  while(uuo(051,2,&val))	/* TTCALL 2, (INCHRS)  */
    ;
#endif
#ifdef SARG20
  ejsys(CFIBF,_PRIIN);		/* Clear input buffer */
#endif
}

zleep(t) int t; {

/*
 *    Sleep and do nothing (don't waste CPU) for t milliseconds
 */

#ifdef SARG10
  calli(072,t);		/* (HIBER) t milliseconds */
#endif
#ifdef SARG20
  ejsys(DISMS,t);	/* DISMISS for t milliseconds */
#endif
#ifdef UNIX
  t = t / 1000;
  if (t == 0) t = 1;
  sleep(t);		/* UNIX can only sleep whole seconds */
#endif
}
