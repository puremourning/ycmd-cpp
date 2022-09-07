DEPS=.build-deps
TARGET=Debug

ASAN=0
ifeq (${TARGET},Debug)
	ASAN=1
endif

ifeq (${ASAN},1)
	CMAKE_ARGS=-DENABLE_ASAN=ON
endif

.PHONY: all clean distclean

PROFILE=default

all: ${TARGET}
	${DEPS}/bin/cmake --build build/${TARGET} --parallel
	${DEPS}/bin/compdb -p build/${TARGET} list > compile_commands.json

${TARGET}: ${DEPS} conanfile.py
	${DEPS}/bin/conan install --profile=${PROFILE} -s build_type=Debug . --build missing
	${DEPS}/bin/conan build .

${DEPS}: dev_requirements.txt
	python3 -m venv ${DEPS}
	${DEPS}/bin/pip3 install -r dev_requirements.txt

distclean:
	rm -rf ${DEPS}
	rm -rf build/${TARGET}

clean:
	${MAKE} -C build/${TARGET} clean
