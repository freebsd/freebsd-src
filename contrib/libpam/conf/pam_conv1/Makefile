#
#
ifeq ($(OS),solaris)

clean:
      @echo not available in Solaris

all:
      @echo not available in Solaris

install:
      @echo not available in Solaris

else

all: pam_conv1

pam_conv1: pam_conv.tab.c lex.yy.c
	$(CC) -o pam_conv1 pam_conv.tab.c -lfl

pam_conv.tab.c: pam_conv.y lex.yy.c
	bison pam_conv.y

lex.yy.c: pam_conv.lex
	flex pam_conv.lex

lclean:
	rm -f core pam_conv1 lex.yy.c pam_conv.tab.c *.o *~
	rm -rf ./pam.d pam_conv.output

clean: lclean

install: pam_conv1
	cp -f ./pam_conv1 ../../bin

endif

remove:
	rm -f ../../bin/pam_conv1

extraclean: remove clean
