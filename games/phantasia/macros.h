/*
 * macros.h - macro definitions for Phantasia
 */

#define ROLL(BASE,INTERVAL)	floor((BASE) + (INTERVAL) * drandom())
#define SGN(X)		((X) < 0 ? -1 : 1)
#define CIRCLE(X, Y)	floor(distance(X, 0.0, Y, 0.0) / 125.0 + 1)
#define MAX(A, B)	((A) > (B) ? (A) : (B))
#define MIN(A, B)	((A) < (B) ? (A) : (B))
#define ILLCMD()	mvaddstr(5, 0, Illcmd)
#define MAXMOVE()	(Player.p_level * 1.5 + 1)
#define ILLMOVE()	mvaddstr(5, 0, Illmove)
#define ILLSPELL()	mvaddstr(5, 0, Illspell)
#define NOMANA()	mvaddstr(5, 0, Nomana)
#define SOMEBETTER()	addstr(Somebetter)
#define NOBETTER()	mvaddstr(17, 0, Nobetter)
