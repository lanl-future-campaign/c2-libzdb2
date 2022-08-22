#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

const std::string FILE_OFFSET = "file_offset=";
const std::string RECORD_SIZE = "record_size=";
const std::string COL         = "col=";
const std::string VDEVIDX     = "vdevidx=";
const std::string DEV         = "dev=";
const std::string OFFSET      = "offset=";
const std::string SIZE        = "size=";

static int process_stanza_body(const std::string &dev_col, const std::string &offset_col,
                               const std::string &size_col, std::map <std::string, std::ifstream *> &devs,
                               char *buf, std::ofstream &output) {
    // find previously opened device
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
    if (argc < 2) {
        std::cerr << "Syntax: " << argv[0] << " output_file" << std::endl;
        return 1;
    }

    std::ofstream output(argv[1], std::ofstream::binary | std::ofstream::trunc);
    if (!output) {
        std::cerr << "Error: Could not open output file" << std::endl;
        return 1;
    }

    // trash string
    std::string str;

    // ignore first line
    // file size: %zu (%zu blocks)
    std::getline(std::cin, str);

    int rc = 0;

    char *buf = nullptr;
    size_t buf_size = 0;

    // avoid reopening devices
    std::map <std::string, std::ifstream *> devs;

    // read stanzas
    std::string line;
    while (std::getline(std::cin, line) && (rc == 0)) {
        std::stringstream s(line);

        // stanza start
        // file_offset=%zu vdev=%s io_offset=%zu record_size=%zu
        if (line.substr(0, FILE_OFFSET.size()) == FILE_OFFSET) {
            s.seekg(FILE_OFFSET.size(), std::ios_base::beg);

            size_t file_offset = 0;
            std::string record_size_col;
            if (!(s >> file_offset >> str >> str >> record_size_col)) {
                std::cerr << "Error: Could not read start of stanza: \"" << line << "\"" << std::endl;
                rc = 1;
                break;
            }

            if (!output.seekp(file_offset)) {
                std::cerr << "Error: Could not seek to file offset: " << file_offset << std::endl;
                rc = 1;
                break;
            }

            size_t record_size = 0;
            if (!(std::stringstream(record_size_col.substr(RECORD_SIZE.size(), std::string::npos)) >> record_size)) {
                std::cerr << "Error: Could not read record size: " << record_size_col << std::endl;
                rc = 1;
                break;
            }

            if (record_size > buf_size) {
                delete [] buf;

                buf_size = record_size;
                buf = new char[buf_size];
            }
        }
        // stanza body
        // col=%zu devidx=%zu dev=%s offset=%zu size=%zu
        else if (line.substr(0, COL.size()) == COL) {
            std::string dev_col, offset_col, size_col;
            if (!(std::stringstream(line) >> str >> str >> dev_col >> offset_col >> size_col)) {
                std::cerr << "Error: Could not read column \"" << line << "\"" << std::endl;
                rc = 1;
                break;
            }

            rc = process_stanza_body(dev_col, offset_col, size_col, devs, buf, output);
        }
        // stanza body
        // devidx=%zu dev=%s offset=%zu size=%zu
        else if (line.substr(0, VDEVIDX.size()) == VDEVIDX) {
            std::string dev_col, offset_col, size_col;
            if (!(s >> str >> dev_col >> offset_col >> size_col)) {
                std::cerr << "Error: Could not read column \"" << line << "\"" << std::endl;
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
