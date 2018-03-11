/* xntp.c -- Python interface to xntpd shm driver
 *
 * Copyright (c) 2003  Jonathan Brady (jtjbrady@sourceforge.net)
 *
 * The SHM code to interface with the NTP SHM reference clock driver and
 * the algorithm for UTCtime from code by Jonathan A. Buzzard
 * (jonathan@buzzard.org.uk)
 *
 * $Log: xntp.c,v $
 * Revision 1.1.1.1  2003/11/07 23:37:21  jtjbrady
 * Initial Revision
 *
 * Revision 1.0  2002/01/29 00:51:02  jtjbrady
 * Initial revision
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "Python.h"
#include <sys/ipc.h>
#include <sys/shm.h>

/*
 * NTPD shared memory reference clock driver structure
 */
#define SHMKEY 0x4e545030
struct shmTime {
        int     mode;
        int     count;
        time_t  clockTimeStampSec;
        int     clockTimeStampUSec;
        time_t  receiveTimeStampSec;
        int     receiveTimeStampUSec;
        int     leap;
        int     precision;
        int     nsamples;
        int     valid;
        int     dummy[10];
};


typedef struct {
    PyObject_HEAD
    struct shmTime *shm;
    int unit;
} shmsegment;


/*
 * Like mktime but ignores the current time zone and daylight savings, expects
 * an already normalized tm stuct, and does not recompute tm_yday and tm_wday.
 */
time_t UTCtime(struct tm *timeptr)
{
    int bits,direction,secs;
    struct tm search;
    time_t timep;


    /* calculate the number of magnitude bits in a time_t */
    for (bits=0,timep=1;timep>0;bits++,timep<<=1)
        ;

    /* if time_t is signed, 0 is the median value else 1<<bits is median */
    timep = (timep<0) ? 0 : ((time_t) 1<<bits);

    /* save the seconds, and take them out of the search */
    secs = timeptr->tm_sec;
    timeptr->tm_sec = 0;

    /* binary search of the time space using the system gmtime() function */
    for (;;) {
            search = *gmtime(&timep);

        /* compare the two times down to the same day */
        if (((direction = (search.tm_year-timeptr->tm_year))==0) &&
            ((direction = (search.tm_mon-timeptr->tm_mon))==0))
            direction = (search.tm_mday-timeptr->tm_mday);

        /* compare the rest of the way if necesary */
        if (direction==0) {
            if (((direction = (search.tm_hour-timeptr->tm_hour))==0) &&
                ((direction = (search.tm_min-timeptr->tm_min))==0))
                direction = search.tm_sec-timeptr->tm_sec;
        }

        /* is the search complete? */
        if (direction==0) {
            timeptr->tm_sec	= secs;
            return timep+secs;
        } else {
            if (bits--<0)
                return -1;
            if (bits<0)
                timep--;
            else if (direction>0)
                timep -= (time_t) 1 << bits;
            else
                timep += (time_t) 1 << bits;
        }
    }

    return -1;
}

/* Accuracy is assumed to be 2^PRECISION seconds -10 is approximately 980uS */
#define PRECISION (-10)
enum { LEAP_NOWARNING=0x00, LEAP_NOTINSYNC=0x03};


static PyObject *settime(shmsegment *self, PyObject *args)
{
    PyObject *retval = Py_None;

    PyObject *sequence;
    if (!PyArg_ParseTuple(args, "O", &sequence))
        return NULL;

    if (!PySequence_Check(sequence) || PySequence_Size(sequence) < 6)
    {
        PyErr_SetString(PyExc_TypeError, "argument must be sequence of at least length 6");
        return NULL;
    }

    PyObject *tuple;
    if (!(tuple = PySequence_Tuple(sequence)))
        return NULL;


    struct tm argtime;

    if (!PyArg_ParseTuple(tuple, "iiiiii|iii",
        &argtime.tm_year, &argtime.tm_mon, &argtime.tm_mday,
        &argtime.tm_hour, &argtime.tm_min, &argtime.tm_sec,
        &argtime.tm_wday, &argtime.tm_yday, &argtime.tm_isdst))
        return NULL;

    Py_DECREF(tuple);

    argtime.tm_mon -=1; /* Relative to January */
    argtime.tm_year -= 1900; /* Relative to 1900 */

    time_t given;
    if ((given = UTCtime(&argtime)) < 0)
    {
        PyErr_SetString(PyExc_ValueError, "Invalid time specified");
        return NULL;
    }

    struct timeval computer;
    if (gettimeofday(&computer, NULL))
    {
        PyErr_SetString(PyExc_RuntimeError, strerror(errno));
        return NULL;
    }

    self->shm->mode = 1;
    self->shm->valid = 0;

    __asm__ __volatile__ ("":::"memory");

    self->shm->leap = LEAP_NOWARNING;
    self->shm->precision = PRECISION;
    self->shm->clockTimeStampSec = given;
    self->shm->clockTimeStampUSec = 0;
    self->shm->receiveTimeStampSec = computer.tv_sec;
    self->shm->receiveTimeStampUSec = computer.tv_usec;

    __asm__ __volatile__ ("":::"memory");

    self->shm->count++;
    self->shm->valid = 1;

    Py_XINCREF(retval);
    return retval;
}


static struct PyMethodDef Methods[] = {
    {"settime", (PyCFunction)settime, METH_VARARGS,
        "settime(tuple)\n"
        "\n"
        "Set memory block according to a time tuple using the time the function is\n"
        "called as the time the message was received.  Format of tuple as per the time\n"
        "module i.e. (tm_year, tm_mon, tm_day, tm_hour, tm_min, tm_sec, tm_wday,\n"
        "tm_yday, tm_isdst) - Note: the last three values are optional and ignored.\n"
        },
    {NULL, NULL, 0, NULL}
};


static PyObject *dealloc(shmsegment *self)
{
    shmdt(self->shm);
    free(self);
}

static PyObject *getattr(shmsegment *self, char *name)
{
    PyObject *retval;

    while(1)
    {
        /* First handle attribute names we know about */
        if (!strcmp(name, "unit"))
        {
            retval = Py_BuildValue("i", self->unit);
            break;
        }

        /* Next handle __members__ to list the above */
        if (!strcmp(name, "__members__"))
        {
            retval = Py_BuildValue("[s]", "unit");
            break;
        }

        /* Finally find any additional methods */
        retval = Py_FindMethod(Methods, (PyObject *)self, name);
        break;
    }

    return retval;
}

static PyObject *repr(shmsegment *self)
{
    return PyString_FromFormat("<xntpshm object at %p, unit '%d'>", self, self->unit);
}

static PyTypeObject ShmSegmentType = {
    PyObject_HEAD_INIT(&PyType_Type)
    0,
    "xntpshm",
    sizeof(shmsegment),
    0,

    (destructor) dealloc,
    (printfunc) 0,
    (getattrfunc) getattr,
    (setattrfunc) 0,
    (cmpfunc) 0,
    (reprfunc) repr,
};

static PyObject *shmsegment_new(PyObject *self, PyObject *args)
{
    int unit = 0;

    if (!PyArg_ParseTuple(args, "|i", &unit))
        return NULL;

    if (unit < 0 || unit > 3)
    {
        PyErr_SetString(PyExc_ValueError, "unit number must be between 0 and 3");
        return NULL;
    }

    struct shmTime *shm;
    int shmid = shmget(SHMKEY+unit, sizeof(struct shmTime), IPC_CREAT | (unit < 2 ? 0700 : 0770));
    if (shmid!=-1)
        shm = (struct shmTime *)shmat(shmid, 0, 0);


    if (shmid==-1 || (shm==(void *)-1) || (shm==0))
    {
        PyErr_SetString(PyExc_RuntimeError, strerror(errno));
        return NULL;
    }

    shmsegment *obj = malloc(sizeof(shmsegment));
    PyObject_Init((PyObject *)obj, &ShmSegmentType);

    obj->shm = shm;
    obj->unit = unit;

    return (PyObject *)obj;
}

static struct PyMethodDef ModuleMethods[] = {
    {"xntpshm", shmsegment_new, METH_VARARGS,
        "xntpshm([int])\n"
        "\n"
        "Create a shared memory segment object for the given xntp unit number (0-3)\n"
        "\n"
        "Attributes:\n"
        "\n"
        "unit - returns the unit number given at startup\n"
        "\n"
        "\n"
        "Functions:\n"
        "\n"
        "settime(tuple)\n"
        "\n"
        "Set memory block according to a time tuple using the time the function is\n"
        "called as the time the message was received.  Format of tuple as per the time\n"
        "module i.e. (tm_year, tm_mon, tm_day, tm_hour, tm_min, tm_sec, tm_wday,\n"
        "tm_yday, tm_isdst) - Note: the last three values are optional and ignored.\n"
         },
    {NULL, NULL, 0, NULL}
};


void initxntp()
{
    PyObject *module = Py_InitModule3("xntp", ModuleMethods,
        "There exists a generic interface to xntpd for a shared memory driver, this\n"
        "module provides a way of accessing it from python.  Whilst there exist many\n"
        "drivers for xntpd not all clocks are supported.  Additionally by using the\n"
        "generic drivers some possible functionality may be lost.  For instance if I\n"
        "use the NMEA driver how do I extract position information to an external\n"
        "program?\n"
        );
}
