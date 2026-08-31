#pragma once

// Firmware identity. VERSION is hand-bumped; BUILD is the compile time
// (a git-hash pre-script can replace it later).
#define SFERIC_NAME "sferic"
#define SFERIC_VERSION "0.1.0"
#ifdef SFERIC_BUILD_GIT
#define SFERIC_BUILD SFERIC_BUILD_GIT
#else
#define SFERIC_BUILD (__DATE__ " " __TIME__)
#endif
