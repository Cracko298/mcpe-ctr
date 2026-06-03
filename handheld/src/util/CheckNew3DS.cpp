#include "CheckNew3DS.h"

#ifdef __3DS__
#include <3ds.h>
#endif

bool IsNew3DS()
{
#ifdef __3DS__
    bool isNew = false;

    Result rc = APT_CheckNew3DS(&isNew);
    if (R_FAILED(rc))
        return false;

    return isNew;
#else
    return false;
#endif
}
