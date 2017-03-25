Operator Commands
=================

Network management commands
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. note:: All commands and names are case insensitive. Parameters
          consisting of one or more separate letters, such as in ``MODE``,
          ``STATS`` and ``WHO``, are case sensitive.

CONNECT
-------

::

   CONNECT target [port] [source]

Initiate a connection attempt to server target. If a port is given,
connect to that port on the target, otherwise use the one given in
``ircd.conf``. If source is given, tell that server to initiate the
connection attempt, otherwise it will be made from the server you are
attached to.

To use the default port with source, specify 0 for port.

SQUIT
-----

::

   SQUIT server [reason]

Closes down the link to server from this side of the network. If a
reason is given, it will be sent out in the server notices on both sides
of the link.

REHASH
------

::

   REHASH [BANS | DNS | MOTD | OMOTD | TKLINES | TDLINES | TXLINES | TRESVS | REJECTCACHE | HELP] [server]

With no parameter given, ``ircd.conf`` will be reread and parsed. The
server argument is a wildcard match of server names.

``BANS``
    Rereads ``kline.conf``, ``dline.conf``, ``xline.conf``,
    ``resv.conf`` and their .perm variants

``DNS``
    Reread ``/etc/resolv.conf``.

``MOTD``
    Reload the ``MOTD`` file

``OMOTD``
    Reload the operator ``MOTD`` file

``TKLINES``
    Clears temporary ``K:lines``.

``TDLINES``
    Clears temporary ``D:lines``.

``TXLINES``
    Clears temporary ``X:lines``.

``TRESVS``
    Clears temporary reservations.

``REJECTCACHE``
    Clears the client rejection cache.

``HELP``
    Refreshes the help system cache.

RESTART
-------

::

   RESTART server

Cause an immediate total shutdown of the IRC server, and restart from
scratch as if it had just been executed.

This reexecutes the ircd using the compiled-in path, visible as ``SPATH`` in
``INFO``.

.. note:: This command cannot be used remotely. The server name is
          used only as a safety measure.

DIE
---

::

   DIE server

Immediately terminate the IRC server, after sending notices to all
connected clients and servers

.. note:: This command cannot be used remotely. The server name is
          used only as a safety measure.

SET
---

::

   SET [ ADMINSTRING | AUTOCONN | AUTOCONNALL | FLOODCOUNT | IDENTTIMEOUT | MAX | OPERSTRING | SPAMNUM | SPAMTIME | SPLITMODE | SPLITNUM | SPLITUSERS ] value

The ``SET`` command sets a runtime-configurable value.

Most of the ``ircd.conf`` equivalents have a ``default_prefix`` and are
only read on startup. ``SET`` is the only way to change these at run time.

Most of the values can be queried by omitting value.

``ADMINSTRING``
    Sets string shown in ``WHOIS`` for admins. (umodes +o and +a set, umode
    +S not set).

``AUTOCONN``
    Sets auto-connect on or off for a particular server. Takes two
    parameters, server name and new state.

    To see these values, use ``/stats c``. Changes to this are lost on a
    rehash.

``AUTOCONNALL``
    Globally sets auto-connect on or off. If disabled, no automatic
    connections are done; if enabled, automatic connections are done
    following the rules for them.

``FLOODCOUNT``
    The number of lines allowed to be sent to a connection before
    throttling it due to flooding. Note that this variable is used for
    both channels and clients.

    For channels, op or voice overrides this; for users, IRC operator
    status or op or voice on a common channel overrides this.

``IDENTTIMEOUT``
    Timeout for requesting ident from a client.

``MAX``
    Sets the maximum number of connections to value.

    This number cannot exceed maxconnections - ``MAX_BUFFER``.
    maxconnections is the rlimit for number of open files. ``MAX_BUFFER``
    is defined in config.h, normally 60.

    ``MAXCLIENTS`` is an alias for this.

``OPERSTRING``
    Sets string shown in ``WHOIS`` for opers (umode ``+o`` set, umodes ``+a`` and ``+S``
    not set).

``SPAMNUM``
    Sets how many join/parts to channels constitutes a possible spambot.

``SPAMTIME``
    Below this time on a channel counts as a join/part as above.

``SPLITMODE``
    Sets splitmode to value:

    ``ON``
        splitmode is permanently on

    ``OFF``
        splitmode is permanently off (default if ``no_create_on_split``
        and ``no_join_on_split`` are disabled)

    ``AUTO``
        ircd chooses splitmode based on ``SPLITUSERS`` and ``SPLITNUM`` (default
        if ``no_create_on_split`` or ``no_join_on_split`` are enabled)

``SPLITUSERS``
    Sets the minimum amount of users needed to deactivate automatic
    splitmode.

``SPLITNUM``
    Sets the minimum amount of servers needed to deactivate automatic
    splitmode. Only servers that have finished bursting count for this.

User management commands
~~~~~~~~~~~~~~~~~~~~~~~~

KILL
----

::

   KILL nick [reason]

Disconnects the user with the given nick from the server they are
connected to, with the reason given, if present, and broadcast a server
notice announcing this.

Your nick and the reason will appear on channels.

CLOSE
-----

Closes all connections from and to clients and servers who have not
completed registering.

KLINE
-----

::

   KLINE [length] [user@host | user@a.b.c.d] [ON servername] [:reason]

Adds a ``K:line`` to ``kline.conf`` to ban the given ``user@host`` from using
that server.

If the optional parameter length is given, the ``K:line`` will be temporary
(i.e. it will not be stored on disk) and last that long in minutes.

If an IP address is given, the ban will be against all hosts matching
that IP regardless of DNS. The IP address can be given as a full address
(``192.168.0.1``), as a CIDR mask (``192.168.0.0/24``), or as a glob
(``192.168.0.*``).

All clients matching the ``K:line`` will be disconnected from the server
immediately.

If a reason is specified, it will be sent to the client when they are
disconnected, and whenever a connection is attempted which is banned.

If the ON part is specified, the ``K:line`` is set on servers matching the
given mask (provided a matching ``shared{}`` block exists there). Otherwise,
if specified in a ``cluster{}`` block, the ``K:Line`` will be propagated across
the network accordingly.

UNKLINE
-------

::

   UNKLINE user@host [ON servername]

Will attempt to remove a ``K:line`` matching ``user@host`` from ``kline.conf``,
and will flush a temporary ``K:line``.

XLINE
-----

::

   XLINE [length] mask [ON servername] [:reason]

Works similarly to ``KLINE``, but matches against the real name field. The
wildcards are ``*`` (any sequence), ``?`` (any character), ``#`` (a digit) and ``@`` (a
letter); wildcard characters can be escaped with a backslash. The
sequence ``\s`` matches a space.

All clients matching the ``X:line`` will be disconnected from the server
immediately.

The reason is never sent to users. Instead, they will be exited with
"Bad user info".

If the ON part is specified, the ``X:line`` is set on servers matching the
given mask (provided a matching ``shared{}`` block exists there). Otherwise,
if specified in a ``cluster{}`` block, the ``X:line`` will be propagated across
the network accordingly.

UNXLINE
-------

::

   UNXLINE mask [ON servername]

Will attempt to remove an ``X:line`` from ``xline.conf``, and will flush a
temporary ``X:line``.

RESV
----

::

   RESV [length] [channel | mask] [ON servername] [:reason]

If used on a channel, “jupes” the channel locally. Joins to the channel
will be disallowed and generate a server notice on ``+y``, and users will
not be able to send to the channel. Channel jupes cannot contain
wildcards.

If used on a nickname mask, prevents local users from using a nick
matching the mask (the same wildcard characters as xlines). There is no
way to exempt the initial nick from this.

In neither case will current users of the nick or channel be kicked or
disconnected.

This facility is not designed to make certain nicks or channels
oper-only.

The reason is never sent to users.

If the ON part is specified, the resv is set on servers matching the
given mask (provided a matching ``shared{}`` block exists there). Otherwise,
if specified in a ``cluster{}`` block, the resv will be propagated across
the network accordingly.

UNRESV
------

::

   UNRESV [channel | mask] [ON servername]

Will attempt to remove a resv from ``resv.conf``, and will flush a
temporary resv.

DLINE
-----

::

   DLINE [length] a.b.c.d [ON servername] [:reason]

Adds a ``D:line`` to ``dline.conf``, which will deny any connections from
the given IP address. The IP address can be given as a full address
(``192.168.0.1``) or as a CIDR mask (``192.168.0.0/24``).

If the optional parameter length is given, the ``D:line`` will be temporary
(i.e. it will not be stored on disk) and last that long in minutes.

All clients matching the ``D:line`` will be disconnected from the server
immediately.

If a reason is specified, it will be sent to the client when they are
disconnected, and, if ``dline_reason`` is enabled, whenever a connection is
attempted which is banned.

``D:lines`` are less load on a server, and may be more appropriate if
somebody is flooding connections.

If the ON part is specified, the ``D:line`` is set on servers matching the
given mask (provided a matching ``shared{}`` block exists there, which is
not the case by default). Otherwise, the D:Line will be set on the local
server only.

Only ``exempt{}`` blocks exempt from ``D:lines``. Being a server or having
``kline_exempt`` in ``auth{}`` does *not* exempt (different from ``K/G/X:lines``).

UNDLINE
-------

::

   UNDLINE a.b.c.d [ON servername]

Will attempt to remove a ``D:line`` from ``dline.conf``, and will flush a
temporary ``D:line``.

TESTGECOS
---------

::

   TESTGECOS gecos

Looks up X:Lines matching the given gecos.

TESTLINE
--------

::

   TESTLINE [nick!] [user@host | a.b.c.d]

Looks up the given hostmask or IP address and reports back on any ``auth{}``
blocks, D: or K: lines found. If nick is given, also searches for nick
resvs.

For temporary items the number of minutes until the item expires is
shown (as opposed to the hit count in STATS q/Q/x/X).

This command will not perform DNS lookups; for best results you must
testline a host and its IP form.

The given username should begin with a tilde (~) if identd is not in
use. As of charybdis 2.1.1, ``no_tilde`` and username truncation will be
taken into account like in the normal client access check.

As of charybdis 2.2.0, a channel name can be specified and the RESV will
be returned, if there is one.

TESTMASK
--------

::

   TESTMASK hostmask [gecos]

Searches the network for users that match the hostmask and gecos given,
returning the number of matching users on this server and other servers.

The hostmask is of the form user@host or user@ip/cidr with \* and ?
wildcards, optionally preceded by nick!.

The gecos field accepts the same wildcards as xlines.

The IP address checked against is ``255.255.255.255`` if the IP address is
unknown (remote client on a TS5 server) or 0 if the IP address is hidden
(``auth{}`` spoof).

LUSERS
------

::
   
   LUSERS [mask] [nick | server]

Shows various user and channel counts.

The mask parameter is obsolete but must be used when querying a remote
server.

TRACE
-----

::

   TRACE [server | nick] [location]

With no argument or one argument which is the current server, TRACE
gives a list of all connections to the current server and a summary of
connection classes.

With one argument which is another server, TRACE displays the path to
the specified server, and all servers, opers and -i users on that
server, along with a summary of connection classes.

With one argument which is a client, TRACE displays the path to that
client, and that client's information.

If location is given, the command is executed on that server; no path is
displayed.

When listing connections, type, name and class is shown in addition to
information depending on the type:

Try.
    A server we are trying to make a TCP connection to.

H.S.
    A server we have established a TCP connection to, but is not yet
    registered.

\?\?\?\?
    An incoming connection that has not yet registered as a user or a
    server (“unknown”). Shows the username, hostname, IP address and the
    time the connection has been open. It is possible that the ident or
    DNS lookups have not completed yet, and in any case no tildes are
    shown here. Unknown connections may not have a name yet.

User
    A registered unopered user. Shows the username, hostname, IP
    address, the time the client has not sent anything (as in STATS l)
    and the time the user has been idle (from PRIVMSG only, as in
    WHOIS).

Oper
    Like User, but opered.

Serv
    A registered server. Shows the number of servers and users reached
    via this link, who made this connection and the time the server has
    not sent anything.

ETRACE
------

::

   ETRACE [nick]

Shows client information about the given target, or about all local
clients if no target is specified.

PRIVS
-----

::

   PRIVS [nick]

Displays effective operator privileges for the specified nick, or for
yourself if no nick is given. This includes all privileges from the
operator block, the name of the operator block and those privileges from
the auth block that have an effect after the initial connection.

The exact output depends on the server the nick is on, see the matching
version of this document. If the remote server does not support this
extension, you will not receive a reply.

MASKTRACE
---------

::

   MASKTRACE hostmask [gecos]

Searches the local server or network for users that match the hostmask
and gecos given. Network searches require the ``oper_spy`` privilege and an
'!' before the hostmask. The matching works the same way as TESTMASK.

The hostmask is of the form ``user@host`` or ``user@ip/cidr`` with ``*`` and ``?``
wildcards, optionally preceded by ``nick!``.

The gecos field accepts the same wildcards as xlines.

The IP address field contains ``255.255.255.255`` if the IP address is
unknown (remote client on a TS5 server) or ``0`` if the IP address is hidden
(``auth{}`` spoof).

CHANTRACE
---------

::

   CHANTRACE channel

Displays information about users in a channel. Opers with the ``oper_spy``
privilege can get the information without being on the channel, by
prefixing the channel name with an ``!``.

The IP address field contains ``255.255.255.255`` if the IP address is
unknown (remote client on a TS5 server) or ``0`` if the IP address is hidden
(``auth{}`` spoof).

SCAN
----

::

   SCAN UMODES +modes-modes [no-list] [list] [global] [list-max number] [mask nick!user@host]

Searches the local server or network for users that have the umodes
given with + and do not have the umodes given with -. no-list disables
the listing of matching users and only shows the count. list enables the
listing (default). global extends the search to the entire network
instead of local users only. list-max limits the listing of matching
users to the given amount. mask causes only users matching the given
nick!user@host mask to be selected. Only the displayed host is
considered, not the IP address or real host behind dynamic spoofs.

The IP address field contains ``255.255.255.255`` if the IP address is
unknown (remote client on a TS5 server) or 0 if the IP address is hidden
(``auth{}`` spoof).

Network searches where a listing is given are operspy commands.

CHGHOST
-------

::

   CHGHOST nick value

Set the hostname associated with a particular nick for the duration of
this session. This command is disabled by default because of the abuse
potential and little practical use.

Miscellaneous commands
~~~~~~~~~~~~~~~~~~~~~~

ADMIN
-----

::

   ADMIN [nick | server]

Shows the information in the ``admin{}`` block.

INFO
----

::

   INFO [nick | server]

Shows information about the authors of the IRC server, and some
information about this server instance. Opers also get a list of
configuration options.

TIME
----

::

   TIME [nick | server]

Shows the local time on the given server, in a human-readable format.

VERSION
-------

::

   VERSION [nick | server]

Shows version information, a few compile/config options, the SID and the
005 numerics. The 005 numeric will be remapped to 105 for remote
requests.

STATS
-----

::

   STATS [type] [nick | server]

Display various statistics and configuration information.

A
    Show DNS servers

b
    Show active nick delays

B
    Show hash statistics

c
    Show connect blocks

d
    Show temporary ``D:lines``

D
    Show permanent ``D:lines``

e
    Show exempt blocks (exceptions to ``D:lines``)

E
    Show events

f
    Show file descriptors

h
    Show ``hub_mask``/``leaf_mask``

i
    Show auth blocks, or matched auth blocks

k
    Show temporary ``K:lines``, or matched ``K:lines``

K
    Show permanent ``K:lines``, or matched ``K:lines``

l
    Show hostname and link information about the given nick. With a
    server name, show information about opers and servers on that
    server; opers get information about all local connections if they
    query their own server. No hostname is shown for server connections.

L
    Like l, but show IP address instead of hostname

m
    Show commands and their usage statistics (total counts, total bytes,
    counts from server connections)

n
    Show blacklist blocks (DNS blacklists) with hit counts since last
    rehash and (parenthesized) reference counts. The reference count
    shows how many clients are waiting on a lookup of this blacklist or
    have been found and are waiting on registration to complete.

o
    Show operator blocks

O
    Show privset blocks

p
    Show logged on network operators which are not set AWAY.

P
    Show listen blocks (ports)

q
    Show temporarily resv'ed nicks and channels with hit counts

Q
    Show permanently resv'ed nicks and channels with hit counts since
    last rehash bans

r
    Show resource usage by the ircd

t
    Show generic server statistics about local connections

u
    Show server uptime

U
    Show shared (c), cluster (C) and service (s) blocks

v
    Show connected servers and brief status

x
    Show temporary ``X:lines`` with hit counts

X
    Show permanent ``X:lines`` with hit counts since last rehash bans

y
    Show class blocks

z
    Show memory usage statistics

Z
    Show ziplinks statistics

?
    Show connected servers and link information about them

WALLOPS
-------

::

   WALLOPS :message

Sends a WALLOPS message to all users who have the +w umode set. This is
for things you don't mind the whole network knowing about.

OPERWALL
--------

::

   OPERWALL :message

Sends an OPERWALL message to all opers who have the +z umode set. +z is
restricted, OPERWALL should be considered private communications.
