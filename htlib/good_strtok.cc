//
// good_strtok.cc
//
// good_strtok: The good_strtok() function is very similar to the 
//              standard strtok() library function, except that good_strtok() 
//    will only skip over 1 separator if it finds one.  This is
//              needed when parsing strings with empty fields.
//
// Part of the ht://Dig package   <http://www.htdig.org/>
// Copyright (c) 1999-2004 The ht://Dig Group
// For copyright details, see the file COPYING in your distribution
// or the GNU Library General Public License (LGPL) version 2 or later 
// <http://www.gnu.org/copyleft/lgpl.html>
//
// $Id: good_strtok.cc,v 1.7 2004/05/28 13:15:21 lha Exp $
//

#ifdef HAVE_CONFIG_H
#include "htconfig.h"
#endif /* HAVE_CONFIG_H */

#include "lib.h"

//
// Perform the same function as the standard strtok() function except that
// multiple separators are NOT collapsed into one.
//
char *good_strtok(char *str, char term)
{
    static char    *string;

    if (str)
    {
  string = str;
    }

    if (string == NULL || *string == '\0')
  return NULL;

    char *p = string;
    while (*string && *string!=term)
  string++;
    if (*string)
  *string++ = '\0';
    return p;
}
