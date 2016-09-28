/** Declaration of the transport client interface.
 *
 *  This interface should be used to transmit files from a client to a server.
 *  "files" are a list of binary objects, containing any kind of data. This is
 *  the atomic entity to be transmitted.
 * 
 *  "files" to be transmitted are pushed in a FIFO. An acknowledgment is received
 *  when files have been processed by the server.
 *
 *  Typical use:
 *  create client config
 *  start client
 *  loop:
 *       create new file to transmit
 *       push file in FIFO
 *       loop:
 *            get last file transmitted
 *            destroy file  
 *       end
 *  end
 *  stop client   
 
 *
 * @file    transport_client.h
 * @see     transport_files.h
 * @author  sylvain.chapeland@cern.ch
*/


/* Avoid multiple includes */
#ifndef transport_client_h
#define transport_client_h

#include "transport_files.h"

#ifdef __cplusplus
extern "C" {
#endif


/** Handle to a client connexion */
typedef struct _TR_client* TR_client_handle;    


/* Proxy_state definitions
   a client can become a proxy to relay transmissions
   to the server from other clients
*/
#define TR_PROXY_CAN_BE_PROXY     1
#define TR_PROXY_CAN_NOT_BE_PROXY 2
#define TR_PROXY_IS_PROXY         3


/** Client configuration structure */
typedef struct {
  char const *  server_name;   /**< the server ip */
  int     server_port;   /**< the server port */
  int     queue_length;  /**< file transmission queue size (number of files) */

  char const *  client_name;   /**< client name */
  int     proxy_state;   /**< proxy status (see defines TR_PROXY_...) */
  
  char const *  msg_queue_path; /**< path to a permanent FIFO storage location
                               if using messages (NULL if not). */
} TR_client_configuration;



/** Start a client with a given configuration.
  * @param  config :  client configuration.
  * @return        :  a handle to the client connexion.
*/
TR_client_handle TR_client_start(TR_client_configuration* config);


/** Stop the client
  * @param   h     :  client handle.
  * @return        :  always 0.
*/
int TR_client_stop(TR_client_handle h);


/** Add a file to client transmit queue.
  * @param  h      : client handle.
  * @param  f      : file to be transmitted.
  * @return        : 0 on success, -1 if buffer full.
*/
int TR_client_queueAddFile(TR_client_handle h,TR_file* f);


/** Get last file transmitted.
  * @param  h      : client handle.
  * @return        : NULL if no file transmition acknowledged lately, or the pointer to the acknowledged file.
*/
TR_file* TR_client_getLastFileSent(TR_client_handle h);


/** Test connection status.
  * @param  h      : client handle.
  * @return        : 1 if connected, 0 if not connected, -1 if bad client handle.
*/
int TR_client_isConnected(TR_client_handle h);


/** Test if queue full.
  * @param  h      : client handle.
  * @return        : 1 if transmit queue full, 0 otherwise.
*/
int TR_client_queueIsFull(TR_client_handle h);


/** Get space left in queue.
  * @param  h      : client handle.
  * @return        : the free space in the queue (number of files that could be appended).
*/
int TR_client_queueGetSpaceLeft(TR_client_handle h);


/** Check if there is a shutdown request from server.
  * @param  h      : client handle.
  * @return        : 1 if the server requested a shutdown, 0 otherwise.
*/
int TR_client_isShutdown(TR_client_handle h);

/** Return files waiting to be sent in transport queue 
  * @return : next file in queue, null if empty
  * This file must be freed when not used any more, with TR_file_dec_usage().
*/
TR_file* TR_client_get_unacknowledged(TR_client_handle h);


/** Send a message (NULL terminated). Requires permfifo_path provided.
  * Messages are copied and buffered locally until reception acknowledged by server.
  * Returns immediately.
  * @return        : 0 on success (message will be sent), -1 if failure.
*/
int TR_client_send_msg(TR_client_handle h, char const *msg);


#ifdef __cplusplus
}
#endif


/* END #ifndef transport_client_h */
#endif
