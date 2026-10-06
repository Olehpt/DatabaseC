#pragma once

#include <fstream>
#include <string>
#include <cstdint>
#include <sstream>

class BinaryFile
{
private:
    std::fstream disk;
    std::stringstream memory;
    std::iostream* stream = &disk;
    std::iostream& file() { return *stream; }
    bool canWrite(std::size_t size);

public:
    BinaryFile() = default;
    ~BinaryFile();

    bool open(const std::string& filename, std::ios::openmode mode);
    void close();

    bool isOpen() const;
    void openMemory(const std::string& bytes = {});
    std::string bytes() const;
    bool finish();
    std::uint64_t remaining();

    bool writeInt32(std::int32_t value);
    bool writeUInt32(std::uint32_t value);
    bool writeDouble(double value);
    bool writeBool(bool value);

    bool writeString(const std::string& value);

    bool readInt32(std::int32_t& value);
    bool readUInt32(std::uint32_t& value);
    bool readDouble(double& value);
    bool readBool(bool& value);

    bool readString(std::string& value);

    std::streampos tell();
    bool seek(std::streampos position);

    bool writeChar(char value);
    bool readChar(char& value);
};
