SUBDIR=lib add create delete info

.include <bsd.subdir.mk>

dists:  bin_dist

bin_dist: all install
	tar czf pkg_install.tar.gz -C /	\
	usr/local/bin/pkg_add		\
	usr/local/bin/pkg_create	\
	usr/local/bin/pkg_delete	\
	usr/local/bin/pkg_info		\
	usr/local/man/man1/pkg_add.1	\
	usr/local/man/man1/pkg_create.1	\
	usr/local/man/man1/pkg_delete.1	\
	usr/local/man/man1/pkg_info.1
