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
#include <stdint.h>
#include <math.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <wctype.h>

#include "utils.h"
#include "grammartypes.h"
#include "value.h"
#include "variable.h"
#include "interpreter.h"
#include "builtin.h"
#include "checkarg.h"

static int g28_valid;
static int g30_valid;
static double g28_pos[MAXAXES];
static double g30_pos[MAXAXES];

/*
 * Function:	pi()
 * Input:	-
 * Output:	scalar: 3.14159265...
 *
 * Simply return the (yummy) number pi.
 */
static value_t *bi_pi(const node_t *ref, int argc)
{
	check_arg_none(ref, "pi", argc);
	value_t *v = value_new(VAL_FLOAT);
	v->d = M_PI;
	return v;
}

/*
 * Function:	position([scalar:naxes])
 * Input:	scalar: naxis - optional number of axes to return, defaults to 3 (XYZ)
 * Output:	vector: with current absolute position
 *
 * Returns the current absolute position as a vector. The default number of
 * coordinates returned is 3 (XYZ). The maximum number of coordinates returned
 * is 6 or 9, depending whether 9-axis mode is enabled on the command-line. The
 * optional argument naxes limits the number of axis returned.
 */
value_t *bi_position(const node_t *ref, int argc)
{
	int i;
	int nax = 3;	/* Default to XYZ */
	const variable_t *var;
	if(argc > 1)
		rtwarning(ref, "postion() has only one optional argument");
	if(argc > 0) {
		const value_t *v = value_get(argc, 1);
		if(v->unit != UNIT_NONE)
			rtwarning(ref, "postion() argument has units attached, ignored");
		nax = value_to_int(v);
		if(nax < 1) {
			rterror(ref, "postion() argument must be > 0");
			nax = 1;
		}
		if(nax > naxes) {
			rtwarning(ref, "postion() argument returns maximum %d vector coordinates", naxes);
			nax = naxes;
		}
	}

	if(!(var = variable_lookup_deref(L"__global_position", 1)))
		rtinternal(ref, "Global position variable cannot be found");
	if(!isvector(var->v))
		rtinternal(ref, "Global position variable not a vector");

	value_t *v = value_new(VAL_VECTOR);
	for(i = 0; i < nax && i < var->v->v.n && i < naxes; i++)
		value_vector_add(v, value_dup(var->v->v.vals[i]));
	return v;
}

/*
 * Function:	atan(scalar:y, scalar:x)
 * Input:	scalar:y - y-coordinate
 * 		scalar:x - x-coordinate
 * Output:	sclalar:atan(y/x)
 *
 * Return the arc-tangent of y/x in radians.
 */
static value_t *bi_atan(const node_t *ref, int argc)
{
	double d0, d1;
	const value_t *arg0, *arg1;
	value_t *v;
	check_arg_two(ref, "atan", argc);
	arg0 = value_get(argc, 0);
	arg1 = value_get(argc, 1);
	check_arg_scalar(ref, "atan", arg0, 0);
	check_arg_scalar(ref, "atan", arg1, 1);
	check_arg_unit_distance(ref, "atan", arg0, 0);
	check_arg_unit_distance(ref, "atan", arg1, 1);
	d0 = value_to_mm(arg0);		/* Units don't matter as long as they are same for x and y in atan2(x, y) */
	d1 = value_to_mm(arg1);
	v = value_new(VAL_FLOAT);
	v->unit = UNIT_RAD;
	v->d = atan2(d0, d1);
	return v;
}

/*
 * Function:	atan_xy(vector:v)
 * Input:	vector:v - Any vector with both X and Y coordinates
 * Output:	scalar:atan(v.y/v.x)
 *
 * Return the arc-tangent of v.y/v.x in radians.
 */
static value_t *bi_atan_xy(const node_t *ref, int argc)
{
	double d0, d1;
	const value_t *arg;
	value_t *v;
	check_arg_one(ref, "atan_xy", argc);
	arg = value_get(argc, 0);
	check_arg_vector(ref, "atan_xy", arg, 0);
	if(arg->v.n < 2)
		rterror(ref, "atan_xy(): Both X and Y coordinates must be present");
	if(!isscalar(arg->v.vals[0]) || !isscalar(arg->v.vals[1]))
		rterror(ref, "atan_xy(): Both X and Y coordinates must be scalar");
	if(!isnoneordistance(arg->v.vals[0]) || !isnoneordistance(arg->v.vals[1]))
		rterror(ref, "atan_xy(): Both X and Y coordinates must be distance");
	if(arg->v.vals[0]->unit != arg->v.vals[1]->unit)
		rtwarning(ref, "atan_xy(): Coordinates have different units (X=%s, Y=%s)", unitname(arg->v.vals[0]->unit), unitname(arg->v.vals[1]->unit));
	d0 = value_to_mm(arg->v.vals[1]);		/* Units don't matter as long as they are same for x and y */
	d1 = value_to_mm(arg->v.vals[0]);
	v = value_new(VAL_FLOAT);
	v->unit = UNIT_RAD;
	v->d = atan2(d0, d1);	/* atan Y/X */
	return v;
}

/*
 * Function:	atan_xz(vector:v)
 * Input:	vector:v - Any vector with both X and Z coordinates
 * Output:	scalar:atan(v.z/v.x)
 *
 * Return the arc-tangent of v.z/v.x in radians.
 */
static value_t *bi_atan_xz(const node_t *ref, int argc)
{
	double d0, d1;
	const value_t *arg;
	value_t *v;
	check_arg_one(ref, "atan_xz", argc);
	arg = value_get(argc, 0);
	check_arg_vector(ref, "atan_xz", arg, 0);
	if(arg->v.n < 3)
		rterror(ref, "atan_xz(): Both X and Z coordinates must be present");
	if(!isscalar(arg->v.vals[0]) || !isscalar(arg->v.vals[2]))
		rterror(ref, "atan_xz(): Both X and Z coordinates must be scalar");
	if(!isnoneordistance(arg->v.vals[0]) || !isnoneordistance(arg->v.vals[2]))
		rterror(ref, "atan_xz(): Both X and Z coordinates must be distance");
	if(arg->v.vals[0]->unit != arg->v.vals[2]->unit)
		rtwarning(ref, "atan_xz(): Coordinates have different units (X=%s, Z=%s)", unitname(arg->v.vals[0]->unit), unitname(arg->v.vals[2]->unit));
	d0 = value_to_mm(arg->v.vals[2]);		/* Units don't matter as long as they are same for x and z */
	d1 = value_to_mm(arg->v.vals[0]);
	v = value_new(VAL_FLOAT);
	v->unit = UNIT_RAD;
	v->d = atan2(d0, d1);	/* atan Z/X */
	return v;
}

/*
 * Function:	atan_yz(vector:v)
 * Input:	vector:v - Any vector with both Y and Z coordinates
 * Output:	scalar:atan(v.z/v.y)
 *
 * Return the arc-tangent of v.z/v.y in radians.
 */
static value_t *bi_atan_yz(const node_t *ref, int argc)
{
	double d0, d1;
	const value_t *arg;
	value_t *v;
	check_arg_one(ref, "atan_yz", argc);
	arg = value_get(argc, 0);
	check_arg_vector(ref, "atan_yz", arg, 0);
	if(arg->v.n < 3)
		rterror(ref, "atan_yz(): Both Y and Z coordinates must be present");
	if(!isscalar(arg->v.vals[1]) || !isscalar(arg->v.vals[2]))
		rterror(ref, "atan_yz(): Both Y and Z coordinates must be scalar");
	if(!isnoneordistance(arg->v.vals[1]) || !isnoneordistance(arg->v.vals[2]))
		rterror(ref, "atan_yz(): Both Y and Z coordinates must be distance");
	if(arg->v.vals[1]->unit != arg->v.vals[2]->unit)
		rtwarning(ref, "atan_yz(): Coordinates have different units (Y=%s, Z=%s)", unitname(arg->v.vals[1]->unit), unitname(arg->v.vals[2]->unit));
	d0 = value_to_mm(arg->v.vals[2]);		/* Units don't matter as long as they are same for y and z */
	d1 = value_to_mm(arg->v.vals[1]);
	v = value_new(VAL_FLOAT);
	v->unit = UNIT_RAD;
	v->d = atan2(d0, d1);	/* atan Z/Y */
	return v;
}

/*
 * Function:	pow(scalar:x, scalar:y)
 * Input:	scalar:x - base
 * 		scalar:y - exponent
 * Output:	sclalar:x**y
 *
 * Return the x raised to the power y. The returned value has no units associated.
 */
static value_t *bi_pow(const node_t *ref, int argc)
{
	double d0, d1;
	const value_t *arg0, *arg1;
	value_t *v;
	check_arg_two(ref, "pow", argc);
	arg0 = value_get(argc, 0);
	arg1 = value_get(argc, 1);
	check_arg_scalar(ref, "pow", arg0, 0);
	check_arg_scalar(ref, "pow", arg1, 1);
	check_arg_unit_none(ref, "pow", arg1, 1);
	d0 = value_to_double(arg0);
	d1 = value_to_double(arg1);
	v = value_new(VAL_FLOAT);
	v->d = pow(d0, d1);
	return v;
}

typedef enum {
	FN_sin,
	FN_cos,
	FN_tan,
	FN_asin,
	FN_acos,
	FN_round,
	FN_floor,
	FN_ceil,
	FN_loge,
	FN_log10,
	FN_log2,
	FN_exp,
	FN_sqrt,
} oneargfns_et;

/*
 * Function:	sin(scalar:x)
 * 		cos(scalar:x)
 * 		tan(scalar:x)
 * 		asin(scalar:x)
 * 		acos(scalar:x)
 * 		round(scalar:x)
 * 		floor(scalar:x)
 * 		ceil(scalar:x)
 * 		loge(scalar:x)
 * 		log10(scalar:x)
 * 		log2(scalar:x)
 * 		exp(scalar:x)
 * 		sqrt(scalar:x)
 * Input:	scalar:x - a number
 * Output:	scalar:calculated result for the appropriate function.
 *
 * Returns the result of the single-argument math function.
 */
static value_t *oneargdoubles(const node_t *ref, int argc, const char *fn, oneargfns_et idx)
{
	double d;
	const value_t *arg;
	value_t *v;
	check_arg_one(ref, fn, argc);
	arg = value_get(argc, 0);
	check_arg_scalar(ref, fn, arg, 0);
	switch(idx) {
	case FN_sin:
	case FN_cos:
	case FN_tan:
		check_arg_unit_angle(ref, fn, arg, 0);
		break;
	case FN_asin:
	case FN_acos:
		check_arg_unit_none(ref, fn, arg, 0);
		break;
	default:
		break;
	}
	d = value_to_double(arg);
	v = value_new(VAL_FLOAT);
	switch(idx) {
	case FN_sin:	v->d = sin(value_to_rad(arg)); break;
	case FN_cos:	v->d = cos(value_to_rad(arg)); break;
	case FN_tan:	v->d = tan(value_to_rad(arg)); break;
	case FN_asin:	check_range_double(ref, fn, 0, d, -1.0, 1.0); v->d = asin(d); v->unit = UNIT_RAD; break;
	case FN_acos:	check_range_double(ref, fn, 0, d, -1.0, 1.0); v->d = acos(d); v->unit = UNIT_RAD; break;
	case FN_round:	v->d = round(d); v->unit = arg->unit; break;
	case FN_floor:	v->d = floor(d); v->unit = arg->unit; break;
	case FN_ceil:	v->d = ceil(d); v->unit = arg->unit; break;
	case FN_loge:	check_range_double(ref, fn, 0, d, EPSILON, HUGE_VAL); v->d = log(d); break;
	case FN_log10:	check_range_double(ref, fn, 0, d, EPSILON, HUGE_VAL); v->d = log10(d); break;
	case FN_log2:	check_range_double(ref, fn, 0, d, EPSILON, HUGE_VAL); v->d = log2(d); break;
	case FN_exp:	v->d = exp(d); break;
	case FN_sqrt:	check_range_double(ref, fn, 0, d, 0.0, HUGE_VAL); v->d = sqrt(d); break;
	}
	return v;
}

#define ONEARGFNS(f)	static value_t *bi_ ## f (const node_t *ref, int argc) { return oneargdoubles(ref, argc, #f, FN_ ## f ); }

ONEARGFNS(sin)
ONEARGFNS(cos)
ONEARGFNS(tan)
ONEARGFNS(asin)
ONEARGFNS(acos)
ONEARGFNS(round)
ONEARGFNS(floor)
ONEARGFNS(ceil)
ONEARGFNS(loge)
ONEARGFNS(log10)
ONEARGFNS(log2)
ONEARGFNS(exp)
ONEARGFNS(sqrt)

/*
 * Function:	to_int(anytype)
 * Input:	anytype, each encapsulated scalar value
 * Output:	same type
 *
 * Returns the same type as the argument with all scalar values converted to
 * integer. Units are preserved in the conversion.
 */
static value_t *bi_to_int(const node_t *ref, int argc)
{
	int i, j;
	const value_t *arg;
	wchar_t *eptr;
	int base = 0;
	value_t *v = value_new(VAL_INT);
	check_arg_one(ref, "to_int", argc);
	arg = value_get(argc, 0);
	switch(arg->type) {
	case VAL_UNDEF:
		v->type = VAL_UNDEF;
		break;
	case VAL_FLOAT:
		v->i = arg->d;
		v->type = VAL_INT;
		v->unit = arg->unit;
		break;
	case VAL_INT:
		v->i = arg->i;
		v->unit = arg->unit;
		break;
	case VAL_VECTOR:
		v->type = VAL_VECTOR;
		for(i = 0; i < arg->v.n; i++) {
			switch(arg->v.vals[i]->type) {
			case VAL_UNDEF:	value_vector_add_undef(v); break;
			case VAL_FLOAT:	value_vector_add(v, value_new_int(value_to_int(arg->v.vals[i]), arg->v.vals[i]->unit)); break;
			case VAL_INT:	value_vector_add(v, value_new_int(arg->v.vals[i]->i, arg->v.vals[i]->unit)); break;
			default:
				rtinternal(ref, "to_int(): Invalid value type '%s' in vector[%d]", valuetypename(arg->v.vals[i]->type), i);
				break;
			}
		}
		break;
	case VAL_VECTORLIST:
		v->type = VAL_VECTORLIST;
		for(i = 0; i < arg->vl.n; i++) {
			const value_t *vec = arg->vl.vecs[i];
			value_t *nv = value_new(VAL_VECTOR);
			assert(vec->type == VAL_VECTOR);
			for(j = 0; j < vec->v.n; j++) {
				switch(vec->v.vals[j]->type) {
				case VAL_UNDEF:	value_vector_add_undef(nv); break;
				case VAL_FLOAT:	value_vector_add(nv, value_new_int(value_to_int(vec->v.vals[j]), vec->v.vals[j]->unit)); break;
				case VAL_INT:	value_vector_add(nv, value_new_int(vec->v.vals[j]->i, vec->v.vals[j]->unit)); break;
				default:
					rtinternal(ref, "to_int(): Invalid value type '%s' in vectorlist[%d][%d]", valuetypename(arg->v.vals[i]->type), i, j);
					break;
				}
			}
			value_vectorlist_add(v, nv);
		}
		break;
	case VAL_STRING:
		if(argc > 1) {
			const value_t *argbase = value_get(argc, 1);
			if(!isint(argbase) || !isnone(argbase))
				rtwarning(ref, "to_int(): Base argument should be integer and have no units attached");
			base = value_to_int(argbase);
			if(base < 0 || base == 1 || base > 36) {
				rtwarning(ref, "to_int(): Base argument must be zero or between [2..36], setting to zero");
				base = 0;
			}
		}
		v->unit = UNIT_NONE;	/* This is the default */
		if(!wcscmp(arg->str.chs, L"<undef>")) {
			v->type = VAL_UNDEF;
			break;
		}
		v->i = wcstol(arg->str.chs, &eptr, base);
		if(*eptr) {
			if(!wcscmp(eptr, L"mm"))
				v->unit = UNIT_MM;
			else if(!wcscmp(eptr, L"in"))
				v->unit = UNIT_IN;
			else if(!wcscmp(eptr, L"mil")) {
				v->unit = UNIT_IN;
				if(v->i % 1000)
					rtwarning(ref, "to_int(): Unit mil in string to integer loses digits because of inch conversion (%dmil != %din)", v->i, v->i / 1000);
				v->i /= 1000;		/* Mil is 1/1000 inch */
			} else if(!wcscmp(eptr, L"rad"))
				v->unit = UNIT_RAD;
			else if(!wcscmp(eptr, L"deg"))
				v->unit = UNIT_DEG;
			else
				rtwarning(ref, "to_int(): String has a trailing context '%ls' which does not translate into a recognized unit", eptr);
		}
		break;
	default:
		rterror(ref, "to_int(): Invalid argument value type '%s'", valuetypename(arg->type));
		break;
	}
	return v;
}

/*
 * Function:	to_float(anytype)
 * Input:	anytype, each encapsulated scalar value
 * Output:	same type
 *
 * Returns the same type as the argument with all scalar values converted to
 * floating point. Units are preserved in the conversion.
 */
static value_t *bi_to_float(const node_t *ref, int argc)
{
	int i, j;
	const value_t *arg;
	wchar_t *eptr;
	value_t *v = value_new(VAL_FLOAT);
	check_arg_one(ref, "to_float", argc);
	arg = value_get(argc, 0);
	switch(arg->type) {
	case VAL_UNDEF:
		v->type = VAL_UNDEF;
		break;
	case VAL_FLOAT:
		v->d = arg->d;
		v->unit = arg->unit;
		break;
	case VAL_INT:
		v->d = arg->i;
		v->type = VAL_FLOAT;
		v->unit = arg->unit;
		break;
	case VAL_VECTOR:
		v->type = VAL_VECTOR;
		for(i = 0; i < arg->v.n; i++) {
			switch(arg->v.vals[i]->type) {
			case VAL_UNDEF:	value_vector_add_undef(v); break;
			case VAL_FLOAT:	value_vector_add(v, value_new_flt(arg->v.vals[i]->d, arg->v.vals[i]->unit)); break;
			case VAL_INT:	value_vector_add(v, value_new_flt((double)arg->v.vals[i]->i, arg->v.vals[i]->unit)); break;
			default:
				rtinternal(ref, "to_float(): Invalid value type '%s' in vector[%d]", valuetypename(arg->v.vals[i]->type), i);
				break;
			}
		}
		break;
	case VAL_VECTORLIST:
		v->type = VAL_VECTORLIST;
		for(i = 0; i < arg->vl.n; i++) {
			const value_t *vec = arg->vl.vecs[i];
			value_t *nv = value_new(VAL_VECTOR);
			assert(vec->type == VAL_VECTOR);
			for(j = 0; j < vec->v.n; j++) {
				switch(vec->v.vals[j]->type) {
				case VAL_UNDEF:	value_vector_add_undef(nv); break;
				case VAL_FLOAT:	value_vector_add(nv, value_new_flt(vec->v.vals[j]->d, vec->v.vals[j]->unit)); break;
				case VAL_INT:	value_vector_add(nv, value_new_flt((double)vec->v.vals[j]->i, vec->v.vals[j]->unit)); break;
				default:
					rtinternal(ref, "to_float(): Invalid value type '%s' in vectorlist[%d][%d]", valuetypename(arg->v.vals[i]->type), i, j);
					break;
				}
			}
			value_vectorlist_add(v, nv);
		}
		break;
	case VAL_STRING:
		v->unit = UNIT_NONE;	/* This is the default */
		if(!wcscmp(arg->str.chs, L"<undef>")) {
			v->type = VAL_UNDEF;
			break;
		}
		v->d = wcstod(arg->str.chs, &eptr);
		if(*eptr) {
			if(!wcscmp(eptr, L"mm"))
				v->unit = UNIT_MM;
			else if(!wcscmp(eptr, L"in"))
				v->unit = UNIT_IN;
			else if(!wcscmp(eptr, L"mil")) {
				v->unit = UNIT_IN;
				v->d /= 1000.0;		/* Mil is 1/1000 inch */
			} else if(!wcscmp(eptr, L"rad"))
				v->unit = UNIT_RAD;
			else if(!wcscmp(eptr, L"deg"))
				v->unit = UNIT_DEG;
			else
				rtwarning(ref, "to_float(): String has a trailing context '%ls' which does not translate into a recognized unit", eptr);
		}
		if(isnan(v->d))
			rtwarning(ref, "to_float(): Value evaluates to NaN");
		if(isinf(v->d))
			rtwarning(ref, "to_float(): Value evaluates to %cInf", isinf(v->d) > 0 ? '+' : '-');
		break;
	default:
		rterror(ref, "to_float(): Invalid argument value type '%s'", valuetypename(arg->type));
		break;
	}
	return v;
}

/*
 * Function:	to_val(string)
 * Input:	string which first char is converted to its Unicode value
 * Output:	integer (none)
 *
 * Converts the first character of the string into the Unicode value which
 * represents the character. The return value is integer with no units
 * attached.
 */
static value_t *bi_to_val(const node_t *ref, int argc)
{
	const value_t *arg;
	value_t *v;
	check_arg_one(ref, "to_val", argc);
	arg = value_get(argc, 0);
	check_arg_string(ref, "to_val", arg, 0);
	v = value_new(VAL_INT);
	v->i = (int)arg->str.chs[0];
	return v;
}

/*
 * Function:	to_chr(integer)
 * Input:	Integer unicode value to be converted into a string
 * Output:	integer (none)
 *
 * Converts the argument's value into a single Unicode character string with
 * the value of the argument.
 */
static value_t *bi_to_chr(const node_t *ref, int argc)
{
	const value_t *arg;
	value_t *v;
	wchar_t chs[2];
	check_arg_one(ref, "to_chr", argc);
	arg = value_get(argc, 0);
	check_arg_scalar(ref, "to_chr", arg, 0);
	if(!isint(arg) || !isnone(arg))
		rtwarning(ref, "to_chr(): Argument should be integer with no units");
	v = value_new(VAL_STRING);
	if(!(chs[0] = value_to_int(arg))) {
		if(cl_pedantic)
			rtwarning(ref, "to_chr(): Argument is zero and creates an empty string");
		v->str.n = 0;
		v->str.na = 1;
	} else {
		v->str.n = 1;
		v->str.na = 2;
	}
	chs[1] = 0;
	v->str.chs = wcsdup(chs);
	assert(v->str.chs != NULL);
	return v;
}

/*
 * Function:	abs(scalar:x)
 * Input:	scalar:x - a value
 * Output:	scalar:absolute value |x|
 *
 * Returns the absolute value of the argument. Both scalar type and units are
 * preserved.
 */
static value_t *bi_abs(const node_t *ref, int argc)
{
	const value_t *arg;
	value_t *v;
	check_arg_one(ref, "abs", argc);
	arg = value_get(argc, 0);
	check_arg_scalar(ref, "abs", arg, 0);
	v = value_new(arg->type);
	switch(arg->type) {
	case VAL_INT:	v->i = abs(arg->i); break;
	case VAL_FLOAT:	v->d = fabs(arg->d); break;
	default:	rterror(ref, "Argument to abs() should be scalar"); break;
	}
	v->unit = arg->unit;
	return v;
}

typedef value_t *(*cvtfunc_t)(const node_t *ref, const value_t *arg);

static value_t *_to_mm(const node_t *ref, const value_t *arg)
{
	value_t *v = value_new(arg->type);
	if(arg->type == VAL_UNDEF)
		return v;
	assert(arg->type == VAL_INT || arg->type == VAL_FLOAT);
	switch(arg->unit) {
	case UNIT_DEG:
	case UNIT_RAD:
		rtwarning(ref, "Conversion from %s to mm not compatible", unitprintname(arg->unit));
		/* Fallthrough */
	case UNIT_NONE:
	case UNIT_MM:
		if(v->type == VAL_INT)
			v->i = arg->i;
		else
			v->d = arg->d;
		break;
	case UNIT_IN:
		if(v->type == VAL_INT)
			v->d = (double)arg->i * 25.4;
		else
			v->d = arg->d * 25.4;
		v->type = VAL_FLOAT;
		break;
	}
	v->unit = UNIT_MM;
	return v;
}

static value_t *_to_in(const node_t *ref, const value_t *arg)
{
	value_t *v = value_new(arg->type);
	if(arg->type == VAL_UNDEF)
		return v;
	assert(arg->type == VAL_INT || arg->type == VAL_FLOAT);
	switch(arg->unit) {
	case UNIT_DEG:
	case UNIT_RAD:
		rtwarning(ref, "Conversion from %s to inch not compatible", unitprintname(arg->unit));
		/* Fallthrough */
	case UNIT_NONE:
	case UNIT_IN:
		if(v->type == VAL_INT)
			v->i = arg->i;
		else
			v->d = arg->d;
		break;
	case UNIT_MM:
		if(v->type == VAL_INT)
			v->d = (double)arg->i / 25.4;
		else
			v->d = arg->d / 25.4;
		v->type = VAL_FLOAT;
		break;
	}
	v->unit = UNIT_IN;
	return v;
}

static value_t *_to_deg(const node_t *ref, const value_t *arg)
{
	value_t *v = value_new(arg->type);
	if(arg->type == VAL_UNDEF)
		return v;
	assert(arg->type == VAL_INT || arg->type == VAL_FLOAT);
	switch(arg->unit) {
	case UNIT_MM:
	case UNIT_IN:
		rtwarning(ref, "Conversion from %s to degrees not compatible", unitprintname(arg->unit));
		/* Fallthrough */
	case UNIT_NONE:
	case UNIT_DEG:
		if(v->type == VAL_INT)
			v->i = arg->i;
		else
			v->d = arg->d;
		break;
	case UNIT_RAD:
		if(v->type == VAL_INT)
			v->d = (double)arg->i / M_PI * 180.0;
		else
			v->d = arg->d / M_PI * 180.0;
		v->type = VAL_FLOAT;
		break;
	}
	v->unit = UNIT_DEG;
	return v;
}

static value_t *_to_rad(const node_t *ref, const value_t *arg)
{
	value_t *v = value_new(arg->type);
	if(arg->type == VAL_UNDEF)
		return v;
	assert(arg->type == VAL_INT || arg->type == VAL_FLOAT);
	switch(arg->unit) {
	case UNIT_MM:
	case UNIT_IN:
		rtwarning(ref, "Conversion from %s to degrees not compatible", unitprintname(arg->unit));
		/* Fallthrough */
	case UNIT_NONE:
	case UNIT_RAD:
		if(v->type == VAL_INT)
			v->i = arg->i;
		else
			v->d = arg->d;
		break;
	case UNIT_DEG:
		if(v->type == VAL_INT)
			v->d = (double)arg->i * M_PI / 180.0;
		else
			v->d = arg->d * M_PI / 180.0;
		v->type = VAL_FLOAT;
		break;
	}
	v->unit = UNIT_RAD;
	return v;
}

static value_t *_to_native(const node_t *ref, const value_t *arg)
{
	value_t *v = value_new(arg->type);
	if(arg->type == VAL_UNDEF)
		return v;
	assert(arg->type == VAL_INT || arg->type == VAL_FLOAT);
	v->unit = arg->unit;
	switch(arg->unit) {
	case UNIT_MM:
		/* Already mm */
		if(v->type == VAL_INT)
			v->i = arg->i;
		else
			v->d = arg->d;

		break;
	case UNIT_IN:
		/* inch -> mm */
		if(v->type == VAL_INT)
			v->d = (double)arg->i * 25.4;
		else
			v->d = arg->d * 25.4;
		v->type = VAL_FLOAT;
		v->unit = UNIT_MM;
		break;
	case UNIT_NONE:	/* 'none' does not need conversion */
	case UNIT_DEG:
		/* Deg is native for gcode */
		if(v->type == VAL_INT)
			v->i = arg->i;
		else
			v->d = arg->d;
		break;
	case UNIT_RAD:
		/* rad -> deg */
		if(v->type == VAL_INT)
			v->d = (double)arg->i / M_PI * 180.0;
		else
			v->d = arg->d / M_PI * 180.0;
		v->type = VAL_FLOAT;
		v->unit = UNIT_DEG;
		break;
	}
	return v;
}

static value_t *_to_none(const node_t *ref, const value_t *arg)
{
	value_t *v = value_new(arg->type);
	(void)ref;
	assert(v->type == VAL_INT || v->type == VAL_FLOAT);
	if(v->type == VAL_INT)
		v->i = arg->i;
	else
		v->d = arg->d;
	v->unit = UNIT_NONE;
	return v;
}

static value_t *_to_distance(const node_t *ref, const value_t *arg)
{
	value_t *v = value_dup(arg);
	assert(v->type == VAL_INT || v->type == VAL_FLOAT);
	if(isangle(v)) {
		rtwarning(ref, "Angular units %s converted to distance units %s", unitname(v->unit), unitname(UNIT_MM) );
		v->unit = UNIT_NONE;
	}
	if(v->unit == UNIT_NONE)
		v->unit = UNIT_MM;
	return v;
}

/*
 * Function:	to_mm(anytype)
 * 		to_in(anytype)
 * 		to_inch(anytype)	- deprecated
 * 		to_deg(anytype)
 * 		to_rad(anytype)
 * 		to_native(anytype)
 * 		to_none(anytype)
 * 		to_distance(anytype)
 * Input:	anytype value
 * Output:	any type value converted to the appropriate content
 *
 * Unit conversion from any to the requested type. Distance-to-angular
 * conversions result in a warning.
 *
 * Native conversion of units are always degrees for angles and mm/inch for
 * distance, depending the command-line settings.
 */
static value_t *unit_conversion(const node_t *ref, const char *fn, int argc, cvtfunc_t cvt)
{
	const value_t *arg;
	value_t *v;
	int i, j;
	check_arg_one(ref, fn, argc);
	arg = value_get(argc, 0);
	switch(arg->type) {
	case VAL_INT:
	case VAL_FLOAT:
		return cvt(ref, arg);
	case VAL_VECTOR:
		v = value_new(VAL_VECTOR);
		for(i = 0; i < arg->v.n; i++)
			value_vector_add(v, cvt(ref, arg->v.vals[i]));
		return v;
	case VAL_VECTORLIST:
		v = value_new(VAL_VECTORLIST);
		for(i = 0; i < arg->vl.n; i++) {
			value_t *vec = arg->vl.vecs[i];
			value_t *newvec = value_new(VAL_VECTOR);
			for(j = 0; j < vec->v.n; j++)
				value_vector_add(newvec, cvt(ref, vec->v.vals[j]));
			value_vectorlist_add(v, newvec);
		}
		return v;
	case VAL_UNDEF:
		break;
	case VAL_STRING:
		rterror(ref, "Strings cannot be converted to value with units in %s()", fn);
		break;
	default:
		rtinternal(ref, "Undefined value type '%d' in %s()", arg->type, fn);
		break;
	}
	return value_new(VAL_UNDEF);
}

static value_t *bi_to_mm(const node_t *ref, int argc)
{
	return unit_conversion(ref, "to_mm", argc, _to_mm);
}

static value_t *bi_to_in(const node_t *ref, int argc)
{
	return unit_conversion(ref, "to_in", argc, _to_in);
}

static value_t *bi_to_inch(const node_t *ref, int argc)
{
	return unit_conversion(ref, "to_in", argc, _to_in);
}

static value_t *bi_to_deg(const node_t *ref, int argc)
{
	return unit_conversion(ref, "to_deg", argc, _to_deg);
}

static value_t *bi_to_rad(const node_t *ref, int argc)
{
	return unit_conversion(ref, "to_rad", argc, _to_rad);
}

static value_t *bi_to_native(const node_t *ref, int argc)
{
	return unit_conversion(ref, "to_native", argc, _to_native);
}

static value_t *bi_to_none(const node_t *ref, int argc)
{
	return unit_conversion(ref, "to_none", argc, _to_none);
}

static value_t *bi_to_distance(const node_t *ref, int argc)
{
	return unit_conversion(ref, "to_distance", argc, _to_distance);
}

/*
 * Function:	count(vector:v)
 * 		count(vectorlist:v)
 * 		count(string:v)
 * Input:	vector|vectorlist|string:v entity to count content for
 * Output:	scalar:number of entries
 *
 * Returns the count of:
 * - coordinates in a vector
 * - vectors in a vectorlist
 * - bytes in a string
 */
static value_t *bi_count(const node_t *ref, int argc)
{
	const value_t *arg;
	value_t *v;
	check_arg_one(ref, "count", argc);
	arg = value_get(argc, 0);
	v = value_new(VAL_INT);
	v->unit = UNIT_NONE;
	switch(arg->type) {
	case VAL_VECTOR:	v->i = arg->v.n; break;
	case VAL_VECTORLIST:	v->i = arg->vl.n; break;
	case VAL_STRING:	v->i = arg->str.n; break;
	case VAL_UNDEF:
	case VAL_INT:
	case VAL_FLOAT:
		rterror(ref, "count() can only count vectors, vectorlists and strings");
		break;
	case VAL_REF:
		rtinternal(ref, "count(): value passed by reference");
		break;
	}
	return v;
}

/*
 * Function:	comment(...)
 * 		literal(...)
 * 		error(...)
 * 		warning(...)
 * 		message(...)
 * Input:	any list of arguments of any type
 * Output:	<undef>
 *
 * Outputs a string, representing the arguments, to the output or console.
 */
static void printvector(FILE *fp, const node_t *ref, const value_t *v)
{
	int i;
	assert(v->type == VAL_VECTOR);
	fprintf(fp, "[");
	for(i = 0; i < v->v.n; i++) {
		if(i)
			fprintf(fp, ",");
		switch(v->v.vals[i]->type) {
		case VAL_UNDEF:		fprintf(fp, "-"); break;
		case VAL_INT:		fprintf(fp, "%d%s", v->v.vals[i]->i, unitshortname(v->v.vals[i]->unit)); break;
		case VAL_FLOAT:		fprintf(fp, "%.*f%s", cl_decimals, v->v.vals[i]->d, unitshortname(v->v.vals[i]->unit)); break;
		default:		rtinternal(ref, "Invalid value type %d in vector printing", v->v.vals[i]->type);
		}
	}
	fprintf(fp, "]");
}

static value_t *printfunc(const node_t *ref, int argc, const char *pfx, FILE *fp, int forcenl)
{
	int i, j;
	int seennl = 0;

	if(pfx)
		fprintf(fp, "%s:%d:%d: %s: ", ref->filename, ref->linenr, ref->charnr, pfx);

	for(i = 0; i < argc; i++) {
		const value_t *v = value_get(argc, i);
		if(i == argc-1 && v->type == VAL_STRING && v->str.chs[0] && v->str.chs[v->str.n-1] == '\n')
			seennl = 1;
		switch(v->type) {
		case VAL_UNDEF:		fprintf(fp, "<undef>"); break;
		case VAL_INT:		fprintf(fp, "%d%s", v->i, unitshortname(v->unit)); break;
		case VAL_FLOAT:		fprintf(fp, "%.*f%s", cl_decimals, v->d, unitshortname(v->unit)); break;
		case VAL_STRING:	fprintf(fp, "%s", ucs32_to_utf8(v->str.chs, v->str.n)); break;
		case VAL_VECTOR:	printvector(fp, ref, v); break;
		case VAL_VECTORLIST:
			fprintf(fp, "{");
			for(j = 0; j < v->vl.n; j++) {
				if(j)
					fprintf(fp, ",");
				printvector(fp, ref, v->vl.vecs[j]);
			}
			fprintf(fp, "}");
			break;
		case VAL_REF:
			rtinternal(ref, "printfunc(): (pfx='%s'): value passed by reference", pfx);
			break;
		}
	}

	if(forcenl && !seennl)
		fprintf(fp, "\n");
	return value_new(VAL_UNDEF);
}

static value_t *bi_comment(const node_t *ref, int argc)
{
	printf("(");
	value_t *v = printfunc(ref, argc, NULL, stdout, 0);
	printf(")\n");
	return v;
}

static value_t *bi_literal(const node_t *ref, int argc)
{
	return printfunc(ref, argc, NULL, stdout, 0);
}

static value_t *bi_error(const node_t *ref, int argc)
{
	runtimeerrors++;
	return printfunc(ref, argc, "Runtime error()", stderr, 1);
}

static value_t *bi_warning(const node_t *ref, int argc)
{
	runtimewarnings++;
	return printfunc(ref, argc, "Runtime warning()", stderr, 1);
}

static value_t *bi_message(const node_t *ref, int argc)
{
	return printfunc(ref, argc, "Runtime message()", stderr, 1);
}

/*
 * Function	to_string(...)
 * Input:	any list of arguments of any type
 * Output:	string:printed result of arguments
 *
 * Returns the string representation of the arguments.
 */
static value_t *tostrfunc(const node_t *ref, int argc, const char *pfx, int forcenl)
{
	int i;
	int seennl = 0;
	wchar_t *s = wcsdup(L"");
	size_t ns = 0;
	value_t *rv;

	assert(s != NULL);

	if(pfx)
		mysprintf(&s, &ns, L"%s:%d:%d: %s: ", ref->filename, ref->linenr, ref->charnr, pfx);

	for(i = 0; i < argc; i++) {
		const value_t *v = value_get(argc, i);
		value_t *sv = value_to_string(ref, v);
		assert(sv->type == VAL_STRING);
		if(i == argc-1 && sv->str.chs[0] && sv->str.chs[sv->str.n-1] == L'\n')
			seennl = 1;
		mysprintf(&s, &ns, L"%ls", sv->str.chs);
	}

	if(forcenl && !seennl)
		mysprintf(&s, &ns, L"\n");
	rv = value_new(VAL_STRING);
	rv->str.n = ns;
	rv->str.na = ns+1;
	rv->str.chs = s;
	return rv;
}

static value_t *bi_to_string(const node_t *ref, int argc)
{
	return tostrfunc(ref, argc, NULL, 0);
}

/*
 * Function:	normalize(vector:v)
 * Input:	vector:v - input vector
 * Output:	vector
 *
 * Returns the scaled version of arg such that length(normalize(v)) equals 1.0.
 * The resulting normalized vector has no units associated.
 */
static value_t *bi_normalize(const node_t *ref, int argc)
{
	int i;
	const value_t *arg;
	value_t *v;
	double sum = 0.0;
	check_arg_one(ref, "normalize", argc);
	arg = value_get(argc, 0);
	check_arg_vector(ref, "normalize", arg, 0);

	/* Units in normalize() are not important as long as they are the same for all */
	for(i = 0; i < arg->v.n; i++) {
		double d = value_to_mm(arg->v.vals[i]);
		check_arg_unit_distance(ref, "normalize", arg->v.vals[i], 0);
		sum += d*d;
	}
	sum = sqrt(sum);
	v = value_new(VAL_VECTOR);
	for(i = 0; i < arg->v.n; i++) {
		switch(arg->v.vals[i]->type) {
		case VAL_UNDEF:
			value_vector_add_undef(v);
			break;
		case VAL_INT:
		case VAL_FLOAT:
			value_vector_add(v, value_new_flt(value_to_mm(arg->v.vals[i]) / sum, UNIT_NONE));
			break;
		default:
			rtinternal(ref, "Vector value contains non-scalar elements");
			break;
		}
		
	}
	return v;
}

/*
 * Function:	length(vector:v)
 * Input:	vector:v - Vector for length determination
 * Output:	scalar: length of v
 *
 * Returns the length of v calculated as sqrt(v * v).
 */
static value_t *bi_length(const node_t *ref, int argc)
{
	int i;
	const value_t *arg;
	double sum = 0.0;
	unit_et u = UNIT_NONE;
	check_arg_one(ref, "length", argc);
	arg = value_get(argc, 0);
	check_arg_vector(ref, "length", arg, 0);

	for(i = 0; i < arg->v.n; i++) {
		if(arg->v.vals[i]->type == VAL_UNDEF)
			continue;
		assert(isscalar(arg->v.vals[i]));
		if(u == UNIT_NONE) {
			if(isdistance(arg->v.vals[i]))
				u = UNIT_MM;
			else if(isangle(arg->v.vals[i]))
				u = UNIT_DEG;
		} else if(((u == UNIT_MM || u == UNIT_IN) && !isdistance(arg->v.vals[i])) || (u == UNIT_DEG && !isangle(arg->v.vals[i]))) {
			rtwarning(ref, "Mix of distance and angular units in vector in length() operation");
			break;
		}
	}
	for(i = 0; i < arg->v.n; i++) {
		double d;
		switch(u) {
		case UNIT_MM:
			d = value_to_mm(arg->v.vals[i]);
			break;
		case UNIT_IN:
			d = value_to_in(arg->v.vals[i]);
			break;
		case UNIT_DEG:
			d = value_to_deg(arg->v.vals[i]);
			break;
		case UNIT_NONE:
		default:
			d = value_to_double(arg->v.vals[i]);
			break;
		}
		sum += d*d;
	}
	return value_new_flt(sqrt(sum), u);
}

/*
 * Function:	rotate_xy(vector:v, scalar:a)
 * 		rotate_xy(vectorlist:v, scalar:angle)
 * 		rotate_xz(vector:v, scalar:angle)
 * 		rotate_xz(vectorlist:v, scalar:angle)
 * 		rotate_yz(vector:v, scalar:angle)
 * 		rotate_yz(vectorlist:v, scalar:angle)
 * Input:	vector|vectorlist:v - argument to rotate
 * 		scalar:a - angle of rotation
 * Output:	vector or vectorlist rotated by angle a
 *
 * Returns v rotated by angle a. The plane of rotation is XY, XZ or YZ for
 * respective functions. If v is of type vectorlist, then each containing
 * vector is rotated and a vectorlist is returned.
 */
typedef enum {
	FN_rotate_xy,
	FN_rotate_xz,
	FN_rotate_yz,
} rot_et;

static inline double getcoord(const value_t *vec, int coord, unit_et u)
{
	if(coord >= vec->v.n)
		return 0.0;
	switch(u) {
	case UNIT_MM:	return value_to_mm(vec->v.vals[coord]);
	case UNIT_IN:	return value_to_in(vec->v.vals[coord]);
	default:	return value_to_double(vec->v.vals[coord]);
	}
}

static inline unit_et getunit(const node_t *ref, const value_t *vec, int coord, const char *fn)
{
	const char ax[3] = "XYZ";
	assert(coord < 3);
	if(coord >= vec->v.n || vec->v.vals[coord]->type == VAL_UNDEF)
		return UNIT_NONE;
	if(!isdistance(vec->v.vals[coord]) && !isnone(vec->v.vals[coord])) {
		rtwarning(ref, "%s() Vector coordinate %d (%c) is not a distance", fn, coord, ax[coord]);
		return UNIT_NONE;
	}
	return vec->v.vals[coord]->unit;
}

static value_t *rotate_vec(const node_t  *ref, const value_t *vec, const value_t *ang, const char *fn, rot_et ft)
{
	double x, y, phi;
	double xr, yr;
	unit_et ux, uy, ut;
	value_t *v = value_dup(vec);
	assert(isvector(vec));
	assert(isscalar(ang));

	phi = value_to_rad(ang);
	if(vec->v.n < 1)
		rtwarning(ref, "%s() Vector missing all coordinates to rotate", fn);

	switch(ft) {
	case FN_rotate_xy:
		while(v->v.n < 2)
			value_vector_add_undef(v);
		ux = getunit(ref, vec, 0, fn);
		uy = getunit(ref, vec, 1, fn);
		if(ux != uy)
			rtwarning(ref, "%s() Vector coordinates for X and Y have different units (X=%s, Y=%s)", fn, unitname(ux), unitname(uy));
		if(ux == uy || uy == UNIT_NONE)
			ut = ux;
		else
			ut = uy;
		x = getcoord(vec, 0, ut);
		y = getcoord(vec, 1, ut);
		xr = x * cos(phi) - y * sin(phi);
		yr = x * sin(phi) + y * cos(phi);
		v->v.vals[0]->type = VAL_FLOAT;
		v->v.vals[0]->unit = ut;
		v->v.vals[0]->d = xr;
		v->v.vals[1]->type = VAL_FLOAT;
		v->v.vals[1]->unit = ut;
		v->v.vals[1]->d = yr;
		break;
	case FN_rotate_xz:
		while(v->v.n < 3)
			value_vector_add_undef(v);
		ux = getunit(ref, vec, 0, fn);
		uy = getunit(ref, vec, 2, fn);
		if(ux != uy)
			rtwarning(ref, "%s() Vector coordinates for X and Z have different units (X=%s, Z=%s)", fn, unitname(ux), unitname(uy));
		if(ux == uy || uy == UNIT_NONE)
			ut = ux;
		else
			ut = uy;
		x = getcoord(vec, 0, ut);
		y = getcoord(vec, 2, ut);
		xr = x * cos(phi) - y * sin(phi);
		yr = x * sin(phi) + y * cos(phi);
		v->v.vals[0]->type = VAL_FLOAT;
		v->v.vals[0]->unit = ut;
		v->v.vals[0]->d = xr;
		v->v.vals[2]->type = VAL_FLOAT;
		v->v.vals[2]->unit = ut;
		v->v.vals[2]->d = yr;
		break;
	case FN_rotate_yz:
		while(v->v.n < 3)
			value_vector_add_undef(v);
		ux = getunit(ref, vec, 1, fn);
		uy = getunit(ref, vec, 2, fn);
		if(ux != uy)
			rtwarning(ref, "%s() Vector coordinates for Y and Z have different units (Y=%s, Z=%s)", fn, unitname(ux), unitname(uy));
		if(ux == uy || uy == UNIT_NONE)
			ut = ux;
		else
			ut = uy;
		x = getcoord(vec, 1, ut);
		y = getcoord(vec, 2, ut);
		xr = x * cos(phi) - y * sin(phi);
		yr = x * sin(phi) + y * cos(phi);
		v->v.vals[1]->type = VAL_FLOAT;
		v->v.vals[1]->unit = ut;
		v->v.vals[1]->d = xr;
		v->v.vals[2]->type = VAL_FLOAT;
		v->v.vals[2]->unit = ut;
		v->v.vals[2]->d = yr;
		break;
	default:
		rtinternal(ref, "Invalid target function reference %d in rotate_vec()", ft);
	}
	return v;
}

static value_t *rotateany(const node_t *ref, int argc, const char *fn, rot_et ft)
{
	const value_t *arg0;
	const value_t *arg1;
	check_arg_two(ref, fn, argc);
	arg0 = value_get(argc, 0);
	arg1 = value_get(argc, 1);
	if(arg0->type != VAL_VECTOR && arg0->type != VAL_VECTORLIST) {
		rterror(ref, "%s() argument 1 must be vector or vectorlist", fn);
		return value_new(VAL_UNDEF);
	}
	check_arg_scalar(ref, fn, arg1, 1);
	check_arg_unit_angle(ref, fn, arg1, 1);

	if(arg0->type == VAL_VECTOR)
		return rotate_vec(ref, arg0, arg1, fn, ft);
	else {
		int i;
		value_t *v = value_new(VAL_VECTORLIST);
		for(i = 0; i < arg0->vl.n; i++)
			value_vectorlist_add(v, rotate_vec(ref, arg0->vl.vecs[i], arg1, fn, ft));
		return v;
	}
}

static value_t *bi_rotate_xy(const node_t *ref, int argc)
{
	return rotateany(ref, argc, "rotate_xy", FN_rotate_xy);
}

static value_t *bi_rotate_xz(const node_t *ref, int argc)
{
	return rotateany(ref, argc, "rotate_xz", FN_rotate_xz);
}

static value_t *bi_rotate_yz(const node_t *ref, int argc)
{
	return rotateany(ref, argc, "rotate_yz", FN_rotate_yz);
}

/*
 * Function:	isfloat(x)
 * 		isint(x)
 * 		isscalar(x)
 * 		isstring(x)
 * 		isundef(x)
 * 		isvector(x)
 * 		isvectorlist(x)
 * Input:	any type
 * Output:	boolean true(1) if the type matched or false(0) if not
 *
 * These functions return an integer (boolean) which is true (1) if arg is of
 * tested type and false (0) otherwise.
 */
typedef enum {
	FN_isfloat,
	FN_isint,
	FN_isscalar,
	FN_isstring,
	FN_isundef,
	FN_isvector,
	FN_isvectorlist,
} boolfunc_et;

#define BOOLFUNC(f)	static value_t *bi_ ## f(const node_t *ref, int argc) { return boolfuncs(ref, argc, #f, FN_ ## f); }

static value_t *boolfuncs(const node_t *ref, int argc, const char *fn, boolfunc_et ft)
{
	const value_t *arg;
	value_t *v = value_new(VAL_INT);
	check_arg_one(ref, fn, argc);
	arg = value_get(argc, 0);
	switch(ft) {
	case FN_isfloat:	v->i = arg->type == VAL_FLOAT; break;
	case FN_isint:		v->i = arg->type == VAL_INT; break;
	case FN_isscalar:	v->i = arg->type == VAL_FLOAT || arg->type == VAL_INT; break;
	case FN_isstring:	v->i = arg->type == VAL_STRING; break;
	case FN_isundef:	v->i = arg->type == VAL_UNDEF; break;
	case FN_isvector:	v->i = arg->type == VAL_VECTOR; break;
	case FN_isvectorlist:	v->i = arg->type == VAL_VECTORLIST; break;
	}
	return v;
}

BOOLFUNC(isfloat)
BOOLFUNC(isint)
BOOLFUNC(isscalar)
BOOLFUNC(isstring)
BOOLFUNC(isundef)
BOOLFUNC(isvector)
BOOLFUNC(isvectorlist)

/*
 * Function:	isnone(scalar:x)
 * 		isangle(scalar:x)
 * 		isdistance(scalar:x)
 * 		isdeg(scalar:x)
 * 		israd(scalar:x)
 * 		ismm(scalar:x)
 * 		isinch(scalar:x)
 * Input:	scalar:x - input for unit test
 * Output:	boolean true(1) if the units test is matched or false(0) if not
 *
 * These functions return an integer (boolean) which is true (1) if arg has the
 * tested units and false (0) otherwise.
 * Function isangle(arg) equals "israd(arg) || isdeg(arg)".
 * Function isdistance(arg) equals "ismm(arg) || isinch(arg)".
 */
typedef enum {
	FN_isnone,
	FN_isangle,
	FN_isdistance,
	FN_isdeg,
	FN_israd,
	FN_ismm,
	FN_isinch,
} unitfunc_et;

static value_t *unitfuncs(const node_t *ref, int argc, const char *fn, unitfunc_et ft)
{
	const value_t *arg;
	value_t *v = value_new(VAL_INT);
	check_arg_one(ref, fn, argc);
	arg = value_get(argc, 0);
	check_arg_scalar(ref, fn, arg, 0);
	if(isscalar(arg)) {
		switch(ft) {
		case FN_isnone:		v->i = arg->unit == UNIT_NONE; break;
		case FN_isangle:	v->i = arg->unit == UNIT_NONE || arg->unit == UNIT_DEG || arg->unit == UNIT_RAD; break;
		case FN_isdistance:	v->i = arg->unit == UNIT_NONE || arg->unit == UNIT_MM || arg->unit == UNIT_IN; break;
		case FN_isdeg:		v->i = arg->unit == UNIT_DEG; break;
		case FN_israd:		v->i = arg->unit == UNIT_RAD; break;
		case FN_ismm:		v->i = arg->unit == UNIT_MM; break;
		case FN_isinch:		v->i = arg->unit == UNIT_IN; break;
		}
	} else
		v->i = 0;
	return v;
}

#define UNITFUNC(f)	static value_t *bi_ ## f(const node_t *ref, int argc) { return unitfuncs(ref, argc, #f, FN_ ## f); }

UNITFUNC(isnone)
UNITFUNC(isangle)
UNITFUNC(isdistance)
UNITFUNC(isdeg)
UNITFUNC(israd)
UNITFUNC(ismm)
UNITFUNC(isinch)


/*
 * Function:	head(vector:arg, scalar:nr)
 * 		head(vectorlist:arg, scalar:nr)
 * 		head(string:arg, scalar:nr)
 * Input:	vector|vectorlist|string:arg - source
 * 		scalar:nr - number of elements requested
 * Output:	number of elements from source requested
 *
 * Returns the first nr number of entries from arg as a vector, vectorlist or
 * string. If arg is a vector and has fewer than nr entries, then the result
 * is padded with undef until the result has nr entries.
 *
 * If arg is a vectorlist or string and has fewer than nr entries, then only
 * the available entries are returned.
 * If nr is negative then a vector/vectorlist/string is returned with |nr|
 * fewer entries than count(arg).
 */
static value_t *bi_head(const node_t *ref, int argc)
{
	int i;
	const value_t *arg0;
	const value_t *arg1;
	value_t *v;
	int count;
	check_arg_two(ref, "head", argc);
	arg0 = value_get(argc, 0);
	if(!isvector(arg0) && !isvectorlist(arg0) && !isstring(arg0)) {
		rterror(ref, "head() argument 0 must be vector, vectorlist or string");
		return value_new(VAL_UNDEF);
	}
	arg1 = value_get(argc, 1);
	check_arg_scalar(ref, "head", arg1, 1);
	check_arg_unit_none(ref, "head", arg1, 1);
	count = value_to_int(arg1);

	if(count < 0) {
		if(isvector(arg0))
			count = arg0->v.n + count;
		else if(isvectorlist(arg0))
			count = arg0->vl.n + count;
		else
			count = arg0->str.n + count;
		if(count < 0)
			count = 0;
	}

	v = value_new(arg0->type);

	/* Zero count --> empty */
	/* However, strings are empty with a zero termination */

	if(isvector(arg0)) {
		for(i = 0; i < count && i < arg0->v.n; i++)
			value_vector_add(v, value_dup(arg0->v.vals[i]));
		while(count < v->v.n)
			value_vector_add_undef(v);
	} else if(isvectorlist(arg0)) {
		for(i = 0; i < count && i < arg0->vl.n; i++)
			value_vectorlist_add(v, value_dup(arg0->vl.vecs[i]));
	} else {
		/* String head */
		if(arg0->str.n < count)
			count = arg0->str.n;	/* Cannot have more than source */
		v->str.n = count;
		v->str.na = count + 1;		/* Implicit zero terminate */
		v->str.chs = calloc(v->str.na, sizeof(v->str.chs[0]));
		assert(v->str.chs != NULL);
		memcpy(v->str.chs, arg0->str.chs, count * sizeof(v->str.chs[0]));
	}
	return v;
}

/*
 * Function:	tail(vector:arg, scalar:nr)
 * 		tail(vectorlist:arg, scalar:nr)
 * 		tail(string:arg, scalar:nr)
 * Input:	vector|vectorlist|string:arg - source
 * 		scalar:nr - number of elements requested
 * Output:	number of elements from source requested
 *
 * Returns the last nr number of entries from arg as a vector, vectorlist or
 * string. If arg is a vector and has fewer than nr entries, then the result is
 * prepended with undef until the result has nr entries.
 *
 * If arg is a vectorlist or string and has fewer than nr entries, then only
 * the available entries are returned.
 * If nr is negative then a vector/vectorlist/string is returned with |nr|
 * fewer entries than count(arg).
 */
static value_t *bi_tail(const node_t *ref, int argc)
{
	int i;
	const value_t *arg0;
	const value_t *arg1;
	value_t *v;
	int count;
	check_arg_two(ref, "tail", argc);
	arg0 = value_get(argc, 0);
	if(!isvector(arg0) && !isvectorlist(arg0) && !isstring(arg0)) {
		rterror(ref, "tail() argument 0 must be vector, vectorlist or string");
		return value_new(VAL_UNDEF);
	}
	arg1 = value_get(argc, 1);
	check_arg_scalar(ref, "tail", arg1, 1);
	check_arg_unit_none(ref, "tail", arg1, 1);
	count = value_to_int(arg1);

	if(count < 0) {
		if(isvector(arg0))
			count = arg0->v.n + count;
		else if(isvectorlist(arg0))
			count = arg0->vl.n + count;
		else
			count = arg0->str.n + count;
		if(count < 0)
			count = 0;
	}

	v = value_new(arg0->type);

	/* Zero count --> empty */
	/* However, strings are empty with a zero termination */

	if(isvector(arg0)) {
		int n = arg0->v.n;
		while(n < count) {
			value_vector_add_undef(v);
			count--;
		}
		i = n > count ? n - count : 0;
		for(; count && i < n; i++, count--)
			value_vector_add(v, value_dup(arg0->v.vals[i]));
	} else if(isvectorlist(arg0)) {
		int n = arg0->vl.n;
		i = n > count ? n - count : 0;
		for(; count && i < n; i++, count--)
			value_vectorlist_add(v, value_dup(arg0->vl.vecs[i]));
	} else {
		/* String tail */
		if(arg0->str.n < count)
			count = arg0->str.n;	/* Cannot have more than source */
		v->str.n = count;
		v->str.na = count + 1;		/* Implicit zero terminate */
		v->str.chs = calloc(v->str.na, sizeof(v->str.chs[0]));
		assert(v->str.chs != NULL);
		memcpy(v->str.chs, arg0->str.chs + (arg0->str.n - count), count * sizeof(v->str.chs[0]));
	}
	return v;
}

/*
 * Function:	delete(vector:arg, scalar:idx [, scalar:cnt])
 * 		delete(vectorlist:arg, scalar:idx [, scalar:cnt])
 * 		delete(string:arg, scalar:idx [, scalar:cnt])
 * Input:	vector|vectorlist|string:arg - source
 * 		scalar:idx - position to delete from
 * 		scalar:cnt - number of elements to delete
 * Output:	source with elements deleted
 *
 * Returns arg with the entry idx removed from the vector, vectorlist or
 * string. The second form removes cnt entries. Argument idx may be positive to
 * remove at the position counting from the start, or negative to start
 * counting from the end (with -1 being the last entry of the
 * vector/vectorlist/string). Argument cnt must be larger or equal to zero.
 */
static value_t *bi_delete(const node_t *ref, int argc)
{
	int i, n;
	const value_t *arg0;
	const value_t *arg1;
	value_t *v;
	int idx;
	int count = 1;
	check_arg_two(ref, "delete", argc);
	arg0 = value_get(argc, 0);
	if(!isvector(arg0) && !isvectorlist(arg0) && !isstring(arg0)) {
		rterror(ref, "delete() argument 0 must be vector, vectorlist or string");
		return value_new(VAL_UNDEF);
	}
	arg1 = value_get(argc, 1);
	check_arg_scalar(ref, "delete", arg1, 1);
	check_arg_unit_none(ref, "delete", arg1, 1);
	idx = value_to_int(arg1);

	if(argc > 2) {
		const value_t *arg2 = value_get(argc, 2);
		check_arg_scalar(ref, "delete", arg2, 2);
		check_arg_unit_none(ref, "delete", arg2, 2);
		count = value_to_int(arg2);
	}

	if(isvector(arg0))
		n = arg0->v.n;
	else if(isvectorlist(arg0))
		n = arg0->vl.n;
	else
		n = arg0->str.n;
	v = value_new(arg0->type);

	if(!n) {
		rtwarning(ref, "delete(): argument 0 %s has no entries, cannot delete", isvector(arg0) ? "vector" : (isvectorlist(arg0) ? "vectorlist" : "string"));
		if(isstring(arg0)) {
			/*  Empty string is zero terminated */
			v->str.n = 0;
			v->str.na = 1;
			v->str.chs = calloc(v->str.na, sizeof(v->str.chs[0]));
			assert(v->str.chs != NULL);
		}
		return v;
	}

	if((idx < 0  && idx + n < 0) || idx >= n) {
		rterror(ref, "delete(): argument 1 index %d out of range [%d..%d]", idx, -n, n-1);
		return v;
	}

	if(count < 0 || count > n) {
		rterror(ref, "delete(): argument 2 count %d out of range [0..%d]", count, n);
		return v;
	}

	if(idx < 0)
		idx += n;
	assert(idx >= 0);

	if(idx + count > n) {
		rtwarning(ref, "delete(): argument 2 count is larger than remaining entries (%d > %d)", idx + count, n);
		count = n - idx;	/* Prevent buffer overrun in string copy */
	}

	if(!isstring(arg0)) {
		for(i = 0; i < n; i++) {
			if(count && i >= idx) {	// Skip indices to delete
				i += count - 1;
				count = 0;
				continue;
			}
			if(isvector(arg0))
				value_vector_add(v, value_dup(arg0->v.vals[i]));
			else
				value_vectorlist_add(v, value_dup(arg0->vl.vecs[i]));
		}
	} else {
		v->str.n = 0;
		v->str.na = arg0->str.n - count + 1;
		v->str.chs = calloc(v->str.na, sizeof(v->str.chs[0]));
		assert(v->str.chs != NULL);
		if(idx > 0) {
			/* Copy what is at the head */
			memcpy(v->str.chs, arg0->str.chs, idx * sizeof(v->str.chs[0]));
			v->str.n += idx;
		}
		if(idx + count < n) {
			/* Copy what is at the tail */
			memcpy(v->str.chs + v->str.n, arg0->str.chs + (idx + count), (n - idx - count) * sizeof(v->str.chs[0]));
			v->str.n += n - idx - count;
		}
	}
	return v;
}

/*
 * Function:	insert(vector:arg, scalar:val, scalar:idx)
 * 		insert(vector:arg, vector:val, scalar:idx)
 * 		insert(vectorlist:arg, vector:val, scalar:idx)
 * 		insert(vectorlist:arg, vectorlist:val, scalar:idx)
 * 		insert(string:arg, string:val, scalar:idx)
 * Input:	vector|vectorlist|string:arg - source
 * 		scalar|vector|vectorlist|string:val - element(s) to insert
 * 		scalar:idx - position to insert
 * Output:	source vector, vectorlist or stringwith element(s) inserted
 *
 * Returns arg with value val inserted into the vector/vectorlist/string at
 * position idx. Inserting a scalar val into a vector or vector val into a
 * vectorlist inserts one single entry. Inserting a vector val into a vector or
 * vectorlist val into a vectorlist inserts all of the contained entries from
 * val.
 *
 * The idx argument specifies the location of insertion with following meaning
 * (with n = count(arg)):
 * idx	=	-n-1	prepend
 * idx	>=	-n	insert
 * idx	=	-1	append
 * idx	=	0	prepend
 * idx	<=	n-1	insert
 * idx	= 	n	append
 *
 * Inserting an empty vector into a vector or an empty vectorlist into a
 * vectorlist has no effect and the result is equal arg.
 */
static value_t *bi_insert(const node_t *ref, int argc)
{
	int i, n;
	const value_t *arg0;
	const value_t *arg1;
	const value_t *arg2;
	value_t *v;
	int idx;
	check_arg_three(ref, "insert", argc);
	arg0 = value_get(argc, 0);
	if(!isvector(arg0) && !isvectorlist(arg0) && !isstring(arg0)) {
		rterror(ref, "insert() argument 0 must be vector, vectorlist or string");
		return value_new(VAL_UNDEF);
	}

	v = value_dup(arg0);

	arg1 = value_get(argc, 1);
	if(isvector(arg0)) {
		if(!isscalar(arg1) && !isvector(arg1)) {
			rterror(ref, "insert() argument 1 must be scalar or vector for argument 0 as vector");
			return v;
		}
		if(isscalar(arg1))
			check_arg_unit_none(ref, "insert", arg1, 1);
	} else if(isvectorlist(arg0)) {
		if(!isvector(arg1) && !isvectorlist(arg1)) {
			rterror(ref, "insert() argument 1 must be vector or vectorlist for argument 0 as vectorlist");
			return v;
		}
	} else {
		if(!isstring(arg1)) {
			rterror(ref, "insert() argument 1 must be string for argument 0 as string");
			return v;
		}
	}

	arg2 = value_get(argc, 2);
	check_arg_scalar(ref, "insert", arg2, 2);
	check_arg_unit_none(ref, "insert", arg2, 2);
	idx = value_to_int(arg2);

	if(isvector(arg0))
		n = arg0->v.n;
	else if(isvectorlist(arg0))
		n = arg0->vl.n;
	else
		n = arg0->str.n;

	/*
	 * Valid idx ranges: -n-1..n
	 *
	 * Meaning:
	 * - idx == -n-1	prepend	(idx --> idx + n + 1 = -n-1 + n + 1 = 0)
	 * - idx >= -n		insert
	 * - idx == -1		append	(idx --> idx + n + 1 = -1 + n + 1 = n)
	 * - idx ==  0		prepend
	 * - idx <=  n-1	insert
	 * - idx ==  n		append
	 */
	if(idx < -n-1 || idx > n) {
		rterror(ref, "insert(): argument 2 index %d out of range [%d..%d]", idx, -n-1, n);
		return v;
	}

	if(idx < 0)
		idx += n+1;

	if(isvector(arg0)) {
		if(isscalar(arg1)) {
			/* Insert a scalar into a vector */
			value_vector_add(v, value_dup(arg1));
			if(idx < n) {
				/* Add needed to be somewhere in the middle */
				value_t *val = v->v.vals[v->v.n-1];
				memmove(&v->v.vals[idx+1], &v->v.vals[idx], (v->v.n - idx - 1) * sizeof(v->v.vals[0]));
				v->v.vals[idx] = val;
			}
		} else if(arg1->v.n) {
			/* Insert a vector's entries into a vector */
			for(i = 0; i < arg1->v.n; i++)
				value_vector_add(v, value_dup(arg1->v.vals[i]));
			if(idx < n) {
				/* Add needed to be somewhere in the middle */
				value_t **lst = calloc(arg1->v.n, sizeof(arg1->v.vals[0]));
				assert(lst != NULL);
				memcpy(lst, &v->v.vals[n], arg1->v.n * sizeof(v->v.vals[0]));
				memmove(&v->v.vals[idx+arg1->v.n], &v->v.vals[idx], (v->v.n - arg1->v.n) * sizeof(v->v.vals[0]));
				memcpy(&v->v.vals[idx], lst, arg1->v.n * sizeof(v->v.vals[0]));
				free(lst);
			}
		}
	} else if(isvectorlist(arg0)) {
		if(isvector(arg1)) {
			/* Insert a scalar into a vector */
			value_vectorlist_add(v, value_dup(arg1));
			if(idx < n) {
				/* Add needed to be somewhere in the middle */
				value_t *val = v->vl.vecs[v->vl.n-1];
				memmove(&v->vl.vecs[idx+1], &v->vl.vecs[idx], (v->vl.n - idx - 1) * sizeof(v->vl.vecs[0]));
				v->vl.vecs[idx] = val;
			}
		} else if(arg1->vl.n) {
			/* Insert a vector's entries into a vector */
			for(i = 0; i < arg1->vl.n; i++)
				value_vectorlist_add(v, value_dup(arg1->vl.vecs[i]));
			if(idx < n) {
				/* Add needed to be somewhere in the middle */
				value_t **lst = calloc(arg1->vl.n, sizeof(arg1->vl.vecs[0]));
				assert(lst != NULL);
				memcpy(lst, &v->vl.vecs[n], arg1->vl.n * sizeof(v->vl.vecs[0]));
				memmove(&v->vl.vecs[idx+arg1->vl.n], &v->vl.vecs[idx], (v->vl.n - arg1->vl.n) * sizeof(v->vl.vecs[0]));
				memcpy(&v->vl.vecs[idx], lst, arg1->vl.n * sizeof(v->vl.vecs[0]));
				free(lst);
			}
		}
	} else {
		if(arg1->str.n) {
			/* Only work if the second argument actually has characters */
			free(v->str.chs);	/* The dup'ed arg0 is too small, recreate at correct size */
			v->str.n = arg0->str.n + arg1->str.n;
			v->str.na = v->str.n + 1;
			v->str.chs = calloc(v->str.na, sizeof(arg0->str.chs[0]));
			assert(v->str.chs != NULL);
			if(!idx) {
				/* Prepend */
				memcpy(v->str.chs, arg1->str.chs, arg1->str.n * sizeof(v->str.chs[0]));
				memcpy(v->str.chs + arg1->str.n, arg0->str.chs, arg0->str.n * sizeof(v->str.chs[0]));
			} else if(idx == arg0->str.n) {
				/* Append */
				memcpy(v->str.chs, arg0->str.chs, arg0->str.n * sizeof(v->str.chs[0]));
				memcpy(v->str.chs + arg0->str.n, arg1->str.chs, arg1->str.n * sizeof(v->str.chs[0]));
			} else {
				/* Insert somewhere */
				memcpy(v->str.chs, arg0->str.chs, idx * sizeof(v->str.chs[0]));
				memcpy(v->str.chs + idx, arg1->str.chs, arg1->str.n * sizeof(v->str.chs[0]));
				memcpy(v->str.chs + idx + arg1->str.n, arg0->str.chs + idx, (arg0->str.n - idx) * sizeof(v->str.chs[0]));
			}
		}
	}

	return v;
}

/*
 * Function:	reverse(vector:v)
 * 		reverse(vectorlist:v)
 * 		reverse(tring:v)
 * Input:	vector|vectorlist|string:v input coordinates, vectors or characters
 * Output:	same type with all coordinates, vectors or characters in
 *		reverse order
 *
 * Returns the reversed version of v such that all coordinates of a vector,
 * vectors of a vectorlist or characters in a string are reversed. I.e. the
 * first becomes the last, etc..
 */
static value_t *bi_reverse(const node_t *ref, int argc)
{
	int i;
	const value_t *arg;
	value_t *v;
	check_arg_one(ref, "reverse", argc);
	arg = value_get(argc, 0);
	if(!isvector(arg) && !isvectorlist(arg) && !isstring(arg)) {
		rterror(ref, "reverse() argument must be vector, vectorlist or string");
		return value_new(VAL_UNDEF);
	}

	v = value_new(arg->type);

	if(isvector(arg)) {
		int n = arg->v.n;
		for(i = 0; i < n; i++)
			value_vector_add(v, value_dup(arg->v.vals[n-i-1]));
	} else if(isvectorlist(arg)) {
		int n = arg->vl.n;
		for(i = 0; i < n; i++)
			value_vectorlist_add(v, value_dup(arg->vl.vecs[n-i-1]));
	} else {
		int n = arg->str.n;
		v->str.n = n;
		v->str.na = n+1;
		v->str.chs = calloc(n, sizeof(v->str.chs[0]));
		assert(v->str.chs != NULL);
		for(i = 0; i < n; i++)
			v->str.chs[i] = arg->str.chs[n-i-1];
	}
	return v;
}

/*
 * Function:	plane(scalar:p)
 * 		plane(string:p)
 * Input:	scalar|string:p - plane selection
 * Output:	scaler: previously selected plane
 *
 * Set the current operational plane to mode or return the current active
 * plane. Argument mode may be one of the following:
 *
 * scalar	(value)	string	Description
 * PLANE_QUERY	(undef)		return current plane as integer
 * PLANE_XY	(0)	"XY"	set XY plane and return previous value (G17, default set in prologue)
 * PLANE_XZ	(1)	"XZ"	set XZ plane and return previous value (G18)
 * PLANE_YZ	(2)	"YZ"	set YZ plane and return previous value (G19)
 *
 * The previously active plane is returned if the plane is set. The current
 * plane of operation is returned as an integer if mode is undef.
 */

/*
 * Function:	scale(vector:v, vector:s)
 * 		scale(vectorlist:v, vector:s)
 * Input:	vector|vectorlist:v - input vector(s) to scale
 * 		vector:s - scaling vector
 * Output:	same as input but scaled by s
 *
 * Multiply v with coefficients from s. Where mult can be seen as the primary
 * diagonal of a multiplication matrix. If the first argument is of type
 * vectorlist, then each containing vector of v is iterated and scaled
 * separately.
 */
static value_t *vecscalemul(const node_t *ref, const value_t *lv, const value_t *rv)
{
	int i;
	value_t *v = value_new(VAL_VECTOR);
	for(i = 0; i < lv->v.n || i < rv->v.n; i++) {
		if(i < lv->v.n) {
			if(i < rv->v.n && !isundef(rv->v.vals[i])) {
				value_vector_add(v, op_mul(ref, value_dup(lv->v.vals[i]), rv->v.vals[i]));
			} else
				value_vector_add(v, value_dup(lv->v.vals[i]));
		} else
			value_vector_add(v, value_new(VAL_UNDEF));
	}
	return v;
}

static value_t *bi_scale(const node_t *ref, int argc)
{
	int i;
	const value_t *arg0;
	const value_t *arg1;
	check_arg_two(ref, "scale", argc);
	arg0 = value_get(argc, 0);
	if(!isvector(arg0) && !isvectorlist(arg0)) {
		rterror(ref, "scale() argument 1 must be vector or vectorlist");
		return value_new(VAL_UNDEF);
	}
	arg1 = value_get(argc, 1);
	check_arg_vector(ref, "scale", arg1, 1);

	if(isvector(arg0)) {
		return vecscalemul(ref, arg0, arg1);
	} else if(isvectorlist(arg0)) {
		value_t *v = value_new(VAL_VECTORLIST);
		for(i = 0; i < arg0->vl.n; i++)
			value_vectorlist_add(v, vecscalemul(ref, arg0->vl.vecs[i], arg1));
		return v;
	}
	return value_new(VAL_UNDEF);
}

/*
 * Function:	isgcode()
 * 		isdxf()
 * 		issvg()
 * Input:	-
 * Output:	scalar: true(1) or false(0)
 *
 * These function return an integer (boolean) which is true (1) is the current
 * output format is set as queried and false (0) otherwise.
 */





/*
 * Function:	ismodemm()
 * Input:	-
 * Output:	scalar: true(1) or false(0)
 *
 * Returns an integer (boolean) which is true (1) if gcmc is run in millimeter
 * mode and false (0) if run in inch mode (option -i/--imperial).
 */
static value_t *bi_ismodemm(const node_t *ref, int argc)
{
	value_t *v = value_new(VAL_INT);
	check_arg_none(ref, "ismodemm", argc);
	v->i = 1;
	return v;
}

/*
 * Function:	isrelative()
 * Input:	-
 * Output:	scalar: true(1) or false(0)
 *
 * Returns an integer (boolean) which is true (1) if the output is set to
 * relative mode (-r option) and false (0) otherwise.
 */

/*
 * Function:	isdefined(string:var)
 * Input:	string:var - variable to lookup
 * Output:	scalar: true(1) or false(0)
 *
 * Returns an integer (boolean) which is true (1) if a variable with name var
 * is currently defined in either local or global scope and false (0)
 * otherwise.
 */
static value_t *bi_isdefined(const node_t *ref, int argc)
{
	const value_t *arg;
	value_t *v = value_new(VAL_INT);
	check_arg_one(ref, "isdefined", argc);
	v->i = 0;
	arg = value_get(argc, 0);
	if(!isstring(arg)) {
		rterror(ref, "isdefined() argument must be string");
		return v;
	}
	if(variable_lookup(arg->str.chs, 0, 0))
		v->i = 1;
	return v;
}

/*
 * Function:	isconst(string:var)
 * Input:	string:var - variable to lookup
 * Output:	scalar: true(1) or false(0)
 *
 * Returns an integer (boolean) which is true (1) if a variable with name var
 * is constant and false (0) otherwise. The return value is undef if the
 * variable is not defined in local or global scope.
 */
static value_t *bi_isconst(const node_t *ref, int argc)
{
	const value_t *arg;
	const variable_t *var;
	value_t *v;
	check_arg_one(ref, "isconst", argc);
	arg = value_get(argc, 0);
	if(!isstring(arg)) {
		rterror(ref, "isconst() argument must be string");
		return value_new(VAL_UNDEF);
	}
	if(!(var = variable_lookup_deref(arg->str.chs, 0)))
		return value_new(VAL_UNDEF);
	v = value_new(VAL_INT);
	v->i = var->isconst ? 1 : 0;
	return v;
}

/*
 * Function:	fixpos_set(scalar:g28g30, vector:pos)
 * Input:	scalar:g28g30 - boolean
 * 		vector:pos - position to set
 * Output:	undef
 *
 * Set the position pos for use in use in G28 and G30. If g28g30 evaluates to
 * false then G28's position is set, otherwise G30's position is set.  This
 * function is an administrative function to allow fixpos_restore() to be
 * called without emitting a warning. The G28/G30 positions are often machine
 * dependent variables and gcmc has no means of knowing them unless set
 * explicitly.
 */
value_t *bi_fixpos_set(const node_t *ref, int argc)
{
	const value_t *arg0;
	const value_t *arg1;
	int b;
	int i;
	check_arg_two(ref, "fixpos_set", argc);
	arg0 = value_get(argc, 0);
	arg1 = value_get(argc, 1);
	check_arg_scalar(ref, "fixpos_set", arg0, 0);
	check_arg_unit_none(ref, "fixpos_set", arg0, 0);
	check_arg_vector(ref, "fixpos_set", arg1, 1);
	b = value_to_boolean(arg0);
	for(i = 0; i < naxes; i++) {
		double co = 0.0;
		if(i < arg1->v.n) {
			const value_t *v = arg1->v.vals[i];
			if(i < 3 || i >= 6) {
				if(v->unit != UNIT_NONE && v->unit != UNIT_MM && v->unit != UNIT_IN)
					rtwarning(ref, "fixpos_set() vector element %d has incompatible angular units attached", i+1);
				co = value_to_dist(v);
			} else {
				if(v->unit != UNIT_NONE && v->unit != UNIT_DEG && v->unit != UNIT_RAD)
					rtwarning(ref, "fixpos_set() vector element %d has incompatible distance units attached", i+1);
				co = value_to_rad(v);
			}
		}
		if(b)
			g30_pos[i] = co;
		else
			g28_pos[i] = co;
	}
	if(b)
		g30_valid = 1;
	else
		g28_valid = 1;
	return value_new(VAL_UNDEF);
}

/*
 * Function:	fixpos_store(scalar:g28g30)
 * Input:	scalar:g28g30 - boolean
 * Output:	undef
 *
 * Emit a G28.1 or G30.1 gcode (store current absolute position). If g28g30
 * evaluates to false then G28.1 is used, otherwise G30.1.
 */
value_t *bi_fixpos_store(const node_t *ref, int argc)
{
	const value_t *arg;
	int b;
	int i;
	check_arg_one(ref, "fixpos_store", argc);
	arg = value_get(argc, 0);
	check_arg_scalar(ref, "fixpos_store", arg, 0);
	check_arg_unit_none(ref, "fixpos_store", arg, 0);
	b = value_to_boolean(arg);
	if(b) {
		for(i = 0; i < naxes; i++)
			g30_pos[i] = *global_pos[i];
		g30_valid = 1;
	} else {
		for(i = 0; i < naxes; i++)
			g28_pos[i] = *global_pos[i];
		g28_valid = 1;
	}
	return value_new(VAL_UNDEF);
}

/*
 * Function:	fixpos_restore(scalar:g28g30 [, vector:rapid])
 * Input:	scalar:g28g30 - boolean
 * 		vector:rapid - pre-restore rapid movement
 * Output:	undef
 *
 * Emit a G28 or G30 gcode (goto predefined position). If g28g30 evaluates to
 * false then G28 is used, otherwise G30. The argument rapid defines a rapid
 * move in absolute coordinates to perform before the rapid move to the
 * indicated stored position is performed.
 * A warning is emitted if the position was never stored using fixpos_store or
 * set using fixpos_set before restored and gcmc's internally tracked position
 * is set to [0, 0, 0, 0, 0, 0, 0, 0, 0].
 */
value_t *bi_fixpos_restore(const node_t *ref, int argc)
{
	const value_t *arg;
	int b;
	int i;
	check_arg_one(ref, "fixpos_restore", argc);
	arg = value_get(argc, 0);
	check_arg_scalar(ref, "fixpos_restore", arg, 0);
	check_arg_unit_none(ref, "fixpos_restore", arg, 0);
	b = value_to_boolean(arg);
	if(b) {
		if(!g30_valid)
			rtwarning(ref, "Position for G30 not previously set, tracking may be off");
		for(i = 0; i < naxes; i++)
			*global_pos[i] = g30_pos[i];
	} else {
		if(!g28_valid)
			rtwarning(ref, "Position for G28 not previously set, tracking may be off");
		for(i = 0; i < naxes; i++)
			*global_pos[i] = g28_pos[i];
	}
	return value_new(VAL_UNDEF);
}

/*
 * Function:	sign(scalar:x)
 * Input:	scalar:x - any scalar
 * Output:	scalar: 1.0 or -1.0, depending input
 *
 * Returns 1.0 for x >= 0.0 and -1.0 for x < 0.0.
 */
static value_t *bi_sign(const node_t *ref, int argc)
{
	double d;
	const value_t *arg;
	value_t *v;
	check_arg_one(ref, "sign", argc);
	arg = value_get(argc, 0);
	check_arg_scalar(ref, "sign", arg, 0);
	d = value_to_double(arg);
	v = value_new(VAL_FLOAT);
	v->d = d >= 0.0 ? 1.0 : -1.0;
	return v;
}

/*
 * Function:	layer(string|scalar:l)
 * Input:	string|scalar:l - ID of the layer to activate
 * Output:	undef
 *
 * Set the active layer on which to output feed movement. The layer ID is one
 * that must be declared with layerstack() first.
 */
static const wchar_t **layers;
int nlayers;
static int nalayers;
int currentlayer = -1;

static int layer_find(const wchar_t *s)
{
	int i;
	/* Linear search for the layer should not be too bad, there are not many */
	for(i = 0; i < nlayers; i++) {
		if(!wcscmp(layers[i], s))
			return i;
	}
	return -1;
}

static int layer_add(const wchar_t *s)
{
	testalloc((void **)&layers, nlayers, &nalayers, sizeof(layers[0]));
	layers[nlayers++] = wcsdup(s);
	return nlayers-1;
}

const wchar_t *layername(int l)
{
	if(l < 0 || l >= nlayers)
		rtinternal(NULL, "Out of bound layer index '%d', only have %d", l, nlayers);
	return layers[l];
}

static int check_valid_layer_string(const wchar_t *s)
{
	if(!*s)
		return 0;	/* Must have some characters */

	/* Check for [a-zA-Z0-9_] */
	while(*s) {
		if(!iswdigit(*s) && !iswalpha(*s) && L'_' != *s)
			return 0;
		s++;
	}
	return 1;
}


/*
 * Function:	layerstack(string:name1, ...)
 * Input:	string:name1... - a list of strings naming the layers in order
 * Output:	undef
 *
 * Declare the stack of layers to be used in DXF and SVG output. Each name
 * argument is entered in the stack in the order in which they are specified.
 * The output routine will output the layers in that order.
 */
static value_t *bi_layerstack(const node_t *ref, int argc)
{
	int i, v;
	const value_t *arg;

	if(argc < 1) {
		rterror(ref, "layerstack(): must have at least one argument");
		return value_new(VAL_UNDEF);
	}
	for(i = 0; i < argc; i++) {
		arg = value_get(argc, i);
		check_arg_string(ref, "layerstack", arg, i);
		if(!check_valid_layer_string(arg->str.chs))
			rterror(ref, "layerstack() invalid characters in layer name %d", i+1);
		else if(-1 == (v = layer_find(arg->str.chs)))
			layer_add(arg->str.chs);
		else
			rterror(ref, "layerstack(): layer '%ls' already defined at level %d", arg->str.chs, v+1);
		if(-1 == currentlayer)
			currentlayer = 0;
	}
	return value_new(VAL_UNDEF);
}


/*
 * Function:	relocate(vector:v)
 * Input:	vector:v - Coordinate relocation base
 * Output:	undef
 *
 * Relocate the output coordinate system by vector v. All output is
 * subsequently offset by the given coordinates. This offset is separate from
 * the command-line offset, which is compounded to the relocation offset.
 */
value_t *bi_relocate(const node_t *ref, int argc)
{
	int i;

	if(!argc) {
		/* No argument zeros the relative offset */
		for(i = 0; i < naxes; i++)
			relative_offs[i] = 0.0;	// Zero offset if coordinate not given
	} else {
		const value_t *arg = value_get(argc, 0);
		check_arg_vector(ref, "relocate", arg, 0);

		for(i = 0; i < naxes; i++) {
			if(arg->v.n > i) {
				if(i < 3 || i >= 6) {
					check_arg_unit_distance(ref, "relocate", arg->v.vals[i], 0);
					relative_offs[i] = value_to_dist(arg->v.vals[i]);
				} else {
					check_arg_unit_angle(ref, "relocate", arg->v.vals[i], 0);
					relative_offs[i] = value_to_rad(arg->v.vals[i]);
				}
			} else
				relative_offs[i] = 0.0;	// Zero offset if coordinate not given
		}
	}

	return value_new(VAL_UNDEF);
}

/*
 * Function:	<placeholder>
 * Input:	-
 * Output:	undef
 *
 * A placeholder function returning undef. Other backends may override the
 * function if required.
 */
static value_t *bi_dummy(const node_t *ref, int argc)
{
	(void)ref;
	(void)argc;
	return value_new(VAL_UNDEF);
}

/* Callable functions from the script */
const builtins_t builtins_gcmc[] = {
	{ L"abs",		bi_abs },
	{ L"acos",		bi_acos },
	{ L"asin",		bi_asin },
	{ L"atan",		bi_atan },
	{ L"atan_xy",		bi_atan_xy },
	{ L"atan_xz",		bi_atan_xz },
	{ L"atan_yz",		bi_atan_yz },
	{ L"ceil",		bi_ceil },
	{ L"comment",		bi_comment },
	{ L"cos",		bi_cos },
	{ L"count",		bi_count },
	{ L"delete",		bi_delete },
	{ L"error",		bi_error },
	{ L"exp",		bi_exp },
	{ L"fixpos_restore",	bi_fixpos_restore },
	{ L"fixpos_set",	bi_fixpos_set },
	{ L"fixpos_store",	bi_fixpos_store },
	{ L"floor",		bi_floor },
	{ L"head",		bi_head },
	{ L"insert",		bi_insert },
	{ L"isangle",		bi_isangle },
	{ L"isconst",		bi_isconst },
	{ L"isdefined",		bi_isdefined },
	{ L"isdeg",		bi_isdeg },
	{ L"isdistance",	bi_isdistance },
	{ L"isfloat",		bi_isfloat },
	{ L"isinch",		bi_isinch },
	{ L"isint",		bi_isint },
	{ L"ismm",		bi_ismm },
	{ L"ismodemm",		bi_ismodemm },
	{ L"isnone",		bi_isnone },
	{ L"israd",		bi_israd },
	{ L"isscalar",		bi_isscalar },
	{ L"isstring",		bi_isstring },
	{ L"isundef",		bi_isundef },
	{ L"isvector",		bi_isvector },
	{ L"isvectorlist",	bi_isvectorlist },
	{ L"layerstack",	bi_layerstack },
	{ L"length",		bi_length },
	{ L"literal",		bi_literal },
	{ L"log10",		bi_log10 },
	{ L"log2",		bi_log2 },
	{ L"loge",		bi_loge },
	{ L"message",		bi_message },
	{ L"normalize",		bi_normalize },
	{ L"pause",		bi_dummy },	/* Override in builtin_gcode */
	{ L"pi",		bi_pi },
	{ L"position",		bi_position },
	{ L"pow",		bi_pow },
	{ L"relocate",		bi_relocate },
	{ L"reverse",		bi_reverse },
	{ L"rotate_xy",		bi_rotate_xy },
	{ L"rotate_xz",		bi_rotate_xz },
	{ L"rotate_yz",		bi_rotate_yz },
	{ L"round",		bi_round },
	{ L"scale",		bi_scale },
	{ L"sign",		bi_sign },
	{ L"sin",		bi_sin },
	{ L"sqrt",		bi_sqrt },
	{ L"svg_closepath",	bi_dummy },	/* Override in builtin_svg */
	{ L"tail",		bi_tail },
	{ L"tan",		bi_tan },
	{ L"to_chr",		bi_to_chr },
	{ L"to_deg",		bi_to_deg },
	{ L"to_distance",	bi_to_distance },
	{ L"to_float",		bi_to_float },
	{ L"to_in",		bi_to_in },
	{ L"to_inch",		bi_to_inch },
	{ L"to_int",		bi_to_int },
	{ L"to_mm",		bi_to_mm },
	{ L"to_native",		bi_to_native },
	{ L"to_none",		bi_to_none },
	{ L"to_rad",		bi_to_rad },
	{ L"to_string",		bi_to_string },
	{ L"to_val",		bi_to_val },
	{ L"undef",		bi_dummy },	/* Returns an undef value */
	{ L"warning",		bi_warning },
};

const int nbuiltins_gcmc = NELEM(builtins_gcmc);
