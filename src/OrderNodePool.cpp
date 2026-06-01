//
// Created by Nitant Panicker on 31/5/26.
//
#include "../include/OrderNodePool.h"
#include <cassert>
#include "../include/OrderBookSide.h"

using namespace std;

OrderNodePool::OrderNodePool(std::size_t capacity): totalCapacity_(capacity) {
   chunks_.push_back(std::make_unique<OrderNode[]>(capacity));
   chunkSizes_.push_back(capacity);
   inUseByChunk_.push_back(vector<bool>(capacity));

   freeNodes_ = vector<OrderNode*>(capacity); // default is nullptr

   auto& firstChunk = chunks_[0];
   for (size_t i = 0; i < capacity; ++i) {

      firstChunk[i].globalChunkIndex = chunkTotal_;
      firstChunk[i].localChunkIndex = i;

      freeNodes_[i] = &firstChunk[i];
      inUseByChunk_[0][i] = false;
   }
   ++chunkTotal_;
}

OrderNodePool::~OrderNodePool() = default;

OrderNode* OrderNodePool::acquire(const Order &order) {
   if (freeNodes_.empty()) doublePoolSize();

   OrderNode* nodePtr = freeNodes_.back();
   assert(!inUseByChunk_[nodePtr->globalChunkIndex][nodePtr->localChunkIndex]);

   nodePtr->order = order;
   nodePtr->next = nullptr;
   nodePtr->prev = nullptr;
   nodePtr->priceLevel = nullptr;

   freeNodes_.pop_back();
   inUseByChunk_[nodePtr->globalChunkIndex][nodePtr->localChunkIndex] = true;
   ++activeCount_;

   return nodePtr;
}

bool OrderNodePool::release(OrderNode* node) {
   if (node == nullptr) return false;
   if (!owns(node)) return false;

   size_t globalI = node->globalChunkIndex;
   size_t localI = node->localChunkIndex;

   if (!inUseByChunk_[globalI][localI]) return false;

   resetNode(*node);

   inUseByChunk_[globalI][localI] = false;
   --activeCount_;

   freeNodes_.push_back(node);
   return true;
}

bool OrderNodePool::owns(const OrderNode* node) const noexcept {
   if (node == nullptr) return false;
   if (chunks_.empty()) return false;

   if (node->globalChunkIndex >= chunkTotal_) return false;
   if (node->localChunkIndex >= chunkSizes_[node->globalChunkIndex]) return false;

   return node == &chunks_[node->globalChunkIndex][node->localChunkIndex];
}

void OrderNodePool::resetNode(OrderNode& node) noexcept {
   // clear values
   node.next = nullptr;
   node.prev = nullptr;
   node.priceLevel = nullptr;
}

void OrderNodePool::doublePoolSize() {
   size_t doubleCapacity = totalCapacity_ * 2;
   // handle 0 capacity
   if (doubleCapacity == 0) doubleCapacity = 1;
   size_t increase = doubleCapacity - totalCapacity_;

   chunks_.push_back(std::make_unique<OrderNode[]>(increase));
   chunkSizes_.push_back(increase);
   freeNodes_.reserve(freeNodes_.size() + increase);
   inUseByChunk_.push_back(vector<bool>(increase, false));

   auto& lastChunk = chunks_.back();
   // adding new elements from capacity -> double capacity
   for (size_t i = 0; i < increase; ++i) {
      lastChunk[i].globalChunkIndex = chunkTotal_;
      lastChunk[i].localChunkIndex = i;
      freeNodes_.push_back(&lastChunk[i]);
   }

   totalCapacity_ = doubleCapacity;
   ++chunkTotal_;
}

bool OrderNodePool::hasAvailable() const noexcept {
   return !freeNodes_.empty();
}

std::size_t OrderNodePool::capacity() const noexcept {
   return totalCapacity_;
}

std::size_t OrderNodePool::activeCount() const noexcept {
   return activeCount_;
}

std::size_t OrderNodePool::availableCount() const noexcept {
   return freeNodes_.size();
}

