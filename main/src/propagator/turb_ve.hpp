/*
 * MIT License
 *
 * Copyright (c) 2021 CSCS, ETH Zurich
 *               2021 University of Basel
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*! @file
 * @brief A Propagator class for modern SPH with generalized volume elements
 *
 * @author Sebastian Keller <sebastian.f.keller@gmail.com>
 * @author Jose A. Escartin <ja.escartin@gmail.com>
 */

#pragma once

#include <filesystem>
#include <variant>

#include "sph/sph.hpp"
#include "sph/hydro_turb/turbulence_data.hpp"

#include "io/mpi_file_utils.hpp"

#include "ipropagator.hpp"
#include "ve_hydro.hpp"
#include "gravity_wrapper.hpp"

namespace sphexa
{

using namespace sph;

//! @brief VE hydro propagator that adds turbulence stirring to the acceleration prior to position update
template<class DomainType, class ParticleDataType>
class TurbVeProp final : public HydroVeProp<DomainType, ParticleDataType>
{
    using Base = HydroVeProp<DomainType, ParticleDataType>;
    using Base::ng0_;
    using Base::ngmax_;
    using Base::rank_;
    using Base::timer;

    sph::TurbulenceData<typename ParticleDataType::RealType, typename ParticleDataType::AcceleratorType> turbulenceData;

public:
    TurbVeProp(size_t ngmax, size_t ng0, std::ostream& output, size_t rank)
        : Base(ngmax, ng0, output, rank)
        , turbulenceData(TurbulenceConstants(), rank == 0)
    {
    }

    void restoreState(const std::string& path, MPI_Comm comm) override
    {
#ifdef SPH_EXA_HAVE_H5PART
        // The file does not exist, we're starting from scratch. Nothing to do.
        if (!std::filesystem::exists(path)) { return; }

        H5PartFile* h5_file = nullptr;
        h5_file             = fileutils::openH5Part(path, H5PART_READ, comm);
        size_t numSteps     = H5PartGetNumSteps(h5_file);

        if (numSteps == 0) { throw std::runtime_error("Cannot initialize phases from empty file\n"); }

        size_t h5_step = numSteps - 1;
        H5PartSetStep(h5_file, h5_step);

        size_t iteration;
        H5PartReadStepAttrib(h5_file, "step", &iteration);
        std::string phaseAttribute = "phases_" + std::to_string(iteration);

        // look for matching phase data in the list of file attributes
        auto   fileAttributes = fileutils::fileAttributeNames(h5_file);
        size_t indexOfPhases =
            std::find(fileAttributes.begin(), fileAttributes.end(), phaseAttribute) - fileAttributes.begin();
        if (indexOfPhases == fileAttributes.size()) { throw std::runtime_error("No data found at " + phaseAttribute); }

        h5part_int64_t typeId, numPhases;
        char           dummy[256];
        H5PartGetFileAttribInfo(h5_file, indexOfPhases, dummy, 256, &typeId, &numPhases);

        if (numPhases != turbulenceData.phases.size())
        {
            throw std::runtime_error("Stored number of phases does not match initialized number of phases\n");
        }

        h5part_int64_t errors = H5PART_SUCCESS;
        errors |= H5PartReadFileAttrib(h5_file, phaseAttribute.c_str(), turbulenceData.phases.data());

        if (errors != H5PART_SUCCESS) { throw std::runtime_error("Could not read turbulence phases\n"); }

        std::cout << "Restored phases from SPH iteration " << iteration << std::endl;
        std::cout << "  first 5 phases: ";
        std::copy_n(turbulenceData.phases.begin(), 5, std::ostream_iterator<double>(std::cout, " "));
        std::cout << std::endl;

        H5PartCloseFile(h5_file);
#endif
    }

    void step(DomainType& domain, ParticleDataType& d) override
    {
        Base::computeForces(domain, d);

        size_t first = domain.startIndex();
        size_t last  = domain.endIndex();

        computeTimestep(d);
        timer.step("Timestep");
        driveTurbulence(first, last, d, turbulenceData);
        timer.step("Turbulence Stirring");

        transferToHost(d, first, last, {"ax", "ay", "az", "du"});
        computePositions(first, last, d, domain.box());
        timer.step("UpdateQuantities");
        updateSmoothingLength(first, last, d, ng0_);
        timer.step("UpdateSmoothingLength");

        timer.stop();
    }

    //! @brief save turbulence mode phases to file
    void dump(size_t iteration, const std::string& path) override
    {
#ifdef SPH_EXA_HAVE_H5PART
        // turbulence phases are identical on all ranks, only rank 0 needs to write data
        if (rank_ > 0) { return; }

        H5PartFile* h5_file       = nullptr;
        h5_file                   = H5PartOpenFile((path + ".h5").c_str(), H5PART_APPEND);
        std::string attributeName = "phases_" + std::to_string(iteration);

        const auto& phases = turbulenceData.phases;
        fileutils::sphexaWriteFileAttrib(h5_file, attributeName.c_str(), phases.data(), phases.size());
        H5PartCloseFile(h5_file);
#else
        throw std::runtime_error("Turbulence phase output only supported with HDF5 support\n");
#endif
    }
};

} // namespace sphexa
