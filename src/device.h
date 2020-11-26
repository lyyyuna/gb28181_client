#ifndef DEVICE_INCLUDE
#define DEVICE_INCLUDE

#include <string>
#include <tuple>
#include "eXosip2/eXosip.h"

using namespace std;

class Device {
public:
    Device() {}

    Device(string server_sip_id, string server_ip, int server_port,
            string device_sip_id, string username, string password,
            int local_port,
            string manufacture): 
            server_sip_id(server_sip_id), 
            server_ip(server_ip),
            server_port(server_port),
            device_sip_id(device_sip_id),
            username(username),
            password(password),
            local_port(local_port),
            manufacture(manufacture) {
        sip_context = nullptr;
        is_running = false;
        is_register = false;
        local_ip = string(128, '0');
    }

    ~Device(){}

    void start();

    void stop();

    void process_request();

    void heartbeat_task();

    void send_request(osip_message_t * request);

    void send_response(eXosip_event_t * evt, osip_message_t * msg);

    osip_message_t * create_request();

    void send_response_ok(eXosip_event_t * evt);

    std::tuple<string, string> get_cmd(const char * body);

public:
    string server_sip_id;
    string server_ip;
    int server_port;
    string device_sip_id;
    string username;
    string password;
    string local_ip;
    int local_port;

    string manufacture;

private:
    eXosip_t* sip_context;
    bool is_running;
    bool is_register;

    string from_sip;
    string to_sip;
};

#endif