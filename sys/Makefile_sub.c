/**/#	From: /usr/src/sys/Makefile_sub.c		V1.4	<stacey@guug.de>
/**/#	$Id: Makefile_sub.c,v 1.1 1994/01/22 07:34:04 rgrimes Exp $

/**/#	Do not edit Makefile_sub, (produced from Makefile_sub.c by Makefile).

/**/#	Copyright: Julian Stacey, Munich, October 1993,
/**/#	Free Software - No Liability Accepted.

/**/#	This Makefile_sub.c is not known as Makefile.c for 2 reasons:
/**/#		recursive make depend might zap Makefile
/**/#		general cleaning shells might also zap Makefile

all:
	@echo Subsidiary makefile has been erroneously called directly.
	@echo It should only be called from Makefile.
	exit 1
	/usr/src/sys/impossible_command

/**/# Next label recreates a compile tree if:
/**/#	- The config description file changes,
/**/#		for example if /sys/compile/GENERICAH/Makefile is older than 
/**/#		/sys/i386/conf/GENERICAH, /sys/compile/GENERICAH/* is rebuilt.
/**/#	- Changes occur to i386/conf/Makefile.i386 devices.i386 files.i386.
/**/#	- A new source tree is imported.  This might not be strictly necessary,
/**/#	  If the Makefile know of Every dependency, but as the kernel
/**/#	  evolves rapidly, it seems a harmless safety net.
/**/# 	- Changes occur to Makefile or Makefile_sub.
CONFIG_TREE_TEST: ${MACHINE}/conf/CONFIG_NAME	\
		${MACHINE}/conf/Makefile.${MACHINE} \
		${MACHINE}/conf/devices.${MACHINE} \
		${MACHINE}/conf/files.${MACHINE} \
		.config.import \
		Makefile MAKEFILE_KERNEL
/**/.if defined(DEBUG_SYS)
	@echo -n Configuring a kernel compilation tree for CONFIG_NAME
	@echo -n " as defined in "
	@echo ${MACHINE}/conf/CONFIG_NAME.
/**/.endif
	cd ${MACHINE}/conf ; config CONFIG_NAME
/**/#	# If config fails, no error code is seen here unfortunately.
/**/#	# This was reccomended for flexfax, may not be necessary now:
/**/#	#	-rm -f compile/CONFIG_NAME/fifo.h compile/CONFIG_NAME/\*.o 
/**/.if defined(LOAD_ADDRESS)
	cd compile/CONFIG_NAME ; make LOAD_ADDRESS=${LOAD_ADDRESS} depend
/**/.else
	cd compile/CONFIG_NAME ; make depend
/**/.endif

/**/.config.import:
	touch $@

/**/#	End of Makefile_sub.c
