User Commands
=============

Standard IRC commands are not listed here. Several of the commands in
the operator commands chapter can also be used by normal users.

ACCEPT
------

::

   ACCEPT nick, -nick, ...

Adds or removes users from your accept list for umode +g and +R. Users
are automatically removed when they quit, split or change nick.

::

   ACCEPT *

Lists all users on your accept list.

Support of this command is indicated by the ``CALLERID`` token in
``RPL_ISUPPORT`` (005); the optional parameter indicates the letter of the
“only allow accept users to send private messages” umode, otherwise +g.
In charybdis this is always +g.

CNOTICE
-------

::

   CNOTICE nick channel :text

Providing you are opped (+o) or voiced (+v) in channel, and nick is a
member of channel, ``CNOTICE`` generates a ``NOTICE`` towards nick.

``CNOTICE`` bypasses any anti-spam measures in place. If you get “Targets
changing too fast, message dropped”, you should probably use this
command, for example sending a notice to every user joining a certain
channel.

As of charybdis 3.1, ``NOTICE`` automatically behaves as ``CNOTICE`` if you are
in a channel fulfilling the conditions.

Support of this command is indicated by the ``CNOTICE`` token in
``RPL_ISUPPORT`` (005).

CPRIVMSG
--------

::

   CPRIVMSG nick channel :text

Providing you are opped (+o) or voiced (+v) in channel, and nick is a
member of channel, ``CPRIVMSG`` generates a ``PRIVMSG`` towards nick.

``CPRIVMSG`` bypasses any anti-spam measures in place. If you get “Targets
changing too fast, message dropped”, you should probably use this
command.

As of charybdis 3.1, ``PRIVMSG`` automatically behaves as ``CPRIVMSG`` if you
are in a channel fulfilling the conditions.

Support of this command is indicated by the ``CPRIVMSG`` token in
``RPL_ISUPPORT`` (005).

FINDFORWARDS
------------

::

   FINDFORWARDS channel

.. note:: This command is only available if the ``m_findforwards.so``
          extension is loaded.

Displays which channels forward to the given channel (via cmode +f). If
there are very many channels the list will be truncated.

You must be a channel operator on the channel or an IRC operator to use
this command.

HELP
----

::

   HELP [topic]

Displays help information. topic can be ``INDEX``, ``CREDITS``, ``UMODE``, ``CMODE``,
``SNOMASK`` or a command name.

There are separate help files for users and opers. Opers can use ``UHELP``
to query the user help files.

IDENTIFY
--------

::

   IDENTIFY parameters...

.. note:: This command is only available if the ``m_identify.so``
          extension is loaded.

Sends an identify command to either NickServ or ChanServ. If the first
parameter starts with #, the command is sent to ChanServ, otherwise to
NickServ. The word ``IDENTIFY``, a space and all parameters are concatenated
and sent as a ``PRIVMSG`` to the service. If the service is not online or
does not have umode +S set, no message will be sent.

The exact syntax for this command depends on the services package in
use.

KNOCK
-----

::

   KNOCK channel

Requests an invite to the given channel. The channel must be locked
somehow (+ikl), must not be +p and you may not be banned or quieted.
Also, this command is rate limited.

If successful, all channel operators will receive a 710 numeric. The
recipient field of this numeric is the channel.

Support of this command is indicated by the ``KNOCK`` token in ``RPL_ISUPPORT``
(005).

MONITOR
-------

Server side notify list. This list contains nicks. When a user connects,
quits with a listed nick or changes to or from a listed nick, you will
receive a 730 numeric if the nick went online and a 731 numeric if the
nick went offline.

Support of this command is indicated by the ``MONITOR`` token in
``RPL_ISUPPORT`` (005); the parameter indicates the maximum number of
nicknames you may have in your monitor list.

You may only use this command once per second.

More details can be found in ``doc/monitor.txt`` in the source
distribution.

::

   MONITOR + nick, ...

Adds nicks to your monitor list. You will receive 730 and 731 numerics
for the nicks.

::

   MONITOR - nick, ...

Removes nicks from your monitor list. No output is generated for this
command.

::

   MONITOR C

Clears your monitor list. No output is generated for this command.

::

   MONITOR L

Lists all nicks on your monitor list, using 732 numerics and ending with
a 733 numeric.

::

   MONITOR S

Shows status for all nicks on your monitor list, using 730 and 731
numerics.
