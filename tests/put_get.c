/* put_get.c - memory-mapped database tester/toy */
/*
 * Copyright 2011-2021 Howard Chu, Symas Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted only as authorized by the OpenLDAP
 * Public License.
 *
 * A copy of this license is available in the file LICENSE in the
 * top-level directory of the distribution or, alternatively, at
 * <http://www.OpenLDAP.org/license.html>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ripdb.h"

#define E(expr) CHECK((rc = (expr)) == RDB_SUCCESS, #expr)
#define CHECK(test, msg) ((test) ? (void)0 : ((void)fprintf(stderr, \
	"%s:%d: %s: %s\n", __FILE__, __LINE__, msg, rdb_strerror(rc)), abort()))

int main(int argc,char * argv[])
{
	int rc;
	RDB_env *env;
	RDB_dbi dbi;
	RDB_val key, data;
	RDB_txn *txn;

	E(rdb_env_create(&env));
	E(rdb_env_set_maxreaders(env, 1));
	E(rdb_env_set_mapsize(env, 10485760));
	E(rdb_env_open(env, "./tests/db", RDB_NOSYNC, 0664));

	E(rdb_txn_begin(env, NULL, 0, &txn));
	E(rdb_dbi_open(txn, NULL, 0, &dbi));

	key.mv_size = strlen("foo");
	key.mv_data = "foo";
	data.mv_size = strlen("bar");
	data.mv_data = "bar";
	E(rdb_put(txn, dbi, &key, &data, 0));

	E(rdb_txn_commit(txn));
	E(rdb_txn_begin(env, NULL, RDB_RDONLY, &txn));
	E(rdb_get(txn, dbi, &key, &data));
	printf("key: %s, data: %s\n", (char *)key.mv_data, (char *)data.mv_data);
	rdb_txn_abort(txn);

	rdb_dbi_close(env, dbi);
	rdb_env_close(env);

	return 0;
}
