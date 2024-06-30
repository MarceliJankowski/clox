#include "backend/chunk.h"
#include "frontend/compiler.h"
#include "global.h"
#include "test_common.h"
#include "util/error.h"
#include "util/memory.h"

// *---------------------------------------------*
// *               STATIC OBJECTS                *
// *---------------------------------------------*

static Chunk chunk;
static int chunk_code_offset;
static int chunk_constant_instruction_index;

// *---------------------------------------------*
// *                  UTILITIES                  *
// *---------------------------------------------*

static CompilationStatus compile(char const *const source_code) {
  chunk_reset(&chunk);
  chunk_code_offset = 0;
  chunk_constant_instruction_index = 0;

  return compiler_compile(source_code, &chunk);
}

#define compile_assert_success(source_code) assert_int_equal(compile(source_code), COMPILATION_SUCCESS)
#define compile_assert_failure(source_code) assert_int_equal(compile(source_code), COMPILATION_FAILURE)
#define compile_assert_unexpected_eof(source_code) assert_int_equal(compile(source_code), COMPILATION_UNEXPECTED_EOF)

#define next_chunk_code_byte() chunk.code[chunk_code_offset++]

#define assert_instruction_line(expected_line) \
  assert_int_equal(chunk_get_instruction_line(&chunk, chunk_code_offset), expected_line)

#define assert_opcode(expected_opcode) assert_int_equal(next_chunk_code_byte(), expected_opcode)
#define assert_opcodes(...) APPLY_TO_EACH_ARG(assert_opcode, OpCode, __VA_ARGS__)

static void assert_constant_instruction(Value const expected_constant) {
  uint32_t constant_index;
  if (chunk_constant_instruction_index > UCHAR_MAX) {
    assert_opcode(OP_CONSTANT_2B);
    uint8_t const constant_index_LSB = next_chunk_code_byte();
    uint8_t const constant_index_MSB = next_chunk_code_byte();
    constant_index = concatenate_bytes(2, constant_index_MSB, constant_index_LSB);
  } else {
    assert_opcode(OP_CONSTANT);
    constant_index = next_chunk_code_byte();
  }

  assert_int_equal(constant_index, chunk_constant_instruction_index);
  assert_int_equal(chunk.constants.values[chunk_constant_instruction_index], expected_constant);
  chunk_constant_instruction_index++;
}
#define assert_constant_instructions(...) APPLY_TO_EACH_ARG(assert_constant_instruction, Value, __VA_ARGS__)

// *---------------------------------------------*
// *                  FIXTURES                   *
// *---------------------------------------------*

int group_setup(void **const _) {
  g_source_file = "compiler_test";
  g_static_err_stream = tmpfile();
  if (g_static_err_stream == NULL) IO_ERROR("%s", strerror(errno));

  chunk_init(&chunk);

  return 0;
}

// *---------------------------------------------*
// *                 TEST CASES                  *
// *---------------------------------------------*
static_assert(OP_OPCODE_COUNT == 9, "Exhaustive OpCode handling");

static void test_unexpected_eof(void **const _) {
  compile_assert_unexpected_eof("");
  compile_assert_unexpected_eof("(");
  compile_assert_unexpected_eof("1 + ");
}

static void test_line_tracking(void **const _) {
  compile_assert_success("1");
  assert_instruction_line(1);

  compile_assert_success("\n2");
  assert_instruction_line(2);

  compile_assert_success("\n\n3");
  assert_instruction_line(3);
}

static void test_numeric_literal(void **const _) {
  compile_assert_success("55");
  assert_constant_instruction(55);
  assert_opcode(OP_RETURN);

  compile_assert_success("-55");
  assert_constant_instruction(55);
  assert_opcodes(OP_NEGATE, OP_RETURN);

  compile_assert_success("10.25");
  assert_constant_instruction(10.25);
  assert_opcode(OP_RETURN);

  compile_assert_success("-10.25");
  assert_constant_instruction(10.25);
  assert_opcodes(OP_NEGATE, OP_RETURN);
}

static void test_arithmetic_operators(void **const _) {
  compile_assert_success("1 + 2");
  assert_constant_instructions(1, 2);
  assert_opcodes(OP_ADD, OP_RETURN);

  compile_assert_success("1 - 2");
  assert_constant_instructions(1, 2);
  assert_opcodes(OP_SUBTRACT, OP_RETURN);

  compile_assert_success("1 * 2");
  assert_constant_instructions(1, 2);
  assert_opcodes(OP_MULTIPLY, OP_RETURN);

  compile_assert_success("1 / 2");
  assert_constant_instructions(1, 2);
  assert_opcodes(OP_DIVIDE, OP_RETURN);

  compile_assert_success("1 % 2");
  assert_constant_instructions(1, 2);
  assert_opcodes(OP_MODULO, OP_RETURN);
}

static void test_arithmetic_operator_associativity(void **const _) {
  compile_assert_success("1 + 2 + 3");
  assert_constant_instructions(1, 2);
  assert_opcode(OP_ADD);
  assert_constant_instruction(3);
  assert_opcodes(OP_ADD, OP_RETURN);

  compile_assert_success("1 - 2 - 3");
  assert_constant_instructions(1, 2);
  assert_opcode(OP_SUBTRACT);
  assert_constant_instruction(3);
  assert_opcodes(OP_SUBTRACT, OP_RETURN);

  compile_assert_success("1 * 2 * 3");
  assert_constant_instructions(1, 2);
  assert_opcode(OP_MULTIPLY);
  assert_constant_instruction(3);
  assert_opcodes(OP_MULTIPLY, OP_RETURN);

  compile_assert_success("1 / 2 / 3");
  assert_constant_instructions(1, 2);
  assert_opcode(OP_DIVIDE);
  assert_constant_instruction(3);
  assert_opcodes(OP_DIVIDE, OP_RETURN);

  compile_assert_success("1 % 2 % 3");
  assert_constant_instructions(1, 2);
  assert_opcode(OP_MODULO);
  assert_constant_instruction(3);
  assert_opcodes(OP_MODULO, OP_RETURN);
}

static void test_arithmetic_operator_precedence(void **const _) {
  compile_assert_success("1 + 2 - 3");
  assert_constant_instructions(1, 2);
  assert_opcode(OP_ADD);
  assert_constant_instruction(3);
  assert_opcodes(OP_SUBTRACT, OP_RETURN);

  compile_assert_success("1 - 2 + 3");
  assert_constant_instructions(1, 2);
  assert_opcode(OP_SUBTRACT);
  assert_constant_instruction(3);
  assert_opcodes(OP_ADD, OP_RETURN);

  compile_assert_success("1 * 2 / 3 % 4");
  assert_constant_instructions(1, 2);
  assert_opcode(OP_MULTIPLY);
  assert_constant_instruction(3);
  assert_opcode(OP_DIVIDE);
  assert_constant_instruction(4);
  assert_opcodes(OP_MODULO, OP_RETURN);

  compile_assert_success("1 % 2 * 3 / 4");
  assert_constant_instructions(1, 2);
  assert_opcode(OP_MODULO);
  assert_constant_instruction(3);
  assert_opcode(OP_MULTIPLY);
  assert_constant_instruction(4);
  assert_opcodes(OP_DIVIDE, OP_RETURN);

  compile_assert_success("1 / 2 % 3 * 4");
  assert_constant_instructions(1, 2);
  assert_opcode(OP_DIVIDE);
  assert_constant_instruction(3);
  assert_opcode(OP_MODULO);
  assert_constant_instruction(4);
  assert_opcodes(OP_MULTIPLY, OP_RETURN);

  compile_assert_success("1 + 2 * 3");
  assert_constant_instructions(1, 2, 3);
  assert_opcodes(OP_MULTIPLY, OP_ADD, OP_RETURN);
}

static void test_grouping_expr(void **const _) {
  compile_assert_success("(1)");
  assert_constant_instruction(1);
  assert_opcode(OP_RETURN);

  compile_assert_success("(1 + 2)");
  assert_constant_instructions(1, 2);
  assert_opcodes(OP_ADD, OP_RETURN);

  compile_assert_success("(1 + 2) * 3");
  assert_constant_instructions(1, 2);
  assert_opcode(OP_ADD);
  assert_constant_instruction(3);
  assert_opcodes(OP_MULTIPLY, OP_RETURN);

  compile_assert_success("(1 + 2) * (3 / 4)");
  assert_constant_instructions(1, 2);
  assert_opcode(OP_ADD);
  assert_constant_instructions(3, 4);
  assert_opcodes(OP_DIVIDE, OP_MULTIPLY, OP_RETURN);
}

int main(void) {
  struct CMUnitTest const tests[] = {
    cmocka_unit_test(test_unexpected_eof),
    cmocka_unit_test(test_line_tracking),
    cmocka_unit_test(test_numeric_literal),
    cmocka_unit_test(test_arithmetic_operators),
    cmocka_unit_test(test_arithmetic_operator_associativity),
    cmocka_unit_test(test_arithmetic_operator_precedence),
    cmocka_unit_test(test_grouping_expr),
  };

  return cmocka_run_group_tests(tests, group_setup, NULL);
}
