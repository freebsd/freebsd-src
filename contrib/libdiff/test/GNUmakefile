.PHONY: test verify clean
test: verify clean

# verify_all.sh runs 'make' on sub-directories containing C tests
verify:
	./verify_all.sh
clean:
	-rm verify.*
	-$(MAKE) -C ../lib clean
	-$(MAKE) -C arraylist_test clean
	-$(MAKE) -C results_test clean
