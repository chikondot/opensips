/*
 * Copyright (C) 2019 - OpenSIPS Project
 *
 * This file is part of opensips, a free SIP server.
 *
 * opensips is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * opensips is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 *
 */

#ifndef _PROTO_DB_H_
#define _PROTO_DB_H_

#include "../../db/db.h"
#include "../../str.h"

int smpp_db_bind(const str *db_url);
int smpp_db_init(const str *db_url);
int smpp_query(const str *smpp_table, db_key_t *cols, int col_nr, db_res_t **res);
void smpp_free_results(db_res_t *res);
void smpp_db_close(void);

#endif