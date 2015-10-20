#ifndef QSORT_H
    #ifndef SHARED_ARGS_H
        struct SharedArgs
        {
            int *shMem;
            int *semId;
            int *MAX_PROC;
            int *MAX_R_CALLS;
            int rCallsCount;
        };
        typedef struct SharedArgs SharedArgs;
        #define SHARED_ARGS_H
    #endif
    void quickSort(int *array, int first, int last, SharedArgs *sharedArgs);
    #define QSORT_H
#endif
