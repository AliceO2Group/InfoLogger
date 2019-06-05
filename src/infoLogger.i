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


#ifdef SWIG_JAVASCRIPT_V8
%inline %{
  #include <node.h>
%}
#endif


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

    int logInfo(const std::string &message);
    int logError(const std::string &message);
    int logWarning(const std::string &message);
    int logFatal(const std::string &message);
    int logDebug(const std::string &message);

    int log(const std::string &message);
    int log(const InfoLoggerMetadata &metadata, const std::string &message);

    int logS(const std::string &message);
    int logM(const InfoLoggerMetadata &metadata, const std::string &message);

    int setDefaultMetadata(const InfoLoggerMetadata &);
    int unsetDefaultMetadata();
};
