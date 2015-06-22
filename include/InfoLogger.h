/// \file InfoLogger.h
/// \brief C interface for the InfoLogger logging interface.
///
/// \author Sylvain Chapeland, CERN


typedef void* InfoLoggerHandle; /// Handle to infoLogger connection


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
/// \return         0 on success, an error code otherwise.
int infoLoggerLog(InfoLoggerHandle handle, const char *message, ...) __attribute__((format(printf, 2, 3)));
