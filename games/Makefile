#	@(#)Makefile	8.2 (Berkeley) 3/31/94

# XXX missing: chess
SUBDIR=	adventure arithmetic atc backgammon battlestar bcd boggle caesar \
	canfield ching cribbage dm factor fish fortune hack hangman \
	larn mille monop morse number phantasia pig pom ppt primes quiz \
	rain random robots rogue sail snake tetris trek wargames worm worms \
	wump

.if exists(${X11BASE}/lib/libX11.a})
SUBDIR+= xneko xroach
.endif

.include <bsd.subdir.mk>
