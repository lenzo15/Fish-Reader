# SPDX-License-Identifier: GPL-3.0-or-later
"""Build a deterministic, allow-listed archive, independent of Git or local build products."""
from pathlib import Path, PurePosixPath
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo
import hashlib
import os
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PREFIX = "FishReader-1.0.1"
MANIFEST = "SOURCE_MANIFEST.txt"


def main():
    names = (ROOT / MANIFEST).read_text(encoding="utf-8").splitlines()
    names = [line.strip() for line in names if line.strip() and not line.startswith("#")]
    if len(names) != len({name.casefold() for name in names}):
        raise ValueError("Duplicate manifest entry")
    out_dir = ROOT / "dist"
    out_dir.mkdir(exist_ok=True)
    target = out_dir / (PREFIX + "-source.zip")
    with tempfile.NamedTemporaryFile(dir=out_dir, delete=False) as tmp:
        temporary = Path(tmp.name)
    try:
        with ZipFile(temporary, "w") as archive:
            for name in sorted(names):
                rel = PurePosixPath(name)
                # Even a mistaken allow-list edit must not ship a build tree or reader profile.
                if (any(p.casefold() in {"bin", "build", "dist", "out", ".git", "__pycache__"} for p in rel.parts)
                        or rel.suffix.casefold() in {".exe", ".dll", ".obj", ".o", ".pdb", ".msi", ".zip", ".lnk", ".pyc"}
                        or rel.name.casefold() == "state.bin"):
                    raise ValueError("Generated or private file is not release source: " + name)
                src = ROOT / name
                if (rel.is_absolute() or ".." in rel.parts or ":" in name or "\\" in name
                        or src.resolve() != src or not src.is_file()):
                    raise ValueError("Unsafe or missing manifest entry: " + name)
                info = ZipInfo(PREFIX + "/" + name, (1980, 1, 1, 0, 0, 0))
                info.create_system = 3
                info.external_attr = 0o100644 << 16
                archive.writestr(info, src.read_bytes(), compress_type=ZIP_DEFLATED, compresslevel=9)
        os.replace(temporary, target)
    finally:
        if temporary.exists():
            temporary.unlink()
    print(f"{len(names)} files: {target}")
    print(f"SHA256 {hashlib.sha256(target.read_bytes()).hexdigest()}")


if __name__ == "__main__":
    main()
