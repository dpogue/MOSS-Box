/*
 MOSS - A server for the Myst Online: Uru Live client/protocol
 Copyright (C) 2011  a'moaca'

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

#include <stdarg.h>
#include <iconv.h>

#include <sys/uio.h> /* for struct iovec */

#include <stdexcept>
#include <string>

#include "machine_arch.h"
#include "exceptions.h"
#include "constants.h"
#include "protocol.h"
#include "msg_typecodes.h"
#include "util.h"
#include "UruString.h"

#include "Logger.h"
#include "NetworkMessage.h"
#include "GatekeeperMessage.h"

NetworkMessage* GatekeeperClientMessage::make_if_enough(const uint8_t *buf, size_t len) {
  if (len < 7) {
    return NULL;
  }
  uint16_t type = read16(buf, 0);
  switch (type) {
  case Cli2GateKeeper_PingRequest:
    if (len < 14) {
      return NULL;
    }
    return new GatekeeperPingMessage(buf, 14);
    break;
  case Cli2GateKeeper_FileSrvIpAddressRequest:
  case Cli2GateKeeper_AuthSrvIpAddressRequest:
    return new GatekeeperClientMessage(buf, 7, type);
    break;
  default:
    return new UnknownMessage(buf, len);
  }
}

uint32_t GatekeeperPingMessage::fill_iovecs(struct iovec *iov, uint32_t iov_ct, uint32_t start_at) {
  if (iov_ct > 0) {
    iov[0].iov_base = m_buf + start_at;
    iov[0].iov_len = m_buflen - start_at;
    return 1;
  } else {
    return 0;
  }
}

uint32_t GatekeeperPingMessage::iovecs_written_bytes(uint32_t byte_ct, uint32_t start_at, bool *msg_done) {
  if (byte_ct + start_at >= m_buflen) {
    *msg_done = true;
    return (byte_ct + start_at) - m_buflen;
  } else {
    *msg_done = false;
    return 0;
  }
}

uint32_t GatekeeperPingMessage::fill_buffer(uint8_t *buffer, size_t len, uint32_t start_at, bool *msg_done) {
  if (m_buflen - start_at <= len) {
    memcpy(buffer, m_buf + start_at, m_buflen - start_at);
    *msg_done = true;
    return m_buflen - start_at;
  } else {
    memcpy(buffer, m_buf + start_at, len);
    *msg_done = false;
    return len;
  }
}

GatekeeperServerMessage::GatekeeperServerMessage(bool for_file, uint32_t reqid, const char *ipaddr) :
    NetworkMessage(for_file ? GateKeeper2Cli_FileSrvIpAddressReply : GateKeeper2Cli_AuthSrvIpAddressReply) {
  UruString addr(ipaddr, false);
  m_buflen = 6 + addr.send_len(true, true, false);
  m_buf = new uint8_t[m_buflen];
  write16(m_buf, 0, m_type);
  write32(m_buf, 2, reqid);
  memcpy(m_buf + 6, addr.get_str(true, true, false), m_buflen - 6);
}

uint32_t GatekeeperServerMessage::fill_iovecs(struct iovec *iov, uint32_t iov_ct, uint32_t start_at) {
  if (iov_ct > 0) {
    iov[0].iov_base = m_buf + start_at;
    iov[0].iov_len = m_buflen - start_at;
    return 1;
  } else {
    return 0;
  }
}

uint32_t GatekeeperServerMessage::iovecs_written_bytes(uint32_t byte_ct, uint32_t start_at, bool *msg_done) {
  if (byte_ct + start_at >= m_buflen) {
    *msg_done = true;
    return (byte_ct + start_at) - m_buflen;
  } else {
    *msg_done = false;
    return 0;
  }
}

uint32_t GatekeeperServerMessage::fill_buffer(uint8_t *buffer, size_t len, uint32_t start_at, bool *msg_done) {
  if (m_buflen - start_at <= len) {
    memcpy(buffer, m_buf + start_at, m_buflen - start_at);
    *msg_done = true;
    return m_buflen - start_at;
  } else {
    memcpy(buffer, m_buf + start_at, len);
    *msg_done = false;
    return len;
  }
}
