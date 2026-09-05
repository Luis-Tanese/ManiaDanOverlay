#include "font.h"

#include <limits.h>
#include <stddef.h>

#include "embedded_fonts.h"

static Font g_regular;
static Font g_bold;
static bool g_loaded = false;

static bool embedded_size_fits_int(size_t size)
{
    return size > 0 && size <= (size_t)INT_MAX;
}

bool UiFontsLoad(void)
{
    if (g_loaded)
        return true;

    if (
        !embedded_size_fits_int(g_torus_regular_size) ||
        !embedded_size_fits_int(g_torus_bold_size)
    )
    {
        return false;
    }

    g_regular = LoadFontFromMemory(
        ".otf",
        g_torus_regular_data,
        (int)g_torus_regular_size,
        64,
        NULL,
        0
    );

    if (g_regular.texture.id == 0)
    {
        g_regular = (Font){0};
        return false;
    }

    g_bold = LoadFontFromMemory(
        ".ttf",
        g_torus_bold_data,
        (int)g_torus_bold_size,
        64,
        NULL,
        0
    );

    if (g_bold.texture.id == 0)
    {
        UnloadFont(g_regular);
        g_regular = (Font){0};
        g_bold = (Font){0};
        return false;
    }

    SetTextureFilter(
        g_regular.texture,
        TEXTURE_FILTER_BILINEAR
    );

    SetTextureFilter(
        g_bold.texture,
        TEXTURE_FILTER_BILINEAR
    );

    g_loaded = true;
    return true;
}

void UiFontsUnload(void)
{
    if (!g_loaded)
        return;

    UnloadFont(g_regular);
    UnloadFont(g_bold);

    g_regular = (Font){0};
    g_bold = (Font){0};
    g_loaded = false;
}

Font UiFontRegular(void)
{
    return g_regular;
}

Font UiFontBold(void)
{
    return g_bold;
}
