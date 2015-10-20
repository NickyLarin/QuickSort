#ifndef PROCESS_CONTROL_H
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
    int createChildProcess(SharedArgs *sharedArgs);
    void stopChildProcess(SharedArgs *sharedArgs);
    #define PROCESS_CONTROL_H
#endif
