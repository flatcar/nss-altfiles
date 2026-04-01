/*
 * altfiles_nss_files_data.h
 *
 * Based on glibc 2.38.
 */

#ifndef ALTFILES_NSS_FILES_DATA_H
#define ALTFILES_NSS_FILES_DATA_H

/* ============================================================
 * nss_files stream and locking state  (glibc >= 2.29)
 * ============================================================
 *
 * glibc source: nss/nss_files/nss_files.h
 *               nss/nss_files/nss_files_data.c
 *
 * -- What glibc does
 *
 * Starting with glibc 2.29, nss_files is a single shared library covering
 * all NSS databases (passwd, group, shadow, hosts, ...).  It manages one
 * { FILE *stream, mutex } pair per database in a global heap-allocated
 * array (struct nss_files_data) indexed by enum nss_files_file.  The array
 * is initialised once via allocate_once.  Four internal functions provide
 * the interface:
 *
 *   __nss_files_data_setent  (re)open or rewind the per-database stream
 *   __nss_files_data_endent  close the per-database stream
 *   __nss_files_data_open    lock + ensure stream is open; return pointer
 *   __nss_files_data_put     unlock (must pair with every successful _open)
 *
 * -- What this shim does differently, and why
 *
 * altfiles also compiles multiple databases into one shared library, so
 * the same isolation requirement applies: passwd enumeration must not
 * interfere with a concurrent group enumeration.
 *
 * We achieve isolation without the global array by making every
 * __nss_files_data_* function static inline.  Each database is compiled as
 * a separate translation unit (nss_passwd.c includes files-XXX.c,
 * nss_group.c includes files-XXX.c, etc.), so the compiler gives each TU
 * its own independent copy of these functions and its own independent
 * static-local struct nss_files_per_file_data.  The result is exactly one
 * { stream, mutex } pair per database with no index needed.
 *
 *      WARNING: this isolation is critical to our functionality!!
 *
 * If two database source files are ever merged into a single translation
 * unit (a single compile line), the static locals become shared and the
 * databases will corrupt each other's stream state under concurrent access.
 * Do not do this without redesigning the state management.
 *
 * -- enum nss_files_file
 *
 * files-XXX.c passes CONCAT(nss_file_, ENTNAME) as the first argument to
 * each __nss_files_data_* function (e.g. nss_file_passwd).  The shim
 * functions ignore this argument entirely; the enum type is declared solely
 * so those identifiers resolve to something valid and the calls compile
 * without modification.  Individual enumerator values are required evaluated.
 *
 * -- herrnop
 *
 * glibc's __nss_files_data_open sets *herrnop = NETDB_INTERNAL on
 * allocation failure.  In the shim there is no allocation; the mutex and
 * stream pointer live in a static local that is always present.  The only
 * failure path is __nss_files_fopen, which is a file-open error unrelated
 * to network name resolution.  Writing herrnop on that path would be
 * misleading, so the shim accepts the parameter for call-site compatibility
 * but never writes it.
 *
 * -- Thread safety
 *
 * The pthread_mutex_t in each per-database struct is a default
 * (non-recursive, non-error-check) mutex.  It guards the persistent stream
 * pointer across both the setent/endent path (which rewinds or closes the
 * stream) and the getent_r path (__nss_files_data_open holds the lock
 * across the entire internal_getent call; __nss_files_data_put releases
 * it).  This matches glibc's per-file locking behaviour.  The lock is
 * never acquired recursively on any reachable call path.
 */

/*
 * fseterr_unlocked is a glibc-internal function that sets the error
 * indicator on a FILE without acquiring its lock.  There is no public
 * equivalent.  It is called by __nss_readline_seek only on the
 * unrecoverable ESPIPE path, immediately before that function returns
 * ESPIPE.  The caller (internal_getent) then returns NSS_STATUS_UNAVAIL
 * and the stream is never used again.  A no-op shim is correct: subsequent
 * reads on an unseekable stream will produce their own errors independently.
 */
#define fseterr_unlocked(fp)         ((void) 0)

/* nss_readline.c */
static
int
__nss_readline (FILE *fp, char *buf, size_t len, off64_t *poffset)
__attribute__((unused));
static
int
__nss_readline_seek (FILE *fp, off64_t offset)
__attribute__((unused));
#include <nss_readline.c>

/* nss_files_fopen.c */
static
FILE * __nss_files_fopen (const char *path)
__attribute__((unused));
#include <nss_files_fopen.c>

/* nss_parse_line_result.c */
static
int
__nss_parse_line_result (FILE *fp, off64_t offset, int parse_line_result)
__attribute__((unused));
#include <nss_parse_line_result.c>

/* struct nss_files_per_file_data
 *
 * Based on: include/nss_files.h
 *
 * One instance lives as a static local inside __nss_files_data_get in each
 * database translation unit.  Do not allocate this on the heap or pass it
 * across translation units.
 */
struct nss_files_per_file_data
{
  FILE           *stream;
  pthread_mutex_t lock;
};

/* enum nss_files_file
 *
 * Based on include/nss_files.h
 *
 * This is used as the internal index for the struct safety
 * of __nss_files_data_get.
 * We are bypassing all this with static translation units.
 *
 * It is easier to define and ignore this than strip it out.
 */
enum nss_files_file
  {
    nss_file_aliasent,
    nss_file_etherent,
    nss_file_grent,
    nss_file_hostent,
    nss_file_netent,
    nss_file_protoent,
    nss_file_pwent,
    nss_file_rpcent,
    nss_file_servent,
    nss_file_sgent,
    nss_file_spent,

    nss_file_count
  };


/* __nss_files_data_get -- return a pointer to the TU-local state struct.
 *
 * PTHREAD_MUTEX_INITIALIZER is required by POSIX for static-storage-duration
 * mutexes; it is not sufficient to rely on zero-initialisation even though
 * they are equivalent on all supported platforms.
 *
 * Do not change this to extern linkage or move the definition to a shared
 * .c file.  Doing so would collapse all databases onto a single stream
 * pointer, causing concurrent lookups (e.g. gethostbyname + getpwnam) to
 * race on the same FILE *.
 *
 * Each translation unit that includes this header gets its own instance of
 * the function and its own internal instance of nss_files_per_file_data.
 * The linker never merges them.  Each database (hosts, passwd, group, ...)
 * therefore owns an independent stream and mutex.
 *
 * If these bits are unused by the TU, they will be dropped by gcc/ld.
 *
 * Not exposed directly; callers use the _open/_put/_setent/_endent
 * interface below.
 */
static inline struct nss_files_per_file_data *
__nss_files_data_get (void)
{
  static struct nss_files_per_file_data data = {
    .stream = NULL,
    .lock   = PTHREAD_MUTEX_INITIALIZER,
  };
  return &data;
}

/* __nss_files_data_setent -- (re)open or rewind the per-database stream.
 *
 * Based on: nss/nss_files_data.c
 *
 * FILE is accepted for call-site compatibility and ignored.
 *
 * The rewind branch is reachable: callers such as sshd, nscd, and systemd
 * may call set<ENTNAME> while a previous enumeration is still open.
 * Rewinding rather than closing and reopening matches glibc's behaviour.
 */
static inline enum nss_status
__nss_files_data_setent (enum nss_files_file file, const char *path)
{
  (void) file;
  struct nss_files_per_file_data *data = __nss_files_data_get ();
  enum nss_status status = NSS_STATUS_SUCCESS;

  pthread_mutex_lock (&data->lock);

  if (data->stream == NULL)
    {
      data->stream = __nss_files_fopen (path);
      if (data->stream == NULL)
        {
          if (errno == EAGAIN)
            status = NSS_STATUS_TRYAGAIN;
          else
            status = NSS_STATUS_UNAVAIL;
        }
    }
  else
    rewind (data->stream);

  pthread_mutex_unlock (&data->lock);
  return status;
}

/* __nss_files_data_endent -- close the per-database stream.
 *
 * Based on: nss/nss_files_data.c
 *
 * FILE is accepted for call-site compatibility and ignored.
 * Safe to call when the stream is already NULL (idempotent).
 */
static inline enum nss_status
__nss_files_data_endent (enum nss_files_file file)
{
  (void) file;
  struct nss_files_per_file_data *data = __nss_files_data_get ();

  pthread_mutex_lock (&data->lock);

  if (data->stream != NULL)
    {
      fclose (data->stream);
      data->stream = NULL;
    }

  pthread_mutex_unlock (&data->lock);
  return NSS_STATUS_SUCCESS;
}

/* __nss_files_data_open -- lock the per-database state and ensure the stream
 * is open, returning a pointer to the locked state via *PDATA.
 *
 * Based on: nss/nss_files_data.c
 *
 * FILE is accepted for call-site compatibility and ignored.
 * HERRNOP is accepted for call-site compatibility and never written;
 * see the section header above for the rationale.
 * ERRNOP may be NULL; it is only written on failure.
 *
 * On NSS_STATUS_SUCCESS the mutex is held and the caller MUST call
 * __nss_files_data_put() to release it.
 *
 * On any error the mutex is released before returning and the caller
 * must NOT call __nss_files_data_put().
 */
static inline enum nss_status
__nss_files_data_open (struct nss_files_per_file_data **pdata,
                       enum nss_files_file file, const char *path,
                       int *errnop, int *herrnop)
{
  (void) file;
  (void) herrnop;
  struct nss_files_per_file_data *data = __nss_files_data_get ();

  pthread_mutex_lock (&data->lock);

  if (data->stream == NULL)
    {
      data->stream = __nss_files_fopen (path);
      if (data->stream == NULL)
        {
          enum nss_status status;
          if (errno == EAGAIN)
            status = NSS_STATUS_TRYAGAIN;
          else
            status = NSS_STATUS_UNAVAIL;
          if (errnop != NULL)
            *errnop = errno;
          pthread_mutex_unlock (&data->lock);
          return status;
        }
    }

  *pdata = data;
  return NSS_STATUS_SUCCESS;
  /* Mutex remains held; caller must call __nss_files_data_put.  */
}

/* __nss_files_data_put -- release the lock acquired by __nss_files_data_open.
 *
 * Based on: nss/nss_files_data.c
 */
static inline void
__nss_files_data_put (struct nss_files_per_file_data *data)
{
  pthread_mutex_unlock (&data->lock);
}

#endif /* ALTFILES_NSS_FILES_DATA_H */
