#ifndef CEDDEC_INTERNAL_H
#define CEDDEC_INTERNAL_H
#if defined(_WIN32) || defined(_WIN64)
    #define CEDDEC_API __declspec(dllexport)
#else
    #define CEDDEC_API __attribute__((visibility("default")))
#endif
#endif
