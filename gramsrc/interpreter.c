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

#define HAVE_CONFIG_H 1
#ifdef HAVE_CONFIG_H
//#include <config.h>
#include "config.h"
#endif

#include <wchar.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <limits.h>

#include "utils.h"
#include "value.h"
#include "variable.h"
#include "interpreter.h"
#include "builtin.h"
#include "../builtin_robot.h"


#pragma warning(disable:4996)

#define WARNSHIFTS	1000	/* Trying to shift this many times may be an error */
#define NSORTFUNC	16	/* Fewer than about 16 functions is faster using linear search */

static value_t *interpret(node_t *n, node_t **brkref);
static void interpret_expr(node_t *n, int islvalue, int istoplevel);

typedef value_t *(*opfunc_t)(const node_t *ref, value_t *lv, const value_t *rv);

static node_t **functions;
static int nfunctions;
static int nafunctions;

const char *nodetypename(node_et n)
{
	switch(n) {
	case NT_INVALID:	return "NT_INVALID";
	case NT_EXPR:		return "NT_EXPR";
	case NT_EXPRLIST:	return "NT_EXPRLIST";
	case NT_IF:		return "NT_IF";
	case NT_ELIF:		return "NT_ELIF";
	case NT_FOR:		return "NT_FOR";
	case NT_FOREACH:	return "NT_FOREACH";
	case NT_REPEAT:		return "NT_REPEAT";
	case NT_WHILE:		return "NT_WHILE";
	case NT_DOWHILE:	return "NT_DOWHILE";
	case NT_FUNCTION:	return "NT_FUNCTION";
	case NT_ARGLIST:	return "NT_ARGLIST";
	case NT_BREAK:		return "NT_BREAK";
	case NT_CONTINUE:	return "NT_CONTINUE";
	case NT_RETURN:		return "NT_RETURN";
	case NT_LOCAL:		return "NT_LOCAL";
	case NT_CONST:		return "NT_CONST";
	}
	return "<unknown node type>";
}

const char *expropname(op_et n)
{
	switch(n) {
	case OP_NULL:		return "OP_NULL";
	case OP_ADD:		return "OP_ADD";
	case OP_ADDOR:		return "OP_ADDOR";
	case OP_SUB:		return "OP_SUB";
	case OP_SUBOR:		return "OP_SUBOR";
	case OP_MUL:		return "OP_MUL";
	case OP_DIV:		return "OP_DIV";
	case OP_MOD:		return "OP_MOD";
	case OP_INDEX:		return "OP_INDEX";
	case OP_INDEXID:	return "OP_INDEXID";
	case OP_LOR:		return "OP_LOR";
	case OP_LAND:		return "OP_LAND";
	case OP_EQ:		return "OP_EQ";
	case OP_NE:		return "OP_NE";
	case OP_GT:		return "OP_GT";
	case OP_LT:		return "OP_LT";
	case OP_GE:		return "OP_GE";
	case OP_LE:		return "OP_LE";
	case OP_SHL:		return "OP_SHL";
	case OP_SHR:		return "OP_SHR";
	case OP_BAND:		return "OP_BAND";
	case OP_BOR:		return "OP_BOR";
	case OP_BXOR:		return "OP_BXOR";
	case OP_BNOT:		return "OP_BNOT";
	case OP_NOT:		return "OP_NOT";
	case OP_INT:		return "OP_INT";
	case OP_FLOAT:		return "OP_FLOAT";
	case OP_STRING:		return "OP_STRING";
	case OP_VECTOR:		return "OP_VECTOR";
	case OP_VECTORLIST:	return "OP_VECTORLIST";
	case OP_ASSIGN:		return "OP_ASSIGN";
	case OP_ADDASSIGN:	return "OP_ADDASSIGN";
	case OP_ADDORASSIGN:	return "OP_ADDORASSIGN";
	case OP_SUBASSIGN:	return "OP_SUBASSIGN";
	case OP_SUBORASSIGN:	return "OP_SUBORASSIGN";
	case OP_MULASSIGN:	return "OP_MULASSIGN";
	case OP_DIVASSIGN:	return "OP_DIVASSIGN";
	case OP_MODASSIGN:	return "OP_MODASSIGN";
	case OP_SHLASSIGN:	return "OP_SHLASSIGN";
	case OP_SHRASSIGN:	return "OP_SHRASSIGN";
	case OP_BORASSIGN:	return "OP_BORASSIGN";
	case OP_BANDASSIGN:	return "OP_BANDASSIGN";
	case OP_BXORASSIGN:	return "OP_BXORASSIGN";
	case OP_PREINC:		return "OP_PREINC";
	case OP_PREDEC:		return "OP_PREDEC";
	case OP_POSTINC:	return "OP_POSTINC";
	case OP_POSTDEC:	return "OP_POSTDEC";
	case OP_DEREF:		return "OP_DEREF";
	case OP_CONDEXPR:	return "OP_CONDEXPR";
	case OP_CALL:		return "OP_CALL";
	}
	return "<unknown expression operation>";
}

const char *unitname(int n)
{
	switch(n) {
	case UNIT_NONE:	return "UNIT_NONE";
	case UNIT_MM:	return "UNIT_MM";
	case UNIT_IN:	return "UNIT_IN";
	case UNIT_DEG:	return "UNIT_DEG";
	case UNIT_RAD:	return "UNIT_RAD";
	}
	return "<unknown unit>";
}

static void unit_check(const node_t *n, const value_t *v)
{
	switch(v->unit) {
	case UNIT_NONE:
	case UNIT_MM:
	case UNIT_IN:
	case UNIT_DEG:
	case UNIT_RAD:
		break;
	default:
		rtinternal(n, "Invalid unit value %d", v->unit);
	}
}

static value_t *op_add_common(const node_t *ref, value_t *lv, const value_t *rv, const char *fn, opfunc_t opself)
{
	int i;
	double dval;
	int unitmismatch = 0;
	assert(lv != NULL);
	assert(rv != NULL);

	/* Special case lv == VAL_STRING */
	if(isstring(lv)) {
		value_t *v = NULL;
		if(!isstring(rv)) {
			v = value_to_string(ref, rv);
			lv->str.n += v->str.n;
		} else
			lv->str.n += rv->str.n;
		lv->str.na = lv->str.n + 1;
		lv->str.chs = realloc(lv->str.chs, (lv->str.n + 1) * sizeof(lv->str.chs[0]));
		assert(lv->str.chs != NULL);
		if(!isstring(rv)) {
			wcscat(lv->str.chs, v->str.chs);
			value_delete(v);
		} else
			wcscat(lv->str.chs, rv->str.chs);
		return lv;
	}

	/* Type check
	 * scalar + scalar
	 * vector + vector
	 * vectorlist + vector
	 * vectorlist + vectorlist	(append)
	 */
	if(!((isscalar(lv) && isscalar(rv)) || (isvector(lv) && isvector(rv)) || (isvectorlist(lv) && (isvector(rv) || isvectorlist(rv))))) {
		rterror(ref, "Scalars and vectors can only be added to scalars vectors and vector lists (lv=%s, rv=%s)", valuetypename(lv->type), valuetypename(rv->type));
		return lv;
	}

	unit_check(ref, lv);
	unit_check(ref, rv);

	if(isangle(lv) && isdistance(rv)) {
		rtwarning(ref, "Operation '+': LHS has angular units and RHS has distance units, RHS units ignored");
		unitmismatch = 1;
	} else if(isdistance(lv) && isangle(rv)) {
		rtwarning(ref, "Operation '+': LHS has distance units and RHS has angular units, RHS units ignored");
		unitmismatch = 1;
	}

	if(lv->type == VAL_INT && rv->type == VAL_FLOAT) {
		lv->d = lv->i;
		lv->type = VAL_FLOAT;
	}

	switch(lv->type) {
	case VAL_INT:
		assert(isscalar(rv));
		if(lv->unit == rv->unit || rv->unit == UNIT_NONE || unitmismatch) {
			lv->i += rv->i;
		} else if(lv->unit == UNIT_NONE) {
			lv->i += rv->i;
			lv->unit = rv->unit;
		} else if(isdistance(lv)) {
			if(rv->unit == UNIT_IN)
				lv->d = (double)lv->i + (double)rv->i * 25.4;
			else
				lv->d = (double)lv->i + (double)rv->i / 25.4;
			lv->type = VAL_FLOAT;
		} else if(isangle(lv)) {
			if(rv->unit == UNIT_DEG)
				lv->d = (double)lv->i + (double)rv->i * M_PI / 180.0;
			else
				lv->d = (double)lv->i + (double)rv->i / M_PI * 180.0;
			lv->type = VAL_FLOAT;
		} else
			rtinternal(ref, "Unit mismatch in %s %d, %d", fn, lv->unit, rv->unit);
		break;
	case VAL_FLOAT:
		assert(isscalar(rv));
		if(rv->type == VAL_INT)
			dval = rv->i;
		else
			dval = rv->d;
		if(lv->unit == rv->unit || rv->unit == UNIT_NONE || unitmismatch) {
			lv->d += dval;
		} else if(lv->unit == UNIT_NONE) {
			lv->d += dval;
			lv->unit = rv->unit;
		} else if(isdistance(lv)) {
			if(rv->unit == UNIT_IN)
				lv->d += dval * 25.4;
			else
				lv->d += dval / 25.4;
		} else if(isangle(lv)) {
			if(rv->unit == UNIT_DEG)
				lv->d += dval * M_PI / 180.0;
			else
				lv->d += dval / M_PI * 180.0;
		} else
			rtinternal(ref, "Unit mismatch in %s %d, %d", fn, lv->unit, rv->unit);
		break;
	case VAL_VECTOR:
		while(lv->v.n < rv->v.n)
			value_vector_add_undef(lv);
		for(i = 0; i < lv->v.n && i < rv->v.n; i++)
			opself(ref, lv->v.vals[i], rv->v.vals[i]);
		break;
	case VAL_VECTORLIST:
		if(rv->type == VAL_VECTOR) {
			for(i = 0; i < lv->vl.n; i++)
				opself(ref, lv->vl.vecs[i], rv);
		} else {
			assert(rv->type == VAL_VECTORLIST);
			/* Append vectorlist, transfer from rv to lv */
			for(i = 0; i < rv->vl.n; i++)
				value_vectorlist_add(lv, value_dup(rv->vl.vecs[i]));
		}
		break;
	case VAL_STRING:
	default:
		rtinternal(ref, "Invalid value type in %s %s", fn, valuetypename(lv->type));
	}
	return lv;
}

static value_t *op_addor(const node_t *ref, value_t *lv, const value_t *rv)
{
	assert(lv != NULL);
	assert(rv != NULL);
	if(!isstring(lv) && rv->type == VAL_UNDEF) {
		/* x +| "undef" = x + 0 = x */
		return lv;
	}
	if(lv->type == VAL_UNDEF) {
		if(!isscalar(rv)) {
			rterror(ref, "Operation '<undef> + non-scalar' not supported");
			return lv;
		}
		/* "undef" +| x = 0 + x */
		*lv = *rv;
		return lv;
	}

	return op_add_common(ref, lv, rv, "op_addor", op_addor);
}

static value_t *op_add(const node_t *ref, value_t *lv, const value_t *rv)
{
	assert(lv != NULL);
	assert(rv != NULL);
	if(!isstring(lv) && lv->type == VAL_UNDEF) {
		/* "undef" + x = "undef" */
		return lv;
	}
	if(rv->type == VAL_UNDEF) {
		/* x + "undef" = x */
		return lv;
	}

	return op_add_common(ref, lv, rv, "op_add", op_add);
}

static value_t *op_sub_common(const node_t *ref, value_t *lv, const value_t *rv, const char *fn, opfunc_t opself)
{
	int i;
	double dval;
	int unitmismatch = 0;
	assert(lv != NULL);
	assert(rv != NULL);

	/* Type check
	 * scalar - scalar
	 * vector - vector
	 * vectorlist - vector
	 */
	if(!((isscalar(lv) && isscalar(rv)) || ((isvector(lv) || isvectorlist(lv)) && isvector(rv)))) {
		rterror(ref, "Only scalars/vectors can be subtracted from scalars/vectors (lv=%s, rv=%s)", valuetypename(lv->type), valuetypename(rv->type));
		return lv;
	}
	unit_check(ref, lv);
	unit_check(ref, rv);

	if(isangle(lv) && isdistance(rv)) {
		rtwarning(ref, "Operation '-': LHS has angular units and RHS has distance units, RHS units ignored");
		unitmismatch = 1;
	} else if(isdistance(lv) && isangle(rv)) {
		rtwarning(ref, "Operation '-': LHS has distance units and RHS has angular units, RHS units ignored");
		unitmismatch = 1;
	}

	if(lv->type == VAL_INT && rv->type == VAL_FLOAT) {
		lv->d = lv->i;
		lv->type = VAL_FLOAT;
	}

	switch(lv->type) {
	case VAL_INT:
		assert(isscalar(rv));
		if(lv->unit == rv->unit || rv->unit == UNIT_NONE || unitmismatch) {
			lv->i -= rv->i;
		} else if(lv->unit == UNIT_NONE) {
			lv->i -= rv->i;
			lv->unit = rv->unit;
		} else if(isdistance(lv)) {
			if(rv->unit == UNIT_IN)
				lv->d = (double)lv->i - (double)rv->i * 25.4;
			else
				lv->d = (double)lv->i - (double)rv->i / 25.4;
			lv->type = VAL_FLOAT;
		} else if(isangle(lv)) {
			if(rv->unit == UNIT_DEG)
				lv->d = (double)lv->i - (double)rv->i * M_PI / 180.0;
			else
				lv->d = (double)lv->i - (double)rv->i / M_PI * 180.0;
			lv->type = VAL_FLOAT;
		} else
			rtinternal(ref, "Unit mismatch in %s %d, %d", fn, lv->unit, rv->unit);
		break;
	case VAL_FLOAT:
		assert(isscalar(rv));
		if(rv->type == VAL_INT)
			dval = rv->i;
		else
			dval = rv->d;
		if(lv->unit == rv->unit || rv->unit == UNIT_NONE || unitmismatch) {
			lv->d -= dval;
		} else if(lv->unit == UNIT_NONE) {
			lv->d -= dval;
			lv->unit = rv->unit;
		} else if(isdistance(lv)) {
			if(rv->unit == UNIT_IN)
				lv->d -= dval * 25.4;
			else
				lv->d -= dval / 25.4;
		} else if(isangle(lv)) {
			if(rv->unit == UNIT_DEG)
				lv->d -= dval * M_PI / 180.0;
			else
				lv->d -= dval / M_PI * 180.0;
		} else
			rtinternal(ref, "Unit mismatch in %s %d, %d", fn, lv->unit, rv->unit);
		break;
	case VAL_VECTOR:
		while(lv->v.n < rv->v.n)
			value_vector_add_undef(lv);
		for(i = 0; i < lv->v.n && i < rv->v.n; i++)
			opself(ref, lv->v.vals[i], rv->v.vals[i]);
		break;
	case VAL_VECTORLIST:
		for(i = 0; i < lv->vl.n; i++)
			opself(ref, lv->vl.vecs[i], rv);
		break;
	case VAL_STRING:
	default:
		rtinternal(ref, "Invalid value type in %s %s", fn, valuetypename(lv->type));
	}
	return lv;
}

static value_t *op_subor(const node_t *ref, value_t *lv, const value_t *rv)
{
	assert(lv != NULL);
	assert(rv != NULL);
	if(rv->type == VAL_UNDEF) {
		/* x - "undef" = x - 0 = x */
		return lv;
	}
	if(lv->type == VAL_UNDEF) {
		/* "undef" - x = 0 - x */
		lv->type = VAL_INT;
		lv->i = 0;
		lv->unit = UNIT_NONE;
	}
	return op_sub_common(ref, lv, rv, "op_subor", op_subor);
}

static value_t *op_sub(const node_t *ref, value_t *lv, const value_t *rv)
{
	assert(lv != NULL);
	assert(rv != NULL);
	if(lv->type == VAL_UNDEF) {
		/* "undef" - x = "undef" */
		return lv;
	}
	if(rv->type == VAL_UNDEF) {
		/* x - "undef" = x */
		return lv;
	}
	return op_sub_common(ref, lv, rv, "op_sub", op_sub);
}

value_t *op_mul(const node_t *ref, value_t *lv, const value_t *rv)
{
	int i;
	double dval;
	int unitmismatch = 0;
	assert(lv != NULL);
	assert(rv != NULL);

	/* Multiply by "undef" = "undef" */
	if(lv->type == VAL_UNDEF)
		return lv;
	if(rv->type == VAL_UNDEF) {
		if(!isscalar(lv)) {
			rterror(ref, "Operation 'non-scalar * <undef>' not supported");
			return lv;
		}
		lv->type = VAL_UNDEF;
		return lv;
	}

	/* Type check
	 * scalar * scalar
	 * scaler * vector
	 * scaler * vectorlist
	 * vector * scalar
	 * vector * vector	(dot product)
	 * vectorlist * scalar	(each vector multiplied by scalar)
	 */
	if(!(	(isscalar(lv) && isscalar(rv)) ||
		(isscalar(lv) && (isvector(rv) || isvectorlist(rv))) ||
		(isvector(lv) && (isscalar(rv) || isvector(rv))) ||
		(isvectorlist(lv) && isscalar(rv)))) {
		rterror(ref, "Scalars and vectors can only be multiplied by scalars and vectors (lv=%s, rv=%s)", valuetypename(lv->type), valuetypename(rv->type));
		return lv;
	}

	unit_check(ref, lv);
	unit_check(ref, rv);

	if(isscalar(lv) && isvector(rv)) {
		/* Special case scalar * vector */
		value_t *vec = value_new(VAL_VECTOR);
		for(i = 0; i < rv->v.n; i++) {
			value_vector_add(vec, value_dup(lv));
			op_mul(ref, vec->v.vals[i], rv->v.vals[i]);
		}
		*lv = *vec;
		vec->type = VAL_UNDEF;
		value_delete(vec);
		return lv;
	} else if(isscalar(lv) && isvectorlist(rv)) {
		/* Special case scalar * vectorlist */
		value_t *vecl = value_new(VAL_VECTORLIST);
		for(i = 0; i < rv->vl.n; i++) {
			value_t *nlv = value_dup(lv);
			value_vectorlist_add(vecl, op_mul(ref, nlv, rv->vl.vecs[i]));
		}
		*lv = *vecl;
		vecl->type = VAL_UNDEF;
		value_delete(vecl);
		return lv;
	}

	if(isangle(lv) && isdistance(rv)) {
		rtwarning(ref, "Operation '*': LHS has angular units and RHS has distance units, RHS units ignored");
		unitmismatch = 1;
	} else if(isdistance(lv) && isangle(rv)) {
		rtwarning(ref, "Operation '*': LHS has distance units and RHS has angular units, RHS units ignored");
		unitmismatch = 1;
	}

	if(lv->type == VAL_INT && rv->type == VAL_FLOAT) {
		lv->d = lv->i;
		lv->type = VAL_FLOAT;
	}

	switch(lv->type) {
	case VAL_INT:
		assert(isscalar(rv));
		if(lv->unit == rv->unit || rv->unit == UNIT_NONE || unitmismatch) {
			lv->i *= rv->i;
		} else if(lv->unit == UNIT_NONE) {
			lv->i *= rv->i;
			lv->unit = rv->unit;
		} else if(isdistance(lv)) {
			if(rv->unit == UNIT_IN)
				lv->d = (double)lv->i * (double)rv->i * 25.4;
			else
				lv->d = (double)lv->i * (double)rv->i / 25.4;
			lv->type = VAL_FLOAT;
		} else if(isangle(lv)) {
			if(rv->unit == UNIT_DEG)
				lv->d = (double)lv->i * (double)rv->i * M_PI / 180.0;
			else
				lv->d = (double)lv->i * (double)rv->i / M_PI * 180.0;
			lv->type = VAL_FLOAT;
		} else
			rtinternal(ref, "Unit mismatch in op_mul %d, %d", lv->unit, rv->unit);
		break;
	case VAL_FLOAT:
		assert(isscalar(rv));
		if(rv->type == VAL_INT)
			dval = rv->i;
		else
			dval = rv->d;
		if(lv->unit == rv->unit || rv->unit == UNIT_NONE || unitmismatch) {
			lv->d *= dval;
		} else if(lv->unit == UNIT_NONE) {
			lv->d *= dval;
			lv->unit = rv->unit;
		} else if(isdistance(lv)) {
			if(rv->unit == UNIT_IN)
				lv->d *= dval * 25.4;
			else
				lv->d *= dval / 25.4;
		} else if(isangle(lv)) {
			if(rv->unit == UNIT_DEG)
				lv->d *= dval * M_PI / 180.0;
			else
				lv->d *= dval / M_PI * 180.0;
		} else
			rtinternal(ref, "Unit mismatch in op_mul %d, %d", lv->unit, rv->unit);
		break;
	case VAL_VECTOR:
		if(isscalar(rv)) {
			for(i = 0; i < lv->v.n; i++) {
				op_mul(ref, lv->v.vals[i], rv);
			}
		} else {
			/* Vector * vector == scalar (dot product) */
			double sum = 0.0;
			unit_et u = UNIT_NONE;
			for(i = 0; i < lv->v.n && i < rv->v.n; i++) {
				if(isangle(lv->v.vals[i]))
					rtwarning(ref, "Angular units found at LHS vector index %d. Dot product on angles is undefined", i);
				if(isangle(rv->v.vals[i]))
					rtwarning(ref, "Angular units found at RHS vector index %d. Dot product on angles is undefined", i);
				if(!isnone(lv->v.vals[i]))
					u = lv->v.vals[i]->unit;
				if(!isnone(rv->v.vals[i]))
					u = rv->v.vals[i]->unit;
				sum += value_to_dist(lv->v.vals[i]) * value_to_dist(rv->v.vals[i]);
				value_delete(lv->v.vals[i]);	/* Free the LHS vector's values */
			}
			free(lv->v.vals);	/* Also free the LHS vector itself --> replaced with dot-product, which is scalar */
			lv->type = VAL_FLOAT;
			lv->unit = u == UNIT_NONE ? UNIT_NONE : UNIT_MM;
			lv->d = sum;
		}
		break;
	case VAL_VECTORLIST:
		assert(isscalar(rv));
		for(i = 0; i < lv->vl.n; i++)
			op_mul(ref, lv->vl.vecs[i], rv);
		break;
	case VAL_STRING:
	default:
		rtinternal(ref, "Invalid value type in op_mul %s", valuetypename(lv->type));
	}
	return lv;
}

static value_t *op_div(const node_t *ref, value_t *lv, const value_t *rv)
{
	int i;
	double dval;
	int unitmismatch = 0;
	assert(lv != NULL);
	assert(rv != NULL);

	/* Divide by "undef" = "undef" */
	if(lv->type == VAL_UNDEF)
		return lv;
	if(rv->type == VAL_UNDEF) {
		if(!isscalar(lv)) {
			rterror(ref, "Operation 'non-scalar / <undef>' not supported");
			return lv;
		}
		lv->type = VAL_UNDEF;
		return lv;
	}

	/* Type check
	 * scalar / scalar
	 * vector / scalar
	 * vectorlist / scalar
	 */
	if(!isscalar(rv) || !(isscalar(lv) || isvector(lv) || isvectorlist(lv))) {
		rterror(ref, "Scalars, vectors and vectorlists can only be divided by scalars (lv=%s, rv=%s)", valuetypename(lv->type), valuetypename(rv->type));
		return lv;
	}

	unit_check(ref, lv);
	unit_check(ref, rv);

	if(isangle(lv) && isdistance(rv)) {
		rtwarning(ref, "Operation '/': LHS has angular units and RHS has distance units, RHS units ignored");
		unitmismatch = 1;
	} else if(isdistance(lv) && isangle(rv)) {
		rtwarning(ref, "Operation '/': LHS has distance units and RHS has angular units, RHS units ignored");
		unitmismatch = 1;
	}

	if(lv->type == VAL_INT && rv->type == VAL_FLOAT) {
		lv->d = lv->i;
		lv->type = VAL_FLOAT;
	}

	switch(lv->type) {
	case VAL_INT:
		assert(isscalar(rv));
		if(!rv->i)
			rterror(ref, "Divide by zero");
		if(lv->unit == rv->unit || unitmismatch) {
			lv->i /= rv->i;
			lv->unit = UNIT_NONE;
		} else if(rv->unit == UNIT_NONE) {
			lv->i /= rv->i;
		} else if(lv->unit == UNIT_NONE) {
			lv->i /= rv->i;
			lv->unit = rv->unit;
		} else if(isdistance(lv)) {
			if(rv->unit == UNIT_IN)
				lv->d = (double)lv->i / ((double)rv->i * 25.4);
			else
				lv->d = (double)lv->i / ((double)rv->i / 25.4);
			lv->type = VAL_FLOAT;
			lv->unit = UNIT_NONE;
		} else if(isangle(lv)) {
			if(rv->unit == UNIT_DEG)
				lv->d = (double)lv->i / ((double)rv->i * M_PI / 180.0);
			else
				lv->d = (double)lv->i / ((double)rv->i / M_PI * 180.0);
			lv->type = VAL_FLOAT;
			lv->unit = UNIT_NONE;
		} else
			rtinternal(ref, "Unit mismatch in op_div %d, %d", lv->unit, rv->unit);
		break;
	case VAL_FLOAT:
		assert(isscalar(rv));
		if(rv->type == VAL_INT)
			dval = rv->i;
		else
			dval = rv->d;
		if(fabs(dval) < EPSILON)
			rterror(ref, "Divide by zero (|val| < %e)", EPSILON);
		if(lv->unit == rv->unit || unitmismatch) {
			lv->d /= dval;
			lv->unit = UNIT_NONE;
		} else if(rv->unit == UNIT_NONE) {
			lv->d /= dval;
		} else if(lv->unit == UNIT_NONE) {
			lv->d /= dval;
			lv->unit = rv->unit;
		} else if(isdistance(lv)) {
			if(rv->unit == UNIT_IN)
				lv->d /= dval * 25.4;
			else
				lv->d /= dval / 25.4;
			lv->unit = UNIT_NONE;
		} else if(isangle(lv)) {
			if(rv->unit == UNIT_DEG)
				lv->d /= dval * M_PI / 180.0;
			else
				lv->d /= dval / M_PI * 180.0;
			lv->unit = UNIT_NONE;
		} else
			rtinternal(ref, "Unit mismatch in op_div %d, %d", lv->unit, rv->unit);
		break;
	case VAL_VECTOR:
		for(i = 0; i < lv->v.n; i++)
			op_div(ref, lv->v.vals[i], rv);
		break;
	case VAL_VECTORLIST:
		for(i = 0; i < lv->vl.n; i++)
			op_div(ref, lv->vl.vecs[i], rv);
		break;
	case VAL_STRING:
	default:
		rtinternal(ref, "Invalid value type in op_div %s", valuetypename(lv->type));
	}
	return lv;
}

static value_t *op_mod(const node_t *ref, value_t *lv, const value_t *rv)
{
	int i;
	double dval;
	int unitmismatch = 0;
	assert(lv != NULL);
	assert(rv != NULL);

	/* Modulo "undef" = "undef" */
	if(lv->type == VAL_UNDEF)
		return lv;
	if(rv->type == VAL_UNDEF) {
		if(!isscalar(lv)) {
			rterror(ref, "Operation 'non-scalar %% <undef>' not supported");
			return lv;
		}
		lv->type = VAL_UNDEF;
		return lv;
	}

	/* Type check
	 * scalar % scalar
	 * vector % scalar
	 * vectorlist % scalar
	 */
	if(!isscalar(rv) || !(isscalar(lv) || isvector(lv) || isvectorlist(lv))) {
		rterror(ref, "Scalar modulo can only be calculated on scalars, vectors and vectorlists (lv=%s, rv=%s)", valuetypename(lv->type), valuetypename(rv->type));
		return lv;
	}

	unit_check(ref, lv);
	unit_check(ref, rv);

	if(isangle(lv) && isdistance(rv)) {
		rtwarning(ref, "Operation '%%': LHS has angular units and RHS has distance units, RHS units ignored");
		unitmismatch = 1;
	} else if(isdistance(lv) && isangle(rv)) {
		rtwarning(ref, "Operation '%%': LHS has distance units and RHS has angular units, RHS units ignored");
		unitmismatch = 1;
	}

	if(lv->type == VAL_INT && rv->type == VAL_FLOAT) {
		lv->d = lv->i;
		lv->type = VAL_FLOAT;
	}

	switch(lv->type) {
	case VAL_INT:
		assert(isscalar(rv));
		if(!rv->i)
			rterror(ref, "Modulo by zero");
		if(lv->unit == rv->unit || rv->unit == UNIT_NONE || unitmismatch) {
			lv->i %= rv->i;
		} else if(lv->unit == UNIT_NONE) {
			lv->i %= rv->i;
			lv->unit = rv->unit;
		} else if(isdistance(lv)) {
			if(rv->unit == UNIT_IN)
				lv->d = fmod((double)lv->i, ((double)rv->i * 25.4));
			else
				lv->d = fmod((double)lv->i, ((double)rv->i / 25.4));
			lv->type = VAL_FLOAT;
		} else if(isangle(lv)) {
			if(rv->unit == UNIT_DEG)
				lv->d = fmod((double)lv->i, ((double)rv->i * M_PI / 180.0));
			else
				lv->d = fmod((double)lv->i, ((double)rv->i / M_PI * 180.0));
			lv->type = VAL_FLOAT;
		} else
			rtinternal(ref, "Unit mismatch in op_mod %d, %d", lv->unit, rv->unit);
		break;
	case VAL_FLOAT:
		assert(isscalar(rv));
		if(rv->type == VAL_INT)
			dval = rv->i;
		else
			dval = rv->d;
		if(fabs(dval) < EPSILON)
			rterror(ref, "Modulo by zero (|val| < %e)", EPSILON);
		if(lv->unit == rv->unit || rv->unit == UNIT_NONE || unitmismatch) {
			lv->d = fmod(lv->d, dval);
		} else if(lv->unit == UNIT_NONE) {
			lv->d = fmod(lv->d, dval);
			lv->unit = rv->unit;
		} else if(isdistance(lv)) {
			if(rv->unit == UNIT_IN)
				lv->d = fmod(lv->d, (dval * 25.4));
			else
				lv->d = fmod(lv->d, (dval / 25.4));
		} else if(isangle(lv)) {
			if(rv->unit == UNIT_DEG)
				lv->d = fmod(lv->d, (dval * M_PI / 180.0));
			else
				lv->d = fmod(lv->d, (dval / M_PI * 180.0));
		} else
			rtinternal(ref, "Unit mismatch in op_mod %d, %d", lv->unit, rv->unit);
		break;
	case VAL_VECTOR:
		for(i = 0; i < lv->v.n; i++)
			op_mod(ref, lv->v.vals[i], rv);
		break;
	case VAL_VECTORLIST:
		for(i = 0; i < lv->vl.n; i++)
			op_mod(ref, lv->vl.vecs[i], rv);
		break;
	case VAL_STRING:
	default:
		rtinternal(ref, "Invalid value type in op_mod %s", valuetypename(lv->type));
	}
	return lv;
}

static value_t *op_shl(const node_t *ref, value_t *lv, const value_t *rv)
{
	int i;
	int sft;
	assert(lv != NULL);
	assert(rv != NULL);

	if(lv->type == VAL_UNDEF || rv->type == VAL_UNDEF) {
		/* "undef" << x = undef */
		/* x << "undef" = x << 0 = x */
		return lv;
	}

	/* Type check
	 * scalar << scalar
	 * vector << scalar
	 * vectorlist << scalar
	 */
	if(!(isscalar(rv) && (isscalar(lv) || isvector(lv) || isvectorlist(lv)))) {
		rterror(ref, "Scalars, vectors and vectorlists can only be shifted by scalars (lv=%s, rv=%s)", valuetypename(lv->type), valuetypename(rv->type));
		return lv;
	}

	unit_check(ref, lv);
	unit_check(ref, rv);

	if(rv->unit != UNIT_NONE)
		rtwarning(ref, "Operation '<<': Shift count has units associated, ignored");
	if(rv->type == VAL_FLOAT && rv->d > (double)INT_MAX) {
		rterror(ref, "Operation '<<': Shift count floating point larger than integer can hold");
		return lv;
	}
	sft = value_to_int(rv);
	if(sft < 0) {
		rterror(ref, "Operation '<<': Shift count less than zero not allowed");
		return lv;
	}

	if(isscalar(lv) && sft > (int)(sizeof(int) * 8 - 1)) {
		rterror(ref, "Operation '<<': Shift count > %d has no effect", (int)(sizeof(int) * 8 - 1));
		return lv;
	} else if(cl_pedantic && !isscalar(lv) && sft > WARNSHIFTS)
		rtwarning(ref, "Operation '<<': Shift count > %d might be too large", sft);

	switch(lv->type) {
	case VAL_INT:
		lv->i <<= sft;
		break;
	case VAL_FLOAT:
		lv->d *= (double)(1 << sft);
		break;
	case VAL_VECTOR:
		for(i = 0; i < sft && lv->v.n; i++) {
			value_delete(lv->v.vals[0]);
			if(lv->v.n > 1)
				memmove(&lv->v.vals[0], &lv->v.vals[1], (lv->v.n-1)*sizeof(lv->v.vals[0]));
			lv->v.n--;
		}
		break;
	case VAL_VECTORLIST:
		for(i = 0; i < sft && lv->vl.n; i++) {
			value_delete(lv->vl.vecs[0]);
			if(lv->vl.n > 1)
				memmove(&lv->vl.vecs[0], &lv->vl.vecs[1], (lv->vl.n-1)*sizeof(lv->vl.vecs[0]));
			lv->vl.n--;
		}
		break;
	case VAL_STRING:
	default:
		rtinternal(ref, "Invalid value type in op_shl %s", valuetypename(lv->type));
	}
	return lv;
}

static value_t *op_shr(const node_t *ref, value_t *lv, const value_t *rv)
{
	int i;
	int sft;
	assert(lv != NULL);
	assert(rv != NULL);

	if(lv->type == VAL_UNDEF || rv->type == VAL_UNDEF) {
		/* "undef" >> x = undef */
		/* x >> "undef" = x << 0 = x */
		return lv;
	}

	/* Type check
	 * scalar >> scalar
	 * vector >> scalar
	 * vectorlist >> scalar
	 */
	if(!(isscalar(rv) && (isscalar(lv) || isvector(lv) || isvectorlist(lv)))) {
		rterror(ref, "Scalars, vectors and vectorlists can only be shifted by scalars (lv=%s, rv=%s)", valuetypename(lv->type), valuetypename(rv->type));
		return lv;
	}

	unit_check(ref, lv);
	unit_check(ref, rv);

	if(rv->unit != UNIT_NONE)
		rtwarning(ref, "Operation '>>': Shift count has units associated, ignored");
	if(rv->type == VAL_FLOAT && rv->d > (double)INT_MAX) {
		rterror(ref, "Operation '>>': Shift count floating point larger than integer can hold");
		return lv;
	}
	sft = value_to_int(rv);
	if(sft < 0) {
		rterror(ref, "Operation '>>': Shift count less than zero not allowed");
		return lv;
	}

	if(isscalar(lv) && sft > (int)(sizeof(int) * 8 - 1)) {
		rterror(ref, "Operation '>>': Shift count > %d has no effect", (int)(sizeof(int) * 8 - 1));
		return lv;
	} else if(cl_pedantic && !isscalar(lv) && sft > WARNSHIFTS)
		rtwarning(ref, "Operation '>>': Shift count > %d might be too large", sft);

	switch(lv->type) {
	case VAL_INT:
		lv->i >>= sft;
		break;
	case VAL_FLOAT:
		lv->d /= (double)(1 << sft);
		break;
	case VAL_VECTOR:
		for(i = 0; i < sft; i++)
			value_vector_prepend(lv, value_new(VAL_UNDEF));
		break;
	case VAL_VECTORLIST:
		for(i = 0; i < sft; i++)
			value_vectorlist_prepend(lv, value_new(VAL_VECTOR));
		break;
	case VAL_STRING:
	default:
		rtinternal(ref, "Invalid value type in op_shl %s", valuetypename(lv->type));
	}
	return lv;
}

static value_t *op_bor(const node_t *ref, value_t *lv, const value_t *rv)
{
	int i;
	int lvi, rvi;
	assert(lv != NULL);
	assert(rv != NULL);

	/* Type check
	 * scalar | scalar
	 * vector | vector
	 */
	if(!((isscalar(lv) && isscalar(rv)) || (isvector(lv) && isvector(rv)))) {
		rterror(ref, "Binary '|' operator only works on scalar and vector values");
		return lv;
	}

	if(isvector(lv)) {
		/* Vector | vector */
		/* Extend LHS to same number of entries as RHS */
		while(lv->v.n < rv->v.n)
			value_vector_add_undef(lv);
		/* Replace any undef in LHS if set in RHS */
		for(i = 0; i < lv->v.n && i < rv->v.n; i++) {
			if(isundef(lv->v.vals[i]) && !isundef(rv->v.vals[i])) {
				value_delete(lv->v.vals[i]);
				lv->v.vals[i] = value_dup(rv->v.vals[i]);
			}
		}
	} else {
		/* Otherwise scalar binary | */
		if(!isint(lv))
			rtwarning(ref, "Binary '|' operator, left-hand-side, should be integer, converted");
		if(!isnone(lv))
			rtwarning(ref, "Binary '|' operator, left-hand-side, has units associated, ignored");
		if(!isint(rv))
			rtwarning(ref, "Binary '|' operator, right-hand-side, should be integer, converted");
		if(!isnone(rv))
			rtwarning(ref, "Binary '|' operator, right-hand-side, has units associated, ignored");
		lvi = value_to_int(lv);
		rvi = value_to_int(rv);
		lv->i = lvi | rvi;
		lv->unit = UNIT_NONE;
	}
	return lv;
}

static value_t *op_band(const node_t *ref, value_t *lv, const value_t *rv)
{
	int i;
	int lvi, rvi;
	assert(lv != NULL);
	assert(rv != NULL);

	/* Type check
	 * scalar & scalar
	 * vector & vector
	 */
	if(!((isscalar(lv) && isscalar(rv)) || (isvector(lv) && isvector(rv)))) {
		rterror(ref, "Binary '&' operator only works on scalar and vector values");
		return lv;
	}

	if(isvector(lv)) {
		/* Vector | vector */
		/* Extend LHS to same number of entries as RHS */
		while(lv->v.n < rv->v.n)
			value_vector_add_undef(lv);
		/* Replace any non-undef in LHS if set in RHS */
		for(i = 0; i < lv->v.n && i < rv->v.n; i++) {
			if(!isundef(lv->v.vals[i]) && !isundef(rv->v.vals[i])) {
				value_delete(lv->v.vals[i]);
				lv->v.vals[i] = value_dup(rv->v.vals[i]);
			}
		}
	} else {
		/* Otherwise scalar binary & */
		if(!isint(lv))
			rtwarning(ref, "Binary '&' operator, left-hand-side, should be integer, converted");
		if(!isnone(lv))
			rtwarning(ref, "Binary '&' operator, left-hand-side, has units associated, ignored");
		if(!isint(rv))
			rtwarning(ref, "Binary '&' operator, right-hand-side, should be integer, converted");
		if(!isnone(rv))
			rtwarning(ref, "Binary '&' operator, right-hand-side, has units associated, ignored");
		lvi = value_to_int(lv);
		rvi = value_to_int(rv);
		lv->i = lvi & rvi;
		lv->unit = UNIT_NONE;
	}
	return lv;
}

static value_t *op_bxor(const node_t *ref, value_t *lv, const value_t *rv)
{
	int lvi, rvi;
	assert(lv != NULL);
	assert(rv != NULL);

	/* Type check
	 * scalar & scalar
	 */
	if(!(isscalar(lv) && isscalar(rv))) {
		rterror(ref, "Binary '^' operator only works on scalar and vector values");
		return lv;
	}

	/* Otherwise scalar binary ^ */
	if(!isint(lv))
		rtwarning(ref, "Binary '^' operator, left-hand-side, should be integer, converted");
	if(!isnone(lv))
		rtwarning(ref, "Binary '^' operator, left-hand-side, has units associated, ignored");
	if(!isint(rv))
		rtwarning(ref, "Binary '^' operator, right-hand-side, should be integer, converted");
	if(!isnone(rv))
		rtwarning(ref, "Binary '^' operator, right-hand-side, has units associated, ignored");
	lvi = value_to_int(lv);
	rvi = value_to_int(rv);
	lv->i = lvi ^ rvi;
	lv->unit = UNIT_NONE;
	return lv;
}

static int funcnamecmp(const void *a, const void *b)
{
	return wcscmp(((const builtins_t *)a)->id, ((const builtins_t *)b)->id);
}

const builtins_t *find_builtin(const wchar_t *s)
{
	const builtins_t *bits = NULL;
	int nbits = 0;
	builtins_t key;
	const builtins_t *func;

	key.id = s;

	bits = builtins_robot;
	nbits = nbuiltins_robot;

	/* Backend dependent functions */
	/* Searching these first allows for overloading */
	if((func = (const builtins_t *)bsearch(&key, bits, nbits, sizeof(*bits), funcnamecmp)))
		return func;

	/* Generic functions */
	if((func = (const builtins_t *)bsearch(&key, builtins_gcmc, nbuiltins_gcmc, sizeof(*builtins_gcmc), funcnamecmp)))
		return func;

	return NULL;
}

static int userfuncnamecmp(const void *a, const void *b)
{
	return wcscmp((*(const node_t **)a)->func.id, (*(const node_t **)b)->func.id);
}

static const function_t *find_userfunc(const wchar_t *s)
{
	if(nfunctions < NSORTFUNC) {
		int i;
		for(i = 0; i < nfunctions; i++) {
			if(!wcscmp(s, functions[i]->func.id))
				return &functions[i]->func;
		}
	} else {
		node_t n;
		node_t *np = &n;
		node_t **npp;
		n.func.id = (wchar_t *)s;
		npp = (node_t **)bsearch(&np, functions, nfunctions, sizeof(*functions), userfuncnamecmp);
		if(npp)
			return &(*npp)->func;
	}
	return NULL;
}

static int is_ident_expr(const node_t *n)
{
	if(n->type != NT_EXPR)
		return 0;
	return n->expr.op == OP_DEREF;
}

static value_t *op_call(node_t *n, int wantret)
{
	value_t *v = NULL;
	int i, j;
	int nargs;
	const builtins_t *bifn;
	const function_t *ft;

	assert(n->expr.args == NULL || n->expr.args->type == NT_EXPRLIST);

	nargs = n->expr.args ? n->expr.args->nlist.n : 0;

	/* Push all arguments onto the value stack */
	for(i = 0; i < nargs; i++) {
		assert(n->expr.args->nlist.nodes[i] != NULL);
		interpret_expr(n->expr.args->nlist.nodes[i], 0, 0);
	}

	/* Check for builtin function */
	if((bifn = find_builtin(n->expr.id))) {
		v = bifn->func(n, nargs);
		if(!v)
			rtinternal(n, "Built-in function '%ls' returned no value", n->expr.id);
		for(i = 0; i < nargs; i++)
			value_delete(value_pop(n));
		if(!wantret) {
			value_delete(v);
			v = NULL;
		}
		return v;
	}

	/* Script defined functions */
	if((ft = find_userfunc(n->expr.id))) {
		assert(!ft->args || ft->args->type == NT_ARGLIST);
		if(ft->args && nargs < ft->args->alist.n) {
			/* We are missing arguments, check and calculate defaults */
			for(j = nargs; j < ft->args->alist.n; j++) {
				if(!ft->args->alist.args[j].expr)
					rterror(n, "Incorrect number of arguments in call to '%ls'", n->expr.id);
				if(ft->args->alist.args[j].isref)
					rtinternal(n, "Call to function '%ls' with argument %d set to default and as reference", n->expr.id, j+1);
				interpret_expr(ft->args->alist.args[j].expr, 0, 0);
				nargs++;
			}
		}
		if((ft->args && ft->args->alist.n != nargs) || (!ft->args && nargs))
			rterror(n, "Incorrect number of arguments in call to '%ls'", n->expr.id);
		assert(ft->args == NULL || nargs == ft->args->alist.n);
		/* Open new scope and push the arguments as local variables */
		variable_scope_push();
		for(j = 0; j < nargs; j++) {
			int revargc = ft->args->alist.n - j - 1;	/* Reverse order processing due to value stack */
			const arg_t *farg = &ft->args->alist.args[revargc];
			if(farg->isref) {
				/* Pass by reference */
				const node_t *narg = n->expr.args->nlist.nodes[revargc];
				value_delete(value_pop(n));	/* Discard the value which got evaluated above */
				if(!is_ident_expr(narg))
					rterror(n, "%ls argument %d: Cannot pass expression by reference", n->expr.id, revargc+1);
				if(!variable_ref_local(farg->id, narg->expr.id))
					rterror(n, "%ls argument %d: No variable by name %ls to map to %ls", n->expr.id, revargc+1, narg->expr.id, farg->id);
			} else {
				/* Pass by value */
				value_t *v = value_pop(n);
				if(isref(v)) {
					value_t *tmp = value_dup(value_deref(v));
					value_delete(v);
					v = tmp;
				}
				if(!variable_set_local(n, farg->id, v))
					rtinternal(n, "Failed to add local variable '%ls'", farg->id);
			}
		}
		v = execute(ft->body, NULL);
		if(wantret) {
			if(v && isref(v)) {
				value_t *tmp = value_dup(value_deref(v));
				value_delete(v);
				v = tmp;
			}
		} else if(v) {
			value_delete(v);
			v = NULL;
		}
		variable_scope_pop();
		return v;
	}

	rterror(n, "Function '%ls' not found", n->expr.id);
	return NULL;
}

static const value_t *op_xassign(const node_t *n, opfunc_t func)
{
	value_t *lv;
	value_t *rv;
	value_t *lvd;

	lv = value_pop(n);	/* Got evaluated right-to-left */
	rv = value_pop(n);
	if(!isref(lv)) {
		rterror(n->expr.left, "Lvalue expected");
		return NULL;
	}
	lvd = (value_t *)value_deref(lv);
	assert(lv != lvd);
	lvd = func(n, lvd, value_deref(rv));
	value_delete(lv);
	value_delete(rv);
	return lvd;
}

static inline void push_dup_or_none(const value_t *v, int istoplevel)
{
	/* Only push a result if not a top-level expression evaluation. At
	 * top-level, no result is required and otherwise needs to be discarded after
	 * being allocated */
	if(!istoplevel)
		value_push(value_dup(v));
}

static void interpret_expr(node_t *n, int islvalue, int istoplevel)
{
	int i;
	value_t *lv;
	value_t *rv;
	const value_t *dlv;
	const value_t *drv;
	int lvb, rvb;
	double lvd, rvd;
	const variable_t *var;

	/* We need a node here because we always need to push something on the value stack */
	assert(n != NULL);

	if(n->type != NT_EXPR)
		rtinternal(n, "Invalid node type %s (%d), expected NT_EXPR", nodetypename(n->type), n->type);

	switch(n->expr.op) {
	case OP_CALL:
		break;
	case OP_ASSIGN:
		/* Right-to-left evaluation */
		assert(n->expr.left != NULL);
		assert(n->expr.right != NULL);
		interpret_expr(n->expr.right, 0, 0);
		interpret_expr(n->expr.left, 1, 0);	/* Indicate to create lvalue variable */
		break;
	case OP_ADDASSIGN:
	case OP_ADDORASSIGN:
	case OP_SUBASSIGN:
	case OP_SUBORASSIGN:
	case OP_MULASSIGN:
	case OP_DIVASSIGN:
	case OP_MODASSIGN:
	case OP_SHLASSIGN:
	case OP_SHRASSIGN:
	case OP_BORASSIGN:
	case OP_BANDASSIGN:
	case OP_BXORASSIGN:
		/* Right-to-left evaluation */
		assert(n->expr.left != NULL);
		assert(n->expr.right != NULL);
		interpret_expr(n->expr.right, 0, 0);
		interpret_expr(n->expr.left, 0, 0);	/* lvalue must exist */
		break;
	case OP_LAND:
	case OP_LOR:
		/* Boolean short-circuit (still left-to-right evaluation) */
		assert(n->expr.left != NULL);
		assert(n->expr.right != NULL);
		interpret_expr(n->expr.left, 0, 0);
		break;
	case OP_CONDEXPR:
		/* Need the condition first, don't yet know which other to evaluate */
		assert(n->expr.cond != NULL);
		assert(n->expr.left != NULL);
		assert(n->expr.right != NULL);
		interpret_expr(n->expr.cond, 0, 0);
		break;
	default:
		/* Left-to-right evaluation */
		if(n->expr.left)
			interpret_expr(n->expr.left, 0, 0);
		if(n->expr.right)
			interpret_expr(n->expr.right, 0, 0);
	}

	switch(n->expr.op) {
	case OP_CALL:
		lv = op_call(n, !istoplevel);
		if(!istoplevel && !lv) {
			rterror(n, "Function call '%ls()' did not return a value, but is used in assignment or calculation", n->expr.id);
			value_push(value_new(VAL_UNDEF));
		} else if(!istoplevel)
			value_push(lv);
		break;
	case OP_ADD:
		assert(istoplevel == 0);
		rv = value_pop(n);
		lv = value_pop(n);
		if(isref(lv)) {
			value_t *tmp = value_dup(value_deref(lv));
			value_delete(lv);
			lv = tmp;
		}
		value_push(op_add(n, lv, value_deref(rv)));
		value_delete(rv);
		break;
	case OP_ADDOR:
		assert(istoplevel == 0);
		rv = value_pop(n);
		lv = value_pop(n);
		if(isref(lv)) {
			value_t *tmp = value_dup(value_deref(lv));
			value_delete(lv);
			lv = tmp;
		}
		value_push(op_addor(n, lv, value_deref(rv)));
		value_delete(rv);
		break;
	case OP_SUB:
		assert(istoplevel == 0);
		rv = value_pop(n);
		lv = value_pop(n);
		if(isref(lv)) {
			value_t *tmp = value_dup(value_deref(lv));
			value_delete(lv);
			lv = tmp;
		}
		value_push(op_sub(n, lv, value_deref(rv)));
		value_delete(rv);
		break;
	case OP_SUBOR:
		assert(istoplevel == 0);
		rv = value_pop(n);
		lv = value_pop(n);
		if(isref(lv)) {
			value_t *tmp = value_dup(value_deref(lv));
			value_delete(lv);
			lv = tmp;
		}
		value_push(op_subor(n, lv, value_deref(rv)));
		value_delete(rv);
		break;
	case OP_MUL:
		assert(istoplevel == 0);
		rv = value_pop(n);
		lv = value_pop(n);
		if(isref(lv)) {
			value_t *tmp = value_dup(value_deref(lv));
			value_delete(lv);
			lv = tmp;
		}
		value_push(op_mul(n, lv, value_deref(rv)));
		value_delete(rv);
		break;
	case OP_SHL:
		assert(istoplevel == 0);
		rv = value_pop(n);
		lv = value_pop(n);
		if(isref(lv)) {
			value_t *tmp = value_dup(value_deref(lv));
			value_delete(lv);
			lv = tmp;
		}
		value_push(op_shl(n, lv, value_deref(rv)));
		value_delete(rv);
		break;
	case OP_SHR:
		assert(istoplevel == 0);
		rv = value_pop(n);
		lv = value_pop(n);
		if(isref(lv)) {
			value_t *tmp = value_dup(value_deref(lv));
			value_delete(lv);
			lv = tmp;
		}
		value_push(op_shr(n, lv, value_deref(rv)));
		value_delete(rv);
		break;
	case OP_DIV:
		assert(istoplevel == 0);
		rv = value_pop(n);
		lv = value_pop(n);
		if(isref(lv)) {
			value_t *tmp = value_dup(value_deref(lv));
			value_delete(lv);
			lv = tmp;
		}
		value_push(op_div(n, lv, value_deref(rv)));
		value_delete(rv);
		break;
	case OP_MOD:
		assert(istoplevel == 0);
		rv = value_pop(n);
		lv = value_pop(n);
		if(isref(lv)) {
			value_t *tmp = value_dup(value_deref(lv));
			value_delete(lv);
			lv = tmp;
		}
		value_push(op_mod(n, lv, value_deref(rv)));
		value_delete(rv);
		break;
	case OP_BAND:
		assert(istoplevel == 0);
		rv = value_pop(n);
		lv = value_pop(n);
		if(isref(lv)) {
			value_t *tmp = value_dup(value_deref(lv));
			value_delete(lv);
			lv = tmp;
		}
		value_push(op_band(n, lv, value_deref(rv)));
		value_delete(rv);
		break;
	case OP_BOR:
		assert(istoplevel == 0);
		rv = value_pop(n);
		lv = value_pop(n);
		if(isref(lv)) {
			value_t *tmp = value_dup(value_deref(lv));
			value_delete(lv);
			lv = tmp;
		}
		value_push(op_bor(n, lv, value_deref(rv)));
		value_delete(rv);
		break;
	case OP_BXOR:
		assert(istoplevel == 0);
		rv = value_pop(n);
		lv = value_pop(n);
		if(isref(lv)) {
			value_t *tmp = value_dup(value_deref(lv));
			value_delete(lv);
			lv = tmp;
		}
		value_push(op_bxor(n, lv, value_deref(rv)));
		value_delete(rv);
		break;
	case OP_BNOT:
		assert(istoplevel == 0);
		lv = value_pop(n);
		dlv = value_deref(lv);
		if(!isint(dlv))
			rtwarning(n->expr.left, "Binary not target should be integer, converted");
		if(!isnone(dlv))
			rtwarning(n->expr.left, "Binary not target has units associated, ignored");
		lvb = value_to_int(dlv);
		value_delete(lv);
		value_push(lv = value_new(VAL_INT));
		lv->i = ~lvb;
		break;
	case OP_INDEX:
	case OP_INDEXID:
		assert(istoplevel == 0);
		/* Left is value to index, right is index */
		rv = value_pop(n);
		lv = value_pop(n);
		dlv = value_deref(lv);
		drv = value_deref(rv);
		if(!isvector(dlv) && !isvectorlist(dlv) && !isstring(dlv))
			rterror(n, "Source for %s operator is not vector, vectorlist or string", n->expr.op == OP_INDEX ? "[]" : ".");
		if(!isscalar(drv))
			rterror(n, "Index of %s operator is not scalar", n->expr.op == OP_INDEX ? "[]" : ".");
		if(drv->unit != UNIT_NONE)
			rtwarning(n, "Index has a unit (%s), ignored", unitname(drv->unit));

		i = value_to_int(drv);
		if(isvector(dlv)) {
			if(islvalue && (i < -dlv->v.n || i >= dlv->v.n)) {
				/* This is a vector on the left of '=', create the missing entry/entries */
				int n = i < 0 ? (-i-1) : i;
				while(dlv->v.n <= n) {
					value_vector_add_undef((value_t *)dlv);
				}
			}
			if(i < -dlv->v.n || i >= dlv->v.n) {
				if(dlv->v.n)
					rtwarning(n, "Vector index out of range (%d outside [%d..%d])", i, -dlv->v.n, dlv->v.n-1);
				else
					rtwarning(n, "Vector index out of range (vector has no entries)");
				value_push(value_new(VAL_UNDEF));
			} else if(isref(lv)) {
				/* If LV is a reference, push a reference again */
				if(i < 0)
					value_push(value_new_ref(dlv->v.vals[dlv->v.n+i]));
				else
					value_push(value_new_ref(dlv->v.vals[i]));
			} else {
				/* Else LV is a constant, just copy the value */
				if(i < 0)
					value_push(value_dup(dlv->v.vals[dlv->v.n+i]));
				else
					value_push(value_dup(dlv->v.vals[i]));
			}
		} else if(isvectorlist(dlv)) { 
			if(islvalue && (i < -dlv->vl.n || i >= dlv->vl.n)) {
				/* This is a vectorlist on the left of '=', create the missing entry/entries */
				int n = i < 0 ? (-i-1) : i;
				while(dlv->vl.n <= n) {
					value_vectorlist_add((value_t *)dlv, value_new(VAL_VECTOR));
				}
			}
			if(i < -dlv->vl.n || i >= dlv->vl.n) {
				if(dlv->vl.n)
					rtwarning(n, "Vectorlist index out of range (%d outside [%d..%d])", i, -dlv->vl.n, dlv->vl.n-1);
				else
					rtwarning(n, "Vectorlist index out of range (vectorlist has no entries)");
				value_push(value_new(VAL_VECTOR));
			} else if(isref(lv)) {
				/* If LV is a reference, push a reference again */
				if(i < 0)
					value_push(value_new_ref(dlv->vl.vecs[dlv->vl.n+i]));
				else
					value_push(value_new_ref(dlv->vl.vecs[i]));
			} else {
				if(i < 0)
					value_push(value_dup(dlv->vl.vecs[dlv->vl.n+i]));
				else
					value_push(value_dup(dlv->vl.vecs[i]));
			}
		} else {
			/* String */
			if(islvalue) {
				/*
				 * This is a string on the left of '='. The
				 * representation has no concept of characters
				 * in the grammar, so we cannot simply assign
				 * to it.
				 */
				rterror(n, "Strings cannot be indexed as lvalues");
				value_push(value_new(VAL_UNDEF));
			} else {
				if(i < -dlv->str.n || i >= dlv->str.n) {
					if(dlv->str.n)
						rtwarning(n, "String index out of range (%d outside [%d..%d])", i, -dlv->str.n, dlv->str.n-1);
					else
						rtwarning(n, "String index out of range (string is empty)");
					value_push(value_new(VAL_UNDEF));
				} else {
					/* Return the character as a string of length one */
					wchar_t chs[2];
					string_t s;
					if(i < 0)
						chs[0] = dlv->str.chs[dlv->str.n+i];
					else
						chs[0] = dlv->str.chs[i];
					chs[1] = 0;
					s.n = 1;
					s.na = 2;
					s.chs = chs;
					value_push(value_new_str(&s));
				}
			}
		}
		value_delete(lv);
		value_delete(rv);
		break;
	case OP_LOR:
	case OP_LAND:
		assert(istoplevel == 0);
		/* Only LHS got evaluated for logical ops */
		lv = value_pop(n);
		lvb = value_to_boolean(lv);
		value_delete(lv);
		/* Short-circuit the boolean operation */
		if(n->expr.op == OP_LAND && !lvb) {
			/* false && <anything> is always false */
			value_push(lv = value_new(VAL_INT));
			lv->i = 0;
			break;
		} else if(n->expr.op == OP_LOR && lvb) {
			/* true || <anything> is always true */
			value_push(lv = value_new(VAL_INT));
			lv->i = 1;
			break;
		}
		/* Undecided on LHS, evaluate RHS */
		interpret_expr(n->expr.right, 0, 0);
		rv = value_pop(n);
		rvb = value_to_boolean(rv);
		value_delete(rv);
		value_push(lv = value_new(VAL_INT));
		switch(n->expr.op) {
		case OP_LOR:	lv->i = lvb || rvb; break;
		case OP_LAND:	lv->i = lvb && rvb; break;
		default:	lv->i = 0; break;
		}
		break;
	case OP_EQ:
	case OP_NE:
	case OP_GT:
	case OP_LT:
	case OP_GE:
	case OP_LE:
		assert(istoplevel == 0);
		rv = value_pop(n);
		lv = value_pop(n);
		drv = value_deref(rv);
		dlv = value_deref(lv);
		if(isvectorlist(dlv) || isvectorlist(drv)) {
			rtwarning(n, "Comparison of vectorlists not supported, result is always false");
compare_error:
			value_delete(rv);
			value_delete(lv);
			value_push(lv = value_new_int(0, UNIT_NONE));
			break;
		}
		if((isvector(dlv) && !isvector(drv)) || (!isvector(dlv) && isvector(drv))) {
			rtwarning(n, "Comparison of vector with non-vector not supported, result is always false");
			goto compare_error;
		}
		if(isstring(dlv) && isstring(drv)) {
			/* String comparison */
			lvb = wcscmp(dlv->str.chs, drv->str.chs);
			value_delete(rv);
			value_delete(lv);
			value_push(lv = value_new(VAL_INT));
			switch(n->expr.op) {
			case OP_EQ:	lv->i = !lvb; break;
			case OP_NE:	lv->i = !!lvb; break;
			case OP_GT:	lv->i = lvb > 0; break;
			case OP_LT:	lv->i = lvb < 0; break;
			case OP_GE:	lv->i = lvb >= 0; break;
			case OP_LE:	lv->i = lvb <= 0; break;
			default:	lv->i = 0; break;
			}
			break;
		}

		if(isvector(dlv) && isvector(drv)) {
			if(n->expr.op != OP_EQ && n->expr.op != OP_NE) {
				rtwarning(n, "Comparison of vectors only supports '==' and '!=', result is always false");
				goto compare_error;
			}
			if(dlv->v.n != drv->v.n) {
				rtwarning(n, "Comparison of vectors with unequal nr. of entries is always false");
				goto compare_error;
			}
			lvb = 1;
			for(i = 0; i < dlv->v.n; i++) {
				if(isundef(dlv->v.vals[i]) || isundef(drv->v.vals[i])) {
					if(isundef(dlv->v.vals[i]) && isundef(drv->v.vals[i])) {
						lvd = rvd = 0.0;
					} else {
						lvd = 1.0;
						rvd = -1.0;
					}
				} else if((isdistance(dlv->v.vals[i]) && isangle(drv->v.vals[i])) || (isangle(dlv->v.vals[i]) && isdistance(drv->v.vals[i]))) {
					rtwarning(n, "Unit mismatch in vector comparison, converted to <none> (entry %d: LHS=%s, RHS=%s)", i, unitname(dlv->unit), unitname(drv->unit));
					rvd = value_to_double(drv->v.vals[i]);
					lvd = value_to_double(dlv->v.vals[i]);
				} else {
					if((isnone(dlv->v.vals[i]) && !isnone(drv->v.vals[i])) || (!isnone(dlv->v.vals[i]) && isnone(drv->v.vals[i])))
						rtwarning(n, "Possible unit mismatch in vector; comparing units %s with none in entry %d", unitname(isnone(dlv) ? drv->unit : dlv->unit), i);
					if(isdistance(dlv->v.vals[i]) || isdistance(drv->v.vals[i])) {
						rvd = value_to_dist(drv->v.vals[i]);
						lvd = value_to_dist(dlv->v.vals[i]);
					} else if(isangle(dlv->v.vals[i]) || isangle(drv->v.vals[i])) {
						rvd = value_to_deg(drv->v.vals[i]);
						lvd = value_to_deg(dlv->v.vals[i]);
					} else {
						rvd = value_to_double(drv->v.vals[i]);
						lvd = value_to_double(dlv->v.vals[i]);
					}
				}
				lvb = lvb && fabs(lvd - rvd) < EPSILON;
			}
			value_delete(rv);
			value_delete(lv);
			value_push(lv = value_new(VAL_INT));
			lv->i = n->expr.op == OP_EQ ? lvb : !lvb;
			break;
		}

		if(!isscalar(dlv) || !isscalar(drv)) {
			rtwarning(n, "Comparison of non-scalar not supported, result is always false");
			goto compare_error;
		}
		/* From here on we have "scalar <op> scalar" */
		if((isdistance(dlv) && isangle(drv)) || (isangle(dlv) && isdistance(drv))) {
			rtwarning(n, "Unit mismatch in comparison, converted to <none> (LHS=%s, RHS=%s)", unitname(dlv->unit), unitname(drv->unit));
			rvd = value_to_double(drv);
			lvd = value_to_double(dlv);
		} else {

			if((isnone(dlv) && !isnone(drv)) || (!isnone(dlv) && isnone(drv)))
				rtwarning(n, "Possible unit mismatch; comparing units %s with none", unitname(isnone(dlv) ? drv->unit : dlv->unit));
			if(isdistance(dlv) || isdistance(drv)) {
				rvd = value_to_dist(drv);
				lvd = value_to_dist(dlv);
			} else if(isangle(dlv) || isangle(drv)) {
				rvd = value_to_deg(drv);
				lvd = value_to_deg(dlv);
			} else {
				rvd = value_to_double(drv);
				lvd = value_to_double(dlv);
			}
		}
		value_delete(rv);
		value_delete(lv);
		value_push(lv = value_new(VAL_INT));
		switch(n->expr.op) {
		case OP_EQ:	lv->i = fabs(lvd - rvd) < EPSILON; break;
		case OP_NE:	lv->i = fabs(lvd - rvd) >= EPSILON; break;
		case OP_GT:	lv->i = (lvd - rvd) > EPSILON; break;
		case OP_LT:	lv->i = (lvd - rvd) < -EPSILON; break;
		case OP_GE:	lv->i = (lvd - rvd) > -EPSILON; break;
		case OP_LE:	lv->i = (lvd - rvd) < EPSILON; break;
		default:	lv->i = 0; break;
		}
		break;
	case OP_NOT:
		assert(istoplevel == 0);
		lv = value_pop(n);
		lvb = value_to_boolean(lv);
		value_delete(lv);
		value_push(lv = value_new(VAL_INT));
		lv->i = !lvb;
		break;
	case OP_INT:
		assert(istoplevel == 0);
		value_push(value_new_int(n->expr.i, n->expr.unit));
		break;
	case OP_FLOAT:
		assert(istoplevel == 0);
		value_push(value_new_flt(n->expr.d, n->expr.unit));
		break;
	case OP_STRING:
		assert(istoplevel == 0);
		value_push(value_new_str(&n->expr.str));
		break;
	case OP_VECTOR:
		assert(istoplevel == 0);
		lv = value_new(VAL_VECTOR);
		for(i = 0; i < n->expr.nlist.n; i++) {
			if(!n->expr.nlist.nodes[i])
				value_vector_add_undef(lv);
			else {
				interpret_expr(n->expr.nlist.nodes[i], 0, 0);
				rv = value_pop(n->expr.nlist.nodes[i]);
				assert(rv != NULL);
				if(isref(rv)) {
					value_t *tmp = value_dup(value_deref(rv));
					value_delete(rv);
					rv = tmp;
				}
				if(!isscalar(rv))
					rterror(n, "Vector entries must be scalar");
				value_vector_add(lv, rv);
			}
		}
		value_push(lv);
		break;
	case OP_VECTORLIST:
		assert(istoplevel == 0);
		lv = value_new(VAL_VECTORLIST);
		for(i = 0; i < n->expr.nlist.n; i++) {
			interpret_expr(n->expr.nlist.nodes[i], 0, 0);
			rv = value_pop(n->expr.nlist.nodes[i]);
			assert(rv != NULL);
			if(isref(rv)) {
				value_t *tmp = value_dup(value_deref(rv));
				value_delete(rv);
				rv = tmp;
			}
			if(rv->type != VAL_VECTOR)
				rterror(n, "Vectorlist entries must be vectors");
			value_vectorlist_add(lv, rv);
		}
		value_push(lv);
		break;
	case OP_ASSIGN:
		lv = value_pop(n);	/* Got evaluated right-to-left */
		rv = value_pop(n);
		if(!isref(lv)) {
			rterror(n->expr.left, "Lvalue expected");
			break;
		}
		if(isref(rv)) {
			value_t *tmp = value_dup(value_deref(rv));
			value_delete(rv);
			rv = tmp;
		}
		push_dup_or_none(value_replace(lv, rv), istoplevel);
		value_delete(rv);
		break;
	case OP_ADDASSIGN:
		push_dup_or_none(op_xassign(n, op_add), istoplevel);
		break;
	case OP_ADDORASSIGN:
		push_dup_or_none(op_xassign(n, op_addor), istoplevel);
		break;
	case OP_SUBASSIGN:
		push_dup_or_none(op_xassign(n, op_sub), istoplevel);
		break;
	case OP_SUBORASSIGN:
		push_dup_or_none(op_xassign(n, op_subor), istoplevel);
		break;
	case OP_MULASSIGN:
		push_dup_or_none(op_xassign(n, op_mul), istoplevel);
		break;
	case OP_DIVASSIGN:
		push_dup_or_none(op_xassign(n, op_div), istoplevel);
		break;
	case OP_MODASSIGN:
		push_dup_or_none(op_xassign(n, op_mod), istoplevel);
		break;
	case OP_SHLASSIGN:
		push_dup_or_none(op_xassign(n, op_shl), istoplevel);
		break;
	case OP_SHRASSIGN:
		push_dup_or_none(op_xassign(n, op_shr), istoplevel);
		break;
	case OP_BORASSIGN:
		push_dup_or_none(op_xassign(n, op_bor), istoplevel);
		break;
	case OP_BANDASSIGN:
		push_dup_or_none(op_xassign(n, op_band), istoplevel);
		break;
	case OP_BXORASSIGN:
		push_dup_or_none(op_xassign(n, op_bxor), istoplevel);
		break;
	case OP_PREINC:
	case OP_POSTINC:
		lv = value_pop(n);
		dlv = value_deref(lv);
		if(n->expr.op == OP_POSTINC)
			push_dup_or_none(dlv, istoplevel);
		switch(dlv->type) {
		case VAL_INT:	((value_t *)dlv)->i++; break;
		case VAL_FLOAT:	((value_t *)dlv)->d += 1.0; break;
		default:
			rtwarning(n, "Increment on non-scalar (%s) has no effect", valuetypename(dlv->type));
			break;
		}
		if(n->expr.op == OP_PREINC)
			push_dup_or_none(dlv, istoplevel);
		value_delete(lv);
		break;
	case OP_PREDEC:
	case OP_POSTDEC:
		lv = value_pop(n);
		dlv = value_deref(lv);
		if(n->expr.op == OP_POSTDEC)
			push_dup_or_none(dlv, istoplevel);
		switch(dlv->type) {
		case VAL_INT:	((value_t *)dlv)->i--; break;
		case VAL_FLOAT:	((value_t *)dlv)->d -= 1.0; break;
		default:
			rtwarning(n, "Decrement on non-scalar (%s) has no effect", valuetypename(dlv->type));
			break;
		}
		if(n->expr.op == OP_PREDEC)
			push_dup_or_none(dlv, istoplevel);
		value_delete(lv);
		break;
	case OP_DEREF:
		assert(istoplevel == 0);
		if(!(var = variable_lookup_deref(n->expr.id, 0))) {
			if(islvalue) {
				/* If this is an lvalue (from assignment), then create the variable */
				variable_set(n->expr.id, lv = value_new(VAL_UNDEF));
				value_push(value_new_ref(lv));
			} else {
				rterror(n, "Variable '%ls' undefined", n->expr.id);
				value_push(value_new(VAL_UNDEF));
			}
		} else {
			if(islvalue && var->isconst) {
				/* Distinguish in case we pass a const var by reference */
				if(wcscmp(n->expr.id, var->id))
					rterror(n, "Attempt to assign to const variable '%ls' (from '%ls')", n->expr.id, var->id);
				else
					rterror(n, "Attempt to assign to const variable '%ls'", n->expr.id);
				value_push(value_new(VAL_UNDEF));
			} else {
				lv = value_new_ref(var->v);
				value_push(lv);
			}
		}
		break;
	case OP_CONDEXPR:	/* ?: */
		assert(istoplevel == 0);
		lv = value_pop(n);	/* Condition got evaluated above */
		lvb = value_to_boolean(lv);
		value_delete(lv);
		if(lvb)
			interpret_expr(n->expr.left, 0, 0);	/* true clause */
		else
			interpret_expr(n->expr.right, 0, 0);	/* false clause */
		if(isref(value_tos(n))) {
			/* The result of ?: is always a value, never a reference */
			value_t *tmp;
			lv = value_pop(n);
			tmp = value_dup(value_deref(lv));
			value_delete(lv);
			value_push(tmp);
		}
		break;
	default:
		rtinternal(n, "Invalid expression operation %s (%d)", expropname(n->expr.op), n->expr.op);
		break;
	}
}

static value_t *op_foreach(node_t *n)
{
	int i;
	int nv;
	value_t *v = NULL;
	value_t *src;
	const value_t *dsrc;
	node_t *brk = NULL;
	assert(n->lfe.src->type == NT_EXPR);
	interpret_expr(n->lfe.src, 0, 0);
	src = value_pop(n);
	dsrc = value_deref(src);
	if(!isvectorlist(dsrc) && !isvector(dsrc))
		rterror(n, "Foreach's source must be vector or vectorlist");
	nv = isvector(dsrc) ? dsrc->v.n : dsrc->vl.n;
	for(i = 0; i < nv; i++) {
		value_t *val = value_dup(isvector(dsrc) ? dsrc->v.vals[i]: dsrc->vl.vecs[i]);
		variable_set(n->lfe.dst, val);
		v = execute(n->lfe.stmts, &brk);
		if(v)
			break;
		if(brk && brk->type == NT_BREAK)
			break;
		if(brk && brk->type == NT_CONTINUE)
			brk = NULL;
	}
	value_delete(src);
	return v;
}

static value_t *op_repeat(node_t *n)
{
	int repeats;
	int step;
	int i;
	value_t *v = NULL;
	value_t *src;
	const value_t *dsrc;
	node_t *brk = NULL;
	assert(n->lfe.src->type == NT_EXPR);
	interpret_expr(n->lfe.src, 0, 0);
	src = value_pop(n);
	dsrc = value_deref(src);
	if(!isscalar(dsrc))
		rterror(n, "Repeat's source must be scalar");
	if(!isnone(dsrc))
		rtwarning(n, "Repeat's source has units associated, ignored");
	if(isfloat(dsrc) && fabs(src->d - round(src->d)) > EPSILON)
		rtwarning(n, "Repeat's source is floating point and has a non-integer fraction, truncated to integer");
	repeats = value_to_int(src);

	if(repeats < 0) {
		repeats = -repeats;
		step = -1;
	} else
		step = 1;

	for(i = 0; i < repeats; i++) {
		if(n->lfe.dst) {
			value_t *val = value_new_int((i+1) * step, UNIT_NONE);
			variable_set(n->lfe.dst, val);
		}
		v = execute(n->lfe.stmts, &brk);
		if(v)
			break;
		if(brk && brk->type == NT_BREAK)
			break;
		if(brk && brk->type == NT_CONTINUE)
			brk = NULL;
	}
	value_delete(src);
	return v;
}

static int func_sort(const void *a, const void *b)
{
	return wcscmp((*(const node_t **)a)->func.id, (*(const node_t **)b)->func.id);
}

static void function_add(node_t *n)
{
	int i;
	assert(n->type == NT_FUNCTION);
	testalloc((void **)&functions, nfunctions, &nafunctions, sizeof(functions[0]));
	for(i = 0; i < nfunctions; i++) {
		if(!wcscmp(functions[i]->func.id, n->func.id)) {
			rterror(n, "Function '%ls' already defined", n->func.id);
			return;
		}
	}
	if(find_builtin(n->func.id)) {
		rterror(n, "Function '%ls' is a built-in function", n->func.id);
		return;
	}
	functions[nfunctions] = n;
	nfunctions++;
	qsort(functions, nfunctions, sizeof(*functions), func_sort);
}

static value_t *op_for(node_t *n)
{
	node_t *brk = NULL;
	value_t *v;
	int b;
	assert(n->lfor.cond->type == NT_EXPR);

	if(n->lfor.init) {
		if(execute(n->lfor.init, NULL))
			rterror(n, "For-loop initialization should not return a value");
	}
	while(1) {
		interpret_expr(n->lfor.cond, 0, 0);
		v = value_pop(n);
		b = value_to_boolean(v);
		value_delete(v);
		if(!b)
			break;
		if((v = execute(n->lfor.stmts, &brk)))
			return v;
		if(brk && brk->type == NT_BREAK)
			break;
		if(brk && brk->type == NT_CONTINUE)
			brk = NULL;
		if(n->lfor.inc) {
			if(execute(n->lfor.inc, NULL))
				rterror(n, "For-loop increment should not return a value");
		}
	}
	return NULL;
}

static value_t *op_while(node_t *n)
{
	node_t *brk = NULL;
	value_t *v;
	int b;
	assert(n->lfor.cond->type == NT_EXPR);

	while(1) {
		interpret_expr(n->lfor.cond, 0, 0);
		v = value_pop(n);
		b = value_to_boolean(v);
		value_delete(v);
		if(!b)
			break;
		if((v = execute(n->lfor.stmts, &brk)))
			return v;
		if(brk && brk->type == NT_BREAK)
			break;
		if(brk && brk->type == NT_CONTINUE)
			brk = NULL;
	}
	return NULL;
}

static value_t *op_dowhile(node_t *n)
{
	node_t *brk = NULL;
	value_t *v;
	int b;
	assert(n->lfor.cond->type == NT_EXPR);

	while(1) {
		if((v = execute(n->lfor.stmts, &brk)))
			return v;
		if(brk && brk->type == NT_BREAK)
			break;
		if(brk && brk->type == NT_CONTINUE)
			brk = NULL;
		else {
			interpret_expr(n->lfor.cond, 0, 0);
			v = value_pop(n);
			b = value_to_boolean(v);
			value_delete(v);
			if(!b)
				break;
		}
	}
	return NULL;
}

static value_t *op_if(node_t *n, node_t **brkref)
{
	value_t *v;
	int b;
	assert(n->cond.cond->type == NT_EXPR);

	interpret_expr(n->cond.cond, 0, 0);
	v = value_pop(n);
	b = value_to_boolean(v);
	value_delete(v);
	if(b) {
		if((v = execute(n->cond.ifclause, brkref)))
			return v;
	} else if(n->cond.elifclauses) {
		node_t *e;
		for(e = n->cond.elifclauses; e; e = e->next) {
			assert(e->type == NT_ELIF);
			interpret_expr(e->cond.cond, 0, 0);
			v = value_pop(n);
			b = value_to_boolean(v);
			value_delete(v);
			if(b) {
				if((v = execute(e->cond.ifclause, brkref)))
					return v;
				break;
			}
		}
		/* Execute any trailing else-clause if no elif matches */
		if(!b && n->cond.elseclause) {
			if((v = execute(n->cond.elseclause, brkref)))
				return v;
		}
	} else if(n->cond.elseclause) {
		if((v = execute(n->cond.elseclause, brkref)))
			return v;
	}
	return NULL;
}

static value_t *interpret(node_t *n, node_t **brkref)
{
	value_t *v;

	if(!n)
		return NULL;

	switch(n->type) {
	case NT_EXPRLIST:	/* Only as call argument */
	case NT_ARGLIST:	/* Only as function arg definition */
	case NT_BREAK:		/* Only in loops */
	case NT_CONTINUE:	/* Only in loops */
	default:
		rtinternal(n, "Invalid node type %s (%d)", nodetypename(n->type), n->type);
		break;
	case NT_EXPR:		interpret_expr(n, 0, 1); break;
	case NT_IF:		return op_if(n, brkref);
	case NT_FOR:		return op_for(n);
	case NT_FOREACH:	return op_foreach(n);
	case NT_REPEAT:		return op_repeat(n);
	case NT_WHILE:		return op_while(n);
	case NT_DOWHILE:	return op_dowhile(n);
	case NT_FUNCTION:	function_add(n); break;
	case NT_RETURN:
		if(n->eref) {
			interpret_expr(n->eref, 0, 0);
			return value_pop(n);
		} else
			return value_new(VAL_UNDEF);
	case NT_LOCAL:
		if(n->lvar.init) {
			interpret_expr(n->lvar.init, 0, 0);
			v = value_pop(n);
			if(isref(v)) {
				value_t *tmp = value_dup(value_deref(v));
				value_delete(v);
				v = tmp;
			}
		} else
			v = value_new(VAL_UNDEF);
		variable_set_local(n, n->lvar.id, v);
		break;

	case NT_CONST:
		if(n->cvar.init) {
			interpret_expr(n->cvar.init, 0, 0);
			v = value_pop(n);
			if(isref(v)) {
				value_t *tmp = value_dup(value_deref(v));
				value_delete(v);
				v = tmp;
			}
		} else {
			rtwarning(n, "Const var '%ls' has no value assigned, setting to undef.", n->cvar.id);
			v = value_new(VAL_UNDEF);
		}
		variable_set_const(n, n->cvar.id, v);
		break;
	}
	return NULL;
}

value_t *execute(node_t *head, node_t **brkref)
{
	node_t *n;
	value_t *v;
	for(n = head; n; n = n->next) {
		if(n->type == NT_BREAK || n->type == NT_CONTINUE) {
			if(brkref) {
				*brkref = n;
				return NULL;
			} else
				rterror(n, "Unexpected break or continue");
		}
		if((v = interpret(n, brkref)))
			return v;
		if(brkref && *brkref)
			return NULL;
	}
	return NULL;
}

void interpreter_cleanup(void)
{
	if(functions) {
		free(functions);
		functions = NULL;
		nfunctions = nafunctions = 0;
	}
}
