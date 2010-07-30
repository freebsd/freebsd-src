#! /bin/sh
#
# Copyright 2007. Petar Zhivkov Petrov 
# pesho.petrov@gmail.com
#
# $FreeBSD$

usage() {
	echo "Usage: $0 clientName serverName"
	echo "       $0 -v"
}

countChars() {
    _count="`echo "$1" | sed -e "s/[^$2]//g" | tr -d "\n" | wc -c`"
	return 0
}

readPassword() {
	while [ true ]; do
		stty -echo
		read -p "$1" _password
		stty echo
		echo ""
		countChars "$_password" ":"
		if [ $_count != 0 ]; then
			echo "Sorry, password must not contain \":\" characters"
			echo ""
		else
			break
		fi
	done
	return 0
}

makeSecret() {
	local clientLower="`echo "$1" | tr "[:upper:]" "[:lower:]"`"
	local serverLower="`echo "$2" | tr "[:upper:]" "[:lower:]"`"
	local secret="`md5 -qs "$clientLower:$serverLower:$3"`"
	_secret="\$md5\$$secret"
}

if [ $# -eq 1 -a "X$1" = "X-v" ]; then
	echo "Csup authentication key generator"
	usage
	exit
elif [ $# -ne 2 ]; then
	usage
	exit
fi

clientName=$1
serverName=$2

#
# Client name must contain exactly one '@' and at least one '.'.
# It must not contain a ':'.
#

countChars "$clientName" "@"
aCount=$_count

countChars "$clientName" "."
dotCount=$_count
if [ $aCount -ne 1 -o $dotCount -eq 0 ]; then
	echo "Client name must have the form of an e-mail address,"
	echo "e.g., \"user@domain.com\""
	exit
fi

countChars "$clientName" ":"
colonCount=$_count
if [ $colonCount -gt 0 ]; then
	echo "Client name must not contain \":\" characters"
	exit
fi

#
# Server name must not contain '@' and must have at least one '.'.
# It also must not contain a ':'.
#

countChars "$serverName" "@"
aCount=$_count

countChars "$serverName" "."
dotCount=$_count
if [ $aCount != 0 -o $dotCount = 0 ]; then
	echo "Server name must be a fully-qualified domain name."
	echo "e.g., \"host.domain.com\""
	exit
fi

countChars "$serverName" ":"
colonCount=$_count
if [ $colonCount -gt 0 ]; then
	echo "Server name must not contain \":\" characters"
	exit
fi

#
# Ask for password and generate secret.
#

while [ true ]; do
	readPassword "Enter password: "
	makeSecret "$clientName" "$serverName" "$_password"
	secret=$_secret

	readPassword "Enter same password again: "
	makeSecret "$clientName" "$serverName" "$_password"
	secret2=$_secret

	if [ "X$secret" = "X$secret2" ]; then
		break
	else
		echo "Passwords did not match.  Try again."
		echo ""
	fi
done

echo ""
echo "Send this line to the server administrator at $serverName:"
echo "-------------------------------------------------------------------------------"
echo "$clientName:$secret::"
echo "-------------------------------------------------------------------------------"
echo "Be sure to send it using a secure channel!"
echo ""
echo "Add this line to your file \"$HOME/.csup/auth\", replacing \"XXX\""
echo "with the password you typed in:"
echo "-------------------------------------------------------------------------------"
echo "$serverName:$clientName:XXX:"
echo "-------------------------------------------------------------------------------"
echo "Make sure the file is readable and writable only by you!"
echo ""

