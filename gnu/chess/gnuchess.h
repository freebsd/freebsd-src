/*
  This file contains code for CHESS.
  Copyright (C) 1986, 1987, 1988 Free Software Foundation, Inc.

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


/* Header file for GNU CHESS */  

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
#define maxdepth 30
#define true 1
#define false 0

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

extern char mvstr1[5],mvstr2[5];
extern struct leaf Tree[2000],*root;
extern short TrPnt[maxdepth],board[64],color[64];
extern short row[64],column[64],locn[8][8];
extern short atak[2][64],PawnCnt[2][8];
extern short castld[2],kingmoved[2];
extern short c1,c2,*atk1,*atk2,*PC1,*PC2;
extern short mate,post,opponent,computer,Sdepth,Awindow,Bwindow,dither;
extern long ResponseTime,ExtraTime,Level,et,et0,time0,cputimer,ft;
extern long NodeCnt,evrate,ETnodes,EvalNodes,HashCnt;
extern short quit,reverse,bothsides,hashflag,InChk,player,force,easy,beep,meter;
extern short timeout,xwndw;
extern struct GameRec GameList[240];
extern short GameCnt,Game50,epsquare,lpost,rcptr,contempt;
extern short MaxSearchDepth;
extern struct BookEntry *Book;
extern struct TimeControlRec TimeControl;
extern short TCflag,TCmoves,TCminutes,OperatorTime;
extern short otherside[3];
extern short Stboard[64];
extern short Stcolor[64];
extern unsigned short hint,PrVar[maxdepth];

#define HZ 60
