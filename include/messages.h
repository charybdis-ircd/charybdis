/************************************************************************
 *   IRC - Internet Relay Chat, include/messages.h
 *   Copyright (C) 1992 Darren Reed
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef INCLUDED_messages_h
#define INCLUDED_messages_h

/*
 * form_str - return a format string for a message number
 * messages are defined below
 */

#define FORM_STR1(n)         NUMERIC_STR_##n
#define form_str(n)          FORM_STR1(n)

#define NUMERIC_STR_1        ":Welcome to the %s Internet Relay Chat Network %s"
#define NUMERIC_STR_2        ":Your host is %s, running version %s"
#define NUMERIC_STR_3        ":This server was created %s"
#define NUMERIC_STR_4        "%s %s %s %s bkloveqjfI"
#define NUMERIC_STR_5        "%s :are supported by this server"
#define NUMERIC_STR_8        "%s :Server notice mask"
#define NUMERIC_STR_10       "%s %d :Please use this Server/Port instead"
#define NUMERIC_STR_15       ":%s"
#define NUMERIC_STR_17       ":End of /MAP"
#define NUMERIC_STR_43       "%s :Nick collision, forcing nick change to your unique ID"
#define NUMERIC_STR_200      "Link %s %s %s"
#define NUMERIC_STR_201      "Try. %s %s"
#define NUMERIC_STR_202      "H.S. %s %s"
#define NUMERIC_STR_203      "???? %s %s (%s) %lu"
#define NUMERIC_STR_204      "Oper %s %s (%s) %lu %lu"
#define NUMERIC_STR_205      "User %s %s (%s) %lu %lu"
#define NUMERIC_STR_206      "Serv %s %dS %dC %s %s!%s@%s %lu"
#define NUMERIC_STR_208      "<newtype> 0 %s"
#define NUMERIC_STR_209      "Class %s %d"
#define NUMERIC_STR_212      "%s %u %lu :%u"
#define NUMERIC_STR_213      "C %s %s %s %d %s %s"
#define NUMERIC_STR_215      "I %s %s %s@%s %d %s"
#define NUMERIC_STR_216      "%c %s * %s :%s%s%s"
#define NUMERIC_STR_217      "%c %d %s :%s"
#define NUMERIC_STR_218      "Y %s %d %d %d %u %d.%d %d.%d %u"
#define NUMERIC_STR_219      "%c :End of /STATS report"
#define NUMERIC_STR_220      "%c %d %s %d :%s%s%s"
#define NUMERIC_STR_221      "%s"
#define NUMERIC_STR_225      "%c %s :%s%s%s"
#define NUMERIC_STR_241      "L %s * %s 0 -1"
#define NUMERIC_STR_242      ":Server Up %d days, %d:%02d:%02d"
#define NUMERIC_STR_243      "O %s@%s * %s %s %s"
#define NUMERIC_STR_244      "H %s * %s 0 -1"
#define NUMERIC_STR_247      "%c %d %s :%s"

#define NUMERIC_STR_248      "U %s %s@%s %s"
#define NUMERIC_STR_250      ":Highest connection count: %d (%d clients) (%lu connections received)"
#define NUMERIC_STR_251      ":There are %d users and %d invisible on %d servers"
#define NUMERIC_STR_252      "%d :IRC Operators online"
#define NUMERIC_STR_253      "%d :unknown connection(s)"
#define NUMERIC_STR_254      "%lu :channels formed"
#define NUMERIC_STR_255      ":I have %d clients and %d servers"
#define NUMERIC_STR_256      "%s :Administrative info"
#define NUMERIC_STR_257      ":%s"
#define NUMERIC_STR_258      ":%s"
#define NUMERIC_STR_259      ":%s"
#define NUMERIC_STR_262      "%s :End of TRACE"
#define NUMERIC_STR_263      ":%s 263 %s %s :This command could not be completed because it has been used recently, and is rate-limited."
#define NUMERIC_STR_265      "%d %d :Current local users %d, max %d"
#define NUMERIC_STR_266      "%d %d :Current global users %d, max %d"
#define NUMERIC_STR_270      "%s :%s"
#define NUMERIC_STR_276      "%s :has client certificate fingerprint %s"
#define NUMERIC_STR_281      ":%s 281 %s %s"
#define NUMERIC_STR_282      ":%s 282 %s :End of /ACCEPT list."
#define NUMERIC_STR_301      "%s :%s"
#define NUMERIC_STR_302      ":%s 302 %s :%s"
#define NUMERIC_STR_303      ":%s 303 %s :"
#define NUMERIC_STR_305      ":You are no longer marked as being away"
#define NUMERIC_STR_306      ":You have been marked as being away"
#define NUMERIC_STR_310      "%s :is available for help."
#define NUMERIC_STR_311      "%s %s %s * :%s"
#define NUMERIC_STR_312      "%s %s :%s"
#define NUMERIC_STR_313      "%s :%s"
#define NUMERIC_STR_314      ":%s 314 %s %s %s %s * :%s"
#define NUMERIC_STR_315      ":%s 315 %s %s :End of /WHO list."
#define NUMERIC_STR_317      "%s %ld %lu :seconds idle, signon time"
#define NUMERIC_STR_318      "%s :End of /WHOIS list."
#define NUMERIC_STR_319      ":%s 319 %s %s :"
#define NUMERIC_STR_320      "%s :%s"
#define NUMERIC_STR_321      ":%s 321 %s Channel :Users  Name"
#define NUMERIC_STR_322      ":%s 322 %s %s%s %lu :%s"
#define NUMERIC_STR_323      ":%s 323 %s :End of /LIST"
#define NUMERIC_STR_324      ":%s 324 %s %s %s"
#define NUMERIC_STR_325      ":%s 325 %s %s %s :is the current channel mode-lock"
#define NUMERIC_STR_329      ":%s 329 %s %s %lu"
#define NUMERIC_STR_330      "%s %s :is logged in as"
#define NUMERIC_STR_331      ":%s 331 %s %s :No topic is set."
#define NUMERIC_STR_332      ":%s 332 %s %s :%s"
#define NUMERIC_STR_333      ":%s 333 %s %s %s %lu"
#define NUMERIC_STR_337      "%s :%s"
#define NUMERIC_STR_338      "%s %s :actually using host"
#define NUMERIC_STR_341      ":%s 341 %s %s %s"
#define NUMERIC_STR_346      ":%s 346 %s %s %s %s %lu"
#define NUMERIC_STR_347      ":%s 347 %s %s :End of Channel Invite List"
#define NUMERIC_STR_348      ":%s 348 %s %s %s %s %lu"
#define NUMERIC_STR_349      ":%s 349 %s %s :End of Channel Exception List"
#ifndef CUSTOM_BRANDING
#define NUMERIC_STR_351      "%s(%s). %s :%s TS%dow %s"
#else
#define NUMERIC_STR_351      "%s(%s,%s). %s :%s TS%dow %s"
#endif
#define NUMERIC_STR_352      ":%s 352 %s %s %s %s %s %s %s :%d %s"
#define NUMERIC_STR_353      ":%s 353 %s %s %s :"
#define NUMERIC_STR_360      ":%s 360 %s %s :was connecting from *@%s %s"
#define NUMERIC_STR_362      ":%s 362 %s %s :Closed. Status = %d"
#define NUMERIC_STR_363      ":%s 363 %s %d :Connections Closed"
#define NUMERIC_STR_364      "%s %s :%d %s"
#define NUMERIC_STR_365      "%s :End of /LINKS list."
#define NUMERIC_STR_366      ":%s 366 %s %s :End of /NAMES list."
#define NUMERIC_STR_367      ":%s 367 %s %s %s %s %lu"
#define NUMERIC_STR_368      ":%s 368 %s %s :End of Channel Ban List"
#define NUMERIC_STR_369      "%s :End of WHOWAS"
#define NUMERIC_STR_371      ":%s"
#define NUMERIC_STR_372      ":%s 372 %s :- %s"
#define NUMERIC_STR_374      ":End of /INFO list."
#define NUMERIC_STR_375      ":%s 375 %s :- %s Message of the Day - "
#define NUMERIC_STR_376      ":%s 376 %s :End of /MOTD command."
#define NUMERIC_STR_378      "%s :is connecting from *@%s %s"
#define NUMERIC_STR_381      ":%s 381 %s :IRCops are just another kind of cop.  Now that you are one, be mindful of that."
#define NUMERIC_STR_382      ":%s 382 %s %s :Rehashing"
#define NUMERIC_STR_386      ":%s 386 %s :%s"
#define NUMERIC_STR_391      "%s :%s"
#define NUMERIC_STR_401      "%s :No such nick/channel"
#define NUMERIC_STR_402      "%s :No such server"
#define NUMERIC_STR_403      "%s :No such channel"
#define NUMERIC_STR_404      "%s :Cannot send to nick/channel"
#define NUMERIC_STR_405      ":%s 405 %s %s :You have joined too many channels"
#define NUMERIC_STR_406      ":%s :There was no such nickname"
#define NUMERIC_STR_407      ":%s 407 %s %s :Too many recipients."
#define NUMERIC_STR_409      ":%s 409 %s :No origin specified"
#define NUMERIC_STR_410      ":%s 410 %s %s :Invalid CAP subcommand"
#define NUMERIC_STR_411      ":%s 411 %s :No recipient given (%s)"
#define NUMERIC_STR_412      ":%s 412 %s :No text to send"
#define NUMERIC_STR_413      "%s :No toplevel domain specified"
#define NUMERIC_STR_414      "%s :Wildcard in toplevel Domain"
#define NUMERIC_STR_416      ":%s 416 %s %s :output too large, truncated"
#define NUMERIC_STR_421      ":%s 421 %s %s :Unknown command"
#define NUMERIC_STR_422      ":%s 422 %s :MOTD File is missing"
#define NUMERIC_STR_431      ":%s 431 %s :No nickname given"
#define NUMERIC_STR_432      ":%s 432 %s %s :Erroneous Nickname"
#define NUMERIC_STR_433      ":%s 433 %s %s :Nickname is already in use."
#define NUMERIC_STR_435      "%s %s :Cannot change nickname while banned on channel"
#define NUMERIC_STR_436      "%s :Nickname collision KILL"
#define NUMERIC_STR_437      ":%s 437 %s %s :Nick/channel is temporarily unavailable"
#define NUMERIC_STR_438      ":%s 438 %s %s %s :Nick change too fast. Please wait %d seconds."
#define NUMERIC_STR_440      "%s :Services are currently unavailable"
#define NUMERIC_STR_441      "%s %s :They aren't on that channel"
#define NUMERIC_STR_442      "%s :You're not on that channel"
#define NUMERIC_STR_443      "%s %s :is already on channel"
#define NUMERIC_STR_451      ":%s 451 * :You have not registered"
#define NUMERIC_STR_456      ":%s 456 %s :Accept list is full"
#define NUMERIC_STR_457      ":%s 457 %s %s :is already on your accept list"
#define NUMERIC_STR_458      ":%s 458 %s %s :is not on your accept list"
#define NUMERIC_STR_461      ":%s 461 %s %s :Not enough parameters"
#define NUMERIC_STR_462      ":%s 462 %s :You may not reregister"
#define NUMERIC_STR_464      ":%s 464 %s :Password Incorrect"
#define NUMERIC_STR_465      ":%s 465 %s :You are banned from this server- %s"
#define NUMERIC_STR_470      "%s %s :Forwarding to another channel"
#define NUMERIC_STR_471      ":%s 471 %s %s :Cannot join channel (+l) - channel is full, try again later"
#define NUMERIC_STR_472      ":%s 472 %s %c :is an unknown mode char to me"
#define NUMERIC_STR_473      ":%s 473 %s %s :Cannot join channel (+i) - you must be invited"
#define NUMERIC_STR_474      ":%s 474 %s %s :Cannot join channel (+b) - you are banned"
#define NUMERIC_STR_475      ":%s 475 %s %s :Cannot join channel (+k) - bad key"
#define NUMERIC_STR_477      ":%s 477 %s %s :Cannot join channel (+r) - you need to be identified with services"
#define NUMERIC_STR_478      ":%s 478 %s %s %s :Channel ban list is full"
#define NUMERIC_STR_479      "%s :Illegal channel name"
#define NUMERIC_STR_480      ":%s 480 %s %s :Cannot join channel (+j) - throttle exceeded, try again later"
#define NUMERIC_STR_481      ":Permission Denied - You're not an IRC operator"
#define NUMERIC_STR_482      ":%s 482 %s %s :You're not a channel operator"
#define NUMERIC_STR_483      ":You can't kill a server!"
#define NUMERIC_STR_484      ":%s 484 %s %s %s :Cannot kick or deop a network service"
#define NUMERIC_STR_486      "%s :You must log in with services to message this user"
#define NUMERIC_STR_489      ":%s 489 %s %s :You're neither voiced nor channel operator"
#define NUMERIC_STR_491      ":No appropriate operator blocks were found for your host"
#define NUMERIC_STR_492      ":Cannot send to user %s (%s)"
#define NUMERIC_STR_494      "%s :cannot answer you while you are %s, your message was not sent"
#define NUMERIC_STR_501      ":%s 501 %s :Unknown MODE flag"
#define NUMERIC_STR_502      ":%s 502 %s :Can't change mode for other users"
#define NUMERIC_STR_504      ":%s 504 %s %s :User is not on this server"
#define NUMERIC_STR_513      ":%s 513 %s :To connect type /QUOTE PONG %08X"
#define NUMERIC_STR_517      "%s :This command has been administratively disabled"
#define NUMERIC_STR_524      ":%s 524 %s %s :Help not found"
#define NUMERIC_STR_670      ":STARTTLS successful, proceed with TLS handshake"
#define NUMERIC_STR_671      "%s :%s"
#define NUMERIC_STR_691      ":%s"
#define NUMERIC_STR_702      ":%s 702 %s %s 0x%lx %s%s :[Version %s] [Description: %s]"
#define NUMERIC_STR_703      ":%s 703 %s :End of /MODLIST."
#define NUMERIC_STR_704      ":%s 704 %s %s :%s"
#define NUMERIC_STR_705      ":%s 705 %s %s :%s"
#define NUMERIC_STR_706      ":%s 706 %s %s :End of /HELP."
#define NUMERIC_STR_707      ":%s 707 %s %s :Targets changing too fast, message dropped"
#define NUMERIC_STR_708      ":%s 708 %s %s %s %s %s %s %s %s :%s"
#define NUMERIC_STR_709      ":%s 709 %s %s %s %s %s %s %s :%s"
#define NUMERIC_STR_710      ":%s 710 %s %s %s!%s@%s :has asked for an invite."
#define NUMERIC_STR_711      ":%s 711 %s %s :Your KNOCK has been delivered."
#define NUMERIC_STR_712      ":%s 712 %s %s :Too many KNOCKs (%s)."
#define NUMERIC_STR_713      "%s :Channel is open."
#define NUMERIC_STR_714      ":%s 714 %s %s :You are already on that channel."
#define NUMERIC_STR_715      ":%s 715 %s :KNOCKs are disabled."
#define NUMERIC_STR_716      "%s :is in +g mode (server-side ignore.)"
#define NUMERIC_STR_717      "%s :has been informed that you messaged them."
#define NUMERIC_STR_718      ":%s 718 %s %s %s@%s :is messaging you, and you have umode +g."
#define NUMERIC_STR_720      ":%s 720 %s :Start of OPER MOTD"
#define NUMERIC_STR_721      ":%s 721 %s :%s"
#define NUMERIC_STR_722      ":%s 722 %s :End of OPER MOTD"
#define NUMERIC_STR_723      ":%s 723 %s %s :Insufficient oper privs"
#define NUMERIC_STR_725      ":%s 725 %s %c %ld %s :%s"
#define NUMERIC_STR_726      ":%s 726 %s %s :No matches"
#define NUMERIC_STR_727      ":%s 727 %s %d %d %s!%s@%s %s :Local/remote clients match"
#define NUMERIC_STR_728      ":%s 728 %s %s q %s %s %lu"
#define NUMERIC_STR_729      ":%s 729 %s %s q :End of Channel Quiet List"
#define NUMERIC_STR_730      ":%s 730 %s :%s"
#define NUMERIC_STR_731      ":%s 731 %s :%s"
#define NUMERIC_STR_732      ":%s 732 %s :%s"
#define NUMERIC_STR_733      ":%s 733 %s :End of MONITOR list"
#define NUMERIC_STR_734      ":%s 734 %s %d %s :Monitor list is full"
#define NUMERIC_STR_740      ":%s 740 %s :%s"
#define NUMERIC_STR_741      ":%s 741 %s :End of CHALLENGE"
#define NUMERIC_STR_742      "%s %c %s :MODE cannot be set due to channel having an active MLOCK restriction policy"
#define NUMERIC_STR_743      "%s %c %s :Invalid ban mask"
#define NUMERIC_STR_750      "%d :matches"
#define NUMERIC_STR_751      "%s %s %s %s %s %s :%s"

#define NUMERIC_STR_800      "1 0 PLAIN,EXTERNAL 512 *"
#define NUMERIC_STR_801      "%s %s %s %ld %s :%s"
#define NUMERIC_STR_802      "%s %s %s :Access entry deleted"
#define NUMERIC_STR_803      "%s :Start of access entries"
#define NUMERIC_STR_804      "%s %s %s %ld %s :%s"
#define NUMERIC_STR_805      "%s :End of access entries"
#define NUMERIC_STR_818      "%s %s :%s"
#define NUMERIC_STR_819      "%s :End of property list"

#define NUMERIC_STR_900      ":%s 900 %s %s!%s@%s %s :You are now logged in as %s"
#define NUMERIC_STR_901      ":%s 901 %s %s!%s@%s :You are now logged out"
#define NUMERIC_STR_902      ":%s 902 %s :You must use a nick assigned to you"
#define NUMERIC_STR_903      ":%s 903 %s :SASL authentication successful"
#define NUMERIC_STR_904      ":%s 904 %s :SASL authentication failed"
#define NUMERIC_STR_905      ":%s 905 %s :SASL message too long"
#define NUMERIC_STR_906      ":%s 906 %s :SASL authentication aborted"
#define NUMERIC_STR_907      ":%s 907 %s :You have already completed SASL authentication"
#define NUMERIC_STR_908      ":%s 908 %s %s :are available SASL mechanisms"

#define NUMERIC_STR_915      "%s %s :Missing access entry"
#define NUMERIC_STR_916      "%s :Too many access entries"
#define NUMERIC_STR_917      "%s :Too many properties"
#define NUMERIC_STR_918      "%s :You cannot edit properties on this object"

#endif
