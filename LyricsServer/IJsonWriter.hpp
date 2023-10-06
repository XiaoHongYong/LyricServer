//
//  IJsonWriter.hpp
//

#pragma once

#ifndef IJsonWriter_hpp
#define IJsonWriter_hpp

#include "Types.h"


class IJsonWriter {
public:
    virtual ~IJsonWriter() { }

    virtual void startObject() = 0;
    virtual void endObject() = 0;

    virtual void writeKey(const char *s, size_t len) = 0;
    virtual void writeKey(const char *s) = 0;
    void writeKey(const string &s) { writeKey(s.c_str(), s.size()); }

    virtual void startArray() = 0;
    virtual void endArray() = 0;

    virtual void writeString(const char *s, size_t len) = 0;
    virtual void writeString(const char *s) = 0;
    void writeString(const string &s) { writeString(s.c_str(), s.size()); }

    virtual void writeNull() = 0;
    virtual void writeBool(bool b) = 0;
    virtual void writeInt(int i) = 0;
    virtual void writeUint(unsigned u) = 0;
    virtual void writeInt64(int64_t i64) = 0;
    virtual void writeUint64(uint64_t u64) = 0;
    virtual void writeDouble(double d) = 0;

    virtual const char *getString() = 0;
    virtual const size_t getSize() = 0;

    void writePropString(const char *name, const string &value) {
        writeKey(name);
        writeString(value);
    }

    void writePropBool(const char *name, bool value) {
        writeKey(name);
        writeBool(value);
    }

    void writePropInt(const char *name, int value) {
        writeKey(name);
        writeInt(value);
    }

    void writePropInt64(const char *name, int64_t value) {
        writeKey(name);
        writeInt64(value);
    }

    void writePropUint(const char *name, uint32_t value) {
        writeKey(name);
        writeUint(value);
    }

    void writePropUint64(const char *name, uint64_t value) {
        writeKey(name);
        writeUint64(value);
    }

    void writePropSize(const char *name, uint64_t value);
    void writePropTime(const char *name, time_t time);

    void writePropStringArray(const char *name, const VecStrings &arr) {
        writeKey(name);
        startArray();
        for (auto &str : arr) {
            writeString(str);
        }
        endArray();
    }

};

typedef std::shared_ptr<IJsonWriter> IJsonWriterPtr;

#endif /* IJsonWriter_hpp */
