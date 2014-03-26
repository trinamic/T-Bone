#!/bin/sh

### BEGIN INIT INFO
# Provides: t_bone_server
# Required-Start: 
# Required-Stop: 
# Default-Start: 2 3 4 5
# Default-Stop: 0 1 6
# Short-Description: Media remote for Pi
# Description: T Bone Web Interface
### END INIT INFO

#link this script as /etc/init.d/t_bone_server.sh

#adapt to the install location of the t-bone script
DIR=<set>

#this should be ok without change
DAEMON=$DIR/src/t_bone/t_bone_server.py
DAEMON_NAME=t_bone_server

# This next line determines what user the script runs as.
# Root generally not recommended but necessary if you are using the Raspberry Pi GPIO from Python.
DAEMON_USER=root

# The process ID of the script when it runs is stored here:
PIDFILE=/var/run/$DAEMON_NAME.pid

. /lib/lsb/init-functions

do_start () {
	log_daemon_msg "Starting system $DAEMON_NAME daemon"
	start-stop-daemon --start --background --pidfile $PIDFILE --make-pidfile --user $DAEMON_USER --startas $DAEMON
	log_end_msg $?
}
do_stop () {
	log_daemon_msg "Stopping system $DAEMON_NAME daemon"
	start-stop-daemon --stop --pidfile $PIDFILE --retry 10
	log_end_msg $?
}

case "$1" in
	start|stop)
		do_${1}
		;;

	restart|reload|force-reload)
		do_stop
		do_start
		;;

	status)
		status_of_proc "$DAEMON_NAME" "$DAEMON" && exit 0 || exit $?
		;;
	*)

	echo "Usage: /etc/init.d/$DEAMON_NAME {start|stop|restart|status}"
	exit 1
	;;
esac
exit