// This file is a part of SeqOthello. Please refer to LICENSE.TXT for the LICENSE
#include "socket.h"

#ifdef WIN32
#include <winsock.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

#include <stdexcept>
#include <vector>
#include <errno.h>

using namespace std;

#define THROW_IF(val) if ((val)) throw runtime_error("error in " + string(__FUNCTION__))
#define THROW_IF_MSG(msg,val) if ((val)) throw runtime_error("error in " + string(__FUNCTION__) + ":" + (msg))

#ifdef WIN32
static bool initialized = false;
#endif

Socket::Socket(int type, int protocol) 
{
#ifdef WIN32
    if (!initialized) {
        WORD wVersionRequested;
        WSADATA wsaData;
        wVersionRequested = MAKEWORD(2, 0);              // Request WinSock v2.0
        THROW_IF_MSG("fail to connect", (WSAStartup(wVersionRequested, &wsaData) != 0));
        initialized = true;
    }
#endif
    THROW_IF_MSG("fail to connect", (sockDesc = socket(PF_INET, type, protocol)) < 0);
}

Socket::Socket(int sockDesc)
{
    this->sockDesc = sockDesc;
}

Socket::~Socket()
{
#ifdef WIN32
    ::closesocket(sockDesc);
#else
    ::close(sockDesc);
#endif
    sockDesc = -1;
}

string Socket::getLocalAddr() 
{
    sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);
    THROW_IF((getsockname(sockDesc, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0));
    return inet_ntoa(addr.sin_addr);
}

unsigned short Socket::getLocalPort() 
{
    sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);
    THROW_IF(getsockname(sockDesc, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0);
    return ntohs(addr.sin_port);
}

void Socket::setLocalPort(unsigned short localPort) 
{
    sockaddr_in localAddr;
    memset(&localAddr, 0, sizeof(localAddr));
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    localAddr.sin_port = htons(localPort);
    THROW_IF_MSG("fail to bind port", (bind(sockDesc, (sockaddr *) &localAddr, sizeof(sockaddr_in)) < 0));
}

void TCPSocket::connect(const string &foreignAddr,
                        unsigned short port) 
{
    sockaddr_in destAddr;
    memset(&destAddr, 0, sizeof(destAddr));
    destAddr.sin_family = AF_INET;
    hostent *host;
    THROW_IF_MSG( "Fail to resolve name"+ foreignAddr,((host = gethostbyname(foreignAddr.c_str())) == NULL));
    destAddr.sin_addr.s_addr = *((unsigned long *) host->h_addr_list[0]);
    destAddr.sin_port = htons(port);
    THROW_IF_MSG("Fail to connect to "+foreignAddr,::connect(sockDesc, (sockaddr *) &destAddr, sizeof(destAddr)) < 0);
}

void TCPSocket::send(const void *buffer, int bufferLen) 
{
    THROW_IF(::send(sockDesc, (void *) buffer, bufferLen, 0) < 0);
}

int TCPSocket::recv(void *buffer, int bufferLen) 
{
    int rtn;
    THROW_IF((rtn = ::recv(sockDesc, (void *) buffer, bufferLen, 0)) < 0);
    return rtn;
}

string TCPSocket::getForeignAddr() 
{
    sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);
    THROW_IF (getpeername(sockDesc, (sockaddr *) &addr,(socklen_t *) &addr_len) < 0);
    return inet_ntoa(addr.sin_addr);
}

unsigned short TCPSocket::getForeignPort() 
{
    sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);
    THROW_IF(getpeername(sockDesc, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0);
    return ntohs(addr.sin_port);
}


TCPSocket::TCPSocket() : Socket(SOCK_STREAM, IPPROTO_TCP) {}

TCPSocket::TCPSocket(const string &foreignAddr, unsigned short foreignPort) : Socket(SOCK_STREAM, IPPROTO_TCP)
{
    connect(foreignAddr, foreignPort);
}

TCPSocket::TCPSocket(int newConnSD) : Socket(newConnSD) {}

void TCPSocket::sendmsg(const string &str)
{
    this->sendmsg(str.c_str(), str.size());
};

void TCPSocket::sendmsg(const char * buf, uint32_t len)
{  
    if (len == 0) {
            int sendlen = 0;
        this->send((void *) (&sendlen), sizeof(int32_t));
        printf("Send empty msg\n");
        return;
    }

    for (unsigned int shift = 0; shift*BUFLEN <len; shift++) {
        int32_t sendlen = -BUFLEN; 
        if (shift*BUFLEN+BUFLEN>=len)
                sendlen = len - (shift*BUFLEN);
        this->send((void *) (&sendlen), sizeof(int32_t));
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpointer-arith"
        this->send(& buf[shift*BUFLEN], sendlen<0?-sendlen:sendlen);
  //      printf("Send sub mesg: %d\n",  sendlen);
#pragma GCC diagnostic pop
    }
    printf("Send mesg %d\n",  len);
}

bool TCPSocket::recvmsg(string &str)
{
    int32_t len;
    str = "";
    while (true) {
        uint32_t siz = this->recv(&len, sizeof(int32_t));
        if (siz != sizeof(uint32_t)) return false;
        if ((len < 0 && len != -BUFLEN) || len>BUFLEN) {
            return false;
        }
        int recvlen = (len <0)?-len:len;
    //    printf("Recvlen %d\n", recvlen);
        if (recvlen >0) {
            this->recv(&recvbuf[0], recvlen);
      //      printf("recv sub mesg: %d\n",  recvlen);
            str += string (&recvbuf[0],(&recvbuf[0])+recvlen);
        }
        if (len>=0) return true;
    }
    return true;
}

TCPServerSocket::TCPServerSocket(unsigned short localPort, int queueLen)
        : Socket(SOCK_STREAM, IPPROTO_TCP)
{
    setLocalPort(localPort);
    THROW_IF_MSG("Fail while setting listening socket", listen(sockDesc, queueLen) < 0);
}

TCPSocket *TCPServerSocket::accept() 
{
    int newConnSD;
    THROW_IF((newConnSD = ::accept(sockDesc, NULL, 0)) < 0);
    return new TCPSocket(newConnSD);
}


