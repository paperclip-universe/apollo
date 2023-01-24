/*
 * memory.c - memory emulation
 *
 * Copyright (C) 1995-1998 David Firth
 * Copyright (C) 1998-2006 Atari800 development team (see DOC/CREDITS)
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atari.h"
#include "antic.h"
#include "cpu.h"
#include "cartridge.h"
#include "gtia.h"
#include "memory.h"
#include "pia.h"
#include "pokeysnd.h"
#include "util.h"
#include "statesav.h"

UBYTE memory[65536 + 2] __attribute__ ((aligned (4)));
UBYTE attrib[65536];
int cartA0BF_enabled = FALSE;

void MEMORY_InitialiseMachine(void) {
	memcpy(memory + 0xf800, atari_os, 0x800);
	dFillMem(0x0000, 0x00, 0xf800);
	SetRAM(0x0000, 0x3fff);
	SetROM(0x4000, 0xffff);
	SetHARDWARE(0xc000, 0xc0ff);	/* 5200 GTIA Chip */
	SetHARDWARE(0xd400, 0xd4ff);	/* 5200 ANTIC Chip */
	SetHARDWARE(0xe800, 0xe8ff);	/* 5200 POKEY Chip */
	SetHARDWARE(0xeb00, 0xebff);	/* 5200 POKEY Chip */
	Coldstart();
}

void MemStateSave(UBYTE SaveVerbose)
{
	SaveUBYTE(&memory[0], 65536);
	SaveUBYTE(&attrib[0], 65536);
}

void MemStateRead(UBYTE SaveVerbose) {
	ReadUBYTE(&memory[0], 65536);
	ReadUBYTE(&attrib[0], 65536);
}

void CopyFromMem(UWORD from, UBYTE *to, int size)
{
	while (--size >= 0) {
		*to++ = GetByte(from);
		from++;
	}
}

void CopyToMem(const UBYTE *from, UWORD to, int size)
{
	while (--size >= 0) {
		PutByte(to, *from);
		from++;
		to++;
	}
}
