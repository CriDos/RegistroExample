pragma Singleton
import QtQuick

QtObject {
    readonly property int fontSizeXs: 11
    readonly property int fontSizeSm: 12
    readonly property int fontSizeMd: 13
    readonly property int fontSizeLg: 14
    readonly property int fontSizeXl: 15
    readonly property int fontSizeXxl: 16
    readonly property int fontSizeTitle: 18
    readonly property int fontSizeHero: 26

    readonly property color errorColor: "#d32f2f"
    readonly property color onError: "#ffffff"
    readonly property color statusActiveBg: "#e6f4ea"
    readonly property color statusArchivedBg: "#f1f3f4"
    readonly property color statusActiveFg: "#137333"
    readonly property color statusArchivedFg: "#5f6368"
    readonly property color tableHeaderBg: "#f1f3f5"
    readonly property color tableBorder: "#e2e8f0"
    readonly property color tableRowAlt: "#f8f9fa"
    readonly property color tableRowHover: "#edf2f7"
    readonly property color tableRowSelected: "#dbeafe"
    readonly property color textSecondary: "#64748b"
    readonly property color primaryColor: "#2563eb"

    readonly property color inputBg: "#ffffff"
    readonly property color inputText: "#1f2937"
    readonly property color inputSelectionText: "#ffffff"
    readonly property color scrollBarIdle: "#9aa6b0"
    readonly property color scrollBarHover: "#7b8794"
    readonly property color scrollBarPressed: "#4a5560"
    readonly property color scrollBarTrack: Qt.rgba(0, 0, 0, 0.05)
}
