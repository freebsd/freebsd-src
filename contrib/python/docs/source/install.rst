Installation
===================================

**Prerequisites**

Python 2.4 or higher, SWIG 1.3 or higher, GNU make

**Download**

You can download the source codes `here`_.
The latest release is 1.4.1, Jan 15, 2009.

.. _here: ldns-1.4.1-py.tar.gz

**Compiling**

After downloading, you can compile the library by doing::

	> tar -xzf ldns-1.4.1-py.tar.gz
	> cd ldns-1.4.1
	> ./configure --with-pyldns
	> make

You need GNU make to compile pyLDNS; SWIG and Python development libraries to compile extension module. 


**Testing**

If the compilation is successfull, you can test the python LDNS extension module by::

	> cd contrib/python
	> make testenv
	> ./ldns-mx.py

This will start a new shell, during which the symbolic links will be working. 
When you exit the shell, then symbolic links will be deleted. 

In ``contrib/examples`` you can find many simple applications in python which demostrates the capabilities of LDNS library.

**Installation**

To install libraries and extension type::

	> cd ldns-1.4.1
	> make install

