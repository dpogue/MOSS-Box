/* -*- c++ -*- */

/*
 MOSS - A server for the Myst Online: Uru Live client/protocol
 Copyright (C) 2008-2009  a'moaca'

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

/** @defgroup SDL State Description Structures
 *
 * State Description Language (also known as SDL) describes data structures passed
 * between the game server (MOSS) and the game client (CWE).
 *
 * There is a textual schema for each data structure, contained in an SDL file.
 * The SDL file description is stored in SDLDesc objects, while instances of values
 * are contained in SDLState objects.
 * @{
 */

/**
 * SDL-handling classes
 *
 * Classes representing Game State.
 * There are two kinds of SDL objects: the descriptions,
 * from the .sdl text files, and the actual data transmitted in a binary form.
 */

//#include <sys/time.h>
//
//#include <list>
//#include <vector>
//
//#include <iostream>
//#include <fstream>
//
//#include "exceptions.h"
//#include "protocol.h"
//#include "PlKey.h"
//#include <stdio.h>
//
//#include "Logger.h"
class SDLDesc {
public:
  /*
   * Data types.
   */
  typedef enum sdl_varoption_e { // CWE plSDLDescriptor.h
    None =            0x00, ///< Initializer value
    Internal =        0x01, ///< SDL DISPLAYOPTIONS=HIDDEN
                            ///< CWE: Don't allow access to this var in Vault Mgr
    AlwaysNew =       0x02, ///< SDL DEFAULTOPTION=VAULT
                            ///< CWE: Treat this var as always having the latest timestamp when FlaggingNewerState
    VariableLength =  0x04  ///< CWE: Var is defined as int32_t foo[], so it's length is variable, starting at 0
  } sdl_varoption_t ;

  static const char* varoptions_c_str(uint32_t t);

  class DescObj {
  public:
    DescObj() :
        m_name(NULL), m_chars(NULL), m_count(0), m_varoptions(None) {
    }
    virtual ~DescObj() {
      if (m_name)
        free(m_name);
    }
    char *m_name;
    uint32_t m_count;
    uint32_t m_varoptions;

  protected:
    const char *m_chars; /// Cached/allocated C type string representation
    std::string m_string; /// Cached/allocated C++ std::string representation
  };

  class Variable: public DescObj {
  public:
    typedef enum sdl_vartype_e { // CWE plSDLDescriptor.h
      None = -1,        ///< -1,
      Int = 0,          ///< 0,
      Float,            ///< 1
      Bool,             ///< 2
      String32,         ///< 3
      Key,              ///< 4 CWE: plKey - basically a uoid
      StateDescriptor,  ///< 5 CWE: this var refers to another state descriptor
      Creatable,        ///< 6 CWE: plCreatable - basically a classIdx and a read/write buffer
      Double,           ///< 7
      Time,             ///< 8 CWE: double which is automatically converted to server clock and back, use for animation times
      Byte,             ///< 9
      Short,            ///< 10
      AgeTimeOfDay,     ///< 11 CWE: float which is automatically set to the current age time of day (0-1)
      Vector3 = 50,     ///< 50
      Point3,           ///< 51
      RGB,              ///< 52
      RGBA,             ///< 53
      Quaternion,       ///< 54
      RGB8,             ///< 55
      RGBA8             ///< 56
    } sdl_vartype_t;

    typedef union {
      int32_t v_int;
      float v_float;
      bool v_bool;
      char v_string[32];
      PlKey v_plkey;
      uint8_t *v_creatable;     ///< first four bytes is following length
      struct timeval v_time;    ///< CWE stores this as a double (kTime) plSDLDescriptor.h
      int8_t v_byte;
      int16_t v_short;
      struct timeval v_agetime; ///< CWE stores this as a float (kAgeTimeOfDay). plSDLDescriptor.h
      float v_point3[3];
      float v_vector3[3];
      float v_quaternion[4];
      uint8_t v_rgb8[3];
    } data_t;

    Variable(sdl_vartype_t type) :
        DescObj(), m_type(type), m_dispoptions(0) {
      memset(&m_default, 0, sizeof(m_default));
    }
    ~Variable();

    sdl_vartype_t m_type;
    char* m_dispoptions;
    data_t m_default;

    static char* c_str(char *buf, size_t buflen, sdl_vartype_t t, SDLDesc::Variable::data_t &d); // helper function
    static std::string str(sdl_vartype_t t, SDLDesc::Variable::data_t &d); // helper function
    static const char* vartype_c_str(sdl_vartype_t t);
    const char* c_str();
    std::string str();

  private:
    Variable();
    Variable(Variable&);
    Variable& operator=(const Variable&);
  };

  class Struct: public DescObj {
  public:
    Struct(SDLDesc *type) :
        DescObj(), m_data(type) {
    }
    virtual ~Struct() {
    }
    SDLDesc *m_data;

    const char* c_str(const char *sep = ", ");
    std::string str(const char *sep = ", ");

  private:
    Struct();
    Struct(Struct&);
    Struct& operator=(const Struct&);
  };

  /**
   * SDLDesc itself; note objects are constructed with parse_file and
   * parse_directory.
   */

  ~SDLDesc();

  typedef Variable::sdl_vartype_e sdl_vartype_t;

  // accessors
  const char* name() const {
    return m_name;
  }
  uint32_t version() const {
    return m_version;
  }
  static SDLDesc* find_by_name(const char *name, const std::list<SDLDesc*> &l, uint32_t version = 0);
  const std::vector<Variable*>& vars() const {
    return m_vars;
  }
  const std::vector<Struct*>& structs() const {
    return m_structs;
  }

  // throws parse_error
  static void parse_file(std::list<SDLDesc*> &sdls, std::ifstream &file);
  // returns non-zero for an error: < 0 for an error, > 0 for dirname
  // not present (or not a directory)
  static int32_t parse_directory(Logger *log, std::list<SDLDesc*> &sdls, std::string &dirname, bool is_common, bool not_present_is_error);

  const char* c_str(const char *sep = ", ");
  std::string str(const char *sep = ", ");

protected:
  char *m_name;
  uint32_t m_version;

  std::vector<Variable*> m_vars;
  std::vector<Struct*> m_structs;

  static SDLDesc* read_desc(std::ifstream &file, uint32_t &lineno, std::list<SDLDesc*> &descs);
  void set_version(uint32_t v) {
    m_version = v;
  }
  void add_var(Variable *v) {
    m_vars.push_back(v);
  }
  void add_struct(Struct *s) {
    m_structs.push_back(s);
  }

  SDLDesc(const std::string &name);

  const char *m_chars;
  std::string m_string;

private:
  static sdl_vartype_t string_to_type(std::string &s);
  static uint32_t name_and_count(std::string &namestr, uint32_t lineno);
};

// utility functions
uint32_t do_message_compression(uint8_t *buf);

class SDLState {
public:
  typedef enum sdl_contentflags_e { // CWE plSDL.h
    HasUoid =             0x01,
    HasNotificationInfo = 0x02,
    HasTimeStamp =        0x04,
    SameAsDefault =       0x08,
    HasDirtyFlag =        0x10,
    WantTimeStamp =       0x20,
    AddedVarLengthIO =  0x8000,        ///< using to establish a new version in the header (comment plSDL.h: can delete in 8/03)
    HasMaximumValue =   0xffff
  } sdl_flag_t;

  enum sdl_stateflags_e { // CWE plSDL.h used in plStateDataRecord
    Volatile = 0x01
  };

  enum sdl_RWOptions_e { // CWE plSDL.h
    DirtyOnly            = 1<< 0,      ///< 0x0001 CWE: write option
    SkipNotificationInfo = 1<< 1,      ///< 0x0002 CWE: read/write option
    Broadcast            = 1<< 2,      ///< 0x0004 CWE: send option
    WriteTimeStamps      = 1<< 3,      ///< 0x0008 CWE: write out time stamps
    TimeStampOnRead      = 1<< 4,      ///< 0x0010 CWE: read: timestamp each var when it gets read. write: request that the reader timestamp the dirty vars.
    TimeStampOnWrite     = 1<< 5,      ///< 0x0020 CWE: read: n/a. write: timestamp each var when it gets written.
    KeepDirty            = 1<< 6,      ///< 0x0040 CWE: don't clear dirty flag on read
    DontWriteDirtyFlag   = 1<< 7,      ///< 0x0080 CWE: write option. don't write var dirty flag.
    MakeDirty            = 1<< 8,      ///< 0x0100 CWE: read/write: set dirty flag on var read/write.
    DirtyNonDefaults     = 1<< 9,      ///< 0x0200 CWE: dirty the var if non default value.
    ForceConvert         = 1<<10,      ///< 0x0400 CWE: always try to convert rec to latest on read
  };

  enum sdl_stateversion_e {
     IOVersion = 0x06
  };

  class StateObj {
  public:
    StateObj(uint32_t index) :
        m_index(index), m_flags(SameAsDefault), m_count(0), m_chars(NULL) {
    }
    uint32_t m_index;
    uint32_t m_flags;
    struct timeval m_ts;
    uint32_t m_count;
  protected:
    const char *m_chars;
    std::string m_string;
  };

  class Variable: public StateObj {
  public:
    SDLDesc::sdl_vartype_t m_type;
    SDLDesc::Variable::data_t *m_value;

    Variable(uint32_t index, const SDLDesc::sdl_vartype_t type) :
        StateObj(index), m_type(type), m_value(NULL) {
    }
    ~Variable();
    // == excludes the timestamp! (both the flag and the actual ts value)
    bool operator==(const Variable &other);
    Variable& operator=(const Variable &other);

    const char* c_str();
    std::string str();
  private:
    Variable();
    Variable(Variable&);
  };

  class Struct: public StateObj {
  public:
    const SDLDesc *m_desc;
    SDLState *m_child;

    Struct(uint32_t index, const SDLDesc *parent) :
        StateObj(index), m_desc(parent), m_child(NULL) {
    }
    ~Struct() {
      if (m_child)
        delete[] m_child;
    }
    Struct& operator=(const Struct &other);

    const char* c_str();
    std::string str();
  private:
    Struct();
    Struct(Struct&);
  };

  /// the constructor is used with no arguments when reading state from network
  // messages (set_desc() and read_in() fill in the data); when creating new,
  // default state at startup, the SDLDesc is passed in
  SDLState(const SDLDesc *desc = NULL);
  ~SDLState();

  /// Read in a full SDL message. Returns the number of bytes used, or
  // -(bytes used) if the SDL is not recognized.
  // throws parse_error, truncated_message
  int32_t read_msg(const uint8_t *buf, size_t bufsize, const std::list<SDLDesc*> &descs);
  /// Returns how many (uncompressed) bytes the entire message requires.
  // When written the message may be smaller due to compression.
  uint32_t send_len() const;
  /// Write out a full SDL message. Returns -1 if the buffer is too small.
  int32_t write_msg(uint8_t *buf, size_t bufsize, bool no_compress = false);
  /// set the SDLDesc that goes with the SDLState being created
  void set_desc(const SDLDesc *desc);
  /// Returns < 0 if the SDL is not recognized. Otherwise returns how many
  // bytes were read.
  // throws truncated_message
  int32_t read_in(const uint8_t *buf, size_t bufsize, const std::list<SDLDesc*> &descs);
  /// Returns how many (uncompressed) bytes the body of the message requires
  // (not including the plKey or the lengths/compression flag).
  uint32_t body_len() const;
  /// Returns < 0 if the buffer is not big enough, otherwise how many bytes
  // were written.
  int32_t write_out(uint8_t *buf, size_t bufsize) const;

  /**
   * These methods manage the persistent SDL for everything in an age.
   */
  /// cons up a plKey for an AgeSDLHook
  void invent_age_key(uint32_t pageid);
  /// we aren't tracking one of these yet, this will become the master copy
  void expand();
  /// update this, the master copy
  void update_from(SDLState *newer, bool vault = false, bool global = true, bool age_load = false);

  /// write encoded form to a file
  static bool save_file(std::ofstream &file, std::list<SDLState*> &save);
  /// read encoded form from a file
  static bool load_file(std::ifstream &file, std::list<SDLState*> &load, std::list<SDLDesc*> &descs, Logger *log);

  static char* sdl_flag_c_str_alloc(uint32_t t);

  /**
   * Utility functions
   */
  /// check name
  bool name_equals(const char *name);
  /// use to determine whether an SDL is avatar-specific
  bool is_avatar_sdl() const;

  /// provide a formatted representation of the SDL object
  const char* c_str(const char *sep = ", ");
  std::string str(const char *sep = ", ");

  /**
   * Accessors
   */

  const SDLDesc* get_desc() const {
    return m_desc;
  }
  PlKey& key() {
    return m_key;
  }
  const std::vector<Variable*>& vars() const {
    return m_vars;
  }
  const std::vector<Struct*>& structs() const {
    return m_structs;
  }

protected:
  PlKey m_key;
  uint16_t m_flag;
  const SDLDesc *m_desc;
  bool m_saving_to_file;

  std::vector<Variable*> m_vars;
  std::vector<Struct*> m_structs;

  const char *m_chars;
  std::string m_string;

private:
  int32_t recursive_parse(const uint8_t *buf, size_t bufsize);
  int32_t recursive_write(uint8_t *buf, size_t bufsize) const;
  uint32_t recursive_len() const;
};

/**
 * @}
 */

/*
 * I am sticking the .age file parser here with the SDL parser because the
 * files are parsed at the same time and so on.
 */
class AgeDesc {
public:
  class Page {
  public:
    Page(const char *name);
    ~Page() {
      if (m_name)
        free(m_name);
    }
    char *m_name;
    bool m_owned; // XXX ownership is not per-page in MOUL
    kinum_t m_owner; // so these two are probably unneeded
    uint32_t m_pagenum;
    uint32_t m_conditional_load;
  };

  ~AgeDesc();

  // throws parse_error
  static AgeDesc* parse_file(std::ifstream &file);

  // accessors
  uint32_t linger_time() const {
    return m_linger;
  }
  int32_t seq_prefix() const {
    return m_seq_prefix;
  }

  const char* c_str();
  std::string str();

protected:
  std::vector<Page*> m_pages;
  uint32_t m_start_date_time;
  float m_daylen;
  uint32_t m_capacity; // used for what??
  uint32_t m_linger;
  int32_t m_seq_prefix;
  int32_t m_release;

  AgeDesc() :
      m_linger(180/*default*/), m_chars(NULL), m_start_date_time(0), m_daylen(0.0),
      m_capacity(0), m_seq_prefix(0), m_release(0) {
  };

  const char *m_chars;
  std::string m_string;
};
