#ifndef WP_WPSTDINCLUDE_H
#define WP_WPSTDINCLUDE_H

#if WIN32
#ifndef WINVER
#define WINVER 0x0501
#endif
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef _WIN32
// ---------------------------------------------------------------------------
// Portability layer for non-Windows builds (macOS): the code base uses the Win32
// CRITICAL_SECTION API for locking. A Win32 critical section is recursive, so it
// is mapped onto std::recursive_mutex with the same call names.
// ---------------------------------------------------------------------------
#include <mutex>

struct CRITICAL_SECTION {
	std::recursive_mutex mutex;
};

inline void InitializeCriticalSection(CRITICAL_SECTION *) {}
inline int InitializeCriticalSectionAndSpinCount(CRITICAL_SECTION *, unsigned long) {return 1;}
inline void DeleteCriticalSection(CRITICAL_SECTION *) {}
inline void EnterCriticalSection(CRITICAL_SECTION *cs) {cs->mutex.lock();}
inline void LeaveCriticalSection(CRITICAL_SECTION *cs) {cs->mutex.unlock();}
#endif

#endif
