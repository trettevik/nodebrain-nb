#ifndef _NB_TLS_H_
#define _NB_TLS_H_

#include <nb-tls.h>
#include <nb.h>

nbTLSX *nbTlsLoadContext(nbCELL context,nbCELL tlsContext,void *handle);
extern nbTLS *nbTlsLoadListener(nbCELL context,nbCELL tlsContext,char *defaultUri,void *handle);
extern int nbTlsConnectNonBlockingAndSchedule(nbCELL context,nbTLS *tls,void *handle,void (*handler)(nbCELL context,int sd,void *handle));

#endif
