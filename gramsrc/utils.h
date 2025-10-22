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
#ifndef __GCMC_UTILS_H
#define __GCMC_UTILS_H

#include <stdlib.h>

#include "bison_utils.h"


#define NELEM(x)	(sizeof((x))/sizeof((x)[0]))

void testalloc(void **p, int n, int *na, size_t elemsize);
void testalloc2(void **p, int n, int *na, size_t elemsize, size_t nelem);
/*
const char *planename(plane_et p);
*/
const char *unitshortname(unit_et u);
const char *unitprintname(unit_et u);


wchar_t *mysprintf(wchar_t **s, size_t *ns, const wchar_t *fmt, ...);

const char *ucs32_to_utf8(const wchar_t *ucs, int len);

#ifndef HAVE_WCSCASECMP
int wcscasecmp(const wchar_t *s1, const wchar_t *s2);
#endif

void utils_cleanup(void);

#endif
