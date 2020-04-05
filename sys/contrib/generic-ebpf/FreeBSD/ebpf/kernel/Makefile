.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../dev/ebpf

IDENT  != uname -i

CFLAGS += -I$(KSRC)/sys/$(MACHINE)/compile/$(IDENT)
CFLAGS += -I../../
CFLAGS += -Wall
CFLAGS += -std=c99

KMOD    = ebpf

SRCS += ebpf_allocator.c
SRCS += ebpf_env.c
SRCS += ebpf_freebsd.c
SRCS += ebpf_interpreter.c
SRCS += ebpf_map.c
SRCS += ebpf_map_array.c
SRCS += ebpf_map_hashtable.c
SRCS += ebpf_obj.c
SRCS += ebpf_prog.c

realinstall:
	install ebpf.ko $(DESTDIR)

.include <bsd.kmod.mk>
