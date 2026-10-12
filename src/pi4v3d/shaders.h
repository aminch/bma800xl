//
// shaders.h
//
// The compiled GPU shader programs (tools/v3dshader): the generated
// shader_*.h headers, each included once, in shaders.cpp.
//
// This file is part of BMA800XL, a bare metal Atari 800XL emulator for the
// Raspberry Pi. See LICENSE for licensing details.
//

#ifndef BMA800XL_PI4V3D_SHADERS_H_
#define BMA800XL_PI4V3D_SHADERS_H_

#include "shader.h"

namespace v3d {

extern const Program &CopyProgram;            // copy.frag
extern const Program &CrtProgram;             // crt.frag
extern const Program &CrtSmoothProgram;       // with SMOOTH
extern const Program &CrtCurveProgram;        // with CURVATURE
extern const Program &CrtCurveSmoothProgram;  // with both

} // namespace v3d

#endif
