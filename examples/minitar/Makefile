
#
# Adjust the following to control which options minitar gets
# built with.  See comments in minitar.c for details.
#
CFLAGS=				\
	-DNO_BZIP2_CREATE	\
	-I../../libarchive	\
	-g

#
# You may need to add additional libraries or link options here
# For example, many Linux systems require -lacl
#
LIBS= -lz -lbz2

# How to link against libarchive.
LIBARCHIVE=	../../libarchive/libarchive.a

all: minitar

minitar: minitar.o
	cc -g -o minitar minitar.o $(LIBARCHIVE) $(LIBS)
	strip minitar
	ls -l minitar

minitar.o: minitar.c

clean::
	rm -f *.o
	rm -f minitar
	rm -f *~
