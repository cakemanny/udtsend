#define _GNU_SOURCE

#include <arpa/inet.h>
#include <sys/stat.h>
#include <libgen.h>
#include <iostream>
#include <string>
#include <cstdio>
#include "udt.h"
#include "udtsend.hpp"

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <file-to-send> ..." << std::endl;
        return 1;
    }
    for (int i = 1; i < argc; i++) {
        struct stat st;
        int res = stat(argv[i], &st);
        if (res < 0) {
            perror("unable to stat file");
            return 1;
        }
    }
    try {
        if (UDT::startup() == UDT::ERROR)
            throwlasterror("UDT startup failed");
        auto udtcleanup = cleanup_later(UDT::cleanup);

        UDTSOCKET ss = UDT::socket(AF_INET, SOCK_STREAM, 0);
        if (ss == UDT::INVALID_SOCK) throwlasterror();
        auto sscleanup = cleanup_later([ss]{ UDT::close(ss); });

        sockaddr_in our_addr = {};
        our_addr.sin_family = AF_INET;
        our_addr.sin_port = htons(9002);
        our_addr.sin_addr.s_addr = INADDR_ANY;
        std::cout << "Binding to (0.0.0.0, 9002)" << std::endl;
        if (UDT::bind(ss, (struct sockaddr*)&our_addr, sizeof our_addr) == UDT::ERROR)
            throwlasterror();

        if (UDT::listen(ss, 1) == UDT::ERROR) throwlasterror();

        int namelen;
        sockaddr_in remote_addr;
        UDTSOCKET s = UDT::accept(ss, (sockaddr*)&remote_addr, &namelen);
        if (s == UDT::INVALID_SOCK) throwlasterror();
        auto scleanup = cleanup_later([s]{ UDT::close(s); });
        std::cout << "Connection from: " <<
            inet_ntoa(remote_addr.sin_addr) << ":" <<
            ntohs(remote_addr.sin_port) << std::endl;

        for (int i = 1; i < argc; i++) {
            struct stat st;
            if (stat(argv[i], &st) < 0)
                throwlasterrno("Unable to stat file");
            std::string filename(basename(argv[i]));
            std::cout << "Sending file: " << filename << std::endl;

            file_header header(filename, &st);
            if (UDT::send(s, (char*)&header, sizeof header, 0) == UDT::ERROR)
                throwlasterror();
            if (UDT::send(s, filename.c_str(), filename.length(), 0) == UDT::ERROR)
                throwlasterror();
            int64_t offset = 0;
            if (UDT::sendfile2(s, argv[i], &offset, st.st_size) == UDT::ERROR)
                throwlasterror();
        }
        std::cout << "Sent all files" << std::endl;
        char end_packet[2] = { (char) 0xee, (char) 0xee };
        if (UDT::send(s, end_packet, 2, 0) == UDT::ERROR) throwlasterror();


    } catch (UDTSendException& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "An unknown error of some form occurred..." << std::endl;
        return 1;
    }
}

