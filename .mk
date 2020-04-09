roots = . $(libs:%=lib/%)
CPPFLAGS += $(roots:%=-I%/include)
ifdef CXXSTD
CXXFLAGS += -std=$(CXXSTD)
endif
ifdef CSTD
CFLAGS += -std=$(CSTD)
endif
VPATH = $(roots:%=%/src)
vpath    %.o build
vpath lib%.a build

.PHONY: clean $(libs)

$(libs:%=lib/%): lib/%:
	@git clone -b $($*_ver) $($*_url) lib/$*

$(libs): %: | lib/%
	@git -C lib/$* fetch
	@git -C lib/$* checkout --force $($*_ver)

build: ; @mkdir -p $@
lib:   ; @mkdir -p $@

%.o: %.cpp
%.o: %.c

build/%.cpp.o: %.cpp | build $(libs:%=lib/%)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

build/%.c.o: %.c | build $(libs:%=lib/%)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/%.cpp.d: %.cpp | build $(libs:%=lib/%)
	@-$(CXX) -MM $(CPPFLAGS) $< -MF $@ -MT "$@ $(@:.d=.o)"

build/%.c.d: %.c | build $(libs:%=lib/%)
	@-$(CC) -MM $(CPPFLAGS) $< -MF $@ -MT "$@ $(@:.d=.o)"

build/lib%.a: | build
	$(AR) rcs $@ $^

%:
	$(if $(findstring .cpp.o,$^),$(CXX),$(CC)) $(LDFLAGS) $^ -o $@

include $(patsubst %,build/%.d,$(notdir $(wildcard \
	$(roots:%=%/src/*.cpp)) $(wildcard $(roots:%=%/src/*.c))))
