// ----------------------------------------------------------------------------
// WavPack lib for Matroska
// ----------------------------------------------------------------------------
// Copyright christophe.paris@free.fr
// Parts by David Bryant http://www.wavpack.com
// Distributed under the BSD Software License
// ----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
#ifndef WAVPACK_COMMON_H_
#define WAVPACK_COMMON_H_
//-----------------------------------------------------------------------------

#define constrain(x,y,z) (((y) < (x)) ? (x) : ((y) > (z)) ? (z) : (y))

//-----------------------------------------------------------------------------

#if defined(_WIN32)

// ----- WIN32 ----->
#include <windows.h>
#include <mmreg.h>
#define wp_alloc(__length) GlobalAlloc(GMEM_ZEROINIT, __length)
#define wp_realloc(__mem,__length) GlobalReAlloc(__mem,__length,GMEM_MOVEABLE)
#define wp_free(__dest) GlobalFree(__dest)
#define wp_memcpy(__buff1,__buff2,__length) CopyMemory(__buff1,__buff2,__length)
#define wp_memclear(__dest,__length) ZeroMemory(__dest,__length)
#define wp_memcmp(__buff1,__buff2,__length) memcmp(__buff1,__buff2,__length)

typedef unsigned __int64 uint64;
typedef unsigned char uchar;

#ifdef _DEBUG
void DebugLog(const char *pFormat,...);
#else
#define DebugLog
#endif
// <----- WIN32 -----

#else

    // Add your os definition here
    
#endif

//-----------------------------------------------------------------------------
#endif WAVPACK_COMMON_H_
//-----------------------------------------------------------------------------
