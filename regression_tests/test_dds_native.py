import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest


def dds(width, height, payload, fourcc=b"DXT1", bits=32,
        masks=(0xFF0000, 0xFF00, 0xFF, 0xFF000000), pitch=0):
    header = bytearray(128)
    header[:4] = b"DDS "
    for offset, value in {
        4: 124, 8: 0x1007 | (8 if pitch else 0), 12: height, 16: width,
        20: pitch, 76: 32, 80: 4 if fourcc else 0x40,
        88: bits, 92: masks[0], 96: masks[1], 100: masks[2],
        104: masks[3], 108: 0x1000,
    }.items():
        struct.pack_into("<I", header, offset, value)
    header[84:88] = fourcc or b"\0" * 4
    return bytes(header) + payload


def color_block(first=0xF800, second=0, selectors=0):
    return struct.pack("<HHI", first, second, selectors)


class DDSNativeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.decoder = os.environ.get("DDS_TEST_DECODER")
        if not cls.decoder:
            raise unittest.SkipTest("Build dds_decode and set DDS_TEST_DECODER")
        if not Path(cls.decoder).is_file():
            raise RuntimeError("DDS_TEST_DECODER does not name an executable")

    def decode(self, data, expected_error=None):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "input.dds"
            output = Path(directory) / "output.rgba"
            source.write_bytes(data)
            result = subprocess.run(
                [self.decoder, str(source), str(output)],
                capture_output=True, text=True, check=False,
            )
            if expected_error:
                self.assertEqual(result.returncode, 1, result.stderr)
                self.assertIn(expected_error, result.stderr)
                self.assertFalse(output.exists())
                return None
            self.assertEqual(result.returncode, 0, result.stderr)
            width, height = map(int, result.stdout.split())
            pixels = output.read_bytes()
            self.assertEqual(len(pixels), width * height * 4)
            return width, height, pixels

    def test_dxt1_rgb_and_green_bit_replication(self):
        self.assertEqual(
            self.decode(dds(4, 4, color_block()))[2],
            bytes((255, 0, 0, 255)) * 16,
        )
        self.assertEqual(
            self.decode(dds(4, 4, color_block(32 << 5)))[2],
            bytes((0, 130, 0, 255)) * 16,
        )

    def test_dxt1_interpolation_and_transparency(self):
        self.assertEqual(
            self.decode(dds(4, 4, color_block(selectors=0xAAAAAAAA)))[2],
            bytes((170, 0, 0, 255)) * 16,
        )
        self.assertEqual(
            self.decode(dds(4, 4, color_block(0, 0xFFFF, 0xFFFFFFFF)))[2],
            bytes(64),
        )

    def test_dxt3_uses_four_colors_even_with_reversed_endpoints(self):
        payload = b"\x88" * 8 + color_block(0, 0xFFFF, 0xFFFFFFFF)
        self.assertEqual(
            self.decode(dds(4, 4, payload, b"DXT3"))[2],
            bytes((170, 170, 170, 136)) * 16,
        )

    def test_dxt5_alpha_interpolation_and_terminal_values(self):
        for first, second, selector, alpha in ((10, 2, 2, 8), (10, 20, 6, 0), (10, 20, 7, 255)):
            with self.subTest(selector=selector, endpoints=(first, second)):
                indices = sum(selector << (i * 3) for i in range(16))
                payload = bytes((first, second)) + indices.to_bytes(6, "little") + color_block()
                self.assertEqual(
                    self.decode(dds(4, 4, payload, b"DXT5"))[2],
                    bytes((255, 0, 0, alpha)) * 16,
                )

    def test_dxt2_and_dxt4_unpremultiply_rgb(self):
        for fourcc, alpha_data, red, alpha in (
            (b"DXT2", b"\x88" * 8, 248, 136),
            (b"DXT4", bytes((128, 0)) + bytes(6), 255, 128),
        ):
            with self.subTest(fourcc=fourcc):
                pixels = self.decode(dds(4, 4, alpha_data + color_block(0x8000), fourcc))[2]
                self.assertEqual(pixels, bytes((red, 0, 0, alpha)) * 16)

    def test_small_and_non_block_aligned_images(self):
        width, height, pixels = self.decode(
            dds(5, 1, color_block() + color_block(0x07E0))
        )
        self.assertEqual((width, height), (5, 1))
        self.assertEqual(pixels, bytes((255, 0, 0, 255)) * 4 + bytes((0, 255, 0, 255)))
        self.assertEqual(
            self.decode(dds(1, 1, color_block()))[2], bytes((255, 0, 0, 255))
        )

    def test_uncompressed_bgra_masks(self):
        self.assertEqual(
            self.decode(dds(1, 1, bytes((3, 2, 1, 4)), fourcc=None))[2],
            bytes((1, 2, 3, 4)),
        )

    def test_rgb24_row_pitch_and_absent_alpha(self):
        data = dds(1, 2, bytes((3, 2, 1, 99, 6, 5, 4, 99)),
                   fourcc=None, bits=24, masks=(0xFF0000, 0xFF00, 0xFF, 0), pitch=4)
        self.assertEqual(self.decode(data)[2], bytes((1, 2, 3, 255, 4, 5, 6, 255)))

    def test_rgb565(self):
        data = dds(1, 1, struct.pack("<H", 0xF800), fourcc=None,
                   bits=16, masks=(0xF800, 0x07E0, 0x001F, 0))
        self.assertEqual(self.decode(data)[2], bytes((255, 0, 0, 255)))

    def test_invalid_headers_formats_and_dimensions(self):
        for data, error in (
            (b"DDS ", "header"),
            (b"BAD " + dds(4, 4, color_block())[4:], "header"),
            (dds(0, 4, color_block()), "dimensions"),
            (dds(0xFFFFFFFF, 4, color_block()), "dimensions"),
            (dds(16384, 16384, color_block()), "dimensions"),
            (dds(4, 4, bytes(16), b"DX10"), "compression"),
        ):
            with self.subTest(error=error, size=len(data)):
                self.decode(data, expected_error=error)

    def test_truncated_payload_and_bad_masks_or_pitch(self):
        for data, error in (
            (dds(4, 4, bytes(7)), "truncated"),
            (dds(4, 4, bytes(15), b"DXT5"), "truncated"),
            (dds(1, 1, bytes(3), fourcc=None), "truncated"),
            (dds(1, 1, bytes(4), fourcc=None, pitch=3), "pitch"),
            (dds(1, 1, bytes(4), fourcc=None, masks=(0xFF, 0xFF, 0xFF00, 0)), "masks"),
            (dds(1, 1, bytes(4), fourcc=None, masks=(0x55, 0xFF00, 0xFF0000, 0)), "masks"),
        ):
            with self.subTest(error=error):
                self.decode(data, expected_error=error)

    def test_nod_texture_index(self):
        subprocess.run([self.decoder, "--index-test"], check=True)
