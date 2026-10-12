//
// shaders.cpp
//
// See shaders.h.
//
// This file is part of BMA800XL, a bare metal Atari 800XL emulator for the
// Raspberry Pi. See LICENSE for licensing details.
//

#include "shaders.h"

#include "shader_copy.h"
#include "shader_crt.h"
#include "shader_crt_curve.h"
#include "shader_crt_curve_smooth.h"
#include "shader_crt_smooth.h"

namespace v3d {

const Program &CopyProgram = copy;
const Program &CrtProgram = crt;
const Program &CrtSmoothProgram = crt_smooth;
const Program &CrtCurveProgram = crt_curve;
const Program &CrtCurveSmoothProgram = crt_curve_smooth;

} // namespace v3d
