##################################################
#               EXTERNAL CONSTANTS               #
##################################################
# meant to provide simple layer of customizability

LOX_EXEC_NAME ?= clox
SRC_DIR := src
INCLUDE_DIR := include
BUILD_DIR := build
BIN_DIR := bin
TEST_DIR := test
TEST_INCLUDE_DIR := ${TEST_DIR}/include
TEST_LIBS := cmocka

FIND ?= find
ECHO ?= echo
MKDIR := mkdir -p
RM := rm -r

CC ?=
CFLAGS ?=
CPPFLAGS ?=
TARGET_ARCH ?=
LDFLAGS ?=

BUILDS += release test debug
RELEASE_CFLAGS ?= -O3 -flto -march=native
TEST_CPPFLAGS ?=
TEST_CFLAGS ?=

# no optimizations; I've seen them interfere with UBSAN (-Og included)
DEBUG_CFLAGS ?= -ggdb -fno-omit-frame-pointer -fsanitize=address,undefined
DEBUG_CPPFLAGS ?= -D DEBUG

# allow user to set .DEFAULT_GOAL through DEFAULT_TARGET environmental variable (or CLI)
# while keeping default value assigned to DEFAULT_TARGET (works around '.DEFAULT_GOAL ?=' not being viable)
DEFAULT_TARGET ?= $(firstword ${BUILDS})
.DEFAULT_GOAL := ${DEFAULT_TARGET}

##################################################
#               INTERNAL VARIABLES               #
##################################################

non_test_builds := $(filter-out test,${BUILDS})

compile_cflags := -Wall -Wextra -Werror -std=c17 -pedantic
compile_cppflags := -I ${INCLUDE_DIR} -MMD -MP
link_flags := -lm

compile = $(strip ${CC} ${compile_cppflags} ${CPPFLAGS} ${compile_cflags} ${CFLAGS} ${TARGET_ARCH} -c)
link = $(strip ${CC} ${compile_cppflags} ${CPPFLAGS} ${compile_cflags} ${CFLAGS} ${link_flags} ${LDFLAGS} ${TARGET_ARCH})

sources := $(shell ${FIND} ${SRC_DIR} -type f -name '*.c')
source_objects := $(patsubst %.c,%.o,${sources})

unit_tests := $(shell ${FIND} ${TEST_DIR}/unit -type f -name '*.c')
unit_test_executables := $(patsubst %.c,${BIN_DIR}/test/unit/%,${unit_tests})

component_tests := $(shell ${FIND} ${TEST_DIR}/component -type f -name '*.c')
component_test_executables := $(patsubst %.c,${BIN_DIR}/test/component/%,${component_tests})

# release objects that component tests depend on; 'main.o' is excluded because each test file defines its own entry point
component_test_release_objects := $(addprefix ${BUILD_DIR}/release/,$(filter-out ${SRC_DIR}/main.o,${source_objects}))

unit_test_makefiles := $(shell ${FIND} ${TEST_DIR}/unit -type f -name '*.mk')
unit_test_mk_target_prefix := ${BIN_DIR}/test/unit/${TEST_DIR}/unit
unit_test_mk_prerequisite_prefix := ${BUILD_DIR}/release/${SRC_DIR}

# compiler generated makefiles tracking header dependencies
dependency_makefiles := $(foreach build,${non_test_builds},$(patsubst %.o,${BUILD_DIR}/${build}/%.d,${source_objects}))
dependency_makefiles += $(patsubst %.c,${BUILD_DIR}/test/%.d,${unit_tests} ${component_tests})

clean_build_targets := $(foreach build,${BUILDS},clean-${build})
clean_targets := clean ${clean_build_targets}

##################################################
#           TARGET-SPECIFIC VARIABLES            #
##################################################

release: compile_cflags += ${RELEASE_CFLAGS}
release: link_flags += ${RELEASE_LDFLAGS}
debug: compile_cppflags += ${DEBUG_CPPFLAGS}
debug: compile_cflags += ${DEBUG_CFLAGS}
test_executables: compile_cppflags += -I ${TEST_INCLUDE_DIR} ${TEST_CPPFLAGS}
test_executables: compile_cflags += -Wno-unused-parameter ${TEST_CFLAGS}
test_executables: link_flags += $(foreach test_lib,${TEST_LIBS},-l${test_lib})

# fix clang failing to link test executables (this happens when some but not all objects are compiled with '-flto' flag)
ifeq "${CC}" "clang"
  test_executables: link_flags += -fuse-ld=lld
endif

##################################################
#                   FUNCTIONS                    #
##################################################

# $(call generate_rule_building_objects_for_given_build,build)
define generate_rule_building_objects_for_given_build
# prevent make from deleting object files
.NOTINTERMEDIATE: ${BUILD_DIR}/$1/%.o

${BUILD_DIR}/$1/%.o: %.c
	@ ${MKDIR} $$(dir $$@)
	$${compile} $$< -o $$@
endef

# $(call generate_rule_cleaning_given_build,build)
define generate_rule_cleaning_given_build
clean-$1:
	${RM} ${BUILD_DIR}/$1 ${BIN_DIR}/$1
endef

##################################################
#                CANNED SEQUENCES                #
##################################################

define make_target_dir_and_link_prerequisites_into_target
	@ ${MKDIR} $(dir $@)
	${link} $^ -o $@
endef

##################################################
#                     RULES                      #
##################################################

.DELETE_ON_ERROR:
.PHONY: all ${BUILDS} ${clean_targets} help

all: ${BUILDS}

# make build
${non_test_builds}: %: ${BIN_DIR}/%/${LOX_EXEC_NAME}

# make test build (split into 2 targets to avoid release/test specific variables being applied simultaneously)
test: release test_executables
test_executables: ${unit_test_executables} ${component_test_executables}

# make build executable
${BIN_DIR}/%/${LOX_EXEC_NAME}: $(addprefix ${BUILD_DIR}/%/,${source_objects})
	${make_target_dir_and_link_prerequisites_into_target}

# make test build executables
${BIN_DIR}/test/unit/%: ${BUILD_DIR}/test/%.o
	${make_target_dir_and_link_prerequisites_into_target}

${BIN_DIR}/test/component/%: ${BUILD_DIR}/test/%.o ${component_test_release_objects}
	${make_target_dir_and_link_prerequisites_into_target}

# make build objects
$(foreach build,${BUILDS}, \
  $(eval $(call generate_rule_building_objects_for_given_build,${build})))

# clean builds
$(foreach build,${BUILDS}, \
  $(eval $(call generate_rule_cleaning_given_build,${build})))

clean:
	${RM} ${BUILD_DIR} ${BIN_DIR}

help:
	@ ${ECHO} "Targets:"
	@ ${ECHO} "    * release -- make release build (default)"
	@ ${ECHO} "    * test -- make test build"
	@ ${ECHO} "    * debug -- make debug build"
	@ ${ECHO} "    * all -- make all builds"
	@ ${ECHO} "    * clean -- clean all builds"
	@ ${ECHO} "    * clean-{build} -- clean specified {build}"

##################################################
#                    INCLUDES                    #
##################################################

# only include makefiles if user doesn't want to just clean up
ifndef MAKECMDGOALS
# if there are no command goals, user doesn't want to clean up
  -include ${dependency_makefiles} ${unit_test_makefiles}
else
  ifneq "$(filter ${clean_targets},${MAKECMDGOALS})" "${MAKECMDGOALS}"
# if there are command goals but not all of them are clean targets, user doesn't want to just clean up
    -include ${dependency_makefiles} ${unit_test_makefiles}
  endif
endif
