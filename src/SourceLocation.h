#pragma once

#include <memory>
#include <string>
struct SourceLocation
{
    std::string filename;
    std::shared_ptr<std::string> source;
    std::size_t byte_offset;
    std::size_t num_bytes;

    [[nodiscard]] std::string text() const { return source->substr(byte_offset, num_bytes); }
    [[nodiscard]] size_t lineStart() const { return source->rfind('\n', byte_offset) + 1; }
    [[nodiscard]] std::string sourceline() const
    {
        const size_t endPos = source->find('\n', byte_offset);
        const size_t startPos = lineStart();
        return source->substr(startPos, endPos - startPos);
    }

    bool operator==(const SourceLocation &other) const
    {
        return filename == other.filename && byte_offset == other.byte_offset && num_bytes == other.num_bytes;
    }
};
