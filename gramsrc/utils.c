/*
 * G-code meta compiler
 *
 * Copyright (C) 2013  B. Stultiens
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "config.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <wctype.h>

#include "utils.h"

#pragma warning(disable:4996)

void testalloc(void **p, int n, int *na, size_t elemsize)
{
	if(!*p) {
		assert(n == 0);
		*p = calloc(8, elemsize);
		assert(*p != NULL);
		*na = 8;
	}
	if(n >= *na) {
		void *pp = calloc(2 * *na, elemsize);
		assert(pp != NULL);
		memcpy(pp, *p, n*elemsize);
		free(*p);
		*p = pp;
		*na *= 2;
	}
}

void testalloc2(void **p, int n, int *na, size_t elemsize, size_t nelem)
{
	if(!*p) {
		assert(n == 0);
		if(nelem < 8)
			nelem = 8;
		*p = calloc(nelem, elemsize);
		assert(*p != NULL);
		*na = nelem;
	}
	if(n + nelem >= *na) {
		while(n + nelem >= *na)
			*na *= 2;
		void *pp = calloc(*na, elemsize);
		assert(pp != NULL);
		memcpy(pp, *p, n*elemsize);
		free(*p);
		*p = pp;
	}
}
/*
const char *planename(plane_et p)
{
	switch(p) {
	case PLANE_XY:	return "PLANE_XY";
	case PLANE_XZ:	return "PLANE_XZ";
	case PLANE_YZ:	return "PLANE_YZ";
	}
	return "<unknown plane>";
}
*/
const char *unitshortname(unit_et u)
{
	switch(u) {
	case UNIT_NONE:	return "";
	case UNIT_MM:	return "mm";
	case UNIT_IN:	return "in";
	case UNIT_DEG:	return "deg";
	case UNIT_RAD:	return "rad";
	}
	return "<unknown units>";
}

const char *unitprintname(unit_et u)
{
	switch(u) {
	case UNIT_NONE:	return "none";
	case UNIT_MM:	return "mm";
	case UNIT_IN:	return "in";
	case UNIT_DEG:	return "degrees";
	case UNIT_RAD:	return "radians";
	}
	return "<unknown units>";
}

wchar_t *mysprintf(wchar_t **s, size_t *ns, const wchar_t *fmt, ...)
{
	int n;
	int size = 128;
	wchar_t *p;
	va_list va;

	p = malloc(size * sizeof(*p));
	assert(p != NULL);

	/* Adapted from snprintf(3) */
	while (1) {
		/* Try to print in the allocated space */
		va_start(va, fmt);
		n = vswprintf(p, size, fmt, va);
		va_end(va);

		/* Break on error or when it fits */
		if(n < 0 || n < size)
			break;

		/* Else try again with more space */
		size = n + 1;       /* Precisely what is needed */

		p = realloc(p, size * sizeof(*p));
		assert(p != NULL);
	}
	assert(n >= 0);
	if(n > 0) {
		*s = realloc(*s, (*ns + n + 1) * sizeof(**s));
		assert(*s != NULL);
		*ns += n;
		wcscat(*s, p);
		free(p);
	}
	return *s;
}

/*
 * Convert UCS string into UTF-8
 * Returns a local buffer that will be reused.
 */
static int lsn;
static int lsna = 0;
static char *ls = NULL;

const char *ucs32_to_utf8(const wchar_t *ucs, int len)
{
	int i;

	assert(ucs != NULL);

	if(len < 0)
		len = wcslen(ucs);

	lsn = 0;
	for(i = 0; i < len; i++) {
		if(!ucs[i]) {
			rterror(NULL, "Invalid NUL character in UCS to UTF-8 conversion at position %d", i);
			return NULL;
		}
		if(ucs[i] & 0x80000000) {
			rterror(NULL, "Invalid UCS character in UCS to UTF-8 conversion at position %d", i);
			return NULL;
		}
		if(cl_pedantic && ucs[i] > 0x10ffff)
			rtwarning(NULL, "UCS character > 0x10ffff in UCS to UTF-8 conversion at position %d", i);
		if(ucs[i] <= 0x7f) {
			/* One-byte sequence */
			testalloc((void **)&ls, lsn, &lsna, sizeof(*ls));
			ls[lsn++] = ucs[i] & 0x7f;
		} else if(ucs[i] <= 0x7ff) {
			/* Two-byte sequence */
			testalloc2((void **)&ls, lsn, &lsna, sizeof(*ls), 2);
			ls[lsn++] = 0xc0 | ((ucs[i] >> 6) & 0x1f);
			ls[lsn++] = 0x80 | ((ucs[i] >> 0) & 0x3f);
		} else if(ucs[i] <= 0xffff) {
			/* Three-byte sequence */
			testalloc2((void **)&ls, lsn, &lsna, sizeof(*ls), 3);
			ls[lsn++] = 0xe0 | ((ucs[i] >> 12) & 0x0f);
			ls[lsn++] = 0x80 | ((ucs[i] >> 6) & 0x3f);
			ls[lsn++] = 0x80 | ((ucs[i] >> 0) & 0x3f);
		} else if(ucs[i] <= 0x1fffff) {
			/* Four-byte sequence */
			testalloc2((void **)&ls, lsn, &lsna, sizeof(*ls), 4);
			ls[lsn++] = 0xf0 | ((ucs[i] >> 18) & 0x07);
			ls[lsn++] = 0x80 | ((ucs[i] >> 12) & 0x3f);
			ls[lsn++] = 0x80 | ((ucs[i] >> 6) & 0x3f);
			ls[lsn++] = 0x80 | ((ucs[i] >> 0) & 0x3f);
		} else if(ucs[i] <= 0x3ffffff) {
			/* Five-byte sequence */
			testalloc2((void **)&ls, lsn, &lsna, sizeof(*ls), 5);
			ls[lsn++] = 0xf8 | ((ucs[i] >> 24) & 0x03);
			ls[lsn++] = 0x80 | ((ucs[i] >> 18) & 0x3f);
			ls[lsn++] = 0x80 | ((ucs[i] >> 12) & 0x3f);
			ls[lsn++] = 0x80 | ((ucs[i] >> 6) & 0x3f);
			ls[lsn++] = 0x80 | ((ucs[i] >> 0) & 0x3f);
		} else if(ucs[i] <= 0x7fffffff) {
			/* Six-byte sequence */
			testalloc2((void **)&ls, lsn, &lsna, sizeof(*ls), 6);
			ls[lsn++] = 0xfc | ((ucs[i] >> 30) & 0x01);
			ls[lsn++] = 0x80 | ((ucs[i] >> 24) & 0x3f);
			ls[lsn++] = 0x80 | ((ucs[i] >> 18) & 0x3f);
			ls[lsn++] = 0x80 | ((ucs[i] >> 12) & 0x3f);
			ls[lsn++] = 0x80 | ((ucs[i] >> 6) & 0x3f);
			ls[lsn++] = 0x80 | ((ucs[i] >> 0) & 0x3f);
		}
	}
	testalloc((void **)&ls, lsn, &lsna, sizeof(*ls));
	ls[lsn] = 0;	/* Zero terminate */
	return ls;
}

#ifndef HAVE_WCSCASECMP
#if !defined(HAVE_TOWLOWER) && !defined(HAVE_TOWUPPER)
#error "Neither wcscasecmp(), towupper() or towlower() are available, cannot do compares"
#endif
/*
 * Local copy of wcscasecmp() because mingw may not provide it. The conversion
 * depends LC_CTYPE and may be slightly different depending locale. However,
 * wcscasecmp() does the same thing and returns the same values as this
 * implementation.
 * The strings s1 and s2 must be zero terminated for this function to work
 * properly. This should be default behavior.
 */
int wcscasecmp(const wchar_t *s1, const wchar_t *s2)
{
	assert(s1 != NULL);
	assert(s2 != NULL);
	while(*s1) {
		wchar_t c1, c2;
		/* If *s2 is zero, then c1 and c2 will be unequal because s1 continues */
#ifdef HAVE_TOWLOWER
		c1 = towlower(*s1);
		c2 = towlower(*s2);
#else
		c1 = towupper(*s1);
		c2 = towupper(*s2);
#endif
		if(c1 != c2)
			return (int)c1 - (int)c2;
		s1++;
		s2++;
	}
	if(*s2)
#ifdef HAVE_TOWLOWER
		return -(int)towlower(*s2);
#else
		return -(int)towupper(*s2);
#endif
	return 0;
}
#endif

void utils_cleanup(void)
{
	if(ls)
		free(ls);
}
