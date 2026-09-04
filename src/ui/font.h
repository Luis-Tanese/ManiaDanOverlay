#ifndef DANOVERLAY_FONT_H
#define DANOVERLAY_FONT_H

#include <stdbool.h>

#include "raylib.h"

bool UiFontsLoad(void);
void UiFontsUnload(void);

Font UiFontRegular(void);
Font UiFontBold(void);

#endif
