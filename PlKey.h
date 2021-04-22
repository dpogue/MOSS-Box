/* -*- c++ -*- */

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
 * It should not be difficult to know what a plKey is. :-)
 * (AKA "uru object descriptor")
 */

#ifndef _PLKEY_H_
#define _PLKEY_H_

//#include "machine_arch.h"
//
//#include "UruString.h"

class PlKey {
public:

  // Enums come from CWE pnKeyedObject/pnUoid.h

  enum Contents_e {
    HasCloneIDs = 0x1,
    HasLoadMask = 0x2
  };
  enum locFlags_e {
    LocalOnly  = 0x1,  ///< Set if nothing in the room saves state.
    Volatile   = 0x2,  ///< Set is nothing in the room persists when the server exits.
    Reserved   = 0x4,
    BuiltIn    = 0x8,
    Itinerant  = 0x10
  };
  enum SequenceNum_e {
    kGlobalFixedLocIdx =           0,                       ///< Fixed keys go here, think of as "global,fixed,keys"
    kSceneViewerLocIdx =           1,

    kLocalLocStartIdx =            3,                       ///< These are a range of #s that go to local, testing-only pages.
    kLocalLocEndIdx =              32,                      ///< You can't go over the network with any keys with these locs.

    kNormalLocStartIdx =           kLocalLocEndIdx + 1,

    kReservedLocStart =            0xff000000,              ///< Reserved locations are ones that aren't real game locations,
    kGlobalServerLocIdx =          kReservedLocStart,       ///< Global pool room for the server. Only the server gets this one

    kReservedLocAvailableStart =   kGlobalServerLocIdx + 1, ///< This is the start of the *really* available ones
    kReservedLocEnd =              0xfffffffe,              ///< But instead act as a holding place for data

    kInvalidLocIdx =               0xffffffff
};
  // read a plKey into the object from the contents of the buffer,
  // returning how many bytes were read
  // throws truncated_message
  uint32_t read_in(const uint8_t *buf, size_t buflen);

  uint32_t send_len() const;
  uint32_t write_out(uint8_t *buf, size_t buflen, bool bitflip = true) const;

  // since I wanted to put this class in a union, I can't have a
  // destructor so I must depend on calling code to clean up
  void delete_name() {
    if (m_name)
      delete m_name;
  }

  // format plKey for logging
  // Adjusted to match CWE plUoid::Stringize format "(<seq#>:<flags>:<ObjName>:C:[<cloneplayerid>,<cloneid>])"
  char* c_str(char *buf, size_t bufsize);

  // operators
  bool operator==(const PlKey &other) const;
  bool operator!=(const PlKey &other) {
    return !(*this == other);
  }

  // "null" keys show up in a few places
  void make_null();
  static uint32_t null_send_len() {
    return 15;
  }
  static uint32_t write_null_key(uint8_t *buf, size_t buflen);

  uint8_t m_contents;
  uint8_t m_qualitycapability;
  uint16_t m_locflags;
  uint16_t m_classtype;
  uint32_t m_locsequencenumber;
  uint32_t m_objectid;
  uint32_t m_cloneid;
  uint32_t m_cloneplayerid;
  UruString *m_name;
};

#endif /* _PLKEY_H_ */
