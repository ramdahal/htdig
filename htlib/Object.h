//
// Object.h
//
// Object: This baseclass defines how an object should behave.
//         This includes the ability to be put into a list
//
// Part of the ht://Dig package   <http://www.htdig.org/>
// Copyright (c) 1999-2004 The ht://Dig Group
// For copyright details, see the file COPYING in your distribution
// or the GNU Library General Public License (LGPL) version 2 or later 
// <http://www.gnu.org/copyleft/lgpl.html>
//
// $Id: Object.h,v 1.9 2004/05/28 13:15:21 lha Exp $
//

#ifndef  _Object_h_
#define  _Object_h_

#include "lib.h"
#include <stdio.h>

class String;

class Object
{
public:
  //
  // Constructor/Destructor
  //
      Object()  {}
  virtual    ~Object()  {}

  //
  // To ensure a consistent comparison interface and to allow comparison
  // of all kinds of different objects, we will define a comparison functions.
  //
  virtual int  compare(const Object &)  const { return 0;}

  //
  // To allow a deep copy of data structures we will define a standard interface...
  // This member will return a copy of itself, freshly allocated and deep copied.
  //
  virtual Object  *Copy() const { fprintf(stderr, "Object::Copy: derived class does not implement Copy\n"); return new Object(); }
};


#endif
