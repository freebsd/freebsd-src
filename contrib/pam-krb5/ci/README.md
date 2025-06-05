# Continuous Integration

The files in this directory are used for continuous integration testing.
`ci/install` installs the prerequisite packages (run as root on a Debian
derivative), and `ci/test` runs the tests.

Most tests will be skipped without a Kerberos configuration.  The scripts
`ci/kdc-setup-heimdal` and `ci/kdc-setup-mit` will (when run as root on a
Debian derivative) set up a Heimdal or MIT Kerberos KDC, respectively, and
generate the files required to run the complete test suite.

Tests are run automatically via GitHub Actions workflows using these
scripts and the configuration in the `.github/workflows` directory.
