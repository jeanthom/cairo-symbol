CFLAGS=`pkg-config --cflags cairomm-1.0`
LDFLAGS=`pkg-config --libs cairomm-1.0`

cairo-symbol: cairo-symbol.cc
	$(CXX) $(CFLAGS) $(LDFLAGS) $< -o $@
