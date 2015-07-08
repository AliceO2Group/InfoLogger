/// \file InfoLogger.h
/// \brief C interface for the InfoLogger logging interface.
///
///  See inline documentation, and testInfoLogger.c for example usage.
///
/// \author Sylvain Chapeland, CERN

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Handle to infoLogger connection
typedef void *InfoLoggerHandle;


/// Open infoLogger connection.
/// Resulting handle should be called by furhter InfoLogger function calls.
/// \param handle   Variable where the resulting handle is stored, on success. Result by reference.
/// \return         0 on success, an error code otherwise.
int infoLoggerOpen(InfoLoggerHandle *handle);

/// Close infoLogger connection.
/// \param handle   Handle to InfoLogger, as created by a previous infoLoggerOpen() call.
/// \return         0 on success, an error code otherwise.
int infoLoggerClose(InfoLoggerHandle handle);

/// Log a message.
/// \param handle   Handle to InfoLogger, as created by a previous infoLoggerOpen() call.
/// \param message  NUL-terminated string message to push to the log system. It uses the same format as specified for printf(), and the function accepts additionnal formatting parameters.
/// \param ...      Extra optionnal parameters for formatting.
/// \return         0 on success, an error code otherwise.
int infoLoggerLog(InfoLoggerHandle handle, const char *message, ...) __attribute__((format(printf, 2, 3)));

/// Log a message, with a list of arguments of type va_list.
/// \param handle   Handle to InfoLogger, as created by a previous infoLoggerOpen() call.
/// \param message  NUL-terminated string message to push to the log system. It uses the same format as specified for printf(), and the function accepts additionnal formatting parameters.
/// \param ap       Variable list of arguments (c.f. vprintf)
/// \return         0 on success, an error code otherwise.
int infoLoggerLogV(InfoLoggerHandle handle, const char *message, va_list ap) __attribute__((format(printf, 2, 0)));

#ifdef __cplusplus
}
#endif

