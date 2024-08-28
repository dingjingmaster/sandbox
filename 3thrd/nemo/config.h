#pragma once
// config.h.in.  Generated from configure.ac by autoheader.

// Define to the version of this package.
#define VERSION "6.0.2"

// Enable debug code
#define ENABLE_DEBUG

// ***** Enable features

// define to enable the empty view that is used for performance measurement
#define ENABLE_EMPTY_VIEW 0
// Define to enable xmp support
//#define HAVE_EXEMPI
// Define to enable EXIF support
//#define HAVE_EXIF
// Define if libselinux is available
#undef HAVE_SELINUX
// Define to enable pango-1.44 fixes
//#define HAVE_PANGO_144



// ***** Localisation

// always defined to indicate that i18n is enabled
#define ENABLE_NLS
#define HAVE_GETTEXT

// the gettext translation domain
#define GETTEXT_PACKAGE "nemo"

// Define to 1 if you have the <locale.h> header file.
#define HAVE_LOCALE_H

// path for translations
#define LOCALEDIR "/usr/local/share/locale"
#define LOCALE_DIR "/usr/local/share/locale"

// Define to 1 if you have the <malloc.h> header file.
#define HAVE_MALLOC_H 1

// Define to 1 if you have the `mallopt' function.
#define HAVE_MALLOPT 1


// Define to 1 if you have the <sys/mount.h> header file.
#define HAVE_SYS_MOUNT_H 1

// Define to 1 if you have the <sys/param.h> header file.
#define HAVE_SYS_PARAM_H 1

// Define to 1 if you have the <sys/vfs.h> header file.
#define HAVE_SYS_VFS_H 1

// Define to 1 if you have the <X11/XF86keysym.h> header file.
#define HAVE_X11_XF86KEYSYM_H 1

#undef ENABLE_TRACKER
