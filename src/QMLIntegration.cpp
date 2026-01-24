#include "QMLIntegration.hpp"

#include "core/Agent.hpp"
#include "core/PolkitListener.hpp"

#include <QStringList>

void CQMLIntegration::onExit() {
    g_pAgent->submitResultThreadSafe(result.toStdString());
}

void CQMLIntegration::setResult(QString str) {
    result = str;
    g_pAgent->submitResultThreadSafe(result.toStdString());
}

QString CQMLIntegration::getMessage() {
    return g_pAgent->listener.session.inProgress ? g_pAgent->listener.session.message : "An application is requesting authentication.";
}

QString CQMLIntegration::getCommand() {
    if (!g_pAgent->listener.session.inProgress)
        return "";

    const auto& details = g_pAgent->listener.session.details;
    const QStringList keys = details.keys();

    const QStringList preferredKeys = {
        "command_line",
        "command-line",
        "command",
        "cmdline",
        "cmd",
        "program",
        "exec",
    };

    for (const auto& key : preferredKeys) {
        const QString value = details.lookup(key);
        if (!value.isEmpty())
            return value;
    }

    for (const auto& key : keys) {
        const QString lowered = key.toLower();
        if (lowered.contains("command") || lowered.contains("cmd") || lowered.contains("program") || lowered.contains("exec")) {
            const QString value = details.lookup(key);
            if (!value.isEmpty())
                return value;
        }
    }

    return "";
}

QString CQMLIntegration::getUser() {
    return g_pAgent->listener.session.inProgress ? g_pAgent->listener.session.selectedUser.toString() : "an unknown user";
}

void CQMLIntegration::setError(QString str) {
    emit setErrorString(str);
}

void CQMLIntegration::focus() {
    emit focusField();
}

void CQMLIntegration::setInputBlocked(bool blocked) {
    emit blockInput(blocked);
}
