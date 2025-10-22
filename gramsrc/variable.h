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
#ifndef __GCMC_VARIABLE_H
#define __GCMC_VARIABLE_H

#include "value.h"

typedef struct __variable_t {
	const wchar_t		*id;
	int			refcount;
	int			isconst;
	struct __variable_t	*ref;
	value_t			*v;
} variable_t;

const variable_t *variable_lookup(const wchar_t *id, int onlyglobal, int scopeoffset);
const variable_t *variable_lookup_deref(const wchar_t *id, int onlyglobal);
const variable_t *variable_set(const wchar_t *id, value_t *v);
const variable_t *variable_set_const(const node_t *n, const wchar_t *id, value_t *v);
const variable_t *variable_set_local(const node_t *n, const wchar_t *id, value_t *v);
const variable_t *variable_ref_local(const wchar_t *dstid, const wchar_t *srcid);
void variable_scope_push(void);
void variable_scope_pop(void);
void variable_purge(void);

#endif
