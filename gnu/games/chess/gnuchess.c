/*
  C source for CHESS  

  Revision: 4-25-88

  Copyright (C) 1986, 1987, 1988 Free Software Foundation, Inc.
  Copyright (c) 1988   John Stanback

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
#include <stdlib.h>
#include <time.h>
#include <alloc.h>
#define ttblsz 4096
#else
#include <sys/param.h>
#include <sys/times.h>
#define ttblsz 16384
#define huge
#endif MSDOS

#include "move.h"

#define neutral 2
#define white 0
#define black 1 
#define no_piece 0
#define pawn 1
#define knight 2
#define bishop 3
#define rook 4
#define queen 5
#define king 6
#define valueP 100
#define valueN 350
#define valueB 355
#define valueR 550
#define valueQ 1100
#define valueK 1200
#define ctlP 0x4000
#define ctlN 0x2800
#define ctlB 0x1800
#define ctlR 0x0400
#define ctlQ 0x0200
#define ctlK 0x0100
#define ctlBQ 0x1200
#define ctlRQ 0x0600
#define ctlNN 0x2000
#define pxx " PNBRQK"
#define qxx " pnbrqk"
#define rxx "12345678"
#define cxx "abcdefgh"
#define check 0x0001
#define capture 0x0002
#define draw 0x0004
#define promote 0x0008
#define cstlmask 0x0010
#define epmask 0x0020
#define exact 0x0040
#define pwnthrt 0x0080
#define truescore 0x0001
#define lowerbound 0x0002
#define upperbound 0x0004
#define maxdepth 30
#define true 1
#define false 0
#define absv(x) ((x) < 0 ? -(x) : (x))
#if (NEWMOVE < 1)
#define taxicab(a,b) (abs(column[a]-column[b]) + abs(row[a]-row[b]))
#endif
struct leaf
  {
    short f,t,score,reply;
    unsigned short flags;
  };
struct GameRec
  {
    unsigned short gmove;
    short score,depth,time,piece,color;
    long nodes;
  };
struct TimeControlRec
  {
    short moves[2];
    long clock[2];
  };
struct BookEntry
  {
    struct BookEntry *next;
    unsigned short *mv;
  };
struct hashval
  {
    unsigned long bd;
    unsigned short key;
  };
struct hashentry
  {
    unsigned long hashbd;
    unsigned short mv,flags;
    short score,depth;
  };

char mvstr1[5],mvstr2[5];
struct leaf Tree[2000],*root;
short TrPnt[maxdepth],board[64],color[64];
short row[64],column[64],locn[8][8],Pindex[64],svalue[64];
short PieceList[2][16],PieceCnt[2],atak[2][64],PawnCnt[2][8];
short castld[2],kingmoved[2],mtl[2],pmtl[2],emtl[2],hung[2];
short c1,c2,*atk1,*atk2,*PC1,*PC2,EnemyKing;
short mate,post,opponent,computer,Sdepth,Awindow,Bwindow,dither;
long ResponseTime,ExtraTime,Level,et,et0,time0,cputimer,ft;
long NodeCnt,evrate,ETnodes,EvalNodes,HashCnt;
short quit,reverse,bothsides,hashflag,InChk,player,force,easy,beep;
short wking,bking,FROMsquare,TOsquare,timeout,Zscore,zwndw,xwndw,slk;
short INCscore;
short HasPawn[2],HasKnight[2],HasBishop[2],HasRook[2],HasQueen[2];
short ChkFlag[maxdepth],CptrFlag[maxdepth],PawnThreat[maxdepth];
short Pscore[maxdepth],Tscore[maxdepth],Threat[maxdepth];
struct GameRec GameList[240];
short GameCnt,Game50,epsquare,lpost,rcptr,contempt;
short MaxSearchDepth;
struct BookEntry *Book;
struct TimeControlRec TimeControl;
short TCflag,TCmoves,TCminutes,OperatorTime;
short otherside[3]={1,0,2};
short rank7[3]={6,1,0};
short map[64]=
   {0,1,2,3,4,5,6,7,
    0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
    0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,
    0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
    0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,
    0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,
    0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,
    0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77};
short unmap[120]=
   {0,1,2,3,4,5,6,7,-1,-1,-1,-1,-1,-1,-1,-1,
    8,9,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,
    16,17,18,19,20,21,22,23,-1,-1,-1,-1,-1,-1,-1,-1,
    24,25,26,27,28,29,30,31,-1,-1,-1,-1,-1,-1,-1,-1,
    32,33,34,35,36,37,38,39,-1,-1,-1,-1,-1,-1,-1,-1,
    40,41,42,43,44,45,46,47,-1,-1,-1,-1,-1,-1,-1,-1,
    48,49,50,51,52,53,54,55,-1,-1,-1,-1,-1,-1,-1,-1,
    56,57,58,59,60,61,62,63};
short Dcode[120]= 
   {0,1,1,1,1,1,1,1,0,0,0,0,0,0,0x0E,0x0F,
    0x10,0x11,0x12,0,0,0,0,0,0,0,0,0,0,0,0x0F,0x1F,
    0x10,0x21,0x11,0,0,0,0,0,0,0,0,0,0,0x0F,0,0,
    0x10,0,0,0x11,0,0,0,0,0,0,0,0,0x0F,0,0,0,
    0x10,0,0,0,0x11,0,0,0,0,0,0,0x0F,0,0,0,0,
    0x10,0,0,0,0,0x11,0,0,0,0,0x0F,0,0,0,0,0,
    0x10,0,0,0,0,0,0x11,0,0,0x0F,0,0,0,0,0,0,
    0x10,0,0,0,0,0,0,0x11};
short Stboard[64]=
   {rook,knight,bishop,queen,king,bishop,knight,rook,
    pawn,pawn,pawn,pawn,pawn,pawn,pawn,pawn,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    pawn,pawn,pawn,pawn,pawn,pawn,pawn,pawn,
    rook,knight,bishop,queen,king,bishop,knight,rook};
short Stcolor[64]=
   {white,white,white,white,white,white,white,white,
    white,white,white,white,white,white,white,white,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    black,black,black,black,black,black,black,black,
    black,black,black,black,black,black,black,black};
short sweep[7]= {false,false,false,true,true,true,false};
short Dpwn[3]={4,6,0};
short Dstart[7]={6,4,8,4,0,0,0};
short Dstop[7]={7,5,15,7,3,7,7};
short Dir[16]={1,0x10,-1,-0x10,0x0F,0x11,-0x0F,-0x11,
               0x0E,-0x0E,0x12,-0x12,0x1F,-0x1F,0x21,-0x21};
short Pdir[34]={0,0x38,0,0,0,0,0,0,0,0,0,0,0,0,0x02,0x35,
                0x38,0x35,0x02,0,0,0,0,0,0,0,0,0,0,0,0,0x02,
                0,0x02};
short pbit[7]={0,0x01,0x02,0x04,0x08,0x10,0x20};
unsigned short killr0[maxdepth],killr1[maxdepth],killr2[maxdepth];
unsigned short killr3[maxdepth],PrVar[maxdepth];
unsigned short PV,hint,Swag0,Swag1,Swag2,Swag3,Swag4;
unsigned short hashkey;
unsigned long hashbd;
struct hashval hashcode[2][7][64];
struct hashentry huge *ttable,*ptbl;
unsigned char history[8192];

short Mwpawn[64],Mbpawn[64],Mknight[2][64],Mbishop[2][64];
short Mking[2][64],Kfield[2][64];
short value[7]={0,valueP,valueN,valueB,valueR,valueQ,valueK};
short control[7]={0,ctlP,ctlN,ctlB,ctlR,ctlQ,ctlK};
short PassedPawn0[8]={0,60,80,120,200,360,600,800};
short PassedPawn1[8]={0,30,40,60,100,180,300,800};
short PassedPawn2[8]={0,15,25,35,50,90,140,800};
short PassedPawn3[8]={0,5,10,15,20,30,140,800};
short ISOLANI[8] = {-12,-16,-20,-24,-24,-20,-16,-12};
short BACKWARD[8] = {-6,-10,-15,-21,-28,-28,-28,-28};
short BMBLTY[14] = {-2,0,2,4,6,8,10,12,13,14,15,16,16,16};
short RMBLTY[14] = {0,2,4,6,8,10,11,12,13,14,14,14,14,14};
short Kthreat[16] = {0,-8,-20,-36,-52,-68,-80,-80,-80,-80,-80,-80,
                     -80,-80,-80,-80};
short KNIGHTPOST,KNIGHTSTRONG,BISHOPSTRONG,KATAK,KBNKsq;
short PEDRNK2B,PWEAKH,PADVNCM,PADVNCI,PAWNSHIELD,PDOUBLED,PBLOK;
short RHOPN,RHOPNX,KHOPN,KHOPNX,KSFTY;
short ATAKD,HUNGP,HUNGX,KCASTLD,KMOVD,XRAY,PINVAL;
short stage,stage2,Zwmtl,Zbmtl,Developed[2],PawnStorm;
short PawnBonus,BishopBonus,RookBonus;
short KingOpening[64]=
   {  0,  0, -4,-10,-10, -4,  0,  0,
     -4, -4, -8,-12,-12, -8, -4, -4,
    -12,-16,-20,-20,-20,-20,-16,-12,
    -16,-20,-24,-24,-24,-24,-20,-16,
    -16,-20,-24,-24,-24,-24,-20,-16,
    -12,-16,-20,-20,-20,-20,-16,-12,
     -4, -4, -8,-12,-12, -8, -4, -4,
      0,  0, -4,-10,-10, -4,  0,  0};
short KingEnding[64]=
   { 0, 6,12,18,18,12, 6, 0,
     6,12,18,24,24,18,12, 6,
    12,18,24,30,30,24,18,12,
    18,24,30,36,36,30,24,18,
    18,24,30,36,36,30,24,18,
    12,18,24,30,30,24,18,12,
     6,12,18,24,24,18,12, 6,
     0, 6,12,18,18,12, 6, 0};
short DyingKing[64]=
   { 0, 8,16,24,24,16, 8, 0,
     8,32,40,48,48,40,32, 8,
    16,40,56,64,64,56,40,16,
    24,48,64,72,72,64,48,24,
    24,48,64,72,72,64,48,24,
    16,40,56,64,64,56,40,16,
     8,32,40,48,48,40,32, 8,
     0, 8,16,24,24,16, 8, 0};
short KBNK[64]=
   {99,90,80,70,60,50,40,40,
    90,80,60,50,40,30,20,40,
    80,60,40,30,20,10,30,50,
    70,50,30,10, 0,20,40,60,
    60,40,20, 0,10,30,50,70,
    50,30,10,20,30,40,60,80,
    40,20,30,40,50,60,80,90,
    40,40,50,60,70,80,90,99};
short pknight[64]=
   { 0, 4, 8,10,10, 8, 4, 0,
     4, 8,16,20,20,16, 8, 4,
     8,16,24,28,28,24,16, 8,
    10,20,28,32,32,28,20,10,
    10,20,28,32,32,28,20,10,
     8,16,24,28,28,24,16, 8,
     4, 8,16,20,20,16, 8, 4,
     0, 4, 8,10,10, 8, 4, 0};
short pbishop[64]=
   {14,14,14,14,14,14,14,14,
    14,22,18,18,18,18,22,14,
    14,18,22,22,22,22,18,14,
    14,18,22,22,22,22,18,14,
    14,18,22,22,22,22,18,14,
    14,18,22,22,22,22,18,14,
    14,22,18,18,18,18,22,14,
    14,14,14,14,14,14,14,14};
short PawnAdvance[64]=
   { 0, 0, 0, 0, 0, 0, 0, 0,
     4, 4, 4, 0, 0, 4, 4, 4,
     6, 8, 2,10,10, 2, 8, 6,
     6, 8,12,16,16,12, 8, 6,
     8,12,16,24,24,16,12, 8,
    12,16,24,32,32,24,16,12,
    12,16,24,32,32,24,16,12,
     0, 0, 0, 0, 0, 0, 0, 0};
     

main(argc,argv)
int argc; char *argv[];
{
#ifdef MSDOS
  ttable = (struct hashentry huge *)farmalloc(ttblsz *
           (unsigned long)sizeof(struct hashentry));
#else
  ttable = (struct hashentry *)malloc(ttblsz *
           (unsigned long)sizeof(struct hashentry));
#endif
  Level = 0; TCflag = false; OperatorTime = 0;
  if (argc == 2) Level = atoi(argv[1]);
  if (argc == 3)
    {
      TCmoves = atoi(argv[1]); TCminutes = atoi(argv[2]); TCflag = true;
    }
  Initialize();
  NewGame();
#if (NEWMOVE > 0)
  Initialize_dist();
#if (NEWMOVE > 1)
  Initialize_moves();
#endif
#endif
  while (!(quit))
    {
      if (bothsides && !mate) SelectMove(opponent,1); else InputCommand();
      if (!(quit || mate || force)) SelectMove(computer,1);
    }
  ExitChess();
}



/* ............    INTERFACE ROUTINES    ........................... */

int VerifyMove(s,iop,mv)
char s[];
short iop;
unsigned short *mv;

/*
   Compare the string 's' to the list of legal moves available for the 
   opponent. If a match is found, make the move on the board. 
*/

{
static short pnt,tempb,tempc,tempsf,tempst,cnt;
static struct leaf xnode;
struct leaf *node;

  *mv = 0;
  if (iop == 2)
    {
      UnmakeMove(opponent,&xnode,&tempb,&tempc,&tempsf,&tempst);
      return(false);
    }
  cnt = 0;
  MoveList(opponent,2);
  pnt = TrPnt[2];
  while (pnt < TrPnt[3])
    {
      node = &Tree[pnt++];
      algbr(node->f,node->t,(short) node->flags & cstlmask);
      if (strcmp(s,mvstr1) == 0 || strcmp(s,mvstr2) == 0)
        {
          cnt++; xnode = *node;
        }
    }
  if (cnt == 1)
    {
      MakeMove(opponent,&xnode,&tempb,&tempc,&tempsf,&tempst);
      if (SqAtakd(PieceList[opponent][0],computer))
        {
          UnmakeMove(opponent,&xnode,&tempb,&tempc,&tempsf,&tempst);
          ShowMessage("Illegal Move!!");
          return(false);
        }
      else
        {
          if (iop == 1) return(true);
          if (xnode.flags & epmask) UpdateDisplay(0,0,1,0);
          else UpdateDisplay(xnode.f,xnode.t,0,xnode.flags & cstlmask);
          if (xnode.flags & cstlmask) Game50 = GameCnt;
          else if (board[xnode.t] == pawn || (xnode.flags & capture)) 
            Game50 = GameCnt;
          GameList[GameCnt].depth = GameList[GameCnt].score = 0;
          GameList[GameCnt].nodes = 0;
          ElapsedTime(1);
          GameList[GameCnt].time = (short)et;
          TimeControl.clock[opponent] -= et;
          --TimeControl.moves[opponent];
          *mv = (xnode.f << 8) + xnode.t;
          algbr(xnode.f,xnode.t,false);
          return(true);
        } 
    }
  if (cnt > 1) ShowMessage("Ambiguous Move!");
  return(false);
}


NewGame()

/*
   Reset the board and other variables to start a new game.
*/

{
short l,r,c,p;

  mate = quit = reverse = bothsides = post = false;
  hashflag = force = PawnStorm = false;
  beep = rcptr = easy = true;
  lpost =  NodeCnt = epsquare = et0 = 0;
  dither = 0;
  Awindow = 90;
  Bwindow = 90;
  xwndw = 90;
  MaxSearchDepth = 29;
  contempt = 0;
  GameCnt = -1; Game50 = 0;
  Zwmtl = Zbmtl = 0;
  Developed[white] = Developed[black] = false;
  castld[white] = castld[black] = false;
  kingmoved[white] = kingmoved[black] = 0;
  PawnThreat[0] = CptrFlag[0] = Threat[0] = false;
  Pscore[0] = 12000; Tscore[0] = 12000;
  opponent = white; computer = black;
  for (r = 0; r < 8; r++)
    for (c = 0; c < 8; c++)
      {
        l = 8*r+c; locn[r][c] = l;
        row[l] = r; column[l] = c;
        board[l] = Stboard[l]; color[l] = Stcolor[l];
      }
  for (c = white; c <= black; c++)
    for (p = pawn; p <= king; p++)
      for (l = 0; l < 64; l++)
        {
          hashcode[c][p][l].key = (unsigned short)rand();
          hashcode[c][p][l].bd = ((unsigned long)rand() << 16) +
                                 (unsigned long)rand();
        }
  ClrScreen();
  if (TCflag) SetTimeControl();
  else if (Level == 0) SelectLevel();
  UpdateDisplay(0,0,1,0);
  InitializeStats();
  time0 = time((long *)0);
  ElapsedTime(1);
  GetOpenings();
}


algbr(f,t,iscastle)
short f,t,iscastle;
{
  mvstr1[0] = cxx[column[f]]; mvstr1[1] = rxx[row[f]];
  mvstr1[2] = cxx[column[t]]; mvstr1[3] = rxx[row[t]];
  mvstr2[0] = qxx[board[f]];
  mvstr2[1] = mvstr1[2]; mvstr2[2] = mvstr1[3];
  mvstr1[4] = '\0'; mvstr2[3] = '\0';
  if (iscastle)
    if (t > f) strcpy(mvstr2,"o-o");
    else strcpy(mvstr2,"o-o-o");
}


/* ............    MOVE GENERATION & SEARCH ROUTINES    .............. */

SelectMove(side,iop)
short side,iop;

/*
   Select a move by calling function search() at progressively deeper 
   ply until time is up or a mate or draw is reached. An alpha-beta 
   window of -90 to +90 points is set around the score returned from the 
   previous iteration. If Sdepth != 0 then the program has correctly 
   predicted the opponents move and the search will start at a depth of 
   Sdepth+1 rather than a depth of 1. 
*/

{
static short i,alpha,beta,score,tempb,tempc,tempsf,tempst,xside,rpt;

  timeout = false;
  xside = otherside[side];
  if (iop != 2) player = side;
  if (TCflag)
    {
      if (((TimeControl.moves[side] + 3) - OperatorTime) != 0)
	ResponseTime = (TimeControl.clock[side]) /
  	               (TimeControl.moves[side] + 3) -
                       OperatorTime;
      else ResponseTime = 0;
      ResponseTime += (ResponseTime*TimeControl.moves[side])/(2*TCmoves+1);
    }
  else ResponseTime = Level;
  if (iop == 2) ResponseTime = 999;
  if (Sdepth > 0 && root->score > Zscore-zwndw) ResponseTime -= ft;
  else if (ResponseTime < 1) ResponseTime = 1;
  ExtraTime = 0;
  ExaminePosition();
  ScorePosition(side,&score);
  ShowSidetomove();
  
  if (Sdepth == 0)
  {
    ZeroTTable();
    SearchStartStuff(side);
    for (i = 0; i < 8192; i++) history[i] = 0;
    FROMsquare = TOsquare = -1;
    PV = 0;
    if (iop != 2) hint = 0;
    for (i = 0; i < maxdepth; i++)
     PrVar[i] = killr0[i] = killr1[i] = killr2[i] = killr3[i] = 0;
    alpha = score-90; beta = score+90;
    rpt = 0;
    TrPnt[1] = 0; root = &Tree[0];
    MoveList(side,1);
    for (i = TrPnt[1]; i < TrPnt[2]; i++) pick(i,TrPnt[2]-1);
    if (Book != NULL) OpeningBook();
    if (Book != NULL) timeout = true;
    NodeCnt = ETnodes = EvalNodes = HashCnt = 0;
    Zscore = 0; zwndw = 20;
  }
  
  while (!timeout && Sdepth < MaxSearchDepth)
    {
      Sdepth++;
      ShowDepth(' ');
      score = search(side,1,Sdepth,alpha,beta,PrVar,&rpt);
      for (i = 1; i <= Sdepth; i++) killr0[i] = PrVar[i];
      if (score < alpha)
        {
          ShowDepth('-');
          ExtraTime = 10*ResponseTime;
          ZeroTTable();
          score = search(side,1,Sdepth,-9000,beta,PrVar,&rpt);
        }
      if (score > beta && !(root->flags & exact))
        {
          ShowDepth('+');
          ExtraTime = 0;
          ZeroTTable();
          score = search(side,1,Sdepth,alpha,9000,PrVar,&rpt);
        }
      score = root->score;
      if (!timeout)
        for (i = TrPnt[1]+1; i < TrPnt[2]; i++) pick(i,TrPnt[2]-1);
      ShowResults(score,PrVar,'.');
      for (i = 1; i <= Sdepth; i++) killr0[i] = PrVar[i];
      if (score > Zscore-zwndw && score > Tree[1].score+250) ExtraTime = 0;
      else if (score > Zscore-3*zwndw) ExtraTime = ResponseTime;
      else ExtraTime = 3*ResponseTime;
      if (root->flags & exact) timeout = true;
      if (Tree[1].score < -9000) timeout = true;
      if (4*et > 2*ResponseTime + ExtraTime) timeout = true;
      if (!timeout)
        {
          Tscore[0] = score;
          if (Zscore == 0) Zscore = score;
          else Zscore = (Zscore+score)/2;
        }
      zwndw = 20+abs(Zscore/12);
      beta = score + Bwindow;
      if (Zscore < score) alpha = Zscore - Awindow - zwndw;
      else alpha = score - Awindow - zwndw;
    }

  score = root->score;
  if (rpt >= 2 || score < -12000) root->flags |= draw;
  if (iop == 2) return(0);
  if (Book == NULL) hint = PrVar[2];
  ElapsedTime(1);

  if (score > -9999 && rpt <= 2)
    {
      MakeMove(side,root,&tempb,&tempc,&tempsf,&tempst);
      algbr(root->f,root->t,(short) root->flags & cstlmask);
    }
  else mvstr1[0] = '\0';
  OutputMove();
  if (score == -9999 || score == 9998) mate = true;
  if (mate) hint = 0;
  if (root->flags & cstlmask) Game50 = GameCnt;
  else if (board[root->t] == pawn || (root->flags & capture)) 
    Game50 = GameCnt;
  GameList[GameCnt].score = score;
  GameList[GameCnt].nodes = NodeCnt;
  GameList[GameCnt].time = (short)et;
  GameList[GameCnt].depth = Sdepth;
  if (TCflag)
    {
      TimeControl.clock[side] -= (et + OperatorTime);
      if (--TimeControl.moves[side] == 0) SetTimeControl();
    }
  if ((root->flags & draw) && bothsides) quit = true;
  if (GameCnt > 238) quit = true;
  player = xside;
  Sdepth = 0;
  fflush(stdin);
  return(0);
}


OpeningBook()

/*
   Go thru each of the opening lines of play and check for a match with 
   the current game listing. If a match occurs, generate a random number. 
   If this number is the largest generated so far then the next move in 
   this line becomes the current "candidate". After all lines are 
   checked, the candidate move is put at the top of the Tree[] array and 
   will be played by the program. Note that the program does not handle 
   book transpositions. 
*/

{
short j,pnt;
unsigned short m,*mp;
unsigned r,r0;
struct BookEntry *p;

  srand((unsigned)time0);
  r0 = m = 0;
  p = Book;
  while (p != NULL)
    {
      mp = p->mv;
      for (j = 0; j <= GameCnt; j++)
        if (GameList[j].gmove != *(mp++)) break;
      if (j > GameCnt)
        if ((r=rand()) > r0)
          {
            r0 = r; m = *mp;
            hint = *(++mp);
          }
      p = p->next;
    }
    
  for (pnt = TrPnt[1]; pnt < TrPnt[2]; pnt++)
    if ((Tree[pnt].f<<8) + Tree[pnt].t == m) Tree[pnt].score = 0;
  pick(TrPnt[1],TrPnt[2]-1);
  if (Tree[TrPnt[1]].score < 0) Book = NULL;
}


#define UpdateSearchStatus\
{\
  if (post) ShowCurrentMove(pnt,node->f,node->t);\
  if (pnt > TrPnt[1])\
    {\
      d = best-Zscore; e = best-node->score;\
      if (best < alpha) ExtraTime = 10*ResponseTime;\
      else if (d > -zwndw && e > 4*zwndw) ExtraTime = -ResponseTime/3;\
      else if (d > -zwndw) ExtraTime = 0;\
      else if (d > -3*zwndw) ExtraTime = ResponseTime;\
      else if (d > -9*zwndw) ExtraTime = 3*ResponseTime;\
      else ExtraTime = 5*ResponseTime;\
    }\
}

int search(side,ply,depth,alpha,beta,bstline,rpt)
short side,ply,depth,alpha,beta,*rpt;
unsigned short bstline[];

/*
   Perform an alpha-beta search to determine the score for the current 
   board position. If depth <= 0 only capturing moves, pawn promotions 
   and responses to check are generated and searched, otherwise all 
   moves are processed. The search depth is modified for check evasions, 
   certain re-captures and threats. Extensions may continue for up to 11 
   ply beyond the nominal search depth. 
*/

#define prune (cf && score+node->score < alpha)
#define ReCapture (rcptr && score > alpha && score < beta &&\
                   ply > 2 && CptrFlag[ply-1] && CptrFlag[ply-2])
#define MateThreat (ply < Sdepth+4 && ply > 4 &&\
                    ChkFlag[ply-2] && ChkFlag[ply-4] &&\
                    ChkFlag[ply-2] != ChkFlag[ply-4])

{
register short j,pnt;
short best,tempb,tempc,tempsf,tempst;
short xside,pbst,d,e,cf,score,rcnt;
unsigned short mv,nxtline[maxdepth];
struct leaf *node,tmp;

  NodeCnt++;
  xside = otherside[side];
  if (depth < 0) depth = 0;
  
  if (ply <= Sdepth+3) repetition(rpt); else *rpt = 0;
  if (*rpt >= 2) return(0);

  score = evaluate(side,xside,ply,alpha,beta);
  if (score > 9000)
    {
      bstline[ply] = 0;
      return(score);
    }
                
  if (depth > 0)
    {
      if (InChk || PawnThreat[ply-1] || ReCapture) ++depth;
    }
  else
    {
      if (score >= alpha &&
         (InChk || PawnThreat[ply-1] || Threat[ply-1])) ++depth;
      else if (score <= beta && MateThreat) ++depth;
    }
    
  if (depth > 0 && hashflag && ply > 1)
    {
      ProbeTTable(side,depth,&alpha,&beta,&score);
      bstline[ply] = PV;
      bstline[ply+1] = 0;
      if (beta == -20000) return(score);
      if (alpha > beta) return(alpha);
    }
    
  if (Sdepth == 1) d = 7; else d = 11;
  if (ply > Sdepth+d || (depth < 1 && score > beta)) return(score);

  if (ply > 1)
    if (depth > 0) MoveList(side,ply);
    else CaptureList(side,xside,ply);
    
  if (TrPnt[ply] == TrPnt[ply+1]) return(score);
    
  cf = (depth < 1 && ply > Sdepth+1 && !ChkFlag[ply-2] && !slk);

  if (depth > 0) best = -12000; else best = score;
  if (best > alpha) alpha = best;
  
  for (pnt = pbst = TrPnt[ply];
       pnt < TrPnt[ply+1] && best <= beta;
       pnt++)
    {
      if (ply > 1) pick(pnt,TrPnt[ply+1]-1);
      node = &Tree[pnt];
      mv = (node->f << 8) + node->t;
      nxtline[ply+1] = 0;
      
      if (prune) break;
      if (ply == 1) UpdateSearchStatus;

      if (!(node->flags & exact))
        {
          MakeMove(side,node,&tempb,&tempc,&tempsf,&tempst);
          CptrFlag[ply] = (node->flags & capture);
          PawnThreat[ply] = (node->flags & pwnthrt);
          Tscore[ply] = node->score;
          PV = node->reply;
          node->score = -search(xside,ply+1,depth-1,-beta,-alpha,
                                nxtline,&rcnt);
          if (abs(node->score) > 9000) node->flags |= exact;
          else if (rcnt == 1) node->score /= 2;
          if (rcnt >= 2 || GameCnt-Game50 > 99 ||
             (node->score == 9999-ply && !ChkFlag[ply]))
            {
              node->flags |= draw; node->flags |= exact;
              if (side == computer) node->score = contempt;
              else node->score = -contempt;
            }
          node->reply = nxtline[ply+1];
          UnmakeMove(side,node,&tempb,&tempc,&tempsf,&tempst);
        }
      if (node->score > best && !timeout)
        {
          if (depth > 0)
            if (node->score > alpha && !(node->flags & exact))
              node->score += depth;
          best = node->score; pbst = pnt;
          if (best > alpha) alpha = best;
          for (j = ply+1; nxtline[j] > 0; j++) bstline[j] = nxtline[j];
          bstline[j] = 0;
          bstline[ply] = mv;
          if (ply == 1)
            {
              if (best == alpha)
                {
                  tmp = Tree[pnt];
                  for (j = pnt-1; j >= 0; j--) Tree[j+1] = Tree[j];
                  Tree[0] = tmp;
                  pbst = 0;
                }
              if (Sdepth > 2)
                if (best > beta) ShowResults(best,bstline,'+');
                else if (best < alpha) ShowResults(best,bstline,'-');
                else ShowResults(best,bstline,'&');
            }
        }
      if (NodeCnt > ETnodes) ElapsedTime(0);
      if (timeout) return(-Tscore[ply-1]);
    }
    
  node = &Tree[pbst];
  mv = (node->f<<8) + node->t;
  if (hashflag && ply <= Sdepth && *rpt == 0 && best == alpha)
    PutInTTable(side,best,depth,alpha,beta,mv);
  if (depth > 0)
    {
      j = (node->f<<6) + node->t; if (side == black) j |= 0x1000;
      if (history[j] < 150) history[j] += 2*depth;
      if (node->t != (GameList[GameCnt].gmove & 0xFF))
        if (best <= beta) killr3[ply] = mv;
        else if (mv != killr1[ply])
          {
            killr2[ply] = killr1[ply];
            killr1[ply] = mv;
          }
      if (best > 9000) killr0[ply] = mv; else killr0[ply] = 0;
    }
  return(best);
}


evaluate(side,xside,ply,alpha,beta)
short side,xside,ply,alpha,beta;

/*
   Compute an estimate of the score by adding the positional score from 
   the previous ply to the material difference. If this score falls 
   inside a window which is 180 points wider than the alpha-beta window 
   (or within a 50 point window during quiescence search) call 
   ScorePosition() to determine a score, otherwise return the estimated 
   score. If one side has only a king and the other either has no pawns 
   or no pieces then the function ScoreLoneKing() is called. 
*/

{
short s,evflag;

  hung[white] = hung[black] = 0;
  slk = ((mtl[white] == valueK && (pmtl[black] == 0 || emtl[black] == 0)) ||
         (mtl[black] == valueK && (pmtl[white] == 0 || emtl[white] == 0)));
  s = -Pscore[ply-1] + mtl[side] - mtl[xside];
  s -= INCscore;
  
  if (slk) evflag = false;
  else evflag = 
     (ply == 1 || ply < Sdepth ||
     ((ply == Sdepth+1 || ply == Sdepth+2) &&
      (s > alpha-xwndw && s < beta+xwndw)) ||
     (ply > Sdepth+2 && s >= alpha-25 && s <= beta+25));
    
  if (evflag)
    {
      EvalNodes++;
      ataks(side,atak[side]);
      if (atak[side][PieceList[xside][0]] > 0) return(10001-ply);
      ataks(xside,atak[xside]);
      InChk = (atak[xside][PieceList[side][0]] > 0);
      ScorePosition(side,&s);
    }
  else
    {
      if (SqAtakd(PieceList[xside][0],side)) return(10001-ply);
      InChk = SqAtakd(PieceList[side][0],xside);
      if (slk) ScoreLoneKing(side,&s);
    }
    
  Pscore[ply] = s - mtl[side] + mtl[xside];
  if (InChk) ChkFlag[ply-1] = Pindex[TOsquare];
  else ChkFlag[ply-1] = 0;
  Threat[ply-1] = (hung[side] > 1 && ply == Sdepth+1);
  return(s);
}


ProbeTTable(side,depth,alpha,beta,score)
short side,depth,*alpha,*beta,*score;

/* 
   Look for the current board position in the transposition table.
*/

{
short hindx;
  if (side == white) hashkey |= 1; else hashkey &= 0xFFFE;
  hindx = (hashkey & (ttblsz-1));
  ptbl = (ttable + hindx);
  if (ptbl->depth >= depth && ptbl->hashbd == hashbd)
    {
      HashCnt++;
      PV = ptbl->mv;
      if (ptbl->flags & truescore)
        {
          *score = ptbl->score;
          *beta = -20000;
          return(true);
        }
/*
      else if (ptbl->flags & upperbound)
        {
          if (ptbl->score < *beta) *beta = ptbl->score+1;
        }
*/
      else if (ptbl->flags & lowerbound)
        {
          if (ptbl->score > *alpha) *alpha = ptbl->score-1;
        }
    }
  return(false);
}


PutInTTable(side,score,depth,alpha,beta,mv)
short side,score,depth,alpha,beta;
unsigned short mv;

/*
   Store the current board position in the transposition table.
*/

{
short hindx;
  if (side == white) hashkey |= 1; else hashkey &= 0xFFFE;
  hindx = (hashkey & (ttblsz-1));
  ptbl = (ttable + hindx);
  ptbl->hashbd = hashbd;
  ptbl->depth = depth;
  ptbl->score = score; 
  ptbl->mv = mv;
  ptbl->flags = 0;
  if (score < alpha) ptbl->flags |= upperbound;
  else if (score > beta) ptbl->flags |= lowerbound;
  else ptbl->flags |= truescore;
}


ZeroTTable()
{
int i;
  if (hashflag)
    for (i = 0; i < ttblsz; i++)
      {
        ptbl = (ttable + i);
        ptbl->depth = 0;
      }
}


MoveList(side,ply)
short side,ply;

/*
   Fill the array Tree[] with all available moves for side to play. Array 
   TrPnt[ply] contains the index into Tree[] of the first move at a ply. 
*/
    
{
register short i;
short xside,f;

  xside = otherside[side];
  if (PV == 0) Swag0 = killr0[ply]; else Swag0 = PV;
  Swag1 = killr1[ply]; Swag2 = killr2[ply];
  Swag3 = killr3[ply]; Swag4 = 0;
  if (ply > 2) Swag4 = killr1[ply-2];
  TrPnt[ply+1] = TrPnt[ply];
  Dstart[pawn] = Dpwn[side]; Dstop[pawn] = Dstart[pawn] + 1;
  for (i = PieceCnt[side]; i >= 0; i--)
    GenMoves(ply,PieceList[side][i],side,xside);
  if (kingmoved[side] == 0 && !castld[side])
    {
      f = PieceList[side][0];
      if (castle(side,f,f+2,0))
        {
          LinkMove(ply,f,f+2,xside);
          Tree[TrPnt[ply+1]-1].flags |= cstlmask;
        }
      if (castle(side,f,f-2,0))
        {
          LinkMove(ply,f,f-2,xside);
          Tree[TrPnt[ply+1]-1].flags |= cstlmask;
        }
    }
}

#if (NEWMOVE < 11)
GenMoves(ply,sq,side,xside)
short ply,sq,side,xside;

/*
   Generate moves for a piece. The from square is mapped onto a special  
   board and offsets (taken from array Dir[]) are added to the mapped 
   location. The newly generated square is tested to see if it falls off 
   the board by ANDing the square with 88 HEX. Legal moves are linked 
   into the tree. 
*/
    
{
register short m,u,d;
short i,m0,piece; 

  piece = board[sq]; m0 = map[sq];
  if (sweep[piece])
    for (i = Dstart[piece]; i <= Dstop[piece]; i++)
      {
        d = Dir[i]; m = m0+d;
        while (!(m & 0x88))
          {
            u = unmap[m];
            if (color[u] == neutral)
              {
                LinkMove(ply,sq,u,xside);
                m += d;
              }
            else if (color[u] == xside)
              {
                LinkMove(ply,sq,u,xside);
                break;
              }
            else break;
          }
      }
  else if (piece == pawn)
    {
      if (side == white && color[sq+8] == neutral)
        {
          LinkMove(ply,sq,sq+8,xside);
          if (row[sq] == 1)
            if (color[sq+16] == neutral)
              LinkMove(ply,sq,sq+16,xside);
        }
      else if (side == black && color[sq-8] == neutral)
        {
          LinkMove(ply,sq,sq-8,xside);
          if (row[sq] == 6)
            if (color[sq-16] == neutral)
              LinkMove(ply,sq,sq-16,xside);
        }
      for (i = Dstart[piece]; i <= Dstop[piece]; i++)
        if (!((m = m0+Dir[i]) & 0x88))
          {
            u = unmap[m];
            if (color[u] == xside || u == epsquare)
              LinkMove(ply,sq,u,xside);
          }
    }
  else
    {
      for (i = Dstart[piece]; i <= Dstop[piece]; i++)
        if (!((m = m0+Dir[i]) & 0x88))
          {
            u = unmap[m];
            if (color[u] != side) LinkMove(ply,sq,u,xside);
          }
    }
}
#endif

LinkMove(ply,f,t,xside)
short ply,f,t,xside;

/*
   Add a move to the tree.  Assign a bonus to order the moves
   as follows:
     1. Principle variation
     2. Capture of last moved piece
     3. Other captures (major pieces first)
     4. Killer moves
     5. "history" killers    
*/

{
register short s,z;
unsigned short mv;
struct leaf *node;

  node = &Tree[TrPnt[ply+1]];
  ++TrPnt[ply+1];
  node->flags = node->reply = 0;
  node->f = f; node->t = t;
  mv = (f<<8) + t;
  s = 0;
  if (mv == Swag0) s = 2000;
  else if (mv == Swag1) s = 60;
  else if (mv == Swag2) s = 50;
  else if (mv == Swag3) s = 40;
  else if (mv == Swag4) s = 30;
  if (color[t] != neutral)
    {
      node->flags |= capture;
      if (t == TOsquare) s += 500;
      s += value[board[t]] - board[f];
    }
  if (board[f] == pawn)
    if (row[t] == 0 || row[t] == 7)
      {
        node->flags |= promote;
        s += 800;
      }
    else if (row[t] == 1 || row[t] == 6)
      {
        node->flags |= pwnthrt;
        s += 600;
      }
    else if (t == epsquare) node->flags |= epmask;
  z = (f<<6) + t; if (xside == white) z |= 0x1000;
  s += history[z];
  node->score = s - 20000;
}

#if (NEWMOVE < 10)
CaptureList(side,xside,ply)
short side,xside,ply;

/*
    Generate captures and Pawn promotions only.
*/

#define LinkCapture\
{\
  node->f = sq; node->t = u;\
  node->reply = 0;\
  node->flags = capture;\
  node->score = value[board[u]] + svalue[board[u]] - piece;\
  if (piece == pawn && (u < 8 || u > 55))\
    {\
      node->flags |= promote;\
      node->score = valueQ;\
    }\
  ++node;\
  ++TrPnt[ply+1];\
}

{
register short m,u;
short d,sq,i,j,j1,j2,m0,r7,d0,piece,*PL;
struct leaf *node;

  TrPnt[ply+1] = TrPnt[ply];
  node = &Tree[TrPnt[ply]];
  Dstart[pawn] = Dpwn[side]; Dstop[pawn] = Dstart[pawn] + 1;
  if (side == white)
    {
      r7 = 6; d0 = 8;
    }
  else
    {
      r7 = 1; d0 = -8;
    }
  PL = PieceList[side];
  for (i = 0; i <= PieceCnt[side]; i++)
    { 
      sq = PL[i];
      m0 = map[sq]; piece = board[sq];
      j1 = Dstart[piece]; j2 = Dstop[piece];
      if (sweep[piece])
        for (j = j1; j <= j2; j++)
          {
            d = Dir[j]; m = m0+d;
            while (!(m & 0x88))
              {
                u = unmap[m];
                if (color[u] == neutral) m += d;
                else
                  {
                    if (color[u] == xside) LinkCapture;
                    break;
                  }
              }
          }
      else
        {
          for (j = j1; j <= j2; j++)
            if (!((m = m0+Dir[j]) & 0x88))
              {
                u = unmap[m];
                if (color[u] == xside) LinkCapture;
              }
          if (piece == pawn && row[sq] == r7)
            {
              u = sq+d0;
              if (color[u] == neutral) LinkCapture;
            }
        }
    }
}
#endif
  
int castle(side,kf,kt,iop)
short side,kf,kt,iop;

/*
   Make or Unmake a castling move.
*/

{
short rf,rt,d,t0,xside;

  xside = otherside[side];
  if (kt > kf)
    {
      rf = kf+3; rt = kt-1; d = 1;
    }
  else
    {
      rf = kf-4; rt = kt+1; d = -1;
    }
  if (iop == 0)
    {
      if (board[kf] != king || board[rf] != rook || color[rf] != side)
        return(false);
      if (color[kt] != neutral || color[rt] != neutral) return(false);
      if (d == -1 && color[kt+d] != neutral) return(false);
      if (SqAtakd(kf,xside)) return(false);
      if (SqAtakd(kt,xside)) return(false);
      if (SqAtakd(kf+d,xside)) return(false);
    }
  else
    {
      if (iop == 1) castld[side] = true; else castld[side] = false;
      if (iop == 2)
        {
          t0 = kt; kt = kf; kf = t0;
          t0 = rt; rt = rf; rf = t0;
        }
      board[kt] = king; color[kt] = side; Pindex[kt] = 0;
      board[kf] = no_piece; color[kf] = neutral;
      board[rt] = rook; color[rt] = side; Pindex[rt] = Pindex[rf];
      board[rf] = no_piece; color[rf] = neutral;
      PieceList[side][Pindex[kt]] = kt;
      PieceList[side][Pindex[rt]] = rt;
      if (hashflag)
        {
          UpdateHashbd(side,king,kf,kt);
          UpdateHashbd(side,rook,rf,rt);
        }
    }
  return(true);
}


EnPassant(xside,f,t,iop)
short xside,f,t,iop;

/*
   Make or unmake an en passant move.
*/

{
short l;
  if (t > f) l = t-8; else l = t+8;
  if (iop == 1)
    {
      board[l] = no_piece; color[l] = neutral;
    }
  else 
    {
      board[l] = pawn; color[l] = xside;
    }
  InitializeStats();
}


MakeMove(side,node,tempb,tempc,tempsf,tempst)
short side,*tempc,*tempb,*tempsf,*tempst;
struct leaf *node;

/*
   Update Arrays board[], color[], and Pindex[] to reflect the new board 
   position obtained after making the move pointed to by node. Also 
   update miscellaneous stuff that changes when a move is made. 
*/
    
{
register short f,t;
short xside,ct,cf;

  xside = otherside[side];
  f = node->f; t = node->t; epsquare = -1;
  FROMsquare = f; TOsquare = t;
  INCscore = 0;
  GameList[++GameCnt].gmove = (f<<8) + t;
  if (node->flags & cstlmask)
    {
      GameList[GameCnt].piece = no_piece;
      GameList[GameCnt].color = side;
      castle(side,f,t,1);
    }
  else
    {
      *tempc = color[t]; *tempb = board[t];
      *tempsf = svalue[f]; *tempst = svalue[t];
      GameList[GameCnt].piece = *tempb;
      GameList[GameCnt].color = *tempc;
      if (*tempc != neutral)
        {
          UpdatePieceList(*tempc,t,1);
          if (*tempb == pawn) --PawnCnt[*tempc][column[t]];
          if (board[f] == pawn)
            {
              --PawnCnt[side][column[f]];
              ++PawnCnt[side][column[t]];
              cf = column[f]; ct = column[t];
              if (PawnCnt[side][ct] > 1+PawnCnt[side][cf])
                INCscore -= 15;
              else if (PawnCnt[side][ct] < 1+PawnCnt[side][cf])
                INCscore += 15;
              else if (ct == 0 || ct == 7 || PawnCnt[side][ct+ct-cf] == 0)
                INCscore -= 15;
            }
          mtl[xside] -= value[*tempb];
          if (*tempb == pawn) pmtl[xside] -= valueP;
          if (hashflag) UpdateHashbd(xside,*tempb,-1,t);
          INCscore += *tempst;
        }
      color[t] = color[f]; board[t] = board[f]; svalue[t] = svalue[f];
      Pindex[t] = Pindex[f]; PieceList[side][Pindex[t]] = t;
      color[f] = neutral; board[f] = no_piece;
      if (board[t] == pawn)
        if (t-f == 16) epsquare = f+8;
        else if (f-t == 16) epsquare = f-8;
      if (node->flags & promote)
        {
          board[t] = queen;
          --PawnCnt[side][column[t]];
          mtl[side] += valueQ - valueP;
          pmtl[side] -= valueP;
          HasQueen[side] = true;
          if (hashflag)
            {
              UpdateHashbd(side,pawn,f,-1);
              UpdateHashbd(side,queen,f,-1);
            }
          INCscore -= *tempsf;
        } 
      if (board[t] == king) ++kingmoved[side];
      if (node->flags & epmask) EnPassant(xside,f,t,1);
      else if (hashflag) UpdateHashbd(side,board[t],f,t);
    }
}


UnmakeMove(side,node,tempb,tempc,tempsf,tempst)
short side,*tempc,*tempb,*tempsf,*tempst;
struct leaf *node;

/*
   Take back a move.
*/

{
register short f,t;
short xside;

  xside = otherside[side];
  f = node->f; t = node->t; epsquare = -1;
  GameCnt--;
  if (node->flags & cstlmask) castle(side,f,t,2);
  else
    {
      color[f] = color[t]; board[f] = board[t]; svalue[f] = *tempsf;
      Pindex[f] = Pindex[t]; PieceList[side][Pindex[f]] = f;
      color[t] = *tempc; board[t] = *tempb; svalue[t] = *tempst;
      if (node->flags & promote)
        {
          board[f] = pawn;
          ++PawnCnt[side][column[t]];
          mtl[side] += valueP - valueQ;
          pmtl[side] += valueP;
          if (hashflag)
            {
              UpdateHashbd(side,queen,-1,t);
              UpdateHashbd(side,pawn,-1,t);
            }
        } 
      if (*tempc != neutral)
        {
          UpdatePieceList(*tempc,t,2);
          if (*tempb == pawn) ++PawnCnt[*tempc][column[t]];
          if (board[f] == pawn)
            {
              --PawnCnt[side][column[t]];
              ++PawnCnt[side][column[f]];
            }
          mtl[xside] += value[*tempb];
          if (*tempb == pawn) pmtl[xside] += valueP;
          if (hashflag) UpdateHashbd(xside,*tempb,-1,t);
        }
      if (board[f] == king) --kingmoved[side];
      if (node->flags & epmask) EnPassant(xside,f,t,2);
      else if (hashflag) UpdateHashbd(side,board[f],f,t);
    }
}


UpdateHashbd(side,piece,f,t)
short side,piece,f,t;

/*
   hashbd contains a 32 bit "signature" of the board position. hashkey 
   contains a 16 bit code used to address the hash table. When a move is 
   made, XOR'ing the hashcode of moved piece on the from and to squares 
   with the hashbd and hashkey values keeps things current. 
*/

{
  if (f >= 0)
    {
      hashbd ^= hashcode[side][piece][f].bd;
      hashkey ^= hashcode[side][piece][f].key;
    }
  if (t >= 0)
    {
      hashbd ^= hashcode[side][piece][t].bd;
      hashkey ^= hashcode[side][piece][t].key;
    }
}


UpdatePieceList(side,sq,iop)
short side,sq,iop;

/*
   Update the PieceList and Pindex arrays when a piece is captured or 
   when a capture is unmade. 
*/

{
register short i;
  if (iop == 1)
    {
      PieceCnt[side]--;
      for (i = Pindex[sq]; i <= PieceCnt[side]; i++)
        {
          PieceList[side][i] = PieceList[side][i+1];
          Pindex[PieceList[side][i]] = i;
        }
    }
  else
    {
      PieceCnt[side]++;
      PieceList[side][PieceCnt[side]] = sq;
      Pindex[sq] = PieceCnt[side];
    }
}


InitializeStats()

/*
   Scan thru the board seeing what's on each square. If a piece is found, 
   update the variables PieceCnt, PawnCnt, Pindex and PieceList. Also 
   determine the material for each side and set the hashkey and hashbd 
   variables to represent the current board position. Array 
   PieceList[side][indx] contains the location of all the pieces of 
   either side. Array Pindex[sq] contains the indx into PieceList for a 
   given square. 
*/

{
register short i,sq;
  epsquare = -1;
  for (i = 0; i < 8; i++)
    PawnCnt[white][i] = PawnCnt[black][i] = 0;
  mtl[white] = mtl[black] = pmtl[white] = pmtl[black] = 0;
  PieceCnt[white] = PieceCnt[black] = 0;
  hashbd = hashkey = 0;
  for (sq = 0; sq < 64; sq++)
    if (color[sq] != neutral)
      {
        mtl[color[sq]] += value[board[sq]];
        if (board[sq] == pawn)
          {
            pmtl[color[sq]] += valueP;
            ++PawnCnt[color[sq]][column[sq]];
          }
        if (board[sq] == king) Pindex[sq] = 0;
          else Pindex[sq] = ++PieceCnt[color[sq]];
        PieceList[color[sq]][Pindex[sq]] = sq;
        hashbd ^= hashcode[color[sq]][board[sq]][sq].bd;
        hashkey ^= hashcode[color[sq]][board[sq]][sq].key;
      }
}


pick(p1,p2)
short p1,p2;

/*  
   Find the best move in the tree between indexes p1 and p2. Swap the 
   best move into the p1 element. 
*/

{
register short p,s;
short p0,s0;
struct leaf temp;

  s0 = Tree[p1].score; p0 = p1;
  for (p = p1+1; p <= p2; p++)
    if ((s = Tree[p].score) > s0)
      {
        s0 = s; p0 = p;
      }
  if (p0 != p1)
    {
      temp = Tree[p1]; Tree[p1] = Tree[p0]; Tree[p0] = temp;
    }
}


repetition(cnt)
short *cnt;

/*
    Check for draw by threefold repetition.
*/

{
register short i,c;
short f,t,b[64];
unsigned short m;
  *cnt = c = 0;
  if (GameCnt > Game50+3)
    {
/*
      memset((char *)b,0,64*sizeof(short));
*/
      for (i = 0; i < 64; b[i++] = 0);
      for (i = GameCnt; i > Game50; i--)
        {
          m = GameList[i].gmove; f = m>>8; t = m & 0xFF;
          if (++b[f] == 0) c--; else c++;
          if (--b[t] == 0) c--; else c++;
          if (c == 0) (*cnt)++;
        }
    }
}

#if (NEWMOVE < 3)
int SqAtakd(sq,side)
short sq,side;

/*
  See if any piece with color 'side' ataks sq.  First check for pawns
  or king, then try other pieces. Array Dcode is used to check for
  knight attacks or R,B,Q co-linearity.  
*/

{
register short m,d;
short i,m0,m1,loc,piece,*PL;

  m1 = map[sq];
  if (side == white) m = m1-0x0F; else m = m1+0x0F;
  if (!(m & 0x88))
    if (board[unmap[m]] == pawn && color[unmap[m]] == side) return(true);
  if (side == white) m = m1-0x11; else m = m1+0x11;
  if (!(m & 0x88))
    if (board[unmap[m]] == pawn && color[unmap[m]] == side) return(true);
  if (distance(sq,PieceList[side][0]) == 1) return(true);
  
  PL = PieceList[side];
  for (i = 1; i <= PieceCnt[side]; i++)
    {
      loc = PL[i]; piece = board[loc];
      if (piece == pawn) continue;
      m0 = map[loc]; d = Dcode[abs(m1-m0)];
      if (d == 0 || (Pdir[d] & pbit[piece]) == 0) continue;
      if (piece == knight) return(true);
      else
        {
          if (m1 < m0) d = -d;
          for (m = m0+d; m != m1; m += d)
            if (color[unmap[m]] != neutral) break;
          if (m == m1) return(true);
        }
    }
  return(false);
}
#endif

#if (NEWMOVE < 2)
ataks(side,a)
short side,*a;

/*
    Fill array atak[][] with info about ataks to a square.  Bits 8-15
    are set if the piece (king..pawn) ataks the square. Bits 0-7
    contain a count of total ataks to the square.
*/

{
register short u,m;
short d,c,j,j1,j2,piece,i,m0,sq,*PL;
 
/*
  memset((char *)a,0,64*sizeof(short));
*/
  for (u = 0; u < 64; a[u++] = 0);
  Dstart[pawn] = Dpwn[side]; Dstop[pawn] = Dstart[pawn] + 1;
  PL = PieceList[side];
  for (i = 0; i <= PieceCnt[side]; i++)
    {
      sq = PL[i];
      m0 = map[sq];
      piece = board[sq];
      c = control[piece]; j1 = Dstart[piece]; j2 = Dstop[piece];
      if (sweep[piece])
        for (j = j1; j <= j2; j++)
          {
            d = Dir[j]; m = m0+d;
            while (!(m & 0x88))
              {
                u = unmap[m];
                a[u] = ++a[u] | c;
                if (color[u] == neutral) m += d;
                else break;
              }
          }
      else
        for (j = j1; j <= j2; j++)
          if (!((m = m0+Dir[j]) & 0x88))
            {
              u = unmap[m];
              a[u] = ++a[u] | c;
            }
    }
}
#endif

/* ............    POSITIONAL EVALUATION ROUTINES    ............ */

ScorePosition(side,score)
short side,*score;

/*
   Perform normal static evaluation of board position. A score is 
   generated for each piece and these are summed to get a score for each 
   side. 
*/

{
register short sq,s;
short i,xside,pscore[3];

  wking = PieceList[white][0]; bking = PieceList[black][0];
  UpdateWeights();
  xside = otherside[side];
  pscore[white] = pscore[black] = 0;

  for (c1 = white; c1 <= black; c1++)
    {
      c2 = otherside[c1];
      if (c1 == white) EnemyKing = bking; else EnemyKing = wking;
      atk1 = atak[c1]; atk2 = atak[c2];
      PC1 = PawnCnt[c1]; PC2 = PawnCnt[c2];
      for (i = 0; i <= PieceCnt[c1]; i++)
        {
          sq = PieceList[c1][i];
          s = SqValue(sq,side);
          pscore[c1] += s;
          svalue[sq] = s;
        }
    }
  if (hung[side] > 1) pscore[side] += HUNGX;
  if (hung[xside] > 1) pscore[xside] += HUNGX;
  
  *score = mtl[side] - mtl[xside] + pscore[side] - pscore[xside] + 10;
  if (dither) *score += rand() % dither;
  
  if (*score > 0 && pmtl[side] == 0)
    if (emtl[side] < valueR) *score = 0;
    else if (*score < valueR) *score /= 2;
  if (*score < 0 && pmtl[xside] == 0)
    if (emtl[xside] < valueR) *score = 0;
    else if (-*score < valueR) *score /= 2;
    
  if (mtl[xside] == valueK && emtl[side] > valueB) *score += 200;
  if (mtl[side] == valueK && emtl[xside] > valueB) *score -= 200;
}


ScoreLoneKing(side,score)
short side,*score;

/* 
   Static evaluation when loser has only a king and winner has no pawns
   or no pieces.
*/

{
short winner,loser,king1,king2,s,i;

  UpdateWeights();
  if (mtl[white] > mtl[black]) winner = white; else winner = black;
  loser = otherside[winner];
  king1 = PieceList[winner][0]; king2 = PieceList[loser][0];
  
  s = 0;
  
  if (pmtl[winner] > 0)
    for (i = 1; i <= PieceCnt[winner]; i++)
      s += ScoreKPK(side,winner,loser,king1,king2,PieceList[winner][i]);
      
  else if (emtl[winner] == valueB+valueN)
    s = ScoreKBNK(winner,king1,king2);
    
  else if (emtl[winner] > valueB)
    s = 500 + emtl[winner] - DyingKing[king2] - 2*distance(king1,king2);
    
  if (side == winner) *score = s; else *score = -s;
}


int ScoreKPK(side,winner,loser,king1,king2,sq)
short side,winner,loser,king1,king2,sq;

/*
   Score King and Pawns versus King endings.
*/

{
short s,r;
  
  if (PieceCnt[winner] == 1) s = 50; else s = 120;
  if (winner == white)
    {
      if (side == loser) r = row[sq]-1; else r = row[sq];
      if (row[king2] >= r && distance(sq,king2) < 8-r) s += 10*row[sq];
      else s = 500+50*row[sq];
      if (row[sq] < 6) sq += 16; else sq += 8;
    }
  else
    {
      if (side == loser) r = row[sq]+1; else r = row[sq];
      if (row[king2] <= r && distance(sq,king2) < r+1) s += 10*(7-row[sq]);
      else s = 500+50*(7-row[sq]);
      if (row[sq] > 1) sq -= 16; else sq -= 8;
    }
  s += 8*(taxicab(king2,sq) - taxicab(king1,sq));
  return(s);
}


int ScoreKBNK(winner,king1,king2)
short winner,king1,king2;

/*
   Score King+Bishop+Knight versus King endings.
   This doesn't work all that well but it's better than nothing.
*/

{
short s;
  s = emtl[winner] - 300;
  if (KBNKsq == 0) s += KBNK[king2];
  else s += KBNK[locn[row[king2]][7-column[king2]]];
  s -= taxicab(king1,king2);
  s -= distance(PieceList[winner][1],king2);
  s -= distance(PieceList[winner][2],king2);
  return(s);
}


SqValue(sq,side)
short sq,side;

/*
   Calculate the positional value for the piece on 'sq'.
*/

{
register short j,fyle,rank;
short s,piece,a1,a2,in_square,r,mob,e,c;

  piece = board[sq];
  a1 = (atk1[sq] & 0x4FFF); a2 = (atk2[sq] & 0x4FFF);
  rank = row[sq]; fyle = column[sq];
  s = 0;
  if (piece == pawn && c1 == white)
    {
      s = Mwpawn[sq];
      if (sq == 11 || sq == 12)
        if (color[sq+8] != neutral) s += PEDRNK2B;
      if ((fyle == 0 || PC1[fyle-1] == 0) &&
          (fyle == 7 || PC1[fyle+1] == 0))
        s += ISOLANI[fyle];
      else if (PC1[fyle] > 1) s += PDOUBLED;
      if (a1 < ctlP && atk1[sq+8] < ctlP)
        {
          s += BACKWARD[a2 & 0xFF];
          if (PC2[fyle] == 0) s += PWEAKH;
          if (color[sq+8] != neutral) s += PBLOK;
        }
      if (PC2[fyle] == 0)
        {
          if (side == black) r = rank-1; else r = rank;
          in_square = (row[bking] >= r && distance(sq,bking) < 8-r);
          if (a2 == 0 || side == white) e = 0; else e = 1;
          for (j = sq+8; j < 64; j += 8)
            if (atk2[j] >= ctlP) { e = 2; break; }
            else if (atk2[j] > 0 || color[j] != neutral) e = 1;
          if (e == 2) s += (stage*PassedPawn3[rank]) / 10;
          else if (in_square || e == 1) s += (stage*PassedPawn2[rank]) / 10;
          else if (emtl[black] > 0) s += (stage*PassedPawn1[rank]) / 10;
          else s += PassedPawn0[rank];
        }
    }
  else if (piece == pawn && c1 == black)
    {
      s = Mbpawn[sq];
      if (sq == 51 || sq == 52)
        if (color[sq-8] != neutral) s += PEDRNK2B;
      if ((fyle == 0 || PC1[fyle-1] == 0) &&
          (fyle == 7 || PC1[fyle+1] == 0))
        s += ISOLANI[fyle];
      else if (PC1[fyle] > 1) s += PDOUBLED;
      if (a1 < ctlP && atk1[sq-8] < ctlP)
        {
          s += BACKWARD[a2 & 0xFF];
          if (PC2[fyle] == 0) s += PWEAKH;
          if (color[sq-8] != neutral) s += PBLOK;
        }
      if (PC2[fyle] == 0)
        {
          if (side == white) r = rank+1; else r = rank;
          in_square = (row[wking] <= r && distance(sq,wking) < r+1);
          if (a2 == 0 || side == black) e = 0; else e = 1;
          for (j = sq-8; j >= 0; j -= 8)
            if (atk2[j] >= ctlP) { e = 2; break; }
            else if (atk2[j] > 0 || color[j] != neutral) e = 1;
          if (e == 2) s += (stage*PassedPawn3[7-rank]) / 10;
          else if (in_square || e == 1) s += (stage*PassedPawn2[7-rank]) / 10;
          else if (emtl[white] > 0) s += (stage*PassedPawn1[7-rank]) / 10;
          else s += PassedPawn0[7-rank];
        }
    }
  else if (piece == knight)
    {
      s = Mknight[c1][sq];
    }
  else if (piece == bishop)
    {
      s = Mbishop[c1][sq];
      BRscan(sq,&s,&mob);
      s += BMBLTY[mob];
    }
  else if (piece == rook)
    {
      s += RookBonus;
      BRscan(sq,&s,&mob);
      s += RMBLTY[mob];
      if (PC1[fyle] == 0) s += RHOPN;
      if (PC2[fyle] == 0) s += RHOPNX;
      if (rank == rank7[c1] && pmtl[c2] > 100) s += 10;
      if (stage > 2) s += 14 - taxicab(sq,EnemyKing);
    }
  else if (piece == queen)
    {
      if (stage > 2) s += 14 - taxicab(sq,EnemyKing);
      if (distance(sq,EnemyKing) < 3) s += 12;
    }
  else if (piece == king)
    {
      s = Mking[c1][sq];
      if (KSFTY > 0)
        if (Developed[c2] || stage > 0) KingScan(sq,&s);
      if (castld[c1]) s += KCASTLD;
      else if (kingmoved[c1]) s += KMOVD;

      if (PC1[fyle] == 0) s += KHOPN;
      if (PC2[fyle] == 0) s += KHOPNX;
      if (fyle == 1 || fyle == 2 || fyle == 3 || fyle == 7)
        {
          if (PC1[fyle-1] == 0) s += KHOPN;
          if (PC2[fyle-1] == 0) s += KHOPNX;
        }
      if (fyle == 4 || fyle == 5 || fyle == 6 || fyle == 0)
        {
          if (PC1[fyle+1] == 0) s += KHOPN;
          if (PC2[fyle+1] == 0) s += KHOPNX;
        }
      if (fyle == 2)
        {
          if (PC1[0] == 0) s += KHOPN;
          if (PC2[0] == 0) s += KHOPNX;
        }
      if (fyle == 5)
        {
          if (PC1[7] == 0) s += KHOPN;
          if (PC2[7] == 0) s += KHOPNX;
        }
    }
    
  if (a2 > 0) 
    {
      c = (control[piece] & 0x4FFF);
      if (a1 == 0 || a2 > c+1)
        {
          s += HUNGP;
          ++hung[c1];
          if (piece != king && trapped(sq,piece)) ++hung[c1];
        }
      else if (piece != pawn || a2 > a1)
        if (a2 >= c || a1 < ctlP) s += ATAKD;
    }
  return(s);
}

#if (NEWMOVE > 6)
KingScan(sq,s)
short sq,*s;

/*
   Assign penalties if king can be threatened by checks, if squares
   near the king are controlled by the enemy (especially the queen),
   or if there are no pawns near the king.
*/

#define ScoreThreat\
  if (color[u] != c2)\
    if (atk1[u] == 0 || (atk2[u] & 0xFF) > 1) ++cnt;\
    else *s -= 3

{
register short m,u;
short d,i,m0,cnt,ok;

  cnt = 0;
  m0 = map[sq];
  if (HasBishop[c2] || HasQueen[c2])
    for (i = Dstart[bishop]; i <= Dstop[bishop]; i++)
      {
        d = Dir[i]; m = m0+d;
        while (!(m & 0x88))
          {
            u = unmap[m];
            if (atk2[u] & ctlBQ) ScoreThreat;
            if (color[u] != neutral) break;
            m += d;
          }
      }
  if (HasRook[c2] || HasQueen[c2])
    for (i = Dstart[rook]; i <= Dstop[rook]; i++)
      {
        d = Dir[i]; m = m0+d;
        while (!(m & 0x88))
          {
            u = unmap[m];
            if (atk2[u] & ctlRQ) ScoreThreat;
            if (color[u] != neutral) break;
            m += d;
          }
      }
  if (HasKnight[c2])
    for (i = Dstart[knight]; i <= Dstop[knight]; i++)
      if (!((m = m0+Dir[i]) & 0x88))
        {
          u = unmap[m];
          if (atk2[u] & ctlNN) ScoreThreat;
        }
  *s += (KSFTY*Kthreat[cnt]) / 16;

  cnt = 0; ok = false;
  m0 = map[sq];
  for (i = Dstart[king]; i <= Dstop[king]; i++)
    if (!((m = m0+Dir[i]) & 0x88))
      {
        u = unmap[m];
        if (board[u] == pawn) ok = true;
        if (atk2[u] > atk1[u])
          {
            ++cnt;
            if (atk2[u] & ctlQ)
              if (atk2[u] > ctlQ+1 && atk1[u] < ctlQ) *s -= 4*KSFTY;
          }
      }
  if (!ok) *s -= KSFTY;
  if (cnt > 1) *s -= KSFTY;
}
#endif

#if (NEWMOVE < 4)
BRscan(sq,s,mob)
short sq,*s,*mob;

/*
   Find Bishop and Rook mobility, XRAY attacks, and pins. Increment the 
   hung[] array if a pin is found. 
*/

{
register short m,u;
short d,j,m0,piece,pin,*Kf;

  Kf = Kfield[c1];
  *mob = 0;
  m0 = map[sq]; piece = board[sq];
  for (j = Dstart[piece]; j <= Dstop[piece]; j++)
    {
      pin = -1;
      d = Dir[j]; m = m0+d;
      while (!(m & 0x88))
        {
          u = unmap[m]; *s += Kf[u];
          if (color[u] == neutral)
            {
              (*mob)++;
              m += d;
            }
          else if (pin < 0)
            {
              if (board[u] == pawn || board[u] == king) break;
              pin = u;
              m += d;
            }
          else if (color[u] == c2 && (board[u] > piece || atk2[u] == 0))
            {
              if (color[pin] == c2)
                {
                  *s += PINVAL;
                  if (atk2[pin] == 0 ||
                      atk1[pin] > control[board[pin]]+1)
                    ++hung[c2];
                }
              else *s += XRAY;
              break;
            }
          else break;
        }
    }
}
#endif

#if (NEWMOVE > 5)
int trapped(sq,piece)
short sq,piece;

/*
   See if the attacked piece has unattacked squares to move to.
*/

{
register short u,m,d;
short i,m0;

  m0 = map[sq];
  if (sweep[piece])
    for (i = Dstart[piece]; i <= Dstop[piece]; i++)
      {
        d = Dir[i]; m = m0+d;
        while (!(m & 0x88))
          {
            u = unmap[m];
            if (color[u] == c1) break;
            if (atk2[u] == 0 || board[u] >= piece) return(false);
            if (color[u] == c2) break;
            m += d;
          }
      }
  else if (piece == pawn)
    {
      if (c1 == white) u = sq+8; else u = sq-8;
      if (color[u] == neutral && atk1[u] >= atk2[u])
        return(false);
      if (!((m = m0+Dir[Dpwn[c1]]) & 0x88))
        if (color[unmap[m]] == c2) return(false);
      if (!((m = m0+Dir[Dpwn[c1]+1]) & 0x88))
        if (color[unmap[m]] == c2) return(false);
    }
  else
    {
      for (i = Dstart[piece]; i <= Dstop[piece]; i++)
        if (!((m = m0+Dir[i]) & 0x88))
          {
            u = unmap[m];
            if (color[u] != c1)
              if (atk2[u] == 0 || board[u] >= piece) return(false);
          }
    }
  return(true);
}
#endif

ExaminePosition()

/*
   This is done one time before the search is started. Set up arrays 
   Mwpawn, Mbpawn, Mknight, Mbishop, Mking which are used in the 
   SqValue() function to determine the positional value of each piece. 
*/

{
register short i,sq;
short wpadv,bpadv,wstrong,bstrong,z,side,pp,j,val,Pd,fyle,rank;

  wking = PieceList[white][0]; bking = PieceList[black][0];
  ataks(white,atak[white]); ataks(black,atak[black]);
  Zwmtl = Zbmtl = 0;
  UpdateWeights();
  HasPawn[white] = HasPawn[black] = 0;
  HasKnight[white] = HasKnight[black] = 0;
  HasBishop[white] = HasBishop[black] = 0;
  HasRook[white] = HasRook[black] = 0;
  HasQueen[white] = HasQueen[black] = 0;
  for (side = white; side <= black; side++)
    for (i = 0; i <= PieceCnt[side]; i++)
      switch (board[PieceList[side][i]])
        {
          case pawn : ++HasPawn[side]; break;
          case knight : ++HasKnight[side]; break;
          case bishop : ++HasBishop[side]; break;
          case rook : ++HasRook[side]; break;
          case queen : ++HasQueen[side]; break;
        }
  if (!Developed[white])
    Developed[white] = (board[1] != knight && board[2] != bishop &&
                        board[5] != bishop && board[6] != knight);
  if (!Developed[black])
    Developed[black] = (board[57] != knight && board[58] != bishop &&
                        board[61] != bishop && board[62] != knight);
  if (!PawnStorm && stage < 5)
    PawnStorm = ((column[wking] < 3 && column[bking] > 4) ||
                 (column[wking] > 4 && column[bking] < 3));
  
  CopyBoard(pknight,Mknight[white]);
  CopyBoard(pknight,Mknight[black]);
  CopyBoard(pbishop,Mbishop[white]);
  CopyBoard(pbishop,Mbishop[black]);
  BlendBoard(KingOpening,KingEnding,Mking[white]);
  BlendBoard(KingOpening,KingEnding,Mking[black]);
  
  for (sq = 0; sq < 64; sq++)
    {
      fyle = column[sq]; rank = row[sq];
      wstrong = bstrong = true;
      for (i = sq; i < 64; i += 8)
        if (atak[black][i] >= ctlP) wstrong = false;
      for (i = sq; i >= 0; i -= 8)
        if (atak[white][i] >= ctlP) bstrong = false;
      wpadv = bpadv = PADVNCM;
      if ((fyle == 0 || PawnCnt[white][fyle-1] == 0) &&
          (fyle == 7 || PawnCnt[white][fyle+1] == 0)) wpadv = PADVNCI;
      if ((fyle == 0 || PawnCnt[black][fyle-1] == 0) &&
          (fyle == 7 || PawnCnt[black][fyle+1] == 0)) bpadv = PADVNCI;
      Mwpawn[sq] = (wpadv*PawnAdvance[sq]) / 10;
      Mbpawn[sq] = (bpadv*PawnAdvance[63-sq]) / 10;
      Mwpawn[sq] += PawnBonus; Mbpawn[sq] += PawnBonus;
      if (castld[white] || kingmoved[white])
        {
          if ((fyle < 3 || fyle > 4) && distance(sq,wking) < 3)
            Mwpawn[sq] += PAWNSHIELD;
        }
      else if (rank < 3 && (fyle < 2 || fyle > 5))
        Mwpawn[sq] += PAWNSHIELD / 2;
      if (castld[black] || kingmoved[black])
        {
          if ((fyle < 3 || fyle > 4) && distance(sq,bking) < 3)
            Mbpawn[sq] += PAWNSHIELD;
        }
      else if (rank > 4 && (fyle < 2 || fyle > 5))
        Mbpawn[sq] += PAWNSHIELD / 2;
      if (PawnStorm)
        {
          if ((column[wking] < 4 && fyle > 4) ||
              (column[wking] > 3 && fyle < 3)) Mwpawn[sq] += 3*rank - 21;
          if ((column[bking] < 4 && fyle > 4) ||
              (column[bking] > 3 && fyle < 3)) Mbpawn[sq] -= 3*rank;
        }
        
      Mknight[white][sq] += 5 - distance(sq,bking);
      Mknight[white][sq] += 5 - distance(sq,wking);
      Mknight[black][sq] += 5 - distance(sq,wking);
      Mknight[black][sq] += 5 - distance(sq,bking);
      Mbishop[white][sq] += BishopBonus;
      Mbishop[black][sq] += BishopBonus;
      for (i = 0; i <= PieceCnt[black]; i++)
        if (distance(sq,PieceList[black][i]) < 3)
          Mknight[white][sq] += KNIGHTPOST;
      for (i = 0; i <= PieceCnt[white]; i++)
        if (distance(sq,PieceList[white][i]) < 3)
          Mknight[black][sq] += KNIGHTPOST;
      if (wstrong) Mknight[white][sq] += KNIGHTSTRONG;
      if (bstrong) Mknight[black][sq] += KNIGHTSTRONG;
      if (wstrong) Mbishop[white][sq] += BISHOPSTRONG;
      if (bstrong) Mbishop[black][sq] += BISHOPSTRONG;
      
      if (HasBishop[white] == 2) Mbishop[white][sq] += 8;
      if (HasBishop[black] == 2) Mbishop[black][sq] += 8;
      if (HasKnight[white] == 2) Mknight[white][sq] += 5;
      if (HasKnight[black] == 2) Mknight[black][sq] += 5;
      
      if (board[sq] == bishop)
        if (rank % 2 == fyle % 2) KBNKsq = 0; else KBNKsq = 7;
        
      Kfield[white][sq] = Kfield[black][sq] = 0;
      if (distance(sq,wking) == 1) Kfield[black][sq] = KATAK;
      if (distance(sq,bking) == 1) Kfield[white][sq] = KATAK;
      
      Pd = 0;
      for (i = 0; i < 64; i++)
        if (board[i] == pawn)
          {
            if (color[i] == white)
              {
                pp = true;
                if (row[i] == 6) z = i+8; else z = i+16;
                for (j = i+8; j < 64; j += 8)
                  if (atak[black][j] > ctlP || board[j] == pawn) pp = false;
              }
            else
              {
                pp = true;
                if (row[i] == 1) z = i-8; else z = i-16;
                for (j = i-8; j >= 0; j -= 8)
                  if (atak[white][j] > ctlP || board[j] == pawn) pp = false;
              }
            if (pp) Pd += 5*taxicab(sq,z); else Pd += taxicab(sq,z);
          }
      if (Pd != 0)
        {
          val = (Pd*stage2) / 10;
          Mking[white][sq] -= val;
          Mking[black][sq] -= val;
        }
    }
}


UpdateWeights()

/* 
   If material balance has changed, determine the values for the 
   positional evaluation terms. 
*/

{
short tmtl;

  if (mtl[white] != Zwmtl || mtl[black] != Zbmtl)
    {
      Zwmtl = mtl[white]; Zbmtl = mtl[black];
      emtl[white] = Zwmtl - pmtl[white] - valueK;
      emtl[black] = Zbmtl - pmtl[black] - valueK;
      tmtl = emtl[white] + emtl[black];
      if (tmtl > 6600) stage = 0;
      else if (tmtl < 1400) stage = 10;
      else stage = (6600-tmtl) / 520;
      if (tmtl > 3600) stage2 = 0;
      else if (tmtl < 1400) stage2 = 10;
      else stage2 = (3600-tmtl) / 220;
      
      PEDRNK2B = -15;         /* centre pawn on 2nd rank & blocked */
      PBLOK = -4;             /* blocked backward pawn */
      PDOUBLED = -14;         /* doubled pawn */
      PWEAKH  = -4;           /* weak pawn on half open file */
      PAWNSHIELD = 10-stage;  /* pawn near friendly king */
      PADVNCM =  10;          /* advanced pawn multiplier */
      PADVNCI = 7;            /* muliplier for isolated pawn */
      PawnBonus = stage;
      
      KNIGHTPOST = (stage+2)/3;   /* knight near enemy pieces */
      KNIGHTSTRONG = (stage+6)/2; /* occupies pawn hole */
      
      BISHOPSTRONG = (stage+6)/2; /* occupies pawn hole */
      BishopBonus = 2*stage;
      
      RHOPN    = 10;          /* rook on half open file */
      RHOPNX   = 4;
      RookBonus = 6*stage;
      
      XRAY     = 8;           /* Xray attack on piece */
      PINVAL   = 10;          /* Pin */
      
      KHOPN    = (3*stage-30) / 2; /* king on half open file */
      KHOPNX   = KHOPN / 2;
      KCASTLD  = 10 - stage;
      KMOVD    = -40 / (stage+1);  /* king moved before castling */
      KATAK    = (10-stage) / 2;   /* B,R attacks near enemy king */
      if (stage < 8) KSFTY = 16-2*stage; else KSFTY = 0;
      
      ATAKD    = -6;          /* defender > attacker */
      HUNGP    = -8;          /* each hung piece */
      HUNGX    = -12;         /* extra for >1 hung piece */
    }
}

#if (NEWMOVE < 1)
int distance(a,b)
short a,b;
{
register short d1,d2;

  d1 = abs(column[a]-column[b]);
  d2 = abs(row[a]-row[b]);
  return(d1 > d2 ? d1 : d2);
}
#endif

BlendBoard(a,b,c)
short a[64],b[64],c[64];
{
register int sq;
  for (sq = 0; sq < 64; sq++)
    c[sq] = (a[sq]*(10-stage) + b[sq]*stage) / 10;
}


CopyBoard(a,b)
short a[64],b[64];
{
register int sq;
  for (sq = 0; sq < 64; sq++)
    b[sq] = a[sq];
}
