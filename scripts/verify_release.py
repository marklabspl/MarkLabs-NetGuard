"""Verify active release hashes and ZIP integrity for both controllers."""

from hashlib import sha256
from pathlib import Path
from zipfile import ZipFile

ROOT = Path(__file__).resolve().parents[1]


def digest(path: Path) -> str:
    value = sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def verify(component: str) -> None:
    firmware = ROOT / component / "firmware"
    checksum_files = list(firmware.glob("SHA256SUMS-*.txt"))
    if len(checksum_files) != 1:
        raise RuntimeError(f"{component}: expected one active SHA256SUMS file")
    for line in checksum_files[0].read_text(encoding="ascii").splitlines():
        expected, name = line.split(maxsplit=1)
        image = firmware / name.strip()
        if digest(image) != expected.lower():
            raise RuntimeError(f"{component}: checksum mismatch for {image.name}")
    for archive in firmware.glob("*.zip"):
        with ZipFile(archive) as package:
            names = package.namelist()
            if len(names) != len(set(names)):
                raise RuntimeError(f"{component}: duplicate ZIP entries in {archive.name}")
            for name in names:
                if not name.endswith(".bin"):
                    continue
                disk_image = firmware / Path(name).name
                if disk_image.exists() and sha256(package.read(name)).hexdigest() != digest(disk_image):
                    raise RuntimeError(f"{component}: ZIP image mismatch for {name}")


verify("master")
verify("power-module")
print("PASS: active release checksums and ZIP contents")
