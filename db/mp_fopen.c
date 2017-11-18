/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999
 *  Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char sccsid[] = "@(#)mp_fopen.c  11.7 (Sleepycat) 10/27/99";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <string.h>
#endif

#include "db_int.h"
#include "db_shash.h"
#include "mp.h"

static int CDB___memp_mf_close __P((DB_MPOOL *, DB_MPOOLFILE *));
static int CDB___memp_mf_open __P((DB_MPOOL *,
    const char *, size_t, db_pgno_t, DB_MPOOL_FINFO *, MPOOLFILE **));

/*
 * CDB_memp_fopen --
 *  Open a backing file for the memory pool.
 */
int
CDB_memp_fopen(dbenv, path, flags, mode, pagesize, finfop, retp)
  DB_ENV *dbenv;
  const char *path;
  u_int32_t flags;
  int mode;
  size_t pagesize;
  DB_MPOOL_FINFO *finfop;
  DB_MPOOLFILE **retp;
{
  DB_MPOOL *dbmp;
  int ret;

  PANIC_CHECK(dbenv);
  ENV_REQUIRES_CONFIG(dbenv, dbenv->mp_handle, DB_INIT_MPOOL);

  dbmp = dbenv->mp_handle;

  /* Validate arguments. */
  if ((ret = CDB___db_fchk(dbenv,
      "CDB_memp_fopen", flags, DB_COMPRESS | DB_CREATE | DB_NOMMAP | DB_RDONLY)) != 0)
    return (ret);

  /*
   * Transparent I/O compression does not work on mmap'd files.
   */
  if(LF_ISSET(DB_COMPRESS))
    LF_SET(DB_NOMMAP);


  /* Require a non-zero pagesize. */
  if (pagesize == 0) {
    CDB___db_err(dbenv, "CDB_memp_fopen: pagesize not specified");
    return (EINVAL);
  }
  if (finfop != NULL && finfop->clear_len > pagesize)
    return (EINVAL);

  return (CDB___memp_fopen(dbmp,
      NULL, path, flags, mode, pagesize, 1, finfop, retp));
}

/*
 * CDB___memp_fopen --
 *  Open a backing file for the memory pool; internal version.
 *
 * PUBLIC: int CDB___memp_fopen __P((DB_MPOOL *, MPOOLFILE *, const char *,
 * PUBLIC:    u_int32_t, int, size_t, int, DB_MPOOL_FINFO *, DB_MPOOLFILE **));
 */
int
CDB___memp_fopen(dbmp, mfp, path, flags, mode, pagesize, needlock, finfop, retp)
  DB_MPOOL *dbmp;
  MPOOLFILE *mfp;
  const char *path;
  u_int32_t flags;
  int mode, needlock;
  size_t pagesize;
  DB_MPOOL_FINFO *finfop;
  DB_MPOOLFILE **retp;
{
  DB_ENV *dbenv;
  DB_MPOOLFILE *dbmfp;
  DB_MPOOL_FINFO finfo;
  db_pgno_t last_pgno;
  size_t maxmap;
  u_int32_t mbytes, bytes, oflags;
  int ret;
  u_int8_t idbuf[DB_FILE_ID_LEN];
  char *rpath;

  dbenv = dbmp->dbenv;
  ret = 0;
  rpath = NULL;

  /*
   * If mfp is provided, we take the DB_MPOOL_FINFO information from
   * the mfp.  We don't bother initializing everything, because some
   * of them are expensive to acquire.  If no mfp is provided and the
   * finfop argument is NULL, we default the values.
   */
  if (finfop == NULL) {
    memset(&finfo, 0, sizeof(finfo));
    if (mfp != NULL) {
      finfo.ftype = mfp->ftype;
      finfo.pgcookie = NULL;
      finfo.fileid = NULL;
      finfo.lsn_offset = mfp->lsn_off;
      finfo.clear_len = mfp->clear_len;
    } else {
      finfo.ftype = 0;
      finfo.pgcookie = NULL;
      finfo.fileid = NULL;
      finfo.lsn_offset = -1;
      finfo.clear_len = 0;
    }
    finfop = &finfo;
  }

  /* Allocate and initialize the per-process structure. */
  if ((ret = CDB___os_calloc(1, sizeof(DB_MPOOLFILE), &dbmfp)) != 0)
    return (ret);
  dbmfp->dbmp = dbmp;
  dbmfp->ref = 1;
  if (LF_ISSET(DB_RDONLY))
    F_SET(dbmfp, MP_READONLY);
  if (LF_ISSET(DB_COMPRESS))
    F_SET(dbmfp, MP_CMPR);

  if (path == NULL) {
    if (LF_ISSET(DB_RDONLY)) {
      CDB___db_err(dbenv,
          "CDB_memp_fopen: temporary files can't be readonly");
      ret = EINVAL;
      goto err;
    }
    if (LF_ISSET(DB_COMPRESS)) {
      CDB___db_err(dbenv,
          "CDB_memp_fopen: temporary files can't be compressed");
      ret = EINVAL;
      goto err;
    }
    last_pgno = 0;
  } else {
         size_t disk_pagesize = F_ISSET(dbmfp, MP_CMPR) ? DB_CMPR_DIVIDE(dbenv, pagesize) : pagesize;
    /* Get the real name for this file and open it. */
    if ((ret = CDB___db_appname(dbenv,
        DB_APP_DATA, NULL, path, 0, NULL, &rpath)) != 0)
      goto err;
    oflags = 0;
    if (LF_ISSET(DB_CREATE))
      oflags |= DB_OSO_CREATE;
    if (LF_ISSET(DB_RDONLY))
      oflags |= DB_OSO_RDONLY;
    if ((ret =
       CDB___os_open(rpath, oflags, mode, &dbmfp->fh)) != 0) {
      CDB___db_err(dbenv, "%s: %s", rpath, CDB_db_strerror(ret));
      goto err;
    }

    /*
     * Don't permit files that aren't a multiple of the pagesize,
     * and find the number of the last page in the file, all the
     * time being careful not to overflow 32 bits.
     *
     * !!!
     * We can't use off_t's here, or in any code in the mainline
     * library for that matter.  (We have to use them in the os
     * stubs, of course, as there are system calls that take them
     * as arguments.)  The reason is that some customers build in
     * environments where an off_t is 32-bits, but still run where
     * offsets are 64-bits, and they pay us a lot of money.
     */
    if ((ret = CDB___os_ioinfo(rpath,
        &dbmfp->fh, &mbytes, &bytes, NULL)) != 0) {
      CDB___db_err(dbenv, "%s: %s", rpath, CDB_db_strerror(ret));
      goto err;
    }

    /* Page sizes have to be a power-of-two, ignore mbytes. */
    if (bytes % disk_pagesize != 0) {
      CDB___db_err(dbenv,
          "%s: file size not a multiple of the pagesize",
          rpath);
      ret = EINVAL;
      goto err;
    }

    last_pgno = mbytes * (MEGABYTE / disk_pagesize);
    last_pgno += bytes / disk_pagesize;

    /* Correction: page numbers are zero-based, not 1-based. */
    if (last_pgno != 0)
      --last_pgno;

    /*
     * Get the file id if we weren't given one.  Generated file id's
     * don't use timestamps, otherwise there'd be no chance of any
     * other process joining the party.
     */
    if (finfop->fileid == NULL) {
      if ((ret = CDB___os_fileid(dbenv, rpath, 0, idbuf)) != 0)
        goto err;
      finfop->fileid = idbuf;
    }
    if (LF_ISSET(DB_COMPRESS)) {
      if((ret = CDB___memp_cmpr_open(dbenv, path, flags, mode, &dbmfp->cmpr_context)) != 0)
        goto err;
    }
  }

  /*
   * If we weren't provided an underlying shared object to join with,
   * find/allocate the shared file objects.  Also allocate space for
   * for the per-process thread lock.
   */
  if (needlock)
    R_LOCK(dbenv, &dbmp->reginfo);
  ret = mfp == NULL ? CDB___memp_mf_open(
      dbmp, path, pagesize, last_pgno, finfop, &mfp) : 0;
  if (needlock)
    R_UNLOCK(dbenv, &dbmp->reginfo);
  if (ret != 0)
    goto err;

  if (F_ISSET(dbenv, DB_ENV_THREAD)) {
    if ((ret = CDB___db_mutex_alloc(
        dbenv, &dbmp->reginfo, &dbmfp->mutexp)) != 0)
      goto err;
    if ((ret = __db_mutex_init(
        dbenv, dbmfp->mutexp, 0, MUTEX_THREAD)) != 0)
      goto err;

    /* XXX: KEITH: CLOSE THE FILE ON FAILURE? */
  }

  dbmfp->mfp = mfp;

  /*
   * If a file:
   *  + is read-only
   *  + isn't temporary
   *  + doesn't require any pgin/pgout support
   *  + the DB_NOMMAP flag wasn't set (in either the file open or
   *    the environment in which it was opened)
   *  + and is less than mp_mmapsize bytes in size
   *
   * we can mmap it instead of reading/writing buffers.  Don't do error
   * checking based on the mmap call failure.  We want to do normal I/O
   * on the file if the reason we failed was because the file was on an
   * NFS mounted partition, and we can fail in buffer I/O just as easily
   * as here.
   *
   * XXX
   * We'd like to test to see if the file is too big to mmap.  Since we
   * don't know what size or type off_t's or size_t's are, or the largest
   * unsigned integral type is, or what random insanity the local C
   * compiler will perpetrate, doing the comparison in a portable way is
   * flatly impossible.  Hope that mmap fails if the file is too large.
   */
#define  DB_MAXMMAPSIZE  (10 * 1024 * 1024)  /* 10 Mb. */
  if (F_ISSET(mfp, MP_CAN_MMAP)) {
    if (!F_ISSET(dbmfp, MP_READONLY))
      F_CLR(mfp, MP_CAN_MMAP);
    if (path == NULL)
      F_CLR(mfp, MP_CAN_MMAP);
    if (finfop->ftype != 0)
      F_CLR(mfp, MP_CAN_MMAP);
    if (LF_ISSET(DB_NOMMAP) || F_ISSET(dbenv, DB_ENV_NOMMAP))
      F_CLR(mfp, MP_CAN_MMAP);
    maxmap = dbenv->mp_mmapsize == 0 ?
        DB_MAXMMAPSIZE : dbenv->mp_mmapsize;
    if (mbytes > maxmap / MEGABYTE ||
        (mbytes == maxmap / MEGABYTE && bytes >= maxmap % MEGABYTE))
      F_CLR(mfp, MP_CAN_MMAP);
  }
  dbmfp->addr = NULL;
  if (F_ISSET(mfp, MP_CAN_MMAP)) {
    dbmfp->len = (size_t)mbytes * MEGABYTE + bytes;
    if (CDB___os_mapfile(dbenv, rpath,
        &dbmfp->fh, dbmfp->len, 1, &dbmfp->addr) != 0) {
      dbmfp->addr = NULL;
      F_CLR(mfp, MP_CAN_MMAP);
    }
  }
  if (rpath != NULL)
    CDB___os_freestr(rpath);

  MUTEX_THREAD_LOCK(dbmp->mutexp);
  TAILQ_INSERT_TAIL(&dbmp->dbmfq, dbmfp, q);
  MUTEX_THREAD_UNLOCK(dbmp->mutexp);

  *retp = dbmfp;
  return (0);

err:  /*
   * Note that we do not have to free the thread mutex, because we
   * never get to here after we have successfully allocated it.
   */
  if (rpath != NULL)
    CDB___os_freestr(rpath);
  if (F_ISSET(&dbmfp->fh, DB_FH_VALID))
    (void)CDB___os_closehandle(&dbmfp->fh);
  if (dbmfp != NULL)
    CDB___os_free(dbmfp, sizeof(DB_MPOOLFILE));
  return (ret);
}
static  MPOOL *CDB_mp;

/*
 * CDB___memp_mf_open --
 *  Open an MPOOLFILE.
 */
static int
CDB___memp_mf_open(dbmp, path, pagesize, last_pgno, finfop, retp)
  DB_MPOOL *dbmp;
  const char *path;
  size_t pagesize;
  db_pgno_t last_pgno;
  DB_MPOOL_FINFO *finfop;
  MPOOLFILE **retp;
{
  MPOOLFILE *mfp;
  int ret;
  void *p;

#define  ISTEMPORARY  (path == NULL)

  /*
   * Walk the list of MPOOLFILE's, looking for a matching file.
   * Temporary files can't match previous files.
   */
  if (!ISTEMPORARY) {
    CDB_mp = dbmp->reginfo.primary;
    for (mfp = SH_TAILQ_FIRST(&CDB_mp->mpfq, __mpoolfile);
        mfp != NULL; mfp = SH_TAILQ_NEXT(mfp, q, __mpoolfile)) {
      if (F_ISSET(mfp, MP_TEMP))
        continue;
      if (!memcmp(finfop->fileid, R_ADDR(&dbmp->reginfo,
          mfp->fileid_off), DB_FILE_ID_LEN)) {
        if (finfop->clear_len != mfp->clear_len ||
            pagesize != mfp->stat.st_pagesize) {
          CDB___db_err(dbmp->dbenv,
            "%s: page size or clear length changed",
              path);
          return (EINVAL);
        }

        /*
         * It's possible that our needs for pre- and
         * post-processing are changing.  For example,
         * an application created a hash subdatabase
         * in a database that was previously all btree.
         */
        if (finfop->ftype != 0)
          mfp->ftype = finfop->ftype;

        /* Found it. */
        *retp = mfp;
        return (0);
      }
    }
  }

  /* Allocate a new MPOOLFILE. */
  if ((ret = CDB___memp_alloc(
      dbmp, &dbmp->reginfo, NULL, sizeof(MPOOLFILE), NULL, &mfp)) != 0)
    return (ret);
  *retp = mfp;

  /* Initialize the structure. */
  memset(mfp, 0, sizeof(MPOOLFILE));
  mfp->ftype = finfop->ftype;
  mfp->lsn_off = finfop->lsn_offset;
  mfp->clear_len = finfop->clear_len;

  /*
   * If the user specifies DB_MPOOL_LAST or DB_MPOOL_NEW on a CDB_memp_fget,
   * we have to know the last page in the file.  Figure it out and save
   * it away.
   */
  mfp->stat.st_pagesize = pagesize;
  mfp->orig_last_pgno = mfp->last_pgno = last_pgno;

  if (ISTEMPORARY)
    F_SET(mfp, MP_TEMP);
  else {
    /* Copy the file path into shared memory. */
    if ((ret = CDB___memp_alloc(dbmp, &dbmp->reginfo,
        NULL, strlen(path) + 1, &mfp->path_off, &p)) != 0)
      goto err;
    memcpy(p, path, strlen(path) + 1);

    /* Copy the file identification string into shared memory. */
    if ((ret = CDB___memp_alloc(dbmp, &dbmp->reginfo,
        NULL, DB_FILE_ID_LEN, &mfp->fileid_off, &p)) != 0)
      goto err;
    memcpy(p, finfop->fileid, DB_FILE_ID_LEN);

    F_SET(mfp, MP_CAN_MMAP);
  }

  /* Copy the page cookie into shared memory. */
  if (finfop->pgcookie == NULL || finfop->pgcookie->size == 0) {
    mfp->pgcookie_len = 0;
    mfp->pgcookie_off = 0;
  } else {
    if ((ret = CDB___memp_alloc(dbmp, &dbmp->reginfo,
        NULL, finfop->pgcookie->size, &mfp->pgcookie_off, &p)) != 0)
      goto err;
    memcpy(p, finfop->pgcookie->data, finfop->pgcookie->size);
    mfp->pgcookie_len = finfop->pgcookie->size;
  }

  /* Prepend the MPOOLFILE to the list of MPOOLFILE's. */
  CDB_mp = dbmp->reginfo.primary;
  SH_TAILQ_INSERT_HEAD(&CDB_mp->mpfq, mfp, q, __mpoolfile);

  if (0) {
err:    if (mfp->path_off != 0)
      CDB___db_shalloc_free(dbmp->reginfo.addr,
          R_ADDR(&dbmp->reginfo, mfp->path_off));
    if (mfp->fileid_off != 0)
      CDB___db_shalloc_free(dbmp->reginfo.addr,
          R_ADDR(&dbmp->reginfo, mfp->fileid_off));
    if (mfp != NULL)
      CDB___db_shalloc_free(dbmp->reginfo.addr, mfp);
    mfp = NULL;
  }
  return (0);
}

/*
 * CDB_memp_fclose --
 *  Close a backing file for the memory pool.
 */
int
CDB_memp_fclose(dbmfp)
  DB_MPOOLFILE *dbmfp;
{
  DB_ENV *dbenv;
  DB_MPOOL *dbmp;
  int ret, t_ret;

  dbmp = dbmfp->dbmp;
  dbenv = dbmp->dbenv;
  ret = 0;

  PANIC_CHECK(dbenv);

  for (;;) {
    MUTEX_THREAD_LOCK(dbmp->mutexp);

    /*
     * We have to reference count DB_MPOOLFILE structures as other
     * threads may be using them.  The problem only happens if the
     * application makes a bad design choice.  Here's the path:
     *
     * Thread A opens a database.
     * Thread B uses thread A's DB_MPOOLFILE to write a buffer
     *    in order to free up memory in the mpool cache.
     * Thread A closes the database while thread B is using the
     *    DB_MPOOLFILE structure.
     *
     * By opening all databases before creating the threads, and
     * closing them after the threads have exited, applications
     * get better performance and avoid the problem path entirely.
     *
     * Regardless, holding the DB_MPOOLFILE to flush a dirty buffer
     * is a short-term lock, even in worst case, since we better be
     * the only thread of control using the DB_MPOOLFILE structure
     * to read pages *into* the cache.  Wait until we're the only
     * reference holder and remove the DB_MPOOLFILE structure from
     * the list, so nobody else can even find it.
     */
    if (dbmfp->ref == 1) {
      TAILQ_REMOVE(&dbmp->dbmfq, dbmfp, q);
      break;
    }
    MUTEX_THREAD_UNLOCK(dbmp->mutexp);

    (void)CDB___os_sleep(1, 0);
  }
  MUTEX_THREAD_UNLOCK(dbmp->mutexp);

  /* Complain if pinned blocks never returned. */
  if (dbmfp->pinref != 0)
    CDB___db_err(dbenv, "%s: close: %lu blocks left pinned",
        CDB___memp_fn(dbmfp), (u_long)dbmfp->pinref);

  /* Close the underlying MPOOLFILE. */
  (void)CDB___memp_mf_close(dbmp, dbmfp);

  /* Discard any mmap information. */
  if (dbmfp->addr != NULL &&
      (ret = CDB___os_unmapfile(dbenv, dbmfp->addr, dbmfp->len)) != 0)
    CDB___db_err(dbenv, "%s: %s", CDB___memp_fn(dbmfp), CDB_db_strerror(ret));

  /* Close the file; temporary files may not yet have been created. */
  if (F_ISSET(&dbmfp->fh, DB_FH_VALID) &&
      (t_ret = CDB___os_closehandle(&dbmfp->fh)) != 0) {
    CDB___db_err(dbenv, "%s: %s", CDB___memp_fn(dbmfp), CDB_db_strerror(t_ret));
    if (ret != 0)
      t_ret = ret;
  }

  if(F_ISSET(dbmfp, MP_CMPR)) {
    if((ret = CDB___memp_cmpr_close(&dbmfp->cmpr_context)) != 0)
    CDB___db_err(dbmp->dbenv,
       "%s: %s", CDB___memp_fn(dbmfp), strerror(ret));
    F_CLR(dbmfp, MP_CMPR);
  }
  
  /* Discard the thread mutex. */
  if (dbmfp->mutexp != NULL)
    CDB___db_mutex_free(dbenv, &dbmp->reginfo, dbmfp->mutexp);

  /* Discard the DB_MPOOLFILE structure. */
  CDB___os_free(dbmfp, sizeof(DB_MPOOLFILE));

  return (ret);
}

/*
 * CDB___memp_mf_close --
 *  Close down an MPOOLFILE.
 */
static int
CDB___memp_mf_close(dbmp, dbmfp)
  DB_MPOOL *dbmp;
  DB_MPOOLFILE *dbmfp;
{
  COMPQUIET(dbmp, NULL);
  COMPQUIET(dbmfp, NULL);

  /*
   * We don't currently close the underlying MPOOLFILE.  The reason falls
   * out of the following:
   *
   * 1. The way we identify a buffer's underlying file is the MPOOLFILE
   *    offset in shared memory, bhp->mf_offset.  If buffers remained in
   *    the pool after removing the MPOOLFILE, they might accidentally
   *    be associated with a newly created MPOOLFILE that happened to be
   *    allocated at the same offset as the previous one.
   * 2. We can't simply remove all the buffers from the pool, because
   *    there may be dirty buffers associated with the MPOOLFILE that
   *    the process hasn't yet flushed to disk.  (Dirty buffers are a
   *    possibility -- if the application specified DB_NOSYNC when it
   *    closed the database, no dirty buffers have been flushed.)
   * 3. We can't simply flush all the buffers to disk, because writing
   *    and re-integrating the memory into the free list is expensive,
   *    and in many cases the process is simply discarding it's pool.
   *
   * We could walk the cache regions and discard the MPOOLFILE if all
   * the buffers are clean, but that's a pain, and probably not worth
   * the effort.
   */
   return (0);
}

/*
 * CDB___memp_fremove --
 *  Remove an underlying file from the system.
 *
 * PUBLIC: int CDB___memp_fremove __P((DB_MPOOLFILE *));
 */
int
CDB___memp_fremove(dbmfp)
  DB_MPOOLFILE *dbmfp;
{
  DB_ENV *dbenv;
  DB_MPOOL *dbmp;

  dbmp = dbmfp->dbmp;
  dbenv = dbmp->dbenv;

  PANIC_CHECK(dbenv);

  R_LOCK(dbenv, &dbmp->reginfo);

  /*
   * Flag that the underlying file has been removed, and remove any
   * necessity for post-processing pages, anybody can discard them.
   */
  dbmfp->mfp->ftype = 0;
  F_SET(dbmfp->mfp, MP_REMOVED);

  R_UNLOCK(dbenv, &dbmp->reginfo);

  return (0);
}

/*
 * CDB___memp_fn --
 *  On errors we print whatever is available as the file name.
 *
 * PUBLIC: char * CDB___memp_fn __P((DB_MPOOLFILE *));
 */
char *
CDB___memp_fn(dbmfp)
  DB_MPOOLFILE *dbmfp;
{
  return (CDB___memp_fns(dbmfp->dbmp, dbmfp->mfp));
}

/*
 * CDB___memp_fns --
 *  On errors we print whatever is available as the file name.
 *
 * PUBLIC: char * CDB___memp_fns __P((DB_MPOOL *, MPOOLFILE *));
 *
 */
char *
CDB___memp_fns(dbmp, mfp)
  DB_MPOOL *dbmp;
  MPOOLFILE *mfp;
{
  if (mfp->path_off == 0)
    return ((char *)"temporary");

  return ((char *)R_ADDR(&dbmp->reginfo, mfp->path_off));
}
