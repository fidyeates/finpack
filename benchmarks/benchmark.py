import finpack
import timeit
from random import randint

@finpack.Compile
class BenchmarkMessage(finpack.Message):
    id = finpack.uINT_TYPE(0)
    value1 = finpack.uINT_TYPE(1)
    value2 = finpack.uINT_TYPE(2)


ntimes = 1000000
t = timeit.timeit('randint(1, 1000), randint(1, 1000), randint(1, 1000)', setup="from __main__ import BenchmarkMessage; from random import randint; _pack = BenchmarkMessage.pack; _unpack = BenchmarkMessage.unpack", number=ntimes)
print "Time Taken: %.3f second, per call: %.3fus" % (t, (t * 1000000.0 / ntimes))
t = timeit.timeit('_unpack(_pack(1, 1, 1))', setup="from __main__ import BenchmarkMessage; from random import randint; _pack = BenchmarkMessage.pack; _unpack = BenchmarkMessage.unpack", number=ntimes)
print "Time Taken: %.3f second, per call: %.3fus" % (t, (t * 1000000.0 / ntimes))