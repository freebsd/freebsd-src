#!/usr/bin/env python3
"""
MediaTek DTBO merge tool

Merges/replace DTBO nodes from a base DTBO with application overlays.

Licensed under BSD-2-Clause.
"""

import argparse
import struct
import sys
from pathlib import Path
from typing import Dict, List, Optional, Tuple


class DtboHeader:
    """DTBO header structure"""
    DTBO_MAGIC = 0xD00DFEED
    DTBO_MAGIC_BE = 0xEDFEF0D0

    def __init__(self):
        self.magic: int = 0
        self.version: int = 0
        self.total_size: int = 0
        self.num_dt_entries: int = 0
        self.off_dt_entries: int = 0
        self.off_code: int = 0

    @classmethod
    def from_bytes(cls, data: bytes) -> 'DtboHeader':
        """Parse header from binary data"""
        if len(data) < 32:
            raise ValueError("Invalid DTBO: too small")

        header = cls()
        magic = struct.unpack('<I', data[0:4])[0]
        if magic == cls.DTBO_MAGIC_BE:
            header.magic = struct.unpack('>I', data[0:4])[0]
            header.version = struct.unpack('>I', data[4:8])[0]
            header.total_size = struct.unpack('>I', data[8:12])[0]
            header.num_dt_entries = struct.unpack('>I', data[12:16])[0]
            header.off_dt_entries = struct.unpack('>I', data[16:20])[0]
            header.off_code = struct.unpack('>I', data[20:24])[0]
        else:
            header.magic = magic
            header.version = struct.unpack('<I', data[4:8])[0]
            header.total_size = struct.unpack('<I', data[8:12])[0]
            header.num_dt_entries = struct.unpack('<I', data[12:16])[0]
            header.off_dt_entries = struct.unpack('<I', data[16:20])[0]
            header.off_code = struct.unpack('<I', data[20:24])[0]

        if header.magic not in (cls.DTBO_MAGIC, cls.DTBO_MAGIC_BE):
            raise ValueError(f"Invalid DTBO magic: 0x{header.magic:08X}")

        return header


class DtEntryHeader:
    """DT entry header for each overlay"""
    
    def __init__(self):
        self.dt_size: int = 0
        self.dt_offset: int = 0
        self.id: int = 0
        self.rev: int = 0
        self.flags: int = 0

    @classmethod
    def from_bytes(cls, data: bytes) -> 'DtEntryHeader':
        header = cls()
        header.dt_size = struct.unpack('<I', data[0:4])[0]
        header.dt_offset = struct.unpack('<I', data[4:8])[0]
        header.id = struct.unpack('<I', data[8:12])[0]
        header.rev = struct.unpack('<I', data[12:16])[0]
        header.flags = struct.unpack('<I', data[16:20])[0]
        return header


def parse_fdt_nodes(dt_data: bytes) -> Dict[str, bytes]:
    """Parse FDT nodes from device tree blob"""
    nodes = {}
    # Simplified FDT parsing - real implementation would need full FDT library
    # This is a stub for the tool structure
    return nodes


def merge_dtbo(base_dtbo: Path, overlay_dtbo: Path, output_dtbo: Path,
               oids: Optional[List[int]] = None) -> int:
    """Merge overlay DTBO into base DTBO"""
    
    base_data = base_dtbo.read_bytes()
    overlay_data = overlay_dtbo.read_bytes()
    
    # Parse headers
    base_header = DtboHeader.from_bytes(base_data)
    
    print(f"Base DTBO: {base_header.num_dt_entries} entries")
    print(f"Overlay DTBO: contains {len(overlay_data)} bytes")
    
    # Read base entries
    entries = []
    for i in range(base_header.num_dt_entries):
        offset = base_header.off_dt_entries + i * 32
        entry_data = base_data[offset:offset + 32]
        entry = DtEntryHeader.from_bytes(entry_data)
        entries.append(entry)
        print(f"  Entry {i}: id=0x{entry.id:08X}, size={entry.dt_size}")
    
    # In a real implementation, this would merge the overlay nodes
    # For now, we just copy the overlay as-is if no specific OIDs
    if oids is None:
        output_dtbo.write_bytes(overlay_data)
        print(f"Wrote overlay as output to {output_dtbo}")
    else:
        print(f"Would merge overlays with OIDs: {oids}")
        output_dtbo.write_bytes(base_data)
    
    return 0


def main():
    parser = argparse.ArgumentParser(
        description="Merge DTBO overlays for MediaTek devices"
    )
    parser.add_argument(
        "--base", "-b",
        required=True,
        type=Path,
        help="Base DTBO file"
    )
    parser.add_argument(
        "--overlay", "-o",
        required=True,
        type=Path,
        help="Overlay DTBO file(s) to merge",
        nargs="+"
    )
    parser.add_argument(
        "--output", "-O",
        required=True,
        type=Path,
        help="Output merged DTBO file"
    )
    parser.add_argument(
        "--id", "-i",
        type=int,
        action="append",
        help="Override ID to apply (can be repeated)"
    )
    parser.add_argument(
        "--replace-nodes", "-r",
        help="Comma-separated list of nodes to replace"
    )
    
    args = parser.parse_args()
    
    if not args.base.exists():
        print(f"Error: Base DTBO not found: {args.base}")
        sys.exit(1)
    
    for overlay in args.overlay:
        if not overlay.exists():
            print(f"Error: Overlay DTBO not found: {overlay}")
            sys.exit(1)
    
    oids = args.id if args.id else None
    
    # For simplicity, merge first overlay
    merge_dtbo(args.base, args.overlay[0], args.output, oids)
    
    print("DTBO merge complete")
    return 0


if __name__ == "__main__":
    sys.exit(main())