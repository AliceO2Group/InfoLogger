#ifndef _INFOLOGGER_MESSAGE_H
#define _INFOLOGGER_MESSAGE_H

#ifdef __cplusplus
extern "C" {
#endif

/* structure to store a field value */
typedef struct {
  /* Value of the field, depending on the type */
  union {
    const char * vString; // NUL terminated.
    int    vInt;
    double vDouble;
  } value;
  
  int length;        /* value size. Defined for strings only (optional), 0 otherwise */
  char isUndefined;  /* set to 1 when field is undefined, 0 otherwise */
} infoLog_msgField_value_t;

/* structure to store a field definition */
typedef struct {  
  enum {
    ILOG_TYPE_NULL,     // to mark end of array
    ILOG_TYPE_STRING,   // char *, memory should be freed after use
    ILOG_TYPE_INT,      // int
    ILOG_TYPE_DOUBLE    // double
  } type;               /* type of the message field */      

  const char *name;           /* name of the message field (should match database column name) */
  const char *description;    /* a field defining the description */
  
} infoLog_msgField_def_t;


/* maximum number of fields for any protocol */
#define INFOLOG_FIELDS_MAX 18


/* structure to define a log message = list of fields */
typedef struct {
  char *version;                                          /* version of the protocol */
  int numberOfFields;                                     /* number of fields in the protocol */
  infoLog_msgField_def_t fields[INFOLOG_FIELDS_MAX];      /* definition of the fields, list terminated by one of type ILOG_TYPE_NULL */
  
  int convertIndex[INFOLOG_FIELDS_MAX];  /* Array to convert fields of this protocol to default protocol fields.
                                            Filled dynamically in infoLog_proto_init().
                                            For each field, if convertIndex -1, there is no matching field in defaul protocol.
                                            If convertIndex>=0, this is the index of the matching field in default protocol.
                                            Matching field means same name and type.
                                         */
} infoLog_msgProtocol_t;


typedef struct _infoLog_msgList_t{
  infoLog_msgField_value_t values[INFOLOG_FIELDS_MAX];

  void *  data;                     /* data containing all above data - the one to be freed */
  struct _infoLog_msgList_t *next;  /* next message */
  
  infoLog_msgProtocol_t *protocol;  /* protocol used to encode this message */
} infoLog_msg_t;


/* Array defining the various protocols.
   The first one in the array is the default protocol.
   The message (text) field should be the last defined field in order to allow 'multi-line' (replacement of \f in message text) decoding
*/
extern infoLog_msgProtocol_t protocols[];


int infoLog_msg_print(infoLog_msg_t *msg);

/* This function encodes a struct message to a string */
/*
  msg: struct to be encoded (according to protocol msg->protocol)
  buffer: a buffer where the result string is printed
  bufferSize: size of the buffer
  splitLinesForThisFieldIndex: if this parameter is not -1, multiple records (all concatenated in the single string buffer output) 
                               are created, one per line of this field (all other fields being equal).
  
  Special characters (*,#,\n) in any field are replaced by a ? in final entry.

  returns: 0 if success, -1 if buffer too small and last field was truncated, or a non-zero error code.
*/
int infoLog_msg_encode(infoLog_msg_t *msg, char *buffer, int bufferSize, int splitLinesForThisFieldIndex);


/* helper functions to easily write/append formatted string to a buffer */
typedef struct {
  int bufferSize; // size of the buffer
  char *buffer;   // content of the buffer (NUL terminated string)
  int strLength;  // string length
  int isError;    // an error occurred while appending string (typically, buffer too small). Following appends will not work.
  int isDynamic;  // if the buffer has been allocated dynamically, or provided by user
} mysprintf_t;

/* init a buffer of given size to be written to. Returns 0 on success, or an error code. */
/* if buffer is NULL, memory is allocated. otherwise buffer is used */
int mysprintf_init(mysprintf_t *s, char *buffer, int size);
/* release resources used by a buffer. Returns 0 on success, or an error code. */
int mysprintf_destroy(mysprintf_t *s);
/* append a string to a buffer. Returns 0 on success, or an error code. */
int mysprintf(mysprintf_t *s, char *format, ...) __attribute__ ((format (printf, 2,3))); /* printf-like format parameters */

/* Function to check and initialize communication protocol definitions.
   Should be called once for all.
*/
int infoLog_proto_init(); 

/* convert a message from a given protocol to the default one */
int infoLog_msg_convert(infoLog_msg_t *msg);


infoLog_msg_t *infoLog_msg_create(void);
void infoLog_msg_destroy(infoLog_msg_t *m);

/* Query the field index of a given infoLogger field name in the default protocol.
   It returns a positive index (i.e. the index to be used to access the field in infoLog_msg_t.values[] array)
   or -1 if field is not found.
*/   
int infoLog_msg_findField(const char *fieldName);


#ifdef __cplusplus
}
#endif


// _INFOLOGGER_MESSAGE_H
#endif
