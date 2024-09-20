# NB. libssh and libopenbsd-compat should be built with the same sanitizer opts.
CC=clang-16
CXX=clang++-16
FUZZ_FLAGS=-fsanitize=address,fuzzer -fno-omit-frame-pointer
FUZZ_LIBS=-L/usr/lib/llvm-16/lib -lFuzzer

CFLAGS=-D_GNU_SOURCE=1 -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-exceptions -Wno-deprecated -I ../../..
CXXFLAGS=$(CFLAGS) $(FUZZ_FLAGS)
LDFLAGS=-L ../../.. -L ../../../openbsd-compat -g
LIBS=-lssh -lopenbsd-compat -lmd -lcrypto -lfido2 -lcbor $(FUZZ_LIBS)
SK_NULL_OBJS=ssh-sk-null.o
COMMON_DEPS=../../../libssh.a

TARGETS=pubkey_fuzz sig_fuzz authopt_fuzz authkeys_fuzz sshsig_fuzz \
	sshsigopt_fuzz privkey_fuzz kex_fuzz agent_fuzz \
	mkcorpus_sntrup761 sntrup761_enc_fuzz sntrup761_dec_fuzz

all: $(TARGETS)

.cc.o:
	$(CXX) $(CXXFLAGS) -c $< -o $@

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

pubkey_fuzz: pubkey_fuzz.o $(SK_NULL_OBJS) $(COMMON_DEPS)
	$(CXX) -o $@ pubkey_fuzz.o $(SK_NULL_OBJS) $(LDFLAGS) $(FUZZ_FLAGS) $(LIBS)

sig_fuzz: sig_fuzz.o $(SK_NULL_OBJS) $(COMMON_DEPS)
	$(CXX) -o $@ sig_fuzz.o $(SK_NULL_OBJS) $(LDFLAGS) $(FUZZ_FLAGS) $(LIBS)

authopt_fuzz: authopt_fuzz.o $(SK_NULL_OBJS) $(COMMON_DEPS)
	$(CXX) -o $@ authopt_fuzz.o $(SK_NULL_OBJS) ../../../auth-options.o $(LDFLAGS) $(FUZZ_FLAGS) $(LIBS)

authkeys_fuzz: authkeys_fuzz.o $(SK_NULL_OBJS) $(COMMON_DEPS)
	$(CXX) -o $@ authkeys_fuzz.o $(SK_NULL_OBJS) ../../../auth-options.o ../../../auth2-pubkeyfile.o $(LDFLAGS) $(FUZZ_FLAGS) $(LIBS)

sshsig_fuzz: sshsig_fuzz.o $(SK_NULL_OBJS) $(COMMON_DEPS)
	$(CXX) -o $@ sshsig_fuzz.o $(SK_NULL_OBJS) ../../../sshsig.o $(LDFLAGS) $(FUZZ_FLAGS) $(LIBS)

sshsigopt_fuzz: sshsigopt_fuzz.o $(SK_NULL_OBJS) $(COMMON_DEPS)
	$(CXX) -o $@ sshsigopt_fuzz.o $(SK_NULL_OBJS) ../../../sshsig.o $(LDFLAGS) $(FUZZ_FLAGS) $(LIBS)

privkey_fuzz: privkey_fuzz.o $(SK_NULL_OBJS) $(COMMON_DEPS)
	$(CXX) -o $@ privkey_fuzz.o $(SK_NULL_OBJS) $(LDFLAGS) $(FUZZ_FLAGS) $(LIBS)

kex_fuzz: kex_fuzz.o $(SK_NULL_OBJS) $(COMMON_DEPS)
	$(CXX) -o $@ kex_fuzz.o $(SK_NULL_OBJS) $(LDFLAGS) $(FUZZ_FLAGS) $(LIBS) -lz

agent_fuzz: agent_fuzz.o agent_fuzz_helper.o sk-dummy.o ../../../ssh-sk.o $(COMMON_DEPS)
	$(CXX) -o $@ agent_fuzz.o agent_fuzz_helper.o sk-dummy.o ../../../ssh-sk.o $(LDFLAGS) $(FUZZ_FLAGS) $(LIBS) -lz

agent_fuzz_helper.o: agent_fuzz_helper.c ../../../ssh-agent.c

sk-dummy.o: ../sk-dummy/sk-dummy.c
	$(CC) $(CFLAGS) -c -o $@ ../sk-dummy/sk-dummy.c -DSK_DUMMY_INTEGRATE=1 $(LDFLAGS) $(FUZZ_FLAGS)

mkcorpus_sntrup761: mkcorpus_sntrup761.o
	$(CC) -o $@ mkcorpus_sntrup761.o $(LDFLAGS) -lcrypto

sntrup761_dec_fuzz: sntrup761_dec_fuzz.o
	$(CXX) -o $@ sntrup761_dec_fuzz.o $(LDFLAGS) $(FUZZ_FLAGS) $(FUZZ_LIBS) -lcrypto

sntrup761_enc_fuzz: sntrup761_enc_fuzz.o
	$(CXX) -o $@ sntrup761_enc_fuzz.o $(LDFLAGS) $(FUZZ_FLAGS) $(FUZZ_LIBS) -lcrypto

clean:
	-rm -f *.o $(TARGETS)

cleandir: clean

