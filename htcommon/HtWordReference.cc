//
// HtWordReference.cc
//
// Part of the ht://Dig package   <http://www.htdig.org/>
// Copyright (c) 1999-2004 The ht://Dig Group
// For copyright details, see the file COPYING in your distribution
// or the GNU Library General Public License (LGPL) version 2 or later
// <http://www.gnu.org/copyleft/lgpl.html>
//
// $Id: HtWordReference.cc,v 1.5 2004/05/28 13:15:12 lha Exp $
//

#ifdef HAVE_CONFIG_H
#include "htconfig.h"
#endif /* HAVE_CONFIG_H */

#include "HtWordReference.h"
#include <stdio.h>
#include <stdlib.h>

//
// Explain the format of data output of the Dump function
//
int HtWordReference::DumpHeader(FILE *fl)
{
  fprintf(fl, "#word\tdocument id\tflags\tlocation\tanchor\n");
  return OK;
}

//
// Ascii representation of a word occurence.
//
int HtWordReference::Dump(FILE *fl) const
{
  fprintf(fl, "%s\t%d\t%d\t%d\t%d\n",
    (char*)Word(),
    DocID(),
    Flags(),
    Location(),
    Anchor());
  return OK;
}

//
// Check the header of the file
//
int HtWordReference::LoadHeader(FILE *fl)
{
  String header;
  header.readLine(fl);
  if (mystrcasecmp("#word\tdocument id\tflags\tlocation\tanchor", header.get()) == 0)
    return OK;
  else
    return NOTOK;
}

//
// Ascii representation of a word occurence.
//
int HtWordReference::Load(const String& s)
{
  String data(s);
  char *token;

  // Format is "%s\t%d\t%d\t%d\t%d

  token = strtok(data, "\t");
  if (!token)
    return NOTOK;
  Word(token);

  token = strtok(0, "\t");
  if (!token)
    return NOTOK;
  DocID(atoi(token));

  token = strtok(0, "\t");
  if (!token)
    return NOTOK;
  Flags(atoi(token));

  token = strtok(0, "\t");
  if (!token)
    return NOTOK;
  Location(atoi(token));

  token = strtok(0, "\t");
  if (!token)
    return NOTOK;
  Anchor(atoi(token));

  return OK;
}

