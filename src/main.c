/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright 2014 Joshua C. Klontz                                           *
 *                                                                           *
 * Licensed under the Apache License, Version 2.0 (the "License");           *
 * you may not use this file except in compliance with the License.          *
 * You may obtain a copy of the License at                                   *
 *                                                                           *
 *     http://www.apache.org/licenses/LICENSE-2.0                            *
 *                                                                           *
 * Unless required by applicable law or agreed to in writing, software       *
 * distributed under the License is distributed on an "AS IS" BASIS,         *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  *
 * See the License for the specific language governing permissions and       *
 * limitations under the License.                                            *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <stdlib.h>
#include <string.h>
#include <likely.h>

// This function should be provided by the likely static compiler
extern likely_mat likely_test_function(const likely_const_mat *args);

int main(int argc, char *argv[])
{
    likely_ensure(argc > 1, "expected at least one argument, the return value.");

    likely_const_mat *const args = (likely_const_mat*) malloc(sizeof(likely_const_mat) * (argc-1));
    for (int i=1; i<argc; i++)
        args[i-1] = !strcmp(argv[i], "-") ? NULL : likely_compute(argv[i]);

    const likely_const_mat result = likely_test_function(args + 1);
    const likely_const_mat rendered = likely_render(result, NULL, NULL);
    if (args[0]) likely_ensure_approximately_equal(args[0], rendered, 0.03f);
    else         likely_show(rendered, argv[0]);
    likely_release_mat(rendered);
    likely_release_mat(result);

    for (int i=0; i<argc-1; i++)
        likely_release_mat(args[i]);
    free((void*) args);

    likely_shutdown();
    return EXIT_SUCCESS;
}
