# NB. libssh and libopenbsd-compat should be built with the same sanitizer opts.
CC=clang-16
CXX=clang++-16
FUZZ_FLAGS=-fsanitize=address,fuzzer -fno-omit-frame-pointer
FUZZ_LIBS=-L/usr/lib/llvm-16/lib -lFuzzer

CXXFLAGS=-O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-exceptions -I ../../.. $(FUZZ_FLAGS)
CFLAGS=$(CXXFLAGS)
LDFLAGS=-L ../../.. -L ../../../openbsd-compat -g $(FUZZ_FLAGS)
LIBS=-lssh -lopenbsd-compat -lmd -lcrypto -lfido2 -lcbor $(FUZZ_LIBS)
SK_NULL_OBJS=ssh-sk-null.o
COMMON_DEPS=../../../libssh.a

TARGETS=pubkey_fuzz sig_fuzz authopt_fuzz authkeys_fuzz sshsig_fuzz \
	sshsigopt_fuzz privkey_fuzz kex_fuzz agent_fuzz

all: $(TARGETS)

.cc.o:
	$(CXX) $(CXXFLAGS) -c $< -o $@

pubkey_fuzz: pubkey_fuzz.o $(SK_NULL_OBJS) $(COMMON_DEPS)
	$(CXX) -o $@ pubkey_fuzz.o $(SK_NULL_OBJS) $(LDFLAGS) $(LIBS)

sig_fuzz: sig_fuzz.o $(SK_NULL_OBJS) $(COMMON_DEPS)
	$(CXX) -o $@ sig_fuzz.o $(SK_NULL_OBJS) $(LDFLAGS) $(LIBS)

authopt_fuzz: authopt_fuzz.o $(SK_NULL_OBJS) $(COMMON_DEPS)
	$(CXX) -o $@ authopt_fuzz.o $(SK_NULL_OBJS) ../../../auth-options.o $(LDFLAGS) $(LIBS)

authkeys_fuzz: authkeys_fuzz.o $(SK_NULL_OBJS) $(COMMON_DEPS)
	$(CXX) -o $@ authkeys_fuzz.o $(SK_NULL_OBJS) ../../../auth-options.o ../../../auth2-pubkeyfile.o $(LDFLAGS) $(LIBS)

sshsig_fuzz: sshsig_fuzz.o $(SK_NULL_OBJS) $(COMMON_DEPS)
	$(CXX) -o $@ sshsig_fuzz.o $(SK_NULL_OBJS) ../../../sshsig.o $(LDFLAGS) $(LIBS)

sshsigopt_fuzz: sshsigopt_fuzz.o $(SK_NULL_OBJS) $(COMMON_DEPS)
	$(CXX) -o $@ sshsigopt_fuzz.o $(SK_NULL_OBJS) ../../../sshsig.o $(LDFLAGS) $(LIBS)

privkey_fuzz: privkey_fuzz.o $(SK_NULL_OBJS) $(COMMON_DEPS)
	$(CXX) -o $@ privkey_fuzz.o $(SK_NULL_OBJS) $(LDFLAGS) $(LIBS)

kex_fuzz: kex_fuzz.o $(SK_NULL_OBJS) $(COMMON_DEPS)
	$(CXX) -o $@ kex_fuzz.o $(SK_NULL_OBJS) $(LDFLAGS) $(LIBS) -lz

agent_fuzz: agent_fuzz.o agent_fuzz_helper.o sk-dummy.o ../../../ssh-sk.o $(COMMON_DEPS)
	$(CXX) -o $@ agent_fuzz.o agent_fuzz_helper.o sk-dummy.o ../../../ssh-sk.o $(LDFLAGS) $(LIBS) -lz

agent_fuzz_helper.o: agent_fuzz_helper.c ../../../ssh-agent.c

sk-dummy.o: ../sk-dummy/sk-dummy.c
	$(CC) $(CFLAGS) -c -o $@ ../sk-dummy/sk-dummy.c -DSK_DUMMY_INTEGRATE=1 $(LDFLAGS)

clean:
	-rm -f *.o $(TARGETS)
