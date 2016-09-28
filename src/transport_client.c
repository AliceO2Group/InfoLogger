/** Implementation of the transport client interface.
 *
 *  This interface should be used to transmit files to a central server.
 *
 * @file  transport_client.c
 * @see    transport_client.h  
 * @author  sylvain.chapeland@cern.ch
*/

#define _GNU_SOURCE

#include <stdio.h>

#include <sys/time.h>
#include <unistd.h>    
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <sys/poll.h>

#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <pthread.h>

#include "utility.h"
#include "simplelog.h"
#include "transport_client.h"
#include "transport_proxy.h"
#include "permanentFIFO.h"


/* enable logging of transport communications with environment variable TRANSPORT_DEBUG_FILE */
FILE *debug_fp=NULL;


#define COMMAND_NONE  0
#define COMMAND_STOP  1

#define STATE_NOT_CONNECTED 1
#define STATE_CONNECTED 2
#define STATE_OPEN_CLIENT 3
#define STATE_CLOSE_CLIENT 4

#define MAX_RETRY_TIME 300

/** Structure containing all client information */
struct _TR_client {
  char *  root_name;  /**< Root server name */
  int     root_port;  /**< Root server port */
  
  char *  proxy_name;  /**< Proxy server name */
  int     proxy_port;  /**< Proxy server port */

  char *  client_name;  /**< client name */
  int     client_id;    /**< client numeric identifier (assigned by server). -1 if undefined */
  int     proxy_state;  /**< proxy status (see defines TR_PROXY_...) */
  
  pthread_t thread;     /**< The thread handling the connection */
  
  int   command;  /**< Inter - thread command */
  int   state;    /**< Current client state */
  int   fd;       /**< Socket descriptor */
  
  struct FIFO  *  input_queue;  /**< The queue of files to transmit */
  pthread_mutex_t input_mutex;  /**< Mutex for the queue */

  struct FIFO  *  output_queue;  /**< The queue of files transmitted and acknowledged */
  pthread_mutex_t output_mutex;  /**< Mutex for the queue */
  
  TR_proxy_handle  proxy;                    /**< Handle to a proxy, if launched */  
  int              server_shutdown_request;  /**< Set to 1 when server has requested client to shut down, 0 otherwise */
  
  struct permFIFO  *input_queue_msg;  /**< Structure to store messages in a permanent way. */
};











/** Open connection */
/*  uses : client->root_name, client->root_port, client->proxy name, client->proxy_port */
/*  modifies : client->fd*/
int transport_TCP_connect(TR_client_handle client){
  struct hostent *    hp;
  in_addr_t           inaddr;
  int                 sock_fd;
  struct sockaddr_in  server_addr,client_addr;
  char *              server_name;
  int                 server_port;
    
  /* connect to server if no proxy defined yet */
  if (client->proxy_name==NULL) {
    server_name=client->root_name;
    server_port=client->root_port;
  } else {
    server_name=client->proxy_name;
    server_port=client->proxy_port;
  }

  
  /* create new socket */
  if ( (sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    slog(SLOG_FATAL,"Can't create TCP socket : %s",strerror(errno));
    return -1;
  }

  /* Bind any local address */
  bzero((char *) &client_addr, sizeof(client_addr));
  client_addr.sin_family = AF_INET;
  client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  client_addr.sin_port = htons(0);
  if ( bind(sock_fd, (struct sockaddr*) &client_addr, sizeof(client_addr)) <0) {
    slog(SLOG_ERROR,"bind error : %s",strerror(errno));
    close(sock_fd);
    return -1;
  }

  
  /* create appropriate structures */
  bzero((char *) &server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(server_port);
  
  if ( (inaddr = inet_addr(server_name)) != -1 ) {
    bcopy((char *) &inaddr, (char *) &server_addr.sin_addr, sizeof(inaddr));
  } else {
    hp = gethostbyname(server_name);
    if (hp == NULL) {
      slog(SLOG_ERROR,"Transport : host name %s : %s",server_name,hstrerror(h_errno));
      close(sock_fd);
      return -1;
    }
    bcopy(hp->h_addr, (char *) &server_addr.sin_addr, hp->h_length);
  }

  /* connect */
  if (connect(sock_fd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
    slog(SLOG_ERROR,"Transport to %s:%d (TCP) : connect error : %s",server_name,server_port,strerror(errno));
    close(sock_fd);
    return -1;
  } else {
    slog(SLOG_INFO,"TCP connection established with %s:%d",server_name,server_port);
  }
  
  /* Here authenticate and connect to proxy if needed */

  client->fd=sock_fd;
  return 0;
}


/** Close connection */
/* modifies client->fd*/
int transport_TCP_disconnect(TR_client_handle client){

  if (client->fd>0) {
    slog(SLOG_DEBUG,"Socket closed");
    close(client->fd);
  }
  
  client->fd=-1;
  return 0;
}




struct TR_buffer {
  char *value;
  int start;
  int stop;
};



/* try to flush buffer to socket - timeout in milliseconds */
int TR_buffer_send(int fd, struct TR_buffer* buf,int timeout){
  struct pollfd ufsd;
  int bytes_sent;

  ufsd.fd=fd;
  ufsd.events=POLLOUT;
  ufsd.revents=0;

  if (poll(&ufsd,1,timeout)<=0) return 0;  /* socket not currently writable - let's try again later */

  if (ufsd.revents & POLLERR) return -1;
  if (ufsd.revents & POLLHUP) return -1;
  if (ufsd.revents & POLLNVAL) return -1;

  
  if (buf->stop<=buf->start) return 0;
  bytes_sent=send(fd,&buf->value[buf->start],buf->stop-buf->start, MSG_DONTWAIT);
  
  /* log in debug file if configured */
  if ((debug_fp!=NULL)&&(bytes_sent>0)) {
    fwrite(&buf->value[buf->start],bytes_sent,1,debug_fp);
    fflush(debug_fp);
  }


  if (bytes_sent>0) {
    buf->start+=bytes_sent;
  } else {
    /* socket writable but no byte written is error */
    return -1;
  }

  
  return bytes_sent;  
}




#define DEFAULT_CONNECTION_WAIT_TIME 1
#define DEFAULT_RECONNECTION_WAIT_TIME 10
#define DEFAULT_CONNECTION_TIMEOUT 10
#define DEFAULT_MAX_PROXY_RETRY 5
#define TR_BUFFER_SIZE 2500

/* client state machine running in a separate thread */
void TR_client_state_machine(void *arg){
  TR_client_handle the_client;
  int wait_count,wait_time;
  int n_retry;

  TR_file *current_file;    /* the current file transmitted */
  TR_blob *current_blob;    /* the current blob of the file being transmitted */
  int current_file_index;    /* the current file index in transmit FIFO */

  /* variables used for the socket send non blocking buffer */
  char buf_val[TR_BUFFER_SIZE];
  struct TR_buffer send_buf;

  /* to receive the commands from server */
  struct lineBuffer *server_commands;
  char *srv_cmd;
    
  /* variables dealing with file transfert status */
  int file_transfert_init;
  int file_transfert_deinit;  

  
  FILE *fp;

  int min_id,maj_id;
  TR_file_id ack_id;  /* the last file acknowledged by the server */
  int result;
  
  TR_file *the_file;
  
  int ini_state;    /* counter for steps in connect init procedure */
  int ini_last_state;      

  char *cptr1;  /* counters to parse strings */
  char *cptr2;


  TR_proxy_configuration proxy_config;  /* proxy configuration */
  
  char *current_cx_name=NULL;
  int current_cx_port=0;

  char *debug_file_name;  /* the filename where to log transport communications */
  
  time_t  watchdog_timer=0;              /* limit maximum time for sending a file */
  int     watchdog_last_index=-1;        /* keep track of last buffer index to see if transmit stalled */
  int     watchdog_count_noprogress=-1;  /* count loop without transmit progress */
  
  /* init state machine */
  the_client=(TR_client_handle)arg;
  the_client->state=STATE_NOT_CONNECTED;
  the_client->fd=-1;
  
  wait_count=0;
  wait_time=DEFAULT_CONNECTION_WAIT_TIME;
  n_retry=0;

  server_commands=NULL;

  
  /* at the beginning we don't know which files may have been already transmitted */
  ack_id.minId=0;
  ack_id.majId=0;


  /* setup logging facility if needed */
  debug_fp=NULL;
  debug_file_name=getenv("TRANSPORT_DEBUG_FILE");
  if (debug_file_name!=NULL) {
    debug_fp=fopen(debug_file_name,"w");
    if (debug_fp!=NULL) {
      slog(SLOG_INFO,"Communication with server logged in %s",debug_file_name);
    } else {
      slog(SLOG_ERROR,"Communication with server - logging disabled : can't open %s",debug_file_name);
    }
  }


            
  for(;;) {

    /*
    fprintf(stderr,"the_client->state = %d\n",the_client->state);
    */
    
    switch(the_client->state) {

      case STATE_NOT_CONNECTED:

        /* flush incoming data after timeout */
        if (the_client->input_queue_msg!=NULL) {
          permFIFO_flush(the_client->input_queue_msg,15);
        }

        /* try to connect after timeout */
        if (wait_count==0) {  
          if (transport_TCP_connect(the_client)==-1) {
            wait_count=wait_time;
            wait_time=wait_time*2;
            
            /* do not exceed max timeout */
            if (wait_time>MAX_RETRY_TIME) {
              wait_time=MAX_RETRY_TIME;
            }


            /* if we are connecting a proxy, reconnect main server if too many failures */
            n_retry++;
            if ((n_retry>DEFAULT_MAX_PROXY_RETRY) && (the_client->proxy_name!=NULL)) {
              checked_free(the_client->proxy_name);
              the_client->proxy_name=NULL;
              n_retry=0;
              wait_count=0;
              wait_time=DEFAULT_CONNECTION_WAIT_TIME;
            }
          } else {
            the_client->state=STATE_OPEN_CLIENT;
            if (the_client->proxy_name==NULL) {
              current_cx_name=the_client->root_name;
              current_cx_port=the_client->root_port;
            } else {
              current_cx_name=the_client->proxy_name;
              current_cx_port=the_client->proxy_port;
            }
          }
        } else {
          wait_count--;
          sleep(1);
        }

        break;

        

      case STATE_OPEN_CLIENT:

        /* create buffer for control flow */
        server_commands=lineBuffer_new();

        /* bypass message exchange for now */
/*
        the_client->state=STATE_CONNECTED;
        break;
*/      

        /* if we are connecting to a proxy, skip the init steps */
        if (the_client->proxy_name==NULL) {
          ini_state=0;
        } else {
          ini_state=1;
        }
        ini_last_state=-1;
        
        /* loop active while in this state and no shutdown is requested */
        while ( (the_client->state==STATE_OPEN_CLIENT) && (the_client->command != COMMAND_STOP) ) {

          /* new init step? - if yes, reset timeout */
          if (ini_state != ini_last_state) {
            wait_count=DEFAULT_CONNECTION_TIMEOUT;
            ini_last_state=ini_state;
          }
          
          /* timeout -> exit */
          if (wait_count==0) {
            the_client->state=STATE_CLOSE_CLIENT;
            break;
          }

          switch (ini_state) {
            case 0:
              /* send init */
              snprintf(buf_val,TR_BUFFER_SIZE,"INI %s %d\n",the_client->client_name,the_client->proxy_state);
              send(the_client->fd,buf_val,strlen(buf_val), 0);
              ini_state=1;
              
              if (debug_fp!=NULL) {
                fprintf(debug_fp,"%s",buf_val);
                fflush(debug_fp);
              }

              
              break;
            
            case 1:
              break;
            default:
              break;          
          }
          
          /* read from server socket */
          lineBuffer_add(server_commands,the_client->fd,1000);

          /* parse replies */
          srv_cmd=NULL;
          for(;;){
            if (srv_cmd!=NULL) {
              free(srv_cmd);
              srv_cmd=NULL;
            }
            
            srv_cmd=lineBuffer_getLine(server_commands);
            if (srv_cmd==NULL) break;

            slog(SLOG_DEBUG,"Server : %s",srv_cmd);

            /* server ready : transmit can begin */
            if (!strcmp(srv_cmd,"READY")) {
              the_client->state=STATE_CONNECTED;
              break;
            }
            
            /* get node id */
            if (!strncmp(srv_cmd,"NODE_ID",7)) {
              if (sscanf(&srv_cmd[7]," %d",&(the_client->client_id))==1) {
                continue;
              }
            }
            
            /* launch proxy */
            if (!strncmp(srv_cmd,"BE_PROXY",8)) {
              if (the_client->proxy!=NULL) {
                slog(SLOG_WARNING,"Transport proxy already started");
              } else {
                proxy_config.server_name=the_client->root_name;
                proxy_config.server_port=the_client->root_port;
                
                cptr1=&srv_cmd[8];
                while(isspace((int)*cptr1)) {
                  cptr1++;
                }
                cptr2=cptr1;
                while(!isspace((int)*cptr2)) {
                  if (cptr2==0) {
                    break;
                  }
                  cptr2++;
                }

                if (sscanf(cptr2," %d",&(proxy_config.proxy_port))==1) {
                  *cptr2=0;
                  proxy_config.proxy_name=cptr1;

                  /* default values could also be used : 
                  proxy_config.proxy_name=the_client->client_name;
                  proxy_config.proxy_port=the_client->root_port;
                  */
                  
                  the_client->proxy=TR_proxy_start(&proxy_config);
                }
              }
              continue;
            }

            /* use proxy */
            if (!strncmp(srv_cmd,"USE_PROXY",9)) {
              cptr1=&srv_cmd[9];
              while(isspace((int)*cptr1)) {
                cptr1++;
              }
              cptr2=cptr1;
              while(!isspace((int)*cptr2)) {
                if (cptr2==0) {
                  break;
                }
                cptr2++;
              }

              if (sscanf(cptr2," %d",&(the_client->proxy_port))==1) {
                if (the_client->proxy_name!=NULL) {
                  checked_free(the_client->proxy_name);
                }
                *cptr2=0;
                the_client->proxy_name=checked_strdup(cptr1);
                slog(SLOG_INFO,"Now using proxy %s %d",the_client->proxy_name,the_client->proxy_port);

                /* disconnect server */
                the_client->state=STATE_CLOSE_CLIENT;
                break;
              }                      
              continue;
            }
            
            /* bad reply */
            slog(SLOG_ERROR,"Bad message from server : %s",srv_cmd);
            the_client->state=STATE_CLOSE_CLIENT;
          }
        
          if (srv_cmd!=NULL) {
            free(srv_cmd);
            srv_cmd=NULL;
          }
          
          wait_count--;
        }
        
        break;



      case STATE_CLOSE_CLIENT:
      
        slog(SLOG_INFO,"Connection to %s:%d closed",current_cx_name,current_cx_port);
        current_cx_name=NULL;
        current_cx_port=0;

        /* Destroy server requests */
        if (server_commands!=NULL) {
          lineBuffer_destroy(server_commands);
          server_commands=NULL;
        }
        
        transport_TCP_disconnect(the_client);
        the_client->state=STATE_NOT_CONNECTED;

        wait_count=DEFAULT_RECONNECTION_WAIT_TIME;
        wait_time=DEFAULT_CONNECTION_WAIT_TIME;
        n_retry=0;
        
        break;        



      case STATE_CONNECTED:

/* This part emulates a direct transmit/acknowledge

for(;;) {
          if (the_client->command == COMMAND_STOP) {      
            break;
          }

            pthread_mutex_lock(&the_client->output_mutex);
            result=FIFO_is_full(the_client->output_queue);
            pthread_mutex_unlock(&the_client->output_mutex);
            if (!result) {
              pthread_mutex_lock(&the_client->input_mutex);
              current_file=FIFO_read(the_client->input_queue);
              pthread_mutex_unlock(&the_client->input_mutex);              
              if (current_file==NULL) {
                usleep(100000);
                continue;
              }
              printf("%d %d\n",current_file->id.minId,current_file->id.majId);
              pthread_mutex_lock(&the_client->output_mutex);
              FIFO_write(the_client->output_queue,current_file);
              pthread_mutex_unlock(&the_client->output_mutex);              
            } else {
              usleep(100000);
            }

}
        transport_TCP_disconnect(the_client);
        the_client->state=STATE_NOT_CONNECTED;

        wait_count=0;
        wait_time=DEFAULT_CONNECTION_WAIT_TIME;      
        n_retry=0;

        break;
*/


        current_file=NULL;
        current_blob=NULL;
        current_file_index=0;
        
        send_buf.start=0;
        send_buf.stop=0;
        file_transfert_init=0;
        file_transfert_deinit=0;  
        
        fp=NULL;

        for (;;) {


          /* process server replies */
          if (lineBuffer_add(server_commands,the_client->fd,0)==-1) break;
          for(;;){
            srv_cmd=lineBuffer_getLine(server_commands);
            if (srv_cmd==NULL) break;
            
            result=0;  /* server reply not parsed yet */
            
            /* What does want the server? */

            /* acknowledgment ? */
            if (!strncmp(srv_cmd,"ACK ",4)) {

              /*
              slog(SLOG_INFO,"ACK : %s\n",srv_cmd);
              */
              
              /* read the file id acknowledged */
              if (sscanf(&srv_cmd[4]," %d %d",&min_id,&maj_id)==2) {
                result=1;  /* succeed to parse server reply */
                
                ack_id.minId=min_id;
                ack_id.majId=maj_id;
              }
            }
            
            /* Close client ? */
            if (!strncmp(srv_cmd,"CLOSE",4)) {
              the_client->server_shutdown_request=1;
              slog(SLOG_INFO,"Transport %s shut down request",the_client->client_name);
              result=1;
            }
            
            /* was the server reply successfully parsed ? */
            if (result==0) {
              slog(SLOG_ERROR,"Transport protocol error: received %s",srv_cmd);
            }
            
            free(srv_cmd);
          }
          
          /* Remove acknowledged files from transmit buffer */
          pthread_mutex_lock(&the_client->input_mutex);
          for(;;){
            the_file=FIFO_read_index(the_client->input_queue,0);

            if (the_file==NULL) break;
            if (the_file==current_file) break;

            if (TR_file_id_compare(the_file->id,ack_id)) {
              break;
            }

            /* this file is not being sent and has been acknowledged */              
            /* try to move it to output queue */
            
            /* special if in message mode */
            if (the_client->input_queue_msg!=NULL) {
              FIFO_read(the_client->input_queue);                              /* remove from input queue */
              permFIFO_ack(the_client->input_queue_msg,the_file->id.minId);    /* ACK input message queue */
              TR_file_dec_usage(the_file);                                     /* destroy file */
              current_file_index--;
              continue;
            }
            
            pthread_mutex_lock(&the_client->output_mutex);
            result=FIFO_is_full(the_client->output_queue);
            if (!result) {
              FIFO_read(the_client->input_queue);    /* remove from input queue */
              FIFO_write(the_client->output_queue,the_file);  /* move to output queue */
              current_file_index--;
            }
            pthread_mutex_unlock(&the_client->output_mutex);
            if (result) break;
          }
          pthread_mutex_unlock(&the_client->input_mutex);


          /* adjust current file index */
          if (current_file_index<0) {
            current_file_index=0;
          }


          /* where are we in transmission ? */
          if ((current_file==NULL)&&(send_buf.start==send_buf.stop)) {

            if (!file_transfert_deinit) {
              /* at this point last file transfert is completed */

              /* get the next file to transmit */
              pthread_mutex_lock(&the_client->input_mutex);
              current_file=FIFO_read_index(the_client->input_queue,current_file_index);

              /* if nothing, populate from message queue if any */
              if ((current_file==NULL)&&(the_client->input_queue_msg!=NULL)&&(!FIFO_is_full(the_client->input_queue))) {
                TR_file *f;
                TR_blob *b,*last;
                struct FIFO_item item;
                               
                f=NULL;
                last=NULL;
                while (!permFIFO_read(the_client->input_queue_msg,&item,0)) {
                  if (f==NULL) {
                    f  = TR_file_new();
                    f -> id.source = checked_strdup(the_client->client_name);
                    f -> id.majId = 1;
                  }
                  f -> id.minId = item.id;
                  b  = (TR_blob *)checked_malloc(sizeof(TR_blob));
                  b -> value = (void *)item.data;
                  b -> size = item.size;
                  b -> next = NULL;
                  if (last==NULL) {
                    f -> first = b;
                  } else {
                    last->next=b;
                  }
                  f -> size += b -> size;
                  last=b;
                  
                  /* currently the server does not accept concatenated blobs, so just send one */
                  break;
	        }

                if (f!=NULL) {
                slog(SLOG_DEBUG,"Inserting file (%d bytes)",f->size);
                // TR_file_dump(f);
                  if (FIFO_write(the_client->input_queue,f)) {
                    slog(SLOG_ERROR,"Writing to non-full FIFO failed");
                    TR_file_dec_usage(f);
                  }
                }
                current_file=f;
              }

              pthread_mutex_unlock(&the_client->input_mutex);

              if (current_file!=NULL) {
                current_file_index++;
                file_transfert_init=0;
                file_transfert_deinit=1;

                /* reset watchdog for each file transfert */
                watchdog_last_index=-1;
                watchdog_count_noprogress=0;
              }

            } else {
              /* at this point file data is transmitted - just the end of file is missing */

              file_transfert_deinit=0;
              send_buf.value="END\n";
              send_buf.start=0;
              send_buf.stop=4;                      
            }
          }


          if ((current_file==NULL)&&(send_buf.start==send_buf.stop)) {
            /* if nothing to transmit wait a bit - 100ms */
            /* but wake-up if incoming traffic */
            struct pollfd ufsd;
            ufsd.fd=the_client->fd;
            ufsd.events=POLLIN | POLLPRI;
            ufsd.revents=0;
                        
            if (poll(&ufsd,1,1000)<0) the_client->state = STATE_CLOSE_CLIENT;
            if (ufsd.revents & (POLLERR | POLLHUP)) the_client->state = STATE_CLOSE_CLIENT;
//          usleep(100000);
            
          } else {

            /* check watchdog */
            if (watchdog_last_index==send_buf.start) {
              watchdog_count_noprogress++;
              if (watchdog_count_noprogress==10) {
                slog(SLOG_INFO,"Watchdog - transfer not progressing, timer started");
                watchdog_timer=time(NULL);
              } else if ((watchdog_count_noprogress % 10) == 0) {
                if (time(NULL)-watchdog_timer>30) {
                  slog(SLOG_INFO,"Watchdog - timeout, reset connection");
                  break;
                }
              }
            } else {
              watchdog_last_index=send_buf.start;              
              if (watchdog_count_noprogress>=10) {
                slog(SLOG_INFO,"Watchdog - transfer now progressing, timer stopped");
              }
              watchdog_count_noprogress=0;
            }

            if (send_buf.start==send_buf.stop) {
              /* the last buffer has been fully sent - fill it with new data */

              if (file_transfert_init==0) {

                /* a new file is being transmitted - send first file header */
                if (current_file->id.source==NULL) {
                  current_file->id.source=checked_strdup("Unknown");
                }
                
                snprintf(buf_val,TR_BUFFER_SIZE,"File %s %d %d %d\n",current_file->id.source,current_file->id.minId,current_file->id.majId,current_file->size);
                send_buf.start=0;
                send_buf.stop=strlen(buf_val);
                send_buf.value=buf_val;

                /*
                   the file is a priori not on disk
                   see comment 10 lines below
                */
                fp=NULL;
                current_blob=NULL;
                                          
                if (current_file->path!=NULL) {
                  fp=fopen(current_file->path,"r");
                  send_buf.value=buf_val;
                } else {
                  current_blob=current_file->first;
                }

                file_transfert_init=1;

              } else {
                
                /* file data is being transmitted
                  take a new piece of it from memory or from disk
                */

                /* Do not do :
                   if (current_file->path!=NULL) {            
                   because file can be saved on disk by TR_cache in the mean time
                   so keep status (disk or memory) when we first tried to transmit it
                   (see 10 lines above).
                */
                if (fp!=NULL) {            
                  /* file is on disk */
                  send_buf.start=0;
                  send_buf.stop=fread(&buf_val,1,TR_BUFFER_SIZE,fp);
                  if ( send_buf.stop <=0) {
                    /* this file transfert is completed */
                    current_file=NULL;
                    fclose(fp);
                    fp=NULL;
                    send_buf.stop=0;
                  }

                } else {
                  /* file is a blob list in memory */
                  if (current_blob!=NULL) {
                    send_buf.start=0;
                    send_buf.stop=current_blob->size;
                    send_buf.value=current_blob->value;
                    current_blob=current_blob->next;
                  } else {
                    /* this file transfert is completed */
                    current_file=NULL;
                  }
                }
              }
            } else {
              /* socket pre-buffer is not empty yet - just wait */
            }
          }

          /* Try to flush part of the socket pre-buffer, timeout = 500 ms */
          if (TR_buffer_send(the_client->fd,&send_buf,500) < 0) {
            break;
          }


          /* check shutdown request */
          if (the_client->command == COMMAND_STOP) {      
            break;
          }
        }

        /* clean up file being sent */
        if (fp!=NULL) {
          fclose(fp);
        }

        /* files sent but not acknowledged will be re-sent on reconnection */

        the_client->state = STATE_CLOSE_CLIENT;
        break;


      default:
        /* state undefined ... do nothing but wait (avoid infinite loop) */
        /* this should never happen */
        sleep(1);
        return;
    }



    /* Shutdown request : disconnect first */
    if (the_client->command == COMMAND_STOP) {
      if (the_client->state == STATE_NOT_CONNECTED) {
        the_client->command = COMMAND_NONE;
        break;
      } else {
        the_client->state = STATE_CLOSE_CLIENT;
      }
    }
  }
  
  

  /* close debug_file if any */
  if (debug_fp!=NULL) {
    fclose(debug_fp);
    debug_fp=NULL;
  }

  
}







/* Start the client thread */
TR_client_handle TR_client_start(TR_client_configuration* config){

  TR_client_handle the_client;
  
  /* create a new client handle */
  the_client=(TR_client_handle)checked_malloc(sizeof(*the_client));

  /* create input queue for messages if configured */
  if (config->msg_queue_path!=NULL) {
    the_client->input_queue_msg=permFIFO_new(config->queue_length,config->msg_queue_path);
    if (the_client->input_queue_msg==NULL) {
      checked_free(the_client);
      return NULL;
    }
  } else {
    the_client->input_queue_msg=NULL;
  }
 
  /* get root server connection parameters */
  the_client->root_name=checked_strdup(config->server_name);
  the_client->root_port=config->server_port;
  
  /* init proxy parameters */
  the_client->proxy_name=NULL;
  the_client->proxy_port=0;
  
  /* keep in mind other config parameters */
  the_client->client_name=checked_strdup(config->client_name);
  the_client->client_id=-1;
  the_client->proxy_state=config->proxy_state;

  /* init queues */
  the_client->input_queue=FIFO_new(config->queue_length);
  pthread_mutex_init(&the_client->input_mutex,NULL);

  the_client->output_queue=FIFO_new(config->queue_length);
  pthread_mutex_init(&the_client->output_mutex,NULL);

  /* finalize client initialization */
  the_client->proxy=NULL;
  the_client->command=COMMAND_NONE;
  the_client->server_shutdown_request=0;
    
  /* launch the thread handling the connection */  
  pthread_create(&the_client->thread,NULL,(void *) &TR_client_state_machine,(void *) the_client);

  slog(SLOG_INFO,"Transport client thread started - connecting to %s:%d",config->server_name,config->server_port);
  
  return the_client;
}





/* stop the client thread */
int TR_client_stop(TR_client_handle the_client) {

  TR_file *current_file;
  int i;
  

  /* ask the communication thread to stop */
  the_client->command=COMMAND_STOP;

  /* wait other thread terminates */
  /* condition variables are not used because this function is likely to be
     called in a signal handler */
  i=0;
  while(the_client->command!=COMMAND_NONE) {
    i++;
    if (i>=2000) {
      slog(SLOG_WARNING,"Transport client : can not stop thread");
      break; /* stop after 20 seconds */
    }
    usleep(10000);  
  }

  /* wait thread to exit - may block, but guarantees resources are freed */
  pthread_join(the_client->thread,NULL);

  /* close proxy if still running */
  if (the_client->proxy!=NULL) {
    TR_proxy_stop(the_client->proxy);
  }

  checked_free(the_client->root_name);
  checked_free(the_client->proxy_name);
  checked_free(the_client->client_name);
  
  /* purge input queue */
  pthread_mutex_destroy(&the_client->input_mutex);
  for(;;) {
    current_file=FIFO_read(the_client->input_queue);
    if (current_file==NULL) break;
    TR_file_dec_usage(current_file);
  }
  FIFO_destroy(the_client->input_queue);


  /* purge output queue */
  pthread_mutex_destroy(&the_client->output_mutex);
  for(;;) {
    current_file=FIFO_read(the_client->output_queue);
    if (current_file==NULL) break;
    TR_file_dec_usage(current_file);
  }
  FIFO_destroy(the_client->output_queue);

  /* close input message queue if any */
  if (the_client->input_queue_msg!=NULL) {
    permFIFO_destroy(the_client->input_queue_msg);
  }
    
  checked_free(the_client);

  slog(SLOG_INFO,"Transport client thread stopped");
  
  return 1;
}





/** Add the file to client transmit queue.
  * @return : 0 on success, -1 if buffer full.
*/
int TR_client_queueAddFile(TR_client_handle the_client,TR_file* file){
  int ret_val;
  
  /* we keep a reference to the file, so be sure nobody destroys it */
  TR_file_inc_usage(file);
  
  pthread_mutex_lock(&the_client->input_mutex);
  ret_val=FIFO_write(the_client->input_queue,file);
  pthread_mutex_unlock(&the_client->input_mutex);

  /* Fix for memory leak due to FIFO being full */
  if(ret_val != -1){
    slog(SLOG_DEBUG,"Transport : Added %d:%d to queue",file->id.majId,file->id.minId);
  }
  else{
    TR_file_dec_usage(file);
  }

  return ret_val;
}



/** Get last file transmitted.
  * @return : NULL if no file transmition acknowledged lately, or the pointer to the acknowledge file.
  * This file must be freed when not use any more, with TR_file_dec_usage().
*/
TR_file* TR_client_getLastFileSent(TR_client_handle the_client){
  void* ret_val;
  
  pthread_mutex_lock(&the_client->output_mutex);
  ret_val=FIFO_read(the_client->output_queue);
  pthread_mutex_unlock(&the_client->output_mutex);
  
  return (TR_file*) ret_val;
}



/** Test connection status.
  * @return : 1 if connected, 0 if not connected, -1 if bad client handle.
*/
int TR_client_isConnected(TR_client_handle h){
  if (h==NULL) {
    return -1;
  }
  if (h->state==STATE_CONNECTED) {
    return 1;
  }
  return 0;
}



/** Add the file to client transmit queue.
  * @return : 0 on success, -1 if buffer full.
*/
int TR_client_queueIsFull(TR_client_handle the_client){
  int ret_val;
  
  pthread_mutex_lock(&the_client->input_mutex);
  ret_val=FIFO_is_full(the_client->input_queue);
  pthread_mutex_unlock(&the_client->input_mutex);
  
  return ret_val;
}



/** Get space left in queue.
  * @return : -1 if bad client handle, or the free space in the queue (number of files that could be appended).
*/
int TR_client_queueGetSpaceLeft(TR_client_handle the_client){
  int ret_val;
  
  pthread_mutex_lock(&the_client->input_mutex);
  ret_val=FIFO_get_space_left(the_client->input_queue);
  pthread_mutex_unlock(&the_client->input_mutex);
  
  return ret_val;
}



/** Proxy shutdown request.
  * @return : 1 if the server requested a shutdown, 0 otherwise.
*/
int TR_client_isShutdown(TR_client_handle h) {
  return h->server_shutdown_request;
}


/** Return files waiting to be sent in transport queue 
  * @return : next file in queue, null if empty
  * This file must be freed when not used any more, with TR_file_dec_usage().
*/
TR_file* TR_client_get_unacknowledged(TR_client_handle h){
  TR_file *f;

  pthread_mutex_lock(&h->input_mutex);
  f=(TR_file*)FIFO_read(h->input_queue);
  pthread_mutex_unlock(&h->input_mutex);

  return f;
}



/** Send a message (NULL terminated). Requires permfifo_path provided.
  * Messages are copied and buffered locally until reception acknowledged by server.
  * Returns immediately.
  * @return        : 0 on success (message will be sent), -1 if failure.
*/
int TR_client_send_msg(TR_client_handle h, char const *msg) {

  if (h->input_queue_msg!=NULL) {
    return permFIFO_write(h->input_queue_msg,msg,0);
  }
  
  return -1;
}
