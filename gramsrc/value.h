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
#ifndef __GCMC_VALUE_H
#define __GCMC_VALUE_H

#include "grammartypes.h"

typedef enum {
	VAL_UNDEF,	/* Empty vector parts */
	VAL_INT,
	VAL_FLOAT,
	VAL_VECTOR,
	VAL_VECTORLIST,
	VAL_STRING,
	VAL_REF,
} val_et;

struct __value_t;
struct __vector_t;
struct __vectorlist_t;

typedef struct __vector_t {
	int			n;
	int			na;
	struct __value_t	**vals;
} vector_t;

typedef struct __vectorlist_t {
	int			n;
	int			na;
	struct __value_t	**vecs;
} vectorlist_t;

typedef struct __value_t {
	val_et	type;
	unit_et	unit;
	union {
		int		i;
		double		d;
		string_t	str;
		vector_t	v;
		vectorlist_t	vl;
		struct __value_t *ref;
	};
#ifdef DEBUG_VALUES
	int	idcnt;
#endif
} value_t;

int value_stackdepth(void);
void value_purge(void);
value_t *value_push(value_t *v);
value_t *value_pop(const node_t *ref);
const value_t *value_tos(const node_t *ref);
const value_t *value_deref(const value_t *v);
value_t *value_dup(const value_t *v);
value_t *value_replace(value_t *dst, value_t *src);
value_t *value_new(int type);
value_t *value_new_int(int i, unit_et unit);
value_t *value_new_flt(double d, unit_et unit);
value_t *value_new_str(const string_t *str);
value_t *value_new_ref(value_t *val);
value_t *value_vector_add(value_t *v, value_t *x);
value_t *value_vector_add_undef(value_t *v);
value_t *value_vector_prepend(value_t *v, value_t *x);
value_t *value_vectorlist_add(value_t *v, value_t *x);
value_t *value_vectorlist_prepend(value_t *v, value_t *x);
void value_delete(value_t *v);
double value_to_dist(const value_t *v);
double value_to_mm(const value_t *v);
double value_to_in(const value_t *v);
double value_to_deg(const value_t *v);
double value_to_rad(const value_t *v);
double value_to_double(const value_t *v);
double value_to_unit(const value_t *v, unit_et unit);
int value_to_int(const value_t *v);
int value_to_boolean(const value_t *v);
const value_t *value_get(int argc, int offset);
value_t *value_to_string(const node_t *ref, const value_t *v);

const char *valuetypename(val_et n);

static inline int isfloat(const value_t *v);
static inline int isint(const value_t *v);
static inline int isscalar(const value_t *v);
static inline int isvector(const value_t *v);
static inline int isvectorlist(const value_t *v);
static inline int isstring(const value_t *v);
static inline int isundef(const value_t *v);
static inline int isref(const value_t *v);
static inline int isangle(const value_t *v);
static inline int isdistance(const value_t *v);
static inline int isnone(const value_t *v);
static inline int isnoneordistance(const value_t *v);
static inline int isfloat(const value_t *v)		{ return v->type == VAL_FLOAT; }
static inline int isint(const value_t *v)		{ return v->type == VAL_INT; }
static inline int isscalar(const value_t *v)		{ return v->type == VAL_INT || v->type == VAL_FLOAT; }
static inline int isvector(const value_t *v)		{ return v->type == VAL_VECTOR; }
static inline int isvectorlist(const value_t *v)	{ return v->type == VAL_VECTORLIST; }
static inline int isstring(const value_t *v)		{ return v->type == VAL_STRING; }
static inline int isundef(const value_t *v)		{ return v->type == VAL_UNDEF; }
static inline int isref(const value_t *v)		{ return v->type == VAL_REF; }
static inline int isangle(const value_t *v)		{ return v->unit == UNIT_DEG || v->unit == UNIT_RAD; }
static inline int isdistance(const value_t *v)		{ return v->unit == UNIT_MM || v->unit == UNIT_IN; }
static inline int isnone(const value_t *v)		{ return v->unit == UNIT_NONE; }
static inline int isnoneordistance(const value_t *v)	{ return isnone(v) || isdistance(v); }

#endif
