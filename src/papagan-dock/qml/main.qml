import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12

Window {
    visible: true
    width: 800
    height: 80
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    Rectangle {
        anchors.fill: parent
        color: "#11111188"
        radius: 10
        Row {
            anchors.centerIn: parent
            spacing: 12
            Repeater {
                model: [{name: "firefox", icon: "firefox"}, {name: "konsole", icon: "terminal"}, {name: "dolphin", icon: "dolphin"}]
                delegate: Item {
                    width: 64; height: 64
                    Rectangle {
                        anchors.fill: parent
                        radius: 8
                        color: "white"
                        opacity: 0.12
                        transform: Scale { origin.x: width/2; origin.y: height/2; xScale: 1.0; yScale: 1.0 }
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            // Launch command: placeholder, to be implemented via C++ bridge
                            console.log("launch", model.name)
                        }
                    }
                }
            }
        }
    }
}
