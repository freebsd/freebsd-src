/*
  UNIX & MSDOS NON-DISPLAY, AND CHESSTOOL interface for Chess
   
  Revision: 4-25-88
   
  Copyright (C) 1986, 1987, 1988 Free Software Foundation, Inc.
  Copyright (c) 1988  John Stanback

  This file is part of CHESS.

  CHESS is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY.  No author or distributor
  accepts responsibility to anyone for the consequences of using it
  or for whether it serves any particular purpose or works at all,
  unless he says so in writing.  Refer to the CHESS General Public
  License for full details.

  Everyone is granted permission to copy, modify and redistribute
  CHESS, but only under the conditions described in the
  CHESS General Public License.   A copy of this license is
  supposed to have been given to you along with CHESS so you
  can know your rights and responsibilities.  It should be in a
  file named COPYING.  Among other things, the copyright notice
  and this notice must be preserved on all copies.
*/


#include <stdio.h>
#include <ctype.h>
#ifdef MSDOS
#include <dos.h>
#include <stdlib.h>
#include <time.h>
#else
#include <sys/param.h>
#include <sys/times.h>
#include <sys/file.h>
struct tms tmbuf1,tmbuf2;
int TerminateSearch(),Die();
#endif MSDOS

#include "gnuchess.h"
#ifdef NEWMOVE
#include "move.h"
#endif

#define printz printf
#define scanz scanf
int mycnt1,mycnt2;


Initialize()
{
  mycnt1 = mycnt2 = 0;
#ifndef MSDOS
#endif
#ifdef CHESSTOOL
  setlinebuf(stdout);
/*
  setvbuf(stdout,NULL,_IOLBF,BUFSIZ);
*/
  printf("Chess\n");
  if (Level == 0 && !TCflag) Level = 15;
#endif CHESSTOOL
}

ExitChess()
{
  ListGame();
  exit(0);
}

#ifndef MSDOS
Die()
{
char s[80];
  printz("Abort? ");
  scanz("%s",s);
  if (strcmp(s,"yes") == 0) ExitChess();
}

TerminateSearch()
{
  timeout = true;
  bothsides = false;
}
#endif MSDOS


InputCommand()

/*
   Process the users command. If easy mode is OFF (the computer is 
   thinking on opponents time) and the program is out of book, then make 
   the 'hint' move on the board and call SelectMove() to find a response. 
   The user terminates the search by entering ^C (quit siqnal) before 
   entering a command. If the opponent does not make the hint move, then 
   set Sdepth to zero. 
*/

{
int i;
short ok,tmp;
long cnt,rate,t1,t2;
unsigned short mv;
char s[80];

  ok = quit = false;
  player = opponent;
  ft = 0;
  if (hint > 0 && !easy && Book == NULL)
    {
      fflush(stdout);
      time0 = time((long *)0);
      algbr(hint>>8,hint & 0xFF,false);
      strcpy(s,mvstr1);
      tmp = epsquare;
      if (VerifyMove(s,1,&mv))
        {
          SelectMove(computer,2);
          VerifyMove(mvstr1,2,&mv);
          if (Sdepth > 0) Sdepth--;
        }
      ft = time((long *)0) - time0;
      epsquare = tmp;
    }
  
#ifndef MSDOS
#endif
  while (!(ok || quit))
    {
      PromptForMove();
      i = scanz("%s",s);
      if (i == EOF || s[0] == 0) ExitChess();
      player = opponent;
      ok = VerifyMove(s,0,&mv);
      if (ok && mv != hint)
        {
          Sdepth = 0;
          ft = 0;
        }
        
      if (strcmp(s,"bd") == 0)
        {
          ClrScreen();
          UpdateDisplay(0,0,1,0);
        }
      if (strcmp(s,"quit") == 0) quit = true;
      if (strcmp(s,"post") == 0) post = !post;
      if (strcmp(s,"set") == 0) EditBoard();
      if (strcmp(s,"go") == 0) ok = true;
      if (strcmp(s,"help") == 0) help();
      if (strcmp(s,"force") == 0) force = !force;
      if (strcmp(s,"book") == 0) Book = NULL;
      if (strcmp(s,"new") == 0) NewGame();
      if (strcmp(s,"list") == 0) ListGame();
      if (strcmp(s,"level") == 0) SelectLevel();
      if (strcmp(s,"hash") == 0) hashflag = !hashflag;
      if (strcmp(s,"beep") == 0) beep = !beep;
      if (strcmp(s,"Awindow") == 0) ChangeAlphaWindow();
      if (strcmp(s,"Bwindow") == 0) ChangeBetaWindow();
      if (strcmp(s,"rcptr") == 0) rcptr = !rcptr;
      if (strcmp(s,"hint") == 0) GiveHint();
      if (strcmp(s,"zero") == 0) ZeroTTable();
      if (strcmp(s,"both") == 0)
        {
          bothsides = !bothsides;
          Sdepth = 0;
          SelectMove(opponent,1);
          ok = true;
        }
      if (strcmp(s,"reverse") == 0)
        {
          reverse = !reverse;
          ClrScreen();
          UpdateDisplay(0,0,1,0);
        }
      if (strcmp(s,"switch") == 0)
        {
          computer = otherside[computer];
          opponent = otherside[opponent];
          force = false;
          Sdepth = 0;
          ok = true;
        }
      if (strcmp(s,"white") == 0)  
        {
          computer = white; opponent = black;
          ok = true; force = false;
          Sdepth = 0;
        }
      if (strcmp(s,"black") == 0)
        {
          computer = black; opponent = white;
          ok = true; force = false;
          Sdepth = 0;
        }
      if (strcmp(s,"undo") == 0 && GameCnt >= 0) Undo();
      if (strcmp(s,"remove") == 0 && GameCnt >= 1) 
        {
          Undo(); Undo();
        }
      if (strcmp(s,"get") == 0) GetGame();
      if (strcmp(s,"save") == 0) SaveGame();
      if (strcmp(s,"depth") == 0) ChangeSearchDepth();
      if (strcmp(s,"random") == 0) dither = 6;
      if (strcmp(s,"easy") == 0) easy = !easy;
      if (strcmp(s,"contempt") == 0) SetContempt();
      if (strcmp(s,"xwndw") == 0) ChangeXwindow();
      if (strcmp(s,"test") == 0)
        {
          t1 = time(0);
          cnt = 0;
          for (i = 0; i < 10000; i++)
            {
              MoveList(opponent,2);
              cnt += TrPnt[3] - TrPnt[2];
            }
          t2 = time(0);
          rate = cnt / (t2-t1);
          printz("cnt= %ld  rate= %ld\n",cnt,rate);
        }
    }
    
  ElapsedTime(1);
  if (force)
    {
      computer = opponent; opponent = otherside[computer];
    }
#ifndef MSDOS
  (void) times(&tmbuf1);
#ifdef CHESSTOOL
  printf("%d. %s\n",++mycnt2,s);
#endif CHESSTOOL
#endif MSDOS
}


help()
{
  ClrScreen();
  printz("CHESS command summary\n");
  printz("g1f3      move from g1 to f3\n");
  printz("nf3       move knight to f3\n");
  printz("o-o       castle king side\n");
  printz("o-o-o     castle queen side\n");
  printz("set       edit board\n");
  printz("switch    sides with computer\n");
  printz("white     computer plays white\n");
  printz("black     computer plays black\n");
  printz("reverse   board display\n");
  printz("both      computer match\n");
  printz("random    randomize play\n");
  printz("undo      undo last move\n");
  printz("time      change level\n");
  printz("depth     set search depth\n");
  printz("post      principle variation\n");
  printz("hint      suggest a move\n");
  printz("bd        redraw board\n");
  printz("clock     set time control\n");
  printz("force     enter game moves\n");
  printz("list      game to chess.lst\n");
  printz("save      game to file\n");
  printz("get       game from file\n");
  printz("new       start new game\n");
  printz("quit      exit CHESS\n");
  printz("Computer: ");
  if (computer == white) printz("WHITE\n"); else printz("BLACK\n");
  printz("Opponent: ");
  if (opponent == white) printz("WHITE\n"); else printz("BLACK\n");
  printz("Response time: %ld",Level," sec.\n");
  printz("Easy mode: ");
  if (easy) printz("ON\n"); else printz("OFF\n");
  printz("Depth: %d\n",MaxSearchDepth);
  printz("Random: "); 
  if (dither) printz("ON\n"); else printz("OFF\n");
  printz("Transposition table: ");
  if (hashflag) printz("ON\n"); else printz("OFF\n");
  UpdateDisplay(0,0,1,0);
}


EditBoard()

/* 
   Set up a board position. Pieces are entered by typing the piece 
   followed by the location. For example, Nf3 will place a knight on 
   square f3. 
*/

{
short a,r,c,sq;
char s[80];

  ClrScreen();
  UpdateDisplay(0,0,1,0);
  printz(".   exit to main\n");
  printz("#   clear board\n");
  printz("enter piece & location: \n");
  
  a = white;
  do
  {
    scanz("%s",s);
    if (s[0] == '#')
      {
        for (sq = 0; sq < 64; sq++)
          {
            board[sq] = no_piece; color[sq] = neutral;
          }
        UpdateDisplay(0,0,1,0);
      }
    if (s[0] == 'c' || s[0] == 'C') a = otherside[a];
    c = s[1]-'a'; r = s[2]-'1';
    if ((c >= 0) && (c < 8) && (r >= 0) && (r < 8))
      {
        sq = locn[r][c];
        color[sq] = a;
        if (s[0] == 'p') board[sq] = pawn;
        else if (s[0] == 'n') board[sq] = knight;
        else if (s[0] == 'b') board[sq] = bishop;
        else if (s[0] == 'r') board[sq] = rook;
        else if (s[0] == 'q') board[sq] = queen;
        else if (s[0] == 'k') board[sq] = king;
        else { board[sq] = no_piece; color[sq] = neutral; }
      }
  }
  while (s[0] != '.');
  if (board[4] != king) kingmoved[white] = 10;
  if (board[60] != king) kingmoved[black] = 10;
  GameCnt = -1; Game50 = 0; Sdepth = 0;
  InitializeStats();
  ClrScreen();
  UpdateDisplay(0,0,1,0);
}


ShowDepth(ch)
char ch;
{
}

ShowResults(score,bstline,ch)
short score;
unsigned short bstline[];
char ch;
{
#ifndef CHESSTOOL
register int i;
  printz("%2d%c  %5d %4ld %7ld   ",Sdepth,ch,score,et,NodeCnt);
  for (i = 1; bstline[i] > 0; i++)
    {
      algbr((short)(bstline[i] >> 8),(short)(bstline[i] & 0xFF),false);
      if (i == 9 || i == 17) printz("\n                          ");
      printz("%5s ",mvstr1);
    }
  printz("\n");
#endif
}


SearchStartStuff(side)
short side;
{
#ifndef MSDOS
#endif
#ifndef CHESSTOOL
  printz("\nMove# %d    Target= %ld    Clock: %ld\n",
          TCmoves-TimeControl.moves[side]+1,
          ResponseTime,TimeControl.clock[side]);
#endif
}


OutputMove()
{
#ifdef CHESSTOOL
  printz("%d. ... %s\n",++mycnt1,mvstr1);
  if (root->flags & draw)
    {
      printz("Draw\n");
      ListGame();
      exit(0);
    }
  if (root->score == -9999)
    {
      if (opponent == white) printz("White\n"); else printz("Black\n");
      ListGame();
      exit(0);
    }
  if (root->score == 9998)
    {
      if (computer == white) printz("White\n"); else printz("Black\n");
      ListGame();
      exit(0);
    }
#else
  printz("Nodes= %ld  Eval= %ld  Hash= %ld  Rate= %ld  ",
          NodeCnt,EvalNodes,HashCnt,evrate);
  printz("CPU= %.2ld:%.2ld.%.2ld\n\n",
          cputimer/6000,(cputimer % 6000)/100,cputimer % 100);
          
  if (root->flags & epmask) UpdateDisplay(0,0,1,0);
  else UpdateDisplay(root->f,root->t,0,root->flags & cstlmask);
  printz("My move is: %s\n\n",mvstr1);
  if (beep) printz("%c",7);
  
  if (root->flags & draw) printz("Draw game!\n");
  else if (root->score == -9999) printz("opponent mates!\n");
  else if (root->score == 9998) printz("computer mates!\n");
  else if (root->score < -9000) printz("opponent will soon mate!\n");
  else if (root->score > 9000)  printz("computer will soon mate!\n");
#endif CHESSTOOL
}


ElapsedTime(iop)
short iop;

/* 
   Determine the time that has passed since the search was started. If 
   the elapsed time exceeds the target (ResponseTime+ExtraTime) then set 
   timeout to true which will terminate the search. 
*/

{
  et = time((long *)0) - time0;
  if (et < 0) et = 0;
  ETnodes += 50;
  if (et > et0 || iop == 1)
    {
      if (et > ResponseTime+ExtraTime && Sdepth > 1) timeout = true;
      et0 = et;
      if (iop == 1)
        {
          time0 = time((long *)0); et0 = 0;
        }
#ifdef MSDOS
      cputimer = 100*et;
      if (et > 0) evrate = NodeCnt/(et+ft); else evrate = 0;
      if (kbhit() && Sdepth > 1)
        {
          timeout = true;
          bothsides = false;
        }
#else
      (void) times(&tmbuf2);
      cputimer = 100*(tmbuf2.tms_utime - tmbuf1.tms_utime) / HZ;
      if (cputimer > 0) evrate = (100*NodeCnt)/(cputimer+100*ft);
      else evrate = 0;
#endif MSDOS
      ETnodes = NodeCnt + 50;
    }
}


SetTimeControl()
{
  if (TCflag)
    {
      TimeControl.moves[white] = TimeControl.moves[black] = TCmoves;
      TimeControl.clock[white] = TimeControl.clock[black] = 60*(long)TCminutes;
    }
  else
    {
      TimeControl.moves[white] = TimeControl.moves[black] = 0;
      TimeControl.clock[white] = TimeControl.clock[black] = 0;
      Level = 60*(long)TCminutes;
    }
  et = 0;
  ElapsedTime(1);
}


ClrScreen()
{
#ifndef CHESSTOOL
  printz("\n");
#endif
}


UpdateDisplay(f,t,flag,iscastle)
short f,t,flag,iscastle;
{
#ifndef CHESSTOOL
short r,c,l;
  if (flag)
    {
      printz("\n");
      for (r = 7; r >= 0; r--)
        {
          for (c = 0; c <= 7; c++)
            {
              if (reverse) l = locn[7-r][7-c]; else l = locn[r][c];
              if (color[l] == neutral) printz(" -");
              else if (color[l] == white) printz(" %c",qxx[board[l]]);
              else printz(" %c",pxx[board[l]]);
            }
          printz("\n");
        }
      printz("\n");
    }
#endif CHESSTOOL
}


GetOpenings()

/*
   Read in the Opening Book file and parse the algebraic notation for a 
   move into an unsigned integer format indicating the from and to 
   square. Create a linked list of opening lines of play, with 
   entry->next pointing to the next line and entry->move pointing to a 
   chunk of memory containing the moves. More Opening lines of up to 256 
   half moves may be added to gnuchess.book. 
*/

{
FILE *fd;
int c,i,j,side;
char buffr[2048];
struct BookEntry *entry;
unsigned short mv,*mp,tmp[100];

  if ((fd = fopen("gnuchess.book","r")) != NULL)
    {
/*
      setvbuf(fd,buffr,_IOFBF,2048);
*/
      Book = NULL;
      i = 0; side = white;
      while ((c = parse(fd,&mv,side)) >= 0)
        if (c == 1)
          {
            tmp[++i] = mv;
            side = otherside[side];
          }
        else if (c == 0 && i > 0)
          {
            entry = (struct BookEntry *)malloc(sizeof(struct BookEntry));
            mp = (unsigned short *)malloc((i+1)*sizeof(unsigned short));
            entry->mv = mp;
            entry->next = Book;
            Book = entry; 
            for (j = 1; j <= i; j++) *(mp++) = tmp[j];
            *mp = 0;
            i = 0; side = white;
          }
      fclose(fd);
    }
}


int parse(fd,mv,side)
FILE *fd;
unsigned short *mv;
short side;
{
int c,i,r1,r2,c1,c2;
char s[100];
  while ((c = getc(fd)) == ' ');
  i = 0; s[0] = c;
  while (c != ' ' && c != '\n' && c != EOF) s[++i] = c = getc(fd);
  s[++i] = '\0';
  if (c == EOF) return(-1);
  if (s[0] == '!' || i < 3)
    {
      while (c != '\n' && c != EOF) c = getc(fd);
      return(0);
    }
  if (s[4] == 'o')
    if (side == black) *mv = 0x3C3A; else *mv = 0x0402;
  else if (s[0] == 'o')
    if (side == black) *mv = 0x3C3E; else *mv = 0x0406;
  else
    {
      c1 = s[0] - 'a'; r1 = s[1] - '1';
      c2 = s[2] - 'a'; r2 = s[3] - '1';
      *mv = (locn[r1][c1]<<8) + locn[r2][c2];
    }
  return(1);
}


GetGame()
{
FILE *fd;
char fname[40];
int c;
short sq;
unsigned short m;

  printz("Enter file name: ");
  scanz("%s",fname);
  if (fname[0] == '\0') strcpy(fname,"chess.000");
  if ((fd = fopen(fname,"r")) != NULL)
    {
      fscanf(fd,"%hd%hd%hd",&computer,&opponent,&Game50);
      fscanf(fd,"%hd%hd%hd%hd",
             &castld[white],&castld[black],
             &kingmoved[white],&kingmoved[black]);
      fscanf(fd,"%hd%hd",&TCflag,&OperatorTime);
      fscanf(fd,"%ld%ld%hd%hd",
             &TimeControl.clock[white],&TimeControl.clock[black],
             &TimeControl.moves[white],&TimeControl.moves[black]);
      for (sq = 0; sq < 64; sq++)
        {
          fscanf(fd,"%hd",&m);
          board[sq] = (m >> 8); color[sq] = (m & 0xFF);
          if (color[sq] == 0) color[sq] = neutral; else --color[sq];
        }
      GameCnt = -1; c = '?';
      while (c != EOF)
        {
          ++GameCnt;
          c = fscanf(fd,"%hd%hd%hd%ld%hd%hd%hd",&GameList[GameCnt].gmove,
                     &GameList[GameCnt].score,&GameList[GameCnt].depth,
                     &GameList[GameCnt].nodes,&GameList[GameCnt].time,
                     &GameList[GameCnt].piece,&GameList[GameCnt].color);
          if (GameList[GameCnt].color == 0) GameList[GameCnt].color = neutral;
          else --GameList[GameCnt].color;
        }
      GameCnt--;
      if (TimeControl.clock[white] > 0) TCflag = true;
      computer--; opponent--;
    }
  fclose(fd);
  InitializeStats();
  UpdateDisplay(0,0,1,0);
  Sdepth = 0;
}


SaveGame()
{
FILE *fd;
char fname[40];
short sq,i,c;

  printz("Enter file name: ");
  scanz("%s",fname);
  
  if (fname[0] == '\0' || access(fname,W_OK) == -1) strcpy(fname,"chess.000");
  fd = fopen(fname,"w");
  fprintf(fd,"%d %d %d\n",computer+1,opponent+1,Game50);
  fprintf(fd,"%d %d %d %d\n",
          castld[white],castld[black],kingmoved[white],kingmoved[black]);
  fprintf(fd,"%d %d\n",TCflag,OperatorTime);
  fprintf(fd,"%ld %ld %d %d\n",
          TimeControl.clock[white],TimeControl.clock[black],
          TimeControl.moves[white],TimeControl.moves[black]);
  for (sq = 0; sq < 64; sq++)
    {
      if (color[sq] == neutral) c = 0; else c = color[sq]+1;
      fprintf(fd,"%d\n",256*board[sq] + c);
    }
  for (i = 0; i <= GameCnt; i++)
    {
      if (GameList[i].color == neutral) c = 0;
      else c = GameList[i].color + 1;
      fprintf(fd,"%d %d %d %ld %d %d %d\n",
              GameList[i].gmove,GameList[i].score,GameList[i].depth,
              GameList[i].nodes,GameList[i].time,
              GameList[i].piece,c);
    }
  fclose(fd);
}


ListGame()
{
FILE *fd;
short i,f,t;
  fd = fopen("chess.lst","w");
  fprintf(fd,"\n");
  fprintf(fd,"       score  depth  nodes  time         ");
  fprintf(fd,"       score  depth  nodes  time\n");
  for (i = 0; i <= GameCnt; i++)
    {
      f = GameList[i].gmove>>8; t = (GameList[i].gmove & 0xFF);
      algbr(f,t,false);
      if ((i % 2) == 0) fprintf(fd,"\n"); else fprintf(fd,"         ");
      fprintf(fd,"%5s  %5d     %2d %6ld %5d",mvstr1,
              GameList[i].score,GameList[i].depth,
              GameList[i].nodes,GameList[i].time);
    }
  fprintf(fd,"\n\n");
  fclose(fd);
} 


Undo()

/*
   Undo the most recent half-move.
*/

{
short f,t;
  f = GameList[GameCnt].gmove>>8;
  t = GameList[GameCnt].gmove & 0xFF;
  if (board[t] == king && distance(t,f) > 1)
    castle(GameList[GameCnt].color,f,t,2);
  else
    {
      board[f] = board[t]; color[f] = color[t];
      board[t] = GameList[GameCnt].piece;
      color[t] = GameList[GameCnt].color;
      if (board[f] == king) --kingmoved[color[f]];
    }
  if (TCflag) ++TimeControl.moves[color[f]];
  GameCnt--; mate = false; Sdepth = 0;
  UpdateDisplay(0,0,1,0);
  InitializeStats();
}


ShowMessage(s)
char *s;
{
#ifndef CHESSTOOL
  printz("%s\n");
#endif CHESSTOOL
}

ShowSidetomove()
{
}

PromptForMove()
{
#ifndef CHESSTOOL
  printz("\nYour move is? ");
#endif CHESSTOOL
}


ShowCurrentMove(pnt,f,t)
short pnt,f,t;
{
}

ChangeAlphaWindow()
{
  printz("window: ");
  scanz("%hd",&Awindow);
}

ChangeBetaWindow()
{
  printz("window: ");
  scanz("%hd",&Bwindow);
}

GiveHint()
{
  algbr((short)(hint>>8),(short)(hint & 0xFF),false);
  printz("try %s\n",mvstr1);
}


SelectLevel()
{
  OperatorTime = 30000;
  printz("Enter #moves #minutes: ");
  scanz("%hd %hd",&TCmoves,&TCminutes);
  printz("Operator time= ");
  scanz("%hd",&OperatorTime);
  TCflag = (TCmoves > 1);
  SetTimeControl();
}


ChangeSearchDepth()
{
  printz("depth= ");
  scanz("%hd",&MaxSearchDepth);
}

SetContempt()
{
  printz("contempt= ");
  scanz("%hd",&contempt);
}

ChangeXwindow()
{
  printz("xwndw= ");
  scanz("%hd",&xwndw);
}
