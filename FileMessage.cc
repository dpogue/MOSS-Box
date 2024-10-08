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

#include <stdarg.h>
#include <pthread.h>

#include <sys/uio.h> /* for struct iovec */

#include <stdexcept>

#include "machine_arch.h"
#include "exceptions.h"
#include "constants.h"
#include "protocol.h"
#include "msg_typecodes.h"
#include "util.h"

#include "Logger.h"
#include "NetworkMessage.h"
#include "FileTransaction.h"
#include "FileMessage.h"

extern const int32_t FileServerMessage::zero = 0;

NetworkMessage * FileClientMessage::make_if_enough(const uint8_t *buf,
               size_t len) {
  if (len < 4) {
    return NULL;
  }
  uint32_t msg_len = read32(buf, 0);
  if (msg_len > 536/*max File message size*/) {
    throw overlong_message(msg_len);
  }
  if (len < msg_len) {
    return NULL;
  }
  if (msg_len <= 0) {
    return new UnknownMessage(buf, msg_len);
  }

  if (msg_len < 8) {
    return new UnknownMessage(buf, msg_len);
  }
  int32_t type = read32(buf, 4);
  switch (type) {
  case Cli2File_ManifestRequest:
  case Cli2File_FileDownloadRequest:
  case Cli2File_FileDownloadChunkAck:
  case Cli2File_ManifestEntryAck:
  case Cli2File_PingRequest:
  case Cli2File_BuildIdRequest:
    return new FileClientMessage(buf, msg_len, type);
  default:
    return new UnknownMessage(buf, msg_len);
  }
}

bool FileClientMessage::check_useable() const {
  switch(m_type) {
  case Cli2File_PingRequest:
  case Cli2File_FileDownloadChunkAck:
  case Cli2File_ManifestEntryAck:
  case Cli2File_BuildIdRequest:
    if (m_buflen >= 12) {
      return true;
    }
    break;
  case Cli2File_ManifestRequest:
  case Cli2File_FileDownloadRequest:
    if (m_buflen >= 14) {
      return true;
    }
    break;
  }
  return false;
}

FileServerMessage::FileServerMessage(FileClientMessage *ping) 
  : NetworkMessage(NULL, 0, Cli2File_PingRequest), m_transaction(NULL) {

  m_buflen = ping->message_len();
  m_buf = new uint8_t[m_buflen];
  memcpy(m_buf, ping->buffer(), m_buflen);
}

FileServerMessage::FileServerMessage(FileTransaction *trans, int32_t reply_type)
  : NetworkMessage(NULL, 0, reply_type), m_transaction(trans) {

  status_code_t status = trans->status();
  m_buflen = 28;
  m_buf = new uint8_t[m_buflen];
  int32_t len = (status == NO_ERROR ? trans->chunk_length() : 0);
  write32(m_buf, 4, m_type);
  write32(m_buf, 8, trans->request_id());
  write32(m_buf, 12, (int32_t)status);
  // XXX unknown
  write32(m_buf, 16, status == NO_ERROR
      ? (m_type == Cli2File_ManifestRequest ? 1 : 4)
      : 0);
  write32(m_buf, 20, status == NO_ERROR ? trans->file_len() : 0);
  write32(m_buf, 24, len);
  if (m_type == Cli2File_ManifestRequest) {
    len *= 2;
  }
  len += m_buflen;
  write32(m_buf, 0, len);
}

FileServerMessage::FileServerMessage(uint32_t reqid, status_code_t status,
             int32_t build_no)
  : NetworkMessage(NULL, 0, Cli2File_BuildIdRequest), m_transaction(NULL) {

  m_buflen = 20;
  m_buf = new uint8_t[m_buflen];
  write32(m_buf, 0, 20);
  write32(m_buf, 4, m_type);
  write32(m_buf, 8, reqid);
  write32(m_buf, 12, (int32_t)status);
  write32(m_buf, 16, build_no);
}

uint32_t FileServerMessage::fill_iovecs(struct iovec *iov, uint32_t iov_ct,
             uint32_t start_at) {
  uint32_t i = 0;

  if (start_at < m_buflen) {
    iov[i].iov_base = m_buf + start_at;
    iov[i].iov_len = m_buflen - start_at;
    start_at = 0;
    i++;
    if (m_transaction && m_transaction->status() == NO_ERROR) {
      i += m_transaction->fill_iovecs(iov+i, iov_ct-i, &start_at);
    }
  }
  else if (m_transaction && m_transaction->status() == NO_ERROR) {
    start_at -= m_buflen;
    i += m_transaction->fill_iovecs(iov+i, iov_ct-i, &start_at);
    if (m_type == Cli2File_ManifestRequest && i < iov_ct && start_at < 2) {
      iov[i].iov_base = (uint8_t*)&zero;
      iov[i].iov_len = 2-start_at;
      i++;
    }
  }
  else {
    // shouldn't happen
  }
  return i;
}

uint32_t FileServerMessage::iovecs_written_bytes(uint32_t byte_ct, uint32_t start_at,
                bool *msg_done) {
  if (start_at < m_buflen) {
    if (byte_ct + start_at < m_buflen) {
      *msg_done = false;
      return 0;
    }
    else {
      byte_ct -= m_buflen - start_at;
      start_at = m_buflen;
    }
  }
  if (m_transaction && m_transaction->status() == NO_ERROR) {
    byte_ct = m_transaction->iovecs_written_bytes(byte_ct,
              start_at - m_buflen,
              msg_done);
    if (m_type == Cli2File_ManifestRequest && *msg_done) {
      if (byte_ct >= 2) {
  byte_ct -= 2;
      }
      else {
  *msg_done = false;
  return 0;
      }
    }
  }
  else {
    if (start_at == m_buflen) {
      *msg_done = true;
    }
    else {
      *msg_done = false;
    }
  }
  return byte_ct;
}

uint32_t FileServerMessage::fill_buffer(uint8_t *buffer, size_t len,
             uint32_t start_at, bool *msg_done) {
  uint32_t offset = 0;

  *msg_done = true;
  if (start_at < m_buflen) {
    offset = m_buflen - start_at;
    if (offset > len) {
      *msg_done = false;
      offset = len;
    }
    memcpy(buffer, m_buf + start_at, offset);
    if (*msg_done) {
      start_at = 0;
    }
    else {
      return offset;
    }
  }
  if (m_transaction && m_transaction->status() == NO_ERROR) {
    offset += m_transaction->fill_buffer(buffer+offset, len - offset,
           &start_at, msg_done);
    if (m_type == Cli2File_ManifestRequest && *msg_done) {
      if (start_at < 2) {
  if (len - offset >= 2-start_at) {
    if (start_at < 1) {
      write16(buffer, offset, 0);
      offset += 2;
    }
    else {
      buffer[offset++] = 0;
    }
  }
  else {
    *msg_done = false;
  }
      }
    }
  }
  return offset;
}
