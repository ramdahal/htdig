//
// htpurge.cc
//
// htpurge: A utility to remove specified URLs and any documents
// marked for removal from the word and document databases.
//
// Part of the ht://Dig package   <http://www.htdig.org/>
// Copyright (c) 1999-2004 The ht://Dig Group
// For copyright details, see the file COPYING in your distribution
// or the GNU Library General Public License (LGPL) version 2 or later
// <http://www.gnu.org/copyleft/lgpl.html>
//
// $Id: htpurge.cc,v 1.6 2004/05/28 13:15:25 lha Exp $
//

#ifdef HAVE_CONFIG_H
#include "htconfig.h"
#endif /* HAVE_CONFIG_H */

#include <fcntl.h>

#include "WordContext.h"
#include "HtWordReference.h"
#include "HtConfiguration.h"
#include "DocumentDB.h"
#include "DocumentRef.h"
#include "defaults.h"
#include "HtURLCodec.h"

#include <errno.h>

#ifndef _MSC_VER /* _WIN32 */
#include <unistd.h>
#endif

// If we have this, we probably want it.
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#elif HAVE_GETOPT_LOCAL
#include <getopt_local.h>
#endif

int      verbose = 0;

Dictionary *purgeDocs(Dictionary *);
void purgeWords(Dictionary *);
void usage();
void reportError(const char *msg);

//*****************************************************************************
// int main(int ac, char **av)
//
int main(int ac, char **av)
{
    int      alt_work_area = 0;
    String    configfile = DEFAULT_CONFIG_FILE;
    int      c;
    extern char    *optarg;
    Dictionary    *discard_ids = 0;
    Dictionary    *discard_urls = new Dictionary;

    while ((c = getopt(ac, av, "vc:au:")) != -1)
    {
  switch (c)
  {
      case 'c':
    configfile = optarg;
    break;
      case 'v':
    verbose++;
    break;
      case 'a':
    alt_work_area++;
    break;
      case 'u':
          discard_urls->Add(optarg, NULL);
    break;
      case '?':
    usage();
    break;
  }
    }

  HtConfiguration* config= HtConfiguration::config();
    config->Defaults(&defaults[0]);

    if (access((char*)configfile, R_OK) < 0)
    {
  reportError(form("Unable to find configuration file '%s'",
       configfile.get()));
    }
  
    config->Read(configfile);

    //
    // Check url_part_aliases and common_url_parts for
    // errors.
    String url_part_errors = HtURLCodec::instance()->ErrMsg();

    if (url_part_errors.length() != 0)
      reportError(form("Invalid url_part_aliases or common_url_parts: %s",
                       url_part_errors.get()));

    if (alt_work_area != 0)
    {
  String  configValue;

  configValue = config->Find("word_db");
  if (configValue.length() != 0)
  {
      configValue << ".work";
      config->Add("word_db", configValue);
  }

  configValue = config->Find("doc_db");
  if (configValue.length() != 0)
  {
      configValue << ".work";
      config->Add("doc_db", configValue);
  }

  configValue = config->Find("doc_index");
  if (configValue.length() != 0)
  {
      configValue << ".work";
      config->Add("doc_index", configValue);
  }

  configValue = config->Find("doc_excerpt");
  if (configValue.length() != 0)
  {
      configValue << ".work";
      config->Add("doc_excerpt", configValue);
  }
    }

    if (optind < ac && strcmp(av[optind], "-") == 0)
    {
  String str;
  while (!cin.eof())
  {
      cin >> str;
      str.chop("\r\n");
      if (str.length() > 0)
    discard_urls->Add(str, NULL);
  }
    }

    WordContext::Initialize(*config);

    // We pass in our list of URLs (which may be empty)
    // and we get back the list of IDs purged from the doc DB
    // to make sure words with these IDs are purged
    discard_ids = purgeDocs(discard_urls);
    delete discard_urls;
    discard_urls = 0;

    purgeWords(discard_ids);
    delete discard_ids;
    discard_ids = 0;

    return 0;
}

//*****************************************************************************
// Dictionary purgeDocs(Dictionary &purgeURLs)
// Pass in a hash of the URLs to delete (it could be empty)
// Return a hash of the IDs deleted from the doc DB
//
Dictionary *purgeDocs(Dictionary *purgeURLs)
{
  HtConfiguration* config= HtConfiguration::config();
    const String  doc_db = config->Find("doc_db");
    const String  doc_index = config->Find("doc_index");
    const String  doc_excerpt = config->Find("doc_excerpt");
    int      remove_unused;
    int      remove_unretrieved;
    DocumentDB    db;
    List    *IDs;
    int      document_count = 0;
    Dictionary    *discard_list = new Dictionary;

    //
    // Start the conversion by going through all the URLs that are in
    // the document database
    //
    if(db.Open(doc_db, doc_index, doc_excerpt) != OK)
      return discard_list; // It's empty right now
    
    IDs = db.DocIDs();

    if (IDs->Count() == 0)
      reportError("Database is empty!");
    
    IDs->Start_Get();
    IntObject    *id;
    String    idStr;
    String    url;
    URL      u_url;

    while ((id = (IntObject *) IDs->Get_Next()))
    {
  DocumentRef  *ref = db[id->Value()];

  if (!ref)
      continue;

  db.ReadExcerpt(*ref);
  url = ref->DocURL();
        u_url = URL((char *)url);

  remove_unused = config->Boolean("server", u_url.host() ,"remove_bad_urls");
  remove_unretrieved = config->Boolean("server", u_url.host(), "remove_unretrieved_urls");
  idStr = 0;
  idStr << id->Value();

  if (ref->DocState() == Reference_noindex)
    {
      // This document either wasn't found or shouldn't be indexed.
      db.Delete(ref->DocID());
            if (verbose)
              cout << "Deleted, noindex: ID: " << idStr << " URL: "
                   << url << endl;
      discard_list->Add(idStr.get(), NULL);
    }
  else if (ref->DocState() == Reference_obsolete)
    {
      // This document was replaced by a newer one
      db.Delete(ref->DocID());
            if (verbose)
              cout << "Deleted, obsolete: ID: " << idStr << " URL: "
                   << url << endl;
      discard_list->Add(idStr.get(), NULL);
    }
  else if (remove_unused && ref->DocState() == Reference_not_found)
    {
      // This document wasn't actually found
      db.Delete(ref->DocID());
            if (verbose)
              cout << "Deleted, not found: ID: " << idStr << " URL: "
                   << url << endl;
      discard_list->Add(idStr.get(), NULL);
    }
  else if (remove_unused && strlen(ref->DocHead()) == 0 
     && ref->DocAccessed() != 0)
    {
      // For some reason, this document was retrieved, but doesn't
      // have an excerpt (probably because of a noindex directive)
      db.Delete(ref->DocID());
            if (verbose)
              cout << "Deleted, no excerpt: ID: " << idStr << " URL:  "
                   << url << endl;
      discard_list->Add(idStr.get(), NULL);
    }
  else if (remove_unretrieved && ref->DocAccessed() == 0)
    {
      // This document has not been retrieved
      db.Delete(ref->DocID());
            if (verbose)
              cout << "Deleted, never retrieved: ID: " << idStr << " URL:  "
                   << url << endl;
      discard_list->Add(idStr.get(), NULL);
    }
  else if (purgeURLs->Exists(url))
    {
      // This document has been marked to be purged by the user
      db.Delete(ref->DocID());
      if (verbose)
        cout << "Deleted, marked by user input: ID: " << idStr << " URL: "
       << url << endl;
      discard_list->Add(idStr.get(), NULL);
    }
  else
    {
      // This is a valid document. Let's keep stats on it.
            if (verbose > 1)
              cout << "ID: " << idStr << " URL: " << url << endl;

      document_count++;
      if (verbose && document_count % 10 == 0)
      {
    cout << "htpurge: " << document_count << '\n';
    cout.flush();
      }
    }
        delete ref;
    }
    if (verbose)
  cout << "\n";

    delete IDs;
    db.Close();

    return discard_list;
}

//
// Callback data dedicated to Dump and dump_word communication
//
class DeleteWordData : public Object
{
public:
  DeleteWordData(const Dictionary& discard_arg) : discard(discard_arg) { deleted = remains = 0; }

  const Dictionary& discard;
  int deleted;
  int remains;
};

//*****************************************************************************
//
//
static int delete_word(WordList *words, WordDBCursor& cursor, const WordReference *word_arg, Object &data)
{
  const HtWordReference *word = (const HtWordReference *)word_arg;
  DeleteWordData& d = (DeleteWordData&)data;
  static String docIDStr;

  docIDStr = 0;
  docIDStr << word->DocID();

  if(d.discard.Exists(docIDStr)) {
    if(words->Delete(cursor) != 0) {
      cerr << "htpurge: deletion of " << (char*)word->Get() << " failed " << strerror(errno) << "\n";
      return NOTOK;
    }
    if (verbose)
      {
  cout << "htpurge: Discarding ";
  if(verbose > 2)
    cout << (char*)word->Get();
  else 
    cout << word->Word();
  cout << "\n";
  cout.flush();
      }
    d.deleted++;
  } else {
    d.remains++;
  }

  return OK;
}

//*****************************************************************************
// void purgeWords(Dictionary *discard_list)
//
void purgeWords(Dictionary *discard_list)
{
  HtConfiguration* config= HtConfiguration::config();
  HtWordList    words(*config);
  DeleteWordData  data(*discard_list); 

  words.Open(config->Find("word_db"), O_RDWR);
  WordCursor* search = words.Cursor(delete_word, &data);
  search->Walk();
  delete search;
  
  words.Close();

  if (verbose)
    cout << "\n";

}

//*****************************************************************************
// void usage()
//   Display program usage information
//
void usage()
{
    cout << "usage: htpurge [-][-u url][-v][-a][-c configfile]\n";
    cout << "This program is part of ht://Dig " << VERSION << "\n\n";
    cout << "Options:\n";
    cout << "\t-\tURL input. Read in a list of URLs to remove, one per line.\n\n";
    cout << "\t-u\tURL input. Add this url to the list of URLs to remove.\n";
    cout << "\t\t(can be specified multiple times)\n\n";
    cout << "\t-v\tVerbose mode.  This increases the verbosity of the\n";
    cout << "\t\tprogram.  Using more than 2 is probably only useful\n";
    cout << "\t\tfor debugging purposes.  The default verbose mode\n";
    cout << "\t\tgives a progress on what it is doing and where it is.\n\n";
    cout << "\t-a\tUse alternate work files.\n";
    cout << "\t\tTells htpurge to append .work to the database files \n";
    cout << "\t\tallowing it to operate on a second set of databases.\n\n";
    cout << "\t-c configfile\n";
    cout << "\t\tUse the specified configuration file instead on the\n";
    cout << "\t\tdefault.\n\n";
    exit(0);
}


//*****************************************************************************
// Report an error and die
//
void reportError(const char *msg)
{
    cout << "htpurge: " << msg << "\n\n";
    exit(1);
}
