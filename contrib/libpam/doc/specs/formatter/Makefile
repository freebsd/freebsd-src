LIBS=-lfl

padout: parse.tab.o
	$(CC) -o padout parse.tab.o $(LIBS)

parse.tab.o: parse.tab.c lex.yy.c
	$(CC) -c parse.tab.c

parse.tab.c: parse.y
	bison parse.y

lex.yy.c: parse.lex
	flex parse.lex

clean:
	rm -f parse.tab.o parse.tab.c lex.yy.c padout *~ core
