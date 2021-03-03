﻿import QtQuick 2.12

/**
 * @brief Manager that creates Toasts dynamically
 */
Column {
    /**
     * Private
     */

    id: root

    z: Infinity
    spacing: 5
    anchors.bottom: parent.bottom
    anchors.bottomMargin: 95
    anchors.horizontalCenter: parent.horizontalCenter
    // anchors.centerIn: parent

    property var toastComponent

    /**
     * @brief Shows a Toast
     *
     * @param {string} text Text to show
     * @param {real} duration Duration to show in milliseconds, defaults to 3000
     */
    function show(text, duration){
        const toast = toastComponent.createObject(root, { background: "#771E1E1E", textColor: "#FFFFFF", margin: 8 })
        toast.selfDestroying = true;
        toast.show(text, duration);
    }

    function warning(text, duration) {
        const toast = toastComponent.createObject(root, { background: "#FFFFFF", textColor: "#333333", margin: 10 })
        toast.selfDestroying = true;
        toast.icon = "qrc:/images/public/toast/icon_warning.svg"
        toast.show(text, duration);
    }

    function error(text, duration) {
        const toast = toastComponent.createObject(root, { background: "#FFFFFF", textColor: "#333333", margin: 10 })
        toast.selfDestroying = true;
        toast.icon = "qrc:/images/public/toast/icon_error.svg"
        toast.show(text, duration);
    }


    function info(text, duration) {
        const toast = toastComponent.createObject(root, { background: "#FFFFFF", textColor: "#333333", margin: 10 })
        toast.selfDestroying = true;
        toast.icon = "qrc:/images/public/toast/icon_info.svg"
        toast.show(text, duration);
    }

    Component.onCompleted: {
        toastComponent = Qt.createComponent("Toast.qml")
    }
}
