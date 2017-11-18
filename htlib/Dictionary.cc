//
// Dictionary.cc
//
// Dictionary: This class provides an object lookup table.  
//             Each object in the dictionary is indexed with a string.  
//             The objects can be returned by mentioning their
//             string index.
//
// Part of the ht://Dig package   <http://www.htdig.org/>
// Copyright (c) 1995-2004 The ht://Dig Group
// For copyright details, see the file COPYING in your distribution
// or the GNU Library General Public License (LGPL) version 2 or later 
// <http://www.gnu.org/copyleft/lgpl.html>
//
// $Id: Dictionary.cc,v 1.16 2004/05/28 13:15:20 lha Exp $
//

#ifdef HAVE_CONFIG_H
#include "htconfig.h"
#endif /* HAVE_CONFIG_H */

#include "Dictionary.h"

#include <stdlib.h>

class DictionaryEntry
{
public:
    unsigned int  hash;
    char    *key;
    Object    *value;
    DictionaryEntry  *next;

    ~DictionaryEntry();
    void    release();
};

DictionaryEntry::~DictionaryEntry()
{
    free(key);
    delete value;
}

void
DictionaryEntry::release()
{
    value = NULL;    // Prevent the value from being deleted
}


//*********************************************************************
//
Dictionary::Dictionary()
{
    init(101, 10.0f);
}

Dictionary::Dictionary(int initialCapacity, float loadFactor)
{
    init(initialCapacity, loadFactor);
}

Dictionary::Dictionary(int initialCapacity)
{
    init(initialCapacity, 0.75f);
}

Dictionary::Dictionary(const Dictionary& other)
{
    init(other.initialCapacity, other.loadFactor);

    DictionaryCursor cursor;
    const char* key;
    for(other.Start_Get(cursor); (key = other.Get_Next(cursor));) {
      Add(key, other[key]);
    }
}

//*********************************************************************
//
Dictionary::~Dictionary()
{
    Destroy();
    
    delete [] table;
}


//*********************************************************************
//
void
Dictionary::Destroy()
{
    DictionaryEntry *t, *n;

    for (int i = 0; i < tableLength; i++)
    {
  if (table[i] != NULL)
  { 
      t = table[i];
      do {                  // clear out hash chain
        n = t->next;
        delete t;
        t = n;
      } while (n);
      table[i] = NULL;
  }
    }
    count = 0;
}

//*********************************************************************
//
void
Dictionary::Release()
{
    DictionaryEntry *t, *n;

    for (int i = 0; i < tableLength; i++)
    {
  if (table[i] != NULL)
  {
      t = table[i];
      do {                  // clear out hash chain
        n = t->next;
        t->release();
        delete t;
        t = n;
      } while (n);
      table[i] = NULL;
  }
    }
    count = 0;
}


//*********************************************************************
//
void
Dictionary::init(int initialCapacity, float loadFactor)
{
    if (initialCapacity <= 0)
  initialCapacity = 101;
    if (loadFactor <= 0.0)
  loadFactor = 0.75f;
    Dictionary::loadFactor = loadFactor;
    table = new DictionaryEntry*[initialCapacity];
    for (int i = 0; i < initialCapacity; i++)
    {
  table[i] = NULL;
    }
    threshold = (int)(initialCapacity * loadFactor);
    tableLength = initialCapacity;
    count = 0;
}

//*********************************************************************
//
unsigned int
Dictionary::hashCode(const char *key) const
{
    char *test;
    long  conv_key = strtol(key,  &test, 10);
    if (key && *key && !*test) // Conversion succeeded
      return conv_key;

    char *base = (char*)malloc(strlen(key) + 2);
    char *tmp_key = base;
    strcpy(tmp_key, key);

    unsigned int h = 0;
    int length = strlen(tmp_key);

    if (length >= 16)
      {
  tmp_key += strlen(tmp_key) - 15;
  length = strlen(tmp_key);
      }
    for (int i = length; i > 0; i--)
      {
  h = (h*37) + *tmp_key++;
      }

    free(base);
    return h;
}

//*********************************************************************
//   Add an entry to the hash table.  This will replace the
//   data associated with an already existing key.
//
void
Dictionary::Add(const String& name, Object *obj)
{
    unsigned int  hash = hashCode(name);
    int      index = hash % tableLength;
    DictionaryEntry  *e;
    
    for (e = table[index]; e != NULL; e = e->next)
    {
  if (e->hash == hash && strcmp(e->key, name) == 0)
  {
      delete e->value;
      e->value = obj;
      return;
  }
    }

    if (count >= threshold)
    {
  rehash();
  Add(name, obj);
  return;
    }

    e = new DictionaryEntry();
    e->hash = hash;
    e->key = strdup(name);
    e->value = obj;
    e->next = table[index];
    table[index] = e;
    count++;
}


//*********************************************************************
//   Remove an entry from the hash table.
//
int
Dictionary::Remove(const String& name)
{
    if (!count)
      return 0;

    unsigned int  hash = hashCode(name);
    int      index = hash % tableLength;
    DictionaryEntry  *e, *prev;

    for (e = table[index], prev = NULL; e != NULL; prev = e, e = e->next)
    {
  if (hash == e->hash && strcmp(e->key, name) == 0)
  {
      if (prev != NULL)
      {
    prev->next = e->next;
      }
      else
      {
    table[index] = e->next;
      }
      count--;
            delete e;
      return 1;
  }
    }
    return 0;
}


//*********************************************************************
//
Object *Dictionary::Find(const String& name) const
{
    if (!count)
  return NULL;

    unsigned int  hash = hashCode(name);
    int      index = hash % tableLength;
    DictionaryEntry  *e;

    for (e = table[index]; e != NULL; e = e->next)
    {
  if (e->hash == hash && strcmp(e->key, name) == 0)
  {
      return e->value;
  }
    }
    return NULL;
}


//*********************************************************************
//
Object *Dictionary::operator[](const String& name) const
{
    return Find(name);
}


//*********************************************************************
//
int Dictionary::Exists(const String& name) const
{
    if (!count)
      return 0;

    unsigned int  hash = hashCode(name);
    int      index = hash % tableLength;
    DictionaryEntry  *e;

    for (e = table[index]; e != NULL; e = e->next)
    {
  if (e->hash == hash && strcmp(e->key, name) == 0)
  {
      return 1;
  }
    }
    return 0;
}


//*********************************************************************
//
void
Dictionary::rehash()
{
    DictionaryEntry  **oldTable = table;
    int      oldCapacity = tableLength;

    int      newCapacity;
    DictionaryEntry  *e;
    int      i, index;
    
    newCapacity = count > oldCapacity ? count * 2 + 1 : oldCapacity * 2 + 1;

    DictionaryEntry  **newTable = new DictionaryEntry*[newCapacity];

    for (i = 0; i < newCapacity; i++)
    {
  newTable[i] = NULL;
    }

    threshold = (int) (newCapacity * loadFactor);
    table = newTable;
    tableLength = newCapacity;
    
    for (i = oldCapacity; i-- > 0;)
    {
  for (DictionaryEntry *old = oldTable[i]; old != NULL;)
  {
      e = old;
      old = old->next;
      index = e->hash % newCapacity;
      e->next = newTable[index];
      newTable[index] = e;
  }
    }
    delete [] oldTable;
}


//*********************************************************************
//
void
Dictionary::Start_Get(DictionaryCursor& cursor) const
{
    cursor.currentTableIndex = -1;
    cursor.currentDictionaryEntry = NULL;
}


//*********************************************************************
//
char *
Dictionary::Get_Next(DictionaryCursor& cursor) const
{
    while (cursor.currentDictionaryEntry == NULL ||
     cursor.currentDictionaryEntry->next == NULL)
    {
  cursor.currentTableIndex++;

  if (cursor.currentTableIndex >= tableLength)
  {
      cursor.currentTableIndex--;
      return NULL;
  }

  cursor.currentDictionaryEntry = table[cursor.currentTableIndex];

  if (cursor.currentDictionaryEntry != NULL)
  {
      return cursor.currentDictionaryEntry->key;
  }
    }

    cursor.currentDictionaryEntry = cursor.currentDictionaryEntry->next;
    return cursor.currentDictionaryEntry->key;
}

//*********************************************************************
//
Object *
Dictionary::Get_NextElement(DictionaryCursor& cursor) const
{
    while (cursor.currentDictionaryEntry == NULL ||
     cursor.currentDictionaryEntry->next == NULL)
    {
  cursor.currentTableIndex++;

  if (cursor.currentTableIndex >= tableLength)
  {
      cursor.currentTableIndex--;
      return NULL;
  }

  cursor.currentDictionaryEntry = table[cursor.currentTableIndex];

  if (cursor.currentDictionaryEntry != NULL)
  {
      return cursor.currentDictionaryEntry->value;
  }
    }

    cursor.currentDictionaryEntry = cursor.currentDictionaryEntry->next;
    return cursor.currentDictionaryEntry->value;
}
