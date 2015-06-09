#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
test_message_single_level.py
"""
__author__  = "Fin"

# Stdlib Imports
import sys
import unittest
import struct

# Third Party Imports

# finpack Imports
import finpack


@finpack.Compile
class TestIntMessage(finpack.Message):
    value = finpack.INT_TYPE(0)


class Test_IntMessage(unittest.TestCase):

    def setUp(self):
        pass

    def test_pack_int_0(self):
        packed = TestIntMessage.pack(0)
        self.assertEquals(TestIntMessage.unpack_into_namedtuple(packed).value, 0)

    def test_pack_int_1(self):
        packed = TestIntMessage.pack(1)
        self.assertEquals(TestIntMessage.unpack_into_namedtuple(packed).value, 1)

    def test_pack_int_maxint(self):
        value = 2147483647
        packed = TestIntMessage.pack(value)
        self.assertEquals(TestIntMessage.unpack_into_namedtuple(packed).value, value)

    def test_pack_int_minint(self):
        value = -2147483648
        packed = TestIntMessage.pack(value)
        self.assertEquals(TestIntMessage.unpack_into_namedtuple(packed).value, value)

    def test_pack_int_out_of_range(self):
        value = 2 ** 32 + 1
        with self.assertRaises(struct.error):
            TestIntMessage.pack(value)


if __name__ == '__main__':
    unittest.main()
