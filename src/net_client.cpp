#include "spdlog/spdlog.h"
#include "device.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

int Device::bind() {

    if (rtp_protocol == "TCP/RTP/AVP") {
        sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    } else {
        sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    }

    sockaddr_in bind_address;
    bind_address.sin_family = AF_INET;
    bind_address.sin_port = htons(local_port);
    bind_address.sin_addr.s_addr = inet_addr(local_ip.c_str());

    auto bind_status = ::bind(sockfd, (struct sockaddr *)&bind_address, sizeof(bind_address));
    if (bind_status) {
        spdlog::error("socket bind failed: {}", bind_status);
        return bind_status;
    }

    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(rtp_port);
    inet_pton(AF_INET, rtp_ip.c_str(), &server_address.sin_addr);

    auto connect_status = ::connect(sockfd, (struct sockaddr *)&server_address, sizeof(server_address));
    if ( connect_status < 0) {
        spdlog::error("socket connect failed: {}", connect_status);
        return connect_status;
    }

    return 0;
}

void Device::send_network_packet(const char * data, int length) {
    if (rtp_protocol == "TCP/RTP/AVP") {
        ::send(sockfd, data, length, 0);
    } else {
        sockaddr_in server_address;
        server_address.sin_family = AF_INET;
        server_address.sin_port = htons(rtp_port);
        inet_pton(AF_INET, rtp_ip.c_str(), &server_address.sin_addr);
        ::sendto(sockfd, data, length, 0, (struct sockaddr *)&server_address, sizeof(server_address));
    }
}