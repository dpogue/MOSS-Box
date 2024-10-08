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

#include <stdarg.h>
#include <pthread.h>
#include <iconv.h>

#include <sys/time.h>
#include <sys/uio.h> /* for struct iovec */

#include <stdexcept>
#include <list>
#include <vector>
#include <string>

#include "machine_arch.h"
#include "exceptions.h"
#include "typecodes.h"
#include "constants.h"
#include "protocol.h"
#include "msg_typecodes.h"
#include "util.h"
#include "UruString.h"
#include "PlKey.h"
#include "Buffer.h"

#include "Logger.h"
#include "SDL.h"
#include "NetworkMessage.h"
#include "BackendMessage.h"
#include "GameMessage.h"

NetworkMessage* GameMessage::make_if_enough(const uint8_t *buf, size_t len, int32_t *want_len, bool become_owner) {
  NetworkMessage *in = NULL;
  bool became_owner = false;
  if (len < 2) {
    *want_len = -1;
    return NULL;
  }
  uint16_t type = read16(buf, 0);
  switch (type) {
  case Cli2Game_PingRequest:
    if (len < 6) {
      *want_len = 6;
      return NULL;
    }
    in = new GamePingMessage(buf, 6);
    break;
  case Cli2Game_JoinAgeRequest:
    in = (NetworkMessage*) GameJoinRequest::make_if_enough(buf, len, want_len);
    break;
  case Cli2Game_PropagateBuffer:
    in = (NetworkMessage*) PropagateBufferMessage::make_if_enough(buf, len, want_len, become_owner);
    became_owner = true;
    break;
  case Cli2Game_GameMgrMsg:
    in = (NetworkMessage*) GameMgrMessage::make_if_enough(buf, len, want_len, become_owner);
    became_owner = true;
    break;
  default:
    in = new UnknownMessage(buf, len);
  }

  if (in && become_owner && !became_owner) {
    // this should never happen
#ifdef DEBUG_ENABLE
    throw std::logic_error("GameMessage ownership of buffer not taken");
#endif
    delete[] buf;
  }
  return in;
}

uint32_t GamePingMessage::fill_iovecs(struct iovec *iov, uint32_t iov_ct, uint32_t start_at) {
  if (iov_ct > 0) {
    iov[0].iov_base = m_buf + start_at;
    iov[0].iov_len = m_buflen - start_at;
    return 1;
  } else {
    return 0;
  }
}

uint32_t GamePingMessage::iovecs_written_bytes(uint32_t byte_ct, uint32_t start_at, bool *msg_done) {
  if (byte_ct + start_at >= m_buflen) {
    *msg_done = true;
    return (byte_ct + start_at) - m_buflen;
  } else {
    *msg_done = false;
    return 0;
  }
}

uint32_t GamePingMessage::fill_buffer(uint8_t *buffer, size_t len, uint32_t start_at, bool *msg_done) {
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

NetworkMessage* GameJoinRequest::make_if_enough(const uint8_t *buf, size_t len, int32_t *want_len) {
  if (len < 30) {
    *want_len = 30;
    return NULL;
  }

  uint32_t reqid = read32(buf, 2);
  uint32_t serverid = read32(buf, 6);
  kinum_t kinum = read32(buf, 26);

  return new GameJoinRequest(reqid, serverid, buf + 10, kinum);
}

uint32_t GameJoinReply::fill_iovecs(struct iovec *iov, uint32_t iov_ct, uint32_t start_at) {
  // bah, this is easier
  if (!m_buf) {
    m_buf = new uint8_t[10];
    m_buflen = 10;
  }
  bool msg_done;
  fill_buffer(m_buf, 10, 0, &msg_done);
  iov[0].iov_base = m_buf + start_at;
  iov[0].iov_len = m_buflen - start_at;
  return 1;
}

uint32_t GameJoinReply::iovecs_written_bytes(uint32_t byte_ct, uint32_t start_at, bool *msg_done) {
  if (byte_ct + start_at >= 10) {
    *msg_done = true;
    return (byte_ct + start_at) - 10;
  } else {
    *msg_done = false;
    return 0;
  }
}

uint32_t GameJoinReply::fill_buffer(uint8_t *buffer, size_t len, uint32_t start_at, bool *msg_done) {
  uint32_t i = 0;
  if (start_at < 2) {
    if (start_at == 0) {
      if (len < 2) {
        buffer[0] = (uint8_t) (m_type & 0xFF);
        *msg_done = false;
        return 1;
      }
      write16(buffer, 0, m_type);
      i += 2;
    } else {
      buffer[0] = (m_type & 0xFF00) >> 8;
      i++;
    }
    start_at = 0;
  } else {
    start_at -= 2;
  }
  uint8_t tmp[8];
  write32(tmp, 0, m_reqid);
  write32(tmp, 4, m_result);
  uint32_t wlen = 8 - start_at;
  if (len - i < wlen) {
    *msg_done = false;
    wlen = len - i;
  } else {
    *msg_done = true;
  }
  memcpy(buffer + i, tmp + start_at, wlen);
  return i + wlen;
}

uint32_t SharedGameMessage::fill_iovecs(struct iovec *iov, uint32_t iov_ct, uint32_t start_at) {
  if (!m_sbuf) {
    // if this happens, it's a programmer error
    return 0;
  }
  if (iov_ct > 0) {
    iov[0].iov_base = m_sbuf->buffer() + start_at;
    iov[0].iov_len = m_buflen - start_at;
    return 1;
  } else {
    return 0;
  }
}

uint32_t SharedGameMessage::iovecs_written_bytes(uint32_t byte_ct, uint32_t start_at, bool *msg_done) {
  if (!m_sbuf) {
    *msg_done = true;
    return 0;
  }
  if (byte_ct + start_at >= m_buflen) {
    *msg_done = true;
    return (byte_ct + start_at) - m_buflen;
  } else {
    *msg_done = false;
    return 0;
  }
}

uint32_t SharedGameMessage::fill_buffer(uint8_t *buffer, size_t len, uint32_t start_at, bool *msg_done) {
  if (!m_sbuf) {
    // if this happens, it's a programmer error
    *msg_done = true;
    return 0;
  }
  if (m_buflen - start_at <= len) {
    memcpy(buffer, m_sbuf->buffer() + start_at, m_buflen - start_at);
    *msg_done = true;
    return m_buflen - start_at;
  } else {
    memcpy(buffer, m_sbuf->buffer() + start_at, len);
    *msg_done = false;
    return len;
  }
}

void SharedGameMessage::make_own_copy() {
  if (m_sbuf) {
    if (!m_sbuf->is_owned()) {
      Buffer *oldbuf = m_sbuf;
      m_sbuf = new Buffer(oldbuf->len());
      memcpy(m_sbuf->buffer(), oldbuf->buffer(), oldbuf->len());
      delete oldbuf;
    }
  }
}

NetworkMessage* PropagateBufferMessage::make_if_enough(const uint8_t *buf, size_t len, int32_t *want_len, bool become_owner) {
  if (len < 10) {
    *want_len = -1;
    return NULL;
  }
  *want_len = read32(buf, 6) + 10;

  // The message length limit is pretty small: the biggest messages are
  // clothing SDL and possibly age SDLs. The only exception is voice messages
  // which can be up to some fairly large size that I don't know. (~1k?)
  // (Also note, due to bugs, messages can grow quite large, containing
  // tens and tens of plKeys (e.g. yeesha page objects in Relto), or the
  // Alcugs-killing harvester subworld flipping in Er'cana (samples up to
  // 3757 seen so far.) Also I see a 1199-long apparently-valid plAnimCmd
  // message, just as a sample...
  if (*want_len > 4500/*somewhat arbitrary guess*/) {
    throw overlong_message(*want_len);
  }

  if (*want_len > (int32_t) len) {
    return NULL;
  }
  uint16_t type = read16(buf, 2); // transmitted as 32 bits (little-endian)
  return new PropagateBufferMessage(type, buf, *want_len, become_owner);
}

uint32_t PropagateBufferMessage::body_offset() const {
  uint32_t offset = 12; // skip wrapper and type
  uint32_t bitvector = read32(m_sbuf->buffer(), offset);
  offset += 4;
  if (bitvector & HasTimeSent) {
    offset += 8;
  }
  if (bitvector & HasTransactionID) {
    // should never happen
    offset += 4;
  }
  if (bitvector & HasPlayerID) {
    offset += 4;
  }
  if (bitvector & HasAcctUUID) {
    // should never happen
    offset += 16;
  }
  if (bitvector & AllowTimeOut) {
    // IP address and port present!?!
    // should never happen
    offset += 6;
  }
  return offset;
}

uint32_t PropagateBufferMessage::body_offset(uint32_t flags) {
  uint32_t offset = 16; // wrapper, type, and bitvector
  if (flags & HasTimeSent) {
    offset += 8;
  }
  if (flags & HasTransactionID) {
    // should never happen
    offset += 4;
  }
  if (flags & HasPlayerID) {
    offset += 4;
  }
  if (flags & HasAcctUUID) {
    // should never happen
    offset += 16;
  }
  if (flags & AllowTimeOut) {
    // IP address and port present!?!
    // should never happen
    offset += 6;
  }
  return offset;
}

kinum_t PropagateBufferMessage::kinum() const {
  kinum_t kinum = 0;
  uint8_t *buf = m_sbuf->buffer();
  uint32_t bitvector = read32(buf, 12);
  uint32_t offset = 16;
  if (bitvector & HasTimeSent) {
    offset += 8;
  }
  if (bitvector & HasTransactionID) {
    // should never happen
    offset += 4;
  }
  if (bitvector & HasPlayerID) {
    kinum = (kinum_t) read32(buf, offset);
  }
  return kinum;
}

void PropagateBufferMessage::set_timestamp() const {
  uint8_t *buf = m_sbuf->buffer();
  uint32_t bitvector = read32(buf, 12);
  if (bitvector & HasTimeSent) {
    struct timeval t;
    gettimeofday(&t, NULL);
    write32(buf, 16, t.tv_sec);
    write32(buf, 20, t.tv_usec);
  }
}

uint32_t PropagateBufferMessage::format_header(uint16_t subtype, uint32_t message_len, uint32_t flags, kinum_t ki) {
  uint8_t *buf = m_sbuf->buffer();
  uint32_t offset = 16;
  write16(buf, 0, Game2Cli_PropagateBuffer);
  write32(buf, 2, subtype);
  write32(buf, 6, message_len - 10);
  write16(buf, 10, subtype);
  write32(buf, 12, flags);
  if (flags & HasTimeSent) {
    // skip 8 bytes of timestamp (filled in later)
    offset += 8;
  }
  if (flags & HasPlayerID) {
    write32(buf, offset, ki);
    offset += 4;
  }
  return offset;
}

PlNetMsgGroupOwner::PlNetMsgGroupOwner(bool is_owner) :
    PropagateBufferMessage() {
  // fixed-length message
  m_buflen = body_offset(HasTimeSent | IsSystemMessage) + 12;
  m_sbuf = new Buffer(m_buflen);

  uint8_t *buf = m_sbuf->buffer();
  uint32_t offset = format_header(plNetMsgGroupOwner, m_buflen, HasTimeSent | IsSystemMessage);
  set_timestamp();
  if (offset != 24) {
    // bug in format_header, should never happen
    throw std::logic_error("Bug in PropagateBufferMessage::format_header()");
  }
  write32(buf, 24, 1); // unk "Mask"
  write32(buf, 28, 0xff000001);
  write16(buf, 32, 0x0004);
  buf[34] = 0x00; // unk
  buf[35] = (is_owner ? 1 : 0);
}

PlNetMsgSDLState::PlNetMsgSDLState(SDLState *sdl, bool is_initial_sdl, bool use_timestamp) :
    PropagateBufferMessage() {
  uint32_t msg_flags = (use_timestamp ? HasTimeSent : 0);
  uint32_t offset = body_offset(msg_flags);
  m_buflen = offset + sdl->send_len() + 3;
  m_sbuf = new Buffer(m_buflen);

  uint8_t *buf = m_sbuf->buffer();
  int32_t ret = sdl->write_msg(buf + offset, m_buflen - offset);
  if (ret < 0) {
    // XXX bug in sdl->send_len() -- serious problem
    // NOTE: sending this empty message crashes the client
  } else {
    offset += (uint32_t) ret;
    // XXX XXX I really do not know what values these should be
    // (2 flags and "end thing") -- when it's figured out they may
    // need to be stored in the SDLState or passed as arguments
    buf[offset++] = (is_initial_sdl ? 0x01 : 0x00);
    buf[offset++] = (is_initial_sdl ? 0x01 : 0x00);
    buf[offset++] = 0x00; // end thing
  }
  // the SDL message might have been compressed, making it shorter, so 
  // set the final length now
  m_buflen = offset;
  format_header(plNetMsgSDLState, m_buflen, msg_flags);
  if (use_timestamp) {
    set_timestamp();
  }
}

PlNetMsgInitialAgeStateSent::PlNetMsgInitialAgeStateSent(uint32_t howmany) :
    PropagateBufferMessage() {
  // fixed-length message
  m_buflen = body_offset(HasTimeSent) + 4;
  m_sbuf = new Buffer(m_buflen);

  uint8_t *buf = m_sbuf->buffer();
  uint32_t offset = format_header(plNetMsgInitialAgeStateSent, m_buflen, HasTimeSent | IsSystemMessage);
  set_timestamp();
  if (offset != 24) {
    // bug in format_header, should never happen
    throw std::logic_error("Bug in PropagateBufferMessage::format_header()");
  }
  write32(buf, 24, howmany);
}

#ifdef STANDALONE
PlNetMsgMembersMsg::PlNetMsgMembersMsg(kinum_t requester_ki)
  : PropagateBufferMessage()
{
  // fixed-length message
  m_buflen = body_offset(HasTimeSent|HasPlayerID|IsSystemMessage) + 2;
  m_sbuf = new Buffer(m_buflen);

  uint8_t *buf = m_sbuf->buffer();
  uint32_t offset = format_header(plNetMsgMembersList, m_buflen,
             HasTimeSent|HasPlayerID|IsSystemMessage,
             requester_ki);
  set_timestamp();
  if (offset != 28) {
    // bug in format_header, should never happen
    throw std::logic_error("Bug in PropagateBufferMessage::format_header()");
  }
  write16(buf, 28, 0);
}
#else
PlNetMsgMembersMsg::PlNetMsgMembersMsg(kinum_t requester_ki) :
    PropagateBufferMessage(), m_requester_ki(requester_ki) {
}

void PlNetMsgMembersMsg::addMember(kinum_t ki, UruString *name, const PlKey *key, bool pagein) {
  struct info news;
  news.ki = ki;
  news.name = name;
  news.key = key;
  news.pagein = pagein;
  m_members.push_back(news);
}

void PlNetMsgMembersMsg::finalize(bool list_or_update) {
  m_buflen = body_offset(HasTimeSent | HasPlayerID | IsSystemMessage);
  std::list<struct info>::iterator iter;
  for (iter = m_members.begin(); iter != m_members.end(); iter++) {
    m_buflen += 10; // flags, content, and kinum
    if (iter->name && iter->pagein) {
      m_buflen += iter->name->send_len(true, false, false) + 1;
    }
    if (iter->key) {
      m_buflen += iter->key->send_len();
    } else {
      m_buflen += PlKey::null_send_len();
    }
  }
  if (list_or_update) {
    m_buflen += 2; // for list length
  } else {
    m_buflen += 1; // for page flag
  }
  m_sbuf = new Buffer(m_buflen);

  uint8_t *buf = m_sbuf->buffer();
  uint32_t offset = format_header(
      (list_or_update ? plNetMsgMembersList : plNetMsgMemberUpdate),
      m_buflen,
      HasTimeSent | HasPlayerID | IsSystemMessage,
      m_requester_ki);
  if (list_or_update) {
    write16(buf, offset, m_members.size());
    offset += 2;
  }
  for (iter = m_members.begin(); iter != m_members.end(); iter++) {
    write32(buf, offset, 0); // unk
    offset += 4;
    // contents
    uint32_t contents = PlayerID;
    if (iter->name && iter->pagein) {
      contents |= PlayerName | CCRLevel;
    }
    write16(buf, offset, contents);
    offset += 2;
    if (contents & PlayerID) {
      write32(buf, offset, iter->ki);
      offset += 4;
    }
    if (contents & PlayerName) {
      uint32_t len = iter->name->send_len(true, false, false);
      // this is NOT NOT NOT an UruString! it must not be bitflipped
      memcpy(buf + offset, iter->name->get_str(true, false, false, false), len);
      offset += len;
    }
    if (contents & CCRLevel) {
      // XXX nonzero values not supported
      buf[offset++] = 0;
    }
    if (iter->key) {
      offset += iter->key->write_out(buf + offset, m_buflen - offset);
    } else {
      offset += PlKey::write_null_key(buf + offset, m_buflen - offset);
    }

    if (!list_or_update) {
      // there should be only one but I am not checking it, callers beware!
      buf[offset++] = iter->pagein ? 1 : 0;
      break;
    }
  }
  if (offset != m_buflen) {
    // XXX uh-oh
    if (offset < m_buflen) {
      // don't send uninitialized memory
      memset(buf + offset, 0, m_buflen - offset);
    }
  }

  set_timestamp();
}
#endif /* !STANDALONE */

uint16_t PlNetMsgGameMessage::msg_type() const {
  const uint8_t *buf = buffer();
  if (!buf) {
    return no_plType;
  }
  return read16(buf, body_offset() + 9);
}

uint32_t PlNetMsgGameMessage::msg_offset() const {
  return body_offset() + 11;
}

void PlNetMsgGameMessage::build_msg(uint32_t prop_flags, kinum_t prop_ki, const uint8_t *msg_buf, size_t msg_len,
    uint16_t msg_type, uint32_t msg_flags, uint8_t end_thing, PlKey *object, PlKey **subobjects, uint32_t subobject_count,
    bool no_compress) {
  m_buflen = body_offset(prop_flags) + msg_len + 11 + 5 + 12 + 1;
  if (object) {
    m_buflen += object->send_len();
  }
  for (uint32_t i = 0; i < subobject_count; i++) {
    m_buflen += 1 + subobjects[i]->send_len();
  }
  m_sbuf = new Buffer(m_buflen);

  uint8_t *buf = m_sbuf->buffer();
  // now, write the uncompressed version of the message
  uint32_t start_at = body_offset(prop_flags);
  uint32_t offset = start_at + 9;
  uint32_t contents_at = offset;
  write16(buf, offset, msg_type);
  offset += 2; // 11 above
  buf[offset++] = (object ? 1 : 0);
  if (object) {
    offset += object->write_out(buf + offset, m_buflen - offset);
  }
  write32(buf, offset, subobject_count);
  offset += 4; // 5 above
  for (uint32_t i = 0; i < subobject_count; i++) {
    buf[offset++] = 1;
    offset += subobjects[i]->write_out(buf + offset, m_buflen - offset);
  }
  write32(buf, offset, 0);
  offset += 4;
  write32(buf, offset, 0);
  offset += 4;
  write32(buf, offset, msg_flags);
  offset += 4; // 12 above
  memcpy(buf + offset, msg_buf, msg_len); // the message itself
  offset += msg_len;

  if (offset + 1 != m_buflen) { // +1 is for "end thing" yet to come
    // bug in PlKey::send_len()
    throw std::logic_error("PlKey::send_len() returned different length than PlKey::write_out()");
  }

  uint32_t len = offset - contents_at;
  write32(buf, start_at, 0);
  buf[start_at + 4] = CompressionNone;
  write32(buf, start_at + 5, len);
  // now check on compression
  if (!no_compress) {
    uint32_t len2 = do_message_compression(buf + start_at);
    if (len2) {
      // set m_buflen to actual filled-in size
      offset = contents_at + len2;
      m_buflen = offset + 1;
    }
  }
  buf[offset++] = end_thing; // 1 above

  // finally, do the outer layer
  format_header(plNetMsgGameMessage, m_buflen, prop_flags, prop_ki);
  if (prop_flags & HasTimeSent) {
    set_timestamp();
  }
}

PlServerReplyMsg::PlServerReplyMsg(bool grant, PlKey &key) :
    PlNetMsgGameMessage() {
  uint32_t result = (grant ? 1 : 0);
  PlKey *keyp = &key;
  build_msg(HasTimeSent, 0, (uint8_t*) &result, 4, plServerReplyMsg, 0x00000800/*unk*/, 0, NULL, &keyp, 1);
}

// XXX retrofit this and *all* clone stuff to use build_msg() -- that means
// parsing the message more, keeping track of whether it's plLoadAvatarMsg
// or plLoadCloneMsg, using NetClientMgr_KEY for the subobject, etc.
// XXX do SDL too afterwards
PlNetMsgLoadClone::PlNetMsgLoadClone(const uint8_t *clonebuf, size_t clonelen,
                                      PlKey &obj_name, kinum_t kinum, bool is_load, uint32_t player) :
    PlNetMsgGameMessage() {
  m_buflen = body_offset(HasTimeSent | HasPlayerID) + clonelen + obj_name.send_len() + 3;
  m_sbuf = new Buffer(m_buflen);
  uint8_t *buf = m_sbuf->buffer();
  uint32_t offset = format_header(plNetMsgLoadClone, m_buflen, HasTimeSent | HasPlayerID, kinum);
  set_timestamp();
  memcpy(buf + offset, clonebuf, clonelen);
  offset += clonelen;
  offset += obj_name.write_out(buf + offset, m_buflen - offset);
  if (offset + 3 != m_buflen) {
    // bug in PlKey::send_len()
    throw std::logic_error("PlKey::send_len() returned different length than PlKey::write_out()");
  }
  buf[offset++] = (uint8_t) (player & 0xFF);
  buf[offset] = (is_load ? PlKey::HasCloneIDs : 0x00);
  buf[offset + 1] = buf[offset];
}

PlNetMsgGameMessageDirected::PlNetMsgGameMessageDirected(TrackMsgForward_BackendMessage *track_fwded) :
    PlNetMsgGameMessage(), m_msg(track_fwded) {
  m_msg->add_ref();
  // m_sbuf is backed by m_msg
  m_buflen = m_msg->fwd_msg_len();
  m_sbuf = new Buffer(m_buflen, m_msg->fwd_msg(), false);
  set_timestamp();
}

PlNetMsgGameMessageDirected::~PlNetMsgGameMessageDirected() {
  if (m_msg->del_ref() < 1) {
    delete m_msg;
  }
}

NetworkMessage* GameMgrMessage::make_if_enough(const uint8_t *buf, size_t len, int32_t *want_len, bool become_owner) {
  if (len < 6) {
    *want_len = -1;
    return NULL;
  }
  *want_len = read32(buf, 2) + 6;

  if (*want_len > 42 + 5 + 516 + 160/*marker game setup message*/) {
    throw overlong_message(*want_len);
  }

  if (*want_len > (int32_t) len) {
    return NULL;
  }
  GameMgrMessage *msg = new GameMgrMessage(buf, *want_len, become_owner);
  return msg;
}

GameMgrMessage::GameMgrMessage(const uint8_t *buf, size_t len, bool become_owner) :
    SharedGameMessage(Cli2Game_GameMgrMsg, buf, len, become_owner), m_msgtype(0), m_reqid(0), m_gameid(0) {
  if (m_buflen >= 18) {
    m_msgtype = read32(buf, 6);
    m_reqid = read32(buf, 10);
    m_gameid = read32(buf, 14);
  }
}

bool GameMgrMessage::check_useable() const {
  if (m_buflen < 18) {
    return false;
  }
  if (is_setup()) {
    uint32_t off = header_len;
    if (m_msgtype == 0) {
      // marker game
    } else {
      // all others
      off += 8;
    }
    if (m_buflen < off + UUID_RAW_LEN + 4) {
      return false;
    }
    // some game types require more data after this; let it be type-specific
  }
  // type-specific checks are still required
  return true;
}

uint32_t GameMgrMessage::format_header(size_t body_len) {
  size_t total_len = header_len - 6 + body_len;
  uint8_t *buf = m_sbuf->buffer();
  write16(buf, 0, Game2Cli_GameMgrMsg);
  write32(buf, 2, total_len);
  write32(buf, 6, m_msgtype);
  write32(buf, 10, m_reqid);
  write32(buf, 14, m_gameid);
  write32(buf, 18, total_len);
  return 22;
}

void GameMgrMessage::clobber_msgtype(uint32_t newtype) {
#ifdef DEBUG_ENABLE
  if (!persistable()) {
    // shouldn't be writing to buffer
    throw std::logic_error("Changing GameMgr message without "
         "creating local copy");
  }
#endif
  m_msgtype = newtype;
  uint8_t *buf = m_sbuf->buffer();
  write32(buf, 6, m_msgtype);
}

GameMgr_Setup_Reply::GameMgr_Setup_Reply(uint32_t gameid, uint32_t reqid, kinum_t clientid, const uint8_t *uuid) :
    GameMgrMessage(0, reqid, 0U) {
  m_buflen = header_len + 12 + UUID_RAW_LEN;
  m_sbuf = new Buffer(m_buflen);
  uint8_t *buf = m_sbuf->buffer();
  uint32_t off = header_len;
  format_header(m_buflen - header_len);

  write32(buf, off, 0);
  off += 4;
  write32(buf, off, clientid);
  off += 4;
  memcpy(buf + off, uuid, UUID_RAW_LEN);
  off += UUID_RAW_LEN;
  write32(buf, off, gameid);
}

GameMgr_Simple_Message::GameMgr_Simple_Message(uint32_t gameid, uint32_t mgr_type) :
    GameMgrMessage(mgr_type, 0, gameid) {
  m_buflen = header_len;
  m_sbuf = new Buffer(m_buflen);
  format_header(0);
}

GameMgr_OneByte_Message::GameMgr_OneByte_Message(uint32_t gameid, uint32_t mgr_type, uint8_t data) :
    GameMgrMessage(mgr_type, 0, gameid) {
  m_buflen = header_len + 1;
  m_sbuf = new Buffer(m_buflen);
  uint8_t *buf = m_sbuf->buffer();
  buf[header_len] = data;
  format_header(1);
}

GameMgr_FourByte_Message::GameMgr_FourByte_Message(uint32_t gameid, uint32_t mgr_type, uint32_t data) :
    GameMgrMessage(mgr_type, 0, gameid) {
  m_buflen = header_len + 4;
  m_sbuf = new Buffer(m_buflen);
  uint8_t *buf = m_sbuf->buffer();
  write32(buf, header_len, data);
  format_header(4);
}

GameMgr_VarSync_VarCreated_Message::GameMgr_VarSync_VarCreated_Message(uint32_t gameid, uint32_t idx, UruString &name,
    double value) :
    GameMgrMessage(Srv2Cli_VarSync_NumericVarCreated, 0, gameid) {
  // these have a fixed-size buffer in them, so the message size is
  // constant
  size_t body_len = 524;
  // XXX it is unnecessary to allocate all this memory; we could avoid it
  // in most cases (unless encryption is disabled) with a custom data
  // structure and fill_buffer()
  m_buflen = header_len + body_len;
  m_sbuf = new Buffer(m_buflen);
  uint8_t *buf = m_sbuf->buffer();
  uint32_t off = header_len;
  format_header(body_len);

  size_t str_len = name.send_len(false, true, true);
  memcpy(buf + off, name.get_str(false, true, true), str_len);
  memset(buf + off + str_len, 0, 512 - str_len);
  off += 512;
  write32(buf, off, idx);
  off += 4;
  write_double(buf, off, value);
}

GameMgr_Marker_GameCreated_Message::GameMgr_Marker_GameCreated_Message(uint32_t gameid, const uint8_t *uuid) :
    GameMgrMessage(Srv2Cli_Marker_TemplateCreated, 0, gameid) {
  m_buflen = header_len + 160/*:-P*/;
  m_sbuf = new Buffer(m_buflen);
  uint8_t *buf = m_sbuf->buffer() + header_len;
  format_header(160);
  // this is kind of a silly set of hoops to jump through but I'm not in the
  // mood to write a special uuid->widestring converter
  char tempstr[UUID_STR_LEN];
  format_uuid(uuid, tempstr);
  UruString temp(tempstr, false);
  memcpy(buf, temp.get_str(false, true, true), UUID_STR_LEN * 2);
  memset(buf + (UUID_STR_LEN * 2), 0, 160 - (UUID_STR_LEN * 2));
}

GameMgr_Marker_GameNameChanged_Message::GameMgr_Marker_GameNameChanged_Message(uint32_t gameid, UruString *name) :
    GameMgrMessage(Srv2Cli_Marker_GameNameChanged, 0, gameid) {
  m_buflen = header_len + 512/*:-P*/;
  m_sbuf = new Buffer(m_buflen);
  uint8_t *buf = m_sbuf->buffer() + header_len;
  format_header(512);
  uint32_t off = name->send_len(false, true, true);
  memcpy(buf, name->get_str(false, true, true), off);
  memset(buf + off, 0, 512 - off);
}

GameMgr_Marker_MarkerAdded_Message::GameMgr_Marker_MarkerAdded_Message(
      uint32_t gameid, const Marker_BackendMessage::marker_data_t *data,
      UruString *name, UruString *age, BackendMessage *orig_msg) :
    GameMgrMessage(Srv2Cli_Marker_MarkerAdded, 0, gameid),
    m_data(data), m_marker_name(*name, true), m_age_name(*age, true), m_backing_msg(orig_msg), m_zeros(NULL) {
  orig_msg->add_ref();
  m_buflen = header_len;
  m_sbuf = new Buffer(m_buflen);
  format_header(28 + 512 + 160);
}

GameMgr_Marker_MarkerAdded_Message::~GameMgr_Marker_MarkerAdded_Message() {
  if (m_backing_msg->del_ref() < 1) {
    delete m_backing_msg;
  }
  if (m_zeros) {
    delete[] m_zeros;
  }
}

uint32_t GameMgr_Marker_MarkerAdded_Message::fill_iovecs(struct iovec *iov, uint32_t iov_ct, uint32_t start_at) {
  if (!m_sbuf) {
    // if this happens, it's a programmer error
    return 0;
  }

  uint32_t len, len2;
  len = m_marker_name.send_len(false, true, true);
  len2 = m_age_name.send_len(false, true, true);
  uint32_t rlen = 512 - len, rlen2 = 160 - len2;
  if (!m_zeros) {
    uint32_t buflen = MIN(rlen, rlen2);
    m_zeros = new uint8_t[buflen];
    memset(m_zeros, 0, buflen);
  }

  uint32_t used = 0;
  if (iov_ct <= used) {
    return used;
  }
  if (start_at < header_len) {
    iov[used].iov_base = m_sbuf->buffer() + start_at;
    iov[used].iov_len = m_buflen - start_at;
    used++;
    start_at = 0;
  } else {
    start_at -= header_len;
  }
  if (iov_ct <= used) {
    return used;
  }
  if (start_at < 28) {
    iov[used].iov_base = ((uint8_t*) m_data) + start_at;
    iov[used].iov_len = 28 - start_at;
    used++;
    start_at = 0;
  } else {
    start_at -= 28;
  }
  if (iov_ct <= used) {
    return used;
  }
  if (start_at < len) {
    iov[used].iov_base = (void*) ((m_marker_name.get_str(false, true, true)) + start_at);
    iov[used].iov_len = len - start_at;
    used++;
    start_at = 0;
  } else {
    start_at -= len;
  }
  if (iov_ct <= used) {
    return used;
  }
  if (start_at < rlen && rlen > 0) {
    iov[used].iov_base = m_zeros;
    iov[used].iov_len = rlen - start_at;
    used++;
    start_at = 0;
  } else {
    start_at -= rlen;
  }
  if (iov_ct <= used) {
    return used;
  }
  if (start_at < len2) {
    iov[used].iov_base = (void*) ((m_age_name.get_str(false, true, true)) + start_at);
    iov[used].iov_len = len2 - start_at;
    used++;
    start_at = 0;
  } else {
    start_at -= len2;
  }
  if (iov_ct <= used) {
    return used;
  }
  if (start_at < rlen2 && rlen2 > 0) {
    iov[used].iov_base = m_zeros;
    iov[used].iov_len = rlen2 - start_at;
    used++;
    start_at = 0;
  } else {
    start_at -= rlen2;
  }
  return used;
}

uint32_t GameMgr_Marker_MarkerAdded_Message::iovecs_written_bytes(uint32_t byte_ct, uint32_t start_at, bool *msg_done) {
  if (!m_sbuf) {
    *msg_done = true;
    return 0;
  }
  // fixed-length message
  uint32_t mylen = header_len + 28 + 512 + 160;
  if (byte_ct + start_at >= mylen) {
    *msg_done = true;
    return (byte_ct + start_at) - mylen;
  } else {
    *msg_done = false;
    return 0;
  }
}

uint32_t GameMgr_Marker_MarkerAdded_Message::fill_buffer(uint8_t *buffer, size_t len, uint32_t start_at, bool *msg_done) {
  if (!m_sbuf) {
    // if this happens, it's a programmer error
    *msg_done = true;
    return 0;
  }

  *msg_done = true;
  uint32_t wlen, offset = 0;
  if (start_at < header_len) {
    wlen = header_len - start_at;
    if (len - offset < wlen) {
      *msg_done = false;
      wlen = len - offset;
    }
    memcpy(buffer + offset, m_sbuf->buffer() + start_at, wlen);
    offset += wlen;
    start_at = 0;
  } else {
    start_at -= header_len;
  }
  if (!*msg_done) {
    return offset;
  }
  if (start_at < 28) {
    wlen = 28 - start_at;
    if (len - offset < wlen) {
      *msg_done = false;
      wlen = len - offset;
    }
    memcpy(buffer + offset, ((uint8_t*) m_data) + start_at, wlen);
    offset += wlen;
    start_at = 0;
  } else {
    start_at -= 28;
  }
  if (!*msg_done) {
    return offset;
  }
  uint32_t slen = m_marker_name.send_len(false, true, true);
  if (start_at < slen) {
    wlen = slen - start_at;
    if (len - offset < wlen) {
      *msg_done = false;
      wlen = len - offset;
    }
    memcpy(buffer + offset, (m_marker_name.get_str(false, true, true)) + start_at, wlen);
    offset += wlen;
    start_at = 0;
  } else {
    start_at -= slen;
  }
  if (!*msg_done) {
    return offset;
  }
  slen = 512 - slen;
  if (start_at < slen) {
    wlen = slen - start_at;
    if (len - offset < wlen) {
      *msg_done = false;
      wlen = len - offset;
    }
    memset(buffer + offset, 0, wlen);
    offset += wlen;
    start_at = 0;
  } else {
    start_at -= slen;
  }
  if (!*msg_done) {
    return offset;
  }
  slen = m_age_name.send_len(false, true, true);
  if (start_at < slen) {
    wlen = slen - start_at;
    if (len - offset < wlen) {
      *msg_done = false;
      wlen = len - offset;
    }
    memcpy(buffer + offset, (m_age_name.get_str(false, true, true)) + start_at, wlen);
    offset += wlen;
    start_at = 0;
  } else {
    start_at -= slen;
  }
  if (!*msg_done) {
    return offset;
  }
  slen = 160 - slen;
  if (start_at < slen) {
    wlen = slen - start_at;
    if (len - offset < wlen) {
      *msg_done = false;
      wlen = len - offset;
    }
    memset(buffer + offset, 0, wlen);
    offset += wlen;
    start_at = 0;
  } else {
    start_at -= slen;
  }
  return offset;
}

GameMgr_Marker_MarkerCaptured_Message::GameMgr_Marker_MarkerCaptured_Message(uint32_t gameid, int32_t marker, char value) :
    GameMgrMessage(Srv2Cli_Marker_MarkerCaptured, 0, gameid) {
  m_buflen = header_len + 5;
  m_sbuf = new Buffer(m_buflen);
  uint8_t *buf = m_sbuf->buffer();
  write32(buf, header_len, marker);
  buf[header_len + 4] = value;
  format_header(5);
}

GameMgr_Marker_MarkerNameChanged_Message::GameMgr_Marker_MarkerNameChanged_Message(uint32_t gameid, int32_t marker,
    UruString *name) :
    GameMgrMessage(Srv2Cli_Marker_MarkerNameChanged, 0, gameid) {
  m_buflen = header_len + 4 + 512/*:-P*/;
  m_sbuf = new Buffer(m_buflen);
  uint8_t *buf = m_sbuf->buffer() + header_len;
  format_header(516);
  write32(buf, 0, marker);
  uint32_t off = name->send_len(false, true, true);
  memcpy(buf + 4, name->get_str(false, true, true), off);
  memset(buf + 4 + off, 0, 512 - off);
}

GameMgr_BlueSpiral_ClothOrder_Message::GameMgr_BlueSpiral_ClothOrder_Message(uint32_t gameid, const uint8_t *order) :
    GameMgrMessage(Srv2Cli_BlueSpiral_ClothOrder, 0, gameid) {
  m_buflen = header_len + 7;
  m_sbuf = new Buffer(m_buflen);
  uint8_t *buf = m_sbuf->buffer() + header_len;
  format_header(7);
  memcpy(buf, order, 7);
}

GameMgr_Heek_PlayGame_Message::GameMgr_Heek_PlayGame_Message(uint32_t gameid, bool playing, bool single, bool enable) :
    GameMgrMessage(Srv2Cli_Heek_PlayGame, 0, gameid) {
  m_buflen = header_len + 3;
  m_sbuf = new Buffer(m_buflen);
  uint8_t *buf = m_sbuf->buffer() + header_len;
  format_header(3);
  buf[0] = playing ? 1 : 0;
  buf[1] = single ? 1 : 0;
  buf[2] = enable ? 1 : 0;
}

GameMgr_Heek_Welcome_Message::GameMgr_Heek_Welcome_Message(uint32_t gameid, int32_t score, uint32_t rank, UruString &name) :
    GameMgrMessage(Srv2Cli_Heek_Welcome, 0, gameid) {
  m_buflen = header_len + 8 + 512/*:-P*/;
  m_sbuf = new Buffer(m_buflen);
  uint8_t *buf = m_sbuf->buffer() + header_len;
  format_header(8 + 512);
  write32(buf, 0, score);
  write32(buf, 4, rank);
  buf += 8;
  uint32_t off = name.send_len(false, true, true);
  memcpy(buf, name.get_str(false, true, true), off);
  memset(buf + off, 0, 512 - off);
}

GameMgr_Heek_PointUpdate_Message::GameMgr_Heek_PointUpdate_Message(uint32_t gameid, bool send_message, int32_t score,
    uint32_t rank) :
    GameMgrMessage(Srv2Cli_Heek_PointUpdate, 0, gameid) {
  m_buflen = header_len + 9;
  m_sbuf = new Buffer(m_buflen);
  uint8_t *buf = m_sbuf->buffer() + header_len;
  format_header(9);
  buf[0] = send_message ? 1 : 0;
  write32(buf, 1, score);
  write32(buf, 5, rank);
}

GameMgr_Heek_WinLose_Message::GameMgr_Heek_WinLose_Message(uint32_t gameid, bool win, uint8_t choice) :
    GameMgrMessage(Srv2Cli_Heek_WinLose, 0, gameid) {
  m_buflen = header_len + 2;
  m_sbuf = new Buffer(m_buflen);
  uint8_t *buf = m_sbuf->buffer() + header_len;
  format_header(2);
  buf[0] = win ? 1 : 0;
  buf[1] = choice;
}

GameMgr_Heek_Lights_Message::GameMgr_Heek_Lights_Message(uint32_t gameid, uint32_t light, GameMgr_Heek_Lights_Message::type_t type) :
    GameMgrMessage(Srv2Cli_Heek_LightState, 0, gameid) {
  m_buflen = header_len + 2;
  m_sbuf = new Buffer(m_buflen);
  uint8_t *buf = m_sbuf->buffer() + header_len;
  format_header(2);
  buf[0] = (uint8_t) light;
  buf[1] = (uint8_t) type;
}
