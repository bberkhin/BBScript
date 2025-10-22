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
#ifndef __GCMC_CHECKARG_H
#define __GCMC_CHECKARG_H

void check_arg_none(const node_t *ref, const char *id, int argc);
void check_arg_one(const node_t *ref, const char *id, int argc);
void check_arg_two(const node_t *ref, const char *id, int argc);
void check_arg_three(const node_t *ref, const char *id, int argc);
void check_arg_scalar(const node_t *ref, const char *id, const value_t *arg, int argn);
void check_arg_vector(const node_t *ref, const char *id, const value_t *arg, int argn);
void check_arg_string(const node_t *ref, const char *id, const value_t *arg, int argn);
void check_arg_unit_none(const node_t *ref, const char *id, const value_t *arg, int argn);
void check_arg_unit_angle(const node_t *ref, const char *id, const value_t *arg, int argn);
void check_arg_unit_distance(const node_t *ref, const char *id, const value_t *arg, int argn);
void check_range_double(const node_t *ref, const char *id, int argn, double val, double mini, double maxi);

#endif
