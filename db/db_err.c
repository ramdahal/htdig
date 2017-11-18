/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999
 *  Sleepycat Software.  All rights reserved.
 */


#include "db_config.h"

#ifndef lint
static const char sccsid[] = "@(#)db_err.c  11.10 (Sleepycat) 11/8/99";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _MSC_VER /* WIN32 */
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#endif
#endif

#include "db_int.h"
#include "db_shash.h"
#include "lock.h"
#include "lock_ext.h"
#include "log.h"
#include "log_ext.h"
#include "mp.h"
#include "mp_ext.h"
#include "txn.h"
#include "txn_ext.h"
#include "common_ext.h"
#include "db_auto.h"

static void CDB___db_errcall __P((const DB_ENV *, int, int, const char *, va_list));
static void CDB___db_errfile __P((const DB_ENV *, int, int, const char *, va_list));

/*
 * CDB___db_fchk --
 *  General flags checking routine.
 *
 * PUBLIC: int CDB___db_fchk __P((DB_ENV *, const char *, u_int32_t, u_int32_t));
 */
int
CDB___db_fchk(dbenv, name, flags, ok_flags)
  DB_ENV *dbenv;
  const char *name;
  u_int32_t flags, ok_flags;
{
  return (LF_ISSET(~ok_flags) ? CDB___db_ferr(dbenv, name, 0) : 0);
}

/*
 * CDB___db_fcchk --
 *  General combination flags checking routine.
 *
 * PUBLIC: int CDB___db_fcchk
 * PUBLIC:    __P((DB_ENV *, const char *, u_int32_t, u_int32_t, u_int32_t));
 */
int
CDB___db_fcchk(dbenv, name, flags, flag1, flag2)
  DB_ENV *dbenv;
  const char *name;
  u_int32_t flags, flag1, flag2;
{
  return (LF_ISSET(flag1) &&
      LF_ISSET(flag2) ? CDB___db_ferr(dbenv, name, 1) : 0);
}

/*
 * CDB___db_ferr --
 *  Common flag errors.
 *
 * PUBLIC: int CDB___db_ferr __P((const DB_ENV *, const char *, int));
 */
int
CDB___db_ferr(dbenv, name, iscombo)
  const DB_ENV *dbenv;
  const char *name;
  int iscombo;
{
  CDB___db_err(dbenv, "illegal flag %sspecified to %s",
      iscombo ? "combination " : "", name);
  return (EINVAL);
}

/*
 * CDB___db_pgerr --
 *  Error when unable to retrieve a specified page.
 *
 * PUBLIC: int CDB___db_pgerr __P((DB *, db_pgno_t));
 */
int
CDB___db_pgerr(dbp, pgno)
  DB *dbp;
  db_pgno_t pgno;
{
  /*
   * Three things are certain:
   * Death, taxes, and lost data.
   * Guess which has occurred.
   */
  CDB___db_err(dbp->dbenv,
      "unable to create/retrieve page %lu", (u_long)pgno);
  return (CDB___db_panic(dbp->dbenv, EIO));
}

/*
 * CDB___db_pgfmt --
 *  Error when a page has the wrong format.
 *
 * PUBLIC: int CDB___db_pgfmt __P((DB *, db_pgno_t));
 */
int
CDB___db_pgfmt(dbp, pgno)
  DB *dbp;
  db_pgno_t pgno;
{
  CDB___db_err(dbp->dbenv,
      "page %lu: illegal page type or format", (u_long)pgno);
  return (CDB___db_panic(dbp->dbenv, EINVAL));
}

#ifdef DIAGNOSTIC
/*
 * __db_assert --
 *  Error when an assertion fails.  Only checked if #DIAGNOSTIC defined.
 *
 * PUBLIC: #ifdef DIAGNOSTIC
 * PUBLIC: void __db_assert __P((const char *, const char *, int));
 * PUBLIC: #endif
 */
void
__db_assert(failedexpr, file, line)
  const char *failedexpr, *file;
  int line;
{
  (void)fprintf(stderr,
      "__db_assert: \"%s\" failed: file \"%s\", line %d\n",
      failedexpr, file, line);
  fflush(stderr);

  /* We want a stack trace of how this could possibly happen. */
  abort();

  /* NOTREACHED */
}
#endif

/*
 * CDB___db_panic_msg --
 *  Just report that someone else paniced.
 *
 * PUBLIC: int CDB___db_panic_msg __P((DB_ENV *));
 */
int
CDB___db_panic_msg(dbenv)
  DB_ENV *dbenv;
{
  CDB___db_err(dbenv, "region error detected; run recovery.");
  /* Hack to make fatal errors really fatal... */
  fprintf(stderr,"DB_RUNRECOVERY: Fatal error, run database recovery\n");
  exit(1);
  return (DB_RUNRECOVERY);
}

/*
 * CDB___db_panic --
 *  Lock out the tree due to unrecoverable error.
 *
 * PUBLIC: int CDB___db_panic __P((DB_ENV *, int));
 */
int
CDB___db_panic(dbenv, errval)
  DB_ENV *dbenv;
  int errval;
{
  if (dbenv != NULL) {
    ((REGENV *)((REGINFO *)dbenv->reginfo)->addr)->panic = 1;

    dbenv->db_panic = errval;

    CDB___db_err(dbenv, "PANIC: %s", CDB_db_strerror(errval));

    if (dbenv->db_paniccall != NULL)
      dbenv->db_paniccall(dbenv, errval);
  }

  /*
   * Chaos reigns within.
   * Reflect, repent, and reboot.
   * Order shall return.
   */
  /* Hack to make fatal errors really fatal... */
  fprintf(stderr,"DB_RUNRECOVERY: Fatal error, run database recovery\n");
  exit(1);
  return (DB_RUNRECOVERY);
}

/*
 * CDB_db_strerror --
 *  ANSI C strerror(3) for DB.
 */
char *
CDB_db_strerror(error)
  int error;
{
  if (error == 0)
    return ("Successful return: 0");
  if (error > 0)
    return (strerror(error));

  /*
   * !!!
   * The Tcl API requires that some of these return strings be compared
   * against strings stored in application scripts.  So, any of these
   * errors that do not invariably result in a Tcl exception may not be
   * altered.
   */
  switch (error) {
  case DB_INCOMPLETE:
    return ("DB_INCOMPLETE: Cache flush was unable to complete");
  case DB_KEYEMPTY:
    return ("DB_KEYEMPTY: Non-existent key/data pair");
  case DB_KEYEXIST:
    return ("DB_KEYEXIST: Key/data pair already exists");
  case DB_LOCK_DEADLOCK:
    return
        ("DB_LOCK_DEADLOCK: Locker killed to resolve a deadlock");
  case DB_LOCK_NOTGRANTED:
    return ("DB_LOCK_NOTGRANTED: Lock not granted");
  case DB_NOTFOUND:
    return ("DB_NOTFOUND: No matching key/data pair found");
  case DB_OLD_VERSION:
    return ("DB_OLDVERSION: Database requires a version upgrade");
  case DB_RUNRECOVERY:
    return ("DB_RUNRECOVERY: Fatal error, run database recovery");
  default: {
    /*
     * !!!
     * Room for a 64-bit number + slop.  This buffer is only used
     * if we're given an unknown error, which should never happen.
     * Note, however, we're no longer thread-safe if it does.
     */
    static char ebuf[40];

    (void)snprintf(ebuf, sizeof(ebuf), "Unknown error: %d", error);
    return(ebuf);
  }
  }
}

/*
 * CDB___db_err --
 *  Standard DB error routine.  The same as db_errx, except that we
 *  don't write to stderr if no output mechanism was specified.
 *
 * PUBLIC: #ifdef __STDC__
 * PUBLIC: void CDB___db_err __P((const DB_ENV *, const char *, ...));
 * PUBLIC: #else
 * PUBLIC: void CDB___db_err();
 * PUBLIC: #endif
 */
void
#if defined( __STDC__) || defined(_MSC_VER) /* WIN32 */
CDB___db_err(const DB_ENV *dbenv, const char *fmt, ...)
#else
CDB___db_err(dbenv, fmt, va_alist)
  const DB_ENV *dbenv;
  const char *fmt;
  va_dcl
#endif
{
  va_list ap;

#if defined(__STDC__) || defined(_MSC_VER) /* WIN32 */
  va_start(ap, fmt);
#else
  va_start(ap);
#endif
  CDB___db_real_err(dbenv, 0, 0, 0, fmt, ap);

  va_end(ap);
}

/*
 * CDB___db_real_err --
 *  All the DB error routines end up here.
 *
 * PUBLIC: void CDB___db_real_err
 * PUBLIC:     __P((const DB_ENV *, int, int, int, const char *, va_list));
 */
void
CDB___db_real_err(dbenv, error, error_set, stderr_default, fmt, ap)
  const DB_ENV *dbenv;
  int error, error_set, stderr_default;
  const char *fmt;
  va_list ap;
{
  if (dbenv != NULL && dbenv->db_errcall != NULL)
    CDB___db_errcall(dbenv, error, error_set, fmt, ap);

  if (dbenv != NULL && dbenv->db_errfile != NULL)
    CDB___db_errfile(dbenv, error, error_set, fmt, ap);

  if (stderr_default && (dbenv == NULL ||
      (dbenv->db_errcall == NULL && dbenv->db_errfile == NULL)))
    CDB___db_errfile(NULL, error, error_set, fmt, ap);
}

/*
 * CDB___db_errcall --
 *  Do the error message work for callback functions.
 */
static void
CDB___db_errcall(dbenv, error, error_set, fmt, ap)
  const DB_ENV *dbenv;
  int error, error_set;
  const char *fmt;
  va_list ap;
{
  char *p;
  char __errbuf[2048];  /* XXX: END OF THE STACK DON'T TRUST SPRINTF. */

  p = __errbuf;
  if (fmt != NULL) {
    p += vsnprintf(__errbuf, sizeof(__errbuf), fmt, ap);
    if (error_set) {
      *p++ = ':';
      *p++ = ' ';
    }
  }
  if (error_set)
    (void)strcpy(p, CDB_db_strerror(error));

  dbenv->db_errcall(dbenv->db_errpfx, __errbuf);
}

/*
 * CDB___db_errfile --
 *  Do the error message work for FILE *s.
 */
static void
CDB___db_errfile(dbenv, error, error_set, fmt, ap)
  const DB_ENV *dbenv;
  int error, error_set;
  const char *fmt;
  va_list ap;
{
  FILE *fp;

  fp = dbenv == NULL ||
      dbenv->db_errfile == NULL ? stderr : dbenv->db_errfile;

  if (dbenv != NULL && dbenv->db_errpfx != NULL)
    (void)fprintf(fp, "%s: ", dbenv->db_errpfx);
  if (fmt != NULL) {
    (void)vfprintf(fp, fmt, ap);
    if (error_set)
      (void)fprintf(fp, ": ");
  }
  if (error_set)
    (void)fprintf(fp, "%s", CDB_db_strerror(error));
  (void)fprintf(fp, "\n");
  (void)fflush(fp);
}

/*
 * CDB___db_logmsg --
 *  Write information into the DB log.
 *
 * PUBLIC: #ifdef __STDC__
 * PUBLIC: int CDB___db_logmsg __P((DB_ENV *,
 * PUBLIC:     DB_TXN *, const char *, u_int32_t, const char *, ...));
 * PUBLIC: #else
 * PUBLIC: int CDB___db_logmsg();
 * PUBLIC: #endif
 */
int
#if defined(__STDC__) || defined(_MSC_VER) /* WIN32 */
CDB___db_logmsg(DB_ENV *dbenv,
    DB_TXN *txnid, const char *opname, u_int32_t flags, const char *fmt, ...)
#else
CDB___db_logmsg(dbenv, txnid, opname, flags, fmt, va_alist)
  DB_ENV *dbenv;
  DB_TXN *txnid;
  const char *opname, *fmt;
  u_int32_t flags;
  va_dcl
#endif
{
  DBT opdbt, msgdbt;
  DB_LSN lsn;
  va_list ap;
  char __logbuf[2048];  /* XXX: END OF THE STACK DON'T TRUST SPRINTF. */

  if (!F_ISSET(dbenv, DB_ENV_LOGGING))
    return (0);

  memset(&opdbt, 0, sizeof(opdbt));
  opdbt.data = (void *)opname;
  opdbt.size = strlen(opname) + 1;

  memset(&msgdbt, 0, sizeof(msgdbt));
  msgdbt.data = __logbuf;
#if defined(__STDC__) || defined(_MSC_VER) /* WIN32 */
  va_start(ap, fmt);
#else
  va_start(ap);
#endif
  msgdbt.size = vsnprintf(__logbuf, sizeof(__logbuf), fmt, ap);
  va_end(ap);

  return (CDB___db_debug_log(dbenv,
      txnid, &lsn, flags, &opdbt, -1, &msgdbt, NULL, 0));
}
