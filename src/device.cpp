#include "device.h"
#include "spdlog/spdlog.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <string>
#include <sstream>

void Device::start() {
    spdlog::info("sip init begin.");

    sip_context = eXosip_malloc();

    if (OSIP_SUCCESS != eXosip_init(sip_context)) {
        spdlog::error("sip init failed.");
        return;
    }

    if (OSIP_SUCCESS != eXosip_listen_addr(sip_context, IPPROTO_UDP, nullptr, local_port, AF_INET, 0)) {
        spdlog::critical("sip port bind failed.");
        eXosip_quit(sip_context);
        sip_context = nullptr;
        return;
    }

    // run
    is_running = true;

    ostringstream from_uri;
    ostringstream contact;
    ostringstream proxy_uri;

    // local ip & port
    eXosip_guess_localip(sip_context, AF_INET, data(local_ip), local_ip.length());
    spdlog::info("local ip is {}", local_ip);

    from_uri << "sip:" << device_sip_id << "@" << local_ip << ":" << local_port;
    contact << "sip:" << device_sip_id << "@" << local_ip << ":" << local_port;
    proxy_uri << "sip:" << server_sip_id << "@" << server_ip << ":" << server_port;

    spdlog::info("from uri is {}", from_uri.str());
    spdlog::info("contact is {}", contact.str());
    spdlog::info("proxy_uri is {}", proxy_uri.str());

    // clear auth
    eXosip_clear_authentication_info(sip_context);

    osip_message_t * register_message = nullptr;
    int register_id = eXosip_register_build_initial_register(sip_context, from_uri.str().c_str(), 
                    proxy_uri.str().c_str(), 
                    contact.str().c_str(), 3600, &register_message);
    if (nullptr == register_message) {
        spdlog::error("eXosip_register_build_initial_register failed");
        return;
    }

    eXosip_lock(sip_context);
	eXosip_register_send_register(sip_context, register_id, register_message);
	eXosip_unlock(sip_context);
}