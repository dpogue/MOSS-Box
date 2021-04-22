/*
 MOSS - A server for the Myst Online: Uru Live client/protocol
 Copyright (C) 2008-2011  a'moaca'

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Functions and macros that should be generally useful in random places.
 */

//#include <stdio.h>
//#include <string.h>
//
//#include <sys/stat.h>
//
//#include "machine_arch.h"
// This construct requires GCC5 or C++17
#ifdef __has_include
# if __has_include(<limits.h>)
#  include <limits.h>
# endif
# if __has_include(<netinet/in.h>)
#  include <netinet/in.h>
# endif
#else
# include <limits.h>
# include <netinet/in.h>
#endif
# include <limits.h>
# include <netinet/in.h>

#ifndef _UTIL_H_
#define _UTIL_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * struct timeval manipulations.
 */

#define timeval_lessthan(t1, t2) \
  (t1.tv_sec < t2.tv_sec ? 1 : \
    (t1.tv_sec > t2.tv_sec ? 0 : \
      t1.tv_usec < t2.tv_usec ? 1 : 0))

#define timeval_add(t1, t2) \
  do { \
    t1.tv_sec += t2.tv_sec; \
    t1.tv_usec += t2.tv_usec; \
    if (t1.tv_usec > 1000000) { \
      t1.tv_usec -= 1000000; \
      t1.tv_sec++; \
    } \
  } while (0);

#define timeval_subtract(t1, t2) \
  do { \
    if (t2.tv_usec > t1.tv_usec) { \
      t1.tv_sec -= 1; \
      t1.tv_usec += 1000000; \
    } \
    t1.tv_sec -= t2.tv_sec; \
    t1.tv_usec -= t2.tv_usec; \
  } while (0);

#define timeval_difference(t1, t2, t) \
  do { \
    if (t2.tv_usec > t1.tv_usec) { \
      t.tv_sec = (t1.tv_sec - 1) - t2.tv_sec; \
      t.tv_usec = (t1.tv_usec + 1000000) - t2.tv_usec; \
    } \
    else { \
      t.tv_sec = t1.tv_sec - t2.tv_sec; \
      t.tv_usec = t1.tv_usec - t2.tv_usec; \
    } \
  } while (0);

/*
 * MIN and MAX
 */
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

/*
 * UUID manipulations.
 */

#define UUID_STR_LEN 37
#define UUID_RAW_LEN 16

/*
 * This formats a "little-endian" UUID (byteswaps the first 8 bytes).
 */
inline void format_uuid(const uint8_t *buf, char *uuid) {
  snprintf(uuid, UUID_STR_LEN, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
      read32(buf, 0), read16(buf, 4), read16(buf, 6),
      buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);
}

/*
 * Creates a type-4 (random) UUID in the buffer, which must be at least 16
 * bytes long if as_text is false, or 37 if as_text is true. If as_text is
 * true the string formatted representation, with dashes and null-terminated,
 * is placed in the buffer. If as_text is false, 16 bytes are placed in the
 * buffer, byteswapped to little-endian (as transmitted on the wire).
 */
void gen_uuid(uint8_t *buf, int32_t as_text);

/*
 * This function converts a string representing a UUID (with or without
 * dashes) to a sequence of 16 bytes. If "byteswap" is true, the first
 * 8 bytes are swapped around to match what is transmitted on the wire.
 *
 * A uuid of "11223344-5566-7788-9900-aabbccddeeff" when byteswapped,
 * results in bytes in the following order:
 * 44 33 22 11 66 55 88 77 99 00 aa bb cc dd ee ff
 */
int32_t uuid_string_to_bytes(uint8_t *buf, uint32_t buflen, const char *uuid_string, uint32_t uuid_string_len, int32_t have_dashes,
    int32_t byteswap);

/*
 * This function converts 16 bytes to a string form of the UUID, placing it
 * in the buffer 'buf' and null-terminating it.
 */
int32_t uuid_bytes_to_string(uint8_t *buf, uint32_t buflen, const uint8_t *uuid_bytes, uint32_t uuid_bytes_len, int32_t want_dashes,
    int32_t byteswap);

/*
 * Utilities.
 */
int32_t recursive_mkdir(const char *pathname, mode_t mode);
void do_random_seed();
void get_random_data(uint8_t *buf, uint32_t buflen);
/*
 * This is a wrapper for (hopefully) thread-safe name resolution. It returns
 * NULL on success, and an error string on failure.
 */
const char* resolve_hostname(const char *hostname, uint32_t *ipaddr);

uint32_t inaddr_c_str(char *buf, size_t buflen, in_addr_t address, in_port_t port, in_port_t default_port);

#ifdef __cplusplus
}
#endif

#endif /* _UTIL_H_ */
