/* 
	HCIDump - HCI packet analyzer	
	Copyright (C) 2000-2001 Maxim Krasnyansky <maxk@qualcomm.com>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License version 2 as
	published by the Free Software Foundation;

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.
	IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) AND AUTHOR(S) BE LIABLE FOR ANY CLAIM,
	OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER
	RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
	NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
	USE OR PERFORMANCE OF THIS SOFTWARE.

	ALL LIABILITY, INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PATENTS, COPYRIGHTS,
	TRADEMARKS OR OTHER RIGHTS, RELATING TO USE OF THIS SOFTWARE IS DISCLAIMED.
*/

/* 
	BNEP parser.
	Copyright (C) 2002 Takashi Sasai <sasai@sm.sony.co.jp>
*/

/*
 * $Id$
 */

#ifndef __BNEP_H
#define __BNEP_H

/* BNEP Type */
#define BNEP_GENERAL_ETHERNET		        0x00
#define BNEP_CONTROL                            0x01
#define BNEP_COMPRESSED_ETHERNET                0x02
#define BNEP_COMPRESSED_ETHERNET_SOURCE_ONLY    0x03
#define BNEP_COMPRESSED_ETHERNET_DEST_ONLY      0x04

/* BNEP Control Packet Type */
#define BNEP_CONTROL_COMMAND_NOT_UNDERSTOOD     0x00
#define BNEP_SETUP_CONNECTION_REQUEST_MSG       0x01
#define BNEP_SETUP_CONNECTION_RESPONSE_MSG      0x02
#define BNEP_FILTER_NET_TYPE_SET_MSG            0x03
#define BNEP_FILTER_NET_TYPE_RESPONSE_MSG       0x04 
#define BNEP_FILTER_MULT_ADDR_SET_MSG           0x05 
#define BNEP_FILTER_MULT_ADDR_RESPONSE_MSG      0x06

/* BNEP Extension Type */
#define BNEP_EXTENSION_CONTROL                  0x00 

#endif /* __BNEP_H */
