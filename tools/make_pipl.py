"""Generuje klasyczny binarny zasob PiPL (typ "PiPL", id 16000) dla filtra.

Photoshop i Photoshop Elements czytaja tez PiPL w formacie JSON (res/pipl.json), ale Affinity ma tylko
parser binarnego PiPL (PhotoshopPluginFile::LoadPiPL), wiec wariant dla Affinity korzysta z tego pliku.

Format (little-endian): int32 wersja=1, 2 bajty zer, int32 liczba wlasciwosci, potem wlasciwosci:
  '8BIM' (bajty "MIB8"), klucz (bajty odwrocone), int32 id=0, int32 dlugosc (z dopelnieniem do 4), dane, dopelnienie.
Uklad zweryfikowany z dzialajacym filtrem Zetmar Kolor zainstalowanym w Photoshopie 2026 (test --selftest).

Uzycie: python tools/make_pipl.py res/pipl_classic.bin
        python tools/make_pipl.py --selftest
"""
import struct
import sys


def pad4(b: bytes) -> bytes:
    return b + b"\0" * (-len(b) % 4)


def prop(key: str, data: bytes) -> bytes:
    data = pad4(data)
    return b"MIB8" + key.encode("ascii")[::-1] + struct.pack("<ii", 0, len(data)) + data


def pstr(s: str) -> bytes:
    raw = s.encode("cp1252")
    assert len(raw) < 256
    return bytes([len(raw)]) + raw


def build(name: str, category: str, component: str,
          enable: str = "in (PSHOP_ImageMode, RGBMode, RGB48Mode) && PSHOP_ImageDepth != 32") -> bytes:
    fici = bytes([
        1, 1, 2, 0,   # 1: plaski obraz bez zaznaczenia
        1, 1, 2, 0,   # 2: plaski obraz z zaznaczeniem
        0, 0, 2, 0,   # 3: zaznaczenie plywajace: nieobslugiwane
        1, 1, 2, 0,   # 4: edytowalna przezroczystosc, bez zaznaczenia
        1, 1, 2, 0,   # 5: edytowalna przezroczystosc, z zaznaczeniem
        1, 1, 2, 0,   # 6: chroniona przezroczystosc, bez zaznaczenia
        1, 1, 2, 0,   # 7: chroniona przezroczystosc, z zaznaczeniem
    ])
    props = [
        prop("kind", b"MFB8"),
        prop("name", pstr(name)),
        prop("catg", pstr(category)),
        prop("vers", struct.pack("<I", 4 << 16)),
        prop("cmpt", struct.pack("<I", 1 << 16) + component.encode("cp1252") + b"\0"),
        prop("8664", b"PluginMain\0"),
        prop("mode", bytes([0x10, 0x10, 0, 0])),          # RGB + RGB48
        prop("enbl", enable.encode("ascii") + b"\0"),
        prop("ms23", struct.pack("<ii", 2000000, 2000000)),
        prop("pmsa", struct.pack("<I", 1)),               # MonitorScalingAware; klucz "ms23" daje w pliku bajty "32sm" jak w referencji
        prop("flly", struct.pack("<I", 0x80)),            # FilterLayerSupport
        prop("fici", fici),
    ]
    # Naglowek (10 B) zgodny z zasobem generowanym przez narzedzia Adobe: wersja, 2 bajty zer, liczba wlasciwosci.
    return struct.pack("<I", 1) + bytes(2) + struct.pack("<I", len(props)) + b"".join(props)


KOLOR_REFERENCE = (
    "0100000000000c0000004d494238646e696b00000000040000004d4642384d494238656d616e00000000200000001e4b6f6c6f7220"
    "286b727a7977652c204c55542c20746f6e696e67292e2e2e004d494238677461630000000008000000065a65746d6172004d494238"
    "737265760000000004000000000004004d49423874706d630000000014000000000001005a65746d6172204b6f6c6f72000000004d"
    "49423834363638000000000c000000506c7567696e4d61696e00004d49423865646f6d0000000004000000101000004d4942386c62"
    "6e650000000044000000696e20285053484f505f496d6167654d6f64652c205247424d6f64652c2052474234384d6f646529202626"
    "205053484f505f496d616765446570746820213d20333200004d4942383332736d000000000800000080841e0080841e004d494238"
    "61736d700000000004000000010000004d494238796c6c660000000004000000800000004d49423869636966000000001c00000001"
    "010200010102000000020001010200010102000101020001010200"
)


def selftest() -> int:
    got = build("Kolor (krzywe, LUT, toning)...", "Zetmar", "Zetmar Kolor").hex()
    if got == KOLOR_REFERENCE:
        print("selftest OK: bajt w bajt zgodne z dzialajacym PiPL Zetmar Kolor")
        return 0
    n = next((i for i in range(min(len(got), len(KOLOR_REFERENCE))) if got[i] != KOLOR_REFERENCE[i]), None)
    print("selftest FAIL, pierwsza roznica na pozycji hex", n, "dlugosci", len(got), len(KOLOR_REFERENCE))
    return 1


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--selftest":
        sys.exit(selftest())
    out = sys.argv[1] if len(sys.argv) > 1 else "res/pipl_classic.bin"
    data = build("Podpis (Watermark)...", "Zetmar", "Zetmar Watermark")
    open(out, "wb").write(data)
    print("zapisano", out, len(data), "B")
