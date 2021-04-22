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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <stdarg.h>
#include <pthread.h>

#include <sys/time.h>

#include <stdexcept>

#include "machine_arch.h"

#include "Logger.h"

static pthread_mutex_t creation_mutex;
void Logger::init() {
  if (pthread_mutex_init(&creation_mutex, NULL)) {
    throw std::bad_alloc();
  }
}
void Logger::shutdown() {
  pthread_mutex_destroy(&creation_mutex);
}

Logger::Logger(const char *system, const char *filename, level_t level) :
    m_log_prefix(NULL), m_log_level(level), m_refct(NULL) {
  setup_logger(system, filename);
}

void Logger::setup_logger(const char *system, const char *filename) {
  m_refct = new uint32_t[1];
  *m_refct = 1;
  m_logf = fopen(filename, "a+");
  m_mutex = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
  if (!m_mutex || pthread_mutex_init(m_mutex, NULL)) {
    // XXX uh-oh
    if (m_logf) {
      fclose(m_logf);
      m_logf = NULL;
    }
  }
  int32_t len = strlen(system) + sizeof(" DEBUG : ") + 17 + 1;
  m_log_prefix = new char[len];
  if (m_log_prefix) {
    snprintf(m_log_prefix, len, "timestamp0.123456 DEBUG %s: ", system);
  }
}

Logger::Logger(const char *system, Logger *to_share, level_t level) :
    m_log_prefix(NULL), m_log_level(level) {
  pthread_mutex_lock(&creation_mutex);
  m_refct = to_share->m_refct;
  (*m_refct)++;
  pthread_mutex_unlock(&creation_mutex);
  if (!*m_refct) {
    // this should really not happen
    setup_logger(system, "backup.log");
  } else {
    m_mutex = to_share->m_mutex;
    m_logf = to_share->m_logf;
  }
  int32_t len = strlen(system) + sizeof(" DEBUG : ") + 17;
  m_log_prefix = new char[len];
  if (m_log_prefix) {
    snprintf(m_log_prefix, len, "timestamp0.123456 DEBUG %s: ", system);
  }
}

Logger::~Logger() {
  pthread_mutex_lock(&creation_mutex);
  if (--(*m_refct) == 0) {
    delete[] m_refct;
    m_refct = NULL;
  }
  pthread_mutex_unlock(&creation_mutex);
  if (m_refct == NULL) {
    pthread_mutex_destroy(m_mutex);
    free(m_mutex);
    if (m_logf) {
      fflush(m_logf);
      fclose(m_logf);
    }
  }
  if (m_log_prefix) {
    delete[] m_log_prefix;
  }
}

Logger::level_t Logger::str_to_level(const char *name) {
  if (!name) {
    return NONE;
  }
  if (!strcasecmp(name, "msgs") || !strcasecmp(name, "log_msgs")) {
    return LOG_MSGS;
  }
  if (!strcasecmp(name, "debug") || !strcasecmp(name, "log_debug")) {
    return LOG_DEBUG;
  }
  if (!strcasecmp(name, "net") || !strcasecmp(name, "log_net")) {
    return LOG_NET;
  }
  if (!strcasecmp(name, "warn") || !strcasecmp(name, "log_warn")) {
    return LOG_WARN;
  }
  if (!strcasecmp(name, "info") || !strcasecmp(name, "log_info")) {
    return LOG_ERR;
  }
  if (!strcasecmp(name, "err") || !strcasecmp(name, "log_err")) {
    return LOG_ERR;
  }
  return NONE;
}

const char* Logger::level_c_str(Logger::level_t level) {
  switch (level) {
  case LOG_MSGS:    return "MSGS";
  case LOG_DEBUG:   return "DEBUG";
  case LOG_NET:     return "NET";
  case LOG_WARN:    return "WARN";
  case LOG_INFO:    return "INFO";
  case LOG_ERR:     return "ERR";
  default:          return "NONE";
  }
}

FILE* Logger::get_lock(Logger::level_t level) {
  if (level < m_log_level || m_logf == NULL) {
    return NULL;
  }
  pthread_mutex_lock(m_mutex);
  return m_logf;
}

const char* Logger::get_prefix(Logger::level_t level) {
  const char *lstr = "  UNK";
  switch (level) {
  case LOG_MSGS:    lstr = " MSGS";   break;
  case LOG_DEBUG:   lstr = "DEBUG";   break;
  case LOG_NET:     lstr = "  NET";   break;
  case LOG_WARN:    lstr = " WARN";   break;
  case LOG_INFO:    lstr = " INFO";   break;
  case LOG_ERR:     lstr = "  ERR";   break;
  default:          lstr = "  UNK";
  }
  if (!m_log_prefix) {
    // return a static string
    return lstr;
  }

  int32_t len = sizeof(" DEBUG") + 17;
  struct timeval t;
  gettimeofday(&t, NULL);
  len = snprintf(m_log_prefix, len, "%10ld.%06ld %s", t.tv_sec, t.tv_usec, lstr);
  m_log_prefix[len] = ' ';
  return m_log_prefix;
}

void Logger::release_lock() {
  pthread_mutex_unlock(m_mutex);
}

void Logger::dump_contents(level_t level, const uint8_t *buf, size_t len) {
  uint32_t i;

  if (len <= 0) {
    return;
  }
  FILE *outf = get_lock(level);
  if (outf) {
    fprintf(outf, "Buffer contents:\n");

    for (i = 0; i < len; i++) {
      if (i % 16 == 0) {
        fprintf(outf, "    ");
      }
      if (i % 16 == 8) {
        fprintf(outf, " ");
      }
      fprintf(outf, " %02x", buf[i]);
      if (i % 16 == 15) {
        fprintf(outf, "    ");
        for (uint32_t j = i - 15; j <= i; j++) {
          // print ASCII version
          if (j % 16 == 8) {
            fprintf(outf, " ");
          }
          if (buf[j] < 0x20 || buf[j] > 0x7e) {
            fprintf(outf, ".");
          } else {
            fprintf(outf, "%c", buf[j]);
          }
        }
        fprintf(outf, "\n");
      }
    }
    if (i % 16 != 0) {
      for (uint32_t j = 0; j < (16 - (i % 16)); j++) {
        fprintf(outf, "   ");
      }
      if (i % 16 < 8) {
        fprintf(outf, " ");
      }
      fprintf(outf, "    ");
      for (uint32_t j = i - (i % 16); j < i; j++) {
        if (j % 16 == 8) {
          fprintf(outf, " ");
        }
        if (buf[j] < 0x20 || buf[j] > 0x7e) {
          fprintf(outf, ".");
        } else {
          fprintf(outf, "%c", buf[j]);
        }
      }
      fprintf(outf, "\n");
    }
    fflush(outf);

    release_lock();
  }
}

#ifdef LOGGER_DEBUG
/*
 * If LOGGER_DEBUG is defined, move the definition of log_*() functions
 * from "inline" functions in Logger.h to standard functions here.
 */

/*
 * Note that these functions use a mutex to protect against concurrent
 * logging. The fmt argument should end in a newline unless you are planning
 * to use log_raw to append one later.
 */
void log_err(Logger *logger, const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  if (logger) {
    FILE *f = logger->get_lock(Logger::LOG_ERR);
    if (f) {
      fprintf(f, "%s", logger->get_prefix(Logger::LOG_ERR));
      vfprintf(f, fmt, ap);
      fflush(f);
      logger->release_lock();
    }
    else {
      vfprintf(stderr, fmt, ap);
      fflush(stderr);
    }
  }
  else {
    vfprintf(stderr, fmt, ap);
    fflush(stderr);
  }
  va_end(ap);
}

#ifdef __GNUC__
/**
 * This adds the ability to display a method name and source file line number
 * as a prefix to log messages.
 *
 *  GCC __PRETTY_FUNCTION__ is a synthetic variable (not macro definition!)
 *  that is created whenever it is referenced inside a function or method.
 *  
 *  It takes the format (type)[Class::]methodname(type, type...)
 *  
 *  Since types are not simple names, but may themselves be templates, classes,
 *  structs, etc, they can be quite lengthy. On the logger output I only wanted
 *  the name/line number as a prefix to help locate the source of a log message.
 *  So I need to remove all the type descriptions without pruning away any of the
 *  class and/or method names.
 *  
 *  The search for '(' and ')' locate the anchor points in the string.
 *  The final result of methodName() is a string like "Class::methodname()"
 *
 *  Example:
 *
 *    global scope function:
 *      __PRETTY_FUNCTION__ = "int32_t main()"
 *      methodName(__PRETTY_FUNCTION__) = "main()"
 *
 *    class method:
 *      __PRETTY_FUNCTION__ = "void Foo::show(std::__cxx11::string)"
 *      methodName(__PRETTY_FUNCTION__) = "Foo::show()"
 *  
 */

std::string methodName(const char *prettyFuncNameChars)
{
  std::string prettyFuncName(prettyFuncNameChars);

  size_t end = prettyFuncName.length() - 1;

  if (prettyFuncName[end] == ')') {
    uint32_t lvl = 1;
    while (lvl > 0 && end >= 0) {
      end -= 1;
      if (prettyFuncName[end] == ')')
        lvl += 1;
      else if (prettyFuncName[end] == '(')
        lvl -= 1;
    }
  }
  size_t begin = prettyFuncName.substr(0, end).rfind(" ") + 1;

  return prettyFuncName.substr(begin, end-begin) + "()";
}
#endif /* __GNUC__ */

void log_at(Logger::level_t level, Logger *logger,
       const char *fmt, ...) {
  va_list ap;

  if (logger) {
    va_start(ap, fmt);
    FILE *f = logger->get_lock(level);
    if (f) {
      fprintf(f, "%s", logger->get_prefix(level));
      vfprintf(f, fmt, ap);
      fflush(f);
      logger->release_lock();
    }
    va_end(ap);
  }
}

void log_at_where(Logger::level_t level, Logger *logger,
      std::string method, int32_t lineno, const char *fmt, ...) {
  va_list ap;

  if (logger) {
    va_start(ap, fmt);
    FILE *f = logger->get_lock(level);
    if (f) {
      fprintf(f, "%s%s#%d ", logger->get_prefix(level), method.c_str(), lineno);
      vfprintf(f, fmt, ap);
      fflush(f);
      logger->release_lock();
    }
    va_end(ap);
  }
}

void log_raw(Logger::level_t level, Logger *logger,
        const char *fmt, ...) {
  va_list ap;

  if (logger) {
    va_start(ap, fmt);
    FILE *f = logger->get_lock(level);
    if (f) {
      vfprintf(f, fmt, ap);
      fflush(f);
      logger->release_lock();
    }
    va_end(ap);
  }
}

#endif /* LOGGER_DEBUG */
