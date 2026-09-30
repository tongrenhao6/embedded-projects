#include <assert.h>
#include <stdio.h>

#include "menu.h"

void test_menu_loads_dishes_from_file(void)
{
    menu_t menu = {0};

    assert(menu_load(&menu, "tests/fixtures/menu.txt") == 0);
    assert(menu.count == 2);
    assert(menu.dishes[0].id == 1);
    assert(menu.dishes[0].price == 12.0f);
    assert(menu_find(&menu, 2) != NULL);
    assert(menu_find(&menu, 99) == NULL);
}
