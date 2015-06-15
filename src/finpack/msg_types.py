#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
msg_types.py

msg_types.py Module Docstring
"""

__version__ = 0.1
__author__  = "Fin"

# Stdlib Imports
import os

# Third Party Imports

# finpack Imports
from errors import InterfaceException

IMPLEMENTATION = os.environ.get("FINPACK_BACKEND", "struct")


class _BASE_TYPE(object):
    _STYPE = None

    def __init__(self, index, length=None):
        self.index = index
        self.length = length

    @property
    def STRUCT_STRING(self):
        return self._STYPE


class CHAR_TYPE(_BASE_TYPE):
    _STYPE = "b"


class uCHAR_TYPE(_BASE_TYPE):
    _STYPE = "B"


class SHORT_TYPE(_BASE_TYPE):
    _STYPE = "h"


class uSHORT_TYPE(_BASE_TYPE):
    _STYPE = "H"


class INT_TYPE(_BASE_TYPE):
    _STYPE = "i"


class uINT_TYPE(_BASE_TYPE):
    _STYPE = "I"


class LONG_TYPE(_BASE_TYPE):
    _STYPE = "l"


class uLONG_TYPE(_BASE_TYPE):
    _STYPE = "L"


class FLOAT_TYPE(_BASE_TYPE):
    _STYPE = "f"


class DOUBLE_TYPE(_BASE_TYPE):
    _STYPE = "q"


class uDOUBLE_TYPE(_BASE_TYPE):
    _STYPE = "Q"


class FSTRING_TYPE(_BASE_TYPE):
    _STYPE = "s"

    @property
    def STRUCT_STRING(self):
        if self.length is None:
            raise InterfaceException("String Type Requires A Length")
        return str(self.length) + "s"

if IMPLEMENTATION == "fin":
    class STRING_TYPE(_BASE_TYPE):
        _STYPE = "S"
else:
    STRING_TYPE = FSTRING_TYPE
