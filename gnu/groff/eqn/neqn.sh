#!/bin/sh
# Provision of this shell script should not be taken to imply that use of
# GNU eqn with groff -Tascii|-Tlatin1 is supported.

exec @g@eqn -Tascii ${1+"$@"}
