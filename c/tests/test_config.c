/* test_config.c - translation of test_config.py */
#include "pdp11_config.h"
#include "pdp11_logger.h"
#include "test.h"

static void test_default(void)
{
    Config config;
    Config_init(&config, NULL);
    ConfigValue name = Config_lookup(&config, "config", "name");
    ASSERT_EQ(name.kind, CONFIG_STRING);
    ASSERT_STR(name.string, "default");
}

static void test_ram(void)
{
    Config config;
    Config_init(&config, NULL);
    ConfigValue bits = Config_lookup(&config, "ram", "bits");
    ASSERT_EQ(bits.kind, CONFIG_NUMBER);
    ASSERT_EQ(bits.number, 16);
}

static void test_barf(void)
{
    Config config;
    Config_init(&config, NULL);
    ConfigValue barf = Config_lookup(&config, "config", "gook");
    ASSERT_STR(barf.string, "ERROR");

    barf = Config_lookup(&config, "gobbledy", "gook");
    ASSERT_STR(barf.string, "ERROR");
}

int main(void)
{
    Logger();
    RUN(test_default);
    RUN(test_ram);
    RUN(test_barf);
    return TEST_SUMMARY("test_config");
}
