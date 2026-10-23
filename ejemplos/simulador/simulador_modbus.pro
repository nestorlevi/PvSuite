# Simulador Modbus TCP para probar los ejemplos sin PLC: qmake simulador_modbus.pro && make
TEMPLATE = app
CONFIG  += console
CONFIG  -= qt app_bundle
SOURCES  = simulador_modbus.cpp
win32:LIBS += -lws2_32
unix:LIBS  += -lpthread
