#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT_DIR/code"
DATA_DIR="/workspace/data/004/1075"

if [[ ! -x "$BIN" ]]; then
  echo "code 不存在或不可执行，先编译：cmake . && make" >&2
  exit 1
fi

if [[ ! -d "$DATA_DIR" ]]; then
  echo "找不到公开数据目录 $DATA_DIR，跳过数据测试，仅做烟雾测试" >&2
  DATA_DIR=""
fi

# 简单烟雾测试
printf "su root sjtu\nlogout\nexit\n" | "$BIN" >/dev/null

echo "基本烟雾测试通过"

# 若有公开数据，批量跑
if [[ -n "$DATA_DIR" ]]; then
  ok=0; total=0
  # 按自然数顺序排序，以模拟评测顺序（可能存在状态依赖）
  while IFS= read -r in_file; do
    [[ -e "$in_file" ]] || continue
    total=$((total+1))
    out_file="${in_file%.in}.out"
    tmp_out="$(mktemp)"
    set +e
    timeout 10s "$BIN" <"$in_file" >"$tmp_out"
    ret=$?
    set -e
    if [[ $ret -ne 0 ]]; then
      echo "[FAIL] $(basename "$in_file") 运行错误 code=$ret"
    elif diff -u "$out_file" "$tmp_out" >/dev/null 2>&1; then
      echo "[OK] $(basename "$in_file")"
      ok=$((ok+1))
    else
      echo "[WA] $(basename "$in_file") 与标准输出不一致"
      echo "--- 期望: $out_file"
      echo "+++ 实际: $tmp_out"
    fi
    rm -f "$tmp_out"
  done < <(ls "$DATA_DIR"/*.in | sort -V)
  echo "通过 $ok/$total 个公开用例"
fi
