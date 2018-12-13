/* 
  infoLogger SWIG interface wrapper  
  See doc/README.md for SWIG generated libraries usage.
*/

%module infoLogger

%{
  #define SWIG_FILE_WITH_INIT

  #include <InfoLogger/InfoLogger.hxx>
  using namespace AliceO2::InfoLogger;
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
class InfoLogger {
  public:
    InfoLogger();
    ~InfoLogger();
    
    void logInfo(const std::string &message);
    void logError(const std::string &message);
};
