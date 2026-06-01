#include "OrderBookSide.h"
#include "TestHelpers.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

namespace {

using test_helpers::expectLevelIntegrity;
using test_helpers::expectActiveOrdersReferenceTheirLevels;
using test_helpers::levelIds;

class OrderBookSideBuyTest : public ::testing::Test {
protected:
    OrderNodePool pool{1'000};
    OrderBookSide side{Side::BUY, pool};
};

class OrderBookSideSellTest : public ::testing::Test {
protected:
    OrderNodePool pool{1'000};
    OrderBookSide side{Side::SELL, pool};
};

} // namespace

TEST_F(OrderBookSideBuyTest, ConstructorSetsSideAndStartsEmpty) {
    EXPECT_EQ(side.side(), Side::BUY);
    EXPECT_TRUE(side.empty());
    EXPECT_EQ(side.priceLevelCount(), 0U);
    EXPECT_EQ(side.orderCount(), 0U);
    EXPECT_FALSE(side.bestPrice().has_value());
    EXPECT_EQ(side.getBestOrder(), nullptr);
}

TEST_F(OrderBookSideBuyTest, AddOrderCreatesPriceLevelAndIndexEntry) {
    side.addOrder(1, Side::BUY, Type::LIMIT, 100, 10);

    EXPECT_FALSE(side.empty());
    EXPECT_EQ(side.priceLevelCount(), 1U);
    EXPECT_EQ(side.orderCount(), 1U);
    EXPECT_TRUE(side.doesOrderExist(1));
    EXPECT_TRUE(side.doesLevelExist(100));

    const Order* order = side.findOrder(1);
    ASSERT_NE(order, nullptr);
    EXPECT_EQ(order->price, 100);
    EXPECT_EQ(order->quantity, 10);

    expectLevelIntegrity(side, 100, {1}, 10U);
}

TEST(OrderBookSidePoolGrowthTest, AddOrderGrowsPoolAndKeepsExistingOrderPointersValid) {
    OrderNodePool pool{1};
    OrderBookSide side{Side::BUY, pool};

    side.addOrder(1, Side::BUY, Type::LIMIT, 100, 10);
    const Order* firstOrder = side.findOrder(1);
    ASSERT_NE(firstOrder, nullptr);

    side.addOrder(2, Side::BUY, Type::LIMIT, 100, 20);
    side.addOrder(3, Side::BUY, Type::LIMIT, 100, 30);

    EXPECT_EQ(pool.capacity(), 4U);
    EXPECT_EQ(pool.activeCount(), 3U);
    EXPECT_EQ(pool.availableCount(), 1U);
    EXPECT_EQ(side.findOrder(1), firstOrder);
    expectLevelIntegrity(side, 100, {1, 2, 3}, 60U);

    EXPECT_TRUE(side.deleteOrderById(2));

    EXPECT_EQ(pool.activeCount(), 2U);
    EXPECT_EQ(pool.availableCount(), 2U);
    expectLevelIntegrity(side, 100, {1, 3}, 40U);
}

TEST_F(OrderBookSideBuyTest, DuplicateIdAndMismatchedSideAreIgnored) {
    side.addOrder(1, Side::BUY, Type::LIMIT, 100, 10);
    side.addOrder(1, Side::BUY, Type::LIMIT, 101, 20);
    side.addOrder(2, Side::SELL, Type::LIMIT, 100, 5);

    EXPECT_EQ(side.orderCount(), 1U);
    EXPECT_EQ(side.priceLevelCount(), 1U);
    EXPECT_EQ(side.bestPrice(), std::optional<int>(100));
    expectLevelIntegrity(side, 100, {1}, 10U);
}

TEST_F(OrderBookSideBuyTest, AddOrderOverloadUsesOrderFieldsAndMaintainsFifo) {
    Order a{1, Side::BUY, Type::LIMIT, 100, 5};
    Order b{2, Side::BUY, Type::LIMIT, 100, 7};
    side.addOrder(a);
    side.addOrder(b);

    expectLevelIntegrity(side, 100, {1, 2}, 12U);
    EXPECT_EQ(side.getOrderIdsAtPrice(100), std::vector<uint64_t>({1, 2}));
}

TEST_F(OrderBookSideBuyTest, BestPriceAndBestOrderAreHighestPriceAndFrontOfQueue) {
    side.addOrder(1, Side::BUY, Type::LIMIT, 100, 5);
    side.addOrder(2, Side::BUY, Type::LIMIT, 101, 7);
    side.addOrder(3, Side::BUY, Type::LIMIT, 101, 9);

    EXPECT_EQ(side.bestPrice(), std::optional<int>(101));

    const Order* best = side.getBestOrder();
    ASSERT_NE(best, nullptr);
    EXPECT_EQ(best->id, 2U);
    EXPECT_EQ(best->quantity, 7);
}

TEST_F(OrderBookSideSellTest, BestPriceForSellIsLowestPrice) {
    side.addOrder(10, Side::SELL, Type::LIMIT, 103, 4);
    side.addOrder(11, Side::SELL, Type::LIMIT, 102, 6);

    EXPECT_EQ(side.bestPrice(), std::optional<int>(102));
    const Order* best = side.getBestOrder();
    ASSERT_NE(best, nullptr);
    EXPECT_EQ(best->id, 11U);
}

TEST_F(OrderBookSideBuyTest, DeleteMissingOrderReturnsFalseWithoutMutation) {
    side.addOrder(1, Side::BUY, Type::LIMIT, 100, 10);

    EXPECT_FALSE(side.deleteOrderById(999));
    expectLevelIntegrity(side, 100, {1}, 10U);
    EXPECT_EQ(side.orderCount(), 1U);
}

TEST_F(OrderBookSideBuyTest, DeleteHeadMiddleAndTailKeepLinksAndStatsConsistent) {
    side.addOrder(1, Side::BUY, Type::LIMIT, 100, 10);
    side.addOrder(2, Side::BUY, Type::LIMIT, 100, 20);
    side.addOrder(3, Side::BUY, Type::LIMIT, 100, 30);

    EXPECT_TRUE(side.deleteOrderById(1));
    expectLevelIntegrity(side, 100, {2, 3}, 50U);

    EXPECT_TRUE(side.deleteOrderById(2));
    expectLevelIntegrity(side, 100, {3}, 30U);

    EXPECT_TRUE(side.deleteOrderById(3));
    EXPECT_EQ(side.findLevel(100), nullptr);
    EXPECT_TRUE(side.empty());
}

TEST_F(OrderBookSideBuyTest, ReduceOrderQuantityHandlesPartialAndExactFill) {
    side.addOrder(1, Side::BUY, Type::LIMIT, 100, 10);
    side.addOrder(2, Side::BUY, Type::LIMIT, 100, 6);

    EXPECT_TRUE(side.reduceOrderQuantity(1, 4));
    const Order* first = side.findOrder(1);
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->quantity, 6);
    expectLevelIntegrity(side, 100, {1, 2}, 12U);

    EXPECT_TRUE(side.reduceOrderQuantity(1, 6));
    EXPECT_FALSE(side.doesOrderExist(1));
    expectLevelIntegrity(side, 100, {2}, 6U);
}

TEST_F(OrderBookSideBuyTest, ReduceOrderQuantityRejectsMissingOrTooLargeDelta) {
    side.addOrder(1, Side::BUY, Type::LIMIT, 100, 5);

    EXPECT_FALSE(side.reduceOrderQuantity(999, 1));
    EXPECT_FALSE(side.reduceOrderQuantity(1, 6));

    const Order* order = side.findOrder(1);
    ASSERT_NE(order, nullptr);
    EXPECT_EQ(order->quantity, 5);
    expectLevelIntegrity(side, 100, {1}, 5U);
}

TEST_F(OrderBookSideBuyTest, RemoveOrderIfFilledHandlesMissingOpenAndFilledOrders) {
    side.removeOrderIfFilled(999);
    EXPECT_TRUE(side.empty());

    side.addOrder(1, Side::BUY, Type::LIMIT, 100, 5);
    side.removeOrderIfFilled(1);

    EXPECT_TRUE(side.doesOrderExist(1));
    expectLevelIntegrity(side, 100, {1}, 5U);

    side.addOrder(2, Side::BUY, Type::LIMIT, 101, 0);
    side.removeOrderIfFilled(2);

    EXPECT_FALSE(side.doesOrderExist(2));
    EXPECT_FALSE(side.doesLevelExist(101));
    expectLevelIntegrity(side, 100, {1}, 5U);
}

TEST_F(OrderBookSideBuyTest, ModifyOrderRejectsInvalidCases) {
    side.addOrder(1, Side::BUY, Type::LIMIT, 100, 5);

    EXPECT_FALSE(side.modifyOrder(Order{999, Side::BUY, Type::LIMIT, 100, 5}, true));
    EXPECT_FALSE(side.modifyOrder(Order{1, Side::BUY, Type::LIMIT, 100, 0}, true));

    expectLevelIntegrity(side, 100, {1}, 5U);
}

TEST_F(OrderBookSideBuyTest, ModifyReduceSamePriceKeepsQueuePosition) {
    side.addOrder(1, Side::BUY, Type::LIMIT, 100, 8);
    side.addOrder(2, Side::BUY, Type::LIMIT, 100, 5);

    EXPECT_TRUE(side.modifyOrder(Order{1, Side::BUY, Type::LIMIT, 100, 3}, false));

    expectLevelIntegrity(side, 100, {1, 2}, 8U);
    const Order* order = side.findOrder(1);
    ASSERT_NE(order, nullptr);
    EXPECT_EQ(order->quantity, 3);
}

TEST_F(OrderBookSideBuyTest, ModifyIncreaseSamePriceMovesOrderToBackWhenQueuePriorityLost) {
    side.addOrder(1, Side::BUY, Type::LIMIT, 100, 5);
    side.addOrder(2, Side::BUY, Type::LIMIT, 100, 5);

    EXPECT_TRUE(side.modifyOrder(Order{1, Side::BUY, Type::LIMIT, 100, 9}, true));

    expectLevelIntegrity(side, 100, {2, 1}, 14U);
}

TEST_F(OrderBookSideBuyTest, ModifyPriceChangeMovesOrderToNewLevelAndBackOfExistingQueue) {
    side.addOrder(1, Side::BUY, Type::LIMIT, 100, 5);
    side.addOrder(2, Side::BUY, Type::LIMIT, 101, 4);
    side.addOrder(3, Side::BUY, Type::LIMIT, 101, 3);

    EXPECT_TRUE(side.modifyOrder(Order{1, Side::BUY, Type::LIMIT, 101, 7}, true));

    EXPECT_EQ(side.findLevel(100), nullptr);
    expectLevelIntegrity(side, 101, {2, 3, 1}, 14U);
}

TEST_F(OrderBookSideBuyTest, OrderNodesKeepPriceLevelPointersThroughSideLifecycle) {
    side.addOrder(1, Side::BUY, Type::LIMIT, 100, 10);
    side.addOrder(2, Side::BUY, Type::LIMIT, 100, 20);
    side.addOrder(3, Side::BUY, Type::LIMIT, 101, 30);
    expectActiveOrdersReferenceTheirLevels(side);

    EXPECT_TRUE(side.reduceOrderQuantity(1, 4));
    expectLevelIntegrity(side, 100, {1, 2}, 26U);
    expectActiveOrdersReferenceTheirLevels(side);

    EXPECT_TRUE(side.modifyOrder(Order{1, Side::BUY, Type::LIMIT, 100, 3}, false));
    expectLevelIntegrity(side, 100, {1, 2}, 23U);
    expectActiveOrdersReferenceTheirLevels(side);

    EXPECT_TRUE(side.modifyOrder(Order{2, Side::BUY, Type::LIMIT, 101, 7}, true));
    expectLevelIntegrity(side, 100, {1}, 3U);
    expectLevelIntegrity(side, 101, {3, 2}, 37U);
    expectActiveOrdersReferenceTheirLevels(side);

    EXPECT_TRUE(side.deleteOrderById(3));
    expectLevelIntegrity(side, 101, {2}, 7U);
    expectActiveOrdersReferenceTheirLevels(side);

    EXPECT_TRUE(side.modifyOrder(Order{2, Side::BUY, Type::LIMIT, 102, 5}, true));
    EXPECT_EQ(side.findLevel(101), nullptr);
    expectLevelIntegrity(side, 102, {2}, 5U);
    expectActiveOrdersReferenceTheirLevels(side);
}

TEST_F(OrderBookSideBuyTest, GetDepthReturnsDescendingForBuyAndRespectsLimit) {
    side.addOrder(1, Side::BUY, Type::LIMIT, 99, 2);
    side.addOrder(2, Side::BUY, Type::LIMIT, 100, 3);
    side.addOrder(3, Side::BUY, Type::LIMIT, 101, 4);

    const std::vector<LevelSnapshot> depth = side.getDepth(2);
    ASSERT_EQ(depth.size(), 2U);

    EXPECT_EQ(depth[0].price, 101);
    EXPECT_EQ(depth[0].totalQuantity, 4U);
    EXPECT_EQ(depth[0].orderCount, 1U);

    EXPECT_EQ(depth[1].price, 100);
    EXPECT_EQ(depth[1].totalQuantity, 3U);
    EXPECT_EQ(depth[1].orderCount, 1U);
}

TEST_F(OrderBookSideSellTest, GetDepthReturnsAscendingForSellAndHandlesZeroLevels) {
    side.addOrder(1, Side::SELL, Type::LIMIT, 104, 2);
    side.addOrder(2, Side::SELL, Type::LIMIT, 103, 3);
    side.addOrder(3, Side::SELL, Type::LIMIT, 103, 5);

    const std::vector<LevelSnapshot> depth = side.getDepth(10);
    ASSERT_EQ(depth.size(), 2U);
    EXPECT_EQ(depth[0].price, 103);
    EXPECT_EQ(depth[0].totalQuantity, 8U);
    EXPECT_EQ(depth[0].orderCount, 2U);
    EXPECT_EQ(depth[1].price, 104);

    EXPECT_TRUE(side.getDepth(0).empty());
}

TEST_F(OrderBookSideBuyTest, OrderAndLevelStatsHelpersMatchStoredState) {
    side.addOrder(1, Side::BUY, Type::LIMIT, 100, 4);
    side.addOrder(2, Side::BUY, Type::LIMIT, 100, 6);
    side.addOrder(3, Side::BUY, Type::LIMIT, 101, 9);

    EXPECT_EQ(side.orderCount(), 3U);
    EXPECT_EQ(side.orderCountAtPrice(100), 2U);
    EXPECT_EQ(side.orderCountAtPrice(999), 0U);
    EXPECT_EQ(side.volumeAtPrice(100), 10U);
    EXPECT_EQ(side.volumeAtPrice(999), 0U);
}

TEST_F(OrderBookSideBuyTest, GetAllOrderIdsContainsAllActiveIds) {
    side.addOrder(10, Side::BUY, Type::LIMIT, 100, 1);
    side.addOrder(7, Side::BUY, Type::LIMIT, 100, 1);
    side.addOrder(21, Side::BUY, Type::LIMIT, 101, 1);

    std::vector<uint64_t> ids = side.getAllOrderIds();
    std::sort(ids.begin(), ids.end());

    EXPECT_EQ(ids, std::vector<uint64_t>({7, 10, 21}));
}

TEST_F(OrderBookSideBuyTest, RemoveLevelIfEmptyIsSafeForMissingAndNonEmptyLevels) {
    side.removeLevelIfEmpty(100);

    side.addOrder(1, Side::BUY, Type::LIMIT, 100, 3);
    side.removeLevelIfEmpty(100);

    expectLevelIntegrity(side, 100, {1}, 3U);
}

TEST_F(OrderBookSideBuyTest, GetOrderIdsAtPriceReturnsEmptyForMissingLevel) {
    EXPECT_TRUE(side.getOrderIdsAtPrice(404).empty());
}
