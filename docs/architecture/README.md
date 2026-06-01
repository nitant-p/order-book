# Architecture Diagrams

This directory contains architecture diagrams used by the root README, plus Mermaid source files that can be rendered with Mermaid CLI when available.

## Matching Engine

The `MatchingEngine` owns one shared `OrderNodePool` and two `OrderBookSide` instances. The buy and sell books do not own order nodes; they borrow active `OrderNode*` slots from the shared pool and return them on cancel, fill, or delete paths.

```mermaid
flowchart TB
    Engine["MatchingEngine"]

    subgraph EngineState["Engine state and APIs"]
        Process["processOrder()"]
        Cancel["cancelOrder()"]
        Modify["modifyOrder()"]
        Trades["tradeHistory"]
        SideIndex["orderIdSide: unordered_map&lt;id, Side&gt;"]
    end

    Pool["sharedOrderPool_: OrderNodePool<br/>owns reusable OrderNode storage"]

    subgraph PoolStorage["Pool-owned storage"]
        Chunks["chunks_: vector&lt;unique_ptr&lt;OrderNode[]&gt;&gt;"]
        FreeList["freeNodes_"]
        Counts["activeCount() / availableCount()"]
    end

    BuyBook["buyBook: OrderBookSide(BUY)"]
    SellBook["sellBook: OrderBookSide(SELL)"]

    subgraph BuyState["Buy book state"]
        BuyLevels["priceToLevels_: map&lt;int, PriceLevel&gt;"]
        BuyIndex["orderNodesById_: unordered_map&lt;id, OrderNode*&gt;"]
    end

    subgraph SellState["Sell book state"]
        SellLevels["priceToLevels_: map&lt;int, PriceLevel&gt;"]
        SellIndex["orderNodesById_: unordered_map&lt;id, OrderNode*&gt;"]
    end

    Engine --> EngineState
    Engine --> BuyBook
    Engine --> SellBook
    Engine --> Pool
    Pool --> PoolStorage
    BuyBook --> BuyState
    SellBook --> SellState
    BuyBook -. "acquire/release" .-> Pool
    SellBook -. "acquire/release" .-> Pool
    BuyIndex -. "borrowed pointers" .-> Chunks
    SellIndex -. "borrowed pointers" .-> Chunks
```

## Price Levels

Each side stores price levels in `std::map<int, PriceLevel>`. Conceptually this is an ordered tree keyed by price. A `PriceLevel` stores aggregate metadata and head/tail pointers into the FIFO queue of borrowed pool nodes.

```mermaid
flowchart TB
    Pool["shared OrderNodePool<br/>owns OrderNode chunks"]
    Map["priceToLevels_: map&lt;int, PriceLevel&gt;"]
    Root["price 100"]
    Left["price 99"]
    Right["price 101"]
    FarRight["price 103"]
    Level["PriceLevel<br/>orderCount<br/>totalQuantity<br/>head: OrderNode*<br/>tail: OrderNode*"]
    First["OrderNode<br/>borrowed from pool"]
    Last["OrderNode<br/>borrowed from pool"]

    Map --> Root
    Root --"left"--> Left
    Root --"right"--> Right
    Right --"right"--> FarRight

    Root -. "map value" .-> Level
    Level --"head"--> First
    Level --"tail"--> Last
    Pool -. "owns storage" .-> First
    Pool -. "owns storage" .-> Last
```

## Order Nodes

`orderNodesById_` is a lookup index of borrowed `OrderNode*` values. The nodes themselves live in pool chunks, link to neighboring active orders at the same price, and point back to their `PriceLevel`.

```mermaid
flowchart LR
    Pool["shared OrderNodePool"]

    subgraph Chunks["Pool-owned chunks"]
        Chunk0["chunk 0<br/>unique_ptr&lt;OrderNode[]&gt;"]
        Chunk1["chunk 1<br/>unique_ptr&lt;OrderNode[]&gt;"]
    end

    IdMap["orderNodesById_: unordered_map&lt;uint64_t, OrderNode*&gt;"]
    Id1["id 41"]
    Id2["id 42"]
    Id3["id 43"]

    subgraph Queue["FIFO order nodes at one price"]
        N1["OrderNode<br/>order id 41"]
        N2["OrderNode<br/>order id 42"]
        N3["OrderNode<br/>order id 43"]

        N1 -- "next" --> N2
        N2 -- "prev" --> N1
        N2 -- "next" --> N3
        N3 -- "prev" --> N2
    end

    Level["PriceLevel"]
    Head["head"]
    Tail["tail"]

    Pool --> Chunks
    Chunk0 -. "contains" .-> N1
    Chunk0 -. "contains" .-> N2
    Chunk1 -. "contains" .-> N3

    IdMap --> Id1
    IdMap --> Id2
    IdMap --> Id3
    Id1 -. "OrderNode*" .-> N1
    Id2 -. "OrderNode*" .-> N2
    Id3 -. "OrderNode*" .-> N3

    N2 -. "priceLevel" .-> Level
    Level --> Head
    Level --> Tail
    Head --> N1
    Tail --> N3
```
