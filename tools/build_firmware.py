"""Build the Keil project, preserving administrator, audit and password Flash pages."""
from pathlib import Path
import argparse
import hashlib
import shutil
import subprocess
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]


def run(command):
    result = subprocess.run(command, capture_output=True, encoding="utf-8", errors="replace")
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    return result.stdout + result.stderr


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--keil", default="E:/Keil5/ARM/ARMCC/bin")
    args = parser.parse_args()
    compiler = Path(args.keil)
    project = ROOT / "basic"
    output = project / "build"
    output.mkdir(exist_ok=True)
    project_xml = ET.parse(project / "led.uvprojx")
    for region in (".//IROM/Size", ".//OCR_RVCT4/Size"):
        regions = project_xml.findall(region)
        assert len(regions) == 1 and int(regions[0].text, 0) == 0xD400, \
            "Keil Flash linker range must stop before administrator storage"
    project_files = [p.text.replace("\\", "/") for p in project_xml.findall(".//FilePath")]
    sources = [p for p in project_files if p.lower().endswith((".c", ".s"))]
    assert sum(Path(s).name == "main.c" for s in sources) == 1
    assert all((project / s).is_file() for s in sources)
    inputs = [project / p for p in project_files] + [project / "led.uvprojx", Path(__file__)]
    hashes = {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    output_name = "locking"
    with tempfile.TemporaryDirectory(prefix="locking-build-", dir=output) as directory:
        temp = Path(directory)
        objects = []
        for name in sources:
            source = project / name
            obj = temp / (source.stem + ".o")
            if source.suffix == ".s":
                command = [str(compiler / "armasm.exe"), "--cpu", "Cortex-M3", "--apcs=interwork"]
            else:
                command = [str(compiler / "armcc.exe"), "--c99", "--cpu", "Cortex-M3",
                           "--apcs=interwork", "--split_sections", "-O1", "-c",
                           "-DSTM32F10X_MD", "-DUSE_STDPERIPH_DRIVER"]
                command += ["-I" + str(project / p) for p in ("start", "libraries", "user", "hardware", "system")]
            diagnostics = run(command + [str(source), "-o", str(obj)])
            if diagnostics.strip():
                print(diagnostics.strip())
            objects.append(obj)
        scatter = temp / "locking.sct"
        scatter.write_text(
            "LR_IROM1 0x08000000 0xD400 {\n ER_IROM1 0x08000000 0xD400 {\n"
            " startup_stm32f10x_md.o (RESET, +First)\n *(InRoot$$Sections)\n .ANY (+RO)\n }\n"
            " RW_IRAM1 0x20000000 0x5000 {\n .ANY (+RW +ZI)\n }\n}\n"
        )
        firmware = output / (output_name + ".axf")
        run([str(compiler / "armlink.exe"), "--cpu", "Cortex-M3", "--scatter", str(scatter),
             "--entry", "Reset_Handler", "--map", "--list", str(output / (output_name + ".map")),
             *(str(obj) for obj in objects), "-o", str(firmware)])
        hex_file = output / (output_name + ".hex")
        run([str(compiler / "fromelf.exe"), "--i32combined", "--output", str(hex_file), str(firmware)])
        # Check generated Intel HEX boundaries/checksums independently of the linker.
        base = 0
        for line in hex_file.read_text().splitlines():
            record = bytes.fromhex(line[1:])
            assert sum(record) % 256 == 0
            if record[3] == 4:
                base = int.from_bytes(record[4:6], "big") << 16
            elif record[3] == 0:
                start = base + int.from_bytes(record[1:3], "big")
                assert 0x08000000 <= start < start + record[0] <= 0x0800D400
        assert all(hashlib.sha256(p.read_bytes()).hexdigest() == h for p, h in hashes.items())
        # 与Keil工程标准Download路径保持一致，避免仍下载旧的Objects/led.axf。
        keil_output = project / "Objects"
        keil_output.mkdir(exist_ok=True)
        shutil.copy2(firmware, keil_output / "led.axf")
        shutil.copy2(hex_file, keil_output / "led.hex")
        manifest = ["LOCKING - main.c state machine; 1 Keypad PIN / 2 Dial PIN / A Admin",
                    "user/main.c compiled; project sources/headers unchanged during build",
                    "HEX SHA256 " + hashlib.sha256(hex_file.read_bytes()).hexdigest()]
        for source, source_hash in hashes.items():
            manifest.append(source_hash + " " + source.relative_to(ROOT).as_posix())
        (output / (output_name + ".manifest.txt")).write_text("\n".join(manifest) + "\n", encoding="utf-8")
        print(f"PASS: {len(objects)} Cortex-M3 translation units linked; HEX checksums and storage bounds verified")
        print(hex_file)


if __name__ == "__main__":
    main()
