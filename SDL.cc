/*
 *
 *
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
#include <ctype.h> /* for tolower() */

#include <stdarg.h>
#include <iconv.h>

#include <sys/time.h>
#include <dirent.h>

#include <stdexcept>
#include <list>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <iterator>
#include <ctime>

#include <zlib.h>

#include "machine_arch.h"
#include "exceptions.h"
#include "typecodes.h"
#include "constants.h"
#include "protocol.h"
#include "util.h"
#include "UruString.h"
#include "PlKey.h"

#include "Logger.h"
#include "SDL.h"

SDLDesc::SDLDesc(const std::string &name) :
    m_name(NULL), m_version(0), m_chars(NULL), m_string("") {
  m_name = new char[name.length() + 1];
  memcpy(m_name, name.c_str(), name.length() + 1);
}

SDLDesc::~SDLDesc() {
  if (m_name) {
    delete[] m_name;
  }
  std::vector<Variable*>::iterator vi;
  for (vi = m_vars.begin(); vi != m_vars.end(); vi++) {
    delete *vi;
  }
  std::vector<Struct*>::iterator si;
  for (si = m_structs.begin(); si != m_structs.end(); si++) {
    delete *si;
  }
}

SDLDesc* SDLDesc::find_by_name(const char *name, const std::list<SDLDesc*> &l, uint32_t version) {
  SDLDesc *ret = NULL;
  std::list<SDLDesc*>::const_iterator iter;
  uint32_t namelen = strlen(name);
  for (iter = l.begin(); iter != l.end(); iter++) {
    SDLDesc *desc = *iter;
    if (strlen(desc->name()) == namelen && !strcasecmp(desc->name(), name)) {
      if (version) {
        if (desc->version() == version)
          return desc;
      } else {
        if (!ret || (ret->version() < desc->version()))
          ret = desc;
      }
    }
  }
  return ret;
}

void SDLDesc::parse_file(std::list<SDLDesc*> &sdls, std::ifstream &file) {
  std::list<SDLDesc*> these;
  uint32_t lineno = 0;
  SDLDesc *ret = read_desc(file, lineno, these);
  while (ret) {
    these.push_front(ret);
    ret = read_desc(file, lineno, these);
  }
  if (these.size() == 0) {
    // empty file
  } else {
    sdls.splice(sdls.end(), these);
  }
}

// Formatter for dirent struct
char* f_dirent(char *out, size_t outsize, struct dirent *d) {
  snprintf(out, outsize, "{ino=%u name=%s}",
      (uint32_t)(d->d_ino), (char *)(d->d_name));
  return out;
}

int32_t SDLDesc::parse_directory(Logger *log, std::list<SDLDesc*> &sdls, std::string &dirname_s,
                                    bool is_common, bool not_present_is_error) {

  const char *dirname = dirname_s.c_str();

  size_t pathmax = pathconf(dirname, _PC_NAME_MAX);
  char *filepath = new char [pathmax+1];

  log_debug(log, "dirname=\"%s\" is_common=%s not_present_is_error=%s\n",
      dirname, is_common ? "true" : "false",
      not_present_is_error ? "true" : "false");

  DIR *dir = opendir(dirname);
  if (!dir) {
    if (not_present_is_error) {
      log_err(log, "Cannot open directory \"%s\" for listing: %s\n", dirname, strerror(errno));
    }
    return 1;
  }

  struct dirent *direntrybuf = (struct dirent*) calloc(sizeof(struct dirent) + pathmax, 1);
  struct dirent *direntry;

  // this needs to be thread-safe because more than one game server could
  // be loading SDL files at the same time
  int32_t ret;
  errno = 0;
  while ((ret = readdir_r(dir, direntrybuf, &direntry)) == 0) {
    if (!direntry) {
      break;
    }
    int32_t ret = strlen(direntry->d_name);
    log_debug(log, "result=\"%s\" %s ret=%u\n", direntry->d_name, f_dirent(filepath, pathmax, direntry), ret);
    if (ret > 4 && !strcasecmp(&direntry->d_name[ret - 4], ".sdl")) {
      std::list<SDLDesc*> these;

      strncpy(filepath, dirname, pathmax);
      strncat(filepath, PATH_SEPARATOR, pathmax);
      strncat(filepath, direntry->d_name, pathmax);
      log_debug(log, "SDL open file=\"%s\" \n", filepath);
      std::ifstream file((const char *)filepath, std::ios_base::binary | std::ios_base::in);
      if (file.fail()) {
        log_err(log, "Cannot open SDL file \"%s\" \n", filepath);
        closedir(dir);
        free(direntrybuf);
        delete[] filepath;

        return -1;
      }
      try {
        SDLDesc::parse_file(these, file);
      } catch (const parse_error &e) {
        log_err(log, "Parse error in file \"%s\" line %u: %s\n", direntry->d_name, e.lineno(), e.what());
        closedir(dir);
        free(direntrybuf);
        delete[] filepath;
        return -1;
      }
      sdls.splice(sdls.end(), these);
    }
    errno = 0;
  }
  closedir(dir);

  free(direntrybuf);
  delete[] filepath;

  if (errno) {
    log_err(log, "Error reading directory %s: %s\n", dirname_s.c_str(), strerror(errno));
    return -1;
  }

  if (is_common) { // <SDL-dir>/common directory
    // now, move the most common SDLs to the front of the list
    std::list<SDLDesc*>::iterator avatarPhysical, avatar, MorphSequence, clothing, physical, Layer, iter;
    avatarPhysical = avatar = MorphSequence = clothing = physical = Layer = sdls.end();
    for (iter = sdls.begin(); iter != sdls.end(); iter++) {
      const char *sdlname = (*iter)->name();
      uint32_t namelen = strlen(sdlname);
      if (namelen == 14 && !strcasecmp(sdlname, "avatarPhysical")) {
        if (avatarPhysical == sdls.end() || ((*avatarPhysical)->version() < (*iter)->version())) {
          avatarPhysical = iter;
        }
      } else if (namelen == 8 && !strcasecmp(sdlname, "physical")) {
        if (physical == sdls.end() || ((*physical)->version() < (*iter)->version())) {
          physical = iter;
        }
      } else if (namelen == 6 && !strcasecmp(sdlname, "avatar")) {
        if (avatar == sdls.end() || ((*avatar)->version() < (*iter)->version())) {
          avatar = iter;
        }
        return 0;
      } else if (namelen == 5 && !strcasecmp(sdlname, "Layer")) {
        if (Layer == sdls.end() || ((*Layer)->version() < (*iter)->version())) {
          Layer = iter;
        }
      } else if (namelen == 13 && !strcasecmp(sdlname, "MorphSequence")) {
        if (MorphSequence == sdls.end() || ((*MorphSequence)->version() < (*iter)->version())) {
          MorphSequence = iter;
        }
      } else if (namelen == 8 && !strcasecmp(sdlname, "clothing")) {
        if (clothing == sdls.end() || ((*clothing)->version() < (*iter)->version())) {
          clothing = iter;
        }
      }
    }
    if (clothing != sdls.end() && clothing != sdls.begin()) {
      sdls.splice(sdls.begin(), sdls, clothing);
    }
    if (MorphSequence != sdls.end() && MorphSequence != sdls.begin()) {
      sdls.splice(sdls.begin(), sdls, MorphSequence);
    }
    if (Layer != sdls.end() && Layer != sdls.begin()) {
      sdls.splice(sdls.begin(), sdls, Layer);
    }
    if (avatar != sdls.end() && avatar != sdls.begin()) {
      sdls.splice(sdls.begin(), sdls, avatar);
    }
    if (physical != sdls.end() && physical != sdls.begin()) {
      sdls.splice(sdls.begin(), sdls, physical);
    }
    if (avatarPhysical != sdls.end() && avatarPhysical != sdls.begin()) {
      sdls.splice(sdls.begin(), sdls, avatarPhysical);
    }
  }
  return 0;
}

SDLDesc* SDLDesc::read_desc(std::ifstream &file, uint32_t &lineno, std::list<SDLDesc*> &descs) {
  std::string line, token1, token2, partial;
  const char *tokenc;
  SDLDesc *desc = NULL;
  bool started, openbrace, version, gotvar, complete;
  sdl_vartype_t type;
  SDLDesc::Variable *var = NULL;
  SDLDesc::Struct *sct = NULL;

  started = openbrace = version = complete = false;
  while (!file.eof()) {
    lineno++;
    gotvar = false;
    std::getline(file, line);
    if (file.fail() && !file.eof()) {
      if (desc) {
        delete desc;
      }
      throw parse_error(lineno, "error reading file");
    }
    std::stringstream ss(line);
    while (ss >> token1) {
      tokenc = token1.c_str();
      if (token1[0] == '#') {
        break;
      } else if (!started) {
        if (!strcasecmp(tokenc, "statedesc")) {
          started = true;
        } else {
          if (desc) {
            delete desc;
          }
          throw parse_error(lineno, "STATEDESC expected");
        }
      } else if (!desc) {
        // this should be the name
        // std::cout << "    Calling SDLDesc(" << token1 << ")\n";
        desc = new SDLDesc(token1);
      } else if (!openbrace) {
        if (strlen(tokenc) == 1 && tokenc[0] == '{') {
          openbrace = true;
        } else {
          delete desc;
          throw parse_error(lineno, "{ expected");
        }
      } else if (!version) {
        if (!strcasecmp(tokenc, "version")) {
          version = true;
        } else {
          delete desc;
          throw parse_error(lineno, "VERSION expected");
        }
      } else if (desc->version() <= 0) {
        // this should be the version number
        uint32_t version;
        std::stringstream vstr(token1);
        vstr >> version;
        if (vstr.fail()) {
          delete desc;
          throw parse_error(lineno, std::string("could not parse ") + token1 + " as number");
        }
        desc->set_version(version);
      } else if (strlen(tokenc) == 1 && tokenc[0] == '}') {
        // the end of the description
        complete = true;
        break;
      } else if (!strcasecmp(tokenc, "var")) {
        if (gotvar) {
          delete desc;
          throw parse_error(lineno, "more than one VAR on one line");
        }
        gotvar = true;
      } else if (!gotvar) {
        delete desc;
        throw parse_error(lineno, std::string("unrecognized token ") + token1);
      } else if (!var && !sct) {
        // here we are looking at different variable types
        if (token1[0] == '$') {
          // struct
          // note, we need to look up the pointer to the SDLDesc object
          // that *must* have been parsed from earlier in the same file, so
          // either it exists or there is an error
          SDLDesc *other = find_by_name(tokenc + 1, descs);
          if (other) {
            sct = new SDLDesc::Struct(other);
          } else {
            delete desc;
            throw parse_error(lineno, std::string("struct type ") + token1 + " not found");
          }
        } else if ((type = string_to_type(token1)) != SDLDesc::Variable::None) {
          var = new SDLDesc::Variable(type);
        } else {
          delete desc;
          throw parse_error(lineno, std::string("invalid type ") + token1);
        }
      } else {
        if (var) {
          if (!var->m_name) {
            // this is the name
            var->m_count = name_and_count(token1, lineno);
            // token1 has been modified, tokenc is not valid
            var->m_name = strdup(token1.c_str()); // XXX check return value
            continue;
          } else if (partial.length() > 0) {
            if (token1[0] == '=') {
              if (strchr(partial.c_str(), '=')) {
                delete var;
                delete desc;
                throw parse_error(lineno, "too many =");
              }
              if (token1.length() == 1) {
                partial = partial + token1;
                continue;
              } else {
                token1 = partial + token1;
                partial = "";
              }
            } else if (partial[partial.length() - 1] == '=') {
              token1 = partial + token1;
              partial = "";
            } else {
              delete var;
              delete desc;
              throw parse_error(lineno, "expected =");
            }
            tokenc = token1.c_str();
          }
          /*
           * The old assumption about "defaultoption" and "displayoption" being equivalent
           * wasn't correct.
           *
           * In CWE plSDL/plSDLParser.cpp, they are processed as follows:
           *
           * - DEFAULTOPTION only takes the VAULT flag.  No other values allowed.
           *   The VAULT flag sets the CWE flag kAlwaysNew.
           *
           * - DISPLAYOPTION takes a HIDDEN flag. and also appears to be unconstrained
           *   as to the names of other flags, up to 256 char max, possibly to pass to Python.
           *   The HIDDEN flag sets the CWE flag kInternal.
           *
           * There are two other attributes in the CWE code, INTERNAL and PHASED, but they
           * are marked with a comment to "delete me by May 2003" so I assume they are
           * deprecated and we won't see them again. As an aside, INTERNAL had the same
           * effect as DISPLAYOPTION=HIDDEN.
           */

          if (strcasestr(tokenc, "defaultoption") == tokenc) {
            // this test must be before "default"
            if (!strchr(tokenc, '=')) {
              partial = token1;
              continue;
            }
            if (strcasestr(tokenc, "vault")) {
              var->m_varoptions |= AlwaysNew;
            }
            else {
              std::string errmsg = std::string("var ") + std::string(var->m_name) + std::string(" unrecognized DEFAULTOPTION ") + token1;
              delete var;
              delete desc;
              throw parse_error(lineno, errmsg);
            }
          }
          else if (strcasestr(tokenc, "displayoption") == tokenc) {
            const char *at;
            if (!(at = strchr(tokenc, '='))) {
              partial = token1;
            }
            at++;
            if (strcasestr(tokenc, "hidden")) { // hidden has special meaning in flags
              var->m_varoptions |= Internal;
            }
            if (!var->m_dispoptions) { // build a string of option1[,option2]...
              var->m_dispoptions = new char[256];
              strncpy(var->m_dispoptions, at, 256);
            } else {
              strncat(var->m_dispoptions, ",", 256);
              strncat(var->m_dispoptions, at, 256);
            }
            continue;
          }
          else if (strcasestr(tokenc, "default") == tokenc) {
            const char *at;
            if (!(at = strchr(tokenc, '='))) {
              partial = token1;
              continue;
            }
            at++;
            std::stringstream vstr(at);
            switch (var->m_type) {

            case Variable::String32:
              if (!strcasecmp(at, "empty")) {
                // do nothing, we have an empty string representation already
              } else {
                strncpy(var->m_default.v_string, at, 32);
              }
              break;

            case Variable::Bool:
              if (strchr(at, '(')) {
                std::getline(vstr, token2, '(');
              }
              if (!strcasecmp(at, "true")) {
                var->m_default.v_bool = true;
              } else if (!strcasecmp(at, "false")) {
                // already zero
              } else {
                int32_t val;
                vstr >> val;
                if (vstr.fail()) {
                  delete var;
                  delete desc;
                  throw parse_error(lineno, std::string("non-boolean ") + at);
                }
                var->m_default.v_bool = (val ? true : false);
              }
              break;

            case Variable::Int:
            case Variable::Byte:
            case Variable::Short:
            case Variable::Time: // really 8 bytes?
            case Variable::AgeTimeOfDay:
              if (strchr(at, '(')) {
                std::getline(vstr, token2, '(');
              }
              int32_t val;
              vstr >> val;
              if (vstr.fail()) {
                delete var;
                delete desc;
                throw parse_error(lineno, std::string("cannot parse ") + at + " as an integer");
              }
              switch (var->m_type) {

              case Variable::Int:
                var->m_default.v_int = val;
                break;

              case Variable::Byte:
                var->m_default.v_byte = (int8_t) val;
                break;

              case Variable::Short:
                var->m_default.v_short = (int16_t) val;
                break;

              case Variable::Time:
                var->m_default.v_time.tv_sec = val;
                break;

              case Variable::AgeTimeOfDay:
                var->m_default.v_agetime.tv_sec = val;
                break;

              default:
                break;
              }
              break;

            case Variable::Float:
              float fval;
              vstr >> fval;
              if (vstr.fail()) {
                delete var;
                delete desc;
                throw parse_error(lineno, std::string("cannot parse ") + at + " as a float");
              }
              var->m_default.v_float = fval;
              break;

            case Variable::Key:
              if (strcmp(at, "nil")) {
                delete var;
                delete desc;
                throw parse_error(lineno, "PLKEY can't have a default");
              }
              break;

            case Variable::Creatable:
              delete var;
              delete desc;
              throw parse_error(lineno, "Creatable can't have a default");

            case Variable::Vector3:
            case Variable::Point3:
            case Variable::Quaternion:
              // tuples
            {
              std::getline(vstr, token2, '(');
              if (vstr.fail()) {
                delete var;
                delete desc;
                throw parse_error(lineno, "tuple expected, ( not found");
              }
              int32_t howmany = 3;
              if (var->m_type == Variable::Quaternion) {
                howmany++;
              }
              for (int32_t i = 0; i < howmany; i++) {
                std::getline(vstr, token2, (i + 1 == howmany ? ')' : ','));
                if (vstr.fail()) {
                  delete var;
                  delete desc;
                  throw parse_error(lineno, "tuple incomplete");
                }
                std::stringstream nstr(token2);
                float nval;
                nstr >> nval;
                if (nstr.fail()) {
                  delete var;
                  delete desc;
                  throw parse_error(lineno, std::string("cannot parse ") + token2 + " as a float");
                }
                switch (var->m_type) {

                case Variable::Vector3:
                  var->m_default.v_vector3[i] = nval;
                  break;

                case Variable::Point3:
                  var->m_default.v_point3[i] = nval;
                  break;

                case Variable::Quaternion:
                  var->m_default.v_quaternion[i] = nval;
                  break;

                default:
                  break;
                }
              }
            }
              break;

            case Variable::RGB8:
              // tuples as well
              std::getline(vstr, token2, '(');
              if (vstr.fail()) {
                delete var;
                delete desc;
                throw parse_error(lineno, "tuple expected, ( not found");
              }
              for (int32_t i = 0; i < 3; i++) {
                std::getline(vstr, token2, (i == 2 ? ')' : ','));
                if (vstr.fail()) {
                  delete var;
                  delete desc;
                  throw parse_error(lineno, "tuple incomplete");
                }
                std::stringstream nstr(token2);
                uint32_t nval;
                nstr >> nval;
                if (nstr.fail()) {
                  delete var;
                  delete desc;
                  throw parse_error(lineno, std::string("cannot parse ") + token2 + " as an integer");
                }
                var->m_default.v_rgb8[i] = (uint8_t) nval;
              }
              break;

            case Variable::None:
            default:
              delete var;
              delete desc;
              throw parse_error(lineno, "DEFAULT present for unknown SDL type");
            }
          } // end DEFAULT
          else {
            delete var;
            delete desc;
            throw parse_error(lineno, std::string("unrecognized token ") + token1);
          }
        } else {
          if (!sct->m_name) {
            // this is the name
            sct->m_count = name_and_count(token1, lineno);
            sct->m_name = strdup(token1.c_str()); // XXX check return value
          }
        }
      }
    } // while (ss >> token1)

    // sanity-check and store any var/struct
    if (var) {
      if (!var->m_name) {
        delete var;
        delete desc;
        throw parse_error(lineno, "no name found before end of line");
      }
      desc->add_var(var);
      var = NULL;
    } else if (sct) {
      if (!sct->m_name) {
        delete sct;
        delete desc;
        throw parse_error(lineno, "no name found before end of line");
      }
      desc->add_struct(sct);
      sct = NULL;
    }
    if (complete) {
      break;
    }
  } // while (!file.eof())

  if (!complete && desc) {
    // city.sdl
    delete desc;
    desc = NULL;
  }
  return desc;
}

SDLDesc::sdl_vartype_t SDLDesc::string_to_type(std::string &s) {
  int32_t len = s.length();
  const char *str = s.c_str();
  if (len == 3 && !strcasecmp(str, "int")) {
    return Variable::Int;
  } else if (len == 4) {
    if (!strcasecmp(str, "bool")) {
      return Variable::Bool;
    } else if (!strcasecmp(str, "time")) {
      return Variable::Time;
    } else if (!strcasecmp(str, "byte")) {
      return Variable::Byte;
    } else if (!strcasecmp(str, "rgb8")) {
      return Variable::RGB8;
    }
  } else if (len == 5) {
    if (!strcasecmp(str, "float")) {
      return Variable::Float;
    } else if (!strcasecmp(str, "plkey")) {
      return Variable::Key;
    } else if (!strcasecmp(str, "short")) {
      return Variable::Short;
    }
  } else if (len == 6 && !strcasecmp(str, "point3")) {
    return Variable::Point3;
  } else if (len == 7) {
    if (!strcasecmp(str, "vector3")) {
      return Variable::Vector3;
    } else if (!strcasecmp(str, "message")) {
      return Variable::Creatable;
    }
  } else if (len == 8 && !strcasecmp(str, "string32")) {
    return Variable::String32;
  } else if (len == 9 && !strcasecmp(str, "creatable")) {
    return Variable::Creatable;
  } else if (len == 10 && !strcasecmp(str, "quaternion")) {
    return Variable::Quaternion;
  } else if (len == 12 && !strcasecmp(str, "agetimeofday")) {
    return Variable::AgeTimeOfDay;
  }
  return Variable::None;
}

const char* SDLDesc::Variable::vartype_c_str(sdl_vartype_t t) {
  switch (t) {
      case Variable::None:            return "None";
      case Variable::Int:             return "Int";
      case Variable::Float:           return "Float";
      case Variable::Bool:            return "Bool";
      case Variable::String32:        return "String32";
      case Variable::Key:             return "Key";
      case Variable::StateDescriptor: return "StateDescriptor";
      case Variable::Creatable:       return "Creatable";
      case Variable::Double:          return "Double";
      case Variable::Time:            return "Time";
      case Variable::Byte:            return "Byte";
      case Variable::Short:           return "Short";
      case Variable::AgeTimeOfDay:    return "AgeTimeOfDay";
      case Variable::Vector3:         return "Vector3";
      case Variable::Point3:          return "Point3";
      case Variable::RGB:             return "RGB";
      case Variable::RGBA:            return "RGBA";
      case Variable::Quaternion:      return "Quaternion";
      case Variable::RGB8:            return "RGB8";
      case Variable::RGBA8:           return "RGBA8";
  default:
      return "(unknown)";
  }
}

const char* SDLDesc::varoptions_c_str(uint32_t t) {
  // return constant to avoid need to manage dynamic strings
  switch (t) {
  case AlwaysNew:          return "AlwaysNew";
  case Internal:           return "Internal";
  case AlwaysNew|Internal: return "AlwaysNew|Internal";
  default:
    return "";
  }
}

uint32_t SDLDesc::name_and_count(std::string &namestr, uint32_t lineno) {
  // parse something of the form word[] or word[1]
  const char *s = namestr.c_str();
  const char *bracket = strchr(s, '[');
  if (!bracket) {
    throw parse_error(lineno, "[ not found");
  }
  if (bracket == s) {
    throw parse_error(lineno, "no variable name present");
  }
  uint32_t val;
  if (bracket[1] == '\0') {
    throw parse_error(lineno, "] not found");
  } else if (bracket[1] == ']') {
    val = 0;
  } else {
    std::stringstream ss(bracket + 1);
    ss >> val;
    if (ss.fail()) {
      throw parse_error(lineno, "non-numeric variable count");
    }
  }
  namestr = namestr.substr(0, (uint32_t) (bracket - s));
  return val;
}

SDLDesc::Variable::~Variable() {
  if (m_dispoptions) {
    delete[] m_dispoptions;
    m_dispoptions = NULL;
  }
}

std::string SDLDesc::Variable::str(sdl_vartype_t t, data_t &d) {

  std::ostringstream out;
  char ctimebuf[32];
  char keybuf[64];
  char *c;
  time_t timebuf;

  switch (t) {
  case Int:         out << (int32_t)d.v_int; break;
  case Byte:        out << (int32_t)d.v_byte; break;
  case Short:       out << (int32_t)d.v_short; break;
  case Float:       out << d.v_float; break;
  case Bool:        out << std::boolalpha << d.v_bool; break;
  case String32:    out << "\"" << d.v_string << "\""; break;

  case Key:
    c = d.v_plkey.c_str(keybuf, sizeof(keybuf));
    out << c;
    break;

  case Creatable:   out << "Creatable[" << read32(d.v_creatable, 0) << "]"; break;

  case Time:
    timebuf = std::time(&d.v_time.tv_sec);
    if (std::strftime(ctimebuf, sizeof(ctimebuf), "%Y-%m-%d %H:%M:%S", std::localtime(&timebuf))) {
      out << ctimebuf;
    }
    break;

  case AgeTimeOfDay:
    timebuf = std::time(&d.v_agetime.tv_sec);
    if (std::strftime(ctimebuf, sizeof(ctimebuf), "%Y-%m-%d %H:%M:%S", std::localtime(&timebuf))) {
      out << ctimebuf;
    }
    break;

  case Vector3:     out << "<" << d.v_vector3[0] << ","
                               << d.v_vector3[1] << ","
                               << d.v_vector3[2] << ">";
    break;

  case Point3:      out << "<" << d.v_point3[0] << ","
                               << d.v_point3[1] << ","
                               << d.v_point3[2] << ">";
    break;

  case Quaternion:  out << "<" << d.v_quaternion[0] << ","
                               << d.v_quaternion[1] << ","
                               << d.v_quaternion[2] << ","
                               << d.v_quaternion[3] << ">";
    break;

  case RGB8:        out << "<" << d.v_rgb8[0] << ","
                               << d.v_rgb8[1] << ","
                               << d.v_rgb8[2] << ">";
  break;

  default:
    out << "!INVALID!";
  }
  return out.str();

}

char* SDLDesc::Variable::c_str(char* buf, size_t buflen, sdl_vartype_t t, SDLDesc::Variable::data_t &d) {
  return strncpy(buf, SDLDesc::Variable::str(t, d).c_str(), buflen);
}

std::string SDLDesc::Variable::str() {
  std::ostringstream out;

  out << m_name << "<" << vartype_c_str(m_type);
  if (m_count > 1) {
    out << "[" << m_count << "]";
  }
  if (m_varoptions) {
    out << "#" << varoptions_c_str(m_varoptions);
  }
  if (m_dispoptions) {
    out << "%" << m_dispoptions;
  }
  out << ">(" << str(m_type, m_default) << ")";

  m_string = out.str();
  return m_string;
}

const char* SDLDesc::Variable::c_str() {
  m_chars = str().c_str();
  return m_chars;
}

std::string SDLDesc::Struct::str(const char *sep) {
  std::ostringstream out;

  out << m_name << "{";
  for (uint32_t i = 0; i < m_count; i++) {
    if (i > 0)
      out << sep;
    out << m_data[i].str(sep);
  }
  out << "}";

  m_string = out.str();
  return m_string;
}

const char* SDLDesc::Struct::c_str(const char *sep) {
  m_chars = str(sep).c_str();
  return m_chars;
}

std::string SDLDesc::str(const char *sep) {
  std::ostringstream out;
  std::list<std::string> elements;
  std::string e;
  std::vector<Variable*>::iterator vi;
  std::vector<Struct*>::iterator si;

  elements.clear();

  for (vi = m_vars.begin(); vi != m_vars.end(); vi++) {
    elements.push_back((*vi)->str());
  }
  for (si = m_structs.begin(); si != m_structs.end(); si++) {
    elements.push_back((*si)->str(sep));
  }

  out << "{SDLDesc: " << m_name << "-v" << m_version << sep;
  if (!elements.empty()) {
    e = elements.back();
    elements.pop_back();
  }
  std::copy(elements.begin(), elements.end(), std::ostream_iterator<std::string>(out, sep));
  if (!e.empty())
    out << e;
  out << "}";

  m_string = out.str();
  return m_string;
}

const char* SDLDesc::c_str(const char *sep) {
  m_chars = str(sep).c_str();
  return m_chars;
}

// utility functions
uint32_t do_message_compression(uint8_t *buf) {
  uint32_t ret = 0;

  if (buf[4] == CompressionDont) {
    return 0;
  }
  uint32_t len = read32(buf, 5);
  if (len > COMPRESS_THRESHOLD) {
    /* the zlib man page says the destination must be at least 0.1% larger
     than the source buffer + 12 bytes */
    unsigned long destlen = len + (len / 1000) + 13;
    uint8_t *buf2 = new uint8_t[destlen];

    int32_t zlib_ret = compress(buf2, &destlen, buf + 11, len - 2);
    if (zlib_ret == Z_OK && destlen < len - 2) {
      ret = destlen + 2;
      write32(buf, 0, len);
      buf[4] = CompressionZlib;
      write32(buf, 5, ret);
      memcpy(buf + 11, buf2, destlen);
    } else if (zlib_ret != Z_OK) {
      buf[4] = CompressionFailed;
    }
    delete[] buf2;
  }

  return ret;
}

SDLState::SDLState(const SDLDesc *desc) :
    m_flag(0), m_desc(NULL), m_saving_to_file(false) {
  if (desc) {
    set_desc(desc);
  }
  m_key.make_null();
}

SDLState::~SDLState() {
  m_key.delete_name();
  std::vector<Variable*>::iterator vi;
  for (vi = m_vars.begin(); vi != m_vars.end(); vi++) {
    if (*vi) {
      delete *vi;
    }
  }
  std::vector<Struct*>::iterator si;
  for (si = m_structs.begin(); si != m_structs.end(); si++) {
    if (*si) {
      delete *si;
    }
  }
}

int32_t SDLState::read_msg(const uint8_t *buf, size_t bufsize, const std::list<SDLDesc*> &descs) {
  uint32_t offset = m_key.read_in(buf, bufsize);
  if (bufsize < offset + 11) {
    throw truncated_message("SDL message ends in in-between stuff");
  }
  uint32_t uncompressed_len = read32(buf, offset);
  offset += 4;
  bool compressed = (buf[offset++] == CompressionZlib);
  uint32_t len = read32(buf, offset);
  offset += 4;
  if (read16(buf, offset) != no_plType) {
    // XXX bad SDL
  }
  // the 0x8000 is counted in the length, even though it's not in the
  // compressed part of the buffer, because the generic compression routine
  // always sticks the type up front (and having no type, SDL uses 0x8000),
  // so yes this does go before offset += 2 (or use "offset+len-2")
  if (bufsize < offset + len) {
    throw truncated_message("SDL message incomplete");
  }
  offset += 2;
  int32_t readlen = 0;
  if (compressed) {
    // compressed
    uint8_t *buf2 = new uint8_t[uncompressed_len];

    unsigned long unclen = uncompressed_len;
    int32_t zlib_ret = uncompress(buf2, &unclen, buf + offset, len - 2);
    if (unclen != uncompressed_len) {
      // XXX something is wrong
    }
    if (zlib_ret == Z_OK) {
      readlen = read_in(buf2, unclen, descs);
      delete[] buf2;
      if (readlen > 0 && ((uint32_t) readlen != uncompressed_len)) {
        // XXX big problem
      }
    } else {
      delete[] buf2;
      if (zlib_ret == Z_MEM_ERROR) {
        throw std::bad_alloc();
      } else if (zlib_ret == Z_BUF_ERROR) {
        throw parse_error(0, "Data decompressed larger than claimed length");
      } else if (zlib_ret == Z_DATA_ERROR) {
        throw parse_error(0, "Compressed data corrupted");
      }
    }
  } else {
    // uncompressed
    readlen = read_in(buf + offset, len - 2, descs);
    if (readlen > 0 && readlen + 2 != (int32_t) len) {
      // XXX big problem
    }
  }
  offset += len - 2;
  if (readlen < 0) {
    // unrecognized SDL
    return -offset;
  }
  return offset;
}

uint32_t SDLState::send_len() const {
  if (!m_desc) {
    // XXX programmer error
    return 0;
  }
  uint32_t len = 11; // for compression/length info, 0x8000
  len += m_key.send_len();
  len += body_len();
  return len;
}

int32_t SDLState::write_msg(uint8_t *buf, size_t bufsize, bool no_compress) {
  uint32_t len, wrote, offset;
  len = m_key.send_len();
  if (bufsize < len + 11) {
    return -1;
  }
  wrote = m_key.write_out(buf, bufsize, true);
  if (wrote != len) {
    // XXX code error
  }
  offset = wrote;
  wrote = write_out(buf + offset + 11, bufsize - (offset + 11));
  if (wrote < 0) {
    // buffer too small -- shouldn't happen
    return -1;
  }
  uint32_t start_at = offset;
  write32(buf, offset, 0);
  offset += 4;
  buf[offset++] = CompressionNone;
  write32(buf, offset, wrote + 2);
  offset += 4;
  write16(buf, offset, no_plType);
  if (!no_compress) {
    uint32_t len2 = do_message_compression(buf + start_at);
    if (len2) {
      offset += len2;
    } else {
      offset += wrote + 2;
    }
  } else {
    offset += wrote + 2;
  }
  return offset;
}

void SDLState::set_desc(const SDLDesc *desc) {
  if (m_desc != NULL) {
    // programmer error
    throw std::logic_error("An SDLState's type cannot be changed");
  }
  m_desc = desc;
}

int32_t SDLState::read_in(const uint8_t *buf, size_t bufsize, const std::list<SDLDesc*> &descs) {
  UruString name(buf, (int32_t) bufsize, true, false, false);
  uint32_t offset = name.arrival_len();
  if (bufsize < offset + 2) {
    throw truncated_message("SDL message too short after name");
  }
  uint16_t version = read16(buf, offset);
  offset += 2;
  m_desc = SDLDesc::find_by_name(name.c_str(), descs, version);
  if (!m_desc) {
    // unrecognized SDL: caller should log & ignore it
    return -1;
  }

  // now recursively parse the message
  int32_t ret = recursive_parse(buf + offset, bufsize - offset);
  if (ret < 0) {
    return ret;
  } else {
    return offset + ret;
  }
}

int32_t SDLState::recursive_parse(const uint8_t *buf, size_t bufsize) {
  if (bufsize < 4) {
    throw truncated_message("SDL message too short for first fields");
  }
  m_flag = read16(buf, 0);
  // I think m_flag 1 is a "don't keep persistent" flag.
  uint32_t offset = 2;
  if (buf[offset++] != IOVersion) {
    // XXX unexpected value
  }
  uint32_t num_vars = buf[offset++];
  bool has_indices = (num_vars < m_desc->vars().size());
  for (uint32_t i = 0; i < num_vars; i++) {
    if (bufsize < offset + 1) {
      throw truncated_message("SDL message too short");
    }
    uint32_t idx = (has_indices ? buf[offset++] : i);
    if (idx >= m_desc->vars().size()) {
      // bad SDL
      throw parse_error(idx, std::string("index too large"));
    }
    if (bufsize < offset + 1) {
      throw truncated_message("SDL message too short");
    }
    if (buf[offset++] & HasNotificationInfo) { // plStateVariable saveFlags (also plStateSimpleVariable)
      if (bufsize < offset + 3) {
        throw truncated_message("SDL message too short");
      }
      if (buf[offset++] != 0) { // NotificationInfo Flags
        // XXX unexpected value
      }
      // now there is an URUSTRING
      if (read16(buf, offset) != 0xf000) { // zero-length ORed with 0xf000
        UruString hintstring(buf + offset, bufsize - offset, true, false, false);
        // I don't need to keep this string, since I am going to send the
        // original SDL message to other clients, and this structure is only
        // used for persistent information
        offset += hintstring.arrival_len();
      } else {
        offset += 2;
      }
    }
    SDLDesc::Variable *d_var = m_desc->vars()[idx];
    Variable *v = new Variable(idx, d_var->m_type);
    m_vars.push_back(v);
    if (bufsize < offset + 1) {
      throw truncated_message("SDL message too short");
    }
    v->m_flags = buf[offset++];
    if (v->m_flags & HasTimeStamp) {
      if (bufsize < offset + 8) {
        throw truncated_message("SDL message too short");
      }
      v->m_ts.tv_sec = read32(buf, offset);
      offset += 4;
      v->m_ts.tv_usec = read32(buf, offset);
      offset += 4;
    }
    if (v->m_flags & SameAsDefault) {
      continue;
    }

    // here we have the data
    v->m_count = d_var->m_count;
    if (v->m_count == 0) {
      if (bufsize < offset + 4) {
        throw truncated_message("SDL message too short");
      }
      v->m_count = read32(buf, offset);
      offset += 4;
    }
    v->m_value = new SDLDesc::Variable::data_t[v->m_count];
    switch (d_var->m_type) {

    case SDLDesc::Variable::Int:
      if (bufsize < offset + (v->m_count * 4)) {
        throw truncated_message("SDL message too short");
      }
      for (uint32_t j = 0; j < v->m_count; j++) {
        v->m_value[j].v_int = read32(buf, offset);
        offset += 4;
      }
      break;

    case SDLDesc::Variable::Float:
      if (bufsize < offset + (v->m_count * 4)) {
        throw truncated_message("SDL message too short");
      }
      for (uint32_t j = 0; j < v->m_count; j++) {
        uint32_t val = read32(buf, offset);
        memcpy(&(v->m_value[j].v_float), &val, 4);
        offset += 4;
      }
      break;

    case SDLDesc::Variable::Bool:
      if (bufsize < offset + (v->m_count)) {
        throw truncated_message("SDL message too short");
      }
      for (uint32_t j = 0; j < v->m_count; j++) {
        v->m_value[j].v_bool = (buf[offset++] ? true : false);
      }
      break;

    case SDLDesc::Variable::String32:
      if (bufsize < offset + (v->m_count * 32)) {
        throw truncated_message("SDL message too short");
      }
      for (uint32_t j = 0; j < v->m_count; j++) {
        memcpy(v->m_value[j].v_string, buf + offset, 32);
        offset += 32;
      }
      break;

    case SDLDesc::Variable::Key:
      for (uint32_t j = 0; j < v->m_count; j++) {
        offset += v->m_value[j].v_plkey.read_in(buf + offset, bufsize - offset);
      }
      break;

    case SDLDesc::Variable::Creatable:
      // note this line means we can't store object clones in the save
      // file (they are currently discarded anyway)
      throw parse_error(idx, std::string("Creatable cannot be transmitted"));

    case SDLDesc::Variable::Time:
      if (bufsize < offset + (v->m_count * 8)) {
        throw truncated_message("SDL message too short");
      }
      for (uint32_t j = 0; j < v->m_count; j++) {
        v->m_value[j].v_time.tv_sec = read32(buf, offset);
        v->m_value[j].v_time.tv_usec = read32(buf, offset + 4);
        offset += 8;
      }
      break;

    case SDLDesc::Variable::Byte:
      if (bufsize < offset + (v->m_count)) {
        throw truncated_message("SDL message too short");
      }
      for (uint32_t j = 0; j < v->m_count; j++) {
        v->m_value[j].v_byte = buf[offset++];
      }
      break;

    case SDLDesc::Variable::Short:
      if (bufsize < offset + (v->m_count * 2)) {
        throw truncated_message("SDL message too short");
      }
      for (uint32_t j = 0; j < v->m_count; j++) {
        v->m_value[j].v_short = read16(buf, offset);
        offset += 2;
      }
      break;

    case SDLDesc::Variable::AgeTimeOfDay:
      if (bufsize < offset + (v->m_count * 8)) {
        throw truncated_message("SDL message too short");
      }
      for (uint32_t j = 0; j < v->m_count; j++) {
        v->m_value[j].v_agetime.tv_sec = read32(buf, offset);
        v->m_value[j].v_agetime.tv_usec = read32(buf, offset + 4);
        offset += 8;
      }
      break;

    case SDLDesc::Variable::Vector3:
    case SDLDesc::Variable::Point3:
      if (bufsize < offset + (v->m_count * 12)) {
        throw truncated_message("SDL message too short");
      }
      for (uint32_t j = 0; j < v->m_count; j++) {
        float *where = (d_var->m_type == SDLDesc::Variable::Vector3 ? v->m_value[j].v_vector3 : v->m_value[j].v_point3);
        for (uint32_t k = 0; k < 3; k++) {
          uint32_t val = read32(buf, offset);
          memcpy(where + k, &val, 4);
          offset += 4;
        }
      }
      break;

    case SDLDesc::Variable::Quaternion:
      if (bufsize < offset + (v->m_count * 16)) {
        throw truncated_message("SDL message too short");
      }
      for (uint32_t j = 0; j < v->m_count; j++) {
        for (uint32_t k = 0; k < 4; k++) {
          uint32_t val = read32(buf, offset);
          memcpy((v->m_value[j].v_quaternion) + k, &val, 4);
          offset += 4;
        }
      }
      break;

    case SDLDesc::Variable::RGB8:
      if (bufsize < offset + (v->m_count * 3)) {
        throw truncated_message("SDL message too short");
      }
      for (uint32_t j = 0; j < v->m_count; j++) {
        for (uint32_t k = 0; k < 3; k++) {
          v->m_value[j].v_rgb8[k] = buf[offset++];
        }
      }
      break;

    default:
      // can't happen
      break;
    }
  } // for (i)

    // now we have structs
  if (bufsize < offset + 1) {
    throw truncated_message("SDL message too short");
  }
  num_vars = buf[offset++];
  has_indices = (num_vars < m_desc->structs().size());
  for (uint32_t i = 0; i < num_vars; i++) {
    if (bufsize < offset + 1) {
      throw truncated_message("SDL message too short");
    }
    uint32_t idx = (has_indices ? buf[offset++] : i);
    if (idx >= m_desc->structs().size()) {
      // bad SDL
      throw parse_error(idx, std::string("index too large"));
    }
    if (bufsize < offset + 1) {
      throw truncated_message("SDL message too short");
    }
    if (buf[offset++] == 0x02) {
      if (bufsize < offset + 3) {
        throw truncated_message("SDL message too short");
      }
      if (buf[offset++] != 0) {
        // XXX unexpected value
      }
      // now there is an URUSTRING
      if (read16(buf, offset) != 0xf000) {
        UruString tagstring(buf + offset, bufsize - offset, true, false, false);
        // I don't need to keep this string (see previous tagstring comment)
        offset += tagstring.arrival_len();
      } else {
        offset += 2;
      }
    }
    SDLDesc::Struct *d_struct = m_desc->structs()[idx];
    Struct *s = new Struct(idx, d_struct->m_data);
    m_structs.push_back(s);
    if (bufsize < offset + 1) {
      throw truncated_message("SDL message too short");
    }
    s->m_flags = buf[offset++];
    if (s->m_flags & HasTimeStamp) {
      if (bufsize < offset + 8) {
        throw truncated_message("SDL message too short");
      }
      s->m_ts.tv_sec = read32(buf, offset);
      offset += 4;
      s->m_ts.tv_usec = read32(buf, offset);
      offset += 4;
    }
    if (s->m_flags & SameAsDefault) {
      continue;
    }

    // here we have the data
    s->m_count = d_struct->m_count;
    if (s->m_count == 0) {
      if (bufsize < offset + 4) {
        throw truncated_message("SDL message too short");
      }
      s->m_count = read32(buf, offset);
      offset += 4;
    }
    // XXX unknown "Sub SDL record lead"
    if (bufsize < offset + 1) {
      throw truncated_message("SDL message too short");
    }
    if (buf[offset++] != 1) {
      // XXX log it?
    }
    s->m_child = new SDLState[s->m_count];
    for (uint32_t j = 0; j < s->m_count; j++) {
      // recurse
      s->m_child[j].set_desc(d_struct->m_data);
      int32_t ret = s->m_child[j].recursive_parse(buf + offset, bufsize - offset);
      if (ret < 0) {
        return ret;
      }
      offset += ret;
    }
  } // for (i)

  return offset;
}

int32_t SDLState::write_out(uint8_t *buf, size_t bufsize) const {
  if (!m_desc) {
    // XXX programmer error
    return -1;
  }
  UruString name(m_desc->name());
  uint32_t offset = name.send_len(true, false, false);
  if (bufsize < offset + 2) {
    // XXX problem
    return -1;
  }
  memcpy(buf, name.get_str(true, false, false, true), offset);
  write16(buf, offset, m_desc->version());
  offset += 2;

  // now recursively write the contents
  int32_t ret = recursive_write(buf + offset, bufsize - offset);
  if (ret < 0) {
    return ret;
  } else {
    return (int32_t) offset + ret;
  }
}

int32_t SDLState::recursive_write(uint8_t *buf, size_t bufsize) const {
  if (bufsize < 4) {
    return -1;
  }
  write16(buf, 0, m_flag);
  uint32_t offset = 2;
  buf[offset++] = IOVersion;
  // How much to write? In UU, IIRC the server would not send SDLs that
  // were "Default", but in MOUL it pretty much sends them all, but not
  // all (e.g. all are sent for Personal except RewardClothing, and hmm,
  // maybe this is why the first month shirts disappeared?)
  uint32_t num_vars = 0;
  for (uint32_t i = 0; i < m_vars.size(); i++) {
    Variable *v = m_vars[i];
    if (v && (!(v->m_flags & SameAsDefault) || (v->m_flags & HasTimeStamp))) {
      num_vars++;
    }
  }
  bool has_indices = (num_vars < m_desc->vars().size());
  buf[offset++] = num_vars;
  for (uint32_t i = 0; i < m_vars.size(); i++) {
    Variable *v = m_vars[i];
    if (!v
        || ((v->m_flags & SameAsDefault)
             && !(v->m_flags & HasTimeStamp))) {
      continue;
    }
    if (has_indices) {
      if (bufsize < offset + 1) {
        return -1;
      }
      buf[offset++] = v->m_index;
    }
    if (bufsize < offset + 5) {
      return -1;
    }
    buf[offset++] = HasNotificationInfo; // CWE plStateVariable::WriteData()
    buf[offset++] = 0;                   // CWE plStateVarNotificationInfo::Write(), unused
    write16(buf, offset, 0xf000);        // zero-length UruString
    offset += 2;
    buf[offset++] = v->m_flags;          // saveFlags in plSimpleStateVariable::WriteData()
    if (v->m_flags & HasTimeStamp) {
      if (bufsize < offset + 8) {
        return -1;
      }
      write32(buf, offset, v->m_ts.tv_sec);
      write32(buf, offset + 4, v->m_ts.tv_usec);
      offset += 8;
    }
    if (v->m_flags & SameAsDefault) {
      continue;
    }

    // here we have the data
    SDLDesc::Variable *d_var = m_desc->vars()[v->m_index];
    if (d_var->m_count == 0) {
      if (bufsize < offset + 4) {
        return -1;
      }
      write32(buf, offset, v->m_count);
      offset += 4;
    }
    switch (d_var->m_type) {

    case SDLDesc::Variable::Int:
      if (bufsize < offset + (v->m_count * 4)) {
        return -1;
      }
      for (uint32_t j = 0; j < v->m_count; j++) {
        write32(buf, offset, v->m_value[j].v_int);
        offset += 4;
      }
      break;

    case SDLDesc::Variable::Float:
      if (bufsize < offset + (v->m_count * 4)) {
        return -1;
      }
      for (uint32_t j = 0; j < v->m_count; j++) {
        uint32_t val;
        memcpy(&val, &(v->m_value[j].v_float), 4);
        write32(buf, offset, val);
        offset += 4;
      }
      break;

    case SDLDesc::Variable::Bool:
      if (bufsize < offset + (v->m_count)) {
        return -1;
      }
      for (uint32_t j = 0; j < v->m_count; j++) {
        buf[offset++] = (v->m_value[j].v_bool ? 1 : 0);
      }
      break;

    case SDLDesc::Variable::String32:
      if (bufsize < offset + (v->m_count * 32)) {
        return -1;
      }
      for (uint32_t j = 0; j < v->m_count; j++) {
        memcpy(buf + offset, v->m_value[j].v_string, 32);
        offset += 32;
      }
      break;

    case SDLDesc::Variable::Key:
      for (uint32_t j = 0; j < v->m_count; j++) {
        PlKey *key = &(v->m_value[j].v_plkey);
        if (bufsize < offset + key->send_len()) {
          return -1;
        }
        key->write_out(buf + offset, bufsize - offset);
        offset += key->send_len();
      }
      break;

    case SDLDesc::Variable::Creatable:
      // writing these values is not supported
      write32(buf, offset, 0);
      offset += 4;
      break;

    case SDLDesc::Variable::Time:
      if (bufsize < offset + (v->m_count * 8)) {
        return -1;
      }
      for (uint32_t j = 0; j < v->m_count; j++) {
        write32(buf, offset, v->m_value[j].v_time.tv_sec);
        write32(buf, offset + 4, v->m_value[j].v_time.tv_usec);
        offset += 8;
      }
      break;

    case SDLDesc::Variable::Byte:
      if (bufsize < offset + (v->m_count)) {
        return -1;
      }
      for (uint32_t j = 0; j < v->m_count; j++) {
        buf[offset++] = v->m_value[j].v_byte;
      }
      break;

    case SDLDesc::Variable::Short:
      if (bufsize < offset + (v->m_count * 2)) {
        return -1;
      }
      for (uint32_t j = 0; j < v->m_count; j++) {
        write16(buf, offset, v->m_value[j].v_short);
        offset += 2;
      }
      break;

    case SDLDesc::Variable::AgeTimeOfDay:
      if (bufsize < offset + (v->m_count * 8)) {
        return -1;
      }
      for (uint32_t j = 0; j < v->m_count; j++) {
        write32(buf, offset, v->m_value[j].v_agetime.tv_sec);
        write32(buf, offset + 4, v->m_value[j].v_agetime.tv_usec);
        offset += 8;
      }
      break;

    case SDLDesc::Variable::Vector3:
    case SDLDesc::Variable::Point3:
      if (bufsize < offset + (v->m_count * 12)) {
        return -1;
      }
      for (uint32_t j = 0; j < v->m_count; j++) {
        float *where = (d_var->m_type == SDLDesc::Variable::Vector3 ? v->m_value[j].v_vector3 : v->m_value[j].v_point3);
        for (uint32_t k = 0; k < 3; k++) {
          uint32_t val;
          memcpy(&val, where + k, 4);
          write32(buf, offset, val);
          offset += 4;
        }
      }
      break;

    case SDLDesc::Variable::Quaternion:
      if (bufsize < offset + (v->m_count * 16)) {
        return -1;
      }
      for (uint32_t j = 0; j < v->m_count; j++) {
        for (uint32_t k = 0; k < 4; k++) {
          uint32_t val;
          memcpy(&val, (v->m_value[j].v_quaternion) + k, 4);
          write32(buf, offset, val);
          offset += 4;
        }
      }
      break;

    case SDLDesc::Variable::RGB8:
      if (bufsize < offset + (v->m_count * 3)) {
        return -1;
      }
      for (uint32_t j = 0; j < v->m_count; j++) {
        for (uint32_t k = 0; k < 3; k++) {
          buf[offset++] = v->m_value[j].v_rgb8[k];
        }
      }
      break;

    default:
      // can't happen
      break;
    }
  } // for (i)

    // now we have structs
  if (bufsize < offset + 1) {
    return -1;
  }
  num_vars = 0;
  for (uint32_t i = 0; i < m_structs.size(); i++) {
    Struct *s = m_structs[i];
    if (s && (!(s->m_flags & SameAsDefault) || (s->m_flags & HasTimeStamp))) {
      num_vars++;
    }
  }
  has_indices = (num_vars < m_desc->structs().size());
  buf[offset++] = num_vars;
  for (uint32_t i = 0; i < m_structs.size(); i++) {
    Struct *s = m_structs[i];
    if (!s || ((s->m_flags & SameAsDefault) && !(s->m_flags & HasTimeStamp))) {
      continue;
    }
    if (has_indices) {
      if (bufsize < offset + 1) {
        return -1;
      }
      buf[offset++] = s->m_index;
    }
    if (bufsize < offset + 5) {
      return -1;
    }
    write32(buf, offset, 0xf0000002);
    offset += 4;
    buf[offset++] = s->m_flags;
    if (s->m_flags & HasTimeStamp) {
      if (bufsize < offset + 8) {
        return -1;
      }
      write32(buf, offset, s->m_ts.tv_sec);
      write32(buf, offset + 4, s->m_ts.tv_usec);
      offset += 8;
    }
    if (s->m_flags & SameAsDefault) {
      continue;
    }

    // here we have the data
    SDLDesc::Struct *d_struct = m_desc->structs()[s->m_index];
    if (d_struct->m_count == 0) {
      if (bufsize < offset + 4) {
        return -1;
      }
      write32(buf, offset, s->m_count);
      offset += 4;
    }
    // XXX unknown "Sub SDL record lead"
    if (bufsize < offset + 1) {
      return -1;
    }
    buf[offset++] = (uint8_t) (s->m_count & 0xFF);
    for (uint32_t j = 0; j < s->m_count; j++) {
      // recurse
      int32_t ret = s->m_child[j].recursive_write(buf + offset, bufsize - offset);
      if (ret < 0) {
        return ret;
      }
      offset += ret;
    }
  } // for (i)

  return offset;
}

uint32_t SDLState::body_len() const {
  if (!m_desc) {
    // XXX programmer error
    return 0;
  }
  UruString name(m_desc->name());
  uint32_t total = name.send_len(true, false, false);
  total += 2; // version
  total += recursive_len();
  return total;
}

uint32_t SDLState::recursive_len() const {
  uint32_t total = 5; // flag, 0x06, num vars, num structs

  uint32_t num_vars = 0;
  for (uint32_t i = 0; i < m_vars.size(); i++) {
    Variable *v = m_vars[i];
    if (v && (!(v->m_flags & SameAsDefault) || (v->m_flags & HasTimeStamp))) {
      num_vars++;
    }
  }
  bool has_indices = (num_vars < m_desc->vars().size());
  for (uint32_t i = 0; i < m_vars.size(); i++) {
    Variable *v = m_vars[i];
    if (!v || ((v->m_flags & SameAsDefault) && !(v->m_flags & HasTimeStamp))) {
      continue;
    }
    if (has_indices) {
      total += 1;
    }
    total += 5;
    if (v->m_flags & HasTimeStamp) {
      total += 8;
    }
    if (v->m_flags & SameAsDefault) {
      continue;
    }

    // here we have the data
    SDLDesc::Variable *d_var = m_desc->vars()[v->m_index];
    if (d_var->m_count == 0) {
      total += 4;
    }
    switch (d_var->m_type) {
    case SDLDesc::Variable::Int:
    case SDLDesc::Variable::Float:
      total += (v->m_count * 4);
      break;
    case SDLDesc::Variable::Bool:
    case SDLDesc::Variable::Byte:
      total += v->m_count;
      break;
    case SDLDesc::Variable::String32:
      total += (v->m_count * 32);
      break;
    case SDLDesc::Variable::Key:
      for (uint32_t j = 0; j < v->m_count; j++) {
        PlKey *key = &(v->m_value[j].v_plkey);
        total += key->send_len();
      }
      break;
    case SDLDesc::Variable::Creatable:
      // writing these values is not supported
      total += 4;
      break;
    case SDLDesc::Variable::Time:
    case SDLDesc::Variable::AgeTimeOfDay:
      total += (v->m_count * 8);
      break;
    case SDLDesc::Variable::Short:
      total += (v->m_count * 2);
      break;
    case SDLDesc::Variable::Vector3:
    case SDLDesc::Variable::Point3:
      total += (v->m_count * 12);
      break;
    case SDLDesc::Variable::Quaternion:
      total += (v->m_count * 16);
      break;
    case SDLDesc::Variable::RGB8:
      total += (v->m_count * 3);
      break;
    default:
      // can't happen
      break;
    }
  } // for (i)

    // now we have structs
  num_vars = 0;
  for (uint32_t i = 0; i < m_structs.size(); i++) {
    Struct *s = m_structs[i];
    if (s && (!(s->m_flags & SameAsDefault) || (s->m_flags & HasTimeStamp))) {
      num_vars++;
    }
  }
  has_indices = (num_vars < m_desc->structs().size());
  for (uint32_t i = 0; i < m_structs.size(); i++) {
    Struct *s = m_structs[i];
    if (!s || ((s->m_flags & SameAsDefault) && !(s->m_flags & HasTimeStamp))) {
      continue;
    }
    if (has_indices) {
      total += 1;
    }
    total += 5;
    if (s->m_flags & HasTimeStamp) {
      total += 8;
    }
    if (s->m_flags & SameAsDefault) {
      continue;
    }

    // here we have the data
    SDLDesc::Struct *d_struct = m_desc->structs()[s->m_index];
    if (d_struct->m_count == 0) {
      total += 4;
    }
    // XXX unknown "Sub SDL record lead"
    total += 1;
    for (uint32_t j = 0; j < s->m_count; j++) {
      // recurse
      total += s->m_child[j].recursive_len();
    }
  } // for (i)

  return total;
}

void SDLState::invent_age_key(uint32_t pageid) {
  m_key.m_locsequencenumber = pageid;
  m_key.m_locflags = 0x0008;
  m_key.m_classtype = plSceneObject;
  m_key.m_objectid = 1;
  m_key.m_name = new UruString("AgeSDLHook", false);
}

void SDLState::expand() {
  if (!m_desc) {
    // XXX programmer error
    return;
  }
  /**
   * Expand the state "value" vectors to match the SDLDesc specification
   * All NULL vars are populated with new Variable()s.
   *   (since HasDirtyFlag is not set these won't get propogated)
   *
   */
  if (m_vars.size() < m_desc->vars().size()) {
    int32_t cur = m_vars.size() - 1;
    m_vars.resize(m_desc->vars().size(), NULL);
    int32_t maxfill = m_desc->vars().size() - 1;
    int32_t fill = maxfill;
    while (fill >= 0) {
      while (cur >= 0 && m_vars[cur] == NULL)
        cur--;
      if (cur >= 0) {
        if (m_vars[cur]->m_index == (uint32_t) fill) {
          if (fill != cur) { // In the wrong place ?!?
            m_vars[fill] = m_vars[cur];
            m_vars[cur] = NULL;
          }
          cur--;
        }
      }
      if (m_vars[fill] == NULL) {
        m_vars[fill] = new Variable(fill, m_desc->vars()[fill]->m_type);
      }
      fill--;
    }
    while (cur >= 0) {
      if (m_vars[cur]) {
        fill = m_vars[cur]->m_index;
        if (fill != cur && fill <= maxfill && m_vars[fill] != NULL) { // In the wrong place ?!?
          m_vars[fill] = m_vars[cur];
          m_vars[cur] = NULL;
        }
      }
      cur--;
    }
  }
  if (m_structs.size() < m_desc->structs().size()) {
    int32_t cur = m_structs.size() - 1;
    m_structs.resize(m_desc->structs().size(), NULL);
    int32_t fill = m_desc->structs().size() - 1;
    int32_t maxfill = fill;
    while (fill >= 0) {
      while (cur >= 0 && m_structs[cur] == NULL)
        cur--;
      if (cur >= 0) {
        if (m_structs[cur]->m_index == (uint32_t)fill) {
          if (fill != cur) {
            m_structs[fill] = m_structs[cur];
            m_structs[cur] = NULL;
          }
        }
        cur--;
      }
      if (m_structs[fill] == NULL) {
        m_structs[fill] = new Struct(fill, m_desc->structs()[fill]->m_data);
      }
      fill--;
    }
    while (cur >= 0) {
      if (m_structs[cur]) {
        fill = m_structs[cur]->m_index;
        if (fill != cur && fill <= maxfill && m_structs[fill] != NULL) { // In the wrong place ?!?
          m_structs[fill] = m_structs[cur];
          m_structs[cur] = NULL;
        }
      }
      cur--;
    }
  }
}

bool SDLState::save_file(std::ofstream &file, std::list<SDLState*> &save) {
  uint32_t buflen = 4096;
  uint8_t *buf = new uint8_t[buflen];

  std::list<SDLState*>::iterator iter;
  for (iter = save.begin(); iter != save.end(); iter++) {
    SDLState *s = *iter;
    // m_flag & Volatile is not sufficient for determining whether an object should
    // be discarded from the persistent state: there are objects without this
    // flag set which should not be persistent, the KI light being the big
    // one. So if the plKey has a client ID, it's avatar-related SDL, or a
    // clone, so don't save it either.
    // note this also discards object clones -- if that is changed, the
    // Creatable handling must be updated so that when reading a file a flag
    // is passed in saying whether it is okay to allow a Creatable (it must
    // *only* be allowed reading in the state file)
    if ((s->m_flag & Volatile) || (s->key().m_contents & HasUoid)) {
      continue;
    }

    s->m_saving_to_file = true;
    uint32_t len = s->send_len();
    if (buflen < len + 4) {
      // XXX make sure this number isn't ridiculously large
      delete[] buf;
      buflen = len + 4;
      buf = new uint8_t[buflen];
    }
    // we don't compress in the save file
    int32_t wrote = s->write_msg(buf + 4, buflen - 4, true);
    s->m_saving_to_file = false;
    if (wrote > 0) {
      write32(buf, 0, wrote);
      // write to file
      file.write((char*) buf, wrote + 4);
      if (file.bad()) {
        // XXX need to log or something
        delete[] buf;
        return false;
      }
    } else {
      // XXX something went wrong
      delete[] buf;
      return false;
    }
  }
  delete[] buf;
  return true;
}

bool SDLState::load_file(std::ifstream &file, std::list<SDLState*> &load, std::list<SDLDesc*> &descs, Logger *log) {
  uint32_t buflen = 4096;
  uint8_t buf[buflen];
  uint32_t offset, fill = 0;

//  uint32_t total_read = 0;

  while (file.good()) {
    if (fill < buflen) {
      file.read((char*) buf + fill, buflen - fill);
      fill += file.gcount();
//      total_read += file.gcount();
    }
    offset = 0;
    while (offset < fill) {
      SDLState *s = NULL;
      if (fill < offset + 4) {
        if (offset != 0 && fill < buflen) {
          // read more
          break;
        } else {
          // there is no more to read, so we're done
          return false;
        }
      }
      uint32_t len = read32(buf, offset);
      if (fill < offset + 4 + len) {
        if (len < buflen) {
          // read more
          break;
        } else {
          // we can't fit it all in the buffer
          return false;
        }
      }
      offset += 4;
      s = new SDLState();
      try {
        int32_t read_size = s->read_msg(buf + offset, len, descs);
        if (read_size < 0) {
          log_err(log, "Unknown SDL found (read_size %d)\n", read_size);
          delete s;
          if ((uint32_t) (-read_size) != len) {
            log_err(log, "SDL length mismatch, claimed %u, read %d\n", len, (-read_size));
          }
        } else {
          // drop any duplicates that might have snuck in
          const char *new_name = s->get_desc()->name();
          std::list<SDLState*>::iterator iter;
          for (iter = load.begin(); iter != load.end(); iter++) {
            SDLState *other = *iter;
            if (s->key() == other->key() && other->name_equals(new_name)) {
              log_warn(log, "Found duplicate SDL while loading file!\n");
              break;
            }
          }
          if (iter != load.end()) {
            delete s;
          } else {
            s->expand();
            load.push_back(s);
          }
          if ((uint32_t) read_size != len) {
            log_err(log, "SDL length mismatch, claimed %u, read %d\n", len, read_size);
          }
        }
      } catch (const truncated_message &e) {
        log_err(log, "SDL truncated - %s\n", e.what());
        delete s;
        // this is not supposed to happen, as the lengths are checked up
        // front; if it does happen we certainly can't go on (the file is
        // truly truncated, or misformatted)
        return false;
      } catch (const parse_error &e) {
        log_err(log, "SDL parse error: %s\n", e.what());
        delete s;
      }
      offset += len;
    }
    if (fill >= offset) {
      fill -= offset;
    } else {
      // we should already have logged "SDL length mismatch"
      fill = 0;
    }
    if (offset > 0 && fill > 0) {
      memcpy(buf, buf + offset, fill);
    }
  }

  return true;
}

void SDLState::update_from(SDLState *newer, bool vault, bool global, bool age_load) {

  if (this == newer) {
    // programmer error, but no harm done
    return;
  }
  if (!m_desc) {
    // XXX programmer error
    return;
  }
  struct timeval now;
  gettimeofday(&now, NULL);
  if (m_vars.size() < m_desc->vars().size()
      || m_structs.size() < m_desc->structs().size()) {
    // XXX programmer error
    expand();
  }
  if (age_load) {
    if (!m_key.m_name) {
      m_key = newer->m_key;
      if (newer->m_key.m_name) {
        m_key.m_name = new UruString(*newer->m_key.m_name, true);
      }
    }
  } else if (vault) {
    // the SDL will be forwarded to clients, potentially; make sure the key
    // is set properly
    if (!newer->m_key.m_name) {
      newer->m_key = m_key;
      if (m_key.m_name) {
        newer->m_key.m_name = new UruString(*m_key.m_name, true);
      }
    }
  }
  for (uint32_t i = 0; i < newer->m_vars.size(); i++) {
    Variable *from = newer->m_vars[i];
    Variable *to = m_vars[from->m_index];
    if (!(from->m_flags & HasDirtyFlag)) {
      // don't update the value unless it has this flag set
      if (vault) {
        // don't forward the value to clients
        delete from;
        newer->m_vars[i] = NULL;
      }
      continue;
    }
    if (to) {
      if (vault &&
      // if the newer message has an older timestamp than the current
      // one, discard the newer value (handles global SDL)
          ((global
              && (to->m_flags & HasTimeStamp)
              && (!(from->m_flags & HasTimeStamp)
                  || timeval_lessthan(from->m_ts, to->m_ts)))
          // if we got a player-vault update and the data is the same, don't
          // update the timestamp or forward
              || (!global && *to == *from))) {
        delete from;
        newer->m_vars[i] = NULL;
        continue;
      }
      // if it's age load, ignore values with no timestamp (since they
      // are from the loaded file and all server-timestamped, this is
      // correct) but make sure what we keep does have a timestamp
      if (age_load
          && !(from->m_flags & HasTimeStamp)) {
        if (!(to->m_flags & SameAsDefault) && !(to->m_flags & HasTimeStamp)) {
          // setting this timestamp means that effectively, vault SDL will
          // override global SDL, but only at the time of first link (after
          // that there will be a timestamp in the age SDL saved state)
          to->m_flags |= HasTimeStamp;
          to->m_ts = now;
        }
        continue;
      }
      // if it's age load, and to has a timestamp, that means there is
      // a timestamp stored in the vault, so use it (note: we already
      // continued above if from does not have a timestamp)
      if (age_load
          && (to->m_flags & HasTimeStamp)
          && timeval_lessthan(from->m_ts, to->m_ts)) {
        continue;
      }
    }
    if (!vault) {
      // if it's not coming from the vault, we can swipe the data in
      // the newer state
      if (to) {
        delete to;
      }
      m_vars[from->m_index] = from;
      newer->m_vars[i] = NULL;
      // make sure there's a timestamp; if it's age load we definitely
      // don't want to modify the timestamps!
      if (!age_load || !(from->m_flags & HasTimeStamp)) {
        from->m_flags |= HasTimeStamp;
        from->m_ts = now;
      }
    } else {
      // if it's from the vault, we have to be more careful: we need to leave
      // the data in the newer state object, so we have to copy it to this one
      if (!to) {
        to = new Variable(from->m_index, from->m_type);
      }
      *to = *from;
      // make sure there's a timestamp if there wasn't one already
      if (!age_load || !(to->m_flags & HasTimeStamp)) {
        to->m_flags |= HasTimeStamp;
        to->m_ts = now;
      }
    }
  }
  // the struct code is the same, except that there really shouldn't be
  // structs in any vault sdl... but if there are, we obey the timestamps
  // but don't try to do equality tests and just copy/forward not-global vault
  // SDL
  for (uint32_t i = 0; i < newer->m_structs.size(); i++) {
    Struct *from = newer->m_structs[i];
    Struct *to = m_structs[from->m_index];
    // always replace structs regardless of "Dirty" flag, which appears to
    // be unused for structs, and there shouldn't be any in vault SDL
    if (to) {
      if (vault
          && ((global
              && (to->m_flags & HasTimeStamp)
              && (!(from->m_flags & HasTimeStamp)|| timeval_lessthan(from->m_ts, to->m_ts))))) {
        delete from;
        newer->m_structs[i] = NULL;
        continue;
      }
    }
    if (!vault) {
      if (to) {
        delete to;
      }
      m_structs[from->m_index] = from;
      newer->m_structs[i] = NULL;
      // do not include a timestamp if it wasn't there already -- this breaks
      // avatar SDL, and so if there ever is a timestamp, it was put there by
      // a client
    } else {
      if (!to) {
        to = new Struct(from->m_index, from->m_desc);
      }
      *to = *from;
    }
  }
}

bool SDLState::name_equals(const char *name) {
  const char *sdl_name = m_desc->name();
  return (strlen(sdl_name) == strlen(name) && !strcasecmp(sdl_name, name));
}

bool SDLState::is_avatar_sdl() const {
  const char *sdl_name = m_desc->name();
  uint32_t name_len = strlen(sdl_name);
  return ((name_len == 5 && !strcasecmp(sdl_name, "Layer"))
          || (name_len == 6 && !strcasecmp(sdl_name, "avatar"))
          || (name_len == 8 && (!strcasecmp(sdl_name, "clothing") || !strcasecmp(sdl_name, "AGMaster")))
          || (name_len == 12 && !strcasecmp(sdl_name, "CloneMessage"))
          || (name_len == 13 && !strcasecmp(sdl_name, "MorphSequence"))
          || (name_len == 14 && !strcasecmp(sdl_name, "avatarPhysical")));
}

char* SDLState::sdl_flag_c_str_alloc(uint32_t t) {
  std::list<std::string> elements;
  std::string e;
  std::ostringstream out;

  // Assemble a string of <flag1[|flag2...]>

  elements.clear();

  if (t & HasUoid)              elements.push_back("HasUoid");
  if (t & HasNotificationInfo)  elements.push_back("HasNotificationInfo");
  if (t & HasTimeStamp)         elements.push_back("HasTimeStamp");
  if (t & SameAsDefault)        elements.push_back("SameAsDefault");
  if (t & HasDirtyFlag)         elements.push_back("HasDirtyFlag");
  if (t & WantTimeStamp)        elements.push_back("WantTimeStamp");
  if (t & AddedVarLengthIO)     elements.push_back("AddedVarLengthIO");

  if (!elements.empty()) {
    e = elements.back();
    elements.pop_back();
  }
  std::copy(elements.begin(), elements.end(), std::ostream_iterator<std::string>(out, "|"));
  out << e;

  // caller must free return pointer
  return strdup(out.str().c_str());
}

std::string SDLState::Variable::str() {
  std::ostringstream out;
  char *s;

  out << "<" << SDLDesc::Variable::vartype_c_str(m_type) << ">";
  if (m_count > 1)
    out << "[";
  for (int32_t i = 0; i < m_count; i++) {
    out << SDLDesc::Variable::str(m_type, m_value[i]);
    if (i > 0)
      out << ",";
  }
  if (m_count > 1)
    out << "]";
  s = (char*) sdl_flag_c_str_alloc(m_flags);
  out << "<" << s << ">";
  free(s);

  m_string = out.str();
  return m_string;
}

const char* SDLState::Variable::c_str() {
  m_chars = str().c_str();
  return m_chars;
}

std::string SDLState::str(const char *sep) {
  std::ostringstream out;
  std::vector<SDLState::Variable*>::const_iterator vi;
  std::vector<SDLState::Struct*>::const_iterator si;
  std::list<std::string> elements;
  std::string str;
  std::string e;
  char *c;
  char keybuf[64];

  out << "{SDLState: " << m_desc->name() << "-v" << m_desc->version();
  c = m_key.c_str(keybuf, sizeof(keybuf));
  if (c) {
    out << " " << c;
  }
  out << sep;

  elements.clear();
  for (vi = m_vars.begin(); vi != m_vars.end(); vi++) {
    if (*vi) {
      SDLState::Variable *v = *vi;
      SDLDesc::Variable *d = m_desc->vars()[v->m_index];

      str = d->str();
      str += ": ";

      if (v->m_count > 1)
        str += "[ ";

      for (uint32_t i = 0; i < v->m_count; i++) {
        if (v->m_flags & SDLState::SameAsDefault) {
          str += SDLDesc::Variable::str(d->m_type, d->m_default);
        } else {
          str += SDLDesc::Variable::str(d->m_type, v->m_value[i]);
        }
        if (i < v->m_count - 1) {
          str += " ";
        }
      }

      if (v->m_count > 1)
        str += " ]";

      if (v->m_flags & SDLState::HasTimeStamp) {
        char buf[64];
        char timebuf[32];
        struct tm *ts = localtime(&v->m_ts.tv_sec);
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", ts);
        snprintf(buf, sizeof(buf), "%s %lu.%06lu",timebuf, v->m_ts.tv_sec, v->m_ts.tv_usec);
        str += " [" + std::string(buf) + "]";
      }

      str += "  <";
      c = SDLState::sdl_flag_c_str_alloc(v->m_flags);
      if (c) {
        str += c;
        free(c);
      }
      str += ">";

      elements.push_back(str);
    }
  }
  for (si = m_structs.begin(); si != m_structs.end(); si++) {
    if (*si) {
      SDLState::Struct *s = *si;
      SDLDesc::Struct *d = m_desc->structs()[s->m_index];
      elements.push_back(d->str(sep));
    }
  }
  if (!elements.empty()) {
    e = elements.back();
    elements.pop_back();
  }
  std::copy(elements.begin(), elements.end(), std::ostream_iterator<std::string>(out, sep));
  if (!e.empty())
    out << e;
  out << "}";

  m_string = out.str();
  return m_string;
}
const char* SDLState::c_str(const char *sep) {
  m_chars = str(sep).c_str();
  // returning c_str() is OK because we keep ref in m_chars for destructor
  return m_chars;
}

bool SDLState::Variable::operator==(const SDLState::Variable &other) {
  // these are expected to be true before operator== is used, but be safe
  if (m_type != other.m_type
      || m_index != other.m_index) {
    return false;
  }
  // make sure to exclude the timestamp, the question is, are the *values*
  // equal?
  if ((m_flags & ~HasTimeStamp) != (other.m_flags & ~HasTimeStamp)) {
    return false;
  }
  if (m_count != other.m_count) {
    return false;
  }
  if (m_type == SDLDesc::Variable::Key) {
    for (uint32_t j = 0; j < m_count; j++) {
      if (m_value[j].v_plkey != other.m_value[j].v_plkey) {
        return false;
      }
    }
  } else if (m_type == SDLDesc::Variable::Creatable) {
    if (!m_value[0].v_creatable && !other.m_value[0].v_creatable) {
    } else if (!m_value[0].v_creatable || !other.m_value[0].v_creatable) {
      return false;
    } else {
      uint32_t len = read32(m_value[0].v_creatable, 0);
      if (read32(other.m_value[0].v_creatable, 0) != len) {
        return false;
      }
      if (memcmp(m_value[0].v_creatable, other.m_value[0].v_creatable, len + 5)) {
        return false;
      }
    }
  } else if (m_type == SDLDesc::Variable::String32) {
    for (uint32_t j = 0; j < m_count; j++) {
      if (memcmp(m_value[j].v_string, other.m_value[j].v_string, 32)) {
        return false;
      }
    }
  } else if (m_type == SDLDesc::Variable::Int) {
    for (uint32_t j = 0; j < m_count; j++) {
      if (m_value[j].v_int != other.m_value[j].v_int) {
        return false;
      }
    }
  } else if (m_type == SDLDesc::Variable::Float) {
    for (uint32_t j = 0; j < m_count; j++) {
      if (m_value[j].v_float != other.m_value[j].v_float) {
        return false;
      }
    }
  } else if (m_type == SDLDesc::Variable::Bool) {
    for (uint32_t j = 0; j < m_count; j++) {
      if (m_value[j].v_bool != other.m_value[j].v_bool) {
        return false;
      }
    }
  } else if (m_type == SDLDesc::Variable::Bool) {
    for (uint32_t j = 0; j < m_count; j++) {
      if (m_value[j].v_bool != other.m_value[j].v_bool) {
        return false;
      }
    }
  } else if (m_type == SDLDesc::Variable::Time) {
    for (uint32_t j = 0; j < m_count; j++) {
      if ((m_value[j].v_time.tv_sec != other.m_value[j].v_time.tv_sec)
          || (m_value[j].v_time.tv_usec != other.m_value[j].v_time.tv_usec)) {
        return false;
      }
    }
  } else if (m_type == SDLDesc::Variable::Byte) {
    for (uint32_t j = 0; j < m_count; j++) {
      if (m_value[j].v_byte != other.m_value[j].v_byte) {
        return false;
      }
    }
  } else if (m_type == SDLDesc::Variable::Short) {
    for (uint32_t j = 0; j < m_count; j++) {
      if (m_value[j].v_short != other.m_value[j].v_short) {
        return false;
      }
    }
  } else if (m_type == SDLDesc::Variable::AgeTimeOfDay) {
    for (uint32_t j = 0; j < m_count; j++) {
      if ((m_value[j].v_time.tv_sec != other.m_value[j].v_time.tv_sec)
          || (m_value[j].v_time.tv_usec != other.m_value[j].v_time.tv_usec)) {
        return false;
      }
    }
  } else if (m_type == SDLDesc::Variable::Vector3 || m_type == SDLDesc::Variable::Point3) {
    for (uint32_t j = 0; j < m_count; j++) {
      float *here, *there;
      if (m_type == SDLDesc::Variable::Vector3) {
        here = m_value[j].v_vector3;
        there = other.m_value[j].v_vector3;
      } else {
        here = m_value[j].v_point3;
        there = other.m_value[j].v_point3;
      }
      if (memcmp(here, there, 3 * sizeof(float))) {
        return false;
      }
    }
  } else if (m_type == SDLDesc::Variable::Quaternion) {
    for (uint32_t j = 0; j < m_count; j++) {
      if (memcmp(m_value[j].v_quaternion, other.m_value[j].v_quaternion, 4 * sizeof(float))) {
        return false;
      }
    }
  } else if (m_type == SDLDesc::Variable::RGB8) {
    for (uint32_t j = 0; j < m_count; j++) {
      if (memcmp(m_value[j].v_rgb8, other.m_value[j].v_rgb8, 3)) {
        return false;
      }
    }
  } else {
    // unknown type (this is bad)
    return false;
  }
  return true;
}

SDLState::Variable::~Variable() {
  if (m_type == SDLDesc::Variable::Key) {
    for (uint32_t j = 0; j < m_count; j++) {
      m_value[j].v_plkey.delete_name();
    }
  }
  if (m_type == SDLDesc::Variable::Creatable) {
    if (m_value[0].v_creatable) {
      delete[] m_value[0].v_creatable;
      m_value[0].v_creatable = NULL;
    }
  }
  delete[] m_value;
}

SDLState::Variable&
SDLState::Variable::operator=(const SDLState::Variable &other) {

  if (this != &other) {
    if ((!(other.m_flags & SameAsDefault)
         || (m_count < other.m_count))
        && (!(m_flags & SameAsDefault))) {
      delete[] m_value;
      m_value = NULL;
    }
    if (!(other.m_flags & SameAsDefault) && !m_value) {
      m_value = new SDLDesc::Variable::data_t[other.m_count];
    }
    m_index = other.m_index;
    m_flags = other.m_flags;
    m_ts = other.m_ts;
    m_count = other.m_count;
    if (!(m_flags & SameAsDefault)) {
      memcpy(m_value, other.m_value, sizeof(SDLDesc::Variable::data_t) * m_count);
    }
    m_type = other.m_type;
    if (m_type == SDLDesc::Variable::Key) {
      for (uint32_t j = 0; j < m_count; j++) {
        // the pointer was copied, make a new string
        m_value[j].v_plkey.m_name = new UruString(*other.m_value[j].v_plkey.m_name, true);
      }
    }
    if (m_type == SDLDesc::Variable::Creatable) {
      // the pointer was copied, make a new buffer
      uint32_t len = read32(other.m_value[0].v_creatable, 0);
      m_value[0].v_creatable = new uint8_t[len + 5];
      memcpy(m_value[0].v_creatable, other.m_value[0].v_creatable, len + 5);
    }
  }
  return *this;
}

SDLState::Struct&
SDLState::Struct::operator=(const SDLState::Struct &other) {

  if (this != &other) {
    /* XXX this is the invariant
     assert(m_cloneid == other.m_cloneid);
     if (!(m_contents & Default)) {
     assert(m_desc == other.m_desc);
     }
     */
    m_index = other.m_index;
    m_flags = other.m_flags;
    m_ts = other.m_ts;
    m_count = other.m_count;
    m_desc = other.m_desc;
    if (!(m_flags & SameAsDefault)) {
      // deep copy
      if (m_child) {
        delete m_child;
      }
      m_child = new SDLState(other.m_child->get_desc());
      m_child->update_from(other.m_child);
    }
  }
  return *this;
}

AgeDesc::Page::Page(const char *name) :
    m_name(NULL), m_owned(false), m_owner(0), m_conditional_load(0), m_pagenum(0) {
  m_name = strdup(name); // XXX check return value
}

AgeDesc::~AgeDesc() {
  std::vector<Page*>::iterator pi;
  for (pi = m_pages.begin(); pi != m_pages.end(); pi++) {
    delete *pi;
  }
}

AgeDesc* AgeDesc::parse_file(std::ifstream &file) {
  std::string line, token;
  uint32_t lineno = 0;
  AgeDesc *age = new AgeDesc();

  while (!file.eof()) {
    lineno++;
    std::getline(file, line);
    if (file.fail() && !file.eof()) {
      delete age;
      throw parse_error(lineno, "error reading file");
    }
    size_t len = line.size();
    if (len > 0 && line[len - 1] == '\r') {
      line.resize(len - 1);
    }
    std::stringstream ss(line);
    std::getline(ss, token, '=');
    if (ss.eof()) {
      if (token.size() > 0) {
        delete age;
        throw parse_error(lineno, "no = found");
      } else {
        // empty line, or newline at end of file
        continue;
      }
    }
    if (token == "DayLength") {
      std::getline(ss, token);
      std::stringstream num(token);
      num >> age->m_daylen;
      if (num.fail()) {
        delete age;
        throw parse_error(lineno, std::string("could not parse '") + token + "' as a float");
      }
    } else if (token == "Page") {
      std::string pagepart;
      std::getline(ss, pagepart, ',');
      if (ss.eof()) {
        delete age;
        throw parse_error(lineno, "Page listing incomplete (tuple expected)");
      }
      Page *newpage = new Page(pagepart.c_str());
      std::getline(ss, pagepart, ',');
      std::stringstream num(pagepart);
      num >> newpage->m_pagenum;
      if (num.fail()) {
        delete newpage;
        delete age;
        throw parse_error(lineno, std::string("could not parse '") + num.str() + "' as a number");
      }
      std::getline(ss, pagepart, ',');
      if (ss.fail()) {
        // only one number present...
        newpage->m_conditional_load = 0;
      } else {
        std::stringstream num2(pagepart);
        num2 >> newpage->m_conditional_load;
        if (num2.fail()) {
          delete newpage;
          delete age;
          throw parse_error(lineno, std::string("could not parse '") + num2.str() + "' as a number");
        }
      }
      age->m_pages.push_back(newpage);
    } else {
      std::string rhs;
      std::getline(ss, rhs);
      std::stringstream num(rhs);
      if (token == "StartDateTime") {
        num >> age->m_start_date_time;
      } else if (token == "MaxCapacity") {
        num >> age->m_capacity;
      } else if (token == "LingerTime") {
        num >> age->m_linger;
      } else if (token == "SequencePrefix") {
        num >> age->m_seq_prefix;
      } else if (token == "ReleaseVersion") {
        num >> age->m_release;
      } else {
        delete age;
        throw parse_error(lineno, std::string("unrecognized field '") + token + "'");
      }
      if (num.fail()) {
        delete age;
        throw parse_error(lineno, std::string("could not parse '") + rhs + "' as a number");
      }
    }
  }

  return age;
}

const char* AgeDesc::c_str() {
  return "";
}
std::string AgeDesc::str() {
  return std::string("");
}
