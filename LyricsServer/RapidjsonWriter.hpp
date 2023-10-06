//
//  RapidjsonWriter.hpp
//

#pragma once

#ifndef RapidjsonWriter_hpp
#define RapidjsonWriter_hpp

#include "IJsonWriter.hpp"
#include <rapidjson/prettywriter.h>
#include <rapidjson/writer.h>


template<typename Writer>
class RapidjsonWriter_t : public IJsonWriter {
public:
    RapidjsonWriter_t() : _writer(_buf) {}

    virtual void startObject() override { _writer.StartObject(); }
    virtual void endObject() override { _writer.EndObject(); }

    virtual void writeKey(const char *s, size_t len) override { _writer.Key(s, (int)len); }
    virtual void writeKey(const char *s) override { _writer.Key(s); }

    virtual void startArray() override { _writer.StartArray(); }
    virtual void endArray() override { _writer.EndArray(); }

    virtual void writeString(const char *s, size_t len) override { _writer.String(s, (int)len); }
    virtual void writeString(const char *s) override { _writer.String(s); }

    virtual void writeNull() override { _writer.Null(); }
    virtual void writeBool(bool b) override { _writer.Bool(b); }
    virtual void writeInt(int i) override { _writer.Int(i); }
    virtual void writeUint(unsigned u) override { _writer.Uint(u); }
    virtual void writeInt64(int64_t i64) override { _writer.Int64(i64); }
    virtual void writeUint64(uint64_t u64) override { _writer.Uint64(u64); }
    virtual void writeDouble(double d) override { _writer.Double(d); }

    virtual const char *getString() override { return _buf.GetString(); }
    virtual const size_t getSize() override { return _buf.GetSize(); }

private:
    rapidjson::StringBuffer                             _buf;
    Writer                                              _writer;

};

using RapidjsonWriterX = RapidjsonWriter_t<rapidjson::Writer<rapidjson::StringBuffer, rapidjson::UTF8<>, rapidjson::UTF8<>, rapidjson::CrtAllocator, rapidjson::kWriteNanAndInfFlag>>;

using RapidjsonPrettyWriterX = RapidjsonWriter_t<rapidjson::PrettyWriter<rapidjson::StringBuffer, rapidjson::UTF8<>, rapidjson::UTF8<>, rapidjson::CrtAllocator, rapidjson::kWriteNanAndInfFlag>>;


#endif /* RapidjsonWriter_hpp */
