 _   _           _      ____                   
| \ | |         | |    |    \          ( )      
|  \| | ___   __| | ___| | ) |_ _  __ _ _ _ __  
|     |/   \ /    |/ _ \    <|  _|/    | |  _ \ 
| |\  | ( ) | ( | |  __/ | ) | |   ( | | | | | |
|_| \_|\___/ \__ _|\___|____/|_|  \__ _|_|_| |_|


NodeBrain 0.9.03

========================================================================

File:        README for nodebrain-0.9.03 release files

Package:     NodeBrain (nb)

Version:     0.9 - Columbo

Release:     0.9.03 

Date:        November 30, 2014

Reference:   http://www.nodebrain.org
               1) Online documentation
               2) Signatures to verify release file integrity

Warning:     This version has incompatibilities with prior versions that
             may require minor changes to rules used with prior releases.

See NEWS for a list of changes in this release.

======================================================================== 

Installation instructions for Linux/Unix Platforms

a) From source code distribution - install to /usr/local directories

     $ tar -xzf nodebrain-0.9.03.tar.gz
     $ cd nodebrain-0.9.03
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
     $ git checkout 0.9.03
     $ ./autogen.sh
     $ ./configure
     $ make
     $ make check
     $ make install

c) To install to an alternate location (replace last step of a or b)

     $ make install DESTDIR=$HOME/usr
     $ make install DESTDIR=/usr

d) To create an RPM file instead of installing

     $ make rpm      [this replaces the make install step]

e) Build binary RPM for source RPM file 

     $ rpmbuild --rebuild nodebrain-0.9.03-1.el6.src.rpm
     $ rpmbuild --rebuild nodebrain-0.9.03-1.src.rpm

f) Install from binary RPM file (x86_64 platform example)

     $ rpm --install nodebrain-0.9.03-1.el6.x86_64.rpm
     $ rpm --install nodebrain-0.9.03-1.x86_64.rpm

======================================================================== 

This release was tested on a CentOS 6.5 x86_64 platform.

The Coverity Scan service is used to check for defects.

The openSUSE Build Service is used to test builds for varius Linux
distributions on i586 and x86_64 architectures. The results are shown
below.  There are still some package policy warning to be cleaned up
on the platforms where the build was successful.  

Success:

  rpm Fedora        20, 19, 18, 17
  rpm CentOS        7, 6
  rpm Scientific    7, 6
  rpm openSUSE      TumbleWeed
  rpm openSUSE      Factory
  rpm openSUSE      13.2, 13.1, 12.3, 12.2, 12.1
  rpm SLE           11 SP1

  deb Debian        7, 6
  deb xUbuntu       14.10, 14.04 13.10, 12.10, 12.04
  deb Univention    3.2, 3.1

Fail:

  rpm RHEL          7, 6, 5                 - libedit unresolvable
  rpm CentOS        5                       - libedit unresolvable
  rpm SLE           12, 11 SP3, 11 SP2, 10  - libedit 

The project has stopped porting to Windows, and the conditional
compilation for Windows is no longer expected to work.  We may support
Windows again in the future, but it is not a high priority at this time.  

======================================================================== 

Copyright (C) 2014 Ed Trettevik <eat@nodebrain.org>

NodeBrain is free software; you can modify and/or redistribute it under the
terms of either the MIT License (Expat) or the NodeBrain License.

See COPYING file for licenses.
