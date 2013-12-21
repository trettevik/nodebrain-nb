#include <nb/nb.h>
#include <openssl/rand.h>

long int nbRandom(void){
  long int randy;
  unsigned char byte[sizeof(long int)];
  int i;

  if(!RAND_bytes(byte,sizeof(long int))){
    fprintf(stderr,"OpenSSL random number generator not properly seeded - terminating");
    exit(NB_EXITCODE_FAIL);
    }
  randy=byte[0]%128;
  for(i=1;i<sizeof(long int);i++) randy=(randy<<8)|byte[i];
  return(randy);
  }

uint16_t nbRand16(void){
  uint16_t randy;
  unsigned char byte[2];

  if(!RAND_bytes(byte,2)){
    fprintf(stderr,"OpenSSL random number generator not properly seeded - terminating");
    exit(NB_EXITCODE_FAIL);
    }
  randy=(byte[0]<<8)|byte[1];
  return(randy);
  }

uint32_t nbRand32(void){
  uint32_t randy;
  unsigned char byte[4];

  if(!RAND_bytes(byte,4)){
    fprintf(stderr,"OpenSSL random number generator not properly seeded - terminating");
    exit(NB_EXITCODE_FAIL);
    }
  randy=(((byte[0]<<8)|byte[1])<<8|byte[2])<<8|byte[3];
  return(randy);
  }

