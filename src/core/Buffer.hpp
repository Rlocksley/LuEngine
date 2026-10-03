#pragma once

#include "Global.hpp"
#include "Allocator.hpp"
#include "Command.hpp"
#include "Constant.hpp"
#include <utility>

namespace Lu
{
    namespace Core
    {
        
        template<typename T>
        struct Buffer
        {
            uint32_t size{0};
            VkBufferUsageFlags usage{0};
            VkBuffer vkBuffer{nullptr};
            VmaAllocation vmaAllocation{nullptr};
            VmaAllocationInfo vmaAllocationInfo{};

            explicit Buffer(uint32_t size, VkBufferUsageFlags usage):
            size(size),
            usage(usage)
            {                
                VkBufferCreateInfo createInfo{};
                createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
                createInfo.size = sizeof(T) * size;
                createInfo.usage = usage;

                VmaAllocationCreateInfo allocInfo{};
                allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

                LU_CHECK_VULKAN
                (vmaCreateBuffer
                (vmaAllocator,
                &createInfo,
                &allocInfo,
                &vkBuffer,
                &vmaAllocation,
                &vmaAllocationInfo),
                "createBuffer",
                "vmaCreateBuffer")
            }

            ~Buffer(){
                if(vkBuffer != nullptr && vmaAllocation != nullptr){
                    vmaDestroyBuffer(vmaAllocator, vkBuffer, vmaAllocation);
                }
            }

            Buffer(const Buffer&) = delete;
            Buffer& operator=(const Buffer&) = delete;

            Buffer(Buffer&& other) noexcept :
                size(other.size),
                vkBuffer(other.vkBuffer),
                usage(other.usage),
                vmaAllocation(other.vmaAllocation),
                vmaAllocationInfo(other.vmaAllocationInfo){
                other.vkBuffer = nullptr;
                other.vmaAllocation = nullptr;
            }

            Buffer& operator=(Buffer&& other) noexcept{
                if(this != &other){
                    if(vkBuffer != nullptr && vmaAllocation != nullptr){
                        vmaDestroyBuffer(vmaAllocator, vkBuffer, vmaAllocation);
                    }
                    size = other.size;
                    vkBuffer = other.vkBuffer;
                    usage = other.usage;
                    vmaAllocation = other.vmaAllocation;
                    vmaAllocationInfo = other.vmaAllocationInfo;
                    other.vkBuffer = nullptr;
                    other.vmaAllocation = nullptr;
                }
                return *this;
            }
       };

        template<typename T>
        struct BufferRanges : public Buffer<T>
        {
            struct Range {
                uint32_t offset;
                uint32_t size;
            };

            uint32_t currentTop{0};
            std::vector<Range> freeRanges;

            explicit BufferRanges(uint32_t size, VkBufferUsageFlags usage)
                : Buffer<T>(size, usage) {}

            Range allocate(uint32_t count){
                LU_ASSERT(count > 0, "BufferRanges", "allocate", "Range size must be greater than zero.")

                for(size_t i = 0; i < freeRanges.size(); ++i){
                    if(freeRanges[i].size < count){
                        continue;
                    }

                    const Range result{freeRanges[i].offset, count};
                    if(freeRanges[i].size == count){
                        freeRanges.erase(freeRanges.begin() + static_cast<ptrdiff_t>(i));
                    } else {
                        freeRanges[i].offset += count;
                        freeRanges[i].size -= count;
                    }
                    return result;
                }

                LU_ASSERT(currentTop <= this->size && count <= this->size - currentTop,
                    "BufferRanges", "allocate", "Buffer range capacity exceeded.")
                const Range result{currentTop, count};
                currentTop += count;
                return result;
            }

            void free(Range range){
                LU_ASSERT(range.size > 0 && range.offset <= this->size &&
                    range.size <= this->size - range.offset,
                    "BufferRanges", "free", "Invalid buffer range.")

                freeRanges.push_back(range);
                std::sort(freeRanges.begin(), freeRanges.end(),
                    [](const Range& left, const Range& right){ return left.offset < right.offset; });

                std::vector<Range> merged;
                merged.reserve(freeRanges.size());
                for(const Range candidate : freeRanges){
                    if(!merged.empty() &&
                        merged.back().offset + merged.back().size == candidate.offset){
                        merged.back().size += candidate.size;
                    } else {
                        merged.push_back(candidate);
                    }
                }
                freeRanges = std::move(merged);
            }
        };

       

        template<typename T>
        struct BufferInterface
        {
            uint32_t size{0};
            VkBufferUsageFlags usage{0};
            VkBuffer vkBuffer{nullptr};
            VmaAllocation vmaAllocation{nullptr};
            VmaAllocationInfo vmaAllocationInfo{};
            T* pMemory{nullptr};

            explicit BufferInterface(uint32_t size, VkBufferUsageFlags usage):
            size(size),
            usage(usage)
            {
                VkBufferCreateInfo createInfo{};
                createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
                createInfo.size = sizeof(T) * size;
                createInfo.usage = usage;

                VmaAllocationCreateInfo allocInfo{};
                allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
                allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
                                  VMA_ALLOCATION_CREATE_MAPPED_BIT;
                allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

                LU_CHECK_VULKAN
                (vmaCreateBuffer
                (vmaAllocator,
                &createInfo,
                &allocInfo,
                &vkBuffer,
                &vmaAllocation,
                &vmaAllocationInfo),
                "BufferInterface",
                "vmaCreateBuffer")

                pMemory = static_cast<T*>(vmaAllocationInfo.pMappedData);
            }

            ~BufferInterface(){
                if(vkBuffer != nullptr && vmaAllocation != nullptr){
                    vmaDestroyBuffer(vmaAllocator, vkBuffer, vmaAllocation);
                }
            }

            BufferInterface(const BufferInterface&) = delete;
            BufferInterface& operator=(const BufferInterface&) = delete;

            BufferInterface(BufferInterface&& other) noexcept :
                size(other.size),
                usage(other.usage),
                vkBuffer(other.vkBuffer),
                vmaAllocation(other.vmaAllocation),
                vmaAllocationInfo(other.vmaAllocationInfo),
                pMemory(other.pMemory){
                other.vkBuffer = nullptr;
                other.vmaAllocation = nullptr;
                other.pMemory = nullptr;
            }

            BufferInterface& operator=(BufferInterface&& other) noexcept{
                if(this != &other){
                    if(vkBuffer != nullptr && vmaAllocation != nullptr){
                        vmaDestroyBuffer(vmaAllocator, vkBuffer, vmaAllocation);
                    }
                    size = other.size;
                    usage = other.usage;
                    vkBuffer = other.vkBuffer;
                    vmaAllocation = other.vmaAllocation;
                    vmaAllocationInfo = other.vmaAllocationInfo;
                    pMemory = other.pMemory;
                    other.vkBuffer = nullptr;
                    other.vmaAllocation = nullptr;
                    other.pMemory = nullptr;
                }
                return *this;
            }

            template<typename Cmd>
            void recordCopyTo(const Buffer<T>& buffer, uint32_t copySize, const Cmd& command,
                uint32_t sourceOffset = 0, uint32_t destinationOffset = 0){
                VkBufferCopy copyRegion{};
                copyRegion.srcOffset = sizeof(T) * sourceOffset;
                copyRegion.dstOffset = sizeof(T) * destinationOffset;
                copyRegion.size = sizeof(T) * copySize;
                vkCmdCopyBuffer(
                    command.vkCommandBuffer,
                    vkBuffer,
                    buffer.vkBuffer,
                    1,
                    &copyRegion
                );
            }

            template<typename Cmd>
            void recordCopyFrom(const Buffer<T>& buffer, uint32_t copySize, const Cmd& command){
                VkBufferCopy copyRegion{};
                copyRegion.size = sizeof(T) * copySize;
                vkCmdCopyBuffer(
                    command.vkCommandBuffer,
                    buffer.vkBuffer,
                    vkBuffer,
                    1,
                    &copyRegion
                );
            }
        };

        class IBufferResource {
        public:
            virtual ~IBufferResource() = default;
            virtual VkBuffer getVkBuffer(uint32_t frameIndex) const = 0;
            virtual VkBufferUsageFlags getVkBufferUsage() const = 0;
        };

        template<typename T>
        class BufferGpu final : public IBufferResource {
        public:
            explicit BufferGpu(uint32_t size, VkBufferUsageFlags usage)
                : buffers(makeBuffers(size, usage)) {}

            BufferGpu(const BufferGpu&) = delete;
            BufferGpu& operator=(const BufferGpu&) = delete;
            BufferGpu(BufferGpu&&) noexcept = default;
            BufferGpu& operator=(BufferGpu&&) noexcept = default;

            VkBuffer getVkBuffer(uint32_t frameIndex) const override { return buffers[frameIndex].vkBuffer; }
            VkBufferUsageFlags getVkBufferUsage() const override { return buffers[0].usage; }

        private:
            static std::vector<Buffer<T>> makeBuffers(
                uint32_t size,
                VkBufferUsageFlags usage
                ) {
                std::vector<Buffer<T>> buffers;
                buffers.reserve(MAX_FRAMES_IN_FLIGHT);
                for(uint32_t frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame){
                    buffers.emplace_back(size, usage);
                }
                return buffers;
            }

            std::vector<Buffer<T>> buffers;
        };

        template<typename T>
        class BufferGpuAddress final : public IBufferResource {
        public:
            explicit BufferGpuAddress(uint32_t size, VkBufferUsageFlags usage,
                                      const void* pNext = nullptr, bool hostVisible = false)
                : size(size), usage(usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT),
                  hostVisible(hostVisible) {
                for(uint32_t frameIndex = 0; frameIndex < MAX_FRAMES_IN_FLIGHT; ++frameIndex){
                    VkBufferCreateInfo createInfo{};
                    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
                    createInfo.pNext = pNext;
                    createInfo.size = sizeof(T) * size;
                    createInfo.usage = this->usage;
                    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

                    VmaAllocationCreateInfo allocationInfo{};
                    allocationInfo.usage = hostVisible
                        ? VMA_MEMORY_USAGE_AUTO_PREFER_HOST
                        : VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
                    if(hostVisible){
                        allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                               VMA_ALLOCATION_CREATE_MAPPED_BIT;
                        allocationInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
                    }

                    LU_CHECK_VULKAN(vmaCreateBuffer(
                        vmaAllocator,
                        &createInfo,
                        &allocationInfo,
                        &buffers[frameIndex].vkBuffer,
                        &buffers[frameIndex].allocation,
                        &buffers[frameIndex].allocationInfo
                    ), "BufferGpuAddress", "vmaCreateBuffer")

                    VkBufferDeviceAddressInfo addressInfo{};
                    addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
                    addressInfo.buffer = buffers[frameIndex].vkBuffer;
                    buffers[frameIndex].address = vkGetBufferDeviceAddress(vkDevice, &addressInfo);
                }
            }

            ~BufferGpuAddress(){
                for(auto& buffer : buffers){
                    if(buffer.vkBuffer != VK_NULL_HANDLE){
                        vmaDestroyBuffer(vmaAllocator, buffer.vkBuffer, buffer.allocation);
                    }
                }
            }

            BufferGpuAddress(const BufferGpuAddress&) = delete;
            BufferGpuAddress& operator=(const BufferGpuAddress&) = delete;
            BufferGpuAddress(BufferGpuAddress&&) = delete;
            BufferGpuAddress& operator=(BufferGpuAddress&&) = delete;

            VkBuffer getVkBuffer(uint32_t frameIndex) const override { return buffers[frameIndex].vkBuffer; }
            VkBufferUsageFlags getVkBufferUsage() const override { return usage; }
            VkDeviceAddress getDeviceAddress(uint32_t frameIndex) const { return buffers[frameIndex].address; }

            void write(uint32_t frameIndex, uint32_t index, const T& value){
                LU_ASSERT(hostVisible && buffers[frameIndex].allocationInfo.pMappedData != nullptr,
                    "BufferGpuAddress", "write", "Buffer is not host visible.")
                LU_ASSERT(index < size, "BufferGpuAddress", "write", "Index exceeds buffer capacity.")
                auto* mapped = static_cast<T*>(buffers[frameIndex].allocationInfo.pMappedData);
                mapped[index] = value;
                LU_CHECK_VULKAN(vmaFlushAllocation(vmaAllocator, buffers[frameIndex].allocation,
                    sizeof(T) * index, sizeof(T)), "BufferGpuAddress", "vmaFlushAllocation")
            }

            void writeAll(uint32_t frameIndex, const T* data, uint32_t count){
                LU_ASSERT(hostVisible && buffers[frameIndex].allocationInfo.pMappedData != nullptr,
                    "BufferGpuAddress", "writeAll", "Buffer is not host visible.")
                LU_ASSERT(count <= size, "BufferGpuAddress", "writeAll", "Data exceeds buffer capacity.")
                if(count == 0){
                    return;
                }
                std::memcpy(buffers[frameIndex].allocationInfo.pMappedData, data, sizeof(T) * count);
                LU_CHECK_VULKAN(vmaFlushAllocation(vmaAllocator, buffers[frameIndex].allocation,
                    0, sizeof(T) * count), "BufferGpuAddress", "vmaFlushAllocation")
            }

        private:
            struct FrameBuffer {
                VkBuffer vkBuffer{VK_NULL_HANDLE};
                VmaAllocation allocation{nullptr};
                VmaAllocationInfo allocationInfo{};
                VkDeviceAddress address{0};
            };

            uint32_t size;
            VkBufferUsageFlags usage;
            bool hostVisible;
            std::array<FrameBuffer, MAX_FRAMES_IN_FLIGHT> buffers{};
        };

        template<typename T>
        class BufferGpuIndexed final : public IBufferResource {
        public:
                        explicit BufferGpuIndexed(uint32_t size, VkBufferUsageFlags usage)
                                : uploadBuffer(1, VK_BUFFER_USAGE_TRANSFER_SRC_BIT), 
                                  buffer(size, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                                  capacity(size) {}

            BufferGpuIndexed(const BufferGpuIndexed&) = delete;
            BufferGpuIndexed& operator=(const BufferGpuIndexed&) = delete;

            VkBuffer getVkBuffer(uint32_t) const override { return buffer.vkBuffer; }
            VkBufferUsageFlags getVkBufferUsage() const override { return buffer.usage; }

            uint32_t allocate(){
                LU_ASSERT(nextIndex < capacity, "BufferGpuIndexed", "allocate", "Indexed buffer capacity exceeded.")
                return nextIndex++;
            }

            template<typename Cmd>
            void upload(uint32_t index, const T& value, const Cmd& command) {
                LU_ASSERT(index < capacity, "IndexedPerFrameGpuBuffer", "upload", "Index exceeds buffer capacity.")
                std::memcpy(uploadBuffer.pMemory, &value, sizeof(T));
                VkBufferCopy copy{};
                copy.srcOffset = 0;
                copy.dstOffset = static_cast<VkDeviceSize>(index) * sizeof(T);
                copy.size = sizeof(T);
                vkCmdCopyBuffer(command.vkCommandBuffer, uploadBuffer.vkBuffer, buffer.vkBuffer, 1, &copy);
            }

        private:
            static std::vector<Buffer<T>> makeBuffers(
                uint32_t size,
                VkBufferUsageFlags usage
                ) {
                std::vector<Buffer<T>> buffers;
                buffers.reserve(MAX_FRAMES_IN_FLIGHT);
                for(uint32_t frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame){
                    buffers.emplace_back(size, usage);
                }
                return buffers;
            }

            Buffer<T> buffer;
            BufferInterface<T> uploadBuffer;
            uint32_t capacity;
            uint32_t nextIndex{0};
        };

        template<typename T>
        struct DualBuffer : public IBufferResource{
            std::array<uint32_t, MAX_FRAMES_IN_FLIGHT> sizeCpuCount{};
            std::array<uint32_t, MAX_FRAMES_IN_FLIGHT> sizeGpuCount{};
            std::array<std::vector<T>, MAX_FRAMES_IN_FLIGHT> bufferCpu;
            std::array<BufferInterface<T>, MAX_FRAMES_IN_FLIGHT> interface;
            std::array<Buffer<T>, MAX_FRAMES_IN_FLIGHT> buffer;
            uint32_t capacity;

            explicit DualBuffer(uint32_t size, VkBufferUsageFlags usage) :
                bufferCpu(makeCpuBuffers(size)),
                interface(makeInterfaces(size)),
                buffer(makeBuffers(size, usage)),
                capacity(size){}

        private:
            template<size_t... Frames>
            static std::array<std::vector<T>, MAX_FRAMES_IN_FLIGHT> makeCpuBuffers(
                uint32_t size,
                std::index_sequence<Frames...>) {
                return {{(static_cast<void>(Frames), std::vector<T>(size))...}};
            }

            static std::array<std::vector<T>, MAX_FRAMES_IN_FLIGHT> makeCpuBuffers(uint32_t size) {
                return makeCpuBuffers(size, std::make_index_sequence<MAX_FRAMES_IN_FLIGHT>{});
            }

            template<size_t... Frames>
            static std::array<BufferInterface<T>, MAX_FRAMES_IN_FLIGHT> makeInterfaces(
                uint32_t size,
                std::index_sequence<Frames...>) {
                return {{(static_cast<void>(Frames), BufferInterface<T>(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT))...}};
            }

            static std::array<BufferInterface<T>, MAX_FRAMES_IN_FLIGHT> makeInterfaces(uint32_t size) {
                return makeInterfaces(size, std::make_index_sequence<MAX_FRAMES_IN_FLIGHT>{});
            }

            template<size_t... Frames>
            static std::array<Buffer<T>, MAX_FRAMES_IN_FLIGHT> makeBuffers(
                uint32_t size,
                VkBufferUsageFlags usage,
                std::index_sequence<Frames...>) {
                return {{(static_cast<void>(Frames), Buffer<T>(size, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT))...}};
            }

            static std::array<Buffer<T>, MAX_FRAMES_IN_FLIGHT> makeBuffers(
                uint32_t size,
                VkBufferUsageFlags usage) {
                return makeBuffers(size, usage, std::make_index_sequence<MAX_FRAMES_IN_FLIGHT>{});
            }

        public:

            VkBuffer getVkBuffer(uint32_t frameIndex) const override {return buffer[frameIndex].vkBuffer;}
            VkBufferUsageFlags getVkBufferUsage() const override {return buffer[0].usage;}


            template<typename Cmd>
            void copyToGpu(const uint32_t frameIndex, const Cmd& command){
                copyToInterface(frameIndex);
                interface[frameIndex].recordCopyTo(buffer[frameIndex], sizeGpuCount[frameIndex], command);
            }

            const uint32_t sizeCpu(uint32_t frameIndex){
                return sizeCpuCount[frameIndex];
            }

            const uint32_t sizeGpu(uint32_t frameIndex){
                return sizeGpuCount[frameIndex];
            }

            void clearCpu(const uint32_t frameIndex){
                sizeCpuCount[frameIndex] = 0;
            }

            void push_back(const uint32_t frameIndex, const T& element){
                LU_ASSERT(sizeCpuCount[frameIndex] < capacity, "DualBuffer", "push_back", "sizeCpu overflow")
                bufferCpu[frameIndex][sizeCpuCount[frameIndex]++] = element;
            }

            private:
            void copyToInterface(const uint32_t frameIndex){
                LU_ASSERT(sizeCpuCount[frameIndex] <= capacity, "DualBuffer", "copyToInterface", "sizeCpu overflow")
                sizeGpuCount[frameIndex] = sizeCpuCount[frameIndex];
                std::memcpy(interface[frameIndex].pMemory, bufferCpu[frameIndex].data(), sizeof(T) * sizeGpuCount[frameIndex]);
                clearCpu(frameIndex);
            }

        };



        template<typename T>
        struct SlotBuffer : public IBufferResource{
            uint32_t sizeGpu;
            uint32_t capacity;
            std::vector<uint32_t> freeSlotIndices;
            std::array<Buffer<T>, MAX_FRAMES_IN_FLIGHT> buffer;

            explicit SlotBuffer(uint32_t size, VkBufferUsageFlags usage) :
                sizeGpu(0),
                capacity(size),
                buffer(makeBuffers(size, usage)){}

        private:
            template<size_t... Frames>
            static std::array<Buffer<T>, MAX_FRAMES_IN_FLIGHT> makeBuffers(
                uint32_t size,
                VkBufferUsageFlags usage,
                std::index_sequence<Frames...>) {
                return {{(static_cast<void>(Frames), Buffer<T>(size, usage))...}};
            }

            static std::array<Buffer<T>, MAX_FRAMES_IN_FLIGHT> makeBuffers(
                uint32_t size,
                VkBufferUsageFlags usage) {
                return makeBuffers(size, usage, std::make_index_sequence<MAX_FRAMES_IN_FLIGHT>{});
            }

        public:

            VkBuffer getVkBuffer(uint32_t frameIndex) const override {return buffer[frameIndex].vkBuffer;}
            VkBufferUsageFlags getVkBufferUsage() const override {return buffer[0].usage;}

            const uint32_t allocate(){
                uint32_t index;
                if(freeSlotIndices.size() > 0){
                    index = freeSlotIndices.back();
                    freeSlotIndices.pop_back();
                } else {
                    LU_ASSERT(sizeGpu < capacity, "SlotBuffer", "allocate", "Exceeded SlotBufferGPU capacity.")
                    index = sizeGpu++;
                }
                return index;
            }

            void free(const uint32_t index){
                freeSlotIndices.push_back(index);
            }

            const uint32_t size() const {
                return sizeGpu;
            }

        };

        template<typename T>
        struct DualSlotBuffer : public IBufferResource{
            uint32_t sizeGpu;
            uint32_t capacity;
            std::vector<uint32_t> freeSlotIndices;
            BufferInterface<T> interface;
            Buffer<T> buffer;

            DualSlotBuffer(uint32_t size, VkBufferUsageFlags usage) :
                sizeGpu(0),
                capacity(size),
                interface(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
                buffer(size, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT){}

            VkBuffer getVkBuffer(uint32_t) const override {return buffer.vkBuffer;}
            VkBufferUsageFlags getVkBufferUsage() const override {return buffer.usage;}

            const uint32_t allocate(){
                uint32_t index;
                if(freeSlotIndices.size() > 0){
                    index = freeSlotIndices.back();
                    freeSlotIndices.pop_back();
                } else {
                    LU_ASSERT(sizeGpu < capacity, "DualSlotBuffer", "allocate", "Exceeded SlotBufferGPU capacity.")
                    index = sizeGpu++;
                }
                return index;
            }

            void free(const uint32_t index){
                freeSlotIndices.push_back(index);
            }

            void add(const T& element){
                const auto index = allocate();
                std::memcpy(interface.pMemory + index, &element, sizeof(T));
            }

            uint32_t size(){
                return sizeGpu;
            }

            template<typename Cmd>
            void recordCopyToGpu(const Cmd& command){
                interface.recordCopyTo(buffer, sizeGpu, command);
            }           

        };

        template<typename T>
        class SlotBufferCpu{
        public:
            explicit SlotBufferCpu(uint32_t size) :
                capacity(size),
                buffer(size),
                sizeCpu(0) {}

            explicit SlotBufferCpu() :
            capacity(0),
            buffer(0),
            sizeCpu(0){}

            void add(const T& element){
                const auto index = allocate();
                buffer[index] = std::move(element);
            }

            T& at(const uint32_t index){
                LU_ASSERT(index < sizeCpu && buffer[index] != nullptr, "SlotBufferCpu", "at", "index or slot is invalid.")
                return buffer[index];
            }

            const T& at(const uint32_t index) const {
                LU_ASSERT(index < sizeCpu && buffer[index] != nullptr, "SlotBufferCpu", "at", "index or slot is invalid.")
                return buffer[index];
            }

            void free(const uint32_t index){
                buffer[index] = T{};
                freeSlotIndices.push_back(index);
            }

            uint32_t size() const { return sizeCpu; }

        private:
            uint32_t capacity;
            uint32_t sizeCpu;
            std::vector<uint32_t> freeSlotIndices;
            std::vector<T> buffer;

            const uint32_t allocate(){
                uint32_t index;
                if(freeSlotIndices.size() > 0){
                    index = freeSlotIndices.back();
                    freeSlotIndices.pop_back();
                } else {
                    LU_ASSERT(sizeCpu < capacity, "SlotBufferCpu", "allocate", "Exceeded SlotBufferCpu capacity.")
                    index = sizeCpu++;
                }
                return index;
            }


        };
}
}