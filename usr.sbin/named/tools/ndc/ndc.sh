#!/bin/sh

USAGE='echo \
	"usage: $0 \
 (status|dumpdb|reload|stats|trace|notrace|querylog|start|stop|restart) \
	 ... \
	"; exit 1'

PATH=%DESTSBIN%:/bin:/usr/bin:$PATH
PIDFILE=%PIDDIR%/named.pid

if [ -f $PIDFILE ]
then
	PID=`cat $PIDFILE`
	PS=`%PS% $PID | tail -1 | grep $PID`
	RUNNING=1
	[ `echo $PS | wc -w` -ne 0 ] || {
		PS="named (pid $PID?) not running"
		RUNNING=0
	}
else
	PS="named (no pid file) not running"
	RUNNING=0
fi

for ARG
do
	case $ARG in
	start|stop|restart)
		;;
	*)
		[ $RUNNING -eq 0 ] && {
			echo $PS
			exit 1
		}
	esac

	case $ARG in
	status)	echo "$PS";;
	dumpdb)	kill -INT $PID && echo Dumping Database;;
	reload)	kill -HUP $PID && echo Reloading Database;;
	stats)	kill -%IOT% $PID && echo Dumping Statistics;;
	trace)	kill -USR1 $PID && echo Trace Level Incremented;;
	notrace) kill -USR2 $PID && echo Tracing Cleared;;
	querylog|qrylog) kill -WINCH $PID && echo Query Logging Toggled;;
	start)
		[ $RUNNING -eq 1 ] && {
			echo "$0: start: named (pid $PID) already running"
			continue
		}
		# If there is a global system configuration file, suck it in.
		if [ -f /etc/sysconfig ]; then
			. /etc/sysconfig
		fi
		rm -f $PIDFILE
		# $namedflags is imported from /etc/sysconfig
		if [ "X${namedflags}" != "XNO" ]; then 
			%INDOT%named ${namedflags} && {
				sleep 5
				echo Name Server Started
			}
		fi
		;;
	stop)
		[ $RUNNING -eq 0 ] && {
			echo "$0: stop: named not running"
			continue
		}
		kill $PID && {
			sleep 5
			rm -f $PIDFILE
			echo Name Server Stopped
		}
		;;
	restart)
		[ $RUNNING -eq 1 ] && {
			kill $PID && sleep 5
		}
		# If there is a global system configuration file, suck it in.
		if [ -f /etc/sysconfig ]; then
			. /etc/sysconfig
		fi
		rm -f $PIDFILE
		# $namedflags is imported from /etc/sysconfig
		if [ "X${namedflags}" != "XNO" ]; then 
			%INDOT%named ${namedflags} && {
				sleep 5
				echo Name Server Restarted
			}
		fi
		;;
	*)	eval "$USAGE";;
	esac
done
test -z "$ARG" && eval "$USAGE"

exit 0
