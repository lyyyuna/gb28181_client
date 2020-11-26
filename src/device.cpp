#include "device.h"
#include "spdlog/spdlog.h"
#include "pugixml.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <string>
#include <sstream>
#include <thread>
#include <tuple> 

static int SN_MAX = 99999999;
static int sn;

static int get_sn() {
	if (sn >= SN_MAX) {
		sn = 0;
	}
	sn++;
	return sn;
}

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

    from_sip = from_uri.str();
    to_sip = proxy_uri.str();

    spdlog::info("from uri is {}", from_sip);
    spdlog::info("contact is {}", contact.str());
    spdlog::info("proxy_uri is {}", to_sip);

    // clear auth
    eXosip_clear_authentication_info(sip_context);

    osip_message_t * register_message = nullptr;
    int register_id = eXosip_register_build_initial_register(sip_context, from_sip.c_str(), 
                    to_sip.c_str(), 
                    contact.str().c_str(), 3600, &register_message);
    if (nullptr == register_message) {
        spdlog::error("eXosip_register_build_initial_register failed");
        return;
    }

    eXosip_lock(sip_context);
	eXosip_register_send_register(sip_context, register_id, register_message);
	eXosip_unlock(sip_context);

    thread heartbeat_task_thread(&Device::heartbeat_task, this);
    heartbeat_task_thread.detach();

    this->process_request();
}

void Device::process_request() {
    while (is_running) {
        auto evt = eXosip_event_wait(sip_context, 0, 500);

        eXosip_lock(sip_context);
        eXosip_automatic_action(sip_context);
        eXosip_unlock(sip_context);

        if (evt == nullptr) {
            continue;
        }


        switch (evt->type)
        {
        case EXOSIP_REGISTRATION_SUCCESS: {
            spdlog::info("register sucess");
            is_register = true;
            break;
        }
        case EXOSIP_REGISTRATION_FAILURE: {
            spdlog::info("register fail");
            if (evt->response == nullptr) {
                spdlog::error("register 401 has no response !!!");
                return;
            }

            if (401 == evt->response->status_code) {
                osip_www_authenticate_t * www_authenticate_header;

                osip_message_get_www_authenticate(evt->response, 0, &www_authenticate_header);

                if (eXosip_add_authentication_info(sip_context, device_sip_id.c_str(), username.c_str(), password.c_str(), 
                                    "MD5", www_authenticate_header->realm)) {
                    spdlog::error("register add auth failed");
                    return;
                };
            };
            break;
        }
        case EXOSIP_MESSAGE_NEW: {
            spdlog::info("got new message");

            if (MSG_IS_MESSAGE(evt->request)) {
                osip_body_t * body = nullptr;
                osip_message_get_body(evt->request, 0, &body);
                if (body != nullptr) {
                    spdlog::info("request: \n{}", body->body);
                }

                this->send_response_ok(evt);

                auto cmd_sn = this->get_cmd(body->body);
                string cmd = get<0>(cmd_sn);
                string sn = get<1>(cmd_sn);
                if ("Catalog" == cmd) {
                    stringstream ss;
                    ss << "<?xml version=\"1.0\" encoding=\"GB2312\"?>\r\n";
                    ss << "<Response>\r\n";
                    ss << "<CmdType>Catalog</CmdType>\r\n";
                    ss << "<SN>" << sn << "</SN>\r\n";
                    ss << "<DeviceID>" << device_sip_id << "</DeviceID>\r\n";
                    ss << "<SumNum>" << 1 << "</SumNum>\r\n";
                    ss << "<DeviceList Num=\"" << 1 << "\">\r\n";
                    ss << "<Item>\r\n";
                    ss << "<DeviceID>" << device_sip_id << "</DeviceID>\r\n";
                    ss << "<Name>IPC</Name>\r\n";
                    ss << "<ParentID>" << server_sip_id << "</ParentID>\r\n";
                    ss << "</Item>\r\n";
                    ss << "</DeviceList>\r\n";
                    ss << "</Response>\r\n";
                    spdlog::info("response: \n{}", ss.str());
                    auto request = create_request();
                    if (request != NULL) {
                        osip_message_set_content_type(request, "Application/MANSCDP+xml");
                        osip_message_set_body(request, ss.str().c_str(), strlen(ss.str().c_str()));
                        send_request(request);
                    }
                } else if ("RecordInfo" == cmd) {

                }

            }
            break;
        }
        
        defaut: 
            spdlog::info("unhandled sip evt type: {}", evt->type);
            break;
        }
    }
}

void Device::heartbeat_task() {
	while (true) {
        if (is_register) {
            stringstream ss;
            ss << "<?xml version=\"1.0\"?>\r\n";
            ss << "<Notify>\r\n";
            ss << "<CmdType>Keepalive</CmdType>\r\n";
            ss << "<SN>" << get_sn() << "</SN>\r\n";
            ss << "<DeviceID>" << device_sip_id << "</DeviceID>\r\n";
            ss << "<Status>OK</Status>\r\n";
            ss << "</Notify>\r\n";

            osip_message_t* request = create_request();
            if (request != NULL) {
                osip_message_set_content_type(request, "Application/MANSCDP+xml");
                osip_message_set_body(request, ss.str().c_str(), strlen(ss.str().c_str()));
                send_request(request);
            }
        }

		std::this_thread::sleep_for(std::chrono::seconds(60));
	}
}

osip_message_t * Device::create_request() {

    osip_message_t * request = nullptr;
    auto status = eXosip_message_build_request(sip_context, &request, "MESSAGE", to_sip.c_str(), from_sip.c_str(), nullptr);
    if (OSIP_SUCCESS != status) {
        spdlog::error("build request failed: {}", status);
    }

    return request;
}

void Device::send_request(osip_message_t * request) {
    eXosip_lock(sip_context);
    eXosip_message_send_request(sip_context, request);
    eXosip_unlock(sip_context);
}

void Device::send_response(eXosip_event_t * evt, osip_message_t * msg) {
    eXosip_lock(sip_context);
    eXosip_message_send_answer(sip_context, evt->tid, 200, msg);
    eXosip_unlock(sip_context);
}

void Device::send_response_ok(eXosip_event_t * evt) {
    auto msg = evt->request;
    eXosip_message_build_answer(sip_context, evt->tid, 200, &msg);
    send_response(evt, msg);
}

std::tuple<string, string> Device::get_cmd(const char * body) {
    pugi::xml_document document;

    if (!document.load(body)) {
        spdlog::error("cannot parse the xml");
        return make_tuple("", "");
    }

    pugi::xml_node root_node = document.first_child();

    if (!root_node) {
        spdlog::error("cannot get root node of xml");
        return make_tuple("", "");
    }

    string root_name = root_node.name();
    if ("Query" != root_name) {
        spdlog::error("invalid query xml with root: {}", root_name);
        return make_tuple("", "");
    }

    auto cmd_node = root_node.child("CmdType");

    if (!cmd_node) {
        spdlog::error("cannot get the cmd type");
        return make_tuple("", "");
    }

    auto sn_node = root_node.child("SN");

    if (!sn_node) {
        spdlog::error("cannot get the SN");
        return make_tuple("", "");
    }

    string cmd = cmd_node.child_value();
    string sn = sn_node.child_value();

    return make_tuple(cmd, sn);
}