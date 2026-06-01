#pragma once
#include <cstdint>
#include <ostream>
enum class Side {BUY, SELL};
enum class Type {LIMIT, MARKET, IOC};

// default values for default constructor
struct Order {
    uint64_t id = 0;
    Side side = Side::BUY;
    Type type = Type::LIMIT;
    int price = 0;
    int quantity = 0;
};

struct AscendingPrice {
    bool operator() (const Order& order1, const Order& order2);
};

struct DescendingPrice {
    bool operator() (const Order& order1, const Order& order2);
};


std::ostream& operator<<(std::ostream& os, const Order& order);
std::ostream& operator<<(std::ostream& os, Side side);
