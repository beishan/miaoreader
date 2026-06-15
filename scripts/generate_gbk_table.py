#!/usr/bin/env python3
"""
GBK-to-Unicode 映射表生成器

从 Unicode.org 的 GBK 映射表生成 C 头文件。
映射表覆盖 GBK 全部 23940 个双字节码位。

使用方式：
  python3 scripts/generate_gbk_table.py

生成文件：
  src/engine/gbk_table.h
"""

import os
import urllib.request
import struct

# GBK 映射表 URL（Unicode.org 官方）
GBK_MAPPING_URL = "https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP936.TXT"

def download_mapping():
    """下载 GBK 映射表"""
    print("下载 GBK 映射表...")
    try:
        import ssl
        ctx = ssl.create_default_context()
        ctx.check_hostname = False
        ctx.verify_mode = ssl.CERT_NONE
        response = urllib.request.urlopen(GBK_MAPPING_URL, timeout=30, context=ctx)
        return response.read().decode('utf-8')
    except Exception as e:
        print(f"下载失败: {e}")
        print("请手动下载映射表文件到 scripts/CP936.TXT")
        return None

def parse_mapping(text):
    """解析映射表，返回 GBK → Unicode 字典"""
    mapping = {}
    for line in text.split('\n'):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        parts = line.split('\t')
        if len(parts) < 2:
            continue
        try:
            gbk_code = int(parts[0], 16)
            unicode_code = int(parts[1], 16)
            mapping[gbk_code] = unicode_code
        except ValueError:
            continue
    return mapping

def generate_table(mapping):
    """生成 126×190 的查找表"""
    # GBK 双字节范围：
    # 第一字节：0x81-0xFE（126 个值）
    # 第二字节：0x40-0xFE，除去 0x7F（189 个值）
    # 总计：126 × 189 = 23814 个码位

    table = [0] * (126 * 190)

    for gbk_code, unicode_code in mapping.items():
        if gbk_code < 0x80:
            continue  # 单字节，跳过

        c1 = (gbk_code >> 8) & 0xFF
        c2 = gbk_code & 0xFF

        if c1 < 0x81 or c1 > 0xFE:
            continue
        if c2 < 0x40 or c2 > 0xFE or c2 == 0x7F:
            continue

        row = c1 - 0x81
        col = c2 - 0x40 - (1 if c2 > 0x7F else 0)
        idx = row * 190 + col

        if 0 <= idx < len(table):
            table[idx] = unicode_code

    return table

def write_header(table, output_path):
    """生成 C 头文件"""
    os.makedirs(os.path.dirname(output_path), exist_ok=True)

    with open(output_path, 'w', encoding='utf-8') as f:
        f.write("/**\n")
        f.write(" * @file gbk_table.h\n")
        f.write(" * @brief GBK-to-Unicode 映射表（自动生成，请勿手动编辑）\n")
        f.write(" *\n")
        f.write(" * 覆盖 GBK 全部双字节码位（0x8140-0xFEFE，除去 0x7F）\n")
        f.write(" * 表大小：126×190 = 23940 个 Unicode 码点\n")
        f.write(" */\n")
        f.write("#pragma once\n")
        f.write("\n")
        f.write("#include <stdint.h>\n")
        f.write("\n")
        f.write("/* GBK → Unicode 映射表\n")
        f.write(" * 索引计算：row = c1 - 0x81, col = c2 - 0x40 - (c2 > 0x7F ? 1 : 0)\n")
        f.write(" * 表大小：23940 个 uint16_t（约 47KB）\n")
        f.write(" */\n")
        f.write("static const uint16_t gbk_to_unicode[23940] = {\n")

        # 每行 16 个值
        for i in range(0, len(table), 16):
            chunk = table[i:i+16]
            values = ', '.join(f'0x{v:04X}' for v in chunk)
            f.write(f"    {values},\n")

        f.write("};\n")

    print(f"生成完成：{output_path}")
    print(f"表大小：{len(table)} 个条目，约 {len(table) * 2 / 1024:.1f} KB")

def main():
    # 尝试下载映射表
    mapping_text = download_mapping()

    if mapping_text is None:
        # 尝试读取本地文件
        local_file = os.path.join(os.path.dirname(__file__), 'CP936.TXT')
        if os.path.exists(local_file):
            print(f"使用本地文件：{local_file}")
            with open(local_file, 'r', encoding='utf-8') as f:
                mapping_text = f.read()
        else:
            print("错误：无法获取映射表文件")
            return 1

    # 解析映射表
    mapping = parse_mapping(mapping_text)
    print(f"解析到 {len(mapping)} 个映射条目")

    # 生成查找表
    table = generate_table(mapping)

    # 输出路径
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_dir = os.path.dirname(script_dir)
    output_path = os.path.join(project_dir, 'src', 'engine', 'gbk_table.h')

    write_header(table, output_path)

    return 0

if __name__ == '__main__':
    exit(main())
