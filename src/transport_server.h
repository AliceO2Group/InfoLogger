/** Declaration of the transport server interface.
 *
 *  This interface should be used to set up a server receiving files sent by
 *  processes using the client interface.
 *
 *  11/2003: added server support for UDP data
 *
 * @file	transport_server.h
 * @author	sylvain.chapeland@cern.ch
*/

/* Avoid multiple includes */
#ifndef edg_monitoring_transport_server_h
#define edg_monitoring_transport_server_h

#include "transport_files.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _TR_server* TR_server_handle;	/**< A handle to a server connexion */


#define TR_SERVER_UDP 1
#define TR_SERVER_TCP 2

/** Server configuration structure */
typedef struct {
	int server_port;	/**< Listening port for new clients */
	int max_clients;	/**< Maximum number of clients allowed */
	int queue_length;	/**< Maximum number of files buffered */
	int server_type;	/**< The server type, one of TR_SERVER_UDP or TR_SERVER_TCP */
} TR_server_configuration;



/** Start a server with a given configuration.
  * @param config : server configuration.
  * @return	: a handle to the server connexion.
*/
TR_server_handle TR_server_start(TR_server_configuration* config);



/** Stop the server
  * @param h : server handle.
*/
int TR_server_stop(TR_server_handle h);



/** Acknowledge the processing of a file.
  * IMPORTANT : files must be acknowledge in the same order they have been read from queue.
  *
  * @param h 		: handle to server.
  * @param file_id 	: the identifier of the file to acknowledge (by reference). It will be freed by the routine.
  * @return 		0 on success, -1 on error.
*/
int TR_server_ack_file(TR_server_handle h,TR_file_id *id);



/** Get a file from queue.
  * 
  * @param h		: handle to server.
  * @param timeout	: maximum time to wait before return. -1 forever, 0 immediate return
  * @return		a file (to be freed when not used any more), NULL if timeout
*/
TR_file *TR_server_get_file(TR_server_handle h, int timeout);



#ifdef __cplusplus
}
#endif



#endif
