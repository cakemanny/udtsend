#define _GNU_SOURCE

#include <arpa/inet.h>
#include <sys/stat.h>
#include <libgen.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <array>
#include <cstdio>
#include <unistd.h> //getopt
#include <getopt.h> //getopt_long
#include <utime.h>
#include "udt.h"
#include "udtsend.hpp"

struct option long_options[] = {
    { "help",   no_argument,        NULL, 'h' },
    { "dir",    required_argument,  NULL, 'd' },
    { "host",   required_argument,  NULL, 'H' },
    { "port",   required_argument,  NULL, 'p' },
    { 0, 0, 0, 0 }
};

char* program_name = NULL;
char* remote_host = NULL;
int remote_port = -1;
const char* output_dir = ".";

static void print_usage_and_exit(int status) {
    FILE* out = (status == 0) ? stdout : stderr;

    fprintf(out, "Usage: %s [OPTION]... FILE\n", program_name);
    fputs("\
\n\
  -h, --help                display this help message\n\
  -d, --dir=DIRECTPRY       directory to write files to\n\
  -H, --host=IP             IP address of master\n\
  -p, --port=PORT           remote port to connect to\n\
", out);
    exit(status);
}

template <typename T>
static inline T min(T t1, T t2)
{
    return (t1 < t2) ? t1 : t2;
}

template <std::size_t N>
static std::array<char, N> readall(UDTSOCKET s)
{
    std::array<char, N> retbuf;
    int bytes_left = N;
    while (bytes_left > 0) {
        int bytes_recvd = UDT::recv(s, retbuf.data() + (N - bytes_left), bytes_left, 0);
        if (bytes_recvd < 0)
            throwlasterror();
        bytes_left -= bytes_recvd;
    }
    return retbuf;
}

static std::vector<char> readall(UDTSOCKET s, std::size_t size)
{
    static char buf[1024];
    std::vector<char> result;
    result.reserve(size);
    size_t bytes_left = size;
    while (bytes_left > 0) {
        int bytes_recvd = UDT::recv(s, buf, min((size_t)1024, bytes_left), 0);
        if (bytes_recvd < 0)
            throwlasterror();
        for (int i = 0; i < bytes_recvd; ++i)
            result.push_back(buf[i]);
        bytes_left -= bytes_recvd;
    }
    return result;
}

int main(int argc, char** argv)
{
    program_name = argv[0];
    int c;
    while ( (c = getopt_long(argc, argv, "hd:H:p:", long_options, NULL)) != -1 ) {
        switch (c) {
            case 'h':
                print_usage_and_exit(0);
                break;
            case 'd':
                output_dir = optarg;
                break;
            case 'H':
                remote_host = optarg;
                break;
            case 'p':
                remote_port = atoi(optarg);
                break;
            case '?':
                print_usage_and_exit(1);
                break;
            default: // Shouldn't happen
                fprintf(stderr, "bad option: %c\n", optopt);
                print_usage_and_exit(1);
                break;
        }
    }
    if (remote_host == NULL) {
        fprintf(stderr, "host argument missing\n");
        print_usage_and_exit(1);
    }
    if (remote_port == -1) {
        fprintf(stderr, "port argument missing\n");
        print_usage_and_exit(1);
    }
    try {
        if (UDT::startup() == UDT::ERROR)
            throwlasterror("UDT startup failed");
        auto udtcleanup = cleanup_later(UDT::cleanup);

        UDTSOCKET s = UDT::socket(AF_INET, SOCK_STREAM, 0);
        if (s == UDT::INVALID_SOCK) throwlasterror();
        auto scleanup = cleanup_later([s]{ UDT::close(s); });

        sockaddr_in remote_addr = {};
        remote_addr.sin_family = AF_INET;
        remote_addr.sin_port = htons(remote_port);
        if (inet_aton(remote_host, &remote_addr.sin_addr) < 0)
            throw UDTSendException("Host IP in unknown format");
        if (UDT::connect(s, (struct sockaddr*)&remote_addr, sizeof remote_addr) < 0)
            throwlasterror();

        for (;;) {
            auto sig = readall<2>(s);
            if (sig[0] == (char)0xee && sig[1] == (char)0xee) {
                std::cout << "End of files" << std::endl;
                break;
            } else if (sig[0] != (char)0xfe || sig[1] != (char)0xfe) {
                throw UDTSendException("Invalid header!");
            } else {
                std::cout << "File header" << std::endl;
            }
            auto restofheader = readall<sizeof(file_header) - 2>(s);
            file_header header;
            char* hdbuf = (char*)&header;
            memcpy(hdbuf, sig.data(), sig.size());
            memcpy(hdbuf + sig.size(), restofheader.data(), restofheader.size());
            unsigned long filesize = ntohl(header.st_size);
            std::cout << "filesize = " << filesize << std::endl;
            long lastmodifiedtime = ntohl(header.mtime);
            std::cout << "lastmodifiedtime = " << lastmodifiedtime << std::endl;
            unsigned short filename_len = ntohs(header.filename_len);
            std::cout << "filename_len = " << filename_len << std::endl;

            std::vector<char> pathv = readall(s, filename_len);
            std::string filepath(pathv.data(), pathv.size());

            std::cout << "filepath = " << filepath << std::endl;
            char pathbuf[filepath.length() + 1];
            memset(pathbuf, 0, filepath.length() + 1);
            memcpy(pathbuf, filepath.c_str(), filepath.length());
            std::string filename(basename(pathbuf));
            std::string output_path = output_dir + ("/" + filename);
            struct stat st;
            if (stat(output_path.c_str(), &st) == 0) {
                std::cerr << "File already exists: " << output_path << std::endl;
                std::cout << "Reading all bytes off of network" << std::endl;
                // Read off the rest of the file
                size_t total_bytes_read = 0;
                std::array<char,8192> buf;
                while (total_bytes_read < filesize) {
                    size_t bytes_to_read = min((decltype(filesize))buf.size(), filesize - total_bytes_read);
                    int bytes_read = UDT::recv(s, buf.data(), bytes_to_read, 0);
                    if (bytes_read < 0)
                        throwlasterror();
                    total_bytes_read += bytes_read;
                }
            } else {
                std::cout << "Writing file to disk: " << output_path << std::endl;
                std::ofstream ofs(output_path, std::ofstream::binary);
                if (ofs.good()) {
                    size_t total_bytes_read = 0;
                    std::array<char,8192> buf;
                    while (total_bytes_read < filesize) {
                        size_t bytes_to_read = min((decltype(filesize))buf.size(), filesize - total_bytes_read);
                        int bytes_read = UDT::recv(s, buf.data(), bytes_to_read, 0);
                        if (bytes_read < 0)
                            throwlasterror();
                        total_bytes_read += bytes_read;
                        ofs.write(buf.data(), bytes_read);
                    }
                } else {
                    throw UDTSendException(("Unable to write file: " + output_path).c_str());
                }
                ofs.close();

                std::cout << "Setting lastmodified time" << std::endl;
                struct utimbuf times = {
                    .actime = lastmodifiedtime,
                    .modtime = lastmodifiedtime
                };
                if (utime(output_path.c_str(), &times) < 0)
                    std::cerr << "Unable to correctly set the modified time for the file" << std::endl;
            }
        }

    } catch (UDTSendException& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "An unknown error of some form occurred..." << std::endl;
        return 1;
    }

}

