# Stdlib Imports

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

data = (0, 0, 0, 0, 0, 0, 0, 0, 13535, 0, "Hello World")
packed = MulipleTest.pack(*data)


@finpack.Compile
class TestIntMessage(finpack.Message):
    value = finpack.INT_TYPE(0)


packed = TestIntMessage.pack(0)
nt = TestIntMessage.unpack_into_namedtuple(packed)
print [nt]
print [TestIntMessage.pack_from_namedtuple(nt)]
print [TestIntMessage.pack_from_dict({"value": 1})]


value = 2 ** 32 + 1
TestIntMessage.pack(value)


@finpack.Compile
class StringTest(finpack.Message):
    message = finpack.STRING_TYPE(0)
    id = finpack.uCHAR_TYPE(1)


message = "Hello World"
packed = StringTest.pack(message, 1)
StringTest.unpack_into_namedtuple(packed).message

message = ""
packed = StringTest.pack(message, 1)
StringTest.unpack_into_namedtuple(packed).message

message = "\x00\x00\x00\x00"
packed = StringTest.pack(message, 1)
StringTest.unpack_into_namedtuple(packed).message
