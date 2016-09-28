#include <Common/SimpleLog.h>


// class to communicate with local infoLoggerD process


class ConfigInfoLoggerClient {
  public:
  ConfigInfoLoggerClient(const char*configPath=nullptr);
  ~ConfigInfoLoggerClient();

  void resetConfig(); // set default configuration parameters

  std::string *txSocketPath;       // name of socket used to receive log messages from clients
  int txSocketOutBufferSize;
};


class InfoLoggerClient {
  public:
  InfoLoggerClient();
  ~InfoLoggerClient();

  
  
  // sends (already encoded) message to infoLoggerD
  // returns 0 on success, an error code otherwise
  int send(const char *message, unsigned int messageSize);

  private:
  ConfigInfoLoggerClient cfg;


  int isInitialized;    // set to 1 when object initialized with success, 0 otherwise
  SimpleLog log;   // object for daemon logging, as defined in config
  int txSocket;  // socket to infoLoggerD. >=0 if set.

};
