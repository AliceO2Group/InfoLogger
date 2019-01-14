/* 
  infoLogger SWIG interface wrapper  
  See doc/README.md for SWIG generated libraries usage.
*/

%module infoLogger

%{
  #define SWIG_FILE_WITH_INIT

  #include <InfoLoggerScripting.hxx>
  using namespace AliceO2::InfoLogger::Scripting;
%}


%include <exception.i>
%exception {
  try {
    $action
  } catch (const std::string& e) {
    SWIG_exception(SWIG_RuntimeError, e.c_str());
  }
}

%include <std_string.i>


class InfoLoggerMetadata {
  public:
  InfoLoggerMetadata();
  InfoLoggerMetadata(const InfoLoggerMetadata &);
  ~InfoLoggerMetadata();
  
  int setField(const std::string &key, const std::string &value);
};


class InfoLogger {
  public:
    InfoLogger();
    ~InfoLogger();

    void logInfo(const std::string &message);
    void logError(const std::string &message);
    void logWarning(const std::string &message);
    void logFatal(const std::string &message);
    void logDebug(const std::string &message);

    void log(const std::string &message);
    void log(const InfoLoggerMetadata &metadata, const std::string &message);

    int setDefaultMetadata(const InfoLoggerMetadata &);
    int unsetDefaultMetadata();
};
