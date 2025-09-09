#!/usr/bin/env python3
import argparse
from pathlib import Path
import numpy as np

try:
    import h5py
except ImportError as e:
    raise SystemExit("Please install h5py: pip install h5py") from e


def write_fvecs(path: Path, X: np.ndarray, chunk: int = 10000) -> None:
    """Write matrix X (n,d) as fvecs: per vector [int32 d][d x float32]."""
    path.parent.mkdir(parents=True, exist_ok=True)
    X = np.asarray(X, dtype=np.float32, order="C")
    n, d = X.shape
    with open(path, "wb") as f:
        for i in range(0, n, chunk):
            block = X[i : i + chunk]
            for row in block:
                f.write(np.int32(d).tobytes())
                f.write(row.tobytes())


def load_queries(h5: h5py.File, split: str) -> np.ndarray:
    # expected layout: root/itest/queries or root/otest/queries
    grp = h5[split]
    q = grp["queries"][()]
    return q


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--h5", required=True, help="input HDF5 file path")
    ap.add_argument("--dataset", required=True, help="dataset name, e.g. ccnews/yahooaq")
    ap.add_argument("--benchmarks", default="./benchmarks", help="benchmarks root directory")
    ap.add_argument(
        "--query_split",
        choices=["itest", "otest", "both"],
        default="itest",
        help="use itest/otest queries; both = concat(itest,otest)",
    )
    ap.add_argument("--limit_base", type=int, default=0, help="cap base rows (0=no cap)")
    ap.add_argument("--limit_query", type=int, default=0, help="cap query rows per split (0=no cap)")
    args = ap.parse_args()

    out_dir = Path(args.benchmarks) / args.dataset / "origin"
    base_path = out_dir / f"{args.dataset}_base.fvecs"
    query_path = out_dir / f"{args.dataset}_query.fvecs"

    with h5py.File(args.h5, "r") as f:
        base = f["train"][()]
        if args.limit_base and args.limit_base > 0:
            base = base[: args.limit_base]

        if args.query_split == "both":
            qi = load_queries(f, "itest")
            qo = load_queries(f, "otest")
            if args.limit_query and args.limit_query > 0:
                qi = qi[: args.limit_query]
                qo = qo[: args.limit_query]
            query = np.concatenate([qi, qo], axis=0)
        else:
            query = load_queries(f, args.query_split)
            if args.limit_query and args.limit_query > 0:
                query = query[: args.limit_query]

    write_fvecs(base_path, base)
    write_fvecs(query_path, query)

    print(f"Wrote base:  {base.shape} -> {base_path}")
    print(f"Wrote query: {query.shape} -> {query_path}")


if __name__ == "__main__":
    main()


