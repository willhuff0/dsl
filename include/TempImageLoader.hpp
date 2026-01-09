#pragma once

#include <DataImage.hpp>

#include <dvk/dvk.hpp>

#include <string>

dvk::Texture::Ref CreateTextureFromImage(const std::string& label, const Data::Image& image);

Data::Image LoadImage(const std::string& path, int channels = 0, Data::Image::ColorSpace colorSpace = Data::Image::ColorSpace::Linear);
Data::Image LoadHDRI(const std::string& path);