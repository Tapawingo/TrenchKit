/// @file Theme.h
/// @brief Application-wide color palette and layout spacing constants.
#ifndef THEME_H
#define THEME_H

#include <QString>

/// @brief Provides the global stylesheet and named constants for colors and spacing.
class Theme {
public:
    /// @brief Returns the full Qt stylesheet string applied to the application.
    static QString getStyleSheet();

    /// @brief CSS hex color strings used throughout the UI.
    struct Colors {
        static constexpr const char* BACKGROUND_PRIMARY = "#1e1e1e";
        static constexpr const char* BACKGROUND_SECONDARY = "#2c2c2c";
        static constexpr const char* BACKGROUND_TERTIARY = "#2d2d2d";
        static constexpr const char* BACKGROUND_HOVER = "#3a3a3a";
        static constexpr const char* BACKGROUND_DISABLED = "#3d3d3d";

        static constexpr const char* BORDER_PRIMARY = "#404040";
        static constexpr const char* BORDER_SECONDARY = "#3d3d3d";
        static constexpr const char* BORDER_HOVER = "#4d4d4d";

        static constexpr const char* ACCENT_BLUE = "#0e639c";
        static constexpr const char* ACCENT_BLUE_HOVER = "#1177bb";
        static constexpr const char* ACCENT_BLUE_PRESSED = "#0d5689";
        static constexpr const char* ACCENT_BLUE_LIGHT = "#1a8dd8";
        static constexpr const char* ACCENT_BLUE_DARK = "#0a4f7a";

        static constexpr const char* ACCENT_GREEN = "#4ec9b0";
        static constexpr const char* ACCENT_RED = "#e81123";
        static constexpr const char* ACCENT_RED_LIGHT = "#f48771";

        static constexpr const char* TEXT_PRIMARY = "#e1d0ab";
        static constexpr const char* TEXT_SECONDARY = "#e0e0e0";
        static constexpr const char* TEXT_TERTIARY = "#cccccc";
        static constexpr const char* TEXT_MUTED = "#888888";
        static constexpr const char* TEXT_DISABLED = "#808080";
    };

    /// @brief Pixel values for margins, padding, and spacing used in layouts.
    struct Spacing {
        static constexpr int CONTAINER_MARGIN = 0;
        static constexpr int CONTAINER_SPACING = 0;

        static constexpr int PANEL_MARGIN = 3;
        static constexpr int PANEL_SPACING = 0;

        static constexpr int MOD_ROW_PADDING_HORIZONTAL = 12;
        static constexpr int MOD_ROW_PADDING_VERTICAL = 8;
        static constexpr int MOD_ROW_INTERNAL_SPACING = 8;

        static constexpr int MOD_LIST_ITEM_SPACING = 0;
        static constexpr int MOD_LIST_TITLE_SPACING = 1;

        static constexpr int PROFILE_ROW_PADDING_HORIZONTAL = 8;
        static constexpr int PROFILE_ROW_PADDING_VERTICAL = 6;
        static constexpr int PROFILE_ROW_INTERNAL_SPACING = 8;
        static constexpr int PROFILE_LIST_ITEM_SPACING = 2;

        static constexpr int TITLE_BAR_MARGIN_LEFT = 9;
        static constexpr int TITLE_BAR_MARGIN_TOP = 8;
        static constexpr int TITLE_BAR_MARGIN_RIGHT = 9;
        static constexpr int TITLE_BAR_MARGIN_BOTTOM = 0;
        static constexpr int TITLE_BAR_ICON_SPACING = 12;

        static constexpr int RIGHT_PANEL_SECTION_SPACING = 8;

        static constexpr int LOG_ENTRY_PADDING_HORIZONTAL = 12;
        static constexpr int LOG_ENTRY_PADDING_VERTICAL = 8;
        static constexpr int LOG_LIST_ITEM_SPACING = 2;

        static constexpr int FORM_SPACING = 12;

        static constexpr int SETTINGS_OUTER_MARGIN = 8;
        static constexpr int SETTINGS_PANEL_MARGIN = 8;
        static constexpr int SETTINGS_CONTAINER_MARGIN = 16;
        static constexpr int SETTINGS_ROW_SPACING = 8;
        static constexpr int SETTINGS_SECTION_SPACING = 8;
    };
};

#endif // THEME_H
