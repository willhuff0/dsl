#pragma once

#include <dvk/dvk.hpp>

class BindingProvider {
public:
    virtual ~BindingProvider() = default;

    virtual dvk::BindSetLayout::Ref GetLayout() = 0;
    virtual void Bind(uint32_t binding, dvk::CommandBuffer& commandBuffer, uint32_t frameIndex) = 0;
};