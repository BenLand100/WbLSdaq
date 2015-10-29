/**
 *  Copyright 2014 by Benjamin Land (a.k.a. BenLand100)
 *
 *  WbLSdaq is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  WbLSdaq is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with WbLSdaq. If not, see <http://www.gnu.org/licenses/>.
 */

#include "LeCroy6Zi.hh"

#include <cmath>
#include <iostream>
#include <arpa/inet.h>

using namespace std;

LeCroy6Zi::LeCroy6Zi(string addr, int port, double _timeout) : seqnum(0) {
    struct hostent *hostent = gethostbyname(addr.c_str());
    if (hostent == NULL) 
        throw runtime_error("Could not find host " + addr);
    
    struct sockaddr_in sockaddr;
    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sin_family = hostent->h_addrtype;
    sockaddr.sin_port = htons(port);
    memcpy(&sockaddr.sin_addr, hostent->h_addr, hostent->h_length);
    
    sockfd = socket(hostent->h_addrtype, SOCK_STREAM, 0);
    if (sockfd < 0) 
        throw runtime_error("Could not create socket");
    if (connect(sockfd, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) < 0)
        throw runtime_error("Could not connect socket " + to_string(errno));
             
    double ipart,fpart;
    fpart = modf(_timeout,&ipart);
    timeout.tv_sec = (int)ipart;
    timeout.tv_usec = round(fpart*1e6);
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) 
        throw runtime_error("Could not set recieve timeout");
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0) 
        throw runtime_error("Could not set send timeout");
        
    clear();
    
}

LeCroy6Zi::~LeCroy6Zi() {
    close(sockfd);
}

void fdwrite(int fd, const void *buf, size_t count) {
    size_t num = 0;
    while (num != count) {
        int res = write(fd,((char*)buf)+num,count-num);
        if (res == -1)
            throw runtime_error("Write failed " + to_string(errno));
        num += res;
    }
}

void fdread(int fd, void *buf, size_t count) {
    size_t num = 0;
    while (num != count) {
        int res = read(fd,((char*)buf)+num,count-num);
        if (res == -1)
            throw runtime_error("Read failed " + to_string(errno));
        if (res == 0) 
            throw runtime_error("EOF Reached");
        num += res;
    }
}

void LeCroy6Zi::send(string msg, uint8_t flags) {
    header h;
    h.operation = flags;
    h.version = 1;
    h.seqnum = ((seqnum++) % 254) + 1;
    h.spare = 0;
    h.length = htonl(msg.size()+1);
    
    //cout << "<<< " << msg << endl;
    
    fdwrite(sockfd,&h,sizeof(h));
    fdwrite(sockfd,msg.c_str(),msg.size());
    fdwrite(sockfd,"\n",1);
}

string LeCroy6Zi::recv() {
    header h;
    vector<uint8_t> buf;
    do {
        fdread(sockfd,&h,sizeof(h));
        h.length = ntohl(h.length);
        size_t off = buf.size();
        buf.resize(off+h.length);
        fdread(sockfd,&buf[off],h.length);
    } while (!(h.operation & OP_EOI));
    buf[h.length-1] = '\0';
    
    //cout << ">>> " << (const char*)&buf[0] << endl;
   
    return string((const char*)&buf[0]);
}

void LeCroy6Zi::clear(double _timeout) { 
    double ipart,fpart;
    fpart = modf(_timeout,&ipart);
    struct timeval clrtimeout;
    clrtimeout.tv_sec = (int)ipart;
    clrtimeout.tv_usec = round(fpart*1e6);
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&clrtimeout, sizeof(clrtimeout)) < 0)
        throw runtime_error("Could not set recieve timeout");
    char *buff = new char[4096];
    while (read(sockfd,buff,4096) > 0) { }
    delete [] buff;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
        throw runtime_error("Could not set recieve timeout");
}

void LeCroy6Zi::checklast() {
    send("cmr?");
    string response = recv();
    if (response.size() > 4) {
        unsigned long code = strtoul(&response[5],NULL,10);
        switch (code) {
            case 0:  return;
            case 1:  throw runtime_error("unrecognized command/query header");
            case 2:  throw runtime_error("illegal header path");
            case 3:  throw runtime_error("illegal number");
            case 4:  throw runtime_error("illegal number suffix");
            case 5:  throw runtime_error("unrecognized keyword");
            case 6:  throw runtime_error("string error");
            case 7:  throw runtime_error("GET embedded in another message");
            case 10: throw runtime_error("arbitrary data block expected");
            case 11: throw runtime_error("non-digit character in byte count field of arbitrary data block");
            case 12: throw runtime_error("EOI detected during definite length data block transfer");
            case 13: throw runtime_error("extra bytes detected during definite length data block transfer");
            default: throw runtime_error("Unknown response from CMR?");
        }
    } else {
        throw runtime_error("Invalid response to CMR?");
    }
}
