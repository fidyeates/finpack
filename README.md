# Project: finpack

(Pronounced thinpack) A very lightweight strict-typed message serialization package. 

## Installation

(via PyPi):

    sudo pip install finpack

(via Github):

    sudo python setup.py install

## Benchmarks: 

test  | pack | unpack->tuple | unpack->dict | unpack->namedtuple
------|------|---------------|--------------|-------------------
3 ints| 0.16us | 0.11us | 1.37us | 0.78us

## Usage

```python
import finpack

@finpack.Compile
class TestMessage(finpack.Message):
    message_id = finpack.uINT_TYPE(0)
    message_body = finpack.STRING_TYPE(1, 16)

packed = TestMessage.pack(1, 'Hello World!')
print packed
> '\x01\x00\x00\x00Hello World!\x00\x00\x00\x00'

unpacked = TestMessage.unpack(packed)
print unpacked
> (1, 'Hello World!\x00\x00\x00\x00')

print TestMessage.unpack_into_dict(packed)
> {'message_body': 'Hello World!\x00\x00\x00\x00', 'message_id': 1}

print TestMessage.unpack_inti_namedtuple(packed)
> TestMessage(message_id=1, message_body='Hello World!\x00\x00\x00\x00')
```