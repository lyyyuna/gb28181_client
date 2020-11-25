#include "spdlog/spdlog.h"
#include "device.h"

using namespace std;

int main() {
    auto device = shared_ptr<Device>(
        new Device("31011500002000000001", "218.98.28.36", 5061, 
            "31011500991320000342", "31011500991320000342", "abcde", 5688)
        );
    device->start();
}