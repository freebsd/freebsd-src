
STRIP=   
PROG= adduser


afterinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 userids /etc
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 adduser.conf /etc

cleandir:
clean:
obj:

.include <bsd.prog.mk>

