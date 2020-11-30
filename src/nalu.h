#ifndef NALU_INCLUDE_H
#define NALU_INCLUDE_H

typedef enum {
    NALU_TYPE_SLICE = 1,
    NALU_TYPE_DPA = 2,
    NALU_TYPE_DPB = 3,
    NALU_TYPE_DPC = 4,
    NALU_TYPE_IDR = 5,
    NALU_TYPE_SEI = 6,
    NALU_TYPE_SPS = 7,
    NALU_TYPE_PPS = 8,
    NALU_TYPE_AUD = 9,
    NALU_TYPE_EOSEQ = 10,
    NALU_TYPE_EOSTREAM = 11,
    NALU_TYPE_FILL = 12,
} NaluType;

class Nalu {
public:
    char * packet;
    int length;
    NaluType type;

    ~Nalu() {
        if (packet != nullptr) {
            delete packet;
            packet = nullptr;
        }
    }
};

#endif