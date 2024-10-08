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
#include <sys/uio.h> /* for struct iovec */

#include <stdexcept>

#include "machine_arch.h"
#include "protocol.h"
#include "util.h"

#include "VaultNode.h"

//    bits            datatype          col_name        fetch_name    fetch_required

static uint32_t CommonBits = NodeID | NodeType | CreateTime | ModifyTime | CreatorAcctID | CreatorID;
static VaultNode::ColumnSpec CommonSpec[] = {
    { NodeID,         VaultNode::UInt,    "nodeid",       "",           false }, // special case...
    { NodeType,       VaultNode::Int,     "nodetype",     "v_nodetype", true },
    { CreateTime,     VaultNode::UInt,    "createtime",   "v_createtime", true },
    { ModifyTime,     VaultNode::UInt,    "modifytime",   "v_modifytime", true },
    { CreatorAcctID,  VaultNode::UUID,    "creatoracctid","v_creatoracctid", true },
    { CreatorID,      VaultNode::UInt,    "creatorid",    "v_creatorid", true }
};
static const size_t CommonSize = sizeof(CommonSpec) / sizeof(VaultNode::ColumnSpec);

static uint32_t CreateAgeBits = CreateAgeName | CreateAgeUUID;
static VaultNode::ColumnSpec CreateAgeSpec[] = {
    { CreateAgeName,  VaultNode::String,  "createagename","v_createagename", false },
    { CreateAgeUUID,  VaultNode::UUID,    "createageuuid","v_createageuuid", false }
};
static const size_t CreateAgeSize = sizeof(CreateAgeSpec) / sizeof(VaultNode::ColumnSpec);

static uint32_t AgeNodeBits = UUID_1 | UUID_2 | String64_1;
static VaultNode::ColumnSpec AgeNodeSpec[] = {
    { UUID_1,         VaultNode::UUID,    "uuid_1",       "v_uuid_1",   true },
    { UUID_2,         VaultNode::UUID,    "uuid_2",       "v_uuid_2",   false }, // parent age
    { String64_1,     VaultNode::String,  "filename",     "v_filename", true }
};
static const size_t AgeNodeSize = sizeof(AgeNodeSpec) / sizeof(VaultNode::ColumnSpec);

static uint32_t AgeInfoNodeBits =
    Int32_1 | Int32_2 | Int32_3 | UInt32_1 | UInt32_2 | UInt32_3 | UUID_1 | UUID_2 | String64_2 | String64_3 | String64_4 | Text_1;
static VaultNode::ColumnSpec AgeInfoNodeSpec[] = {
    { Int32_1,        VaultNode::Int,     "int32_1",      "v_int32_1",  true },
    { Int32_2,        VaultNode::Int,     "int32_2",      "v_int32_2",  false },
    { Int32_3,        VaultNode::Int,     "int32_3",      "v_int32_3",  true },
    { UInt32_1,       VaultNode::UInt,    "uint32_1",     "v_uint32_1", true },
    { UInt32_2,       VaultNode::UInt,    "uint32_2",     "v_uint32_2", true },
    { UInt32_3,       VaultNode::UInt,    "uint32_3",     "v_uint32_3", true },
    { UUID_1,         VaultNode::UUID,    "uuid_1",       "v_uuid_1",   true },
    { UUID_2,         VaultNode::UUID,    "uuid_2",       "v_uuid_2",   false },
    { String64_2,     VaultNode::String,  "string64_2",   "v_string64_2", true },
    { String64_3,     VaultNode::String,  "string64_3",   "v_string64_3", true },
    { String64_4,     VaultNode::String,  "string64_4",   "v_string64_4", false },
    { Text_1,         VaultNode::String,  "text_1",       "v_text_1",   false }
};
static const size_t AgeInfoNodeSize = sizeof(AgeInfoNodeSpec) / sizeof(VaultNode::ColumnSpec);

static uint32_t AgeInfoListNodeBits = Int32_1;
static VaultNode::ColumnSpec AgeInfoListNodeSpec[] = {
    { Int32_1,        VaultNode::Int,     "type",         "v_type",     true }
};
static const size_t AgeInfoListNodeSize = sizeof(AgeInfoListNodeSpec) / sizeof(VaultNode::ColumnSpec);

static uint32_t AgeLinkNodeBits = Int32_1 | Int32_2 | Blob_1;
static VaultNode::ColumnSpec AgeLinkNodeSpec[] = {
    { Int32_1,        VaultNode::Int,     "shared",       "v_int32_1",  false },
    { Int32_2,        VaultNode::Int,     "volatile",     "v_int32_2",  false },
    { Blob_1,         VaultNode::Blob,    "linkpoints",   "v_linkpoints", false }
};
static const size_t AgeLinkNodeSize = sizeof(AgeLinkNodeSpec) / sizeof(VaultNode::ColumnSpec);

static uint32_t ChronicleNodeBits = Int32_1 | String64_1 | Text_1;
static VaultNode::ColumnSpec ChronicleNodeSpec[] = {
    { Int32_1,        VaultNode::Int,     "type",         "v_type",     false },
    { String64_1,     VaultNode::String,  "name",         "v_name",     true },
    { Text_1,         VaultNode::String,  "value",        "v_value",    true }
};
static const size_t ChronicleNodeSize = sizeof(ChronicleNodeSpec) / sizeof(VaultNode::ColumnSpec);

static uint32_t FolderNodeBits = Int32_1 | String64_1;
static VaultNode::ColumnSpec FolderNodeSpec[] = {
    { Int32_1,        VaultNode::Int,     "type",         "v_type",     false },
    { String64_1,     VaultNode::String,  "name",         "v_name",     false }
};
static const size_t FolderNodeSize = sizeof(FolderNodeSpec) / sizeof(VaultNode::ColumnSpec);

static uint32_t ImageNodeBits = Int32_1 | String64_1 | Blob_1;
static VaultNode::ColumnSpec ImageNodeSpec[] = {
    { Int32_1,        VaultNode::Int,     "exists",       "v_exists",   true },
    { String64_1,     VaultNode::String,  "name",         "v_name",     true },
    { Blob_1,         VaultNode::Blob,    "image",        "v_image",    false }
};
static const size_t ImageNodeSize = sizeof(ImageNodeSpec) / sizeof(VaultNode::ColumnSpec);

static uint32_t MarkergameNodeBits = Text_1 | UUID_1;
static VaultNode::ColumnSpec MarkergameNodeSpec[] = {
    { Text_1,         VaultNode::String,    "name",       "v_name",     true },
    { UUID_1,         VaultNode::UUID,      "uuid_1",     "v_uuid_1",   true }
};
static const size_t MarkergameNodeSize = sizeof(MarkergameNodeSpec) / sizeof(VaultNode::ColumnSpec);

static uint32_t PlayerNodeBits = Int32_1 | Int32_2 | UInt32_1 | UUID_1 | UUID_2 | String64_1 | IString64_1;
static VaultNode::ColumnSpec PlayerNodeSpec[] = {
    { Int32_1,        VaultNode::Int,       "int32_1",    "v_int32_1",  true },
    { Int32_2,        VaultNode::Int,       "int32_2",    "v_int32_2",  true },
    { UInt32_1,       VaultNode::UInt,      "uint32_1",   "v_uint32_1", true },
    { UUID_1,         VaultNode::UUID,      "uuid_1",     "v_uuid_1",   true },
    { UUID_2,         VaultNode::UUID,      "uuid_2",     "v_uuid_2",   false },
    { String64_1,     VaultNode::String,    "gender",     "v_gender",   true },
    { IString64_1,    VaultNode::String,    "name",       "v_name",     true }
};
static const size_t PlayerNodeSize = sizeof(PlayerNodeSpec) / sizeof(VaultNode::ColumnSpec);

static uint32_t PlayerInfoNodeBits = Int32_1 | UInt32_1 | UUID_1 | String64_1 | IString64_1;
static VaultNode::ColumnSpec PlayerInfoNodeSpec[] = {
    { Int32_1,        VaultNode::Int,       "online",     "v_online",   false },
    { UInt32_1,       VaultNode::UInt,      "ki",         "v_ki",       true },
    { UUID_1,         VaultNode::UUID,      "uuid_1",     "v_uuid_1",   false },
    { String64_1,     VaultNode::String,    "string64_1", "v_string64_1", false },
    { IString64_1,    VaultNode::String,    "name",       "v_name",     true }
};
static const size_t PlayerInfoNodeSize = sizeof(PlayerInfoNodeSpec) / sizeof(VaultNode::ColumnSpec);

static uint32_t PlayerInfoListNodeBits = Int32_1;
static VaultNode::ColumnSpec PlayerInfoListNodeSpec[] = {
    { Int32_1,        VaultNode::Int,       "type",        "v_type",    true }
};
static const size_t PlayerInfoListNodeSize = sizeof(PlayerInfoListNodeSpec) / sizeof(VaultNode::ColumnSpec);

static uint32_t SDLNodeBits = Int32_1 | String64_1 | Blob_1;
static VaultNode::ColumnSpec SDLNodeSpec[] = {
    { Int32_1,        VaultNode::Int,       "int32_1",      "v_int32_1", false },
    { String64_1,     VaultNode::String,    "name",         "v_name",   false },
    { Blob_1,         VaultNode::Blob,      "blob",         "v_blob",   false }
};
static const size_t SDLNodeSize = sizeof(SDLNodeSpec) / sizeof(VaultNode::ColumnSpec);

static uint32_t SystemNodeBits = 0;
static VaultNode::ColumnSpec SystemNodeSpec[] = { };
static const size_t SystemNodeSize = sizeof(SystemNodeSpec) / sizeof(VaultNode::ColumnSpec);

static uint32_t TextNoteNodeBits = Int32_1 | Int32_2 | String64_1 | Text_1;
static VaultNode::ColumnSpec TextNoteNodeSpec[] = {
    { Int32_1,        VaultNode::Int,       "int32_1",      "v_int32_1", false },
    { Int32_2,        VaultNode::Int,       "int32_2",      "v_int32_2", false },
    { String64_1,     VaultNode::String,    "title",        "v_title",  false },
    { Text_1,         VaultNode::String,    "value",        "v_value",  false }
};
static const size_t TextNoteNodeSize = sizeof(TextNoteNodeSpec) / sizeof(VaultNode::ColumnSpec);

// this data is internal to this file and is used in for loops to avoid
// loads of code repetition for each bit
typedef struct {
  uint32_t bit;
  VaultNode::datatype_t type;
} int_colspec_t;
//    bit             type
static const int_colspec_t colspecs[] = {
    { NodeID,         VaultNode::UInt },
    { CreateTime,     VaultNode::UInt },
    { ModifyTime,     VaultNode::UInt },
    { CreateAgeName,  VaultNode::String },
    { CreateAgeUUID,  VaultNode::UUID },
    { CreatorAcctID,  VaultNode::UUID },
    { CreatorID,      VaultNode::UInt },
    { NodeType,       VaultNode::Int },
    { Int32_1,        VaultNode::Int },
    { Int32_2,        VaultNode::Int },
    { Int32_3,        VaultNode::Int },
    { Int32_4,        VaultNode::Int },
    { UInt32_1,       VaultNode::UInt },
    { UInt32_2,       VaultNode::UInt },
    { UInt32_3,       VaultNode::UInt },
    { UInt32_4,       VaultNode::UInt },
    { UUID_1,         VaultNode::UUID },
    { UUID_2,         VaultNode::UUID },
    { UUID_3,         VaultNode::UUID },
    { UUID_4,         VaultNode::UUID },
    { String64_1,     VaultNode::String },
    { String64_2,     VaultNode::String },
    { String64_3,     VaultNode::String },
    { String64_4,     VaultNode::String },
    { String64_5,     VaultNode::String },
    { String64_6,     VaultNode::String },
    { IString64_1,    VaultNode::String },
    { IString64_2,    VaultNode::String },
    { Text_1,         VaultNode::String },
    { Text_2,         VaultNode::String },
    { Blob_1,         VaultNode::Blob },
    { Blob_2,         VaultNode::Blob }
};

bool VaultNode::check_len_by_bitfields(const uint8_t *buf, size_t len) {
  uint32_t bits1, bits2;
  bits1 = read32(buf, 4);
  bits2 = read32(buf, 8);

  uint32_t offset = 12;
  for (uint32_t i = 0; i < 32; i++) {
    if (bits1 & colspecs[i].bit) {
      switch (colspecs[i].type) {
      case Int:
      case UInt:
        offset += 4;
        break;
      case UUID:
        offset += 16;
        break;
      case String:
      case Blob:
        if (offset + 4 > len) {
          return false;
        }
        offset += 4 + read32(buf, offset);
        break;
      default:
        // can't happen
        break;
      }
    }
  }
  // second bitfield unused??
  if (offset > len) {
    return false;
  }
  return true;
}

VaultNode::VaultNode() :
    m_length(8), m_bits1(0), m_bits2(0), m_owns_bufs(true) {
  memset((char*) m_fields1, 0, sizeof(m_fields1));
}

VaultNode::VaultNode(const uint8_t *inbuf, bool copy_data) :
    m_length(8), m_bits1(read32(inbuf, 4)), m_bits2(read32(inbuf, 8)), m_owns_bufs(copy_data) {
  memset((char*) m_fields1, 0, sizeof(m_fields1));

  uint32_t buflen;
  uint32_t offset = 12;

  for (uint32_t i = 0; i < 32; i++) {
    if (m_bits1 & colspecs[i].bit) {
      switch (colspecs[i].type) {
      case Int:
      case UInt:
        m_fields1[i].intval = read32le(inbuf, offset);
        offset += 4;
        break;
      case UUID:
        if (copy_data) {
          m_fields1[i].bufval = new uint8_t[UUID_RAW_LEN];
          memcpy(m_fields1[i].bufval, inbuf + offset, UUID_RAW_LEN);
        } else {
          m_fields1[i].bufval = const_cast<uint8_t*>(inbuf + offset);
        }
        offset += 16;
        break;
      case String:
      case Blob:
        buflen = read32(inbuf, offset);
        if (copy_data) {
          m_fields1[i].bufval = new uint8_t[buflen + 4];
          memcpy(m_fields1[i].bufval, inbuf + offset, buflen + 4);
        } else {
          m_fields1[i].bufval = const_cast<uint8_t*>(inbuf + offset);
        }
        offset += buflen + 4;
        break;
      default:
        // can't happen
        break;
      }
    }
  }

  m_length = offset - 4;
}

VaultNode::~VaultNode() {
  if (m_owns_bufs) {
    for (uint32_t i = 0; i < 32; i++) {
      if ((colspecs[i].type != Int) && (colspecs[i].type != UInt) && (m_bits1 & colspecs[i].bit) && m_fields1[i].bufval) {
        delete[] m_fields1[i].bufval;
      }
    }
  }
}

uint32_t VaultNode::all_bits_for_type(vault_nodetype_t type) {
  switch (type) {
  case PlayerNode:
    return CommonBits | PlayerNodeBits;
  case AgeNode:
    return CommonBits | AgeNodeBits;
  case FolderNode:
    return CommonBits | FolderNodeBits;
  case PlayerInfoNode:
    return CommonBits | PlayerInfoNodeBits;
  case SystemNode:
    return CommonBits | SystemNodeBits;
  case ImageNode:
    return CommonBits | ImageNodeBits;
  case TextNoteNode:
    return CommonBits | TextNoteNodeBits;
  case SDLNode:
    return CommonBits | SDLNodeBits;
  case AgeLinkNode:
    return CommonBits | AgeLinkNodeBits;
  case ChronicleNode:
    return CommonBits | ChronicleNodeBits;
  case PlayerInfoListNode:
    return CommonBits | PlayerInfoListNodeBits;
  case AgeInfoNode:
    return CommonBits | AgeInfoNodeBits;
  case AgeInfoListNode:
    return CommonBits | AgeInfoListNodeBits;
  case MarkergameNode:
    return CommonBits | MarkergameNodeBits;
  default:
    // unknown type!
    return 0;
  }
}

const VaultNode::ColumnSpec* VaultNode::get_spec(vault_nodetype_t type, vault_bitfield_t bit) {
  ColumnSpec *spec = NULL;
  uint32_t count = 0;

  switch (type) {
  case PlayerNode:
    if (PlayerNodeBits & bit) {
      spec = PlayerNodeSpec;
      count = PlayerNodeSize;
    } else if (CommonBits & bit) {
      spec = CommonSpec;
      count = CommonSize;
    }
    break;
  case AgeNode:
    if (AgeNodeBits & bit) {
      spec = AgeNodeSpec;
      count = AgeNodeSize;
    } else if (CommonBits & bit) {
      spec = CommonSpec;
      count = CommonSize;
    }
    break;
  case FolderNode:
    if (FolderNodeBits & bit) {
      spec = FolderNodeSpec;
      count = FolderNodeSize;
    } else if (CommonBits & bit) {
      spec = CommonSpec;
      count = CommonSize;
    } else if (CreateAgeBits & bit) {
      spec = CreateAgeSpec;
      count = CreateAgeSize;
    }
    break;
  case PlayerInfoNode:
    if (PlayerInfoNodeBits & bit) {
      spec = PlayerInfoNodeSpec;
      count = PlayerInfoNodeSize;
    } else if (CommonBits & bit) {
      spec = CommonSpec;
      count = CommonSize;
    }
    break;
  case SystemNode:
    if (SystemNodeBits & bit) {
      spec = SystemNodeSpec;
      count = SystemNodeSize;
    } else if (CommonBits & bit) {
      spec = CommonSpec;
      count = CommonSize;
    }
    break;
  case ImageNode:
    if (ImageNodeBits & bit) {
      spec = ImageNodeSpec;
      count = ImageNodeSize;
    } else if (CommonBits & bit) {
      spec = CommonSpec;
      count = CommonSize;
    } else if (CreateAgeBits & bit) {
      spec = CreateAgeSpec;
      count = CreateAgeSize;
    }
    break;
  case TextNoteNode:
    if (TextNoteNodeBits & bit) {
      spec = TextNoteNodeSpec;
      count = TextNoteNodeSize;
    } else if (CommonBits & bit) {
      spec = CommonSpec;
      count = CommonSize;
    } else if (CreateAgeBits & bit) {
      spec = CreateAgeSpec;
      count = CreateAgeSize;
    }
    break;
  case SDLNode:
    if (SDLNodeBits & bit) {
      spec = SDLNodeSpec;
      count = SDLNodeSize;
    } else if (CommonBits & bit) {
      spec = CommonSpec;
      count = CommonSize;
    } else if (CreateAgeBits & bit) {
      spec = CreateAgeSpec;
      count = CreateAgeSize;
    }
    break;
  case AgeLinkNode:
    if (AgeLinkNodeBits & bit) {
      spec = AgeLinkNodeSpec;
      count = AgeLinkNodeSize;
    } else if (CommonBits & bit) {
      spec = CommonSpec;
      count = CommonSize;
    } else if (CreateAgeBits & bit) {
      spec = CreateAgeSpec;
      count = CreateAgeSize;
    }
    break;
  case ChronicleNode:
    if (ChronicleNodeBits & bit) {
      spec = ChronicleNodeSpec;
      count = ChronicleNodeSize;
    } else if (CommonBits & bit) {
      spec = CommonSpec;
      count = CommonSize;
    } else if (CreateAgeBits & bit) {
      spec = CreateAgeSpec;
      count = CreateAgeSize;
    }
    break;
  case PlayerInfoListNode:
    if (PlayerInfoListNodeBits & bit) {
      spec = PlayerInfoListNodeSpec;
      count = PlayerInfoListNodeSize;
    } else if (CommonBits & bit) {
      spec = CommonSpec;
      count = CommonSize;
    }
    break;
  case AgeInfoNode:
    if (AgeInfoNodeBits & bit) {
      spec = AgeInfoNodeSpec;
      count = AgeInfoNodeSize;
    } else if (CommonBits & bit) {
      spec = CommonSpec;
      count = CommonSize;
    }
    break;
  case AgeInfoListNode:
    if (AgeInfoListNodeBits & bit) {
      spec = AgeInfoListNodeSpec;
      count = AgeInfoListNodeSize;
    } else if (CommonBits & bit) {
      spec = CommonSpec;
      count = CommonSize;
    }
    break;
  case MarkergameNode:
    if (MarkergameNodeBits & bit) {
      spec = MarkergameNodeSpec;
      count = MarkergameNodeSize;
    } else if (CommonBits & bit) {
      spec = CommonSpec;
      count = CommonSize;
    } else if (CreateAgeBits & bit) {
      spec = CreateAgeSpec;
      count = CreateAgeSize;
    }
    break;
  default:
    break;
  }

  if (!spec) {
    return NULL;
  }
  for (uint32_t i = 0; i < count; i++) {
    if (spec[i].bit == bit) {
      return spec + i;
    }
  }
  return NULL;
}

VaultNode::vault_nodetype_t VaultNode::type() const {
  return (vault_nodetype_t) le32toh(m_fields1[7].intval);
}

uint32_t& VaultNode::num_ref(vault_bitfield_t bit) {
  for (uint32_t i = 0; i < 32; i++) {
    if (colspecs[i].bit == bit) {
      if (colspecs[i].type != Int && colspecs[i].type != UInt) {
        // non-integer field!
        break;
      }
      if (!(m_bits1 & colspecs[i].bit)) {
        m_bits1 |= colspecs[i].bit;
        m_length += 4;
      }
      return m_fields1[i].intval;
    }
  }
  // unknown or non-integer field! throw an exception, because there is no
  // safe value to return
  throw std::logic_error("Attempt to get a non-integer vault node field "
      "as an integer");
}

uint8_t* VaultNode::uuid_ptr(vault_bitfield_t bit) {
  for (uint32_t i = 0; i < 32; i++) {
    if (colspecs[i].bit == bit) {
      if (colspecs[i].type != UUID) {
        // non-UUID field!
        break;
      }
      if (!m_fields1[i].bufval) {
        m_fields1[i].bufval = new uint8_t[UUID_RAW_LEN];
      }
      if (!(m_bits1 & colspecs[i].bit)) {
        m_bits1 |= colspecs[i].bit;
        m_length += 16;
      }
      return m_fields1[i].bufval;
    }
  }
  // unknown or non-UUID field! XXX throw an exception?
  return NULL;
}

uint8_t* VaultNode::data_ptr(vault_bitfield_t bit, uint32_t len) {
  for (uint32_t i = 0; i < 32; i++) {
    if (colspecs[i].bit == bit) {
      if (colspecs[i].type != String && colspecs[i].type != Blob) {
        // non-blob/string field!
        break;
      }
      if (!m_fields1[i].bufval) {
        m_fields1[i].bufval = new uint8_t[len + 4];
        if (!(m_bits1 & colspecs[i].bit)) {
          // should always be true
          m_bits1 |= colspecs[i].bit;
          m_length += len + 4;
        }
      } else {
        // not a normal code path
        uint32_t old_len = read32(m_fields1[i].bufval, 0);
        if (m_bits1 & colspecs[i].bit) {
          // should always be true
          m_length -= old_len + 4;
        } else {
          m_bits1 |= colspecs[i].bit;
        }
        m_length += len + 4;
        if (old_len < len) {
          delete[] m_fields1[i].bufval;
          m_fields1[i].bufval = NULL;
          m_fields1[i].bufval = new uint8_t[len + 4];
        }
      }
      write32(m_fields1[i].bufval, 0, len);
      return (m_fields1[i].bufval) + 4;
    }
  }
  // unknown or non-blob/string field! XXX throw an exception?
  return NULL;
}

const uint32_t VaultNode::num_val(vault_bitfield_t bit) const {
  for (uint32_t i = 0; i < 32; i++) {
    if (colspecs[i].bit == bit) {
      if (colspecs[i].type != Int && colspecs[i].type != UInt) {
        // non-integer field!
        break;
      }
      return le32toh(m_fields1[i].intval);
    }
  }
  // unknown or non-integer field! throw an exception, because there is no
  // safe value to return
  throw std::logic_error("Attempt to get a non-integer vault node field "
      "as an integer");
}

const uint8_t* VaultNode::const_uuid_ptr(vault_bitfield_t bit) const {
  for (uint32_t i = 0; i < 32; i++) {
    if (colspecs[i].bit == bit) {
      if (colspecs[i].type != UUID) {
        // non-UUID field!
        break;
      }
      return m_fields1[i].bufval;
    }
  }
  // unknown or non-UUID field! XXX throw an exception?
  return NULL;
}

const uint8_t* VaultNode::const_data_ptr(vault_bitfield_t bit) const {
  for (uint32_t i = 0; i < 32; i++) {
    if (colspecs[i].bit == bit) {
      if (colspecs[i].type != String && colspecs[i].type != Blob) {
        // non-blob/string field!
        break;
      }
      return m_fields1[i].bufval;
    }
  }
  // unknown or non-blob/string field! XXX throw an exception?
  return NULL;
}

const char* VaultNode::tablename_for_type(vault_nodetype_t type) {
  switch (type) {
  case CCRNode:            return "ccr";
  case PlayerNode:         return "player";
  case AgeNode:            return "age";
  case FolderNode:         return "folder";
  case PlayerInfoNode:     return "playerinfo";
  case SystemNode:         return "system";
  case ImageNode:          return "image";
  case TextNoteNode:       return "textnote";
  case SDLNode:            return "sdl";
  case AgeLinkNode:        return "agelink";
  case ChronicleNode:      return "chronicle";
  case PlayerInfoListNode: return "playerinfolist";
  case AgeInfoNode:        return "ageinfo";
  case AgeInfoListNode:    return "ageinfolist";
  case MarkergameNode:     return "markergame";
  default:
    return "unknown_table_name";
  }
}

uint32_t VaultNode::fill_iovecs(struct iovec *iov, uint32_t iov_ct, uint32_t start_at) {
  uint32_t to_include = all_bits_for_type(type()) & m_bits1;
  uint32_t wrlen, done = 0;

  if (start_at < 12) {
    write32(m_header, 0, m_length);
    write32(m_header, 4, to_include);
    write32(m_header, 8, 0);
    iov[done].iov_base = m_header + start_at;
    iov[done].iov_len = 12 - start_at;
    start_at = 0;
    done++;
  } else {
    start_at -= 12;
  }
  for (uint32_t i = 0; i < 32; i++) {
    if (done >= iov_ct) {
      break;
    }
    if (to_include & colspecs[i].bit) {
      switch (colspecs[i].type) {
      case Int:
      case UInt:
        if (start_at < 4) {
          iov[done].iov_base = (&m_fields1[i].intval) + start_at;
          iov[done].iov_len = 4 - start_at;
          start_at = 0;
          done++;
        } else {
          start_at -= 4;
        }
        break;
      case UUID:
        if (start_at < 16) {
          iov[done].iov_base = (m_fields1[i].bufval) + start_at;
          iov[done].iov_len = 16 - start_at;
          start_at = 0;
          done++;
        } else {
          start_at -= 16;
        }
        break;
      case String:
      case Blob:
        wrlen = read32(m_fields1[i].bufval, 0) + 4;
        if (start_at < wrlen) {
          iov[done].iov_base = (m_fields1[i].bufval) + start_at;
          iov[done].iov_len = wrlen - start_at;
          start_at = 0;
          done++;
        } else {
          start_at -= wrlen;
        }
        break;
      default:
        // can't happen
        break;
      }
    }
  }
  return done;
}

uint32_t VaultNode::iovecs_written_bytes(uint32_t byte_ct, uint32_t start_at, bool *msg_done) {
  if (byte_ct + start_at < m_length + 4) {
    *msg_done = false;
    return 0;
  } else {
    *msg_done = true;
    return (byte_ct + start_at) - (m_length + 4);
  }
}

uint32_t VaultNode::fill_buffer(uint8_t *buffer, size_t len, uint32_t start_at, bool *msg_done) const {
  uint32_t to_include = all_bits_for_type(type()) & m_bits1;
  uint32_t wrlen, done = 0;
  *msg_done = true;

  if (start_at < 4) {
    wrlen = 4 - start_at;
    if (wrlen > len) {
      *msg_done = false;
      wrlen = len;
    }
    if (wrlen == 4) {
      write32(buffer, 0, m_length);
    } else {
      uint8_t temp[4];
      write32(temp, 0, m_length);
      memcpy(buffer, temp + start_at, wrlen);
    }
    done += wrlen;
    start_at = 0;
  } else {
    start_at -= 4;
  }
  if (start_at < 4) {
    wrlen = 4 - start_at;
    if (wrlen > len - done) {
      *msg_done = false;
      wrlen = len - done;
    }
    if (wrlen == 4) {
      write32(buffer, done, to_include);
    } else {
      uint8_t temp[4];
      write32(temp, 0, to_include);
      memcpy(buffer + done, temp + start_at, wrlen);
    }
    done += wrlen;
    start_at = 0;
  } else {
    start_at -= 4;
  }
  if (start_at < 4) {
    wrlen = 4 - start_at;
    if (wrlen > len - done) {
      *msg_done = false;
      wrlen = len - done;
    }
    if (wrlen == 4) {
      write32(buffer, done, 0);
    } else {
      memset(buffer + done, 0, wrlen);
    }
    done += wrlen;
    start_at = 0;
  } else {
    start_at -= 4;
  }
  for (uint32_t i = 0; i < 32; i++) {
    if (to_include & colspecs[i].bit) {
      switch (colspecs[i].type) {
      case Int:
      case UInt:
        if (start_at < 4) {
          wrlen = 4 - start_at;
          if (wrlen > len - done) {
            *msg_done = false;
            wrlen = len - done;
          }
          memcpy(buffer + done, (&m_fields1[i].intval) + start_at, wrlen);
          done += wrlen;
          start_at = 0;
        } else {
          start_at -= 4;
        }
        break;
      case UUID:
        if (start_at < 16) {
          wrlen = 16 - start_at;
          if (wrlen > len - done) {
            *msg_done = false;
            wrlen = len - done;
          }
          memcpy(buffer + done, (m_fields1[i].bufval) + start_at, wrlen);
          done += wrlen;
          start_at = 0;
        } else {
          start_at -= 16;
        }
        break;
      case String:
      case Blob:
        wrlen = read32(m_fields1[i].bufval, 0) + 4;
        if (start_at < wrlen) {
          if (wrlen > len - done) {
            *msg_done = false;
            wrlen = len - done;
          }
          memcpy(buffer + done, (m_fields1[i].bufval) + start_at, wrlen);
          done += wrlen;
          start_at = 0;
        } else {
          start_at -= wrlen;
        }
        break;
      default:
        // can't happen
        break;
      }
      if (!(*msg_done)) {
        break;
      }
    }
  }
  return done;
}
