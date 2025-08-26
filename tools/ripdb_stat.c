/* rdb_stat.c - memory-mapped database status tool */
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
#include <unistd.h>
#include "ripdb.h"

#ifdef	_WIN32
#define	Z	"I"
#else
#define	Z	"z"
#endif

static void prstat(RDB_stat *ms)
{
#if 0
	printf("  Page size: %u\n", ms->ms_psize);
#endif
	printf("  Tree depth: %u\n", ms->ms_depth);
	printf("  Branch pages: %"Z"u\n", ms->ms_branch_pages);
	printf("  Leaf pages: %"Z"u\n", ms->ms_leaf_pages);
	printf("  Overflow pages: %"Z"u\n", ms->ms_overflow_pages);
	printf("  Entries: %"Z"u\n", ms->ms_entries);
}

static void usage(char *prog)
{
	fprintf(stderr, "usage: %s [-V] [-n] [-e] [-r[r]] [-f[f[f]]] [-a|-s subdb] dbpath\n", prog);
	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	int i, rc;
	RDB_env *env;
	RDB_txn *txn;
	RDB_dbi dbi;
	RDB_stat mst;
	RDB_envinfo mei;
	char *prog = argv[0];
	char *envname;
	char *subname = NULL;
	int alldbs = 0, envinfo = 0, envflags = 0, freinfo = 0, rdrinfo = 0;

	if (argc < 2) {
		usage(prog);
	}

	/* -a: print stat of main DB and all subDBs
	 * -s: print stat of only the named subDB
	 * -e: print env info
	 * -f: print freelist info
	 * -r: print reader info
	 * -n: use NOSUBDIR flag on env_open
	 * -V: print version and exit
	 * (default) print stat of only the main DB
	 */
	while ((i = getopt(argc, argv, "Vaefnrs:")) != EOF) {
		switch(i) {
		case 'V':
			printf("%s\n", RDB_VERSION_STRING);
			exit(0);
			break;
		case 'a':
			if (subname)
				usage(prog);
			alldbs++;
			break;
		case 'e':
			envinfo++;
			break;
		case 'f':
			freinfo++;
			break;
		case 'n':
			envflags |= RDB_NOSUBDIR;
			break;
		case 'r':
			rdrinfo++;
			break;
		case 's':
			if (alldbs)
				usage(prog);
			subname = optarg;
			break;
		default:
			usage(prog);
		}
	}

	if (optind != argc - 1)
		usage(prog);

	envname = argv[optind];
	rc = rdb_env_create(&env);
	if (rc) {
		fprintf(stderr, "rdb_env_create failed, error %d %s\n", rc, rdb_strerror(rc));
		return EXIT_FAILURE;
	}

	if (alldbs || subname) {
		rdb_env_set_maxdbs(env, 4);
	}

	rc = rdb_env_open(env, envname, envflags | RDB_RDONLY, 0664);
	if (rc) {
		fprintf(stderr, "rdb_env_open failed, error %d %s\n", rc, rdb_strerror(rc));
		goto env_close;
	}

	if (envinfo) {
		(void)rdb_env_stat(env, &mst);
		(void)rdb_env_info(env, &mei);
		printf("Environment Info\n");
		printf("  Map address: %p\n", mei.me_mapaddr);
		printf("  Map size: %"Z"u\n", mei.me_mapsize);
		printf("  Page size: %u\n", mst.ms_psize);
		printf("  Max pages: %"Z"u\n", mei.me_mapsize / mst.ms_psize);
		printf("  Number of pages used: %"Z"u\n", mei.me_last_pgno+1);
		printf("  Last transaction ID: %"Z"u\n", mei.me_last_txnid);
		printf("  Max readers: %u\n", mei.me_maxreaders);
		printf("  Number of readers used: %u\n", mei.me_numreaders);
	}

	if (rdrinfo) {
		printf("Reader Table Status\n");
		rc = rdb_reader_list(env, (RDB_msg_func *)fputs, stdout);
		if (rdrinfo > 1) {
			int dead;
			rdb_reader_check(env, &dead);
			printf("  %d stale readers cleared.\n", dead);
			rc = rdb_reader_list(env, (RDB_msg_func *)fputs, stdout);
		}
		if (!(subname || alldbs || freinfo))
			goto env_close;
	}

	rc = rdb_txn_begin(env, NULL, RDB_RDONLY, &txn);
	if (rc) {
		fprintf(stderr, "rdb_txn_begin failed, error %d %s\n", rc, rdb_strerror(rc));
		goto env_close;
	}

	if (freinfo) {
		RDB_cursor *cursor;
		RDB_val key, data;
		size_t pages = 0, *iptr;

		printf("Freelist Status\n");
		dbi = 0;
		rc = rdb_cursor_open(txn, dbi, &cursor);
		if (rc) {
			fprintf(stderr, "rdb_cursor_open failed, error %d %s\n", rc, rdb_strerror(rc));
			goto txn_abort;
		}
		rc = rdb_stat(txn, dbi, &mst);
		if (rc) {
			fprintf(stderr, "rdb_stat failed, error %d %s\n", rc, rdb_strerror(rc));
			goto txn_abort;
		}
		prstat(&mst);
		while ((rc = rdb_cursor_get(cursor, &key, &data, RDB_NEXT)) == 0) {
			iptr = data.mv_data;
			pages += *iptr;
			if (freinfo > 1) {
				char *bad = "";
				size_t pg, prev;
				ssize_t i, j, span = 0;
				j = *iptr++;
				for (i = j, prev = 1; --i >= 0; ) {
					pg = iptr[i];
					if (pg <= prev)
						bad = " [bad sequence]";
					prev = pg;
					pg += span;
					for (; i >= span && iptr[i-span] == pg; span++, pg++) ;
				}
				printf("    Transaction %"Z"u, %"Z"d pages, maxspan %"Z"d%s\n",
					*(size_t *)key.mv_data, j, span, bad);
				if (freinfo > 2) {
					for (--j; j >= 0; ) {
						pg = iptr[j];
						for (span=1; --j >= 0 && iptr[j] == pg+span; span++) ;
						printf(span>1 ? "     %9"Z"u[%"Z"d]\n" : "     %9"Z"u\n",
							pg, span);
					}
				}
			}
		}
		rdb_cursor_close(cursor);
		printf("  Free pages: %"Z"u\n", pages);
	}

	rc = rdb_dbi_open(txn, subname, 0, &dbi);
	if (rc) {
		fprintf(stderr, "rdb_dbi_open failed, error %d %s\n", rc, rdb_strerror(rc));
		goto txn_abort;
	}

	rc = rdb_stat(txn, dbi, &mst);
	if (rc) {
		fprintf(stderr, "rdb_stat failed, error %d %s\n", rc, rdb_strerror(rc));
		goto txn_abort;
	}
	printf("Status of %s\n", subname ? subname : "Main DB");
	prstat(&mst);

	if (alldbs) {
		RDB_cursor *cursor;
		RDB_val key;

		rc = rdb_cursor_open(txn, dbi, &cursor);
		if (rc) {
			fprintf(stderr, "rdb_cursor_open failed, error %d %s\n", rc, rdb_strerror(rc));
			goto txn_abort;
		}
		while ((rc = rdb_cursor_get(cursor, &key, NULL, RDB_NEXT_NODUP)) == 0) {
			char *str;
			RDB_dbi db2;
			if (memchr(key.mv_data, '\0', key.mv_size))
				continue;
			str = malloc(key.mv_size+1);
			memcpy(str, key.mv_data, key.mv_size);
			str[key.mv_size] = '\0';
			rc = rdb_dbi_open(txn, str, 0, &db2);
			if (rc == RDB_SUCCESS)
				printf("Status of %s\n", str);
			free(str);
			if (rc) continue;
			rc = rdb_stat(txn, db2, &mst);
			if (rc) {
				fprintf(stderr, "rdb_stat failed, error %d %s\n", rc, rdb_strerror(rc));
				goto txn_abort;
			}
			prstat(&mst);
			rdb_dbi_close(env, db2);
		}
		rdb_cursor_close(cursor);
	}

	if (rc == RDB_NOTFOUND)
		rc = RDB_SUCCESS;

	rdb_dbi_close(env, dbi);
txn_abort:
	rdb_txn_abort(txn);
env_close:
	rdb_env_close(env);

	return rc ? EXIT_FAILURE : EXIT_SUCCESS;
}
