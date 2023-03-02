DEPS=.build-deps
TARGET=Debug

# TODO: SOmething better...
ifeq (${TARGET},Debug)
	PRESET=debug
else
	PRESET=release
endif

.PHONY: all clean distclean test conaninstall_${TARGET} cmake_${TARGET}

PROFILE=default

all: conaninstall_${TARGET} cmake_${TARGET}
	${DEPS}/bin/cmake --build build/${TARGET} --parallel

conaninstall_${TARGET}: ${DEPS} conanfile.py
	${DEPS}/bin/conan install --profile=${PROFILE} -s compiler.cppstd=20 -s build_type=Debug . --build missing

cmake_${TARGET}: conaninstall_${TARGET}
	${DEPS}/bin/cmake --preset ${PRESET}
	${DEPS}/bin/compdb -p build/${TARGET} list > compile_commands.json

${DEPS}: dev_requirements.txt
	python3 -m venv ${DEPS}
	${DEPS}/bin/pip3 install -r dev_requirements.txt

distclean:
	rm -rf ${DEPS}
	rm -rf build

clean:
	${MAKE} -C build/${TARGET} clean

test: all
	${MAKE} -C build/${TARGET} test
