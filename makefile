
# Capture top level folder before any Makefile includes
# Note: MAKEFILE_LIST's last entry is the last processed Makefile.
#       That should be the current Makefile, assuming no includes
TopLevelFolder := $(abspath $(dir $(lastword ${MAKEFILE_LIST})))


SRCDIR := .
BUILDDIR := .build
BINDIR := $(BUILDDIR)/bin
OBJDIR := $(BUILDDIR)/obj
DEPDIR := $(BUILDDIR)/deps
OUTPUT := $(BINDIR)/netfixserver

CFLAGS := -std=c++17 -g -Wall -Wno-unknown-pragmas
LDFLAGS := -lstdc++ -lm

DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td

COMPILE.cpp = $(CXX) $(DEPFLAGS) $(CFLAGS) $(TARGET_ARCH) -c
POSTCOMPILE = @mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d && touch $@

SRCS := $(shell find $(SRCDIR) -name '*.cpp')
OBJS := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS))
FOLDERS := $(sort $(dir $(SRCS)))

all: $(OUTPUT)

$(OUTPUT): $(OBJS)
	@mkdir -p ${@D}
	$(CXX) $^ $(LDFLAGS) -o $@

$(OBJS): $(OBJDIR)/%.o : $(SRCDIR)/%.cpp $(DEPDIR)/%.d | build-folder
	$(COMPILE.cpp) $(OUTPUT_OPTION) $<
	$(POSTCOMPILE)

.PHONY: build-folder
build-folder:
	@mkdir -p $(patsubst $(SRCDIR)/%,$(OBJDIR)/%, $(FOLDERS))
	@mkdir -p $(patsubst $(SRCDIR)/%,$(DEPDIR)/%, $(FOLDERS))

$(DEPDIR)/%.d: ;
.PRECIOUS: $(DEPDIR)/%.d

include $(wildcard $(patsubst $(SRCDIR)/%.cpp,$(DEPDIR)/%.d,$(SRCS)))

.PHONY: clean clean-deps clean-all
clean:
	-rm -fr $(OBJDIR)
	-rm -fr $(DEPDIR)
	-rm -fr $(BINDIR)
clean-deps:
	-rm -fr $(DEPDIR)
clean-all:
	-rm -rf $(BUILDDIR)


# Build rules relating to Docker images

DockerFolder := ${TopLevelFolder}/.circleci/
DockerRunFlags := --volume ${TopLevelFolder}:/code
DockerRepository := outpostuniverse

ImageName := netfix
ImageVersion := 1.0

.PHONY: build-image
build-image:
	docker build ${DockerFolder}/ --file ${DockerFolder}/${ImageName}.Dockerfile --tag ${DockerRepository}/${ImageName}:latest --tag ${DockerRepository}/${ImageName}:${ImageVersion}

.PHONY: run-image
run-image:
	docker run ${DockerRunFlags} --rm --tty ${DockerRepository}/${ImageName}

.PHONY: debug-image
debug-image:
	docker run ${DockerRunFlags} --rm --tty --interactive ${DockerRepository}/${ImageName} bash

.PHONY: root-debug-image
root-debug-image:
	docker run ${DockerRunFlags} --rm --tty --interactive --user=0 ${DockerRepository}/${ImageName} bash

.PHONY: push-image
push-image:
	docker push ${DockerRepository}/${ImageName}
