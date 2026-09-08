/*
 * debug.h
 * contains utilitary functions for debugging
 *
 * Copyright (c) 2008 Jonathan Beck All Rights Reserved.
 * Copyright (c) 2010 Martin S. All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __DEBUG_H
#define __DEBUG_H

#include <plist/plist.h>

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L && !defined(STRIP_DEBUG_CODE)
#define debug_info(...) debug_info_real (__func__, __FILE__, __LINE__, __VA_ARGS__)
#define debug_plist(a) debug_plist_real (__func__, __FILE__, __LINE__, a)
#elif defined(__GNUC__) && __GNUC__ >= 3 && !defined(STRIP_DEBUG_CODE)
#define debug_info(...) debug_info_real (__FUNCTION__, __FILE__, __LINE__, __VA_ARGS__)
#define debug_plist(a) debug_plist_real (__FUNCTION__, __FILE__, __LINE__, a)
#else
#define debug_info(...) debug_info_real (__func__, __FILE__, __LINE__, __VA_ARGS__)
#define debug_plist(a) debug_plist_real (__func__, __FILE__, __LINE__, a)
#endif

/* DESK-3375: gate for PER-I/O tracing - a debug_info() that fires once per read, write or
 * packet rather than once per connection or session. Use this, not debug_info(), for anything
 * on a data path; that is the whole reason this macro exists.
 *
 * Why: measured on station FLAD067's real 12-device logs (134,993,367 bytes), FOUR per-I/O
 * debug_info() sites carried 70.32% of every byte written - idevice.cc pre-read 50.48%,
 * idevice.cc SSL_write 11.46%, afc.c afc_file_read 5.58%, afc.c afc_receive_data 2.80%. That
 * volume (~550 KB/s) refilled the ~51 MB jw_libimobile.log cap every ~95 seconds and held the
 * rotation daemon in the 1 Hz retry that ended in an uncatchable CRT fast-fail. Behind this
 * gate the same capture runs at ~165 KB/s, so the live file holds ~5 minutes of history.
 *
 * The narrative (connect, session, lockdown, errors) is NOT gated and still prints at level 1.
 * Level 2 (JW_LIBIMOBILE_DEBUG=2) opens this one. The line prefix stays correct because
 * debug_info()'s __FILE__/__LINE__/__func__ expand at the call site, not here.
 *
 * NOTE: do not "optimise" this by gating debug_buffer() instead. That was recommended and then
 * refuted by the same measurement - debug_buffer() emits ZERO bytes in production, because
 * every call site in this tree is commented out but lockdown-cu.c's, which stations never
 * reach. See the repo README.md. */
#define DEBUG_LEVEL_IO_TRACING 2
#define debug_info_io(...) do { if (internal_debug_io_enabled()) debug_info(__VA_ARGS__); } while (0)

void debug_info_real(const char *func,
											const char *file,
											int	line,
											const char *format, ...);

void debug_buffer(const char *data, const int length);
void debug_buffer_to_file(const char *file, const char *data, const int length);
void debug_plist_real(const char *func,
											const char *file,
											int	line,
											plist_t plist);

void internal_set_debug_level(int level);

/* DESK-3375: is the level high enough for per-I/O tracing (debug_info_io)? The threshold is
 * necessarily restated here because this C library cannot include jwcore's C++ logging.h; the
 * integer handed over by idevice_set_debug_level() is the contract between the two, and
 * StdLogRotateTest pins it from both ends. */
int internal_debug_io_enabled(void);
void internal_set_stderr(FILE* err);
void internal_init_mutex();

#endif
