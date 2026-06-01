#pragma once

#include <cstddef>
#include <memory>
#include <vector>

struct Order;
struct OrderNode;

class OrderNodePool {
public:
    explicit OrderNodePool(std::size_t capacity);
    ~OrderNodePool();

    OrderNodePool(const OrderNodePool&) = delete;
    OrderNodePool& operator=(const OrderNodePool&) = delete;

    OrderNodePool(OrderNodePool&&) = delete;
    OrderNodePool& operator=(OrderNodePool&&) = delete;

    OrderNode* acquire(const Order& order);
    bool release(OrderNode* node);

    bool owns(const OrderNode* node) const noexcept;
    bool hasAvailable() const noexcept;

    std::size_t capacity() const noexcept;
    std::size_t activeCount() const noexcept;
    std::size_t availableCount() const noexcept;

private:
    void resetNode(OrderNode& node) noexcept;
    void doublePoolSize();

    std::vector<std::unique_ptr<OrderNode[]>> chunks_;
    std::vector<std::size_t> chunkSizes_;
    std::vector<OrderNode*> freeNodes_;
    std::vector<std::vector<bool>> inUseByChunk_;
    std::size_t totalCapacity_ = 0;
    std::size_t activeCount_ = 0;
    std::size_t chunkTotal_ = 0;
};
