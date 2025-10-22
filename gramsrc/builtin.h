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
#ifndef __GCMC_BUILTIN_H
#define __GCMC_BUILTIN_H

#include "value.h"

typedef value_t *(*func_t)(const node_t *ref, int argc);

typedef struct _builtins_t {
	const wchar_t	*id;
	func_t		func;
} builtins_t;

extern const builtins_t builtins_gcmc[];
extern const int nbuiltins_gcmc;

value_t *bi_fixpos_restore(const node_t *ref, int argc);
value_t *bi_fixpos_store(const node_t *ref, int argc);
value_t *bi_position(const node_t *ref, int argc);
value_t *bi_relocate(const node_t *ref, int argc);

extern int currentlayer;
extern int nlayers;
const wchar_t *layername(int l);

#endif
