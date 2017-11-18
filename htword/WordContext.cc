//
// WordContext.cc 
//
// WordContext: call Initialize for all classes that need to.
//              This will enable the Instance() static member
//              of each to return a properly allocated and configured 
//              object.
//
// Part of the ht://Dig package   <http://www.htdig.org/>
// Copyright (c) 1999-2004 The ht://Dig Group
// For copyright details, see the file COPYING in your distribution
// or the GNU Library General Public License (LGPL) version 2 or later
// <http://www.gnu.org/copyleft/lgpl.html>
//
// $Id: WordContext.cc,v 1.5 2004/05/28 13:15:26 lha Exp $
//

#ifdef HAVE_CONFIG_H
#include "htconfig.h"
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>

#include "WordContext.h"
#include "WordType.h"
#include "WordKeyInfo.h"
#include "WordDBInfo.h"
#include "WordRecord.h"
#include "WordMonitor.h"

void WordContext::Initialize(Configuration &config)
{
#if !defined(HAVE_LIBZ) || !defined(HAVE_ZLIB_H)
  config.Add("wordlist_compress", "false");
#endif

  WordType::Initialize(config);
  WordKeyInfo::Initialize(config);
  WordRecordInfo::Initialize(config);
  WordDBInfo::Initialize(config);
  if(config.Boolean("wordlist_monitor"))
    WordMonitor::Initialize(config);
}

Configuration *WordContext::Initialize(const ConfigDefaults* config_defaults /* = 0 */)
{
  Configuration *config = new Configuration();

  if(config_defaults)
    config->Defaults(config_defaults);

  String filename;
  //
  // Check file pointed by MIFLUZ_CONFIG environment variable
  //
  if(getenv("MIFLUZ_CONFIG")) {
    filename << getenv("MIFLUZ_CONFIG");
    struct stat statbuf;
    if(stat((char*)filename, &statbuf) < 0) {
      if(errno != ENOENT) {
  fprintf(stderr, "WordContext::Initialize: MIFLUZ_CONFIG could not stat %s\n", (char*)filename);
  perror("");
      }
      filename.trunc();
    }
  }
  //
  // Check for ~/.mifluz
  //
  if(filename.empty()) {
    const char* home = getenv("HOME");
    if(home) {
      filename << home << "/.mifluz";
      struct stat statbuf;
      if(stat((char*)filename, &statbuf) < 0) {
  if(errno != ENOENT) {
    fprintf(stderr, "WordContext::Initialize: could not stat %s\n", (char*)filename);
    perror("");
  }
  filename.trunc();
      }
    }
  }

  if(!filename.empty())
    config->Read(filename);

  Initialize(*config);

  if(filename.empty() && !config_defaults) {
    delete config;
    config = 0;
  }

  return config;
}

void WordContext::Finish()
{
  delete WordType::Instance();
  delete WordKeyInfo::Instance();
  delete WordRecordInfo::Instance();
  delete WordDBInfo::Instance();
  if(WordMonitor::Instance()) delete WordMonitor::Instance();
}
