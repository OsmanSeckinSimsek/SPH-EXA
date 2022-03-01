#pragma once

#ifdef USE_MPI
#include "mpi.h"
#endif

#include "arg_parser.hpp"
#include "timer.hpp"
#include "file_utils.hpp"
#include "printer.hpp"
#include "utils.hpp"

#if defined(USE_CUDA)
#include "sph/cuda/sph.cuh"
#endif

#include "sph/density.hpp"
#include "sph/iad.hpp"
#include "sph/grad_p.hpp"
#include "sph/kernels.hpp"
#include "sph/eos.hpp"
#include "sph/timestep.hpp"
#include "sph/positions.hpp"
#include "sph/total_energy.hpp"
#include "sph/update_h.hpp"
