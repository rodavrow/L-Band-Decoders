#include <iostream>
#include <fstream>
#include <complex>
#include <vector>
#include <thread>

#include "viterbi.h"
#include "diff.h"

// Processing buffer size
#define BUFFER_SIZE (8192 * 5)

// Small function that returns 1 bit from any type
template <typename T>
inline bool getBit(T data, int bit)
{
    return (data >> bit) & 1;
}

// Return filesize
size_t getFilesize(std::string filepath)
{
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    std::size_t fileSize = file.tellg();
    file.close();
    return fileSize;
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        std::cout << "Usage : " << argv[0] << " inputfile.bin outputframes.bin" << std::endl;
        return 1;
    }

    // Output and Input file
    std::ifstream data_in(argv[1], std::ios::binary);
    std::ofstream data_out(argv[2], std::ios::binary);

    // Our 2 Viterbi decoders and differential decoder
    FengyunViterbi viterbi1(true, 0.150f, 5, 20, 50), viterbi2(true, 0.150f, 5, 20, 50);
    FengyunDiff diff;

    // Viterbi output buffer
    uint8_t *viterbi1_out = new uint8_t[BUFFER_SIZE];
    uint8_t *viterbi2_out = new uint8_t[BUFFER_SIZE];

    // A few vectors for processing
    std::vector<std::complex<float>> *iSamples = new std::vector<std::complex<float>>(BUFFER_SIZE),
                                     *qSamples = new std::vector<std::complex<float>>(BUFFER_SIZE);

    // Read buffer
    std::complex<float> buffer[BUFFER_SIZE];

    // Diff decoder input and output
    std::vector<uint8_t> *diff_in = new std::vector<uint8_t>, *diff_out = new std::vector<uint8_t>;

    // Complete filesize
    size_t filesize = getFilesize(argv[1]);

    // Data we wrote out
    size_t data_out_total = 0;

    std::cout << "FengYun Decoder by Aang23" << std::endl;

    // Read until there is no more data
    while (!data_in.eof())
    {
        // Read a buffer
        data_in.read((char *)buffer, sizeof(std::complex<float>) * BUFFER_SIZE);

        // Deinterleave I & Q for the 2 Viterbis
        for (int i = 0; i < BUFFER_SIZE / 2; i++)
        {
            using namespace std::complex_literals;
            std::complex<float> iS = buffer[i * 2].imag() + buffer[i * 2 + 1].imag() * 1if;
            std::complex<float> qS = buffer[i * 2].real() + buffer[i * 2 + 1].real() * 1if;
            iSamples->push_back(iS);
            qSamples->push_back(qS);
        }

        // Run Viterbi!
        int v1 = viterbi1.work(*qSamples, qSamples->size(), viterbi1_out);
        int v2 = viterbi2.work(*iSamples, iSamples->size(), viterbi2_out);

        // Interleave and pack output into 2 bits chunks
        if (v1 == v2 && v1 > 0 && v2 > 0)
        {
            uint8_t bit1, bit2, bitCb;
            for (int y = 0; y < v1; y++)
            {
                for (int i = 7; i >= 0; i--)
                {
                    bit1 = getBit<uint8_t>(viterbi1_out[y], i);
                    bit2 = getBit<uint8_t>(viterbi2_out[y], i);
                    bitCb = bit2 << 1 | bit1;
                    diff_in->push_back(bitCb);
                }
            }
        }

        // Perform differential decoding
        *diff_out = diff.work(*diff_in);

        // Reconstruct into bytes and write to output file
        for (int i = 0; i < diff_out->size() / 4; i++)
        {
            uint8_t toPush = ((*diff_out)[i * 4] << 6) | ((*diff_out)[i * 4 + 1] << 4) | ((*diff_out)[i * 4 + 2] << 2) | (*diff_out)[i * 4 + 3];
            data_out.write((char *)&toPush, 1);
        }

        data_out_total += diff_out->size() / 4;

        // Console stuff
        std::cout << '\r' << "Viterbi 1 : " << (viterbi1.getState() == 0 ? "NO SYNC" : viterbi1.getState() == 1 ? "SYNCING" : "SYNCED") << ", Viterbi 2 : " << (viterbi2.getState() == 0 ? "NO SYNC" : viterbi2.getState() == 1 ? "SYNCING" : "SYNCED") << ", Data out : " << round(data_out_total / 1e5) / 10.0f << " MB, Progress : " << round(((float)data_in.tellg() / (float)filesize) * 1000.0f) / 10.0f << "%     " << std::flush;

        // Clear everything for the next run
        diff_in->clear();
        iSamples->clear();
        qSamples->clear();
    }

    std::cout << std::endl
              << "Done! Enjoy" << std::endl;

    // Close files
    data_in.close();
    data_out.close();
}