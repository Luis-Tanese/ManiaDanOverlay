#ifndef MANIADANOVERLAY_SETTINGS_PANEL_H
#define MANIADANOVERLAY_SETTINGS_PANEL_H

#include <stdbool.h>

#include "raylib.h"
#include "settings/app_settings.h"
#include "ui/theme.h"

bool SettingsPanelDraw(
    AppSettings *settings,
    DanTheme theme,
    Vector2 mouse_position,
    float canvas_width,
    float canvas_height,
    float ui_scale
);

#endif
