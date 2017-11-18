//
// WordStat.h
//
// WordStat: Kind of record that holds statistics about each distinct word
//            in the database.
//
// Part of the ht://Dig package   <http://www.htdig.org/>
// Copyright (c) 1999-2004 The ht://Dig Group
// For copyright details, see the file COPYING in your distribution
// or the GNU Library General Public License (LGPL) version 2 or later
// <http://www.gnu.org/copyleft/lgpl.html>
//
// $Id: WordStat.h,v 1.5 2004/05/28 13:15:28 lha Exp $
//
#ifndef _WordStat_h_
#define _WordStat_h_

#include "WordReference.h"

class WordStat : public WordReference
{
 public:
  //
  // Construction/Destruction
  //
  WordStat()  { record.type = WORD_RECORD_STATS; }
  WordStat(const String& key_arg, const String& record_arg) : WordReference(key_arg, record_arg) {
    record.type = WORD_RECORD_STATS;
  }
  WordStat(const String& word) {
    Clear();
    key.SetWord(String("\001") + word);
    record.type = WORD_RECORD_STATS;
  }
  
  ~WordStat()  {}

  //
  // Accessors
  //
  unsigned int Noccurrence() const { return record.info.stats.noccurrence; }
  unsigned int &Noccurrence() { return record.info.stats.noccurrence; }

  //
  // Return upper boundary key of reference count records
  //
  static inline const WordReference& Last() {
    if(!word_stat_last)
      word_stat_last = new WordReference("\002");
    return *word_stat_last;
  }

 protected:

  static WordReference*    word_stat_last;
};

#endif


