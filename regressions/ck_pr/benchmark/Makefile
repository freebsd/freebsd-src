.PHONY: clean

all: ck_pr_cas_64 ck_pr_fas_64 ck_pr_cas_64_2 fp

fp: fp.c
	$(CC) $(CFLAGS) -o fp fp.c

ck_pr_cas_64_2: ck_pr_cas_64_2.c
	$(CC) $(CFLAGS) -o ck_pr_cas_64_2 ck_pr_cas_64_2.c -lm

ck_pr_cas_64: ck_pr_cas_64.c
	$(CC) $(CFLAGS) -o ck_pr_cas_64 ck_pr_cas_64.c -lm

ck_pr_fas_64: ck_pr_fas_64.c
	$(CC) $(CFLAGS) -o ck_pr_fas_64 ck_pr_fas_64.c -lm

clean:
	rm -rf ck_pr_cas_64 ck_pr_fas_64 ck_pr_cas_64_2 *.dSYM *.exe

include ../../../build/regressions.build
CFLAGS+=$(PTHREAD_CFLAGS) -D_GNU_SOURCE
