#ifndef COMPAT_WINDOWS_H
#define COMPAT_WINDOWS_H

#include <stdint.h>
#include <unistd.h>
#include <wchar.h>

typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef int BOOL;
typedef void *HANDLE;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define STD_OUTPUT_HANDLE ((DWORD)-11)

#define FOREGROUND_BLUE 0x0001
#define FOREGROUND_GREEN 0x0002
#define FOREGROUND_RED 0x0004
#define FOREGROUND_INTENSITY 0x0008
#define BACKGROUND_BLUE 0x0010
#define BACKGROUND_GREEN 0x0020
#define BACKGROUND_RED 0x0040
#define BACKGROUND_INTENSITY 0x0080

typedef struct _COORD
{
    short X;
    short Y;
} COORD;

typedef struct _CONSOLE_FONT_INFOEX
{
    unsigned long cbSize;
    unsigned long nFont;
    COORD dwFontSize;
    unsigned int FontFamily;
    unsigned int FontWeight;
    wchar_t FaceName[32];
} CONSOLE_FONT_INFOEX;

static inline HANDLE GetStdHandle(DWORD nStdHandle)
{
    (void)nStdHandle;
    return (HANDLE)0;
}

static inline BOOL SetConsoleTextAttribute(HANDLE hConsoleOutput, WORD wAttributes)
{
    (void)hConsoleOutput;
    (void)wAttributes;
    return TRUE;
}

static inline BOOL GetCurrentConsoleFontEx(HANDLE hConsoleOutput, BOOL bMaximumWindow, CONSOLE_FONT_INFOEX *lpConsoleCurrentFontEx)
{
    (void)hConsoleOutput;
    (void)bMaximumWindow;
    (void)lpConsoleCurrentFontEx;
    return FALSE;
}

static inline BOOL SetCurrentConsoleFontEx(HANDLE hConsoleOutput, BOOL bMaximumWindow, CONSOLE_FONT_INFOEX *lpConsoleCurrentFontEx)
{
    (void)hConsoleOutput;
    (void)bMaximumWindow;
    (void)lpConsoleCurrentFontEx;
    return FALSE;
}

static inline BOOL SetConsoleOutputCP(unsigned int wCodePageID)
{
    (void)wCodePageID;
    return TRUE;
}

static inline BOOL SetConsoleCP(unsigned int wCodePageID)
{
    (void)wCodePageID;
    return TRUE;
}

static inline void Sleep(DWORD dwMilliseconds)
{
    usleep((useconds_t)dwMilliseconds * 1000U);
}

#endif /* COMPAT_WINDOWS_H */
