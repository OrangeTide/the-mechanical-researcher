/* ncdemo.c : phase 1 link/smoke test for the netchan core */
/* Made by a machine. PUBLIC DOMAIN (CC0-1.0) */

#include "netchan.h"
#include <stdio.h>

int
main(void)
{
    struct netchan *c;
    struct nc_chan *rel, *unrel;

    c = nc_open(0);
    if (!c) {
        printf("nc_open failed\n");
        return 1;
    }

    rel = nc_chan_open(c, NC_RELIABLE);
    unrel = nc_chan_open(c, NC_UNRELIABLE);

    printf("netchan core OK: state=%d reliable_chan=%d unreliable_chan=%d\n",
           nc_state(c), nc_chan_id(rel), nc_chan_id(unrel));
    printf("config: MTU=%d WINDOW=%d MAX_CHAN=%d MAX_CONN=%d\n",
           NC_MTU, NC_WINDOW, NC_MAX_CHAN, NC_MAX_CONN);

    nc_close(c);
    return 0;
}
