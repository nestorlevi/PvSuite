/***************************************************************************
                          pvbthreaddata.h  -  description
                             -------------------
    begin                : 2026
    copyright            : (C) Nestor Palomeque, pvbrowser community
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This library is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU LESSER GENERAL PUBLIC LICENSE as        *
 *   published by the Free Software Foundation                             *
 *                                                                         *
 ***************************************************************************/
#ifndef _PVB_THREAD_DATA_H_
#define _PVB_THREAD_DATA_H_

#ifdef __MINGW32__
#include <pthread.h>
#endif

//! Dato de cada hilo (= de cada cliente). En MinGW un thread_local con destructor puede romper el
//! pvserver al terminar el hilo (emutls libera la memoria antes de llamar al destructor), asi que ahi
//! se usa una clave de pthread con el objeto en el heap. Tag: un numero distinto por cada dato del mismo tipo
//! (los de util.cpp usan 100 en adelante).
template<class T, int Tag = 0> inline T &pvbThreadData()
{
#ifdef __MINGW32__
  struct Key   // estatica local: se crea una vez (sin std::call_once, que tambien usa thread_local)
  {
    pthread_key_t key;
    Key() { pthread_key_create(&key, destroy); }
    static void destroy(void *v) { delete (T *) v; }
  };
  static Key k;
  T *v = (T *) pthread_getspecific(k.key);
  if(v == NULL) { v = new T(); pthread_setspecific(k.key, v); }
  return *v;
#else
  static thread_local T v;
  return v;
#endif
}

#endif
