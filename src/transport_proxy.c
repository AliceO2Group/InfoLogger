/** Implementation of a proxy for the transport.
 *
 * history:
 * 11/2003	Server configured for TCP
 *
 * @file	transport_proxy.c
 * @see		transport_proxy.h  
 * @author	sylvain.chapeland@cern.ch
*/

#define _GNU_SOURCE

#include "transport_proxy.h"
#include "transport_client.h"
#include "transport_server.h"

#include "simplelog.h"
#include "utility.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

/** Proxy data */
struct _TR_proxy {
	char *   	server_name;    	/**< the server ip */
	int       	server_port;    	/**< the server port */

	char *   	proxy_name;     	/**< proxy name */
	int     	proxy_port;     	/**< proxy port */

	pthread_t	thread;          	/**< the thread running the proxy main loop */
	int      	running;        	/**< 1 when running, 0 otherwise */
	int      	shutdown_request;	/**< 0 when running, 1 to shut down the thread */
};





/** Proxy main loop.
  *
  * It should be called as a separate thread. Returns only when 'shutdown request' in the handle has been set to one.
  *
  * @param arg : pointer to a proxy handle.  
  */
void * TR_proxy_main(void *arg){

	TR_file *f;
	TR_file_id *id;
	
	TR_server_configuration srv_config;
	TR_server_handle srv_h;
	
	TR_client_configuration cl_config;
	TR_client_handle cl_h;

	int want_delay;

	TR_proxy_handle the_proxy;


	/* get parameters */
	the_proxy=(TR_proxy_handle) arg;
	
	/* Configuration constants for client and server */	
	#define TR_PROXY_CLIENT_QUEUE	100	/**< length of transmit queue */
	#define TR_PROXY_SERVER_QUEUE	50	/**< lenth of reception queue (one for each client connected) */
	#define TR_PROXY_MAX_CLIENTS	100	/**< maximum number of clients connected to proxy */
	#define TR_PROXY_LOOP_DELAY	10	/**< delay (in milliseconds) to wait when nothing occurs */	



	/* Setup client */
	cl_config.server_name=the_proxy->server_name;
	cl_config.server_port=the_proxy->server_port;
	cl_config.queue_length=TR_PROXY_CLIENT_QUEUE;
	cl_config.client_name=the_proxy->proxy_name;
	cl_config.proxy_state=TR_PROXY_IS_PROXY;
	cl_config.msg_queue_path=NULL;
	
	cl_h=TR_client_start(&cl_config);
	if (cl_h==NULL) {
		slog(SLOG_ERROR,"Proxy : can not start client");
		return NULL;
	}

	
	
	/* Setup server */
	srv_config.server_type=TR_SERVER_TCP;
	srv_config.server_port=the_proxy->proxy_port;
	srv_config.max_clients=TR_PROXY_MAX_CLIENTS;
	srv_config.queue_length=TR_PROXY_SERVER_QUEUE;

	
	srv_h=TR_server_start(&srv_config);
	if (srv_h==NULL) {
		TR_client_stop(cl_h);
		slog(SLOG_ERROR,"Proxy : can not start server");
		return NULL;
	}


	/* finalize init */
	the_proxy->running=1;

	
	/* Main loop */

	for(;;) {
		want_delay=0;	/* This variable is incremented by each part of
				   the control code if it wishes some delay
				   at the end of the loop. */



		/* PART ONE : file transmission */

		/* Is there some free space in transmit buffer? */
		if (TR_client_queueIsFull(cl_h)) {
				want_delay++;
		} else {
			/* Get next file received */
			f=TR_server_get_file(srv_h,0);

			/* Add it to transmit queue */
			if (f!=NULL) {
				TR_client_queueAddFile(cl_h,f);
			} else {
				want_delay++;
			}
		}
		


		/* PART TWO : file acknowledgment */
		
		/* Get last file acknowledged */
		f=TR_client_getLastFileSent(cl_h);
		if (f!=NULL) {
			id=(TR_file_id *)checked_malloc(sizeof(TR_file_id));
			memcpy(id,&f->id,sizeof(TR_file_id));
			TR_server_ack_file(srv_h,id);
			TR_file_destroy(f);
		} else {
			want_delay++;
		}

		

		/* Shutdown request ? */
		if (the_proxy->shutdown_request) {
			break;			
		}
		if (TR_client_isShutdown(cl_h)) {
			/* this occurs when remote server requests a shutdown */
			break;
		}

		/* Now see if all parts want a delay (because nothing to do) */
		if (want_delay==2) {
			usleep(TR_PROXY_LOOP_DELAY*1000);
		}
	}

	TR_client_stop(cl_h);
	TR_server_stop(srv_h);
	the_proxy->running=0;

	return NULL;
}




/** Start a proxy with a given configuration.
  * @param config 	: proxy configuration.
  * @return		: a handle to the proxy connexion.
*/
TR_proxy_handle TR_proxy_start(TR_proxy_configuration* config){

	#define TR_PROXY_START_TIMEOUT 2		/**< Timeout in seconds when waiting proxy starts */


	int i;
	TR_proxy_handle the_proxy;
	

	/* create a new proxy handle */
	the_proxy=(TR_proxy_handle)checked_malloc(sizeof(*the_proxy));
	
	/* copy parameters */
	the_proxy->server_name=checked_strdup(config->server_name);
	the_proxy->server_port=config->server_port;

	the_proxy->proxy_name=checked_malloc(strlen(config->proxy_name)+7);
	sprintf(the_proxy->proxy_name,"proxy@%s",config->proxy_name);	/* to identify a proxy on the server */
	the_proxy->proxy_port=config->proxy_port;


	/* finalize init of structure */
	the_proxy->shutdown_request=0;
	the_proxy->running=0;
	
	/* launch the thread handling the connection */	
	pthread_create(&the_proxy->thread,NULL,TR_proxy_main,(void *) the_proxy);

	for(i=0;i<TR_PROXY_START_TIMEOUT*100;i++){
		usleep(10000);
		if (the_proxy->running) {		
			break;
		}
	}

	if (the_proxy->running) {
		slog(SLOG_INFO,"Proxy %s started",the_proxy->proxy_name);
	} else {
		slog(SLOG_WARNING,"Proxy start up timeout");
	}
	

	return the_proxy;
}




/** Stop the proxy.
  * @param h	: proxy handle.
  * @return 	: 0 on success, -1 on error.
*/
int TR_proxy_stop(TR_proxy_handle h){

	#define TR_PROXY_STOP_TIMEOUT 10	/**< Timeout in seconds when waiting proxy stops */


	int i;


	/* valid handle? */
	if (h==NULL) return -1;

	/* stop proxy */
	if (h->running) {
		h->shutdown_request=1;
		for(i=0;i<TR_PROXY_STOP_TIMEOUT*100;i++){
			usleep(10000);
			if (!h->running) break;
		}
		if (h->running) {
			slog(SLOG_WARNING,"Proxy shut down timeout");
		} else {
			slog(SLOG_INFO,"Proxy stopped");
		}
	} else {
		slog(SLOG_WARNING,"Proxy already stopped");
	}

        /* wait thread to exit - may block, but guarantees resources are freed */
        pthread_join(h->thread,NULL);
	
	/* free resources */
	checked_free(h->server_name);
	checked_free(h->proxy_name);
	checked_free(h);
	
	return 0;	
}
