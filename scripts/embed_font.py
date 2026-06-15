#!/usr/bin/env python3
"""把字模二进制转换为 C 头文件 (.h) 数组。"""
from __future__ import annotations
import sys
from pathlib import Path

def to_c_array(bin_path: Path, out_path: Path, name: str = "ui_font_16") -> None:
    data = bin_path.read_bytes()
    lines = [f"// Auto-generated from {bin_path.name} ({len(data)} bytes)",
             f"#pragma once",
             f"#include <stdint.h>",
             f"",
             f"static const unsigned int {name}_len = {len(data)};",
             f"static const uint8_t {name}[{len(data)}] = {{"]
    # 16 字节一行
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        hex_str = ", ".join(f"0x{b:02x}" for b in chunk)
        lines.append(f"    {hex_str},")
    lines.append("};")
    lines.append("")
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text("\n".join(lines))
    print(f"[ok] 生成 {out_path}: {len(data)} 字节 → {len(lines)} 行")


if __name__ == "__main__":
    bin_path = Path(sys.argv[1])
    out_path = Path(sys.argv[2])
    to_c_array(bin_path, out_path)
