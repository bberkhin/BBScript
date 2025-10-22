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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "utils.h"
#include "value.h"

/* Calculation stack */
static value_t **valuestack;
static int nvaluestack;
static int navaluestack;

/* Free-list values to reduce malloc/free overhead */
static value_t **flscalar;
static int nflscalar;
static int naflscalar;
static value_t **flvector;
static int nflvector;
static int naflvector;
static value_t **flvectorlist;
static int nflvectorlist;
static int naflvectorlist;

const char *valuetypename(val_et n)
{
	switch(n) {
	case VAL_UNDEF:		return "VAL_UNDEF";
	case VAL_INT:		return "VAL_INT";
	case VAL_FLOAT:		return "VAL_FLOAT";
	case VAL_VECTOR:	return "VAL_VECTOR";
	case VAL_VECTORLIST:	return "VAL_VECTORLIST";
	case VAL_STRING:	return "VAL_STRING";
	case VAL_REF:		return "VAL_REF";
	}
	return "<unknown value type>";
}

int value_stackdepth(void)
{
	return nvaluestack;
}

value_t *value_push(value_t *v)
{
//printf("value_push  : %p\n", v);
	testalloc((void **)&valuestack, nvaluestack, &navaluestack, sizeof(*valuestack));
/*
	if(!valuestack) {
		valuestack = calloc(16, sizeof(*valuestack));
		assert(valuestack != NULL);
		nvaluestack = 0;
		navaluestack = 16;
	} else if(nvaluestack >= navaluestack) {
		valuestack = realloc(valuestack, navaluestack * 2 * sizeof(*valuestack));
		assert(valuestack != NULL);
		navaluestack *= 2;
	}
*/
	valuestack[nvaluestack] = v;
	nvaluestack++;
//printf("push(%d)\n", nvaluestack);
	return v;
}

void value_purge(void)
{
	int i;
	if(nvaluestack)
		rtwarning(NULL, "Valuestack not empty");
	for(i = 0; i < nvaluestack; i++)
		value_delete(valuestack[i]);
	if(valuestack)
		free(valuestack);

	for(i = 0; i < nflscalar; i++)
		free(flscalar[i]);
	for(i = 0; i < nflvector; i++) {
		free(flvector[i]->v.vals);
		free(flvector[i]);
	}
	for(i = 0; i < nflvectorlist; i++) {
		free(flvectorlist[i]->vl.vecs);
		free(flvectorlist[i]);
	}
	free(flscalar);
	free(flvector);
	free(flvectorlist);
}

value_t *value_dup(const value_t *v)
{
	int i;
	value_t *nv = value_new(v->type);
	nv->unit = v->unit;
	switch(nv->type) {
	case VAL_UNDEF:
		break;
	case VAL_INT:
		nv->i = v->i;
		break;
	case VAL_FLOAT:
		nv->d = v->d;
		break;
	case VAL_VECTOR:
		for(i = 0; i < v->v.n; i++)
			value_vector_add(nv, value_dup(v->v.vals[i]));
		break;
	case VAL_VECTORLIST:
		for(i = 0; i < v->vl.n; i++)
			value_vectorlist_add(nv, value_dup(v->vl.vecs[i]));
		break;
	case VAL_STRING:
		nv->str.n = v->str.n;
		nv->str.na = v->str.n+1;
		nv->str.chs = malloc(nv->str.na * sizeof(nv->str.chs[0]));
		assert(nv->str.chs != NULL);
		memcpy(nv->str.chs, v->str.chs, nv->str.na * sizeof(nv->str.chs[0]));
		break;
	default:
		rtinternal(NULL, "Invalid value type %s", valuetypename(nv->type));
		break;
	}
	return nv;
}

value_t *value_replace(value_t *dst, value_t *src)
{
	int i;
	value_t *v = dst;
	assert(dst != NULL);
	assert(isref(dst));
	assert(src != NULL);
	while(isref(dst))
		dst = dst->ref;
	assert(v != dst);
	value_delete(v);
	/* Clean out destination */
	switch(dst->type) {
	case VAL_STRING:
		free(dst->str.chs);
		break;
	case VAL_VECTOR:
		for(i = 0; i < dst->v.n; i++)
			value_delete(dst->v.vals[i]);
		free(dst->v.vals);
		break;
	case VAL_VECTORLIST:
		for(i = 0; i < dst->vl.n; i++)
			value_delete(dst->vl.vecs[i]);
		free(dst->vl.vecs);
		break;
	default:
		/* will we overwritten */
		break;
	}
	*dst = *src;			/* Copy from source value */
	src->type = VAL_UNDEF;		/* Clean out source, but allow value_delete() tracking */
	return dst;
}

value_t *value_pop(const node_t *ref)
{
	if(nvaluestack <= 0)
		rtinternal(ref, "Value stack underflow");

	nvaluestack--;
	return valuestack[nvaluestack];
}

const value_t *value_tos(const node_t *ref)
{
	if(nvaluestack <= 0)
		rtinternal(ref, "Value stack underflow");

	return valuestack[nvaluestack - 1];
}

const value_t *value_deref(const value_t *v)
{
	if(!v)
		return NULL;
	if(isref(v))
		assert(v != v->ref);
	while(isref(v))
		v = v->ref;
	return v;
}

#ifndef DEBUG_VALUES
value_t *value_new(int type)
#else
value_t *__value_new(int type)
#endif
{
#ifdef DEBUG_VALUES
	static int idcnt;
#endif
	value_t *v;
	switch(type) {
	case VAL_UNDEF:
	case VAL_INT:
	case VAL_FLOAT:
	case VAL_STRING:
	case VAL_REF:
		if(nflscalar) {
			nflscalar--;
			v = flscalar[nflscalar];
			v->type = type;
#ifdef DEBUG_VALUES
			v->idcnt = idcnt++;
#endif
			return v;
		}
		break;
	case VAL_VECTOR:
		if(nflvector) {
			nflvector--;
#ifdef DEBUG_VALUES
			flvector[nflvector]->idcnt = idcnt++;
#endif
			return flvector[nflvector];
		}
		break;
	case VAL_VECTORLIST:
		if(nflvectorlist) {
			nflvectorlist--;
#ifdef DEBUG_VALUES
			flvectorlist[nflvectorlist]->idcnt = idcnt++;
#endif
			return flvectorlist[nflvectorlist];
		}
		break;
	default:
		rtinternal(NULL, "value_new(): Undefined type '%s' (%d) requested", valuetypename(type), type);
		break;
	}
	v = calloc(1, sizeof(*v));
	assert(v != NULL);
	v->type = type;
#ifdef DEBUG_VALUES
	v->idcnt = idcnt++;
#endif
	return v;
}

#ifdef DEBUG_VALUES
value_t *value_new(int type)
{
	value_t *v = __value_new(type);
	printf("value_new(%s) --> %d  %p\n", valuetypename(v->type), v->idcnt, v);
	return v;
}
#endif

value_t *value_new_int(int i, unit_et unit)
{
	value_t *v = value_new(VAL_INT);
	v->i = i;
	v->unit = unit;
	return v;
}

value_t *value_new_flt(double d, unit_et unit)
{
	value_t *v = value_new(VAL_FLOAT);
	v->d = d;
	v->unit = unit;
	return v;
}

value_t *value_new_str(const string_t *str)
{
	value_t *v = value_new(VAL_STRING);
	v->str.chs = _wcsdup(str->chs);
	assert(v->str.chs != NULL);
	v->str.n = str->n;
	assert(v->str.n == wcslen(v->str.chs));
	v->str.na = v->str.n + 1;
	return v;
}

value_t *value_new_ref(value_t *val)
{
	value_t *v = value_new(VAL_REF);
	v->ref = val;
	return v;
}

value_t *value_vector_add(value_t *v, value_t *x)
{
	assert(v != NULL);
	assert(x != NULL);
	assert(v->type == VAL_VECTOR);
	assert(x->type == VAL_INT || x->type == VAL_FLOAT || x->type == VAL_UNDEF);
	testalloc((void **)&v->v.vals, v->v.n, &v->v.na, sizeof(v->v.vals[0]));
	v->v.vals[v->v.n] = x;
	v->v.n++;
	return v;
}

value_t *value_vector_add_undef(value_t *v)
{
	return value_vector_add(v, value_new(VAL_UNDEF));
}

value_t *value_vector_prepend(value_t *v, value_t *x)
{
	assert(v != NULL);
	assert(x != NULL);
	assert(v->type == VAL_VECTOR);
	assert(x->type == VAL_INT || x->type == VAL_FLOAT || x->type == VAL_UNDEF);
	testalloc((void **)&v->v.vals, v->v.n, &v->v.na, sizeof(v->v.vals[0]));
	if(v->v.n > 0)
		memmove(&v->v.vals[1], &v->v.vals[0], v->v.n*sizeof(v->v.vals[0]));
	v->v.vals[0] = x;
	v->v.n++;
	return x;
}

value_t *value_vectorlist_add(value_t *v, value_t *x)
{
	assert(v != NULL);
	assert(x != NULL);
	assert(v->type == VAL_VECTORLIST);
	assert(x->type == VAL_VECTOR);
	testalloc((void **)&v->vl.vecs, v->vl.n, &v->vl.na, sizeof(v->vl.vecs[0]));
	v->vl.vecs[v->vl.n] = x;
	v->vl.n++;
	return v;
}

value_t *value_vectorlist_prepend(value_t *v, value_t *x)
{
	assert(v != NULL);
	assert(x != NULL);
	assert(v->type == VAL_VECTORLIST);
	assert(x->type == VAL_VECTOR);
	testalloc((void **)&v->vl.vecs, v->vl.n, &v->vl.na, sizeof(v->vl.vecs[0]));
	if(v->vl.n > 0)
		memmove(&v->vl.vecs[1], &v->vl.vecs[0], v->vl.n*sizeof(v->vl.vecs[0]));
	v->vl.vecs[0] = x;
	v->vl.n++;
	return x;
}

static void value_delete_vector(value_t *v)
{
	int i;
	if(!v)
		return;
	assert(v->type == VAL_VECTOR);
	for(i = 0; i < v->v.n; i++)
		value_delete(v->v.vals[i]);
	v->v.n = 0;
	memset(v->v.vals, 0, v->v.na * sizeof(v->v.vals[0]));
	testalloc((void **)&flvector, nflvector, &naflvector, sizeof(flvector[0]));
	flvector[nflvector] = v;
	nflvector++;
}

void value_delete(value_t *v)
{
	int i;
	assert(v != NULL);

	//printf("value_delete(%s) --> %d  %p\n", valuetypename(v->type), v->idcnt, v);

	switch(v->type) {
	case VAL_STRING:
		free(v->str.chs);
		/* Fallthrough */
	case VAL_UNDEF:
	case VAL_INT:
	case VAL_FLOAT:
	case VAL_REF:
		memset(v, 0, sizeof(*v));
		testalloc((void **)&flscalar, nflscalar, &naflscalar, sizeof(flscalar[0]));
		flscalar[nflscalar] = v;
		nflscalar++;
		break;
	case VAL_VECTOR:
		value_delete_vector(v);
		break;
	case VAL_VECTORLIST:
		for(i = 0; i < v->vl.n; i++)
			value_delete(v->vl.vecs[i]);
		v->vl.n = 0;
		memset(v->vl.vecs, 0, v->vl.na * sizeof(v->vl.vecs[0]));
		testalloc((void **)&flvectorlist, nflvectorlist, &naflvectorlist, sizeof(flvectorlist[0]));
		flvectorlist[nflvectorlist] = v;
		nflvectorlist++;
		break;
	}
}

double value_to_dist(const value_t *v)
{
	return value_to_mm(v);
}

double value_to_mm(const value_t *v)
{
	double d;
	v = value_deref(v);
	if(!v || (v->type != VAL_INT && v->type != VAL_FLOAT))
		return 0.0;
	d = v->type == VAL_INT ? (double)v->i : v->d;
	switch(v->unit) {
	case UNIT_IN:	return d * 25.4;
	default:	return d;
	}
}

double value_to_in(const value_t *v)
{
	double d;
	v = value_deref(v);
	if(!v || (v->type != VAL_INT && v->type != VAL_FLOAT))
		return 0.0;
	d = v->type == VAL_INT ? (double)v->i : v->d;
	switch(v->unit) {
	case UNIT_MM:	return d / 25.4;
	default:	return d;
	}
}

double value_to_deg(const value_t *v)
{
	double d;
	v = value_deref(v);
	if(!v || (v->type != VAL_INT && v->type != VAL_FLOAT))
		return 0.0;
	d = v->type == VAL_INT ? (double)v->i : v->d;
	switch(v->unit) {
	case UNIT_RAD:	return d / M_PI * 180.0;
	default:	return d;
	}
}

double value_to_rad(const value_t *v)
{
	double d;
	v = value_deref(v);
	if(!v || (v->type != VAL_INT && v->type != VAL_FLOAT))
		return 0.0;
	d = v->type == VAL_INT ? (double)v->i : v->d;
	switch(v->unit) {
	case UNIT_DEG:	return d * M_PI / 180.0;
	default:	return d;
	}
}

double value_to_double(const value_t *v)
{
	if(!v)
		return 0.0;
	v = value_deref(v);
	switch(v->type) {
	case VAL_INT:	return (double)v->i;
	case VAL_FLOAT:	return v->d;
	default:	return 0.0;
	}
}

int value_to_int(const value_t *v)
{
	if(!v)
		return 0;
	v = value_deref(v);
	switch(v->type) {
	case VAL_INT:	return v->i;
	case VAL_FLOAT:	return (int)(fabs(v->d - round(v->d)) > EPSILON ? v->d : round(v->d));
	default:	return 0;
	}
}

double value_to_unit(const value_t *v, unit_et unit)
{
	v = value_deref(v);
	switch(unit) {
	default:
	case UNIT_NONE:	return value_to_double(v);
	case UNIT_MM:	return value_to_mm(v);
	case UNIT_IN:	return value_to_in(v);
	case UNIT_DEG:	return value_to_deg(v);
	case UNIT_RAD:	return value_to_rad(v);
	}
}

int value_to_boolean(const value_t *v)
{
	if(!v)
		return 0;
	v = value_deref(v);
	switch(v->type) {
	case VAL_INT:		return v->i != 0;
	case VAL_FLOAT:		return fabs(v->d) >= EPSILON;
	case VAL_VECTOR:	return v->v.n != 0;
	case VAL_VECTORLIST:	return v->vl.n != 0;
	case VAL_STRING:	return v->str.chs[0] != 0;
	default:		return 0 == 1;
	}
}

const value_t *value_get(int argc, int offset)
{
	value_t *v;
	if(argc < 1)
		rtinternal(NULL, "Call argument retrieval atempt with no arguments available (argc=%d, offset=%d)", argc, offset);
	if(offset > argc-1 || offset < 0)
		rtinternal(NULL, "Call argument request outside pushed bounds (argc=%d, offset=%d)", argc, offset);
	offset -= argc-1;
	if(nvaluestack + offset < 0 || offset > 0)
		rtinternal(NULL, "Call argument request over/underflows valuestack (argc=%d, offset=%d, nvaluestack=%d)", argc, offset, nvaluestack);
	v = valuestack[nvaluestack + offset - 1];
	return value_deref(v);
}

static void sprintvector(wchar_t **s, size_t *ns, const node_t *ref, const value_t *v)
{
	int i;
	assert(v->type == VAL_VECTOR);
	mysprintf(s, ns, L"[");
	for(i = 0; i < v->v.n; i++) {
		if(i)
			mysprintf(s, ns, L",");
		switch(v->v.vals[i]->type) {
		case VAL_UNDEF:		mysprintf(s, ns, L"-"); break;
		case VAL_INT:		mysprintf(s, ns, L"%d%s", v->v.vals[i]->i, unitshortname(v->v.vals[i]->unit)); break;
		case VAL_FLOAT:		mysprintf(s, ns, L"%.*f%s", cl_decimals, v->v.vals[i]->d, unitshortname(v->v.vals[i]->unit)); break;
		default:		rtinternal(ref, "Invalid value type %d in vector printing", v->v.vals[i]->type);
		}
	}
	mysprintf(s, ns, L"]");
}

value_t *value_to_string(const node_t *ref, const value_t *v)
{
	int j;
	wchar_t *s = _wcsdup(L"");
	size_t ns = 0;
	value_t *rv;

	assert(s != NULL);
	v = value_deref(v);

	switch(v->type) {
	case VAL_UNDEF:		mysprintf(&s, &ns, L"<undef>"); break;
	case VAL_INT:		mysprintf(&s, &ns, L"%d%s", v->i, unitshortname(v->unit)); break;
	case VAL_FLOAT:		mysprintf(&s, &ns, L"%.*f%s", cl_decimals, v->d, unitshortname(v->unit)); break;
	case VAL_STRING:	mysprintf(&s, &ns, L"%ls", v->str.chs); break;
	case VAL_VECTOR:	sprintvector(&s, &ns, ref, v); break;
	case VAL_VECTORLIST:
		mysprintf(&s, &ns, L"{");
		for(j = 0; j < v->vl.n; j++) {
			if(j)
				mysprintf(&s, &ns, L",");
			sprintvector(&s, &ns, ref, v->vl.vecs[j]);
		}
		mysprintf(&s, &ns, L"}");
		break;
	default:
		rtinternal(ref, "Invalid value type %s in value_to_str()", valuetypename(v->type));
	}

	rv = value_new(VAL_STRING);
	rv->str.chs = s;
	rv->str.n = ns;
	rv->str.na = ns + 1;
	return rv;
}

