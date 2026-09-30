#include <assert.h>
#include <stdio.h>

#include "cart.h"

int main(void)
{
    dish_t rice = { .id = 1, .category = "主食", .name = "鸡腿饭", .price = 12.0f };
    cart_t cart;

    cart_init(&cart);
    assert(cart_add(&cart, &rice, 2) == 0);

    /* - 按钮点击一次后，只减少一份，不应整项删除。 */
    assert(cart_remove(&cart, 1) == 0);
    assert(cart_total_price(&cart) == 12.0f);

    cart_clear(&cart);
    puts("remove one test passed");
    return 0;
}
