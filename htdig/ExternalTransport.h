//
// ExternalTransport.h
//
// ExternalTransport: Allows external programs to retrieve given URLs with
//                    unknown protocols.
//
// Part of the ht://Dig package   <http://www.htdig.org/>
// Copyright (c) 1999 The ht://Dig Group
// For copyright details, see the file COPYING in your distribution
// or the GNU Public License version 2 or later
// <http://www.gnu.org/copyleft/gpl.html>
//
// $Id: ExternalTransport.h,v 1.2 2000/02/19 05:28:51 ghutchis Exp $
//

#ifndef _ExternalTransport_h_
#define _ExternalTransport_h_

#include "Transport.h"
#include "htString.h"

#include <stdio.h>

// First we must declare a derived Transport_Response class
// This requires declaring the main class in advance
class ExternalTransport;
class ExternalTransport_Response : public Transport_Response
{
  friend ExternalTransport;

  // Nothing else... We just want it so we can access the protected fields
};

// Right, now we get on with the show...
class ExternalTransport : public Transport
{
public:
    //
    // Construction/Destruction
    //
                        ExternalTransport(char *protocol);
    virtual		~ExternalTransport();


    //
    // Check if the given protocol has a handler
    //
    static int		canHandle(char *protocol);
    
    // Setting connections is obviously a bit different than the base class
    // from a URL pointer
    void SetConnection (URL *u);
    
    // from a URL object
    void SetConnection (URL &u)
        { SetConnection (&u); }

    // Make the request
    DocStatus Request();
   
    // Get the response or the status
    ExternalTransport_Response *GetResponse()	 { return _Response; }
    DocStatus GetDocumentStatus() { return GetDocumentStatus(_Response); }
    

private:
    // The command to handle the current protocol
    String			_Handler;
    // And the current protocol
    String			_Protocol;
    
    // The URL to Request()
    URL				_URL;
    
    // The result of the Request()
    ExternalTransport_Response	*_Response;

    
    
    // Private helper to read in the result from the handler
    int			readLine(FILE *, String &);
    // Work out the DocStatus from the HTTP-style status codes
    DocStatus		GetDocumentStatus(ExternalTransport_Response *r);
};

#endif


