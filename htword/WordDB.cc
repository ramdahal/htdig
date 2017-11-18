//
// WordDB.cc
//
// Part of the ht://Dig package   <http://www.htdig.org/>
// Copyright (c) 1999-2004 The ht://Dig Group
// For copyright details, see the file COPYING in your distribution
// or the GNU Library General Public License (LGPL) version 2 or later
// <http://www.gnu.org/copyleft/lgpl.html>
//
// $Id: WordDB.cc,v 1.10 2004/05/28 13:15:26 lha Exp $
//

#ifdef HAVE_CONFIG_H
#include "htconfig.h"
#endif /* HAVE_CONFIG_H */

#include "defaults.h"
#include "WordDB.h"

#include "../db/db.h"

const char* dberror(int errval) {
#define DB_MAX_ERROR  (-DB_TXN_CKP + 1)
  static const char* dbstr[DB_MAX_ERROR] = {
    "",
    "DB_INCOMPLETE",
    "DB_KEYEMPTY",
    "DB_KEYEXISTS",
    "DB_LOCK_DEADLOCK",
    "DB_LOCK_NOTGRANTED",
    "DB_LOCK_NOTHELD",
    "DB_NOTFOUND",
    "DB_RUNRECOVERY",
    "DB_DELETED",
    "DB_NEEDSPLIT",
    "DB_SWAPBYTES",
    "DB_TXN_CKP",
  };
  if(errval < 0 && -errval < DB_MAX_ERROR)
    return dbstr[-errval];
  else
    return strerror(errval);
}

int WordDB::Open(const String& filename, DBTYPE type, int flags, int mode) {
  if(is_open) {
    int error = 0;
    if((error = Close()) != 0)
      return error;
  }

  if(!dbenv) {
    const char* progname = "WordDB";

    //
    // Environment initialization
    //
    // Output errors to the application's log.
    //
    db->set_errfile(db, stderr);
    db->set_errpfx(db, progname);
 
  }

  int error = db->open(db, filename, NULL, type, (u_int32_t)flags, mode);

  if(error == 0)
    is_open = 1;

  return error;
}
