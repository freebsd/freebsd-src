TESTS = test corner ensor getch ncurses hanoi knight rain worm \
	over scroll2 battle newdemo scroll3 xmas copy firework testcurs \
	scroll

CLEANFILES += $(TESTS)

LIBS += -lncurses -lmytinfo

all: $(TESTS)

$(TESTS):
	$(CC) $(CFLAGS) $(LDFLAGS) ${.CURDIR}/$@.c -o $@ $(LIBS)

test: test.c
corner: corner.c
ensor: ensor.c
getch: getch.c
ncurses: ncurses.c
hanoi: hanoi.c
knight: knight.c
rain: rain.c
worm: worm.c
over: over.c
scroll2: scroll2.c
battle: battle.c
newdemo: newdemo.c
scroll3: scroll3.c
xmas: xmas.c
copy: copy.c
firework: firework.c
testcurs: testcurs.c
scroll: scroll.c

.include <bsd.prog.mk>

