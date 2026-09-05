#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

int main(void);

int WINAPI WinMain(
    HINSTANCE instance,
    HINSTANCE previous_instance,
    LPSTR command_line,
    int show_command
)
{
    (void)instance;
    (void)previous_instance;
    (void)command_line;
    (void)show_command;

    return main();
}
#endif
