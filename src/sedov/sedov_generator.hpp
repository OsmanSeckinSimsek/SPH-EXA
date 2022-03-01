#pragma once

#include <iostream>
#include <cmath>
#include <vector>

#include "sph/kernels.hpp"

namespace sphexa
{

class SedovDataGenerator
{
public:
    static inline const unsigned dim           = 3;
    static inline const double   gamma         = 5. / 3.;
    static inline const double   omega         = 0.;
    static inline const double   r0            = 0.;
    static inline const double   r1            = 0.5;
    static inline const double   mTotal        = 1.;
    static inline const double   energyTotal   = 1.;
    static inline const double   width         = 0.1;
    static inline const double   ener0         = energyTotal / std::pow(M_PI, 1.5) / 1. / std::pow(width, 3.0);
    static inline const double   rho0          = 1.;
    static inline const double   u0            = 1.e-08;
    static inline const double   p0            = 0.;
    static inline const double   vr0           = 0.;
    static inline const double   cs0           = 0.;
    static inline const double   firstTimeStep = 1.e-6;

    template<class Dataset>
    static Dataset generate(const size_t side)
    {
        Dataset pd;

        if (pd.rank == 0 && side < 8)
        {
            printf("ERROR::Sedov::init()::SmoothingLength n too small\n");
#ifdef USE_MPI
            MPI_Finalize();
#endif
            exit(0);
        }

#ifdef USE_MPI
        pd.comm = MPI_COMM_WORLD;
        MPI_Comm_size(pd.comm, &pd.nrank);
        MPI_Comm_rank(pd.comm, &pd.rank);
#endif

        pd.side  = side;
        pd.n     = side * side * side;
        pd.count = pd.n;

        load(pd);
        init(pd);

        return pd;
    }

    // void load(const std::string &filename)
    template<class Dataset>
    static void load(Dataset& pd)
    {
        size_t split     = pd.n / pd.nrank;
        size_t remaining = pd.n - pd.nrank * split;

        pd.count = split;
        if (pd.rank == 0) pd.count += remaining;

        resize(pd, pd.count);

        if (pd.rank == 0)
            std::cout << "Approx: " << pd.count * (pd.data().size() * 64.) / (8. * 1000. * 1000. * 1000.)
                      << "GB allocated on rank 0." << std::endl;

        size_t offset = pd.rank * split;
        if (pd.rank > 0) offset += remaining;

        double step = (2. * r1) / pd.side;

#pragma omp parallel for
        for (size_t i = 0; i < pd.side; ++i)
        {
            double lz = -r1 + (i * step);

            for (size_t j = 0; j < pd.side; ++j)
            {
                double lx = -r1 + (j * step);

                for (size_t k = 0; k < pd.side; ++k)
                {
                    size_t lindex = (i * pd.side * pd.side) + (j * pd.side) + k;

                    if (lindex >= offset && lindex < offset + pd.count)
                    {
                        double ly = -r1 + (k * step);

                        pd.z[lindex - offset] = lz;
                        pd.y[lindex - offset] = ly;
                        pd.x[lindex - offset] = lx;

                        pd.vx[lindex - offset] = 0.;
                        pd.vy[lindex - offset] = 0.;
                        pd.vz[lindex - offset] = 0.;
                    }
                }
            }
        }
    }

    template<class Dataset>
    static void init(Dataset& pd)
    {
        const double step  = (2. * r1) / pd.side;
        const double hIni  = 1.5 * step;
        const double mPart = mTotal / pd.n;

#pragma omp parallel for schedule(static)
        for (size_t i = 0; i < pd.count; i++)
        {
            const double radius = std::sqrt(std::pow(pd.x[i], 2) + std::pow(pd.y[i], 2) + std::pow(pd.z[i], 2));

            pd.h[i] = hIni;
            pd.m[i] = mPart;
            pd.u[i] = ener0 * exp(-(std::pow(radius, 2) / std::pow(width, 2))) + u0;

            // pd.mui[i] = 10.;
            pd.du_m1[i] = 0.;

            pd.dt[i]    = firstTimeStep;
            pd.dt_m1[i] = firstTimeStep;
            pd.minDt    = firstTimeStep;

            pd.x_m1[i] = pd.x[i] - pd.vx[i] * firstTimeStep;
            pd.y_m1[i] = pd.y[i] - pd.vy[i] * firstTimeStep;
            pd.z_m1[i] = pd.z[i] - pd.vz[i] * firstTimeStep;
        }

        pd.etot = 0.;
        pd.ecin = 0.;
        pd.eint = 0.;
        pd.ttot = 0.;
    }
};

} // namespace sphexa
