/*
 * util.c - utility functions
 *
 * Copyright (c) 2005 Piotr Fusik
 * Copyright (c) 2005 Atari800 development team (see DOC/CREDITS)
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
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef WIN32
#include <windows.h>
#endif

#include "atari.h"
#include "util.h"

int Util_chrieq(char c1, char c2)
{
	switch (c1 ^ c2) {
	case 0x00:
		return TRUE;
	case 0x20:
		return (c1 >= 'A' && c1 <= 'Z') || (c1 >= 'a' && c1 <= 'z');
	default:
		break;
	}
	return FALSE;
}

char *Util_stpcpy(char *dest, const char *src)
{
	size_t len = strlen(src);
	memcpy(dest, src, len + 1);
	return dest + len;
}

void *Util_malloc(size_t size)
{
	void *ptr = malloc(size);
	if (ptr == NULL) {
		Atari800_Exit();
		exit(1);
	}
	memset(ptr, 0, size);
	return ptr;
}

void Util_splitpath(const char *path, char *dir_part, char *file_part)
{
	const char *p;
	/* find the last DIR_SEP_CHAR except the last character */
	for (p = path + strlen(path) - 2; p >= path; p--) {
		if (*p == DIR_SEP_CHAR
#ifdef BACK_SLASH
/* on DOSish systems slash can be also used as a directory separator */
		 || *p == '/'
#endif
		   ) {
			if (dir_part != NULL) {
				int len = p - path;
				if (p == path || (p == path + 2 && path[1] == ':'))
					/* root dir: include DIR_SEP_CHAR in dir_part */
					len++;
				memcpy(dir_part, path, len);
				dir_part[len] = '\0';
			}
			if (file_part != NULL)
				strcpy(file_part, p + 1);
			return;
		}
	}
	/* no DIR_SEP_CHAR: current dir */
	if (dir_part != NULL)
		dir_part[0] = '\0';
	if (file_part != NULL)
		strcpy(file_part, path);
}

void Util_catpath(char *result, const char *path1, const char *path2)
{
#ifdef HAVE_SNPRINTF
	snprintf(result, FILENAME_MAX,
#else
	sprintf(result,
#endif
		path1[0] == '\0' || path2[0] == DIR_SEP_CHAR || path1[strlen(path1) - 1] == DIR_SEP_CHAR
#ifdef BACK_SLASH
		 || path2[0] == '/' || path1[strlen(path1) - 1] == '/'
#endif
			? "%s%s" : "%s" DIR_SEP_STR "%s", path1, path2);
}

static int Util_fileexists(const char *filename)
{
	FILE *fp = fopen(filename, "rb");
	if (fp == NULL)
		return FALSE;
	fclose(fp);
	return TRUE;
}

#ifdef WIN32
int Util_direxists(const char *filename)
{
	DWORD attr;
#ifdef UNICODE
	WCHAR wfilename[FILENAME_MAX];
	if (MultiByteToWideChar(CP_ACP, 0, filename, -1, wfilename, FILENAME_MAX) <= 0)
		return FALSE;
	attr = GetFileAttributes(wfilename);
#else
	attr = GetFileAttributes(filename);
#endif /* UNICODE */
	if (attr == 0xffffffff)
		return FALSE;
	return (attr & FILE_ATTRIBUTE_DIRECTORY) ? TRUE : FALSE;
}
#elif defined(HAVE_STAT)
int Util_direxists(const char *filename)
{
	struct stat filestatus;
	return stat(filename, &filestatus) == 0 && (filestatus.st_mode & S_IFDIR);
}
#else
int Util_direxists(const char *filename)
{
	return TRUE;
}
#endif /* defined(HAVE_STAT) */


int Util_flen(FILE *fp)
{
	fseek(fp, 0, SEEK_END);
	return (int) ftell(fp);
}

/* Creates a file that does not exist and fills in filename with its name.
   filename must point to FILENAME_MAX characters buffer which doesn't need
   to be initialized. */
FILE *Util_uniqopen(char *filename, const char *mode)
{
	/* We cannot simply call tmpfile(), because we don't want the file
	   to be deleted when we close it, and we need the filename. */

#if defined(HAVE_MKSTEMP) && defined(HAVE_FDOPEN)
	/* this is the only implementation without a race condition */
	strcpy(filename, "a8XXXXXX");
	/* mkstemp() modifies the 'X'es and returns an open descriptor */
	return fdopen(mkstemp(filename), mode);
#elif defined(HAVE_TMPNAM)
	/* tmpnam() is better than mktemp(), because it creates filenames
	   in system's temporary directory. It is also more portable. */
	return fopen(tmpnam(filename), mode);
#elif defined(HAVE_MKTEMP)
	strcpy(filename, "a8XXXXXX");
	/* mktemp() modifies the 'X'es and returns filename */
	return fopen(mktemp(filename), mode);
#else
	/* Roll-your-own */
	int no;
	for (no = 0; no < 1000000; no++) {
		sprintf(filename, "a8%06d", no);
		if (!Util_fileexists(filename))
			return fopen(filename, mode);
	}
	return NULL;
#endif
}

#if defined(WIN32) && defined(UNICODE)
int Util_unlink(const char *filename)
{
	WCHAR wfilename[FILENAME_MAX];
	if (MultiByteToWideChar(CP_ACP, 0, filename, -1, wfilename, FILENAME_MAX) <= 0)
		return -1;
	return (DeleteFile(wfilename) != 0) ? 0 : -1;
}
#endif /* defined(WIN32) && defined(UNICODE) */
