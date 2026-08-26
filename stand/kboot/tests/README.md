# kboot HTTP tests

Local pytest harness for kboot HTTP/HTTPS support.  It builds a
`KBOOT_HTTP_TEST=yes` loader.kboot and runs it on FreeBSD with linuxulator.

Before running the suite, enable linuxulator:

```sh
service linux onestart
```

To build the test loader and run the tests:

```sh
make test
```
