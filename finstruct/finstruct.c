/*  
Finstruct c code
*/

#include "Python.h"

static PyTypeObject FinstructType;

static PyObject *pylong_ulong_mask = NULL;

typedef struct _formatdef {
    char format;
    size_t size;
    long dynamic;
    PyObject* (*unpack)(const char *,
        const struct _formatdef *);
    int (*pack)(char *, PyObject *,
        const struct _formatdef *);
} formatdef;

typedef struct _formatcode {
    const struct _formatdef *fmtdef;
    size_t offset;
    size_t size;
} formatcode;

typedef struct {
    PyObject_HEAD
    PyObject *format;
    formatcode *codes;
    size_t size;
    size_t len;
    long dynamic;
} FinstructObject;

static PyObject *FinstructError;

/* Helpers */

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
    PyErr_SetString(FinstructError,
            "cannot convert argument to long");
    return NULL;
}

static int
get_long(PyObject *v, long *p)
{
    long x = PyInt_AsLong(v);
    if (x == -1 && PyErr_Occurred()) {
        if (PyErr_ExceptionMatches(PyExc_TypeError))
            PyErr_SetString(FinstructError,
                    "required argument is not an integer");
        return -1;
    }
    *p = x;
    return 0;
}

/* Packing Types */

static int
get_wrapped_long(PyObject *v, long *p)
{
    if (get_long(v, p) < 0) {
        if (PyLong_Check(v) &&
            PyErr_ExceptionMatches(PyExc_OverflowError)) {
            PyObject *wrapped;
            long x;
            PyErr_Clear();
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
        wrapped = PyNumber_And(v, pylong_ulong_mask);
        if (wrapped == NULL)
            return -1;
        x = (long)PyLong_AsUnsignedLong(wrapped);
        Py_DECREF(wrapped);
        if (x == -1 && PyErr_Occurred())
            return -1;
    }
    *p = (unsigned long)x;
    return 0;
}

static PyObject *
unpack_byte(const char *p, const formatdef *f)
{
    return PyInt_FromLong((long) *(signed char *)p);
}

static int
pack_byte(char *p, PyObject *v, const formatdef *f)
{
    long x;
    if (get_long(v, &x) < 0)
        return -1;
    if (x < -128 || x > 127) {
        PyErr_SetString(FinstructError,
                "byte requires range(-128,127)");
        return -1;
    }
    *p = (char)x;
    return 0;
}

static PyObject *
unpack_ubyte(const char *p, const formatdef *f)
{
    return PyInt_FromLong((long) *(unsigned char *)p);
}

static int
pack_ubyte(char *p, PyObject *v, const formatdef *f)
{
    long x;
    if (get_long(v, &x) < 0)
        return -1;
    if (x < 0 || x > 255){
        PyErr_SetString(FinstructError,
                "ubyte format requires 0 <= number <= 255");
        return -1;
    }
    *p = (char)x;
    return 0;
}

static PyObject *
unpack_int(const char *p, const formatdef *f)
{
    long x = 0;
    size_t i = f->size;
    const unsigned char *bytes = (const unsigned char *)p;
    do {
        x = (x<<8) | bytes[--i];
    } while (i > 0);
    if (4 > f->size)
        x |= -(x & (1L << ((8 * f->size) - 1)));
    return PyInt_FromLong(x);
}

static int
pack_int(char *p, PyObject *v, const formatdef *f)
{
    long x;
    size_t i;
    if (get_wrapped_long(v, &x) < 0)
        return -1;
    i = f->size;
    do {
        *p++ = (char)x;
        x >>= 8;
    } while (--i > 0);
    return 0;
}

static PyObject *
unpack_uint(const char *p, const formatdef *f)
{
    unsigned long x = 0;
    size_t i = f->size;
    const unsigned char *bytes = (const unsigned char *)p;
    do {
        x = (x<<8) | bytes[--i];
    } while (i > 0);
    return PyLong_FromUnsignedLong((long)x);
}

static int
pack_uint(char *p, PyObject *v, const formatdef *f)
{
    unsigned long x;
    size_t i;
    if (get_wrapped_ulong(v, &x) < 0)
        return -1;
    i = f->size;
    if (i != 4) {
        unsigned long maxint = 1;
        maxint <<= (unsigned long)(i * 8);
    }
    do {
        *p++ = (char)x;
        x >>= 8;
    } while (--i > 0);
    return 0;
}

static PyObject *
unpack_longlong(const char *p, const formatdef *f)
{
    PY_LONG_LONG x = 0;
    size_t i = f->size;
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
}

static int
pack_longlong(char *p, PyObject *v, const formatdef *f)
{
    int res;
    v = get_pylong(v);
    if (v == NULL)
        return -1;
    res = _PyLong_AsByteArray((PyLongObject*)v,
                  (unsigned char *)p,
                  8,
                  1, /* little_endian */
                  1  /* signed */);
    Py_DECREF(v);
    return res;
}

static PyObject *
unpack_ulonglong(const char *p, const formatdef *f)
{
    unsigned PY_LONG_LONG x = 0;
    size_t i = f->size;
    const unsigned char *bytes = (const unsigned char *)p;
    do {
        x = (x<<8) | bytes[--i];
    } while (i > 0);
    if (x <= LONG_MAX)
        return PyInt_FromLong(Py_SAFE_DOWNCAST(x, unsigned PY_LONG_LONG, long));
    return PyLong_FromUnsignedLongLong(x);
}

static int
pack_ulonglong(char *p, PyObject *v, const formatdef *f)
{
    int res;
    v = get_pylong(v);
    if (v == NULL)
        return -1;
    res = _PyLong_AsByteArray((PyLongObject*)v,
                  (unsigned char *)p,
                  8,
                  1, /* little_endian */
                  0  /* signed */);
    Py_DECREF(v);
    return res;
}

static PyObject *
unpack_float(const char *p, const formatdef *f)
{
    double x;
    x = _PyFloat_Unpack4((unsigned char *)p, 1);
    if (x == -1.0 && PyErr_Occurred())
        return NULL;
    return PyFloat_FromDouble(x);
}

static int
pack_float(char *p, PyObject *v, const formatdef *f)
{
    double x = PyFloat_AsDouble(v);
    if (x == -1 && PyErr_Occurred()) {
        PyErr_SetString(FinstructError,
                "required argument is not a float");
        return -1;
    }
    return _PyFloat_Pack4(x, (unsigned char *)p, 1);
}

static PyObject *
unpack_double(const char *p, const formatdef *f)
{
    double x;
    x = _PyFloat_Unpack8((unsigned char *)p, 1);
    if (x == -1.0 && PyErr_Occurred())
        return NULL;
    return PyFloat_FromDouble(x);
}

static int
pack_double(char *p, PyObject *v, const formatdef *f)
{
    double x = PyFloat_AsDouble(v);
    if (x == -1 && PyErr_Occurred()) {
        PyErr_SetString(FinstructError,
                "required argument is not a float");
        return -1;
    }
    return _PyFloat_Pack8(x, (unsigned char *)p, 1);
}

static formatdef format_table[] = {
    {'b',   1,      0,      unpack_byte,    pack_byte},
    {'B',   1,      0,      unpack_ubyte,   pack_ubyte},
    {'h',   2,      0,      unpack_int,     pack_int},
    {'H',   2,      0,      unpack_uint,    pack_uint},
    {'i',   4,      0,      unpack_int,     pack_int},
    {'I',   4,      0,      unpack_uint,    pack_uint},
    {'l',   4,      0,      unpack_int,     pack_int},
    {'L',   4,      0,      unpack_uint,    pack_uint},
    {'q',   8,      0,      unpack_longlong,    pack_longlong},
    {'Q',   8,      0,      unpack_ulonglong,   pack_ulonglong},
    {'f',   4,      0,      unpack_float,   pack_float},
    {'d',   8,      0,      unpack_double,  pack_double},
    {'s',   1,      1,      NULL},
    {0}
};

/* Finstruct Class */

static PyObject *
f_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyObject *self;

    assert(type != NULL && type->tp_alloc != NULL);

    self = type->tp_alloc(type, 0);
    if (self != NULL) {
        FinstructObject *s = (FinstructObject*)self;
        Py_INCREF(Py_None);
        s->format = Py_None;
        s->size = -1;
        s->len  = -1;
        s->dynamic = 0;
    }
    return self;
}

static const formatdef *
getentry(int c, const formatdef *f)
{
    for (; f->format != '\0'; f++) {
        if (f->format == c) {
            return f;
        }
    }
    PyErr_Format(FinstructError, "bad char in struct format: %c", f->format);
    return NULL;
}

static int
configure(FinstructObject *self)
{
    /*
    In this function we want to parse through the format codes
    and work out if we have a dynamic length struct and create a list
    of unpack/pack methods.
    */
    const formatdef *e;
    formatcode *codes;

    const char *s;
    const char *fmt;
    char c;
    size_t size, len;

    fmt = PyString_AS_STRING(self->format);
    s = fmt;

    size = 0;
    len = 0;

    while ((c = *s++) != '\0') {
        if (isspace(Py_CHARMASK(c)))
            continue;

        /* grab the entry from the table */
        e = getentry(c, format_table);

        if (e == NULL)
            return -1;

        len++;
        if (e->dynamic == 0) {
            size += e->size;    
        }
        else
        {
            self->dynamic = 1;
        }       
    }

    self->size = size;
    self->len = len;
    codes = PyMem_MALLOC((len + 1) * sizeof(formatcode));
    if (codes == NULL) {
        PyErr_NoMemory();
        return -1;
    }
    self->codes = codes;

    s = fmt;
    while ((c = *s++) != '\0') {
        if (isspace(Py_CHARMASK(c)))
            continue;

        e = getentry(c, format_table);
        codes->offset = e->size;
        codes->fmtdef = e;
        codes++;
    }
    codes->fmtdef = NULL;
    codes->offset = size;

    return 0;
}

static int
f_init(PyObject *self, PyObject *args, PyObject *kwds)
{
    FinstructObject *fself = (FinstructObject *)self;
    PyObject *o_format = NULL;
    int ret = 0;
    static char *kwlist[] = {"format", 0};

    assert(PyStruct_Check(self));

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "S:Struct", kwlist,
                     &o_format))
        return -1;

    Py_INCREF(o_format);
    Py_CLEAR(fself->format);
    fself->format = o_format;

    ret = configure(fself);
    return ret;
}

/* List of functions */
static PyObject *
unpack_internal(FinstructObject *fself, char *start)
{
    formatcode *code;
    size_t i = 0, index = 0;
    PyObject *result = PyTuple_New(fself->len);
    if (result == NULL)
        return NULL;

    for (code = fself->codes; code->fmtdef != NULL; code++)
    {
        PyObject *v;
        const formatdef *e = code->fmtdef;
        const char *res = start + index;
        index += code->offset;
        if (e->format == 's')
        {
            size_t size = *(unsigned char*)res;
            v = PyString_FromStringAndSize(res + 1, size);
            index += size;
        }
        else
        {
            v = e->unpack(res, e);
        }
        PyTuple_SET_ITEM(result, i++, v);
    }

    return result;
}

PyDoc_STRVAR(pack__doc__, "Pack Method");
static PyObject *
unpack(PyObject *self, PyObject *inputstr)
{
    char *start;
    size_t len;

    // Get Arguments
    PyObject *args=NULL, *result;
    FinstructObject *fself = (FinstructObject *)self;
    assert(PyStruct_Check(self));
    assert(fself->codes != NULL);
    if (inputstr == NULL)
        goto fail;
    if (PyString_Check(inputstr) &&
        PyString_GET_SIZE(inputstr) == fself->size) {
            return unpack_internal(fself, PyString_AS_STRING(inputstr));
    }
    args = PyTuple_Pack(1, inputstr);
    if (!PyArg_ParseTuple(args, "s#:unpack", &start, &len))
        goto fail;
    if (args == NULL )
        return NULL;

    result = unpack_internal(fself, start);

    Py_DECREF(args);
    return result;
fail:
    Py_XDECREF(args);
    PyErr_Format(FinstructError,
        "unpack requires a string argument of length %zd",
        fself->size);
    return NULL;
}

static int
pack_internal(FinstructObject *fself, PyObject *args, int offset, char *buf)
{
    formatcode *code;
    size_t i, index;
    memset(buf, '\0', fself->size);
    i = offset;
    index = 0;

    for (code = fself->codes; code->fmtdef != NULL; code++)
    {
        size_t n;
        PyObject *v = PyTuple_GET_ITEM(args, i++);
        const formatdef *e = code->fmtdef;
        char *res = buf + index;
        index += code->offset;
        switch(e->format)
        {
            case 's':
            {
                if (!PyString_Check(v)) {
                    PyErr_SetString(FinstructError,
                        "Argument for 's' must be a string");
                    return -1;
                }
                n = PyString_GET_SIZE(v);
                if (n > 0) {
                    *res++ = (char)n;
                    memcpy(res, PyString_AS_STRING(v), n);
                }
                index += n;
                break;
            }
            default:
            {
                if (e->pack(res, v, e) < 0) {
                    PyErr_SetString(FinstructError,
                        "Error Parsing Integer Argument");
                    return -1;
                }
                break;
            }
        }
    }

    return 0;
}

PyDoc_STRVAR(unpack__doc__, "Unpack Method");
static PyObject *
pack(PyObject *self, PyObject *args)
{
    FinstructObject *fself;
    PyObject *result = NULL;

    /* Validate Args */
    fself = (FinstructObject *)self;
    assert(PyStruct_Check(self));
    assert(fself->codes != NULL);
    if (PyTuple_GET_SIZE(args) != fself->len)
    {
        PyErr_Format(FinstructError,
            "pack requires exactly %zd arguments", fself->len);
        return NULL;
    }

    size_t newsize, i;
    i = 0;
    newsize = fself->size;
    formatcode *code;
    if (fself->dynamic == 1)
    {
        /* Dynamic Length Allocation */
        for (code = fself->codes; code->fmtdef != NULL; code++) {
            const formatdef *e = code->fmtdef;
            if (e->format == 's') {
                PyObject *v = PyTuple_GET_ITEM(args, i);
                newsize += PyString_GET_SIZE(v) + 1; 
            }
            i++;
        }
    }

    result = PyString_FromStringAndSize((char *)NULL, newsize);
    if (result == NULL)
        return NULL;

    /* Call Internal Pack */
    if ( pack_internal(fself, args, 0, PyString_AS_STRING(result)) != 0 )
    {
        Py_DECREF(result);
        return NULL;
    }

    return result;
}

static void
f_dealloc(FinstructObject *s)
{
    Py_XDECREF(s->format);
    Py_TYPE(s)->tp_free((PyObject *)s);
}

static struct PyMethodDef f_methods[] = {
    {"pack",    pack,     METH_VARARGS,     pack__doc__},
    {"unpack",  unpack,       METH_O,       unpack__doc__},
    {NULL,   NULL}      /* sentinel */
};

static PyObject *
f_get_format(FinstructObject *self, void *unused)
{
    Py_INCREF(self->format);
    return self->format;
}

static PyObject *
f_get_size(FinstructObject *self, void *unused)
{
    return PyInt_FromSsize_t(self->size);
}

static PyObject *
f_get_dynamic(FinstructObject *self, void *unused)
{
    return PyBool_FromLong(self->dynamic);
}

static PyGetSetDef f_getsetters[] = {
    {"format", (getter)f_get_format, (setter)NULL, "finstruct format string", NULL},
    {"size", (getter)f_get_size, (setter)NULL, "finstruct size", NULL},
    {"is_dynamic", (getter)f_get_dynamic, (setter)NULL, "finstruct is dynamic", NULL},
    {NULL} /* sentinel */
};

PyDoc_STRVAR(f__doc__, "Compiled struct object");

static
PyTypeObject FinstructType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "Finstruct",
    sizeof(FinstructObject),
    0,
    (destructor)f_dealloc,  /* tp_dealloc */
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
    f__doc__,           /* tp_doc */
    0,                  /* tp_traverse */
    0,                  /* tp_clear */
    0,                  /* tp_richcompare */
    0,                  /* tp_weaklistoffset */
    0,                  /* tp_iter */
    0,                  /* tp_iternext */
    f_methods,          /* tp_methods */
    NULL,               /* tp_members */
    f_getsetters,       /* tp_getset */
    0,                  /* tp_base */
    0,                  /* tp_dict */
    0,                  /* tp_descr_get */
    0,                  /* tp_descr_set */
    0,                  /* tp_dictoffset */
    f_init,             /* tp_init */
    PyType_GenericAlloc,/* tp_alloc */
    f_new,              /* tp_new */
    PyObject_Del,       /* tp_free */
};


/* Method Declerations */

static struct PyMethodDef module_functions[] = {
    {NULL,      NULL}
};


/* Module Init */

PyDoc_STRVAR(module_doc, 
"Fuctions similar to struct to pack between python values and C like structs");

PyMODINIT_FUNC
initfinstruct(void)
{
    PyObject *ver, *module;
    ver = PyString_FromString("0.1");
    if (ver == NULL)
        return;

    module = Py_InitModule3("finstruct", module_functions, module_doc);
    if (module == NULL)
        return;

    Py_TYPE(&FinstructType) = &PyType_Type;
    if (PyType_Ready(&FinstructType) < 0)
        return;

    if (FinstructError == NULL) {
        FinstructError = PyErr_NewException("finstruct.error", NULL, NULL);
        if (FinstructError == NULL)
            return;
    }

    pylong_ulong_mask = PyLong_FromString("FFFFFFFF", NULL, 16);

    Py_INCREF(FinstructError);
    PyModule_AddObject(module, "error", FinstructError);

    Py_INCREF((PyObject*)&FinstructType);
    PyModule_AddObject(module, "Finstruct", (PyObject*)&FinstructType);

    PyModule_AddObject(module, "__version__", ver);
}