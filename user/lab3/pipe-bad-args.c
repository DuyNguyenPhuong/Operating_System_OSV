#include <lib/test.h>
#include <lib/malloc.h>

int main()
{
    int ret;

    if ((ret = pipe((int*)0)) != ERR_FAULT)
    {
        error("pipetest: pipe() did not return ERR_FAULT appropriately, return value was %d", ret);
    }

    if ((ret = pipe((int*)0x12345)) != ERR_FAULT)
    {
        error("pipetest: pipe() did not return ERR_FAULT appropriately, return value was %d", ret);
    }

    pass("pipe-bad-args");
    exit(0);
    return 0;
}
