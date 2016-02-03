/*
 * charybdis: An advanced ircd.
 * exmask.h: Management for user exemption masks.
 *
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id$
 */

#ifndef EXMASK_H
#define EXMASK_H


#define EX_EXTENDCHANS      0x00000001   /* Extended channel limit                                */
#define EX_RESV             0x00000002   /* Can use reserved names                                */
#define EX_KLINE            0x00000004   /* Cannot be K-Lined                                     */
#define EX_FLOOD            0x00000008   /* Bypasses various repetition limits                    */
#define EX_SPAM             0x00000010   /* Bypasses the spam detections                          */
#define EX_SHIDE            0x00000020   /* Can see additional server data                        */
#define EX_JUPE             0x00000040   /* Can use juped items                                   */
#define EX_ACCEPT           0x00000080   /* Walks over +g/+R or some other acceptances            */
#define EX_DYNSPOOF         0x00000100   /* Can see through dynspoofs/cloaks                      */
#define EX_INVISOP          0x00000200   /* Allows +i with extensions/no_oper_invis               */
#define EX_STATSP           0x00000400   /* Won't show up in /STATS p                             */
#define EX_STATSL           0x00000800   /* Won't show up in /STATS l                             */
#define EX_WHOISOPS         0x00001000   /* Bypasses conf.hide_opers_in_whois                     */
#define EX_JOINSPLIT        0x00002000   /* Can join and/or create channels on netsplits          */


extern uint exmask_modes[256];

// Convert bitmask to mode character string (static)
const char *exmask_to_modes(const uint mask);

// Convert one bitflag to its mode character (otherwise '+')
char exmask_to_mode(const uint flag);

// Convert mode character string to bitmask (starting from mask)
uint exmask_to_mask(uint mask, const char *buf);

// Find next available bit (0 on fail) (for modules)
uint exmask_find_slot(void);


#endif
