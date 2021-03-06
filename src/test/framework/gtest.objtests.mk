#  IBM_PROLOG_BEGIN_TAG
#  IBM_PROLOG_END
# A sample Makefile for building Google Test and using it in user
# tests.  Please tweak it to suit your environment and project.  You
# may want to move it to your project's root directory.
#
# SYNOPSIS:
#
#   make [all]  - makes everything.
#   make TARGET - makes the given target.
#   make clean  - removes all files generated by make.

# Please tweak the following variable definitions as needed by your
# project, except GTEST_HEADERS, which you can use in your own targets
# but shouldn't modify.

# Points to the root of Google Test, relative to where this file is.
# Remember to tweak this if you move this file.
GTEST_DIR =$(ROOTPATH)/src/test/framework/googletest/googletest

UNAME=$(shell uname)

# Flags passed to the preprocessor.
# Set Google Test's header directory as a system directory, such that
# the compiler doesn't generate warnings in Google Test headers.
ifeq ($(UNAME),Linux)
CPPFLAGS += -isystem $(GTEST_DIR)/include
else
CPPFLAGS += -DOLD_ANSIC_AIX_VERSION -I $(GTEST_DIR)/include
endif

# Flags passed to the C++ compiler.
#CXXFLAGS += -g -Wall -Wextra -pthread



# All Google Test headers.  Usually you shouldn't change this
# definition.
GTEST_HEADERS = $(GTEST_DIR)/include/gtest/*.h \
                $(GTEST_DIR)/include/gtest/internal/*.h


# Builds gtest.a and gtest_main.a.

# Usually you shouldn't tweak such internal variables, indicated by a
# trailing _.
GTEST_SRCS_ = $(GTEST_DIR)/src/*.cc $(GTEST_DIR)/src/*.h $(GTEST_HEADERS)

# For simplicity and to avoid depending on Google Test's
# implementation details, the dependencies specified below are
# conservative and not optimized.  
GTEST_TARGETS = $(TESTDIR)/gtest-all.o  \
                $(TESTDIR)/gtest_main.o \
                $(TESTDIR)/gtest.o

ifeq ($(UNAME),AIX)
GTEST_TARGETS += $(TESTDIR)/64obj/gtest-all.o  \
                 $(TESTDIR)/64obj/gtest_main.o  \
                 $(TESTDIR)/64obj/gtest.o

$(TESTDIR)/64obj/gtest-all.o : $(GTEST_SRCS_)
	@mkdir -p ${TESTDIR}/64obj
	$(CXX) -q64 $(CPPFLAGS) -I$(GTEST_DIR) $(CXXFLAGS) -c \
            $(GTEST_DIR)/src/gtest-all.cc  -o $@

$(TESTDIR)/64obj/gtest_main.o : $(GTEST_SRCS_)
	@mkdir -p ${TESTDIR}/64obj
	$(CXX) -q64 $(CPPFLAGS) -I$(GTEST_DIR) $(CXXFLAGS) -c \
            $(GTEST_DIR)/src/gtest_main.cc  -o $@

$(TESTDIR)/64obj/gtest.o : $(GTEST_SRCS_)
	@mkdir -p ${TESTDIR}/64obj
	$(CXX) -q64 $(CPPFLAGS) -I$(GTEST_DIR) $(CXXFLAGS) -c \
            $(GTEST_DIR)/src/gtest.cc  -o $@
endif

$(TESTDIR)/gtest-all.o : $(GTEST_SRCS_)
	$(CXX) $(CPPFLAGS) -I$(GTEST_DIR) $(CXXFLAGS) -c \
            $(GTEST_DIR)/src/gtest-all.cc  -o $@


$(TESTDIR)/gtest_main.o : $(GTEST_SRCS_)
	$(CXX) $(CPPFLAGS) -I$(GTEST_DIR) $(CXXFLAGS) -c \
            $(GTEST_DIR)/src/gtest_main.cc  -o $@

$(TESTDIR)/gtest.o : $(GTEST_SRCS_)
	$(CXX) $(CPPFLAGS) -I$(GTEST_DIR) $(CXXFLAGS) -c \
            $(GTEST_DIR)/src/gtest.cc  -o $@
