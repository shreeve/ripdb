/* cursors_delete.c - memory-mapped database tester/toy */
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
#define RES(err, expr) ((rc = expr) == (err) || (CHECK(!rc, #expr), 0))
#define CHECK(test, msg) ((test) ? (void)0 : ((void)fprintf(stderr, \
	"%s:%d: %s: %s\n", __FILE__, __LINE__, msg, rdb_strerror(rc)), abort()))

int main(int argc,char * argv[])
{
	int i = 0, j = 0, rc;
	RDB_env *env;
	RDB_dbi dbi;
	RDB_val key, data;
	RDB_txn *txn;
	RDB_stat mst;
	RDB_cursor *cursor, *cur2;
	RDB_cursor_op op;

	E(rdb_env_create(&env));
	E(rdb_env_set_maxreaders(env, 1));
	E(rdb_env_set_mapsize(env, 10485760));
	E(rdb_env_open(env, "./tests/db", RDB_NOSYNC, 0664));

	E(rdb_txn_begin(env, NULL, 0, &txn));
	E(rdb_dbi_open(txn, NULL, 0, &dbi));
	key.mv_size = sizeof(int);
	key.mv_data = &i;

	printf("Adding 100 values\n");
	for (i=0;i<100;i++) {
		data.mv_size = sizeof(int);
		data.mv_data = &i;
		if (RES(RDB_KEYEXIST, rdb_put(txn, dbi, &key, &data, RDB_NOOVERWRITE)))
			j++;
	}
	if (j) printf("%d duplicates skipped\n", j);
	E(rdb_txn_commit(txn));
	E(rdb_env_stat(env, &mst));

	E(rdb_txn_begin(env, NULL, RDB_RDONLY, &txn));
	E(rdb_cursor_open(txn, dbi, &cursor));
	while ((rc = rdb_cursor_get(cursor, &key, &data, RDB_NEXT)) == 0) {
		printf("key: %p %.*s, data: %p %.*s\n",
			key.mv_data,  (int) key.mv_size,  (char *) key.mv_data,
			data.mv_data, (int) data.mv_size, (char *) data.mv_data);
	}
	CHECK(rc == RDB_NOTFOUND, "rdb_cursor_get");
	rdb_cursor_close(cursor);
	rdb_txn_abort(txn);

	E(rdb_txn_begin(env, NULL, 0, &txn));
	E(rdb_cursor_open(txn, dbi, &cur2));
	for (i=0; i<50; i++) {
		if (RES(RDB_NOTFOUND, rdb_cursor_get(cur2, &key, &data, RDB_NEXT)))
			break;
		printf("key: %p %.*s, data: %p %.*s\n",
			key.mv_data,  (int) key.mv_size,  (char *) key.mv_data,
			data.mv_data, (int) data.mv_size, (char *) data.mv_data);
		E(rdb_del(txn, dbi, &key, NULL));
	}

	printf("Restarting cursor in txn\n");
	for (op=RDB_FIRST, i=0; i<=32; op=RDB_NEXT, i++) {
		if (RES(RDB_NOTFOUND, rdb_cursor_get(cur2, &key, &data, op)))
			break;
		printf("key: %p %.*s, data: %p %.*s\n",
			key.mv_data,  (int) key.mv_size,  (char *) key.mv_data,
			data.mv_data, (int) data.mv_size, (char *) data.mv_data);
	}
	rdb_cursor_close(cur2);
	E(rdb_txn_commit(txn));

	printf("Restarting cursor outside txn\n");
	E(rdb_txn_begin(env, NULL, 0, &txn));
	E(rdb_cursor_open(txn, dbi, &cursor));
	for (op=RDB_FIRST, i=0; i<=32; op=RDB_NEXT, i++) {
		if (RES(RDB_NOTFOUND, rdb_cursor_get(cursor, &key, &data, op)))
			break;
		printf("key: %p %.*s, data: %p %.*s\n",
			key.mv_data,  (int) key.mv_size,  (char *) key.mv_data,
			data.mv_data, (int) data.mv_size, (char *) data.mv_data);
	}
	rdb_cursor_close(cursor);
	rdb_txn_abort(txn);

	rdb_env_close(env);
	return 0;
}
