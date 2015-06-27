from finstruct import Struct
s = Struct("<HSS")
packed = s.pack(1, "Hello", "World")
print [packed]
print s.unpack(packed)
