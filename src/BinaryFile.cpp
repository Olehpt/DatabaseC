#include "BinaryFile.h"
#include <bit>
#include <limits>

BinaryFile::~BinaryFile() { close(); }
bool BinaryFile::open(const std::string& filename, std::ios::openmode mode) {
    close(); stream = &disk; disk.clear();
    disk.open(filename, mode | std::ios::binary);
    return disk.is_open();
}
void BinaryFile::close() { if (disk.is_open()) disk.close(); }
bool BinaryFile::isOpen() const { return stream == &memory || disk.is_open(); }
void BinaryFile::openMemory(const std::string& bytes) {
    close(); memory.clear(); memory.str(bytes); stream = &memory;
}
std::string BinaryFile::bytes() const { return memory.str(); }
bool BinaryFile::finish() {
    file().flush();
    if (!file().good()) return false;
    if (disk.is_open()) disk.close();
    return file().good();
}
bool BinaryFile::canWrite(std::size_t size) {
    constexpr std::uint64_t limit = 64 * 1024 * 1024;
    auto position = file().tellp();
    return position != std::streampos(-1) && size <= limit &&
        static_cast<std::uint64_t>(position) <= limit - size;
}
std::uint64_t BinaryFile::remaining() {
    auto position = file().tellg();
    if (position == std::streampos(-1)) return 0;
    file().seekg(0, std::ios::end);
    auto end = file().tellg();
    file().seekg(position);
    return end < position ? 0 : static_cast<std::uint64_t>(end - position);
}
bool BinaryFile::writeUInt32(std::uint32_t value) {
    if (!canWrite(4)) return false;
    char bytes[4];
    for (unsigned i = 0; i < 4; ++i) bytes[i] = static_cast<char>((value >> (i * 8)) & 0xff);
    file().write(bytes, 4);
    return file().good();
}
bool BinaryFile::readUInt32(std::uint32_t& value) {
    unsigned char bytes[4];
    if (!file().read(reinterpret_cast<char*>(bytes), 4)) return false;
    value = 0;
    for (unsigned i = 0; i < 4; ++i) value |= static_cast<std::uint32_t>(bytes[i]) << (i * 8);
    return true;
}
bool BinaryFile::writeInt32(std::int32_t value) { return writeUInt32(std::bit_cast<std::uint32_t>(value)); }
bool BinaryFile::readInt32(std::int32_t& value) {
    std::uint32_t bits;
    if (!readUInt32(bits)) return false;
    value = std::bit_cast<std::int32_t>(bits); return true;
}
bool BinaryFile::writeDouble(double value) {
    static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);
    if (!canWrite(8)) return false;
    auto bits = std::bit_cast<std::uint64_t>(value);
    return writeUInt32(static_cast<std::uint32_t>(bits)) && writeUInt32(static_cast<std::uint32_t>(bits >> 32));
}
bool BinaryFile::readDouble(double& value) {
    std::uint32_t low, high;
    if (!readUInt32(low) || !readUInt32(high)) return false;
    value = std::bit_cast<double>(static_cast<std::uint64_t>(low) | (static_cast<std::uint64_t>(high) << 32));
    return true;
}
bool BinaryFile::writeBool(bool value) { return writeChar(value ? 1 : 0); }
bool BinaryFile::readBool(bool& value) {
    char byte;
    if (!readChar(byte) || (byte != 0 && byte != 1)) return false;
    value = byte == 1; return true;
}
bool BinaryFile::writeString(const std::string& value) {
    if (value.size() > 16 * 1024 * 1024 || !canWrite(value.size() + 4) ||
        !writeUInt32(static_cast<std::uint32_t>(value.size()))) return false;
    file().write(value.data(), static_cast<std::streamsize>(value.size()));
    return file().good();
}
bool BinaryFile::readString(std::string& value) {
    std::uint32_t size;
    if (!readUInt32(size) || size > 16 * 1024 * 1024 || size > remaining()) return false;
    std::string candidate(size, '\0');
    if (!file().read(candidate.data(), size)) return false;
    value = std::move(candidate); return true;
}
std::streampos BinaryFile::tell() { return file().tellg(); }
bool BinaryFile::seek(std::streampos position) {
    file().clear();
    file().seekg(position);
    return file().good();
}
bool BinaryFile::writeChar(char value) {
    if (!canWrite(1)) return false;
    file().write(&value, 1); return file().good();
}
bool BinaryFile::readChar(char& value) {
    file().read(&value, 1); return file().good();
}
