#pragma once

#include <QObject>
#include <QQmlApplicationEngine>
#include <QPixmap>
#include <QIcon>
#include <QVariantList>

class CQMLIntegration : public QObject {
    Q_OBJECT;
    Q_PROPERTY(QString errorText MEMBER errorText);

  public:
    explicit CQMLIntegration(QObject* parent = nullptr) : QObject(parent) {
        ;
    }
    virtual ~CQMLIntegration() {
        ;
    }

    void                setError(QString str);
    void                setInfo(QString str);
    void                setPrompt(QString str, bool echo);
    void                clearField();
    void                focus();
    void                setInputBlocked(bool blocked);

    QString             result = "fail", errorText = "";

    Q_INVOKABLE QString getMessage();
    Q_INVOKABLE QString getCommand();
    Q_INVOKABLE QString getUser();
    Q_INVOKABLE QString getActionId();
    Q_INVOKABLE QString getVendor();
    Q_INVOKABLE QString getVendorUrl();
    Q_INVOKABLE QString getIconName();
    Q_INVOKABLE QVariantList getDetailList();
    Q_INVOKABLE QString getInitialPrompt();
    Q_INVOKABLE bool    getInitialPromptEcho();

    Q_INVOKABLE void    setResult(QString str);

  public slots:
    void onExit();

  signals:
    void setErrorString(QString err);
    void setInfoString(QString info);
    void setPromptString(QString text, bool echo);
    void focusField();
    void blockInput(bool block);
    void clearPasswordField();
};
