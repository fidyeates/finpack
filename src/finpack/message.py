#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
_base_message.py

Base message implementation
"""

__version__ = 0.1
__author__  = "Fin"

# Stdlib Imports
import finstruct
from collections import namedtuple

# Third Party Imports

# finpack Imports
import msg_types

__all__ = ["Message", "Compile"]

DEFAULT_INTERFACE = lambda x: x


class Message(object):
    USE_NAMEDTUPLES = True
    INTERFACE = None
    _ENDIANNESS = "<"  # little-endian
    STRUCT = None
    STRUCT_STRING = None

    def __init__(self, index):
        self.index = index

    #@classmethod
    def read(cls, raw_string):
        unpacked = cls.STRUCT.unpack(raw_string)
        if cls.INTERFACE is None:
            return unpacked
        return cls.INTERFACE(*unpacked)

    @classmethod
    def size(cls):
        if cls.STRUCT is None:
            return 0
        return cls.STRUCT.size

    @classmethod
    def write(cls, *out):
        return cls.STRUCT.pack(*out)

    @classmethod
    def _detect_attributes(cls):
        return dict(filter(_is_itype, cls.__dict__.items()))

    @staticmethod
    def generate_struct_string(attr_list):
        return "".join(map(lambda attr_tup: attr_tup[1].STRUCT_STRING, attr_list))

    @classmethod
    def compile(cls):
        attrs = cls._detect_attributes()
        sorted_attrs = sorted(map(lambda attr_tup: (attr_tup[1].index, attr_tup[1], attr_tup[0]), attrs.items()))
        cls.STRUCT_STRING = cls._ENDIANNESS + Message.generate_struct_string(sorted_attrs)

        _struct = finstruct.Struct(cls.STRUCT_STRING)
        cls.STRUCT = _struct
        _unpack = _struct.unpack
        _pack = _struct.pack

        # Read Methods
        #_read = lambda raw_string: _unpack(raw_string)
        cls.unpack = _unpack  # _read

        interface = namedtuple(cls.__name__, map(lambda attr_tup: attr_tup[2], sorted_attrs))
        _read_into_tuple = lambda raw_string: interface(*_unpack(raw_string))
        cls.unpack_into_namedtuple = staticmethod(_read_into_tuple)

        field_names = map(lambda attr_tup: attr_tup[2], sorted_attrs)
        _read_into_dict = lambda raw_string: dict(zip(field_names, _unpack(raw_string)))
        cls.unpack_into_dict = staticmethod(_read_into_dict)

        # Write Methods
        #_write = lambda *out: _pack(*out)
        cls.pack = _pack  # staticmethod(_write)
        cls.pack_from_string = _pack

_is_itype = lambda attr_tup: isinstance(attr_tup[1], (msg_types._BASE_TYPE, Message))


class Compile(object):

    def __new__(self, cls):
        cls.compile()
        return cls
