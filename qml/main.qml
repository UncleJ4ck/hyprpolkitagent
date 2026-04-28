import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window

    readonly property real s: fontMetrics.height * 0.4
    readonly property real s2: s * 2
    readonly property real s3: s * 3
    readonly property real s4: s * 4
    readonly property real windowWidth: fontMetrics.height * 36
    readonly property real windowMargin: s * 4

    property string cleanUser: {
        var user = hpa.getUser();
        if (user.startsWith("unix-user:")) {
            return user.substring(10);
        }
        return user;
    }

    property string rawMessage: hpa.getMessage()
    property string commandText: {
        var candidate = "";
        var fromDetails = "";
        if (hpa.getCommand) {
            fromDetails = hpa.getCommand();
        }
        if (fromDetails && fromDetails.length > 0) {
            candidate = fromDetails;
        } else {
            var match = rawMessage.match(/`([^`]+)`/);
            if (!match) {
                match = rawMessage.match(/'([^']+)'/);
            }
            if (!match) {
                match = rawMessage.match(/"([^"]+)"/);
            }
            if (!match) {
                match = rawMessage.match(/run\s+(.+?)(?:\s+as\s+the\s+|\s+as\s+|\.?$)/i);
            }
            candidate = match ? match[1] : "";
        }
        return normalizeCommandText(candidate);
    }
    property string messageText: {
        if (commandText !== "") {
            return "Authentication is needed to run";
        }
        return rawMessage;
    }

    property string promptText: hpa.getInitialPrompt()
    property bool promptEcho: hpa.getInitialPromptEcho()
    property string infoText: ""
    property bool capsOn: false
    property bool detailsExpanded: false

    function isQuoteChar(ch) {
        return ch === "`" || ch === "'" || ch === "\"";
    }

    function normalizeCommandText(cmd) {
        var cleaned = (cmd || "").trim();
        var changed = true;
        while (changed && cleaned.length >= 2) {
            var first = cleaned[0];
            var last = cleaned[cleaned.length - 1];
            if (isQuoteChar(first) && isQuoteChar(last)) {
                cleaned = cleaned.slice(1, -1).trim();
            } else {
                changed = false;
            }
        }
        return cleaned;
    }

    function normalizePromptLabel(s) {
        var t = (s || "").trim();
        while (t.length > 0 && t[t.length - 1] === ":") {
            t = t.slice(0, -1).trim();
        }
        return t.length > 0 ? t : "Password";
    }

    flags: Qt.Dialog | Qt.WindowStaysOnTopHint

    width: windowWidth
    height: minimumHeight
    minimumWidth: windowWidth
    maximumWidth: windowWidth
    minimumHeight: contentLayout.implicitHeight + windowMargin * 2
    maximumHeight: minimumHeight
    visible: true

    onClosing: {
        hpa.setResult("fail");
    }

    FontMetrics {
        id: fontMetrics
    }

    SystemPalette {
        id: activePalette
        colorGroup: SystemPalette.Active
    }

    SystemPalette {
        id: disabledPalette
        colorGroup: SystemPalette.Disabled
    }

    Connections {
        target: hpa
        function onSetErrorString(e) { errorLabel.text = e; infoText = ""; }
        function onSetInfoString(t) { infoText = t; errorLabel.text = ""; }
        function onSetPromptString(text, echo) {
            promptText = normalizePromptLabel(text);
            promptEcho = echo;
        }
        function onClearPasswordField() { passwordField.text = ""; }
        function onFocusField() { passwordField.forceActiveFocus(); }
        function onBlockInput(block) {
            passwordField.readOnly = block;
            if (!block) { passwordField.forceActiveFocus(); passwordField.selectAll(); }
        }
    }

    Item {
        id: mainLayout
        anchors.fill: parent

        Keys.onEscapePressed: hpa.setResult("fail")
        Keys.onReturnPressed: { if (passwordField.text.length > 0) hpa.setResult("auth:" + passwordField.text); }
        Keys.onEnterPressed: { if (passwordField.text.length > 0) hpa.setResult("auth:" + passwordField.text); }
        Keys.onPressed: (e) => { if (e.key === Qt.Key_CapsLock) capsOn = !capsOn; }

        ColumnLayout {
            id: contentLayout
            anchors.fill: parent
            anchors.margins: windowMargin
            spacing: 0

            // === IDENTITY ===
            Image {
                id: actionIcon
                source: {
                    var n = hpa.getIconName();
                    var fallback = "system-lock-screen,dialog-password,security-high";
                    return "image://themeicon/" + (n && n.length > 0 ? n + "," + fallback : fallback);
                }
                visible: status === Image.Ready
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: fontMetrics.height * 2.6
                Layout.preferredHeight: fontMetrics.height * 2.6
                sourceSize.width: fontMetrics.height * 2.6
                sourceSize.height: fontMetrics.height * 2.6
                fillMode: Image.PreserveAspectFit
                smooth: true
            }

            Label {
                text: "Authentication Required"
                color: activePalette.windowText
                font.bold: true
                font.pointSize: Math.round(fontMetrics.height * 1.25)
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: s
            }

            Label {
                text: "for " + cleanUser
                color: disabledPalette.windowText
                font.pointSize: Math.round(fontMetrics.height * 0.9)
                Layout.alignment: Qt.AlignHCenter
            }

            // === CONTEXT ===
            Label {
                text: messageText
                color: activePalette.windowText
                font.pointSize: Math.round(fontMetrics.height * 0.95)
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                Layout.topMargin: s3
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Rectangle {
                id: commandBox
                visible: commandText !== ""
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: Math.min(commandLabel.implicitWidth + s4, windowWidth - s4 * 2)
                Layout.maximumWidth: windowWidth - s4 * 2
                Layout.preferredHeight: commandLabel.implicitHeight + s2
                Layout.minimumHeight: fontMetrics.height * 2
                Layout.topMargin: s2

                color: activePalette.base
                border.color: Qt.rgba(activePalette.windowText.r, activePalette.windowText.g, activePalette.windowText.b, 0.20)
                border.width: 1
                radius: 6

                Label {
                    id: commandLabel
                    anchors.fill: parent
                    anchors.margins: s
                    text: commandText
                    color: activePalette.windowText
                    font.family: "monospace"
                    font.pointSize: Math.round(fontMetrics.height * 0.9)
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.WrapAnywhere
                }
            }

            // === ACTION ===
            Item {
                id: passwordRow
                Layout.fillWidth: true
                Layout.leftMargin: s3
                Layout.rightMargin: s3
                Layout.topMargin: s3
                implicitHeight: passwordField.implicitHeight

                TextField {
                    id: passwordField
                    anchors.fill: parent
                    placeholderText: promptText
                    horizontalAlignment: TextInput.AlignHCenter
                    hoverEnabled: true
                    persistentSelection: true
                    echoMode: promptEcho ? TextInput.Normal : TextInput.Password
                    focus: true
                    rightPadding: revealButton.visible ? revealButton.width + s : 0
                    leftPadding: revealButton.visible ? revealButton.width + s : 0

                    onTextChanged: {
                        if (errorLabel.text !== "") errorLabel.text = "";
                        if (infoText !== "") infoText = "";
                    }

                    Keys.onPressed: (e) => { if (e.key === Qt.Key_CapsLock) capsOn = !capsOn; }
                }

                Button {
                    id: revealButton
                    visible: !promptEcho
                    flat: true
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.rightMargin: s * 0.5
                    height: parent.height - s * 0.5
                    padding: s * 0.5
                    checkable: true
                    checked: passwordField.echoMode === TextInput.Normal
                    text: "Show"
                    onToggled: {
                        passwordField.echoMode = checked ? TextInput.Normal : TextInput.Password
                        passwordField.forceActiveFocus()
                    }
                }
            }

            // status messages
            Label {
                id: capsLockLabel
                visible: capsOn && passwordField.activeFocus
                text: "Caps Lock is on"
                color: activePalette.linkVisited
                font.italic: true
                font.pointSize: Math.round(fontMetrics.height * 0.8)
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: s
            }

            Label {
                id: errorLabel
                color: activePalette.link
                font.italic: true
                font.pointSize: Math.round(fontMetrics.height * 0.85)
                text: ""
                visible: text !== ""
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: s
            }

            Label {
                id: infoLabel
                color: disabledPalette.windowText
                font.pointSize: Math.round(fontMetrics.height * 0.85)
                text: infoText
                visible: text !== ""
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                Layout.topMargin: s
            }

            // primary actions
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: s3
                spacing: s2

                Button {
                    text: "Cancel"
                    onClicked: hpa.setResult("fail")
                }

                Button {
                    text: "Authenticate"
                    highlighted: true
                    enabled: passwordField.text.length > 0
                    onClicked: hpa.setResult("auth:" + passwordField.text)
                }
            }

            // details disclosure
            Button {
                id: detailsButton
                visible: hpa.getActionId().length > 0 || hpa.getVendor().length > 0 || hpa.getVendorUrl().length > 0 || hpa.getDetailList().length > 0
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: s2
                flat: true
                checkable: true
                checked: detailsExpanded
                text: detailsExpanded ? "Hide details" : "Show details"
                font.pointSize: Math.round(fontMetrics.height * 0.85)
                onToggled: detailsExpanded = checked
            }

            Rectangle {
                id: detailsBox
                visible: detailsExpanded
                Layout.fillWidth: true
                Layout.topMargin: s
                Layout.preferredHeight: detailsContent.implicitHeight + s4

                color: activePalette.base
                border.color: Qt.rgba(activePalette.windowText.r, activePalette.windowText.g, activePalette.windowText.b, 0.18)
                border.width: 1
                radius: 6

                ColumnLayout {
                    id: detailsContent
                    anchors.fill: parent
                    anchors.margins: s2
                    spacing: s2

                    DetailRow { keyText: "Action";     valueText: hpa.getActionId();  monospace: true  }
                    DetailRow { keyText: "Vendor";     valueText: hpa.getVendor();    monospace: false }
                    DetailRow { keyText: "Vendor URL"; valueText: hpa.getVendorUrl(); monospace: true  }

                    Repeater {
                        model: hpa.getDetailList()
                        delegate: DetailRow {
                            keyText: modelData.key
                            valueText: modelData.value
                            monospace: true
                        }
                    }
                }
            }
        }
    }

    component DetailRow: RowLayout {
        property string keyText: ""
        property string valueText: ""
        property bool monospace: false
        visible: valueText && valueText.length > 0
        Layout.fillWidth: true
        spacing: s2

        Label {
            text: keyText
            color: disabledPalette.windowText
            font.pointSize: Math.round(fontMetrics.height * 0.85)
            Layout.preferredWidth: fontMetrics.height * 6
            Layout.alignment: Qt.AlignTop
            horizontalAlignment: Text.AlignRight
        }
        Label {
            text: valueText
            color: activePalette.windowText
            font.family: monospace ? "monospace" : font.family
            font.pointSize: Math.round(fontMetrics.height * 0.9)
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            wrapMode: Text.WrapAnywhere
        }
    }
}
