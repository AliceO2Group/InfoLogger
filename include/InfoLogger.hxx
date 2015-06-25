/// \file InfoLogger.hxx
/// \brief C++ interface for the InfoLogger logging interface.
///
///  See inline documentation, and testInfoLogger.cxx for example usage.
///
/// \author Sylvain Chapeland, CERN

#include <string>
#include <iostream>
#include <boost/lexical_cast.hpp>

class InfoLoggerPrivate;


/// This class instanciates an infoLogger connection.
class InfoLogger {

  public:

  /// Constructor
  InfoLogger();

  /// Destructor
  ~InfoLogger();

  /// Log a message.
  /// \param message  NUL-terminated string message to push to the log system. It uses the same format as specified for printf(), and the function accepts additionnal formatting parameters.
  /// \param ...      Extra optionnal parameters for formatting.
  /// \return         0 on success, an error code otherwise.
  int log(const char *message, ...) __attribute__((format(printf, 2, 3)));


  /// Control commands for infoLogger stream (accepted by << operator)
  enum StreamOps {
    endm    ///< tag to mark end of message, and flush current buffer
  };
  

  /// Specialized << version for std::string  
  InfoLogger & operator<<(const std::string message);

  /// Specialized << version for control commands
  /// In particular, to mark (and flush) the end of a message.
  InfoLogger & operator<<(const InfoLogger::StreamOps op);

 
  /// Log a message using the << operator, like for std::cout.
  /// All messages must be ended with the InfoLogger::StreamOps::endm tag. 
  template<typename T>
  InfoLogger & operator<<(const T &message) {
    *this << boost::lexical_cast<std::string>(message);
    return *this;
  }


  
  private:
    InfoLoggerPrivate *dPtr;
};

