/*
                               VTTEST.C

         Written Novemeber 1983 - July 1984 by Per Lindberg,
         Stockholm University Computer Center (QZ), Sweden.

                  THE MAD PROGRAMMER STRIKES AGAIN!

                   This software is (c) 1984 by QZ
               Non-commercial use and copying allowed.

If you are developing a commercial product, and use this program to do
it, and that product is successful, please send a sum of money of your
choice to the address below.

*/

/* $FreeBSD$ */

#include "header.h"

char inchar(), *instr(), *lookup();

struct table {
    int key;
    char *msg;
} paritytable[] = {
    { 1, "NONE" },
    { 4, "ODD"  },
    { 5, "EVEN" },
    { -1, "" }
},nbitstable[] = {
    { 1, "8" },
    { 2, "7" },
    { -1,"" }
},speedtable[] = {
    {   0,    "50" },
    {   8,    "75" },
    {  16,   "110" },
    {  24,   "132.5"},
    {  32,   "150" },
    {  40,   "200" },
    {  48,   "300" },
    {  56,   "600" },
    {  64,  "1200" },
    {  72,  "1800" },
    {  80,  "2000" },
    {  88,  "2400" },
    {  96,  "3600" },
    { 104,  "4800" },
    { 112,  "9600" },
    { 120, "19200" },
    { -1, "" }
};

#ifdef USEMYSTTY
#ifndef stty
int stty(fd,ptr)
int fd;
struct sgttyb *ptr;
{
	return(ioctl(fd, TIOCSETP, ptr));
}
#endif
#ifndef gtty
int gtty(fd,ptr)
int fd;
struct sgttyb *ptr;
{
	return(ioctl(fd, TIOCGETP, ptr));
}
#endif
#endif

main() {

  int menuchoice;

  static char *mainmenu[] = {
      "Exit",
      "Test of cursor movements",
      "Test of screen features",
      "Test of character sets",
      "Test of double-sized characters",
      "Test of keyboard",
      "Test of terminal reports",
      "Test of VT52 mode",
      "Test of VT102 features (Insert/Delete Char/Line)",
      "Test of known bugs",
      "Test of reset and self-test",
      ""
    };

#ifdef UNIX
  initterminal(setjmp(intrenv));
  signal(SIGINT, onbrk);
  signal(SIGTERM, onterm);
  reading = 0;
#else
  initterminal(0);
#endif
  do {
#ifdef SARG20
    ttybin(1);	/* set line to binary mode again. It's reset somehow!! */
#endif
    ed(2);
    cup(5,10); printf("VT100 test program, version %s", VERSION);
    cup(7,10); println("Choose test type:");
    menuchoice = menu(mainmenu);
    switch (menuchoice) {
      case 1:  tst_movements();   break;
      case 2:  tst_screen();      break;
      case 3:  tst_characters();  break;
      case 4:  tst_doublesize();  break;
      case 5:  tst_keyboard();    break;
      case 6:  tst_reports();     break;
      case 7:  tst_vt52();        break;
      case 8:  tst_insdel();      break;
      case 9:  tst_bugs();        break;
      case 10: tst_rst();         break;
    }
  } while (menuchoice);
  bye();
}

tst_movements() {

  /* Test of:
     CUF (Cursor Forward)
     CUB (Cursor Backward)
     CUD (Cursor Down)      IND (Index)  NEL (Next Line)
     CUU (Cursor Up)        RI  (Reverse Index)
     CUP (Cursor Position)  HVP (Horizontal and Vertical Position)
     ED  (Erase in Display)
     EL  (Erase in Line)
     DECALN (Screen Alignment Display)
     <CR> <BS>
     Cursor control characters inside CSI sequences
  */

  int i, row, col, pass, width, hlfxtra;
  char c, *ctext = "This is a correct sentence";

  for (pass = 0; pass <= 1; pass++) {
    if (pass == 0) { rm("?3"); width =  80; hlfxtra =  0; }
    else           { sm("?3"); width = 132; hlfxtra = 26; }

    decaln();
    cup( 9,10+hlfxtra); ed(1);
    cup(18,60+hlfxtra); ed(0); el(1);
    cup( 9,71+hlfxtra); el(0);
    for (row = 10; row <= 16; row++) {
      cup(row, 10+hlfxtra); el(1);
      cup(row, 71+hlfxtra); el(0);
    }
    cup(17,30); el(2);
    for (col = 1; col <= width; col++) {
      hvp(24, col); printf("*");
      hvp( 1, col); printf("*");
    }
    cup(2,2);
    for (row = 2; row <= 23; row++) {
      printf("+");
      cub(1);
      ind();
    }
    cup(23,width-1);
    for (row = 23; row >=2; row--) {
      printf("+");
      cub(1); ri();
    }
    cup(2,1);
    for (row = 2; row <= 23; row++) {
      printf("*");
	cup(row, width);
      printf("*");
      cub(10);
      if(row < 10)
	nel();
      else
        printf("\n");
    }
    cup(2,10);
    cub(42+hlfxtra); cuf(2);
    for (col = 3; col <= width-2; col++) {
      printf("+");
      cuf(0); cub(2); cuf(1);
    }
    cup(23,70+hlfxtra);
    cuf(42+hlfxtra); cub(2);
    for (col = width-2; col >= 3; col--) {
      printf("+");
      cub(1); cuf(1); cub(0); printf("%c", 8);
    }
    cup( 1, 1); cuu(10); cuu(1); cuu(0);
    cup(24,width); cud(10); cud(1); cud(0);

    cup(10,12+hlfxtra);
    for (row = 10; row <= 15; row++) {
      for (col = 12+hlfxtra; col <= 69+hlfxtra; col++) printf(" ");
      cud(1); cub(58);
    }
    cuu(5); cuf(1);
    printf("The screen should be cleared,  and have an unbroken bor-");
    cup(12,13+hlfxtra);
    printf("der of *'s and +'s around the edge,   and exactly in the");
    cup(13,13+hlfxtra);
    printf("middle  there should be a frame of E's around this  text");
    cup(14,13+hlfxtra);
    printf("with  one (1) free position around it.    ");
    holdit();
  }
  rm("?3");

  ed(2);
  cup(1,1);
  println("Test of cursor-control characters inside ESC sequences.");
  println("Below should be two identical lines:");
  println("");
  println("A B C D E F G H I J K L M N O P Q R S");
  for (i = 1; i < 20; i++) {
    printf("%c", 64 + i);
    brcstr("2\010", 'C');	/* Two forward, one backspace */
  }
  println("");
  println("");
  holdit();

  ed(2);
  cup(1,1);
  println("Test of leading zeros in ESC sequences.");
  printf("Two lines below you should see the sentence \"%s\".",ctext);
  for (col = 1; *ctext; col++)
   printf("\033[00000000004;00000000%dH%c",col,*ctext++);
  cup(20,1);
  holdit();
}

tst_screen() {

  /* Test of:
     - DECSTBM (Set Top and Bottom Margins)
     - TBC     (Tabulation Clear)
     - HTS     (Horizontal Tabulation Set)
     - SM RM   (Set/Reset mode): - 80/132 chars
                                 - Origin: Realtive/absolute
				 - Scroll: Smooth/jump
				 - Wraparound
     - SGR     (Select Graphic Rendition)
     - SM RM   (Set/Reset Mode) - Inverse
     - DECSC   (Save Cursor)
     - DECRC   (Restore Cursor)
  */

  int i, j, cset, row, col, down, soft, background;

  static char *tststr = "*qx`";
  static char *attr[5] = { ";0", ";1", ";4", ";5", ";7" };

  cup(1,1);
  sm("?7");  /* Wrap Around ON */
  for (col = 1; col <= 160; col++) printf("*");
  rm("?7");  /* Wrap Around OFF */
  cup(3,1);
  for (col = 1; col <= 160; col++) printf("*");
  sm("?7");  /* Wrap Around ON */
  cup(5,1);
  println("This should be three identical lines of *'s completely filling");
  println("the top of the screen without any empty lines between.");
  println("(Test of WRAP AROUND mode setting.)");
  holdit();

  ed(2);
  tbc(3);
  cup(1,1);
  for (col = 1; col <= 78; col += 3) {
    cuf(3); hts();
  }
  cup(1,4);
  for (col = 4; col <= 78; col += 6) {
    tbc(0); cuf(6);
  }
  cup(1,7); tbc(1); tbc(2); /* no-op */
  cup(1,1); for (col = 1; col <= 78; col += 6) printf("\t*");
  cup(2,2); for (col = 2; col <= 78; col += 6) printf("     *");
  cup(4,1);
  println("Test of TAB setting/resetting. These two lines");
  printf("should look the same. ");
  holdit();
  for (background = 0; background <= 1; background++) {
    if (background) rm("?5");
    else            sm("?5");
    sm("?3"); /* 132 cols */
    ed(2);    /* VT100 clears screen on SM3/RM3, but not obviously, so... */
    cup(1,1); tbc(3);
    for (col = 1; col <= 132; col += 8) {
      cuf(8); hts();
    }
    cup(1,1); for (col = 1; col <= 130; col += 10) printf("1234567890");
    printf("12");
    for (row = 3; row <= 20; row++) {
      cup(row,row);
      printf("This is 132 column mode, %s background.",
      background ? "dark" : "light");
    }
    holdit();
    rm("?3"); /* 80 cols */
    ed(2);    /* VT100 clears screen on SM3/RM3, but not obviously, so... */
    cup(1,1); for (col = 1; col <= 80; col += 10) printf("1234567890");
    for (row = 3; row <= 20; row++) {
      cup(row,row);
      printf("This is 80 column mode, %s background.",
      background ? "dark" : "light");
    }
    holdit();
  }

  ed(2);
  sm("?6"); /* Origin mode (relative) */
  for (soft = -1; soft <= 0; soft++) {
    if (soft) sm("?4");
    else      rm("?4");
    for (row = 12; row >= 1; row -= 11) {
      decstbm(row, 24-row+1);
      ed(2);
      for (down = 0; down >= -1; down--) {
        if (down) cuu(24);
	else      cud(24);
	for (i = 1; i <= 30; i++) {
	  printf("%s scroll %s region %d Line %d\n",
		 soft ? "Soft" : "Jump",
		 down ? "down" : "up",
		 2*(13-row), i);
	  if (down) { ri(); ri(); }
	}
      }
      holdit();
    }
  }
  ed(2);
  decstbm(23,24);
  printf(
  "\nOrigin mode test. This line should be at the bottom of the screen.");
  cup(1,1);
  printf("%s",
  "This line should be the one above the bottom of the screeen. ");
  holdit();
  ed(2);
  rm("?6"); /* Origin mode (absolute) */
  cup(24,1);
  printf(
  "Origin mode test. This line should be at the bottom of the screen.");
  cup(1,1);
  printf("%s", "This line should be at the top if the screen. ");
  holdit();
  decstbm(1,24);

  ed(2);
  cup( 1,20); printf("Graphic rendition test pattern:");
  cup( 4, 1); sgr("0");         printf("vanilla");
  cup( 4,40); sgr("0;1");       printf("bold");
  cup( 6, 6); sgr(";4");        printf("underline");
  cup( 6,45);sgr(";1");sgr("4");printf("bold underline");
  cup( 8, 1); sgr("0;5");       printf("blink");
  cup( 8,40); sgr("0;5;1");     printf("bold blink");
  cup(10, 6); sgr("0;4;5");     printf("underline blink");
  cup(10,45); sgr("0;1;4;5");   printf("bold underline blink");
  cup(12, 1); sgr("1;4;5;0;7"); printf("negative");
  cup(12,40); sgr("0;1;7");     printf("bold negative");
  cup(14, 6); sgr("0;4;7");     printf("underline negative");
  cup(14,45); sgr("0;1;4;7");   printf("bold underline negative");
  cup(16, 1); sgr("1;4;;5;7");  printf("blink negative");
  cup(16,40); sgr("0;1;5;7");   printf("bold blink negative");
  cup(18, 6); sgr("0;4;5;7");   printf("underline blink negative");
  cup(18,45); sgr("0;1;4;5;7"); printf("bold underline blink negative");
  sgr("");

  rm("?5"); /* Inverse video off */
  cup(23,1); el(0); printf("Dark background. "); holdit();
  sm("?5"); /* Inverse video */
  cup(23,1); el(0); printf("Light background. "); holdit();
  rm("?5");
  ed(2);
  cup(8,12); printf("normal");
  cup(8,24); printf("bold");
  cup(8,36); printf("underscored");
  cup(8,48); printf("blinking");
  cup(8,60); printf("reversed");
  cup(10,1); printf("stars:");
  cup(12,1); printf("line:");
  cup(14,1); printf("x'es:");
  cup(16,1); printf("diamonds:");
  for (cset = 0; cset <= 3; cset++) {
    for (i = 0; i <= 4; i++) {
    cup(10 + 2 * cset, 12 + 12 * i);
    sgr(attr[i]);
    if (cset == 0 || cset == 2) scs(0,'B');
    else                        scs(0,'0');
      for (j = 0; j <= 4; j++) {
        printf("%c", tststr[cset]);
      }
      decsc();
      cup(cset + 1, i + 1); sgr(""); scs(0,'B'); printf("A");
      decrc();
      for (j = 0; j <= 4; j++) {
        printf("%c", tststr[cset]);
      }
    }
  }
  sgr("0"); scs(0,'B'); cup(21,1);
  println("Test of the SAVE/RESTORE CURSOR feature. There should");
  println("be ten characters of each flavour, and a rectangle");
  println("of 5 x 4 A's filling the top left of the screen.");
  holdit();
}

tst_characters() {
  /* Test of:
     SCS    (Select character Set)
  */

  int i, j, g, cset;
  char chcode[5], *setmsg[5];

  chcode[0] = 'A';
  chcode[1] = 'B';
  chcode[2] = '0';
  chcode[3] = '1';
  chcode[4] = '2';
  setmsg[0] = "UK / national";
  setmsg[1] = "US ASCII";
  setmsg[2] = "Special graphics and line drawing";
  setmsg[3] = "Alternate character ROM standard characters";
  setmsg[4] = "Alternate character ROM special graphics";

  cup(1,10); printf("Selected as G0 (with SI)");
  cup(1,48); printf("Selected as G1 (with SO)");
  for (cset = 0; cset <= 4; cset++) {
    scs(1,'B');
    cup(3 + 4 * cset, 1);
    sgr("1");
    printf("Character set %c (%s)",chcode[cset], setmsg[cset]);
    sgr("0");
    for (g = 0; g <= 1; g++) {
      scs(g,chcode[cset]);
      for (i = 1; i <= 3; i++) {
        cup(3 + 4 * cset + i, 10 + 38 * g);
        for (j = 0; j <= 31; j++) {
	  printf("%c", i * 32 + j);
	}
      }
    }
  }
  scs(1,'B');
  cup(24,1); printf("These are the installed character sets. ");
  holdit();
}

tst_doublesize() {
  /* Test of:
     DECSWL  (Single Width Line)
     DECDWL  (Double Width Line)
     DECDHL  (Double Height Line) (also implicit double width)
  */

  int col, i, w, w1;

  /* Print the test pattern in both 80 and 132 character width  */

  for(w = 0; w <= 1; w++) {
    w1 = 13 * w;

    ed(2);
    cup(1, 1);
    if (w) { sm("?3"); printf("132 column mode"); }
    else   { rm("?3"); printf(" 80 column mode"); }

    cup( 5, 3 + 2 * w1);
    printf("v------- left margin");

    cup( 7, 3 + 2 * w1);
    printf("This is a normal-sized line");
    decdhl(0); decdhl(1); decdwl(); decswl();

    cup( 9, 2 + w1);
    printf("This is a Double-width line");
    decswl(); decdhl(0); decdhl(1); decdwl();

    cup(11, 2 + w1);
    decdwl(); decswl(); decdhl(1); decdhl(0);
    printf("This is a Double-width-and-height line");
    cup(12, 2 + w1);
    decdwl(); decswl(); decdhl(0); decdhl(1);
    printf("This is a Double-width-and-height line");

    cup(14, 2 + w1);
    decdwl(); decswl(); decdhl(1); decdhl(0); el(2);
    printf("This is another such line");
    cup(15, 2 + w1);
    decdwl(); decswl(); decdhl(0); decdhl(1);
    printf("This is another such line");

    cup(17, 3 + 2 * w1);
    printf("^------- left margin");

    cup(21, 1);
    printf("This is not a double-width line");
    for (i = 0; i <= 1; i++) {
      cup(21,6);
      if (i) { printf("**is**"); decdwl(); }
      else   { printf("is not"); decswl(); }
      cup(23,1); holdit();
    }
  }
  /* Set vanilla tabs for next test */
  cup(1,1); tbc(3); for (col = 1; col <= 132; col += 8) { cuf(8); hts(); }
  rm("?3");
  ed(2);
  scs(0,'0');

  cup( 8,1); decdhl(0); printf("lqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqk");
  cup( 9,1); decdhl(1); printf("lqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqk");
  cup(10,1); decdhl(0); printf("x%c%c%c%c%cx",9,9,9,9,9);
  cup(11,1); decdhl(1); printf("x%c%c%c%c%cx",9,9,9,9,9);
  cup(12,1); decdhl(0); printf("x%c%c%c%c%cx",9,9,9,9,9);
  cup(13,1); decdhl(1); printf("x%c%c%c%c%cx",9,9,9,9,9);
  cup(14,1); decdhl(0); printf("x                                      x");
  cup(15,1); decdhl(1); printf("x                                      x");
  cup(16,1); decdhl(0); printf("mqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqj");
  cup(17,1); decdhl(1); printf("mqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqj");
  scs(0,'B'); sgr("1;5");
  cup(12,3);
  printf("* The mad programmer strikes again * ");
  cup(13,3); printf("%c",9); cub(6);
  printf("* The mad programmer strikes again *");
  sgr("0");
  cup(22,1);
  println("Another test pattern...  a frame with blinking bold text,");
  printf("all in double-height double-width size. ");
  holdit();

  decstbm(8,24); /* Absolute origin mode, so cursor is set at (1,1) */
  cup(8,1);
  for (i = 1; i <= 12; i++)
    ri();
  decstbm(0,0);	/* No scroll region	*/
  cup(1,1);
  printf("%s", "Exactly half of the box should remain. ");
  holdit();
}

tst_keyboard() {

/* Test of:
     - DECLL   (Load LEDs)
     - Keyboard return messages
     - SM RM   (Set/Reset Mode) - Cursor Keys
                                - Auto repeat
     - DECKPAM (Keypad Application Mode)
     - DECKPNM (Keypad Numeric Mode)

The standard VT100 keayboard layout:

                                                        UP   DN   LE  RI

ESC   1!   2@   3#   4$   5%   6^   7&   8*   9(   0)   -_   =+   `~  BS

TAB*    qQ   wW   eE   rR   tT   yY   uU   iI   oO   pP   [{   ]}      DEL

**   **   aA   sS   dD   fF   gG   hH   jJ   kK   lL   ;:   ,"   RETN  \|

**   ****   zZ   xX   cC   vV   bB   nN   mM   ,<   .>   /?   ****   LF

             ****************SPACE BAR****************

                                                           PF1 PF2 PF3 PF4

                                                           *7* *8* *9* *-*

                                                           *4* *5* *6* *,*

                                                           *1* *2* *3*

                                                           ***0*** *.* ENT
*/

  char *ledmsg[6], *ledseq[6];

  int  i, j, okflag;
  int  kblayout;
  int  ckeymode;
  int  fkeymode;
  char kbdc;
  char *kbds = " ";
  char *curkeystr, *fnkeystr, *abmstr;
  char arptstring[500];

  static struct key {
      char c;
      int  row;
      int  col;
      char *symbol;
  } keytab [] = {
      { 27, 1,  0, "ESC" },
      { '1', 1,  6, "1" },    { '!', 1,  7, "!" },
      { '2', 1, 11, "2" },    { '@', 1, 12, "@" },
      { '3', 1, 16, "3" },    { '#', 1, 17, "#" },
      { '4', 1, 21, "4" },    { '$', 1, 22, "$" },
      { '5', 1, 26, "5" },    { '%', 1, 27, "%" },
      { '6', 1, 31, "6" },    { '^', 1, 32, "^" },
      { '7', 1, 36, "7" },    { '&', 1, 37, "&" },
      { '8', 1, 41, "8" },    { '*', 1, 42, "*" },
      { '9', 1, 46, "9" },    { '(', 1, 47, "(" },
      { '0', 1, 51, "0" },    { ')', 1, 52, ")" },
      { '-', 1, 56, "-" },    { '_', 1, 57, "_" },
      { '=', 1, 61, "=" },    { '+', 1, 62, "+" },
      { '`', 1, 66, "`" },    { '~', 1, 67, "~" },
      {   8, 1, 70, "BS" },
      {   9, 2,  0, " TAB " },
      { 'q', 2,  8, "q" },    { 'Q', 2,  9, "Q" },
      { 'w', 2, 13, "w" },    { 'W', 2, 14, "W" },
      { 'e', 2, 18, "e" },    { 'E', 2, 19, "E" },
      { 'r', 2, 23, "r" },    { 'R', 2, 24, "R" },
      { 't', 2, 28, "t" },    { 'T', 2, 29, "T" },
      { 'y', 2, 33, "y" },    { 'Y', 2, 34, "Y" },
      { 'u', 2, 38, "u" },    { 'U', 2, 39, "U" },
      { 'i', 2, 43, "i" },    { 'I', 2, 44, "I" },
      { 'o', 2, 48, "o" },    { 'O', 2, 49, "O" },
      { 'p', 2, 53, "p" },    { 'P', 2, 54, "P" },
      { '[', 2, 58, "[" },    { '{', 2, 59, "{" },
      { ']', 2, 63, "]" },    { '}', 2, 64, "}" },
      { 127, 2, 71, "DEL" },
      { 'a', 3, 10, "a" },    { 'A', 3, 11, "A" },
      { 's', 3, 15, "s" },    { 'S', 3, 16, "S" },
      { 'd', 3, 20, "d" },    { 'D', 3, 21, "D" },
      { 'f', 3, 25, "f" },    { 'F', 3, 26, "F" },
      { 'g', 3, 30, "g" },    { 'G', 3, 31, "G" },
      { 'h', 3, 35, "h" },    { 'H', 3, 36, "H" },
      { 'j', 3, 40, "j" },    { 'J', 3, 41, "J" },
      { 'k', 3, 45, "k" },    { 'K', 3, 46, "K" },
      { 'l', 3, 50, "l" },    { 'L', 3, 51, "L" },
      { ';', 3, 55, ";" },    { ':', 3, 56, ":" },
      {'\'', 3, 60, "'" },    { '"', 3, 61,"\"" },
      {  13, 3, 65, "RETN"},
      {'\\', 3, 71,"\\" },    { '|', 3, 72, "|" },
      { 'z', 4, 12, "z" },    { 'Z', 4, 13, "Z" },
      { 'x', 4, 17, "x" },    { 'X', 4, 18, "X" },
      { 'c', 4, 22, "c" },    { 'C', 4, 23, "C" },
      { 'v', 4, 27, "v" },    { 'V', 4, 28, "V" },
      { 'b', 4, 32, "b" },    { 'B', 4, 33, "B" },
      { 'n', 4, 37, "n" },    { 'N', 4, 38, "N" },
      { 'm', 4, 42, "m" },    { 'M', 4, 43, "M" },
      { ',', 4, 47, "," },    { '<', 4, 48, "<" },
      { '.', 4, 52, "." },    { '>', 4, 53, ">" },
      { '/', 4, 57, "/" },    { '?', 4, 58, "?" },
      {  10, 4, 69, "LF" },
      { ' ', 5, 13, "                SPACE BAR                "},
      {'\0', 0,  0, ""  }
    };

  static struct natkey {
      char natc;
      int  natrow;
      int  natcol;
      char *natsymbol;
  } natkeytab [][29] = {
      {
        { '"', 1, 12, "\""},
        { '&', 1, 32, "&" },
        { '/', 1, 37, "/" },
        { '(', 1, 42, "(" },
        { ')', 1, 47, ")" },
        { '=', 1, 52, "=" },
        { '+', 1, 56, "+" },    { '?', 1, 57, "?" },
        { '`', 1, 61, "`" },    { '@', 1, 62, "@" },
        { '<', 1, 66, "<" },    { '>', 1, 67, ">" },
        { '}', 2, 58, "}" },    { ']', 2, 59, "]" },
        { '^', 2, 63, "^" },    { '~', 2, 64, "~" },
        { '|', 3, 55, "|" },    {'\\', 3, 56,"\\" },
        { '{', 3, 60, "{" },    { '[', 3, 61, "[" },
        {'\'', 3, 71, "'" },    { '*', 3, 72, "*" },
        { ',', 4, 47, "," },    { ';', 4, 48, ";" },
        { '.', 4, 52, "." },    { ':', 4, 53, ":" },
        { '-', 4, 57, "-" },    { '_', 4, 58, "_" },
        {'\0', 0,  0, ""  }
      },
      {
        { '"', 1, 12, "\""},
        { '&', 1, 32, "&" },
        { '/', 1, 37, "/" },
        { '(', 1, 42, "(" },
        { ')', 1, 47, ")" },
        { '=', 1, 52, "=" },
        { '+', 1, 56, "+" },    { '?', 1, 57, "?" },
        { '`', 1, 61, "`" },    { '@', 1, 62, "@" },
        { '<', 1, 66, "<" },    { '>', 1, 67, ">" },
        { '}', 2, 58, "}" },    { ']', 2, 59, "]" },
        { '~', 2, 63, "~" },    { '^', 2, 64, "^" },
        { '|', 3, 55, "|" },    {'\\', 3, 56,"\\" },
        { '{', 3, 60, "{" },    { '[', 3, 61, "[" },
        {'\'', 3, 71, "'" },    { '*', 3, 72, "*" },
        { ',', 4, 47, "," },    { ';', 4, 48, ";" },
        { '.', 4, 52, "." },    { ':', 4, 53, ":" },
        { '-', 4, 57, "-" },    { '_', 4, 58, "_" },
        {'\0', 0,  0, ""  }
      }
  };

  static struct curkey {
      char *curkeymsg[3];
      int  curkeyrow;
      int  curkeycol;
      char *curkeysymbol;
      char *curkeyname;
  } curkeytab [] = {

      /* A Reset, A Set,  VT52  */

      {{"\033[A","\033OA","\033A"}, 0, 56, "UP",  "Up arrow"   },
      {{"\033[B","\033OB","\033B"}, 0, 61, "DN",  "Down arrow" },
      {{"\033[D","\033OD","\033D"}, 0, 66, "LT",  "Left arrow" },
      {{"\033[C","\033OC","\033C"}, 0, 71, "RT",  "Right arrow"},
      {{"",      "",       ""     }, 0,  0, "",    "" }
  };

  static struct fnkey {
      char *fnkeymsg[4];
      int  fnkeyrow;
      int  fnkeycol;
      char *fnkeysymbol;
      char *fnkeyname;
  } fnkeytab [] = {

      /* ANSI-num,ANSI-app,VT52-nu,VT52-ap,  r, c,  symb   name         */

      {{"\033OP","\033OP","\033P","\033P" }, 6, 59, "PF1", "PF1"        },
      {{"\033OQ","\033OQ","\033Q","\033Q" }, 6, 63, "PF2", "PF2"        },
      {{"\033OR","\033OR","\033R","\033R" }, 6, 67, "PF3", "PF3"        },
      {{"\033OS","\033OS","\033S","\033S" }, 6, 71, "PF4", "PF4"        },
      {{"7",     "\033Ow","7",    "\033?w"}, 7, 59, " 7 ", "Numeric 7"  },
      {{"8",     "\033Ox","8",    "\033?x"}, 7, 63, " 8 ", "Numeric 8"  },
      {{"9",     "\033Oy","9",    "\033?y"}, 7, 67, " 9 ", "Numeric 9"  },
      {{"-",     "\033Om","-",    "\033?m"}, 7, 71, " - ", "Minus"      },
      {{"4",     "\033Ot","4",    "\033?t"}, 8, 59, " 4 ", "Numeric 4"  },
      {{"5",     "\033Ou","5",    "\033?u"}, 8, 63, " 5 ", "Numeric 5"  },
      {{"6",     "\033Ov","6",    "\033?v"}, 8, 67, " 6 ", "Numeric 6"  },
      {{",",     "\033Ol",",",    "\033?l"}, 8, 71, " , ", "Comma"      },
      {{"1",     "\033Oq","1",    "\033?q"}, 9, 59, " 1 ", "Numeric 1"  },
      {{"2",     "\033Or","2",    "\033?r"}, 9, 63, " 2 ", "Numeric 2"  },
      {{"3",     "\033Os","3",    "\033?s"}, 9, 67, " 3 ", "Numeric 3"  },
      {{"0",     "\033Op","0",    "\033?p"},10, 59,"   O   ","Numeric 0"},
      {{".",     "\033On",".",    "\033?n"},10, 67, " . ", "Point"      },
      {{"\015",  "\033OM","\015", "\033?M"},10, 71, "ENT", "ENTER"      },
      {{"","","",""},       0,  0, "",    ""           }
    };

  static struct ckey {
      int  ccount;
      char *csymbol;
  } ckeytab [] = {
      { 0, "NUL (CTRL-@ or CTRL-Space)" },
      { 0, "SOH (CTRL-A)" },
      { 0, "STX (CTRL-B)" },
      { 0, "ETX (CTRL-C)" },
      { 0, "EOT (CTRL-D)" },
      { 0, "ENQ (CTRL-E)" },
      { 0, "ACK (CTRL-F)" },
      { 0, "BEL (CTRL-G)" },
      { 0, "BS  (CTRL-H) (BACK SPACE)" },
      { 0, "HT  (CTRL-I) (TAB)" },
      { 0, "LF  (CTRL-J) (LINE FEED)" },
      { 0, "VT  (CTRL-K)" },
      { 0, "FF  (CTRL-L)" },
      { 0, "CR  (CTRL-M) (RETURN)" },
      { 0, "SO  (CTRL-N)" },
      { 0, "SI  (CTRL-O)" },
      { 0, "DLE (CTRL-P)" },
      { 0, "DC1 (CTRL-Q) (X-On)" },
      { 0, "DC2 (CTRL-R)" },
      { 0, "DC3 (CTRL-S) (X-Off)" },
      { 0, "DC4 (CTRL-T)" },
      { 0, "NAK (CTRL-U)" },
      { 0, "SYN (CTRL-V)" },
      { 0, "ETB (CTRL-W)" },
      { 0, "CAN (CTRL-X)" },
      { 0, "EM  (CTRL-Y)" },
      { 0, "SUB (CTRL-Z)" },
      { 0, "ESC (CTRL-[) (ESCAPE)" },
      { 0, "FS  (CTRL-\\ or CTRL-? or CTRL-_)" },
      { 0, "GS  (CTRL-])" },
      { 0, "RS  (CTRL-^ or CTRL-~ or CTRL-`)" },
      { 0, "US  (CTRL-_ or CTRL-?)" }
  };

  static char *keyboardmenu[] = {
      "Standard American ASCII layout",
      "Swedish national layout D47",
      "Swedish national layout E47",
      /* add new keyboard layouts here */
      ""
    };

  static char *curkeymodes[3] = {
      "ANSI / Cursor key mode RESET",
      "ANSI / Cursor key mode SET",
      "VT52 Mode"
  };

  static char *fnkeymodes[4] = {
      "ANSI Numeric mode",
      "ANSI Application mode",
      "VT52 Numeric mode",
      "VT52 Application mode"
  };

  ledmsg[0] = "L1 L2 L3 L4"; ledseq[0] = "1;2;3;4";
  ledmsg[1] = "   L2 L3 L4"; ledseq[1] = "1;0;4;3;2";
  ledmsg[2] = "   L2 L3";    ledseq[2] = "1;4;;2;3";
  ledmsg[3] = "L1 L2";       ledseq[3] = ";;2;1";
  ledmsg[4] = "L1";          ledseq[4] = "1";
  ledmsg[5] = "";            ledseq[5] = "";

#ifdef UNIX
  fflush(stdout);
#endif
  ed(2);
  cup(10,1);
  println("These LEDs (\"lamps\") on the keyboard should be on:");
  for (i = 0; i <= 5; i++) {
    cup(10,52); el(0); printf("%s", ledmsg[i]);
    decll("0");
    decll(ledseq[i]);
    cup(12,1); holdit();
  }

  ed(2);
  cup(10,1);
  println("Test of the AUTO REPEAT feature");
  println("");
  println("Hold down an alphanumeric key for a while, then push RETURN.");
  printf("%s", "Auto Repeat OFF: ");
  rm("?8");
  inputline(arptstring);
  if (strlen(arptstring) == 0)      println("No characters read!??");
  else if (strlen(arptstring) == 1) println("OK.");
  else                              println("Too many characters read.");
  println("");
  println("Hold down an alphanumeric key for a while, then push RETURN.");
  printf("%s", "Auto Repeat ON: ");
  sm("?8");
  inputline(arptstring);
  if (strlen(arptstring) == 0)      println("No characters read!??");
  else if (strlen(arptstring) == 1) println("Not enough characters read.");
  else                              println("OK.");
  println("");
  holdit();

  ed(2);
  cup(5,10);
  println("Choose keyboard layout:");
  kblayout = menu(keyboardmenu);
  if (kblayout) {
    kblayout--;
    for (j = 0; natkeytab[kblayout][j].natc != '\0'; j++) {
      for (i = 0; keytab[i].c != '\0'; i++) {
	if (keytab[i].row == natkeytab[kblayout][j].natrow &&
	    keytab[i].col == natkeytab[kblayout][j].natcol) {
	  keytab[i].c = natkeytab[kblayout][j].natc;
	  keytab[i].symbol = natkeytab[kblayout][j].natsymbol;
	  break;
	}
      }
    }
  }

  ed(2);
  for (i = 0; keytab[i].c != '\0'; i++) {
    cup(1 + 2 * keytab[i].row, 1 + keytab[i].col);
    sgr("7");
    printf("%s", keytab[i].symbol);
    sgr("");
  }
  cup(22,1);
#ifdef UNIX
  sgttyNew.sg_flags &= ~CRMOD;
  sgttyNew.sg_flags &= ~ECHO;
  stty(0, &sgttyNew);
#endif
  inflush();
  printf("Press each key, both shifted and unshifted. Finish with RETURN:");
  do { /* while (kbdc != 13) */
    cup(23,1); kbdc = inchar();
    cup(23,1); el(0);
    sprintf(kbds, "%c", kbdc);
    chrprint(kbds);
    for (i = 0; keytab[i].c != '\0'; i++) {
      if (keytab[i].c == kbdc) {
        cup(1 + 2 * keytab[i].row, 1 + keytab[i].col);
	printf("%s", keytab[i].symbol);
	break;
      }
    }
  } while (kbdc != 13);
#ifdef SARG10
  inchar();  /* Local hack: Read LF that TOPS-10 adds to CR */
#endif
  cup(23,1); el(0);

  for (ckeymode = 0; ckeymode <= 2; ckeymode++) {
    if (ckeymode) sm("?1");
    else            rm("?1");
    for (i = 0; curkeytab[i].curkeysymbol[0] != '\0'; i++) {
      cup(1 + 2 * curkeytab[i].curkeyrow, 1 + curkeytab[i].curkeycol);
      sgr("7");
      printf("%s", curkeytab[i].curkeysymbol);
      sgr("");
    }
    cup(20,1); printf("<%s>%20s", curkeymodes[ckeymode], "");
    cup(22,1); el(0);
    cup(22,1); printf("%s", "Press each cursor key. Finish with TAB.");
    for(;;) {
      cup(23,1);
      if (ckeymode == 2) rm("?2"); /* VT52 mode */
      curkeystr = instr();
      esc("<");                      /* ANSI mode */
      cup(23,1); el(0);
      cup(23,1); chrprint(curkeystr);
      if (!strcmp(curkeystr,"\t")) break;
      for (i = 0; curkeytab[i].curkeysymbol[0] != '\0'; i++) {
	if (!strcmp(curkeystr,curkeytab[i].curkeymsg[ckeymode])) {
	  sgr("7");
	  printf(" (%s key) ", curkeytab[i].curkeyname);
	  sgr("");
	  cup(1 + 2 * curkeytab[i].curkeyrow,
	      1 + curkeytab[i].curkeycol);
	  printf("%s", curkeytab[i].curkeysymbol);
	  break;
	}
      }
      if (i == sizeof(curkeytab) / sizeof(struct curkey) - 1) {
	sgr("7");
	printf("%s", " (Unknown cursor key) ");
	sgr("");
      }
    }
  }

  for (fkeymode = 0; fkeymode <= 3; fkeymode++) {
    for (i = 0; fnkeytab[i].fnkeysymbol[0] != '\0'; i++) {
      cup(1 + 2 * fnkeytab[i].fnkeyrow, 1 + fnkeytab[i].fnkeycol);
      sgr("7");
      printf("%s", fnkeytab[i].fnkeysymbol);
      sgr("");
    }
    cup(20,1); printf("<%s>%20s", fnkeymodes[fkeymode], "");
    cup(22,1); el(0);
    cup(22,1); printf("%s", "Press each function key. Finish with TAB.");
    for(;;) {
      cup(23,1);
      if (fkeymode >= 2)  rm("?2");    /* VT52 mode */
      if (fkeymode % 2)   deckpam();   /* Application mode */
      else                 deckpnm();	/* Numeric mode     */
      fnkeystr = instr();
      esc("<");				/* ANSI mode */
      cup(23,1); el(0);
      cup(23,1); chrprint(fnkeystr);
      if (!strcmp(fnkeystr,"\t")) break;
      for (i = 0; fnkeytab[i].fnkeysymbol[0] != '\0'; i++) {
	if (!strcmp(fnkeystr,fnkeytab[i].fnkeymsg[fkeymode])) {
	  sgr("7");
	  printf(" (%s key) ", fnkeytab[i].fnkeyname);
	  sgr("");
	  cup(1 + 2 * fnkeytab[i].fnkeyrow, 1 + fnkeytab[i].fnkeycol);
	  printf("%s", fnkeytab[i].fnkeysymbol);
	  break;
	}
      }
      if (i == sizeof(fnkeytab) / sizeof(struct fnkey) - 1) {
	sgr("7");
	printf("%s", " (Unknown function key) ");
	sgr("");
      }
    }
  }

#ifdef UNIX
  sgttyNew.sg_flags |= CRMOD;
  stty(0, &sgttyNew);
#endif
  ed(2);
  cup(5,1);
  println("Finally, a check of the ANSWERBACK MESSAGE, which can be sent");
  println("by pressing CTRL-BREAK. The answerback message can be loaded");
  println("in SET-UP B by pressing SHIFT-A and typing e.g.");
  println("");
  println("         \" H e l l o , w o r l d Return \"");
  println("");
  println("(the double-quote characters included).  Do that, and then try");
  println("to send an answerback message with CTRL-BREAK.  If it works,");
  println("the answerback message should be displayed in reverse mode.");
  println("Finish with a single RETURN.");

#ifdef UNIX
  sgttyNew.sg_flags &= ~CRMOD;
  stty(0, &sgttyNew);
#endif
  do {
    cup(17,1);
    inflush();
    abmstr = instr();
    cup(17,1);
    el(0);
    chrprint(abmstr);
  } while (strcmp(abmstr,"\r"));

  ed(2);
  for (i = 0; i < 32; i++) {
    cup(1 + (i % 16), 1 + 40 * (i / 16));
    sgr("7");
    printf("%s", ckeytab[i].csymbol);
    sgr("0");
  }
  cup(19,1);
#ifdef UNIX
  sgttyNew.sg_flags |= CRMOD;
  stty(0, &sgttyNew);
#endif
  println(
  "Push each CTRL-key TWICE. Note that you should be able to send *all*");
  println(
  "CTRL-codes twice, including CTRL-S (X-Off) and CTRL-Q (X-Off)!");
  println(
  "Finish with DEL (also called DELETE or RUB OUT), or wait 1 minute.");
#ifdef UNIX
#ifdef SIII
  sgttyNew.sg_flags &= ~CBREAK;
  stty(0, &sgttyNew);
#endif
  sgttyNew.sg_flags |= RAW;
  stty(0, &sgttyNew);
#endif
  ttybin(1);
#ifdef SARG20
  page(0);	/* Turn off all character processing at input */
  superbin(1);	/* Turn off ^C (among others). Keep your fingers crossed!! */
#endif
  do {
    cup(23,1); kbdc = inchar();
    cup(23,1); el(0);
    if (kbdc < 32) printf("  %s", ckeytab[kbdc].csymbol);
    else {
      sprintf(kbds, "%c", kbdc);
      chrprint(kbds);
      printf("%s", " -- not a CTRL key");
    }
    if (kbdc < 32) ckeytab[kbdc].ccount++;
    if (ckeytab[kbdc].ccount == 2) {
      cup(1 + (kbdc % 16), 1 + 40 * (kbdc / 16));
      printf("%s", ckeytab[kbdc].csymbol);
    }
  } while (kbdc != '\177');
#ifdef UNIX
  sgttyNew.sg_flags &= ~RAW;
  sgttyNew.sg_flags |= ECHO;
  stty(0, &sgttyNew);
#ifdef SIII
  sgttyNew.sg_flags |= CBREAK;
  stty(0, &sgttyNew);
#endif
#endif
  ttybin(0);
#ifdef SARG20
  superbin(0);	/* Puuuh! We made it!? */
  page(1);	/* Back to normal input processing */
  ttybin(1);	/* This must be the mode for DEC20 */
#endif
  cup(24,1);
  okflag = 1;
  for (i = 0; i < 32; i++) if (ckeytab[i].ccount < 2) okflag = 0;
  if (okflag) printf("%s", "OK. ");
  else        printf("%s", "You have not been able to send all CTRL keys! ");
  holdit();
}

tst_reports() {
  /* Test of:
       <ENQ>       (AnswerBack Message)
       SM RM       (Set/Reset Mode) - LineFeed / Newline
       DSR         (Device Status Report)
       DA          (Device Attributes)
       DECREQTPARM (Request Terminal Parameters)
  */

  int parity, nbits, xspeed, rspeed, clkmul, flags;
  int i, reportpos;
  char *report, *report2;
  static char *attributes[][2] = {
    { "\033[?1;0c",   "No options (vanilla VT100)" },
    { "\033[?1;1c",   "VT100 with STP" },
    { "\033[?1;2c",   "VT100 with AVO (could be a VT102)" },
    { "\033[?1;3c",   "VT100 with STP and AVO" },
    { "\033[?1;4c",   "VT100 with GPO" },
    { "\033[?1;5c",   "VT100 with STP and GPO" },
    { "\033[?1;6c",   "VT100 with AVO and GPO" },
    { "\033[?1;7c",   "VT100 with STP, AVO and GPO" },
    { "\033[?1;11c",  "VT100 with PP and AVO" },
    { "\033[?1;15c",  "VT100 with PP, GPO and AVO" },
    { "\033[?4;2c",   "VT132 with AVO" },
    { "\033[?4;3c",   "VT132 with AVO and STP" },
    { "\033[?4;6c",   "VT132 with GPO and AVO" },
    { "\033[?4;7c",   "VT132 with GPO, AVO, and STP" },
    { "\033[?4;11c",  "VT132 with PP and AVO" },
    { "\033[?4;15c",  "VT132 with PP, GPO and AVO" },
    { "\033[?7c",     "VT131" },
    { "\033[?12;5c",  "VT125" },           /* VT125 also has ROM version */
    { "\033[?12;7c",  "VT125 with AVO" },  /* number, so this won't work */
    { "\033[?5;0c",   "VK100 (GIGI)" },
    { "\033[?5c",     "VK100 (GIGI)" },
    { "", "" }
  };

#ifdef UNIX
  sgttyNew.sg_flags &= ~ECHO;
  stty(0, &sgttyNew);
#endif
  cup(5,1);
  println("This is a test of the ANSWERBACK MESSAGE. (To load the A.B.M.");
  println("see the TEST KEYBOARD part of this program). Below here, the");
  println("current answerback message in your terminal should be");
  println("displayed. Finish this test with RETURN.");
  cup(10,1);
  inflush();
  printf("%c", 5); /* ENQ */
  report = instr();
  cup(10,1);
  chrprint(report);
  cup(12,1);
  holdit();

  ed(2);
  cup(1,1);
  println("Test of LineFeed/NewLine mode.");
  cup(3,1);
  sm("20");
#ifdef UNIX
  sgttyNew.sg_flags &= ~CRMOD;
  stty(0, &sgttyNew);
#endif
  printf("NewLine mode set. Push the RETURN key: ");
  report = instr();
  cup(4,1);
  el(0);
  chrprint(report);
  if (!strcmp(report, "\015\012")) printf(" -- OK");
  else                             printf(" -- Not expected");
  cup(6,1);
  rm("20");
  printf("NewLine mode reset. Push the RETURN key: ");
  report = instr();
  cup(7,1);
  el(0);
  chrprint(report);
  if (!strcmp(report, "\015")) printf(" -- OK");
  else                         printf(" -- Not expected");
  cup(9,1);
#ifdef UNIX
  sgttyNew.sg_flags |= CRMOD;
  stty(0, &sgttyNew);
#endif
  holdit();

  ed(2);
  cup(1,1);
  printf("Test of Device Status Report 5 (report terminal status).");
  cup(2,1);
  dsr(5);
  report = instr();
  cup(2,1);
  el(0);
  printf("Report is: ");
  chrprint(report);
  if      (!strcmp(report,"\033[0n")) printf(" -- means \"TERMINAL OK\"");
  else if (!strcmp(report,"\033[3n")) printf(" -- means \"TERMINAL OK\"");
  else                                printf(" -- Unknown response!");

  cup(4,1);
  println("Test of Device Status Report 6 (report cursor position).");
  cup(5,1);
  dsr(6);
  report = instr();
  cup(5,1);
  el(0);
  printf("Report is: ");
  chrprint(report);
  if (!strcmp(report,"\033[5;1R")) printf(" -- OK");
  else                             printf(" -- Unknown response!");

  cup(7,1);
  println("Test of Device Attributes report (what are you)");
  cup(8,1);
  da(0);
  report = instr();
  cup(8,1);
  el(0);
  printf("Report is: ");
  chrprint(report);
  for (i = 0; *attributes[i][0] != '\0'; i++) {
    if (!strcmp(report,attributes[i][0])) break;
  }
  if (*attributes[i][0] == '\0')
  printf(" -- Unknown response, refer to the manual");
  else {
    printf(" -- means %s", attributes[i][1]);
    if (i) {
      cup(9,1);
      println("Legend: STP = Processor Option");
      println("        AVO = Advanced Video Option");
      println("        GPO = Graphics Processor Option");
      println("        PP  = Printer Port");
    }
  }

  cup(14,1);
  println("Test of the \"Request Terminal Parameters\" feature, argument 0.");
  cup(15,1);
  decreqtparm(0);
  report = instr();
  cup(15,1);
  el(0);
  printf("Report is: ");
  chrprint(report);
  if (strlen(report) < 16
   || report[0] != '\033'
   || report[1] != '['
   || report[2] != '2'
   || report[3] != ';')
  println(" -- Bad format");
  else {
    reportpos = 4;
    parity = scanto(report, &reportpos, ';');
    nbits  = scanto(report, &reportpos, ';');
    xspeed = scanto(report, &reportpos, ';');
    rspeed = scanto(report, &reportpos, ';');
    clkmul = scanto(report, &reportpos, ';');
    flags  = scanto(report, &reportpos, 'x');
    if (parity == 0 || nbits == 0 || clkmul == 0) println(" -- Bad format");
    else                                          println(" -- OK");
    printf(
    "This means: Parity %s, %s bits, xmitspeed %s, recvspeed %s.\n",
    lookup(paritytable, parity),
    lookup(nbitstable, nbits),
    lookup(speedtable, xspeed),
    lookup(speedtable, rspeed));
    printf("(CLoCk MULtiplier = %d, STP option flags = %d)\n", clkmul, flags);
  }

  cup(19,1);
  println("Test of the \"Request Terminal Parameters\" feature, argument 1.");
  cup(20,1);
  decreqtparm(1);	/* Does the same as decreqtparm(0), reports "3" */
  report2 = instr();
  cup(20,1);
  el(0);
  printf("Report is: ");
  chrprint(report2);
  if (strlen(report2) < 3
   || report2[2] != '3')
  println(" -- Bad format");
  else {
    report2[2] = '2';
    if (!strcmp(report,report2)) println(" -- OK");
    else                         println(" -- Bad format");
  }
  cup(24,1);
  holdit();
#ifdef UNIX
  sgttyNew.sg_flags |= ECHO;
  stty(0, &sgttyNew);
#endif
}

tst_vt52() {

  static struct rtabl {
      char *rcode;
      char *rmsg;
  } resptable[] = {
      { "\033/K", " -- OK (means Standard VT52)" },
      { "\033/Z", " -- OK (means VT100 emulating VT52)" },
      { "",       " -- Unknown response"}
  };

  int i,j;
  char *response;

  rm("?2");  /* Reset ANSI (VT100) mode, Set VT52 mode	*/
  esc("H");  /* Cursor home	*/
  esc("J");  /* Erase to end of screen	*/
  esc("H");  /* Cursor home	*/
  for (i = 0; i <= 23; i++) {
    for (j = 0; j <= 9; j++)
    printf("%s", "FooBar ");
    println("Bletch");
  }
  esc("H");  /* Cursor home	*/
  esc("J");  /* Erase to end of screen	*/

  vt52cup(7,47);
  printf("nothing more.");
  for (i = 1; i <= 10; i++) printf("THIS SHOULD GO AWAY! ");
  for (i = 1; i <= 5; i++) {
    vt52cup(1,1);
    printf("%s", "Back scroll (this should go away)");
    esc("I"); 		/* Reverse LineFeed (with backscroll!)	*/
  }
  vt52cup(12,60);
  esc("J");  /* Erase to end of screen	*/
  for (i = 2; i <= 6; i++) {
    vt52cup(i,1);
    esc("K");		/* Erase to end of line	*/
  }

  for (i = 2; i <= 23; i++) {
    vt52cup(i,70); printf("%s", "**Foobar");
  }
  vt52cup(23,10);
  for (i = 23; i >= 2; i--) {
    printf("%s", "*");
    printf("%c", 8);	/* BS */
    esc("I");		/* Reverse LineFeed (LineStarve)	*/
  }
  vt52cup(1,70);
  for (i = 70; i >= 10; i--) {
    printf("%s", "*");
    esc("D"); esc("D");	/* Cursor Left */
  }
  vt52cup(24,10);
  for (i = 10; i <= 70; i++) {
    printf("%s", "*");
    printf("%c", 8);	/* BS */
    esc("C");		/* Cursor Right	*/
  }
  vt52cup(2,11);
  for (i = 2; i <= 23; i++) {
    printf("%s", "!");
    printf("%c", 8);	/* BS */
    esc("B");		/* Cursor Down	*/
  }
  vt52cup(23,69);
  for (i = 23; i >= 2; i--) {
    printf("%s", "!");
    printf("%c", 8);	/* BS */
    esc("A");		/* Cursor Up	*/
  }
  for (i = 2; i <= 23; i++) {
    vt52cup(i,71);
    esc("K");		/* Erase to end of line	*/
  }

  vt52cup(10,16);
  printf("%s", "The screen should be cleared, and have a centered");
  vt52cup(11,16);
  printf("%s", "rectangle of \"*\"s with \"!\"s on the inside to the");
  vt52cup(12,16);
  printf("%s", "left and right. Only this, and");
  vt52cup(13,16);
  holdit();

  esc("H");  /* Cursor home	*/
  esc("J");  /* Erase to end of screen	*/
  printf("%s", "This is the normal character set:");
  for (j =  0; j <=  1; j++) {
    vt52cup(3 + j, 16);
    for (i = 0; i <= 47; i++)
    printf("%c", 32 + i + 48 * j);
  }
  vt52cup(6,1);
  printf("%s", "This is the special graphics character set:");
  esc("F");	/* Select Special Graphics character set	*/
  for (j =  0; j <=  1; j++) {
    vt52cup(8 + j, 16);
    for (i = 0; i <= 47; i++)
    printf("%c", 32 + i + 48 * j);
  }
  esc("G");	/* Select ASCII character set	*/
  vt52cup(12,1);
  holdit();

  esc("H");  /* Cursor home	*/
  esc("J");  /* Erase to end of screen	*/
  println("Test of terminal response to IDENTIFY command");
  esc("Z");	/* Identify	*/
  response = instr();
  println("");
  printf("Response was");
  esc("<");  /* Enter ANSI mode (VT100 mode) */
  chrprint(response);
  for(i = 0; resptable[i].rcode[0] != '\0'; i++)
    if (!strcmp(response, resptable[i].rcode))
      break;
  printf("%s", resptable[i].rmsg);
  println("");
  println("");
  holdit();
}

tst_insdel() {

    /* Test of:
       SM/RM(4) (= IRM (Insertion/replacement mode))
       ICH (Insert Character)
       DCH (Delete character)
       IL  (Insert line)
       DL  (Delete line)
    */

  int i, row, col, sw, dblchr, scr132;

  for(scr132 = 0; scr132 <= 1; scr132++) {
    if (scr132) { sm("?3"); sw = 132; }
    else        { rm("?3"); sw =  80; }
    ed(2);
    cup(1,1);
    for (row=1; row<=24; row++) {
	cup(row,1);
	for (col=1; col<=sw; col++)
	    printf("%c", 'A'-1+row);
    }
    cup(4,1);
    printf("Screen accordion test (Insert & Delete Line). "); holdit();
    ri(); el(2);
    decstbm( 2,23);
    sm("?6");
    cup(1,1);
    for (row=1; row<=24; row++) {
      il(row);
      dl(row);
    }
    rm("?6");
    decstbm( 0, 0);
    cup(2,1);
    printf(
    "Top line: A's, bottom line: X's, this line, nothing more. ");
    holdit();
    cup(2,1); ed(0);
    cup(1,2);
    printf("B");
    cub(1);
    sm("4");
    for (col=2; col<=sw-1; col++)
      printf("*");
    rm("4");
    cup(4,1);
    printf("Test of 'Insert Mode'. The top line should be 'A*** ... ***B'. ");
    holdit(); ri(); el(2);
    cup(1,2);
    dch(sw-2);
    cup(4,1);
    printf("Test of 'Delete Character'. The top line should be 'AB'. ");
    holdit();

    for(dblchr = 1; dblchr <= 2; dblchr++) {
      ed(2);
      for (row=1; row<=24; row++) {
	cup(row,1);
	if (dblchr == 2) decdwl();
	for (col=1; col<=sw/dblchr; col++)
	  printf("%c", 'A'-1+row);
	cup(row,sw/dblchr-row);
	dch(row);
      }
      cup(4,1);
      println("The right column should be staggered ");
      printf("by one.  ");
      holdit();
    }
    ed(2);
    cup(1,1);
    println("If your terminal has the ANSI 'Insert Character' function");
    println("(the VT102 does not), then you should see a line like this");
    println("  A B C D E F G H I J K L M N O P Q R S T U V W X Y Z");
    println("below:");
    println("");
    for (i = 'Z'; i >= 'A'; i--) {
      printf("%c\010",i);
      ich(2);
    }
    cup(10,1);
    holdit();

    if (sw == 132) rm("?3");
  }
}

dch(pn) int pn; { brc(pn, 'P'); }  /* Delete character */
ich(pn) int pn; { brc(pn, '@'); }  /* Insert character -- not in VT102 */
dl(pn)  int pn; { brc(pn, 'M'); }  /* Delete line */
il(pn)  int pn; { brc(pn, 'L'); }  /* Insert line */

/*  Test of some known VT100 bugs and misfeatures  */

tst_bugs() {

  int i, menuchoice;

  static char *menutable[] = {
    "Exit to main menu",
    "Bug A: Smooth scroll to jump scroll",
    "Bug B: Scrolling region",
    "Bug C: Wide to narrow screen",
    "Bug D: Narrow to wide screen",
    "Bug E: Cursor move from double- to single-wide line",
    "Bug F: Column mode escape sequence",
    "Wrap around with cursor addressing",
    "Erase right half of double width lines",
    "Funny scroll regions",
    /* Add more here */
    ""
  };

  static char *hmsg[] = {
  "Test of known bugs in the DEC VT100 series. The numbering of some of",
  "the bugs (A-F) refers to the article 'VT100 MAGIC' by Sami Tabih in",
  "the 'Proceedings of the DEC Users Society' at St. Louis, Missouri, May",
  "1983. To understand some of the tests, you have to look at the source",
  "code or the article. Of course, a good VT100-compatible terminal",
  "should not have these bugs (or have some means of disabling them)! If",
  "a bug appears, you might want to RESET the terminal before continuing",
  "the test. There is a test of the RESET function in the main menu.",
  "" };

  do {
    ed(2); cup(1,1);
    for (i = 0; *hmsg[i]; i++) println(hmsg[i]);
    println("");
    println("          Choose bug test number:");
    menuchoice = menu(menutable);
    switch (menuchoice) {
      case  1:  bug_a();  break;
      case  2:  bug_b();  break;
      case  3:  bug_c();  break;
      case  4:  bug_d();  break;
      case  5:  bug_e();  break;
      case  6:  bug_f();  break;
      case  7:  bug_w();  break;
      case  8:  bug_l();  break;
      case  9:  bug_s();  break;
    }
  } while (menuchoice);
}

/* Bug A: Smooth scroll to jump scroll */

bug_a() {
  int i;

  cup (10, 1);
  println("This is a test of the VT100 'Scroll while toggle softscroll'");
  println("bug.  The cursor may disappear, or move UP the screen, or");
  println("multiple copies of some lines may appear.");
  holdit();

  /*  Invoke the bug  */

  esc ("[24H");				/* Simplified cursor movement	*/
  rm("?4"); for (i = 1; i <= 20; i++) printf("\n");
  sm("?4"); for (i = 1; i <= 10; i++) printf("\n");
  rm("?4"); for (i = 1; i <=  5; i++) printf("\n");

  /* That should be enough to show the bug. But we'll try another way:	*/
  sm ("?4");				/* Set soft scroll		*/
  nel ();				/* "NextLine", move down	*/
  rm ("?4");				/* Reset soft scroll		*/
  nel ();				/* "NextLine", move down	*/
  for (i = 1; i <= 10; i++) {		/* Show the bug			*/
      printf ("Softscroll bug test, line %d.  ", i);
      holdit();
  }
  println("That should have been enough to show the bug, if present.");
  holdit();
}

/*  Bug B: Scrolling region  */

bug_b() {
  char c;

  decaln();
  cup( 1,1); el(0);
  printf("Line 11 should be double-wide, line 12 should be cleared.");
  cup( 2,1); el(0);
  printf("Then, the letters A-P should be written at the beginning");
  cup( 3,1); el(0);
  printf("of lines 12-24, and the empty line and A-E are scrolled away.");
  cup( 4,1); el(0);
  printf("If the bug is present, some lines are confused, look at K-P.");
  cup(11,1); decdwl();
  decstbm(12,24);
  cup(12,1); el(0); printf("Here we go... "); holdit();
  cup(12,1); ri();					/* Bug comes here */
  for (c = 'A'; c <= 'P'; c++) printf("%c\n",c);	/* Bug shows here */
  holdit();
  decstbm(0,0);						/* No scr. region */
}

/*  Bug C: Wide to narrow screen  */

bug_c() {
  sm("?3");						/* 132 column mode */
  cup(1,81);
  rm("?3");						/*  80 column mode */
  cup(12,5);
  printf("Except for this line, the screen should be blank. ");
  holdit();
}

/*  Bug D: Narrow to wide screen  */

bug_d() {
  int i;
  char result;
  /* Make the bug appear */
  do {
    cup(14,1);

    /* The original code in the article says
     * PRINT ESC$; "[13;1H"; CHR$(10%);
     * but I guess a cup(14,1); would do.
     * (To output a pure LF might be tricky).
     */

    sm("?3");		      /* Make the bug visible */
    cup(1,9); decdwl();
    println("You should see blinking text at the bottom line.");
    cup(3,9); decdwl();
    println("Enter 0 to exit, 1 to try to invoke the bug again.");
    cup(24,9); decdwl(); sgr("1;5;7");
    printf("If you can see this then the bug did not appear."); sgr("");
    cup(4,9); decdwl();
    result = inchar(); readnl();
    rm("?3");
  } while (result == '1');
  sm("?4");	/* Syrup scroll */
  cup(23,1);
  for (i = 1; i <= 5; i++)
  println("If the bug is present, this should make things much worse!");
  holdit();
  rm("?4");	/* Jump scroll */
}

/*  Bug E: Cursor move from double- to single-wide line  */

bug_e() {
  int i;
  static char *rend[2] = { "\033[m", "\033[7m" };
  sm("?3");
  cup(1,1); decdwl();
  println("This test should put an 'X' at line 3 column 100.");
  for (i = 1; i <= 12; i++) printf("1234567890%s",rend[i & 1]);
  cup(1,1);	/* The bug appears when we jump from a dobule-wide line */
  cup(3,100);	/* to a single-wide line, column > 66.                  */
  printf("X");
  cup(4, 66); printf("!                                 !");
  cup(5,1);
  printf("--------------------------- The 'X' should NOT be above here -");
  printf("---+------------ but above here -----+");
  cup(10,1); decdwl(); holdit();
  rm("?3");
}

/*  Bug F: Column mode escape sequence  */

bug_f() {
  int i, row, col;

 /*
  *  VT100 "toggle origin mode, forget rest" bug.  If you try to set
  *	(or clear) parameters and one of them is the "origin mode"
  *	("?6") parameter, parameters that appear after the "?6"
  *	remain unaffected.  This is also true on CIT-101 terminals.
  */
  sm ("?5");				/* Set reverse mode		*/
  sm ("?3");				/* Set 132 column mode		*/
  println("Test VT100 'Toggle origin mode, forget rest' bug, part 1.");
  println("The screen should be in reverse, 132 column mode.");
  holdit();
  ed (2);
  rm ("?6;5;3");		/* Reset (origin, reverse, 132 col)	*/
  println("Test VT100 'Toggle origin mode, forget rest' bug, part 2.\n");
  println("The screen should be in non-reverse, 80 column mode.");
  holdit();
}

  /*	Bug W:
   *	The dreaded "wraparound" bug!  You CUP to col 80, write a char,
   *	CUP to another line in col 80, write a char. And the brain-damaged
   *	terminal thinks that "Hokay, so he's written a char in col 80, so
   *	I stay in col 80 and wait for next character. Let's see now, here
   *	comes another character, and I'm still in col 80, so I must make
   *	a NewLine first." -- It doesn't clear that "still in col 80" flag
   *	on a CUP. Argh!
   */

bug_w() {
  int row, col;

  cup (16,1);
  println("   This illustrates the \"wrap around bug\" which exists on a");
  println("   standard VT100. At the top of the screen there should be");
  println("   a row of +'s, and the rightmost column should be filled");
  println("   with *'s. But if the bug is present, some of the *'s may");
  println("   be placed in other places, e.g. in the leftmost column,");
  println("   and the top line of +'s may be scrolled away.");

  cup(1,1);
  for (col = 1; col <= 79; col++)
      printf ("+");
  for (row = 1; row <= 24; row++) {
      hvp (row, 80);
      printf ("*");
  }
  cup(24,1);
  holdit();
}

  /*	Bug L:
   *	Check if the right half of double-width lines comes back
   *	when a line is first set to single-width, filled with stuff,
   *	set to double-width, and finally reset to single-width.
   *
   *	A VT100 has this misfeature, and many others. Foo!
   */

bug_l() {
  cup(15, 1);
  printf("This-is-a-long-line-This-is-a-long-line-");
  printf("This-is-a-long-line-This-is-a-long-line-");
  cup(1, 1);
  printf("This is a test of what happens to the right half of double-width");
  println(" lines.");
  printf("A common misfeature is that the right half does not come back");
  println(" when a long");
  printf("single-width line is set to double-width and then reset to");
  println(" single-width.");

  cup(5, 1);
  println("Now the line below should contain 80 characters in single width.");
  holdit();
  cup(15, 1); decdwl();
  cup(8, 1);
  println("Now the line below should contain 40 characters in double width.");
  holdit();
  cup(15, 1); decswl();
  cup(11, 1);
  println("Now the line below should contain 80 characters in single width.");
  holdit();

  /* ...and in 132 column mode  */

  sm("?3");
  ed(2);
  cup(15, 1);
  printf("This-is-a-long-line-This-is-a-long-line-");
  printf("This-is-a-long-line-This-is-a-long-line-");
  printf("This-is-a-long-line-This-is-a-long-line-");
  printf("ending-here-");

  cup(1, 1);
  printf("This is the same test in 132 column mode.");

  cup(5, 1);
  println("Now the line below should contain 132 characters in single width.");
  holdit();
  cup(15, 1); decdwl();
  cup(8, 1);
  println("Now the line below should contain 66 characters in double width.");
  holdit();
  cup(15, 1); decswl();
  cup(11, 1);
  println("Now the line below should contain 132 characters in single width.");
  holdit();
  rm("?3");
}

bug_s() {
  int i;
  decstbm(20,10);	/* 20-10=-10, < 2, so no scroll region. */
  cup(1,1);
  for (i=1; i<=20; i++)
    printf("This is 20 lines of text (line %d), no scroll region.\n", i);
  holdit();
  ed(2);
  decstbm(0,1);		/* Should be interpreted as decstbm(1,1) = none */
  cup(1,1);
  for (i=1; i<=20; i++)
    printf("This is 20 lines of text (line %d), no scroll region.\n", i);
  holdit();
  decstbm(0,0);		/* No scroll region (just in case...)	*/
}

tst_rst() {

  /*
   * Test of
   *	- RIS    (Reset to Initial State)
   *	- DECTST (invoke terminal test)
   */

  cup(10,1);
  printf ("The terminal will now be RESET. ");
  holdit();
  ris();
#ifdef UNIX
  fflush(stdout);
#endif
  zleep(5000);		/* Wait 5.0 seconds */
  cup(10,1);
  println("The terminal is now RESET. Next, the built-in confidence test");
  printf("%s", "will be invoked. ");
  holdit();
  ed(2);
  dectst(1);
#ifdef UNIX
  fflush(stdout);
#endif
  zleep(5000);		/* Wait 5.0 seconds */
  cup(10,1);
  println("If the built-in confidence test found any errors, a code");
  printf("%s", "is visible above. ");
  holdit();
}

initterminal(pn) int pn; {

#ifdef UNIX
  if (pn==0) {
    fflush(stdout);
    gtty(0,&sgttyOrg);
    gtty(0,&sgttyNew);
    sgttyNew.sg_flags |= CBREAK;
    }
  else  {
    fflush(stdout);
    inflush();
    sleep(2);
    sgttyNew.sg_flags = sgttyOrg.sg_flags | CBREAK;
    }
  stty(0,&sgttyNew);
#ifdef SIII
  close(2);
  open(_PATH_TTY,O_RDWR|O_NDELAY);
#endif
#endif
#ifdef SARG10
  /* Set up neccesary TOPS-10 terminal parameters	*/

  trmop(02041, `VT100`);	/* tty type vt100	*/
  trmop(02002, 0);	/* tty no tape	*/
  trmop(02003, 0);	/* tty lc	*/
  trmop(02005, 1);	/* tty tab	*/
  trmop(02010, 1);	/* tty no crlf	*/
  trmop(02020, 0);	/* tty no tape	*/
  trmop(02021, 1);	/* tty page	*/
  trmop(02025, 0);	/* tty blanks	*/
  trmop(02026, 1);	/* tty no alt	*/
  trmop(02040, 1);	/* tty defer	*/
#endif
#ifdef SARG20
  ttybin(1);	/* set line to binary mode */
#endif
  /* Set up my personal prejudices	*/

  esc("<");	/* Enter ANSI mode (if in VT52 mode)	*/
  rm("?1");     /* cursor keys normal   */
  rm("?3");	/* 80 col mode		*/
  rm("?4");     /* Jump scroll          */
  rm("?5");     /* Normal screen        */
  rm("?6");	/* Absolute origin mode	*/
  sm("?7");	/* Wrap around on	*/
  rm("?8");	/* Auto repeat off	*/
  decstbm(0,0);	/* No scroll region	*/
  sgr("0");     /* Normal character attributes  */

}

bye () {
  /* Force my personal prejudices upon the poor luser	*/

  esc("<");	/* Enter ANSI mode (if in VT52 mode)	*/
  rm("?1");     /* cursor keys normal   */
  rm("?3");	/* 80 col mode		*/
  rm("?5");     /* Normal screen        */
  rm("?6");	/* Absolute origin mode	*/
  sm("?7");	/* Wrap around on	*/
  sm("?8");	/* Auto repeat on	*/
  decstbm(0,0);	/* No scroll region	*/
  sgr("0");     /* Normal character attributes  */

  /* Say goodbye */

  ed(2);
  cup(12,30);
  printf("That's all, folks!\n");
  printf("\n\n\n");
  inflush();
#ifdef SARG20
  ttybin(0);	/* reset line to normal mode */
#endif
#ifdef UNIX
  stty(0,&sgttyOrg);
#endif
  exit();
}

#ifdef UNIX
onbrk() {
  signal(SIGINT, onbrk);
  if (reading)
    brkrd = 1;
  else
    longjmp(intrenv, 1);
}

onterm() {
  signal(SIGTERM, onterm);
  longjmp(intrenv, 1);
}
#endif

holdit() {
  inflush();
  printf("Push <RETURN>");
  readnl();
}

readnl() {
#ifdef UNIX
  char ch;
  fflush(stdout);
  brkrd = 0;
  reading = 1;
  do { read(0,&ch,1); } while(ch != '\n' && !brkrd);
  if (brkrd)
    kill(getpid(), SIGTERM);
  reading = 0;
#endif
#ifdef SARG10
 while (getchar() != '\n')
 ;
#endif
#ifdef SARG20
 while (getchar() != '\n')
   ;
#endif
}

scanto(str, pos, toc) char *str; int *pos; char toc; {
  char c;
  int result = 0;

  while (toc != (c = str[(*pos)++])) {
    if (isdigit(c)) result = result * 10 + c - '0';
    else break;
  }
  if (c == toc) return(result);
  else          return(0);
}

char *lookup(t, k) struct table t[]; int k; {

  int i;
  for (i = 0; t[i].key != -1; i++) {
    if (t[i].key == k) return(t[i].msg);
  }
  return("BAD VALUE");
}

menu(table) char *table[]; {

  int i, tablesize, choice;
  char c;
  char storage[80];
  char *s = storage;
  println("");
  tablesize = 0;
  for (i = 0; *table[i] != '\0'; i++) {
    printf("          %d. %s\n", i, table[i]);
    tablesize++;
  }
  tablesize--;

  printf("\n          Enter choice number (0 - %d): ", tablesize);
  for(;;) {
    inputline(s);
    choice = 0;
    while (c = *s++) choice = 10 * choice + c - '0';
    if (choice >= 0 && choice <= tablesize) {
      ed(2);
      return (choice);
    }
    printf("          Bad choice, try again: ");
  }
}

chrprint (s) char *s; {

  int i;

  printf("  ");
  sgr("7");
  printf(" ");
  for (i = 0; s[i] != '\0'; i++) {
    if (s[i] <= ' ' || s[i] == '\177')
    printf("<%d> ", s[i]);
    else printf("%c ", s[i]);
  }
  sgr("");
}
