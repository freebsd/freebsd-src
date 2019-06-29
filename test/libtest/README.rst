=====================================
test(3) - a library for writing tests
=====================================

The ``test(3)`` API and its related scaffolding generator
(`make-test-scaffolding(1) <mts_>`_) work together to reduce the
boilerplate needed for tests.

.. _mts: bin/make-test-scaffolding

Quick Start
===========

The following source code defines a test suite that contains a single
test:

.. code:: c

	/* File: test.c */
	#include "test.h"

	enum test_result
	tf_goodbye_world(testcase_state tcs)
	{
		return (TEST_PASS);
	}

By convention, test functions are named using a ``tf_`` prefix.

Given an object file compiled from this source, the
`make-test-scaffolding(1) <mts_>`_ utility would generate scaffolding
describing a single invocable test named "``goodbye_world``".

Test Cases
----------

Test functions that are related to each other can be grouped into test
cases.  The following code snippet defines a test suite with two test
functions contained in a test case named "``helloworld``":

.. code:: c

	/* File: test.c */
	#include "test.h"

	TEST_CASE_DESCRIPTION(helloworld) =
	    "A description of the helloworld test case.";

	enum test_result
	tf_helloworld_hello(testcase_state tcs)
	{
		return (TEST_PASS);
	}

	enum test_result
	tf_helloworld_goodbye(testcase_state tcs)
	{
		return (TEST_FAIL);
	}

Test cases can define their own set up and tear down functions:

.. code:: c

	/* File: test.c continued. */
	struct helloworld_test { .. state used by the helloworld tests .. };

	enum testcase_status
	tc_setup_helloworld(testcase_state *tcs)
	{
		*tcs = ..allocate a struct helloworld_test.. ;
		return (TEST_CASE_OK);
	}

	enum testcase_status
	tc_teardown_helloworld(testcase_state tcs)
	{
		.. deallocate test case state..
		return (TEST_CASE_OK);
	}

The set up function for a test case will be invoked prior to any of
the functions that are part of the test case.  The set up function can
allocate test-specific state, which is then passed to each test function
for its use.

The tear down function for a test case will be invoked after the test
functions in the test case are invoked.  This function is responsible for
deallocating the resources allocated by its corresponding set up function.

Building Tests
--------------

Within the `Elftoolchain Project`_'s sources, the ``elftoolchain.test.mk``
rule set handles the process of invoking the `make-test-scaffolding(1)
<mts_>`_ utility and building an test executable.

.. code:: make

	# Example Makefile.

	TOP=	..path to the top of the elftoolchain source tree..

	TEST_SRCS=	test.c

	.include "$(TOP)/mk/elftoolchain.test.mk"


.. _Elftoolchain Project: http://elftoolchain.sourceforge.net/

Further Reading
===============

- The `test(3) <lib/test.3>`_ manual page.
- The `make-test-scaffolding(1) <bin/make-test-scaffolding.1>`_ manual page.
- `Example code <examples/>`_.
