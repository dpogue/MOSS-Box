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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h> /* for close() */
#endif

#include <stdarg.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <sys/uio.h> /* for struct iovec */

#include <stdexcept>

#include "machine_arch.h"
#include "constants.h"
#include "protocol.h"

#include "Logger.h"
#include "FileTransaction.h"

/*
 * if mmap() MAP_FILE flag is not defined by system, define it here as a no-op
 */
#ifndef MAP_FILE
#define MAP_FILE (0)
#endif

FileTransaction::FileTransaction(uint32_t request_id, Logger *logger, bool is_manifest, bool is_auth) :
    m_log(logger), m_id(request_id), m_manifest(is_manifest), m_auth(is_auth), m_file_ct(0), m_fd(-1), m_filesize(0), m_mapped(
        NULL), m_status(NO_ERROR), m_offset(0), m_chunk_remaining(0), m_real_offset(0), m_backup_buf(NULL), m_backup_len(
        0), m_backup_fill(0) {
}

int32_t FileTransaction::init(const char *dirname, char *fname) {
  int32_t ret;
  uint32_t len;
  struct stat s;

  len = strlen(dirname) + strlen(fname) + 2;
  char path[len];
  snprintf(path, len, "%s/%s", dirname, fname);

  ret = stat(path, &s);
  if (ret < 0) {
    if (m_manifest) {
      log_warn(m_log, "Server tried to mumble to itself about %s, "
          "but no one is listening.\n", fname);
    } else {
      log_warn(m_log, "Request for nonexistent file %s\n", fname);
    }
    m_status = ERROR_FILE_NOT_FOUND;
    return 1;
  }
  m_filesize = s.st_size;
  m_fd = open(path, O_RDONLY, 0);
  if (m_fd < 0) {
    log_err(m_log, "Open failed for file %s: %s\n", fname, strerror(errno));
    m_status = ERROR_FILE_NOT_FOUND;
    return 1;
  }
  log_msgs(m_log, "File server transaction %u -> file %s\n", m_id, fname);

  m_mapped = (uint8_t*) mmap(NULL, m_filesize, PROT_READ,
  MAP_FILE | MAP_PRIVATE, m_fd, 0);
  if (m_mapped == MAP_FAILED) {
    log_err(m_log, "mmap() of %s failed: %s\n", fname, strerror(errno));
    // so mmap() failed, use backup
    if (!m_auth) {
      m_backup_len = FILE_CHUNKSIZE + (4 * (FILE_CHUNKSIZE / 152));
    } else if (!m_manifest) {
      m_backup_len = AUTH_CHUNKSIZE;
    } else {
      // no chunks
      m_backup_len = m_filesize;
    }
    m_backup_buf = new uint8_t[m_backup_len];
    if (!m_backup_buf) {
      log_err(m_log, "Cannot allocate memory for %s backup_buf\n", fname);
      close(m_fd);
      m_fd = -1;
      m_status = ERROR_INTERNAL;
      return -1;
    }
  }

  if (!m_auth && m_manifest) {
    // read the first four bytes to get the message count
    if (!m_backup_buf) {
      m_file_ct = read32(m_mapped, m_offset);
    } else {
      ret = read(m_fd, m_backup_buf, 4);
      if (ret < 0) {
        log_err(m_log, "Read failed for file %s: %s\n", fname, strerror(errno));
        close(m_fd);
        m_fd = -1;
        m_status = ERROR_INTERNAL;
      } else if (ret < 4) {
        log_err(m_log, "Short read (%d) of file %s\n", ret, fname);
        close(m_fd);
        m_fd = -1;
        m_status = ERROR_INTERNAL;
      } else {
        m_file_ct = read32(m_backup_buf, 0);
      }
    }
    m_real_offset = 4;
  }
  // compute first chunk
  if (chunk_acked() < 0) {
    close(m_fd);
    m_fd = -1;
  }

  return 0;
}

FileTransaction::~FileTransaction() {
  if (m_backup_buf) {
    delete[] m_backup_buf;
  }
  if (m_fd >= 0) {
    if (munmap(m_mapped, m_filesize) < 0) {
      log_err(m_log, "munmap failed: %s\n", strerror(errno));
    }
    close(m_fd);
  }
}

size_t FileTransaction::file_len() const {
  if (m_manifest) {
    return m_file_ct;
  } else {
    return m_filesize;
  }
}

int32_t FileTransaction::chunk_acked() {
  // set offset to account for acknowledgement
  if (m_manifest) {
    m_offset += m_real_offset;
  } else {
    m_offset += m_chunk_remaining;
  }

  // compute next chunk size (tentative for manifests)
  if (file_complete()) {
    m_chunk_remaining = 0;
    return 0;
  }
  if (m_auth) {
    m_chunk_remaining = AUTH_CHUNKSIZE;
  } else {
    m_chunk_remaining = FILE_CHUNKSIZE;
  }
  if (m_filesize - m_offset < m_chunk_remaining || (m_auth && m_manifest)) {
    m_chunk_remaining = m_filesize - m_offset;
  }

  if (!m_auth && m_manifest) {
    uint32_t total_len, file_len, real_len;
    uint8_t *buf;

    if (m_backup_buf) {
      if (m_backup_fill > m_real_offset) {
        m_backup_fill -= m_real_offset;
        memcpy(m_backup_buf, m_backup_buf + m_real_offset, m_backup_fill);
      } else {
        m_backup_fill = 0;
      }
      file_len = m_backup_len - m_backup_fill;
      if (m_filesize - m_offset < file_len) {
        file_len = m_filesize - m_offset;
      }
      int32_t ret = read(m_fd, m_backup_buf + m_backup_fill, file_len);
      if (ret < (int32_t) file_len) {
        // the file changed since we got its size?
        log_err(m_log, "Transaction %u short read (%d)!\n", m_id, ret);
        m_status = ERROR_INTERNAL;
        return -1;
      } else {
        m_backup_fill += ret;
      }
      buf = m_backup_buf;
      total_len = m_backup_fill;
    } else {
      buf = m_mapped + m_offset;
      total_len = m_filesize - m_offset;
    }

    file_len = real_len = m_real_offset = 0;
    while (m_real_offset + 4 <= total_len) {
      file_len = read32(buf, m_real_offset);
      if (file_len + real_len <= m_chunk_remaining) {
        real_len += file_len;
        m_real_offset += file_len + 4;
      } else {
        break;
      }
    }
    if (m_real_offset > total_len) {
      // we ran out of buffer first! There must be a problem with the file
      log_err(m_log, "Ran out of buffer space! Check format of file for "
          "transaction %u\n", m_id);
      m_real_offset = total_len;
    }
    m_chunk_remaining = real_len;
  } else {
    if (m_backup_buf) {
      int32_t ret = read(m_fd, m_backup_buf, m_chunk_remaining);
      if (ret < (int32_t) m_chunk_remaining) {
        // the file changed since we got its size?
        log_err(m_log, "Transaction %u short read (%d)!\n", m_id, ret);
        m_status = ERROR_INTERNAL;
        return -1;
      } else {
        m_backup_fill = ret;
      }
    }
  }
  return 0;
}

bool FileTransaction::file_complete() const {
  return m_offset >= m_filesize;
}

bool FileTransaction::in_last_chunk() const {
  uint32_t next_offset = m_offset;
  // set offset to account for acknowledgement
  if (m_manifest) {
    next_offset += m_real_offset;
  } else {
    next_offset += m_chunk_remaining;
  }
  return next_offset >= m_filesize;
}

uint32_t FileTransaction::chunk_length() const {
  if (m_manifest) {
    return (m_chunk_remaining / 2) + 1;
  } else {
    return m_chunk_remaining;
  }
}

// only used for auth file downloads
uint32_t FileTransaction::chunk_offset() const {
  return m_offset;
}

// returns how many iovecs were filled in
uint32_t FileTransaction::fill_iovecs(struct iovec *iov, uint32_t iov_ct, uint32_t *start_at) {
  if (!m_auth && m_manifest) {
    uint32_t buflen, iov_off = 0, tmp_offset = 0, file_len;
    uint8_t *the_buf;

    if (m_backup_buf) {
      the_buf = m_backup_buf;
      buflen = m_backup_fill;
    } else {
      the_buf = m_mapped + m_offset;
      buflen = m_filesize - m_offset;
    }

    while (iov_off < iov_ct && tmp_offset < m_real_offset) {
      file_len = read32(the_buf, tmp_offset);
      if (tmp_offset + file_len + 4 > buflen) {
        log_err(m_log, "Transaction %u contents of the file changed since "
            "the chunk was computed!!\n", m_id);
        // XXX throw exception
      }
      if (*start_at >= file_len) {
        // skip this one
        *start_at -= file_len;
      } else {
        iov[iov_off].iov_base = the_buf + tmp_offset + 4 + *start_at;
        iov[iov_off].iov_len = file_len - *start_at;
        iov_off++;
        *start_at = 0;
      }
      tmp_offset += 4 + file_len;
    }
    return iov_off;
  } else {
    if (m_backup_buf) {
      if (m_backup_fill > *start_at) {
        iov[0].iov_base = m_backup_buf + *start_at;
        iov[0].iov_len = m_backup_fill - *start_at;
        *start_at = 0;
        return 1;
      } else {
        *start_at -= m_backup_fill;
        return 0;
      }
    } else {
      if (m_chunk_remaining > *start_at) {
        iov[0].iov_base = m_mapped + m_offset + *start_at;
        iov[0].iov_len = m_chunk_remaining - *start_at;
        *start_at = 0;
        return 1;
      } else {
        *start_at -= m_chunk_remaining;
        return 0;
      }
    }
  }
}

uint32_t FileTransaction::iovecs_written_bytes(uint32_t u_byte_ct, uint32_t start_at, bool *chunk_done) {
  // I do not feel like rewriting the code to not depend on byte_ct being
  // signed, because there will be so much more code that way.
  int32_t byte_ct = (int32_t) u_byte_ct;
  if (!m_auth && m_manifest) {
    uint32_t buflen, tmp_offset = 0, file_len;
    uint8_t *buf;

    if (m_backup_buf) {
      buf = m_backup_buf;
      buflen = m_backup_fill;
    } else {
      buf = m_mapped + m_offset;
      buflen = m_filesize - m_offset;
    }

    while (tmp_offset < m_real_offset && byte_ct > 0) {
      file_len = read32(buf, tmp_offset);
      if (tmp_offset + file_len + 4 > buflen) {
        log_err(m_log, "Transaction %u contents of the file changed since "
            "the chunk was computed!!\n", m_id);
        // XXX throw exception
      }
      if (start_at >= file_len) {
        start_at -= file_len;
      } else {
        byte_ct -= file_len - start_at;
        start_at = 0;
      }
      tmp_offset += 4 + file_len;
    }

    if (tmp_offset >= m_real_offset && byte_ct >= 0) {
      *chunk_done = true;
      return byte_ct;
    } else {
      *chunk_done = false;
      return 0;
    }
  } else {
    byte_ct -= m_chunk_remaining - start_at;
    if (byte_ct >= 0) {
      *chunk_done = true;
      return byte_ct;
    } else {
      *chunk_done = false;
      return 0;
    }
  }
}

uint32_t FileTransaction::fill_buffer(uint8_t *buffer, size_t len, uint32_t *start_at, bool *chunk_done) {
  if (!m_auth && m_manifest) {
    uint32_t buflen, tmp_offset = 0, file_len, written = 0, len_to_write;
    uint8_t *the_buf;

    if (m_backup_buf) {
      the_buf = m_backup_buf;
      buflen = m_backup_fill;
    } else {
      the_buf = m_mapped + m_offset;
      buflen = m_filesize - m_offset;
    }

    while (tmp_offset < m_real_offset && len > written) {
      file_len = read32(the_buf, tmp_offset);
      if (tmp_offset + file_len + 4 > buflen) {
        log_err(m_log, "Transaction %u contents of the file changed since "
            "the chunk was computed!!\n", m_id);
        // XXX throw exception
      }
      if (*start_at >= file_len) {
        // skip this one
        *start_at -= file_len;
      } else {
        len_to_write = file_len - *start_at;
        if (len_to_write > len - written) {
          len_to_write = len - written;
        }
        memcpy(buffer + written, the_buf + tmp_offset + 4 + *start_at, len_to_write);
        written += len_to_write;
        *start_at = 0;
        if (len_to_write < file_len - *start_at) {
          break;
        }
      }
      tmp_offset += 4 + len;
    }
    if (tmp_offset >= m_real_offset) {
      *chunk_done = true;
    } else {
      *chunk_done = false;
    }
    return written;
  } else {
    if (*start_at >= m_chunk_remaining) {
      *start_at -= m_chunk_remaining;
      *chunk_done = true;
      return 0;
    }
    uint32_t len_to_write = m_chunk_remaining - *start_at;
    if (len_to_write > len) {
      len_to_write = len;
    }
    if (m_backup_buf) {
      memcpy(buffer, m_backup_buf + *start_at, len_to_write);
    } else {
      memcpy(buffer, m_mapped + m_offset + *start_at, len_to_write);
    }
    if (len_to_write + *start_at < m_chunk_remaining) {
      *chunk_done = false;
    } else {
      *chunk_done = true;
    }
    *start_at = 0;
    return len_to_write;
  }
}
