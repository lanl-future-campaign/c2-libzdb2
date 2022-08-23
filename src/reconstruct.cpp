#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

const std::string FILE_OFFSET = "file_offset=";
const std::string COL         = "col=";
const std::string VDEVIDX     = "vdevidx=";
const std::string DEV         = "dev=";

static int process_stanza_body(const std::string &dev_name, const std::size_t offset,
                               const std::size_t size, std::map <std::string, std::ifstream *> &devs,
                               char *buf, std::ofstream &output) {
    // find previously opened device
    std::istream *dev = nullptr;
    std::map <std::string, std::ifstream *>::const_iterator it = devs.find(dev_name);
    if (it == devs.end()) {
        std::ifstream *newdev = new std::ifstream(dev_name, std::ios::binary);
        if (!*newdev) {
            std::cerr << "Error: Could not open device " << dev_name << std::endl;
            delete newdev;
            return 1;
        }

        devs[dev_name] = newdev;
        dev = newdev;
    }
    else {
        dev = it->second;
    }

    if (!dev->seekg(offset, std::ios_base::beg)) {
        std::cerr << "Error: Could not move device to offset" << std::endl;
        return 1;
    }

    if (!dev->read(buf, size)) {
        std::cerr << "Error: Could not read from device" << std::endl;
        return 1;
    }

    output.write(buf, size);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Syntax: " << argv[0] << " output_file" << std::endl;
        return 1;
    }

    std::ofstream output(argv[1], std::ofstream::binary | std::ofstream::trunc);
    if (!output) {
        std::cerr << "Error: Could not open output file" << std::endl;
        return 1;
    }

    // file size: %zu (%zu blocks)
    std::size_t expected_size = 0;
    std::size_t expected_blocks = 0;
    if (std::scanf("file size: %zu (%zu blocks)\n", &expected_size, &expected_blocks) != 2) {
        std::cerr << "Error: Could not parse first line of input" << std::endl;
        return 1;
    }

    int rc = 0;

    std::size_t stanza_start_size = 0;
    std::size_t stanza_blocks     = 0;
    std::size_t stanza_row_size   = 0;

    // buffer for reading from drives
    char *buf = nullptr;
    std::size_t buf_size = 0;

    // avoid reopening devices
    std::map <std::string, std::ifstream *> devs;

    // read stanzas
    std::string line;
    while (std::getline(std::cin, line) && (rc == 0)) {
        // stanza start
        // file_offset=%zu vdev=%s io_offset=%zu record_size=%zu
        if (line.substr(0, FILE_OFFSET.size()) == FILE_OFFSET) {
            std::size_t file_offset = 0;
            std::size_t record_size = 0;

            static const std::string format = FILE_OFFSET + "%zu vdev=%*s io_offset=%*zu record_size=%zu";
            if (std::sscanf(line.c_str(), format.c_str(), &file_offset, &record_size) != 2) {
                std::cerr << "Error: Could not parse start of stanza: \"" << line << "\"" << std::endl;
                rc = 1;
                break;
            }

            if (!output.seekp(file_offset)) {
                std::cerr << "Error: Could not seek to file offset: " << file_offset << std::endl;
                rc = 1;
                break;
            }

            if (record_size > buf_size) {
                delete [] buf;

                buf_size = record_size;
                buf = new char[buf_size];
            }

            stanza_start_size += record_size;
            stanza_blocks++;
        }
        // stanza body
        // col=%zu vdevidx=%zu dev=%s offset=%zu size=%zu
        else if (line.substr(0, COL.size()) == COL) {
            std::stringstream s(line);
            s.ignore(line.size(), ' '); // ignore col
            s.ignore(line.size(), ' '); // ignore devidx

            std::string dev;
            std::size_t offset;
            std::size_t size;

            static const std::string format = COL + "%*zu vdevidx=%*zu dev=%*s offset=%zu size=%zu";
            if (!(s >> dev) ||
                (std::sscanf(line.c_str(), format.c_str(), &offset, &size) != 2)) {
                std::cerr << "Error: Could not parse line: \"" << line << "\"" << std::endl;
                rc = 1;
                break;
            }

            stanza_row_size += size;

            rc = process_stanza_body(dev.substr(DEV.size(), std::string::npos),
                                     offset, size, devs, buf, output);
        }
        // stanza body
        // vdevidx=%zu dev=%s offset=%zu size=%zu
        else if (line.substr(0, VDEVIDX.size()) == VDEVIDX) {
            std::stringstream s(line);
            s.ignore(line.size(), ' '); // ignore devidx

            std::string dev;
            std::size_t offset;
            std::size_t size;

            static const std::string format = VDEVIDX + "%*zu dev=%*s offset=%zu size=%zu";
            if (!(s >> dev) ||
                (std::sscanf(line.c_str(), format.c_str(), &offset, &size) != 2)) {
                std::cerr << "Error: Could not parse line: \"" << line << "\"" << std::endl;
                rc = 1;
                break;
            }

            stanza_row_size += size;

            rc = process_stanza_body(dev.substr(DEV.size(), std::string::npos),
                                     offset, size, devs, buf, output);
        }
        else {
            std::cerr << "Error: Got unexpected data in first column of \"" << line << "\"" << std::endl;
            rc = 1;
            break;
        }
    }

    output << std::flush;

    for(decltype(devs)::value_type &dev : devs) {
        delete dev.second;
    }
    delete [] buf;

    if (rc == 0) {
        if (expected_size != stanza_start_size) {
            std::cerr << "Warning: Sum of stanza sizes (" << stanza_start_size << ") does not match expected size (" << expected_size << ")" << std::endl;
        }

        if (expected_blocks != stanza_blocks) {
            std::cerr << "Warning: Got " << stanza_blocks << " blocks of data even though input declared " << expected_blocks << std::endl;
        }

        if (expected_size != stanza_row_size) {
            std::cerr << "Warning: Sum of stanza row sizes (" << stanza_row_size << ") does not match expected size (" << expected_size << ")" << std::endl;
        }
    }

    return rc;
}
