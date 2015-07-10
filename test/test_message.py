#!/usr/bin/env python
# -*- coding: utf-8 -*-

__version__ = 0.1
__author__  = "Fin"

# Stdlib Imports
import sys
import unittest
import finstruct

# Third Party Imports

# DSP Imports
import finpack


@finpack.Compile
class MulipleTest(finpack.Message):
    uCHAR   = finpack.uCHAR_TYPE(0)
    CHAR    = finpack.CHAR_TYPE(1)
    SHORT   = finpack.SHORT_TYPE(2)
    uSHORT  = finpack.uSHORT_TYPE(3)
    INT     = finpack.INT_TYPE(4)
    uINT    = finpack.uINT_TYPE(5)
    LONG    = finpack.LONG_TYPE(6)
    uLONG   = finpack.uLONG_TYPE(7)
    FLOAT   = finpack.FLOAT_TYPE(8)
    DOUBLE  = finpack.DOUBLE_TYPE(9)
    STRING  = finpack.STRING_TYPE(12)


class Test_StringMultiple(unittest.TestCase):

    def test_pack_multiple(self):
        data = (0, 0, 0, 0, 0, 0, 0, 0, 13535, 0, "Hello World")
        packed = MulipleTest.pack(*data)
        self.assertEquals(MulipleTest.unpack(packed), data)


@finpack.Compile
class TestIntMessage(finpack.Message):
    value = finpack.INT_TYPE(0)


class Test_IntMessage(unittest.TestCase):

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

    def test_pack_int_out_of_range(self):
        # TODO
        value = 2 ** 32 + 1
        with self.assertRaises(finstruct.error):
            TestIntMessage.pack(value)


@finpack.Compile
class StringTest(finpack.Message):
    message = finpack.STRING_TYPE(0)
    id = finpack.uCHAR_TYPE(1)


class Test_StringMessage(unittest.TestCase):

    def test_pack_string(self):
        message = "Hello World"
        packed = StringTest.pack(message, 1)
        self.assertEquals(StringTest.unpack_into_namedtuple(packed).message, message)

    def test_pack_empty_string(self):
        message = ""
        packed = StringTest.pack(message, 1)
        self.assertEquals(StringTest.unpack_into_namedtuple(packed).message, message)

    def test_pack_string_null_bytes(self):
        message = "\x00\x00\x00\x00"
        packed = StringTest.pack(message, 1)
        self.assertEquals(StringTest.unpack_into_namedtuple(packed).message, message)


unittest.main()
