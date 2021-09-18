#if !defined(HP9816_COMMON_H_INCLUDED_)
#define HP9816_COMMON_H_INCLUDED_
#endif

#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <malloc.h>
#include <memory.h>
#include <assert.h>
#include <EZ.h>

typedef unsigned short  WORD;
typedef short           SWORD;
typedef unsigned char   BYTE;
typedef unsigned int    DWORD;
typedef int             BOOL;
typedef char            TCHAR;
typedef char            CHAR;
typedef void            VOID;
typedef void           *LPVOID;
typedef unsigned char  *LPBYTE;
typedef char           *LPSTR;
typedef char           *LPTSTR;
typedef const char     *LPCSTR;
typedef const char     *LPCTSTR;
typedef unsigned int    UINT;
typedef int             INT;
typedef long            LONG;
typedef unsigned long   ULONG;
typedef int             LRESULT;
typedef struct {
  int left;
  int top;
  int right;
  int bottom;
} RECT;

#define MAX_PATH 256
#define FALSE 0
#define TRUE 1
#define _ASSERT(X) assert((X))
#define _T(X) X
#define __forceinline inline
#define min(X,Y) ((X)<=(Y)?(X):(Y))
#define wsprintf sprintf
#define OutputDebugString(X) fprintf(stderr,X)
