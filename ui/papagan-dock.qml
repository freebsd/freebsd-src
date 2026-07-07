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
                model: ["firefox", "konsole", "dolphin"]
                delegate: Item {
                    width: 64; height: 64
                    Rectangle {
                        anchors.fill: parent
                        radius: 8
                        color: "white"
                        opacity: 0.12
                        scale: mouseArea.pressed ? 0.9 : 1.0
                        transform: Scale { origin.x: width/2; origin.y: height/2; xScale: 1.0; yScale: 1.0 }
                    }
                    MouseArea {
                        id: mouseArea
                        anchors.fill: parent
                        onClicked: { console.log("launch", modelData) /* later: call exec */ }
                    }
                }
            }
        }
    }
}
