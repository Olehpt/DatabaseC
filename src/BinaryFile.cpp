#include "BinaryFile.h"

BinaryFile::~BinaryFile()
{
    close();
}

bool BinaryFile::open(const std::string& filename, std::ios::openmode mode)
{
    file.open(filename, mode | std::ios::binary);
return file.is_open();
}

void BinaryFile::close()
{
    if (file.is_open())
        file.close();
}

bool BinaryFile::isOpen() const
{
    return file.is_open();
}


//writing

bool BinaryFile::writeInt32(std::int32_t value)
{
    file.write(reinterpret_cast <const char*> (&value), sizeof(value));
    return file.good();
}

bool BinaryFile::writeUInt32(std::uint32_t value)
{
    file.write(reinterpret_cast <const char*> (&value), sizeof(value));
    return file.good();
}

bool BinaryFile::writeDouble(double value)
{
    file.write(reinterpret_cast <const char*> (&value), sizeof(value));
    return file.good();
}

bool BinaryFile::writeBool(bool value)
{
    file.write(reinterpret_cast <const char*> (&value), sizeof(value));
    return file.good();
}

bool BinaryFile::writeString(const std::string& value)
{
    std::uint32_t size = static_cast<std::uint32_t>(value.size());

    if (!writeUInt32(size))
        return false;

    file.write(value.data(), size);

    return file.good();
}


//reading

bool BinaryFile::readInt32(std::int32_t & value)
{
    file.read(reinterpret_cast<char*>(&value), sizeof(value));
    return file.good();
}

bool BinaryFile::readUInt32(std::uint32_t & value)
{
    file.read(reinterpret_cast<char*>(&value), sizeof(value));
    return file.good();
}

bool BinaryFile::readDouble(double & value)
{
    file.read(reinterpret_cast<char*>(&value), sizeof(value));
    return file.good();
}

bool BinaryFile::readBool(bool & value)
{
    file.read(reinterpret_cast<char*>(&value), sizeof(value));
    return file.good();
}

bool BinaryFile::readString(std::string & value)
{
    std::uint32_t size;

    if (!readUInt32(size))
        return false;

    value.resize(size);

    file.read(value.data(), size);

    return file.good();
}

//position

std::streampos BinaryFile::tell()
{
    return file.tellg();
}

bool BinaryFile::seek(std::streampos position)
{
    file.seekg(position);
    file.seekp(position);

    return file.good();
}

bool BinaryFile::writeChar(char value)
{
    file.write(&value, sizeof(value));
    return file.good();
}

bool BinaryFile::readChar(char& value)
{
    file.read(&value, sizeof(value));
    return file.good();
}