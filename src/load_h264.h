#ifndef LOAD_H264_INCLUDE_H
#define LOAD_H264_INCLUDE_H

#include <vector>
#include "nalu.h"

extern std::vector<Nalu*> nalu_vector;
int load(const char *path);

#endif