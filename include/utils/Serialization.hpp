#pragma once

#include "InputStream.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <concepts>
#include <bit>
#include <type_traits>
#include <string>
#include <ostream>
#include <ios>
#include <vector>

namespace Serialization {
#if __cplusplus >= 202302L
#define byteswap std::byteswap
#else
    template<std::integral T>
    T byteswap(T value) {
        size_t size = sizeof(T);
        auto* p = reinterpret_cast<std::uint8_t*>(&value);
        for (size_t i = 0; i < size / 2; ++i) {
            std::swap(p[i], p[size - 1 - i]);
        }
        return value;
    }
#endif

    template<std::integral T>
    [[nodiscard]] constexpr T hostToNetwork(T value) noexcept {
        if constexpr (std::endian::native == std::endian::big) {
            return value;
        } else {
            return byteswap(value);
        }
    }

    template<std::integral T>
    [[nodiscard]] constexpr T networkToHost(T value) noexcept {
        return hostToNetwork(value);
    }

    template<std::integral T>
    void Write(std::ostream& stream, T value) {
        value = hostToNetwork<T>(value);
        stream.write(reinterpret_cast<char*>(&value), sizeof(T));
    }

    inline void Write(std::ostream& stream, float value) {
        Write<uint32_t>(stream, std::bit_cast<uint32_t>(value));
    }
    inline void Write(std::ostream& stream, double value) {
        Write<uint64_t>(stream, std::bit_cast<uint64_t>(value));
    }

    inline void Read(InputStream& stream, void* dest, std::streamsize size) {
        stream.Read(static_cast<char*>(dest), size);
    }

    template<typename T>
    T Read(InputStream& stream);

    template<std::integral T>
    T Read(InputStream& stream) {
        T value;
        stream.Read(reinterpret_cast<char*>(&value), sizeof(T));
        return networkToHost<T>(value);
    }

    template<>
    inline float Read(InputStream& stream) {
        return std::bit_cast<float>(Read<uint32_t>(stream));
    }
    template<>
    inline double Read(InputStream& stream) {
        return std::bit_cast<double>(Read<uint64_t>(stream));
    }

    template<typename T, typename CompT>
    requires std::is_base_of_v<glm::vec<2, CompT>, T>
    glm::vec<2, CompT> Read(InputStream& stream) {
        return { Read<CompT>(stream), Read<CompT>(stream) };
    }
    template<typename T, typename CompT>
    requires std::is_base_of_v<glm::vec<3, CompT>, T>
    glm::vec<3, CompT> Read(InputStream& stream) {
        return { Read<CompT>(stream), Read<CompT>(stream), Read<CompT>(stream) };
    }
    template<typename T, typename CompT>
    requires std::is_base_of_v<glm::qua<CompT>, T>
    glm::qua<CompT> Read(InputStream& stream) {
        return { Read<CompT>(stream), Read<CompT>(stream), Read<CompT>(stream), Read<CompT>(stream) };
    }

    template<typename CompT>
    void Write(std::ostream& stream, const glm::vec<2, CompT>& value) {
        Write(stream, value.x);
        Write(stream, value.y);
    }
    template<typename CompT>
    void Write(std::ostream& stream, const glm::vec<3, CompT>& value) {
        Write(stream, value.x);
        Write(stream, value.y);
        Write(stream, value.z);
    }
    template<typename CompT>
    void Write(std::ostream& stream, const glm::qua<CompT>& value) {
        Write(stream, value.x);
        Write(stream, value.y);
        Write(stream, value.z);
        Write(stream, value.w);
    }

    template<std::integral SizeT>
    void Write(std::ostream& stream, const std::string& value) {
        auto length = static_cast<SizeT>(value.size());
        Write<SizeT>(stream, length);
        if (length == 0) return;
        stream.write(value.data(), length);
    }
    inline void Write(std::ostream& stream, const std::string& value) { return Write<uint32_t>(stream, value); }

    template<typename T, std::integral SizeT>
    requires std::is_base_of_v<std::string, T>
    std::string Read(InputStream& stream, SizeT maxLength = 0) {
        auto length = Read<SizeT>(stream);
        if (length == 0) return {};
        if (maxLength > 0 && length > maxLength) throw std::runtime_error("String is longer than the max length!");
        std::string value;
        value.resize(length);
        stream.Read(value.data(), length);
        return value;
    }
    template<>
    inline std::string Read(InputStream& stream) { return Read<std::string, uint32_t>(stream); }

    template<typename ItemT, std::integral SizeT>
    void Write(std::ostream& stream, const std::vector<ItemT>& value) {
        auto length = static_cast<SizeT>(value.size());
        Write<SizeT>(stream, length);
        if (length == 0) return;
        stream.write(static_cast<char*>(value.data()), length * sizeof(ItemT));
    }
    template<typename ItemT>
    void Write(std::ostream& stream, const std::vector<ItemT>& value) { return Write<ItemT, uint32_t>(stream, value); }

    template<typename T, typename ItemT, std::integral SizeT>
    requires std::is_base_of_v<std::vector<ItemT>, T>
    std::vector<ItemT> Read(InputStream& stream, SizeT maxLength = 0) {
        SizeT length = Read<SizeT>(stream);
        if (length == 0) return {};
        if (maxLength > 0 && length > maxLength) throw std::runtime_error("Vector is longer than the max length!");
        std::vector<ItemT> value;
        value.resize(length);
        auto size = length * sizeof(ItemT);
        stream.Read(static_cast<char*>(value.data()), size);
        return value;
    }
    template<typename T, typename ItemT>
    requires std::is_base_of_v<std::vector<ItemT>, T>
    std::vector<ItemT> Read(InputStream& stream) { return Read<std::vector<ItemT>, ItemT, uint32_t>(stream); }
}
