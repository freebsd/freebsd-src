/*
  ALPHA interface for CHESS
   
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
#include <sys/param.h>
#include <sys/times.h>
#include <sys/file.h>
#include <curses.h>
#include <signal.h>
#include "gnuchess.h"
#ifdef NEWMOVE
#include "move.h"
#endif
#include "pathnames.h"

struct tms tmbuf1,tmbuf2;
void TerminateSearch(),Die();

#define scanz fflush(stdout),scanw
#define printz printw


Initialize()
{
  signal(SIGINT,Die); signal(SIGQUIT,Die);
  initscr();
  crmode();
}


ExitChess()
{
  nocrmode();
  endwin();
  exit(0);
}


void
Die()
{
char s[80];
  signal(SIGINT,SIG_IGN);
  signal(SIGQUIT,SIG_IGN);
  ShowMessage("Abort? ");
  scanz("%s",s);
  if (strcmp(s,"yes") == 0) ExitChess();
  signal(SIGINT,Die); signal(SIGQUIT,Die);
}


void
TerminateSearch()
{
  signal(SIGINT,SIG_IGN);
  signal(SIGQUIT,SIG_IGN);
  timeout = true;
  bothsides = false;
  signal(SIGINT,Die); signal(SIGQUIT,Die);
}


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
short ok,i,tmp;
long cnt,rate,t1,t2;
unsigned short mv;
char s[80];

  ok = quit = false;
  player = opponent;
  ShowSidetomove();
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
          PromptForMove();
          SelectMove(computer,2);
          VerifyMove(mvstr1,2,&mv);
          if (Sdepth > 0) Sdepth--;
        }
      ft = time((time_t *)0) - time0;
      epsquare = tmp;
    }
  
  signal(SIGINT,Die); signal(SIGQUIT,Die);
  while (!(ok || quit))
    {
      PromptForMove();
      scanz("%s",s);
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
      if (strcmp(s,"edit") == 0) EditBoard();
      if (strcmp(s,"go") == 0) ok = true;
      if (strcmp(s,"help") == 0) help();
      if (strcmp(s,"force") == 0) force = !force;
      if (strcmp(s,"book") == 0) Book = NULL;
      if (strcmp(s,"undo") == 0 && GameCnt >= 0) Undo();
      if (strcmp(s,"new") == 0) NewGame();
      if (strcmp(s,"list") == 0) ListGame();
      if (strcmp(s,"level") == 0) SelectLevel();
      if (strcmp(s,"hash") == 0) hashflag = !hashflag;
      if (strcmp(s,"beep") == 0) beep = !beep;
      if (strcmp(s,"Awindow") == 0) ChangeAlphaWindow();
      if (strcmp(s,"Bwindow") == 0) ChangeBetaWindow();
      if (strcmp(s,"hint") == 0) GiveHint();
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
          gotoXY(50,24);
          printz("cnt= %ld  rate= %ld",cnt,rate);
          ClrEoln();
        }
      if (strcmp(s,"p") == 0) ShowPostnValues();
      if (strcmp(s,"debug") == 0) DoDebug();
    }
    
  ClearMessage();
  ElapsedTime(1);
  if (force)
    {
      computer = opponent; opponent = otherside[computer];
    }
  (void) times(&tmbuf1);
  signal(SIGINT,TerminateSearch); signal(SIGQUIT,TerminateSearch);
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
  gotoXY(50,2); printz(".   Exit to main");
  gotoXY(50,3); printz("#   Clear board");
  gotoXY(49,5); printz("Enter piece & location: ");
  a = white;
  do
  {
    gotoXY(73,5); ClrEoln(); scanz("%s",s);
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
        DrawPiece(sq);
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


help()
{
  ClrScreen();
  gotoXY(28,1); printz("CHESS command summary");
  gotoXY(1,3); printz("g1f3      move from g1 to f3");
  gotoXY(1,4); printz("nf3       move knight to f3");
  gotoXY(1,5); printz("o-o       castle king side");
  gotoXY(1,6); printz("o-o-o     castle queen side");
  gotoXY(1,7); printz("edit      edit board");
  gotoXY(1,8); printz("switch    sides with computer");
  gotoXY(1,9); printz("white     computer plays white");
  gotoXY(1,10); printz("black     computer plays black");
  gotoXY(1,11); printz("reverse   board display");
  gotoXY(1,12); printz("both      computer match");
  gotoXY(1,13); printz("random    randomize play");
  gotoXY(1,14); printz("undo      undo last move");
  gotoXY(42,3); printz("level     change level");
  gotoXY(42,4); printz("depth     set search depth");
  gotoXY(42,5); printz("post      principle variation");
  gotoXY(42,6); printz("hint      suggest a move");
  gotoXY(42,7); printz("bd        redraw board");
  gotoXY(42,8); printz("force     enter game moves");
  gotoXY(42,9); printz("list      game to chess.lst");
  gotoXY(42,10); printz("save      game to file");
  gotoXY(42,11); printz("get       game from file");
  gotoXY(42,12); printz("new       start new game");
  gotoXY(42,13); printz("quit      exit CHESS");
  gotoXY(10,21); printz("Computer: ");
  if (computer == white) printz("WHITE"); else printz("BLACK");
  gotoXY(10,22); printz("Opponent: ");
  if (opponent == white) printz("WHITE"); else printz("BLACK");
  gotoXY(10,23); printz("Level: %ld",Level," sec.");
  gotoXY(10,24); printz("Easy mode: ");
  if (easy) printz("ON"); else printz("OFF");
  gotoXY(40,21); printz("Depth: %d",MaxSearchDepth);
  gotoXY(40,22); printz("Random: "); 
  if (dither) printz("ON"); else printz("OFF");
  gotoXY(40,23); printz("Transposition table: ");
  if (hashflag) printz("ON"); else printz("OFF");
  refresh();
  while (getchar() != 27);
  ClrScreen();
  UpdateDisplay(0,0,1,0);
}


ShowDepth(ch)
char ch;
{
  gotoXY(50,4); printz("Depth= %d%c ",Sdepth,ch); ClrEoln();
}


ShowResults(score,bstline,ch)
short score;
unsigned short bstline[];
char ch;
{
short d,e,ply;
  if (post && player == computer)
    {
      e = lpost;
      gotoXY(50,5); printz("Score= %d",score); ClrEoln();
      d = 8; gotoXY(50,d); ClrEoln();
      for (ply = 1; bstline[ply] > 0; ply++)
        {
          algbr(bstline[ply] >> 8,bstline[ply] & 0xFF,false);
          if (ply == 5 || ply == 9 || ply == 13 || ply == 17)
            {
              gotoXY(50,++d); ClrEoln();
            }
          printz("%5s ",mvstr1);
        }
      ClrEoln();
      lpost = d;
      while (++d <= e)
        {
          gotoXY(50,d); ClrEoln();
        }
    }
}


SearchStartStuff(side)
short side;
{
short i;
  signal(SIGINT,TerminateSearch); signal(SIGQUIT,TerminateSearch);
  if (player == computer)
    for (i = 5; i < 14; i++)
      {
        gotoXY(50,i); ClrEoln();
      }
}


OutputMove()
{
  if (root->flags & epmask) UpdateDisplay(0,0,1,0);
  else UpdateDisplay(root->f,root->t,0,root->flags & cstlmask);
  gotoXY(50,17); printz("My move is: %s",mvstr1);
  if (beep) putchar(7);
  ClrEoln();
  
  gotoXY(50,24);
  if (root->flags & draw) printz("Draw game!");
  else if (root->score == -9999) printz("opponent mates!");
  else if (root->score == 9998) printz("computer mates!");
  else if (root->score < -9000) printz("opponent will soon mate!");
  else if (root->score > 9000)  printz("computer will soon mate!");
  ClrEoln();
  
  if (post)
    {
      gotoXY(50,22); printz("Nodes=   %6ld",NodeCnt); ClrEoln();
      gotoXY(50,23); printz("Nodes/Sec= %4ld",evrate); ClrEoln();
    }
}


ElapsedTime(iop)

/* 
   Determine the time that has passed since the search was started. If 
   the elapsed time exceeds the target (ResponseTime+ExtraTime) then set 
   timeout to true which will terminate the search. 
*/

short iop;
{
  et = time((time_t *)0) - time0;
  if (et < 0) et = 0;
  ETnodes += 50;
  if (et > et0 || iop == 1)
    {
      if (et > ResponseTime+ExtraTime && Sdepth > 1) timeout = true;
      et0 = et;
      if (iop == 1)
        {
          time0 = time((time_t *)0); et0 = 0;
        }
      (void) times(&tmbuf2);
      cputimer = 100*(tmbuf2.tms_utime - tmbuf1.tms_utime) / HZ;
      if (cputimer > 0) evrate = (100*NodeCnt)/(cputimer+100*ft);
      else evrate = 0;
      ETnodes = NodeCnt + 50;
      UpdateClocks();
    }
}


UpdateClocks()
{
short m,s;
  m = et/60; s = (et - 60*m);
  if (TCflag)
    {
      m = (TimeControl.clock[player] - et) / 60;
      s = TimeControl.clock[player] - et - 60*m;
    }
  if (m < 0) m = 0;
  if (s < 0) s = 0;
  if (player == white)
    if (reverse) gotoXY(20,2); else gotoXY(20,23);
  else
    if (reverse) gotoXY(20,23); else gotoXY(20,2);
  printz("%d:%2d   ",m,s);
  if (post)
    {
      gotoXY(50,22); printz("Nodes=   %6ld",NodeCnt);
      gotoXY(50,23); printz("Nodes/Sec= %4ld",evrate);
    }
  refresh();
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


gotoXY(x,y)
short x,y;
{
  move(y-1,x-1);
}


ClrScreen()
{
  clear(); refresh();
}


ClrEoln()
{
  clrtoeol(); refresh();
}


DrawPiece(sq)
short sq;
{
short r,c; char x;
  if (reverse) r = 7-row[sq]; else r = row[sq];
  if (reverse) c = 7-column[sq]; else c = column[sq];
  if (color[sq] == black) x = '*'; else x = ' ';
  gotoXY(5+5*c,5+2*(7-r)); printz("%c%c ",x,pxx[board[sq]]);
}


UpdateDisplay(f,t,flag,iscastle)
short f,t,flag,iscastle;
{
short i,l,z; 
  if (flag)
    {
      gotoXY(56,2); printz("CHESS");
      i = 3;
      gotoXY(3,++i);
      printz("|----|----|----|----|----|----|----|----|");
      while (i<19)
        {
          gotoXY(1,++i);
          if (reverse) z = (i/2)-1; else z = 10-(i/2);
          printz("%d |    |    |    |    |    |    |    |    |",z);
          gotoXY(3,++i);
          if (i < 19)
            printz("+----+----+----+----+----+----+----+----+");
        }
      printz("|----|----|----|----|----|----|----|----|");
      gotoXY(3,21);
      if (reverse) printz("  h    g    f    e    d    c    b    a");
              else printz("  a    b    c    d    e    f    g    h");
      if (reverse) gotoXY(5,23); else gotoXY(5,2);
      if (computer == black) printz("Computer"); else printz("Human   ");
      if (reverse) gotoXY(5,2); else gotoXY(5,23);
      if (computer == white) printz("Computer"); else printz("Human   ");
      for (l = 0; l < 64; l++) DrawPiece(l);
    }
  else
    {
      DrawPiece(f); DrawPiece(t);
      if (iscastle)
        if (t > f)
          { DrawPiece(f+3); DrawPiece(t-1); }
        else
          { DrawPiece(f-4); DrawPiece(t+1); }
    }
  refresh();
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
struct BookEntry *entry;
unsigned short mv,*mp,tmp[100];

  if ((fd = fopen(_PATH_CHESSBOOK,"r")) != NULL)
    {
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
    else
      {
	fprintf(stderr, "\nchess: can't read %s.\n", _PATH_CHESSBOOK);
	exit(1);
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

  ShowMessage("File name: ");
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

  ShowMessage("File name: ");
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
  gotoXY(50,24); printz("%s",s); ClrEoln();
}

ClearMessage()
{
  gotoXY(50,24); ClrEoln();
}

ShowSidetomove()
{
  gotoXY(50,14);
  if (player == white) printz("%2d:   WHITE",1+(GameCnt+1)/2);
  else printz("%2d:   BLACK",1+(GameCnt+1)/2);
  ClrEoln();
}

PromptForMove()
{
  gotoXY(50,19); printz("Your move is? "); ClrEoln();
}

ShowCurrentMove(pnt,f,t)
short pnt,f,t;
{
  algbr(f,t,false);
  gotoXY(50,7); printz("(%2d) %4s",pnt,mvstr1);
}

ChangeAlphaWindow()
{
  ShowMessage("window: ");
  scanz("%hd",&Awindow);
}

ChangeBetaWindow()
{
  ShowMessage("window: ");
  scanz("%hd",&Bwindow);
}

GiveHint()
{
char s[40];
  algbr((short)(hint>>8),(short)(hint & 0xFF),false);
  strcpy(s,"try ");
  strcat(s,mvstr1);
  ShowMessage(s);
}

ChangeSearchDepth()
{
  ShowMessage("depth= ");
  scanz("%hd",&MaxSearchDepth);
}

SetContempt()
{
  ShowMessage("contempt= ");
  scanz("%hd",&contempt);
}

ChangeXwindow()
{
  ShowMessage("xwndw= ");
  scanz("%hd",&xwndw);
}


SelectLevel()
{
  ClrScreen();
  gotoXY(32,2); printz("CHESS");
  gotoXY(20,4); printz(" 1.   60 moves in   5 minutes");
  gotoXY(20,5); printz(" 2.   60 moves in  15 minutes");
  gotoXY(20,6); printz(" 3.   60 moves in  30 minutes");
  gotoXY(20,7); printz(" 4.   40 moves in  30 minutes");
  gotoXY(20,8); printz(" 5.   40 moves in  60 minutes");
  gotoXY(20,9); printz(" 6.   40 moves in 120 minutes");
  gotoXY(20,10); printz(" 7.   40 moves in 240 minutes");
  gotoXY(20,11); printz(" 8.    1 move  in  15 minutes");
  gotoXY(20,12); printz(" 9.    1 move  in  60 minutes");
  gotoXY(20,13); printz("10.    1 move  in 600 minutes");
  
  OperatorTime = 0; TCmoves = 60; TCminutes = 5;

  gotoXY(20,17); printz("Enter Level: ");
  refresh();
  scanz("%ld",&Level);
  switch (Level)
    {
      case 1 : TCmoves = 60; TCminutes = 5; break;
      case 2 : TCmoves = 60; TCminutes = 15; break;
      case 3 : TCmoves = 60; TCminutes = 30; break;
      case 4 : TCmoves = 40; TCminutes = 30; break;
      case 5 : TCmoves = 40; TCminutes = 60; break;
      case 6 : TCmoves = 40; TCminutes = 120; break;
      case 7 : TCmoves = 40; TCminutes = 240; break;
      case 8 : TCmoves = 1; TCminutes = 15; break;
      case 9 : TCmoves = 1; TCminutes = 60; break;
      case 10 : TCmoves = 1; TCminutes = 600; break;
    }

  TCflag = (TCmoves > 1);
  SetTimeControl();
  ClrScreen();
  UpdateDisplay(0,0,1,0);
}


ShowPostnValues()
{
short i,r,c;
  ExaminePosition();
  for (i = 0; i < 64; i++)
    {
      if (reverse) r = 7-row[i]; else r = row[i];
      if (reverse) c = 7-column[i]; else c = column[i];
      gotoXY(4+5*c,5+2*(7-r));
      c1 = color[i]; c2 = otherside[c1];
      PC1 = PawnCnt[c1]; PC2 = PawnCnt[c2];
      atk1 = atak[c1]; atk2 = atak[c2];
      if (color[i] == neutral) printz("   ");
      else printz("%3d ",SqValue(i,opponent));
    }
  ScorePosition(opponent,&i);
  gotoXY(50,24);
  printz("Score= %d",i); ClrEoln();
}


DoDebug()
{
short k,p,i,r,c,tp,tc;
char s[40];
  ExaminePosition();
  ShowMessage("Enter piece: ");
  scanz("%s",s);
  if (s[0] == 'w') k = white; else k = black;
  if (s[1] == 'p') p = pawn;
  else if (s[1] == 'n') p = knight;
  else if (s[1] == 'b') p = bishop;
  else if (s[1] == 'r') p = rook;
  else if (s[1] == 'q') p = queen;
  else if (s[1] == 'k') p = king;
  else p = no_piece;
  for (i = 0; i < 64; i++)
    {
      if (reverse) r = 7-row[i]; else r = row[i];
      if (reverse) c = 7-column[i]; else c = column[i];
      gotoXY(4+5*c,5+2*(7-r));
      tp = board[i]; tc = color[i];
      board[i] = p; color[i] = k;
      c1 = k; c2 = otherside[c1];
      PC1 = PawnCnt[c1]; PC2 = PawnCnt[c2];
      atk1 = atak[c1]; atk2 = atak[c2];
      printz("%3d ",SqValue(i,opponent));
      board[i] = tp; color[i] = tc;
    }
  ScorePosition(opponent,&i);
  gotoXY(50,24);
  printz("Score= %d",i); ClrEoln();
}
