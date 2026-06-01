//
// Created by Nitant Panicker on 28/4/26.
//

#include "../include/Order.h"

#include <ostream>

std::string sideToString(Side side) {
    return side == Side::BUY ? "BUY" : "SELL";
}

std::string typeToString(Type type) {
    switch (type) {
        case Type::LIMIT:
            return "LIMIT";
        case Type::MARKET:
            return "MARKET";
        case Type::IOC:
            return "IOC";
    }

    return "UNKNOWN";
}

// sort by ascending price, id for tie-breaker
bool AscendingPrice::operator()(const Order &order1, const Order &order2) {
    if (order1.price == order2.price) return order1.id < order2.id;
    return order1.price < order2.price;
}

// sort by descending price, id for tie-breaker
bool DescendingPrice::operator()(const Order &order1, const Order &order2) {
    if (order1.price == order2.price) return order1.id < order2.id;
    return order1.price > order2.price;
}

std::ostream& operator<<(std::ostream& os, const Order& order) {
    os << "Order{id=" << order.id
       << ", side=" << sideToString(order.side)
       << ", type=" << typeToString(order.type)
       << ", price=" << order.price
       << ", quantity=" << order.quantity
       << "}";

    return os;
}

std::ostream& operator<<(std::ostream& os, Side side) {
    os << sideToString(side);
    return os;
}
