# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.13

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /Sources/usr/local/Cellar/cmake/3.13.0/bin/cmake

# The command to remove a file.
RM = /Sources/usr/local/Cellar/cmake/3.13.0/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /Sources/research/fs/ipfs/cpp-hive-api/hive-client

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /Sources/research/fs/ipfs/cpp-hive-api/hive-client/build

# Include any dependencies generated for this target.
include CMakeFiles/cluster.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/cluster.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/cluster.dir/flags.make

CMakeFiles/cluster.dir/cluster.cc.o: CMakeFiles/cluster.dir/flags.make
CMakeFiles/cluster.dir/cluster.cc.o: ../cluster.cc
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/Sources/research/fs/ipfs/cpp-hive-api/hive-client/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/cluster.dir/cluster.cc.o"
	/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/cluster.dir/cluster.cc.o -c /Sources/research/fs/ipfs/cpp-hive-api/hive-client/cluster.cc

CMakeFiles/cluster.dir/cluster.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/cluster.dir/cluster.cc.i"
	/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /Sources/research/fs/ipfs/cpp-hive-api/hive-client/cluster.cc > CMakeFiles/cluster.dir/cluster.cc.i

CMakeFiles/cluster.dir/cluster.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/cluster.dir/cluster.cc.s"
	/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /Sources/research/fs/ipfs/cpp-hive-api/hive-client/cluster.cc -o CMakeFiles/cluster.dir/cluster.cc.s

# Object files for target cluster
cluster_OBJECTS = \
"CMakeFiles/cluster.dir/cluster.cc.o"

# External object files for target cluster
cluster_EXTERNAL_OBJECTS =

cluster: CMakeFiles/cluster.dir/cluster.cc.o
cluster: CMakeFiles/cluster.dir/build.make
cluster: /usr/lib/libcurl.dylib
cluster: CMakeFiles/cluster.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/Sources/research/fs/ipfs/cpp-hive-api/hive-client/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable cluster"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/cluster.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/cluster.dir/build: cluster

.PHONY : CMakeFiles/cluster.dir/build

CMakeFiles/cluster.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/cluster.dir/cmake_clean.cmake
.PHONY : CMakeFiles/cluster.dir/clean

CMakeFiles/cluster.dir/depend:
	cd /Sources/research/fs/ipfs/cpp-hive-api/hive-client/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /Sources/research/fs/ipfs/cpp-hive-api/hive-client /Sources/research/fs/ipfs/cpp-hive-api/hive-client /Sources/research/fs/ipfs/cpp-hive-api/hive-client/build /Sources/research/fs/ipfs/cpp-hive-api/hive-client/build /Sources/research/fs/ipfs/cpp-hive-api/hive-client/build/CMakeFiles/cluster.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/cluster.dir/depend

