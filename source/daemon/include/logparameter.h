#ifndef LOGPARAMETER_H
#define LOGPARAMETER_H
#include <QString>

struct LogParameter {
public:
    enum { LOG_NEVER = 0,
        LOG_ONCE,
        LOG_EVERY,
        LOG_LATEST,
        LOG_AVERAGE,
        LOG_ON_CHANGE };

    LogParameter() = default;
    LogParameter(const QString& a_name, const QString& a_value, int a_logType = LOG_EVERY, bool updatedRecently = false)
        : fName(a_name)
        , fValue(a_value)
        , updated(updatedRecently)
        , fLogType(a_logType)
    {
    }

    void setUpdatedRecently(bool updatedRecently) { updated = updatedRecently; }
    void setName(const QString& a_name) { fName = a_name; }
    void setValue(const QString& a_value) { fValue = a_value; }

    const QString& value() const { return fValue; }
    const QString& name() const { return fName; }
    bool updatedRecently() const { return updated; }
    int logType() const { return fLogType; }

private:
    QString fName, fValue;
    bool updated;
    int fLogType = LOG_NEVER;
};

#endif // LOGPARAMETER_H
