/* header file for move generator hes 890318
   Modified: 890510 minor bug fixed in Newataks
             890606 NEWMOVE levels 1-6 */

#if (NEWMOVE >= 1)
extern short distdata[64][64];
extern short taxidata[64][64];

#define taxicab(a,b) taxidata[a][b]
#define distance(a,b) distdata[a][b]

extern void Initialize_dist();
#endif
  
#if (NEWMOVE >= 2)
struct sqdata {
  short nextpos;
  short nextdir;
};
extern struct sqdata posdata[3][8][64][64];

extern void Initialize_moves();

#define ataks(side,a)\
{\
  register short u,c,sq;\
  register struct sqdata *p;\
  short i,piece,*PL;\
  \
  for (u = 64; u; a[--u] = 0); \
  PL = PieceList[side];\
  for (i = 0; i <= PieceCnt[side]; i++)\
    {\
      sq = PL[i];\
      piece = board[sq];\
      c = control[piece];\
      p = posdata[side][piece][sq];\
      if (piece == pawn) {\
	u = p[sq].nextdir; /* follow captures thread */\
	while (u != sq) {\
	  a[u] = ++a[u] | c;\
	  u = p[u].nextdir;\
	}\
      }\
      else {\
	u = p[sq].nextpos;\
	while (u != sq) {\
          a[u] = ++a[u] | c;\
	  if (color[u] == neutral)\
	    u = p[u].nextpos;\
	  else\
	    u = p[u].nextdir;\
	}\
      }\
    }\
}
#endif

#if (NEWMOVE >= 3)
extern short PieceList[2][16];

extern int Sqatakd();
#endif

#if (NEWMOVE > 3)
extern short Kfield[2][64],PINVAL,control[7],hung[2],XRAY;

extern BRscan();
#endif

#if (NEWMOVE > 4)
#define valueQ 1100

extern short PieceCnt[2],value[7],svalue[64];

extern CaptureList();
#endif

#if (NEWMOVE > 5)
extern GenMoves();
#endif
