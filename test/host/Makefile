# Pc-tests van de decoders: make -C test/host
SRC = ../../src
CXXFLAGS = -std=gnu++17 -O1 -g -Wall -Wextra -Wno-unused-parameter -Ishim -I$(SRC) -fsanitize=address,undefined \
           -DBARO_HISTORY_ONLY
OBJS = main.cpp test_sensors.cpp test_n2k.cpp test_baro.cpp test_energy.cpp test_datalog.cpp \
       $(SRC)/sensors.cpp $(SRC)/n2k_decode.cpp $(SRC)/baro.cpp $(SRC)/energy.cpp $(SRC)/datalog.cpp

run: tests
	./tests

tests: $(OBJS) check.h shim/Arduino.h shim/mbedtls/aes.h $(SRC)/sensors.h $(SRC)/n2k_decode.h $(SRC)/baro.h $(SRC)/energy.h $(SRC)/datalog.h
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) -lcrypto

# Speelt een NMEA 2000-opname af door de decoder (zie scripts/n2k_compare.py)
n2k_replay: n2k_replay.cpp $(SRC)/n2k_decode.cpp $(SRC)/n2k_decode.h
	$(CXX) $(CXXFLAGS) -o $@ n2k_replay.cpp $(SRC)/n2k_decode.cpp

clean:
	rm -f tests n2k_replay

.PHONY: run clean
