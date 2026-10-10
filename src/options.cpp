//
// options.cpp
//
// This file is part of BMA800XL, a bare metal Atari 800XL emulator for the
// Raspberry Pi. See LICENSE for licensing details.
//

#include "options.h"

#include <stdlib.h>
#include <string.h>

Options *Options::s_pThis = 0;

Options::Options(void) : m_nCount(0) {
  s_pThis = this;

  CBcmPropertyTags Tags;
  if (!Tags.GetTag(PROPTAG_GET_COMMAND_LINE, &m_TagCommandLine,
                   sizeof m_TagCommandLine)) {
    return;
  }
  if (m_TagCommandLine.Tag.nValueLength >= sizeof m_TagCommandLine.String) {
    return;
  }
  m_TagCommandLine.String[m_TagCommandLine.Tag.nValueLength] = '\0';

  // Split "a=1 b=2 c" in place into name/value pairs. The firmware adds its
  // own options (some without values), which are kept too and are harmless.
  char *p = (char *)m_TagCommandLine.String;
  while (*p != '\0' && m_nCount < OPTIONS_MAX) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
      p++;
    }
    if (*p == '\0') {
      break;
    }
    char *name = p;
    while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
      p++;
    }
    if (*p != '\0') {
      *p++ = '\0';
    }
    char *value = strchr(name, '=');
    if (value != 0) {
      *value++ = '\0';
    } else {
      value = name + strlen(name);
    }
    m_pName[m_nCount] = name;
    m_pValue[m_nCount] = value;
    m_nCount++;
  }
}

const char *Options::Get(const char *name, const char *default_value) const {
  // Later options win, so a user's value overrides an earlier default.
  for (int i = m_nCount - 1; i >= 0; i--) {
    if (strcmp(m_pName[i], name) == 0) {
      return m_pValue[i];
    }
  }
  return default_value;
}

int Options::GetInt(const char *name, int default_value) const {
  const char *value = Get(name);
  return value && *value ? atoi(value) : default_value;
}

double Options::GetDouble(const char *name, double default_value) const {
  const char *value = Get(name);
  return value && *value ? strtod(value, 0) : default_value;
}

bool Options::GetBool(const char *name, bool default_value) const {
  const char *value = Get(name);
  if (value == 0 || *value == '\0') {
    return default_value;
  }
  return strcmp(value, "1") == 0 || strcmp(value, "yes") == 0 ||
         strcmp(value, "true") == 0 || strcmp(value, "on") == 0;
}

Options *Options::Get(void) { return s_pThis; }
