
.include "Makefile.boot.pre"
# Don't build shared libraries during bootstrap
NO_PIC=	yes
.include "../../../share/mk/bsd.lib.mk"
.include "Makefile.boot"
