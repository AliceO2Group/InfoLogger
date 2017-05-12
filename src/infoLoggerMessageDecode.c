#include "infoLoggerMessage.h"
#include "transport_server.h"
#include "simplelog.h"

#include "infoLoggerMessage.c"

/********************/
/* Message parsing  */
/********************/



/* Decode message from string:
   This function is destructive, (memory) file content is altered (blobs content removed) to avoid data copy.
*/
infoLog_msg_t * infoLog_decode(TR_file *f){

  char              *ptr,*start,*end;
  infoLog_msg_t     *newMsg,*old,*first;
  TR_blob           *b;
  int               is_error;

  first=NULL;
  old=NULL;
  is_error=0;

  /* buffer to keep copy of last message */
  #define MSG_BUFFER_SIZE 200
  char buffer[MSG_BUFFER_SIZE+1];
  int buffer_i=0;
  buffer[0]=0;
  
  
  /* parse all blobs */
  for (b=f->first;b!=NULL;b=b->next){

    /* keep a copy */
    buffer_i=b->size;
    if (buffer_i>=MSG_BUFFER_SIZE) {
    	buffer_i=MSG_BUFFER_SIZE;
    }
    memcpy(buffer,b->value,buffer_i);
    buffer[buffer_i]=0;

    /* create structure */
    newMsg=infoLog_msg_create();

    /* steal data from blob */
    newMsg->data=b->value;    
    end=((char *)b->value)+b->size;
    b->value=NULL;
    b->size=0;
    
    /* chain item to current list */
    if (first==NULL) {
      first=newMsg;
    } else {
      old->next=newMsg;
    }
    old=newMsg;
    
    /* parse data */
    ptr=(char *)newMsg->data;

//    slog(SLOG_INFO,"MSG=%s[end of MSG]",ptr);

    /* check initial marker */
    if (*ptr!='*') {is_error=__LINE__; break;}
    ptr++;
    if (ptr>=end) {is_error=__LINE__; break;}

    /* read version number */
    for(start=ptr;(*ptr!='#')&&(ptr<end);ptr++) {}
    if (ptr>=end) {is_error=__LINE__; break;}
    *ptr=0;
    ptr++;

    /* find good protocol */
    int protoIx;
    int protoN;
    protoN=sizeof(protocols)/sizeof(infoLog_msgProtocol_t);
    for (protoIx=0;protoIx<protoN;protoIx++) {
      if (!strcmp(start,protocols[protoIx].version)) {break;}
    }
    if (protoIx==protoN) {
      is_error=__LINE__; break;   // protocol not found
    }
    newMsg->protocol=&protocols[protoIx];
    
    /* decode protocol fields accordingly */
    /* should be like: *1.3#I#1099570259#pcald10#roleName#30287#slord#DAQ#testclient#defaultLog#12345#blablabla [NULL terminated] */   

    int fieldIx=0;
    for(fieldIx=0;protocols[protoIx].fields[fieldIx].type!=ILOG_TYPE_NULL;fieldIx++) {
      int length=0;
      /* if last field, we assume value until end of line, i.e. no halt on '#' */
      int lastField=0;
      if (protocols[protoIx].fields[fieldIx+1].type==ILOG_TYPE_NULL) {lastField=1;}

      if (lastField) {
        start=ptr;
        *end=0;
        ptr=end;
        length=strlen(start);
      } else {
        for(start=ptr;(*ptr!='#')&&(ptr<end);ptr++) {
          length++;
        }  // skip data until end of field / end of string
        if (ptr>=end) {is_error=__LINE__; break;}
        *ptr=0; // truncate string
        ptr++;  //
      }

      // by default, new value is undefined, length 0 (infolog_msg_create())
      int vali=0;
      double vald=0;
      switch(protocols[protoIx].fields[fieldIx].type) {
        case ILOG_TYPE_STRING:          
          if (length) {
            newMsg->values[fieldIx].value.vString=start;
            newMsg->values[fieldIx].length=length;
            newMsg->values[fieldIx].isUndefined=0;
          }
          break;
        case ILOG_TYPE_INT:
          if (*start) {
            if (sscanf(start,"%d",&vali)!=1) {
//              is_error=__LINE__;
            } else {
              if (vali>0) {  // filter out negative/zero values
                newMsg->values[fieldIx].value.vInt=vali;
                newMsg->values[fieldIx].isUndefined=0;
              }
            }
          }
          break;
        case ILOG_TYPE_DOUBLE:

          if (*start) {
            if (sscanf(start,"%lf",&vald)!=1) {
//              is_error=__LINE__;
            } else {
              if (vald>0) {  // filter out negative/zero values
                newMsg->values[fieldIx].value.vDouble=vald;
                newMsg->values[fieldIx].isUndefined=0;
              }
            }
          }
          break;

        default:
          is_error=__LINE__;
          break;
      }
      if (is_error) {break;}      
    }
    
    
//    infoLog_msg_print(new);

    /* convert message to default protocol */
    if (infoLog_msg_convert(newMsg)) {
      is_error=__LINE__;
      break;
    }

//    infoLog_msg_print(newMsg);
        
    /*
    // print decoded message
    if (!is_error) {
      infoLog_msg_print(newMsg, &protocols[protoIx]);
    }
    */
  }
  
  
  /* we have emptied file from blobs */
  f->size=0;

  if (!is_error) {
    return first;
  }

  /* in case of errors */
  slog(SLOG_ERROR,"Decoding failed (%d) for message: %s",is_error,buffer);
  
  /* free memory on error */
  infoLog_msg_destroy(first);  

  return NULL;
}
