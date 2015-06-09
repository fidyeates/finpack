import finpack

@finpack.Compile
class TestMessage(finpack.Message):
    message_id = finpack.uINT_TYPE(0)
    message_body = finpack.STRING_TYPE(1, 16)

packed = TestMessage.pack(1, 'Hello World!')
unpacked = TestMessage.unpack(packed)
print unpacked
