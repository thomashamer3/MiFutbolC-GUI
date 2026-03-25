#ifndef COMPAT_SHLOBJ_H
#define COMPAT_SHLOBJ_H

#ifdef _WIN32
typedef int HRESULT;
#ifndef S_OK
#define S_OK 0
#endif
#ifndef MAX_PATH
#define MAX_PATH 260
#endif
#define CSIDL_PERSONAL 0x0005
#define CSIDL_LOCAL_APPDATA 0x001c
int SHGetFolderPathA(void *hwndOwner, int nFolder, void *hToken, unsigned long dwFlags, char *pszPath);
#else

typedef int HRESULT;

#ifndef S_OK
#define S_OK 0
#endif

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif

#define CSIDL_PERSONAL 0x0005
#define CSIDL_LOCAL_APPDATA 0x001c

static inline int SHGetFolderPathA(void *hwndOwner, int nFolder, void *hToken, unsigned long dwFlags, char *pszPath)
{
    (void)hwndOwner;
    (void)nFolder;
    (void)hToken;
    (void)dwFlags;
    if (pszPath)
    {
        pszPath[0] = '\0';
    }
    return S_OK;
}

#endif /* !_WIN32 */

#endif /* COMPAT_SHLOBJ_H */
