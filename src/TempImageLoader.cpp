#include <TempImageLoader.hpp>

#include "Provider.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <float16_t.hpp>
using numeric::float16_t;

#include <memory>
#include <cstdint>

static vk::Format convertResFormat(int channels, Data::Image::CompressionMode compressionMode, Data::Image::ColorSpace colorSpace) {
    switch(channels) {
        case 1:
            switch(compressionMode) {
            case Data::Image::CompressionMode::Uncompressed: return vk::Format::eR8Unorm;
            case Data::Image::CompressionMode::BC4: return vk::Format::eBc4UnormBlock;
            default: throw std::runtime_error("Unexpected channel/compression combo");
        }
        case 2:
            switch(compressionMode) {
            case Data::Image::CompressionMode::Uncompressed: return vk::Format::eR8G8Unorm;
            case Data::Image::CompressionMode::BC5: return vk::Format::eBc5UnormBlock;
            default: throw std::runtime_error("Unexpected channel/compression combo");
        }
        case 3:
            switch(compressionMode) {
            case Data::Image::CompressionMode::Uncompressed: return colorSpace == Data::Image::ColorSpace::Linear ? vk::Format::eR8G8B8A8Unorm : vk::Format::eR8G8B8A8Srgb;
            default: throw std::runtime_error("Unexpected channel/compression combo");
        }
        case 4:
            switch(compressionMode) {
            case Data::Image::CompressionMode::Uncompressed: return colorSpace == Data::Image::ColorSpace::Linear ? vk::Format::eR8G8B8A8Unorm : vk::Format::eR8G8B8A8Srgb;
            case Data::Image::CompressionMode::BC7: return colorSpace == Data::Image::ColorSpace::Linear ? vk::Format::eBc7UnormBlock : vk::Format::eBc7SrgbBlock;
            default: throw std::runtime_error("Unexpected channel/compression combo");
        }
        default: throw std::runtime_error("Unexpected channel count");
    }
}

dvk::Texture::Ref CreateTextureFromImage(const std::string& label, const Data::Image& image) {
    auto texture = dvk::Texture::Create(GetDevice())
        .SetLabel(label)
        .SetUsage(vk::ImageUsageFlagBits::eSampled)
        .SetFormat(convertResFormat(image.channels, image.compressionMode, image.colorSpace))
        .SetExtent({ static_cast<uint32_t>(image.width), static_cast<uint32_t>(image.height), 1 })
        .EnableWriteWithStaging()
        .Build();

    if (image.channels == 3) {
        assert(image.compressionMode == Data::Image::CompressionMode::Uncompressed);
        // Convert RGB to RGBA
        unsigned char* rgb = image.data.get();
        unsigned char* rgba = new unsigned char[image.width * image.height * 4];
        for (unsigned long i = 0; i < image.width * image.height; i++) {
            rgba[i * 4 + 0] = rgb[i * 3 + 0];
            rgba[i * 4 + 1] = rgb[i * 3 + 1];
            rgba[i * 4 + 2] = rgb[i * 3 + 2];
            rgba[i * 4 + 3] = 255;
        }
        texture->WriteWithStaging(GetCommandEncoder(), rgba, image.width * image.height * 4);
        delete[] rgba;
    } else {
        texture->WriteWithStaging(GetCommandEncoder(), image.data.get(), image.size);
    }
    return texture;
}

Data::Image LoadImage(const std::string& path, int channels, Data::Image::ColorSpace colorSpace) {
    Data::Image image;

    stbi_uc* data;
    data = stbi_load(
        path.c_str(),
        &image.width,
        &image.height,
        &image.channels,
        channels
    );
    if (data == nullptr) throw std::runtime_error("stbi failed to decode image!");
    if (channels != 0) image.channels = channels;

    image.data = std::shared_ptr<uint8_t>(data, [](void* ptr) { stbi_image_free(ptr); });
    image.bytesPerSubpixel = sizeof(uint8_t);
    image.size = image.width * image.height * image.channels * image.bytesPerSubpixel;
    image.compressionMode = Data::Image::CompressionMode::Uncompressed;
    image.colorSpace = colorSpace;

    return image;
}

Data::Image LoadHDRI(const std::string& path) {
    Data::Image image;

    float* data;
    stbi_set_flip_vertically_on_load(true);
    data = stbi_loadf(
        path.c_str(),
        &image.width,
        &image.height,
        &image.channels,
        4
    );
    stbi_set_flip_vertically_on_load(false);
    if (data == nullptr) throw std::runtime_error("stbi failed to decode image!");

    image.channels = 4;
    image.bytesPerSubpixel = sizeof(float16_t);
    image.size = image.width * image.height * image.channels * image.bytesPerSubpixel;
    image.compressionMode = Data::Image::CompressionMode::Uncompressed;
    image.colorSpace = Data::Image::ColorSpace::Linear;

    image.data = std::shared_ptr<uint8_t>((uint8_t*)(new float16_t[4 * image.width * image.height]), [](void* ptr) { delete[] (float16_t*)ptr; });
    auto* halfFloatData = (float16_t*)image.data.get();
    for (int i = 0; i < 4 * image.width * image.height; i++) {
        halfFloatData[i] = std::min<float>(data[i], (float)std::numeric_limits<float16_t>::max());
    }

    stbi_image_free(data);

    return image;
}
