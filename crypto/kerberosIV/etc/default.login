#
# Sample /etc/default/login file, read by the login program
#
# For more info consult SysV login(1)
#
# Most things are environment variables.
# HZ and TZ are set only if they are still uninitialized.

# This really variable TZ
#TIMEZONE=EST5EDT

#HZ=100

# File size limit, se ulimit(2).
# Note that the limit must be specified in units of 512-byte blocks.
#ULIMIT=0

# If CONSOLE is set, root can only login on that device.
# When not set root can log in on any device.
#CONSOLE=/dev/console

# PASSREQ determines if login requires a password.
PASSREQ=YES

# ALTSHELL, really set SHELL=/bin/bash or other shell
# Extension: when ALTSHELL=YES, we set the SHELL variable even if it is /bin/sh
ALTSHELL=YES

# Default PATH
#PATH=/usr/bin:

# Default PATH for root user
#SUPATH=/usr/sbin:/usr/bin

# TIMEOUT sets the number of seconds (between 0 and 900) to wait before
# abandoning a login session.
# 
#TIMEOUT=300

# Use this for default umask(2) value
#UMASK=022

# Sleeptime between failed logins
# SLEEPTIME

# Maximum number of failed login attempts, well the user can always reconnect
# MAXTRYS
