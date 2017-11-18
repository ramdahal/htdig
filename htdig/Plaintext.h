//
// Plaintext.h
//
// Plaintext: Parses plaintext files. Not much to do, really.
//
// Part of the ht://Dig package   <http://www.htdig.org/>
// Copyright (c) 1995-2004 The ht://Dig Group
// For copyright details, see the file COPYING in your distribution
// or the GNU Library General Public License (LGPL) version 2 or later
// <http://www.gnu.org/copyleft/lgpl.html>
//
// $Id: Plaintext.h,v 1.6 2004/05/28 13:15:15 lha Exp $
//
#ifndef _Plaintext_h_
#define _Plaintext_h_

#include "Parsable.h"

class URL;


class Plaintext : public Parsable
{
public:
    //
    // Construction/Destruction
    //
                        Plaintext();
    virtual    ~Plaintext();

    //
    // Main parser interface.
    //
    virtual void  parse(Retriever &retriever, URL &);
  
private:
};

#endif


