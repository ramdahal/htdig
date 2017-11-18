//
// HtWordList.cc
//
// HtWordList: Specialized WordList class that can hold a list 
//         of words waiting to be inserted in the database.
//
// Part of the ht://Dig package   <http://www.htdig.org/>
// Copyright (c) 1995-2004 The ht://Dig Group
// For copyright details, see the file COPYING in your distribution
// or the GNU Library General Public License (LGPL) version 2 or later
// <http://www.gnu.org/copyleft/lgpl.html>
//
// $Id: HtWordList.cc,v 1.7 2004/05/28 13:15:12 lha Exp $
//

#ifdef HAVE_CONFIG_H
#include "htconfig.h"
#endif /* HAVE_CONFIG_H */

#include "HtWordList.h"
#include "HtWordReference.h"
#include "WordRecord.h"
#include "WordType.h"
#include "HtConfiguration.h"
#include "htString.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef HAVE_STD
#include <iostream>
#include <fstream>
#ifdef HAVE_NAMESPACES
using namespace std;
#endif
#else
#include <iostream.h>
#include <fstream.h>
#endif /* HAVE_STD */

#include <errno.h>

//*****************************************************************************
// HtWordList::~HtWordList()
//
HtWordList::~HtWordList()
{
    delete words;
}

//*****************************************************************************
//
HtWordList::HtWordList(const HtConfiguration& config_arg) :
  WordList(config_arg)
{
    words = new List;
}

//*****************************************************************************
//
void HtWordList::Replace(const WordReference& arg)
{
  //
  // New word.  Create a new reference for it and cache it in the object.
  //
  words->Add(new WordReference(arg));
}

//*****************************************************************************
// void HtWordList::Flush()
//   Dump the current list of words to the database.  After
//   the words have been dumped, the list will be destroyed to make
//   room for the words of the next document.
//   
void HtWordList::Flush()
{
  HtWordReference  *wordRef;

    // Provided for backwards compatibility
  if (!isopen)
    Open(config["word_db"], O_RDWR);

  words->Start_Get();
  while ((wordRef = (HtWordReference *) words->Get_Next()))
    {
      if (wordRef->Word().length() == 0) {
  cerr << "HtWordList::Flush: unexpected empty word\n";
  continue;
      }

      Override(*wordRef);
    }  
    
  // Cleanup
  words->Destroy();
}

//*****************************************************************************
// void HtWordList::Skip()
//   The current document has disappeared or been modified. 
//   We do not need to store these words.
//
void HtWordList::Skip()
{
  words->Destroy();
}

//
// Callback data dedicated to Dump and dump_word communication
//
class DumpWordData : public Object
{
public:
  DumpWordData(FILE* fl_arg) { fl = fl_arg; }

  FILE* fl;
};

//*****************************************************************************
//
// Write the ascii representation of a word occurence. Helper
// of WordList::Dump
//
static int dump_word(WordList *, WordDBCursor &, const WordReference *word, Object &data)
{
  const HtWordReference *word_tmp = (const HtWordReference *)word;

  DumpWordData &info = (DumpWordData &)data;

  word_tmp->Dump(info.fl);

  return OK;
}

//*****************************************************************************
// int HtWordList::Dump(char* filename)
//
// Write an ascii version of the word database in <filename>
//
int HtWordList::Dump(const String& filename)
{
  FILE    *fl;

  if (!isopen) {
    cerr << "WordList::Dump: database must be opened first\n";
    return NOTOK;
  }

  if((fl = fopen(filename, "w")) == 0) {
    perror(form("WordList::Dump: opening %s for writing", (const char*)filename));
    return NOTOK;
  }

  HtWordReference::DumpHeader(fl);
  DumpWordData data(fl);
  WordCursor* search = Cursor(dump_word, &data);
  search->Walk();
  delete search;
  
  fclose(fl);

  return OK;
}

//*****************************************************************************
// int HtWordList::Load(char* filename)
//
// Read in an ascii version of the word database in <filename>
//
int HtWordList::Load(const String& filename)
{
  FILE    *fl;
  String  data;
  HtWordReference *next;

  if (!isopen) {
    cerr << "WordList::Load: database must be opened first\n";
    return NOTOK;
  }

  if((fl = fopen(filename, "r")) == 0) {
    perror(form("WordList::Load: opening %s for reading", (const char*)filename));
    return NOTOK;
  }

  if (HtWordReference::LoadHeader(fl) != OK)
    {
      cerr << "WordList::Load: header is not correct\n";
      return NOTOK;
    }

  while (data.readLine(fl))
    {
      next = new HtWordReference;
      if (next->Load(data) != OK)
  {
    delete next;
    continue;
  }
  
      words->Add(next);
    }

  Flush();
  fclose(fl);

  return OK;
}
