import binascii
import random
import struct
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'linux'))
from heatshrink_codec import encode, packet
from zero_rle import encode as rle_encode, decode as rle_decode, upload_packet as rle_packet
from profiles import validate


def reference_decode(packed,length):
    bits=''.join(format(b,'08b') for b in packed);cursor=0;out=bytearray()
    def take(n):
        nonlocal cursor
        assert cursor+n<=len(bits)
        value=int(bits[cursor:cursor+n],2);cursor+=n;return value
    while len(out)<length:
        if take(1):out.append(take(8))
        else:
            distance,count=take(12)+1,take(4)+1
            for _ in range(count):out.append(out[-distance] if distance<=len(out) else 0)
        assert len(out)<=length
    assert len(bits)-cursor<8 and not any(b=='1' for b in bits[cursor:])
    return bytes(out)


class CompressionTests(unittest.TestCase):
    def test_zero_run_roundtrip_and_rejects_bad_blocks(self):
        rng=random.Random(2026)
        for size in (1,2,3,127,128,129,4093,4096):
            for data in (bytes(size), rng.randbytes(size), (b'X\0\0Y\0\0\0Z'*600)[:size]):
                self.assertEqual(rle_decode(rle_encode(data),size),data)
        data=bytes(128)
        self.assertLess(len(rle_packet(data)),len(data))
        for packed,length in ((b'\x00',1),(b'\xff',127),(b'\xff',129),
                              (b'\x00X\x00',1)):
            with self.assertRaises(ValueError):rle_decode(packed,length)

    def test_encoder_roundtrip_and_reset(self):
        rng=random.Random(87)
        for size in (1,15,128,512,2048,4096):
            for data in (rng.randbytes(size),b'\0'*size,b'X'*size):
                packed=encode(data)
                self.assertEqual(reference_decode(packed,size),data)
                self.assertEqual(encode(data),packed)

    def test_raw_fallback_and_decoded_checksum(self):
        self.assertIsNone(packet(random.Random(20).randbytes(4096)))
        self.assertIsNone(packet(b'A'))
        data=b'\0'*4096;packed=packet(data)
        size,checksum=struct.unpack('<HH',packed[:4])
        self.assertEqual((size,checksum),(4096,binascii.crc_hqx(data,0xffff)))
        self.assertLessEqual(len(packed),3072)
        self.assertEqual(reference_decode(packed[4:],size),data)

    def test_block_bounds(self):
        for data in (b'',b'A'*4097):
            with self.assertRaises(ValueError):encode(data)

    def test_profile_default_and_toggle(self):
        self.assertTrue(validate({})['compression'])
        self.assertFalse(validate({'compression':False})['compression'])
        with self.assertRaises(ValueError):validate({'compression':'false'})
