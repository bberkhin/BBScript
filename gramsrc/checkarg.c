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

#include "utils.h"
#include "grammartypes.h"
#include "value.h"
#include "checkarg.h"

void check_arg_none(const node_t *ref, const char *id, int argc)
{
	if(argc > 0)
		rtwarning(ref, "%s() has no arguments", id);
}

void check_arg_one(const node_t *ref, const char *id, int argc)
{
	if(argc < 1)
		rterror(ref, "%s() requires one argument", id);
}

void check_arg_two(const node_t *ref, const char *id, int argc)
{
	if(argc < 2)
		rterror(ref, "%s() requires two arguments", id);
}

void check_arg_three(const node_t *ref, const char *id, int argc)
{
	if(argc < 3)
		rterror(ref, "%s() requires three arguments", id);
}

void check_arg_scalar(const node_t *ref, const char *id, const value_t *arg, int argn)
{
	if(arg->type != VAL_INT && arg->type != VAL_FLOAT)
		rterror(ref, "%s() argument %d must be scalar", id, argn+1);
}

void check_arg_vector(const node_t *ref, const char *id, const value_t *arg, int argn)
{
	if(arg->type != VAL_VECTOR)
		rterror(ref, "%s() argument %d must be vector", id, argn+1);
}

void check_arg_string(const node_t *ref, const char *id, const value_t *arg, int argn)
{
	if(arg->type != VAL_STRING)
		rterror(ref, "%s() argument %d must be string", id, argn+1);
}

void check_arg_unit_none(const node_t *ref, const char *id, const value_t *arg, int argn)
{
	if(arg->unit != UNIT_NONE)
		rtwarning(ref, "%s() argument %d has units attached", id, argn+1);
}

void check_arg_unit_angle(const node_t *ref, const char *id, const value_t *arg, int argn)
{
	if(arg->unit != UNIT_NONE && arg->unit != UNIT_DEG && arg->unit != UNIT_RAD)
		rtwarning(ref, "%s() argument %d has incompatible distance units attached", id, argn+1);
}

void check_arg_unit_distance(const node_t *ref, const char *id, const value_t *arg, int argn)
{
	if(arg->unit != UNIT_NONE && arg->unit != UNIT_MM && arg->unit != UNIT_IN)
		rtwarning(ref, "%s() argument %d has incompatible angular units attached", id, argn+1);
}

void check_range_double(const node_t *ref, const char *id, int argn, double val, double mini, double maxi)
{
	if(val < mini || val > maxi)
		rtwarning(ref, "%s() argument %d out of range [%f,%f]", id, argn+1, mini, maxi);
}
