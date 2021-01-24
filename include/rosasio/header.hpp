#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <cstring>

namespace rosasio
{
    class Message
    {
    public:
        Message()
            : buf(sizeof(uint32_t), 0)
        {
            //
        }

        void add_field(const std::string &name, const std::string &value)
        {
            add_field(name + "=" + value);
        }

        void add_field(const std::string &value)
        {
            uint32_t len = value.size();
            uint8_t *bytes = reinterpret_cast<uint8_t *>(&len);
            std::copy(bytes, bytes + sizeof(len), std::back_inserter(buf));
            std::copy(value.begin(), value.end(), std::back_inserter(buf));
        }

        void finish()
        {
            uint32_t len = buf.size() - sizeof(uint32_t);
            uint8_t *bytes = reinterpret_cast<uint8_t *>(&len);
            std::memcpy(buf.data(), bytes, sizeof(len));
        }

        std::vector<unsigned char> buf;
    };
} // namespace rosasio