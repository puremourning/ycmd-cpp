DEPS=.build-deps
BUILD=cmake-build-debug

.PHONY: all clean

all: ${BUILD}
	${DEPS}/bin/cmake --build ${BUILD} --parallel
	${DEPS}/bin/compdb -p ${BUILD} list > compile_commands.json

${BUILD}: ${DEPS} conanfile.py
	${DEPS}/bin/conan install -s build_type=Debug . --build missing
	${DEPS}/bin/conan build .

${DEPS}: dev_requirements.txt
	python3 -m venv ${DEPS}
	${DEPS}/bin/pip3 install -r dev_requirements.txt

distclean:
	rm -rf ${DEPS}
	rm -rf ${BUILD}

clean:
	${MAKE} -C ${BUILD} clean
