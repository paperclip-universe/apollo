/*
 * devices.c - emulation of H:, P:, E: and K: Atari devices
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
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef _WIN32
#include <direct.h> /* mkdir, rmdir */
#else
#include <unistd.h>
#endif
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#include "atari.h"
#include "cpu.h"
#include "devices.h"
#include "memory.h"
#include "sio.h"
#include "util.h"

#ifndef S_IREAD
#define S_IREAD S_IRUSR
#endif
#ifndef S_IWRITE
#define S_IWRITE S_IWUSR
#endif

#ifdef _WIN32
#include <windows.h>

#undef FILENAME_CONV
#undef FILENAME

#ifdef UNICODE
#define FILENAME_CONV \
	WCHAR wfilename[FILENAME_MAX]; \
	if (MultiByteToWideChar(CP_ACP, 0, filename, -1, wfilename, FILENAME_MAX) <= 0) \
		return FALSE;
#define FILENAME wfilename
#else /* UNICODE */
#define FILENAME_CONV
#define FILENAME filename
#endif /* UNICODE */

#endif /* WIN32 */


/* Read Directory abstraction layer -------------------------------------- */

#ifdef _WIN32
static char dir_path[FILENAME_MAX];
static WIN32_FIND_DATA wfd;
static HANDLE dh = INVALID_HANDLE_VALUE;

static int Device_OpenDir(const char *filename)
{
	FILENAME_CONV;
	Util_splitpath(filename, dir_path, NULL);
	if (dh != INVALID_HANDLE_VALUE)
		FindClose(dh);
	dh = FindFirstFile(FILENAME, &wfd);
	if (dh == INVALID_HANDLE_VALUE) {
		/* don't raise error if the path is ok but no file matches:
		   Win98 returns ERROR_FILE_NOT_FOUND,
		   WinCE returns ERROR_NO_MORE_FILES */
		DWORD err = GetLastError();
		if (err != ERROR_FILE_NOT_FOUND && err != ERROR_NO_MORE_FILES)
			return FALSE;
	}
	return TRUE;
}

static int Device_ReadDir(char *fullpath, char *filename, int *isdir,
                          int *readonly, int *size, char *timetext)
{
#ifdef UNICODE
	char afilename[MAX_PATH];
#endif
	if (dh == INVALID_HANDLE_VALUE)
		return FALSE;
	/* don't match "." nor ".."  */
	while (wfd.cFileName[0] == '.' &&
	       (wfd.cFileName[1] == '\0' || (wfd.cFileName[1] == '.' && wfd.cFileName[2] == '\0'))
	) {
		if (!FindNextFile(dh, &wfd)) {
			FindClose(dh);
			dh = INVALID_HANDLE_VALUE;
			return FALSE;
		}
	}
#ifdef UNICODE
	if (WideCharToMultiByte(CP_ACP, 0, wfd.cFileName, -1, afilename, MAX_PATH, NULL, NULL) <= 0)
		strcpy(afilename, "?ERROR");
#define FOUND_FILENAME afilename
#else
#define FOUND_FILENAME wfd.cFileName
#endif /* UNICODE */
	if (filename != NULL)
		strcpy(filename, FOUND_FILENAME);
	if (fullpath != NULL)
		Util_catpath(fullpath, dir_path, FOUND_FILENAME);
	if (isdir != NULL)
		*isdir = (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? TRUE : FALSE;
	if (readonly != NULL)
		*readonly = (wfd.dwFileAttributes & FILE_ATTRIBUTE_READONLY) ? TRUE : FALSE;
	if (size != NULL)
		*size = (int) wfd.nFileSizeLow;
	if (timetext != NULL) {
		FILETIME lt;
		SYSTEMTIME st;
		if (FileTimeToLocalFileTime(&wfd.ftLastWriteTime, &lt) != 0
		 && FileTimeToSystemTime(&lt, &st) != 0) {
			int hour = st.wHour;
			char ampm = 'a';
			if (hour >= 12) {
				hour -= 12;
				ampm = 'p';
			}
			if (hour == 0)
				hour = 12;
			sprintf(timetext, "%2d-%02d-%02d %2d:%02d%c",
				st.wMonth, st.wDay, st.wYear % 100, hour, st.wMinute, ampm);
		}
		else
			strcpy(timetext, " 1-01-01 12:00p");
	}

	if (!FindNextFile(dh, &wfd)) {
		FindClose(dh);
		dh = INVALID_HANDLE_VALUE;
	}
	return TRUE;
}

#define DO_DIR

#elif defined(HAVE_OPENDIR)

static int match(const char *pattern, const char *filename)
{
	if (strcmp(pattern, "*.*") == 0)
		return TRUE;

	for (;;) {
		switch (*pattern) {
		case '\0':
			return (*filename == '\0');
		case '?':
			if (*filename == '\0' || *filename == '.')
				return FALSE;
			pattern++;
			filename++;
			break;
		case '*':
			if (Util_chrieq(*filename, pattern[1]))
				pattern++;
			else if (*filename == '\0')
				return FALSE; /* because pattern[1] != '\0' */
			else
				filename++;
			break;
		default:
			if (!Util_chrieq(*pattern, *filename))
				return FALSE;
			pattern++;
			filename++;
			break;
		}
	}
}

static char dir_path[FILENAME_MAX];
static char filename_pattern[FILENAME_MAX];
static DIR *dp = NULL;

static int Device_OpenDir(const char *filename)
{
	Util_splitpath(filename, dir_path, filename_pattern);
	if (dp != NULL)
		closedir(dp);
	dp = opendir(dir_path);
	return dp != NULL;
}

static int Device_ReadDir(char *fullpath, char *filename, int *isdir,
                          int *readonly, int *size, char *timetext)
{
	struct dirent *entry;
	char temppath[FILENAME_MAX];
#ifdef HAVE_STAT
	struct stat status;
#endif
	for (;;) {
		entry = readdir(dp);
		if (entry == NULL) {
			closedir(dp);
			dp = NULL;
			return FALSE;
		}
		if (entry->d_name[0] == '.') {
			/* don't match Unix hidden files unless specifically requested */
			if (filename_pattern[0] != '.')
				continue;
			/* never match "." */
			if (entry->d_name[1] == '\0')
				continue;
			/* never match ".." */
			if (entry->d_name[1] == '.' && entry->d_name[2] == '\0')
				continue;
		}
		if (match(filename_pattern, entry->d_name))
			break;
	}
	if (filename != NULL)
		strcpy(filename, entry->d_name);
	Util_catpath(temppath, dir_path, entry->d_name);
	if (fullpath != NULL)
		strcpy(fullpath, temppath);
#ifdef HAVE_STAT
	if (stat(temppath, &status) == 0) {
		if (isdir != NULL)
			*isdir = (status.st_mode & S_IFDIR) ? TRUE : FALSE;
		if (readonly != NULL)
			*readonly = (status.st_mode & S_IWRITE) ? FALSE : TRUE;
		if (size != NULL)
			*size = (int) status.st_size;
		if (timetext != NULL) {
#ifdef HAVE_LOCALTIME
			struct tm *ft;
			int hour;
			char ampm = 'a';
			ft = localtime(&status.st_mtime);
			hour = ft->tm_hour;
			if (hour >= 12) {
				hour -= 12;
				ampm = 'p';
			}
			if (hour == 0)
				hour = 12;
			sprintf(timetext, "%2d-%02d-%02d %2d:%02d%c",
				ft->tm_mon + 1, ft->tm_mday, ft->tm_year % 100,
				hour, ft->tm_min, ampm);
#else
			strcpy(timetext, " 1-01-01 12:00p");
#endif /* HAVE_LOCALTIME */
		}
	}
	else
#endif /* HAVE_STAT */
	{
		if (isdir != NULL)
			*isdir = FALSE;
		if (readonly != NULL)
			*readonly = FALSE;
		if (size != NULL)
			*size = 0;
		if (timetext != NULL)
			strcpy(timetext, " 1-01-01 12:00p");
	}
	return TRUE;
}

#define DO_DIR

#elif defined(PS2)

extern char dir_path[FILENAME_MAX];

int Atari_OpenDir(const char *filename);

#define Device_OpenDir Atari_OpenDir

int Atari_ReadDir(char *fullpath, char *filename, int *isdir,
                  int *readonly, int *size, char *timetext);

static int Device_ReadDir(char *fullpath, char *filename, int *isdir,
                          int *readonly, int *size, char *timetext)
{
	char tmp_filename[FILENAME_MAX];
	if (filename == NULL)
		filename = tmp_filename;
	do {
		if (!Atari_ReadDir(fullpath, filename, isdir, readonly, size, timetext))
			return FALSE;
		/* reject "." and ".." */
	} while (filename[0] == '.' &&
	         (filename[1] == '\0' || (filename[1] == '.' && filename[2] == '\0')));
	return TRUE;
}

#define DO_DIR

#endif /* defined(PS2) */


/* Rename File/Directory abstraction layer ------------------------------- */

#ifdef _WIN32
#define DO_RENAME
#elif defined(HAVE_RENAME)
#define DO_RENAME
#endif


/* Set/Reset Read-Only Attribute abstraction layer ----------------------- */

#ifdef _WIN32
/* Enables/disables read-only mode for the file. Returns TRUE on success. */
#define DO_LOCK
#elif defined(HAVE_CHMOD)
#define DO_LOCK
#endif /* defined(HAVE_CHMOD) */

/* Make Directory abstraction layer -------------------------------------- */

#ifdef _WIN32
#define DO_MKDIR
#elif defined(HAVE_MKDIR)
#define DO_MKDIR
#endif /* defined(HAVE_MKDIR) */

/* Remove Directory abstraction layer ------------------------------------ */
#ifdef _WIN32
#define DO_RMDIR
#elif defined(HAVE_RMDIR)
#define DO_RMDIR
#endif /* defined(HAVE_RMDIR) */

/* H: device emulation --------------------------------------------------- */

/* host path for each H: unit */
static char atari_h_dir[4][FILENAME_MAX] = { "", "", "", "" };

/* ';'-separated list of Atari paths checked by the "load executable"
   command. if a path does not start with "Hn:", then the selected device
   is used. */
static char h_exe_path[FILENAME_MAX] = DEFAULT_H_PATH;

/* h_current_dir must be empty or terminated with DIR_SEP_CHAR;
   only DIR_SEP_CHAR can be used as a directory separator here */
static char h_current_dir[4][FILENAME_MAX];

/* stream open via H: device per IOCB */
static FILE *h_fp[8] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

/* H: text mode per IOCB */
static int h_textmode[8];

/* last read character was CR, per IOCB */
static int h_wascr[8];

/* last operation: 'o': open, 'r': read, 'w': write, per IOCB */
/* (this is needed to apply fseek(fp, 0, SEEK_CUR) between reads and writes
   in update (12) mode) */
static char h_lastop[8];

Util_tmpbufdef(static, h_tmpbuf[8])

/* IOCB #, 0-7 */
static int h_iocb;

/* H: device number, 0-3 */
static int h_devnum;

/* filename as specified after "Hn:" */
static char atari_filename[FILENAME_MAX];

/* atari_filename applied to H:'s current dir, with DIR_SEP_CHARs only */
static char atari_path[FILENAME_MAX];

/* full filename for the current operation */
static char host_path[FILENAME_MAX];

void Device_Initialise(void) {
	int i;
	h_current_dir[0][0] = '\0';
	h_current_dir[1][0] = '\0';
	h_current_dir[2][0] = '\0';
	h_current_dir[3][0] = '\0';
	for (i = 0; i < 8; i++)
		if (h_fp[i] != NULL) {
			Util_fclose(h_fp[i], h_tmpbuf[i]);
			h_fp[i] = NULL;
		}
}

#define IS_DIR_SEP(c) ((c) == '/' || (c) == '\\' || (c) == ':' || (c) == '>')

static int Device_IsValidForFilename(char ch)
{
	if ((ch >= 'A' && ch <= 'Z')
	 || (ch >= 'a' && ch <= 'z')
	 || (ch >= '0' && ch <= '9'))
		return TRUE;
	switch (ch) {
	case '!':
	case '#':
	case '$':
	case '&':
	case '\'':
	case '(':
	case ')':
	case '*':
	case '-':
	case '.':
	case '?':
	case '@':
	case '_':
		return TRUE;
	default:
		break;
	}
	return FALSE;
}

static UWORD Device_SkipDeviceName(void)
{
	UWORD bufadr;
	for (bufadr = dGetWordAligned(ICBALZ); ; bufadr++) {
		char c = (char) dGetByte(bufadr);
		if (c == ':')
			return (UWORD) (bufadr + 1);
		if (c < '!' || c > '\x7e')
			return 0;
	}
}

/* devnum must be 0-3; p must point inside atari_filename */
static UWORD Device_GetAtariPath(int devnum, char *p)
{
	UWORD bufadr = Device_SkipDeviceName();
	if (bufadr != 0) {
		while (p < atari_filename + sizeof(atari_filename) - 1) {
			char c = (char) dGetByte(bufadr);
			if (Device_IsValidForFilename(c) || IS_DIR_SEP(c) || c == '<') {
				*p++ = c;
				bufadr++;
			}
			else {
				/* end of filename */
				/* now apply it to h_current_dir */
				const char *q = atari_filename;
				*p = '\0';
				if (IS_DIR_SEP(*q)) {
					/* absolute path on H: device */
					q++;
					p = atari_path;
				}
				else {
					strcpy(atari_path, h_current_dir[devnum]);
					p = atari_path + strlen(atari_path);
				}
				for (;;) {
					/* we are here at the beginning of a path element,
					   i.e. at the beginning of atari_path or after DIR_SEP_CHAR */
					if (*q == '<'
					 || (*q == '.' && q[1] == '.' && (q[2] == '\0' || IS_DIR_SEP(q[2])))) {
						/* "<" or "..": parent directory */
						if (p == atari_path) {
							regY = 150; /* Sparta: directory not found */
							regP |= N_FLAG;
							return 0;
						}
						do
							p--;
						while (p > atari_path && p[-1] != DIR_SEP_CHAR);
						if (*q == '.') {
							if (q[2] != '\0')
								q++;
							q++;
						}
						q++;
						continue;
					}
					if (IS_DIR_SEP(*q)) {
						/* duplicate DIR_SEP */
						regY = 165; /* bad filename */
						regP |= N_FLAG;
						return 0;
					}
					do {
						if (p >= atari_path + sizeof(atari_path) - 1) {
							regY = 165; /* bad filename */
							regP |= N_FLAG;
							return 0;
						}
						*p++ = *q;
						if (*q == '\0')
							return bufadr;
						q++;
					} while (!IS_DIR_SEP(*q));
					*p++ = DIR_SEP_CHAR;
					q++;
				}
			}
		}
	}
	regY = 165; /* bad filename */
	regP |= N_FLAG;
	return 0;
}

static int Device_GetIOCB(void)
{
	if ((regX & 0x8f) != 0) {
		regY = 134; /* invalid IOCB number */
		regP |= N_FLAG;
		return FALSE;
	}
	h_iocb = regX >> 4;
	return TRUE;
}

static int Device_GetNumber(int set_textmode)
{
	int devnum;
	if (!Device_GetIOCB())
		return -1;
	devnum = dGetByte(ICDNOZ);
	if (devnum > 9 || devnum == 0 || devnum == 5) {
		regY = 160; /* invalid unit/drive number */
		regP |= N_FLAG;
		return -1;
	}
	if (devnum < 5) {
		if (set_textmode)
			h_textmode[h_iocb] = FALSE;
		return devnum - 1;
	}
	if (set_textmode)
		h_textmode[h_iocb] = TRUE;
	return devnum - 6;
}

static UWORD Device_GetHostPath(int set_textmode)
{
	UWORD bufadr;
	h_devnum = Device_GetNumber(set_textmode);
	if (h_devnum < 0)
		return 0;
	bufadr = Device_GetAtariPath(h_devnum, atari_filename);
	if (bufadr == 0)
		return 0;
	Util_catpath(host_path, atari_h_dir[h_devnum], atari_path);
	return bufadr;
}

static void Device_H_Open(void)
{
	FILE *fp;
	UBYTE aux1;
#ifdef DO_DIR
	UBYTE aux2;
	char entryname[FILENAME_MAX];
	int isdir;
	int readonly;
	int size;
	char timetext[16];
#endif

	if (Device_GetHostPath(TRUE) == 0)
		return;

	if (h_fp[h_iocb] != NULL)
		Util_fclose(h_fp[h_iocb], h_tmpbuf[h_iocb]);

	fp = NULL;
	h_wascr[h_iocb] = FALSE;
	h_lastop[h_iocb] = 'o';

	aux1 = dGetByte(ICAX1Z);
	switch (aux1) {
	case 4:
		/* don't bother using "r" for textmode:
		   we want to support LF, CR/LF and CR, not only native EOLs */
		fp = Util_fopen(host_path, "rb", h_tmpbuf[h_iocb]);
		if (fp != NULL) {
			regY = 1;
			regP &= (~N_FLAG);
		}
		else {
			regY = 170; /* file not found */
			regP |= N_FLAG;
		}
		break;
#ifdef DO_DIR
	case 6:
	case 7:
		fp = Util_tmpopen(h_tmpbuf[h_iocb]);
		if (fp == NULL) {
			regY = 144; /* device done error */
			regP |= N_FLAG;
			break;
		}
		if (!Device_OpenDir(host_path)) {
			Util_fclose(fp, h_tmpbuf[h_iocb]);
			fp = NULL;
			regY = 144; /* device done error */
			regP |= N_FLAG;
			break;
		}
		aux2 = dGetByte(ICAX2Z);
		if (aux2 >= 128) {
			fprintf(fp, "\nVolume:    HDISK%c\nDirectory: ", '1' + h_devnum);
			if (strchr(atari_path, DIR_SEP_CHAR) == NULL)
				fprintf(fp, "MAIN\n\n");
			else {
				char end_dir_str[FILENAME_MAX];
				Util_splitpath(dir_path, NULL, end_dir_str);
				fprintf(fp, "%s\n\n", end_dir_str);
			}
		}

		while (Device_ReadDir(NULL, entryname, &isdir, &readonly, &size,
		                      (aux2 >= 128) ? timetext : NULL)) {
			char *ext = strrchr(entryname, '.');
			if (ext == NULL)
				ext = "";
			else {
				/* replace the dot with NUL,
				   so entryname is without extension */
				*ext++ = '\0';
				if (ext[0] != '\0' && ext[1] != '\0' && ext[2] != '\0' && ext[3] != '\0') {
					ext[2] = '+';
					ext[3] = '\0';
				}
			}
			if (strlen(entryname) > 8) {
				entryname[7] = '+';
				entryname[8] = '\0';
			}
			if (aux2 >= 128) {
				if (isdir)
					fprintf(fp, "%-13s<DIR>  %s\n", entryname, timetext);
				else {
					if (size > 999999)
						size = 999999;
					fprintf(fp, "%-9s%-3s %6d %s\n", entryname, ext, size, timetext);
				}
			}
			else {
				char dirchar = ' ';
				size = (size + 255) >> 8;
				if (size > 999)
					size = 999;
				if (isdir) {
					if (dGetByte(0x700) == 'M') /* MyDOS */
						dirchar = ':';
					else /* Sparta */
						ext = "\304\311\322"; /* "DIR" with bit 7 set */
				}
				fprintf(fp, "%c%c%-8s%-3s %03d\n", readonly ? '*' : ' ',
				        dirchar, entryname, ext, size);
			}
		}

		if (aux2 >= 128)
			fprintf(fp, "   999 FREE SECTORS\n");
		else
			fprintf(fp, "999 FREE SECTORS\n");

		Util_rewind(fp);
		h_textmode[h_iocb] = TRUE;
		regY = 1;
		regP &= (~N_FLAG);
		break;
#endif /* DO_DIR */
	case 8: /* write: "w" */
	case 9: /* write at end of file (append): "a" */
	case 12: /* write and read (update): "r+" || "w+" */
	case 13: /* append and read: "a+" */
		regY = 163; /* disk write-protected */
		regP |= N_FLAG;
		break;
	default:
		regY = 168; /* invalid device command */
		regP |= N_FLAG;
		break;
	}
	h_fp[h_iocb] = fp;
}

static void Device_H_Close(void)
{
	if (!Device_GetIOCB())
		return;
	if (h_fp[h_iocb] != NULL) {
		Util_fclose(h_fp[h_iocb], h_tmpbuf[h_iocb]);
		h_fp[h_iocb] = NULL;
	}
	regY = 1;
	regP &= (~N_FLAG);
}

static void Device_H_Read(void)
{
	if (!Device_GetIOCB())
		return;
	if (h_fp[h_iocb] != NULL) {
		int ch;
		if (h_lastop[h_iocb] == 'w')
			fseek(h_fp[h_iocb], 0, SEEK_CUR);
		h_lastop[h_iocb] = 'r';
		ch = fgetc(h_fp[h_iocb]);
		if (ch != EOF) {
			if (h_textmode[h_iocb]) {
				switch (ch) {
				case 0x0d:
					h_wascr[h_iocb] = TRUE;
					ch = 0x9b;
					break;
				case 0x0a:
					if (h_wascr[h_iocb]) {
						/* ignore LF next to CR */
						ch = fgetc(h_fp[h_iocb]);
						if (ch != EOF) {
							if (ch == 0x0d) {
								h_wascr[h_iocb] = TRUE;
								ch = 0x9b;
							}
							else
								h_wascr[h_iocb] = FALSE;
						}
						else {
							regY = 136; /* end of file */
							regP |= N_FLAG;
							break;
						}
					}
					else
						ch = 0x9b;
					break;
				default:
					h_wascr[h_iocb] = FALSE;
					break;
				}
			}
			regA = (UBYTE) ch;
			regY = 1;
			regP &= (~N_FLAG);
		}
		else {
			regY = 136; /* end of file */
			regP |= N_FLAG;
		}
	}
	else {
		regY = 136; /* end of file; XXX: this seems to be what Atari DOSes return */
		regP |= N_FLAG;
	}
}

static void Device_H_Write(void)
{
	if (!Device_GetIOCB())
		return;
	if (h_fp[h_iocb] != NULL) {
		int ch;
		if (h_lastop[h_iocb] == 'r')
			fseek(h_fp[h_iocb], 0, SEEK_CUR);
		h_lastop[h_iocb] = 'w';
		ch = regA;
		if (ch == 0x9b && h_textmode[h_iocb])
			ch = '\n';
		fputc(ch, h_fp[h_iocb]);
		regY = 1;
		regP &= (~N_FLAG);
	}
	else {
		regY = 135; /* attempted to write to a read-only device */
		            /* XXX: this seems to be what Atari DOSes return */
		regP |= N_FLAG;
	}
}

static void Device_H_Status(void)
{
	regY = 146; /* function not implemented in handler; XXX: check file existence? */
	regP |= N_FLAG;
}

static void Device_H_Note(void)
{
	if (!Device_GetIOCB())
		return;
	if (h_fp[h_iocb] != NULL) {
		long pos = ftell(h_fp[h_iocb]);
		if (pos >= 0) {
			int iocb = IOCB0 + h_iocb * 16;
			dPutByte(iocb + ICAX5, (UBYTE) pos);
			dPutByte(iocb + ICAX3, (UBYTE) (pos >> 8));
			dPutByte(iocb + ICAX4, (UBYTE) (pos >> 16));
			regY = 1;
			regP &= (~N_FLAG);
		}
		else {
			regY = 144; /* device done error */
			regP |= N_FLAG;
		}
	}
	else {
		regY = 130; /* specified device does not exist; XXX: correct? */
		regP |= N_FLAG;
	}
}

static void Device_H_Point(void)
{
	if (!Device_GetIOCB())
		return;
	if (h_fp[h_iocb] != NULL) {
		int iocb = IOCB0 + h_iocb * 16;
		long pos = (dGetByte(iocb + ICAX4) << 16) +
			(dGetByte(iocb + ICAX3) << 8) + (dGetByte(iocb + ICAX5));
		if (fseek(h_fp[h_iocb], pos, SEEK_SET) == 0) {
			regY = 1;
			regP &= (~N_FLAG);
		}
		else {
			regY = 166; /* invalid POINT request */
			regP |= N_FLAG;
		}
	}
	else {
		regY = 130; /* specified device does not exist; XXX: correct? */
		regP |= N_FLAG;
	}
}

static FILE *binf = NULL;
static int runBinFile;
static int initBinFile;

/* Read a word from file */
static int Device_H_BinReadWord(void)
{
	UBYTE buf[2];
	if (fread(buf, 1, 2, binf) != 2) {
		fclose(binf);
		binf = NULL;
		if (start_binloading) {
			start_binloading = FALSE;
			regY = 180; /* MyDOS: not a binary file */
			regP |= N_FLAG;
			return -1;
		}
		if (runBinFile)
			regPC = dGetWordAligned(0x2e0);
		regY = 1;
		regP &= (~N_FLAG);
		return -1;
	}
	return buf[0] + (buf[1] << 8);
}

static void Device_H_BinLoaderCont(void)
{
	if (binf == NULL)
		return;
	if (start_binloading) {
		dPutByte(0x244, 0);
		dPutByte(0x09, 1);
	}
	else
		regS += 2;				/* pop ESC code */

	dPutByte(0x2e3, 0xd7);
	do {
		int temp;
		UWORD from;
		UWORD to;
		do
			temp = Device_H_BinReadWord();
		while (temp == 0xffff);
		if (temp < 0)
			return;
		from = (UWORD) temp;

		temp = Device_H_BinReadWord();
		if (temp < 0)
			return;
		to = (UWORD) temp;

		if (start_binloading) {
			if (runBinFile)
				dPutWordAligned(0x2e0, from);
			start_binloading = FALSE;
		}

		to++;
		do {
			int byte = fgetc(binf);
			if (byte == EOF) {
				fclose(binf);
				binf = NULL;
				if (runBinFile)
					regPC = dGetWordAligned(0x2e0);
				if (initBinFile && (dGetByte(0x2e3) != 0xd7)) {
					/* run INIT routine which RTSes directly to RUN routine */
					regPC--;
					dPutByte(0x0100 + regS--, regPC >> 8);	/* high */
					dPutByte(0x0100 + regS--, regPC & 0xff);	/* low */
					regPC = dGetWordAligned(0x2e2);
				}
				return;
			}
			PutByte(from, (UBYTE) byte);
			from++;
		} while (from != to);
	} while (!initBinFile || dGetByte(0x2e3) == 0xd7);

	regS--;
	Atari800_AddEsc((UWORD) (0x100 + regS), ESC_BINLOADER_CONT, Device_H_BinLoaderCont);
	regS--;
	dPutByte(0x0100 + regS--, 0x01);	/* high */
	dPutByte(0x0100 + regS, regS + 1);	/* low */
	regS--;
	regPC = dGetWordAligned(0x2e2);
	SetC;

	dPutByte(0x0300, 0x31);		/* for "Studio Dream" */
}

static void Device_H_LoadProceed(int mydos)
{
	if (mydos) {
		switch (dGetByte(ICAX1Z) /* XXX: & 7 ? */) {
		case 4:
			runBinFile = TRUE;
			initBinFile = TRUE;
			break;
		case 5:
			runBinFile = TRUE;
			initBinFile = FALSE;
			break;
		case 6:
			runBinFile = FALSE;
			initBinFile = TRUE;
			break;
		case 7:
		default:
			runBinFile = FALSE;
			initBinFile = FALSE;
			break;
		}
	}
	else {
		if (dGetByte(ICAX2Z) < 128)
			runBinFile = TRUE;
		else
			runBinFile = FALSE;
		initBinFile = TRUE;
	}

	start_binloading = TRUE;
	Device_H_BinLoaderCont();
}

static void Device_H_Load(int mydos)
{
	const char *p;
	UBYTE buf[2];
	h_devnum = Device_GetNumber(FALSE);
	if (h_devnum < 0)
		return;

	/* search for program on h_exe_path */
	for (p = h_exe_path; *p != '\0'; ) {
		int devnum;
		const char *q;
		char *r;
		if (p[0] == 'H' && p[1] >= '1' && p[1] <= '4' && p[2] == ':') {
			devnum = p[1] - '1';
			p += 3;
		}
		else
			devnum = h_devnum;
		for (q = p; *q != '\0' && *q != ';'; q++);
		r = atari_filename + (q - p);
		if (q != p) {
			memcpy(atari_filename, p, q - p);
			if (!IS_DIR_SEP(q[-1]))
				*r++ = '>';
		}
		if (Device_GetAtariPath(devnum, r) == 0)
			return;
		Util_catpath(host_path, atari_h_dir[devnum], atari_path);
		binf = fopen(host_path, "rb");
		if (binf != NULL || *q == '\0')
			break;
		p = q + 1;
	}

	if (binf == NULL) {
		/* open from the specified location */
		if (Device_GetAtariPath(h_devnum, atari_filename) == 0)
			return;
		Util_catpath(host_path, atari_h_dir[h_devnum], atari_path);
		binf = fopen(host_path, "rb");
		if (binf == NULL) {
			regY = 170;
			regP |= N_FLAG;
			return;
		}
	}

	/* check header */
	if (fread(buf, 1, 2, binf) != 2 || buf[0] != 0xff || buf[1] != 0xff) {
		fclose(binf);
		binf = NULL;
		regY = 180;
		regP |= N_FLAG;
		return;
	}

	Device_H_LoadProceed(mydos);
}

static void Device_H_FileLength(void)
{
	if (!Device_GetIOCB())
		return;
	/* if IOCB is closed then assume it is a MyDOS Load File command */
	if (h_fp[h_iocb] == NULL)
		Device_H_Load(TRUE);
	/* if we are running MyDOS then assume it is a MyDOS Load File command */
	else if (dGetByte(0x700) == 'M') {
		/* XXX: if (binf != NULL) fclose(binf); ? */
		binf = h_fp[h_iocb];
		Device_H_LoadProceed(TRUE);
		/* XXX: don't close binf when complete? */
	}
	/* otherwise assume it is a file length command */
	else {
		int iocb = IOCB0 + h_iocb * 16;
		int filesize;
		FILE *fp = h_fp[h_iocb];
		long currentpos = ftell(fp);
		filesize = Util_flen(fp);
		fseek(fp, currentpos, SEEK_SET);
		dPutByte(iocb + ICAX3, (UBYTE) filesize);
		dPutByte(iocb + ICAX4, (UBYTE) (filesize >> 8));
		dPutByte(iocb + ICAX5, (UBYTE) (filesize >> 16));
		regY = 1;
		regP &= (~N_FLAG);
	}
}

static void Device_H_ChangeDirectory(void)
{
	if (Device_GetHostPath(FALSE) == 0)
		return;

	if (!Util_direxists(host_path)) {
		regY = 150;
		regP |= N_FLAG;
		return;
	}

	if (atari_path[0] == '\0')
		h_current_dir[h_devnum][0] = '\0';
	else {
		char *p = Util_stpcpy(h_current_dir[h_devnum], atari_path);
		p[0] = DIR_SEP_CHAR;
		p[1] = '\0';
	}

	regY = 1;
	regP &= (~N_FLAG);
}

static void Device_H_DiskInfo(void)
{
	static UBYTE info[16] = {
		0x20,                                                  /* disk version: Sparta >= 2.0 */
		0x00,                                                  /* sector size: 0x100 */
		0xff, 0xff,                                            /* total sectors: 0xffff */
		0xff, 0xff,                                            /* free sectors: 0xffff */
		'H', 'D', 'I', 'S', 'K', '1' /* + devnum */, ' ', ' ', /* disk name */
		1,                                                     /* seq. number (number of writes) */
		1 /* + devnum */                                       /* random number (disk id) */
	};
	int devnum;

	devnum = Device_GetNumber(FALSE);
	if (devnum < 0)
		return;

	info[11] = (UBYTE) ('1' + devnum);
	info[15] = (UBYTE) (1 + devnum);
	CopyToMem(info, (UWORD) dGetWordAligned(ICBLLZ), 16);

	regY = 1;
	regP &= (~N_FLAG);
}

static void Device_H_ToAbsolutePath(void)
{
	UWORD bufadr;
	const char *p;

	if (Device_GetHostPath(FALSE) == 0)
		return;

	/* XXX: we sometimes check here for directories
	   with a trailing DIR_SEP_CHAR. It seems to work on Win32 and DJGPP. */
	if (!Util_direxists(host_path)) {
		regY = 150;
		regP |= N_FLAG;
		return;
	}

	bufadr = dGetWordAligned(ICBLLZ);
	if (atari_path[0] != '\0') {
		PutByte(bufadr, '>');
		bufadr++;
		for (p = atari_path; *p != '\0'; p++) {
			if (*p == DIR_SEP_CHAR) {
				if (p[1] == '\0')
					break;
				PutByte(bufadr, '>');
			}
			else
				PutByte(bufadr, (UBYTE) *p);
			bufadr++;
		}
	}
	PutByte(bufadr, 0x00);

	regY = 1;
	regP &= (~N_FLAG);
}

static void Device_H_Special(void)
{
	switch (dGetByte(ICCOMZ)) {
#ifdef DO_RENAME
	case 0x20:
		regY = 163;
		regP |= N_FLAG;
		return;
#endif
#ifdef HAVE_UTIL_UNLINK
	case 0x21:
		regY = 163;
		regP |= N_FLAG;
		return;
#endif
#ifdef DO_LOCK
	case 0x23:
		regY = 163;
		regP |= N_FLAG;
		return;
	case 0x24:
		regY = 163;
		regP |= N_FLAG;
		return;
#endif
	case 0x26:
		Device_H_Note();
		return;
	case 0x25:
		Device_H_Point();
		return;
	case 0x27: /* Sparta, MyDOS=Load */
		Device_H_FileLength();
		return;
	case 0x28: /* Sparta */
		Device_H_Load(FALSE);
		return;
#ifdef DO_MKDIR
	case 0x22: /* MyDOS */
	case 0x2a: /* MyDOS, Sparta */
		regY = 163;
		regP |= N_FLAG;
		return;
#endif
#ifdef DO_RMDIR
	case 0x2b: /* Sparta */
		regY = 163;
		regP |= N_FLAG;
		return;
#endif
	case 0x29: /* MyDOS */
	case 0x2c: /* Sparta */
		Device_H_ChangeDirectory();
		return;
	case 0x2f: /* Sparta */
		Device_H_DiskInfo();
		return;
	case 0x30: /* Sparta */
		Device_H_ToAbsolutePath();
		return;
	case 0xfe:
		break;
	default:
		break;
	}

	regY = 168; /* invalid device command */
	regP |= N_FLAG;
}

/* Patches management ---------------------------------------------------- */

/* New handling of H: device.
   Previously we simply replaced C: device in OS with our H:.
   Now we don't change ROM for H: patch, but add H: to HATABS in RAM
   and put the device table and patches in unused address space
   (0xd100-0xd1ff), which is meant for 'new devices' (like hard disk).
   We have to continuously check if our H: is still in HATABS,
   because RESET routine in Atari OS clears HATABS and initializes it
   using a table in ROM (see Device_PatchOS).
   Before we put H: entry in HATABS, we must make sure that HATABS is there.
   For example a program that doesn't use Atari OS can use this memory area
   for its own data, and we shouldn't place 'H' there.
   We also allow an Atari program to change address of H: device table.
   So after we put H: entry in HATABS, we only check if 'H' is still where
   we put it (h_entry_address).
   Device_UpdateHATABSEntry and Device_RemoveHATABSEntry can be used to add
   other devices than H:. */

#define H_DEVICE_BEGIN  0xd140
#define H_TABLE_ADDRESS 0xd140
#define H_PATCH_OPEN    0xd150
#define H_PATCH_CLOS    0xd153
#define H_PATCH_READ    0xd156
#define H_PATCH_WRIT    0xd159
#define H_PATCH_STAT    0xd15c
#define H_PATCH_SPEC    0xd15f
#define H_DEVICE_END    0xd161

void Device_UpdatePatches(void)
{
	/* change memory attributes for the area, where we put
	   the H: handler table and patches */
	SetROM(H_DEVICE_BEGIN, H_DEVICE_END);
	/* set handler table */
	dPutWord(H_TABLE_ADDRESS + DEVICE_TABLE_OPEN, H_PATCH_OPEN - 1);
	dPutWord(H_TABLE_ADDRESS + DEVICE_TABLE_CLOS, H_PATCH_CLOS - 1);
	dPutWord(H_TABLE_ADDRESS + DEVICE_TABLE_READ, H_PATCH_READ - 1);
	dPutWord(H_TABLE_ADDRESS + DEVICE_TABLE_WRIT, H_PATCH_WRIT - 1);
	dPutWord(H_TABLE_ADDRESS + DEVICE_TABLE_STAT, H_PATCH_STAT - 1);
	dPutWord(H_TABLE_ADDRESS + DEVICE_TABLE_SPEC, H_PATCH_SPEC - 1);
	/* set patches */
	Atari800_AddEscRts(H_PATCH_OPEN, ESC_HHOPEN, Device_H_Open);
	Atari800_AddEscRts(H_PATCH_CLOS, ESC_HHCLOS, Device_H_Close);
	Atari800_AddEscRts(H_PATCH_READ, ESC_HHREAD, Device_H_Read);
	Atari800_AddEscRts(H_PATCH_WRIT, ESC_HHWRIT, Device_H_Write);
	Atari800_AddEscRts(H_PATCH_STAT, ESC_HHSTAT, Device_H_Status);
	Atari800_AddEscRts(H_PATCH_SPEC, ESC_HHSPEC, Device_H_Special);
	/* H: in HATABS will be added next frame by Device_Frame */
}
