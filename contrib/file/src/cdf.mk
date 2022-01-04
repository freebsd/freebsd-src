CFLAGS+=-DTEST -DCDF_DEBUG -g -DHAVE_CONFIG_H -I..
cdf: cdf.o cdf_time.o
	${CC} ${CFLAGS} -o $@ $>
