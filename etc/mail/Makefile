# $FreeBSD$

all: access.db mailertable.db virtusertable.db	\
# /etc/mail/aliases.db

access.db: access
	/usr/sbin/makemap hash access < access

virtusertable.db: virtusertable
	/usr/sbin/makemap hash virtusertable < virtusertable

mailertable.db: mailertable
	/usr/sbin/makemap hash mailertable < mailertable

#/etc/mail/aliases.db: /etc/mail/aliases
#	newaliases

mailertable:
	@echo Generating empty mailertable
	sed -e 's/^/#/' < mailertable.sample > mailertable

access:
	@echo Generating empty access
	sed -e 's/^/#/' < access.sample > access

virtusertable:
	@echo Generating empty virtusertable
	sed -e 's/^/#/' < virtusertable.sample > virtusertable

clean:
	rm -f access.db virtusertable.db mailertable.db
