/*
 *	Name: Towers of Hanoi.
 *
 *	Desc:
 *		This is a playable copy of towers of hanoi.
 *		Its sole purpose is to demonstrate my Amiga Curses package.
 *		This program should compile on any system that has Curses.
 *		'hanoi'		will give a manual game with 7 playing pieces.
 *		'hanoi n'	will give a manual game with n playing pieces.
 *		'hanoi n a' will give an auto solved game with n playing pieces.
 *
 *	Author: Simon J Raybould	(sie@fulcrum.bt.co.uk).
 *
 *	Date: 05.Nov.90
 *
 */

#include <ncurses.h>

#define NPEGS			3	/* This is not configurable !! */
#define MINTILES		3
#define MAXTILES		9
#define DEFAULTTILES	7
#define TOPLINE			6
#define BASELINE		16
#define STATUSLINE		(LINES-3)
#define LEFTPEG			19
#define MIDPEG			39
#define RIGHTPEG		59

#define LENTOIND(x)             (((x)-1)/2)
#define OTHER(a,b)		(3-((a)+(b)))

struct Peg {
	int Length[MAXTILES];
	int Count;
};

struct Peg Pegs[NPEGS];
int PegPos[] = { LEFTPEG, MIDPEG, RIGHTPEG };
int TileColour[] = {
	COLOR_GREEN,	/* Length 3 */
	COLOR_MAGENTA,	/* Length 5 */
	COLOR_RED,	/* Length 7 */
	COLOR_BLUE,	/* Length 9 */
	COLOR_CYAN,	/* Length 11 */
	COLOR_YELLOW, 	/* Length 13 */
	COLOR_GREEN,  	/* Length 15 */
	COLOR_MAGENTA,	/* Length 17 */
	COLOR_RED,	/* Length 19 */
};
int NMoves = 0;

static unsigned char AutoFlag;

void InitTiles(), DisplayTiles(), MakeMove(), AutoMove(), Usage();

int
main(int argc, char **argv)
{
int NTiles, FromCol, ToCol;

	switch(argc) {
	case 1:
		NTiles = DEFAULTTILES;
		break;
	case 2:
		NTiles = atoi(argv[1]);
		if (NTiles > MAXTILES || NTiles < MINTILES) {
			fprintf(stderr, "Range %d to %d\n", MINTILES, MAXTILES);
			exit(1);
		}
		break;
	case 3:
		if (strcmp(argv[2], "a")) {
			Usage();
			exit(1);
		}
		NTiles = atoi(argv[1]);
		if (NTiles > MAXTILES || NTiles < MINTILES) {
			fprintf(stderr, "Range %d to %d\n", MINTILES, MAXTILES);
			exit(1);
		}
		AutoFlag = TRUE;
		break;
	default:
		Usage();
		exit(1);
	}
	initscr();
	if (!has_colors()) {
		puts("terminal doesn't support color.");
		exit(1);
	}
	start_color();
	{
	int i;
	for (i = 0; i < 9; i++)
		init_pair(i+1, COLOR_BLACK, TileColour[i]);
	}
	cbreak();
	if (LINES < 24) {
		fprintf(stderr, "Min screen length 24 lines\n");
		endwin();
		exit(1);
	}
	if(AutoFlag)
		leaveok(stdscr, TRUE);	/* Attempt to remove cursor */
	InitTiles(NTiles);
	DisplayTiles();
	if(AutoFlag) {
		do {
			noecho();
			AutoMove(0, 2, NTiles);
		} while(!Solved(NTiles));
		sleep(2);
	} else {
		for(;;) {
			if(GetMove(&FromCol, &ToCol))
				break;
			if (AutoFlag) {
				AutoMove(0, 2, NTiles);
				break;
			}
			if(InvalidMove(FromCol, ToCol)) {
				mvaddstr(STATUSLINE, 0, "Invalid Move !!");
				refresh();
				beep();
				continue;
			}
			MakeMove(FromCol, ToCol);
			if(Solved(NTiles)) {
				mvprintw(STATUSLINE, 0, "Well Done !! You did it in %d moves", NMoves);
				refresh();
				sleep(5);
				break;
			}
		}
	}
	endwin();
}

int
InvalidMove(int From, int To)
{
	if(From == To)
		return TRUE;
	if(!Pegs[From].Count)
		return TRUE;
	if(Pegs[To].Count &&
			Pegs[From].Length[Pegs[From].Count-1] >
			Pegs[To].Length[Pegs[To].Count-1])
		return TRUE;
	return FALSE;
}

void
InitTiles(int NTiles)
{
int Size, SlotNo;

	for(Size=NTiles*2+1, SlotNo=0; Size>=3; Size-=2)
		Pegs[0].Length[SlotNo++] = Size;

	Pegs[0].Count = NTiles;
	Pegs[1].Count = 0;
	Pegs[2].Count = 0;
}

#define gc()	{noecho();getch();echo();}

void
DisplayTiles()
{
	int Line, Peg, SlotNo;
	char TileBuf[BUFSIZ];

	erase();
	mvaddstr(1, 24, "T O W E R S   O F   H A N O I");
	mvaddstr(3, 34, "SJR 1990");
	mvprintw(19, 5, "Moves : %d", NMoves);
	standout();
	mvaddstr(BASELINE, 8, "                                                               ");

	for(Line=TOPLINE; Line<BASELINE; Line++) {
		mvaddch(Line, LEFTPEG, ' ');
		mvaddch(Line, MIDPEG, ' ');
		mvaddch(Line, RIGHTPEG, ' ');
	}
	mvaddch(BASELINE, LEFTPEG, '1');
	mvaddch(BASELINE, MIDPEG, '2');
	mvaddch(BASELINE, RIGHTPEG, '3');
	standend();

	/* Draw tiles */
	for(Peg=0; Peg<NPEGS; Peg++) {
		for(SlotNo=0; SlotNo<Pegs[Peg].Count; SlotNo++) {
			memset(TileBuf, ' ', Pegs[Peg].Length[SlotNo]);
			TileBuf[Pegs[Peg].Length[SlotNo]] = '\0';
			attrset(COLOR_PAIR(LENTOIND(Pegs[Peg].Length[SlotNo])));
			mvaddstr(BASELINE-(SlotNo+1),
					PegPos[Peg]-Pegs[Peg].Length[SlotNo]/2, TileBuf);
		}
	}
	attrset(A_NORMAL);
	refresh();
}

int
GetMove(int *From, int *To)
{
	mvaddstr(STATUSLINE, 0, "Next move ('q' to quit) from ");
	clrtoeol();
	refresh();
	if((*From = getch()) == 'q')
		return TRUE;
	else if (*From == 'a') {
		AutoFlag = TRUE;
		return FALSE;
	}
	*From -= ('0'+1);
	addstr(" to ");
	clrtoeol();
	refresh();
	if((*To = getch()) == 'q')
		return TRUE;
	*To -= ('0'+1);
	move(STATUSLINE, 0);
	clrtoeol();
	refresh();
	return FALSE;
}

void
MakeMove(int From, int To)
{

	Pegs[From].Count--;
	Pegs[To].Length[Pegs[To].Count] = Pegs[From].Length[Pegs[From].Count];
	Pegs[To].Count++;
	NMoves++;
	DisplayTiles();
}

void
AutoMove(int From, int To, int Num)
{

	if(Num == 1) {
		MakeMove(From, To);
		return;
	}
	AutoMove(From, OTHER(From, To), Num-1);
	MakeMove(From, To);
	AutoMove(OTHER(From, To), To, Num-1);
}

int
Solved(int NumTiles)
{
int i;

	for(i = 1; i < NPEGS; i++)
		if (Pegs[i].Count == NumTiles)
			return TRUE;
	return FALSE;
}

void
Usage()
{
	fprintf(stderr, "Usage: hanoi [<No Of Tiles>] [a]\n");
	fprintf(stderr, "The 'a' option causes the tower to be solved automatically\n");
}

