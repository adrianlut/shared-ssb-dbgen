//
// Created by adrian on 06.01.23.
//

#ifndef SSB_DBGEN_UTIL_H
#define SSB_DBGEN_UTIL_H

void
print_long_array(char * name, long * arr, int size) {
    printf("%s: [", name);
    for (int i = 0; i < size - 1; ++i) {
        printf("%ld,", arr[i]);
    }
    printf("%ld]\n", arr[size - 1]);
}

void
print_int_array(char * name, int * arr, int size) {
    printf("%s: [", name);
    for (int i = 0; i < size - 1; ++i) {
        printf("%d,", arr[i]);
    }
    printf("%d]\n", arr[size - 1]);
}

#endif //SSB_DBGEN_UTIL_H
