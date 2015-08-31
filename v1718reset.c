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

#include <CAENVMElib.h>
#include <iostream>

int main(int argc, char **argv) {

    if (argc != 2) {
        std::cout << "./v1718reset [link number]" << std::endl;
        return -1;
    }

	int handle;
	CAENVME_Init(cvV1718,atoi(argv[1]),0,&handle);
	CAENVME_SystemReset(handle);
	CAENVME_End(handle);
	return 0;

}
