/* struct module -- pack values into and (out of) strings */

/* New version supporting byte order, alignment and size options,
   character strings, and unsigned numbers */

#define PY_SSIZE_T_CLEAN

#include "Python.h"
#include "structseq.h"
#include "structmember.h"
#include <ctype.h>

static PyTypeObject PyStructType;

/* compatibility macros */
#if (PY_VERSION_HEX < 0x02050000)
typedef int Py_ssize_t;
#endif

/* If PY_STRUCT_OVERFLOW_MASKING is defined, the struct module will wrap all input
   numbers for explicit endians such that they fit in the given type, much
   like explicit casting in C. A warning will be raised if the number did
   not originally fit within the range of the requested type. If it is
   not defined, then all range errors and overflow will be struct.error
   exceptions. */

#define PY_STRUCT_OVERFLOW_MASKING 1

#ifdef PY_STRUCT_OVERFLOW_MASKING
static PyObject *pylong_ulong_mask = NULL;
static PyObject *pyint_zero = NULL;
#endif

/* If PY_STRUCT_FLOAT_COERCE is defined, the struct module will allow float
   arguments for integer formats with a warning for backwards
   compatibility. */

#define PY_STRUCT_FLOAT_COERCE 1

#ifdef PY_STRUCT_FLOAT_COERCE
#define FLOAT_COERCE "integer argument expected, got float"
#endif


/* The translation function for each format character is table driven */
typedef struct _formatdef {
    char format;
    Py_ssize_t size;
    Py_ssize_t alignment;
    PyObject* (*unpack)(const char *,
                const struct _formatdef *);
    int (*pack)(char *, PyObject *,
            const struct _formatdef *);
} formatdef;

typedef struct _formatcode {
    const struct _formatdef *fmtdef;
    Py_ssize_t offset;
    Py_ssize_t size;
} formatcode;

/* Struct object interface */

typedef struct {
    PyObject_HEAD
    Py_ssize_t s_size;
    Py_ssize_t s_len;
    formatcode *s_codes;
    PyObject *s_format;
    PyObject *weakreflist; /* List of weak references */
} PyStructObject;


#define PyStruct_Check(op) PyObject_TypeCheck(op, &PyStructType)
#define PyStruct_CheckExact(op) (Py_TYPE(op) == &PyStructType)


/* Exception */

static PyObject *StructError;


/* Define various structs to figure out the alignments of types */


typedef struct { char c; short x; } st_short;
typedef struct { char c; int x; } st_int;
typedef struct { char c; long x; } st_long;
typedef struct { char c; float x; } st_float;
typedef struct { char c; double x; } st_double;
typedef struct { char c; void *x; } st_void_p;

#define SHORT_ALIGN (sizeof(st_short) - sizeof(short))
#define INT_ALIGN (sizeof(st_int) - sizeof(int))
#define LONG_ALIGN (sizeof(st_long) - sizeof(long))
#define FLOAT_ALIGN (sizeof(st_float) - sizeof(float))
#define DOUBLE_ALIGN (sizeof(st_double) - sizeof(double))
#define VOID_P_ALIGN (sizeof(st_void_p) - sizeof(void *))

/* We can't support q and Q in native mode unless the compiler does;
   in std mode, they're 8 bytes on all platforms. */
#ifdef HAVE_LONG_LONG
typedef struct { char c; PY_LONG_LONG x; } s_long_long;
#define LONG_LONG_ALIGN (sizeof(s_long_long) - sizeof(PY_LONG_LONG))
#endif

#ifdef HAVE_C99_BOOL
#define BOOL_TYPE _Bool
typedef struct { char c; _Bool x; } s_bool;
#define BOOL_ALIGN (sizeof(s_bool) - sizeof(BOOL_TYPE))
#else
#define BOOL_TYPE char
#define BOOL_ALIGN 0
#endif

#define STRINGIFY(x)    #x

#ifdef __powerc
#pragma options align=reset
#endif

/* Helper to get a PyLongObject by hook or by crook.  Caller should decref. */

static PyObject *
get_pylong(PyObject *v)
{
    PyNumberMethods *m;

    assert(v != NULL);
    if (PyInt_Check(v))
        return PyLong_FromLong(PyInt_AS_LONG(v));
    if (PyLong_Check(v)) {
        Py_INCREF(v);
        return v;
    }
    m = Py_TYPE(v)->tp_as_number;
    if (m != NULL && m->nb_long != NULL) {
        v = m->nb_long(v);
        if (v == NULL)
            return NULL;
        if (PyLong_Check(v))
            return v;
        Py_DECREF(v);
    }
    PyErr_SetString(StructError,
            "cannot convert argument to long");
    return NULL;
}

/* Helper routine to get a Python integer and raise the appropriate error
   if it isn't one */

static int
get_long(PyObject *v, long *p)
{
    long x = PyInt_AsLong(v);
    if (x == -1 && PyErr_Occurred()) {
#ifdef PY_STRUCT_FLOAT_COERCE
        if (PyFloat_Check(v)) {
            PyObject *o;
            int res;
            PyErr_Clear();
            if (PyErr_WarnEx(PyExc_DeprecationWarning, FLOAT_COERCE, 2) < 0)
                return -1;
            o = PyNumber_Int(v);
            if (o == NULL)
                return -1;
            res = get_long(o, p);
            Py_DECREF(o);
            return res;
        }
#endif
        if (PyErr_ExceptionMatches(PyExc_TypeError))
            PyErr_SetString(StructError,
                    "required argument is not an integer");
        return -1;
    }
    *p = x;
    return 0;
}


/* Same, but handling unsigned long */

static int
get_ulong(PyObject *v, unsigned long *p)
{
    if (PyLong_Check(v)) {
        unsigned long x = PyLong_AsUnsignedLong(v);
        if (x == (unsigned long)(-1) && PyErr_Occurred())
            return -1;
        *p = x;
        return 0;
    }
    if (get_long(v, (long *)p) < 0)
        return -1;
    if (((long)*p) < 0) {
        PyErr_SetString(StructError,
                "unsigned argument is < 0");
        return -1;
    }
    return 0;
}

#ifdef HAVE_LONG_LONG

/* Same, but handling native long long. */

static int
get_longlong(PyObject *v, PY_LONG_LONG *p)
{
    PY_LONG_LONG x;

    v = get_pylong(v);
    if (v == NULL)
        return -1;
    assert(PyLong_Check(v));
    x = PyLong_AsLongLong(v);
    Py_DECREF(v);
    if (x == (PY_LONG_LONG)-1 && PyErr_Occurred())
        return -1;
    *p = x;
    return 0;
}

/* Same, but handling native unsigned long long. */

static int
get_ulonglong(PyObject *v, unsigned PY_LONG_LONG *p)
{
    unsigned PY_LONG_LONG x;

    v = get_pylong(v);
    if (v == NULL)
        return -1;
    assert(PyLong_Check(v));
    x = PyLong_AsUnsignedLongLong(v);
    Py_DECREF(v);
    if (x == (unsigned PY_LONG_LONG)-1 && PyErr_Occurred())
        return -1;
    *p = x;
    return 0;
}

#endif

#ifdef PY_STRUCT_OVERFLOW_MASKING

/* Helper routine to get a Python integer and raise the appropriate error
   if it isn't one */

#define INT_OVERFLOW "struct integer overflow masking is deprecated"

static int
get_wrapped_long(PyObject *v, long *p)
{
    if (get_long(v, p) < 0) {
        if (PyLong_Check(v) &&
            PyErr_ExceptionMatches(PyExc_OverflowError)) {
            PyObject *wrapped;
            long x;
            PyErr_Clear();
#ifdef PY_STRUCT_FLOAT_COERCE
            if (PyFloat_Check(v)) {
                PyObject *o;
                int res;
                PyErr_Clear();
                if (PyErr_WarnEx(PyExc_DeprecationWarning, FLOAT_COERCE, 2) < 0)
                    return -1;
                o = PyNumber_Int(v);
                if (o == NULL)
                    return -1;
                res = get_wrapped_long(o, p);
                Py_DECREF(o);
                return res;
            }
#endif
            if (PyErr_WarnEx(PyExc_DeprecationWarning, INT_OVERFLOW, 2) < 0)
                return -1;
            wrapped = PyNumber_And(v, pylong_ulong_mask);
            if (wrapped == NULL)
                return -1;
            x = (long)PyLong_AsUnsignedLong(wrapped);
            Py_DECREF(wrapped);
            if (x == -1 && PyErr_Occurred())
                return -1;
            *p = x;
        } else {
            return -1;
        }
    }
    return 0;
}

static int
get_wrapped_ulong(PyObject *v, unsigned long *p)
{
    long x = (long)PyLong_AsUnsignedLong(v);
    if (x == -1 && PyErr_Occurred()) {
        PyObject *wrapped;
        PyErr_Clear();
#ifdef PY_STRUCT_FLOAT_COERCE
        if (PyFloat_Check(v)) {
            PyObject *o;
            int res;
            PyErr_Clear();
            if (PyErr_WarnEx(PyExc_DeprecationWarning, FLOAT_COERCE, 2) < 0)
                return -1;
            o = PyNumber_Int(v);
            if (o == NULL)
                return -1;
            res = get_wrapped_ulong(o, p);
            Py_DECREF(o);
            return res;
        }
#endif
        wrapped = PyNumber_And(v, pylong_ulong_mask);
        if (wrapped == NULL)
            return -1;
        if (PyErr_WarnEx(PyExc_DeprecationWarning, INT_OVERFLOW, 2) < 0) {
            Py_DECREF(wrapped);
            return -1;
        }
        x = (long)PyLong_AsUnsignedLong(wrapped);
        Py_DECREF(wrapped);
        if (x == -1 && PyErr_Occurred())
            return -1;
    }
    *p = (unsigned long)x;
    return 0;
}

#define RANGE_ERROR(x, f, flag, mask) \
    do { \
        if (_range_error(f, flag) < 0) \
            return -1; \
        else \
            (x) &= (mask); \
    } while (0)

#else

#define get_wrapped_long get_long
#define get_wrapped_ulong get_ulong
#define RANGE_ERROR(x, f, flag, mask) return _range_error(f, flag)

#endif

/* Floating point helpers */

static PyObject *
unpack_float(const char *p,  /* start of 4-byte string */
             int le)         /* true for little-endian, false for big-endian */
{
    double x;

    x = _PyFloat_Unpack4((unsigned char *)p, le);
    if (x == -1.0 && PyErr_Occurred())
        return NULL;
    return PyFloat_FromDouble(x);
}

static PyObject *
unpack_double(const char *p,  /* start of 8-byte string */
              int le)         /* true for little-endian, false for big-endian */
{
    double x;

    x = _PyFloat_Unpack8((unsigned char *)p, le);
    if (x == -1.0 && PyErr_Occurred())
        return NULL;
    return PyFloat_FromDouble(x);
}

/* Helper to format the range error exceptions */
static int
_range_error(const formatdef *f, int is_unsigned)
{
    /* ulargest is the largest unsigned value with f->size bytes.
     * Note that the simpler:
     *     ((size_t)1 << (f->size * 8)) - 1
     * doesn't work when f->size == sizeof(size_t) because C doesn't
     * define what happens when a left shift count is >= the number of
     * bits in the integer being shifted; e.g., on some boxes it doesn't
     * shift at all when they're equal.
     */
    const size_t ulargest = (size_t)-1 >> ((SIZEOF_SIZE_T - f->size)*8);
    assert(f->size >= 1 && f->size <= SIZEOF_SIZE_T);
    if (is_unsigned)
        PyErr_Format(StructError,
            "'%c' format requires 0 <= number <= %zu",
            f->format,
            ulargest);
    else {
        const Py_ssize_t largest = (Py_ssize_t)(ulargest >> 1);
        PyErr_Format(StructError,
            "'%c' format requires %zd <= number <= %zd",
            f->format,
            ~ largest,
            largest);
    }
#ifdef PY_STRUCT_OVERFLOW_MASKING
    {
        PyObject *ptype, *pvalue, *ptraceback;
        PyObject *msg;
        int rval;
        PyErr_Fetch(&ptype, &pvalue, &ptraceback);
        assert(pvalue != NULL);
        msg = PyObject_Str(pvalue);
        Py_XDECREF(ptype);
        Py_XDECREF(pvalue);
        Py_XDECREF(ptraceback);
        if (msg == NULL)
            return -1;
        rval = PyErr_WarnEx(PyExc_DeprecationWarning,
                    PyString_AS_STRING(msg), 2);
        Py_DECREF(msg);
        if (rval == 0)
            return 0;
    }
#endif
    return -1;
}

static PyObject *
nu_char(const char *p, const formatdef *f)
{
    return PyString_FromStringAndSize(p, 1);
}

static PyObject *
nu_byte(const char *p, const formatdef *f)
{
    return PyInt_FromLong((long) *(signed char *)p);
}

static PyObject *
nu_ubyte(const char *p, const formatdef *f)
{
    return PyInt_FromLong((long) *(unsigned char *)p);
}

static int
np_byte(char *p, PyObject *v, const formatdef *f)
{
    long x;
    if (get_long(v, &x) < 0)
        return -1;
    if (x < -128 || x > 127){
        PyErr_SetString(StructError,
                "byte format requires -128 <= number <= 127");
        return -1;
    }
    *p = (char)x;
    return 0;
}

static int
np_ubyte(char *p, PyObject *v, const formatdef *f)
{
    long x;
    if (get_long(v, &x) < 0)
        return -1;
    if (x < 0 || x > 255){
        PyErr_SetString(StructError,
                "ubyte format requires 0 <= number <= 255");
        return -1;
    }
    *p = (char)x;
    return 0;
}

static int
np_char(char *p, PyObject *v, const formatdef *f)
{
    if (!PyString_Check(v) || PyString_Size(v) != 1) {
        PyErr_SetString(StructError,
                "char format require string of length 1");
        return -1;
    }
    *p = *PyString_AsString(v);
    return 0;
}

static PyObject *
bu_bool(const char *p, const formatdef *f)
{
    char x;
    memcpy((char *)&x, p, sizeof x);
    return PyBool_FromLong(x != 0);
}

static int
bp_bool(char *p, PyObject *v, const formatdef *f)
{
    char y; 
    y = PyObject_IsTrue(v);
    memcpy(p, (char *)&y, sizeof y);
    return 0;
}

static PyObject *
lu_int(const char *p, const formatdef *f)
{
    long x = 0;
    Py_ssize_t i = f->size;
    const unsigned char *bytes = (const unsigned char *)p;
    do {
        x = (x<<8) | bytes[--i];
    } while (i > 0);
    /* Extend the sign bit. */
    if (SIZEOF_LONG > f->size)
        x |= -(x & (1L << ((8 * f->size) - 1)));
    return PyInt_FromLong(x);
}

static PyObject *
lu_uint(const char *p, const formatdef *f)
{
    unsigned long x = 0;
    Py_ssize_t i = f->size;
    const unsigned char *bytes = (const unsigned char *)p;
    do {
        x = (x<<8) | bytes[--i];
    } while (i > 0);
    if (x <= LONG_MAX)
        return PyInt_FromLong((long)x);
    return PyLong_FromUnsignedLong((long)x);
}

static PyObject *
lu_longlong(const char *p, const formatdef *f)
{
#ifdef HAVE_LONG_LONG
    PY_LONG_LONG x = 0;
    Py_ssize_t i = f->size;
    const unsigned char *bytes = (const unsigned char *)p;
    do {
        x = (x<<8) | bytes[--i];
    } while (i > 0);
    /* Extend the sign bit. */
    if (SIZEOF_LONG_LONG > f->size)
        x |= -(x & ((PY_LONG_LONG)1 << ((8 * f->size) - 1)));
    if (x >= LONG_MIN && x <= LONG_MAX)
        return PyInt_FromLong(Py_SAFE_DOWNCAST(x, PY_LONG_LONG, long));
    return PyLong_FromLongLong(x);
#else
    return _PyLong_FromByteArray((const unsigned char *)p,
                      8,
                      1, /* little-endian */
                      1  /* signed */);
#endif
}

static PyObject *
lu_ulonglong(const char *p, const formatdef *f)
{
#ifdef HAVE_LONG_LONG
    unsigned PY_LONG_LONG x = 0;
    Py_ssize_t i = f->size;
    const unsigned char *bytes = (const unsigned char *)p;
    do {
        x = (x<<8) | bytes[--i];
    } while (i > 0);
    if (x <= LONG_MAX)
        return PyInt_FromLong(Py_SAFE_DOWNCAST(x, unsigned PY_LONG_LONG, long));
    return PyLong_FromUnsignedLongLong(x);
#else
    return _PyLong_FromByteArray((const unsigned char *)p,
                      8,
                      1, /* little-endian */
                      0  /* signed */);
#endif
}

static PyObject *
lu_float(const char *p, const formatdef *f)
{
    return unpack_float(p, 1);
}

static PyObject *
lu_double(const char *p, const formatdef *f)
{
    return unpack_double(p, 1);
}

static int
lp_int(char *p, PyObject *v, const formatdef *f)
{
    long x;
    Py_ssize_t i;
    if (get_wrapped_long(v, &x) < 0)
        return -1;
    i = f->size;
    if (i != SIZEOF_LONG) {
        if ((i == 2) && (x < -32768 || x > 32767))
            RANGE_ERROR(x, f, 0, 0xffffL);
#if (SIZEOF_LONG != 4)
        else if ((i == 4) && (x < -2147483648L || x > 2147483647L))
            RANGE_ERROR(x, f, 0, 0xffffffffL);
#endif
#ifdef PY_STRUCT_OVERFLOW_MASKING
        else if ((i == 1) && (x < -128 || x > 127))
            RANGE_ERROR(x, f, 0, 0xffL);
#endif
    }
    do {
        *p++ = (char)x;
        x >>= 8;
    } while (--i > 0);
    return 0;
}

static int
lp_uint(char *p, PyObject *v, const formatdef *f)
{
    unsigned long x;
    Py_ssize_t i;
    if (get_wrapped_ulong(v, &x) < 0)
        return -1;
    i = f->size;
    if (i != SIZEOF_LONG) {
        unsigned long maxint = 1;
        maxint <<= (unsigned long)(i * 8);
        if (x >= maxint)
            RANGE_ERROR(x, f, 1, maxint - 1);
    }
    do {
        *p++ = (char)x;
        x >>= 8;
    } while (--i > 0);
    return 0;
}

static int
lp_longlong(char *p, PyObject *v, const formatdef *f)
{
    v = get_pylong(v);
    if (v == NULL)
        return -1;
    long long x = PyLong_AsLongLong(v);
    memcpy(p, &x, sizeof(long long));
    Py_DECREF(v);
    return 0;
}

static int
lp_ulonglong(char *p, PyObject *v, const formatdef *f)
{
    v = get_pylong(v);
    if (v == NULL)
        return -1;
    unsigned long long x = PyLong_AsUnsignedLongLong(v);
    memcpy(p, &x, sizeof(unsigned long long));
    Py_DECREF(v);
    return 0;
}

static int
lp_float(char *p, PyObject *v, const formatdef *f)
{
    double x = PyFloat_AsDouble(v);
    if (x == -1 && PyErr_Occurred()) {
        PyErr_SetString(StructError,
                "required argument is not a float");
        return -1;
    }
    memcpy(p, &x, sizeof(float));
    Py_DECREF(v);
    return 0;
}

static int
lp_double(char *p, PyObject *v, const formatdef *f)
{
    double x = PyFloat_AsDouble(v);
    if (x == -1 && PyErr_Occurred()) {
        PyErr_SetString(StructError,
                "required argument is not a float");
        return -1;
    }
    Py_DECREF(v);
    memcpy(p, &x, sizeof(double));
    return 0;
}

static formatdef lilendian_table[] = {
    {'x',   1,      0,      NULL},
#ifdef PY_STRUCT_OVERFLOW_MASKING
    /* Native packers do range checking without overflow masking. */
    {'b',   1,      0,      nu_byte,    lp_int},
    {'B',   1,      0,      nu_ubyte,   lp_uint},
#else
    {'b',   1,      0,      nu_byte,    np_byte},
    {'B',   1,      0,      nu_ubyte,   np_ubyte},
#endif
    {'c',   1,      0,      nu_char,    np_char},
    {'s',   1,      0,      NULL},
    {'S',   1,      0,      NULL},
    {'p',   1,      0,      NULL},
    {'h',   2,      0,      lu_int,     lp_int},
    {'H',   2,      0,      lu_uint,    lp_uint},
    {'i',   4,      0,      lu_int,     lp_int},
    {'I',   4,      0,      lu_uint,    lp_uint},
    {'l',   4,      0,      lu_int,     lp_int},
    {'L',   4,      0,      lu_uint,    lp_uint},
    {'q',   8,      0,      lu_longlong,    lp_longlong},
    {'Q',   8,      0,      lu_ulonglong,   lp_ulonglong},
    {'?',   1,      0,      bu_bool,    bp_bool}, /* Std rep not endian dep,
        but potentially different from native rep -- reuse bx_bool funcs. */
    {'f',   4,      0,      lu_float,   lp_float},
    {'d',   8,      0,      lu_double,  lp_double},
    {0}
};


static const formatdef *
whichtable(char **pfmt)
{
    const char *fmt = (*pfmt)++; /* May be backed out of later */
    return lilendian_table;
}


/* Get the table entry for a format code */

static const formatdef *
getentry(int c, const formatdef *f)
{
    for (; f->format != '\0'; f++) {
        if (f->format == c) {
            return f;
        }
    }
    PyErr_SetString(StructError, "bad char in struct format");
    return NULL;
}


/* Align a size according to a format code */

static int
align(Py_ssize_t size, char c, const formatdef *e)
{
    if (e->format == c) {
        if (e->alignment) {
            size = ((size + e->alignment - 1)
                / e->alignment)
                * e->alignment;
        }
    }
    return size;
}


/* calculate the size of a format string */

static int
prepare_s(PyStructObject *self)
{
    const formatdef *f;
    const formatdef *e;
    formatcode *codes;

    const char *s;
    const char *fmt;
    char c;
    Py_ssize_t size, len, num, itemsize, x;

    fmt = PyString_AS_STRING(self->s_format);

    f = whichtable((char **)&fmt);

    s = fmt;
    size = 0;
    len = 0;
    while ((c = *s++) != '\0') {
        if (isspace(Py_CHARMASK(c)))
            continue;
        if ('0' <= c && c <= '9') {
            num = c - '0';
            while ('0' <= (c = *s++) && c <= '9') {
                x = num*10 + (c - '0');
                if (x/10 != num) {
                    PyErr_SetString(
                        StructError,
                        "overflow in item count");
                    return -1;
                }
                num = x;
            }
            if (c == '\0')
                break;
        }
        else
            num = 1;

        e = getentry(c, f);
        if (e == NULL)
            return -1;

        switch (c) {
            case 's': /* fall through */
            case 'p': len++; break;
            case 'x': break;
            default: len += num; break;
        }

        itemsize = e->size;
        size = align(size, c, e);
        x = num * itemsize;
        size += x;
        if (x/itemsize != num || size < 0) {
            PyErr_SetString(StructError,
                    "total struct size too long");
            return -1;
        }
    }

    /* check for overflow */
    if ((len + 1) > (PY_SSIZE_T_MAX / sizeof(formatcode))) {
        PyErr_NoMemory();
        return -1;
    }

    self->s_size = size;
    self->s_len = len;
    codes = PyMem_MALLOC((len + 1) * sizeof(formatcode));
    if (codes == NULL) {
        PyErr_NoMemory();
        return -1;
    }
    self->s_codes = codes;

    s = fmt;
    size = 0;
    while ((c = *s++) != '\0') {
        if (isspace(Py_CHARMASK(c)))
            continue;
        if ('0' <= c && c <= '9') {
            num = c - '0';
            while ('0' <= (c = *s++) && c <= '9')
                num = num*10 + (c - '0');
            if (c == '\0')
                break;
        }
        else
            num = 1;

        e = getentry(c, f);

        size = align(size, c, e);
        if (c == 's' || c == 'p') {
            codes->offset = size;
            codes->size = num;
            codes->fmtdef = e;
            codes++;
            size += num;
        } else if (c == 'x') {
            size += num;
        } else {
            while (--num >= 0) {
                codes->offset = size;
                codes->size = e->size;
                codes->fmtdef = e;
                codes++;
                size += e->size;
            }
        }
    }
    codes->fmtdef = NULL;
    codes->offset = size;
    codes->size = 0;

    return 0;
}

static PyObject *
s_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyObject *self;

    assert(type != NULL && type->tp_alloc != NULL);

    self = type->tp_alloc(type, 0);
    if (self != NULL) {
        PyStructObject *s = (PyStructObject*)self;
        Py_INCREF(Py_None);
        s->s_format = Py_None;
        s->s_codes = NULL;
        s->s_size = -1;
        s->s_len = -1;
    }
    return self;
}

static int
s_init(PyObject *self, PyObject *args, PyObject *kwds)
{
    PyStructObject *soself = (PyStructObject *)self;
    PyObject *o_format = NULL;
    int ret = 0;
    static char *kwlist[] = {"format", 0};

    assert(PyStruct_Check(self));

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "S:Struct", kwlist,
                     &o_format))
        return -1;

    Py_INCREF(o_format);
    Py_CLEAR(soself->s_format);
    soself->s_format = o_format;

    ret = prepare_s(soself);
    return ret;
}

static void
s_dealloc(PyStructObject *s)
{
    if (s->weakreflist != NULL)
        PyObject_ClearWeakRefs((PyObject *)s);
    if (s->s_codes != NULL) {
        PyMem_FREE(s->s_codes);
    }
    Py_XDECREF(s->s_format);
    Py_TYPE(s)->tp_free((PyObject *)s);
}

static PyObject *
s_unpack_internal(PyStructObject *soself, char *startfrom) {
    formatcode *code;
    Py_ssize_t i = 0;
    PyObject *result = PyTuple_New(soself->s_len);
    if (result == NULL)
        return NULL;

    for (code = soself->s_codes; code->fmtdef != NULL; code++) {
        PyObject *v;
        const formatdef *e = code->fmtdef;
        const char *res = startfrom + code->offset;
        if (e->format == 's') {
            v = PyString_FromStringAndSize(res, code->size);
        } else if (e->format == 'S') {
            Py_ssize_t size = *(unsigned char*)res;
            startfrom += size;
            v = PyString_FromStringAndSize(res + 1, size);
        } else if (e->format == 'p') {
            Py_ssize_t n = *(unsigned char*)res;
            if (n >= code->size)
                n = code->size - 1;
            v = PyString_FromStringAndSize(res + 1, n);
        } else {
            v = e->unpack(res, e);
        }
        if (v == NULL)
            goto fail;
        PyTuple_SET_ITEM(result, i++, v);
    }

    return result;
fail:
    Py_DECREF(result);
    return NULL;
}


PyDoc_STRVAR(s_unpack__doc__,
"S.unpack(str) -> (v1, v2, ...)\n\
\n\
Return tuple containing values unpacked according to this Struct's format.\n\
Requires len(str) == self.size. See struct.__doc__ for more on format\n\
strings.");

static PyObject *
s_unpack(PyObject *self, PyObject *inputstr)
{
    char *start;
    Py_ssize_t len;
    PyObject *args=NULL, *result;
    PyStructObject *soself = (PyStructObject *)self;
    assert(PyStruct_Check(self));
    assert(soself->s_codes != NULL);
    if (inputstr == NULL)
        goto fail;
    if (PyString_Check(inputstr) &&
        PyString_GET_SIZE(inputstr) == soself->s_size) {
            return s_unpack_internal(soself, PyString_AS_STRING(inputstr));
    }
    args = PyTuple_Pack(1, inputstr);
    if (args == NULL)
        return NULL;
    if (!PyArg_ParseTuple(args, "s#:unpack", &start, &len))
        goto fail;
    // TODO: if (soself->s_size != len)
    //     goto fail;
    result = s_unpack_internal(soself, start);
    Py_DECREF(args);
    return result;

fail:
    Py_XDECREF(args);
    PyErr_Format(StructError,
        "unpack requires a string argument of length %zd",
        soself->s_size);
    return NULL;
}

/*
 * Guts of the pack function.
 *
 * Takes a struct object, a tuple of arguments, and offset in that tuple of
 * argument for where to start processing the arguments for packing, and a
 * character buffer for writing the packed string.  The caller must insure
 * that the buffer may contain the required length for packing the arguments.
 * 0 is returned on success, 1 is returned if there is an error.
 *
 */
static int
s_pack_internal(PyStructObject *soself, PyObject *args, int offset, char* buf)
{
    formatcode *code;
    /* XXX(nnorwitz): why does i need to be a local?  can we use
       the offset parameter or do we need the wider width? */
    Py_ssize_t i;
    memset(buf, '\0', soself->s_size);
    i = offset;
    for (code = soself->s_codes; code->fmtdef != NULL; code++) {
        Py_ssize_t n;
        PyObject *v = PyTuple_GET_ITEM(args, i++);
        const formatdef *e = code->fmtdef;
        char *res = buf + code->offset;
        switch(e->format)
        {
            case 's':
            {
                if (!PyString_Check(v)) {
                    PyErr_SetString(StructError,
                        "argument for 's' must be a string");
                    return -1;
                }
                n = PyString_GET_SIZE(v);
                if (n > code->size)
                    n = code->size;
                if (n > 0)
                    memcpy(res, PyString_AS_STRING(v), n);
                break;
            }
            case 'S':
            {
                if (!PyString_Check(v)) {
                    PyErr_SetString(StructError,
                        "argument for 'S' must be a string");
                    return -1;
                }
                n = PyString_GET_SIZE(v);
                if (n > 0) {
                    *res++ = (char)n;
                    memcpy(res, PyString_AS_STRING(v), n);
                }
                buf += n;
                break;
            }
            case 'p':
            {
                if (!PyString_Check(v)) {
                    PyErr_SetString(StructError,
                            "argument for 'p' must be a string");
                    return -1;
                }
                n = PyString_GET_SIZE(v);
                if (n > (code->size - 1))
                    n = code->size - 1;
                if (n > 0)
                    memcpy(res + 1, PyString_AS_STRING(v), n);
                if (n > 255)
                    n = 255;
                *res = Py_SAFE_DOWNCAST(n, Py_ssize_t, unsigned char);
                break;
            }
            default:
            {
                if (e->pack(res, v, e) < 0) {
                    if (PyLong_Check(v) && PyErr_ExceptionMatches(PyExc_OverflowError))
                        PyErr_SetString(StructError,
                            "long too large to convert to int");
                    return -1;
                }
                break;
            }
        }
    }

    /* Success */
    return 0;
}


PyDoc_STRVAR(s_pack__doc__,
"S.pack(v1, v2, ...) -> string\n\
\n\
Return a string containing values v1, v2, ... packed according to this\n\
Struct's format. See struct.__doc__ for more on format strings.");

static PyObject *
s_pack(PyObject *self, PyObject *args)
{
    PyStructObject *soself;
    PyObject *result = NULL;

    /* Validate arguments. */
    soself = (PyStructObject *)self;
    assert(PyStruct_Check(self));
    assert(soself->s_codes != NULL);
    if (PyTuple_GET_SIZE(args) != soself->s_len)
    {
        PyErr_Format(StructError,
            "pack requires exactly %zd arguments", soself->s_len);
        return NULL;
    }

    Py_ssize_t newsize, i;
    i = 0;
    newsize = soself->s_size;
    formatcode *code;
    for (code = soself->s_codes; code->fmtdef != NULL; code++) {
        const formatdef *e = code->fmtdef;
        if (e->format == 'S') {
            PyObject *v = PyTuple_GET_ITEM(args, i);
            newsize += PyString_GET_SIZE(v);
        }
        i++;
    }

    /* Allocate a new string */
    result = PyString_FromStringAndSize((char *)NULL, newsize);
    if (result == NULL)
        return NULL;

    /* Call the guts */
    if ( s_pack_internal(soself, args, 0, PyString_AS_STRING(result)) != 0 ) {
        Py_DECREF(result);
        return NULL;
    }

    return result;
}


static PyObject *
s_get_format(PyStructObject *self, void *unused)
{
    Py_INCREF(self->s_format);
    return self->s_format;
}

static PyObject *
s_get_size(PyStructObject *self, void *unused)
{
    return PyInt_FromSsize_t(self->s_size);
}

/* List of functions */

static struct PyMethodDef s_methods[] = {
    {"pack",    s_pack,     METH_VARARGS, s_pack__doc__},
    {"unpack",  s_unpack,       METH_O, s_unpack__doc__},
    {NULL,   NULL}      /* sentinel */
};

PyDoc_STRVAR(s__doc__, "Compiled struct object");

#define OFF(x) offsetof(PyStructObject, x)

static PyGetSetDef s_getsetlist[] = {
    {"format", (getter)s_get_format, (setter)NULL, "struct format string", NULL},
    {"size", (getter)s_get_size, (setter)NULL, "struct size in bytes", NULL},
    {NULL} /* sentinel */
};

static
PyTypeObject PyStructType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "Struct",
    sizeof(PyStructObject),
    0,
    (destructor)s_dealloc,  /* tp_dealloc */
    0,                  /* tp_print */
    0,                  /* tp_getattr */
    0,                  /* tp_setattr */
    0,                  /* tp_compare */
    0,                  /* tp_repr */
    0,                  /* tp_as_number */
    0,                  /* tp_as_sequence */
    0,                  /* tp_as_mapping */
    0,                  /* tp_hash */
    0,                  /* tp_call */
    0,                  /* tp_str */
    PyObject_GenericGetAttr,    /* tp_getattro */
    PyObject_GenericSetAttr,    /* tp_setattro */
    0,                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_WEAKREFS,/* tp_flags */
    s__doc__,           /* tp_doc */
    0,                  /* tp_traverse */
    0,                  /* tp_clear */
    0,                  /* tp_richcompare */
    offsetof(PyStructObject, weakreflist),  /* tp_weaklistoffset */
    0,                  /* tp_iter */
    0,                  /* tp_iternext */
    s_methods,          /* tp_methods */
    NULL,               /* tp_members */
    s_getsetlist,       /* tp_getset */
    0,                  /* tp_base */
    0,                  /* tp_dict */
    0,                  /* tp_descr_get */
    0,                  /* tp_descr_set */
    0,                  /* tp_dictoffset */
    s_init,             /* tp_init */
    PyType_GenericAlloc,/* tp_alloc */
    s_new,              /* tp_new */
    PyObject_Del,       /* tp_free */
};


/* ---- Standalone functions  ---- */

#define MAXCACHE 100
static PyObject *cache = NULL;

static PyObject *
cache_struct(PyObject *fmt)
{
    PyObject * s_object;

    if (cache == NULL) {
        cache = PyDict_New();
        if (cache == NULL)
            return NULL;
    }

    s_object = PyDict_GetItem(cache, fmt);
    if (s_object != NULL) {
        Py_INCREF(s_object);
        return s_object;
    }

    s_object = PyObject_CallFunctionObjArgs((PyObject *)(&PyStructType), fmt, NULL);
    if (s_object != NULL) {
        if (PyDict_Size(cache) >= MAXCACHE)
            PyDict_Clear(cache);
        /* Attempt to cache the result */
        if (PyDict_SetItem(cache, fmt, s_object) == -1)
            PyErr_Clear();
    }
    return s_object;
}

PyDoc_STRVAR(clearcache_doc,
"Clear the internal cache.");

static PyObject *
clearcache(PyObject *self)
{
    Py_CLEAR(cache);
    Py_RETURN_NONE;
}

PyDoc_STRVAR(calcsize_doc,
"Return size of C struct described by format string fmt.");

static PyObject *
calcsize(PyObject *self, PyObject *fmt)
{
    Py_ssize_t n;
    PyObject *s_object = cache_struct(fmt);
    if (s_object == NULL)
        return NULL;
    n = ((PyStructObject *)s_object)->s_size;
    Py_DECREF(s_object);
        return PyInt_FromSsize_t(n);
}

PyDoc_STRVAR(pack_doc,
"Return string containing values v1, v2, ... packed according to fmt.");


static PyObject *
pack(PyObject *self, PyObject *args)
{
    PyObject *s_object, *fmt, *newargs, *result;
    Py_ssize_t n = PyTuple_GET_SIZE(args);

    if (n == 0) {
        PyErr_SetString(PyExc_TypeError, "missing format argument");
        return NULL;
    }
    fmt = PyTuple_GET_ITEM(args, 0);
    newargs = PyTuple_GetSlice(args, 1, n);
    if (newargs == NULL)
        return NULL;

    s_object = cache_struct(fmt);
    if (s_object == NULL) {
        Py_DECREF(newargs);
        return NULL;
    }
        result = s_pack(s_object, newargs);
    Py_DECREF(newargs);
    Py_DECREF(s_object);
    return result;
}

PyDoc_STRVAR(unpack_doc,
"Unpack the string containing packed C structure data, according to fmt.\n\
Requires len(string) == calcsize(fmt).");

static PyObject *
unpack(PyObject *self, PyObject *args)
{
    PyObject *s_object, *fmt, *inputstr, *result;

    if (!PyArg_UnpackTuple(args, "unpack", 2, 2, &fmt, &inputstr))
        return NULL;

    s_object = cache_struct(fmt);
    if (s_object == NULL)
        return NULL;
        result = s_unpack(s_object, inputstr);
    Py_DECREF(s_object);
    return result;
}

static struct PyMethodDef module_functions[] = {
    {"_clearcache", (PyCFunction)clearcache,    METH_NOARGS,    clearcache_doc},
    {"calcsize",    calcsize,   METH_O,     calcsize_doc},
    {"pack",    pack,       METH_VARARGS,   pack_doc},
    {"unpack",  unpack,         METH_VARARGS,   unpack_doc},
    {NULL,   NULL}      /* sentinel */
};


/* Module initialization */

PyDoc_STRVAR(module_doc,
"Functions to convert between Python values and C structs.\n\
Python strings are used to hold the data representing the C struct\n\
and also as format strings to describe the layout of data in the C struct.\n\
\n\
The optional first format char indicates byte order, size and alignment:\n\
 @: native order, size & alignment (default)\n\
 =: native order, std. size & alignment\n\
 <: little-endian, std. size & alignment\n\
 >: big-endian, std. size & alignment\n\
 !: same as >\n\
\n\
The remaining chars indicate types of args and must match exactly;\n\
these can be preceded by a decimal repeat count:\n\
  x: pad byte (no data); c:char; b:signed byte; B:unsigned byte;\n\
  h:short; H:unsigned short; i:int; I:unsigned int;\n\
  l:long; L:unsigned long; f:float; d:double.\n\
Special cases (preceding decimal count indicates length):\n\
  s:string (array of char); p: pascal string (with count byte).\n\
Special case (only available in native format):\n\
  P:an integer type that is wide enough to hold a pointer.\n\
Special case (not in native mode unless 'long long' in platform C):\n\
  q:long long; Q:unsigned long long\n\
Whitespace between formats is ignored.\n\
\n\
The variable struct.error is an exception raised on errors.\n");

PyMODINIT_FUNC
initfinstruct(void)
{
    PyObject *ver, *m;

    ver = PyString_FromString("0.2");
    if (ver == NULL)
        return;

    m = Py_InitModule3("finstruct", module_functions, module_doc);
    if (m == NULL)
        return;

    Py_TYPE(&PyStructType) = &PyType_Type;
    if (PyType_Ready(&PyStructType) < 0)
        return;

#ifdef PY_STRUCT_OVERFLOW_MASKING
    if (pyint_zero == NULL) {
        pyint_zero = PyInt_FromLong(0);
        if (pyint_zero == NULL)
            return;
    }
    if (pylong_ulong_mask == NULL) {
#if (SIZEOF_LONG == 4)
        pylong_ulong_mask = PyLong_FromString("FFFFFFFF", NULL, 16);
#else
        pylong_ulong_mask = PyLong_FromString("FFFFFFFFFFFFFFFF", NULL, 16);
#endif
        if (pylong_ulong_mask == NULL)
            return;
    }

#else
    /* This speed trick can't be used until overflow masking goes away, because
       native endian always raises exceptions instead of overflow masking. */

    /* Check endian and swap in faster functions */
    {
        int one = 1;
        formatdef *native = native_table;
        formatdef *other, *ptr;
        if ((int)*(unsigned char*)&one)
            other = lilendian_table;
        else
            other = bigendian_table;
        /* Scan through the native table, find a matching
           entry in the endian table and swap in the
           native implementations whenever possible
           (64-bit platforms may not have "standard" sizes) */
        while (native->format != '\0' && other->format != '\0') {
            ptr = other;
            while (ptr->format != '\0') {
                if (ptr->format == native->format) {
                    /* Match faster when formats are
                       listed in the same order */
                    if (ptr == other)
                        other++;
                    /* Only use the trick if the
                       size matches */
                    if (ptr->size != native->size)
                        break;
                    /* Skip float and double, could be
                       "unknown" float format */
                    if (ptr->format == 'd' || ptr->format == 'f')
                        break;
                    ptr->pack = native->pack;
                    ptr->unpack = native->unpack;
                    break;
                }
                ptr++;
            }
            native++;
        }
    }
#endif

    /* Add some symbolic constants to the module */
    if (StructError == NULL) {
        StructError = PyErr_NewException("finstruct.error", NULL, NULL);
        if (StructError == NULL)
            return;
    }

    Py_INCREF(StructError);
    PyModule_AddObject(m, "error", StructError);

    Py_INCREF((PyObject*)&PyStructType);
    PyModule_AddObject(m, "Struct", (PyObject*)&PyStructType);

    PyModule_AddObject(m, "__version__", ver);

    PyModule_AddIntConstant(m, "_PY_STRUCT_RANGE_CHECKING", 1);
#ifdef PY_STRUCT_OVERFLOW_MASKING
    PyModule_AddIntConstant(m, "_PY_STRUCT_OVERFLOW_MASKING", 1);
#endif
#ifdef PY_STRUCT_FLOAT_COERCE
    PyModule_AddIntConstant(m, "_PY_STRUCT_FLOAT_COERCE", 1);
#endif

}