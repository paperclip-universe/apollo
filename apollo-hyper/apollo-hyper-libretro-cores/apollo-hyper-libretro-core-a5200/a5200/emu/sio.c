/*
 * sio.c - Serial I/O emulation
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "antic.h"  /* ypos */
#include "atari.h"
#include "compfile.h"
#include "cpu.h"
#include "memory.h"
#include "pokeysnd.h"
#include "sio.h"
#include "util.h"
#include "statesav.h"

/* If ATR image is in double density (256 bytes per sector),
   then the boot sectors (sectors 1-3) can be:
   - logical (as seen by Atari) - 128 bytes in each sector
   - physical (as stored on the disk) - 256 bytes in each sector.
     Only the first half of sector is used for storing data, the rest is zero.
   - SIO2PC (the type used by the SIO2PC program) - 3 * 128 bytes for data
     of boot sectors, then 3 * 128 unused bytes (zero)
   The XFD images in double density have either logical or physical
   boot sectors. */
#define BOOT_SECTORS_LOGICAL	0
#define BOOT_SECTORS_PHYSICAL	1
#define BOOT_SECTORS_SIO2PC		2
static int boot_sectors_type[MAX_DRIVES];

static int header_size[MAX_DRIVES];
static FILE *disk[MAX_DRIVES] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static int sectorcount[MAX_DRIVES];
static int sectorsize[MAX_DRIVES];
static int format_sectorcount[MAX_DRIVES];
static int format_sectorsize[MAX_DRIVES];
static int io_success[MAX_DRIVES];

UnitStatus drive_status[MAX_DRIVES];
char sio_filename[MAX_DRIVES][FILENAME_MAX];

Util_tmpbufdef(static, sio_tmpbuf[MAX_DRIVES])

/* Serial I/O emulation support */
static UBYTE CommandFrame[6];
static int CommandIndex = 0;
static UBYTE DataBuffer[256 + 3];
static int DataIndex = 0;
static int TransferStatus = SIO_NoFrame;
static int ExpectedBytes = 0;

void SIO_Initialise(void) {
	int i;
	for (i = 0; i < MAX_DRIVES; i++) {
		strcpy(sio_filename[i], "Off");
		drive_status[i] = Off;
		format_sectorsize[i] = 128;
		format_sectorcount[i] = 720;
	}
	TransferStatus = SIO_NoFrame;
}

static void SIO_Dismount(int diskno)
{
	if (disk[diskno - 1] != NULL) {
		Util_fclose(disk[diskno - 1], sio_tmpbuf[diskno - 1]);
		disk[diskno - 1] = NULL;
		drive_status[diskno - 1] = NoDisk;
		strcpy(sio_filename[diskno - 1], "Empty");
	}
}


/* umount disks so temporary files are deleted */
void SIO_Exit(void) {
	int i;
	for (i = 1; i <= MAX_DRIVES; i++)
		SIO_Dismount(i);
}

static int SIO_Mount(int diskno, const char *filename, int b_open_readonly) {
	FILE *f = NULL;
	UnitStatus status = ReadWrite;
	struct ATR_Header header;

	/* avoid overruns in sio_filename[] */
	if (strlen(filename) >= FILENAME_MAX)
		return FALSE;

	/* release previous disk */
	SIO_Dismount(diskno);

	/* open file */
	if (!b_open_readonly)
		f = Util_fopen(filename, "rb+", sio_tmpbuf[diskno - 1]);
	if (f == NULL) {
		f = Util_fopen(filename, "rb", sio_tmpbuf[diskno - 1]);
		if (f == NULL)
			return FALSE;
		status = ReadOnly;
	}

	/* read header */
	if (fread(&header, 1, sizeof(struct ATR_Header), f) != sizeof(struct ATR_Header)) {
		fclose(f);
		return FALSE;
	}

	/* detect compressed image and uncompress */
	switch (header.magic1) {
	case 0xf9:
	case 0xfa:
		/* DCM */
		{
			FILE *f2 = Util_tmpopen(sio_tmpbuf[diskno - 1]);
			if (f2 == NULL)
				return FALSE;
			Util_rewind(f);
			if (!CompressedFile_DCMtoATR(f, f2)) {
				Util_fclose(f2, sio_tmpbuf[diskno - 1]);
				fclose(f);
				return FALSE;
			}
			fclose(f);
			f = f2;
		}
		Util_rewind(f);
		if (fread(&header, 1, sizeof(struct ATR_Header), f) != sizeof(struct ATR_Header)) {
			Util_fclose(f, sio_tmpbuf[diskno - 1]);
			return FALSE;
		}
		status = ReadOnly;
		/* XXX: status = b_open_readonly ? ReadOnly : ReadWrite; */
		break;
	case 0x1f:
		if (header.magic2 == 0x8b) {
			/* ATZ/ATR.GZ, XFZ/XFD.GZ */
			fclose(f);
			f = Util_tmpopen(sio_tmpbuf[diskno - 1]);
			if (f == NULL)
				return FALSE;
			if (!CompressedFile_ExtractGZ(filename, f)) {
				Util_fclose(f, sio_tmpbuf[diskno - 1]);
				return FALSE;
			}
			Util_rewind(f);
			if (fread(&header, 1, sizeof(struct ATR_Header), f) != sizeof(struct ATR_Header)) {
				Util_fclose(f, sio_tmpbuf[diskno - 1]);
				return FALSE;
			}
			status = ReadOnly;
			/* XXX: status = b_open_readonly ? ReadOnly : ReadWrite; */
		}
		break;
	default:
		break;
	}

	boot_sectors_type[diskno - 1] = BOOT_SECTORS_LOGICAL;

	if (header.magic1 == MAGIC1 && header.magic2 == MAGIC2) {
		/* ATR (may be temporary from DCM or ATR/ATR.GZ) */
		header_size[diskno - 1] = 16;

		sectorsize[diskno - 1] = (header.secsizehi << 8) + header.secsizelo;
		if (sectorsize[diskno - 1] != 128 && sectorsize[diskno - 1] != 256) {
			Util_fclose(f, sio_tmpbuf[diskno - 1]);
			return FALSE;
		}

		if (header.writeprotect != 0)
			status = ReadOnly;

		/* ATR header contains length in 16-byte chunks. */
		/* First compute number of 128-byte chunks
		   - it's number of sectors on single density disk */
		sectorcount[diskno - 1] = ((header.hiseccounthi << 24)
			+ (header.hiseccountlo << 16)
			+ (header.seccounthi << 8)
			+ header.seccountlo) >> 3;

		/* Fix number of sectors if double density */
		if (sectorsize[diskno - 1] == 256) {
			if ((sectorcount[diskno - 1] & 1) != 0)
				/* logical (128-byte) boot sectors */
				sectorcount[diskno - 1] += 3;
			else {
				/* 256-byte boot sectors */
				/* check if physical or SIO2PC: physical if there's
				   a non-zero byte in bytes 0x190-0x30f of the ATR file */
				UBYTE buffer[0x180];
				int i;
				fseek(f, 0x190, SEEK_SET);
				if (fread(buffer, 1, 0x180, f) != 0x180) {
					Util_fclose(f, sio_tmpbuf[diskno - 1]);
					return FALSE;
				}
				boot_sectors_type[diskno - 1] = BOOT_SECTORS_SIO2PC;
				for (i = 0; i < 0x180; i++)
					if (buffer[i] != 0) {
						boot_sectors_type[diskno - 1] = BOOT_SECTORS_PHYSICAL;
						break;
					}
			}
			sectorcount[diskno - 1] >>= 1;
		}
	}
	else {
		/* XFD (may be temporary from XFZ/XFD.GZ) */
		int file_length = Util_flen(f);

		header_size[diskno - 1] = 0;

		if (file_length <= (1040 * 128)) {
			/* single density */
			sectorsize[diskno - 1] = 128;
			sectorcount[diskno - 1] = file_length >> 7;
		}
		else {
			/* double density */
			sectorsize[diskno - 1] = 256;
			if ((file_length & 0xff) == 0) {
				boot_sectors_type[diskno - 1] = BOOT_SECTORS_PHYSICAL;
				sectorcount[diskno - 1] = file_length >> 8;
			}
			else
				sectorcount[diskno - 1] = (file_length + 0x180) >> 8;
		}
	}

	format_sectorsize[diskno - 1] = sectorsize[diskno - 1];
	format_sectorcount[diskno - 1] = sectorcount[diskno - 1];
	strcpy(sio_filename[diskno - 1], filename);
	drive_status[diskno - 1] = status;
	disk[diskno - 1] = f;
	return TRUE;
}

static void SizeOfSector(UBYTE unit, int sector, int *sz, ULONG *ofs)
{
	int size;
	ULONG offset;

	if (start_binloading) {
		if (sz)
			*sz = 128;
		if (ofs)
			*ofs = 0;
		return;
	}

	if (sector < 4) {
		/* special case for first three sectors in ATR and XFD image */
		size = 128;
		offset = header_size[unit] + (sector - 1) * (boot_sectors_type[unit] == BOOT_SECTORS_PHYSICAL ? 256 : 128);
	}
	else {
		size = sectorsize[unit];
		offset = header_size[unit] + (boot_sectors_type[unit] == BOOT_SECTORS_LOGICAL ? 0x180 : 0x300) + (sector - 4) * size;
	}

	if (sz)
		*sz = size;

	if (ofs)
		*ofs = offset;
}

static int SeekSector(int unit, int sector)
{
	ULONG offset;
	int size;

	SizeOfSector((UBYTE) unit, sector, &size, &offset);
	fseek(disk[unit], offset, SEEK_SET);

	return size;
}

static int WriteSector(int unit, int sector, const UBYTE *buffer)
{
	int size;
	io_success[unit] = -1;
	if (drive_status[unit] == Off)
		return 0;
	if (disk[unit] == NULL)
		return 'N';
	if (drive_status[unit] != ReadWrite || sector <= 0 || sector > sectorcount[unit])
		return 'E';
	size = SeekSector(unit, sector);
	fwrite(buffer, 1, size, disk[unit]);
	io_success[unit] = 0;
	return 'C';
}

/* Set density and number of sectors
   This function is used before the format (0x21) command
   to set how the disk will be formatted.
   Note this function does *not* affect the currently attached disk
   (previously sectorsize/sectorcount were used which could result in
   a corrupted image).
*/
static int WriteStatusBlock(int unit, const UBYTE *buffer)
{
	int size;
	if (drive_status[unit] == Off)
		return 0;
	/* We only care about the density and the sector count here.
	   Setting everything else right here seems to be non-sense.
	   I'm not sure about this density settings, my XF551
	   honors only the sector size and ignores the density */
	size = buffer[6] * 256 + buffer[7];
	if (size == 128 || size == 256)
		format_sectorsize[unit] = size;
	/* Note that the number of heads are minus 1 */
	format_sectorcount[unit] = buffer[0] * (buffer[2] * 256 + buffer[3]) * (buffer[4] + 1);
	if (format_sectorcount[unit] < 1 || format_sectorcount[unit] > 65535)
		format_sectorcount[unit] = 720;
	return 'C';
}

UBYTE SIO_ChkSum(const UBYTE *buffer, int length)
{
	int checksum = 0;
	while (--length >= 0)
		checksum += *buffer++;
	do
		checksum = (checksum & 0xff) + (checksum >> 8);
	while (checksum > 255);
	return checksum;
}

/* Enable/disable the command frame */
void SwitchCommandFrame(int onoff)
{
	if (onoff) {				/* Enabled */
		CommandIndex = 0;
		DataIndex = 0;
		ExpectedBytes = 5;
		TransferStatus = SIO_CommandFrame;
	}
	else {
		if (TransferStatus != SIO_StatusRead && TransferStatus != SIO_NoFrame &&
			TransferStatus != SIO_ReadFrame) {
			TransferStatus = SIO_NoFrame;
		}
		CommandIndex = 0;
	}
}

static UBYTE WriteSectorBack(void)
{
	UWORD sector;
	UBYTE unit;

	sector = CommandFrame[2] + (CommandFrame[3] << 8);
	unit = CommandFrame[0] - '1';
	if (unit >= MAX_DRIVES)		/* UBYTE range ! */
		return 0;
	switch (CommandFrame[1]) {
	case 0x4f:				/* Write Status Block */
		return WriteStatusBlock(unit, DataBuffer);
	case 0x50:				/* Write */
	case 0x57:
	case 0xD0:				/* xf551 hispeed */
	case 0xD7:
		return WriteSector(unit, sector, DataBuffer);
	default:
		return 'E';
	}
}

/* Put a byte that comes out of POKEY. So get it here... */
void SIO_PutByte(int byte)
{
	switch (TransferStatus) {
	case SIO_CommandFrame:
		if (CommandIndex < ExpectedBytes) {
			CommandFrame[CommandIndex++] = byte;
			if (CommandIndex >= ExpectedBytes) {
				if (CommandFrame[0] >= 0x31 && CommandFrame[0] <= 0x38) {
					TransferStatus = SIO_StatusRead;
					DELAYED_SERIN_IRQ = SERIN_INTERVAL + ACK_INTERVAL;
				}
				else
					TransferStatus = SIO_NoFrame;
			}
		}
		else {
			TransferStatus = SIO_NoFrame;
		}
		break;
	case SIO_WriteFrame:		/* Expect data */
		if (DataIndex < ExpectedBytes) {
			DataBuffer[DataIndex++] = byte;
			if (DataIndex >= ExpectedBytes) {
				UBYTE sum = SIO_ChkSum(DataBuffer, ExpectedBytes - 1);
				if (sum == DataBuffer[ExpectedBytes - 1]) {
					UBYTE result = WriteSectorBack();
					if (result != 0) {
						DataBuffer[0] = 'A';
						DataBuffer[1] = result;
						DataIndex = 0;
						ExpectedBytes = 2;
						DELAYED_SERIN_IRQ = SERIN_INTERVAL + ACK_INTERVAL;
						TransferStatus = SIO_FinalStatus;
					}
					else
						TransferStatus = SIO_NoFrame;
				}
				else {
					DataBuffer[0] = 'E';
					DataIndex = 0;
					ExpectedBytes = 1;
					DELAYED_SERIN_IRQ = SERIN_INTERVAL + ACK_INTERVAL;
					TransferStatus = SIO_FinalStatus;
				}
			}
		}
		break;
	}
	DELAYED_SEROUT_IRQ = SEROUT_INTERVAL;
}

void SIOStateSave(void)
{
	int i;

	for (i = 0; i < 8; i++) {
		SaveINT((int *) &drive_status[i], 1);
		SaveFNAME(sio_filename[i]);
	}
}

void SIOStateRead(void)
{
	int i;

	for (i = 0; i < 8; i++) {
		int saved_drive_status;
		char filename[FILENAME_MAX];

		ReadINT(&saved_drive_status, 1);
		drive_status[i] = saved_drive_status;

		ReadFNAME(filename);
		if (filename[0] == 0)
			continue;

		/* If the disk drive wasn't empty or off when saved,
		   mount the disk */
		switch (saved_drive_status) {
		case ReadOnly:
			SIO_Mount(i + 1, filename, TRUE);
			break;
		case ReadWrite:
			SIO_Mount(i + 1, filename, FALSE);
			break;
		default:
			break;
		}
	}
}
