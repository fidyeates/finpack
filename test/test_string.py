#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
test_string.py

test_string.py Module Docstring
"""

__version__ = 0.1
__author__  = "Fin"

# Stdlib Imports

# Third Party Imports

# DSP Imports
import finpack


@finpack.Compile
class StringTest(finpack.Message):
    message = finpack.STRING_TYPE(0)
    id = finpack.uCHAR_TYPE(1)


packed = StringTest.pack("Hello World", 123)
print [packed]
unpacked = StringTest.unpack(packed)
print unpacked
