#ifndef _UTIL_H_
#define _UTIL_H_

#include "config.h"
#include <stdio.h> /* For FILE */
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

/* String functions ------------------------------------------------------ */

/* Returns TRUE if the characters are equal or represent the same letter
   in different case. */
int Util_chrieq(char c1, char c2);

/* Same as stpcpy() in some C libraries: copies src to dest
   and returns a pointer to the trailing NUL in dest. */
char *Util_stpcpy(char *dest, const char *src);

/* Memory management ----------------------------------------------------- */

/* malloc() with out-of-memory checking. Never returns NULL. */
void *Util_malloc(size_t size);

/* Filenames ------------------------------------------------------------- */

#ifdef BACK_SLASH
#define DIR_SEP_CHAR '\\'
#define DIR_SEP_STR  "\\"
#else
#define DIR_SEP_CHAR '/'
#define DIR_SEP_STR  "/"
#endif

/* Splits a filename into directory part and file part. */
/* dir_part or file_part may be NULL. */
void Util_splitpath(const char *path, char *dir_part, char *file_part);

/* Concatenates file paths.
   Places directory separator char between paths, unless path1 is empty
   or ends with the separator char, or path2 starts with the separator char. */
void Util_catpath(char *result, const char *path1, const char *path2);


/* File I/O -------------------------------------------------------------- */

/* Returns TRUE if the specified directory exists. */
int Util_direxists(const char *filename);

/* Rewinds the stream to its beginning. */
#define Util_rewind(fp) fseek(fp, 0, SEEK_SET)

/* Returns the length of an open stream.
   May change the current position. */
int Util_flen(FILE *fp);

/* Deletes a file, returns 0 on success, -1 on failure. */
#ifdef WIN32
#ifdef UNICODE
int Util_unlink(const char *filename);
#else
#define Util_unlink(filename)  ((DeleteFile(filename) != 0) ? 0 : -1)
#endif /* UNICODE */
#define HAVE_UTIL_UNLINK
#elif defined(HAVE_UNLINK)
#define Util_unlink  unlink
#define HAVE_UTIL_UNLINK
#endif /* defined(HAVE_UNLINK) */

/* Creates a file that does not exist and fills in filename with its name. */
FILE *Util_uniqopen(char *filename, const char *mode);

/* Support for temporary files.

   Util_tmpbufdef() defines storage for names of temporary files, if necessary.
     Example use:
     Util_tmpbufdef(static, mytmpbuf[4]) // four buffers with "static" storage
   Util_fopen() opens a *non-temporary* file and marks tmpbuf
   as *not containing* name of a temporary file.
   Util_tmpopen() creates a temporary file with "wb+" mode.
   Util_fclose() closes a file. If it was temporary, it gets deleted.

   There are 3 implementations of the above:
   - one that uses tmpfile() (preferred)
   - one that stores names of temporary files and deletes them when they
     are closed
   - one that creates unique files but doesn't delete them
     because Util_unlink is not available
*/
#if defined(HAVE_UTIL_UNLINK)
#define Util_tmpbufdef(modifier, def)       modifier char def [FILENAME_MAX];
#define Util_fopen(filename, mode, tmpbuf)  (tmpbuf[0] = '\0', fopen(filename, mode))
#define Util_tmpopen(tmpbuf)                Util_uniqopen(tmpbuf, "wb+")
#define Util_fclose(fp, tmpbuf)             (fclose(fp), tmpbuf[0] != '\0' && Util_unlink(tmpbuf))
#else
/* if we can't delete the created file, leave it to the user */
#define Util_tmpbufdef(modifier, def)
#define Util_fopen(filename, mode, tmpbuf)  fopen(filename, mode)
#define Util_tmpopen(tmpbuf)                Util_uniqopen(NULL, "wb+")
#define Util_fclose(fp, tmpbuf)             fclose(fp)
#endif

#endif /* _UTIL_H_ */
