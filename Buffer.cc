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
 
#include "Buffer.hh"

Buffer::Buffer(size_t _size) : size(_size) {
    buffer = new char[size*2];
    r_ptr = w_ptr = buffer;
    pthread_mutex_init(&mutex,NULL);
    pthread_cond_init(&cond, NULL);
}

Buffer::~Buffer() {
    delete [] buffer;
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);
}

