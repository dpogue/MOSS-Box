/* -*- c++ -*- */

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

/*
 * This file includes gatekeeper messages.
 */

//#include <sys/uio.h> /* for struct iovec */
//
//#include "msg_typecodes.h"
//
//#include "Logger.h"
//#include "NetworkMessage.h"
#ifndef _GATEKEEPER_MESSAGE_H_
#define _GATEKEEPER_MESSAGE_H_

class GatekeeperPingMessage: public NetworkMessage {
public:
  /*
   * This class copies the buffer passed in.
   */
  GatekeeperPingMessage(const uint8_t *msg_buf, size_t len) :
      NetworkMessage(kGateKeeper2Cli_PingReply) {
    m_buf = new uint8_t[len];
    m_buflen = len;
    memcpy(m_buf, msg_buf, len);
  }
  virtual ~GatekeeperPingMessage() {
    if (m_buf)
      delete[] m_buf;
  }

  // the message is only made if it's long enough
  virtual bool check_useable() const {
    return true;
  }

  uint32_t fill_iovecs(struct iovec *iov, uint32_t iov_ct, uint32_t start_at);
  uint32_t iovecs_written_bytes(uint32_t byte_ct, uint32_t start_at, bool *msg_done);
  uint32_t fill_buffer(uint8_t *buffer, size_t len, uint32_t start_at, bool *msg_done);

#ifdef DEBUG_ENABLE
  virtual bool persistable() const { return true; } // copies buffer
#endif
};

class GatekeeperClientMessage: public NetworkMessage {
public:
  static NetworkMessage* make_if_enough(const uint8_t *buf, size_t len);

  bool check_useable() const {
    return true;
  }

  virtual ~GatekeeperClientMessage() {
  }

  /*
   * Additional accessors
   */
  bool wants_file() const {
    return (m_type == kCli2GateKeeper_FileSrvIpAddressRequest);
  }
  uint32_t reqid() const {
    return m_reqid;
  }

protected:
  uint32_t m_reqid;
  uint8_t m_unknown;

  GatekeeperClientMessage(const uint8_t *msg_buf, size_t len, uint16_t type) :
      NetworkMessage(NULL, len, type) {
    m_reqid = read32(msg_buf, 2);
    m_unknown = msg_buf[6];
  }

#ifdef DEBUG_ENABLE
public:
  // message should not be queued
  // virtual bool persistable() const { return true; } // copies data
#endif
};

class GatekeeperServerMessage: public NetworkMessage {
public:
  GatekeeperServerMessage(bool for_file, uint32_t reqid, const char *ipaddr);

  virtual ~GatekeeperServerMessage() {
    if (m_buf)
      delete[] m_buf;
  }

  virtual uint32_t fill_iovecs(struct iovec *iov, uint32_t iov_ct, uint32_t start_at);
  virtual uint32_t iovecs_written_bytes(uint32_t byte_ct, uint32_t start_at, bool *msg_done);
  virtual uint32_t fill_buffer(uint8_t *buffer, size_t len, uint32_t start_at, bool *msg_done);

#ifdef DEBUG_ENABLE
  virtual bool persistable() const { return true; }
#endif
};

#endif /* _GATEKEEPER_MESSAGE_H_ */
