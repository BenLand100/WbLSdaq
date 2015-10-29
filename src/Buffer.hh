/**
 *  Copyright 2014 by Benjamin Land (a.k.a. BenLand100)
 *
 *  WbLSdaq is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  WbLSdaq is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with WbLSdaq. If not, see <http://www.gnu.org/licenses/>.
 */

#include <pthread.h>
#include <cstring>

#ifndef Buffer__hh
#define Buffer__hh

class Buffer {
    public:
        Buffer(size_t _size);
        
        virtual ~Buffer();
        
        inline double pct() {
            return 100.0*fill()/size;
        }
        
        inline size_t free() {
            pthread_mutex_lock(&mutex);
            const size_t amt = size - (w_ptr - r_ptr);
            pthread_mutex_unlock(&mutex);
            return amt;
        }
        
        inline size_t fill() {
            pthread_mutex_lock(&mutex);
            const size_t amt = w_ptr - r_ptr;
            pthread_mutex_unlock(&mutex);
            return amt;
        }
        
        inline void inc(size_t amt) {
            pthread_mutex_lock(&mutex);
            w_ptr += amt;
            if ((size_t)(r_ptr - buffer) >= size) {
                const size_t total = w_ptr - r_ptr;
                memcpy(buffer,r_ptr,total);
                r_ptr = buffer;
                w_ptr = buffer + total;
            }
            pthread_mutex_unlock(&mutex);
            pthread_cond_signal(&cond);
        }
        
        inline void dec(size_t amt) {
            pthread_mutex_lock(&mutex);
            r_ptr += amt;
            pthread_mutex_unlock(&mutex);
        }
        
        inline char* wptr() {
            pthread_mutex_lock(&mutex);
            char *ptr = w_ptr;
            pthread_mutex_unlock(&mutex);
            return ptr;
        }
        
        inline char* rptr() {
            pthread_mutex_lock(&mutex);
            char *ptr = r_ptr;
            pthread_mutex_unlock(&mutex);
            return ptr;
        }
        
        inline void ready() {
            pthread_mutex_lock(&mutex);
            while (w_ptr - r_ptr == 0) pthread_cond_wait(&cond,&mutex);
            pthread_mutex_unlock(&mutex);
        }
        
    protected:
        pthread_mutex_t mutex;
        pthread_cond_t cond;
        char *buffer, *r_ptr, *w_ptr;
        size_t size;
};

#endif

