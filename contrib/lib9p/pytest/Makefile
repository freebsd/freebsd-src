PYTHON?=python

selftest:
	for f in lerrno p9err pfod protocol sequencer; do \
	    ${PYTHON} $$f.py; \
	done

clean cleandir:
	rm -rf *.pyc __pycache__ *.log
