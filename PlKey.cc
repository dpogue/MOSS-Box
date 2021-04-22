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

#include <iconv.h>

#include <stdexcept>

#include "machine_arch.h"
#include "exceptions.h"

#include "UruString.h"
#include "PlKey.h"

uint32_t PlKey::read_in(const uint8_t *buf, size_t buflen) {
  // corresponds to CWE plUoid.cpp plUoid::Read()
  if (buflen < 1) {
    throw truncated_message("Buffer too short for plKey");
  }

  uint32_t needlen = 11;
  needlen += 4;
  if (buf[0] & HasLoadMask) {
    needlen += 2;
  }
  if (buflen < needlen) {
    throw truncated_message("Buffer too short for plKey");
  }

  uint32_t offset = 0;
  m_contents = buf[offset++];
  m_locsequencenumber = read32(buf, offset);
  offset += 4;
  m_locflags = read16(buf, offset);
  offset += 2;
  if (m_contents & HasLoadMask) {
    m_qualitycapability = buf[offset++];
  } else {
    m_qualitycapability = 0;
  }
  m_classtype = read16(buf, offset);
  offset += 2;
  m_objectid = read32(buf, offset);
  offset += 4;
  // NOTE: we are not calling delete_name()
  m_name = new UruString(buf + offset, buflen - offset, true, false);
  offset += m_name->arrival_len();

  if ((m_contents & HasCloneIDs) && (buflen < offset + 8)) {
    throw truncated_message("Buffer too short for plKey");
  }

  if (m_contents & HasCloneIDs) {
    m_cloneid = read16(buf, offset);
    offset += 2;
    /* dummy = */ read16(buf, offset);
    offset += 2;
    m_cloneplayerid = read32(buf, offset);
    offset += 4;
  } else {
    m_cloneid = m_cloneplayerid = 0;
  }

  return offset;
}

uint32_t PlKey::send_len() const {
  uint32_t len = 9;
  len += 4;
  if (m_contents & HasLoadMask) {
    len += 1;
  }
  if (m_contents & HasCloneIDs) {
    len += 8;
  }
  if (m_name) {
    len += m_name->send_len(true, false, false);
  } else {
    len += 2;
  }
  return len;
}

uint32_t PlKey::write_out(uint8_t *buf, size_t buflen, bool bitflip) const {
  if (buflen < send_len()) {
    // XXX programmer error
    return 0;
  }
  uint32_t offset = 0;
  buf[offset++] = m_contents;
  write32(buf, offset, m_locsequencenumber);
  offset += 4;
  write16(buf, offset, m_locflags);
  offset += 2;
  if (m_contents & HasLoadMask) {
    buf[offset++] = m_qualitycapability;
  }
  write16(buf, offset, m_classtype);
  offset += 2;
  write32(buf, offset, m_objectid);
  offset += 4;
  /*
   * Note that in CWE hsStream.cpp WriteSafeString() that the length
   * *always* is ORed with 0xf000, but the UruString() implementation
   * does not issue the length (in the first 2 bytes) in this way.
   * Here where m_name is not defined, we *do* write the zero length
   * ORed with 0xf000.
   */
  if (m_name) {
    uint32_t l = m_name->send_len(true, false, false);
    memcpy(buf + offset, m_name->get_str(true, false, false, bitflip), l);
    offset += l;
  } else {
    write16(buf, offset, /* len | */ 0xf000);
    offset += 2;
  }
  if (m_contents & HasCloneIDs) {
    write16(buf, offset, m_cloneid);
    offset += 2;
    write16(buf, offset, /*dummy*/0);
    offset += 2;
    write32(buf, offset, m_cloneplayerid);
    offset += 4;
  }
  return offset;
}

char* PlKey::c_str(char *buf, size_t bufsize) {
  char flagbuf[128];
  char *f = flagbuf;

  if (m_locflags & LocalOnly) {
    f += strlcpy(f, "LocalOnly", sizeof(flagbuf) - (f - flagbuf));
  }
  if (m_locflags & Volatile) {
    if ((f - flagbuf) > 0 && (f - flagbuf) < sizeof(flagbuf))
      *f++ = '|';
    f += strlcpy(f, "Volatile", sizeof(flagbuf) - (f - flagbuf));
  }
  if (m_locflags & Reserved) {
    if ((f - flagbuf) > 0 && (f - flagbuf) < sizeof(flagbuf))
      *f++ = '|';
    f += strlcpy(f, "Reserved", sizeof(flagbuf) - (f - flagbuf));
  }
  if (m_locflags & BuiltIn) {
    if ((f - flagbuf) > 0 && (f - flagbuf) < sizeof(flagbuf))
      *f++ = '|';
    f += strlcpy(f, "BuiltIn", sizeof(flagbuf) - (f - flagbuf));
  }
  if (m_locflags & Itinerant) {
    if ((f - flagbuf) > 0 && (f - flagbuf) < sizeof(flagbuf))
      *f++ = '|';
    f += strlcpy(f, "Itinerant", sizeof(flagbuf) - (f - flagbuf));
  }
  *f = 0;
  snprintf(buf, bufsize, "(0x%08x:0x%x<%s>:%s:C:[0x%x,0x%x])",
                      m_locsequencenumber, m_locflags, flagbuf,
                      (m_name? m_name->c_str(): "(null)"),
                      m_cloneplayerid, m_cloneid);
  return buf;
}

bool PlKey::operator==(const PlKey &other) const {
  if (this == &other) {
    return true;
  }
  if (m_contents != other.m_contents
      || m_qualitycapability != other.m_qualitycapability
      || m_locsequencenumber != other.m_locsequencenumber
      || m_locflags != other.m_locflags
      || m_classtype != other.m_classtype
      || m_objectid != other.m_objectid
      || m_cloneid != other.m_cloneid
      || m_cloneplayerid != other.m_cloneplayerid) {
    return false;
  }
  if (m_name) {
    if (other.m_name) {
      return (*m_name == *other.m_name);
    } else {
      return (strlen(m_name->c_str()) == 0);
    }
  } else if (other.m_name) {
    return (strlen(other.m_name->c_str()) == 0);
  } else {
    // both strings are NULL
    return true;
  }
}

void PlKey::make_null() {
  memset(this, 0, sizeof(PlKey));
  m_locsequencenumber = 0xFFFFFFFF;
}

uint32_t PlKey::write_null_key(uint8_t *buf, size_t buflen) {
  if (buflen < 15) {
    // XXX programmer error
    return 0;
  }
  memcpy(buf, "\0\xFF\xFF\xFF\xFF\0\0\0\0\0\0\0\0\0\xF0", 15);
  return 15;
}
