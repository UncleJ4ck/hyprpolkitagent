#include "QMLIntegration.hpp"

#include "core/Agent.hpp"
#include "core/PolkitListener.hpp"

#include <QStringList>
#include <QVariantMap>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusObjectPath>
#include <QFile>
#include <polkitqt1-authority.h>
#include <polkitqt1-actiondescription.h>

void CQMLIntegration::onExit() {
    g_pAgent->submitResultThreadSafe(result.toStdString());
}

void CQMLIntegration::setResult(QString str) {
    result = str;
    g_pAgent->submitResultThreadSafe(result.toStdString());
    if (str.startsWith("auth:")) {
        result.fill(QChar('\0'));
        str.fill(QChar('\0'));
    }
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

QString CQMLIntegration::getActionId() {
    return g_pAgent->listener.session.inProgress ? g_pAgent->listener.session.actionId : "";
}

QString CQMLIntegration::getVendor() {
    if (!g_pAgent->listener.session.inProgress)
        return "";
    return g_pAgent->listener.session.details.lookup("polkit.vendor");
}

QString CQMLIntegration::getVendorUrl() {
    if (!g_pAgent->listener.session.inProgress)
        return "";
    return g_pAgent->listener.session.details.lookup("polkit.vendor_url");
}

QString CQMLIntegration::getIconName() {
    if (!g_pAgent->listener.session.inProgress)
        return "";
    if (!g_pAgent->listener.session.iconName.isEmpty())
        return g_pAgent->listener.session.iconName;
    return g_pAgent->listener.session.details.lookup("polkit.icon_name");
}

QVariantList CQMLIntegration::getDetailList() {
    QVariantList out;
    if (!g_pAgent->listener.session.inProgress)
        return out;

    const auto& details = g_pAgent->listener.session.details;
    const QStringList commandKeys = {"command_line", "command-line", "command", "cmdline", "cmd", "program", "exec"};

    for (const auto& key : details.keys()) {
        if (key.startsWith("polkit."))
            continue;
        if (commandKeys.contains(key))
            continue;
        const QString value = details.lookup(key);
        if (value.isEmpty())
            continue;
        QVariantMap row;
        row["key"]   = key;
        row["value"] = value;
        out.append(row);
    }
    return out;
}

QVariantList CQMLIntegration::getIdentityList() {
    QVariantList out;
    if (!g_pAgent->listener.session.inProgress)
        return out;

    for (const auto& id : g_pAgent->listener.session.identities) {
        const QString s = id.toString();
        QVariantMap row;
        row["id"]   = s;
        row["name"] = s.startsWith("unix-user:") ? s.mid(10) : s;
        out.append(row);
    }
    return out;
}

void CQMLIntegration::selectUser(QString identityString) {
    g_pAgent->listener.selectUser(identityString);
}

QString CQMLIntegration::getInitialPrompt() {
    return "Password";
}

bool CQMLIntegration::getInitialPromptEcho() {
    return false;
}

QString CQMLIntegration::getKeepAuthorizationNotice() {
    if (!g_pAgent->listener.session.inProgress)
        return "";

    auto* authority = PolkitQt1::Authority::instance();
    if (!authority)
        return "";

    const PolkitQt1::ActionDescription::List actions = authority->enumerateActionsSync();
    const QString actionId = g_pAgent->listener.session.actionId;

    for (const auto& a : actions) {
        if (a.actionId() != actionId)
            continue;
        const auto active = a.implicitActive();
        if (active == PolkitQt1::ActionDescription::AuthenticationRequiredRetained || active == PolkitQt1::ActionDescription::AdministratorAuthenticationRequiredRetained)
            return "This authorization will be remembered for the rest of this session.";
        break;
    }
    return "";
}

QString CQMLIntegration::getAvatarPath() {
    if (!g_pAgent->listener.session.inProgress)
        return "";

    const QString idStr = g_pAgent->listener.session.selectedUser.toString();
    if (!idStr.startsWith("unix-user:"))
        return "";

    bool ok = false;
    const quint64 uid = idStr.mid(10).toULongLong(&ok);
    if (!ok)
        return "";

    QDBusInterface accounts("org.freedesktop.Accounts", "/org/freedesktop/Accounts", "org.freedesktop.Accounts", QDBusConnection::systemBus());
    if (!accounts.isValid())
        return "";

    QDBusReply<QDBusObjectPath> userPath = accounts.call("FindUserById", QVariant::fromValue<qlonglong>(static_cast<qlonglong>(uid)));
    if (!userPath.isValid() || userPath.value().path().isEmpty())
        return "";

    QDBusInterface user("org.freedesktop.Accounts", userPath.value().path(), "org.freedesktop.DBus.Properties", QDBusConnection::systemBus());
    if (!user.isValid())
        return "";

    QDBusReply<QVariant> icon = user.call("Get", "org.freedesktop.Accounts.User", "IconFile");
    if (!icon.isValid())
        return "";

    const QString path = icon.value().toString();
    if (path.isEmpty() || !QFile::exists(path))
        return "";

    return "file://" + path;
}

void CQMLIntegration::setError(QString str) {
    emit setErrorString(str);
}

void CQMLIntegration::setInfo(QString str) {
    emit setInfoString(str);
}

void CQMLIntegration::setPrompt(QString str, bool echo) {
    emit setPromptString(str, echo);
}

void CQMLIntegration::clearField() {
    emit clearPasswordField();
}

void CQMLIntegration::focus() {
    emit focusField();
}

void CQMLIntegration::setInputBlocked(bool blocked) {
    emit blockInput(blocked);
}
