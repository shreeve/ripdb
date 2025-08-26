/* secondary_db.c - memory-mapped database tester/toy */
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

/* Just like mtest.c, but using a subDB instead of the main DB */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
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
	RDB_cursor *cursor;
	int count;
	int *values;
	char sval[32] = "";

	srand(time(NULL));

	count = (rand()%384) + 64;
	values = (int *)malloc(count*sizeof(int));

	for(i = 0;i<count;i++) {
		values[i] = rand()%1024;
	}

	E(rdb_env_create(&env));
	E(rdb_env_set_maxreaders(env, 1));
	E(rdb_env_set_mapsize(env, 10485760));
	E(rdb_env_set_maxdbs(env, 4));
	E(rdb_env_open(env, "./tests/db", RDB_NOSYNC, 0664));

	E(rdb_txn_begin(env, NULL, 0, &txn));
	E(rdb_dbi_open(txn, "id1", RDB_CREATE, &dbi));

	key.mv_size = sizeof(int);
	key.mv_data = sval;

	printf("Adding %d values\n", count);
	for (i=0;i<count;i++) {
		sprintf(sval, "%03x %d foo bar", values[i], values[i]);
		data.mv_size = sizeof(sval);
		data.mv_data = sval;
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

	j=0;
	key.mv_data = sval;
	for (i= count - 1; i > -1; i-= (rand()%5)) {
		j++;
		txn=NULL;
		E(rdb_txn_begin(env, NULL, 0, &txn));
		sprintf(sval, "%03x ", values[i]);
		if (RES(RDB_NOTFOUND, rdb_del(txn, dbi, &key, NULL))) {
			j--;
			rdb_txn_abort(txn);
		} else {
			E(rdb_txn_commit(txn));
		}
	}
	free(values);
	printf("Deleted %d values\n", j);

	E(rdb_env_stat(env, &mst));
	E(rdb_txn_begin(env, NULL, RDB_RDONLY, &txn));
	E(rdb_cursor_open(txn, dbi, &cursor));
	printf("Cursor next\n");
	while ((rc = rdb_cursor_get(cursor, &key, &data, RDB_NEXT)) == 0) {
		printf("key: %.*s, data: %.*s\n",
			(int) key.mv_size,  (char *) key.mv_data,
			(int) data.mv_size, (char *) data.mv_data);
	}
	CHECK(rc == RDB_NOTFOUND, "rdb_cursor_get");
	printf("Cursor prev\n");
	while ((rc = rdb_cursor_get(cursor, &key, &data, RDB_PREV)) == 0) {
		printf("key: %.*s, data: %.*s\n",
			(int) key.mv_size,  (char *) key.mv_data,
			(int) data.mv_size, (char *) data.mv_data);
	}
	CHECK(rc == RDB_NOTFOUND, "rdb_cursor_get");
	rdb_cursor_close(cursor);
	rdb_txn_abort(txn);

	rdb_dbi_close(env, dbi);
	rdb_env_close(env);
	return 0;
}
