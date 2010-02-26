// 2009-04-19 eat 0.7.5 - nbprotocol.c is now coupled with nb_peer.c 
//   The coupling of nbprotocol.c is very messy, unfortunate, and something we need to clean up
//   later.  Right now it is being done to rescue the copy command for sending files to a remote queue
//   which is needed by an application batch commands for better performance.

typedef struct NB_MOD_NBP_CLIENT{
  nbCELL specCell;          // specification string
  NB_Term *brainTerm;       // brain term for old routines
  struct BRAIN *brain;      // brain structure
  int option;               // 0 - store, 1 - store and forward, 2 - write directly
  struct LISTENER  *ear;    // old listener to support transition - used if queue and schedule specified
  nbIDENTITY self;          // identity to portray for self when connecting
  nbCELL     scheduleCell;  // schedule - (optional when used with queue)
  nbCELL     synapseCell;   // synapse - used to respond to schedule
  } nbClient;
