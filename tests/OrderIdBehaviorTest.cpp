#include "MatchingEngine.h"
#include "TestHelpers.h"

#include <gtest/gtest.h>

#include <vector>

namespace {

using test_helpers::levelIds;

class OrderIdBehaviorTest : public ::testing::Test {
protected:
    MatchingEngine engine{1'000};
};

} // namespace

TEST_F(OrderIdBehaviorTest, EngineAssignsMonotonicIdsAcrossAllIncomingOrders) {
    engine.processOrder(Side::SELL, Type::LIMIT, 105, 5);  // id 1 rests
    engine.processOrder(Side::BUY, Type::MARKET, 0, 5);    // id 2 matched, not resting
    engine.processOrder(Side::BUY, Type::LIMIT, 100, 3);   // id 3 rests

    EXPECT_EQ(levelIds(engine.getBuyBook(), 100), std::vector<uint64_t>({3}));
    EXPECT_TRUE(engine.getSellOrders().empty());
}

TEST_F(OrderIdBehaviorTest, IdUniquenessPreservedAfterCancelAndNewOrder) {
    engine.processOrder(Side::BUY, Type::LIMIT, 100, 5); // id 1
    const bool cancelled = engine.cancelOrder(1);
    EXPECT_TRUE(cancelled);

    engine.processOrder(Side::BUY, Type::LIMIT, 99, 4); // id 2

    EXPECT_EQ(levelIds(engine.getBuyBook(), 99), std::vector<uint64_t>({2}));
}

TEST_F(OrderIdBehaviorTest, IocOrderConsumesIdButDoesNotCreateCancellableRestingOrder) {
    engine.processOrder(Side::BUY, Type::IOC, 100, 5);   // id 1, not resting
    engine.processOrder(Side::BUY, Type::LIMIT, 99, 4);  // id 2

    EXPECT_FALSE(engine.cancelOrder(1));
    EXPECT_EQ(levelIds(engine.getBuyBook(), 99), std::vector<uint64_t>({2}));
}
