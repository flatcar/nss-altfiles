/*
 * altfiles_scratch_buffer.h
 *
 * Based on glibc 2.38.
 */

#ifndef ALTFILES_SCRATCH_BUFFER_H
#define ALTFILES_SCRATCH_BUFFER_H

/* ============================================================
 * Scratch Buffer
 * ============================================================
 *
 * glibc internal: include/scratch_buffer_grow.c
 *
 * Make it an extern here so we can have a single built object
 * to link in later and avoid symbol redefinition or name clash.
 */

#include <scratch_buffer.h>
extern bool __libc_scratch_buffer_grow (struct scratch_buffer *buffer);

#endif /* ALTFILES_SCRATCH_BUFFER_H */
