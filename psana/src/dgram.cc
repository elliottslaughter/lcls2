
#include "xtcdata/xtc/ShapesData.hh"
#include "xtcdata/xtc/DescData.hh"
#include "xtcdata/xtc/Dgram.hh"
#include "xtcdata/xtc/TypeId.hh"
#include "xtcdata/xtc/XtcIterator.hh"
#include "xtcdata/xtc/NamesIter.hh"

#include <Python.h>
#include <stdio.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <numpy/arrayobject.h>
#include <numpy/ndarraytypes.h>
#include <structmember.h>
#include <assert.h>

using namespace XtcData;
#define BUFSIZE 0x4000000
#define TMPSTRINGSIZE 256
#define CHUNKSIZE 1<<20
#define MAXRETRIES 5

static const char* PyNameDelim=".";

using namespace std;

// to avoid compiler warnings for debug variables
#define _unused(x) ((void)(x))

struct ContainerInfo {
    PyObject* containermod;
    PyObject* pycontainertype;
};

struct PyDgramObject {
    PyObject_HEAD
    PyObject* dict;
    PyObject* pyseq;
    bool is_pyseq_valid;
    PyObject* dgrambytes;
    Dgram* dgram;
    int file_descriptor;
    ssize_t offset;
    Py_buffer buf;
    ContainerInfo contInfo;
};

static void addObj(PyDgramObject* dgram, const char* name, PyObject* obj) {
    char namecopy[TMPSTRINGSIZE];
    strncpy(namecopy,name,TMPSTRINGSIZE);
    PyObject* parent = (PyObject*)dgram;
    char *key = ::strtok(namecopy,PyNameDelim);
    while(1) {
        char* next = ::strtok(NULL, PyNameDelim);
        bool last = (next == NULL);
        if (last) {
            // add the real object
            int fail = PyObject_SetAttrString(parent, key, obj);
            if (fail) printf("Dgram: failed to set object attribute\n");
            Py_DECREF(obj); // transfer ownership to parent
            break;
        } else {
            if (!PyObject_HasAttrString(parent, key)) {
                PyObject* container = PyObject_CallObject(dgram->contInfo.pycontainertype, NULL);
                int fail = PyObject_SetAttrString(parent, key, container);
                if (fail) printf("Dgram: failed to set container attribute\n");
                Py_DECREF(container); // transfer ownership to parent
            }
            parent = PyObject_GetAttrString(parent, key);
            Py_DECREF(parent); // we just want the pointer, not a new reference
        }
        key=next;
    }
}

static void setAlg(PyDgramObject* pyDgram, const char* baseName, Alg& alg) {
    const char* algName = alg.name();
    const uint32_t _v = alg.version();
    char keyName[TMPSTRINGSIZE];

    PyObject* software = Py_BuildValue("s", algName);
    PyObject* version  = Py_BuildValue("iii", (_v>>16)&0xff, (_v>>8)&0xff, (_v)&0xff);

    snprintf(keyName,TMPSTRINGSIZE,"software%s%s%ssoftware",
             PyNameDelim,baseName,PyNameDelim);
    addObj(pyDgram, keyName, software);
    snprintf(keyName,TMPSTRINGSIZE,"software%s%s%sversion",
             PyNameDelim,baseName,PyNameDelim);
    addObj(pyDgram, keyName, version);

    assert(Py_GETREF(software)==1);
}

static void setDetInfo(PyDgramObject* pyDgram, Names& names) {
    char keyName[TMPSTRINGSIZE];
    PyObject* detType = Py_BuildValue("s", names.detType());
    snprintf(keyName,TMPSTRINGSIZE,"software%s%s%sdettype",
             PyNameDelim,names.detName(),PyNameDelim);
    addObj(pyDgram, keyName, detType);

    PyObject* detId = Py_BuildValue("s", names.detId());
    snprintf(keyName,TMPSTRINGSIZE,"software%s%s%sdetid",
             PyNameDelim,names.detName(),PyNameDelim);
    addObj(pyDgram, keyName, detId);

    assert(Py_GETREF(detType)==1);
}

void DictAssignAlg(PyDgramObject* pyDgram, std::vector<NameIndex>& namesVec)
{
    // This function gets called at configure: add attribute "software" and "version" to pyDgram and return
    char baseName[TMPSTRINGSIZE];

    for (unsigned i = 0; i < namesVec.size(); i++) {
        Names& names = namesVec[i].names();
        Alg& detAlg = names.alg();
        snprintf(baseName,TMPSTRINGSIZE,"%s%s%s",
                 names.detName(),PyNameDelim,names.alg().name());
        setAlg(pyDgram,baseName,detAlg);
        setDetInfo(pyDgram, names);

        for (unsigned j = 0; j < names.num(); j++) {
            Name& name = names.get(j);
            Alg& alg = name.alg();
            snprintf(baseName,TMPSTRINGSIZE,"%s%s%s%s%s",
                     names.detName(),PyNameDelim,names.alg().name(),
                     PyNameDelim,name.name());
            setAlg(pyDgram,baseName,alg);
        }
    }
}

void DictAssign(PyDgramObject* pyDgram, DescData& descdata)
{
    Names& names = descdata.nameindex().names(); // event names, chan0, chan1

    char keyName[TMPSTRINGSIZE];
    for (unsigned i = 0; i < names.num(); i++) {
        Name& name = names.get(i);
        const char* tempName = name.name();
        PyObject* newobj=0;

        if (name.rank() == 0) {
            switch (name.type()) {
            case Name::UINT8: {
	            const auto tempVal = descdata.get_value<uint8_t>(tempName);
                newobj = Py_BuildValue("I", tempVal);
                break;
            }
            case Name::UINT16: {
                const auto tempVal = descdata.get_value<uint16_t>(tempName);
                newobj = Py_BuildValue("I", tempVal);
                break;
            }
            case Name::UINT32: {
                const auto tempVal = descdata.get_value<uint32_t>(tempName);
                newobj = Py_BuildValue("k", tempVal);
                break;
            }
            case Name::UINT64: {
                const auto tempVal = descdata.get_value<uint64_t>(tempName);
                newobj = Py_BuildValue("K", tempVal);
                break;
            }
            case Name::INT8: {
                const auto tempVal = descdata.get_value<int8_t>(tempName);
                newobj = Py_BuildValue("i", tempVal);
                break;
            }
            case Name::INT16: {
                const auto tempVal = descdata.get_value<int16_t>(tempName);
                newobj = Py_BuildValue("i", tempVal);
                break;
            }
            case Name::INT32: {
                const auto tempVal = descdata.get_value<int32_t>(tempName);
                newobj = Py_BuildValue("l", tempVal);
                break;
            }
            case Name::INT64: {
                const auto tempVal = descdata.get_value<int64_t>(tempName);
                newobj = Py_BuildValue("L", tempVal);
                break;
            }
            case Name::FLOAT: {
                const auto tempVal = descdata.get_value<float>(tempName);
                newobj = Py_BuildValue("f", tempVal);
                break;
            }
            case Name::DOUBLE: {
                const auto tempVal = descdata.get_value<double>(tempName);
                newobj = Py_BuildValue("d", tempVal);
                break;
            }
            }
        } else {
            npy_intp dims[name.rank()];
            uint32_t* shape = descdata.shape(name);
            for (unsigned j = 0; j < name.rank(); j++) {
                dims[j] = shape[j];
            }
            switch (name.type()) {
            case Name::UINT8: {
                auto arr = descdata.get_array<uint8_t>(i);
                newobj = PyArray_SimpleNewFromData(name.rank(), dims,
                                                   NPY_UINT8, arr.data());
                break;
            }
            case Name::UINT16: {
                auto arr = descdata.get_array<uint16_t>(i);
                newobj = PyArray_SimpleNewFromData(name.rank(), dims,
                                                   NPY_UINT16, arr.data());
                break;
            }
            case Name::UINT32: {
                auto arr = descdata.get_array<uint32_t>(i);
                newobj = PyArray_SimpleNewFromData(name.rank(), dims,
                                                   NPY_UINT32, arr.data());
                break;
            }
            case Name::UINT64: {
                auto arr = descdata.get_array<uint64_t>(i);
                newobj = PyArray_SimpleNewFromData(name.rank(), dims,
                                                   NPY_UINT64, arr.data());
                break;
            }
            case Name::INT8: {
                auto arr = descdata.get_array<int8_t>(i);
                newobj = PyArray_SimpleNewFromData(name.rank(), dims,
                                                   NPY_INT8, arr.data());
                break;
            }
            case Name::INT16: {
                auto arr = descdata.get_array<int16_t>(i);
                newobj = PyArray_SimpleNewFromData(name.rank(), dims,
                                                   NPY_INT16, arr.data());
                break;
            }
            case Name::INT32: {
                auto arr = descdata.get_array<int32_t>(i);
                newobj = PyArray_SimpleNewFromData(name.rank(), dims,
                                                   NPY_INT32, arr.data());
                break;
            }
            case Name::INT64: {
                auto arr = descdata.get_array<int64_t>(i);
                newobj = PyArray_SimpleNewFromData(name.rank(), dims,
                                                   NPY_INT64, arr.data());
                break;
            }
            case Name::FLOAT: {
                auto arr = descdata.get_array<float>(i);
                newobj = PyArray_SimpleNewFromData(name.rank(), dims,
                                                   NPY_FLOAT, arr.data());
                break;
            }
            case Name::DOUBLE: {
                auto arr = descdata.get_array<double>(i);
                newobj = PyArray_SimpleNewFromData(name.rank(), dims,
                                                   NPY_DOUBLE, arr.data());
                break;
            }
            }
            if (PyArray_SetBaseObject((PyArrayObject*)newobj, pyDgram->dgrambytes) < 0) {
                printf("Failed to set BaseObject for numpy array.\n");
            }
            // PyArray_SetBaseObject steals a reference to the dgrambytes
            // but we want the dgram to also keep a reference to it as well.
            Py_INCREF(pyDgram->dgrambytes);
            //clear NPY_ARRAY_WRITEABLE flag
            PyArray_CLEARFLAGS((PyArrayObject*)newobj, NPY_ARRAY_WRITEABLE);
        }
        snprintf(keyName,TMPSTRINGSIZE,"%s%s%s%s%s",
                 names.detName(),PyNameDelim,names.alg().name(),
                 PyNameDelim,name.name());
        addObj(pyDgram, keyName, newobj);
    }
}

class PyConvertIter : public XtcIterator
{
public:
    enum { Stop, Continue };
    PyConvertIter(Xtc* xtc, PyDgramObject* pyDgram, std::vector<NameIndex>& namesVec) :
        XtcIterator(xtc), _pyDgram(pyDgram), _namesVec(namesVec)
    {
    }

    int process(Xtc* xtc)
    {
        switch (xtc->contains.id()) { //enum Type { Parent, ShapesData, Shapes, Data, Names, NumberOf };
        case (TypeId::Parent): {
            iterate(xtc); // look inside anything that is a Parent
            break;
        }
        case (TypeId::ShapesData): {
            ShapesData& shapesdata = *(ShapesData*)xtc;
            // lookup the index of the names we are supposed to use
            unsigned namesId = shapesdata.shapes().namesId();
            // protect against the fact that this datagram
            // may not have a _namesVec
            if (namesId<_namesVec.size()) {
                DescData descdata(shapesdata, _namesVec[namesId]);
                DictAssign(_pyDgram, descdata);
            }
            break;
        }
        default:
            break;
        }
        return Continue;
    }

private:
    PyDgramObject*          _pyDgram;
    std::vector<NameIndex>& _namesVec; // need one of these for each source
};

void AssignDict(PyDgramObject* self, PyObject* configDgram) {
    bool isConfig;
    isConfig = (configDgram == 0) ? true : false;
    
    if (isConfig) configDgram = (PyObject*)self; // we weren't passed a config, so we must be config
    
    NamesIter namesIter(&((PyDgramObject*)configDgram)->dgram->xtc);
    namesIter.iterate();
    
    if (isConfig) DictAssignAlg((PyDgramObject*)configDgram, namesIter.namesVec());
    
    PyConvertIter iter(&self->dgram->xtc, self, namesIter.namesVec());
    iter.iterate();
}

static void dgram_dealloc(PyDgramObject* self)
{
    // cpo: this should not need to be XDECREF for pyseq.  how are
    // we creating dgrams with a NULL value for pyseq?
    Py_XDECREF(self->pyseq);
    Py_XDECREF(self->dict);
    if (self->buf.buf == NULL) {
        // can be NULL if we had a problem early in dgram_init
        Py_XDECREF(self->dgrambytes);
    } else {
        PyBuffer_Release(&(self->buf));
    }
    // can be NULL if we had a problem early in dgram_init
    Py_XDECREF(self->contInfo.containermod);
    Py_XDECREF(self->contInfo.pycontainertype);

    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* dgram_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    PyDgramObject* self;
    self = (PyDgramObject*)type->tp_alloc(type, 0);
    if (self != NULL) self->dict = PyDict_New();
    return (PyObject*)self;
}

static int dgram_read(PyDgramObject* self, ssize_t dgram_size, int sequential)
{
    ssize_t readSuccess=0;
    if (sequential) {
        readSuccess = read(self->file_descriptor, self->dgram, sizeof(Dgram));
        if (readSuccess <= 0) {
            PyErr_SetString(PyExc_StopIteration, "loading self->dgram was unsuccessful");
            return -1;
        }
        readSuccess = read(self->file_descriptor, self->dgram + 1, self->dgram->xtc.sizeofPayload());
        if (readSuccess <= 0) {
            PyErr_SetString(PyExc_StopIteration, "loading self->dgram was unsuccessful");
            return -1;
        }

    } else {
        off_t fOffset = (off_t)self->offset;
        readSuccess = pread(self->file_descriptor, self->dgram, dgram_size, fOffset); // for read with offset
        if (readSuccess <= 0) {
            char s[TMPSTRINGSIZE];
            snprintf(s, TMPSTRINGSIZE, "loading self->dgram was unsuccessful -- %s", strerror(errno));
            PyErr_SetString(PyExc_StopIteration, s);
            return -1;
        }
    }
    return 0;
}

static int dgram_init(PyDgramObject* self, PyObject* args, PyObject* kwds)
{
    static char* kwlist[] = {(char*)"file_descriptor",
                             (char*)"config",
                             (char*)"offset",
                             (char*)"size",
                             (char*)"view",
                             NULL};

    int fd=-1;
    PyObject* configDgram=0;
    self->offset=0;
    ssize_t dgram_size=0;
    bool isView=0;
    PyObject* view=0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds,
                                     "|iOllO", kwlist,
                                     &fd,
                                     &configDgram,
                                     &self->offset,
                                     &dgram_size,
                                     &view)) {
        return -1;
    }
    
    if (fd > -1) {
        if (fcntl(fd, F_GETFD) == -1) {
            PyErr_SetString(PyExc_OSError, "invalid file descriptor");
            return -1;
        }
    }

    isView = (view!=0) ? true : false;

    self->contInfo.containermod = PyImport_ImportModule("psana.container");
    self->contInfo.pycontainertype = PyObject_GetAttrString(self->contInfo.containermod,"Container");

    if (!isView) {
        PyObject* arglist = Py_BuildValue("(i)",BUFSIZE);
        // I believe this memsets the buffer to 0, which we don't need.
        // Perhaps ideally we would write a custom object to avoid this. - cpo
        self->dgrambytes = PyObject_CallObject((PyObject*)&PyByteArray_Type, arglist);
        Py_DECREF(arglist);
        self->dgram = (Dgram*)(PyByteArray_AS_STRING(self->dgrambytes));
    } else {
        if (PyObject_GetBuffer(view, &(self->buf), PyBUF_SIMPLE) == -1) {
            PyErr_SetString(PyExc_MemoryError, "unable to create dgram with the given view");
            return -1;
        }
        self->dgram = (Dgram*)(((char *)self->buf.buf) + self->offset);
    }

    if (self->dgram == NULL) {
        PyErr_SetString(PyExc_MemoryError, "insufficient memory to create Dgram object");
        return -1;
    }

    // The pyseq fields are initialized in dgram_get_*
    self->is_pyseq_valid = false;

    // Read the data if this dgram is not a view
    if (!isView) {
        if (fd==-1 && configDgram==0) {
            self->dgram->xtc.extent = 0; // for empty dgram
        } else {
            if (fd==-1) {
                self->file_descriptor=((PyDgramObject*)configDgram)->file_descriptor;
            } else {
                self->file_descriptor=fd;
            }

            bool sequential = (fd==-1) != (configDgram==0);
            int err = dgram_read(self, dgram_size, sequential);
            if (err) return err;
        }
    }
    AssignDict(self, configDgram);

    return 0;
}

#if PY_MAJOR_VERSION < 3
static Py_ssize_t PyDgramObject_getsegcount(PyDgramObject *self, Py_ssize_t *lenp) {
    return 1; // only supports single segment
}

static Py_ssize_t PyDgramObject_getreadbuf(PyDgramObject *self, Py_ssize_t segment, void **ptrptr) {
    *ptrptr = (void*)self->dgram;
    if (self->dgram->xtc.extent == 0) {
        return BUFSIZE;
    } else {
        return sizeof(*self->dgram) + self->dgram->xtc.sizeofPayload();
    }
}

static Py_ssize_t PyDgramObject_getwritebuf(PyDgramObject *self, Py_ssize_t segment, void **ptrptr) {
    return PyDgramObject_getreadbuf(self, segment, (void **)ptrptr);
}

static Py_ssize_t PyDgramObject_getcharbuf(PyDgramObject *self, Py_ssize_t segment, constchar **ptrptr) {
    return PyDgramObject_getreadbuf(self, segment, (void **) ptrptr);
}
#endif /* PY_MAJOR_VERSION < 3 */

static int PyDgramObject_getbuffer(PyObject *obj, Py_buffer *view, int flags)
{
    if (view == 0) {
        PyErr_SetString(PyExc_ValueError, "NULL view in getbuffer");
        return -1;
    }

    PyDgramObject* self = (PyDgramObject*)obj;
    view->obj = (PyObject*)self;
    view->buf = (void*)self->dgram;
    if (self->dgram->xtc.extent == 0) {
        view->len = BUFSIZE; // share max size for empty dgram 
    } else {
        view->len = sizeof(*self->dgram) + self->dgram->xtc.sizeofPayload();
    }
    view->readonly = 1;
    view->itemsize = 1;
    view->format = (char *)"s";
    view->ndim = 1;
    view->shape = &view->len;
    view->strides = &view->itemsize;
    view->suboffsets = NULL;
    view->internal = NULL;
    
    Py_INCREF(self);  
    return 0;    
}

static PyObject* dgram_get_seq(PyDgramObject* self, void *closure) {
    if (!self->is_pyseq_valid) {
        //cpo: wasteful to do the Import here every time?
        PyObject* seqmod = PyImport_ImportModule("psana.seq");
        PyObject* pyseqtype = PyObject_GetAttrString(seqmod,"Seq");
        PyObject* capsule = PyCapsule_New((void*)&(self->dgram->seq), NULL, NULL);
        PyObject* arglist = Py_BuildValue("(O)", capsule);
        Py_DECREF(capsule); // now owned by the arglist
        Py_DECREF(seqmod);
        Py_DECREF(pyseqtype);
        self->pyseq = PyObject_CallObject(pyseqtype, arglist);
        self->is_pyseq_valid = true;
    }

    Py_INCREF(self->pyseq);
    return self->pyseq;
}

static PyBufferProcs PyDgramObject_as_buffer = {
#if PY_MAJOR_VERSION < 3
    (readbufferproc)PyDgramObject_getreadbuf,   /*bf_getreadbuffer*/
    (writebufferproc)PyDgramObject_getwritebuf, /*bf_getwritebuffer*/
    (segcountproc)PyDgramObject_getsegcount,    /*bf_getsegcount*/
    (charbufferproc)PyDgramObject_getcharbuf,   /*bf_getcharbuffer*/
#endif
    // this definition is only compatible with Python 3.3 and above
    (getbufferproc)PyDgramObject_getbuffer,
    (releasebufferproc)0, // no special release required
};

static PyMemberDef dgram_members[] = {
    { (char*)"__dict__",
      T_OBJECT_EX, offsetof(PyDgramObject, dict),
      0,
      (char*)"attribute dictionary" },
    { (char*)"_file_descriptor",
      T_INT, offsetof(PyDgramObject, file_descriptor),
      0,
      (char*)"attribute file_descriptor" },
    { (char*)"_offset",
      T_INT, offsetof(PyDgramObject, offset),
      0,
      (char*)"attribute offset" },
    { (char*)"_dgrambytes",
      T_OBJECT_EX, offsetof(PyDgramObject, dgrambytes),
      0,
      (char*)"attribute offset" },
    { NULL }
};

static PyGetSetDef dgram_getset[] = {
    { (char*)"seq",
      (getter)dgram_get_seq,
      NULL,
      (char*)"Dgram::Sequence",
      NULL },
    { NULL }
};

static PyObject* dgram_assign_dict(PyDgramObject* self) {
    AssignDict(self, 0); // Todo: may need to be fixed for other non-config dgrams
    Py_RETURN_NONE;
}

static PyMethodDef dgram_methods[] = {
    {"_assign_dict", (PyCFunction)dgram_assign_dict, METH_NOARGS,
     "Assign dictionary to the dgram"
    },
    {NULL}  /* Sentinel */
};

static PyTypeObject dgram_DgramType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "dgram.Dgram", /* tp_name */
    sizeof(PyDgramObject), /* tp_basicsize */
    0, /* tp_itemsize */
    (destructor)dgram_dealloc, /* tp_dealloc */
    0, /* tp_print */
    0, /* tp_getattr */
    0, /* tp_setattr */
    0, /* tp_compare */
    0, /* tp_repr */
    0, /* tp_as_number */
    0, /* tp_as_sequence */
    0, /* tp_as_mapping */
    0, /* tp_hash */
    0, /* tp_call */
    0, /* tp_str */
    0, /* tp_getattro */
    0, /* tp_setattro */
    &PyDgramObject_as_buffer, /* tp_as_buffer */
    (Py_TPFLAGS_DEFAULT 
#if PY_MAJOR_VERSION < 3
    | Py_TPFLAGS_CHECKTYPES
    | Py_TPFLAGS_HAVE_NEWBUFFER
#endif
    | Py_TPFLAGS_BASETYPE),   /* tp_flags */
    0, /* tp_doc */
    0, /* tp_traverse */
    0, /* tp_clear */
    0, /* tp_richcompare */
    0, /* tp_weaklistoffset */
    0, /* tp_iter */
    0, /* tp_iternext */
    dgram_methods, /* tp_methods */
    dgram_members, /* tp_members */
    dgram_getset, /* tp_getset */
    0, /* tp_base */
    0, /* tp_dict */
    0, /* tp_descr_get */
    0, /* tp_descr_set */
    offsetof(PyDgramObject, dict), /* tp_dictoffset */
    (initproc)dgram_init, /* tp_init */
    0, /* tp_alloc */
    dgram_new, /* tp_new */
    0, /* tp_free;  Low-level free-memory routine */
    0, /* tp_is_gc;  For PyObject_IS_GC */
    0, /* tp_bases*/
    0, /* tp_mro;  method resolution order */
    0, /* tp_cache*/
    0, /* tp_subclasses*/
    0, /* tp_weaklist*/
    (destructor)dgram_dealloc, /* tp_del*/
};

#if PY_MAJOR_VERSION > 2
static PyModuleDef dgrammodule =
{ PyModuleDef_HEAD_INIT, "dgram", NULL, -1, NULL, NULL, NULL, NULL, NULL };
#endif

#ifndef PyMODINIT_FUNC /* declarations for DLL import/export */
#define PyMODINIT_FUNC void
#endif

#if PY_MAJOR_VERSION > 2
PyMODINIT_FUNC PyInit_dgram(void)
{
    PyObject* m;

    import_array();

    if (PyType_Ready(&dgram_DgramType) < 0) {
        return NULL;
    }

    m = PyModule_Create(&dgrammodule);
    if (m == NULL) {
        return NULL;
    }

    Py_INCREF(&dgram_DgramType);
    PyModule_AddObject(m, "Dgram", (PyObject*)&dgram_DgramType);
    return m;
}
#else
PyMODINIT_FUNC initdgram(void) {
    PyObject *m;
    
    import_array();

    if (PyType_Ready(&dgram_DgramType) < 0)
        return;

    m = Py_InitModule3("dgram", dgram_methods, "Dgram module.");

    if (m == NULL)
        return;

    Py_INCREF(&dgram_DgramType);
    PyModule_AddObject(m, "Dgram", (PyObject *)&dgram_DgramType);
}
#endif
