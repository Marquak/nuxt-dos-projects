#!/usr/bin/env python3
"""Build the CW7 one-file DOS EXE from a verified owner-supplied CW image."""

from __future__ import annotations

import argparse
import os
from pathlib import Path

from build_integrated import integrate
from build_patch import patch_game, sha256


HELPER_SHA256 = "adff25f7154439aa5fbf0bbe736d0823a688d6456f9fde6aef3aff065c89dfc8"
OUTPUT_SHA256 = "3f84a1fa160b3344deffdff3a2391234be2b1188323a205b7b952b3e552833f8"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("game", type=Path, help="game folder or its CW.EXE file")
    args = parser.parse_args()
    source_path = args.game / "CW.EXE" if args.game.is_dir() else args.game
    if source_path.name.upper() != "CW.EXE":
        parser.error("Select a game folder or a file named CW.EXE")
    game_dir = source_path.parent
    backup_path = game_dir / "CW.BAK"
    stage_path = game_dir / "CW7.TMP"
    try:
        occupied = {entry.name.upper() for entry in game_dir.iterdir()}
        if "CW.BAK" in occupied:
            raise ValueError("CW.BAK already exists; move that backup before patching")
        if "CW7.TMP" in occupied:
            raise ValueError("CW7.TMP already exists; move that file before patching")
        source = source_path.read_bytes()
        patched, variant = patch_game(source)
        helper = (Path(__file__).resolve().parent / "CW7HELP.BIN").read_bytes()
        if sha256(helper) != HELPER_SHA256:
            raise ValueError("CW7HELP.BIN is missing or has changed")
        result = integrate(patched, helper)
        if sha256(result) != OUTPUT_SHA256:
            raise ValueError("CW7 output hash mismatch")
        # Stage the entire image on the same filesystem before renaming the
        # existing EXE. The original remains in place if validation or writing
        # fails. Exclusive creation prevents clobbering an old temp file.
        try:
            with stage_path.open("xb") as output:
                output.write(result)
                output.flush()
                os.fsync(output.fileno())
        except OSError:
            stage_path.unlink(missing_ok=True)
            raise
        if sha256(stage_path.read_bytes()) != OUTPUT_SHA256:
            stage_path.unlink()
            raise ValueError("Staged output did not verify")
        # Rename only after the complete output has been checked. On any
        # install failure, put the original EXE back under its original name.
        moved_original = False
        try:
            source_path.rename(backup_path)
            moved_original = True
            stage_path.rename(source_path)
            if sha256(source_path.read_bytes()) != OUTPUT_SHA256:
                raise ValueError("Installed output did not verify")
        except (OSError, ValueError):
            if moved_original:
                source_path.unlink(missing_ok=True)
                backup_path.rename(source_path)
            stage_path.unlink(missing_ok=True)
            raise
    except (OSError, ValueError) as exc:
        parser.exit(1, f"Patch failed: {exc}\n")
    print(f"Input: {variant} ({len(source)} bytes; SHA-256 {sha256(source)})")
    print(f"Backup: {backup_path} (original EXE)")
    print(f"Output: {source_path} ({len(result)} bytes; SHA-256 {sha256(result)})")
    print("Keep the other game data files in that folder. Run CW from DOS.")


if __name__ == "__main__":
    main()
