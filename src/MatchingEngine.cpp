//
// Created by Nitant Panicker on 27/4/26.
//

#include "../include/MatchingEngine.h"

#include <iostream>
#include <limits>

using namespace std;

#ifndef ORDER_BOOK_DISABLE_LOGGING
#define ORDER_BOOK_LOG(message) do { cout << message << endl; } while (false)
#else
#define ORDER_BOOK_LOG(message) do { } while (false)
#endif

MatchingEngine::MatchingEngine(std::size_t capacity):
    sharedOrderPool_(capacity),
    buyBook(Side::BUY, sharedOrderPool_),
    sellBook(Side::SELL, sharedOrderPool_) {}

vector<Trade> MatchingEngine::processOrder(Side side, Type type, int price, int quantity) {
    Order newOrder {this->getAndIncrementNextOrderId(), side, type, price, quantity};
    ORDER_BOOK_LOG("Processing order:" << newOrder);

    if (newOrder.side == Side::BUY) return this->processBuyOrder(newOrder);
    return this->processSellOrder(newOrder);
}

void MatchingEngine::printBook() {
    if (buyBook.empty()) {
        cout << "There are no buy orders." << endl;
    } else {
        cout << "These are the following buy orders:" << endl;
        buyBook.printBook();
    }

     if (sellBook.empty()) {
        cout << "There are no sell orders." << endl;
    } else {
        cout << "These are the following sell orders:" << endl;
        sellBook.printBook();
    }

    if (this->tradeHistory.empty()) {
        cout << "No trades have been made." << endl;
    } else {
        cout << "These trades have been made: " << endl;
        for (auto t : this->tradeHistory) {
            cout << t << endl;
        }
    }
    
}

// void MatchingEngine::removeEmptyOrders(Side side) {
//     auto isEmpty = [](Order o) {
//         return o.quantity == 0;
//     };

//     if (side == Side::BUY) {
//         erase_if(this->buyBook, isEmpty);
//     } else {
//         erase_if(this->sellBook, isEmpty);
//     }
// }

Trade MatchingEngine::processMatchedOrders(
    Order& incomingOrder,
    const Order& restingOrder,
    uint64_t buyId,
    uint64_t sellId,
    OrderBookSide& book
    ) {
    const int minQuantity = min(incomingOrder.quantity, restingOrder.quantity);
    const int priceTraded = restingOrder.price;
    ORDER_BOOK_LOG("Quantity traded: " << minQuantity);
    incomingOrder.quantity -= minQuantity;

    auto result = book.reduceOrderQuantity(restingOrder.id, minQuantity);
    // TODO: add exception if result is false

    ORDER_BOOK_LOG("Price traded at: " << priceTraded);

    const Trade t(buyId, sellId, priceTraded,  minQuantity);
    tradeHistory.push_back(t);
    return t;
}

bool MatchingEngine::cancelOrder(uint64_t id) {
    const Order* order = getOrderById(id);
    if (order == nullptr) return false;
    
    bool didCancel = order->side == Side::BUY
        ? buyBook.deleteOrderById(id)
        : sellBook.deleteOrderById(id);
    if (didCancel) {
        orderIdSide.erase(id);
    }
    return didCancel;

}


const OrderBookSide& MatchingEngine::getBuyBook() const {
    return this->buyBook;
}

const OrderBookSide& MatchingEngine::getSellOrders() const {
    return this->sellBook;
}

uint64_t MatchingEngine::getAndIncrementNextOrderId() {
    return this->nextOrderId++;
}

void MatchingEngine::addNewOrder(Side side, Order& newOrder) {
    uint64_t id = newOrder.id;
    if (side == Side::BUY) {
        this->buyBook.addOrder(id, newOrder.side, newOrder.type, newOrder.price, newOrder.quantity);
    } else {
        this->sellBook.addOrder(id, newOrder.side, newOrder.type, newOrder.price, newOrder.quantity);
    }

    orderIdSide[newOrder.id] = newOrder.side;
}


bool MatchingEngine::canContinueAgainstPrice(const Order& incoming, int opposingPrice) {
    if (incoming.type == Type::MARKET) return true;
    if (incoming.side == Side::BUY) return incoming.price >= opposingPrice;
    return incoming.price <= opposingPrice;
}

vector<Trade> MatchingEngine::processBuyOrder(Order &newOrder) {
    vector<Trade> tradeList;

    auto best = this->sellBook.bestPrice();
    if (!best) {
        if (newOrder.type == Type::LIMIT and newOrder.quantity > 0) {
            this->addNewOrder(Side::BUY, newOrder);
        }
        return tradeList;
    }
    int bestSellPrice = *best;

    while (newOrder.quantity > 0 and canContinueAgainstPrice(newOrder, bestSellPrice)) {
        const Order* currSellOrder = sellBook.getBestOrder();
        if (currSellOrder == nullptr) break;
        
        ORDER_BOOK_LOG("Matched with sell order: " << *currSellOrder);
        Trade t = this->processMatchedOrders(newOrder, *currSellOrder, newOrder.id, currSellOrder->id, sellBook);
        tradeList.push_back(t);

        best = this->sellBook.bestPrice();
        if (!best) break;
        bestSellPrice = *best;
    }

    if (newOrder.type == Type::LIMIT and newOrder.quantity > 0) this->addNewOrder(Side::BUY, newOrder);

    return tradeList;
}

vector<Trade> MatchingEngine::processSellOrder(Order &newOrder) {
    vector<Trade> tradeList;

    auto best = this->buyBook.bestPrice();
    if (!best) {
        if (newOrder.type == Type::LIMIT and newOrder.quantity > 0) {
            this->addNewOrder(Side::SELL, newOrder);
        }
        return tradeList;
    }
    int bestBuyPrice = *best;

    while (newOrder.quantity > 0 and canContinueAgainstPrice(newOrder, bestBuyPrice)) {
        const Order* currBuyOrder = buyBook.getBestOrder();
        if (currBuyOrder == nullptr) break;
        
        ORDER_BOOK_LOG("Matched with buy order: " << *currBuyOrder);
        Trade t = this->processMatchedOrders(newOrder, *currBuyOrder, currBuyOrder->id, newOrder.id, buyBook);
        tradeList.push_back(t);

        best = this->buyBook.bestPrice();
        if (!best) break;
        bestBuyPrice = *best;
    }

    if (newOrder.type == Type::LIMIT and newOrder.quantity > 0) this->addNewOrder(Side::SELL, newOrder);

    return tradeList;
}

bool MatchingEngine::modifyOrder(uint64_t orderId, int newPrice, int newQuantity) {
    const Order* orderPtr = this->getOrderById(orderId);
    if (orderPtr == nullptr) {
        ORDER_BOOK_LOG("Order ID does not exist");
        return false;
    }

    bool loseQueuePos = true;

    if (newQuantity <= 0 or newPrice <= 0) {
        ORDER_BOOK_LOG("Quantity and price must be positive.");
        return false;
    } else if (newQuantity == orderPtr->quantity and newPrice == orderPtr->price) {
        ORDER_BOOK_LOG("Order has not changed. Ignore request.");
        return false;
    } else if (newQuantity < orderPtr->quantity and newPrice == orderPtr->price) {
        // can keep queue position
        ORDER_BOOK_LOG("Only quantity has reduced. Order can keep its queue position.");
        loseQueuePos = false;
    } else {
        ORDER_BOOK_LOG("Order must be reinserted into queue.");
    }

    
    // construct dummy updated order
    Order updatedOrder{orderId, orderPtr->side, orderPtr->type, newPrice, newQuantity};


    return orderPtr->side == Side::BUY 
        ? buyBook.modifyOrder(updatedOrder, loseQueuePos)
        : sellBook.modifyOrder(updatedOrder, loseQueuePos);
}

void MatchingEngine::saveOrderId(uint64_t id, Side side) {
    orderIdSide[id] = side;
}

const Order* MatchingEngine::getOrderById(uint64_t id) {
    auto ptr = orderIdSide.find(id);
    if (ptr == orderIdSide.end()) return nullptr;

    Side side = ptr->second;
    const Order* order = side == Side::BUY 
        ? buyBook.findOrder(id)
        : sellBook.findOrder(id);

    if (order == nullptr) {
        // invalid state/stale
        orderIdSide.erase(ptr);
        return nullptr;
    }

    return order;
}
