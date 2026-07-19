import QtQuick
import QtQuick.Controls
import ProjectO

Item {
    id: root

    property bool active: false

    readonly property bool canUse:
        active && AppController.hasPreview && !AppController.processing
    readonly property bool masked: AppController.showMaskedPreview

    function prevPage() {
        if (AppController.currentPage > 0)
            AppController.currentPage = AppController.currentPage - 1
    }

    function nextPage() {
        if (AppController.currentPage < AppController.pageCount - 1)
            AppController.currentPage = AppController.currentPage + 1
    }

    function firstPage() {
        if (AppController.pageCount > 0)
            AppController.currentPage = 0
    }

    function lastPage() {
        if (AppController.pageCount > 0)
            AppController.currentPage = AppController.pageCount - 1
    }

    function toggleStyle() {
        AppController.mosaicStyle = AppController.mosaicStyle === 0 ? 1 : 0
    }

    function deleteMark() {
        AppController.deleteSelectedMark()
    }

    // --- Fit view (edit + masked) ---
    Shortcut {
        sequence: "R"
        enabled: root.canUse
        context: Qt.ApplicationShortcut
        onActivated: AppController.resetPreviewView()
    }

    // --- Edit mode navigation ---
    Shortcut {
        sequences: ["Q", "Left", "PgUp"]
        enabled: root.canUse && !root.masked
        context: Qt.ApplicationShortcut
        onActivated: root.prevPage()
    }
    Shortcut {
        sequences: ["E", "Right", "PgDown"]
        enabled: root.canUse && !root.masked
        context: Qt.ApplicationShortcut
        onActivated: root.nextPage()
    }
    Shortcut {
        sequence: "Home"
        enabled: root.canUse && !root.masked
        context: Qt.ApplicationShortcut
        onActivated: root.firstPage()
    }
    Shortcut {
        sequence: "End"
        enabled: root.canUse && !root.masked
        context: Qt.ApplicationShortcut
        onActivated: root.lastPage()
    }

    // --- Edit mode tools ---
    Shortcut {
        sequence: "1"
        enabled: root.canUse && !root.masked
        context: Qt.ApplicationShortcut
        onActivated: AppController.toolMode = "draw"
    }
    Shortcut {
        sequence: "2"
        enabled: root.canUse && !root.masked
        context: Qt.ApplicationShortcut
        onActivated: AppController.toolMode = "select"
    }
    Shortcut {
        sequences: ["3", "Delete", "Backspace"]
        enabled: root.canUse && !root.masked
        context: Qt.ApplicationShortcut
        onActivated: root.deleteMark()
    }
    Shortcut {
        sequences: ["Return", "Enter"]
        enabled: root.canUse && !root.masked
        context: Qt.ApplicationShortcut
        onActivated: AppController.showMaskedPreview = true
    }
    Shortcut {
        sequence: "Tab"
        enabled: root.canUse && !root.masked
        context: Qt.ApplicationShortcut
        onActivated: root.toggleStyle()
    }

    // --- Masked preview mode ---
    Shortcut {
        sequence: "Tab"
        enabled: root.canUse && root.masked
        context: Qt.ApplicationShortcut
        onActivated: root.toggleStyle()
    }
    Shortcut {
        sequences: ["Return", "Enter"]
        enabled: root.canUse && root.masked
        context: Qt.ApplicationShortcut
        onActivated: AppController.showMaskedPreview = false
    }
    Shortcut {
        sequences: ["Delete", "Backspace"]
        enabled: root.canUse && root.masked
        context: Qt.ApplicationShortcut
        onActivated: root.deleteMark()
    }
    Shortcut {
        sequence: "Escape"
        enabled: root.canUse && root.masked
        context: Qt.ApplicationShortcut
        onActivated: AppController.showMaskedPreview = false
    }

    // --- Shared while preview loaded ---
    Shortcut {
        sequences: ["Q", "Left", "PgUp"]
        enabled: root.canUse && root.masked
        context: Qt.ApplicationShortcut
        onActivated: root.prevPage()
    }
    Shortcut {
        sequences: ["E", "Right", "PgDown"]
        enabled: root.canUse && root.masked
        context: Qt.ApplicationShortcut
        onActivated: root.nextPage()
    }
    Shortcut {
        sequence: "Home"
        enabled: root.canUse && root.masked
        context: Qt.ApplicationShortcut
        onActivated: root.firstPage()
    }
    Shortcut {
        sequence: "End"
        enabled: root.canUse && root.masked
        context: Qt.ApplicationShortcut
        onActivated: root.lastPage()
    }
}
