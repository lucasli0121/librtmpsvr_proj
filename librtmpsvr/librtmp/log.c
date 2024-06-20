/*
 *  Copyright (C) 2008-2009 Andrej Stepanchuk
 *  Copyright (C) 2009-2010 Howard Chu
 *
 *  This file is part of librtmp.
 *
 *  librtmp is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 2.1,
 *  or (at your option) any later version.
 *
 *  librtmp is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with librtmp see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 *  http://www.gnu.org/copyleft/lgpl.html
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#include "rtmp_sys.h"
#include "log.h"

#define MAX_PRINT_LEN	2048

static int neednl = 0;
static const char *levels[] = {
  "CRIT", "ERROR", "WARNING", "INFO",
  "DEBUG", "DEBUG2"
};

static void rtmp_log_default(RTMP_LogContext *ctx, int level, const char *format, va_list vl)
{
	if(ctx == NULL)
		return;
	char str[MAX_PRINT_LEN]="";
	vsnprintf(str, MAX_PRINT_LEN-1, format, vl);
	if(ctx->file == NULL)
		ctx->file = stderr;
	time_t t = time(NULL);
	struct tm *ptm = localtime(&t);
	fprintf(ctx->file, "%s %s: %s\n", asctime(ptm), levels[level], str);
	fflush(ctx->file);
}

RTMP_LogContext* RTMP_LogInit(void)
{
	RTMP_LogContext *ctx = (RTMP_LogContext*)malloc(sizeof(RTMP_LogContext) + 16);
	if(ctx == NULL)
		return NULL;
	memset(ctx, 0, sizeof(RTMP_LogContext));
	ctx->file = stderr;
	ctx->level = RTMP_LOGERROR;
	ctx->cb = rtmp_log_default;
	ctx->mutex = createMutex();
	return ctx;
}

void RTMP_LogUninit(RTMP_LogContext* ctx)
{
	if(ctx == NULL)
		return;
	if (ctx->mutex) {
		destroyMutex(ctx->mutex);
		ctx->mutex = NULL;
	}
	free(ctx);
}

void RTMP_LogSetOutput(RTMP_LogContext* r, FILE *file)
{
	if(r == NULL)
		return;
	r->mutex->lock(&r->mutex->mutex);
	r->file = file;
	r->mutex->unlock(&r->mutex->mutex);
}

void RTMP_LogSetLevel(RTMP_LogContext* r, RTMP_LogLevel level)
{
	if(r == NULL)
		return;
	r->level = level;
}

void RTMP_LogSetCallback(RTMP_LogContext* r, RTMP_LogCallback *cbp)
{
	if(r == NULL)
		return;
	r->cb = cbp;
}

RTMP_LogLevel RTMP_LogGetLevel(RTMP_LogContext* r)
{
	if(r == NULL)
		return RTMP_LOGERROR;
	return r->level;
}

void RTMP_Log(RTMP_LogContext* r, int level, const char *format, ...)
{
	if(r == NULL)
		return;
	if(level > r->level)
		return;
	if(r->mutex) {
		r->mutex->lock(&r->mutex->mutex);
	}		
	va_list args;
	va_start(args, format);
	if(r->cb == NULL)
		rtmp_log_default(r, level, format, args);
	else
		r->cb(r, level, format, args);
	va_end(args);
	if(r->mutex) {
		r->mutex->unlock(&r->mutex->mutex);
	}
}

static const char hexdig[] = "0123456789abcdef";

void RTMP_LogHex(RTMP_LogContext* r, int level, const uint8_t *data, unsigned long len)
{
	unsigned long i;
	char line[50], *ptr;

	if(r == NULL)
		return;
	if ( !data || level > r->level )
		return;
	ptr = line;

	for(i=0; i<len; i++) {
		*ptr++ = hexdig[0x0f & (data[i] >> 4)];
		*ptr++ = hexdig[0x0f & data[i]];
		if ((i & 0x0f) == 0x0f) {
			*ptr = '\0';
			ptr = line;
			RTMP_Log(r, level, "%s", line);
		} else {
			*ptr++ = ' ';
		}
	}
	if (i & 0x0f) {
		*ptr = '\0';
		RTMP_Log(r,level, "%s", line);
	}
}

void RTMP_LogHexString(RTMP_LogContext* r, int level, const uint8_t *data, unsigned long len)
{
#define BP_OFFSET 9
#define BP_GRAPH 60
#define BP_LEN	80
	char	line[BP_LEN];
	unsigned long i;

	if(r == NULL)
		return;
	if ( !data || level > r->level )
		return;
	/* in case len is zero */
	line[0] = '\0';

	for ( i = 0 ; i < len ; i++ ) {
		int n = i % 16;
		unsigned off;

		if( !n ) {
			if( i ) RTMP_Log(r, level, "%s", line );
			memset( line, ' ', sizeof(line)-2 );
			line[sizeof(line)-2] = '\0';

			off = i % 0x0ffffU;

			line[2] = hexdig[0x0f & (off >> 12)];
			line[3] = hexdig[0x0f & (off >>  8)];
			line[4] = hexdig[0x0f & (off >>  4)];
			line[5] = hexdig[0x0f & off];
			line[6] = ':';
		}

		off = BP_OFFSET + n*3 + ((n >= 8)?1:0);
		line[off] = hexdig[0x0f & ( data[i] >> 4 )];
		line[off+1] = hexdig[0x0f & data[i]];

		off = BP_GRAPH + n + ((n >= 8)?1:0);

		if ( isprint( data[i] )) {
			line[BP_GRAPH + n] = data[i];
		} else {
			line[BP_GRAPH + n] = '.';
		}
	}

	RTMP_Log(r, level, "%s", line );
}

/* These should only be used by apps, never by the library itself */
void RTMP_LogPrintf(const char *format, ...)
{
	char str[MAX_PRINT_LEN]="";
	int len;
	va_list args;
	va_start(args, format);
	len = vsnprintf(str, MAX_PRINT_LEN-1, format, args);
	va_end(args);

    if (len > MAX_PRINT_LEN-1)
          len = MAX_PRINT_LEN-1;
	fprintf(stderr, "%s", str);
    if (str[len-1] == '\n')
		fflush(stderr);
}