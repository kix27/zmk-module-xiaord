#include <stdbool.h>
#include <zephyr/kernel.h>

struct status_page_ops {
    void (*create)(void);
    void (*destroy)(void);
    void (*show)(void);
    void (*hide)(void);
};

static void noop(void) {}

const struct status_page_ops page_bt_ops = {
    .create = noop,
    .destroy = noop,
    .show = noop,
    .hide = noop,
};
