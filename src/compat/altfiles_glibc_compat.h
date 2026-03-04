/*
 * altfiles_glibc_compat.h
 *
 * Based on glibc 2.38.
 */

#ifndef ALTFILES_GLIBC_COMPAT_H
#define ALTFILES_GLIBC_COMPAT_H

/* -- Includes
 *
 * All headers needed by shims in this file are pulled in here so that
 * shims in translation units can just expect the headers they need exist.
 */

/* Standard system headers */
#include <errno.h>
#include <limits.h>
#include <nss.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <arpa/inet.h>

/* ============================================================
 * Module naming and NSS entry-point redirection
 * ============================================================
 *
 * These macros control the names under which NSS entry points are exported
 * from the altfiles shared library.  They are the primary customisation
 * point for downstream users who want a different module name.
 *
 * ALTFILES_DATADIR     -- directory the NSS module reads database files from.
 *                         Defaults to /lib/; override via -DALTFILES_DATADIR=...
 *                         at compile time.
 *                         NOTE: Path MUST end with `/`
 *
 * ALTFILES_MODULE_NAME -- the NSS module name as it appears in
 *                         /etc/nsswitch.conf and in exported symbol names.
 *                         For example, "altfiles" produces symbols such as
 *                         _nss_altfiles_getgrgid_r.
 *                         Override via -DALTFILES_MODULE_NAME=... at compile
 *                         time.
 */
#ifndef ALTFILES_DATADIR
#  define ALTFILES_DATADIR "/lib/"
#endif

#ifndef ALTFILES_MODULE_NAME
#  define ALTFILES_MODULE_NAME altfiles
#endif

/* Token-pasting helpers
 *
 * Two levels of indirection force macro expansion of ALTFILES_MODULE_NAME
 * before the ## operator is applied, per C99 6.10.3.1.
 *
 * ALTFILES_CONCAT1(suffix)          expands to _nss_<MODULE>_<suffix>
 * ALTFILES_CONCAT2(suffix1,suffix2) expands to _nss_<MODULE>_<suffix1><suffix2>
 */

/* Core token pasting */
#define __ALTFILES_SYMBOL1(n, a)    _nss_##n##_##a
#define __ALTFILES_SYMBOL2(n, a, b) _nss_##n##_##a##b

/* Force macro expansion first */
#define _ALTFILES_SYMBOL1(n, a)     __ALTFILES_SYMBOL1 (n, a)
#define _ALTFILES_SYMBOL2(n, a, b)  __ALTFILES_SYMBOL2 (n, a, b)

/* Useful macros */
#define ALTFILES_CONCAT1(a)         _ALTFILES_SYMBOL1 (ALTFILES_MODULE_NAME, a)
#define ALTFILES_CONCAT2(a, b)      _ALTFILES_SYMBOL2 (ALTFILES_MODULE_NAME, a, b)

/* Entry-point redirection
 *
 * The .c files copied from glibc define functions named _nss_files_*.
 * These macros rewrite those names to _nss_<MODULE>_* so that the resulting
 * shared library exports the correct NSS entry points without changes to
 * the upstream source.
 *
 * When adding a new glibc source file, add a corresponding redirect here
 * for each exported function it defines or the linker will hide it.
 */
#define _nss_files_gethostbyname3_r  ALTFILES_CONCAT1(gethostbyname3_r)
#define _nss_files_gethostbyname_r   ALTFILES_CONCAT1(gethostbyname_r)
#define _nss_files_gethostbyname2_r  ALTFILES_CONCAT1(gethostbyname2_r)
#define _nss_files_gethostbyname4_r  ALTFILES_CONCAT1(gethostbyname4_r)
#define _nss_files_initgroups_dyn    ALTFILES_CONCAT1(initgroups_dyn)

/* ============================================================
 * Compiler attributes
 * ============================================================
 *
 * glibc internal: include/sys/cdefs.h
 *                 include/libc-symbols.h
 *
 * attribute_hidden
 *   Marks a symbol with default visibility inside glibc so that it is not
 *   exported from the DSO.  Outside glibc the macro is not defined.  We
 *   define it here so vendored glibc headers compile without modification.
 *
 * attribute_unused
 *   Suppresses "defined but not used" warnings on functions or variables
 *   that are conditionally referenced.  Maps to __attribute__((unused)).
 *
 * __glibc_likely / __glibc_unlikely annotate boolean conditions with
 * expected branch outcomes so the compiler can lay out code optimally.
 *
 * __builtin_expect is a stable GCC/Clang extension available at our
 * baseline.
 *
 * Inside glibc, libc_hidden_proto(sym) emits a hidden-visibility extern
 * declaration for sym, and libc_hidden_def(sym) marks the definition.
 * They are used in pairs: proto in headers, def after the closing brace
 * of the function body.
 *
 * Outside glibc, hidden visibility is handled by the linker.
 */
#define attribute_hidden __attribute__ ((visibility ("hidden")))
#define attribute_unused __attribute__ ((unused))
#define __glibc_likely(cond)   __builtin_expect ((cond), 1)
#define __glibc_unlikely(cond) __builtin_expect ((cond), 0)
#define libc_hidden_proto(sym)
#define libc_hidden_def(sym)
#define weak_alias(sym, other)

/* ============================================================
 * String functions
 * ============================================================
 *
 * glibc internal: stdio-common/snprintf.c
 *                 string/strcasecmp.c
 *
 * __snprintf    maps to  snprintf    (POSIX.1-2001)
 * __strcasecmp  maps to  strcasecmp  (POSIX.1-2008)
 */
#define __snprintf snprintf
#define __strcasecmp strcasecmp

/* ============================================================
 * errno helpers
 * ============================================================
 *
 * glibc internal: sysdeps/generic/sysdep.h
 *
 * __set_errno is a glibc macro that assigns to errno and evaluates to void.
 * The public equivalent is a plain assignment.  Wrapped as a macro so the
 * glibc sources compile without modification.
 */
#define __set_errno(errval)  (errno = (errval))

/* ============================================================
 * Stdio functions
 * ============================================================
 *
 * glibc internal: various stdio internals
 *
 *   __ftello64        maps to  ftello          (POSIX.1-2008 + _GNU_SOURCE)
 *   __fgets_unlocked  maps to  fgets_unlocked  (POSIX.1-2008)
 *   __feof_unlocked   maps to  feof_unlocked   (POSIX.1-2008)
 *   __fseeko64        maps to  fseeko          (POSIX.1-2008 + _GNU_SOURCE)
 *   __getline         maps to  getline         (POSIX.1-2008)
 */
#define __ftello64(fp)               ftello (fp)
#define __fgets_unlocked(b, n, fp)   fgets_unlocked ((b), (n), (fp))
#define __feof_unlocked(fp)          feof_unlocked (fp)
#define __fseeko64(fp, off, whence)  fseeko ((fp), (off), (whence))
#define __getline(l, ln, s)          getline((l), (ln), (s))

/* ============================================================
 * Mutual exclusion (locking)
 * ============================================================
 *
 * glibc internal: sysdeps/generic/libc-lock.h
 *
 * glibc defines a family of __libc_lock_* macros wrapping its internal lock
 * type.  Outside glibc the closest equivalent is pthread_mutex_t.
 *
 * __libc_lock_define_initialized declares and zero-initialises a mutex.
 * CLASS is the storage class (e.g. "static"); it precedes the declaration
 * as in the glibc sources so the macro can be used at file scope.
 *
 * Return values from pthread_mutex_{lock,unlock} are intentionally ignored:
 * glibc's internal locking does not propagate lock-acquisition errors to
 * callers, and a default (non-recursive, non-error-check) mutex locked by
 * a thread that does not already hold it will not fail.
 *
 * __libc_lock_init initialises a mutex with default attributes, equivalent
 * to PTHREAD_MUTEX_INITIALIZER.  It is used in nss_files_data.c to
 * initialise per-element mutexes in a heap-allocated array.
 */
#define __libc_lock_define_initialized(CLASS, lockvar) \
    CLASS pthread_mutex_t lockvar = PTHREAD_MUTEX_INITIALIZER;

#define __libc_lock_init(lock)   pthread_mutex_init (&(lock), NULL)
#define __libc_lock_lock(lock)   pthread_mutex_lock (&(lock))
#define __libc_lock_unlock(lock) pthread_mutex_unlock (&(lock))

/* ============================================================
 * Network helpers
 * ============================================================
 *
 * glibc internal: resolv/inet_pton.c
 *
 * __inet_pton  maps to  inet_pton  (POSIX.1-2001)
 *
 * __inet_network is glibc's internal linkage name for inet_network(3)
 * (BSD/glibc extension, <arpa/inet.h>)
 */
#define __inet_pton inet_pton
#define __inet_network inet_network

/* ============================================================
 * Mask Internal bypasses
 * ============================================================
 *
 * glibc internal: nss/fgetgrent_r.c
 *                 nss/fgetpwent_r.c
 *
 * glibc has internal call sites which we are not using.
 * Here we simply mask them out to a noop.
 */
#define __nss_fgetent_r(s, r, b, l, p) (0)


/* ============================================================
 * failure helpers
 * ============================================================
 *
 * glibc internal: elf/dl-minimal.c
 *
 * __libc_fatal is a glibc macro that terminates the program.
 * The public equivalent is a print and abort.  Shimmed here so
 * glibc sources compile without modification.
 */
__attribute__((noreturn))
static inline void __libc_fatal(const char *message)
{
    if (message) {
        const char *p = message;
        while (*p)
            p++;
        write(STDERR_FILENO, message, (size_t)(p - message));
    }

    abort();
}

/* ============================================================
 * Function focused shims and linkage guards
 * ============================================================
 */
#include "altfiles_alloc_buffer.h"
#include "altfiles_scratch_buffer.h"
#include "altfiles_nss_files_data.h"

#endif /* ALTFILES_GLIBC_COMPAT_H */
