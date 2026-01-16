#pragma once

#define SRPH_VERIFY(call, msg)               \
    {                                        \
        int r = (call);                      \
        if (r < 0)                           \
        {                                    \
            Log::Critical("{}: {}", msg, r); \
        }                                    \
    }