SHELL=	/bin/sh
ED=	../obj/ed

all: check
	@:

check: build test
	@if grep -h '\*\*\*' errs.o scripts.o; then :; else \
		echo "tests completed successfully."; \
	fi

build: mkscripts.sh
	@if [ -f errs.o ]; then :; else \
		echo "building test scripts for $(ED) ..."; \
		$(SHELL) mkscripts.sh $(ED); \
	fi

test: build ckscripts.sh
	@echo testing $(ED) ...
	@$(SHELL) ckscripts.sh $(ED)

clean:
	rm -f *.ed *.red *.[oz] *~
