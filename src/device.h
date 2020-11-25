#ifndef DEVICE_INCLUDE
#define DEVICE_INCLUDE

#include <string>
#include "eXosip2/eXosip.h"

using namespace std;

class Device {
public:
    Device() {}

    Device(string server_sip_id, string server_ip, int server_port,
            string device_sip_id, string username, string password,
            int local_port): 
            server_sip_id(server_sip_id), 
            server_ip(server_ip),
            server_port(server_port),
            device_sip_id(device_sip_id),
            username(username),
            password(password),
            local_port(local_port) {
        sip_context = nullptr;
        is_running = false;
        local_ip = string(128, '0');
    }

    ~Device(){}

    void start();

    void stop();

public:
    string server_sip_id;
    string server_ip;
    int server_port;
    string device_sip_id;
    string username;
    string password;
    string local_ip;
    int local_port;

private:
    eXosip_t* sip_context;
    bool is_running;
};

#endif