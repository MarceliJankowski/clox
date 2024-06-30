#include "test_common.h"
#include "util/memory.h"

// *---------------------------------------------*
// *                 TEST CASES                  *
// *---------------------------------------------*

static void get_byte__retrieves_bytes_in_LSB_to_MSB_order(void **const _) {
  uint8_t const MSB = 1;
  uint8_t const LSB = 0;
  uint32_t const object = ((0u | MSB) << CHAR_BIT) | LSB;

  uint8_t const retrieved_first_byte = get_byte(object, 0);
  uint8_t const retrieved_second_byte = get_byte(object, 1);

  assert_int_equal(retrieved_first_byte, LSB);
  assert_int_equal(retrieved_second_byte, MSB);
}

static void concatenate_bytes__concatenates_bytes_in_MSB_to_LSB_order(void **const _) {
  uint32_t const object = 0x010203;
  uint8_t const LSB = (uint8_t)object;
  uint8_t const middle_byte = (uint8_t)(object >> CHAR_BIT);
  uint8_t const MSB = (uint8_t)(object >> 2 * CHAR_BIT);

  uint32_t const result = concatenate_bytes(3, MSB, middle_byte, LSB);

  assert_true(result == object);
}

int main(void) {
  struct CMUnitTest const tests[] = {
    cmocka_unit_test(get_byte__retrieves_bytes_in_LSB_to_MSB_order),
    cmocka_unit_test(concatenate_bytes__concatenates_bytes_in_MSB_to_LSB_order)
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
