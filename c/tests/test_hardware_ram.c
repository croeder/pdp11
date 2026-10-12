/* test_hardware_ram.c - translation of test_hardware_ram.py */
#include "pdp11_config.h"
#include "pdp11_hardware.h"
#include "pdp11_logger.h"
#include "test.h"

/* class attributes: reg, config, Logger(), ram */
static Registers reg;
static Config config;
static Lock lock;
static Ram ram;

static void test_ram_16(void)
{
    logging_info("test_ram_16");
    Lock l;
    Lock_init(&l);
    Ram r;
    Ram_init(&r, &l, &reg, 16);
    ASSERT_EQ(r.top_of_memory, 0177777);
    ASSERT_EQ(r.io_space, 0160000);
    Ram_free(&r);
}

static void test_ram_18(void)
{
    logging_info("test_ram_18");
    Lock l;
    Lock_init(&l);
    Ram r;
    Ram_init(&r, &l, &reg, 18);
    ASSERT_EQ(r.top_of_memory, 0777777);
    ASSERT_EQ(r.io_space, 0760000);
    Ram_free(&r);
}

static void test_ram_24(void)
{
    logging_info("test_ram_24");
    Lock l;
    Lock_init(&l);
    Ram r;
    Ram_init(&r, &l, &reg, 24);
    ASSERT_EQ(r.top_of_memory, 077777777);
    ASSERT_EQ(r.io_space, 077760000);
    Ram_free(&r);
}

static void test_ram_byte_1(void)
{
    logging_info("test_ram_byte_1");
    pyint test_value = 0107;
    pyint test_address = 04123;
    Ram_write_byte(&ram, test_address, test_value);
    ASSERT_EQ(*Ram_memory(&ram, test_address), test_value);
    ASSERT_EQ(Ram_read_byte(&ram, test_address), test_value);
}

static void test_ram_byte_2(void)
{
    logging_info("test_ram_byte_2");
    pyint test_value = 0270;
    pyint test_address = 04123;
    Ram_write_byte(&ram, test_address, test_value);
    ASSERT_EQ(*Ram_memory(&ram, test_address), test_value);
    ASSERT_EQ(Ram_read_byte(&ram, test_address), test_value);
}

static void test_ram_bytes(void)
{
    logging_info("test_ram_bytes");
    pyint test_value = 0105;
    pyint test_address = ram.top_of_memory - 1;
    Ram_write_byte(&ram, test_address, test_value);
    ASSERT_EQ(Ram_read_byte(&ram, test_address), test_value);

    for (pyint i = 0; i < 0277; i++)
        Ram_write_byte(&ram, i, i);

    for (pyint i = 0; i < 0277; i += 2)
        ASSERT_EQ(Ram_read_byte(&ram, i), i);
}

static void test_ram_word_1(void)
{
    logging_info("test_ram_word_1");
    pyint test_value = 0107070;
    pyint test_address = 06234;
    Ram_write_word(&ram, test_address, test_value);
    pyint actual_value = Ram_read_word(&ram, test_address);
    ASSERT_EQ(*Ram_memory(&ram, test_address), test_value & MASK_LOW_BYTE);
    ASSERT_EQ(*Ram_memory(&ram, test_address + 1), (test_value & MASK_HIGH_BYTE) >> 8);
    ASSERT_EQ(actual_value, test_value);
}

static void test_ram_word_2(void)
{
    logging_info("test_ram_word_2");
    pyint test_value = 0170707;
    pyint test_address = 06234;
    Ram_write_word(&ram, test_address, test_value);
    pyint actual_value = Ram_read_word(&ram, test_address);
    ASSERT_EQ(*Ram_memory(&ram, test_address), test_value & MASK_LOW_BYTE);
    ASSERT_EQ(*Ram_memory(&ram, test_address + 1), (test_value & MASK_HIGH_BYTE) >> 8);
    ASSERT_EQ(actual_value, test_value);
}

static void test_ram_word_top(void)
{
    logging_info("test_ram_word_top");
    pyint test_value = 0123232;
    pyint test_address = ram.top_of_memory - 1;
    Ram_write_word(&ram, test_address, test_value);
    ASSERT_EQ(*Ram_memory(&ram, test_address), test_value & MASK_LOW_BYTE);
    ASSERT_EQ(*Ram_memory(&ram, test_address + 1), (test_value & MASK_HIGH_BYTE) >> 8);
    ASSERT_EQ(Ram_read_word(&ram, test_address), test_value);
}

static void test_ram_words(void)
{
    logging_info("test_ram_words");
    pyint test_value = 0123234;
    for (pyint i = 1000; i < 2000; i += 2)
        Ram_write_word(&ram, i, test_value + i);
    for (pyint i = 1000; i < 2000; i += 2)
        ASSERT_EQ(Ram_read_word(&ram, i), test_value + i);
}

static void test_ram_wordbyte(void)
{
    logging_info("test_ram_wordbyte");
    pyint test_base = 010000;
    pyint test_range = 030000;
    for (pyint value = 0; value < test_range; value += 2)
        Ram_write_word(&ram, test_base + value, value);

    for (pyint value = 0; value < test_range; value += 2) {
        pyint low_byte = Ram_read_byte(&ram, test_base + value);
        pyint high_byte = Ram_read_byte(&ram, test_base + value + 1);
        pyint sum = low_byte + (high_byte << 8);
        ASSERT_EQ(low_byte, value & MASK_LOW_BYTE);
        ASSERT_EQ(high_byte, (value & MASK_HIGH_BYTE) >> 8);
        ASSERT_EQ(sum, value);
    }
}

int main(void)
{
    Registers_init(&reg);
    Config_init(&config, NULL);
    Logger();
    Lock_init(&lock);
    Ram_init(&ram, &lock, &reg, 16);

    RUN(test_ram_16);
    RUN(test_ram_18);
    RUN(test_ram_24);
    RUN(test_ram_byte_1);
    RUN(test_ram_byte_2);
    RUN(test_ram_bytes);
    RUN(test_ram_word_1);
    RUN(test_ram_word_2);
    RUN(test_ram_word_top);
    RUN(test_ram_words);
    RUN(test_ram_wordbyte);
    return TEST_SUMMARY("test_hardware_ram");
}
