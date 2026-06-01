#include "MatchingEngine.h"
#include "TestHelpers.h"

#include <gtest/gtest.h>

#include <vector>

namespace {

using test_helpers::levelIds;

class OrderTypeBehaviorTest : public ::testing::Test {
protected:
    MatchingEngine engine{1'000};
};

} // namespace

TEST_F(OrderTypeBehaviorTest, MarketBuyConsumesBestAsksAcrossMultipleLevels) {
    engine.processOrder(Side::SELL, Type::LIMIT, 100, 3); // id 1
    engine.processOrder(Side::SELL, Type::LIMIT, 101, 4); // id 2
    engine.processOrder(Side::SELL, Type::LIMIT, 102, 5); // id 3

    const std::vector<Trade> trades = engine.processOrder(Side::BUY, Type::MARKET, 0, 7); // id 4

    ASSERT_EQ(trades.size(), 2U);
    EXPECT_EQ(trades[0].buyOrderId, 4U);
    EXPECT_EQ(trades[0].sellOrderId, 1U);
    EXPECT_EQ(trades[0].executionPrice, 100);
    EXPECT_EQ(trades[0].executionQuantity, 3);

    EXPECT_EQ(trades[1].buyOrderId, 4U);
    EXPECT_EQ(trades[1].sellOrderId, 2U);
    EXPECT_EQ(trades[1].executionPrice, 101);
    EXPECT_EQ(trades[1].executionQuantity, 4);

    EXPECT_EQ(levelIds(engine.getSellOrders(), 102), std::vector<uint64_t>({3}));
}

TEST_F(OrderTypeBehaviorTest, MarketSellConsumesBestBidsAcrossMultipleLevels) {
    engine.processOrder(Side::BUY, Type::LIMIT, 103, 2); // id 1
    engine.processOrder(Side::BUY, Type::LIMIT, 102, 3); // id 2
    engine.processOrder(Side::BUY, Type::LIMIT, 101, 4); // id 3

    const std::vector<Trade> trades = engine.processOrder(Side::SELL, Type::MARKET, 0, 5); // id 4

    ASSERT_EQ(trades.size(), 2U);
    EXPECT_EQ(trades[0].buyOrderId, 1U);
    EXPECT_EQ(trades[0].executionPrice, 103);
    EXPECT_EQ(trades[0].executionQuantity, 2);

    EXPECT_EQ(trades[1].buyOrderId, 2U);
    EXPECT_EQ(trades[1].executionPrice, 102);
    EXPECT_EQ(trades[1].executionQuantity, 3);

    EXPECT_EQ(levelIds(engine.getBuyBook(), 101), std::vector<uint64_t>({3}));
}

TEST_F(OrderTypeBehaviorTest, MarketOrdersIgnoreProvidedPriceField) {
    engine.processOrder(Side::SELL, Type::LIMIT, 105, 2); // id 1
    const std::vector<Trade> buyTrades = engine.processOrder(Side::BUY, Type::MARKET, 1, 2); // id 2

    ASSERT_EQ(buyTrades.size(), 1U);
    EXPECT_EQ(buyTrades[0].executionPrice, 105);

    engine.processOrder(Side::BUY, Type::LIMIT, 95, 3); // id 3
    const std::vector<Trade> sellTrades = engine.processOrder(Side::SELL, Type::MARKET, 9999, 2); // id 4

    ASSERT_EQ(sellTrades.size(), 1U);
    EXPECT_EQ(sellTrades[0].executionPrice, 95);
}

TEST_F(OrderTypeBehaviorTest, MarketOrderWithNoLiquidityDoesNotRestInBook) {
    const std::vector<Trade> buyTrades = engine.processOrder(Side::BUY, Type::MARKET, 0, 10); // id 1
    const std::vector<Trade> sellTrades = engine.processOrder(Side::SELL, Type::MARKET, 0, 10); // id 2

    EXPECT_TRUE(buyTrades.empty());
    EXPECT_TRUE(sellTrades.empty());
    EXPECT_TRUE(engine.getBuyBook().empty());
    EXPECT_TRUE(engine.getSellOrders().empty());
}

TEST_F(OrderTypeBehaviorTest, MarketOrderWithInsufficientLiquidityDiscardsRemainder) {
    engine.processOrder(Side::SELL, Type::LIMIT, 100, 2); // id 1
    engine.processOrder(Side::SELL, Type::LIMIT, 101, 3); // id 2

    const std::vector<Trade> trades = engine.processOrder(Side::BUY, Type::MARKET, 0, 10); // id 3

    ASSERT_EQ(trades.size(), 2U);
    EXPECT_EQ(trades[0].executionQuantity, 2);
    EXPECT_EQ(trades[1].executionQuantity, 3);
    EXPECT_TRUE(engine.getSellOrders().empty());
    EXPECT_TRUE(engine.getBuyBook().empty());
}

TEST_F(OrderTypeBehaviorTest, LimitOrderWithInsufficientLiquidityRestsRemainingQuantity) {
    engine.processOrder(Side::SELL, Type::LIMIT, 100, 2); // id 1

    const std::vector<Trade> trades = engine.processOrder(Side::BUY, Type::LIMIT, 100, 10); // id 2

    ASSERT_EQ(trades.size(), 1U);
    EXPECT_EQ(trades[0].executionQuantity, 2);
    EXPECT_EQ(levelIds(engine.getBuyBook(), 100), std::vector<uint64_t>({2}));
}

TEST_F(OrderTypeBehaviorTest, IocBuyWithNoLiquidityDoesNotRestInBook) {
    const std::vector<Trade> trades = engine.processOrder(Side::BUY, Type::IOC, 100, 10); // id 1

    EXPECT_TRUE(trades.empty());
    EXPECT_TRUE(engine.getBuyBook().empty());
    EXPECT_TRUE(engine.getSellOrders().empty());
    EXPECT_FALSE(engine.cancelOrder(1));
}

TEST_F(OrderTypeBehaviorTest, IocSellWithNoLiquidityDoesNotRestInBook) {
    const std::vector<Trade> trades = engine.processOrder(Side::SELL, Type::IOC, 100, 10); // id 1

    EXPECT_TRUE(trades.empty());
    EXPECT_TRUE(engine.getBuyBook().empty());
    EXPECT_TRUE(engine.getSellOrders().empty());
    EXPECT_FALSE(engine.cancelOrder(1));
}

TEST_F(OrderTypeBehaviorTest, IocBuyDoesNotRestWhenBestAskDoesNotCross) {
    engine.processOrder(Side::SELL, Type::LIMIT, 105, 4); // id 1

    const std::vector<Trade> trades = engine.processOrder(Side::BUY, Type::IOC, 100, 4); // id 2

    EXPECT_TRUE(trades.empty());
    EXPECT_TRUE(engine.getBuyBook().empty());
    EXPECT_EQ(levelIds(engine.getSellOrders(), 105), std::vector<uint64_t>({1}));
    EXPECT_FALSE(engine.cancelOrder(2));
}

TEST_F(OrderTypeBehaviorTest, IocBuyPartiallyFillsAndDiscardsRemainder) {
    engine.processOrder(Side::SELL, Type::LIMIT, 100, 3); // id 1

    const std::vector<Trade> trades = engine.processOrder(Side::BUY, Type::IOC, 100, 10); // id 2

    ASSERT_EQ(trades.size(), 1U);
    EXPECT_EQ(trades[0].buyOrderId, 2U);
    EXPECT_EQ(trades[0].sellOrderId, 1U);
    EXPECT_EQ(trades[0].executionPrice, 100);
    EXPECT_EQ(trades[0].executionQuantity, 3);

    EXPECT_TRUE(engine.getBuyBook().empty());
    EXPECT_TRUE(engine.getSellOrders().empty());
    EXPECT_FALSE(engine.cancelOrder(2));
}

TEST_F(OrderTypeBehaviorTest, IocBuyPartialFillLeavesRestingOrderRemainder) {
    engine.processOrder(Side::SELL, Type::LIMIT, 100, 10); // id 1

    const std::vector<Trade> trades = engine.processOrder(Side::BUY, Type::IOC, 100, 4); // id 2

    ASSERT_EQ(trades.size(), 1U);
    EXPECT_EQ(trades[0].buyOrderId, 2U);
    EXPECT_EQ(trades[0].sellOrderId, 1U);
    EXPECT_EQ(trades[0].executionPrice, 100);
    EXPECT_EQ(trades[0].executionQuantity, 4);

    EXPECT_TRUE(engine.getBuyBook().empty());
    EXPECT_EQ(levelIds(engine.getSellOrders(), 100), std::vector<uint64_t>({1}));
    EXPECT_EQ(engine.getSellOrders().volumeAtPrice(100), 6U);
    EXPECT_FALSE(engine.cancelOrder(2));
}

TEST_F(OrderTypeBehaviorTest, IocBuySweepsCrossingLevelsThenStopsAtPriceLimit) {
    engine.processOrder(Side::SELL, Type::LIMIT, 100, 2); // id 1
    engine.processOrder(Side::SELL, Type::LIMIT, 101, 3); // id 2
    engine.processOrder(Side::SELL, Type::LIMIT, 103, 4); // id 3

    const std::vector<Trade> trades = engine.processOrder(Side::BUY, Type::IOC, 101, 10); // id 4

    ASSERT_EQ(trades.size(), 2U);
    EXPECT_EQ(trades[0].sellOrderId, 1U);
    EXPECT_EQ(trades[0].executionPrice, 100);
    EXPECT_EQ(trades[0].executionQuantity, 2);

    EXPECT_EQ(trades[1].sellOrderId, 2U);
    EXPECT_EQ(trades[1].executionPrice, 101);
    EXPECT_EQ(trades[1].executionQuantity, 3);

    EXPECT_TRUE(engine.getBuyBook().empty());
    EXPECT_TRUE(levelIds(engine.getSellOrders(), 100).empty());
    EXPECT_TRUE(levelIds(engine.getSellOrders(), 101).empty());
    EXPECT_EQ(levelIds(engine.getSellOrders(), 103), std::vector<uint64_t>({3}));
}

TEST_F(OrderTypeBehaviorTest, IocSellSweepsCrossingLevelsThenStopsAtPriceLimit) {
    engine.processOrder(Side::BUY, Type::LIMIT, 103, 2); // id 1
    engine.processOrder(Side::BUY, Type::LIMIT, 102, 3); // id 2
    engine.processOrder(Side::BUY, Type::LIMIT, 100, 4); // id 3

    const std::vector<Trade> trades = engine.processOrder(Side::SELL, Type::IOC, 102, 10); // id 4

    ASSERT_EQ(trades.size(), 2U);
    EXPECT_EQ(trades[0].buyOrderId, 1U);
    EXPECT_EQ(trades[0].executionPrice, 103);
    EXPECT_EQ(trades[0].executionQuantity, 2);

    EXPECT_EQ(trades[1].buyOrderId, 2U);
    EXPECT_EQ(trades[1].executionPrice, 102);
    EXPECT_EQ(trades[1].executionQuantity, 3);

    EXPECT_TRUE(engine.getSellOrders().empty());
    EXPECT_TRUE(levelIds(engine.getBuyBook(), 103).empty());
    EXPECT_TRUE(levelIds(engine.getBuyBook(), 102).empty());
    EXPECT_EQ(levelIds(engine.getBuyBook(), 100), std::vector<uint64_t>({3}));
}

TEST_F(OrderTypeBehaviorTest, MarketVsLimitAtSameQuantityBehaveDifferentlyWhenPriceDoesNotCross) {
    engine.processOrder(Side::SELL, Type::LIMIT, 110, 4); // id 1

    const std::vector<Trade> limitTrades = engine.processOrder(Side::BUY, Type::LIMIT, 100, 4); // id 2
    EXPECT_TRUE(limitTrades.empty());
    EXPECT_EQ(levelIds(engine.getBuyBook(), 100), std::vector<uint64_t>({2}));

    const std::vector<Trade> marketTrades = engine.processOrder(Side::BUY, Type::MARKET, 100, 4); // id 3
    ASSERT_EQ(marketTrades.size(), 1U);
    EXPECT_EQ(marketTrades[0].executionPrice, 110);

    EXPECT_EQ(levelIds(engine.getBuyBook(), 100), std::vector<uint64_t>({2}));
    EXPECT_TRUE(engine.getSellOrders().empty());
}
