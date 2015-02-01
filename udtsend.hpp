#ifndef __UDTSEND_HPP__
#define __UDTSEND_HPP__

#include <cstring> // strerror
#include <string>
#include <arpa/inet.h>
#include <exception>
#include <stdint.h>
#include "udt.h"

struct packet_header {
    uint8_t magic0;
    uint8_t magic1;
};

struct file_header {
    uint8_t magic0;
    uint8_t magic1;
    uint32_t st_size; // So we can't support files larger than 4GB...
    int32_t mtime;
    uint16_t filename_len;

    file_header() {}

    file_header(std::string filename, struct stat* st)
        : magic0(0xfe), magic1(0xfe), st_size(htonl(st->st_size)),
        mtime(htonl(st->st_mtime)), filename_len(htons(filename.length()))
    { }
};

class UDTSendException : std::exception {
    const char * message;
public:
    UDTSendException(const char * msg) : message(msg) {}
    const char * what() const noexcept { return message; }
};


template <class Fn>
struct Cleanup {
    const Fn& cleanup_fn;
    Cleanup(Fn& fn) : cleanup_fn(fn) { }
    ~Cleanup() { cleanup_fn(); }
};

template <typename F>
static auto cleanup_later(F&& f) -> Cleanup<decltype(f)> {
    return Cleanup<decltype(f)>(f);
};

static void throwlasterror()
{
    throw UDTSendException(UDT::getlasterror().getErrorMessage());
}

const size_t ex_msg_buflen = 1024;
static char ex_msg_buf[ex_msg_buflen];

static void throwlasterror(const char * msg)
{
    snprintf(ex_msg_buf, ex_msg_buflen, "%s: %s", msg, UDT::getlasterror().getErrorMessage());
    throw UDTSendException(ex_msg_buf);
}

//static void throwlasterrno()
//{
//    int e = errno;
//    throw UDTSendException(strerror(e));
//}

void throwlasterrno(const char * msg)
{
    int e = errno;
    snprintf(ex_msg_buf, ex_msg_buflen, "%s: %s", msg, strerror(e));
    throw UDTSendException(ex_msg_buf);
}

#endif // __UDTSEND_HPP__
