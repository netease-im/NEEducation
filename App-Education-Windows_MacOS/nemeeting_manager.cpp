#include "nemeeting_manager.h"
#include <QDesktopServices>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <future>
#include <iostream>

NEMeetingManager::NEMeetingManager(QObject* parent)
    : QObject(parent)
    , m_initialized(false) {
    connect(this, &NEMeetingManager::unInitializeSignal, this, []() { qApp->exit(); });
}

void NEMeetingManager::initialize(const QString& strAppkey) {
    if (m_initialized) {
        emit initializeSignal(0, "");
        return;
    }

    NEMeetingSDKConfig config;
    QString displayName = QObject::tr("智慧云课堂");
    QByteArray byteDisplayName = displayName.toUtf8();
    config.setLogSize(10);
    config.getAppInfo()->ProductName(byteDisplayName.data());
    config.getAppInfo()->OrganizationName("NetEase");
    config.getAppInfo()->ApplicationName("ClassSample");
    config.setDomain("yunxin.163.com");
    config.setAppKey(strAppkey.toStdString());
    auto pMeetingSDK = NEMeetingSDK::getInstance();
    // level value: DEBUG = 0, INFO = 1, WARNING=2, ERROR=3, FATAL=4
    pMeetingSDK->setLogHandler([](int level, const std::string& log) {
        switch (level) {
            case 0:
                qDebug() << log.c_str();
                break;
            case 1:
                qInfo() << log.c_str();
                break;
            case 2:
                qWarning() << log.c_str();
                break;
            case 3:
                qCritical() << log.c_str();
                break;
            case 4:
                qFatal("%s", log.c_str());
                break;
            default:
                qInfo() << log.c_str();
        }
    });
    pMeetingSDK->initialize(config, [this](NEErrorCode errorCode, const std::string& errorMessage) {
        qInfo() << "Initialize callback, error code: " << errorCode << ", error message: " << QString::fromStdString(errorMessage);

        auto ipcMeetingService = NEMeetingSDK::getInstance()->getMeetingService();
        if (ipcMeetingService) {
            ipcMeetingService->addMeetingStatusListener(this);
            ipcMeetingService->setOnInjectedMenuItemClickListener(this);
        }
        auto ipcPreMeetingService = NEMeetingSDK::getInstance()->getPremeetingService();
        if (ipcPreMeetingService) {
            ipcPreMeetingService->registerScheduleMeetingStatusListener(this);
        }
        auto settingsService = NEMeetingSDK::getInstance()->getSettingsService();
        if (settingsService) {
            settingsService->setNESettingsChangeNotifyHandler(this);
        }

        m_initialized = true;
        emit initializeSignal(errorCode, QString::fromStdString(errorMessage));
    });
}

void NEMeetingManager::unInitialize() {
    qInfo() << "Do uninitialize, initialize flag: " << m_initialized;

    if (!m_initialized) {
        emit unInitializeSignal(ERROR_CODE_SUCCESS, "");
        qInfo() << "Uninitialized successfull";
        return;
    }

    NEMeetingSDK::getInstance()->setExceptionHandler(nullptr);
    NEMeetingSDK::getInstance()->unInitialize([&](NEErrorCode errorCode, const std::string& errorMessage) {
        qInfo() << "Uninitialize callback, error code: " << errorCode << ", error message: " << QString::fromStdString(errorMessage);
        m_initialized = false;
        emit unInitializeSignal(errorCode, QString::fromStdString(errorMessage));
        qInfo() << "Uninitialized successfull";
    });
}

bool NEMeetingManager::isInitializd() {
    return NEMeetingSDK::getInstance()->isInitialized();
}

void NEMeetingManager::loginAnonymous() {
    auto ipcAuthService = NEMeetingSDK::getInstance()->getAuthService();
    if (ipcAuthService) {
        ipcAuthService->loginAnonymous([this](NEErrorCode errorCode, const std::string& errorMessage) {
            qInfo() << "Login callback, error code: " << errorCode << ", error message: " << QString::fromStdString(errorMessage);
            emit loginSignal(errorCode, QString::fromStdString(errorMessage));
        });
    }
}

void NEMeetingManager::logout() {
    qInfo() << "Logout from apaas server";

    auto ipcAuthService = NEMeetingSDK::getInstance()->getAuthService();
    if (ipcAuthService) {
        ipcAuthService->logout(true, [this](NEErrorCode errorCode, const std::string& errorMessage) {
            qInfo() << "Logout callback, error code: " << errorCode << ", error message: " << QString::fromStdString(errorMessage);
            emit logoutSignal(errorCode, QString::fromStdString(errorMessage));
        });
    }
}

void NEMeetingManager::scheduleMeeting(const QString& meetingSubject,
                                       qint64 startTime,
                                       qint64 endTime,
                                       const QString& password,
                                       bool attendeeAudioOff,
                                       int classType) {
    qInfo() << "startTime : " << startTime << ", endTime : " << endTime;
    QString strPassword = password.isEmpty() ? "null" : "no null";
    while (!m_initialized) {
        std::this_thread::yield();
    }
    auto ipcPreMeetingService = NEMeetingSDK::getInstance()->getPremeetingService();
    if (ipcPreMeetingService) {
        NEMeetingItem item;
        item.subject = meetingSubject.toUtf8().data();
        item.startTime = startTime;
        item.endTime = endTime;
        item.password = password.toUtf8().data();
        item.setting.attendeeAudioOff = attendeeAudioOff;
        // 1v1
        if (classType == 0) {
            item.setting.scene.code = "education-1-to-1";
            NEMeetingRoleConfiguration configStudent;
            configStudent.roleType = normal;
            configStudent.maxCount = 1;
            NEMeetingRoleConfiguration configTeacher;
            configTeacher.roleType = host;
            configTeacher.maxCount = 1;
            item.setting.scene.roleTypes.push_back(configStudent);
            item.setting.scene.roleTypes.push_back(configTeacher);
        }  //小班课
        else if (classType == 1) {
            item.setting.scene.code = "education-small-class";
            NEMeetingRoleConfiguration configStudent;
            configStudent.roleType = normal;
            configStudent.maxCount = 30;
            NEMeetingRoleConfiguration configTeacher;
            configTeacher.roleType = host;
            configTeacher.maxCount = 1;
            item.setting.scene.roleTypes.push_back(configStudent);
            item.setting.scene.roleTypes.push_back(configTeacher);
        }
        ipcPreMeetingService->scheduleMeeting(item, [this](NEErrorCode errorCode, const std::string& errorMessage, const NEMeetingItem& item) {
            qInfo() << "Schedule meeting callback, error code: " << errorCode << ", error message: " << QString::fromStdString(errorMessage);
            if (errorCode == ERROR_CODE_SUCCESS) {
                QJsonObject json;
                json.insert("classname", QString::fromStdString(item.subject));
                json.insert("classid", QString::fromStdString(item.meetingId));
                json.insert("classtype", QString::fromStdString(item.setting.scene.code));
                emit scheduleSuccessSignal(json);
            } else {
                emit scheduleSignal(errorCode, QString::fromStdString(errorMessage));
            }
        });
    }
}

void NEMeetingManager::getMeetingList() {
    qInfo() << "Get meeting list from IPC client.";

    auto ipcPreMeetingService = NEMeetingSDK::getInstance()->getPremeetingService();
    if (ipcPreMeetingService) {
        std::list<NEMeetingItemStatus> status;
        status.push_back(MEETING_INIT);
        status.push_back(MEETING_STARTED);
        status.push_back(MEETING_ENDED);
        ipcPreMeetingService->getMeetingList(
            status, [this](NEErrorCode errorCode, const std::string& errorMessage, std::list<NEMeetingItem>& meetingItems) {
                qInfo() << "GetMeetingList callback, error code: " << errorCode << ", error message: " << QString::fromStdString(errorMessage);
                QJsonArray jsonArray;
                if (errorCode == ERROR_CODE_SUCCESS) {
                    for (auto& item : meetingItems) {
                        qInfo() << "Got meeting list, unique meeting ID: " << item.meetingUniqueId
                                << ", meeting ID: " << QString::fromStdString(item.meetingId) << ", topic: " << QString::fromStdString(item.subject)
                                << ", start time: " << item.startTime << ", end time: " << item.endTime << ", create time: " << item.createTime
                                << ", update time: " << item.updateTime << ", status: " << item.status
                                << ", mute after member join: " << item.setting.attendeeAudioOff;
                        QJsonObject object;
                        object["uniqueMeetingId"] = item.meetingUniqueId;
                        object["meetingId"] = QString::fromStdString(item.meetingId);
                        object["topic"] = QString::fromStdString(item.subject);
                        object["startTime"] = item.startTime;
                        object["endTime"] = item.endTime;
                        object["createTime"] = item.createTime;
                        object["updateTime"] = item.updateTime;
                        object["status"] = item.status;
                        object["password"] = QString::fromStdString(item.password);
                        object["attendeeAudioOff"] = item.setting.attendeeAudioOff;
                        jsonArray.push_back(object);
                    }
                    emit getScheduledMeetingList(errorCode, jsonArray);
                } else {
                    emit getScheduledMeetingList(errorCode, jsonArray);
                }
            });
    }
}

void NEMeetingManager::invokeStart(const QString& meetingId,
                                   const QString& nickname,
                                   bool audio,
                                   bool video,
                                   bool teacher,
                                   bool enableChatroom /* = true*/,
                                   bool enableInvitation /* = true*/,
                                   int displayOption) {
    qInfo() << "Start a meeting with meeting ID:" << meetingId << ", nickname: " << nickname << ", audio: " << audio << ", video: " << video
            << ", display id: " << displayOption;

    auto ipcMeetingService = NEMeetingSDK::getInstance()->getMeetingService();
    if (ipcMeetingService) {
        QByteArray byteMeetingId = meetingId.toUtf8();
        QByteArray byteNickname = nickname.toUtf8();

        NEStartMeetingParams params;
        params.meetingId = byteMeetingId.data();
        params.displayName = byteNickname.data();

        NEStartMeetingOptions options;
        options.noAudio = !audio;
        options.noVideo = !video;
        options.noChat = !enableChatroom;
        options.noInvite = !enableInvitation;
        options.meetingIdDisplayOption = (NEShowMeetingIdOption)displayOption;
        if (teacher) {
            options.viewMode = WHITEBOARD_MODE;
        }
        // pushSubmenus(options.full_more_menu_items_, kFirstinjectedMenuId);
        ipcMeetingService->startMeeting(params, options, [this](NEErrorCode errorCode, const std::string& errorMessage) {
            QString err = QString::fromStdString(errorMessage);
            err.replace("会议", "课堂");
            qInfo() << "Start meeting callback, error code: " << errorCode << ", error message: " << err;
            emit startSignal(errorCode, err);
        });
    }
}

void NEMeetingManager::invokeJoin(const QString& meetingId,
                                  const QString& nickname,
                                  bool audio,
                                  bool video,
                                  bool teacher,
                                  bool enableChatroom /* = true*/,
                                  bool enableInvitation /* = true*/,
                                  const QString& password,
                                  int displayOption) {
    qInfo() << "Join a meeting with meeting ID:" << meetingId << ", nickname: " << nickname << ", audio: " << audio << ", video: " << video
            << ", display id: " << displayOption;

    while (!m_initialized) {
        std::this_thread::yield();
    }

    auto ipcMeetingService = NEMeetingSDK::getInstance()->getMeetingService();
    if (ipcMeetingService) {
        QByteArray byteMeetingId = meetingId.toUtf8();
        QByteArray byteNickname = nickname.toUtf8();

        NEJoinMeetingParams params;
        params.meetingId = byteMeetingId.data();
        params.displayName = byteNickname.data();
        params.password = password.toUtf8().data();

        NEJoinMeetingOptions options;
        options.noAudio = !audio;
        options.noVideo = !video;
        options.noChat = !enableChatroom;
        options.noInvite = !enableInvitation;
        options.meetingIdDisplayOption = (NEShowMeetingIdOption)displayOption;
        if (teacher) {
            options.viewMode = WHITEBOARD_MODE;
        }
        // pushSubmenus(options.full_more_menu_items_, kFirstinjectedMenuId);
        ipcMeetingService->joinMeeting(params, options, [this](NEErrorCode errorCode, const std::string& errorMessage) {
            QString err = QString::fromStdString(errorMessage);
            err.replace("会议", "课堂");
            qInfo() << "Join meeting callback, error code: " << errorCode << ", error message: " << QString::fromStdString(errorMessage);
            emit joinSignal(errorCode, err);
        });
    }
}

void NEMeetingManager::leaveMeeting(bool finish) {
    auto ipcMeetingService = NEMeetingSDK::getInstance()->getMeetingService();
    if (ipcMeetingService) {
        ipcMeetingService->leaveMeeting(finish, [=](NEErrorCode errorCode, const std::string& errorMessage) {
            qInfo() << "Leave meeting callback, error code: " << errorCode << ", error message: " << QString::fromStdString(errorMessage);
            emit leaveSignal(errorCode, QString::fromStdString(errorMessage));
        });
    }
}

int NEMeetingManager::getMeetingStatus() {
    auto ipcMeetingService = NEMeetingSDK::getInstance()->getMeetingService();
    if (ipcMeetingService) {
        return ipcMeetingService->getMeetingStatus();
    }

    return MEETING_STATUS_IDLE;
}

void NEMeetingManager::getMeetingInfo() {
    auto ipcMeetingService = NEMeetingSDK::getInstance()->getMeetingService();
    if (ipcMeetingService) {
        ipcMeetingService->getCurrentMeetingInfo([this](NEErrorCode errorCode, const std::string& errorMessage, const NEMeetingInfo& meetingInfo) {
            if (errorCode == ERROR_CODE_SUCCESS)
                emit getCurrentMeetingInfo(QString::fromStdString(meetingInfo.meetingId), meetingInfo.isHost, meetingInfo.isLocked,
                                           meetingInfo.duration);
            else
                emit error(errorCode, QString::fromStdString(errorMessage));
        });
    }
}

void NEMeetingManager::onMeetingStatusChanged(int status, int code) {
    qInfo() << "Meeting status changed, status:" << status << ", code:" << code;
    emit meetingStatusChanged(status, code);
}

void NEMeetingManager::onInjectedMenuItemClick(const NEMeetingMenuItem& meeting_menu_item) {
    qInfo() << "Meeting injected menu item clicked, item index: " << meeting_menu_item.itemId
            << ", guid: " << QString::fromStdString(meeting_menu_item.itemGuid) << ", title: " << QString::fromStdString(meeting_menu_item.itemTitle)
            << ", image path: " << QString::fromStdString(meeting_menu_item.itemImage);

    QDesktopServices::openUrl(QUrl(QString::fromStdString("https://www.google.com.hk/search?q=" + meeting_menu_item.itemTitle)));

    emit meetingInjectedMenuItemClicked(meeting_menu_item.itemId, QString::fromStdString(meeting_menu_item.itemGuid),
                                        QString::fromStdString(meeting_menu_item.itemTitle), QString::fromStdString(meeting_menu_item.itemImage));
}

void NEMeetingManager::onScheduleMeetingStatusChanged(uint64_t uniqueMeetingId, const int& meetingStatus) {
    qInfo() << "Scheduled meeting status changed, unique meeting ID:" << uniqueMeetingId << meetingStatus;
    QMetaObject::invokeMethod(this, "onGetMeetingListUI", Qt::AutoConnection);
}

void NEMeetingManager::onInjectedMenuItemClickEx(const NEMeetingMenuItem& meeting_menu_item, const NEInjectedMenuItemClickCallback& cb) {
    qInfo() << "Meeting injected menu item clicked, item index: " << meeting_menu_item.itemId
            << ", guid: " << QString::fromStdString(meeting_menu_item.itemGuid) << ", title: " << QString::fromStdString(meeting_menu_item.itemTitle)
            << ", image path: " << QString::fromStdString(meeting_menu_item.itemImage)
            << ", title2: " << QString::fromStdString(meeting_menu_item.itemTitle2)
            << ", image path2: " << QString::fromStdString(meeting_menu_item.itemImage2)
            << ", itemVisibility: " << (int)meeting_menu_item.itemVisibility << ", itemCheckedIndex: " << meeting_menu_item.itemCheckedIndex;

    if ((kFirstinjectedMenuId + 1) == meeting_menu_item.itemId || (kFirstinjectedMenuId + 50 + 1) == meeting_menu_item.itemId) {
        cb(meeting_menu_item.itemId, meeting_menu_item.itemGuid, 1 == meeting_menu_item.itemCheckedIndex ? 2 : 1);
    } else {
        cb(meeting_menu_item.itemId, meeting_menu_item.itemGuid, meeting_menu_item.itemCheckedIndex);
    }

    emit meetingInjectedMenuItemClicked(
        meeting_menu_item.itemId, QString::fromStdString(meeting_menu_item.itemGuid),
        QString::fromStdString(1 == meeting_menu_item.itemCheckedIndex ? meeting_menu_item.itemTitle : meeting_menu_item.itemTitle2),
        QString::fromStdString(meeting_menu_item.itemImage));
}

QString NEMeetingManager::personalMeetingId() const {
    return m_personalMeetingId;
}

void NEMeetingManager::setPersonalMeetingId(const QString& personalMeetingId) {
    m_personalMeetingId = personalMeetingId;
    emit personalMeetingIdChanged();
}

void NEMeetingManager::OnAudioSettingsChange(bool status) {
    emit deviceStatusChanged(1, status);
}

void NEMeetingManager::OnVideoSettingsChange(bool status) {
    emit deviceStatusChanged(2, status);
}

void NEMeetingManager::OnOtherSettingsChange(bool status) {}

void NEMeetingManager::pushSubmenus(std::vector<NEMeetingMenuItem>& items_list, int MenuIdIndex) {
    auto applicationPath = qApp->applicationDirPath();
#ifdef Q_OS_WIN32
    QByteArray byteImage = QString(QGuiApplication::applicationDirPath() + "/" + "feedback.png").toUtf8();
    QByteArray byteImage2 = QString(QGuiApplication::applicationDirPath() + "/" + "feedback 2.png").toUtf8();
#else
    QByteArray byteImage = QString(QGuiApplication::applicationDirPath() + "/../Resources/feedback.png").toUtf8();
    QByteArray byteImage2 = QString(QGuiApplication::applicationDirPath() + "/../Resources/feedback 2.png").toUtf8();
#endif

    NEMeetingMenuItem item;
    item.itemId = MenuIdIndex++;
    item.itemTitle = QString(QStringLiteral("Menu1")).toStdString();
    item.itemImage = byteImage.data();
    item.itemVisibility = NEMenuVisibility::VISIBLE_TO_HOST_ONLY;
    items_list.push_back(item);

    NEMeetingMenuItem item2;
    item2.itemId = MenuIdIndex++;
    item2.itemTitle = QString(QStringLiteral("Menu2")).toStdString();
    item2.itemImage = byteImage.data();
    item2.itemTitle2 = QString(QStringLiteral("Menu2_2")).toStdString();
    item2.itemImage2 = byteImage2.data();
    item2.itemVisibility = NEMenuVisibility::VISIBLE_TO_HOST_ONLY;
    items_list.push_back(item2);

    return;
    NEMeetingMenuItem item3;
    item3.itemId = MenuIdIndex++;
    item3.itemTitle = QString(QStringLiteral("Menu3")).toStdString();
    item3.itemImage = byteImage.data();
    item3.itemTitle2 = QString(QStringLiteral("Menu3_2")).toStdString();
    item3.itemImage2 = byteImage2.data();
    items_list.push_back(item3);

    NEMeetingMenuItem item4;
    item4.itemId = MenuIdIndex++;
    item4.itemTitle = QString(QStringLiteral("Menu4")).toStdString();
    item4.itemImage = byteImage.data();
    items_list.push_back(item4);

    NEMeetingMenuItem item5;
    item5.itemId = MenuIdIndex++;
    item5.itemTitle = QString(QStringLiteral("Menu5")).toStdString();
    item5.itemImage = byteImage.data();
    items_list.push_back(item5);

    NEMeetingMenuItem item6;
    item6.itemId = MenuIdIndex++;
    item6.itemTitle = QString(QStringLiteral("Menu6")).toStdString();
    item6.itemImage = byteImage.data();
    items_list.push_back(item6);

    NEMeetingMenuItem item7;
    item7.itemId = MenuIdIndex++;
    item7.itemTitle = QString(QStringLiteral("Menu7")).toStdString();
    item7.itemImage = byteImage.data();
    items_list.push_back(item7);
}

void NEMeetingManager::onGetMeetingListUI() {
    getMeetingList();
}

bool NEMeetingManager::checkAudio() {
    auto ipcSettingService = NEMeetingSDK::getInstance()->getSettingsService();
    if (ipcSettingService) {
        auto AudioController = ipcSettingService->GetAudioController();
        if (AudioController) {
            // std::promise<bool> promise;
            AudioController->isTurnOnMyAudioWhenJoinMeetingEnabled([this](NEErrorCode errorCode, const std::string& errorMessage, const bool& bOn) {
                if (errorCode == ERROR_CODE_SUCCESS)
                    OnAudioSettingsChange(bOn);
                else {
                    // promise.set_value(false);
                    emit error(errorCode, QString::fromStdString(errorMessage));
                }
            });

            // std::future<bool> future = promise.get_future();
            return true;  // future.get();
        }
    }
    return false;
}

void NEMeetingManager::setCheckAudio(bool checkAudio) {
    auto ipcSettingService = NEMeetingSDK::getInstance()->getSettingsService();
    if (ipcSettingService) {
        auto audioController = ipcSettingService->GetAudioController();
        if (audioController) {
            audioController->setTurnOnMyAudioWhenJoinMeeting(checkAudio, [this, checkAudio](NEErrorCode errorCode, const std::string& errorMessage) {
                if (errorCode != ERROR_CODE_SUCCESS)
                    emit error(errorCode, QString::fromStdString(errorMessage));

                OnAudioSettingsChange(checkAudio);
            });
        }
    }
}

bool NEMeetingManager::checkVideo() {
    auto ipcSettingService = NEMeetingSDK::getInstance()->getSettingsService();
    if (ipcSettingService) {
        auto videoController = ipcSettingService->GetVideoController();
        if (videoController) {
            videoController->isTurnOnMyVideoWhenJoinMeetingEnabled([this](NEErrorCode errorCode, const std::string& errorMessage, const bool& bOn) {
                if (errorCode == ERROR_CODE_SUCCESS)
                    OnVideoSettingsChange(bOn);
                else {
                    // promise.set_value(false);
                    emit error(errorCode, QString::fromStdString(errorMessage));
                }
            });

            return true;
        }
    }
    return false;
}

void NEMeetingManager::setCheckVideo(bool checkVideo) {
    auto ipcSettingService = NEMeetingSDK::getInstance()->getSettingsService();
    if (ipcSettingService) {
        auto videoController = ipcSettingService->GetVideoController();
        if (videoController) {
            videoController->setTurnOnMyVideoWhenJoinMeeting(checkVideo, [this, checkVideo](NEErrorCode errorCode, const std::string& errorMessage) {
                if (errorCode != ERROR_CODE_SUCCESS)
                    emit error(errorCode, QString::fromStdString(errorMessage));

                OnVideoSettingsChange(checkVideo);
            });
        }
    }
}

// bool NEMeetingManager::checkDuration()
//{
//    auto ipcSettingService = NEMeetingSDK::getInstance()->getSettingsService();
//    if (ipcSettingService)
//    {
//        auto otherController = ipcSettingService->GetOtherController();
//        if (otherController)
//        {
//            std::promise<bool> promise;
//            otherController->isShowMyMeetingElapseTimeEnabled([this, &promise](NEErrorCode errorCode, const std::string& errorMessage, const bool&
//            bOn){
//                if (errorCode == ERROR_CODE_SUCCESS)
//                    promise.set_value(bOn);
//                else
//                {
//                    promise.set_value(false);
//                    emit error(errorCode, QString::fromStdString(errorMessage));
//                }
//            });

//            std::future<bool> future = promise.get_future();
//            return future.get();
//        }
//    }
//    return false;
//}

// void NEMeetingManager::setCheckDuration(bool checkDuration)
//{
//    auto ipcSettingService = NEMeetingSDK::getInstance()->getSettingsService();
//    if (ipcSettingService)
//    {
//        auto otherController = ipcSettingService->GetOtherController();
//        if (otherController)
//        {
//            otherController->enableShowMyMeetingElapseTime(checkDuration, [this](NEErrorCode errorCode, const std::string& errorMessage){
//                if (errorCode != ERROR_CODE_SUCCESS)
//                   emit error(errorCode, QString::fromStdString(errorMessage));
//            });
//        }
//    }
//}
