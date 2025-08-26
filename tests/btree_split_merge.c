/* btree_split_merge.c - memory-mapped database tester/toy */
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

/* Tests for DB splits and merges */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "ripdb.h"

static char *hex_dkey(RDB_val *key, char *buf) {
	if (!key || !buf) return "";
	unsigned char *c = (unsigned char*)key->mv_data;
	char *ptr = buf;
	for (size_t i = 0; i < key->mv_size; i++) {
		ptr += sprintf(ptr, "%02x", *c++);
	}
	return buf;
}

#define E(expr) CHECK((rc = (expr)) == RDB_SUCCESS, #expr)
#define RES(err, expr) ((rc = expr) == (err) || (CHECK(!rc, #expr), 0))
#define CHECK(test, msg) ((test) ? (void)0 : ((void)fprintf(stderr, \
	"%s:%d: %s: %s\n", __FILE__, __LINE__, msg, rdb_strerror(rc)), abort()))

char dkbuf[1024];

int main(int argc,char * argv[])
{
	int i = 0, rc;
	RDB_env *env;
	RDB_dbi dbi;
	RDB_val key, data, sdata;
	RDB_txn *txn;
	RDB_stat mst;
	RDB_cursor *cursor;
	int count;
	int *values;
	long kval;
	char *sval;

	(void)count;
	(void)values;

	srand(time(NULL));

	E(rdb_env_create(&env));
	E(rdb_env_set_mapsize(env, 10485760));
	E(rdb_env_set_maxdbs(env, 4));
	E(rdb_env_open(env, "./tests/db", RDB_NOSYNC, 0664));

	E(rdb_txn_begin(env, NULL, 0, &txn));
	E(rdb_dbi_open(txn, "id6", RDB_CREATE|RDB_INTEGERKEY, &dbi));
	E(rdb_cursor_open(txn, dbi, &cursor));
	E(rdb_stat(txn, dbi, &mst));

	sval = calloc(1, mst.ms_psize / 4);
	key.mv_size = sizeof(long);
	key.mv_data = &kval;
	sdata.mv_size = mst.ms_psize / 4 - 30;
	sdata.mv_data = sval;

	printf("Adding 12 values, should yield 3 splits\n");
	for (i=0;i<12;i++) {
		kval = i*5;
		sprintf(sval, "%08lx", kval);
		data = sdata;
		(void)RES(RDB_KEYEXIST, rdb_cursor_put(cursor, &key, &data, RDB_NOOVERWRITE));
	}
	printf("Adding 12 more values, should yield 3 splits\n");
	for (i=0;i<12;i++) {
		kval = i*5+4;
		sprintf(sval, "%08lx", kval);
		data = sdata;
		(void)RES(RDB_KEYEXIST, rdb_cursor_put(cursor, &key, &data, RDB_NOOVERWRITE));
	}
	printf("Adding 12 more values, should yield 3 splits\n");
	for (i=0;i<12;i++) {
		kval = i*5+1;
		sprintf(sval, "%08lx", kval);
		data = sdata;
		(void)RES(RDB_KEYEXIST, rdb_cursor_put(cursor, &key, &data, RDB_NOOVERWRITE));
	}
	E(rdb_cursor_get(cursor, &key, &data, RDB_FIRST));

	do {
		printf("key: %p %s, data: %p %.*s\n",
			key.mv_data,  hex_dkey(&key, dkbuf),
			data.mv_data, (int) data.mv_size, (char *) data.mv_data);
	} while ((rc = rdb_cursor_get(cursor, &key, &data, RDB_NEXT)) == 0);
	CHECK(rc == RDB_NOTFOUND, "rdb_cursor_get");
	rdb_cursor_close(cursor);
	rdb_txn_commit(txn);

#if 0
	j=0;

	for (i= count - 1; i > -1; i-= (rand()%5)) {
		j++;
		txn=NULL;
		E(rdb_txn_begin(env, NULL, 0, &txn));
		sprintf(kval, "%03x", values[i & ~0x0f]);
		sprintf(sval, "%03x %d foo bar", values[i], values[i]);
		key.mv_size = sizeof(int);
		key.mv_data = kval;
		data.mv_size = sizeof(sval);
		data.mv_data = sval;
		if (RES(RDB_NOTFOUND, rdb_del(txn, dbi, &key, &data))) {
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
#endif
	rdb_env_close(env);

	return 0;
}
