#!/usr/bin/env python3
"""Convert Google Benchmark JSON output into the project benchmark CSV format."""

from __future__ import annotations

import csv
import json
import sys
from pathlib import Path


METADATA = {
    "BM_ProcessOrder_AddOnly": (
        "MatchingEngine",
        "processOrder",
        "Add-only limit orders",
        "Resting-only inserts acquire reusable OrderNode slots, then update price levels and order-ID indexing.",
    ),
    "BM_ProcessOrder_MatchHeavy": (
        "MatchingEngine",
        "processOrder",
        "Match-heavy market buys",
        "Matching releases filled resting nodes back to the shared pool while incoming market orders avoid resting.",
    ),
    "BM_ProcessOrder_Mixed": (
        "MatchingEngine",
        "processOrder",
        "Mixed limits and markets",
        "Mixed flow combines passive adds, fills, and node-slot reuse.",
    ),
    "BM_CancelOrder_Existing": (
        "MatchingEngine",
        "cancelOrder",
        "Existing resting orders",
        "Cancel unlinks the order and returns its reusable node slot to the pool.",
    ),
    "BM_ModifyOrder_SamePriceReduce": (
        "MatchingEngine",
        "modifyOrder",
        "Same-price quantity reduction",
        "Same-price quantity reduction keeps the same node and queue position.",
    ),
    "BM_ModifyOrder_PriceChange": (
        "MatchingEngine",
        "modifyOrder",
        "Price change",
        "Price changes relink the existing node into another price level without reallocating it.",
    ),
    "BM_OrderBookSide_AddOrder": (
        "OrderBookSide",
        "addOrder",
        "Direct side insert",
        "Direct insertion acquires nodes from the shared pool and skips engine orchestration.",
    ),
    "BM_OrderBookSide_BestPrice": (
        "OrderBookSide",
        "bestPrice",
        "Best price read",
        "Best-price reads are mostly independent of node allocation strategy.",
    ),
    "BM_OrderBookSide_GetBestOrder": (
        "OrderBookSide",
        "getBestOrder",
        "Best order pointer read",
        "Best-order reads return the head pointer at the best level.",
    ),
    "BM_OrderBookSide_FindOrder": (
        "OrderBookSide",
        "findOrder",
        "Existing ID lookup",
        "Average O(1) ID lookup returns a borrowed node pointer owned by the pool.",
    ),
    "BM_OrderBookSide_DeleteOrderById": (
        "OrderBookSide",
        "deleteOrderById",
        "Existing ID delete",
        "Delete unlinks the node and returns its slot to the pool.",
    ),
    "BM_OrderBookSide_ReduceQuantityPartial": (
        "OrderBookSide",
        "reduceOrderQuantity",
        "Partial reduction",
        "Partial reduction keeps the order resting, so no pool release is needed.",
    ),
    "BM_OrderBookSide_ReduceQuantityExactFill": (
        "OrderBookSide",
        "reduceOrderQuantity",
        "Exact fill",
        "Exact fill unlinks the order and recycles its node slot.",
    ),
    "BM_OrderBookSide_ModifySamePriceReduce": (
        "OrderBookSide",
        "modifyOrder",
        "Same-price quantity reduction",
        "Same-price modification preserves node identity and queue position.",
    ),
    "BM_OrderBookSide_ModifyPriceChange": (
        "OrderBookSide",
        "modifyOrder",
        "Price change",
        "Price-change modification relinks the same node into another price level.",
    ),
    "BM_OrderBookSide_GetDepth": (
        "OrderBookSide",
        "getDepth",
        None,
        "Depth reads are dominated by level traversal and snapshot materialization.",
    ),
}


def parse_run_name(run_name: str) -> tuple[str, str, str | None]:
    parts = run_name.split("/")
    benchmark_name = parts[0]
    args = parts[1:]
    if benchmark_name == "BM_OrderBookSide_GetDepth":
        return benchmark_name, args[1], f"Depth {args[0]}"
    return benchmark_name, args[0], None


def main() -> None:
    if len(sys.argv) != 3:
        print("usage: benchmark_json_to_csv.py <benchmark_raw.json> <output.csv>", file=sys.stderr)
        raise SystemExit(2)

    input_path = Path(sys.argv[1])
    output_path = Path(sys.argv[2])

    data = json.loads(input_path.read_text())
    rows: list[dict[str, str]] = []

    for benchmark in data["benchmarks"]:
        if benchmark.get("aggregate_name") != "mean":
            continue

        benchmark_name, scale, depth_case = parse_run_name(benchmark["run_name"])
        if benchmark_name not in METADATA:
            continue

        layer, api, case, finding = METADATA[benchmark_name]
        if depth_case is not None:
            case = depth_case

        operations = float(scale)
        cpu_time = float(benchmark["cpu_time"])
        ns_per_op = cpu_time / operations
        ops_per_sec = float(benchmark.get("items_per_second", operations / (cpu_time / 1_000_000_000)))

        rows.append(
            {
                "layer": layer,
                "api": api,
                "case": case or "",
                "scale": scale,
                "ns_per_op": f"{ns_per_op:.1f}",
                "ops_per_sec": f"{ops_per_sec:.0f}",
                "finding": finding,
            }
        )

    with output_path.open("w", newline="") as f:
        writer = csv.DictWriter(
            f,
            fieldnames=["layer", "api", "case", "scale", "ns_per_op", "ops_per_sec", "finding"],
            lineterminator="\n",
        )
        writer.writeheader()
        writer.writerows(rows)

    print(f"wrote {output_path} ({len(rows)} rows)")


if __name__ == "__main__":
    main()
