#!/usr/bin/env python3
"""Guard the shipped content against the two ways RC_BasicDemo.umap once shipped broken.

Both defects came from copying a file out of the Pro plugin instead of authoring it here, and
neither is visible in a diff - a .umap is opaque binary, so review cannot catch either one.

1. Pro identifiers (issue #2). The copied map imported
   /Script/RollbackCorePro.RollbackDemoEnvironment and still carried
   /RollbackCorePro/Maps/RC_BasicDemo as its own package path. Neither resolves in this plugin,
   so the engine dropped the entire actor export rather than loading it partially, and the level
   opened with zero actors and a black viewport.

2. Engine version. That same file was saved by UE 5.7 (FileVersionUE5 1018). Unreal packages are
   forward-compatible ONLY: an engine older than the one that saved a package reports it as
   MISSING rather than as a version mismatch, which reads like a packaging bug and sends you
   looking in the wrong place. Every shipped asset must be authored on the support floor, and a
   newer editor cannot save one downward - an asset above the floor has to be recreated there.

Usage:  python Tools/check-shipped-content.py [--floor 1013] [--path Content]
Exit:   0 clean, 1 something would ship broken.

FileVersionUE5 ceilings, from each engine's EUnrealEngineObjectUE5Version::AUTOMATIC_VERSION:
    UE 5.5 -> 1013     UE 5.6 -> 1017     UE 5.7 -> 1018
"""
import argparse
import struct
import sys
from pathlib import Path

PACKAGE_MAGIC = 0x9E2A83C1
FOREIGN = b"RollbackCorePro"


def file_version_ue5(path):
    """Return the package's FileVersionUE5, or None if the file is not an Unreal package."""
    with path.open("rb") as handle:
        head = handle.read(20)
    if len(head) < 20:
        return None
    tag, _legacy, _legacy_ue3, _ue4, ue5 = struct.unpack("<Iiiii", head)
    return ue5 if tag == PACKAGE_MAGIC else None


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--floor", type=int, default=1013,
                        help="highest allowed FileVersionUE5 (default 1013 = UE 5.5)")
    parser.add_argument("--path", default="Content",
                        help="directory to scan, relative to the repo root (default Content)")
    args = parser.parse_args()

    root = Path(__file__).resolve().parent.parent
    target = (root / args.path).resolve()

    # Never pass quietly on a path that is not there. A scan that cannot find its target must
    # fail, not report OK - that is exactly how a guard like this reports green while looking
    # at nothing.
    if not target.is_dir():
        print("FAILED - not a directory: %s" % target)
        return 1

    checked, too_new, foreign = 0, [], []
    for asset in sorted(target.rglob("*")):
        if asset.suffix.lower() not in (".uasset", ".umap"):
            continue
        version = file_version_ue5(asset)
        if version is None:
            continue
        checked += 1
        try:
            name = asset.relative_to(root).as_posix()
        except ValueError:
            name = asset.as_posix()
        if version > args.floor:
            too_new.append((version, name))
        if FOREIGN in asset.read_bytes():
            foreign.append(name)

    print("Checked %d asset(s) under %s" % (checked, target))

    # A guard that inspected nothing has proved nothing.
    if checked == 0:
        print("FAILED - no assets were examined, so nothing was proved. The scan is broken.")
        return 1

    if foreign:
        print("\nFAILED - these carry Pro identifiers and will not resolve in this plugin:")
        for name in foreign:
            print("  %s" % name)
        print("\nRenaming the file is not enough: the module import and the package's own mount")
        print("path are stored inside it. Recreate the asset here, against /Script/RollbackCore.")

    if too_new:
        print("\nFAILED - these were saved by a newer engine than the support floor (%d):" % args.floor)
        for version, name in sorted(too_new):
            print("  %d  %s" % (version, name))
        print("\nAn engine at the floor reports these as MISSING, not as a version error. Recreate")
        print("them on the floor engine - a newer editor cannot save a package downward.")

    if foreign or too_new:
        return 1

    print("OK - every shipped asset is authored on the floor and free of Pro identifiers.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
