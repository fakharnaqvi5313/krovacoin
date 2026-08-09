# Windows 7 baseline API level. Without this, mingw-w64's headers leave
# Vista+/Win7+ declarations (e.g. iphlpapi.h's GetTcp6Table/TCP_ESTATS_TYPE,
# which libevent's evutil.c needs) undeclared, and the build fails with
# "unknown type name" deep inside a third-party dependency rather than
# anywhere obviously related to this setting.
mingw32_CPPFLAGS=-D_WIN32_WINNT=0x0601 -DWINVER=0x0601

# GCC 14+ (as shipped by current Homebrew mingw-w64) defaults to stricter
# C23-aligned semantics for empty-parameter-list function declarations
# (`void g(){}` now means truly zero args, not K&R's old "unspecified args").
# GMP 6.2.1's own configure test programs rely on the old lenient behavior
# and fail to compile as "too many arguments to function" under the new
# default -- pin an older C dialect so vintage depends packages compiled
# against this newer toolchain don't fail on default-semantics drift that
# has nothing to do with this project's own code.
mingw32_CFLAGS=-pipe -std=gnu17
mingw32_CXXFLAGS=-pipe

mingw32_release_CFLAGS=-O2
mingw32_release_CXXFLAGS=$(mingw32_release_CFLAGS)

mingw32_debug_CFLAGS=-O1
mingw32_debug_CXXFLAGS=$(mingw32_debug_CFLAGS)

mingw32_debug_CPPFLAGS=-D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC
