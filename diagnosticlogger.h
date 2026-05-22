#ifndef DIAGNOSTICLOGGER_H
#define DIAGNOSTICLOGGER_H

#include <stdarg.h>

typedef enum
{
	KMI_LOG_LEVEL_DEBUG,
	KMI_LOG_LEVEL_INFO,
	KMI_LOG_LEVEL_WARN,
	KMI_LOG_LEVEL_ERROR
} KmiLogLevel;

#ifdef __cplusplus
extern "C" {
#endif

void kmiLogf(KmiLogLevel level, const char *format, ...);
void kmiVLogf(KmiLogLevel level, const char *format, va_list args);

#define LOG_DBG(...)  kmiLogf(KMI_LOG_LEVEL_DEBUG, __VA_ARGS__)
#define LOG_INFO(...) kmiLogf(KMI_LOG_LEVEL_INFO, __VA_ARGS__)
#define LOG_WARN(...) kmiLogf(KMI_LOG_LEVEL_WARN, __VA_ARGS__)
#define LOG_ERR(...)  kmiLogf(KMI_LOG_LEVEL_ERROR, __VA_ARGS__)

#ifdef __cplusplus
}

extern "C++" {

#include <QString>

namespace DiagnosticLogger
{
void initialize();
QString logDirectoryPath();
QString currentLogFilePath();
}

}
#endif

#endif // DIAGNOSTICLOGGER_H