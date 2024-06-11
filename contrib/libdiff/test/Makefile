.PHONY: test verify clean

test: verify clean

# verify_all.sh runs 'make' on sub-directories containing C tests
verify:
	./verify_all.sh

clean:
	-rm verify.*
	make -C arraylist_test clean
	make -C results_test clean
