//
//  IJsonWriter.cpp
//

#include "IJsonWriter.hpp"
#include "../../TinyJS/utils/StringEx.h"
#include <time.h>


void IJsonWriter::writePropSize(const char *name, uint64_t value) {
    writeKey(name);

    double k = (double)value / 1024;
    if (k < 1) {
        writeString(stringPrintf("%dB", value).c_str());
        return;
    }

    double m = k / 1024;
    if (m < 1) {
        writeString(stringPrintf("%d (%.1fK)", value, k).c_str());
        return;
    }

    double g = m / 1024;
    if (g < 1) {
        writeString(stringPrintf("%d (%.1fM)", value, m).c_str());
        return;
    }

    double t = g / 1024;
    if (t < 1) {
        writeString(stringPrintf("%d (%.1fG)", value, g).c_str());
        return;
    }

    writeString(stringPrintf("%d (%d.1fT)", value, t).c_str());
}


#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

void IJsonWriter::writePropTime(const char *name, time_t time) {
    writeKey(name);

    char buffer[256] = "";
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localtime(&time));

    writeString(buffer);
}
