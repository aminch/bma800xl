//
// options.h
//
// Reads key=value options from cmdline.txt on the SD card.
//
// This file is part of BMA800XL, a bare metal Atari 800XL emulator for the
// Raspberry Pi. See LICENSE for licensing details.
//

#ifndef BMA800XL_OPTIONS_H_
#define BMA800XL_OPTIONS_H_

#include <circle/bcmpropertytags.h>

#define OPTIONS_MAX 32

class Options {
public:
  Options(void);

  // Value of option NAME, or DEFAULT_VALUE if it was not given.
  const char *Get(const char *name, const char *default_value = 0) const;
  int GetInt(const char *name, int default_value) const;
  double GetDouble(const char *name, double default_value) const;
  bool GetBool(const char *name, bool default_value) const;

  static Options *Get(void);

private:
  TPropertyTagCommandLine m_TagCommandLine;
  int m_nCount;
  const char *m_pName[OPTIONS_MAX];
  const char *m_pValue[OPTIONS_MAX];

  static Options *s_pThis;
};

#endif
