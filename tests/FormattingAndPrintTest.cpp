#include "MatchingEngine.h"
#include "Order.h"
#include "OrderBookSide.h"
#include "Trade.h"

#include <gtest/gtest.h>

#include <sstream>
#include <string>

namespace {

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

} // namespace

TEST(OrderComparatorTest, AscendingPriceOrdersByPriceThenId) {
    const Order lowPrice{3, Side::BUY, Type::LIMIT, 99, 1};
    const Order highPrice{1, Side::BUY, Type::LIMIT, 101, 1};
    const Order earlierAtSamePrice{2, Side::BUY, Type::LIMIT, 100, 1};
    const Order laterAtSamePrice{4, Side::BUY, Type::LIMIT, 100, 1};

    AscendingPrice ascending;

    EXPECT_TRUE(ascending(lowPrice, highPrice));
    EXPECT_FALSE(ascending(highPrice, lowPrice));
    EXPECT_TRUE(ascending(earlierAtSamePrice, laterAtSamePrice));
    EXPECT_FALSE(ascending(laterAtSamePrice, earlierAtSamePrice));
}

TEST(OrderComparatorTest, DescendingPriceOrdersByPriceThenId) {
    const Order lowPrice{3, Side::SELL, Type::LIMIT, 99, 1};
    const Order highPrice{1, Side::SELL, Type::LIMIT, 101, 1};
    const Order earlierAtSamePrice{2, Side::SELL, Type::LIMIT, 100, 1};
    const Order laterAtSamePrice{4, Side::SELL, Type::LIMIT, 100, 1};

    DescendingPrice descending;

    EXPECT_TRUE(descending(highPrice, lowPrice));
    EXPECT_FALSE(descending(lowPrice, highPrice));
    EXPECT_TRUE(descending(earlierAtSamePrice, laterAtSamePrice));
    EXPECT_FALSE(descending(laterAtSamePrice, earlierAtSamePrice));
}

TEST(FormattingTest, OrderStreamFormatsSideAndTypeVariants) {
    std::ostringstream buyLimit;
    buyLimit << Order{1, Side::BUY, Type::LIMIT, 100, 10};
    EXPECT_EQ(buyLimit.str(), "Order{id=1, side=BUY, type=LIMIT, price=100, quantity=10}");

    std::ostringstream sellMarket;
    sellMarket << Order{2, Side::SELL, Type::MARKET, 0, 5};
    EXPECT_EQ(sellMarket.str(), "Order{id=2, side=SELL, type=MARKET, price=0, quantity=5}");

    std::ostringstream buyIoc;
    buyIoc << Order{3, Side::BUY, Type::IOC, 101, 6};
    EXPECT_EQ(buyIoc.str(), "Order{id=3, side=BUY, type=IOC, price=101, quantity=6}");
}

TEST(FormattingTest, SideStreamFormatsBothSides) {
    std::ostringstream output;
    output << Side::BUY << " " << Side::SELL;

    EXPECT_EQ(output.str(), "BUY SELL");
}

TEST(FormattingTest, TradeStreamFormatsExecutionFields) {
    std::ostringstream output;
    output << Trade{4, 1, 100, 3};

    EXPECT_EQ(output.str(), "Trade{buyOrderId=4, sellOrderId=1, executionPrice=100, executionQuantity=3}");
}

TEST(PrintBookTest, OrderBookSidePrintBookWritesEachActiveOrder) {
    OrderNodePool pool{10};
    OrderBookSide side{Side::BUY, pool};
    side.addOrder(1, Side::BUY, Type::LIMIT, 100, 5);
    side.addOrder(2, Side::BUY, Type::MARKET, 101, 7);

    testing::internal::CaptureStdout();
    side.printBook();
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(contains(output, "Order{id=1, side=BUY, type=LIMIT, price=100, quantity=5}"));
    EXPECT_TRUE(contains(output, "Order{id=2, side=BUY, type=MARKET, price=101, quantity=7}"));
}

TEST(PrintBookTest, MatchingEnginePrintBookReportsEmptyBooksAndNoTrades) {
    MatchingEngine engine{10};

    testing::internal::CaptureStdout();
    engine.printBook();
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(contains(output, "There are no buy orders."));
    EXPECT_TRUE(contains(output, "There are no sell orders."));
    EXPECT_TRUE(contains(output, "No trades have been made."));
}

TEST(PrintBookTest, MatchingEnginePrintBookReportsBooksAndTradeHistory) {
    MatchingEngine engine{10};
    engine.processOrder(Side::BUY, Type::LIMIT, 100, 5);   // id 1
    engine.processOrder(Side::SELL, Type::LIMIT, 105, 2);  // id 2
    engine.processOrder(Side::SELL, Type::LIMIT, 100, 3);  // id 3

    testing::internal::CaptureStdout();
    engine.printBook();
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(contains(output, "These are the following buy orders:"));
    EXPECT_TRUE(contains(output, "Order{id=1, side=BUY, type=LIMIT, price=100, quantity=2}"));
    EXPECT_TRUE(contains(output, "These are the following sell orders:"));
    EXPECT_TRUE(contains(output, "Order{id=2, side=SELL, type=LIMIT, price=105, quantity=2}"));
    EXPECT_TRUE(contains(output, "These trades have been made:"));
    EXPECT_TRUE(contains(output, "Trade{buyOrderId=1, sellOrderId=3, executionPrice=100, executionQuantity=3}"));
}
