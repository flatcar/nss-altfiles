/*
 * altfiles_alloc_buffer.h
 *
 * Based on glibc 2.38.
 */

#ifndef ALTFILES_ALLOC_BUFFER_H
#define ALTFILES_ALLOC_BUFFER_H

/* ============================================================
 * Allocation buffer  (glibc >= 2.17)
 * ============================================================
 *
 * glibc source: malloc/alloc_buffer_*.c
 *
 * Make it an extern here so we can have a single built object
 * to link in later and avoid symbol redefinition or name clash.
 */
#include <alloc_buffer.h>
extern
struct alloc_buffer
__libc_alloc_buffer_allocate (size_t size, void **pptr);

extern
void *
__libc_alloc_buffer_alloc_array (struct alloc_buffer *buf,
                                 size_t element_size, size_t align,
                                 size_t count);

extern
void
__libc_alloc_buffer_create_failure (void *start, size_t size);

extern
struct alloc_buffer
__libc_alloc_buffer_copy_bytes (struct alloc_buffer buf,
                                const void *src, size_t length);

extern
struct alloc_buffer
__libc_alloc_buffer_copy_string (struct alloc_buffer buf, const char *src);

#endif /* ALTFILES_ALLOC_BUFFER_H */
