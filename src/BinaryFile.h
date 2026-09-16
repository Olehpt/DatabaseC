#pragma once

#include <fstream>
#include <string>
#include <cstdint>

class BinaryFile
{
private:
    std::fstream file;

public:
    BinaryFile() = default;
    ~BinaryFile();

    bool open(const std::string& filename, std::ios::openmode mode);
    void close();

    bool isOpen() const;

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