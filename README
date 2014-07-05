 _   _           _      ____                   
| \ | |         | |    |    \          ( )      
|  \| | ___   __| | ___| | ) |_ _  __ _ _ _ __  
|     |/   \ /    |/ _ \    <|  _|/    | |  _ \ 
| |\  | ( ) | ( | |  __/ | ) | |   ( | | | | | |
|_| \_|\___/ \__ _|\___|____/|_|  \__ _|_|_| |_|


NodeBrain 0.9.02

========================================================================

File:        README for nodebrain-0.9.02 release files

Package:     NodeBrain (nb)

Version:     0.9 - Columbo

Release:     0.9.02 

Date:        July 13, 2014

Reference:   http://www.nodebrain.org
               1) Online documentation
               2) Signatures to verify release file integrity

Warning:     This version has incompatibilities with prior versions that
             may require minor changes to rules used with prior releases.

See NEWS for a list of changes in this release.

======================================================================== 

Installation instructions for Linux/Unix Platforms

a) From source code distribution - install to /usr/local directories

     $ tar -xzf nodebrain-0.9.02.tar.gz
     $ cd nodebrain-0.9.02
     $ ./configure
     $ make
     $ make check
     $ make install

   To get a list of configure script options use --help.

     $ ./configure --help

   You can exclude NodeBrain modules (plugins) from a build using
   a --disable-nb_<module> parameter.  Examples for the Peer and
   Webster modules are illustrated here.

     $ ./configure --disable-nb_peer
     $ ./configure --disable-nb_webster

   To exclude features and components that depend on openssl, add the
   configure script parameter --without-tls.
     
     $ ./configure --without-tls

   This will exclude the following API's from the NodeBrain library:

     nbTls      - Abstraction of SSL/TLS layer provided by OpenSSL.
                  (GnuTLS may be supported also in a future release.)

     nbPeer     - Non-blocking data transport between NodeBrain agents
 
     nbProxy    - Message page forwarding for proxies.

     nbMsg      - Decoupled event streams with reliable one-time delivery

     nbWebster  - Interface for HTTPS communication

   It also excludes the following modules that depend on one or more
   of these excluded API's.

     nb_mail    - SMTP send and receive

     nb_message - Message steam producer, consumer, client and server
 
     nb_webster - Minimal web server for caboodle administration

b) From git repository - requires autoconf/automake/libtools

     $ git clone git://git.code.sf.net/p/nodebrain/nb nodebrain-nb
     $ cd nodebrain-nb
     $ git checkout 0.9.02
     $ ./autogen.sh
     $ ./configure
     $ make
     $ make check
     $ make install

c) From source RPM file 

     $ rpmbuild --rebuild nodebrain-0.9.02-1.el6.src.rpm
     $ rpmbuild --rebuild nodebrain-0.9.02-1.src.rpm

d) From binary RPM file (x86_64 platform example)

     $ rpm --install nodebrain-0.9.02-1.el6.x86_64.rpm
     $ rpm --install nodebrain-0.9.02-1.x86_64.rpm

======================================================================== 

Although prior releases of NodeBrain were tested on various platforms,
at time of release, this version has only been tested on RHEL 6.4, x86_64.
It should work on other Linux platforms, and be relatively easy to port
to Unix platforms.  The project has stopped porting to Windows, and the
conditional compilation for Windows is no longer expected to work.  We
hope to support Windows again at some future release, but it is not a
high priority at this time.  

======================================================================== 

Copyright (C) 2014 Ed Trettevik <eat@nodebrain.org>

NodeBrain is free software; you can modify and/or redistribute it under the
terms of either the MIT License (Expat) or the NodeBrain License.

See COPYING file for licenses.
