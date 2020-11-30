#include "device.h"
#include "spdlog/spdlog.h"
#include "pugixml.hpp"
#include "gb28181_header_maker.h"
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

void Device::push_rtp_stream() {
    is_pushing = true;

    auto status = this->bind();
    if (status != 0) {
        spdlog::error("device bind socket address failed: {}", status);
        return ;
    }

	char ps_header[PS_HDR_LEN];

	char ps_system_header[SYS_HDR_LEN];

	char ps_map_header[PSM_HDR_LEN];

	char pes_header[PES_HDR_LEN];

	char rtp_header[RTP_HDR_LEN];    

	int time_base = 90000;
	int fps = 24;
	int send_packet_interval = 1000 / fps;

	int interval = time_base / fps;
	long pts = 0;

	char frame[1024 * 128];

	int single_packet_max_length = 1400;

	char rtp_packet[RTP_HDR_LEN+1400];

	// int ssrc = 0xffffffff;
	int rtp_seq = 0;

    // Nalu *nalu = new Nalu();
	// nalu->packet = (char *)malloc(1024*128);
	// nalu->length = 1024 * 128;

    while (is_pushing) {
        for (auto i = 0; i < nalu_vector.size(); i++) {
            auto nalu = nalu_vector.at(i);

            NaluType  type = nalu->type;
            int length = nalu->length;
            char * packet = nalu->packet;

            int index = 0;
            if (NALU_TYPE_IDR == type) {
                gb28181_make_ps_header(ps_header, pts);

                memcpy(frame,ps_header,PS_HDR_LEN);
                index += PS_HDR_LEN;

                gb28181_make_sys_header(ps_system_header, 0x3f);

                memcpy(frame+ index, ps_system_header, SYS_HDR_LEN);
                index += SYS_HDR_LEN;

                gb28181_make_psm_header(ps_map_header);

                memcpy(frame + index, ps_map_header, PSM_HDR_LEN);
                index += PSM_HDR_LEN;

            } else {
                gb28181_make_ps_header(ps_header, pts);

                memcpy(frame, ps_header, PS_HDR_LEN);
                index += PS_HDR_LEN;
            }

            //封装pes
            gb28181_make_pes_header(pes_header, 0xe0, length, pts, pts);

            memcpy(frame+index, pes_header, PES_HDR_LEN);
            index += PES_HDR_LEN;

            memcpy(frame + index, packet, length);
            index += length;

            //组包rtp

            int rtp_packet_count = ((index - 1) / single_packet_max_length) + 1;

            for (int i = 0; i < rtp_packet_count; i++) {

                gb28181_make_rtp_header(rtp_header, rtp_seq, pts, atoi(ssrc.c_str()), i == (rtp_packet_count - 1));

                int writed_count = single_packet_max_length;

                if ((i + 1)*single_packet_max_length > index) {
                    writed_count = index - (i* single_packet_max_length);
                }
                //添加包长字节
                int rtp_start_index=0;

                unsigned short rtp_packet_length = RTP_HDR_LEN + writed_count;
                if (rtp_protocol == "TCP/RTP/AVP") {
                    unsigned char packt_length_ary[2];
                    packt_length_ary[0] = (rtp_packet_length >> 8) & 0xff;
                    packt_length_ary[1] = rtp_packet_length & 0xff;
                    memcpy(rtp_packet, packt_length_ary, 2);
                    rtp_start_index = 2;
                }

                memcpy(rtp_packet+ rtp_start_index, rtp_header, RTP_HDR_LEN);
                memcpy(rtp_packet+ +rtp_start_index + RTP_HDR_LEN, frame+ (i* single_packet_max_length), writed_count);
                rtp_seq++;

                if (is_pushing) {
                    send_network_packet(rtp_packet, rtp_start_index + rtp_packet_length);
                }
                else {
                    if (nalu != nullptr) {
                        delete nalu;
                        nalu = nullptr;
                    }
                    return;
                }
            }

            pts += interval;

            std::this_thread::sleep_for(std::chrono::milliseconds(send_packet_interval));
        }
    }

    is_pushing = false;
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
        auto evt = shared_ptr<eXosip_event_t>(
            eXosip_event_wait(sip_context, 0, 100),
            eXosip_event_free);

        eXosip_lock(sip_context);
        eXosip_automatic_action(sip_context);
        eXosip_unlock(sip_context);

        if (evt == nullptr) {
            continue;
        }

        switch (evt->type)
        {
        case eXosip_event_type::EXOSIP_REGISTRATION_SUCCESS: {
            spdlog::info("got REGISTRATION_SUCCESS");
            is_register = true;
            break;
        }
        case eXosip_event_type::EXOSIP_REGISTRATION_FAILURE: {
            spdlog::info("got REGISTRATION_FAILURE");
            if (evt->response == nullptr) {
                spdlog::error("register 401 has no response !!!");
                break;
            }

            if (401 == evt->response->status_code) {
                osip_www_authenticate_t * www_authenticate_header;

                osip_message_get_www_authenticate(evt->response, 0, &www_authenticate_header);

                if (eXosip_add_authentication_info(sip_context, device_sip_id.c_str(), username.c_str(), password.c_str(), 
                                    "MD5", www_authenticate_header->realm)) {
                    spdlog::error("register add auth failed");
                    break;
                };
            };
            break;
        }
        case eXosip_event_type::EXOSIP_MESSAGE_NEW: {
            spdlog::info("got MESSAGE_NEW");

            if (MSG_IS_MESSAGE(evt->request)) {
                osip_body_t * body = nullptr;
                osip_message_get_body(evt->request, 0, &body);
                if (body != nullptr) {
                    spdlog::info("new message request: \n{}", body->body);
                }

                this->send_response_ok(evt);

                auto cmd_sn = this->get_cmd(body->body);
                string cmd = get<0>(cmd_sn);
                string sn = get<1>(cmd_sn);
                spdlog::info("got new cmd: {}", cmd);
                if ("Catalog" == cmd) {
                    this->process_catalog_query(sn);
                } else if ("DeviceStatus" == cmd) {
                    this->process_devicestatus_query(sn);
                } else if ("DeviceInfo" == cmd) {
                    this->process_deviceinfo_query(sn);
                } else if ("DeviceControl" == cmd) {
                    this->process_devicecontrol_query(sn);
                } else {
                    spdlog::error("unhandled cmd: {}", cmd);
                }
            } else if (MSG_IS_BYE(evt->request)) {
                spdlog::info("got BYE message");
                this->send_response_ok(evt);
                break;
            }
            break;
        }
        case eXosip_event_type::EXOSIP_CALL_INVITE: {
            spdlog::info("got CALL_INVITE");

            auto sdp_msg = eXosip_get_remote_sdp(sip_context, evt->did);
            if (!sdp_msg) {
                spdlog::error("eXosip_get_remote_sdp failed");
                break;
            }

            auto connection = eXosip_get_video_connection(sdp_msg);
            if (!connection) {
                spdlog::error("eXosip_get_video_connection failed");
                break;                
            }

            rtp_ip = connection->c_addr;

            auto video_sdp = eXosip_get_video_media(sdp_msg);
            if (!video_sdp) {
                spdlog::error("eXosip_get_video_media failed");
                break;                  
            }

            rtp_port = atoi(video_sdp->m_port);

            spdlog::info("rtp server: {}:{}", rtp_ip, rtp_port);

            rtp_protocol = video_sdp->m_proto;

            spdlog::info("rtp protocol: {}", rtp_protocol);

            osip_body_t *sdp_body = NULL;
			osip_message_get_body(evt->request, 0, &sdp_body);
            if (nullptr == sdp_body) {
                spdlog::error("osip_message_get_body failed");
                break; 
            }

            string body = sdp_body->body;
            auto y_sdp_first_index = body.find("y=");
            auto y_sdp = body.substr(y_sdp_first_index);
            auto y_sdp_last_index = y_sdp.find("\r\n");
            ssrc = y_sdp.substr(2, y_sdp_last_index-1);
            spdlog::info("ssrc: {}", ssrc);

            stringstream ss;
            ss << "v=0\r\n";
            ss << "o=" << device_sip_id << " 0 0 IN IP4 " << local_ip << "\r\n";
            ss << "s=Play\r\n";
            ss << "c=IN IP4 " << local_ip << "\r\n";
            ss << "t=0 0\r\n";
            if (rtp_protocol == "TCP/RTP/AVP") {
                ss << "m=video " << local_port << " TCP/RTP/AVP 96\r\n";
            }
            else {
                ss << "m=video " << local_port << " RTP/AVP 96\r\n";
            }
            ss << "a=sendonly\r\n";
            ss << "a=rtpmap:96 PS/90000\r\n";
            ss << "y=" << ssrc << "\r\n";
            string sdp_output_str  = ss.str();

            size_t size = sdp_output_str.size();

            osip_message_t * message = evt->request;
            int status = eXosip_call_build_answer(sip_context, evt->tid, 200, &message);

            if (status != 0) {
                spdlog::error("call invite build answer failed");
                break;
            }
            
            osip_message_set_content_type(message, "APPLICATION/SDP");
            osip_message_set_body(message, sdp_output_str.c_str(), sdp_output_str.size());

            eXosip_call_send_answer(sip_context, evt->tid, 200, message);

            spdlog::info("reply call invite: \n{}", sdp_output_str);
            break;
        }
        case eXosip_event_type::EXOSIP_CALL_ACK: {
            spdlog::info("got CALL_ACK: begin pushing rtp stream...");
            if (is_pushing) {
                spdlog::info("already pushing rtp stream");
            } else {
                thread t(&Device::push_rtp_stream, this);
                t.detach();
            }
            break;
        }
        case eXosip_event_type::EXOSIP_CALL_CLOSED: {
            spdlog::info("got CALL_CLOSED: stop pushing rtp stream...");

            break;
        }
        case eXosip_event_type::EXOSIP_MESSAGE_ANSWERED: {
            spdlog::info("got MESSAGE_ANSWERED: unhandled");
            break;
        }
        
        default: {
            spdlog::info("unhandled sip evt type: {}", evt->type);
            break;
        }
        }
    }
}

void Device::process_catalog_query(string sn) {
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
    ss << "<Manufacturer>" << manufacture << "</Manufacturer>\r\n";
    ss << "<Status>ON</Status>\r\n";
    ss << "<Name>IPC</Name>\r\n";
    ss << "<ParentID>" << server_sip_id << "</ParentID>\r\n";
    ss << "</Item>\r\n";
    ss << "</DeviceList>\r\n";
    ss << "</Response>\r\n";
    spdlog::info("catalog response: \n{}", ss.str());
    auto request = create_msg();
    if (request != NULL) {
        osip_message_set_content_type(request, "Application/MANSCDP+xml");
        osip_message_set_body(request, ss.str().c_str(), strlen(ss.str().c_str()));
        send_request(request);
    }
}

void Device::process_devicestatus_query(string sn) {
    stringstream ss;

    time_t rawtime;
    struct tm* timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    char curtime[72] = {0};
    sprintf(curtime, "%d-%d-%dT%02d:%02d:%02d", (timeinfo->tm_year + 1900), (timeinfo->tm_mon + 1),
                        timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);    
    
    ss << "<?xml version=\"1.0\"?>\r\n";
    ss << "<Response>\r\n";
    ss << "<CmdType>DeviceStatus</CmdType>\r\n";
    ss << "<SN>" << get_sn() << "</SN>\r\n";
    ss << "<DeviceID>" << device_sip_id << "</DeviceID>\r\n";
    ss << "<Result>OK</Result>\r\n";
    ss << "<Online>ONLINE</Online>\r\n";
    ss << "<Status>OK</Status>\r\n";
    ss << "<DeviceTime>" << curtime << "</DeviceTime>\r\n";
    ss << "<Alarmstatus Num=\"0\">\r\n";
    ss << "</Alarmstatus>\r\n";
    ss << "<Encode>ON</Encode>\r\n";
    ss << "<Record>OFF</Record>\r\n";
    ss << "</Response>\r\n";

    spdlog::info("devicestatus response: \n{}", ss.str());
    auto request = create_msg();
    if (request != NULL) {
        osip_message_set_content_type(request, "Application/MANSCDP+xml");
        osip_message_set_body(request, ss.str().c_str(), strlen(ss.str().c_str()));
        send_request(request);
    }
}

void Device::process_deviceinfo_query(string sn) {
    stringstream ss;

    ss << "<?xml version=\"1.0\"?>\r\n";
    ss <<    "<Response>\r\n";
    ss <<    "<CmdType>DeviceInfo</CmdType>\r\n";
    ss <<    "<SN>" << get_sn() << "</SN>\r\n";
    ss <<    "<DeviceID>" << device_sip_id << "</DeviceID>\r\n";
    ss <<    "<Result>OK</Result>\r\n";
    ss <<    "<DeviceType>simulate client</DeviceType>\r\n";
    ss <<    "<Manufacturer>ZHD</Manufacturer>\r\n";
    ss <<    "<Model>28181</Model>\r\n";
    ss <<    "<Firmware>fireware</Firmware>\r\n";
    ss <<    "<MaxCamera>1</MaxCamera>\r\n";
    ss <<    "<MaxAlarm>0</MaxAlarm>\r\n";
    ss <<    "</Response>\r\n";

    spdlog::info("deviceinfo response: \n{}", ss.str());
    auto request = create_msg();
    if (request != NULL) {
        osip_message_set_content_type(request, "Application/MANSCDP+xml");
        osip_message_set_body(request, ss.str().c_str(), strlen(ss.str().c_str()));
        send_request(request);
    }
}

void Device::process_devicecontrol_query(string sn) {

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

            osip_message_t* request = create_msg();
            if (request != NULL) {
                osip_message_set_content_type(request, "Application/MANSCDP+xml");
                osip_message_set_body(request, ss.str().c_str(), strlen(ss.str().c_str()));
                send_request(request);
                spdlog::info("sent heartbeat");
            }
        }

		std::this_thread::sleep_for(std::chrono::seconds(60));
	}
}

osip_message_t * Device::create_msg() {

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

void Device::send_response(shared_ptr<eXosip_event_t> evt, osip_message_t * msg) {
    eXosip_lock(sip_context);
    eXosip_message_send_answer(sip_context, evt->tid, 200, msg);
    eXosip_unlock(sip_context);
}

void Device::send_response_ok(shared_ptr<eXosip_event_t> evt) {
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