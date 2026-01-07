#pragma once

#include <DataImage.hpp>

#include <string>

Data::Image LoadImage(const std::string& path, int channels = 3, Data::Image::ColorSpace colorSpace = Data::Image::ColorSpace::Linear);
Data::Image LoadHDRI(const std::string& path);