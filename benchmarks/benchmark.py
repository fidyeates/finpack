import finpack
import timeit
from random import randint


@finpack.Compile
class BenchmarkMessage(finpack.Message):
    id = finpack.uINT_TYPE(0)
    value1 = finpack.uINT_TYPE(1)
    value2 = finpack.uINT_TYPE(2)

_pack = BenchmarkMessage.pack
_unpack = BenchmarkMessage.unpack
_unpack_dict = BenchmarkMessage.unpack_into_dict
_unpack_namedtuple = BenchmarkMessage.unpack_into_namedtuple


def test_function(name, data, iterations=1000000):
    call = "%s(%s)" % (name, data)
    setup = "from __main__ import {}".format(name)
    time_taken = timeit.timeit(call, setup=setup, number=iterations)
    print ("%s with input '%r' calls: {:,}" % (name, data)).format(iterations)
    print "Time Taken: %.3fs, Per Call: %.3fus" % (time_taken, time_taken * 1000000.0 / iterations)


def test_basic():
    print "Basic Data Benchmarks"
    data_in = "*(100022004, 100022004, 100022004)"
    data_out = "'%s'" % _pack(100022004, 100022004, 100022004)
    test_function("_pack", data_in)
    test_function("_unpack", data_out)
    test_function("_unpack_dict", data_out)
    test_function("_unpack_namedtuple", data_out)


if __name__ == '__main__':
    test_basic()
