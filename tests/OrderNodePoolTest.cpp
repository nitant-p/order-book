#include "OrderBookSide.h"
#include "OrderNodePool.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <set>
#include <utility>

namespace {

Order makeOrder(uint64_t id, int price = 100, int quantity = 10) {
    return Order{id, Side::BUY, Type::LIMIT, price, quantity};
}

using NodeSlot = std::pair<std::size_t, std::size_t>;

NodeSlot slotOf(const OrderNode* node) {
    return {node->globalChunkIndex, node->localChunkIndex};
}

} // namespace

TEST(OrderNodePoolTest, ConstructorInitializesCapacityAndCounts) {
    OrderNodePool pool(3);

    EXPECT_EQ(pool.capacity(), 3U);
    EXPECT_EQ(pool.activeCount(), 0U);
    EXPECT_EQ(pool.availableCount(), 3U);
    EXPECT_TRUE(pool.hasAvailable());
}

TEST(OrderNodePoolTest, AcquireReturnsInitializedNodeAndUpdatesCounts) {
    OrderNodePool pool(2);
    const Order order = makeOrder(1, 101, 25);

    OrderNode* node = pool.acquire(order);

    ASSERT_NE(node, nullptr);
    EXPECT_TRUE(pool.owns(node));
    EXPECT_EQ(node->order.id, order.id);
    EXPECT_EQ(node->order.side, order.side);
    EXPECT_EQ(node->order.type, order.type);
    EXPECT_EQ(node->order.price, order.price);
    EXPECT_EQ(node->order.quantity, order.quantity);
    EXPECT_EQ(node->prev, nullptr);
    EXPECT_EQ(node->next, nullptr);
    EXPECT_EQ(node->priceLevel, nullptr);
    EXPECT_TRUE(pool.owns(node));
    EXPECT_EQ(pool.activeCount(), 1U);
    EXPECT_EQ(pool.availableCount(), 1U);
    EXPECT_TRUE(pool.hasAvailable());
}

TEST(OrderNodePoolTest, AcquireBeyondInitialCapacityTriggersGrowth) {
    OrderNodePool pool(2);

    OrderNode* first = pool.acquire(makeOrder(1));
    OrderNode* second = pool.acquire(makeOrder(2));
    OrderNode* third = pool.acquire(makeOrder(3));

    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    ASSERT_NE(third, nullptr);
    EXPECT_NE(first, second);
    EXPECT_NE(first, third);
    EXPECT_NE(second, third);
    EXPECT_TRUE(pool.owns(first));
    EXPECT_TRUE(pool.owns(second));
    EXPECT_TRUE(pool.owns(third));
    EXPECT_EQ(pool.capacity(), 4U);
    EXPECT_EQ(pool.activeCount(), 3U);
    EXPECT_EQ(pool.availableCount(), 1U);
    EXPECT_TRUE(pool.hasAvailable());
}

TEST(OrderNodePoolTest, RepeatedAcquireGrowsFromZeroCapacity) {
    OrderNodePool pool(0);

    OrderNode* first = pool.acquire(makeOrder(1));
    OrderNode* second = pool.acquire(makeOrder(2));

    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_NE(first, second);
    EXPECT_TRUE(pool.owns(first));
    EXPECT_TRUE(pool.owns(second));
    EXPECT_EQ(pool.capacity(), 2U);
    EXPECT_EQ(pool.activeCount(), 2U);
    EXPECT_EQ(pool.availableCount(), 0U);
}

TEST(OrderNodePoolTest, ReleaseReturnsNodeToPoolAndClearsLinks) {
    OrderNodePool pool(1);
    OrderNode* node = pool.acquire(makeOrder(1));
    ASSERT_NE(node, nullptr);
    const NodeSlot slot = slotOf(node);

    PriceLevel level;
    OrderNode previous{makeOrder(99), nullptr, node, &level, 999, 0};
    OrderNode next{makeOrder(100), node, nullptr, &level, 1000, 0};
    node->prev = &previous;
    node->next = &next;
    node->priceLevel = &level;

    EXPECT_TRUE(pool.release(node));

    EXPECT_EQ(node->prev, nullptr);
    EXPECT_EQ(node->next, nullptr);
    EXPECT_EQ(node->priceLevel, nullptr);
    EXPECT_EQ(slotOf(node), slot);
    EXPECT_EQ(pool.activeCount(), 0U);
    EXPECT_EQ(pool.availableCount(), 1U);
    EXPECT_TRUE(pool.hasAvailable());
}

TEST(OrderNodePoolTest, ReleaseRejectsNodeNotOwnedByPool) {
    OrderNodePool pool(1);
    OrderNode foreignNode{makeOrder(42), nullptr, nullptr, nullptr, 0, 0};

    EXPECT_FALSE(pool.owns(&foreignNode));
    EXPECT_FALSE(pool.release(&foreignNode));
    EXPECT_EQ(pool.activeCount(), 0U);
    EXPECT_EQ(pool.availableCount(), 1U);
}

TEST(OrderNodePoolTest, ReleaseRejectsNullPointer) {
    OrderNodePool pool(1);

    EXPECT_FALSE(pool.release(nullptr));
    EXPECT_EQ(pool.activeCount(), 0U);
    EXPECT_EQ(pool.availableCount(), 1U);
}

TEST(OrderNodePoolTest, ReleaseRejectsDoubleRelease) {
    OrderNodePool pool(1);
    OrderNode* node = pool.acquire(makeOrder(1));
    ASSERT_NE(node, nullptr);

    EXPECT_TRUE(pool.release(node));
    EXPECT_FALSE(pool.release(node));
    EXPECT_EQ(pool.activeCount(), 0U);
    EXPECT_EQ(pool.availableCount(), 1U);
}

TEST(OrderNodePoolTest, ReleasedNodeCanBeReusedWithFreshOrderAndClearedLinks) {
    OrderNodePool pool(1);
    OrderNode* first = pool.acquire(makeOrder(1, 100, 10));
    ASSERT_NE(first, nullptr);
    const NodeSlot slot = slotOf(first);

    PriceLevel level;
    first->prev = first;
    first->next = first;
    first->priceLevel = &level;

    ASSERT_TRUE(pool.release(first));

    const Order replacement = makeOrder(2, 105, 40);
    OrderNode* second = pool.acquire(replacement);

    ASSERT_EQ(second, first);
    EXPECT_EQ(second->order.id, replacement.id);
    EXPECT_EQ(second->order.price, replacement.price);
    EXPECT_EQ(second->order.quantity, replacement.quantity);
    EXPECT_EQ(second->prev, nullptr);
    EXPECT_EQ(second->next, nullptr);
    EXPECT_EQ(second->priceLevel, nullptr);
    EXPECT_EQ(slotOf(second), slot);
    EXPECT_EQ(pool.activeCount(), 1U);
    EXPECT_EQ(pool.availableCount(), 0U);
}

TEST(OrderNodePoolTest, OwnsReturnsTrueOnlyForNodesInsidePoolStorage) {
    OrderNodePool pool(2);
    OrderNode* first = pool.acquire(makeOrder(1));
    OrderNode* second = pool.acquire(makeOrder(2));
    OrderNode foreignNode{makeOrder(3), nullptr, nullptr, nullptr, 0, 0};

    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_TRUE(pool.owns(first));
    EXPECT_TRUE(pool.owns(second));
    EXPECT_FALSE(pool.owns(&foreignNode));
    EXPECT_FALSE(pool.owns(nullptr));
}

TEST(OrderNodePoolTest, PoolIndexesAreStableAndUniqueAcrossAllSlotsAfterGrowth) {
    OrderNodePool pool(3);
    std::array<OrderNode*, 5> nodes{
        pool.acquire(makeOrder(1)),
        pool.acquire(makeOrder(2)),
        pool.acquire(makeOrder(3)),
        pool.acquire(makeOrder(4)),
        pool.acquire(makeOrder(5)),
    };

    std::set<NodeSlot> slots;
    for (OrderNode* node : nodes) {
        ASSERT_NE(node, nullptr);
        EXPECT_TRUE(pool.owns(node));
        slots.insert(slotOf(node));
    }

    EXPECT_EQ(slots.size(), nodes.size());

    for (OrderNode* node : nodes) {
        const NodeSlot slot = slotOf(node);
        ASSERT_TRUE(pool.release(node));
        EXPECT_EQ(slotOf(node), slot);
    }
}

TEST(OrderNodePoolTest, OldNodePointersRemainValidAfterGrowth) {
    OrderNodePool pool(1);
    OrderNode* oldNode = pool.acquire(makeOrder(1, 100, 10));
    ASSERT_NE(oldNode, nullptr);
    const NodeSlot oldSlot = slotOf(oldNode);
    Order* oldOrder = &oldNode->order;

    OrderNode* newNode = pool.acquire(makeOrder(2, 101, 20));

    ASSERT_NE(newNode, nullptr);
    EXPECT_NE(oldNode, newNode);
    EXPECT_EQ(slotOf(oldNode), oldSlot);
    EXPECT_EQ(oldOrder, &oldNode->order);
    EXPECT_EQ(oldNode->order.id, 1U);
    EXPECT_EQ(oldNode->order.price, 100);
    EXPECT_EQ(oldNode->order.quantity, 10);
    EXPECT_TRUE(pool.owns(oldNode));
    EXPECT_TRUE(pool.owns(newNode));
    EXPECT_EQ(pool.capacity(), 2U);
    EXPECT_EQ(pool.activeCount(), 2U);
    EXPECT_EQ(pool.availableCount(), 0U);
}

TEST(OrderNodePoolTest, ReleaseOldChunkNodeAfterGrowthUpdatesCounts) {
    OrderNodePool pool(1);
    OrderNode* oldNode = pool.acquire(makeOrder(1));
    OrderNode* newNode = pool.acquire(makeOrder(2));
    ASSERT_NE(oldNode, nullptr);
    ASSERT_NE(newNode, nullptr);
    ASSERT_NE(oldNode->globalChunkIndex, newNode->globalChunkIndex);

    EXPECT_TRUE(pool.release(oldNode));

    EXPECT_EQ(pool.capacity(), 2U);
    EXPECT_EQ(pool.activeCount(), 1U);
    EXPECT_EQ(pool.availableCount(), 1U);
    EXPECT_TRUE(pool.owns(newNode));
    EXPECT_TRUE(pool.hasAvailable());
}

TEST(OrderNodePoolTest, ReleaseNewChunkNodeAfterGrowthUpdatesCounts) {
    OrderNodePool pool(1);
    OrderNode* oldNode = pool.acquire(makeOrder(1));
    OrderNode* newNode = pool.acquire(makeOrder(2));
    ASSERT_NE(oldNode, nullptr);
    ASSERT_NE(newNode, nullptr);
    ASSERT_NE(oldNode->globalChunkIndex, newNode->globalChunkIndex);

    EXPECT_TRUE(pool.release(newNode));

    EXPECT_EQ(pool.capacity(), 2U);
    EXPECT_EQ(pool.activeCount(), 1U);
    EXPECT_EQ(pool.availableCount(), 1U);
    EXPECT_TRUE(pool.owns(oldNode));
    EXPECT_TRUE(pool.hasAvailable());
}

TEST(OrderNodePoolTest, AcquireAfterReleasingOldChunkNodeReusesThatSlot) {
    OrderNodePool pool(1);
    OrderNode* oldNode = pool.acquire(makeOrder(1));
    OrderNode* newNode = pool.acquire(makeOrder(2));
    ASSERT_NE(oldNode, nullptr);
    ASSERT_NE(newNode, nullptr);
    const NodeSlot oldSlot = slotOf(oldNode);

    ASSERT_TRUE(pool.release(oldNode));
    OrderNode* reused = pool.acquire(makeOrder(3, 103, 30));

    ASSERT_EQ(reused, oldNode);
    EXPECT_EQ(slotOf(reused), oldSlot);
    EXPECT_EQ(reused->order.id, 3U);
    EXPECT_EQ(reused->order.price, 103);
    EXPECT_EQ(reused->order.quantity, 30);
    EXPECT_EQ(pool.activeCount(), 2U);
    EXPECT_EQ(pool.availableCount(), 0U);
}

TEST(OrderNodePoolTest, CountsTrackMultipleGrowthAndReleaseSteps) {
    OrderNodePool pool(2);

    OrderNode* first = pool.acquire(makeOrder(1));
    OrderNode* second = pool.acquire(makeOrder(2));
    EXPECT_EQ(pool.capacity(), 2U);
    EXPECT_EQ(pool.activeCount(), 2U);
    EXPECT_EQ(pool.availableCount(), 0U);
    EXPECT_FALSE(pool.hasAvailable());

    OrderNode* third = pool.acquire(makeOrder(3));
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    ASSERT_NE(third, nullptr);
    EXPECT_EQ(pool.capacity(), 4U);
    EXPECT_EQ(pool.activeCount(), 3U);
    EXPECT_EQ(pool.availableCount(), 1U);
    EXPECT_TRUE(pool.hasAvailable());

    ASSERT_TRUE(pool.release(second));
    EXPECT_EQ(pool.capacity(), 4U);
    EXPECT_EQ(pool.activeCount(), 2U);
    EXPECT_EQ(pool.availableCount(), 2U);

    OrderNode* fourth = pool.acquire(makeOrder(4));
    OrderNode* fifth = pool.acquire(makeOrder(5));
    ASSERT_NE(fourth, nullptr);
    ASSERT_NE(fifth, nullptr);
    EXPECT_EQ(pool.capacity(), 4U);
    EXPECT_EQ(pool.activeCount(), 4U);
    EXPECT_EQ(pool.availableCount(), 0U);

    OrderNode* sixth = pool.acquire(makeOrder(6));
    ASSERT_NE(sixth, nullptr);
    EXPECT_EQ(pool.capacity(), 8U);
    EXPECT_EQ(pool.activeCount(), 5U);
    EXPECT_EQ(pool.availableCount(), 3U);
}
