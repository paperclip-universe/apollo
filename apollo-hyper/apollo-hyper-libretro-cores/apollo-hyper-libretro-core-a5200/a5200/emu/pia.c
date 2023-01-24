/*
 * pia.c - PIA chip emulation
 *
 * Copyright (C) 1995-1998 David Firth
 * Copyright (C) 1998-2005 Atari800 development team (see DOC/CREDITS)
 *
 * This file is part of the Atari800 emulator project which emulates
 * the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.
 *
 * Atari800 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Atari800 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Atari800; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "config.h"

#include "atari.h"
#include "cpu.h"
#include "memory.h"
#include "pia.h"
#include "sio.h"
#include "input.h"
#include "statesav.h"

UBYTE PACTL;
UBYTE PBCTL;
UBYTE PORTA;
UBYTE PORTB;
UBYTE PORT_input[2];

static int xe_bank = 0;
static int selftest_enabled = 0;

UBYTE atari_basic[8192];
UBYTE atari_os[16384];

UBYTE PORTA_mask;
UBYTE PORTB_mask;

void PIA_Initialise(void)
{
	PACTL = 0x3f;
	PBCTL = 0x3f;
	PORTA = 0xff;
	PORTB = 0xff;
	PORTA_mask = 0xff;
	PORTB_mask = 0xff;
	PORT_input[0] = 0xff;
	PORT_input[1] = 0xff;
}

void PIA_Reset(void)
{
	PORTA = 0xff;
	PORTB = 0xff;
}

UBYTE PIA_GetByte(UWORD addr)
{
   switch (addr & 0x03)
   {
      case _PACTL:
         return PACTL & 0x3f;
      case _PBCTL:
         return PBCTL & 0x3f;
      case _PORTA:
         /* direction register */
         if ((PACTL & 0x04) == 0)
            return ~PORTA_mask;
         /* port state */
         return PORT_input[0] & (PORTA | PORTA_mask);
      case _PORTB:
         /* direction register */
         if ((PBCTL & 0x04) == 0)
            return ~PORTB_mask;
         /* port state */
         return PORT_input[1] & (PORTB | PORTB_mask);
   }
	/* for stupid compilers */
	return 0xff;
}

void PIA_PutByte(UWORD addr, UBYTE byte)
{
   switch (addr & 0x03)
   {
      case _PACTL:
         PACTL = byte;
         break;
      case _PBCTL:
         /* This code is part of the serial I/O emulation */
         /* The command line status has changed */
         if ((PBCTL ^ byte) & 0x08)
            SwitchCommandFrame(byte & 0x08 ? 0 : 1);
         PBCTL = byte;
         break;
      case _PORTA:
         /* set direction register */
         if ((PACTL & 0x04) == 0)
            PORTA_mask = ~byte;
         /* set output register */
         else
            PORTA = byte;		/* change from thor */
         break;
      case _PORTB:
         /* direction register */
         if ((PBCTL & 0x04) == 0)
            PORTB_mask = ~byte;
         else
            PORTB = byte;
         break;
   }
}

void PIAStateSave(void)
{
	int Ram256 = 0;

	SaveUBYTE( &PACTL, 1 );
	SaveUBYTE( &PBCTL, 1 );
	SaveUBYTE( &PORTA, 1 );
	SaveUBYTE( &PORTB, 1 );

	SaveINT( &xe_bank, 1 );
	SaveINT( &selftest_enabled, 1 );
	SaveINT( &Ram256, 1 );

	SaveINT( &cartA0BF_enabled, 1 );

	SaveUBYTE( &PORTA_mask, 1 );
	SaveUBYTE( &PORTB_mask, 1 );
}

void PIAStateRead(void)
{
	int Ram256 = 0;

	ReadUBYTE( &PACTL, 1 );
	ReadUBYTE( &PBCTL, 1 );
	ReadUBYTE( &PORTA, 1 );
	ReadUBYTE( &PORTB, 1 );

	ReadINT( &xe_bank, 1 );
	ReadINT( &selftest_enabled, 1 );
	ReadINT( &Ram256, 1 );

	ReadINT( &cartA0BF_enabled, 1 );

	ReadUBYTE( &PORTA_mask, 1 );
	ReadUBYTE( &PORTB_mask, 1 );
}
