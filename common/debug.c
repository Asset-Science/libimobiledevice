/*
 * debug.c
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

 #ifdef HAVE_CONFIG_H
 #include <config.h>
 #endif
 #include <stdarg.h>
 #define _GNU_SOURCE 1
 #define __USE_GNU 1
 #include <stdio.h>
 #include <stdint.h>
 #include <stdlib.h>
 #include <time.h>
 
 #include "debug.h"
 #include "libimobiledevice/libimobiledevice.h"
 #include "libimobiledevice-glue/thread.h"
 #include "src/idevice.h"
 
 #ifndef STRIP_DEBUG_CODE
 #include "asprintf.h"
 #endif
 
 static int debug_level;
 static mutex_t mutex;
 // DESK-3375: mutex_t is a CRITICAL_SECTION on Windows, so locking one that was never
 // initialised is undefined behaviour. Only take the lock once internal_init_mutex() has
 // run (jwservice does that at startup - see logging.cc); a consumer that never inits keeps
 // the previous unsynchronised behaviour rather than corrupting an uninitialised lock.
 static int mutex_ready = 0;
 
 void internal_set_debug_level(int level)
 {
	 debug_level = level;
 }

 // DESK-3375: see debug_info_io() in debug.h. Deliberately a >= comparison rather than a
 // truthiness test, so level 1 keeps the narrative while the per-I/O sites that carried 70% of
 // the bytes stay silent until someone explicitly asks for level 2.
 int internal_debug_io_enabled(void)
 {
	 return debug_level >= DEBUG_LEVEL_IO_TRACING;
 }
 
 
 // used to redirect stderr output in a safer manner
 static FILE* mStderr = NULL;
 
 // DESK-3375: this stream is process-global and written by every device thread, while
 // StdLogDaemon swaps it on rotation. Swapping without holding the lock the writers take let
 // a thread carry the old FILE* across the swap and fprintf() through it after the rotator
 // had fclose()d it; the CRT catches that in stream validation and calls __fastfail, which
 // surfaces as exception 0xc0000409 in ucrtbase.dll - uncatchable, and it kills the whole
 // process (all slots). Serialising the swap against the writers also gives the caller the
 // guarantee it needs: once this returns, no writer is still using the previous stream, so
 // the caller may then safely close it.
 void internal_set_stderr(FILE* err)
 {
	 if (mutex_ready) {
		 mutex_lock(&mutex);
		 mStderr = err;
		 mutex_unlock(&mutex);
	 }
	 else {
		 mStderr = err;
	 }
 }

 void internal_init_mutex() {
	 // Idempotent: re-initialising a live CRITICAL_SECTION would reset it under active callers.
	 if (mutex_ready)
		 return;
	 mutex_init(&mutex);
	 mutex_ready = 1;
 }
 
 #define MAX_PRINT_LEN (16*1024)
 
 #ifndef STRIP_DEBUG_CODE
 static void debug_print_line(const char *func, const char *file, int line, const char *buffer)
 {
	 char *str_time = NULL;
	 char *header = NULL;
	 time_t the_time;
 
	 time(&the_time);
	 str_time = (char*)malloc(255);
	 strftime(str_time, 254, "%H:%M:%S", localtime (&the_time));
 
	 /* generate header text */
	 (void)asprintf(&header, "%s %s:%d %s()", str_time, file, line, func);
	 free (str_time);
 
	 /* trim ending newlines */

	 /* DESK-3375: hold the lock across BOTH writes and read the stream once into a local.
	  * Holding it is what makes internal_set_stderr()'s close-after-swap contract sound (a
	  * rotation cannot land between the header and its content, or between the read of the
	  * pointer and the write through it). It also stops two device threads interleaving a
	  * header with another line's body, which used to garble this log. */
	 if (mutex_ready)
		 mutex_lock(&mutex);
	 {
		 FILE* out = mStderr != NULL ? mStderr : stderr;

		 /* print header */
		 fprintf(out, "%s: ", header);

		 /* print actual debug content */
		 fprintf(out, "%s\n", buffer);
	 }
	 if (mutex_ready)
		 mutex_unlock(&mutex);

	 free (header);
 }
 #endif
 
 void debug_info_real(const char *func, const char *file, int line, const char *format, ...)
 {
 #ifndef STRIP_DEBUG_CODE
 // DESK-3375: locking lives in debug_print_line() now, held across the writes themselves.
 // Taking it out here as well would deadlock (the same non-recursive CRITICAL_SECTION), and
 // holding it across vasprintf() would serialise formatting for no benefit.
	 va_list args;
	 char *buffer = NULL;
 
	 if (!debug_level)
		 return;
 
	 /* run the real fprintf */
	 va_start(args, format);
	 (void)vasprintf(&buffer, format, args);
	 va_end(args);
	 debug_print_line(func, file, line, buffer);
 
	 free(buffer);
 #endif
 }
 
 void debug_buffer(const char *data, const int length)
 {
 #ifndef STRIP_DEBUG_CODE
	 int i;
	 int j;
	 unsigned char c;
 
	 if (debug_level) {
		 /* DESK-3375: same contract as debug_print_line - one lock for the whole dump, and the
		  * stream read once into a local, so a rotation can neither split a hexdump nor free
		  * the FILE* between the read and the write. */
		 FILE* out;
		 if (mutex_ready)
			 mutex_lock(&mutex);
		 out = mStderr != NULL ? mStderr : stderr;
		 for (i = 0; i < length; i += 16) {
			 fprintf(out, "%04x: ", i);
			 for (j = 0; j < 16; j++) {
				 if (i + j >= length) {
					 fprintf(out, "   ");
					 continue;
				 }
				 fprintf(out, "%02x ", *(data + i + j) & 0xff);
			 }
			 fprintf(out, "  | ");
			 for (j = 0; j < 16; j++) {
				 if (i + j >= length)
					 break;
				 c = *(data + i + j);
				 if ((c < 32) || (c > 127)) {
					 fprintf(out, ".");
					 continue;
				 }
				 fprintf(out, "%c", c);
			 }
			 fprintf(out, "\n");
		 }
		 fprintf(out, "\n");
		 if (mutex_ready)
			 mutex_unlock(&mutex);
	 }
 #endif
 }
 
 void debug_buffer_to_file(const char *file, const char *data, const int length)
 {
 #ifndef STRIP_DEBUG_CODE
	 if (debug_level) {
		 /* DESK-3375: fwrite/fclose on a NULL FILE* raise the CRT invalid-parameter fast-fail
		  * (0xc0000409 in ucrtbase), which is uncatchable and kills the process. */
		 FILE *f = fopen(file, "wb");
		 if (f == NULL) {
			 return;
		 }
		 fwrite(data, 1, length, f);
		 fflush(f);
		 fclose(f);
	 }
 #endif
 }
 
 void debug_plist_real(const char *func, const char *file, int line, plist_t plist)
 {
 #ifndef STRIP_DEBUG_CODE
	 /* DESK-3375: check the level BEFORE serialising. debug_info_real() also early-returns on
	  * !debug_level, but by then plist_to_xml() below has already built - and this function has
	  * thrown away - a complete XML document, at every debug_plist() site, on every call, with
	  * debug logging switched off. */
	 if (!debug_level)
		 return;

	 if (!plist)
		 return;
 
	 char *buffer = NULL;
	 uint32_t length = 0;
	 plist_to_xml(plist, &buffer, &length);
 
	 /* get rid of ending newline as one is already added in the debug line */
	 if (buffer[length-1] == '\n')
		 buffer[length-1] = '\0';
 
	 if (length <= MAX_PRINT_LEN)
		 debug_info_real(func, file, line, "printing %i bytes plist:\n%s", length, buffer);
	 else
		 debug_info_real(func, file, line, "supress printing %i bytes plist...\n", length);
 
	 free(buffer);
 #endif
 }
 
 