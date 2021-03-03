// Copyright (c) 2014-2020 NetEase, Inc.
// All right reserved.

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "nemeeting_manager.h"
#include "utils/clipboard.h"
int main(int argc, char* argv[]) {
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QGuiApplication::setOrganizationName("NetEase");
    QGuiApplication::setOrganizationDomain("yunxin.163.com");
    QGuiApplication::setApplicationName("MeetingSample");
    QGuiApplication::setApplicationDisplayName("NetEase Meeting");

    QGuiApplication app(argc, argv);

    QQuickStyle::setStyle("Material");

    QQmlApplicationEngine engine;

    NEMeetingManager meetingManager;

    engine.rootContext()->setContextProperty("meetingManager", &meetingManager);

    qmlRegisterUncreatableType<MeetingsStatus>("NetEase.Meeting.MeetingStatus", 1, 0, "MeetingStatus", "");
    qmlRegisterUncreatableType<RunningStatus>("NetEase.Meeting.RunningStatus", 1, 0, "RunningStatus", "");
    qmlRegisterType<Clipboard>("NetEase.Meeting.Clipboard", 1, 0, "Clipboard");

    const QUrl url(QStringLiteral("qrc:/qml/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app,
                     [url](QObject* obj, const QUrl& objUrl) {
                         if (!obj && url == objUrl)
                             QCoreApplication::exit(-1);
                     },
                     Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
