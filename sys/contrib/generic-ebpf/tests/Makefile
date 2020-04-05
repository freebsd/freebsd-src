all clean:
	for D in ${SUBDIR}; do (cd $$D && make $@); done
check-all: check check-kern
check:
	for D in ${SUBDIR_CU}; do (cd $$D && make $@); done
check-kern:
	for D in ${SUBDIR_CK}; do (cd $$D && make check); done
.PHONY: all clean check check-all check-kern

install:

SUBDIR= ebpf_prog_tests ebpf_map_tests

SUBDIR_CU=	ebpf_prog_tests ebpf_map_tests
