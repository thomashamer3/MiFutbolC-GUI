#ifndef COMPAT_DIRECT_H
#define COMPAT_DIRECT_H

#ifdef _WIN32
int _mkdir(const char *path);
#else
#include <sys/stat.h>
#include <unistd.h>

static inline int _mkdir(const char *path)
{
    return mkdir(path, 0755);
}

#endif

#endif
