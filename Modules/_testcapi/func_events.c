#include "parts.h"

static PyObject *pyfunc_callback = NULL;
static PyFunction_EventCallback orig_callback = NULL;

static void
call_pyfunc_callback(PyFunction_Event event, PyFunctionObject *func, PyObject *new_value)
{
    PyObject *event_obj = PyLong_FromLong(event);
    if (event_obj == NULL) {
        PyErr_Clear();
        return;
    }
    if (new_value == NULL) {
        new_value = Py_None;
    }
    Py_INCREF(new_value);
    /* Don't expose a function that's about to be destroyed to managed code */
    PyObject *func_or_id = (PyObject *) func;
    if (event == PYFUNC_EVENT_DESTROY) {
        func_or_id = PyLong_FromLong((long) func);
        if (func_or_id == NULL) {
            Py_DECREF(new_value);
            Py_DECREF(event_obj);
            return;
        }
    } else {
        Py_INCREF(func);
    }
    PyObject *stack[] = {event_obj, func_or_id, new_value};
    PyObject_Vectorcall(pyfunc_callback, stack, 3, NULL);
    Py_DECREF(new_value);
    Py_DECREF(event_obj);
    Py_DECREF(func_or_id);
}

static int
add_event(PyObject *module, const char *name, PyFunction_Event event)
{
    PyObject *value = PyLong_FromLong(event);
    if (value == NULL) {
        return -1;
    }
    int ok = PyModule_AddObjectRef(module, name, value);
    Py_DECREF(value);
    return ok;
}

static PyObject *
set_func_event_callback(PyObject *self, PyObject *func)
{
    if (!PyFunction_Check(func)) {
        PyErr_SetString(PyExc_TypeError, "'func' must be a function");
        return NULL;
    }
    if (pyfunc_callback != NULL) {
        PyErr_SetString(PyExc_RuntimeError, "already set callback");
        return NULL;
    }
    Py_INCREF(func);
    pyfunc_callback = func;
    orig_callback = PyFunction_GetEventCallback();
    PyFunction_SetEventCallback(call_pyfunc_callback);
    Py_RETURN_NONE;
}

static PyObject *
restore_func_event_callback() {
    if (pyfunc_callback == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "nothing to restore");
        return NULL;
    }
    PyFunction_SetEventCallback(orig_callback);
    orig_callback = NULL;
    Py_CLEAR(pyfunc_callback);
    Py_RETURN_NONE;
}

static PyMethodDef TestMethods[] = {
    {"set_func_event_callback", set_func_event_callback, METH_O},
    {"restore_func_event_callback", restore_func_event_callback, METH_NOARGS},
    {NULL},
};

int
_PyTestCapi_Init_FuncEvents(PyObject *m) {
    if (PyModule_AddFunctions(m, TestMethods) < 0) {
        return -1;
    }

    /* Expose each event as an attribute on the module */
#define ADD_EVENT(event)  \
    if (add_event(m, "PYFUNC_EVENT_" #event, PYFUNC_EVENT_##event)) { \
        return -1;                                    \
    }
    FOREACH_FUNC_EVENT(ADD_EVENT);
#undef ADD_EVENT

    return 0;
}
