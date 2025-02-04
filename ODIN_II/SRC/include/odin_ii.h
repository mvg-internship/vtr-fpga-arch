/*
 * Copyright 2023 CAS—Atlantic (University of New Brunswick, CASA)
 * 
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef ODIN_II_H
#define ODIN_II_H

#include "odin_types.h"
/* Odin-II exit status enumerator */
enum ODIN_ERROR_CODE {
    SUCCESS,
    ERROR_INITIALIZATION,
    ERROR_PARSE_CONFIG,
    ERROR_PARSE_ARGS,
    ERROR_PARSE_ARCH,
    ERROR_PARSE_BLIF,
    ERROR_ELABORATION,
    ERROR_OPTIMIZATION,
    ERROR_TECHMAP,
    ERROR_SYNTHESIS,
    ERROR_OUTPUT
};

netlist_t* start_odin_ii(int argc, char** argv);
int terminate_odin_ii(netlist_t* odin_netlist);

void set_default_config();
void get_options(int argc, char** argv);

#endif
