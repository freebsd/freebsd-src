/* move generator hes@log-sv.se 890318
   Modified: 890606 NEWMOVE Levels 1-6 for easier debugging */
#include "move.h"
#include "gnuchess.h"

short distdata[64][64];
short taxidata[64][64];

void Initialize_dist() {
register short a,b,d,di;

  /* init taxi and dist data */
  for(a=0;a<64;a++)
    for(b=0;b<64;b++) {
      d = abs(column[a]-column[b]);
      di = abs(row[a]-row[b]);
      taxidata[a][b] = d + di;
      distdata[a][b] = (d > di ? d : di);
    };
}

#if (NEWMOVE > 1)
struct sqdata posdata[3][8][64][64];

static short direc[8][8] = {
    0,  0,  0,  0,  0,  0,  0,  0, /* no_piece = 0 */
  -10,-11, -9,  0,  0,  0,  0,  0, /* wpawn    = 1 */
  -21,-19,-12, -8, 21, 19, 12,  8, /* knight   = 2 */
  -11, -9, 11,  9,  0,  0,  0,  0, /* bishop   = 3 */
  -10, -1, 10,  1,  0,  0,  0,  0, /* rook     = 4 */
  -11, -9,-10, -1, 11,  9, 10,  1, /* queen    = 5 */
  -11, -9,-10, -1, 11,  9, 10,  1, /* king     = 6 */
    0,  0,  0,  0,  0,  0,  0,  0};/* no_piece = 7 */

static short dc[3] = {-1,1,0};

static short max_steps [8] = {0,2,1,7,7,7,1,0};

static short unmap[120] = {
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1, 0, 1, 2, 3, 4, 5, 6, 7,-1,
  -1, 8, 9,10,11,12,13,14,15,-1,
  -1,16,17,18,19,20,21,22,23,-1,
  -1,24,25,26,27,28,29,30,31,-1,
  -1,32,33,34,35,36,37,38,39,-1,
  -1,40,41,42,43,44,45,46,47,-1,
  -1,48,49,50,51,52,53,54,55,-1,
  -1,56,57,58,59,60,61,62,63,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1};

void Initialize_moves() {
  short c,ptyp,po,p0,d,di,s;
  struct sqdata *p;
  short dest[8][8];
  short steps[8];
  short sorted[8];

  /* init posdata */
  for(c=0;c<3;c++)
    for(ptyp=0;ptyp<8;ptyp++)
      for(po=0;po<64;po++)
	for(p0=0;p0<64;p0++) {
	  posdata[c][ptyp][po][p0].nextpos = po;
	  posdata[c][ptyp][po][p0].nextdir = po;
	};
  /* dest is a function of dir and step */
  for(c=0;c<2;c++)
    for(ptyp=1;ptyp<7;ptyp++)
      for(po=21;po<99;po++)
	if (unmap[po] >= 0) {
          p = posdata[c][ptyp][unmap[po]];
	  for(d=0;d<8;d++) {
	    dest[d][0] = unmap[po];
	    if (dc[c]*direc[ptyp][d] != 0) {
	      p0=po;
	      for(s=0;s<max_steps[ptyp];s++) {
		p0 = p0 + dc[c]*direc[ptyp][d];
		/* break if (off board) or
		   (pawns move two steps from home square) */
		if (unmap[p0] < 0 ||
		    (ptyp == pawn && s>0 && (d>0 || Stboard[unmap[po]] != ptyp)))
		  break;
		else
		  dest[d][s] = unmap[p0];
	      }
	    }
	    else s=0;
	    /* sort dest in number of steps order */
	    steps[d] = s;
	    for(di=d;di>0;di--)
	      if (steps[sorted[di-1]] < s)
		sorted[di] = sorted[di-1];
	      else
		break;
	    sorted[di] = d;
	  }
	  /* update posdata, pawns have two threads (capture and no capture) */
	  p0=unmap[po];
	  if (ptyp == pawn) {
	    for(s=0;s<steps[0];s++) {
	      p[p0].nextpos = dest[0][s];
	      p0 = dest[0][s];
	    }
	    p0=unmap[po];
	    for(d=1;d<3;d++) {
	      p[p0].nextdir = dest[d][0];
	      p0 = dest[d][0];
	    }
	  }
	  else {
	    p[p0].nextdir = dest[sorted[0]][0];
	    for(d=0;d<8;d++)
	      for(s=0;s<steps[sorted[d]];s++) {
		p[p0].nextpos = dest[sorted[d]][s];
		p0 = dest[sorted[d]][s];
		if (d < 7)
		  p[p0].nextdir = dest[sorted[d+1]][0];
		/* else is already initialised */
	      }
	  }
#ifdef DEBUG
	  printf("Ptyp:%d Position:%d\n{",ptyp,unmap[po]);
	  for(p0=0;p0<63;p0++) printf("%d,",p[p0].nextpos);
	  printf("%d};\n",p[63].nextpos);
	  for(p0=0;p0<63;p0++) printf("%d,",p[p0].nextdir);
	  printf("%d};\n",p[63].nextdir);
#endif DEBUG
	}
}
#endif


#if (NEWMOVE > 2)
int SqAtakd(sq,side)
short sq,side;

/*
  See if any piece with color 'side' ataks sq. First check pawns
  Then Queen, Bishop, Rook and King and last Knight.
*/

{
  register short u;
  register struct sqdata *p;

  p = posdata[1-side][pawn][sq];
  u = p[sq].nextdir; /* follow captures thread */
  while (u != sq) {
    if (board[u] == pawn && color[u] == side) return(true);
    u = p[u].nextdir;
  }
  /* king capture */
  if (distance(sq,PieceList[side][0]) == 1) return(true);
  /* try a queen bishop capture */
  p = posdata[side][bishop][sq];
  u = p[sq].nextpos;
  while (u != sq) {
    if (color[u] == neutral) {
      u = p[u].nextpos;
    }
    else {
      if (color[u] == side && 
	  (board[u] == queen || board[u] == bishop))
	return(true);
      u = p[u].nextdir;
    }
  }
  /* try a queen rook capture */
  p = posdata[side][rook][sq];
  u = p[sq].nextpos;
  while (u != sq) {
    if (color[u] == neutral) {
      u = p[u].nextpos;
    }
    else {
      if (color[u] == side && 
	  (board[u] == queen || board[u] == rook))
	return(true);
      u = p[u].nextdir;
    }
  }
  /* try a knight capture */
  p = posdata[side][knight][sq];
  u = p[sq].nextpos;
  while (u != sq) {
    if (color[u] == neutral) {
      u = p[u].nextpos;
    }
    else {
      if (color[u] == side && board[u] == knight) return(true);
      u = p[u].nextdir;
    }
  }
  return(false);
}
#endif

#if (NEWMOVE > 3)
BRscan(sq,s,mob)
short sq,*s,*mob;
/*
   Find Bishop and Rook mobility, XRAY attacks, and pins. Increment the 
   hung[] array if a pin is found. 
*/
{
  register short u,piece,pin;
  register struct sqdata *p;
  short *Kf;

  Kf = Kfield[c1];
  *mob = 0;
  piece = board[sq];
  p = posdata[color[sq]][piece][sq];
  u = p[sq].nextpos;
  pin = -1; /* start new direction */
  while (u != sq) {
    *s += Kf[u];
    if (color[u] == neutral) {
      (*mob)++;
      if (p[u].nextpos == p[u].nextdir) pin = -1; /* oops new direction */
      u = p[u].nextpos;
    }
    else if (pin < 0) {
      if (board[u] == pawn || board[u] == king)
	u = p[u].nextdir;
      else {
	if (p[u].nextpos != p[u].nextdir)
	  pin = u; /* not on the edge and on to find a pin */
	u = p[u].nextpos;
      }
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
	pin = -1; /* new direction */
	u = p[u].nextdir;
      }
    else {
      pin = -1; /* new direction */
      u = p[u].nextdir;
    }
  }
}
#endif

#if (NEWMOVE >= 5)
CaptureList(side,xside,ply)
short side,xside,ply;
{
  register short u,sq;
  register struct sqdata *p;
  short i,piece,*PL;
  struct leaf *node;
  
  TrPnt[ply+1] = TrPnt[ply];
  node = &Tree[TrPnt[ply]];
  PL = PieceList[side];
  for (i = 0; i <= PieceCnt[side]; i++)
    {
      sq = PL[i];
      piece = board[sq];
      p = posdata[side][piece][sq];
      if (piece == pawn) {
	u = p[sq].nextdir; /* follow captures thread */
	while (u != sq) {
	  if (color[u] == xside) {
            node->f = sq; node->t = u;
            node->flags = capture;
            if (u < 8 || u > 55)
              {
                node->flags |= promote;
                node->score = valueQ;
              }
	    else
              node->score = value[board[u]] + svalue[board[u]] - piece;
            ++node;
            ++TrPnt[ply+1];
           }
	  u = p[u].nextdir;
	}
      }
      else {
	u = p[sq].nextpos;
	while (u != sq) {
          if (color[u] == neutral)
          u = p[u].nextpos;
          else {
          if (color[u] == xside) {
              node->f = sq; node->t = u;
              node->flags = capture;
              node->score = value[board[u]] + svalue[board[u]] - piece;
              ++node;
              ++TrPnt[ply+1];
             }
	   u = p[u].nextdir;
	 }
       }
     }
   }
}
#endif

#if (NEWMOVE > 5)
GenMoves(ply,sq,side,xside)
     short ply,sq,side,xside;
 
/*
  Generate moves for a piece. The moves are taken from the
  precalulated array posdata. If the board is free, next move
  is choosen from nextpos else from nextdir.
*/
 
{
  register short u,piece;
  register struct sqdata *p;
	 
  piece = board[sq];
  p = posdata[side][piece][sq];
  if (piece == pawn) {
    u = p[sq].nextdir; /* follow captures thread */
    while (u != sq) {
      if (color[u] == xside) LinkMove(ply,sq,u,xside);
      u = p[u].nextdir;
    }
    u = p[sq].nextpos; /* and follow no captures thread */
    while (u != sq) {
      if (color[u] == neutral && (u != sq+16 || color[u-8] == neutral)
          && (u != sq-16 || color[u+8] == neutral)) {
        LinkMove(ply,sq,u,xside);
      }
      u = p[u].nextpos;
    }
  }  
  else {
    u = p[sq].nextpos;
    while (u != sq) {
      if (color[u] == neutral) {
        LinkMove(ply,sq,u,xside);
        u = p[u].nextpos;
      }
      else {
        if (color[u] == xside) LinkMove(ply,sq,u,xside);
	u = p[u].nextdir;
      }
    }
  }    
} 
#endif
