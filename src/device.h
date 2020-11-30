#ifndef DEVICE_INCLUDE
#define DEVICE_INCLUDE

#include <string>
#include <tuple>
#include <memory>
#include "eXosip2/eXosip.h"
#include "load_h264.h"

using namespace std;

class Device {
public:
    Device() {}

    Device(string server_sip_id, string server_ip, int server_port,
            string device_sip_id, string username, string password,
            int local_port,
            string manufacture,
            string filepath): 
            server_sip_id(server_sip_id), 
            server_ip(server_ip),
            server_port(server_port),
            device_sip_id(device_sip_id),
            username(username),
            password(password),
            local_port(local_port),
            manufacture(manufacture),
            filepath(filepath) {
        sip_context = nullptr;
        is_running = false;
        is_register = false;
        local_ip = string(128, '0');

        load(filepath.c_str());
    }

    ~Device(){}

    void start();

    void stop();

    void process_request();

    void process_catalog_query(string sn);

    void process_deviceinfo_query(string sn);

    void process_devicestatus_query(string sn);

    void process_devicecontrol_query(string sn);

    void heartbeat_task();

    void send_request(osip_message_t * request);

    void send_response(shared_ptr<eXosip_event_t> evt, osip_message_t * msg);

    osip_message_t * create_msg();

    void send_response_ok(shared_ptr<eXosip_event_t> evt);

    std::tuple<string, string> get_cmd(const char * body);

    void push_rtp_stream();

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
    string rtp_ip;
    int rtp_port;
    string rtp_protocol;

    string filepath;

private:
    eXosip_t* sip_context;
    bool is_running;
    bool is_register;
    bool is_pushing;

    string from_sip;
    string to_sip;
    string ssrc;

    int sockfd;
    int bind();
    void send_network_packet(const char * data, int length);
};

#endif