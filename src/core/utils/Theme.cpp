#include "Theme.h"

QString Theme::getStyleSheet() {
    return R"(
        /* ===== MAIN WINDOW ===== */
        QWidget#centralwidget {
            background-color: #1e1e1e;
        }

        /* ===== PANELS ===== */
        #leftPanel, #middlePanel, #rightPanel {
            margin: 8px;
            border: 1px solid #171717;
            border-radius: 3px;
        }

        #middlePanel {
            margin-left: 0px;
            margin-right: 0px;
        }

        GradientFrame {
            margin: 0px;
        }

        /* ===== SETTINGS ===== */
        #settingsPanel {
            margin: 0px;
        }
        #settingsContainer {
            margin: 0px;
        }
        #settingsTitle {
            color: #e1d0ab;
            font-size: 24px;
            font-weight: bold;
            margin-bottom: 8px;
        }
        #settingsSectionHeader {
            color: #e1d0ab;
            font-size: 16px;
            font-weight: bold;
            margin-top: 12px;
            margin-bottom: 4px;
        }
        #settingsLabel {
            color: #e1d0ab;
            font-size: 13px;
        }
        #settingsStatus {
            color: #9e9278;
            font-size: 12px;
        }
        #settingsPanel QLabel {
            margin: 0px;
            padding: 0px;
        }
        #settingsPanel QLineEdit,
        #settingsPanel QComboBox,
        #settingsPanel QCheckBox {
            margin: 0px;
        }

        /* ===== TITLE BAR ===== */
        #titleBar {
            background-color: #2c2c2c;
        }
        #minimizeButton, #maximizeButton, #closeButton {
            background-color: transparent;
            color: #e1d0ab;
            border: none;
            border-left: 1px solid rgba(225, 209, 173, 0.25);
            font-size: 28px;
            font-weight: 300;
            padding: 0px;
            text-align: center;
        }
        #maximizeButton {
            font-size: 18px;
        }
        #settingsButton, #updateButton {
            background-color: transparent;
            border: none;
            border-left: 1px solid rgba(225, 209, 173, 0.25);
            padding: 0px;
        }
        #minimizeButton:hover, #maximizeButton:hover, #settingsButton:hover, #updateButton:hover {
            background-color: #404040;
        }
        #closeButton:hover {
            background-color: #e81123;
        }

        /* ===== SECTION TITLES ===== */
        #installPathTitle,
        #profileTitle,
        QLabel[objectName="sectionTitle"] {
            color: #e1d0ab;
            font-size: 18px;
            font-weight: bold;
            margin-bottom: 4px;
        }

        /* ===== INSTALL PATH WIDGET ===== */
        #installPathInput {
            background-color: #16130e;
            color: #e1d0ab;
            border: 1px solid #404040;
            padding: 6px;
        }
        #installPathInput:focus {
            border-color: #3d402e;
        }
        #installPathBrowse {
            background-color: #3d402e;
            color: #e1d0ab;
            border: none;
            padding: 6px 12px;
        }
        #installPathBrowse:hover {
            background-color: #373A29;
        }
        #installPathBrowse:pressed {
            background-color: #313325;
        }
        #installPathStatus {
            color: #cccccc;
            font-size: 12px;
            margin-top: 4px;
        }
        #installPathStatus[status="valid"] {
            color: #4ec9b0;
        }
        #installPathStatus[status="invalid"] {
            color: #f48771;
        }
        #installPathStatus[status="loading"] {
            color: #cccccc;
        }

        /* ===== PROFILE MANAGER ===== */
        #profileList {
            background-color: #16130e;
            color: #e1d0ab;
            border: 1px solid #404040;
            padding: 4px;
        }
        #profileList::item {
            padding: 0px;
            background-color: transparent;
        }
        #profileList::item:selected {
            background-color: transparent;
        }
        #profileList::item:hover {
            background-color: transparent;
        }
        #profileButton {
            background-color: #3d402e;
            color: #e1d0ab;
            border: none;
            padding: 6px 12px;
            font-size: 12px;
        }
        #profileButton:hover {
            background-color: #373A29;
        }
        #profileButton:pressed {
            background-color: #313325;
        }
        #profileButton:disabled {
            background-color: #646658;
            color: #808080;
        }
        #importIconButton {
            background-color: #3d402e;
            border: none;
            padding: 4px;
        }
        #importIconButton:hover {
            background-color: #373A29;
        }
        #importIconButton:pressed {
            background-color: #313325;
        }

        /* ===== PROFILE ROW WIDGET ===== */
        ProfileRowWidget {
            background-color: #2c2c2c;
            min-height: 32px;
            margin-bottom: 3px;
        }
        ProfileRowWidget[selected="true"] {
            background-color: #716856;
        }
        ProfileRowWidget:hover {
            background-color: #716856;
        }
        ProfileRowWidget[selected="true"]:hover {
            background-color: #716856;
        }
        #profileNameLabel {
            color: #e1d0ab;
            font-size: 13px;
        }
        #profileNameLabel[active="true"] {
            color: #4ec9b0;
        }

        /* ===== MOD LIST ===== */
        QListWidget {
            background-color: transparent;
            border: none;
            outline: none;
        }
        QListWidget::item {
            background-color: transparent;
            border: none;
            padding: 0px;
        }
        QListWidget::item:selected {
            background-color: transparent;
            border: none;
        }
        QListWidget::item:hover {
            background-color: transparent;
        }

        /* ===== MOD LIST WIDGET ===== */
        #modLoadingLabel {
            color: #888888;
            font-size: 14px;
            font-style: italic;
            padding: 20px;
        }
        #modListTitle {
            color: #e1d0ab;
            font-size: 18px;
            font-weight: bold;
            margin-bottom: 4px;
        }
        #modCountLabel {
            color: #9e9278;
            font-size: 14px;
            margin-bottom: 4px;
        }

        /* ===== MOD ROW WIDGET ===== */
        ModRowWidget {
            background-color: transparent;
            border-bottom: 1px solid #000000;
            margin: 0px;
        }
        ModRowWidget[selected="true"] {
            background-color: rgba(225, 209, 173, 0.25);
        }
        ModRowWidget:hover {
            background-color: rgba(225, 209, 173, 0.25);
        }
        ModRowWidget[selected="true"]:hover {
            background-color: rgba(225, 209, 173, 0.25);
        }
        #modNameLabel {
            font-size: 14px;
            font-weight: bold;
            color: #e1d0ab;
        }
        #modDateLabel {
            font-size: 11px;
            color: #888888;
            margin-left: 28px;
        }
        QPushButton#modUpdateButton {
            background: transparent;
            border: none;
            padding: 4px;
        }
        QPushButton#modUpdateButton:hover {
            background: rgba(61, 64, 46, 0.6);
            border-radius: 4px;
        }
        QPushButton#modConflictButton {
            background: transparent;
            border: none;
            padding: 4px;
        }
        QPushButton#modConflictButton:hover {
            background: rgba(232, 17, 35, 0.1);
            border-radius: 4px;
        }
        QPushButton#modNoticeButton {
            background: transparent;
            border: none;
            padding: 4px;
        }
        QPushButton#modNoticeButton:hover {
            background: rgba(61, 64, 46, 0.6);
            border-radius: 4px;
        }
        QPushButton#modDependencyButton {
            background: transparent;
            border: none;
            padding: 4px;
        }
        QPushButton#modDependencyButton:hover {
            background: rgba(61, 64, 46, 0.6);
            border-radius: 4px;
        }
        #conflictTooltipLabel {
            color: #e1d0ab;
            font-size: 12px;
            padding: 6px 8px;
        }

        /* ===== RIGHT PANEL WIDGET ===== */
        #modActionsTitle, #launchTitle, #backupTitle, #activityLogTitle {
            color: #e1d0ab;
            font-size: 18px;
            font-weight: bold;
            margin-bottom: 8px;
        }

        /* ===== BUTTONS (RIGHT PANEL) ===== */
        QPushButton {
            background-color: #3d402e;
            color: #e1d0ab;
            border: none;
            padding: 8px 16px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #373A29;
        }
        QPushButton:pressed {
            background-color: #313325;
        }
        QPushButton:disabled {
            background-color: #646658;
            color: #888888;
        }

        /* ===== TOOL BUTTON (LAUNCH BUTTON) ===== */
        QToolButton {
            background-color: #3d402e;
            color: #e1d0ab;
            border: none;
            padding: 12px 20px;
            font-weight: bold;
            font-size: 13px;
        }
        QToolButton:hover {
            background-color: #373A29;
        }
        QToolButton:pressed {
            background-color: #313325;
        }
        QToolButton::menu-button {
            background-color: #3d402e;
            border: none;
            border-left: 1px solid #2c2c2c;
            width: 22px;
        }
        QToolButton::menu-button:hover {
            background-color: #373A29;
        }
        QToolButton::menu-button:pressed {
            background-color: #313325;
        }
        QToolButton::menu-arrow {
            image: url(:/icon_down.png);
            width: 14px;
            height: 14px;
        }
        QToolButton::menu-indicator {
            subcontrol-position: right center;
            subcontrol-origin: content;
            width: 8px;
            height: 8px;
            right: 6px;
        }

        /* ===== SEPARATORS ===== */
        QFrame[frameShape="4"][frameShadow="16"] {
            background-color: #141413;
            height: 2px;
        }

        #actionsSeparator {
            background-color: #141413;
            height: 2px;
        }

        /* ===== CHECKBOXES ===== */
        QCheckBox {
            color: #e1d0ab;
            spacing: 6px;
        }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border: 1px solid #e1d0ab;
            border-radius: 1px;
            background-color: #171511;
        }
        QCheckBox::indicator:hover {
            background-color: #716856;
        }
        QCheckBox::indicator:checked {
            image: url(:/icon_checkmark.png);
        }

        /* ===== LABELS ===== */
        QLabel {
            color: #e1d0ab;
        }

        /* ===== DIALOGS ===== */
        QDialog {
            background-color: #2c2c2c;
            color: #e1d0ab;
        }

        /* ===== LINE EDITS ===== */
        QLineEdit {
            background-color: #2c2c2c;
            color: #e1d0ab;
            border: 1px solid #404040;
            padding: 6px;
        }
        QLineEdit:focus {
            border-color: #3d402e;
        }

        /* ===== TEXT EDITS ===== */
        QTextEdit {
            background-color: #2c2c2c;
            color: #e1d0ab;
            border: 1px solid #404040;
            padding: 6px;
        }
        QTextEdit:focus {
            border-color: #3d402e;
        }

        /* ===== MESSAGE BOXES ===== */
        QMessageBox {
            background-color: #2c2c2c;
        }
        QMessageBox QLabel {
            color: #e1d0ab;
        }

        /* ===== MENUS ===== */
        QMenu {
            background-color: #2c2c2c;
            color: #e1d0ab;
            border: 1px solid #404040;
            padding: 4px;
        }
        QMenu::item {
            padding: 6px 20px;
        }
        QMenu::item:selected {
            background-color: #3d402e;
        }
        QMenu::separator {
            height: 1px;
            background-color: #404040;
            margin: 4px 0px;
        }

        /* ===== SCROLL BARS ===== */
        QScrollBar:vertical {
            background-color: #2c2c2c;
            width: 6px;
            border-radius: 2px;
        }
        QScrollBar::handle:vertical {
            background-color: #e1d0ab;
            min-height: 20px;
        }
        QScrollBar::handle:vertical:hover {
            background-color: #9e9278;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar:horizontal {
            background-color: #2c2c2c;
            height: 6px;
            border-radius: 2px;
        }
        QScrollBar::handle:horizontal {
            background-color: #e1d0ab;
            min-width: 20px;
        }
        QScrollBar::handle:horizontal:hover {
            background-color: #9e9278;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
        }

        /* ===== ACTIVITY LOG ===== */
        LogEntryWidget {
            border-radius: 2px;
            border-width: 1px;
            border-style: solid;
        }

        LogEntryWidget[level="info"] {
            background-color: qlineargradient(x1: 0, y1: 0.5, x2: 1, y2: 0.5, stop: 0 #2a2a2a, stop: 0.6 #00000000);
        }

        LogEntryWidget[level="success"] {
            background-color: qlineargradient(x1: 0, y1: 0.5, x2: 1, y2: 0.5, stop: 0 #1a4d3f, stop: 0.6 #00000000);
        }

        LogEntryWidget[level="warning"] {
            background-color: qlineargradient(x1: 0, y1: 0.5, x2: 1, y2: 0.5, stop: 0 #4d2f1a, stop: 0.6 #00000000);
        }

        LogEntryWidget[level="error"] {
            background-color: qlineargradient(x1: 0, y1: 0.5, x2: 1, y2: 0.5, stop: 0 #4d1a1f, stop: 0.6 #00000000);
        }

        LogEntryWidget QLabel {
            color: #e1d0ab;
        }
    )";
}
