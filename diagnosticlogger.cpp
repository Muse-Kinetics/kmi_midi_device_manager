#include "diagnosticlogger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QTextStream>

#include <cstdarg>
#include <cstdio>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace
{
QMutex g_logMutex;
QFile g_logFile;
QString g_logDirectoryPath;
QString g_currentLogFilePath;
bool g_initialized = false;
bool g_mirrorQtMessagesToConsole = false;

QString logLevelName(KmiLogLevel level)
{
    switch (level)
    {
    case KMI_LOG_LEVEL_DEBUG:
        return "DEBUG";
    case KMI_LOG_LEVEL_INFO:
        return "INFO";
    case KMI_LOG_LEVEL_WARN:
        return "WARN";
    case KMI_LOG_LEVEL_ERROR:
        return "ERROR";
    }

    return "LOG";
}

QString messageTypeName(QtMsgType type)
{
    switch (type)
    {
    case QtDebugMsg:
        return "DEBUG";
    case QtInfoMsg:
        return "INFO";
    case QtWarningMsg:
        return "WARN";
    case QtCriticalMsg:
        return "ERROR";
    case QtFatalMsg:
        return "FATAL";
    }

    return "LOG";
}

QString normalizeMessage(QString message)
{
    while (message.endsWith('\n') || message.endsWith('\r'))
    {
        message.chop(1);
    }

    return message;
}

QString launchFileName()
{
    QString appName = QCoreApplication::applicationName();
    if (appName.isEmpty())
    {
        appName = "SoftStep";
    }

    appName.replace(' ', '-');
    appName.replace('/', '-');
    appName.replace('\\', '-');

    return QString("%1-%2.log")
        .arg(appName)
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz"));
}

void writeLogLineLocked(const QString &line)
{
    if (!g_logFile.isOpen())
    {
        return;
    }

    QTextStream stream(&g_logFile);
    stream << line << Qt::endl;
    stream.flush();
    g_logFile.flush();
}

void emitLogLineLocked(const QString &level, const QString &message)
{
    const QString formatted = QString("[%1] [%2] %3")
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
        .arg(level)
        .arg(message);

    writeLogLineLocked(formatted);

#ifdef Q_OS_WIN
    OutputDebugStringW(reinterpret_cast<LPCWSTR>((formatted + "\r\n").utf16()));
#endif

    if (g_mirrorQtMessagesToConsole)
    {
        const QByteArray localMessage = formatted.toLocal8Bit();
        std::fprintf(stderr, "%s\n", localMessage.constData());
        std::fflush(stderr);
    }
}

void pruneOldLogs(const QDir &logDir)
{
    QFileInfoList logFiles = logDir.entryInfoList(QStringList() << "*.log", QDir::Files, QDir::Time);

    const int maxLaunchLogs = 5;
    const qint64 maxTotalBytes = 5 * 1024 * 1024;

    int keptLogs = 0;
    qint64 keptBytes = 0;

    for (const QFileInfo &logFileInfo : logFiles)
    {
        const qint64 fileSize = logFileInfo.size();
        const bool keepFile = (keptLogs == 0) || (keptLogs < maxLaunchLogs && (keptBytes + fileSize) <= maxTotalBytes);

        if (keepFile)
        {
            ++keptLogs;
            keptBytes += fileSize;
            continue;
        }

        QFile::remove(logFileInfo.absoluteFilePath());
    }
}

void diagnosticMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    QMutexLocker locker(&g_logMutex);

    QString formattedMessage = normalizeMessage(message);
    if (context.file != nullptr && context.line > 0)
    {
        formattedMessage.append(QString(" (%1:%2)").arg(context.file).arg(context.line));
    }

    emitLogLineLocked(messageTypeName(type), formattedMessage);

    if (type == QtFatalMsg)
    {
        std::abort();
    }
}
}

namespace DiagnosticLogger
{
void initialize()
{
    QMutexLocker locker(&g_logMutex);

    if (g_initialized)
    {
        return;
    }

    QString appDataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataDirectory.isEmpty())
    {
        return;
    }

    g_logDirectoryPath = appDataDirectory + "/logs";

    QDir logDir(g_logDirectoryPath);
    if (!logDir.exists())
    {
        QDir().mkpath(g_logDirectoryPath);
    }

    pruneOldLogs(logDir);

    g_currentLogFilePath = logDir.filePath(launchFileName());
    g_logFile.setFileName(g_currentLogFilePath);
    g_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);

#ifdef Q_OS_WIN
    g_mirrorQtMessagesToConsole = (GetConsoleWindow() != nullptr);
#else
    g_mirrorQtMessagesToConsole = true;
#endif

    writeLogLineLocked(QString("==== Launch start: %1 ====").arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs)));
    qInstallMessageHandler(diagnosticMessageHandler);

    pruneOldLogs(logDir);
    g_initialized = true;
}

QString logDirectoryPath()
{
    return g_logDirectoryPath;
}

QString currentLogFilePath()
{
    return g_currentLogFilePath;
}
}

extern "C" void kmiVLogf(KmiLogLevel level, const char *format, va_list args)
{
    char stackBuffer[4096];
    va_list argsCopy;
    int requiredLength;
    QString message;

    if (format == nullptr)
    {
        return;
    }

    va_copy(argsCopy, args);
    requiredLength = std::vsnprintf(stackBuffer, sizeof(stackBuffer), format, argsCopy);
    va_end(argsCopy);

    if (requiredLength < 0)
    {
        message = QString::fromLatin1(format);
    }
    else if (requiredLength < static_cast<int>(sizeof(stackBuffer)))
    {
        message = QString::fromLocal8Bit(stackBuffer, requiredLength);
    }
    else
    {
        QByteArray dynamicBuffer(requiredLength + 1, '\0');
        std::vsnprintf(dynamicBuffer.data(), dynamicBuffer.size(), format, args);
        message = QString::fromLocal8Bit(dynamicBuffer.constData(), requiredLength);
    }

    message = normalizeMessage(message);

    QMutexLocker locker(&g_logMutex);
    emitLogLineLocked(logLevelName(level), message);
}

extern "C" void kmiLogf(KmiLogLevel level, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    kmiVLogf(level, format, args);
    va_end(args);
}