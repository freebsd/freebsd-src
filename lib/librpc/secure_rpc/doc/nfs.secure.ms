.\" Must use  --  pic tbl eqn  --   with this one.
.\"
.\" @(#)nfs.secure.ms	2.2 88/08/09 4.0 RPCSRC
.de BT
.if \\n%=1 .tl ''- % -''
..
.ND
.\" prevent excess underlining in nroff
.if n .fp 2 R
.OH 'Secure Networking''Page %'
.EH 'Page %''Secure Networking'
.if \\n%=1 .bp
.EQ
delim $$
gsize 11
.EN
.SH
\&Secure Networking
.nr OF 1
.IX "security" "of networks" "" "" PAGE START
.IX "network security" "" "" "" PAGE START
.IX "NFS security" "" "" "" PAGE START
.LP
RPCSRC 4.0 includes an authentication system
that greatly improves the security of network environments.
The system is general enough to be used by other
.UX
and non-UNIX systems.
The system uses DES encryption and public key cryptography
to authenticate both users and machines in the network.
(DES stands for Data Encryption Standard.)
.LP
Public key cryptography is a cipher system that involves two keys:
one public and the other private.
The public key is published, while the private key is not;
the private (or secret) key is used to encrypt and decrypt data.
Sun's system differs from some other public key cryptography systems
in that the public and secret keys are used to generate a common key,
which is used in turn to create a DES key.
DES is relatively fast,
and on Sun Workstations,
optional hardware is available to make it even faster.
.#
.NH 0
\&Administering Secure RPC
.IX "administering secure RPC"
.IX "security" "RPC administration"
.LP
This section describes what the system administrator must do
in order to use secure networking.
.IP 1
RPCSRC now includes the
.I /etc/publickey
.IX "etc/publickey" "" "\&\fI/etc/publickey\fP"
database, which should contain three fields for each user:
the user's netname, a public key, and an encrypted secret key.
The corresponding Yellow Pages map is available to YP clients as
.I publickey.byname
but the database should reside only on the YP master.  Make sure
.I /etc/netid
exists on the YP master server.
As normally installed, the only user is
.I nobody .
This is convenient administratively,
because users can establish their own public keys using
.I chkey (1)
.IX "chkey command" "" "\&\fIchkey\fP command"
without administrator intervention.
For even greater security,
the administrator can establish public keys for everyone using
.I newkey (8).
.IX "newkey command" "" "\&\fInewkey\fP command"
Note that the Yellow Pages take time to propagate a new map,
so it's a good idea for users to run
.I chkey ,
or for the administrator to run
.I newkey ,
just before going home for the night.
.IP 2
Verify that the
.I keyserv (8c)
.IX "keyserv daemon" "" "\&\fIkeyserv\fP daemon"
daemon was started by
.I /etc/rc.local
and is still running.
This daemon performs public key encryption
and stores the private key (encrypted, of course) in
.I /etc/keystore :
.DS
% \fBps aux | grep keyserv\fP
root   1354  0.0  4.1  128  296 p0 I  Oct 15 0:13 keyserv
.DE
When users log in with
.I login 
.IX "login command" "" "\&\fIlogin\fP command"
or remote log in with
.I rlogin ,
these programs use the typed password to decrypt the secret key stored in
.I /etc/publickey .
This becomes the private key, and gets passed to the
.I keyserv 
daemon.
If users don't type a password for
.I login 
or
.I rlogin ,
either because their password field is empty
or because their machine is in the
.I hosts\fR.\fPequiv 
.IX "etc/hosts.equiv" "" "\&\fI/etc/hosts.equiv\fP"
file of the remote host,
they can still place a private key in
.I /etc/keystore 
by invoking the
.I keylogin (1)
.IX "keylogin command" "" "\&\fIkeylogin\fP command"
program.
Administrators should take care not to delete
.I /etc/keystore 
and
.I /etc/.rootkey 
(the latter file contains the private key for
.I root ).
.IP 3
When you reinstall, move, or upgrade a machine, save
.I /etc/keystore 
and
.I /etc/.rootkey 
along with everything else you normally save.
.LP
.LP
Note that if you
.I login ,
.I rlogin ,
or
.I telnet 
to another machine, are asked for your password, and type it correctly,
you've given away access to your own account.
This is because your secret key is now stored in
.I /etc/keystore
on that remote machine.
This is only a concern if you don't trust the remote machine.
If this is the case,
don't ever log in to a remote machine if it asks for your password.
Instead, use NFS to remote mount the files you're looking for.
At this point there is no
.I keylogout 
command, even though there should be.
.LP
The remainder of this chapter discusses the theory of secure networking,
and is useful as a background for both users and administrators.
.#
.NH 1
\&Security Shortcomings of NFS
.IX "security" "shortcomings of NFS"
.LP
Sun's Remote Procedure Call (RPC) mechanism has proved to be a very 
powerful primitive for building network services.
The most well-known of these services is the Network File System (NFS),
a service that provides transparent file-sharing
between heterogeneous machine architectures and operating systems.
The NFS is not without its shortcomings, however. 
Currently, an NFS server authenticates a file request by authenticating the
machine making the request, but not the user. 
On NFS-based filesystems, it is a simple matter of running
.I su 
.IX "su command" "" "\&\fIsu\fP command"
to impersonate the rightful owner of a file.
But the security weaknesses of the NFS are nothing new. 
The familiar command
.I rlogin 
is subject to exactly the same attacks as the NFS
because it uses the same kind of authentication. 
.LP
A common solution to network security problems
is to leave the solution to each application.
A far better solution is to put authentication at the RPC level.
The result is a standard authentication system
that covers all RPC-based applications,
such as the NFS and the Yellow Pages (a name-lookup service).
Our system allows the authentication of users as well as machines.
The advantage of this is that it makes a network environment
more like the older time-sharing environment.
Users can log in on any machine,
just as they could log in on any terminal. 
Their login password is their passport to network security.
No knowledge of the underlying authentication system is required.
Our goal was a system that is as secure and easy to use
as a time-sharing system. 
.LP
Several remarks are in order.  Given
.I root 
access and a good knowledge of network programming,
anyone is capable of injecting arbitrary data into the network,
and picking up any data from the network.
However, on a local area network, no machine is capable of packet smashing \(en
capturing packets before they reach their destination, changing the contents, 
then sending packets back on their original course \(en
because packets reach all machines, including the server, at the same time.
Packet smashing is possible on a gateway, though,
so make sure you trust all gateways on the network.
The most dangerous attacks are those involving the injection of data,
such as impersonating a user by generating the right packets,
or recording conversations and replaying them later.
These attacks affect data integrity.
Attacks involving passive eavesdropping \(en
merely listening to network traffic without impersonating anybody \(en
are not as dangerous, since data integrity had not been compromised.
Users can protect the privacy of sensitive information
by encrypting data that goes over the network.
It's not easy to make sense of network traffic, anyway.
.#
.NH 1
\&RPC Authentication
.IX "RPC authentication"
.IX "authentication" "RPC"
.LP
RPC is at the core of the new network security system.
To understand the big picture,
it's necessary to understand how authentication works in RPC.
RPC's authentication is open-ended: 
a variety of authentication systems may be plugged into it
and may coexist on the network.
Currently, we have two: UNIX and DES. 
UNIX authentication is the older, weaker system;
DES authentication is the new system discussed in this chapter.
Two terms are important for any RPC authentication system:
.I credentials
and
.I verifiers .
Using ID badges as an example, the credential is what identifies a person:
a name, address, birth date, etc.
The verifier is the photo attached to the badge:
you can be sure the badge has not been stolen by checking the photo 
on the badge against the person carrying it.
In RPC, things are similar.
The client process sends both a credential and a verifier 
to the server with each RPC request.
The server sends back only a verifier,
since the client already knows the server's credentials.
.#
.NH 2
\&UNIX Authentication
.IX "UNIX authentication"
.IX "authentication" "UNIX"
.LP
UNIX authentication was used by most of Sun's original network services.
The credentials contain the client's machine-name, 
.I uid ,
.I gid ,
and group-access-list.
The verifier contains \fBnothing\fP!
There are two problems with this system.
The glaring problem is the empty verifier,
which makes it easy to cook up the right credential using
.I hostname 
.IX "hostname command" "" "\&\fIhostname\fP command"
and
.I su .
.IX "su command" "" "\&\fIsu\fP command"
If you trust all root users in the network, this is not really a problem.
But many networks \(en especially at universities \(en are not this secure.
The NFS tries to combat deficiencies in UNIX authentication 
by checking the source Internet address of
.I mount 
requests as a verifier of the
.I hostname 
field, and accepting requests only from privileged Internet ports.
Still, it is not difficult to circumvent these measures, 
and NFS really has no way to verify the user-ID.
.LP
The other problem with UNIX authentication appears in the name UNIX.
It is unrealistic to assume that all machines on a network
will be UNIX machines.
The NFS works with MS-DOS and VMS machines,
but UNIX authentication breaks down when applied to them.
For instance, MS-DOS doesn't even have a notion of different user IDs.
.LP
Given these shortcomings,
it is clear what is needed in a new authentication system:
operating system independent credentials, and secure verifiers.
This is the essence of DES authentication discussed below.
.#
.NH 2
\&DES Authentication
.IX "DES authentication"
.IX "authentication" "DES"
.LP
The security of DES authentication is based on
a sender's ability to encrypt the current time,
which the receiver can then decrypt and check against its own clock.
The timestamp is encrypted with DES.
Two things are necessary for this scheme to work:
1) the two agents must agree on what the current time is, and
2) the sender and receiver must be using the same encryption key.
.LP
If a network has time synchronization (Berkeley's TEMPO for example), 
then client/server time synchronization is performed automatically.
However, if this is not available,
timestamps can be computed using the server's time instead of network time.
In order to do this, the client asks the server what time it is,
before starting the RPC session,
then computes the time difference between its own clock and the server's.
This difference is used to offset the client's clock when computing timestamps.
If the client and server clocks get out of sync
to the point where the server begins rejecting the client's requests,
the DES authentication system just resynchronizes with the server.
.LP
Here's how the client and server arrive at the same encryption key.
When a client wishes to talk to a server, it generates at random 
a key to be used for encrypting the timestamps (among other things). 
This key is known as the
.I "conversation key, CK."
The client encrypts the conversation key using a public key scheme,
and sends it to the server in its first transaction.
This key is the only thing that is ever encrypted with public key cryptography.
The particular scheme used is described further on in this chapter.
For now, suffice to say that for any two agents A and B,
there is a DES key $K sub AB$ that only A and B can deduce.
This key is known as the
.I "common key,"
$K sub AB$.
.EQ
gsize 10
.EN
.ne 1i
.PS
.in +.7i
circlerad=.4
boxht=.2
boxwid=1.3
circle "\s+9A\s-9" "(client)" at 0,1.2
circle "\s+9B\s-9" "(server)" at 5.1,1.2
line invis at .5,2 ; box invis "\fBCredential\fP"; line invis;
	box invis "\fBVerifier\fP"
arrow at .5,1.7; box "$A, K sub AB (CK), CK(win)$"; arrow;
	box "$CK(t sub 1 ), CK(win + 1)$"; arrow
arrow <- at .5,1.4; line right 1.3; line;
	box "$CK(t sub 1 - 1), ID$"; arrow <-
arrow at .5,1; box "ID"; arrow;
	box "$CK(t sub 2 )$"; arrow
arrow <- at .5,.7; line right 1.3; line;
	box "$CK(t sub 2 - 1), ID$"; arrow <-
arrow at .5,.3; box "ID"; arrow;
	box "$CK(t sub n )$"; arrow
arrow <- at .5,0; line right 1.3; line;
	box "$CK(t sub n - 1), ID$"; arrow <-
.PE
.EQ
gsize 11
.EN
.in -.7i
.LP
The figure above illustrates the authentication protocol in more detail,
describing client A talking to server B.
A term of the form $K(x)$ means $x$ encrypted with the DES key $K$.
Examining the figure, you can see that for its first request,
the client's credential contains three things: 
its name $A$, the conversation key $CK$ encrypted with the common key 
$K sub AB$, and a thing called $win$ (window) encrypted with $CK$.
What the window says to the server, in effect, is this:
.LP
.I
I will be sending you many credentials in the future,
but there may be crackers sending them too,
trying to impersonate me with bogus timestamps.
When you receive a timestamp, check to see if your current time
is somewhere between the timestamp and the timestamp plus the window.
If it's not, please reject the credential. 
.LP
For secure NFS filesystems, the window currently defaults to 30 minutes.
The client's verifier in the first request contains the encrypted timestamp
and an encrypted verifier of the specified window, $win + 1$. 
The reason this exists is the following.
Suppose somebody wanted to impersonate A by writing a program
that instead of filling in the encrypted fields of the credential and verifier,
just stuffs in random bits.
The server will decrypt CK into some random DES key,
and use it to decrypt the window and the timestamp.
These will just end up as random numbers.
After a few thousand trials, there is a good chance
that the random window/timestamp pair will pass the authentication system.
The window verifier makes guessing the right credential much more difficult.
.LP
After authenticating the client,
the server stores four things into a credential table:
the client's name A, the conversation key $CK$, the window, and the timestamp.
The reason the server stores the first three things should be clear:
it needs them for future use.
The reason for storing the timestamp is to protect against replays.
The server will only accept timestamps
that are chronologically greater than the last one seen,
so any replayed transactions are guaranteed to be rejected.
The server returns to the client in its verifier an index ID
into its credential table, plus the client's timestamp minus one,
encrypted by $CK$.
The client knows that only the server could have sent such a verifier,
since only the server knows what timestamp the client sent.
The reason for subtracting one from it is to insure that it is invalid
and cannot be reused as a client verifier.
.LP
The first transaction is rather complicated,
but after this things go very smoothly.
The client just sends its ID and an encrypted timestamp to the server,
and the server sends back the client's timestamp minus one,
encrypted by $CK$.
.#
.NH 1
\&Public Key Encryption
.IX "public key encryption"
.LP
The particular public key encryption scheme Sun uses
is the Diffie-Hellman method.
The way this algorithm works is to generate a
.I "secret key"
$SK sub A$ at random
and compute a
.I "public key"
$PK sub A$ using the following formula
($PK$ and $SK$ are 192 bit numbers and \(*a is a well-known constant):
.EQ
PK sub A ~ = ~ alpha sup {SK sub A}
.EN
Public key $PK sub A$ is stored in a public directory,
but secret key $SK sub A$ is kept private.
Next, $PK sub B$ is generated from $SK sub B$ in the same manner as above.
Now common key $K sub AB$ can be derived as follows:
.EQ
K sub AB ~ = ~ PK sub B sup {SK sub A} ~ = ~
( alpha sup {SK sub B} ) sup {SK sub A} ~ = ~
alpha sup {( SK sub A SK sub B )}
.EN
Without knowing the client's secret key,
the server can calculate the same common key $K sub AB$
in a different way, as follows:
.EQ
K sub AB ~ = ~ PK sub A sup {SK sub B} ~ = ~
( alpha sup {SK sub A} ) sup {SK sub B} ~ = ~
alpha sup {( SK sub A SK sub B )}
.EN
Notice that nobody else but the server and client can calculate $K sub AB$,
since doing so requires knowing either one secret key or the other.
All of this arithmetic is actually computed modulo $M$,
which is another well-known constant.
It would seem at first that somebody could guess your secret key
by taking the logarithm of your public one, 
but $M$ is so large that this is a computationally infeasible task.
To be secure, $K sub AB$ has too many bits to be used as a DES key,
so 56 bits are extracted from it to form the DES key.
.LP
Both the public and the secret keys
are stored indexed by netname in the Yellow Pages map
.I publickey.byname
the secret key is DES-encrypted with your login password.
When you log in to a machine, the
.I login 
program grabs your encrypted secret key,
decrypts it with your login password,
and gives it to a secure local keyserver to save
for use in future RPC transactions.
Note that ordinary users do not have to be aware of 
their public and secret keys.
In addition to changing your login password, the
.I yppasswd 
.IX "yppasswd command" "" "\&\fIyppasswd\fP command"
program randomly generates a new public/secret key pair as well.
.LP
The keyserver
.I keyserv (8c)
.IX "keyserv daemon" "" "\&\fIkeyserv\fP daemon"
is an RPC service local to each machine
that performs all of the public key operations,
of which there are only three.  They are:
.DS
setsecretkey(secretkey)
encryptsessionkey(servername, des_key)
decryptsessionkey(clientname, des_key)
.DE
.I setsecretkey()
tells the keyserver to store away your secret key $SK sub A$ for future use;
it is normally called by
.I login .
The client program calls
.I encryptsessionkey()
to generate the encrypted conversation key
that is passed in the first RPC transaction to a server.
The keyserver looks up
.I servername 's
public key and combines it with the client's secret key (set up by a previous
.I setsecretkey()
call) to generate the key that encrypts
.I des_key .
The server asks the keyserver to decrypt the conversation key by calling
.I decryptsessionkey().
Note that implicit in these procedures is the name of caller,
who must be authenticated in some manner.
The keyserver cannot use DES authentication to do this,
since it would create deadlock. 
The keyserver solves this problem by storing the secret keys by
.I uid ,
and only granting requests to local root processes.
The client process then executes a
.I setuid 
process, owned by root, which makes the request on the part of the client,
telling the keyserver the real
.I uid 
of the client.  Ideally, the three operations described above
would be system calls, and the kernel would talk to the keyserver directly,
instead of executing the
.I setuid 
program.
.#
.NH 1
\&Naming of Network Entities
.IX "naming of network entities"
.IX "network naming"
.LP
The old UNIX authentication system has a few problems when it comes to naming.
Recall that with UNIX authentication,
the name of a network entity is basically the
.I uid .
These
.I uid s
are assigned per Yellow Pages naming domain,
which typically spans several machines.
We have already stated one problem with this system,
that it is too UNIX system oriented, 
but there are two other problems as well.
One is the problem of
.I uid 
clashes when domains are linked together.
The other problem is that the super-user (with
.I uid 
of 0) should not be assigned on a per-domain basis, 
but rather on a per-machine basis.
By default, the NFS deals with this latter problem in a severe manner:
it does not allow root access across the network by
.I uid 
0 at all.
.LP
DES authentication corrects these problems
by basing naming upon new names that we call
.I netnames.
Simply put, a netname is just a string of printable characters,
and fundamentally, it is really these netnames that we authenticate.
The public and secret keys are stored on a per-netname,
rather than per-username, basis.
The Yellow Pages map
.I netid.byname
maps the netname into a local
.I uid 
and group-access-list, 
though non-Sun environments may map the netname into something else.
.LP
We solve the Internet naming problem by choosing globally unique netnames.
This is far easier then choosing globally unique user IDs.
In the Sun environment, user names are unique within each Yellow Page domain.
Netnames are assigned by concatenating the operating system and user ID
with the Yellow Pages and ARPA domain names.
For example, a UNIX system user with a user ID of 508 in the domain
.I eng.sun.COM 
would be assigned the following netname:
.I unix.508@eng.sun.COM .
A good convention for naming domains is to append 
the ARPA domain name (COM, EDU, GOV, MIL) to the local domain name.
Thus, the Yellow Pages domain
.I eng 
within the ARPA domain
.I sun.COM 
becomes
.I eng.sun.COM .
.LP
We solve the problem of multiple super-users per domain
by assigning netnames to machines as well as to users.
A machine's netname is formed much like a user's.
For example, a UNIX machine named
.I hal 
in the same domain as before has the netname
.I unix.hal@eng.sun.COM .
Proper authentication of machines is very important for diskless machines
that need full access to their home directories over the net.
.LP
Non-Sun environments will have other ways of generating netnames, 
but this does not preclude them from accessing
the secure network services of the Sun environment.
To authenticate users from any remote domain,
all that has to be done is make entries for them in two Yellow Pages databases.
One is an entry for their public and secret keys, 
the other is for their local
.I uid 
and group-access-list mapping.
Upon doing this, users in the remote domain
will be able access all of the local network services,
such as the NFS and remote logins.
.#
.NH 1
\&Applications of DES Authentication
.IX "applications of DES authentication"
.IX "authentication" "DES"
.LP
The first application of DES authentication
is a generalized Yellow Pages update service. 
This service allows users to update private fields in Yellow Page databases.
So far the Yellow Pages maps
.I hosts,
.I ethers,
.I bootparams
and
.I publickey
employ the DES-based update service.
Before the advent of an update service for mail aliases,
Sun had to hire a full-time person just to update mail aliases.
.LP
The second application of DES authentication is the most important: 
a more secure Network File System.
There are three security problems with the 
old NFS using UNIX authentication.
The first is that verification of credentials occurs only at mount time
when the client gets from the server a piece of information
that is its key to all further requests: the
.I "file handle" .
Security can be broken if one can figure out a file handle
without contacting the server, perhaps by tapping into the net or by guessing.
After an NFS file system has been mounted,
there is no checking of credentials during file requests,
which brings up the second problem.
If a file system has been mounted from a server that serves multiple clients
(as is typically the case), there is no protection
against someone who has root permission on their machine using
.I su 
(or some other means of changing
.I uid )
gaining unauthorized access to other people's files.
The third problem with the NFS is the severe method it uses to circumvent
the problem of not being able to authenticate remote client super-users: 
denying them super-user access altogether. 
.LP
The new authentication system corrects all of these problems. 
Guessing file handles is no longer a problem since in order to gain 
unauthorized access, the miscreant will also have to guess the right 
encrypted timestamp to place in the credential,
which is a virtually impossible task.
The problem of authenticating root users is solved,
since the new system can authenticate machines.
At this point, however,
secure NFS is not used for root filesystems.
Root users of nonsecure filesystems are identified by IP address.
.LP
Actually, the level of security associated with each filesystem
may be altered by the administrator.  The file
.I /etc/exports 
.IX "etc/exports" "" "\&\fI/etc/exports\fP"
contains a list of filesystems and which machines may mount them. 
By default, filesystems are exported with UNIX authentication,
but the administrator can have them exported with DES authentication
by specifying
.I -secure 
on any line in the
.I /etc/exports 
file.  Associated with DES authentication is a parameter:
the maximum window size that the server is willing to accept.
.#
.NH 1
\&Security Issues Remaining
.IX "security" "issues remaining"
.IX "remaining security issues"
.LP
There are several ways to break DES authentication, but using
.I su 
is not one of them.  In order to be authenticated,
your secret key must be stored by your workstation.
This usually occurs when you login, with the
.I login 
program decrypting your secret key with your login password,
and storing it away for you.
If somebody tries to use
.I su 
to impersonate you, it won't work,
because they won't be able to decrypt your secret key.  Editing
.I /etc/passwd 
isn't going to help them either, because the thing they need to edit, 
your encrypted secret key, is stored in the Yellow Pages.
If you log into somebody else's workstation and type in your password,
then your secret key would be stored in their workstation and they could use
.I su 
to impersonate you.  But this is not a problem since you should not
be giving away your password to a machine you don't trust anyway. 
Someone on that machine could just as easily change
.I login 
to save all the passwords it sees into a file. 
.LP
Not having
.I su 
to employ any more, how can nefarious users impersonate others now?
Probably the easiest way is to guess somebody's password,
since most people don't choose very secure passwords.
We offer no protection against this;
it's up to each user to choose a secure password.
.LP
The next best attack would be to attempt replays.
For example, let's say I have been squirreling away
all of your NFS transactions with a particular server.
As long as the server remains up,
I won't succeed by replaying them since the server always demands timestamps
that are greater than the previous ones seen.
But suppose I go and pull the plug on your server, causing it to crash.
As it reboots, its credential table will be clean,
so it has lost all track of previously seen timestamps,
and now I am free to replay your transactions.
There are few things to be said about this.
First of all, servers should be kept in a secure place
so that no one can go and pull the plug on them.
But even if they are physically secure,
servers occasionally crash without any help.
Replaying transactions is not a very big security problem,
but even so, there is protection against it.
If a client specifies a window size that is smaller than the time it takes
a server to reboot (5 to 10 minutes), the server will reject
any replayed transactions because they will have expired.
.LP
There are other ways to break DES authentication, 
but they are much more difficult.
These methods involve breaking the DES key itself,
or computing the logarithm of the public key,
both of which would would take months of compute time on a supercomputer.
But it is important to keep our goals in mind.
Sun did not aim for super-secure network computing.
What we wanted was something as secure as a good time-sharing system,
and in that we have been successful.
.LP
There is another security issue that DES authentication does not address, 
and that is tapping of the net.
Even with DES authentication in place, 
there is no protection against somebody watching what goes across the net.
This is not a big problem for most things,
such as the NFS, since very few files are not publically readable, and besides,
trying to make sense of all the bits flying over the net is not a trivial task.
For logins, this is a bit of a problem because you wouldn't 
want somebody to pick up your password over the net.
As we mentioned before,
a side effect of the authentication system is a key exchange, 
so that the network tapping problem can be tackled on a per-application basis.
.#
.NH 1
\&Performance
.IX "performance of DES authentication"
.IX "authentication" "performance"
.LP
Public key systems are known to be slow,
but there is not much actual public key encryption going on in Sun's system.
Public key encryption only occurs in the first transaction with a service,
and even then, there is caching that speeds things up considerably.
The first time a client program contacts a server,
both it and the server will have to calculate the common key.
The time it takes to compute the common key is basically the time it takes
to compute an exponential modulo $M$.
On a Sun-3 using a 192-bit modulus, this takes roughly 1 second, 
which means it takes 2 seconds just to get things started,
since both client and server have to perform this operation.
This is a long time,
but you have to wait only the first time you contact a machine.
Since the keyserver caches the results of previous computations, 
it does not have to recompute the exponential every time.
.LP
The most important service in terms of performance is the secure NFS,
which is acceptably fast.
The extra overhead that DES authentication requires versus UNIX authentication 
is the encryption.
A timestamp is a 64-bit quantity,
which also happens to be the DES block size.
Four encryption operations take place in an average RPC transaction:
the client encrypts the request timestamp, the server decrypts it,
the server encrypts the reply timestamp, and the client decrypts it.
On a Sun-3, the time it takes to encrypt one block is about
half a millisecond if performed by hardware,
and 1.2 milliseconds if performed by software.
So, the extra time added to the round trip time is about 
2 milliseconds for hardware encryption and 5 for software.
The round trip time for the average NFS request is about 20 milliseconds,
resulting in a performance hit of 10 percent if one has encryption hardware, 
and 25 percent if not.
Remember that this is the impact on network performance.
The fact is that not all file operations go over the wire,
so the impact on total system performance will actually be lower than this.
It is also important to remember that security is optional, 
so environments that require higher performance can turn it off.
.#
.NH 1
\&Problems with Booting and \&\fBsetuid\fP Programs
.IX "problems with booting and \&\fIsetuid\fP programs"
.IX "booting and \&\fIsetuid\fP problems"
.LP
Consider the problem of a machine rebooting,
say after a power failure at some strange hour when nobody is around.
All of the secret keys that were stored get wiped out,
and now no process will be able to access secure network services,
such as mounting an NFS filesystem.
The important processes at this time are usually root processes,
so things would work OK if root's secret key were stored away, 
but nobody is around to type the password that decrypts it.
The solution to this problem is to store root's decrypted secret key in a file,
which the keyserver can read.
This works well for diskful machines that can store the secret key
on a physically secure local disk,
but not so well for diskless machines,
whose secret key must be stored across the network.
If you tap the net when a diskless machine is booting,
you will find the decrypted key.
This is not very easy to accomplish, though.
.LP
Another booting problem is the single-user boot.
There is a mode of booting known as single-user mode, where a
.I root 
login shell appears on the console.
The problem here is that a password is not required for this. 
With C2 security installed,
a password is required in order to boot single-user.
Without C2 security installed,
machines can still be booted single-user without a password,
as long as the entry for
.I console 
in the
.I /etc/ttytab 
.IX "etc/ttytab" "" "\&\fI/etc/ttytab\fP"
file is labeled as physically
.I secure 
(this is the default).
.LP
Yet another problem is that diskless machine booting is not totally secure.
It is possible for somebody to impersonate the boot-server,
and boot a devious kernel that, for example, 
makes a record of your secret key on a remote machine.
The problem is that our system is set up to provide protection
only after the kernel and the keyserver are running.
Before that, there is no way to authenticate
the replies given by the boot server.
We don't consider this a serious problem,
because it is highly unlikely that somebody would be able to write
this funny kernel without source code.
Also, the crime is not without evidence.
If you polled the net for boot-servers, 
you would discover the devious boot-server's location.
.LP
Not all
.I setuid 
programs will behave as they should.
For example, if a 
.I setuid 
program is owned by
.I dave ,
who has not logged into the machine since it booted,
then the program will not be able to access any secure network services as
.I dave .
The good news is that most
.I setuid 
programs are owned by root, 
and since root's secret key is always stored at boot time, 
these programs will behave as they always have.
.#
.NH 1
\&Conclusion
.IX "network security" "summary"
.LP
Our goal was to build a system as secure as a time-shared system. 
This goal has been met.
The way you are authenticated in a time-sharing system
is by knowing your password.
With DES authentication, the same is true.
In time-sharing the person you trust is your system administrator,
who has an ethical obligation
not to change your password in order to impersonate you.
In Sun's system, you trust your network administrator,
who does not alter your entry in the public key database.
In one sense, our system is even more secure than time-sharing,
because it is useless to place a tap on the network
in hopes of catching a password or encryption key, 
since these are encrypted.
Most time-sharing environments do not encrypt data emanating from the terminal;
users must trust that nobody is tapping their terminal lines.
.LP
DES authentication is perhaps not the ultimate authentication system. 
In the future it is likely there will be sufficient advances 
in algorithms and hardware to render the public key system
as we have defined it useless.
But at least DES authentication offers a smooth migration path for the future.
Syntactically speaking,
nothing in the protocol requires the encryption of the conversation 
key to be Diffie-Hellman, or even public key encryption in general. 
To make the authentication stronger in the future,
all that needs to be done is to strengthen the way
the conversation key is encrypted. 
Semantically, this will be a different protocol,
but the beauty of RPC is that it can be plugged in
and live peacefully with any authentication system.
.LP
For the present at least, DES authentication satisfies our requirements
for a secure networking environment.
From it we built a system secure enough for use in unfriendly networks,
such as a student-run university workstation environment.
The price for this security is not high.
Nobody has to carry around a magnetic card or remember
any hundred digit numbers.
You use your login password to authenticate yourself, just as before.
There is a small impact on performance,
but if this worries you and you have a friendly net,
you can turn authentication off.
.#
.NH 1
\&References
.IX "references on network security"
.LP
Diffie and Hellman, ``New Directions in Cryptography,''
\fIIEEE Transactions on Information Theory IT-22,\fP
November 1976.
.LP
Gusella & Zatti, ``TEMPO: A Network Time Controller
for a Distributed Berkeley UNIX System,''
\fIUSENIX 1984 Summer Conference Proceedings,\fP
June 1984.
.LP
National Bureau of Standards, ``Data Encryption Standard,''
\fIFederal Information Processing Standards Publication 46,\fP
January 15, 1977.
.LP
Needham & Schroeder, ``Using Encryption for Authentication
in Large Networks of Computers,''
\fIXerox Corporation CSL-78-4,\fP
September 1978.
.EQ
delim off
.EN
.IX "security" "of networks" "" "" PAGE END
.IX "network security" "" "" "" PAGE END
.IX "NFS security" "" "" "" PAGE END
