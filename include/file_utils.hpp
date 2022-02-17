#pragma once

#include <fstream>
#include <vector>

#ifdef USE_MPI
#include "mpi_file_utils.hpp"
#endif
#include "exceptions.hpp"

namespace sphexa
{
namespace fileutils
{
namespace details
{
void writeParticleDataToBinFile(std::ofstream&) {}

template<typename Arg, typename... Args>
void writeParticleDataToBinFile(std::ofstream& file, const Arg& first, const Args&... args)

{
    file.write((char*)&first[0], first.size() * sizeof(first[0]));

    writeParticleDataToBinFile(file, args...);
}

void writeParticleDataToAsciiFile(std::ostream&, size_t, char) {}

template<typename Arg, typename... Args>
void writeParticleDataToAsciiFile(std::ostream& file, size_t idx, char separator, const Arg& first, const Args&... data)
{
    file << first[idx] << separator;

    writeParticleDataToAsciiFile(file, idx, separator, data...);
}

void readParticleDataFromBinFile(std::ifstream&) {}

template<typename Arg, typename... Args>
void readParticleDataFromBinFile(std::ifstream& file, Arg& first, Args&... args)
{
    file.read(reinterpret_cast<char*>(&first[0]), first.size() * sizeof(first[0]));

    readParticleDataFromBinFile(file, args...);
}

void readParticleDataFromAsciiFile(std::ifstream&, size_t) {}

template<typename Arg, typename... Args>
void readParticleDataFromAsciiFile(std::ifstream& file, size_t idx, Arg& first, Args&... data)
{
    file >> first[idx];

    readParticleDataFromAsciiFile(file, idx, data...);
}

} // namespace details

template<typename Dataset, typename... Args>
void writeParticleCheckpointDataToBinFile(const Dataset& d, const std::string& path, Args&... data)
{
    std::ofstream checkpoint;
    checkpoint.open(path, std::ofstream::out | std::ofstream::binary);

    if (checkpoint.is_open())
    {
        checkpoint.write((char*)&d.n, sizeof(d.n));
        checkpoint.write((char*)&d.ttot, sizeof(d.ttot));
        checkpoint.write((char*)&d.minDt, sizeof(d.minDt));

        details::writeParticleDataToBinFile(checkpoint, data...);

        checkpoint.close();
    }
    else
    {
        throw FileNotOpenedException("Can't open file to write Checkpoint at path: " + path);
    }
}

template<typename... Args>
void writeParticleDataToBinFile(const std::string& path, Args&... data)
{
    std::ofstream checkpoint;
    checkpoint.open(path, std::ofstream::out | std::ofstream::binary);

    if (checkpoint.is_open())
    {
        details::writeParticleDataToBinFile(checkpoint, data...);

        checkpoint.close();
    }
    else
    {
        throw FileNotOpenedException("Can't open file at path: " + path);
    }
}

template<typename... Args>
void writeParticleDataToAsciiFile(size_t firstIndex, size_t lastIndex, const std::string& path, const bool append,
                                  const char separator, Args&... data)
{
    std::ios_base::openmode mode;
    if (append)
        mode = std::ofstream::app;
    else
        mode = std::ofstream::out;

    std::ofstream dump(path, mode);

    if (dump.is_open())
    {
        for (size_t i = firstIndex; i < lastIndex; ++i)
        {
            details::writeParticleDataToAsciiFile(dump, i, separator, data...);
            dump << std::endl;
        }
    }
    else
    {
        throw FileNotOpenedException("Can't open file at path: " + path);
    }

    dump.close();
}

template<typename... Args>
void readParticleDataFromBinFile(const std::string& path, Args&... data)
{
    std::ifstream inputfile(path, std::ios::binary);

    if (inputfile.is_open())
    {
        details::readParticleDataFromBinFile(inputfile, data...);
        inputfile.close();
    }
    else
    {
        throw FileNotOpenedException("Can't open file at path: " + path);
    }
}

/*! @brief read input data from an ASCII file
 *
 * @tparam Args  variadic number of vector-like objects
 * @param  path  the input file to read from
 * @param  data  the data containers to read into
 *               each data container needs to be the same size (checked)
 *
 *  Each data container will get one column of the input file.
 *  The number of rows to read is determined by the data container size.
 */
template<typename... Args>
void readParticleDataFromAsciiFile(const std::string& path, Args&... data)
{
    std::ifstream inputfile(path);

    std::array<size_t, sizeof...(Args)> sizes{data.size()...};
    size_t                              readSize = sizes[0];

    if (std::count(sizes.begin(), sizes.end(), readSize) != sizeof...(Args))
    {
        throw std::runtime_error("Argument vector sizes to read into are not equal\n");
    }

    if (inputfile.is_open())
    {
        for (size_t i = 0; i < readSize; ++i)
        {
            details::readParticleDataFromAsciiFile(inputfile, i, data...);
        }
        inputfile.close();
    }
    else
    {
        throw FileNotOpenedException("Can't open file at path: " + path);
    }
}

} // namespace fileutils
} // namespace sphexa
