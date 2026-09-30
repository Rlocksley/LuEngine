#pragma once
#include "TripleBuffer.hpp"
#include "Channel.hpp"
#include "instance/TransformInstance.hpp"

namespace Lu{
    namespace Core{
        inline TripleBuffer<std::vector<EcsRequest::UpdateTransform>>& GetTransformTripleBuffer(){
        static TripleBuffer<std::vector<EcsRequest::UpdateTransform>> transforms;
        return transforms;
    }
    }
}