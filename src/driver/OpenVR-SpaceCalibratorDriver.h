#pragma once

#ifdef __GNUC__
#define OPENVRSPACECALIBRATORDRIVER_API extern "C" __attribute__ ((visibility ("default")))
#else
#define OPENVRSPACECALIBRATORDRIVER_API extern "C"
#endif

