#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

const std::string FILE_OFFSET = "file_offset=";
const std::string COL         = "col=";
const std::string VDEVIDX     = "vdevidx=";
const std::string DEV         = "dev=";
const std::string OFFSET      = "offset=";
const std::string SIZE        = "size=";

static int process_stanza_body(const std::string &dev_col, const std::string &offset_col,
                               const std::string &size_col, std::map <std::string, std::ifstream *> &devs,
                               char *buf, std::ofstream &output) {
    const std::string dev_name = dev_col.substr(DEV.size(), std::string::npos);
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

    std::stringstream offset_ss(offset_col.substr(OFFSET.size(), std::string::npos));
    std::stringstream size_ss(size_col.substr(SIZE.size(), std::string::npos));

    size_t offset = 0;
    size_t size = 0;
    if (!(offset_ss >> offset) || !(size_ss >> size)) {
        std::cerr << "Error: Could not read offset and/or size " << offset << " " << size << std::endl;
        return 1;
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
    if (argc < 3) {
        std::cerr << "Syntax: " << argv[0] << " output_file record_size" << std::endl;
        return 1;
    }

    std::ofstream output(argv[1], std::ofstream::binary | std::ofstream::trunc);
    if (!output) {
        std::cerr << "Error: Could not open output file" << std::endl;
        return 1;
    }

    size_t record_size;
    if (!(std::stringstream(argv[2]) >> record_size)) {
        std::cerr << "Error: Invalid record size" << std::endl;
        return 1;
    }

    // read first line
    // file size: %zu (%zu blocks)
    std::string str;
    char c;
    size_t size;
    size_t blocks;
    if (!(std::cin >> str >> str >> size >> c >> blocks >> str)) {
        std::cerr << "Error: Could not read first line of libzdb2 output" << std::endl;
        return 1;
    }

    std::getline(std::cin, str); // remove newline from first line

    int rc = 0;
    char *buf = new char[record_size];

    // avoid reopening devices
    std::map <std::string, std::ifstream *> devs;

    // read stanzas
    std::string line;
    while (std::getline(std::cin, line) && (rc == 0)) {
        std::cout << line << std::endl;
        // stanza start
        // file_offset=%zu vdev=%s io_offset=%zu record_size=%zu
        if (line.substr(0, FILE_OFFSET.size()) == FILE_OFFSET) {
            size_t file_offset = 0;
            if (!(std::stringstream(line.substr(FILE_OFFSET.size(), std::string::npos)) >> file_offset)) {
                std::cerr << "Error: Could not read file offset from \"" << line << "\"" << std::endl;
                rc = 1;
                break;
            }

            if (!output.seekp(file_offset)) {
                std::cerr << "Error: Could not seek to file offset" << std::endl;
                rc = 1;
                break;
            }
        }
        // stanza body
        // col=%zu devidx=%zu dev=%s offset=%zu size=%zu
        else if (line.substr(0, COL.size()) == COL) {
            std::string str, dev_col, offset_col, size_col;
            if (!(std::stringstream(line) >> str >> str >> dev_col >> offset_col >> size_col)) {
                std::cerr << "Error: Could not read size column" << std::endl;
                rc = 1;
                break;
            }

            rc = process_stanza_body(dev_col, offset_col, size_col, devs, buf, output);
        }
        // stanza body
        // devidx=%zu dev=%s offset=%zu size=%zu
        else if (line.substr(0, VDEVIDX.size()) == VDEVIDX) {
            std::string str, dev_col, offset_col, size_col;
            if (!(std::stringstream(line) >> str >> dev_col >> offset_col >> size_col)) {
                std::cerr << "Error: Could not read size column" << std::endl;
                rc = 1;
                break;
            }

            rc = process_stanza_body(dev_col, offset_col, size_col, devs, buf, output);
        }
        else {
            std::cerr << "Error: Got unexpected data in first column of \"" << line << "\"" << std::endl;
            rc = 1;
            break;
        }
    }

    output << std::flush;

    for(decltype(devs)::value_type & dev : devs) {
        delete dev.second;
    }
    delete [] buf;

    return rc;
}
