import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window

    property real windowWidth: fontMetrics.height * 34
    property real baseWindowHeight: fontMetrics.height * 16
    property real windowMargin: fontMetrics.height * 1.2

    // Clean up user string - remove "unix-user:" prefix if present
    property string cleanUser: {
        var user = hpa.getUser();
        if (user.startsWith("unix-user:")) {
            return user.substring(10);
        }
        return user;
    }

    // Parse the message to extract command if present
    // Polkit commonly uses backticks or single quotes around commands.
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

    flags: Qt.Dialog | Qt.WindowStaysOnTopHint

    width: windowWidth
    height: minimumHeight
    minimumWidth: windowWidth
    maximumWidth: windowWidth
    minimumHeight: Math.max(baseWindowHeight, contentLayout.implicitHeight + windowMargin * 2)
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

    Item {
        id: mainLayout

        anchors.fill: parent
        Keys.onEscapePressed: (e) => {
            hpa.setResult("fail");
        }
        Keys.onReturnPressed: (e) => {
            hpa.setResult("auth:" + passwordField.text);
        }
        Keys.onEnterPressed: (e) => {
            hpa.setResult("auth:" + passwordField.text);
        }

        ColumnLayout {
            id: contentLayout
            anchors.fill: parent
            anchors.margins: windowMargin
            spacing: 0

            // Header - centered
            Label {
                text: "Authentication Required"
                color: activePalette.windowText
                font.bold: true
                font.pointSize: Math.round(fontMetrics.height * 1.2)
                Layout.alignment: Qt.AlignHCenter
            }

            // User info - centered, slightly muted
            Label {
                text: "for " + cleanUser
                color: disabledPalette.windowText
                font.pointSize: Math.round(fontMetrics.height * 0.9)
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: fontMetrics.height * 0.2
            }

            // Spacing after header
            Item { Layout.preferredHeight: fontMetrics.height * 1.0 }

            HSeparator {}

            // Spacing after separator
            Item { Layout.preferredHeight: fontMetrics.height * 0.8 }

            // Message text (without command)
            Label {
                text: messageText
                color: activePalette.windowText
                font.pointSize: Math.round(fontMetrics.height * 0.95)
                Layout.alignment: Qt.AlignHCenter
                Layout.maximumWidth: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            // Command box - only shown if there's a command
            Item {
                Layout.preferredHeight: fontMetrics.height * 0.5
                visible: commandText !== ""
            }

            Rectangle {
                id: commandBox
                visible: commandText !== ""
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: false
                Layout.preferredWidth: Math.min(commandLabel.implicitWidth + fontMetrics.height * 1.0, windowWidth - fontMetrics.height * 4)
                Layout.maximumWidth: windowWidth - fontMetrics.height * 4
                Layout.preferredHeight: commandLabel.implicitHeight + fontMetrics.height * 1.0
                Layout.minimumHeight: fontMetrics.height * 2

                // More visible background using base color (input field background)
                color: activePalette.base
                border.color: Qt.rgba(activePalette.windowText.r, activePalette.windowText.g, activePalette.windowText.b, 0.25)
                border.width: 1
                radius: 6

                Label {
                    id: commandLabel
                    anchors.fill: parent
                    anchors.margins: fontMetrics.height * 0.5
                    text: commandText
                    color: activePalette.windowText
                    font.family: "monospace"
                    font.pointSize: Math.round(fontMetrics.height * 0.9)
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.WrapAnywhere
                    elide: Text.ElideNone
                }
            }

            // Spacing before password
            Item { Layout.preferredHeight: fontMetrics.height * 1.0 }

            HSeparator {}

            // Spacing after separator
            Item { Layout.preferredHeight: fontMetrics.height * 0.8 }

            // Password field - uses system styling
            TextField {
                id: passwordField

                Layout.fillWidth: true
                Layout.leftMargin: fontMetrics.height * 3
                Layout.rightMargin: fontMetrics.height * 3
                placeholderText: "Password"
                horizontalAlignment: TextInput.AlignHCenter
                hoverEnabled: true
                persistentSelection: true
                echoMode: TextInput.Password
                focus: true

                Connections {
                    target: hpa
                    function onFocusField() {
                        passwordField.focus = true;
                    }
                    function onBlockInput(block) {
                        passwordField.readOnly = block;
                        if (!block) {
                            passwordField.focus = true;
                            passwordField.selectAll();
                        }
                    }
                }
            }

            // Error label
            Label {
                id: errorLabel
                color: activePalette.link
                font.italic: true
                font.pointSize: Math.round(fontMetrics.height * 0.85)
                text: ""
                visible: text !== ""
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: fontMetrics.height * 0.3

                Connections {
                    target: hpa
                    function onSetErrorString(e) {
                        errorLabel.text = e;
                    }
                }
            }

            // Flexible spacer - minimal, just for visual balance
            Item {
                Layout.preferredHeight: fontMetrics.height * 0.5
                Layout.fillHeight: true
                Layout.maximumHeight: fontMetrics.height * 2
            }

            HSeparator {}

            // Spacing before buttons
            Item { Layout.preferredHeight: fontMetrics.height * 0.8 }

            // Action buttons - centered
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: fontMetrics.height * 1.0

                Button {
                    text: "Cancel"
                    onClicked: {
                        hpa.setResult("fail");
                    }
                }

                Button {
                    text: "Authenticate"
                    highlighted: true
                    onClicked: {
                        hpa.setResult("auth:" + passwordField.text);
                    }
                }
            }
        }
    }

    // Separator using system palette
    component Separator: Rectangle {
        color: Qt.rgba(activePalette.windowText.r, activePalette.windowText.g, activePalette.windowText.b, 0.12)
    }

    component HSeparator: Separator {
        implicitHeight: 1
        Layout.fillWidth: true
        Layout.leftMargin: fontMetrics.height * 2
        Layout.rightMargin: fontMetrics.height * 2
    }
}
