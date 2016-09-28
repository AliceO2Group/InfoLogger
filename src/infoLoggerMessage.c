#include "infoLoggerMessage.h"

/* This file statically defines the existing infoLogger protocols */

infoLog_msgProtocol_t protocols[]={
  {
      "1.4",   /* protocol 1.4, 08/2016 */
      16,
      {
        { ILOG_TYPE_STRING, "severity", ""},
        { ILOG_TYPE_INT,    "level", "" },
        { ILOG_TYPE_DOUBLE, "timestamp", ""},
        { ILOG_TYPE_STRING, "hostname", ""},
        { ILOG_TYPE_STRING, "rolename", ""},
        { ILOG_TYPE_INT,    "pid", ""},
        { ILOG_TYPE_STRING, "username", ""},
        { ILOG_TYPE_STRING, "system", ""},
        { ILOG_TYPE_STRING, "facility", ""},
        { ILOG_TYPE_STRING, "detector", ""},
        { ILOG_TYPE_STRING, "partition", ""},
        { ILOG_TYPE_INT,    "run", ""},
        { ILOG_TYPE_INT,    "errcode", ""},
        { ILOG_TYPE_INT,    "errline", ""},
        { ILOG_TYPE_STRING, "errsource", ""},
        { ILOG_TYPE_STRING, "message", ""},
        { ILOG_TYPE_NULL}
      },
      {-1}
  },
  {
      "1.3",   /* protocol 1.3, 08/2010 */
      17,
      {
        { ILOG_TYPE_STRING, "severity", ""},
        { ILOG_TYPE_INT,    "level", "" },
        { ILOG_TYPE_DOUBLE, "timestamp", ""},
        { ILOG_TYPE_STRING, "hostname", ""},
        { ILOG_TYPE_STRING, "rolename", ""},
        { ILOG_TYPE_INT,    "pid", ""},
        { ILOG_TYPE_STRING, "username", ""},
        { ILOG_TYPE_STRING, "system", ""},
        { ILOG_TYPE_STRING, "facility", ""},
        { ILOG_TYPE_STRING, "detector", ""},
        { ILOG_TYPE_STRING, "partition", ""},
        { ILOG_TYPE_STRING, "dest", ""},
        { ILOG_TYPE_INT,    "run", ""},
        { ILOG_TYPE_INT,    "errcode", ""},
        { ILOG_TYPE_INT,    "errline", ""},
        { ILOG_TYPE_STRING, "errsource", ""},
        { ILOG_TYPE_STRING, "message", ""},
        { ILOG_TYPE_NULL}
      },
      {-1}
  },
  {
      "1.2",   /* protocol 1.2, 04/2005 */
      11,
      {
        { ILOG_TYPE_STRING, "severity", ""},
        { ILOG_TYPE_DOUBLE, "timestamp", ""},
        { ILOG_TYPE_STRING, "hostname", ""},
        { ILOG_TYPE_STRING, "rolename", ""},
        { ILOG_TYPE_INT,    "pid", ""},
        { ILOG_TYPE_STRING, "username", ""},
        { ILOG_TYPE_STRING, "system", ""},
        { ILOG_TYPE_STRING, "facility", ""},
        { ILOG_TYPE_STRING, "dest", ""},
        { ILOG_TYPE_INT,    "run", ""},
        { ILOG_TYPE_STRING, "message", ""},
        { ILOG_TYPE_NULL}
      },
      {-1}
  }
};


/********************/
/* helper functions */
/********************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* print a decoded message */
int infoLog_msg_print(infoLog_msg_t *m) {
  if (m==NULL) return -1;
  if (m->protocol==NULL) return -1;
  
  int i;
  FILE *fd;
  fd=stdout;
  fprintf(fd,"protocol=%s\t",m->protocol->version);
  for (i=0;i<m->protocol->numberOfFields;i++) {
    fprintf(fd,"%s = ",m->protocol->fields[i].name);
    if (m->values[i].isUndefined) {
      fprintf(fd,"? (undefined)");
    } else {
      switch(m->protocol->fields[i].type) {
        case ILOG_TYPE_STRING:
          fprintf(fd,"%s",m->values[i].value.vString);
          break;
        case ILOG_TYPE_INT:
          fprintf(fd,"%d",m->values[i].value.vInt);
          break;
        case ILOG_TYPE_DOUBLE:
          fprintf(fd,"%lf",m->values[i].value.vDouble);
          break;
        default:
          fprintf(fd,"? (decoding not implemented for type %d)",m->protocol->fields[i].type);
          break;
      }
    }
    if (i==m->protocol->numberOfFields-1) {
      fprintf(fd,"\n");
    } else {
      fprintf(fd,"\t");
    }
  }
  fflush(fd);
    
  return 0;
}






#include <stdarg.h>
int mysprintf_init(mysprintf_t *s, char *buffer, int size) {
  if (s==NULL) {return __LINE__;}
  s->isError=1;
  if (size<=0) {return __LINE__;}
  if (buffer==NULL) {
    s->isDynamic=1;
    s->buffer=(char *)malloc(sizeof(char)*size);
    if (s->buffer==NULL) {return __LINE__;}
  } else {
    s->isDynamic=0;
    s->buffer=buffer;
  }
  s->bufferSize=size;
  s->strLength=0;
  s->buffer[0]=0;
  s->isError=0;
  return 0;
}
int mysprintf_destroy(mysprintf_t *s) {
  if (s==NULL) {return __LINE__;}
  if (s->isDynamic) {
    if (s->buffer!=NULL) {
      free(s->buffer);
    }
  }
  return 0;
}
/* TODO: what about dynamically increase buffer size if too small? */
int mysprintf(mysprintf_t *s, char *format, ...) __attribute__ ((format (printf, 2,3)));  /* ask compiler to check for printf-like format */
int mysprintf(mysprintf_t *s, char *format, ...){
  if (s==NULL) {return __LINE__;}
  if (format==NULL) {return __LINE__;}
  if (s->isError) {return -1;}
  int nsp=0;
  va_list ap;
  va_start(ap, format);
  nsp=vsnprintf(&s->buffer[s->strLength], s->bufferSize-s->strLength, format, ap);
  va_end(ap);
  s->buffer[s->bufferSize-1]=0; /* ensures buffer always NUL terminated */
  if (nsp<0) {
    s->isError=1;
    return -1;
  }
  if (nsp+s->strLength>=s->bufferSize) {
    s->isError=1;
    s->strLength=s->bufferSize-1;
    return -1;  
  }
  s->strLength+=nsp;
  return 0;
}


int infoLog_msg_encode(infoLog_msg_t *msg, char *buffer, int bufferSize, int splitLinesForThisFieldIndex) {

  const char *sol=NULL; // start of line
  char *eol=NULL; // end of line

  if (msg==NULL) {return __LINE__;}
  if (msg->protocol==NULL) {return __LINE__;}
  if (buffer==NULL) {return __LINE__;}
  if (bufferSize<=0) {return __LINE__;}
  if (splitLinesForThisFieldIndex<-1) {return __LINE__;}
  if (splitLinesForThisFieldIndex>=0) {
    if (splitLinesForThisFieldIndex>=msg->protocol->numberOfFields) {return __LINE__;}
    if (msg->protocol->fields[splitLinesForThisFieldIndex].type!=ILOG_TYPE_STRING) {return __LINE__;}
    if (!msg->values[splitLinesForThisFieldIndex].isUndefined) {
      sol=msg->values[splitLinesForThisFieldIndex].value.vString;
    }
  }
  
  mysprintf_t s;
  mysprintf_init(&s,buffer,bufferSize);

  int i,ix=-1,ix_lastGoodRecord=-1;
  const char *str;
  char *ptr;
  int isError=0;
  int truncated=0;
  for (;;) {
    if (sol!=NULL) {
      eol=strchr(sol,'\n');
    }

    mysprintf(&s,"*%s",msg->protocol->version);

    for (i=0;i<msg->protocol->numberOfFields;i++) {
      mysprintf(&s,"#");
      ix=-1;
      if (!msg->values[i].isUndefined) {
        switch(msg->protocol->fields[i].type) {
          case ILOG_TYPE_STRING:

            ix=s.strLength;        /* keep index of end of current record = beginning of string being added */          
            if (i==splitLinesForThisFieldIndex) {
              str=sol;
            } else {
              str=msg->values[i].value.vString;
            }
            /* append string */
            if (str==NULL) break;
            mysprintf(&s,"%s",str);
            
            /* truncate output buffer to end of current line */
            if ((i==splitLinesForThisFieldIndex)&&(eol!=NULL)) {
              int newl;
              newl=ix+(eol-sol);
              if ((newl>=0)&&(newl<s.bufferSize)) {
                  s.strLength=newl;
                  s.buffer[newl]=0;
                  s.isError=0;
                }
              }

            /* replace special characters - we can do it only in output buffer to avoid modifying the source (which can be static) */
            for (ptr=&s.buffer[ix];*ptr!=0;ptr++) {
              if ((*ptr=='*')||(*ptr=='#')||(*ptr=='\n')) {*ptr='?';}
              /* TODO: escape chars: replace ? by \* \# \n \\ */
            }
              
            break;
          case ILOG_TYPE_INT:
            mysprintf(&s,"%d",msg->values[i].value.vInt);
            break;
          case ILOG_TYPE_DOUBLE:
            mysprintf(&s,"%lf",msg->values[i].value.vDouble);
            break;
          default:
            mysprintf(&s,"?");
            isError=__LINE__;
            break;
        }
      }
      if (s.isError) break;
    }
    /* if there was a buffer problem for the last field (usually, it means message too long), try to truncate it
       ix is the index of the beginning of the too-long string
    */
        
    mysprintf(&s,"\n");

    char truncateMsg[]=" [...]\n";  /* this string should end like a proper end of record, c.f. last mysprintf above */
    
    if ( (s.isError)  /* could not complete record */
      || ((eol!=NULL)&&((int)(s.strLength+sizeof(truncateMsg))>bufferSize)) /* or will not be able to complete next one if any */
      ) {
      int ix_truncateMsg=-1;
      if ((ix>=0)&&((i+1)>=msg->protocol->numberOfFields)) {
        /* truncation occurred in last field */
        ix_truncateMsg=bufferSize-sizeof(truncateMsg);
        if (ix_truncateMsg<ix) { ix_truncateMsg=-1; }  /* we don't have even space for truncate warning */
      }
      if ((ix_truncateMsg<0)&&(ix_lastGoodRecord>=0)) {
        /* truncation occurred anywhere but we had some previous good full record */
        ix_truncateMsg=ix_lastGoodRecord-1;
      }
      if ((ix_truncateMsg<0)||((int)(ix_truncateMsg+sizeof(truncateMsg))>bufferSize)) {isError=__LINE__;}
      else {
        s.isError=0;
        s.strLength=ix_truncateMsg;
        mysprintf(&s,"%s",truncateMsg);
        truncated=1;
      }
    }

    if (s.isError) {isError=__LINE__;}
    if (isError) {break;}
    if (truncated) {break;}
    if (eol==NULL) {break;} /* we have processed all lines */
    sol=eol+1; /* move to next line */
    ix_lastGoodRecord=s.strLength;
  }
  mysprintf_destroy(&s);

  if (truncated) {return -1;}

  return isError;
}



/* conversion statics */
static int infolog_msg_isInit=0;

/* This function validates the statically defined protocols
   and initialize conversion functions
*/
int infoLog_proto_init() {
  unsigned int i;
  int j;

  // check protocol definitions
  for (i=0;i<sizeof(protocols)/sizeof(infoLog_msgProtocol_t);i++) {
    if (protocols[i].version==NULL) {return __LINE__;}
    if (protocols[i].numberOfFields<=0) {return __LINE__;}
    if (protocols[i].numberOfFields>INFOLOG_FIELDS_MAX) {return __LINE__;}
    for (j=0;j<protocols[i].numberOfFields;j++) {
      if (protocols[i].fields[j].name==NULL) {return __LINE__;}
      if (   (protocols[i].fields[j].type!=ILOG_TYPE_STRING)
          && (protocols[i].fields[j].type!=ILOG_TYPE_INT)
          && (protocols[i].fields[j].type!=ILOG_TYPE_DOUBLE)
          ) {return __LINE__;}
    }
    // check last one is of type 'STRING' = 'message'    
    j=protocols[i].numberOfFields-1;
    if (protocols[i].fields[j].type!=ILOG_TYPE_STRING) {return __LINE__;}
    if (strcmp(protocols[i].fields[j].name,"message")) {return __LINE__;}

    // init conversion table to first protocol
    int nmatch=0;
    for (j=0;j<protocols[i].numberOfFields;j++) {
      // look for field with same name/type in reference protocol
      int k,found=0;
      for (k=0;k<protocols[0].numberOfFields;k++) {
        if (protocols[0].fields[k].type!=protocols[i].fields[j].type) continue;
        if (strcmp(protocols[0].fields[k].name,protocols[i].fields[j].name)) continue;
        found=1;
        break;
      }
      // if found, keep index of corresponding field, otherwise drop field
      if (found) {
        protocols[i].convertIndex[j]=k;
        nmatch++;
      } else {
        protocols[i].convertIndex[j]=-1;
      }
    }
    // printf("Protocol %s : %d / %d fields can be converted\n",protocols[i].version,nmatch,protocols[i].numberOfFields);
  }
  
  infolog_msg_isInit=1;
  return 0;
}

int infoLog_msg_convert(infoLog_msg_t *msg) {
  int i;
  
  if (msg==NULL) {return __LINE__;}
  if (msg->protocol==&protocols[0]) return 0;  /* Nothing to do if no conversion needed */
  if (!infolog_msg_isInit) {return __LINE__;}
  if (msg->protocol==NULL) {return __LINE__;}

  infoLog_msgField_value_t newvalues[INFOLOG_FIELDS_MAX];
  memset(newvalues,0,sizeof(newvalues));
  for (i=0;i<INFOLOG_FIELDS_MAX;i++) {
    newvalues[i].isUndefined=1;
  }
  for (i=0;i<msg->protocol->numberOfFields;i++) {
    if (msg->protocol->convertIndex[i]!=-1) {
      newvalues[msg->protocol->convertIndex[i]]=msg->values[i];
    }
  }
  memcpy(msg->values,newvalues,sizeof(newvalues));
  msg->protocol=&protocols[0];
  return 0;
}

#include "utility.h"

int infoLog_msg_init(infoLog_msg_t *m){
  if (m==NULL) {return -1;}
  memset(m,0,sizeof(infoLog_msg_t));
  m->data=NULL;
  m->next=NULL;
  m->protocol=NULL;
  int i;
  for (i=0;i<INFOLOG_FIELDS_MAX;i++) {
    m->values[i].isUndefined=1;
    m->values[i].length=0;
  }
  return 0;
}

/* create an empty message structure - initializes partially the structure (just enough to be destroyed) */
/* uses checked_malloc() */
infoLog_msg_t *infoLog_msg_create(void){
  infoLog_msg_t *m;
  
  m=checked_malloc(sizeof(infoLog_msg_t));
  infoLog_msg_init(m);

  return m;
}

/* free memory allocated to a infoLog_msg_t */
/* uses checked_free() */
void infoLog_msg_destroy(infoLog_msg_t *m){
  infoLog_msg_t     *new,*old;

  for(new=m;new!=NULL;new=old){
    old=new->next;
    checked_free(new->data);
    checked_free(new);
  }
  
}

int infoLog_msg_findField(const char *fieldName) {
  int i;
  if (fieldName==NULL) return -1;
  for (i=0;i<protocols[0].numberOfFields;i++) {
    if (protocols[0].fields[i].name==NULL) {continue;}
    if (!strcmp(protocols[0].fields[i].name,fieldName)) {return i;}
  }
  return -1;
}
