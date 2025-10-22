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
#ifndef __GCMC_INTERPRETER_H
#define __GCMC_INTERPRETER_H

value_t *execute(node_t *head, node_t **brkref);
void interpreter_cleanup(void);

value_t *op_mul(const node_t *ref, value_t *lv, const value_t *rv);

const char *nodetypename(node_et n);
const char *expropname(op_et n);
const char *unitname(int n);

#endif
