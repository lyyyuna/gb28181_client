#include <string>
#include "spdlog/spdlog.h"
#include "cxxopts.h"
#include "device.h"

using namespace std;

int main(int argc, const char* argv[]) {
    cxxopts::ParseResult options_result;

    try {
        cxxopts::Options options(argv[0], "a gb28181 client - mainly used for testing");

        options.add_options()
            ("h, help", "Print usage")
            ("server-id", "specify the sip server id", cxxopts::value<std::string>())
            ("server-ip", "specify the sip server ip address", cxxopts::value<std::string>())
            ("server-port", "specify the sip server port", cxxopts::value<int>())
            ("device-id", "specify the gb28181 device id", cxxopts::value<std::string>())
            ("device-port", "specify the gb28181 device port", cxxopts::value<int>())
            ("username", "specify the gb28181 device username", cxxopts::value<std::string>())
            ("password", "specify the gb28181 device password", cxxopts::value<std::string>())
            ("manufacture", "specify the manufacture of the gb28181 device", cxxopts::value<std::string>())
            ("filepath", "specify the file path of the video sample", cxxopts::value<std::string>())
            ;

        options_result = options.parse(argc, argv);

        if (options_result.count("help"))
        {
            std::cout << options.help() << std::endl;
            exit(0);
        }

    } catch (const cxxopts::OptionException& e) {
        spdlog::critical("error parsing options: {}", e.what());
        exit(1);
    }

    string server_id;
    if (!options_result.count("server-id")) {
        server_id = "31011500002000000001";
    } else {
        server_id = options_result["server-id"].as<string>();
    }

    string server_ip;
    if (!options_result.count("server-ip")) {
        server_ip = "192.28.28.36";
    } else {
        server_ip = options_result["server-ip"].as<string>();
    }

    int server_port;
    if (!options_result.count("server-port")) {
        server_port = 5061;
    } else {
        server_port = options_result["server-port"].as<int>();
    }

    string device_id;
    if (!options_result.count("device-id")) {
        device_id = "31011500991320000342";
    } else {
        device_id = options_result["device-id"].as<string>();
    }

    int device_port;
    if (!options_result.count("device-port")) {
        device_port = 5688;
    } else {
        device_port = options_result["device-port"].as<int>();
    }

    string username;
    if (!options_result.count("username")) {
        username = device_id;
    } else {
        username = options_result["username"].as<string>();
    }

    string password;
    if (!options_result.count("password")) {
        password = "";
    } else {
        password = options_result["password"].as<string>();
    }   

    string manufacture;
    if (!options_result.count("manufacture")) {
        manufacture = "LYY";
    } else {
        manufacture = options_result["manufacture"].as<string>();
    }

    string filepath;
    if (!options_result.count("filepath")) {
        filepath = "sample.h264";
    } else {
        filepath = options_result["filepath"].as<string>();
    }

    spdlog::info("device info: ");
    spdlog::info("server sip sid: {}", server_id);
    spdlog::info("server ip address: {}", server_ip);
    spdlog::info("server port: {}", server_port);
    spdlog::info("device sip id: {}", device_id);
    spdlog::info("username: {}", username);
    spdlog::info("password: {}", password);
    spdlog::info("manufacture: {}", manufacture);
    spdlog::info("sample file path: {}", filepath);
    spdlog::info("");

    auto device = shared_ptr<Device>(
        new Device(server_id, server_ip, server_port, 
            device_id, username, password, device_port, manufacture,
            filepath)
        );
    device->start();
}